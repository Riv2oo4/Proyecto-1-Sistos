#include <iostream>
#include <string>
#include <vector>
#include <thread>
#include <mutex>
#include <atomic>
#include <gtk/gtk.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <cstring>

// Definición de constantes
#define BUFFER_SIZE 1024
#define MAX_USERNAME_LENGTH 32

// Enumeración para los tipos de mensajes
enum MessageType {
    REGISTER_USER = 1,
    USER_LIST = 2,
    USER_INFO = 3,
    CHANGE_STATUS = 4,
    BROADCAST_MSG = 5,
    DIRECT_MSG = 6,
    DISCONNECT = 7
};

// Enumeración para los estados de usuario
enum UserStatus {
    ACTIVE = 1,
    BUSY = 2,
    INACTIVE = 3
};

// Estructura para representar un usuario
struct User {
    std::string username;
    std::string ip;
    UserStatus status;
};

// Estructura para representar un mensaje
struct Message {
    std::string sender;
    std::string content;
    bool isPrivate;
    std::string recipient;
};

// Clase del chat cliente
class ChatClient {
    private:
        // Conexión
        std::string username;
        std::string serverIP;
        int serverPort;
        int clientSocket;
        
        // Estado
        std::atomic<bool> running;
        std::atomic<UserStatus> status;
        
        // Lista de conectados
        std::vector<User> connectedUsers;
        std::mutex usersMutex;
        
        // Historial
        std::vector<Message> messageHistory;
        std::mutex messagesMutex;
        
        // Hilos de recepción
        std::thread receiverThread;
    
        GtkWidget *window;
        GtkWidget *messageView;
        GtkTextBuffer *messageBuffer;
        GtkWidget *messageEntry;
        GtkWidget *userListView;
        GtkListStore *userListStore;
        GtkWidget *statusComboBox;
        
        // Métodos privados
        void messageReceiver();
        bool sendMessage(MessageType type, const std::string &content, const std::string &recipient = "");
        
        void updateUserList();
        void addMessage(const std::string &sender, const std::string &content, bool isPrivate = false, const std::string &recipient = "");
        
    public:
        ChatClient();
        ~ChatClient();
        
        bool initialize(const std::string &username, const std::string &serverIP, int serverPort);
        void run(GtkApplication *app);
        void shutdown();
        void onSendButtonClicked();
        void onStatusChanged();
        void onWindowClosed();
        
        // Ventana de login
        static bool showLoginDialog(std::string &username, std::string &serverIP, int &serverPort);
    };

// Constructor/Destructor
ChatClient::ChatClient() : running(false), status(ACTIVE), clientSocket(-1) {}

ChatClient::~ChatClient() {
    shutdown();
}

// Inicialización
bool ChatClient::initialize(const std::string &username, const std::string &serverIP, int serverPort) {
    this->username = username;
    this->serverIP = serverIP;
    this->serverPort = serverPort;
    
    // Crear socket
    clientSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (clientSocket < 0) {
        std::cerr << "Error al crear el socket" << std::endl;
        return false;
    }
    
    // Configurar dirección del servidor
    struct sockaddr_in serverAddr;
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(serverPort);
    
    if (inet_pton(AF_INET, serverIP.c_str(), &serverAddr.sin_addr) <= 0) {
        std::cerr << "Dirección IP inválida" << std::endl;
        close(clientSocket);
        return false;
    }
    
    // Conectar al servidor
    if (connect(clientSocket, (struct sockaddr *)&serverAddr, sizeof(serverAddr)) < 0) {
        std::cerr << "Error al conectar con el servidor" << std::endl;
        close(clientSocket);
        return false;
    }
    running = true;
    status = ACTIVE;
    
    // Enviar mensaje de registro al servidor
    if (!sendMessage(REGISTER_USER, username)) {
        std::cerr << "Error al registrarse con el servidor" << std::endl;
        shutdown();
        return false;
    }
    
    return true;
}


// Método principal de ejecución
void ChatClient::run(GtkApplication *app) {
    // Crear la ventana principal
    window = gtk_application_window_new(app);
    gtk_window_set_title(GTK_WINDOW(window), "Chat Client");
    gtk_window_set_default_size(GTK_WINDOW(window), 800, 600);
    
    // Crear layout principal
    GtkWidget *mainBox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
    gtk_container_add(GTK_CONTAINER(window), mainBox);
    
    // Crear header con información del usuario
    GtkWidget *headerBox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);
    gtk_widget_set_margin_start(headerBox, 10);
    gtk_widget_set_margin_end(headerBox, 10);
    gtk_widget_set_margin_top(headerBox, 10);
    gtk_widget_set_margin_bottom(headerBox, 5);
    gtk_container_add(GTK_CONTAINER(mainBox), headerBox);
    
    // Etiqueta de usuario
    GtkWidget *userLabel = gtk_label_new(("Usuario: " + username).c_str());
    gtk_container_add(GTK_CONTAINER(headerBox), userLabel);
    
    // Combo box para el estado
    GtkWidget *statusLabel = gtk_label_new("Estado: ");
    gtk_container_add(GTK_CONTAINER(headerBox), statusLabel);
    
    statusComboBox = gtk_combo_box_text_new();
    gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(statusComboBox), NULL, "ACTIVO");
    gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(statusComboBox), NULL, "OCUPADO");
    gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(statusComboBox), NULL, "INACTIVO");
    gtk_combo_box_set_active(GTK_COMBO_BOX(statusComboBox), 0);
    gtk_container_add(GTK_CONTAINER(headerBox), statusComboBox);
    
    // Etiqueta de servidor
    GtkWidget *serverLabel = gtk_label_new(("Servidor: " + serverIP + ":" + std::to_string(serverPort)).c_str());
    gtk_container_add(GTK_CONTAINER(headerBox), serverLabel);
    
    // Crear área principal con chat y lista de usuarios
    GtkWidget *contentBox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);
    gtk_widget_set_margin_start(contentBox, 10);
    gtk_widget_set_margin_end(contentBox, 10);
    gtk_widget_set_margin_top(contentBox, 5);
    gtk_widget_set_margin_bottom(contentBox, 5);
    gtk_widget_set_vexpand(contentBox, TRUE);
    gtk_container_add(GTK_CONTAINER(mainBox), contentBox);
    
    // Área de mensajes
    GtkWidget *messageScrollWindow = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(messageScrollWindow),
                                 GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
    gtk_widget_set_hexpand(messageScrollWindow, TRUE);
    gtk_container_add(GTK_CONTAINER(contentBox), messageScrollWindow);
    
    messageView = gtk_text_view_new();
    gtk_text_view_set_editable(GTK_TEXT_VIEW(messageView), FALSE);
    gtk_text_view_set_cursor_visible(GTK_TEXT_VIEW(messageView), FALSE);
    gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(messageView), GTK_WRAP_WORD_CHAR);
    gtk_container_add(GTK_CONTAINER(messageScrollWindow), messageView);
    
    messageBuffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(messageView));
    
    // Lista de usuarios
    GtkWidget *userScrollWindow = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(userScrollWindow),
                                 GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
    gtk_widget_set_size_request(userScrollWindow, 200, -1);
    gtk_container_add(GTK_CONTAINER(contentBox), userScrollWindow);
    
    // Crear modelo para la lista de usuarios
    userListStore = gtk_list_store_new(2, G_TYPE_STRING, G_TYPE_STRING);
    userListView = gtk_tree_view_new_with_model(GTK_TREE_MODEL(userListStore));
    
    // Columna de usuario
    GtkTreeViewColumn *userColumn = gtk_tree_view_column_new();
    gtk_tree_view_column_set_title(userColumn, "Usuario");
    gtk_tree_view_append_column(GTK_TREE_VIEW(userListView), userColumn);
    
    GtkCellRenderer *userRenderer = gtk_cell_renderer_text_new();
    gtk_tree_view_column_pack_start(userColumn, userRenderer, TRUE);
    gtk_tree_view_column_add_attribute(userColumn, userRenderer, "text", 0);
    
    // Columna de estado
    GtkTreeViewColumn *statusColumn = gtk_tree_view_column_new();
    gtk_tree_view_column_set_title(statusColumn, "Estado");
    gtk_tree_view_append_column(GTK_TREE_VIEW(userListView), statusColumn);
    
    GtkCellRenderer *statusRenderer = gtk_cell_renderer_text_new();
    gtk_tree_view_column_pack_start(statusColumn, statusRenderer, TRUE);
    gtk_tree_view_column_add_attribute(statusColumn, statusRenderer, "text", 1);
    
    gtk_container_add(GTK_CONTAINER(userScrollWindow), userListView);
    
    // Área de entrada de mensajes
    GtkWidget *inputBox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);
    gtk_widget_set_margin_start(inputBox, 10);
    gtk_widget_set_margin_end(inputBox, 10);
    gtk_widget_set_margin_top(inputBox, 5);
    gtk_widget_set_margin_bottom(inputBox, 10);
    gtk_container_add(GTK_CONTAINER(mainBox), inputBox);
    
    messageEntry = gtk_entry_new();
    gtk_widget_set_hexpand(messageEntry, TRUE);
    gtk_container_add(GTK_CONTAINER(inputBox), messageEntry);
    
    GtkWidget *sendButton = gtk_button_new_with_label("Enviar");
    gtk_container_add(GTK_CONTAINER(inputBox), sendButton);
    
    // Conectar señales
    g_signal_connect(window, "destroy", G_CALLBACK(gtk_main_quit), NULL);
    g_signal_connect_swapped(sendButton, "clicked", G_CALLBACK(+[](gpointer data) {
        static_cast<ChatClient*>(data)->onSendButtonClicked();
    }), this);
    g_signal_connect_swapped(messageEntry, "activate", G_CALLBACK(+[](gpointer data) {
        static_cast<ChatClient*>(data)->onSendButtonClicked();
    }), this);
    g_signal_connect_swapped(statusComboBox, "changed", G_CALLBACK(+[](gpointer data) {
        static_cast<ChatClient*>(data)->onStatusChanged();
    }), this);
    
    // Mostrar todos los widgets
    gtk_widget_show_all(window);
    
    // Iniciar el hilo receptor de mensajes
    receiverThread = std::thread(&ChatClient::messageReceiver, this);
}

// Detener el cliente
void ChatClient::shutdown() {
    if (running) {
        running = false;
        
        // Enviar mensaje de desconexión al servidor
        sendMessage(DISCONNECT, username);
        
        // Cerrar el socket
        if (clientSocket >= 0) {
            close(clientSocket);
            clientSocket = -1;
        }
        
        // Esperar a que el hilo receptor termine
        if (receiverThread.joinable()) {
            receiverThread.join();
        }
    }
}
void ChatClient::messageReceiver() {
    char buffer[BUFFER_SIZE];
    
    while (running) {
        // Recibir datos del servidor
        memset(buffer, 0, BUFFER_SIZE);
        int bytesRead = recv(clientSocket, buffer, BUFFER_SIZE - 1, 0);
        
        if (bytesRead > 0) {
            buffer[bytesRead] = '\0';
            
            // Actualizar la interfaz desde el hilo principal
            gdk_threads_add_idle(+[](gpointer data) -> gboolean {
                ChatClient client = static_cast<ChatClient>(data);
                client->addMessage("Servidor", std::string(static_cast<char*>(g_object_get_data(G_OBJECT(data), "buffer"))));
                return FALSE;
            }, this);
            
            g_object_set_data_full(G_OBJECT(this), "buffer", g_strdup(buffer), g_free);
        } else if (bytesRead == 0) {
            // Conexión cerrada por el servidor
            gdk_threads_add_idle(+[](gpointer data) -> gboolean {
                ChatClient client = static_cast<ChatClient>(data);
                client->addMessage("Sistema", "Conexión cerrada por el servidor");
                return FALSE;
            }, this);
            
            running = false;
            break;
        } else {
            // Error en la recepción
            if (running) {
                gdk_threads_add_idle(+[](gpointer data) -> gboolean {
                    ChatClient client = static_cast<ChatClient>(data);
                    client->addMessage("Sistema", "Error al recibir datos del servidor");
                    return FALSE;
                }, this);
                
                running = false;
                break;
            }
        }
    }
}
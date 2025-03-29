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

// Clase principal del cliente de chat
class ChatClient {
private:
    // Información de conexión
    std::string username;
    std::string serverIP;
    int serverPort;
    int clientSocket;
    
    // Estado del cliente
    std::atomic<bool> running;
    std::atomic<UserStatus> status;
    
    // Lista de usuarios conectados
    std::vector<User> connectedUsers;
    std::mutex usersMutex;
    
    // Historial de mensajes
    std::vector<Message> messageHistory;
    std::mutex messagesMutex;
    
    // Hilos para recepción de mensajes
    std::thread receiverThread;

    // Widgets de GTK
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
    
    // Métodos para eventos de GTK
    void onSendButtonClicked();
    void onStatusChanged();
    void onWindowClosed();
    
    // Método estático para mostrar la ventana de login
    static bool showLoginDialog(std::string &username, std::string &serverIP, int &serverPort);
};

// Constructor/Destructor
ChatClient::ChatClient() : running(false), status(ACTIVE), clientSocket(-1) {}

ChatClient::~ChatClient() {
    shutdown();
}

// Inicialización del cliente
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
    
    // Inicializar el estado
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

// Hilo receptor de mensajes
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
                ChatClient *client = static_cast<ChatClient*>(data);
                client->addMessage("Servidor", std::string(static_cast<char*>(g_object_get_data(G_OBJECT(data), "buffer"))));
                return FALSE;
            }, this);
            
            g_object_set_data_full(G_OBJECT(this), "buffer", g_strdup(buffer), g_free);
        } else if (bytesRead == 0) {
            // Conexión cerrada por el servidor
            gdk_threads_add_idle(+[](gpointer data) -> gboolean {
                ChatClient *client = static_cast<ChatClient*>(data);
                client->addMessage("Sistema", "Conexión cerrada por el servidor");
                return FALSE;
            }, this);
            
            running = false;
            break;
        } else {
            // Error en la recepción
            if (running) {
                gdk_threads_add_idle(+[](gpointer data) -> gboolean {
                    ChatClient *client = static_cast<ChatClient*>(data);
                    client->addMessage("Sistema", "Error al recibir datos del servidor");
                    return FALSE;
                }, this);
                
                running = false;
                break;
            }
        }
    }
}

// Enviar mensaje al servidor
bool ChatClient::sendMessage(MessageType type, const std::string &content, const std::string &recipient) {
    // Aquí implementaríamos el formato según el protocolo acordado
    // Por ahora, solo enviaremos el mensaje tal cual
    std::string message;
    
    switch (type) {
        case REGISTER_USER:
            message = "REGISTER " + content;
            break;
        case USER_LIST:
            message = "LIST";
            break;
        case USER_INFO:
            message = "INFO " + content;
            break;
        case CHANGE_STATUS:
            message = "STATUS " + content;
            break;
        case BROADCAST_MSG:
            message = "BROADCAST " + content;
            break;
        case DIRECT_MSG:
            message = "MSG " + recipient + " " + content;
            break;
        case DISCONNECT:
            message = "DISCONNECT";
            break;
        default:
            return false;
    }
    
    // Enviar mensaje
    int bytesSent = send(clientSocket, message.c_str(), message.length(), 0);
    return bytesSent == static_cast<int>(message.length());
}

// Añadir mensaje al historial
void ChatClient::addMessage(const std::string &sender, const std::string &content, bool isPrivate, const std::string &recipient) {
    std::lock_guard<std::mutex> lock(messagesMutex);
    
    Message msg;
    msg.sender = sender;
    msg.content = content;
    msg.isPrivate = isPrivate;
    msg.recipient = recipient;
    
    messageHistory.push_back(msg);
    
    // Actualizar el área de texto
    GtkTextIter end;
    gtk_text_buffer_get_end_iter(messageBuffer, &end);
    
    // Formatear el mensaje
    std::string formattedMessage;
    if (isPrivate) {
        if (sender == username) {
            formattedMessage = "[Privado a " + recipient + "]: " + content + "\n";
        } else {
            formattedMessage = "[Privado de " + sender + "]: " + content + "\n";
        }
    } else {
        formattedMessage = sender + ": " + content + "\n";
    }
    
    // Insertar el mensaje
    gtk_text_buffer_insert(messageBuffer, &end, formattedMessage.c_str(), -1);
    
    // Hacer scroll al final
    gtk_text_view_scroll_to_mark(GTK_TEXT_VIEW(messageView),
                              gtk_text_buffer_get_mark(messageBuffer, "insert"),
                              0.0, FALSE, 0.0, 1.0);
}

// Actualizar lista de usuarios
void ChatClient::updateUserList() {
    std::lock_guard<std::mutex> lock(usersMutex);
    
    // Limpiar la lista actual
    gtk_list_store_clear(userListStore);
    
    // Añadir los usuarios
    for (const auto &user : connectedUsers) {
        GtkTreeIter iter;
        gtk_list_store_append(userListStore, &iter);
        
        std::string statusStr;
        switch (user.status) {
            case ACTIVE: statusStr = "ACTIVO"; break;
            case BUSY: statusStr = "OCUPADO"; break;
            case INACTIVE: statusStr = "INACTIVO"; break;
        }
        
        gtk_list_store_set(userListStore, &iter,
                        0, user.username.c_str(),
                        1, statusStr.c_str(),
                        -1);
    }
}

// Manejador para el botón de enviar
void ChatClient::onSendButtonClicked() {
    const gchar *text = gtk_entry_get_text(GTK_ENTRY(messageEntry));
    
    if (text && *text) {
        std::string input(text);
        
        // Limpiar el campo de entrada
        gtk_entry_set_text(GTK_ENTRY(messageEntry), "");
        
        // Procesar comando o mensaje
        if (input[0] == '/') {
            // Extraer comando y argumentos
            std::string cmd = input.substr(1);
            size_t spacePos = cmd.find(' ');
            std::string arg;
            
            if (spacePos != std::string::npos) {
                arg = cmd.substr(spacePos + 1);
                cmd = cmd.substr(0, spacePos);
            }
            
            if (cmd == "msg" || cmd == "m") {
                // Mensaje privado: /msg usuario mensaje
                size_t msgStart = arg.find(' ');
                if (msgStart != std::string::npos) {
                    std::string recipient = arg.substr(0, msgStart);
                    std::string message = arg.substr(msgStart + 1);
                    
                    sendMessage(DIRECT_MSG, message, recipient);
                    addMessage(username, message, true, recipient);
                } else {
                    addMessage("Sistema", "Uso: /msg usuario mensaje");
                }
            } else if (cmd == "users" || cmd == "u") {
                // Solicitar lista de usuarios
                sendMessage(USER_LIST, "");
            } else if (cmd == "info" || cmd == "i") {
                // Solicitar información de un usuario: /info usuario
                if (!arg.empty()) {
                    sendMessage(USER_INFO, arg);
                } else {
                    addMessage("Sistema", "Uso: /info usuario");
                }
            } else if (cmd == "help" || cmd == "h") {
                // Mostrar ayuda
                addMessage("Sistema", "Comandos disponibles:");
                addMessage("Sistema", "/msg, /m usuario mensaje - Enviar mensaje privado");
                addMessage("Sistema", "/users, /u - Listar usuarios conectados");
                addMessage("Sistema", "/info, /i usuario - Ver información de un usuario");
                addMessage("Sistema", "/help, /h - Mostrar esta ayuda");
                addMessage("Sistema", "/quit, /q - Salir del chat");
            } else if (cmd == "quit" || cmd == "q") {
                // Salir del chat
                gtk_widget_destroy(window);
            } else {
                // Comando no reconocido
                addMessage("Sistema", "Comando no reconocido. Usa /help para ver los comandos disponibles.");
            }
        } else {
            // Es un mensaje de broadcast
            sendMessage(BROADCAST_MSG, input);
            addMessage(username, input);
        }
    }
}

// Manejador para el cambio de estado
void ChatClient::onStatusChanged() {
    gint active = gtk_combo_box_get_active(GTK_COMBO_BOX(statusComboBox));
    
    UserStatus newStatus;
    switch (active) {
        case 0: newStatus = ACTIVE; break;
        case 1: newStatus = BUSY; break;
        case 2: newStatus = INACTIVE; break;
        default: newStatus = ACTIVE; break;
    }
    
    // Actualizar estado local
    status = newStatus;
    
    // Enviar cambio de estado al servidor
    sendMessage(CHANGE_STATUS, std::to_string(static_cast<int>(newStatus)));
}

// Método estático para mostrar la ventana de login
bool ChatClient::showLoginDialog(std::string &username, std::string &serverIP, int &serverPort) {
    GtkWidget *dialog, *content_area;
    GtkWidget *grid;
    GtkWidget *usernameLabel, *usernameEntry;
    GtkWidget *ipLabel, *ipEntry;
    GtkWidget *portLabel, *portEntry;
    GtkDialogFlags flags;
    
    // Crear diálogo
    flags = static_cast<GtkDialogFlags>(GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT);
    dialog = gtk_dialog_new_with_buttons("Iniciar sesión",
                                       NULL,
                                       flags,
                                       "_Cancelar",
                                       GTK_RESPONSE_CANCEL,
                                       "_Conectar",
                                       GTK_RESPONSE_ACCEPT,
                                       NULL);
    
    // Obtener el área de contenido
    content_area = gtk_dialog_get_content_area(GTK_DIALOG(dialog));
    
    // Crear una rejilla para organizar los widgets
    grid = gtk_grid_new();
    gtk_grid_set_column_spacing(GTK_GRID(grid), 10);
    gtk_grid_set_row_spacing(GTK_GRID(grid), 10);
    gtk_widget_set_margin_start(grid, 20);
    gtk_widget_set_margin_end(grid, 20);
    gtk_widget_set_margin_top(grid, 20);
    gtk_widget_set_margin_bottom(grid, 20);
    gtk_container_add(GTK_CONTAINER(content_area), grid);
    
    // Campos de entrada
    usernameLabel = gtk_label_new("Nombre de usuario:");
    gtk_widget_set_halign(usernameLabel, GTK_ALIGN_START);
    gtk_grid_attach(GTK_GRID(grid), usernameLabel, 0, 0, 1, 1);
    
    usernameEntry = gtk_entry_new();
    gtk_entry_set_max_length(GTK_ENTRY(usernameEntry), MAX_USERNAME_LENGTH);
    gtk_widget_set_hexpand(usernameEntry, TRUE);
    gtk_grid_attach(GTK_GRID(grid), usernameEntry, 1, 0, 1, 1);
    
    ipLabel = gtk_label_new("IP del servidor:");
    gtk_widget_set_halign(ipLabel, GTK_ALIGN_START);
    gtk_grid_attach(GTK_GRID(grid), ipLabel, 0, 1, 1, 1);
    
    ipEntry = gtk_entry_new();
    gtk_entry_set_text(GTK_ENTRY(ipEntry), "127.0.0.1");
    gtk_widget_set_hexpand(ipEntry, TRUE);
    gtk_grid_attach(GTK_GRID(grid), ipEntry, 1, 1, 1, 1);
    
    portLabel = gtk_label_new("Puerto del servidor:");
    gtk_widget_set_halign(portLabel, GTK_ALIGN_START);
    gtk_grid_attach(GTK_GRID(grid), portLabel, 0, 2, 1, 1);
    
    portEntry = gtk_entry_new();
    gtk_entry_set_text(GTK_ENTRY(portEntry), "8080");
    gtk_widget_set_hexpand(portEntry, TRUE);
    gtk_grid_attach(GTK_GRID(grid), portEntry, 1, 2, 1, 1);
    
    // Validación numérica para el puerto
    g_signal_connect(portEntry, "changed", G_CALLBACK(+[](GtkEditable *editable, gpointer user_data) {
        gchar *text = gtk_editable_get_chars(editable, 0, -1);
        
        // Verificar que solo contenga dígitos
        for (gchar *p = text; *p != '\0'; p++) {
            if (*p < '0' || *p > '9') {
                // Eliminar caracteres no numéricos
                gchar *new_text = g_strdup(text);
                gchar *q = new_text;
                for (gchar *p = text; *p != '\0'; p++) {
                    if (*p >= '0' && *p <= '9') {
                        *q++ = *p;
                    }
                }
                *q = '\0';
                
                // Actualizar el texto
                // Simplemente aplicar el nuevo texto
                gtk_entry_set_text(GTK_ENTRY(editable), new_text);
                
                g_free(new_text);
                break;
            }
        }
        
        g_free(text);
    }), NULL);
    
    // Mostrar todos los widgets
    gtk_widget_show_all(dialog);
    
    // Ejecutar el diálogo
    gint result = gtk_dialog_run(GTK_DIALOG(dialog));
    
    if (result == GTK_RESPONSE_ACCEPT) {
        // Obtener valores
        username = std::string(gtk_entry_get_text(GTK_ENTRY(usernameEntry)));
        serverIP = std::string(gtk_entry_get_text(GTK_ENTRY(ipEntry)));
        
        // Convertir puerto a entero
        try {
            serverPort = std::stoi(std::string(gtk_entry_get_text(GTK_ENTRY(portEntry))));
        } catch (...) {
            serverPort = 8080; // Valor por defecto
        }
        
        // Verificar entradas
        if (username.empty()) {
            GtkWidget *errorDialog = gtk_message_dialog_new(GTK_WINDOW(dialog),
                                                         GTK_DIALOG_DESTROY_WITH_PARENT,
                                                         GTK_MESSAGE_ERROR,
                                                         GTK_BUTTONS_CLOSE,
                                                         "El nombre de usuario no puede estar vacío");
            gtk_dialog_run(GTK_DIALOG(errorDialog));
            gtk_widget_destroy(errorDialog);
            gtk_widget_destroy(dialog);
            return false;
        }
        
        if (serverIP.empty()) {
            serverIP = "127.0.0.1"; // Valor por defecto
        }
        
        if (serverPort <= 0 || serverPort > 65535) {
            serverPort = 8080; // Valor por defecto
        }
        
        gtk_widget_destroy(dialog);
        return true;
    } else {
        gtk_widget_destroy(dialog);
        return false;
    }
}

// Función de activación para la aplicación GTK
static void activate(GtkApplication *app, gpointer user_data) {
    ChatClient *client = static_cast<ChatClient*>(user_data);
    client->run(app);
}

// Función principal
int main(int argc, char *argv[]) {
    // Inicializar GTK
    gtk_init(&argc, &argv);
    
    // Variables para la información de conexión
    std::string username, serverIP;
    int serverPort;
    
    // Mostrar ventana de login
    if (!ChatClient::showLoginDialog(username, serverIP, serverPort)) {
        return 0; // El usuario canceló
    }
    
    // Crear e inicializar el cliente
    ChatClient client;
    if (!client.initialize(username, serverIP, serverPort)) {
        GtkWidget *errorDialog = gtk_message_dialog_new(NULL,
                                                     GTK_DIALOG_DESTROY_WITH_PARENT,
                                                     GTK_MESSAGE_ERROR,
                                                     GTK_BUTTONS_CLOSE,
                                                     "Error al conectar con el servidor %s:%d",
                                                     serverIP.c_str(), serverPort);
        gtk_dialog_run(GTK_DIALOG(errorDialog));
        gtk_widget_destroy(errorDialog);
        return 1;
    }
    
    // Crear aplicación GTK
    GtkApplication *app = gtk_application_new("org.chat.client", G_APPLICATION_DEFAULT_FLAGS);
    g_signal_connect(app, "activate", G_CALLBACK(activate), &client);
    
    // Ejecutar la aplicación
    int status = g_application_run(G_APPLICATION(app), 0, NULL);
    
    // Liberar recursos
    g_object_unref(app);
    
    return status;
}
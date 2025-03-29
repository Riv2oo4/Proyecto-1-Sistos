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
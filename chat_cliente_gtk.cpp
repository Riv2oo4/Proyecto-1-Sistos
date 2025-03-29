#include <iostream>
#include <string>
#include <vector>
#include <thread>
#include <mutex>
#include <atomic>
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

#include <wx/wx.h>
#include <wx/listbox.h>
#include <wx/stattext.h>
#include <wx/choice.h>
#include <boost/asio.hpp>
#include <boost/beast.hpp>
#include <iostream>
#include <vector>
#include <thread>
#include <unordered_map>
#include <mutex>
#include <memory>
#include <string>
#include <functional>
#include <algorithm>

// Namespace aliases for cleaner code
namespace net = boost::asio;
namespace beast = boost::beast;
namespace websocket = beast::websocket;
using tcp = net::ip::tcp;

// Protocol message types
enum ProtocolMessageType : uint8_t {
    // Client to server messages
    MSG_CLIENT_REQUEST_USERS = 1,
    MSG_CLIENT_GET_USER_INFO = 2,
    MSG_CLIENT_UPDATE_STATUS = 3,
    MSG_CLIENT_SEND_MESSAGE = 4,
    MSG_CLIENT_REQUEST_HISTORY = 5,

    // Server to client messages
    MSG_SERVER_ERROR = 50,
    MSG_SERVER_USER_LIST = 51,
    MSG_SERVER_USER_INFO = 52,
    MSG_SERVER_USER_JOINED = 53,
    MSG_SERVER_STATUS_UPDATE = 54,
    MSG_SERVER_NEW_MESSAGE = 55,
    MSG_SERVER_CHAT_HISTORY = 56
};

// Error codes from server
enum ServerErrorCode : uint8_t {
    ERR_USER_NOT_FOUND = 1,
    ERR_INVALID_STATUS = 2,
    ERR_EMPTY_MESSAGE = 3,
    ERR_RECIPIENT_OFFLINE = 4
};

// User status
enum class UserStatus : uint8_t {
    OFFLINE = 0,
    ONLINE = 1,
    BUSY = 2,
    AWAY = 3
};

class Contact {
    private:
        std::string username;
        UserStatus status;
    
    public:
        Contact() : username(""), status(UserStatus::OFFLINE) {}
        Contact(std::string username, UserStatus status) 
            : username(std::move(username)), status(status) {}
        
        const std::string& getName() const { return username; }
        UserStatus getStatus() const { return status; }
        
        void setName(const std::string& name) { username = name; }
        void setStatus(UserStatus newStatus) { status = newStatus; }
        
        wxString getFormattedName() const {
            std::string statusIndicator;
            switch (status) {
                case UserStatus::ONLINE: statusIndicator = "[+] "; break;
                case UserStatus::BUSY: statusIndicator = "[!] "; break;
                case UserStatus::AWAY: statusIndicator = "[~] "; break;
                case UserStatus::OFFLINE: statusIndicator = "[-] "; break;
            }
            return wxString(statusIndicator + username);
        }
    };

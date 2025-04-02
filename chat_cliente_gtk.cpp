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

class ChatView;
class LoginView;
class MessengerApp;


class MessengerApp : public wxApp {
public:
    virtual bool OnInit() override;
};

wxIMPLEMENT_APP(MessengerApp);


class ChatView : public wxFrame {
public:
    ChatView(std::shared_ptr<websocket::stream<tcp::socket>> connection, const std::string& username);
    ~ChatView();

private:

    wxListBox* contactListBox;
    wxTextCtrl* chatHistoryDisplay;
    wxTextCtrl* messageInputField;
    wxButton* sendMessageButton;
    wxButton* addContactButton;
    wxButton* userInfoButton;
    wxButton* refreshButton;
    wxChoice* statusSelector;
    wxStaticText* chatTitleLabel;
    wxStaticText* statusDisplayLabel;
    
 
    std::shared_ptr<websocket::stream<tcp::socket>> connection;
    std::string currentUser;
    std::string activeChatPartner;
    bool isRunning;
    std::mutex chatDataMutex;
    UserStatus userCurrentStatus;

    std::unordered_map<std::string, Contact> contactDirectory;
    std::unordered_map<std::string, std::vector<std::string>> messageHistory;
    
    void onSendMessage(wxCommandEvent& event);
    void onAddContact(wxCommandEvent& event);
    void onContactSelected(wxCommandEvent& event);
    void onRequestUserInfo(wxCommandEvent& event);
    void onRefreshContacts(wxCommandEvent& event);
    void onStatusChanged(wxCommandEvent& event);
    
    void fetchUserList();
    void fetchChatHistory();
    void startMessageListener();
    bool checkConnection();
    bool reconnect();
    
    std::vector<uint8_t> createUserListRequest();
    std::vector<uint8_t> createUserInfoRequest(const std::string& username);
    std::vector<uint8_t> createStatusUpdateRequest(UserStatus newStatus);
    std::vector<uint8_t> createSendMessageRequest(const std::string& recipient, const std::string& message);
    std::vector<uint8_t> createHistoryRequest(const std::string& chatPartner);
    
    void handleErrorMessage(const std::vector<uint8_t>& messageData);
    void handleUserListMessage(const std::vector<uint8_t>& messageData);
    void handleUserInfoMessage(const std::vector<uint8_t>& messageData);
    void handleNewUserMessage(const std::vector<uint8_t>& messageData);
    void handleStatusChangeMessage(const std::vector<uint8_t>& messageData);
    void handleChatMessage(const std::vector<uint8_t>& messageData);
    void handleChatHistoryMessage(const std::vector<uint8_t>& messageData);
    
    void updateContactList();
    void updateStatusDisplay();
    bool canSendMessages() const;
    bool isConnected();
};


class LoginView : public wxFrame {
    public:
        LoginView();
    
    private:
        wxTextCtrl* usernameField;
        wxTextCtrl* serverAddressField;
        wxTextCtrl* serverPortField;
        wxStaticText* connectionStatusLabel;
    
        void onConnectButtonClicked(wxCommandEvent& event);
    };
    
    bool MessengerApp::OnInit() {
        LoginView* loginScreen = new LoginView();
        loginScreen->Show(true);
        return true;
    }
    
    LoginView::LoginView() 
        : wxFrame(nullptr, wxID_ANY, "Messenger Login", wxDefaultPosition, wxSize(400, 250)) {
        
        wxPanel* mainPanel = new wxPanel(this);
        wxBoxSizer* mainLayout = new wxBoxSizer(wxVERTICAL);
        
        wxBoxSizer* usernameLayout = new wxBoxSizer(wxHORIZONTAL);
        usernameLayout->Add(new wxStaticText(mainPanel, wxID_ANY, "Username:"), 
                           0, wxALL | wxALIGN_CENTER_VERTICAL, 10);
        usernameField = new wxTextCtrl(mainPanel, wxID_ANY);
        usernameLayout->Add(usernameField, 1, wxALL, 10);
        mainLayout->Add(usernameLayout, 0, wxEXPAND);
    
        wxBoxSizer* addressLayout = new wxBoxSizer(wxHORIZONTAL);
        addressLayout->Add(new wxStaticText(mainPanel, wxID_ANY, "Server Address:"), 
                         0, wxALL | wxALIGN_CENTER_VERTICAL, 10);
        serverAddressField = new wxTextCtrl(mainPanel, wxID_ANY, "3.13.27.172");
        addressLayout->Add(serverAddressField, 1, wxALL, 10);
        mainLayout->Add(addressLayout, 0, wxEXPAND);
    
        wxBoxSizer* portLayout = new wxBoxSizer(wxHORIZONTAL);
        portLayout->Add(new wxStaticText(mainPanel, wxID_ANY, "Server Port:"), 
                      0, wxALL | wxALIGN_CENTER_VERTICAL, 10);
        serverPortField = new wxTextCtrl(mainPanel, wxID_ANY, "3000");
        portLayout->Add(serverPortField, 1, wxALL, 10);
        mainLayout->Add(portLayout, 0, wxEXPAND);
    
        wxButton* connectButton = new wxButton(mainPanel, wxID_ANY, "Connect");
        mainLayout->Add(connectButton, 0, wxALL | wxCENTER, 10);
    
        connectionStatusLabel = new wxStaticText(mainPanel, wxID_ANY, "", 
                                              wxDefaultPosition, wxDefaultSize, wxALIGN_CENTRE_HORIZONTAL);
        connectionStatusLabel->SetForegroundColour(wxColour(255, 0, 0));
        mainLayout->Add(connectionStatusLabel, 0, wxALL | wxEXPAND, 10);
    
        connectButton->Bind(wxEVT_BUTTON, &LoginView::onConnectButtonClicked, this);
        
        mainPanel->SetSizer(mainLayout);
    }
    
    void LoginView::onConnectButtonClicked(wxCommandEvent&) {
        std::string username = usernameField->GetValue().ToStdString();
        std::string serverAddress = serverAddressField->GetValue().ToStdString();
        std::string serverPort = serverPortField->GetValue().ToStdString();
        
        if (username.empty()) {
            connectionStatusLabel->SetLabel("Error: Username cannot be empty");
            return;
        }
        
        if (username == "~") {
            connectionStatusLabel->SetLabel("Error: '~' is reserved for general chat");
            return;
        }
        
        if (serverAddress.empty()) {
            connectionStatusLabel->SetLabel("Error: Server address cannot be empty");
            return;
        }
        
        if (serverPort.empty()) {
            connectionStatusLabel->SetLabel("Error: Server port cannot be empty");
            return;
        }
        
        connectionStatusLabel->SetLabel("Connecting...");
    
        std::thread([this, username, serverAddress, serverPort]() {
            try {
                std::cout << "Connecting to " << serverAddress << ":" << serverPort << std::endl;
    
                net::io_context ioContext;
                tcp::resolver resolver(ioContext);
                auto endpoints = resolver.resolve(serverAddress, serverPort);
                
                std::cout << "Address resolved, connecting TCP socket..." << std::endl;
        
                tcp::socket socket(ioContext);
                net::connect(socket, endpoints);
                
                std::cout << "TCP socket connected, creating WebSocket stream..." << std::endl;
    
                auto wsConnection = std::make_shared<websocket::stream<tcp::socket>>(std::move(socket));
                wsConnection->set_option(websocket::stream_base::timeout::suggested(beast::role_type::client));
    
                std::string host = serverAddress;
                std::string target = "/?name=" + username;
                
                std::cout << "Starting WebSocket handshake with host=" << host 
                         << " and target=" << target << std::endl;
        
                wsConnection->handshake(host, target);
                std::cout << "WebSocket handshake successful!" << std::endl;
        
                wxGetApp().CallAfter([this, wsConnection, username]() {
                    ChatView* chatWindow = new ChatView(wsConnection, username);
                    chatWindow->Show(true);
                    Close();
                });
            } 
            catch (const beast::error_code& ec) {
                wxGetApp().CallAfter([this, ec]() {
                    std::string errorMsg = "Connection error: " + ec.message();
                    connectionStatusLabel->SetLabel("Error: " + errorMsg);
                    std::cerr << errorMsg << std::endl;
                });
            }
            catch (const std::exception& e) {
                wxGetApp().CallAfter([this, e]() {
                    std::string errorMsg = e.what();
                    connectionStatusLabel->SetLabel("Error: " + errorMsg);
                    std::cerr << "Exception: " << errorMsg << std::endl;
                });
            }
            catch (...) {
                wxGetApp().CallAfter([this]() {
                    connectionStatusLabel->SetLabel("Error: Unknown exception during connection");
                    std::cerr << "Unknown error during connection" << std::endl;
                });
            }
        }).detach();
    }
    
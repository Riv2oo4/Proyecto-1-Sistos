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

    
ChatView::ChatView(std::shared_ptr<websocket::stream<tcp::socket>> connection, const std::string& username)
: wxFrame(nullptr, wxID_ANY, "Messenger - " + username, wxDefaultPosition, wxSize(800, 600)), 
  connection(connection), 
  currentUser(username),
  isRunning(true),
  userCurrentStatus(UserStatus::ONLINE) {

contactDirectory.insert({"~", Contact("General Chat", UserStatus::ONLINE)});
contactDirectory.insert({username, Contact(username, UserStatus::ONLINE)});

wxPanel* mainPanel = new wxPanel(this);
wxBoxSizer* mainLayout = new wxBoxSizer(wxHORIZONTAL);

wxBoxSizer* leftPanel = new wxBoxSizer(wxVERTICAL);

wxBoxSizer* statusLayout = new wxBoxSizer(wxHORIZONTAL);
statusLayout->Add(new wxStaticText(mainPanel, wxID_ANY, "Status:"), 
                0, wxALIGN_CENTER_VERTICAL | wxALL, 5);

wxString statusOptions[] = {"Online", "Busy", "Away"};
statusSelector = new wxChoice(mainPanel, wxID_ANY, wxDefaultPosition, wxDefaultSize, 3, statusOptions);
statusSelector->SetSelection(0);
statusLayout->Add(statusSelector, 1, wxALL, 5);

leftPanel->Add(statusLayout, 0, wxEXPAND);

statusDisplayLabel = new wxStaticText(mainPanel, wxID_ANY, "Current status: ONLINE");
statusDisplayLabel->SetForegroundColour(wxColour(0, 128, 0));
leftPanel->Add(statusDisplayLabel, 0, wxALL, 5);

leftPanel->Add(new wxStaticText(mainPanel, wxID_ANY, "Contacts:"), 0, wxALL, 5);
contactListBox = new wxListBox(mainPanel, wxID_ANY);
leftPanel->Add(contactListBox, 1, wxALL | wxEXPAND, 5);

wxBoxSizer* contactButtonLayout = new wxBoxSizer(wxHORIZONTAL);

addContactButton = new wxButton(mainPanel, wxID_ANY, "Add");
contactButtonLayout->Add(addContactButton, 1, wxALL, 5);

userInfoButton = new wxButton(mainPanel, wxID_ANY, "Info");
contactButtonLayout->Add(userInfoButton, 1, wxALL, 5);

refreshButton = new wxButton(mainPanel, wxID_ANY, "Refresh");
contactButtonLayout->Add(refreshButton, 1, wxALL, 5);

leftPanel->Add(contactButtonLayout, 0, wxEXPAND);

wxBoxSizer* rightPanel = new wxBoxSizer(wxVERTICAL);

chatTitleLabel = new wxStaticText(mainPanel, wxID_ANY, "Chat with: [Select a contact]");
rightPanel->Add(chatTitleLabel, 0, wxALL, 5);

chatHistoryDisplay = new wxTextCtrl(mainPanel, wxID_ANY, "", wxDefaultPosition, wxDefaultSize, 
                                 wxTE_MULTILINE | wxTE_READONLY | wxTE_RICH2);
rightPanel->Add(chatHistoryDisplay, 1, wxALL | wxEXPAND, 5);

wxBoxSizer* messageInputLayout = new wxBoxSizer(wxHORIZONTAL);
messageInputField = new wxTextCtrl(mainPanel, wxID_ANY, "", wxDefaultPosition, wxDefaultSize, wxTE_PROCESS_ENTER);
messageInputLayout->Add(messageInputField, 1, wxALL, 5);

sendMessageButton = new wxButton(mainPanel, wxID_ANY, "Send");
messageInputLayout->Add(sendMessageButton, 0, wxALL, 5);

rightPanel->Add(messageInputLayout, 0, wxEXPAND);

mainLayout->Add(leftPanel, 1, wxEXPAND | wxALL, 10);
mainLayout->Add(rightPanel, 2, wxEXPAND | wxALL, 10);

sendMessageButton->Bind(wxEVT_BUTTON, &ChatView::onSendMessage, this);
messageInputField->Bind(wxEVT_TEXT_ENTER, &ChatView::onSendMessage, this);
addContactButton->Bind(wxEVT_BUTTON, &ChatView::onAddContact, this);
userInfoButton->Bind(wxEVT_BUTTON, &ChatView::onRequestUserInfo, this);
refreshButton->Bind(wxEVT_BUTTON, &ChatView::onRefreshContacts, this);
contactListBox->Bind(wxEVT_LISTBOX, &ChatView::onContactSelected, this);
statusSelector->Bind(wxEVT_CHOICE, &ChatView::onStatusChanged, this);

mainPanel->SetSizer(mainLayout);
mainLayout->Fit(this);

startMessageListener();
fetchUserList();
updateContactList();

contactListBox->SetSelection(contactListBox->FindString("[+] General Chat"));
activeChatPartner = "~";
chatTitleLabel->SetLabel("Chat with: General Chat");
}

ChatView::~ChatView() {
isRunning = false;
try {
    connection->close(websocket::close_code::normal);
} catch (...) {
}
}

void ChatView::fetchUserList() {
try {
    std::vector<uint8_t> request = createUserListRequest();
    connection->write(net::buffer(request));
} catch (const std::exception& e) {
    wxMessageBox("Error requesting user list: " + std::string(e.what()),
               "Error", wxOK | wxICON_ERROR);
}
}

void ChatView::fetchChatHistory() {
if (activeChatPartner.empty()) return;

try {
    std::vector<uint8_t> request = createHistoryRequest(activeChatPartner);
    connection->write(net::buffer(request));
} catch (const std::exception& e) {
    wxMessageBox("Error requesting chat history: " + std::string(e.what()),
               "Error", wxOK | wxICON_ERROR);
}
}

bool ChatView::canSendMessages() const {
return userCurrentStatus == UserStatus::ONLINE || userCurrentStatus == UserStatus::AWAY;
}

bool ChatView::isConnected() {
if (!connection) return false;

try {
    return connection->is_open() && connection->next_layer().is_open();
} catch (...) {
    return false;
}
}

void ChatView::onSendMessage(wxCommandEvent&) {
if (activeChatPartner.empty()) {
    wxMessageBox("Please select a contact first", "Notice", wxOK | wxICON_INFORMATION);
    return;
}

if (!canSendMessages()) {
    wxMessageBox("You can't send messages while BUSY or OFFLINE",
               "Notice", wxOK | wxICON_WARNING);
    return;
}

if (!checkConnection()) {
    return;  
}

std::string messageText = messageInputField->GetValue().ToStdString();
if (messageText.empty()) return;

try {
    std::vector<uint8_t> messageData = createSendMessageRequest(activeChatPartner, messageText);
    if (messageData.empty()) return; 

    try {
        connection->write(net::buffer(messageData));
        messageInputField->Clear();
    } catch (const std::exception& e) {
        if (reconnect()) {
            try {
                connection->write(net::buffer(messageData));
                messageInputField->Clear();
                wxMessageBox("Message sent after reconnecting", "Reconnection Successful", wxOK | wxICON_INFORMATION);
            } catch (const std::exception& e2) {
                wxMessageBox("Couldn't send message after reconnecting: " + std::string(e2.what()),
                           "Error", wxOK | wxICON_ERROR);
            }
        } else {
            wxMessageBox("Error sending message: " + std::string(e.what()),
                       "Error", wxOK | wxICON_ERROR);
        }
    }
} catch (const std::exception& e) {
    wxMessageBox("Error preparing message: " + std::string(e.what()),
               "Error", wxOK | wxICON_ERROR);
}
}

void ChatView::startMessageListener() {
std::thread([this]() {
    try {
        while (isRunning) {
            beast::flat_buffer buffer;
            connection->read(buffer);
            
            std::string dataStr = beast::buffers_to_string(buffer.data());
            std::vector<uint8_t> message(dataStr.begin(), dataStr.end());
            
            if (!message.empty()) {
                uint8_t messageType = message[0];
                
                switch (messageType) {
                    case MSG_SERVER_ERROR:
                        handleErrorMessage(message);
                        break;
                    case MSG_SERVER_USER_LIST:
                        handleUserListMessage(message);
                        break;
                    case MSG_SERVER_USER_INFO:
                        handleUserInfoMessage(message);
                        break;
                    case MSG_SERVER_USER_JOINED:
                        handleNewUserMessage(message);
                        break;
                    case MSG_SERVER_STATUS_UPDATE:
                        handleStatusChangeMessage(message);
                        break;
                    case MSG_SERVER_NEW_MESSAGE:
                        handleChatMessage(message);
                        break;
                    case MSG_SERVER_CHAT_HISTORY:
                        handleChatHistoryMessage(message);
                        break;
                    default:
                        break;
                }
            }
        }
    } catch (const beast::error_code& ec) {
        if (ec == websocket::error::closed) {
            wxGetApp().CallAfter([this]() {
                wxMessageBox("Connection closed by server", "Notice", wxOK | wxICON_INFORMATION);
                Close();
            });
        } else {
            wxGetApp().CallAfter([this, ec]() {
                wxMessageBox("Connection error: " + ec.message(),
                           "Error", wxOK | wxICON_ERROR);
                Close();
            });
        }
    } catch (const std::exception& e) {
        wxGetApp().CallAfter([this, e]() {
            wxMessageBox("Connection error: " + std::string(e.what()),
                       "Error", wxOK | wxICON_ERROR);
            Close();
        });
    }
}).detach();
}

void ChatView::onAddContact(wxCommandEvent&) {
wxTextEntryDialog dialog(this, "Enter contact username:",
                       "Add Contact", "");

if (dialog.ShowModal() == wxID_OK) {
    std::string contactName = dialog.GetValue().ToStdString();
    if (!contactName.empty()) {
        try {
            std::vector<uint8_t> request = createUserInfoRequest(contactName);
            connection->write(net::buffer(request));
        } catch (const std::exception& e) {
            wxMessageBox("Error requesting user information: " + std::string(e.what()),
                       "Error", wxOK | wxICON_ERROR);
        }
    }
}
}

void ChatView::onContactSelected(wxCommandEvent& evt) {
wxString selectedItem = contactListBox->GetString(evt.GetSelection());
wxString contactName = selectedItem.AfterFirst(']').Trim(true).Trim(false);

if (contactName == "General Chat") {
    activeChatPartner = "~";  
} else {
    activeChatPartner = contactName.ToStdString();
}

wxString titleText = wxString("Chat with: ") + 
                  (activeChatPartner == "~" ? wxString("General Chat") : wxString(activeChatPartner));
chatTitleLabel->SetLabel(titleText);

chatHistoryDisplay->Clear();
fetchChatHistory();
}

void ChatView::onRequestUserInfo(wxCommandEvent&) {
if (contactListBox->GetSelection() == wxNOT_FOUND) {
    wxMessageBox("Please select a user first", "Notice", wxOK | wxICON_INFORMATION);
    return;
}

if (!checkConnection()) {
    return; 
}

wxString selectedItem = contactListBox->GetString(contactListBox->GetSelection());
wxString contactName = selectedItem.AfterFirst(']').Trim(true).Trim(false);
std::string username = contactName.ToStdString();

if (username == "General Chat" || username == "~") {
    wxMessageBox("Cannot get information for general chat", "Notice", wxOK | wxICON_INFORMATION);
    return;
}

try {
    std::vector<uint8_t> request = createUserInfoRequest(username);
    connection->write(net::buffer(request));
} catch (const std::exception& e) {
    if (reconnect()) {
        try {
            std::vector<uint8_t> request = createUserInfoRequest(username);
            connection->write(net::buffer(request));
        } catch (const std::exception& e2) {
            wxMessageBox("Couldn't get user information after reconnecting: " + std::string(e2.what()),
                       "Error", wxOK | wxICON_ERROR);
        }
    } else {
        wxMessageBox("Error requesting user information: " + std::string(e.what()),
                   "Error", wxOK | wxICON_ERROR);
    }
}
}

void ChatView::onRefreshContacts(wxCommandEvent&) {
fetchUserList();
}

void ChatView::updateStatusDisplay() {
wxString statusString;
wxColour statusColor;

switch (userCurrentStatus) {
    case UserStatus::ONLINE:
        statusString = "ONLINE";
        statusColor = wxColour(0, 128, 0);  
        break;
    case UserStatus::BUSY:
        statusString = "BUSY";
        statusColor = wxColour(255, 0, 0);  
        break;
    case UserStatus::AWAY:
        statusString = "AWAY";
        statusColor = wxColour(128, 128, 0); 
        break;
    case UserStatus::OFFLINE:
        statusString = "OFFLINE";
        statusColor = wxColour(128, 128, 128); 
        break;
}

statusDisplayLabel->SetLabel("Current status: " + statusString);
statusDisplayLabel->SetForegroundColour(statusColor);

auto it = contactDirectory.find(currentUser);
if (it != contactDirectory.end()) {
    it->second.setStatus(userCurrentStatus);
}

updateContactList();
}

void ChatView::onStatusChanged(wxCommandEvent&) {
int selection = statusSelector->GetSelection();
UserStatus newStatus;

switch (selection) {
    case 0: newStatus = UserStatus::ONLINE; break;
    case 1: newStatus = UserStatus::BUSY; break;
    case 2: newStatus = UserStatus::AWAY; break;
    default: newStatus = UserStatus::ONLINE; break;
}

UserStatus previousStatus = userCurrentStatus;

try {
    std::vector<uint8_t> statusUpdate = createStatusUpdateRequest(newStatus);
    
    userCurrentStatus = newStatus;
    updateStatusDisplay();

    connection->write(net::buffer(statusUpdate));

    std::vector<uint8_t> refreshRequest = createUserListRequest();
    connection->write(net::buffer(refreshRequest));
    
    std::cout << "Status change message sent successfully" << std::endl;
    
} catch (const std::exception& e) {
    std::cerr << "Error changing status: " << e.what() << std::endl;

    userCurrentStatus = previousStatus;
    updateStatusDisplay();
    
    wxMessageBox("Error changing status: " + std::string(e.what()), 
               "Error", wxOK | wxICON_ERROR);
}
}

bool ChatView::reconnect() {
try {
    connection->close(websocket::close_code::normal);

    net::io_context ioContext;
    tcp::resolver resolver(ioContext);

    std::string serverAddress = connection->next_layer().remote_endpoint().address().to_string();
    unsigned short serverPort = connection->next_layer().remote_endpoint().port();
    
    auto endpoints = resolver.resolve(serverAddress, std::to_string(serverPort));
    
    tcp::socket socket(ioContext);
    net::connect(socket, endpoints);
    
    auto newConnection = std::make_shared<websocket::stream<tcp::socket>>(std::move(socket));
    newConnection->set_option(websocket::stream_base::timeout::suggested(beast::role_type::client));
    
    std::string host = serverAddress;
    std::string target = "/?name=" + currentUser;
    
    newConnection->handshake(host, target);

    connection = newConnection;
    
    fetchUserList();
    
    return true;
} catch (const std::exception& e) {
    std::cerr << "Error reconnecting: " << e.what() << std::endl;
    return false;
}
}

bool ChatView::checkConnection() {
if (!isConnected()) {
    bool reconnected = reconnect();
    if (reconnected) {
        wxMessageBox("Connection successfully reestablished.", 
                   "Reconnection", wxOK | wxICON_INFORMATION);
        return true;
    } else {
        wxMessageBox("Could not reestablish connection to server.", 
                   "Connection Error", wxOK | wxICON_ERROR);
        return false;
    }
}
return true;
}

std::vector<uint8_t> ChatView::createUserListRequest() {
    return {MSG_CLIENT_REQUEST_USERS};
}

std::vector<uint8_t> ChatView::createUserInfoRequest(const std::string& username) {
    std::vector<uint8_t> message = {MSG_CLIENT_GET_USER_INFO, static_cast<uint8_t>(username.size())};
    message.insert(message.end(), username.begin(), username.end());
    return message;
}

std::vector<uint8_t> ChatView::createStatusUpdateRequest(UserStatus newStatus) {
    std::vector<uint8_t> message = {
        MSG_CLIENT_UPDATE_STATUS, 
        static_cast<uint8_t>(currentUser.size())
    };
    message.insert(message.end(), currentUser.begin(), currentUser.end());
    message.push_back(static_cast<uint8_t>(newStatus));
    return message;
}

std::vector<uint8_t> ChatView::createSendMessageRequest(const std::string& recipient, const std::string& message) {
    if (message.size() > 255) {
        wxMessageBox("Message is too long (maximum 255 characters)", 
                    "Notice", wxOK | wxICON_WARNING);
        return {};
    }
    
    try {
        std::vector<uint8_t> data = {
            MSG_CLIENT_SEND_MESSAGE, 
            static_cast<uint8_t>(recipient.size())
        };
        data.insert(data.end(), recipient.begin(), recipient.end());
        data.push_back(static_cast<uint8_t>(message.size()));
        data.insert(data.end(), message.begin(), message.end());
        return data;
    } catch (const std::exception& e) {
        wxMessageBox("Error creating message: " + std::string(e.what()), 
                   "Error", wxOK | wxICON_ERROR);
        return {};
    }
}

std::vector<uint8_t> ChatView::createHistoryRequest(const std::string& chatPartner) {
    std::vector<uint8_t> message = {MSG_CLIENT_REQUEST_HISTORY, static_cast<uint8_t>(chatPartner.size())};
    message.insert(message.end(), chatPartner.begin(), chatPartner.end());
    return message;
}
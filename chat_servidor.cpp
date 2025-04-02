#include <boost/asio.hpp>
#include <boost/beast.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/websocket.hpp>
#include <boost/algorithm/string.hpp>
#include <chrono>
#include <ctime>
#include <deque>
#include <fstream>
#include <functional>
#include <iomanip>
#include <iostream>
#include <memory>
#include <mutex>
#include <sstream>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

namespace io = boost::asio;
namespace web = boost::beast;
namespace http = web::http;
namespace ws = web::websocket;
using tcp = io::ip::tcp;

// Protocol enumerations
namespace protocol {
    enum ClientRequest : uint8_t {
        GET_PARTICIPANTS = 1,
        PARTICIPANT_INFO = 2,
        SET_AVAILABILITY = 3,
        SEND_COMMUNICATION = 4,
        FETCH_COMMUNICATIONS = 5
    };

    enum ServerResponse : uint8_t {
        FAILURE = 50,
        PARTICIPANT_LIST = 51,
        PARTICIPANT_DETAILS = 52, 
        PARTICIPANT_JOINED = 53,
        AVAILABILITY_UPDATE = 54,
        COMMUNICATION = 55,
        COMMUNICATION_HISTORY = 56
    };

    enum FailureReason : uint8_t {
        PARTICIPANT_UNKNOWN = 1,
        INVALID_AVAILABILITY = 2,
        COMMUNICATION_EMPTY = 3,
        PARTICIPANT_UNAVAILABLE = 4
    };

    enum Availability : uint8_t {
        OFFLINE = 0,
        AVAILABLE = 1,
        BUSY = 2,
        AWAY = 3
    };
}

// Logging facility
class SystemLogger {
private:
    std::mutex mutex_;
    std::ofstream file_;
    bool console_output_{true};

public:
    explicit SystemLogger(const std::string& filename) {
        file_.open(filename, std::ios::app);
        if (!file_.is_open()) {
            std::cerr << "Failed to open log file: " << filename << std::endl;
        }
    }

    ~SystemLogger() {
        if (file_.is_open()) {
            file_.close();
        }
    }

    void record(const std::string& entry) {
        std::lock_guard<std::mutex> lock(mutex_);
        
        auto now = std::chrono::system_clock::now();
        auto now_time_t = std::chrono::system_clock::to_time_t(now);
        
        std::stringstream formatted_entry;
        formatted_entry << "[" << std::put_time(std::localtime(&now_time_t), "%Y-%m-%d %H:%M:%S") 
                       << "] " << entry;
        
        if (file_.is_open()) {
            file_ << formatted_entry.str() << std::endl;
        }
        
        if (console_output_) {
            std::cout << formatted_entry.str() << std::endl;
        }
    }

    void set_console_output(bool enabled) {
        console_output_ = enabled;
    }
};

// Communication record
struct Communication {
    std::string sender;
    std::string recipient;
    std::string content;
    std::chrono::system_clock::time_point timestamp;
    
    Communication(std::string s, std::string r, std::string c)
        : sender(std::move(s)), 
          recipient(std::move(r)), 
          content(std::move(c)),
          timestamp(std::chrono::system_clock::now()) {}
};

// Forward declarations
class ParticipantRegistry;
class CommunicationRepository;
class RequestHandler;
class ConnectionHandler;
class ProtocolUtils;

// System participant
class Participant {
public:
    std::string identifier;
    protocol::Availability availability;
    std::shared_ptr<ws::stream<tcp::socket>> connection;
    std::deque<Communication> personal_history;
    std::chrono::system_clock::time_point last_activity;
    io::ip::address network_address;
    
    Participant(std::string id, std::shared_ptr<ws::stream<tcp::socket>> conn, 
                io::ip::address addr)
        : identifier(std::move(id)), 
          availability(protocol::Availability::AVAILABLE), 
          connection(std::move(conn)),
          last_activity(std::chrono::system_clock::now()),
          network_address(std::move(addr)) {}
    
    bool is_available() const {
        return availability == protocol::Availability::AVAILABLE;
    }
    
    bool can_receive_communications() const {
        return availability != protocol::Availability::OFFLINE && 
               availability != protocol::Availability::BUSY;
    }
    
    void update_last_activity() {
        last_activity = std::chrono::system_clock::now();
    }
};

// Protocol utilities
class ProtocolUtils {
public:
    static std::vector<uint8_t> create_error_response(protocol::FailureReason reason) {
        return {protocol::ServerResponse::FAILURE, static_cast<uint8_t>(reason)};
    }
    
    static std::vector<uint8_t> create_participant_list(const std::vector<std::shared_ptr<Participant>>& participants) {
        uint8_t count = static_cast<uint8_t>(std::min(participants.size(), static_cast<size_t>(255)));
        
        std::vector<uint8_t> response = {protocol::ServerResponse::PARTICIPANT_LIST, count};
        
        for (size_t i = 0; i < count; i++) {
            const auto& participant = participants[i];
            
            uint8_t id_length = static_cast<uint8_t>(participant->identifier.size());
            response.push_back(id_length);
            response.insert(response.end(), participant->identifier.begin(), participant->identifier.end());
            response.push_back(static_cast<uint8_t>(participant->availability));
        }
        
        return response;
    }
    
    static std::vector<uint8_t> create_participant_details(const std::shared_ptr<Participant>& participant) {
        if (!participant) {
            return create_error_response(protocol::FailureReason::PARTICIPANT_UNKNOWN);
        }
        
        std::vector<uint8_t> response = {
            protocol::ServerResponse::PARTICIPANT_DETAILS, 
            static_cast<uint8_t>(participant->identifier.size())
        };
        
        response.insert(response.end(), participant->identifier.begin(), participant->identifier.end());
        response.push_back(static_cast<uint8_t>(participant->availability));
        
        return response;
    }
    
    static std::vector<uint8_t> create_availability_update(const std::string& participant_id, 
                                                          protocol::Availability status) {
        std::vector<uint8_t> response = {
            protocol::ServerResponse::AVAILABILITY_UPDATE,
            static_cast<uint8_t>(participant_id.size())
        };
        
        response.insert(response.end(), participant_id.begin(), participant_id.end());
        response.push_back(static_cast<uint8_t>(status));
        
        return response;
    }
    
    static std::vector<uint8_t> create_new_participant_notification(const std::string& participant_id) {
        std::vector<uint8_t> response = {
            protocol::ServerResponse::PARTICIPANT_JOINED,
            static_cast<uint8_t>(participant_id.size())
        };
        
        response.insert(response.end(), participant_id.begin(), participant_id.end());
        response.push_back(static_cast<uint8_t>(protocol::Availability::AVAILABLE));
        
        return response;
    }
    
    static std::vector<uint8_t> create_communication_message(const std::string& sender, 
                                                           const std::string& content) {
        std::vector<uint8_t> response = {
            protocol::ServerResponse::COMMUNICATION,
            static_cast<uint8_t>(sender.size())
        };
        
        response.insert(response.end(), sender.begin(), sender.end());
        
        uint8_t content_size = static_cast<uint8_t>(std::min(content.size(), static_cast<size_t>(255)));
        response.push_back(content_size);
        
        auto content_begin = content.begin();
        auto content_end = content_size < content.size() ? content_begin + content_size : content.end();
        
        response.insert(response.end(), content_begin, content_end);
        
        return response;
    }
    
    static std::vector<uint8_t> create_history_response(const std::vector<Communication>& history) {
        uint8_t count = static_cast<uint8_t>(std::min(history.size(), static_cast<size_t>(255)));
        
        std::vector<uint8_t> response = {protocol::ServerResponse::COMMUNICATION_HISTORY, count};
        
        for (size_t i = 0; i < count; i++) {
            const auto& comm = history[i];
            
            uint8_t sender_size = static_cast<uint8_t>(comm.sender.size());
            response.push_back(sender_size);
            response.insert(response.end(), comm.sender.begin(), comm.sender.end());
            
            uint8_t content_size = static_cast<uint8_t>(std::min(comm.content.size(), static_cast<size_t>(255)));
            response.push_back(content_size);
            
            auto content_begin = comm.content.begin();
            auto content_end = content_size < comm.content.size() ? content_begin + content_size : comm.content.end();
            
            response.insert(response.end(), content_begin, content_end);
        }
        
        return response;
    }
    
    static std::string parse_query_parameter(const std::string& query_string, const std::string& param_name) {
        std::string value;
        
        size_t pos = query_string.find(param_name + "=");
        if (pos != std::string::npos) {
            value = query_string.substr(pos + param_name.size() + 1);
            
            pos = value.find('&');
            if (pos != std::string::npos) {
                value = value.substr(0, pos);
            }
            
            boost::replace_all(value, "%20", " ");
        }
        
        return value;
    }
};

// Registry of all participants
class ParticipantRegistry {
private:
    std::unordered_map<std::string, std::shared_ptr<Participant>> participants_;
    std::mutex mutex_;
    SystemLogger& logger_;

public:
    explicit ParticipantRegistry(SystemLogger& logger) : logger_(logger) {}

    std::mutex& get_mutex() {
        return mutex_;
    }
    
    bool register_participant(const std::string& id, 
                              std::shared_ptr<ws::stream<tcp::socket>> conn,
                              io::ip::address addr) {
        std::lock_guard<std::mutex> lock(mutex_);
        
        auto it = participants_.find(id);
        if (it != participants_.end()) {
            if (it->second->availability != protocol::Availability::OFFLINE) {
                return false;
            }
            
            it->second->connection = conn;
            it->second->availability = protocol::Availability::AVAILABLE;
            it->second->update_last_activity();
            it->second->network_address = addr;
        } else {
            participants_[id] = std::make_shared<Participant>(id, conn, addr);
        }
        
        return true;
    }
    
    std::shared_ptr<Participant> get_participant(const std::string& id) {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = participants_.find(id);
        if (it != participants_.end()) {
            return it->second;
        }
        return nullptr;
    }
    
    bool set_availability(const std::string& id, protocol::Availability status) {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = participants_.find(id);
        if (it != participants_.end()) {
            it->second->availability = status;
            it->second->update_last_activity();
            return true;
        }
        return false;
    }
    
    std::vector<std::shared_ptr<Participant>> get_all_participants() {
        std::lock_guard<std::mutex> lock(mutex_);
        std::vector<std::shared_ptr<Participant>> result;
        
        for (const auto& [id, participant] : participants_) {
            if (participant->availability != protocol::Availability::OFFLINE) {
                result.push_back(participant);
            }
        }
        
        return result;
    }
    
    void broadcast(const std::vector<uint8_t>& message) {
        std::lock_guard<std::mutex> lock(mutex_);
        
        for (auto& [id, participant] : participants_) {
            if (participant->availability != protocol::Availability::OFFLINE) {
                try {
                    participant->connection->write(io::buffer(message));
                } catch (const std::exception& e) {
                    logger_.record("Failed to broadcast to " + id + ": " + e.what());
                }
            }
        }
    }
};

// Central communication repository
class CommunicationRepository {
private:
    std::deque<Communication> public_communications_;
    std::mutex mutex_;
    static constexpr size_t MAX_HISTORY_SIZE = 1000;

public:
    void add_public_communication(const Communication& comm) {
        std::lock_guard<std::mutex> lock(mutex_);
        public_communications_.push_back(comm);
        
        if (public_communications_.size() > MAX_HISTORY_SIZE) {
            public_communications_.pop_front();
        }
    }
    
    void add_private_communication(const Communication& comm, 
                                   std::shared_ptr<Participant> sender,
                                   std::shared_ptr<Participant> recipient) {
        // Add to sender's history
        if (sender) {
            sender->personal_history.push_back(comm);
            if (sender->personal_history.size() > MAX_HISTORY_SIZE) {
                sender->personal_history.pop_front();
            }
        }
        
        // Add to recipient's history
        if (recipient) {
            recipient->personal_history.push_back(comm);
            if (recipient->personal_history.size() > MAX_HISTORY_SIZE) {
                recipient->personal_history.pop_front();
            }
        }
    }
    
    std::vector<Communication> get_public_history(size_t max_count = 255) {
        std::lock_guard<std::mutex> lock(mutex_);
        
        std::vector<Communication> result;
        size_t count = std::min(public_communications_.size(), max_count);
        
        if (count > 0) {
            auto start = public_communications_.end() - count;
            for (size_t i = 0; i < count; i++) {
                result.push_back(*(start + i));
            }
        }
        
        return result;
    }
    
    std::vector<Communication> get_private_history(std::shared_ptr<Participant> participant, 
                                                  size_t max_count = 255) {
        std::vector<Communication> result;
        
        if (!participant) {
            return result;
        }
        
        size_t count = std::min(participant->personal_history.size(), max_count);
        
        if (count > 0) {
            auto start = participant->personal_history.end() - count;
            for (size_t i = 0; i < count; i++) {
                result.push_back(*(start + i));
            }
        }
        
        return result;
    }
};

// Activity monitor
class ActivityMonitor {
    private:
        ParticipantRegistry& registry_;
        SystemLogger& logger_;
        std::chrono::seconds inactivity_timeout_;
        std::atomic<bool> running_;
        std::thread monitor_thread_;
        
    public:
        ActivityMonitor(ParticipantRegistry& registry, SystemLogger& logger, 
                       std::chrono::seconds timeout = std::chrono::seconds(60))
            : registry_(registry), logger_(logger), inactivity_timeout_(timeout), running_(true) {
            
            monitor_thread_ = std::thread([this]() {
                this->monitor_loop();
            });
            
            monitor_thread_.detach();
        }
        
        ~ActivityMonitor() {
            running_ = false;
        }
        
        void set_timeout(std::chrono::seconds timeout) {
            inactivity_timeout_ = timeout;
            logger_.record("Inactivity timeout set to " + std::to_string(timeout.count()) + " seconds");
        }
        
    private:
        void monitor_loop() {
            while (running_) {
                std::this_thread::sleep_for(std::chrono::seconds(10));
                
                auto now = std::chrono::system_clock::now();
                auto participants = registry_.get_all_participants();
                
                for (const auto& participant : participants) {
                    if (participant->availability == protocol::Availability::AVAILABLE) {
                        auto inactive_time = std::chrono::duration_cast<std::chrono::seconds>(
                            now - participant->last_activity);
                        
                        if (inactive_time > inactivity_timeout_) {
                            registry_.set_availability(participant->identifier, protocol::Availability::AWAY);
                            
                            logger_.record("Participant " + participant->identifier + 
                                          " set to AWAY due to inactivity");
                            
                            auto notification = ProtocolUtils::create_availability_update(
                                participant->identifier, protocol::Availability::AWAY);
                            
                            registry_.broadcast(notification);
                        }
                    }
                }
            }
        }
    };
     


// Request handler
class RequestHandler {
private:
    ParticipantRegistry& registry_;
    CommunicationRepository& repository_;
    SystemLogger& logger_;
    
public:
    RequestHandler(ParticipantRegistry& registry, 
                  CommunicationRepository& repository,
                  SystemLogger& logger)
        : registry_(registry), repository_(repository), logger_(logger) {}
    
    void handle_get_participants(const std::string& requester) {
        logger_.record("Participant " + requester + " requests participant list");
        
        auto participants = registry_.get_all_participants();
        auto response = ProtocolUtils::create_participant_list(participants);
        
        auto requester_participant = registry_.get_participant(requester);
        if (requester_participant) {
            try {
                requester_participant->connection->write(io::buffer(response));
            } catch (const std::exception& e) {
                logger_.record("Failed to send participant list to " + requester + ": " + e.what());
            }
        }
    }
    
    void handle_participant_info(const std::string& requester, const std::vector<uint8_t>& data) {
        if (data.size() < 2) {
            auto error = ProtocolUtils::create_error_response(protocol::FailureReason::PARTICIPANT_UNKNOWN);
            send_to_participant(requester, error);
            return;
        }
        
        uint8_t id_length = data[1];
        if (data.size() < 2 + id_length) {
            auto error = ProtocolUtils::create_error_response(protocol::FailureReason::PARTICIPANT_UNKNOWN);
            send_to_participant(requester, error);
            return;
        }
        
        std::string target_id(data.begin() + 2, data.begin() + 2 + id_length);
        logger_.record("Participant " + requester + " requests info for " + target_id);
        
        auto target = registry_.get_participant(target_id);
        auto response = ProtocolUtils::create_participant_details(target);
        
        send_to_participant(requester, response);
    }
    
    void handle_set_availability(const std::string& requester, const std::vector<uint8_t>& data) {
        if (data.size() < 3) {
            auto error = ProtocolUtils::create_error_response(protocol::FailureReason::INVALID_AVAILABILITY);
            send_to_participant(requester, error);
            return;
        }
        
        uint8_t id_length = data[1];
        if (data.size() < 2 + id_length + 1) {
            auto error = ProtocolUtils::create_error_response(protocol::FailureReason::INVALID_AVAILABILITY);
            send_to_participant(requester, error);
            return;
        }
        
        std::string target_id(data.begin() + 2, data.begin() + 2 + id_length);
        uint8_t status = data[2 + id_length];
        
        if (status > 3) {
            auto error = ProtocolUtils::create_error_response(protocol::FailureReason::INVALID_AVAILABILITY);
            send_to_participant(requester, error);
            return;
        }
        
        logger_.record("Participant " + requester + " requests availability change for " + 
                      target_id + " to " + std::to_string(status));
        
        if (requester != target_id) {
            auto error = ProtocolUtils::create_error_response(protocol::FailureReason::PARTICIPANT_UNKNOWN);
            send_to_participant(requester, error);
            return;
        }
        
        auto target = registry_.get_participant(target_id);
        if (!target || target->availability == protocol::Availability::OFFLINE) {
            auto error = ProtocolUtils::create_error_response(protocol::FailureReason::PARTICIPANT_UNKNOWN);
            send_to_participant(requester, error);
            return;
        }
        
        registry_.set_availability(target_id, static_cast<protocol::Availability>(status));
        
        auto notification = ProtocolUtils::create_availability_update(target_id, static_cast<protocol::Availability>(status));
        registry_.broadcast(notification);
    }
    
    void handle_send_communication(const std::string& sender, const std::vector<uint8_t>& data) {
        if (data.size() < 2) {
            auto error = ProtocolUtils::create_error_response(protocol::FailureReason::COMMUNICATION_EMPTY);
            send_to_participant(sender, error);
            return;
        }
        
        uint8_t recipient_length = data[1];
        if (data.size() < 2 + recipient_length + 1) {
            auto error = ProtocolUtils::create_error_response(protocol::FailureReason::COMMUNICATION_EMPTY);
            send_to_participant(sender, error);
            return;
        }
        
        std::string recipient(data.begin() + 2, data.begin() + 2 + recipient_length);
        
        uint8_t content_length = data[2 + recipient_length];
        if (data.size() < 3 + recipient_length + content_length) {
            auto error = ProtocolUtils::create_error_response(protocol::FailureReason::COMMUNICATION_EMPTY);
            send_to_participant(sender, error);
            return;
        }
        
        std::string content(data.begin() + 3 + recipient_length, data.begin() + 3 + recipient_length + content_length);
        
        if (content.empty()) {
            auto error = ProtocolUtils::create_error_response(protocol::FailureReason::COMMUNICATION_EMPTY);
            send_to_participant(sender, error);
            return;
        }
        
        logger_.record("Participant " + sender + " sends communication to " + recipient + ": " + content);
        
        auto sender_participant = registry_.get_participant(sender);
        if (sender_participant) {
            sender_participant->update_last_activity();
        }
        
        auto response = ProtocolUtils::create_communication_message(sender, content);
        
        if (recipient == "~") {  // Public communication
            Communication comm(sender, recipient, content);
            repository_.add_public_communication(comm);
            
            registry_.broadcast(response);
        } else {  // Private communication
            auto recipient_participant = registry_.get_participant(recipient);
            
            if (!recipient_participant || recipient_participant->availability == protocol::Availability::OFFLINE) {
                auto error = ProtocolUtils::create_error_response(protocol::FailureReason::PARTICIPANT_UNAVAILABLE);
                send_to_participant(sender, error);
                return;
            }
            
            Communication comm(sender, recipient, content);
            repository_.add_private_communication(comm, sender_participant, recipient_participant);
            
            bool delivered = false;
            
            if (recipient_participant->can_receive_communications()) {
                try {
                    recipient_participant->connection->write(io::buffer(response));
                    delivered = true;
                } catch (const std::exception& e) {
                    logger_.record("Failed to deliver communication to " + recipient + ": " + e.what());
                }
            }
            
            // Always send confirmation to sender
            try {
                sender_participant->connection->write(io::buffer(response));
            } catch (const std::exception& e) {
                logger_.record("Failed to send confirmation to " + sender + ": " + e.what());
            }
            
            logger_.record("Communication from " + sender + " to " + recipient + 
                          (delivered ? " delivered" : " not delivered (recipient busy or away)"));
        }
    }
    
    void handle_fetch_communications(const std::string& requester, const std::vector<uint8_t>& data) {
        if (data.size() < 2) {
            auto error = ProtocolUtils::create_error_response(protocol::FailureReason::PARTICIPANT_UNKNOWN);
            send_to_participant(requester, error);
            return;
        }
        
        uint8_t channel_length = data[1];
        if (data.size() < 2 + channel_length) {
            auto error = ProtocolUtils::create_error_response(protocol::FailureReason::PARTICIPANT_UNKNOWN);
            send_to_participant(requester, error);
            return;
        }
        
        std::string channel(data.begin() + 2, data.begin() + 2 + channel_length);
        logger_.record("Participant " + requester + " requests communications for channel " + channel);
        
        std::vector<Communication> history;
        
        if (channel == "~") {  // Public communications
            history = repository_.get_public_history();
        } else {  // Private communications
            auto participant = registry_.get_participant(channel);
            
            if (!participant) {
                auto error = ProtocolUtils::create_error_response(protocol::FailureReason::PARTICIPANT_UNKNOWN);
                send_to_participant(requester, error);
                return;
            }
            
            history = repository_.get_private_history(participant);
        }
        
        auto response = ProtocolUtils::create_history_response(history);
        send_to_participant(requester, response);
    }
    
private:
    void send_to_participant(const std::string& participant_id, const std::vector<uint8_t>& message) {
        auto participant = registry_.get_participant(participant_id);
        if (participant) {
            try {
                participant->connection->write(io::buffer(message));
            } catch (const std::exception& e) {
                logger_.record("Failed to send message to " + participant_id + ": " + e.what());
            }
        }
    }
};

// Connection handler
class ConnectionHandler {
    private:
        tcp::socket socket_;
        std::string participant_id_;
        ParticipantRegistry& registry_;
        RequestHandler& request_handler_;
        SystemLogger& logger_;
        
    public:
        ConnectionHandler(tcp::socket socket, 
                         ParticipantRegistry& registry,
                         RequestHandler& request_handler,
                         SystemLogger& logger)
            : socket_(std::move(socket)), 
              registry_(registry),
              request_handler_(request_handler),
              logger_(logger) {}
        
        void process() {
            try {
                web::flat_buffer buffer;
                http::request<http::string_body> req;
                
                http::read(socket_, buffer, req);
                
                std::string query_string = extract_query_string(req.target());
                participant_id_ = ProtocolUtils::parse_query_parameter(query_string, "name");
                
                if (participant_id_.empty()) {
                    reject_connection("Empty participant identifier");
                    return;
                }
                
                if (participant_id_ == "~") {
                    reject_connection("Reserved participant identifier");
                    return;
                }
                
                // Check if participant is already connected
                if (!registry_.register_participant(participant_id_, nullptr, 
                                                  socket_.remote_endpoint().address())) {
                    reject_connection("Participant already connected");
                    return;
                }
                
                auto ws = std::make_shared<ws::stream<tcp::socket>>(std::move(socket_));
                ws->set_option(ws::stream_base::timeout::suggested(web::role_type::server));
                
                try {
                    ws->accept(req);
                    logger_.record("WebSocket connection accepted for: " + participant_id_);
                    logger_.record("Nuevo cliente conectando desde IP: " + 
                        socket_.remote_endpoint().address().to_string() + 
                        " con ID: " + participant_id_);         
                } catch (const std::exception& e) {
                    logger_.record("WebSocket handshake failed for " + participant_id_ + ": " + e.what());
                    return;
                }
                
                // Update registry with WebSocket connection
                registry_.register_participant(participant_id_, ws, ws->next_layer().remote_endpoint().address());
                
                // Notify all participants about new connection
                auto notification_join = ProtocolUtils::create_new_participant_notification(participant_id_);
                registry_.broadcast(notification_join);
                
                // Handle messages
                web::flat_buffer msg_buffer;
                
                while (true) {
                    try {
                        ws->read(msg_buffer);
                        
                        auto data = web::buffers_to_string(msg_buffer.data());
                        std::vector<uint8_t> binary_data(data.begin(), data.end());
                        msg_buffer.consume(msg_buffer.size());
                        
                        if (binary_data.empty()) {
                            continue;
                        }
                        
                        handle_client_message(binary_data);
                        
                    } catch (const web::error_code& ec) {
                        if (ec == ws::error::closed) {
                            logger_.record("Connection closed by participant: " + participant_id_);
                            break;
                        } else {
                            logger_.record("Error reading from participant " + participant_id_ + ": " + ec.message());
                            break;
                        }
                    } catch (const std::exception& e) {
                        logger_.record("Error processing message from " + participant_id_ + ": " + e.what());
                        break;
                    }
                }
                
                // Set participant as offline
                registry_.set_availability(participant_id_, protocol::Availability::OFFLINE);
                logger_.record("Participant " + participant_id_ + " marked as OFFLINE");
                
                // Notify all other participants
                auto notification_offline = ProtocolUtils::create_availability_update(
                    participant_id_, protocol::Availability::OFFLINE);
                registry_.broadcast(notification_offline);
                
            } catch (const std::exception& e) {
                logger_.record("Connection handling error: " + std::string(e.what()));
            }
        }
        
    private:
        void reject_connection(const std::string& reason) {
            http::response<http::string_body> res{http::status::bad_request, 11};
            res.set(http::field::server, "MessagingSystem");
            res.set(http::field::content_type, "text/plain");
            res.body() = reason;
            res.prepare_payload();
            
            http::write(socket_, res);
            logger_.record("Connection rejected for " + participant_id_ + ": " + reason);
        }
        
        std::string extract_query_string(boost::beast::string_view target) {
            auto pos = target.find('?');
            if (pos != boost::beast::string_view::npos) {
                return std::string(target.substr(pos + 1));
            }
            return "";
        }
        
        void handle_client_message(const std::vector<uint8_t>& data) {
            if (data.empty()) {
                return;
            }
            
            switch (data[0]) {
                case protocol::ClientRequest::GET_PARTICIPANTS:
                    request_handler_.handle_get_participants(participant_id_);
                    break;
                    
                case protocol::ClientRequest::PARTICIPANT_INFO:
                    request_handler_.handle_participant_info(participant_id_, data);
                    break;
                    
                case protocol::ClientRequest::SET_AVAILABILITY:
                    request_handler_.handle_set_availability(participant_id_, data);
                    break;
                    
                case protocol::ClientRequest::SEND_COMMUNICATION:
                    request_handler_.handle_send_communication(participant_id_, data);
                    break;
                    
                case protocol::ClientRequest::FETCH_COMMUNICATIONS:
                    request_handler_.handle_fetch_communications(participant_id_, data);
                    break;
                    
                default:
                    logger_.record("Unknown message type from " + participant_id_ + ": " + 
                                  std::to_string(data[0]));
                    break;
            }
        }
    };

// Main system class
class MessageSystem {
private:
    io::io_context io_context_;
    tcp::acceptor acceptor_;
    ParticipantRegistry registry_;
    CommunicationRepository repository_;
    RequestHandler request_handler_;
    ActivityMonitor activity_monitor_;
    SystemLogger logger_;
    
public:
    MessageSystem(unsigned short port, const std::string& log_file = "messaging_system.log")
        : io_context_(1),
          acceptor_(io_context_, {tcp::v4(), port}),
          registry_(logger_),
          repository_(),
          request_handler_(registry_, repository_, logger_),
          logger_(log_file),
          activity_monitor_(registry_, logger_) {
        
        acceptor_.set_option(io::socket_base::reuse_address(true));
        logger_.record("System initialized on port " + std::to_string(port));
    }
    
    void set_inactivity_timeout(int seconds) {
        activity_monitor_.set_timeout(std::chrono::seconds(seconds));
    }
    
    void run() {
        logger_.record("System Running...");
        
        while (true) {
            tcp::socket socket{io_context_};
            acceptor_.accept(socket);
            
            std::string remote_address = socket.remote_endpoint().address().to_string();
            unsigned short remote_port = socket.remote_endpoint().port();
            
            logger_.record("New connection from " + remote_address + ":" + std::to_string(remote_port));
            
            socket.set_option(tcp::socket::keep_alive(true));
            
            std::thread([this, sock = std::move(socket)]() mutable {
                ConnectionHandler handler(std::move(sock), registry_, request_handler_, logger_);
                handler.process();
            }).detach();
        }
    }
};


// Entry point
int main(int argc, char* argv[]) {
    try {
        if (argc != 2) {
            std::cerr << "Usage: " << argv[0] << " <port>" << std::endl;
            return 1;
        }
        
        unsigned short port = static_cast<unsigned short>(std::stoi(argv[1]));
        
        MessageSystem system(port);
        system.set_inactivity_timeout(120);
        
        std::cout << "Messaging system running on port " << port << std::endl;
        system.run();
        
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}
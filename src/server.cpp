#include "server.h"
#include "imgui.h"
#include <websocketpp/config/asio_no_tls.hpp>
#include <websocketpp/server.hpp>

#include <iostream>
#include <set>
#include <websocketpp/common/thread.hpp>
#include <unordered_map>
using namespace std;

typedef websocketpp::server<websocketpp::config::asio> server;

using websocketpp::connection_hdl;
using websocketpp::lib::placeholders::_1;
using websocketpp::lib::placeholders::_2;
using websocketpp::lib::bind;

using websocketpp::lib::thread;
using websocketpp::lib::mutex;
using websocketpp::lib::lock_guard;
using websocketpp::lib::unique_lock;
using websocketpp::lib::condition_variable;

/* on_open insert connection_hdl into channel
 * on_close remove connection_hdl from channel
 * on_message queue send to all channels
 */

int partyCount = 0xDEAD;

enum action_type {
    SUBSCRIBE,
    UNSUBSCRIBE,
    MESSAGE
};

struct action {
    action(action_type t, connection_hdl h) : type(t), hdl(h) {}
    action(action_type t, connection_hdl h, server::message_ptr m)
      : type(t), hdl(h), msg(m) {}

    action_type type;
    websocketpp::connection_hdl hdl;
    server::message_ptr msg;
};

class broadcast_server {
public:
    broadcast_server() {
        // Initialize Asio Transport
        m_server.init_asio();

        // Register handler callbacks
        m_server.set_open_handler(bind(&broadcast_server::on_open,this,::_1));
        m_server.set_close_handler(bind(&broadcast_server::on_close,this,::_1));
        m_server.set_message_handler(bind(&broadcast_server::on_message,this,::_1,::_2));
    }

    void run(uint16_t port) {
        // listen on specified port
        m_server.listen(port);

        // Start the server accept loop
        m_server.start_accept();

        // Start the ASIO io_service run loop
        try {
            m_server.run();
        } catch (const std::exception & e) {
            std::cout << e.what() << std::endl;
        }
    }

    void on_open(connection_hdl hdl) {
        {
            lock_guard<mutex> guard(m_action_lock);
            std::cout << "on_open" << std::endl;
            m_actions.push(action(SUBSCRIBE,hdl));
        }
        m_action_cond.notify_one();
    }

    void on_close(connection_hdl hdl) {
        {
            lock_guard<mutex> guard(m_action_lock);
            std::cout << "on_close" << std::endl;
            m_actions.push(action(UNSUBSCRIBE,hdl));
        }
        m_action_cond.notify_one();
    }

    void on_message(connection_hdl hdl, server::message_ptr msg) {
        // queue message up for sending by processing thread
        {
            lock_guard<mutex> guard(m_action_lock);
           // std::cout << "on_message" << std::endl;
            m_actions.push(action(MESSAGE,hdl,msg));
        }
        m_action_cond.notify_one();
    }

    void process_messages() {
        while(1) {
            unique_lock<mutex> lock(m_action_lock);

            while(m_actions.empty()) {
                m_action_cond.wait(lock);
            }

            action a = m_actions.front();
            m_actions.pop();

            lock.unlock();

            if (a.type == SUBSCRIBE)
            {
                lock_guard<mutex> guard(m_connection_lock);
                auto inserted = m_connections.insert(a.hdl);
                if( inserted.second)
                {
                    connectionMap[partyCount] = a.hdl;
                    char messageBuff[256];
                    sprintf(messageBuff, "partyID %x", partyCount++);
                    string message = messageBuff;
                    websocketpp::lib::error_code ec;
                    m_server.send(a.hdl, message, websocketpp::frame::opcode::text, ec);
                }
            } else if (a.type == UNSUBSCRIBE) 
            {
                lock_guard<mutex> guard(m_connection_lock);
                for (auto it=connectionMap.begin(); it!=connectionMap.end(); ++it)
                {
                    if((*it).second.lock() == a.hdl.lock())
                    {
                        connectionMap.erase(it);
                        break;
                    }
                }
                m_connections.erase(a.hdl);
            }
            else if (a.type == MESSAGE)
            {
                lock_guard<mutex> guard(m_connection_lock);
                string message = a.msg->get_payload();
                char messageType[256];
                int pos;
                int n = sscanf(message.c_str(), "%256s%n", messageType,&pos);
                if(n && strcmp("join",messageType) == 0)
                {
                    int partnerID;
                    int adv;
                    int n = sscanf(message.c_str() + pos, "%x%n", &partnerID, &adv);
                    pos += adv;
                    auto found = connectionMap.find(partnerID);
                    if(found != connectionMap.end())
                    {
                        websocketpp::lib::error_code ec;
                        for (auto it=connectionMap.begin(); it!=connectionMap.end(); ++it)
                        {
                            if((*it).second.lock() == a.hdl.lock())
                            {
                                char messageBuff[256];
                                sprintf(messageBuff, "partner %x", (*it).first);
                                string partnerMessage = messageBuff;

                                m_server.send((*found).second, partnerMessage, websocketpp::frame::opcode::text, ec);   
                                if (ec) {
                                    std::cout << "> Error sending message: " << ec.message() << std::endl;
                                }  
                                break;
                            }
                        }

                        websocketpp::lib::error_code ec2;
                        char messageBuff[256];
                        sprintf(messageBuff, "partner %x",partnerID);
                        string partnerMessage = messageBuff;
                        m_server.send(a.hdl, partnerMessage, websocketpp::frame::opcode::text, ec2);   
                        if (ec2) {
                            std::cout << "> Error sending message: " << ec.message() << std::endl;
                        }                    
                    }
                }
                else if(n && strcmp("data",messageType) == 0)
                {
                    int partnerID;
                    int adv;
                    int data;
                    int n = sscanf(message.c_str() + pos, "%x %x%n", &partnerID, &data, &adv);
                    pos += adv;
                    auto found = connectionMap.find(partnerID);
                    if(found != connectionMap.end())
                    {
                        websocketpp::lib::error_code ec;
                        char messageBuff[256];
                        sprintf(messageBuff, "data %x", data);
                        string partnerMessage = messageBuff;
                        m_server.send((*found).second, partnerMessage, websocketpp::frame::opcode::text, ec);   
                        if (ec) {
                            std::cout << "> Error sending message: " << ec.message() << std::endl;
                        }  
                    }
                }
            } else {
                // undefined.
            }
        }
    }
    void display_debug()    
    {
        lock_guard<mutex> guard(m_connection_lock);
        ImGui::LabelText("connected","%d", m_connections.size());
    }
private:
    typedef std::set<connection_hdl,std::owner_less<connection_hdl> > con_list;

    server m_server;
    con_list m_connections;
    unordered_map<int, connection_hdl> connectionMap; 
    std::queue<action> m_actions;

    mutex m_action_lock;
    mutex m_connection_lock;
    condition_variable m_action_cond;
};

broadcast_server server_instance;

void Server::init()
{
     std::thread first ([]
     {   
        thread t(bind(&broadcast_server::process_messages,&server_instance));
        server_instance.run(9002);
        t.join();
    }); 
    first.detach();

}

bool Server::update()
{
    bool done = false;
    std::string input;
    ImGui::Begin("Server");

    static bool launched = false;

    if(!launched)
    {
         init();
         launched = true;
    }


    if (ImGui::Button("quit"))
        done = true;

    if (ImGui::Button("init"))
        init();

    server_instance.display_debug();

    ImGui::End();

    return done;
}
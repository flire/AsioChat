#pragma once
#include "Client.h"
#include "message.pb.h"

#include <boost/thread/executors/basic_thread_pool.hpp>
#include <boost/asio.hpp>
#include <boost/thread.hpp>

#include <memory>

using ru::spbau::chat::commons::protocol::Message;
using boost::executors::basic_thread_pool;
using boost::shared_mutex;
using boost::thread_group;
using namespace std;

class Chat
{
public:
    Chat(io_service &argService): service(argService), commandThread(1) 
    {
    }

    void connect(shared_ptr<Client> user);
    void disconnect(shared_ptr<Client> user);
    void broadcast(const Message& message, shared_ptr<Client> sender);
    void executeCommand(string cmd, shared_ptr<Client> user_to_send_result);
    io_service& getService();

private:
    void deliverToAllExceptSender(const RawMessage & message, shared_ptr<Client> sender);
    void runServiceWorkers();
    static string getCurrentDirContents();

    int threadsNumber;
    int port;
    basic_thread_pool commandThread;
    vector<shared_ptr<Client>> clientsPool;
    shared_mutex clientsPoolMutex;
    io_service &service;
    thread_group workers;
};


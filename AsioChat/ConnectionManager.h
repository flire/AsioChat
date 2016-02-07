#pragma once
#include <boost/asio.hpp>
#include "Chat.h"

using namespace boost::asio;

class ConnectionManager
{
public:
    ConnectionManager(io_service &service, int port, Chat &chat):
        socket(service),
        acceptor(service, ip::tcp::endpoint(ip::tcp::v4(), port)),
        chat(chat)
    {}

    void accept();

private:
    ip::tcp::socket socket;
    ip::tcp::acceptor acceptor;
    Chat &chat;
};


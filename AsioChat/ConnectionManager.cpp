#include "ConnectionManager.h"

void ConnectionManager::accept()
{
    acceptor.async_accept(socket, [this](boost::system::error_code)
    {
        if (socket.is_open())
        {
            socket.set_option(ip::tcp::no_delay(true));
            chat.connect(std::make_shared<Client>(std::move(socket), chat));
        }
        accept();
    });
}


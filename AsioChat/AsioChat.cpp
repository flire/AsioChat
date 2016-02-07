#include <iostream>
#include <boost/thread.hpp>
#include "message.pb.h"
#include "Chat.h"
#include "Client.h"
#include "ConnectionManager.h"

using namespace boost::asio;
using namespace std;

void main(int argc, char *argv[])
{
    if (argc < 3) 
    {
        cout << "Expected port and a number of threads" << endl;
        return;
    }
    int port = atoi(argv[1]);
    size_t threadsNumber = atoi(argv[2]);

    try
    {
        io_service service(threadsNumber);
        Chat chat(service);

        ConnectionManager manager(service, port, chat);
        manager.accept();

        boost::thread_group workers;
        auto worker = boost::bind(&io_service::run, &service);
        for (size_t i = 0; i < threadsNumber; i++)
        {
            workers.create_thread(worker);
        }

        cout << "Running..." << endl;
        cin.get();
        cout << "Stopping..." << endl;

        service.stop();
        workers.join_all();
    }
    catch (exception &e) 
    {
        cerr << e.what() << endl;
    }

    google::protobuf::ShutdownProtobufLibrary();
}
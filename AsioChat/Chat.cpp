#include <string>
#include <codecvt>
#include <boost/filesystem.hpp>
#include <google/protobuf/io/coded_stream.h>
#include "Chat.h"
#include "ConnectionManager.h"
#include "RawMessage.h"

using google::protobuf::io::CodedOutputStream;

RawMessage getRawMessage(Message const &message)
{
    uint8 tempBuffer[32];
    size_t messageSize = message.ByteSize();
    uint8 *recordEnd = CodedOutputStream::WriteVarint32ToArray(messageSize, tempBuffer);

    size_t varintSize = recordEnd - tempBuffer;
    shared_ptr<uint8> messageData(new uint8[varintSize + messageSize]);
    memcpy(messageData.get(), tempBuffer, varintSize);
    
    message.SerializeToArray(messageData.get() + varintSize, messageSize);
    return { messageSize + varintSize, messageData };
}

void Chat::connect(shared_ptr<Client> client)
{
    clientsPoolMutex.lock();
    clientsPool.push_back(client);
    clientsPoolMutex.unlock();
    client->listen();
}

void Chat::disconnect(shared_ptr<Client> clientToDisconnect)
{
    clientsPoolMutex.lock();
    auto ctdIterator = find_if(
        clientsPool.begin(), 
        clientsPool.end(), 
        [clientToDisconnect](shared_ptr<Client> &poolClient)
        {
            return poolClient.get() == clientToDisconnect.get();
        }
    );
    if (ctdIterator != clientsPool.end())
    {
        clientsPool.erase(ctdIterator);
    }
    clientsPoolMutex.unlock();
}

void Chat::broadcast(const Message& message, shared_ptr<Client> sender)
{
    RawMessage rawMessage(getRawMessage(message));

    clientsPoolMutex.lock_shared();
    deliverToAllExceptSender(rawMessage, sender);
    clientsPoolMutex.unlock_shared();
    
    runServiceWorkers();
}

void Chat::runServiceWorkers()
{
    for (size_t i = 0; i < clientsPool.size() - 1; i++)
    {
        if (!service.poll_one())
        {
            break;
        }
    }
}

void Chat::deliverToAllExceptSender(const RawMessage &message, shared_ptr<Client> sender)
{
    for (const auto &poolClient : clientsPool)
    {
        if (poolClient.get() != sender.get())
        {
            poolClient->deliverMessage(message);
        }
    }
}

void Chat::executeCommand(string command, shared_ptr<Client> receiver)
{
    commandThread.submit([receiver, command]()
    {
        Message result;
        result.set_type(Message::MESSAGE);
        result.set_author("Command result");
        if (command == "dir") 
        {
            result.add_text(getCurrentDirContents());
        }
        else 
        {
            result.add_text("Unknown command");
        }

        receiver->deliverMessage(getRawMessage(result));
    });
}

io_service & Chat::getService()
{
    return service;
}

string Chat::getCurrentDirContents()
{
    wstringstream resultStream;

    namespace fs = boost::filesystem;
    fs::path currentDir(".");
    fs::directory_iterator dirEnd;

    for (fs::directory_iterator dirIter(currentDir); dirIter != dirEnd; ++dirIter)
    {
        resultStream << dirIter->path().filename().c_str() << endl;
    }
    wstring_convert<codecvt_utf8<wchar_t>> unwideConverter;
    return unwideConverter.to_bytes(resultStream.str());
}

#pragma once
#include <utility>
#include <memory>
#include <queue>
#include <google/protobuf/stubs/port.h>
#include <boost/asio.hpp>
#include <boost/thread.hpp>
#include "RawMessage.h"
#include "message.pb.h"

class Chat;

using google::protobuf::uint8;
using google::protobuf::uint32;
using namespace ru::spbau::chat::commons::protocol;
using namespace boost::asio;
using namespace std;

struct ParseStatus
{
    bool isSuccessful;
    uint32 result;
    size_t numberOfBytesReceived;
};

struct BufferStatus
{
    uint8* freeSpaceStart;
    size_t freeSpaceLeft;
};

class Client:
    public enable_shared_from_this<Client>
{
private:
    const size_t INITIAL_BUFFER_SIZE = 1 << 20; // 1 Mb
    const size_t MESSAGE_SIZE_BYTES = 3; //we expect no more than 64 mb of a single message (according to google guides), but it's varint
public:
    Client(ip::tcp::socket argSocket, Chat &argChat):
        socket(move(argSocket)),
        chat(argChat),
        messageBuffer(INITIAL_BUFFER_SIZE),
        bufferOffset(0),
        bufferUnprocessedBytes(0),
        writeBuffer(INITIAL_BUFFER_SIZE),
        writeInProgress(false)
    {
    }

    void listen();
    void receiveMessage(uint32 messageSize);
    void deliverMessage(RawMessage const &message);

private:
    void registerBytesProcessed(size_t bytesReceived);
    void listenAsync();
    ParseStatus tryParseMessageSize();
    BufferStatus getBufferStatus();
    void readMessageAsync(size_t expectedMessageSize);
    void processMessageBuffer(size_t expectedMessageSize);
    void processMessage(const Message & message);
    Message parseMessageFromBuffer(size_t expectedMessageSize);
    void resetMessageBuffer();
    void doWrite();
    void shiftBuffer();
    size_t writePendingToBuffer();
    bool shouldContinueWriting();
    void writeAsync(size_t written);
    void expandBuffer(vector<uint8> &buffer, size_t size);

    ip::tcp::socket socket;
    Chat &chat;
    vector<uint8> messageBuffer;
    size_t bufferOffset;
    size_t bufferUnprocessedBytes;
    vector<uint8> writeBuffer;
    queue<RawMessage> messagesToDeliver;
    boost::mutex deliverMutex;
    bool writeInProgress;
};

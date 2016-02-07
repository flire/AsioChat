#include <google/protobuf/io/coded_stream.h>
#include "Client.h"
#include "Chat.h"

using namespace google::protobuf::io;

void Client::listen()
{
    if (bufferOffset + MESSAGE_SIZE_BYTES > messageBuffer.size())
    {
        resetMessageBuffer();
        expandBuffer(messageBuffer, MESSAGE_SIZE_BYTES);
    }

    ParseStatus sizeReadingStatus = tryParseMessageSize();
    if (sizeReadingStatus.isSuccessful)
    {
        registerBytesProcessed(sizeReadingStatus.numberOfBytesReceived);
        receiveMessage(sizeReadingStatus.result);
    }
    else 
    {
        listenAsync();
    }
}

void Client::registerBytesProcessed(size_t bytesProcessed)
{
    bufferOffset += bytesProcessed;
    bufferUnprocessedBytes -= bytesProcessed;
}

void Client::listenAsync()
{
    auto self(shared_from_this());
    auto bufferStatus = getBufferStatus();
    async_read(
        socket,
        buffer(bufferStatus.freeSpaceStart, bufferStatus.freeSpaceLeft),
        transfer_at_least(1),
        [this, self](boost::system::error_code errcode, size_t numberOfBytesReceived)
    {
        if (errcode)
        {
            chat.disconnect(shared_from_this());
        }
        else
        {
            bufferUnprocessedBytes += numberOfBytesReceived;
            listen();
        }
    });

}

ParseStatus Client::tryParseMessageSize() 
{
    if (bufferUnprocessedBytes == 0)
    {
        return { false, 0, 0 };
    }

    CodedInputStream input(messageBuffer.data() + bufferOffset, bufferUnprocessedBytes);
    
    uint32 result;
    bool isSuccessful = input.ReadVarint32(&result);
    
    return { isSuccessful, result, (size_t) input.CurrentPosition() };
}

BufferStatus Client::getBufferStatus()
{
    size_t bytesDirty = bufferOffset + bufferUnprocessedBytes;
    return { messageBuffer.data() + bytesDirty,  messageBuffer.size() - bytesDirty };
}

void Client::receiveMessage(uint32 expectedMessageSize)
{
    if (bufferOffset + expectedMessageSize > messageBuffer.size())
    {
        resetMessageBuffer();
        expandBuffer(messageBuffer, expectedMessageSize);
    }

    if (bufferUnprocessedBytes >= expectedMessageSize)
    {
        try 
        {
            processMessageBuffer(expectedMessageSize);
        }
        catch (exception&) 
        {
            chat.disconnect(shared_from_this());
            return;
        }
        listen();
    }
    else 
    {
        readMessageAsync(expectedMessageSize);
    }
}

void Client::processMessageBuffer(size_t expectedMessageSize)
{
    Message message = parseMessageFromBuffer(expectedMessageSize);
    processMessage(message);
}

Message Client::parseMessageFromBuffer(size_t expectedMessageSize)
{
    Message message;
    message.ParseFromArray(messageBuffer.data() + bufferOffset, expectedMessageSize);
    registerBytesProcessed(expectedMessageSize);
    return message;
}

void Client::processMessage(const Message &message)
{
    if (message.type() == Message::COMMAND)
    {
        chat.executeCommand(message.text(0), shared_from_this());
    }
    else
    {
        chat.broadcast(message, shared_from_this());
    }
}

void Client::readMessageAsync(size_t expectedMessageSize)
{
    auto self(shared_from_this());
    auto bufferStatus = getBufferStatus();
    async_read(
        socket, 
        buffer(bufferStatus.freeSpaceStart, bufferStatus.freeSpaceLeft),
        transfer_at_least(expectedMessageSize - bufferUnprocessedBytes),
        [this, self, expectedMessageSize](boost::system::error_code errcode, size_t bytesReceived)
    {
        if (errcode)
        {
            chat.disconnect(shared_from_this());
        }
        else
        {
            bufferUnprocessedBytes += bytesReceived;
            receiveMessage(expectedMessageSize);
        }
    });

}

void Client::deliverMessage(RawMessage const & message)
{
    boost::unique_lock<boost::mutex> lock(deliverMutex);
    messagesToDeliver.push(message);
    if (!writeInProgress)
    {
        writeInProgress = true;
        auto self(shared_from_this());
        chat.getService().post([this, self]() {doWrite(); });
    }
}

void Client::resetMessageBuffer()
{
    if (bufferUnprocessedBytes)
    {
        shiftBuffer();
    }
    bufferOffset = 0;
}

void Client::shiftBuffer()
{
    rotate(messageBuffer.begin(), messageBuffer.begin() + bufferOffset, messageBuffer.end());
}

void Client::doWrite()
{
    boost::unique_lock<boost::mutex> lock(deliverMutex);
    expandBuffer(writeBuffer, messagesToDeliver.front().size);
    size_t written = writePendingToBuffer();
    writeAsync(written);
}

size_t Client::writePendingToBuffer()
{
    size_t offset = 0;
    while (!messagesToDeliver.empty())
    {
        const RawMessage &message = messagesToDeliver.front();
        if (message.size > writeBuffer.size() - offset)
        {
            break;
        }
        memcpy(writeBuffer.data() + offset, message.data.get(), message.size);
        offset += message.size;
        messagesToDeliver.pop();
    }
    return offset;
}

void Client::writeAsync(size_t written)
{
    auto self(shared_from_this());
    async_write(
        socket, 
        buffer(writeBuffer.data(), written),
        [this, self](boost::system::error_code errcode, size_t)
        {
            if (errcode)
            {
                chat.disconnect(shared_from_this());
            }
            else
            {
                if (shouldContinueWriting())
                {
                    doWrite();
                }
            }
        }
    );

}

void Client::expandBuffer(vector<uint8>& buffer, size_t size)
{
    if (buffer.size() < size) 
    {
        buffer.resize(size);
    }
}

bool Client::shouldContinueWriting() {
    boost::unique_lock<boost::mutex> lock(deliverMutex);
    writeInProgress = !messagesToDeliver.empty();
    return writeInProgress;
}
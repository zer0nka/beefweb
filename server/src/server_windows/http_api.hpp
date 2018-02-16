#pragma once

#include "iocp.hpp"
#include "../server_core.hpp"
#include "../string_utils.hpp"
#include "../core_types.hpp"

#include <http.h>

#include <boost/variant.hpp>

namespace msrv {
namespace server_windows {

class HttpApiInit;
class HttpRequestQueue;
class HttpUrlBinding;
class ReceiveRequestTask;
class SendResponseTask;
class HttpRequest;

class HttpApiInit
{
public:
    explicit HttpApiInit(ULONG flags);

    ~HttpApiInit()
    {
        if (flags_ != 0)
            ::HttpTerminate(flags_, nullptr);
    }

private:
    ULONG flags_;

    MSRV_NO_COPY_AND_ASSIGN(HttpApiInit);
};

class HttpRequestQueue
{
public:
    HttpRequestQueue(IoCompletionPort* ioPort);
    ~HttpRequestQueue();

    HANDLE handle() { return handle_.get(); }
    void setListener(RequestEventListener* listener) { listener_ = listener; }

    void bindPrefix(std::wstring prefix);
    void start();

private:
    friend class HttpRequest;

    static constexpr size_t MAX_REQUESTS = 64;

    void notifyReady(HttpRequest* request);
    void notifyDone(HttpRequest* request, bool wasReady);

    HttpApiInit apiInit_;
    WindowsHandle handle_;

    std::vector<std::unique_ptr<HttpUrlBinding>> boundPrefixes_;
    std::vector<std::unique_ptr<HttpRequest>> requests_;

    RequestEventListener* listener_;

    MSRV_NO_COPY_AND_ASSIGN(HttpRequestQueue);
};

class HttpUrlBinding
{
public:
    HttpUrlBinding(HttpRequestQueue* queue, std::wstring prefix);
    ~HttpUrlBinding();

private:
    HttpRequestQueue* queue_;
    std::wstring prefix_;

    MSRV_NO_COPY_AND_ASSIGN(HttpUrlBinding);
};

class ReceiveRequestTask : public OverlappedTask
{
public:
    explicit ReceiveRequestTask(HttpRequest* request);
    virtual ~ReceiveRequestTask();

    void run();
    virtual void complete(OverlappedResult* result) override;

    HTTP_REQUEST* request() { return reinterpret_cast<HTTP_REQUEST*>(buffer_.get()); }

private:
    // Should be enought for everyone :-)
    static constexpr size_t BUFFER_SIZE = 256 * 1024;

    HttpRequest* request_;
    MallocPtr<char> buffer_;
};

class SendResponseTask : public OverlappedTask
{
public:
    explicit SendResponseTask(HttpRequest* request);
    virtual ~SendResponseTask();

    bool isBusy() { return isBusy_; }

    void run(HTTP_REQUEST_ID requestId, ResponseCorePtr response, ULONG flags);
    void run(HTTP_REQUEST_ID requestId, ResponseCore::Body body, ULONG flags);

    virtual void complete(OverlappedResult* result) override;

private:
    void setResponse(ResponseCorePtr response);
    void setBody(ResponseCore::Body body);
    void reset();

    HttpRequest* request_;
    ResponseCorePtr response_;
    ::HTTP_RESPONSE responseData_;
    ResponseCore::Body body_;
    ::HTTP_DATA_CHUNK bodyChunk_;
    std::vector<::HTTP_UNKNOWN_HEADER> unknownHeaders_;
    bool isBusy_;
};

class HttpRequest : public RequestCore
{
public:
    explicit HttpRequest(HttpRequestQueue* queue);
    ~HttpRequest();

    HttpRequestQueue* queue() const { return queue_; }

    virtual HttpMethod method() override;
    virtual std::string path() override;
    virtual HttpKeyValueMap headers() override;
    virtual HttpKeyValueMap queryParams() override;
    virtual StringView body() override;
    virtual void releaseResources() override;

    virtual void abort() override;

    virtual void sendResponse(ResponseCorePtr response) override;
    virtual void sendResponseBegin(ResponseCorePtr response) override;
    virtual void sendResponseBody(ResponseCore::Body body) override;
    virtual void sendResponseEnd() override;

private:
    friend class HttpRequestQueue;
    friend class ReceiveRequestTask;
    friend class SendResponseTask;

    HTTP_REQUEST* data() { return receiveTask_->request(); }

    void receive();
    void reset();

    void notifyReceiveCompleted(OverlappedResult* result);
    void notifySendCompleted(OverlappedResult* result);

    HttpRequestQueue* queue_;
    HTTP_REQUEST_ID requestId_;
    TaskPtr<ReceiveRequestTask> receiveTask_;
    TaskPtr<SendResponseTask> sendTask_;
    std::deque<ResponseCore::Body> pending_;
    bool endAfterSendingAllChunks_;

    MSRV_NO_COPY_AND_ASSIGN(HttpRequest);
};

}}
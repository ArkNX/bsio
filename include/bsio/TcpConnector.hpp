#pragma once

#include <memory>
#include <functional>

#include <bsio/IoContextThreadPool.hpp>
#include <bsio/Functor.hpp>
#include <bsio/SharedSocket.hpp>

namespace bsio { namespace net {

    class TcpConnector
    {
    public:
        explicit TcpConnector(IoContextThreadPool::Ptr ioContextThreadPool)
            :
            mIoContextThreadPool(std::move(ioContextThreadPool))
        {
        }

        virtual ~TcpConnector() = default;

        void    asyncConnect(
            asio::ip::tcp::endpoint endpoint,
            std::chrono::nanoseconds timeout,
            const SocketEstablishHandler& successCallback,
            const SocketFailedConnectHandler& failedCallback,
            const std::vector<SocketProcessingHandler>& socketProcessingHandlerList)
        {
            wrapperAsyncConnect(mIoContextThreadPool->pickIoContextThread(),
                { std::move(endpoint) },
                timeout,
                successCallback,
                failedCallback,
                socketProcessingHandlerList);
        }

        static void    asyncConnect(
            const std::shared_ptr<IoContextThread>& ioContextThread,
            asio::ip::tcp::endpoint endpoint,
            std::chrono::nanoseconds timeout,
            const SocketEstablishHandler& successCallback,
            const SocketFailedConnectHandler& failedCallback,
            const std::vector<SocketProcessingHandler>& socketProcessingHandlerList)
        {
            wrapperAsyncConnect(ioContextThread,
                { std::move(endpoint) },
                timeout,
                successCallback,
                failedCallback,
                socketProcessingHandlerList);
        }

    private:
        static void    wrapperAsyncConnect(
            const IoContextThread::Ptr& ioContextThread,
            const std::vector<asio::ip::tcp::endpoint>& endpoints,
            std::chrono::nanoseconds timeout,
            const SocketEstablishHandler& successCallback,
            const SocketFailedConnectHandler& failedCallback,
            const std::vector<SocketProcessingHandler>& socketProcessingHandlerList)
        {
            auto sharedSocket = SharedSocket::Make(
                asio::ip::tcp::socket(ioContextThread->context()),
                ioContextThread->context());
            auto timeoutTimer = ioContextThread->wrapperIoContext().runAfter(timeout, [=]()
            {
                failedCallback();
            });

            asio::async_connect(sharedSocket->socket(),
                endpoints,
                [=](std::error_code ec, const asio::ip::tcp::endpoint&)
                {
                    timeoutTimer->cancel();
                    if (ec)
                    {
                        if (failedCallback != nullptr)
                        {
                            failedCallback();
                        }
                        return;
                    }

                    for (const auto& handler : socketProcessingHandlerList)
                    {
                        handler(sharedSocket->socket());
                    }
                    if (successCallback != nullptr)
                    {
                        successCallback(std::move(sharedSocket->socket()));
                    }
                });
        }

    private:
        IoContextThreadPool::Ptr mIoContextThreadPool;
    };

} }
#pragma once

#include "logic_context.hpp"
#include "tcp_session.hpp"

#include <asio.hpp> //NOLINT

#include <cstdint>
#include <iostream>
#include <memory>
#include <system_error>
#include <utility>

/// @brief This is TCP acceptor server based on ASIO.
/// @note This object could be placed in own thread.
class AsioAcceptTcpServer
{
  public:
    AsioAcceptTcpServer(asio::io_context &io_context, std::uint16_t port, // NOLINT
                        LogicContext logicContext) :
        acceptor_(io_context, asio::ip::tcp::endpoint(asio::ip::tcp::v4(), port)), // NOLINT
        logicContext_(std::move(logicContext))
    {
        std::cout << "ASIO Server started on port " << port << std::endl;
    }

    void do_accept()
    {
        // Do not block thread, call lambda when we have incoming.
        acceptor_.async_accept([this](std::error_code ec, asio::ip::tcp::socket socket) {
            if (!ec)
            {
                std::make_shared<TcpSession>(std::move(socket), logicContext_)->start();
            }
            if (logicContext_.canContinue())
            {
                do_accept();
            }
        });
    }

  private:
    asio::ip::tcp::acceptor acceptor_;
    LogicContext logicContext_;
};

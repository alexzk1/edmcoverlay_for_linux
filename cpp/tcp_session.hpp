#pragma once

#include "drawables.h"
#include "logic_context.hpp"
#include "svgbuilder.h"

#include <asio.hpp> // NOLINT

#include <algorithm>
#include <cstddef>
#include <exception>
#include <iostream>
#include <iterator>
#include <memory>
#include <string>
#include <system_error>
#include <utility>

class TcpSession : public std::enable_shared_from_this<TcpSession>
{
  public:
    // NOLINTNEXTLINE
    TcpSession(asio::ip::tcp::socket socket, LogicContext logicContext) :
        socket_(std::move(socket)),
        logicContext_(std::move(logicContext))
    {
    }

    void start()
    {
        readHeader();
    }

  private:
    /// @brief Message is: numeric+len#body
    void readHeader()
    {
        auto self(shared_from_this());
        asio::async_read_until( // NOLINT
          socket_, stream_buffer_, '#', [this, self](std::error_code ec, std::size_t /*length*/) {
              if (!ec)
              {
                  std::istream is(&stream_buffer_);
                  std::string header;
                  if (std::getline(is, header, '#'))
                  {
                      try
                      {
                          const std::size_t body_size = std::stoul(header);
                          readBody(body_size);
                      }
                      catch (...)
                      {
                          std::cerr << "Could not parse length prior # sign in packet."
                                    << std::endl;
                          socket_.close();
                      }
                  }
              }
          });
    }

    /// @brief Reads body of the message of length @p size and tries to parse it as json, translate
    /// to internal objects according LogicContext passed on construction.
    void readBody(std::size_t size)
    {
        auto self(shared_from_this());
        const std::size_t to_read =
          (size > stream_buffer_.size()) ? (size - stream_buffer_.size()) : 0;
        asio::async_read(socket_, // NOLINT
                         stream_buffer_,
                         asio::transfer_exactly(to_read), // NOLINT
                         [this, self, size](std::error_code ec, std::size_t /*length*/) {
                             if (!ec)
                             {
                                 std::string body;
                                 body.resize(size);
                                 std::istream is(&stream_buffer_);
                                 is.read(&body[0], size);
                                 process_payload(body);

                                 // Keep-Alive!
                                 // TODO: uncomment code to keep connection
                                 readHeader();
                             }
                         });
    }

    /// @brief Does actual json parsing according to internal logic.
    /// Fills provided OutputContext by new data received.
    void process_payload(const std::string &json_str)
    {
        draw_task::draw_items_t incoming_draws;
        try
        {
            incoming_draws = draw_task::parseJsonString(json_str);
        }
        catch (std::exception &e)
        {
            std::cerr << "Json parse failed with message: " << e.what() << std::endl;
            incoming_draws.clear();
        }
        catch (...)
        {
            std::cerr << "Json parse failed with unknown reason." << std::endl;
            incoming_draws.clear();
        }

        if (!incoming_draws.empty())
        {
            // Get rid of all drawables, convert them into SVG.
            for (auto &element : incoming_draws)
            {
                auto svgTask = SvgBuilder(logicContext_.window_width, logicContext_.window_height,
                                          std::move(element.second))
                                 .BuildSvgTask();
                element.second = std::move(svgTask);
            }

            if (!logicContext_.canContinue())
            {
                return;
            }

            logicContext_.outputContext.accessContext([this, &incoming_draws](auto &allDraws) {
                incoming_draws.merge(allDraws);
                // Anti-flickering support.
                if (!allDraws.empty())
                {
                    for (const auto &old : allDraws)
                    {
                        if (!logicContext_.canContinue())
                        {
                            break;
                        }
                        const auto it = incoming_draws.find(old.first);
                        if (it != incoming_draws.end())
                        {
                            if (it->second.isEqualStoredData(old.second))
                            {
                                it->second.setAlreadyRendered();
                            }
                        }
                    }
                    allDraws.clear();
                }
                removeRenamedDuplicates(incoming_draws);
                std::swap(allDraws, incoming_draws);
            });
        }
    }

    static void removeRenamedDuplicates(draw_task::draw_items_t &src)
    {
        for (auto iter = src.begin(); iter != std::prev(src.end());)
        {
            const auto dup = std::find_if(std::next(iter), src.end(), [&iter](const auto &item) {
                return item.second.isEqualStoredData(iter->second);
            });

            if (dup == src.end())
            {
                ++iter;
            }
            else
            {
                const bool rendered = iter->second.already_rendered || dup->second.already_rendered;
                if (iter->second.ttl.created_at < dup->second.ttl.created_at)
                {
                    dup->second.already_rendered = rendered;
                    iter = src.erase(iter);
                }
                else
                {
                    iter->second.already_rendered = rendered;
                    src.erase(dup);
                    ++iter;
                }
            }
        }
    }

    asio::ip::tcp::socket socket_;
    asio::streambuf stream_buffer_;
    LogicContext logicContext_;
};

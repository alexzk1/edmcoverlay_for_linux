// this file was heavy simplified by alexzkhr@gmail.com in 2021

#include "asio_accept_tcp_server.hpp"
#include "drawables.h"
#include "logic_context.hpp"
#include "runners.h"
#include "strutils.h"
#include "xoverlayoutput.h"

#include <asio.hpp> //NOLINT
#include <stdlib.h> //NOLINT

#include <chrono>
#include <csignal>
#include <cstddef>
#include <exception>
#include <functional>
#include <iostream>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <thread>

namespace {

const std::string windowClassName = "edmc_linux_overlay_class";
constexpr unsigned short port = 5010;

std::shared_ptr<std::thread> serverAcceptThread{nullptr};
void sighandler(int signum)
{
    std::cout << "edmc_linux_overlay: got signal " << signum << std::endl;

    if ((signum == SIGINT) || (signum == SIGTERM))
    {
        std::cout << "edmc_linux_overlay: SIGINT/SIGTERM, exiting" << std::endl;
        serverAcceptThread.reset();
    }
}

} // namespace

/*
    FYI: test string to send over "telnet 127.0.0.1 5010"

    111#{"id": "test1", "text": "You are low on fuel!", "size": "normal", "color": "red", "x": 200,
   "y": 100, "ttl": 8} 110#{"id": "test1", "text": "You are low on fuel!", "font_size": 50, "color":
   "red", "x": 200, "y": 100, "ttl": 8} 128#{"id": "test1", "text": "You are low on fuel!", "size":
   "normal", "font_size": 70, "color": "red", "x": 200, "y": 100, "ttl": 8}

    This contains UTF-8 chars and will fail with json parser on non-utf locale too:
    118#{"id": "test1", "text": "You are low on 水 fuel 水 !", "font_size": 50, "color": "red", "x":
   200, "y": 100, "ttl": 8}
*/

int main(int argc, char *argv[])
{
    using namespace std::chrono_literals;

    if (argc < 5 || argc > 6)
    {
        std::cerr << "Usage: overlay X Y W H [BinaryNameToOverlay]" << std::endl;
        return 1;
    }

    std::string programName;
    if (argc == 6)
    {
        programName = std::string(argv[5]);
        utility::trim(programName);
    }

    const auto window_width = atoi(argv[3]);
    const auto window_height = atoi(argv[4]);

    auto &drawer = XOverlayOutput::get(windowClassName, atoi(argv[1]), atoi(argv[2]), window_width,
                                       window_height);

    // std::cout << "edmcoverlay2: overlay starting up..." << std::endl;
    signal(SIGINT, sighandler);
    signal(SIGTERM, sighandler);

    drawer.cleanFrame();
    drawer.showVersionString("Binary is awaiting connection(s) from EDMC's plugins...", "green");
    drawer.flushFrame();
    // std::cout << "edmcoverlay2: overlay ready." << std::endl;

    draw_task::draw_items_t allDraws;
    OutputContext outputContext{std::make_shared<std::mutex>(), allDraws};

    serverAcceptThread = utility::startNewRunner(
      [&outputContext, window_height, window_width](const auto &should_close_ptr) {
          try
          {
              asio::io_context io_context; // NOLINT
              const AsioAcceptTcpServer server(
                io_context, port,
                LogicContext{window_width, window_height, outputContext, should_close_ptr});
              while (!(*should_close_ptr))
              {
                  io_context.run_for(250ms); // NOLINT
              }
              io_context.stop();
          }
          catch (std::exception &e)
          {
              std::cerr << "ASIO Server error: " << e.what() << std::endl;
          }
      });

    // Main thread loop. It draws and manages remove of the expired items.
    try
    {
        constexpr auto kAppActivityCheck = 1500ms; // NOLINT
        auto lastCheckTime = std::chrono::steady_clock::now() - kAppActivityCheck;
        bool targetAppActive = false;
        bool commandHideLayer = false;

        const std::map<std::string, std::function<bool()>> commandCallbacks = {
          {"exit",
           [&]() {
               serverAcceptThread.reset();
               // Skipping next loop by false here
               targetAppActive = false;
               return true; // break the loop
           }},
          {"overlay_on",
           [&]() {
               commandHideLayer = false;
               return false;
           }},
          {"overlay_off",
           [&]() {
               commandHideLayer = true;
               return false;
           }},
        };

        bool window_was_hidden = false;
        while (serverAcceptThread)
        {
            static std::size_t transparencyChecksCounter = 0;
            std::this_thread::sleep_for(100ms); // NOLINT
            if (lastCheckTime + kAppActivityCheck < std::chrono::steady_clock::now())
            {
                ++transparencyChecksCounter;
                lastCheckTime = std::chrono::steady_clock::now();
                targetAppActive =
                  programName.empty()
                  || utility::strcontains(drawer.getFocusedWindowBinaryPath(), programName);
                if (transparencyChecksCounter % 5 == 0 && !drawer.isTransparencyAvail())
                {
                    // It can be some service restart....
                    std::this_thread::sleep_for(500ms); // NOLINT
                    if (!drawer.isTransparencyAvail())
                    {
                        std::cerr << "Lost transparency. Closing overlay." << std::endl;
                        break;
                    }
                }
            }

            outputContext.accessContext([&](auto &allDraws) {
                bool skip_render = true;
                for (auto iter = allDraws.begin(); iter != allDraws.end();)
                {
                    const bool isCommand = iter->second.isCommand();
                    if (isCommand)
                    {
                        // std::cout << iter->second.command << std::endl;
                        const auto cmd_iter = commandCallbacks.find(iter->second.command);
                        if (cmd_iter != commandCallbacks.end())
                        {
                            if (cmd_iter->second())
                            {
                                skip_render = false;
                                break;
                            }
                        }
                    }

                    skip_render = skip_render && iter->second.already_rendered;

                    if (isCommand || iter->second.isExpired())
                    {
                        skip_render = false;
                        iter = allDraws.erase(iter);
                    }
                    else
                    {
                        ++iter;
                    }
                }

                if (targetAppActive && !commandHideLayer)
                {
                    if (!skip_render || window_was_hidden)
                    {
                        drawer.cleanFrame();
                        for (auto &drawitem : allDraws)
                        {
                            drawer.draw(drawitem.second);
                            drawitem.second.setAlreadyRendered();
                        }
                        drawer.flushFrame();
                    }
                    window_was_hidden = false;
                }
                else
                {
                    if (!window_was_hidden)
                    {
                        drawer.cleanFrame();
                        drawer.flushFrame();
                    }
                    window_was_hidden = true;
                }
            });
        }
    }
    catch (...)
    {
        std::cerr << "Exception into drawing loop. Program is going to exit..." << std::endl;
    }

    serverAcceptThread.reset();
    outputContext.accessContext([](auto &allDraws) {
        std::cout << "Final cleanup: " << allDraws.size() << " items left." << std::endl;
    });
    return 0;
}

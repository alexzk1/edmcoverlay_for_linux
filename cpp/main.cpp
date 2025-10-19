// this file was heavy simplified by alexzkhr@gmail.com in 2021

#include "drawables.h"
#include "json_message.hh"
#include "runners.h"
#include "socket.hh"
#include "strutils.h"
#include "svgbuilder.h"
#include "xoverlayoutput.h"

#include <stdlib.h>

#include <algorithm>
#include <chrono>
#include <csignal>
#include <cstddef>
#include <exception>
#include <functional>
#include <iostream>
#include <iterator>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <utility>

namespace {

const std::string windowClassName = "edmc_linux_overlay_class";
constexpr unsigned short port = 5010;

std::shared_ptr<std::thread> serverThread{nullptr};
void sighandler(int signum)
{
    std::cout << "edmc_linux_overlay: got signal " << signum << std::endl;

    if ((signum == SIGINT) || (signum == SIGTERM))
    {
        std::cout << "edmc_linux_overlay: SIGINT/SIGTERM, exiting" << std::endl;
        serverThread.reset();
        exit(0);
    }
}

void removeRenamedDuplicates(draw_task::draw_items_t &src)
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

    const auto mut = std::make_shared<std::mutex>();
    draw_task::draw_items_t allDraws;

    // FIXME: replace all that by boost:asio
    serverThread = utility::startNewRunner([&mut, &allDraws, window_height,
                                            window_width](const auto &should_close_ptr) {
        const std::shared_ptr<tcp_server_t> server(new tcp_server_t(port), [](tcp_server_t *p) {
            if (p)
            {
                // have no idea why it cannot be called from server destructor, but ok
                // let's do wrapper
                p->close();
                delete p;
            }
        });

        while (!(*should_close_ptr))
        {
            auto socket = server->accept_autoclose(should_close_ptr);
            if (!socket)
            {
                break;
            }

            // Let the while() continue and start to listen back ASAP.
            std::thread([socket = std::move(socket), &allDraws, mut, should_close_ptr,
                         window_height, window_width]() {
                try
                {
                    const std::string request = read_response(*socket);
                    // std::cout << "Request: " << request << std::endl;

                    draw_task::draw_items_t incoming_draws;
                    try
                    {
                        incoming_draws = draw_task::parseJsonString(request);
                    }
                    catch (std::exception &e)
                    {
                        std::cerr << "Json parse failed with message: " << e.what() << std::endl;
                        incoming_draws.clear();
                    }
                    catch (...)
                    {
                        std::cerr << "Json parse failed with uknnown reason." << std::endl;
                        incoming_draws.clear();
                    }

                    if (!incoming_draws.empty())
                    {
                        // Get rid of all drawables, convert them into SVG.
                        for (auto &element : incoming_draws)
                        {
                            auto svgTask = SvgBuilder(window_width, window_height,
                                                      TIndependantFont{}, std::move(element.second))
                                             .BuildSvgTask();
                            element.second = std::move(svgTask);
                        }

                        const std::lock_guard grd(*mut);
                        if ((!(*should_close_ptr)))
                        {
                            incoming_draws.merge(allDraws);
                            // Anti-flickering support.
                            if (!allDraws.empty())
                            {
                                for (const auto &old : allDraws)
                                {
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
                        }
                    }
                }
                catch (...)
                {
                }
            }).detach();
        };
    });

    // Main thread loop. It draws and manages remove of the expired items.
    try
    {
        constexpr auto kAppActivityCheck = 1500ms;
        auto lastCheckTime = std::chrono::steady_clock::now() - kAppActivityCheck;
        bool targetAppActive = false;
        bool commandHideLayer = false;

        const std::map<std::string, std::function<bool()>> commandCallbacks = {
          {"exit",
           [&]() {
               serverThread.reset();
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
        while (serverThread)
        {
            static std::size_t transparencyChecksCounter = 0;
            std::this_thread::sleep_for(100ms);
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
                    std::this_thread::sleep_for(500ms);
                    if (!drawer.isTransparencyAvail())
                    {
                        std::cerr << "Lost transparency. Closing overlay." << std::endl;
                        break;
                    }
                }
            }
            {
                const std::lock_guard grd(*mut);
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
            }
        }
    }
    catch (...)
    {
        std::cerr << "Exception into drawing loop. Program is going to exit..." << std::endl;
    }

    serverThread.reset();

    // Wait while all detached threads will stop writting parsed messages.
    const std::lock_guard grd(*mut);
    return 0;
}

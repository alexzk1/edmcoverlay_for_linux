//this file was heavy simplified by alexzkhr@gmail.com in 2021

#include <chrono>
#include <iostream>
#include <stdlib.h>
#include <csignal>
#include <mutex>
#include <map>

#include "socket.hh"
#include "json_message.hh"
#include "drawables.h"
#include "xoverlayoutput.h"
#include "runners.h"

static const std::string stop_cmd = "NEED_TO_STOP";
constexpr unsigned short port = 5010;

static std::shared_ptr<std::thread> serverThread{nullptr};

static void sighandler(int signum)
{
    std::cout << "edmcoverlay2: got signal " << signum << std::endl;

    if ((signum == SIGINT) || (signum == SIGTERM))
    {
        std::cout << "edmcoverlay2: SIGINT/SIGTERM, exiting" << std::endl;
        serverThread.reset();
        exit(0);
    }
}

/*
 * FYI: test string to send over "telnet 127.0.0.1 5010"

111#{"id": "test1", "text": "You are low on fuel!", "size": "normal", "color": "red", "x": 200, "y": 100, "ttl": 8}
110#{"id": "test1", "text": "You are low on fuel!", "font_size": 50, "color": "red", "x": 200, "y": 100, "ttl": 8}
128#{"id": "test1", "text": "You are low on fuel!", "size": "normal", "font_size": 70, "color": "red", "x": 200, "y": 100, "ttl": 8}
*/

int main(int argc, char* argv[])
{
    using namespace std::chrono_literals;

    if (argc != 5)
    {
        std::cerr << "Usage: overlay X Y W H" << std::endl;
        return 1;
    }

    auto& drawer = XOverlayOutput::get(atoi(argv[1]), atoi(argv[2]), atoi(argv[3]), atoi(argv[4]));

    //std::cout << "edmcoverlay2: overlay starting up..." << std::endl;
    signal(SIGINT, sighandler);
    signal(SIGTERM, sighandler);

    drawer.cleanFrame();
    drawer.showVersionString("Binary is awaiting connection(s) from EDMC's plugins...", "green");
    drawer.flushFrame();
    //std::cout << "edmcoverlay2: overlay ready." << std::endl;

    std::atomic_bool thread_stopped_loop{false};

    std::mutex mut;
    draw_task::draw_items_t allDraws;

    //FIXME: replace all that by boost:asio
    serverThread = utility::startNewRunner([&thread_stopped_loop, &mut,
                                            &allDraws](auto should_close_ptr)
    {
        std::shared_ptr<tcp_server_t> server(new tcp_server_t(port), [](tcp_server_t *p)
        {
            if (p)
            {
                //have no idea why it cannot be called from server destructor, but ok
                //let's do wrapper
                p->close();
                delete p;
            }
        });

        std::atomic<std::int64_t> detachedCounter{0};
        while (!(*should_close_ptr))
        {
            auto socket = server->accept_autoclose(should_close_ptr);
            if (!socket)
            {
                break;
            }

            //Let the while() continue and start to listen back ASAP.
            ++detachedCounter;
            std::thread([socket = std::move(socket), should_close_ptr, &allDraws, &mut, &detachedCounter]()
            {
                try
                {
                    const std::string request = read_response(*socket);
                    //std::cout << "Request: " << request << std::endl;

                    if (request == stop_cmd)
                    {
                        *should_close_ptr = true;
                    }
                    else
                    {
                        draw_task::draw_items_t incoming_draws;
                        try
                        {
                            incoming_draws = draw_task::parseJsonString(request);
                        }
                        catch (std::exception& e)
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
                            std::lock_guard grd(mut);
                            incoming_draws.merge(allDraws);
                            allDraws.clear();
                            std::swap(allDraws, incoming_draws);
                        }
                    }
                }
                catch(...)
                {
                }
                --detachedCounter;
            }).detach();
        };

        while (detachedCounter > 0)
        {
            std::this_thread::sleep_for(100ms);
        }

        thread_stopped_loop = true;
    });

    //Main thread loop. It draws and manages remove of the expired items.
    while (!thread_stopped_loop)
    {
        std::this_thread::sleep_for(500ms);

        drawer.cleanFrame();
        {
            std::lock_guard grd(mut);
            for(auto iter = allDraws.begin(); iter != allDraws.end(); )
            {
                if (iter->second.isExpired())
                {
                    iter = allDraws.erase(iter);
                }
                else
                {
                    ++iter;
                }
            }

            for (const auto& drawitem : allDraws)
            {
                drawer.draw(drawitem.second);
            }
        }
        drawer.flushFrame();
    }

    serverThread.reset();

    return 0;
}


#pragma once

#include <atomic>
#include <chrono>
#include <iostream>
#include <sstream>
#include <string>
#include <map>

#include "json.hpp"

namespace draw_task
{
    using json = nlohmann::json;

    struct timestamp_t
    {
        std::chrono::steady_clock::time_point created_at{std::chrono::steady_clock::now()};
        std::chrono::seconds ttl{-1};

        bool isValid() const
        {
            //original: https://github.com/inorton/EDMCOverlay/issues/42
            return ttl < std::chrono::seconds::zero()
                   || std::chrono::steady_clock::now() <= created_at + ttl;
        }

        bool isExpired() const
        {
            return !isValid();
        }

        timestamp_t& operator=(int aSeconds)
        {
            ttl = std::chrono::seconds(aSeconds);
            return *this;
        }
    };

    enum class drawmode_t
    {
        idk,
        text,
        shape,
    };

    struct drawitem_t
    {
        timestamp_t ttl;
        std::string id;

        drawmode_t  drawmode{drawmode_t::idk};

        // common
        int x{0};
        int y{0};
        std::string color;

        struct drawtext_t
        {
            // text
            std::string text;
            std::string size;
        } text;

        struct drawshape_t
        {
            // shape
            std::string shape;
            std::string fill;
            int w{0};
            int h{0};
            json vect;
        } shape;

        bool isExpired() const
        {
            return ttl.isExpired();
        }
    };

    using draw_items_t = std::map<std::string, drawitem_t>;

    /* text message: id, text, color, x, y, ttl, size
    * shape message: id, shape, color, fill, x, y, w, h, ttl
    * color: "red", "yellow", "green", "blue", "#rrggbb"
    * shape: "rect"
    * size: "normal", "large"
    */
    inline draw_items_t parseJsonString(const std::string& src)
    {
        //hate chained IFs, lets do it more readable....
#define FUNC_PARAMS const json& node, drawitem_t& drawitem
#define LHDR [](FUNC_PARAMS)->void
#define NINT node.get<int>()
#define NSTR node.get<std::string>()
        const static std::map<std::string, std::function<void(FUNC_PARAMS)>> processors =
        {
            {"x", LHDR{drawitem.x = NINT;}},
            {"y", LHDR{drawitem.y = NINT;}},
            {"color", LHDR{drawitem.color = NSTR;}},
            {"text", LHDR{drawitem.drawmode = drawmode_t::text; drawitem.text.text = NSTR;}},
            {"size", LHDR{drawitem.drawmode = drawmode_t::text; drawitem.text.size = NSTR;}},
            {"shape", LHDR{drawitem.drawmode = drawmode_t::shape; drawitem.shape.shape = NSTR;}},
            {"fill", LHDR{drawitem.drawmode = drawmode_t::shape; drawitem.shape.fill = NSTR;}},
            {"w", LHDR{drawitem.drawmode = drawmode_t::shape; drawitem.shape.w = NINT;}},
            {"h", LHDR{drawitem.drawmode = drawmode_t::shape; drawitem.shape.h = NINT;}},
            {"vector", LHDR{drawitem.drawmode = drawmode_t::shape; drawitem.shape.vect = node;}},

            {"ttl", LHDR{drawitem.ttl = NINT;}},
            {"id", LHDR{drawitem.id = NSTR;}},
        };
#undef NINT
#undef NSTR
#undef LHDR
#undef FUNC_PARAMS

        draw_items_t result;
        const auto parseSingleObject = [&result](const auto& aObject)
        {
            drawitem_t drawitem;
            for (const auto& kv : aObject.items())
            {
                //std::cout << "Key: [" << kv.key() << "]" << std::endl;

                const auto it = processors.find(kv.key());
                if (it != processors.end())
                {
                    const auto prev_mode  = drawitem.drawmode;
                    it->second(kv.value(), drawitem);
                    if (prev_mode != drawmode_t::idk && drawitem.drawmode != prev_mode)
                    {
                        std::cout << "Mode was double switched text/shape in the same JSON. Ignoring."  << std::endl;
                        drawitem.drawmode = drawmode_t::idk;
                        break;
                    }
                }
                else
                {
                    std::cout << "bad key: \"" << kv.key() <<"\"" << std::endl;
                }
            }
            if (drawitem.drawmode != draw_task::drawmode_t::idk)
            {
                if (drawitem.id.empty())
                {
                    static const std::string prefix = "AUTOID:";
                    static std::atomic<std::size_t> id{0};
                    drawitem.id = prefix + std::to_string(id++);
                    if (drawitem.ttl.ttl < std::chrono::seconds::zero())
                    {
                        //We do not allow messages without ID stay forever.
                        //Because without ID it cannot be overwritten / cleansed.
                        drawitem.ttl.ttl = std::chrono::seconds(60);
                    }
                }

                result[drawitem.id] = drawitem;
            }
        };


        if (!src.empty())
        {
            const auto jsrc = json::parse(src);
            if (jsrc.is_array())
            {
                for (const auto& arr_elem : jsrc)
                {
                    parseSingleObject(arr_elem);
                }
            }
            else
            {
                parseSingleObject(jsrc);
            }
        }

        return result;
    }

    //generates lines (x1;y1)-(x2;y2) and calls user callback with it
    //to avoid copy-paste of code for different output devices
    template <class Callback>
    inline bool ForEachVectorPointsPair(const drawitem_t& src, const Callback& func)
    {
        if (src.drawmode == draw_task::drawmode_t::shape && src.shape.shape == "vect")
        {
            constexpr static int UNINIT_COORD = std::numeric_limits<int>::max();
            int x1 = UNINIT_COORD, y1 = UNINIT_COORD, x2 = UNINIT_COORD, y2 = UNINIT_COORD;
            for (const auto& node_ : src.shape.vect.items())
            {
                // node_ is a point
                const auto& val = node_.value();
                int x, y;
                try
                {
                    x = val["x"].get<int>();
                    y = val["y"].get<int>();
                }
                catch (std::exception& e)
                {
                    std::cerr << "Json-point parse failed with message: " << e.what() << std::endl;
                    break;
                }
                catch (...)
                {
                    std::cerr << "Json-point parse failed with uknnown reason." << std::endl;
                    break;
                }

                if (x1 == UNINIT_COORD)
                {
                    x1 = x;
                    y1 = y;
                    continue;
                }
                if (x2 == UNINIT_COORD)
                {
                    x2 = x;
                    y2 = y;
                    func(x1, y1, x2, y2);
                    continue;
                }
                x1 = x2;
                y1 = y2;
                x2 = x;
                y2 = y;
                func(x1, y1, x2, y2);
            }
            return true;
        }
        return false;
    }
}

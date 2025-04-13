#pragma once

#include "json.hpp"

#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <functional>
#include <iostream>
#include <limits>
#include <map>
#include <optional>
#include <ostream>
#include <string>
#include <tuple>
#include <unordered_map>

namespace draw_task {
using json = nlohmann::json;

struct timestamp_t
{
    std::chrono::steady_clock::time_point created_at{std::chrono::steady_clock::now()};
    std::chrono::seconds ttl{-1};

    [[nodiscard]]
    bool isValid() const
    {
        // original: https://github.com/inorton/EDMCOverlay/issues/42
        return ttl < std::chrono::seconds::zero()
               || std::chrono::steady_clock::now() <= created_at + ttl;
    }

    [[nodiscard]]
    bool isExpired() const
    {
        return !isValid();
    }

    timestamp_t &operator=(int aSeconds)
    {
        ttl = std::chrono::seconds(aSeconds);
        return *this;
    }
};

enum class drawmode_t : std::uint8_t {
    idk,
    text,
    shape,
};

inline std::ostream &operator<<(std::ostream &os, drawmode_t val)
{
    static std::unordered_map<drawmode_t, std::string> enum2str{
      {drawmode_t::idk, "unknown"},
      {drawmode_t::text, "text"},
      {drawmode_t::shape, "shape"},
    };
    os << enum2str.at(val);
    return os;
}

struct drawitem_t
{
    timestamp_t ttl;
    std::string id;
    std::string command;

    drawmode_t drawmode{drawmode_t::idk};
    // common
    int x{0};
    int y{0};
    std::string color;

    struct drawtext_t
    {
        // text
        std::string text;
        std::string size;
        std::optional<int> fontSize{std::nullopt};
        bool operator==(const drawtext_t &other) const
        {
            static const auto tie = [](const drawtext_t &val) {
                return std::tie(val.text, val.size, val.fontSize);
            };

            return tie(*this) == tie(other);
        }
    } text;

    struct drawshape_t
    {
        // shape
        std::string shape;
        std::string fill;
        int w{0};
        int h{0};
        int vector_font_size{0};
        json vect;

        bool operator==(const drawshape_t &other) const
        {
            static const auto tie = [](const drawshape_t &val) {
                return std::tie(val.shape, val.fill, val.w, val.h, val.vect);
            };

            return tie(*this) == tie(other);
        }
    } shape;

    // Anti-flickering field,
    bool already_rendered{false};

    [[nodiscard]]
    bool IsEqualStoredData(const drawitem_t &other) const
    {
        static const auto tie = [](const drawitem_t &item) {
            return std::tie(item.drawmode, item.color, item.text, item.shape, item.x, item.y,
                            item.color);
        };
        return tie(*this) == tie(other);
    }

    [[nodiscard]]
    bool isExpired() const
    {
        return ttl.isExpired();
    }

    [[nodiscard]]
    bool isCommand() const
    {
        return !command.empty();
    }

    void SetAlreadyRendered()
    {
        already_rendered = true;
    }
};

using draw_items_t = std::map<std::string, drawitem_t>;

/*
    text message: id, text, color, x, y, ttl, size, [font_size]
    shape message: id, shape, color, fill, x, y, w, h, ttl
    color: "red", "yellow", "green", "blue", "#rrggbb"
    shape: "rect"
    size: "normal", "large"
    fontSize: if given, overrides "size" field. This is TTF font's size.
    command: text string command.
*/

inline draw_items_t parseJsonString(const std::string &src)
{
    // I hate chained IFs, lets do it more readable....
    const static std::map<std::string, std::function<void(const json &, drawitem_t &)>> processors =
      {
        {"x",
         [](const json &node, drawitem_t &drawitem) {
             drawitem.x = node.get<int>();
         }},

        {"y",
         [](const json &node, drawitem_t &drawitem) {
             drawitem.y = node.get<int>();
         }},

        {"w",
         [](const json &node, drawitem_t &drawitem) {
             drawitem.drawmode = drawmode_t::shape;
             drawitem.shape.w = node.get<int>();
         }},

        {"h",
         [](const json &node, drawitem_t &drawitem) {
             drawitem.drawmode = drawmode_t::shape;
             drawitem.shape.h = node.get<int>();
         }},

        {"color",
         [](const json &node, drawitem_t &drawitem) {
             drawitem.color = node.get<std::string>();
         }},

        {"text",
         [](const json &node, drawitem_t &drawitem) {
             drawitem.drawmode = drawmode_t::text;
             drawitem.text.text = node.get<std::string>();
         }},
        {"size",
         [](const json &node, drawitem_t &drawitem) {
             drawitem.drawmode = drawmode_t::text;
             drawitem.text.size = node.get<std::string>();
         }},

        {"font_size",
         [](const json &node, drawitem_t &drawitem) {
             drawitem.drawmode = drawmode_t::text;
             drawitem.text.fontSize = node.get<int>();
         }},

        {"vector_font_size",
         [](const json &node, drawitem_t &drawitem) {
             drawitem.drawmode = drawmode_t::shape;
             drawitem.shape.vector_font_size = node.get<int>();
         }},

        {"shape",
         [](const json &node, drawitem_t &drawitem) {
             drawitem.drawmode = drawmode_t::shape;
             drawitem.shape.shape = node.get<std::string>();
         }},

        {"fill",
         [](const json &node, drawitem_t &drawitem) {
             drawitem.drawmode = drawmode_t::shape;
             drawitem.shape.fill = node.get<std::string>();
         }},

        {"vector",
         [](const json &node, drawitem_t &drawitem) {
             drawitem.drawmode = drawmode_t::shape;
             drawitem.shape.vect = node;
         }},

        {"ttl",
         [](const json &node, drawitem_t &drawitem) {
             drawitem.ttl = node.get<int>();
         }},

        {"id",
         [](const json &node, drawitem_t &drawitem) {
             drawitem.id = node.get<std::string>();
         }},

        {"msgid",
         [](const json &node, drawitem_t &drawitem) {
             drawitem.id = node.get<std::string>();
         }},

        {"shapeid",
         [](const json &node, drawitem_t &drawitem) {
             drawitem.id = node.get<std::string>();
         }},

        {"command",
         [](const json &node, drawitem_t &drawitem) {
             drawitem.command = node.get<std::string>();
         }},
      };

    draw_items_t result;
    const auto parseSingleObject = [&result, &src](const auto &aObject) {
        drawitem_t drawitem;
        for (const auto &kv : aObject.items())
        {
            // std::cout << "Key: [" << kv.key() << "]" << std::endl;

            const auto it = processors.find(kv.key());
            if (it != processors.end())
            {
                const auto prev_mode = drawitem.drawmode;
                it->second(kv.value(), drawitem);
                if (prev_mode != drawmode_t::idk && drawitem.drawmode != prev_mode)
                {
                    std::cerr << "Mode was double switched text/shape in the same JSON. "
                              << "From " << prev_mode << " to " << drawitem.drawmode << ". "
                              << "Ignoring. Full source json:\n"
                              << src << std::endl;
                    drawitem.drawmode = drawmode_t::idk;
                    break;
                }
            }
            else
            {
                std::cout << "bad key: \"" << kv.key() << "\"" << std::endl;
            }
        }
        if (drawitem.drawmode != draw_task::drawmode_t::idk || drawitem.isCommand())
        {
            if (drawitem.id.empty())
            {
                static const std::string prefix = "AUTOID:";
                static std::atomic<std::size_t> id{0};
                drawitem.id = prefix + std::to_string(id++);
                if (drawitem.ttl.ttl < std::chrono::seconds::zero())
                {
                    // We do not allow messages without ID stay forever.
                    // Because without ID it cannot be overwritten / cleansed.
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
            for (const auto &arr_elem : jsrc)
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

/// @brief Represents shape "vector" / marker style in json.
struct TMarkerInVectorInShape
{
    int x{-1};
    int y{-1};

    std::string color;
    std::string type;
    std::string text;

    [[nodiscard]]
    bool IsSet() const
    {
        return !color.empty();
    }

    [[nodiscard]]
    bool IsCross() const
    {
        return type == "cross";
    }

    [[nodiscard]]
    bool IsCircle() const
    {
        return type == "circle";
    }

    [[nodiscard]]
    bool HasText() const
    {
        return !text.empty();
    }

    static TMarkerInVectorInShape FromVectorNode(const json &val)
    {
        static constexpr auto kMarkerKey = "marker";
        static constexpr auto kMarkerColorKey = "color";
        static constexpr auto kMarkerTextKey = "text";

        TMarkerInVectorInShape marker;
        const auto getStr = [&](const auto &key) -> std::string {
            if (val.contains(key))
            {
                return val[key].template get<std::string>();
            }
            return {};
        };

        marker.x = val["x"].get<int>();
        marker.y = val["y"].get<int>();
        marker.color = getStr(kMarkerColorKey);
        marker.type = getStr(kMarkerKey);
        marker.text = getStr(kMarkerTextKey);
        return marker;
    }
};

/// @brief Parses json's "vect" object and calls related drawers.
/// @note It uses provided drawer to avoid copy-paste of code for different output devices (like
/// X11/Wayland).
/// @returns false if @p src is not a "vector" shape.
/// @param taLineDrawer - drawer for lines.
/// @param taMarkerDrawer - drawer for markers.
template <typename taLineDrawer, typename taMarkerDrawer>
inline bool ForEachVectorPointsPair(const drawitem_t &src, const taLineDrawer &lineDrawer,
                                    const taMarkerDrawer &markerDrawer)
{
    if (!(src.drawmode == draw_task::drawmode_t::shape && src.shape.shape == "vect"))
    {
        return false;
    }
    constexpr static int UNINIT_COORD = std::numeric_limits<int>::max();
    int x1 = UNINIT_COORD, y1 = UNINIT_COORD, x2 = UNINIT_COORD, y2 = UNINIT_COORD;

    for (const auto &node_ : src.shape.vect.items())
    {
        // node_ is a point
        const auto &val = node_.value();
        int x = 0, y = 0;
        TMarkerInVectorInShape marker;
        try
        {
            x = val["x"].get<int>();
            y = val["y"].get<int>();
            marker = TMarkerInVectorInShape::FromVectorNode(val);
        }
        catch (std::exception &e)
        {
            std::cerr << "Json-point parse failed with message: " << e.what() << std::endl;
            break;
        }
        catch (...)
        {
            std::cerr << "Json-point parse failed with uknnown reason." << std::endl;
            break;
        }

        if (marker.IsSet())
        {
            markerDrawer(marker, src.shape.vector_font_size);
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
            lineDrawer(x1, y1, x2, y2);
            continue;
        }
        x1 = x2;
        y1 = y2;
        x2 = x;
        y2 = y;
        lineDrawer(x1, y1, x2, y2);
    }
    return true;
}
} // namespace draw_task

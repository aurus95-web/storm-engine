#pragma once

#include <cstdint>
#include <string_view>
#include <utility>

#include "string_compare.hpp"

namespace storm
{

constexpr std::string_view GetLayerName(size_t index)
{
    switch (index)
    {
    case 0:
        return "EXECUTE";
    case 1:
        return "REALIZE";
    case 2:
        return "SEA_EXECUTE";
    case 3:
        return "SEA_REALIZE";
    case 4:
        return "INTERFACE_EXECUTE";
    case 5:
        return "INTERFACE_REALIZE";
    case 6:
        return "FADER_EXECUTE";
    case 7:
        return "FADER_REALIZE";
    case 8:
        return "LIGHTER_EXECUTE";
    case 9:
        return "LIGHTER_REALIZE";
    case 10:
        return "VIDEO_EXECUTE";
    case 11:
        return "VIDEO_REALIZE";
    case 12:
        return "EDITOR_REALIZE";
    case 13:
        return "INFO_REALIZE";
    case 14:
        return "SOUND_DEBUG_REALIZE";
    case 15:
        return "SEA_REFLECTION";
    case 16:
        return "SEA_REFLECTION2";
    case 17:
        return "SEA_SUNROAD";
    case 18:
        return "SUN_TRACE";
    case 19:
        return "SAILS_TRACE";
    case 20:
        return "HULL_TRACE";
    case 21:
        return "MAST_ISLAND_TRACE";
    case 22:
        return "MAST_SHIP_TRACE";
    case 23:
        return "SHIP_CANNON_TRACE";
    case 24:
        return "FORT_CANNON_TRACE";
    case 25:
        return "ISLAND_TRACE";
    case 26:
        return "SHADOW";
    case 27:
        return "BLOOD";
    case 28:
        return "RAIN_DROPS";
    case 29:
        return "29";
    case 30:
        return "30";
    case 31:
        return "31";
    default:
        return "INVALID";
    }
}

// Index of a layer by its name, including the names of Pirates of the Caribbean scripts ("sea_realize",
// "iRealize"...), -1 for an unknown name
inline int32_t GetLayerIndexByName(std::string_view name)
{
    constexpr std::pair<std::string_view, int32_t> oldNames[] = {
        {"iexecute", 4},      // INTERFACE_EXECUTE
        {"irealize", 5},      // INTERFACE_REALIZE
        {"fader_execute", 6}, // FADER_EXECUTE
        {"fader_realize", 7}, // FADER_REALIZE
        {"inf_realize", 13},  // INFO_REALIZE
    };
    for (const auto &[oldName, oldIndex] : oldNames)
    {
        if (iEquals(oldName, name))
        {
            return oldIndex;
        }
    }
    for (int32_t i = 0; GetLayerName(i) != "INVALID"; i++)
    {
        if (iEquals(GetLayerName(i), name))
        {
            return i;
        }
    }
    return -1;
}

} // namespace storm

////////////////////////////////////////////////////////////////////////////////
// Filename: JaniConfig.h
////////////////////////////////////////////////////////////////////////////////
#pragma once

#include <cstdint>
#include <string>
#include <cmath>


namespace Jani
{
    static std::string pretty_bytes(uint64_t _bytes)
    {
        char out_buffer[64];
        const char* suffixes[7];
        suffixes[0] = "B";
        suffixes[1] = "KB";
        suffixes[2] = "MB";
        suffixes[3] = "GB";
        suffixes[4] = "TB";
        suffixes[5] = "PB";
        suffixes[6] = "EB";
        uint64_t s = 0; // which suffix to use
        double count = _bytes;
        while (count >= 1024 && s < 7)
        {
            s++;
            count /= 1024;
        }

        if (count - std::floor(count) == 0.0)
            std::snprintf(out_buffer, sizeof(out_buffer), "%d %s", (int)count, suffixes[s]);
        else
            std::snprintf(out_buffer, sizeof(out_buffer), "%.1f %s", count, suffixes[s]);

        return std::string(out_buffer);
    }
}
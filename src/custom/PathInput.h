#pragma once

#include <string_view>
#include "types.h"

namespace glimmer
{
    namespace custom
    {
        glimmer::WidgetDrawResult PathInput(std::string_view id, char* out, int size, bool isDirectory, 
            std::string_view initialPath, std::string_view placeholder = "", int32_t geometry = ToBottomRight, 
            const NeighborWidgets& neighbors = NeighborWidgets{});

        struct PathSet
        {
            char** out = nullptr;
            int size = 0;
            int maxPaths = 0;
            int count = 0;
            bool isDirectory = false;
        };

        glimmer::WidgetDrawResult MultiPathInput(std::string_view id, PathSet& paths, 
            std::string_view initialPath, std::string_view placeholder = "",bool isDirectory , int32_t geometry = ToBottomRight, 
            const NeighborWidgets& neighbors = NeighborWidgets{});
    }
}


#include "PathInput.h"

#include "widgets.h"
#include "layout.h"
#include "style.h"
#include <vector>
#include <string>

#include <unordered_map>
#include <span>

namespace glimmer
{
    namespace custom
    {
        struct PathInputData
        {
            IPlatform::FileDialogTarget target = IPlatform::FileDialogTarget::OneFile;
            std::string_view path;
            char* out = nullptr;
            int size = 0;
        };
        static std::unordered_map<int32_t, PathInputData> PathInputConfigs;

        WidgetDrawResult PathInput(std::string_view id, char* out, int size, bool isDirectory,
            std::string_view path, std::string_view placeholder, int32_t geometry,
            const NeighborWidgets& neighbors)
        {
            // You can customize the behavior here if needed
            BeginFlexLayout(DIR_Horizontal, geometry, false, {}, {}, neighbors);

            // Input box for paths
            PushStyle("width: 300px;");
            TextInput(out, size, placeholder, geometry, neighbors);
            PopStyle();

            // Browse button with icon
            auto nid = Icon(id, SymbolIcon::Browse, IconSizingType::CurrentFontSz, geometry, neighbors).id;
            void* data = (void*)(intptr_t)(nid);

            if (PathInputConfigs.find(nid) == PathInputConfigs.end())
            {
                auto& entry = PathInputConfigs[nid];
                entry.out = out; entry.size = size;
                entry.path = path;
                entry.target = isDirectory ? IPlatform::FileDialogTarget::OneDirectory :
                    IPlatform::FileDialogTarget::OneFile;

                GetUIConfig().platform->PushEventHandler([](void* data, const IODescriptor& desc) {
                    auto id = (int32_t)reinterpret_cast<intptr_t>(data);
                    auto bounds = ICustomWidget::GetBounds(id);
                    auto config = PathInputConfigs.at(id);

                    if (desc.clicked() && bounds.Contains(desc.mousepos))
                    {
                        std::span<char> out{ config.out, (std::size_t)config.size };
                        int32_t res = GetUIConfig().platform->ShowFileDialog(&out, 1,
                            (int32_t)config.target, config.path, nullptr, 0,
                            IPlatform::DialogProperties{ "Select Path", "Select", "Cancel" });
                        /*if (res > 0)
                        {
                            auto copysz = std::min((size_t)config.size - 1, out.size());
                            memcpy(config.out, out.data(), copysz);
                            config.out[copysz] = '\0';
                        }*/
                    }

                    return true;
                    }, data);
            }

            return EndLayout();
        }

        constexpr float ANIMATION_DURATION_MS = 300.0f;

        enum class AnimState
        {
            None,
            Appearing,
            Disappearing,
            Flashing
        };

        struct PathInfo
        {
            std::string path;
            AnimState state = AnimState::Appearing;
            float animProgress = 0.0f;
            int32_t closeButtonId = -1;
        };

        struct MultiPathInputData
        {
            std::vector<PathInfo> pathsInfo;
            char textInputBuffer[1024]{ 0 };
            bool isDirectory = false;
            std::string initialPath;
            PathSet* userPaths = nullptr;
            int32_t textInputId = -1;
            int32_t browseButtonId = -1;
            bool isInitialized = false;
            int currentPathCount = 0;
        };

        static std::unordered_map<std::string_view, MultiPathInputData> MultiPathData;

        static void SyncUserPaths(MultiPathInputData& data)
        {
            int i = 0;
            for (const auto& pInfo : data.pathsInfo)
                if (pInfo.state != AnimState::Disappearing && i < data.userPaths->count)
                    strncpy_s(data.userPaths->out[i], data.userPaths->size, pInfo.path.c_str(), _TRUNCATE);
            i++;

            data.currentPathCount = i;
            for (; i < data.userPaths->count; ++i)
                data.userPaths->out[i][0] = '\0';
        }

        static void AddPath(MultiPathInputData& data, std::string_view newPath)
        {
            if (newPath.empty() || data.currentPathCount >= data.userPaths->count) return;

            for (auto& pInfo : data.pathsInfo) {
                if (pInfo.path == newPath && pInfo.state != AnimState::Disappearing) {
                    pInfo.state = AnimState::Flashing;
                    pInfo.animProgress = 0.0f;
                    return;
                }
            }

            data.pathsInfo.insert(data.pathsInfo.begin(), { std::string(newPath), AnimState::Appearing, 0.0f });
            SyncUserPaths(data);
        }

        bool HandleMultiPathInputEvents(void* data, const IODescriptor& desc)
        {
            auto& widgetData = *static_cast<MultiPathInputData*>(data);

            if (desc.clicked() && widgetData.browseButtonId != -1 &&
                ICustomWidget::GetBounds(widgetData.browseButtonId).Contains(desc.mousepos))
            {
                char selectedPath[1024] = { 0 };
                std::span<char> out{ selectedPath, sizeof(selectedPath) };
                auto target = widgetData.isDirectory ? IPlatform::FileDialogTarget::OneDirectory : IPlatform::FileDialogTarget::OneFile;
                int32_t res = GetUIConfig().platform->ShowFileDialog(&out, 1, (int32_t)target, widgetData.initialPath, nullptr, 0, { "Select Path", "Select", "Cancel" });
                if (res > 0)
                {
                    AddPath(widgetData, selectedPath);
                    widgetData.textInputBuffer[0] = '\0';
                }
                return true;
            }

            if (widgetData.textInputId != -1 && ICustomWidget::IsInState(widgetData.textInputId, WS_Focused) &&
                desc.isKeyPressed(Key::Key_Enter))
            {
                AddPath(widgetData, widgetData.textInputBuffer);
                widgetData.textInputBuffer[0] = '\0';
                return true;
            }

            for (auto& pathInfo : widgetData.pathsInfo)
            {
                if (pathInfo.closeButtonId != -1 && ICustomWidget::GetBounds(pathInfo.closeButtonId).Contains(desc.mousepos)
                    && desc.clicked())
                {
                    if (pathInfo.state == AnimState::None)
                    {
                        pathInfo.state = AnimState::Disappearing;
                        pathInfo.animProgress = 0.0f;
                        return true;
                    }
                }
            }

            return true;
        }

        WidgetDrawResult MultiPathInput(std::string_view id,
            PathSet& paths,
            bool isDirectory,
            std::string_view initialPath,
            std::string_view placeholder,
            
            int32_t geometry,
            const NeighborWidgets& neighbors)
        {
            auto& data = MultiPathData[id];

            if (!data.isInitialized)
            {
                data.isDirectory = isDirectory;
                data.initialPath = initialPath;
                data.userPaths = &paths;
                for (int i = 0; i < paths.count && paths.out[i][0] != '\0'; ++i)
                {
                    data.pathsInfo.push_back({ std::string(paths.out[i]), AnimState::None, 1.0f });
                    data.currentPathCount++;
                }
                GetUIConfig().platform->PushEventHandler(HandleMultiPathInputEvents, &data);
                data.isInitialized = true;
            }

            data.userPaths = &paths;

            BeginFlexLayout(DIR_Vertical, geometry, false, {}, {}, neighbors);
            BeginFlexLayout(DIR_Horizontal, geometry, false, {}, {}, neighbors);
            auto inputResult = TextInput(data.textInputBuffer, sizeof(data.textInputBuffer), placeholder, geometry, neighbors);
            data.textInputId = inputResult.id;

            auto browseResult = Icon(id, SymbolIcon::Browse, IconSizingType::CurrentFontSz);
            data.browseButtonId = browseResult.id;
            EndLayout();

            for (auto it = data.pathsInfo.begin(); it != data.pathsInfo.end(); )
            {
                auto& pathInfo = *it;
                float itemHeight = 24.0f;
                float opacity = 1.0f;

                BeginFlexLayout(DIR_Horizontal, geometry, false, {}, {}, neighbors);

                if (pathInfo.state == AnimState::Appearing)
                {
                    anim::EaseOut(pathInfo.animProgress, ANIMATION_DURATION_MS);
                    itemHeight = anim::Interpolate(0.f, 24.f, pathInfo.animProgress);
                    opacity = pathInfo.animProgress;
                    if (pathInfo.animProgress >= 1.0f)
                    {
                        pathInfo.state = AnimState::None;
                        pathInfo.animProgress = 1.0f;
                    }
                }
                else if (pathInfo.state == AnimState::Disappearing)
                {
                    anim::EaseIn(pathInfo.animProgress, ANIMATION_DURATION_MS);
                    itemHeight = anim::Interpolate(24.f, 0.f, 1.0f - pathInfo.animProgress);
                    opacity = 1.0f - pathInfo.animProgress;
                    if (pathInfo.animProgress >= 1.0f)
                    {
                        it = data.pathsInfo.erase(it);
                        SyncUserPaths(data);
                        continue;
                    }
                }
                else if (pathInfo.state == AnimState::Flashing)
                {
                    anim::EaseOut(pathInfo.animProgress, ANIMATION_DURATION_MS * 2.0f);
                    if (pathInfo.animProgress >= 1.0f)
                    {
                        pathInfo.state = AnimState::None;
                        pathInfo.animProgress = 1.0f;
                    }
                }

                if (pathInfo.state == AnimState::Flashing)
                {
                    float flashAmount = 1.0f - pathInfo.animProgress;
                    uint8_t alpha = (uint8_t)(128 * flashAmount);
                    PushStyleFmt("background-color: rgba(255, 255, 0, %f);", alpha / 255.0f);
                }

                PushStyleFmt("height: %fpx; color: rgba(0,0,0,%f);", itemHeight, opacity);
                Label(pathInfo.path, pathInfo.path);
                PopStyle();
                if (pathInfo.state == AnimState::Flashing) PopStyle();

                auto closeButtonResult = Icon(pathInfo.path, SymbolIcon::Cross, IconSizingType::CurrentFontSz);
                pathInfo.closeButtonId = closeButtonResult.id;
                PopStyle();

                ++it;
                EndLayout();
            }

            return EndLayout();
        }
    }
}

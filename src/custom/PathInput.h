#pragma once

#include "widgets.h"
#include "layout.h"

#include <unordered_map>

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
			std::string_view path, std::string_view placeholder = "", int32_t geometry = ToBottomRight, 
			const NeighborWidgets& neighbors = NeighborWidgets{})
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
	}
}

#include "PluginAPI.h"

#include <cmath>
#include <cstring>
#include <vector>

using namespace swg::plugin;

namespace
{
    struct ProceduralState
    {
        HostContext host;
        std::vector<float> brushFalloff;
    };

    ProceduralState g_state{};

    bool onLoad(const HostContext &context)
    {
        g_state.host = context;

        g_state.brushFalloff.resize(32u);
        for (std::size_t i = 0; i < g_state.brushFalloff.size(); ++i)
        {
            const auto t = static_cast<float>(i) / static_cast<float>(g_state.brushFalloff.size() - 1);
            g_state.brushFalloff[i] = 1.0f - std::cos(t * 3.14159265f);
        }

        if (context.dispatch.log)
        {
            const char *message = "WorldBuilderProcedural plugin loaded";
            context.dispatch.log(LogLevel::Info, StringView{message, std::strlen(message)});
        }

        return true;
    }

    void onUnload()
    {
        g_state.brushFalloff.clear();
    }

    void onTick(double)
    {
        // No-op placeholder; real implementation would stream collaborative edits.
    }
}

extern "C" bool SwgRegisterPlugin(const HostContext &context, PluginDescriptor &descriptor, Lifecycle &lifecycle)
{
    descriptor.name = StringView{"WorldBuilderProcedural", std::strlen("WorldBuilderProcedural")};
    descriptor.description = StringView{"Adds procedural placement brushes and collaborative editing.", std::strlen("Adds procedural placement brushes and collaborative editing.")};
    descriptor.pluginVersion = makeVersion(0, 1, 0);
    descriptor.compatibleApiMin = makeVersion(1, 0, 0);
    descriptor.compatibleApiMax = makeVersion(1, 0, 0);

    lifecycle.onLoad = &onLoad;
    lifecycle.onUnload = &onUnload;
    lifecycle.onTick = &onTick;

    return lifecycle.onLoad ? lifecycle.onLoad(context) : false;
}


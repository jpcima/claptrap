#include "ct_defs.hpp"

namespace ct {

#if VERBOSE_PLUGIN_CALLS
static thread_local size_t plugin_call__depth_v = 0;

void plugin_call__enter()
{
    ++plugin_call__depth_v;
}

void plugin_call__leave()
{
    --plugin_call__depth_v;
}

size_t plugin_call__depth()
{
    return plugin_call__depth_v;
}
#endif

} // namespace ct

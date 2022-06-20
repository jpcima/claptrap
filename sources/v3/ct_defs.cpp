#include "ct_defs.hpp"

namespace ct {

#if VERBOSE_PLUGIN_CALLS
static thread_local size_t plugin_call__depth_v = 0;

void plugin_call__enter(void *self, const char *funcsig)
{
    message_printer lp = log_printer();
    lp.print((plugin_call__depth_v > 0) ?
             CT_MESSAGE_PREFIX_SPACES : CT_MESSAGE_PREFIX);
    for (size_t i = 0; i < plugin_call__depth_v; ++i)
        lp.print((i + 1 < plugin_call__depth_v) ? "   " : "+- ");
    if (self)
        lp.print("called (", (self), ") ", funcsig, '\n');
    else
        lp.print("called ", funcsig, '\n');
    //
    ++plugin_call__depth_v;
}

void plugin_call__leave()
{
    --plugin_call__depth_v;
}

message_printer plugin_log__return()
{
    message_printer lp = log_printer();
    lp.print(CT_MESSAGE_PREFIX_SPACES);
    for (size_t i = 0; i < plugin_call__depth_v; ++i)
        lp.print("   ");
    lp.print("returned ");
    return lp;
}
#endif

} // namespace ct

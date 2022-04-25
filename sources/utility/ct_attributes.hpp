#pragma once

#if !defined(CT_ATTRIBUTE_MALLOC)
#   if defined(__GNUC__)
#       define CT_ATTRIBUTE_MALLOC __attribute__((malloc))
#   else
#       define CT_ATTRIBUTE_MALLOC
#   endif
#endif

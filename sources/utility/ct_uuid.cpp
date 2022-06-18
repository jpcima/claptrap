#include "ct_uuid.hpp"
#include "ct_assert.hpp"

#define UUID_IMPLEMENTATION
#define UUID_API static
#define SHA1_API static
#define MD5_API static
#define CRYPTORAND_API static

#if defined(__GNUC__)
#   pragma GCC diagnostic push
#   pragma GCC diagnostic ignored "-Wunused-function"
#endif

#include "../../thirdparty/uuid/external/md5/md5.c"
#include "../../thirdparty/uuid/external/sha1/sha1.c"
#include "../../thirdparty/uuid/uuid.h"

#if defined(__GNUC__)
#   pragma GCC diagnostic pop
#endif

namespace ct {

void generate_uuid(uint8_t *result_uuid, const uint8_t *namespace_uuid, const char *name)
{
    uuid_result result = uuid5(result_uuid, namespace_uuid, name);
    CT_ASSERT(result == UUID_SUCCESS);
    (void)result;
}

} // namespace ct

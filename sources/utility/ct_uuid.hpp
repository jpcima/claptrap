#pragma once
#include <cstdint>

namespace ct {

void generate_uuid(uint8_t *result_uuid, const uint8_t *namespace_uuid, const char *name);

} // namespace ct

#pragma once
#include "driver/ext/memory.h"
#include <nlohmann/json.hpp>
#include <string>

namespace radar {
extern nlohmann::json m_data;
void Run(const Memory &mem, uintptr_t clientBase);
} // namespace radar

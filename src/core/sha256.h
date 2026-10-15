#pragma once
#include <cstdint>
#include <cstddef>
#include <string>

std::string Sha256Hex(const uint8_t* data, size_t len);

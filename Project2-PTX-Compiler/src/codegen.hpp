#pragma once

#include "ast.hpp"

#include <string>

namespace tinyptx {
std::string generatePTX(const Program& program);
}

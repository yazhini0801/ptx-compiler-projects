#pragma once

#include <string>
#include <vector>

namespace tinyptx {
enum class TokenKind { End, Identifier, Integer, Float, Keyword, Symbol };
struct Token { TokenKind kind; std::string text; int line; int column; };
std::vector<Token> lex(const std::string& source);
}

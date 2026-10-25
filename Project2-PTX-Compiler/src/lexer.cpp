#include "lexer.hpp"
#include "diagnostic.hpp"

#include <cctype>
#include <set>

namespace tinyptx {
std::vector<Token> lex(const std::string& source) {
    std::vector<Token> tokens;
    size_t i = 0;
    int line = 1, column = 1;
    const std::set<std::string> keywords{"kernel", "int", "float", "if", "else", "while", "for"};
    auto advance = [&](char c) { ++i; if (c == '\n') { ++line; column = 1; } else ++column; };
    while (i < source.size()) {
        const char c = source[i];
        if (std::isspace(static_cast<unsigned char>(c))) { advance(c); continue; }
        if (c == '/' && i + 1 < source.size() && source[i + 1] == '/') {
            while (i < source.size() && source[i] != '\n') advance(source[i]);
            continue;
        }
        const int tokenLine = line, tokenColumn = column;
        if (std::isalpha(static_cast<unsigned char>(c)) || c == '_') {
            const size_t start = i;
            while (i < source.size() && (std::isalnum(static_cast<unsigned char>(source[i])) || source[i] == '_')) advance(source[i]);
            std::string text = source.substr(start, i - start);
            tokens.push_back({keywords.count(text) ? TokenKind::Keyword : TokenKind::Identifier, text, tokenLine, tokenColumn});
            continue;
        }
        if (std::isdigit(static_cast<unsigned char>(c)) || (c == '.' && i + 1 < source.size() && std::isdigit(static_cast<unsigned char>(source[i + 1])))) {
            const size_t start = i;
            bool isFloat = false;
            while (i < source.size() && std::isdigit(static_cast<unsigned char>(source[i]))) advance(source[i]);
            if (i < source.size() && source[i] == '.') { isFloat = true; advance(source[i]); while (i < source.size() && std::isdigit(static_cast<unsigned char>(source[i]))) advance(source[i]); }
            if (i < source.size() && (source[i] == 'e' || source[i] == 'E')) {
                isFloat = true; advance(source[i]);
                if (i < source.size() && (source[i] == '+' || source[i] == '-')) advance(source[i]);
                const size_t exponentStart = i;
                while (i < source.size() && std::isdigit(static_cast<unsigned char>(source[i]))) advance(source[i]);
                if (i == exponentStart) throw CompileError(tokenLine, tokenColumn, "malformed floating-point exponent");
            }
            tokens.push_back({isFloat ? TokenKind::Float : TokenKind::Integer, source.substr(start, i - start), tokenLine, tokenColumn});
            continue;
        }
        std::string symbol(1, c);
        if ((c == '<' || c == '>' || c == '=' || c == '!' || c == '&' || c == '|') && i + 1 < source.size() && source[i + 1] == c && (c == '&' || c == '|')) {
            symbol.push_back(c); advance(c); advance(c);
        } else if ((c == '<' || c == '>' || c == '=' || c == '!') && i + 1 < source.size() && source[i + 1] == '=') {
            symbol.push_back('='); advance(c); advance('=');
        } else if (std::string("(){}[],;+-*/<>=").find(c) != std::string::npos) advance(c);
        else throw CompileError(tokenLine, tokenColumn, std::string("invalid character '") + c + "'");
        tokens.push_back({TokenKind::Symbol, symbol, tokenLine, tokenColumn});
    }
    tokens.push_back({TokenKind::End, "<end>", line, column});
    return tokens;
}
}

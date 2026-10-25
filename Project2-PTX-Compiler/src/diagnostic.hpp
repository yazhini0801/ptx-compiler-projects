#pragma once

#include <stdexcept>
#include <string>

namespace tinyptx {
class CompileError : public std::runtime_error {
public:
    CompileError(int line, int column, const std::string& message)
        : std::runtime_error("line " + std::to_string(line) + ", column " + std::to_string(column) + ": " + message),
          line_(line), column_(column) {}
    int line() const noexcept { return line_; }
    int column() const noexcept { return column_; }
private:
    int line_;
    int column_;
};
}

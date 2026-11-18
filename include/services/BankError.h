#pragma once
#include <stdexcept>
#include <string>

class BankError : public std::runtime_error {
public:
    BankError(int status, std::string code, const std::string& message)
        : std::runtime_error(message), status_(status), code_(std::move(code)) {}
    int status() const noexcept { return status_; }
    const std::string& code() const noexcept { return code_; }
private:
    int status_;
    std::string code_;
};

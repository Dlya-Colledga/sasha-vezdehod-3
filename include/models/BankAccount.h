#pragma once
#include <cstdint>
#include <string>

class BankAccount {
public:
    BankAccount(int64_t id, int64_t clientId, int64_t balanceCents = 0, std::string currency = "KGS");
    std::string getInfo() const;
    void deposit(int64_t amountCents);
    void withdraw(int64_t amountCents);
    int64_t getBalance() const noexcept;
    int64_t getId() const noexcept;
    int64_t getClientId() const noexcept;
    const std::string& getCurrency() const noexcept;
private:
    int64_t id_;
    int64_t clientId_;
    int64_t balanceCents_;
    std::string currency_;
};

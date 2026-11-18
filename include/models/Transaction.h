#pragma once
#include <cstdint>
#include <optional>
#include <string>

class Transaction {
public:
    Transaction(int64_t id, int64_t accountId, std::optional<int64_t> targetAccountId,
                std::string type, int64_t amountCents, std::string createdAt);
    std::string getInfo() const;
    const std::string& getType() const noexcept;
    int64_t getAmount() const noexcept;
    const std::string& getCreatedAt() const noexcept;
    int64_t getId() const noexcept;
    int64_t getAccountId() const noexcept;
    std::optional<int64_t> getTargetAccountId() const noexcept;
private:
    int64_t id_;
    int64_t accountId_;
    std::optional<int64_t> targetAccountId_;
    std::string type_;
    int64_t amountCents_;
    std::string createdAt_;
};

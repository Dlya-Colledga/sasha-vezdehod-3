#include "models/Transaction.h"
#include "services/BankError.h"
#include <utility>

Transaction::Transaction(int64_t id, int64_t accountId, std::optional<int64_t> targetAccountId,
                         std::string type, int64_t amountCents, std::string createdAt)
    : id_(id), accountId_(accountId), targetAccountId_(targetAccountId),
      type_(std::move(type)), amountCents_(amountCents), createdAt_(std::move(createdAt)) {
    if (id_ < 0 || accountId_ <= 0 || amountCents_ <= 0 || createdAt_.empty() ||
        (type_ != "deposit" && type_ != "withdraw" && type_ != "transfer") ||
        (type_ == "transfer" && (!targetAccountId_ || *targetAccountId_ <= 0 || *targetAccountId_ == accountId_)) ||
        (type_ != "transfer" && targetAccountId_))
        throw BankError(422, "invalid_transaction", "Недопустимые данные операции");
}
std::string Transaction::getInfo() const {
    std::string s = "Операция №" + std::to_string(id_) + " (" + type_ + "), счёт №" +
                    std::to_string(accountId_);
    if (targetAccountId_) s += " -> №" + std::to_string(*targetAccountId_);
    return s + ", " + std::to_string(amountCents_) + " тыйын, " + createdAt_;
}
const std::string& Transaction::getType() const noexcept { return type_; }
int64_t Transaction::getAmount() const noexcept { return amountCents_; }
const std::string& Transaction::getCreatedAt() const noexcept { return createdAt_; }
int64_t Transaction::getId() const noexcept { return id_; }
int64_t Transaction::getAccountId() const noexcept { return accountId_; }
std::optional<int64_t> Transaction::getTargetAccountId() const noexcept { return targetAccountId_; }

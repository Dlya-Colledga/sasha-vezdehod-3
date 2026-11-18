#include "models/BankAccount.h"
#include "services/BankError.h"
#include <limits>
#include <utility>

BankAccount::BankAccount(int64_t id, int64_t clientId, int64_t balanceCents, std::string currency)
    : id_(id), clientId_(clientId), balanceCents_(balanceCents), currency_(std::move(currency)) {
    if (id_ < 0 || clientId_ <= 0 || balanceCents_ < 0 || currency_ != "KGS")
        throw BankError(422, "invalid_account", "Недопустимые данные банковского счёта");
}
std::string BankAccount::getInfo() const {
    return "Счёт №" + std::to_string(id_) + ", владелец №" + std::to_string(clientId_) +
           ", баланс: " + std::to_string(balanceCents_) + " тыйын (KGS)";
}
void BankAccount::deposit(int64_t amountCents) {
    if (amountCents <= 0) throw BankError(422, "invalid_amount", "Сумма должна быть больше нуля");
    if (amountCents > std::numeric_limits<int64_t>::max() - balanceCents_)
        throw BankError(409, "balance_overflow", "Превышен максимально допустимый баланс");
    balanceCents_ += amountCents;
}
void BankAccount::withdraw(int64_t amountCents) {
    if (amountCents <= 0) throw BankError(422, "invalid_amount", "Сумма должна быть больше нуля");
    if (amountCents > balanceCents_)
        throw BankError(409, "insufficient_funds", "Недостаточно средств на счёте");
    balanceCents_ -= amountCents;
}
int64_t BankAccount::getBalance() const noexcept { return balanceCents_; }
int64_t BankAccount::getId() const noexcept { return id_; }
int64_t BankAccount::getClientId() const noexcept { return clientId_; }
const std::string& BankAccount::getCurrency() const noexcept { return currency_; }

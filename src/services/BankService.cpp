#include "services/BankService.h"
#include "services/BankError.h"
#include <algorithm>
#include <chrono>
#include <ctime>
#include <string>
#include <utility>

namespace {
// UTF-8 -> Unicode; для поиска переводим кириллицу в нижний регистр вручную.
// SQLite NOCASE не умеет искать русские буквы без учёта регистра.
std::u32string lowerUnicode(const std::string& text) {
    std::u32string result;
    for (size_t i = 0; i < text.size();) {
        auto first = static_cast<unsigned char>(text[i]);
        char32_t cp = 0;
        size_t length = 0;
        if (first < 0x80) { cp = first; length = 1; }
        else if ((first & 0xE0) == 0xC0) { cp = first & 0x1F; length = 2; }
        else if ((first & 0xF0) == 0xE0) { cp = first & 0x0F; length = 3; }
        else if ((first & 0xF8) == 0xF0) { cp = first & 0x07; length = 4; }
        else { result.push_back(U'\uFFFD'); ++i; continue; }
        if (i + length > text.size()) { result.push_back(U'\uFFFD'); break; }
        bool valid = true;
        for (size_t j = 1; j < length; ++j) {
            auto c = static_cast<unsigned char>(text[i + j]);
            if ((c & 0xC0) != 0x80) { valid = false; break; }
            cp = (cp << 6) | (c & 0x3F);
        }
        if (!valid || cp > 0x10FFFF || (cp >= 0xD800 && cp <= 0xDFFF) ||
            (length == 2 && cp < 0x80) || (length == 3 && cp < 0x800) ||
            (length == 4 && cp < 0x10000)) {
            result.push_back(U'\uFFFD'); ++i; continue;
        }
        i += length;
        if (cp >= U'A' && cp <= U'Z') cp += 32;
        else if (cp >= 0x0410 && cp <= 0x042F) cp += 32; // А-Я
        else if (cp == 0x0401) cp = 0x0451; // Ё
        else if (cp == 0x04A2 || cp == 0x04E8 || cp == 0x04AE) cp += 1; // Ң Ө Ү
        result.push_back(cp);
    }
    return result;
}
std::string nowUtc() {
    const auto now = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
    std::tm tm{};
    gmtime_r(&now, &tm);
    char buf[32]{};
    std::strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%SZ", &tm);
    return buf;
}
} // namespace

BankService::BankService(const std::filesystem::path& dbPath) : database_(dbPath) {}
Client BankService::requireClient(int64_t id) {
    auto value = database_.selectClient(id);
    if (!value) throw BankError(404, "client_not_found", "Клиент не найден");
    return *value;
}
BankAccount BankService::requireAccount(int64_t id) {
    auto value = database_.selectAccount(id);
    if (!value) throw BankError(404, "account_not_found", "Банковский счёт не найден");
    return *value;
}
Client BankService::addClient(const std::string& name, int age, const std::string& phone) {
    std::lock_guard lock(mutex_);
    Client client(0, name, age, phone);
    const int64_t id = database_.insertClient(client);
    return requireClient(id);
}
std::vector<Client> BankService::getClients(const std::string& search) {
    std::lock_guard lock(mutex_);
    auto clients = database_.listClients();
    if (search.empty()) return clients;
    const auto pattern = lowerUnicode(search);
    std::erase_if(clients, [&](const Client& client) {
        return lowerUnicode(client.getName()).find(pattern) == std::u32string::npos;
    });
    return clients;
}
Client BankService::findClient(int64_t id) {
    std::lock_guard lock(mutex_);
    return requireClient(id);
}
Client BankService::updateClient(int64_t id, const std::optional<std::string>& name,
                                 const std::optional<int>& age, const std::optional<std::string>& phone) {
    std::lock_guard lock(mutex_);
    Client client = requireClient(id);
    if (name) client.changeName(*name);
    if (age) client.changeAge(*age);
    if (phone) client.changePhone(*phone);
    database_.updateClient(client);
    return client;
}
void BankService::deleteClient(int64_t id) {
    std::lock_guard lock(mutex_);
    requireClient(id);
    if (database_.clientHasAccounts(id))
        throw BankError(409, "client_has_accounts", "Нельзя удалить клиента с открытыми счетами");
    database_.deleteClient(id);
}
BankAccount BankService::createAccount(int64_t clientId) {
    std::lock_guard lock(mutex_);
    requireClient(clientId);
    const int64_t id = database_.insertAccount(clientId);
    return requireAccount(id);
}
std::vector<BankAccount> BankService::getAccounts() {
    std::lock_guard lock(mutex_);
    return database_.listAccounts();
}
BankAccount BankService::findAccount(int64_t id) {
    std::lock_guard lock(mutex_);
    return requireAccount(id);
}
std::vector<BankAccount> BankService::getClientAccounts(int64_t clientId) {
    std::lock_guard lock(mutex_);
    requireClient(clientId);
    return database_.clientAccounts(clientId);
}
void BankService::deleteAccount(int64_t id) {
    std::lock_guard lock(mutex_);
    const auto account = requireAccount(id);
    if (account.getBalance() != 0)
        throw BankError(409, "nonzero_balance", "Нельзя закрыть счёт с ненулевым балансом");
    if (database_.accountHasTransactions(id))
        throw BankError(409, "account_has_history", "Нельзя закрыть счёт с историей операций");
    database_.deleteAccount(id);
}
BankAccount BankService::deposit(int64_t accountId, int64_t amountCents) {
    std::lock_guard lock(mutex_);
    Database::TransactionScope tx(database_);
    BankAccount account = requireAccount(accountId);
    account.deposit(amountCents);
    database_.updateBalance(account);
    database_.insertTransaction(Transaction(0, accountId, std::nullopt, "deposit", amountCents, nowUtc()));
    tx.commit();
    return account;
}
BankAccount BankService::withdraw(int64_t accountId, int64_t amountCents) {
    std::lock_guard lock(mutex_);
    Database::TransactionScope tx(database_);
    BankAccount account = requireAccount(accountId);
    account.withdraw(amountCents);
    database_.updateBalance(account);
    database_.insertTransaction(Transaction(0, accountId, std::nullopt, "withdraw", amountCents, nowUtc()));
    tx.commit();
    return account;
}
BankAccount BankService::transfer(int64_t fromId, int64_t toId, int64_t amountCents) {
    if (fromId == toId)
        throw BankError(422, "same_account", "Нельзя перевести деньги на тот же счёт");
    std::lock_guard lock(mutex_);
    Database::TransactionScope tx(database_);
    BankAccount from = requireAccount(fromId);
    BankAccount to = requireAccount(toId);
    from.withdraw(amountCents);
    to.deposit(amountCents);
    database_.updateBalance(from);
    database_.updateBalance(to);
    Transaction record(0, fromId, toId, "transfer", amountCents, nowUtc());
    database_.insertTransaction(record);
    tx.commit();
    return from;
}
std::vector<Transaction> BankService::getTransactions(int64_t accountId) {
    std::lock_guard lock(mutex_);
    requireAccount(accountId);
    return database_.listTransactions(accountId);
}

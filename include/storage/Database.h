#pragma once
#include "models/BankAccount.h"
#include "models/Client.h"
#include "models/Transaction.h"
#include <cstdint>
#include <filesystem>
#include <optional>
#include <vector>

struct sqlite3;

class Database {
public:
    explicit Database(const std::filesystem::path& path);
    ~Database();
    Database(const Database&) = delete;
    Database& operator=(const Database&) = delete;

    class TransactionScope {
    public:
        explicit TransactionScope(Database& database);
        ~TransactionScope() noexcept;
        TransactionScope(const TransactionScope&) = delete;
        TransactionScope& operator=(const TransactionScope&) = delete;
        void commit();
    private:
        Database& database_;
        bool active_ = true;
    };

    void initialize();
    int64_t insertClient(const Client& client);
    std::vector<Client> listClients();
    std::optional<Client> selectClient(int64_t id);
    void updateClient(const Client& client);
    void deleteClient(int64_t id);
    bool clientHasAccounts(int64_t clientId);

    int64_t insertAccount(int64_t clientId);
    std::vector<BankAccount> listAccounts();
    std::optional<BankAccount> selectAccount(int64_t id);
    std::vector<BankAccount> clientAccounts(int64_t clientId);
    void updateBalance(const BankAccount& account);
    bool accountHasTransactions(int64_t accountId);
    void deleteAccount(int64_t id);

    int64_t insertTransaction(const Transaction& transaction);
    std::vector<Transaction> listTransactions(int64_t accountId);

private:
    friend class TransactionScope;
    void exec(const char* sql);
    sqlite3* handle_ = nullptr;
};

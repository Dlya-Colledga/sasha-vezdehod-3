#pragma once
#include "storage/Database.h"
#include <filesystem>
#include <mutex>
#include <optional>
#include <string>
#include <vector>

class BankService {
public:
    explicit BankService(const std::filesystem::path& dbPath);
    Client addClient(const std::string& name, int age, const std::string& phone);
    std::vector<Client> getClients(const std::string& search = "");
    Client findClient(int64_t id);
    Client updateClient(int64_t id, const std::optional<std::string>& name,
                        const std::optional<int>& age, const std::optional<std::string>& phone);
    void deleteClient(int64_t id);

    BankAccount createAccount(int64_t clientId);
    std::vector<BankAccount> getAccounts();
    BankAccount findAccount(int64_t id);
    std::vector<BankAccount> getClientAccounts(int64_t clientId);
    void deleteAccount(int64_t id);

    BankAccount deposit(int64_t accountId, int64_t amountCents);
    BankAccount withdraw(int64_t accountId, int64_t amountCents);
    BankAccount transfer(int64_t fromId, int64_t toId, int64_t amountCents);
    std::vector<Transaction> getTransactions(int64_t accountId);
private:
    Client requireClient(int64_t id);
    BankAccount requireAccount(int64_t id);
    Database database_;
    std::mutex mutex_;
};

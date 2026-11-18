#pragma once
#include <httplib.h>
#include <nlohmann/json.hpp>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>

class BankCli {
public:
    explicit BankCli(std::string baseUrl);
    void run();
private:
    using json = nlohmann::json;
    std::unique_ptr<httplib::Client> client_;
    std::optional<json> request(const std::string& method, const std::string& path,
                                const std::optional<json>& body = std::nullopt);
    static std::string prompt(const std::string& label, bool allowEmpty = false);
    static std::optional<int64_t> number(const std::string& label);
    static std::optional<std::string> amount(const std::string& label);
    static bool confirm(const std::string& text);
    static std::string money(const std::string& amount);
    static void showClient(const json& client);
    static void showAccount(const json& account);
    static void showHistory(const json& history, int64_t accountId);
    static std::string encode(const std::string& value);
    void listClients();
    void addClient();
    void findClients();
    void editClient();
    void removeClient();
    void accountsMenu();
    void operationsMenu();
    void historyMenu();
};

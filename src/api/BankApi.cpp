#include "api/BankApi.h"
#include "services/BankError.h"
#include <nlohmann/json.hpp>
#include <charconv>
#include <cstdint>
#include <functional>
#include <initializer_list>
#include <limits>
#include <optional>
#include <string>
#include <string_view>

using json = nlohmann::json;

namespace {
void sendJson(httplib::Response& res, const json& value, int status = 200) {
    res.status = status;
    res.set_content(value.dump(), "application/json; charset=utf-8");
}
void safe(httplib::Response& res, const std::function<void()>& work) {
    try {
        work();
    } catch (const BankError& e) {
        sendJson(res, {{"error", {{"code", e.code()}, {"message", e.what()}}}}, e.status());
    } catch (const json::exception&) {
        sendJson(res, {{"error", {{"code", "invalid_json"}, {"message", "Неверный формат JSON"}}}}, 400);
    } catch (const std::exception&) {
        sendJson(res, {{"error", {{"code", "internal_error"},
                                   {"message", "Внутренняя ошибка сервера"}}}}, 500);
    }
}
json parseBody(const httplib::Request& req) {
    try {
        auto value = json::parse(req.body);
        if (!value.is_object())
            throw BankError(400, "invalid_json", "Тело запроса должно быть JSON-объектом");
        return value;
    } catch (const json::parse_error&) {
        throw BankError(400, "invalid_json", "Невозможно прочитать JSON в теле запроса");
    }
}
void allowOnly(const json& obj, std::initializer_list<std::string_view> names) {
    for (auto it = obj.begin(); it != obj.end(); ++it) {
        bool allowed = false;
        for (auto name : names) {
            if (it.key() == name) { allowed = true; break; }
        }
        if (!allowed)
            throw BankError(400, "unknown_field", "Неизвестное поле запроса: " + it.key());
    }
}
const json& require(const json& obj, const char* key) {
    if (!obj.contains(key))
        throw BankError(422, "missing_field", "Отсутствует обязательное поле: " + std::string(key));
    return obj.at(key);
}
std::string getString(const json& value, const char* key) {
    if (!value.is_string())
        throw BankError(422, "invalid_type", "Поле " + std::string(key) + " должно быть строкой");
    return value.get<std::string>();
}
int64_t getInteger(const json& value, const char* key) {
    if (!value.is_number_integer())
        throw BankError(422, "invalid_type", "Поле " + std::string(key) + " должно быть целым числом");
    if (value.is_number_unsigned()) {
        const auto number = value.get<uint64_t>();
        if (number > static_cast<uint64_t>(std::numeric_limits<int64_t>::max()))
            throw BankError(422, "invalid_value", "Число слишком большое");
        return static_cast<int64_t>(number);
    }
    return value.get<int64_t>();
}
int64_t positiveId(const json& value, const char* key) {
    const int64_t number = getInteger(value, key);
    if (number <= 0) throw BankError(422, "invalid_id", "ID должен быть положительным числом");
    return number;
}
int64_t pathId(const httplib::Request& req, size_t index = 1) {
    const std::string part = req.matches[index].str();
    int64_t number = 0;
    const auto r = std::from_chars(part.data(), part.data() + part.size(), number);
    if (r.ec != std::errc{} || r.ptr != part.data() + part.size() || number <= 0)
        throw BankError(422, "invalid_id", "Недопустимый ID в адресе запроса");
    return number;
}
int parseAge(const json& value) {
    const int64_t age = getInteger(value, "age");
    if (age < 18 || age > 100)
        throw BankError(422, "invalid_age", "Возраст должен быть от 18 до 100 лет");
    return static_cast<int>(age);
}
int64_t parseMoney(const json& value) {
    const auto amount = getString(value, "amount");
    if (amount.empty())
        throw BankError(422, "invalid_amount", "Введите сумму, например 150.50");
    int64_t whole = 0;
    int64_t fraction = 0;
    size_t i = 0;
    while (i < amount.size() && amount[i] >= '0' && amount[i] <= '9') {
        const int digit = amount[i++] - '0';
        if (whole > (std::numeric_limits<int64_t>::max() - digit) / 10)
            throw BankError(422, "invalid_amount", "Сумма слишком большая");
        whole = whole * 10 + digit;
    }
    if (i == 0)
        throw BankError(422, "invalid_amount", "Введите положительную сумму в сомах, например 150.50");
    if (i < amount.size()) {
        if (amount[i++] != '.' || i == amount.size() || amount.size() - i > 2)
            throw BankError(422, "invalid_amount", "Неверный формат суммы. Пример: 150.50");
        fraction = amount[i++] - '0';
        if (fraction < 0 || fraction > 9)
            throw BankError(422, "invalid_amount", "Неверный формат суммы. Пример: 150.50");
        fraction *= 10;
        if (i < amount.size()) {
            if (amount[i] < '0' || amount[i] > '9')
                throw BankError(422, "invalid_amount", "Неверный формат суммы. Пример: 150.50");
            fraction += amount[i++] - '0';
        }
    }
    if (i != amount.size() || whole > (std::numeric_limits<int64_t>::max() - fraction) / 100)
        throw BankError(422, "invalid_amount", "Неверный формат или слишком большая сумма");
    const int64_t cents = whole * 100 + fraction;
    if (cents == 0) throw BankError(422, "invalid_amount", "Сумма должна быть больше нуля");
    return cents;
}
std::string money(int64_t cents) {
    const auto part = cents % 100;
    return std::to_string(cents / 100) + "." + (part < 10 ? "0" : "") + std::to_string(part);
}
json clientJson(const Client& c) {
    const Person& person = c;  // Реальный полиморфизм: виртуальный Client::getInfo() через Person&.
    return {{"id", c.getId()}, {"name", c.getName()}, {"age", c.getAge()},
            {"phone", c.getPhone()}, {"info", person.getInfo()}};
}
json accountJson(const BankAccount& a) {
    return {{"id", a.getId()}, {"client_id", a.getClientId()},
            {"balance", money(a.getBalance())}, {"currency", a.getCurrency()}};
}
json transactionJson(const Transaction& t) {
    json target = nullptr;
    if (t.getTargetAccountId()) target = *t.getTargetAccountId();
    return {{"id", t.getId()}, {"account_id", t.getAccountId()},
            {"target_account_id", target}, {"type", t.getType()},
            {"amount", money(t.getAmount())}, {"created_at", t.getCreatedAt()}};
}
} // namespace

BankApi::BankApi(BankService& service) : service_(service) { registerRoutes(); }
bool BankApi::start(const std::string& host, int port) { return server_.listen(host, port); }
void BankApi::registerRoutes() {
    server_.Get("/health", [](const httplib::Request&, httplib::Response& res) {
        sendJson(res, {{"status", "ok"}, {"message", "Сервер работает"}});
    });

    server_.Get("/clients", [this](const httplib::Request& req, httplib::Response& res) {
        safe(res, [&] {
            const auto search = req.has_param("search") ? req.get_param_value("search") : "";
            json result = json::array();
            for (const auto& c : service_.getClients(search)) result.push_back(clientJson(c));
            sendJson(res, result);
        });
    });
    server_.Post("/clients", [this](const httplib::Request& req, httplib::Response& res) {
        safe(res, [&] {
            const auto body = parseBody(req);
            allowOnly(body, {"name", "age", "phone"});
            sendJson(res, clientJson(service_.addClient(
                getString(require(body, "name"), "name"), parseAge(require(body, "age")),
                getString(require(body, "phone"), "phone"))), 201);
        });
    });
    server_.Get(R"(/clients/(\d+))", [this](const httplib::Request& req, httplib::Response& res) {
        safe(res, [&] { sendJson(res, clientJson(service_.findClient(pathId(req)))); });
    });
    server_.Patch(R"(/clients/(\d+))", [this](const httplib::Request& req, httplib::Response& res) {
        safe(res, [&] {
            const auto body = parseBody(req);
            allowOnly(body, {"name", "age", "phone"});
            if (body.empty()) throw BankError(422, "empty_update", "Укажите хотя бы одно поле для изменения");
            std::optional<std::string> name, phone;
            std::optional<int> age;
            if (body.contains("name")) name = getString(body.at("name"), "name");
            if (body.contains("age")) age = parseAge(body.at("age"));
            if (body.contains("phone")) phone = getString(body.at("phone"), "phone");
            sendJson(res, clientJson(service_.updateClient(pathId(req), name, age, phone)));
        });
    });
    server_.Delete(R"(/clients/(\d+))", [this](const httplib::Request& req, httplib::Response& res) {
        safe(res, [&] { service_.deleteClient(pathId(req)); res.status = 204; });
    });

    server_.Get("/accounts", [this](const httplib::Request&, httplib::Response& res) {
        safe(res, [&] {
            json result = json::array();
            for (const auto& a : service_.getAccounts()) result.push_back(accountJson(a));
            sendJson(res, result);
        });
    });
    server_.Post("/accounts", [this](const httplib::Request& req, httplib::Response& res) {
        safe(res, [&] {
            const auto body = parseBody(req);
            allowOnly(body, {"client_id"});
            sendJson(res, accountJson(service_.createAccount(
                positiveId(require(body, "client_id"), "client_id"))), 201);
        });
    });
    server_.Get(R"(/accounts/(\d+))", [this](const httplib::Request& req, httplib::Response& res) {
        safe(res, [&] { sendJson(res, accountJson(service_.findAccount(pathId(req)))); });
    });
    server_.Delete(R"(/accounts/(\d+))", [this](const httplib::Request& req, httplib::Response& res) {
        safe(res, [&] { service_.deleteAccount(pathId(req)); res.status = 204; });
    });
    server_.Get(R"(/clients/(\d+)/accounts)", [this](const httplib::Request& req, httplib::Response& res) {
        safe(res, [&] {
            json result = json::array();
            for (const auto& a : service_.getClientAccounts(pathId(req))) result.push_back(accountJson(a));
            sendJson(res, result);
        });
    });
    server_.Post(R"(/accounts/(\d+)/deposit)", [this](const httplib::Request& req, httplib::Response& res) {
        safe(res, [&] {
            const auto body = parseBody(req);
            allowOnly(body, {"amount"});
            sendJson(res, accountJson(service_.deposit(pathId(req), parseMoney(require(body, "amount")))));
        });
    });
    server_.Post(R"(/accounts/(\d+)/withdraw)", [this](const httplib::Request& req, httplib::Response& res) {
        safe(res, [&] {
            const auto body = parseBody(req);
            allowOnly(body, {"amount"});
            sendJson(res, accountJson(service_.withdraw(pathId(req), parseMoney(require(body, "amount")))));
        });
    });
    server_.Post("/transfers", [this](const httplib::Request& req, httplib::Response& res) {
        safe(res, [&] {
            const auto body = parseBody(req);
            allowOnly(body, {"from_account_id", "to_account_id", "amount"});
            const auto from = positiveId(require(body, "from_account_id"), "from_account_id");
            const auto to = positiveId(require(body, "to_account_id"), "to_account_id");
            const auto amount = parseMoney(require(body, "amount"));
            sendJson(res, { {"message", "Перевод выполнен"},
                            {"sender", accountJson(service_.transfer(from, to, amount))} });
        });
    });
    server_.Get(R"(/accounts/(\d+)/transactions)", [this](const httplib::Request& req, httplib::Response& res) {
        safe(res, [&] {
            json result = json::array();
            for (const auto& t : service_.getTransactions(pathId(req))) result.push_back(transactionJson(t));
            sendJson(res, result);
        });
    });
    server_.set_error_handler([](const httplib::Request&, httplib::Response& res) {
        if (res.status == 404)
            sendJson(res, {{"error", {{"code", "route_not_found"},
                                      {"message", "Запрошенный адрес не найден"}}}}, 404);
        else if (res.status == 405)
            sendJson(res, {{"error", {{"code", "method_not_allowed"},
                                      {"message", "HTTP-метод не поддерживается для этого адреса"}}}}, 405);
    });
}

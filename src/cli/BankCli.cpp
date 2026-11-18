#include "BankCli.h"
#include <charconv>
#include <cctype>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <limits>
#include <sstream>
#include <utility>

BankCli::BankCli(std::string baseUrl) {
    while (!baseUrl.empty() && baseUrl.back() == '/') baseUrl.pop_back();
    if (baseUrl.empty()) baseUrl = "http://127.0.0.1:8000";
    client_ = std::make_unique<httplib::Client>(baseUrl);
    client_->set_connection_timeout(3, 0);
    client_->set_read_timeout(5, 0);
    std::cout << "Адрес сервера: " << baseUrl << '\n';
}
std::optional<BankCli::json> BankCli::request(const std::string& method,
    const std::string& path, const std::optional<json>& body) {
    try {
        auto receive = [&](httplib::Result result) -> std::optional<json> {
            if (!result) {
                std::cout << "Ошибка соединения с сервером. Проверьте, запущен ли bank_server.\n";
                return std::nullopt;
            }
            if (result->status == 204) return json::object();
            json parsed;
            try {
                parsed = json::parse(result->body);
            } catch (const json::exception&) {
                std::cout << "Сервер вернул ответ в неверном формате. HTTP " << result->status << '\n';
                return std::nullopt;
            }
            if (result->status >= 200 && result->status < 300) return parsed;
            if (parsed.is_object() && parsed.contains("error") && parsed["error"].is_object()) {
                const auto& error = parsed["error"];
                std::cout << "Ошибка: " << error.value("message", std::string("Неизвестная ошибка"))
                          << " (HTTP " << result->status << ")\n";
            } else {
                std::cout << "Ошибка запроса. HTTP " << result->status << '\n';
            }
            return std::nullopt;
        };
        if (method == "GET") return receive(client_->Get(path.c_str()));
        if (method == "POST") return receive(client_->Post(path.c_str(), body.value_or(json::object()).dump(),
                                                       "application/json"));
        if (method == "PATCH") return receive(client_->Patch(path.c_str(), body.value_or(json::object()).dump(),
                                                         "application/json"));
        if (method == "DELETE") return receive(client_->Delete(path.c_str()));
        std::cout << "Неподдерживаемый HTTP-метод.\n";
    } catch (const std::exception&) {
        std::cout << "Сетевая ошибка. Проверьте адрес API_URL и подключение.\n";
    }
    return std::nullopt;
}
std::string BankCli::prompt(const std::string& label, bool allowEmpty) {
    std::cout << label;
    std::string line;
    if (!std::getline(std::cin, line)) return "";
    if (!allowEmpty && line.find_first_not_of(" \t\r\n") == std::string::npos)
        std::cout << "Ошибка: поле не должно быть пустым.\n";
    return line;
}
std::optional<int64_t> BankCli::number(const std::string& label) {
    const auto line = prompt(label);
    if (line.empty()) return std::nullopt;
    int64_t value = 0;
    const auto conversion = std::from_chars(line.data(), line.data() + line.size(), value);
    if (conversion.ec != std::errc{} || conversion.ptr != line.data() + line.size() || value < 0) {
        std::cout << "Ошибка: введите целое неотрицательное число.\n";
        return std::nullopt;
    }
    return value;
}
std::optional<std::string> BankCli::amount(const std::string& label) {
    const auto input = prompt(label + " (сомы, например 150.50): ");
    if (input.empty()) return std::nullopt;
    uint64_t whole = 0;
    size_t i = 0;
    while (i < input.size() && input[i] >= '0' && input[i] <= '9') {
        const int digit = input[i++] - '0';
        if (whole > (static_cast<uint64_t>(std::numeric_limits<int64_t>::max()) - digit) / 10) {
            std::cout << "Ошибка: слишком большая сумма.\n";
            return std::nullopt;
        }
        whole = whole * 10 + digit;
    }
    if (i == 0) {
        std::cout << "Ошибка: неверный формат суммы.\n";
        return std::nullopt;
    }
    int64_t fraction = 0;
    if (i < input.size()) {
        if (input[i++] != '.' || i == input.size() || input.size() - i > 2) {
            std::cout << "Ошибка: используйте точку и не более двух знаков после неё.\n";
            return std::nullopt;
        }
        if (input[i] < '0' || input[i] > '9') {
            std::cout << "Ошибка: неверный формат суммы.\n";
            return std::nullopt;
        }
        fraction = (input[i++] - '0') * 10;
        if (i < input.size()) {
            if (input[i] < '0' || input[i] > '9') {
                std::cout << "Ошибка: неверный формат суммы.\n";
                return std::nullopt;
            }
            fraction += input[i++] - '0';
        }
    }
    if (i != input.size() || whole > (static_cast<uint64_t>(std::numeric_limits<int64_t>::max()) - fraction) / 100 ||
        (whole == 0 && fraction == 0)) {
        std::cout << "Ошибка: требуется положительная сумма без переполнения.\n";
        return std::nullopt;
    }
    return std::to_string(whole) + "." + (fraction < 10 ? "0" : "") + std::to_string(fraction);
}
bool BankCli::confirm(const std::string& text) {
    return prompt(text + " (да/нет): ") == "да";
}
std::string BankCli::money(const std::string& amount) { return amount + " сом"; }
void BankCli::showClient(const json& c) {
    std::cout << "ID: " << c.value("id", 0LL) << " | " << c.value("name", std::string())
              << " | возраст: " << c.value("age", 0)
              << " | телефон: " << c.value("phone", std::string()) << '\n';
}
void BankCli::showAccount(const json& a) {
    std::cout << "Счёт №" << a.value("id", 0LL)
              << " | клиент №" << a.value("client_id", 0LL)
              << " | баланс: " << money(a.value("balance", std::string("0.00")))
              << " | " << a.value("currency", std::string("KGS")) << '\n';
}
void BankCli::showHistory(const json& history, int64_t accountId) {
    if (!history.is_array() || history.empty()) {
        std::cout << "История операций пуста.\n";
        return;
    }
    for (const auto& t : history) {
        const std::string type = t.value("type", std::string());
        std::string description;
        if (type == "deposit") description = "Пополнение";
        else if (type == "withdraw") description = "Снятие";
        else if (type == "transfer") description = "Перевод";
        else description = "Операция";
        std::cout << "№" << t.value("id", 0LL) << " | " << description;
        if (type == "transfer") {
            const auto from = t.value("account_id", 0LL);
            const int64_t to = t.contains("target_account_id") && !t["target_account_id"].is_null()
                ? t["target_account_id"].get<int64_t>() : 0;
            std::cout << " №" << from << " -> №" << to
                      << (accountId == to ? " (входящий)" : " (исходящий)");
        }
        std::cout << " | " << money(t.value("amount", std::string("0.00")))
                  << " | " << t.value("created_at", std::string()) << '\n';
    }
}
std::string BankCli::encode(const std::string& text) {
    const char* digits = "0123456789ABCDEF";
    std::string encoded;
    for (unsigned char ch : text) {
        if (std::isalnum(ch) && ch < 128) encoded.push_back(static_cast<char>(ch));
        else if (ch == '-' || ch == '_' || ch == '.' || ch == '~') encoded.push_back(static_cast<char>(ch));
        else { encoded.push_back('%'); encoded.push_back(digits[ch >> 4]); encoded.push_back(digits[ch & 15]); }
    }
    return encoded;
}
void BankCli::listClients() {
    auto response = request("GET", "/clients");
    if (!response) return;
    if (response->empty()) { std::cout << "Клиентов пока нет.\n"; return; }
    for (const auto& c : *response) showClient(c);
}
void BankCli::addClient() {
    auto name = prompt("ФИО: "); if (name.empty()) return;
    auto age = number("Возраст (18–100): "); if (!age) return;
    auto phone = prompt("Телефон: "); if (phone.empty()) return;
    auto response = request("POST", "/clients", json{{"name", name}, {"age", *age}, {"phone", phone}});
    if (response) { std::cout << "Клиент добавлен.\n"; showClient(*response); }
}
void BankCli::findClients() {
    std::cout << "1. По ID\n2. По ФИО (частичное совпадение)\n0. Назад\n";
    auto choice = number("Выберите действие: "); if (!choice) return;
    if (*choice == 1) {
        auto id = number("ID клиента: "); if (!id) return;
        auto response = request("GET", "/clients/" + std::to_string(*id));
        if (response) showClient(*response);
    } else if (*choice == 2) {
        auto name = prompt("Введите ФИО или часть имени: "); if (name.empty()) return;
        auto response = request("GET", "/clients?search=" + encode(name));
        if (response) {
            if (response->empty()) std::cout << "Клиенты не найдены.\n";
            else for (const auto& c : *response) showClient(c);
        }
    } else if (*choice != 0) std::cout << "Неизвестная команда.\n";
}
void BankCli::editClient() {
    auto id = number("ID клиента: "); if (!id) return;
    auto current = request("GET", "/clients/" + std::to_string(*id)); if (!current) return;
    showClient(*current);
    while (!std::cin.eof()) {
        std::cout << "\n=====================================\n"
                     "          ИЗМЕНЕНИЕ КЛИЕНТА\n"
                     "=====================================\n"
                     "1. Изменить ФИО\n2. Изменить возраст\n3. Изменить телефон\n0. Назад\n"
                     "=====================================\n";
        auto choice = number("Выберите действие: "); if (!choice) continue;
        if (*choice == 0) return;
        json body = json::object();
        if (*choice == 1) { auto v = prompt("Новое ФИО: "); if (v.empty()) continue; body["name"] = v; }
        else if (*choice == 2) { auto v = number("Новый возраст: "); if (!v) continue; body["age"] = *v; }
        else if (*choice == 3) { auto v = prompt("Новый телефон: "); if (v.empty()) continue; body["phone"] = v; }
        else { std::cout << "Неизвестная команда.\n"; continue; }
        auto response = request("PATCH", "/clients/" + std::to_string(*id), body);
        if (response) { std::cout << "Данные клиента изменены.\n"; showClient(*response); }
    }
}
void BankCli::removeClient() {
    auto id = number("ID клиента: "); if (!id) return;
    if (!confirm("Подтвердите удаление клиента №" + std::to_string(*id))) return;
    if (request("DELETE", "/clients/" + std::to_string(*id))) std::cout << "Клиент удалён.\n";
}
void BankCli::accountsMenu() {
    while (!std::cin.eof()) {
        std::cout << "\n=====================================\n"
                     "          БАНКОВСКИЕ СЧЕТА\n"
                     "=====================================\n"
                     "1. Создать счёт\n2. Показать все счета\n3. Найти счёт\n"
                     "4. Показать счета клиента\n5. Посмотреть баланс\n6. Закрыть счёт\n"
                     "0. Назад\n=====================================\n";
        auto choice = number("Выберите действие: "); if (!choice) continue;
        if (*choice == 0) return;
        if (*choice == 1) {
            auto id = number("ID клиента: "); if (!id) continue;
            auto res = request("POST", "/accounts", json{{"client_id", *id}});
            if (res) { std::cout << "Счёт успешно открыт.\n"; showAccount(*res); }
        } else if (*choice == 2) {
            auto res = request("GET", "/accounts");
            if (res) {
                if (res->empty()) std::cout << "Счетов пока нет.\n";
                else for (const auto& a : *res) showAccount(a);
            }
        } else if (*choice == 3 || *choice == 5) {
            auto id = number("ID счёта: "); if (!id) continue;
            auto res = request("GET", "/accounts/" + std::to_string(*id));
            if (res) showAccount(*res);
        } else if (*choice == 4) {
            auto id = number("ID клиента: "); if (!id) continue;
            auto res = request("GET", "/clients/" + std::to_string(*id) + "/accounts");
            if (res) {
                if (res->empty()) std::cout << "У клиента нет счетов.\n";
                else for (const auto& a : *res) showAccount(a);
            }
        } else if (*choice == 6) {
            auto id = number("ID счёта: "); if (!id) continue;
            if (!confirm("Подтвердите закрытие счёта №" + std::to_string(*id))) continue;
            if (request("DELETE", "/accounts/" + std::to_string(*id)))
                std::cout << "Счёт закрыт.\n";
        } else std::cout << "Неизвестная команда.\n";
    }
}
void BankCli::historyMenu() {
    auto id = number("ID счёта: "); if (!id) return;
    auto response = request("GET", "/accounts/" + std::to_string(*id) + "/transactions");
    if (response) showHistory(*response, *id);
}
void BankCli::operationsMenu() {
    while (!std::cin.eof()) {
        std::cout << "\n=====================================\n"
                     "         БАНКОВСКИЕ ОПЕРАЦИИ\n"
                     "=====================================\n"
                     "1. Пополнить счёт\n2. Снять деньги\n3. Перевести деньги\n"
                     "4. Посмотреть историю операций\n0. Назад\n"
                     "=====================================\n";
        auto choice = number("Выберите действие: "); if (!choice) continue;
        if (*choice == 0) return;
        if (*choice == 1 || *choice == 2) {
            auto id = number("ID счёта: "); if (!id) continue;
            auto value = amount("Сумма"); if (!value) continue;
            const std::string action = *choice == 1 ? "deposit" : "withdraw";
            auto res = request("POST", "/accounts/" + std::to_string(*id) + "/" + action,
                               json{{"amount", *value}});
            if (res) { std::cout << "Операция выполнена успешно.\n"; showAccount(*res); }
        } else if (*choice == 3) {
            auto from = number("ID счёта отправителя: "); if (!from) continue;
            auto to = number("ID счёта получателя: "); if (!to) continue;
            auto value = amount("Сумма перевода"); if (!value) continue;
            auto res = request("POST", "/transfers", json{{"from_account_id", *from},
                               {"to_account_id", *to}, {"amount", *value}});
            if (res) {
                std::cout << "Перевод выполнен успешно. Остаток на счёте отправителя:\n";
                showAccount(res->at("sender"));
            }
        } else if (*choice == 4) historyMenu();
        else std::cout << "Неизвестная команда.\n";
    }
}
void BankCli::run() {
    // Проверка связи не блокирует меню: пользователь может запустить сервер позже.
    request("GET", "/health");
    while (!std::cin.eof()) {
        std::cout << "\n=====================================\n"
                     "         БАНКОВСКОЕ ПРИЛОЖЕНИЕ\n"
                     "=====================================\n"
                     "1. Добавить клиента\n2. Показать всех клиентов\n3. Найти клиента\n"
                     "4. Изменить данные клиента\n5. Удалить клиента\n6. Управление счетами\n"
                     "7. Банковские операции\n8. История операций\n0. Выход\n"
                     "=====================================\n";
        auto choice = number("Выберите действие: "); if (!choice) continue;
        switch (*choice) {
        case 0: std::cout << "Работа завершена.\n"; return;
        case 1: addClient(); break;
        case 2: listClients(); break;
        case 3: findClients(); break;
        case 4: editClient(); break;
        case 5: removeClient(); break;
        case 6: accountsMenu(); break;
        case 7: operationsMenu(); break;
        case 8: historyMenu(); break;
        default: std::cout << "Неизвестная команда. Выберите пункт меню.\n"; break;
        }
    }
}

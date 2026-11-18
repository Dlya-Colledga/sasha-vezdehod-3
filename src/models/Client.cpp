#include "models/Client.h"
#include "services/BankError.h"
#include <algorithm>
#include <cctype>
#include <utility>

Client::Client(int64_t id, std::string name, int age, std::string phone)
    : Person(std::move(name), age), id_(id), phone_() {
    if (id < 0) throw BankError(422, "invalid_id", "Недопустимый ID клиента");
    changePhone(phone);
}
std::string Client::getInfo() const {
    return "Клиент №" + std::to_string(id_) + ", " + Person::getInfo() + ", телефон: " + phone_;
}
void Client::changePhone(const std::string& phone) {
    if (phone.empty() || std::all_of(phone.begin(), phone.end(), [](unsigned char ch) {
        return std::isspace(ch) != 0;
    })) throw BankError(422, "invalid_phone", "Телефон не должен быть пустым");
    phone_ = phone;
}
int64_t Client::getId() const noexcept { return id_; }
const std::string& Client::getPhone() const noexcept { return phone_; }

#include "models/Person.h"
#include "services/BankError.h"
#include <algorithm>
#include <cctype>
#include <utility>

namespace {
bool isBlank(const std::string& value) {
    return std::all_of(value.begin(), value.end(), [](unsigned char ch) {
        return std::isspace(ch) != 0;
    });
}
}
Person::Person(std::string name, int age) : name_(std::move(name)), age_(age) {
    changeName(name_);
    changeAge(age_);
}
std::string Person::getInfo() const {
    return "ФИО: " + name_ + ", возраст: " + std::to_string(age_);
}
void Person::changeName(const std::string& name) {
    if (name.empty() || isBlank(name))
        throw BankError(422, "invalid_name", "ФИО не должно быть пустым");
    name_ = name;
}
void Person::changeAge(int age) {
    if (age < 18 || age > 100)
        throw BankError(422, "invalid_age", "Возраст должен быть от 18 до 100 лет");
    age_ = age;
}
const std::string& Person::getName() const noexcept { return name_; }
int Person::getAge() const noexcept { return age_; }

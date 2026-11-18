#pragma once
#include "models/Person.h"
#include <cstdint>
#include <string>

class Client : public Person {
public:
    Client(int64_t id, std::string name, int age, std::string phone);
    std::string getInfo() const override;
    void changePhone(const std::string& phone);
    int64_t getId() const noexcept;
    const std::string& getPhone() const noexcept;
private:
    int64_t id_;
    std::string phone_;
};

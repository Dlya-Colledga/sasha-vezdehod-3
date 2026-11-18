#pragma once
#include <string>

class Person {
public:
    Person(std::string name, int age);
    virtual ~Person() = default;
    virtual std::string getInfo() const;
    void changeName(const std::string& name);
    void changeAge(int age);
    const std::string& getName() const noexcept;
    int getAge() const noexcept;
private:
    std::string name_;
    int age_;
};

#include "BankCli.h"
#include <cstdlib>
#include <iostream>
#include <string>

int main() {
    const char* env = std::getenv("API_URL");
    const std::string url = env && *env ? env : "http://127.0.0.1:8000";
    try {
        BankCli cli(url);
        cli.run();
    } catch (const std::exception& e) {
        std::cerr << "Ошибка запуска консольного клиента: " << e.what() << '\n';
        return 1;
    }
    return 0;
}

#include "api/BankApi.h"
#include <iostream>

int main() {
    try {
        BankService service("data/bank.db");
        BankApi api(service);
        std::cout << "Банковский сервер: http://127.0.0.1:8000\n"
                     "База данных: data/bank.db\nДля остановки нажмите Ctrl+C.\n";
        if (!api.start()) {
            std::cerr << "Ошибка: не удалось запустить сервер. Возможно, порт 8000 занят.\n";
            return 1;
        }
    } catch (const std::exception& e) {
        std::cerr << "Ошибка сервера: " << e.what() << '\n';
        return 1;
    }
    return 0;
}

#pragma once
#include "services/BankService.h"
#include <httplib.h>

class BankApi {
public:
    explicit BankApi(BankService& service);
    bool start(const std::string& host = "127.0.0.1", int port = 8000);
private:
    void registerRoutes();
    BankService& service_;
    httplib::Server server_;
};

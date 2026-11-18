cmake_minimum_required(VERSION 3.21)
project(banking_app VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Установку внешних зависимостей выполняет vcpkg по манифесту vcpkg.json.
find_package(httplib CONFIG REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)
find_package(unofficial-sqlite3 CONFIG REQUIRED)
find_package(Threads REQUIRED)

add_library(bank_core STATIC
    src/models/Person.cpp
    src/models/Client.cpp
    src/models/BankAccount.cpp
    src/models/Transaction.cpp
    src/services/BankService.cpp
    src/storage/Database.cpp
    src/api/BankApi.cpp
)
target_include_directories(bank_core PUBLIC "${PROJECT_SOURCE_DIR}/include")
target_link_libraries(bank_core
    PUBLIC httplib::httplib Threads::Threads
    PRIVATE nlohmann_json::nlohmann_json unofficial::sqlite3::sqlite3
)

add_executable(bank_server src/server/main.cpp)
target_link_libraries(bank_server PRIVATE bank_core)

add_executable(bank_cli src/cli/main.cpp src/cli/BankCli.cpp)
target_include_directories(bank_cli PRIVATE "${PROJECT_SOURCE_DIR}/src/cli")
target_link_libraries(bank_cli PRIVATE httplib::httplib nlohmann_json::nlohmann_json Threads::Threads)

foreach(target bank_core bank_server bank_cli)
    target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic)
endforeach()

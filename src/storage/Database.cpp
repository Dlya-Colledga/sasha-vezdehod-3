#include "storage/Database.h"
#include <sqlite3.h>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
void ensure(int rc, sqlite3* db) {
    if (rc != SQLITE_OK && rc != SQLITE_DONE && rc != SQLITE_ROW)
        throw std::runtime_error("Ошибка SQLite: " + std::string(sqlite3_errmsg(db)));
}

class Statement {
public:
    Statement(sqlite3* db, const char* sql) : db_(db) {
        ensure(sqlite3_prepare_v2(db_, sql, -1, &stmt_, nullptr), db_);
    }
    ~Statement() { if (stmt_) sqlite3_finalize(stmt_); }
    Statement(const Statement&) = delete;
    Statement& operator=(const Statement&) = delete;
    void integer(int index, int64_t value) { ensure(sqlite3_bind_int64(stmt_, index, value), db_); }
    void string(int index, const std::string& value) {
        ensure(sqlite3_bind_text(stmt_, index, value.c_str(), static_cast<int>(value.size()), SQLITE_TRANSIENT), db_);
    }
    void nullValue(int index) { ensure(sqlite3_bind_null(stmt_, index), db_); }
    bool next() {
        int rc = sqlite3_step(stmt_);
        ensure(rc, db_);
        return rc == SQLITE_ROW;
    }
    int64_t number(int column) const { return sqlite3_column_int64(stmt_, column); }
    std::string text(int column) const {
        const auto* ptr = sqlite3_column_text(stmt_, column);
        return ptr ? reinterpret_cast<const char*>(ptr) : "";
    }
    bool isNull(int column) const { return sqlite3_column_type(stmt_, column) == SQLITE_NULL; }
private:
    sqlite3* db_;
    sqlite3_stmt* stmt_ = nullptr;
};

Client readClient(const Statement& stmt) {
    return Client(stmt.number(0), stmt.text(1), static_cast<int>(stmt.number(2)), stmt.text(3));
}
BankAccount readAccount(const Statement& stmt) {
    return BankAccount(stmt.number(0), stmt.number(1), stmt.number(2), stmt.text(3));
}
Transaction readTransaction(const Statement& stmt) {
    std::optional<int64_t> target;
    if (!stmt.isNull(2)) target = stmt.number(2);
    return Transaction(stmt.number(0), stmt.number(1), target, stmt.text(3), stmt.number(4), stmt.text(5));
}
} // namespace

Database::Database(const std::filesystem::path& path) {
    if (path.has_parent_path()) std::filesystem::create_directories(path.parent_path());
    const std::string dbFilename = path.string();
    int rc = sqlite3_open_v2(dbFilename.c_str(), &handle_,
                             SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE | SQLITE_OPEN_FULLMUTEX, nullptr);
    if (rc != SQLITE_OK) {
        std::string detail = handle_ ? sqlite3_errmsg(handle_) : "невозможно открыть соединение";
        if (handle_) sqlite3_close(handle_);
        handle_ = nullptr;
        throw std::runtime_error("Не удалось открыть базу данных: " + detail);
    }
    try {
        sqlite3_busy_timeout(handle_, 5000);
        exec("PRAGMA foreign_keys = ON;");
        initialize();
    } catch (...) {
        sqlite3_close(handle_);
        handle_ = nullptr;
        throw;
    }
}
Database::~Database() { if (handle_) sqlite3_close(handle_); }

void Database::exec(const char* sql) {
    char* error = nullptr;
    int rc = sqlite3_exec(handle_, sql, nullptr, nullptr, &error);
    if (rc != SQLITE_OK) {
        std::string detail = error ? error : sqlite3_errmsg(handle_);
        sqlite3_free(error);
        throw std::runtime_error("Ошибка SQLite: " + detail);
    }
}
void Database::initialize() {
    exec(R"SQL(
CREATE TABLE IF NOT EXISTS clients (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    name TEXT NOT NULL CHECK(length(trim(name)) > 0),
    age INTEGER NOT NULL CHECK(age BETWEEN 18 AND 100),
    phone TEXT NOT NULL CHECK(length(trim(phone)) > 0)
);
CREATE TABLE IF NOT EXISTS accounts (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    client_id INTEGER NOT NULL REFERENCES clients(id) ON DELETE RESTRICT,
    balance_cents INTEGER NOT NULL DEFAULT 0 CHECK(balance_cents >= 0),
    currency TEXT NOT NULL DEFAULT 'KGS' CHECK(currency = 'KGS')
);
CREATE TABLE IF NOT EXISTS transactions (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    account_id INTEGER NOT NULL REFERENCES accounts(id) ON DELETE RESTRICT,
    target_account_id INTEGER REFERENCES accounts(id) ON DELETE RESTRICT,
    type TEXT NOT NULL CHECK(type IN ('deposit', 'withdraw', 'transfer')),
    amount_cents INTEGER NOT NULL CHECK(amount_cents > 0),
    created_at TEXT NOT NULL,
    CHECK ((type = 'transfer' AND target_account_id IS NOT NULL AND target_account_id != account_id)
        OR (type != 'transfer' AND target_account_id IS NULL))
);
CREATE INDEX IF NOT EXISTS idx_accounts_client ON accounts(client_id);
CREATE INDEX IF NOT EXISTS idx_transactions_account ON transactions(account_id);
CREATE INDEX IF NOT EXISTS idx_transactions_target ON transactions(target_account_id);
)SQL");
}
Database::TransactionScope::TransactionScope(Database& database) : database_(database) {
    database_.exec("BEGIN IMMEDIATE TRANSACTION;");
}
Database::TransactionScope::~TransactionScope() noexcept {
    if (active_) {
        try { database_.exec("ROLLBACK;"); } catch (...) {}
    }
}
void Database::TransactionScope::commit() {
    database_.exec("COMMIT;");
    active_ = false;
}

int64_t Database::insertClient(const Client& client) {
    Statement s(handle_, "INSERT INTO clients(name, age, phone) VALUES(?, ?, ?);");
    s.string(1, client.getName()); s.integer(2, client.getAge()); s.string(3, client.getPhone());
    s.next();
    return sqlite3_last_insert_rowid(handle_);
}
std::vector<Client> Database::listClients() {
    Statement s(handle_, "SELECT id, name, age, phone FROM clients ORDER BY id;");
    std::vector<Client> out;
    while (s.next()) out.push_back(readClient(s));
    return out;
}
std::optional<Client> Database::selectClient(int64_t id) {
    Statement s(handle_, "SELECT id, name, age, phone FROM clients WHERE id = ?;");
    s.integer(1, id);
    if (s.next()) return readClient(s);
    return std::nullopt;
}
void Database::updateClient(const Client& client) {
    Statement s(handle_, "UPDATE clients SET name = ?, age = ?, phone = ? WHERE id = ?;");
    s.string(1, client.getName()); s.integer(2, client.getAge());
    s.string(3, client.getPhone()); s.integer(4, client.getId()); s.next();
}
void Database::deleteClient(int64_t id) {
    Statement s(handle_, "DELETE FROM clients WHERE id = ?;");
    s.integer(1, id); s.next();
}
bool Database::clientHasAccounts(int64_t clientId) {
    Statement s(handle_, "SELECT EXISTS(SELECT 1 FROM accounts WHERE client_id = ?);");
    s.integer(1, clientId); s.next();
    return s.number(0) != 0;
}

int64_t Database::insertAccount(int64_t clientId) {
    Statement s(handle_, "INSERT INTO accounts(client_id, balance_cents, currency) VALUES(?, 0, 'KGS');");
    s.integer(1, clientId); s.next();
    return sqlite3_last_insert_rowid(handle_);
}
std::vector<BankAccount> Database::listAccounts() {
    Statement s(handle_, "SELECT id, client_id, balance_cents, currency FROM accounts ORDER BY id;");
    std::vector<BankAccount> out;
    while (s.next()) out.push_back(readAccount(s));
    return out;
}
std::optional<BankAccount> Database::selectAccount(int64_t id) {
    Statement s(handle_, "SELECT id, client_id, balance_cents, currency FROM accounts WHERE id = ?;");
    s.integer(1, id);
    if (s.next()) return readAccount(s);
    return std::nullopt;
}
std::vector<BankAccount> Database::clientAccounts(int64_t clientId) {
    Statement s(handle_, "SELECT id, client_id, balance_cents, currency FROM accounts WHERE client_id = ? ORDER BY id;");
    s.integer(1, clientId);
    std::vector<BankAccount> out;
    while (s.next()) out.push_back(readAccount(s));
    return out;
}
void Database::updateBalance(const BankAccount& account) {
    Statement s(handle_, "UPDATE accounts SET balance_cents = ? WHERE id = ?;");
    s.integer(1, account.getBalance()); s.integer(2, account.getId()); s.next();
    if (sqlite3_changes(handle_) != 1)
        throw std::runtime_error("Ошибка обновления баланса: счёт не найден");
}
bool Database::accountHasTransactions(int64_t accountId) {
    Statement s(handle_, "SELECT EXISTS(SELECT 1 FROM transactions WHERE account_id = ? OR target_account_id = ?);");
    s.integer(1, accountId); s.integer(2, accountId); s.next();
    return s.number(0) != 0;
}
void Database::deleteAccount(int64_t id) {
    Statement s(handle_, "DELETE FROM accounts WHERE id = ?;");
    s.integer(1, id); s.next();
}
int64_t Database::insertTransaction(const Transaction& tx) {
    Statement s(handle_, "INSERT INTO transactions(account_id, target_account_id, type, amount_cents, created_at) "
                         "VALUES(?, ?, ?, ?, ?);");
    s.integer(1, tx.getAccountId());
    if (tx.getTargetAccountId()) s.integer(2, *tx.getTargetAccountId());
    else s.nullValue(2);
    s.string(3, tx.getType()); s.integer(4, tx.getAmount()); s.string(5, tx.getCreatedAt()); s.next();
    return sqlite3_last_insert_rowid(handle_);
}
std::vector<Transaction> Database::listTransactions(int64_t accountId) {
    Statement s(handle_, "SELECT id, account_id, target_account_id, type, amount_cents, created_at "
                         "FROM transactions WHERE account_id = ? OR target_account_id = ? ORDER BY id DESC;");
    s.integer(1, accountId); s.integer(2, accountId);
    std::vector<Transaction> out;
    while (s.next()) out.push_back(readTransaction(s));
    return out;
}

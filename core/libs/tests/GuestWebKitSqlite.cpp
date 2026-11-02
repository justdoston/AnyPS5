#include "prx/libSceNKWebKitRequirements/libSceNKWebKitRequirements.hpp"
#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <stdexcept>
#include <string>
#include <vector>

using NKWebKitRequirements::GuestSqlite;
using NKWebKitRequirements::GuestSqliteContext;
using NKWebKitRequirements::GuestSqliteStatement;
using NKWebKitRequirements::GuestSqliteValue;

namespace {

constexpr int Ok = 0;
constexpr int Error = 1;
constexpr int Busy = 5;
constexpr int Interrupt = 9;
constexpr int Constraint = 19;
constexpr int Auth = 23;
constexpr int Row = 100;
constexpr int Done = 101;
constexpr int ConstraintPrimaryKey = 1555;
constexpr int OpenReadWrite = 0x2;
constexpr int OpenCreate = 0x4;
constexpr int OpenUri = 0x40;
constexpr int Utf8 = 1;
constexpr int Utf16 = 4;
constexpr int Integer = 1;
constexpr int Float = 2;
constexpr int Text = 3;
constexpr int Blob = 4;
constexpr int Null = 5;
constexpr int Read = 20;
constexpr int Deny = 1;

void Require(bool value, const char* message) {
    if (value) return;
    std::fprintf(stderr, "WebKit SQLite: %s\n", message);
    std::abort();
}

template<typename TException, typename TAction>
bool Throws(TAction action) {
    try { action(); } catch (const TException&) { return true; }
    return false;
}

GuestSqliteStatement* Prepare(GuestSqlite* database, const char* sql) {
    GuestSqliteStatement* statement = nullptr;
    Require(sqlite3_prepare_v2_nid_postfix(database, sql, -1, &statement, nullptr) == Ok && statement, sql);
    return statement;
}

void Execute(GuestSqlite* database, const char* sql) {
    const char* tail = sql;
    while (tail && *tail) {
        GuestSqliteStatement* statement = nullptr;
        Require(sqlite3_prepare_v2_nid_postfix(database, tail, -1, &statement, &tail) == Ok, sql);
        if (!statement) break;
        int result;
        while ((result = sqlite3_step_nid_postfix(statement)) == Row) {}
        Require(result == Done, sql);
        Require(sqlite3_finalize_nid_postfix(statement) == Ok, sql);
    }
}

std::u16string Utf16Text(const void* text) {
    return text ? std::u16string(static_cast<const char16_t*>(text)) : std::u16string();
}

int destructorCalls;
void* destructorValue;
void APS5_VABI Destroy(void* value) {
    ++destructorCalls;
    destructorValue = value;
}

int marker;
int scalarCalls;
std::vector<int> scalarTypes;
void APS5_VABI Scalar(GuestSqliteContext* context, std::int32_t count, GuestSqliteValue** values) {
    ++scalarCalls;
    Require(count == 1 && sqlite3_user_data_nid_postfix(context) == &marker, "scalar user data");
    const int type = sqlite3_value_type_nid_postfix(values[0]);
    scalarTypes.push_back(type);
    if (type == Text) {
        Require(Utf16Text(sqlite3_value_text16_nid_postfix(values[0])) == u"bad", "scalar text argument");
        sqlite3_result_error_nid_postfix(context, "scalar refused text", -1);
    } else if (type == Float) {
        Require(sqlite3_value_double_nid_postfix(values[0]) == 2.5, "scalar float argument");
    }
}

int aggregateSteps;
void APS5_VABI AggregateStep(GuestSqliteContext*, std::int32_t, GuestSqliteValue**) {
    ++aggregateSteps;
}

void APS5_VABI AggregateFinal(GuestSqliteContext* context) {
    const std::string message = "steps=" + std::to_string(aggregateSteps);
    sqlite3_result_error_nid_postfix(context, message.c_str(), -1);
}

int compareCalls;
void* collationDestroyed;
std::int32_t APS5_VABI Reverse(void* argument, std::int32_t leftSize, const void* left, std::int32_t rightSize, const void* right) {
    ++compareCalls;
    Require(argument == &marker, "collation argument");
    const int common = std::memcmp(right, left, static_cast<std::size_t>(std::min(leftSize, rightSize)));
    return common != 0 ? common : rightSize - leftSize;
}

void APS5_VABI CollationDestroy(void* argument) {
    collationDestroyed = argument;
}

int authorizerCalls;
std::int32_t APS5_VABI Authorize(void* argument, std::int32_t action, const char* first, const char*, const char*, const char*) {
    ++authorizerCalls;
    Require(argument == &marker, "authorizer argument");
    return action == Read && first && std::strcmp(first, "secret") == 0 ? Deny : Ok;
}

int walCalls;
int walPages;
std::string walName;
std::int32_t APS5_VABI WalHook(void* argument, GuestSqlite*, const char* name, std::int32_t pages) {
    ++walCalls;
    Require(argument == &marker, "WAL hook argument");
    walName = name;
    walPages = pages;
    return Ok;
}

std::vector<int> busyCounts;
std::int32_t APS5_VABI BusyHandler(void* argument, std::int32_t count) {
    Require(argument == &marker, "busy handler argument");
    busyCounts.push_back(count);
    return count < 2;
}

void Values(GuestSqlite* database) {
    Execute(database, "CREATE TABLE t(id INTEGER PRIMARY KEY, n INTEGER, r REAL, s TEXT, b BLOB, x)");
    GuestSqliteStatement* insert = Prepare(database, "INSERT INTO t(n, r, s, b, x) VALUES (?, ?, ?, ?, ?)");
    Require(sqlite3_bind_parameter_count_nid_postfix(insert) == 5, "parameter count");
    static const char16_t text[] = u"héllo";
    char16_t transient[sizeof(text) / sizeof(text[0])];
    std::memcpy(transient, text, sizeof(text));
    static std::uint8_t bytes[] = {1, 2, 0, 3};
    Require(sqlite3_bind_int_nid_postfix(insert, 1, 42) == Ok && sqlite3_bind_double_nid_postfix(insert, 2, 2.5) == Ok, "bind numbers");
    Require(sqlite3_bind_text16_nid_postfix(insert, 3, transient, -1, reinterpret_cast<NKWebKitRequirements::GuestSqliteDestructor>(-1)) == Ok, "bind transient text");
    std::memset(transient, 'x', sizeof(transient) - sizeof(char16_t));
    destructorCalls = 0;
    Require(sqlite3_bind_blob_nid_postfix(insert, 4, bytes, sizeof(bytes), Destroy) == Ok, "bind blob with destructor");
    Require(sqlite3_bind_null_nid_postfix(insert, 5) == Ok && destructorCalls == 0, "bind null");
    Require(sqlite3_step_nid_postfix(insert) == Done && sqlite3_last_insert_rowid_nid_postfix(database) == 1, "insert row");
    Require(sqlite3_reset_nid_postfix(insert) == Ok && sqlite3_clear_bindings_nid_postfix(insert) == Ok, "reset and clear");
    Require(destructorCalls == 1 && destructorValue == bytes, "clearing calls the guest destructor once");
    Require(sqlite3_bind_int64_nid_postfix(insert, 1, 0x123456789A) == Ok, "bind int64");
    std::uint8_t copied[3] = {7, 8, 9};
    Require(sqlite3_bind_blob_nid_postfix(insert, 4, copied, 3, reinterpret_cast<NKWebKitRequirements::GuestSqliteDestructor>(-1)) == Ok, "bind transient blob");
    std::memset(copied, 0, sizeof(copied));
    Require(sqlite3_step_nid_postfix(insert) == Done && sqlite3_total_changes_nid_postfix(database) == 2, "second row");
    Require(sqlite3_reset_nid_postfix(insert) == Ok && sqlite3_bind_blob_nid_postfix(insert, 4, bytes, 2, nullptr) == Ok, "bind static blob");
    Require(sqlite3_finalize_nid_postfix(insert) == Ok && destructorCalls == 1, "static blob has no destructor");
    Require(sqlite3_bind_blob_nid_postfix(nullptr, 1, bytes, 1, Destroy) != Ok && destructorCalls == 2, "failed bind still runs the destructor");

    GuestSqliteStatement* select = Prepare(database, "SELECT n, r, s, b, x FROM t ORDER BY id");
    Require(sqlite3_stmt_busy_nid_postfix(select) == 0 && sqlite3_data_count_nid_postfix(select) == 0, "fresh statement");
    Require(std::string(sqlite3_sql_nid_postfix(select)) == "SELECT n, r, s, b, x FROM t ORDER BY id", "statement SQL");
    Require(sqlite3_next_stmt_nid_postfix(database, nullptr) == select && sqlite3_next_stmt_nid_postfix(database, select) == nullptr, "statement list");
    Require(sqlite3_step_nid_postfix(select) == Row && sqlite3_stmt_busy_nid_postfix(select) == 1 && sqlite3_data_count_nid_postfix(select) == 5, "first row");
    Require(Utf16Text(sqlite3_column_name16_nid_postfix(select, 2)) == u"s", "column name");
    Require(std::string(sqlite3_column_decltype_nid_postfix(select, 1)) == "REAL" && sqlite3_column_decltype_nid_postfix(select, 4) == nullptr, "declared types");
    Require(sqlite3_column_type_nid_postfix(select, 0) == Integer && sqlite3_column_int_nid_postfix(select, 0) == 42, "integer column");
    Require(sqlite3_column_type_nid_postfix(select, 1) == Float && sqlite3_column_double_nid_postfix(select, 1) == 2.5, "real column");
    Require(sqlite3_column_type_nid_postfix(select, 2) == Text && Utf16Text(sqlite3_column_text16_nid_postfix(select, 2)) == text, "text column");
    Require(sqlite3_column_bytes16_nid_postfix(select, 2) == 10, "text bytes");
    Require(sqlite3_column_type_nid_postfix(select, 3) == Blob && sqlite3_column_bytes_nid_postfix(select, 3) == 4, "blob column");
    Require(std::memcmp(sqlite3_column_blob_nid_postfix(select, 3), bytes, 4) == 0, "blob bytes");
    Require(sqlite3_column_type_nid_postfix(select, 4) == Null, "null column");
    GuestSqliteValue* value = sqlite3_column_value_nid_postfix(select, 1);
    Require(sqlite3_value_type_nid_postfix(value) == Float && sqlite3_value_double_nid_postfix(value) == 2.5, "column value");
    Require(sqlite3_step_nid_postfix(select) == Row && sqlite3_column_int64_nid_postfix(select, 0) == 0x123456789A, "second row");
    static const std::uint8_t copiedBytes[] = {7, 8, 9};
    Require(sqlite3_column_bytes_nid_postfix(select, 3) == 3 && std::memcmp(sqlite3_column_blob_nid_postfix(select, 3), copiedBytes, 3) == 0, "transient blob was copied");
    Require(sqlite3_column_type_nid_postfix(select, 2) == Null, "second row text");
    Require(sqlite3_step_nid_postfix(select) == Done && sqlite3_finalize_nid_postfix(select) == Ok, "select done");
}

void Errors(GuestSqlite* database) {
    GuestSqliteStatement* statement = nullptr;
    Require(sqlite3_prepare_v2_nid_postfix(database, "SELEC 1", -1, &statement, nullptr) == Error && !statement, "syntax error");
    Require(sqlite3_errcode_nid_postfix(database) == Error && std::string(sqlite3_errmsg_nid_postfix(database)).find("syntax error") != std::string::npos, "error message");
    Require(std::string(sqlite3_errstr_nid_postfix(Busy)) == "database is locked", "error string");
    GuestSqliteStatement* duplicate = Prepare(database, "INSERT INTO t(id) VALUES (1)");
    Require(sqlite3_step_nid_postfix(duplicate) == Constraint, "basic constraint code");
    Require(sqlite3_finalize_nid_postfix(duplicate) == Constraint, "finalize reports the error");
    Require(sqlite3_extended_result_codes_nid_postfix(database, 1) == Ok, "extended codes");
    duplicate = Prepare(database, "INSERT INTO t(id) VALUES (1)");
    Require(sqlite3_step_nid_postfix(duplicate) == ConstraintPrimaryKey && sqlite3_errcode_nid_postfix(database) == ConstraintPrimaryKey, "extended constraint code");
    sqlite3_finalize_nid_postfix(duplicate);
    sqlite3_extended_result_codes_nid_postfix(database, 0);
    Require(sqlite3_get_autocommit_nid_postfix(database) == 1, "autocommit");
    Execute(database, "BEGIN");
    Require(sqlite3_get_autocommit_nid_postfix(database) == 0, "inside a transaction");
    Execute(database, "COMMIT");
    GuestSqliteStatement* slow = Prepare(database, "WITH RECURSIVE c(i) AS (SELECT 1 UNION ALL SELECT i + 1 FROM c) SELECT i FROM c");
    Require(sqlite3_step_nid_postfix(slow) == Row, "recursive query runs");
    sqlite3_interrupt_nid_postfix(database);
    int result;
    while ((result = sqlite3_step_nid_postfix(slow)) == Row) {}
    Require(result == Interrupt, "interrupt");
    sqlite3_finalize_nid_postfix(slow);
}

void Functions(GuestSqlite* database) {
    Require(sqlite3_create_function_nid_postfix(database, "probe", 1, Utf8, &marker, Scalar, nullptr, nullptr) == Ok, "create scalar");
    GuestSqliteStatement* statement = Prepare(database, "SELECT probe(2.5)");
    Require(sqlite3_step_nid_postfix(statement) == Row && sqlite3_column_type_nid_postfix(statement, 0) == Null, "scalar without a result is NULL");
    sqlite3_finalize_nid_postfix(statement);
    statement = Prepare(database, "SELECT probe('bad')");
    Require(sqlite3_step_nid_postfix(statement) == Error && std::string(sqlite3_errmsg_nid_postfix(database)) == "scalar refused text", "scalar error");
    sqlite3_finalize_nid_postfix(statement);
    Require(scalarCalls == 2 && scalarTypes == std::vector<int>{Float, Text}, "scalar calls");
    Require(sqlite3_create_function_nid_postfix(database, "both", 1, Utf8, nullptr, Scalar, AggregateStep, AggregateFinal) != Ok, "scalar and aggregate together are misuse");
    Require(sqlite3_create_function_nid_postfix(database, "tally", 1, Utf16, &marker, nullptr, AggregateStep, AggregateFinal) == Ok, "create aggregate");
    statement = Prepare(database, "SELECT tally(n) FROM t");
    Require(sqlite3_step_nid_postfix(statement) == Error && std::string(sqlite3_errmsg_nid_postfix(database)) == "steps=2", "aggregate runs every step then final");
    sqlite3_finalize_nid_postfix(statement);

    Require(sqlite3_create_collation_v2_nid_postfix(database, "reverse", Utf8, &marker, Reverse, CollationDestroy) == Ok, "create collation");
    Execute(database, "CREATE TABLE words(w TEXT); INSERT INTO words VALUES ('b'), ('a'), ('c')");
    statement = Prepare(database, "SELECT w FROM words ORDER BY w COLLATE reverse");
    std::u16string order;
    while (sqlite3_step_nid_postfix(statement) == Row) order += Utf16Text(sqlite3_column_text16_nid_postfix(statement, 0));
    sqlite3_finalize_nid_postfix(statement);
    Require(order == u"cba" && compareCalls > 0, "collation order");
    Require(sqlite3_create_collation_v2_nid_postfix(database, "reverse", 99, &marker, Reverse, CollationDestroy) != Ok, "bad collation encoding");

    Require(sqlite3_set_authorizer_nid_postfix(database, Authorize, &marker) == Ok, "set authorizer");
    Execute(database, "CREATE TABLE secret(v)");
    GuestSqliteStatement* denied = nullptr;
    Require(sqlite3_prepare_v2_nid_postfix(database, "SELECT v FROM secret", -1, &denied, nullptr) == Auth && !denied && authorizerCalls > 0, "authorizer denies reads");
    Require(sqlite3_set_authorizer_nid_postfix(database, nullptr, nullptr) == Ok, "clear authorizer");
    statement = Prepare(database, "SELECT v FROM secret");
    sqlite3_finalize_nid_postfix(statement);
}

void Files() {
    const auto path = (std::filesystem::temp_directory_path() / "anyps5-webkit-sqlite.db").string();
    std::filesystem::remove(path);
    std::filesystem::remove(path + "-wal");
    std::filesystem::remove(path + "-shm");
    GuestSqlite* first = nullptr;
    GuestSqlite* second = nullptr;
    Require(sqlite3_open_v2_nid_postfix(path.c_str(), &first, OpenReadWrite | OpenCreate, nullptr) == Ok && first, "open file database");
    Require(sqlite3_open_v2_nid_postfix(path.c_str(), &second, OpenReadWrite, nullptr) == Ok && second, "second connection");
    Execute(first, "PRAGMA journal_mode=WAL; CREATE TABLE k(v)");
    Require(sqlite3_wal_hook_nid_postfix(first, WalHook, &marker) == reinterpret_cast<void*>(1000), "WAL hook replaces the automatic checkpoint");
    Execute(first, "INSERT INTO k VALUES (1)");
    Require(walCalls == 1 && walName == "main" && walPages > 0, "WAL hook called on commit");
    Require(sqlite3_wal_hook_nid_postfix(first, nullptr, nullptr) == &marker, "WAL hook returns the guest argument");
    std::int32_t log = -1;
    std::int32_t checkpointed = -1;
    Require(sqlite3_wal_checkpoint_v2_nid_postfix(first, "main", 0, &log, &checkpointed) == Ok && log >= 0 && checkpointed >= 0, "checkpoint");

    Require(sqlite3_busy_handler_nid_postfix(second, BusyHandler, &marker) == Ok, "busy handler");
    Execute(first, "BEGIN IMMEDIATE");
    GuestSqliteStatement* write = Prepare(second, "BEGIN IMMEDIATE");
    Require(sqlite3_step_nid_postfix(write) == Busy && busyCounts == std::vector<int>{0, 1, 2}, "busy handler retries then gives up");
    sqlite3_finalize_nid_postfix(write);
    Require(sqlite3_busy_timeout_nid_postfix(second, 0) == Ok, "busy timeout clears the handler");
    write = Prepare(second, "BEGIN IMMEDIATE");
    Require(sqlite3_step_nid_postfix(write) == Busy && busyCounts.size() == 3, "no guest handler after busy timeout");
    sqlite3_finalize_nid_postfix(write);
    Execute(first, "COMMIT");
    GuestSqliteStatement* pending = Prepare(first, "SELECT v FROM k");
    Require(sqlite3_close_nid_postfix(first) == Busy, "close with a live statement");
    sqlite3_finalize_nid_postfix(pending);
    Require(sqlite3_close_nid_postfix(first) == Ok && sqlite3_close_nid_postfix(second) == Ok, "close both");
    std::filesystem::remove(path);
    std::filesystem::remove(path + "-wal");
    std::filesystem::remove(path + "-shm");

    GuestSqlite* unused = nullptr;
    Require(Throws<std::runtime_error>([&] { sqlite3_open_v2_nid_postfix(":memory:", &unused, OpenReadWrite, "unix-dotfile"); }), "named VFS throws");
    Require(Throws<std::runtime_error>([&] { sqlite3_open_v2_nid_postfix("file:x.db", &unused, OpenReadWrite | OpenCreate | OpenUri, nullptr); }), "URI throws");
    Require(Throws<std::invalid_argument>([&] { sqlite3_open_v2_nid_postfix(":memory:", nullptr, OpenReadWrite, nullptr); }), "null connection throws");
    Require(Throws<std::invalid_argument>([&] { sqlite3_open_v2_nid_postfix(nullptr, &unused, OpenReadWrite, nullptr); }), "null name throws");
    Require(sqlite3_close_nid_postfix(nullptr) == Ok, "closing null");
}

}

int main() {
    Require(sqlite3_initialize_nid_postfix() == Ok, "initialize");
    GuestSqlite* database = nullptr;
    Require(sqlite3_open_v2_nid_postfix(":memory:", &database, OpenReadWrite | OpenCreate, nullptr) == Ok && database, "open memory database");
    Values(database);
    Errors(database);
    Functions(database);
    Require(sqlite3_close_nid_postfix(database) == Ok && collationDestroyed == &marker, "close runs the collation destructor");
    Files();
    return 0;
}

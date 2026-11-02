#include "prx/libSceNKWebKitRequirements/libSceNKWebKitRequirements.hpp"
#include "prx/libc/include/General.hpp"
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>
#include "sqlite3.h"

namespace {

using NKWebKitRequirements::GuestSqlite;
using NKWebKitRequirements::GuestSqliteAuthorizer;
using NKWebKitRequirements::GuestSqliteBusyHandler;
using NKWebKitRequirements::GuestSqliteCompare;
using NKWebKitRequirements::GuestSqliteContext;
using NKWebKitRequirements::GuestSqliteDestructor;
using NKWebKitRequirements::GuestSqliteFinal;
using NKWebKitRequirements::GuestSqliteFunction;
using NKWebKitRequirements::GuestSqliteStatement;
using NKWebKitRequirements::GuestSqliteValue;
using NKWebKitRequirements::GuestSqliteWalHook;

struct FunctionRecord {
    GuestSqliteFunction function;
    GuestSqliteFunction step;
    GuestSqliteFinal final;
    void* application;
};

struct CollationRecord {
    GuestSqliteCompare compare;
    GuestSqliteDestructor destroy;
    void* argument;
};

struct Connection {
    GuestSqliteBusyHandler busy = nullptr;
    void* busyArgument = nullptr;
    GuestSqliteAuthorizer authorizer = nullptr;
    void* authorizerArgument = nullptr;
    GuestSqliteWalHook walHook = nullptr;
    void* walArgument = nullptr;
    std::vector<std::unique_ptr<FunctionRecord>> functions;
};

std::mutex connectionMutex;
std::unordered_map<sqlite3*, std::unique_ptr<Connection>> connections;
std::mutex destructorMutex;
std::unordered_multimap<const void*, GuestSqliteDestructor> destructors;

sqlite3* Host(GuestSqlite* database) {
    return reinterpret_cast<sqlite3*>(database);
}

sqlite3_stmt* Host(GuestSqliteStatement* statement) {
    return reinterpret_cast<sqlite3_stmt*>(statement);
}

sqlite3_value* Host(GuestSqliteValue* value) {
    return reinterpret_cast<sqlite3_value*>(value);
}

sqlite3_context* Host(GuestSqliteContext* context) {
    return reinterpret_cast<sqlite3_context*>(context);
}

Connection& Find(sqlite3* database) {
    std::lock_guard lock(connectionMutex);
    const auto found = connections.find(database);
    if (found == connections.end()) throw std::runtime_error("sqlite3: connection was not opened by sqlite3_open_v2");
    return *found->second;
}

int HostBusy(void* connection, int count) {
    auto* self = static_cast<Connection*>(connection);
    return self->busy(self->busyArgument, count);
}

int HostAuthorizer(void* connection, int action, const char* first, const char* second, const char* database, const char* trigger) {
    auto* self = static_cast<Connection*>(connection);
    return self->authorizer(self->authorizerArgument, action, first, second, database, trigger);
}

int HostWalHook(void* connection, sqlite3* database, const char* name, int pages) {
    auto* self = static_cast<Connection*>(connection);
    return self->walHook(self->walArgument, reinterpret_cast<GuestSqlite*>(database), name, pages);
}

void HostFunction(sqlite3_context* context, int count, sqlite3_value** values) {
    auto* record = static_cast<FunctionRecord*>(sqlite3_user_data(context));
    record->function(reinterpret_cast<GuestSqliteContext*>(context), count, reinterpret_cast<GuestSqliteValue**>(values));
}

void HostStep(sqlite3_context* context, int count, sqlite3_value** values) {
    auto* record = static_cast<FunctionRecord*>(sqlite3_user_data(context));
    record->step(reinterpret_cast<GuestSqliteContext*>(context), count, reinterpret_cast<GuestSqliteValue**>(values));
}

void HostFinal(sqlite3_context* context) {
    auto* record = static_cast<FunctionRecord*>(sqlite3_user_data(context));
    record->final(reinterpret_cast<GuestSqliteContext*>(context));
}

int HostCompare(void* record, int leftSize, const void* left, int rightSize, const void* right) {
    auto* self = static_cast<CollationRecord*>(record);
    return self->compare(self->argument, leftSize, left, rightSize, right);
}

void HostCollationDestroy(void* record) {
    std::unique_ptr<CollationRecord> self(static_cast<CollationRecord*>(record));
    if (self->destroy) self->destroy(self->argument);
}

void HostDestructor(void* value) {
    GuestSqliteDestructor destructor = nullptr;
    {
        std::lock_guard lock(destructorMutex);
        const auto found = destructors.find(value);
        if (found == destructors.end()) throw std::runtime_error("sqlite3: no guest destructor for bound value");
        destructor = found->second;
        destructors.erase(found);
    }
    destructor(value);
}

sqlite3_destructor_type Destructor(const void* value, GuestSqliteDestructor destructor) {
    if (!destructor) return SQLITE_STATIC;
    if (reinterpret_cast<std::intptr_t>(destructor) == -1) return SQLITE_TRANSIENT;
    std::lock_guard lock(destructorMutex);
    destructors.emplace(value, destructor);
    return HostDestructor;
}

std::string HostFilename(const char* filename, int flags) {
    if (!filename) throw std::invalid_argument("sqlite3_open_v2: null file name");
    if (filename[0] == '\0' || std::strcmp(filename, ":memory:") == 0) return filename;
    if (std::strncmp(filename, "file:", 5) == 0 && (flags & SQLITE_OPEN_URI) != 0)
        throw std::runtime_error("sqlite3_open_v2: URI filenames are not implemented");
    const std::filesystem::path path = ResolvePath_nid_no_patch(filename);
    if ((flags & SQLITE_OPEN_READWRITE) != 0) RecordWrittenPath_nid_no_patch(path);
    const std::u8string resolved = path.u8string();
    return std::string(reinterpret_cast<const char*>(resolved.data()), resolved.size());
}

} // namespace

extern "C" {

std::int32_t APS5_VABI sqlite3_initialize_nid_postfix() {
    return sqlite3_initialize();
}

std::int32_t APS5_VABI sqlite3_open_v2_nid_postfix(const char* filename, GuestSqlite** database, std::int32_t flags, const char* vfs) {
    if (!database) throw std::invalid_argument("sqlite3_open_v2: null connection pointer");
    if (vfs) throw std::runtime_error("sqlite3_open_v2: named VFS selection is not implemented");
    const std::string host = HostFilename(filename, flags);
    sqlite3* opened = nullptr;
    const int result = sqlite3_open_v2(host.c_str(), &opened, flags, nullptr);
    if (opened) {
        std::lock_guard lock(connectionMutex);
        connections[opened] = std::make_unique<Connection>();
    }
    *database = reinterpret_cast<GuestSqlite*>(opened);
    return result;
}

std::int32_t APS5_VABI sqlite3_close_nid_postfix(GuestSqlite* database) {
    const int result = sqlite3_close(Host(database));
    if (result == SQLITE_OK && database) {
        std::lock_guard lock(connectionMutex);
        connections.erase(Host(database));
    }
    return result;
}

std::int32_t APS5_VABI sqlite3_prepare_v2_nid_postfix(GuestSqlite* database, const char* sql, std::int32_t size, GuestSqliteStatement** statement, const char** tail) {
    return sqlite3_prepare_v2(Host(database), sql, size, reinterpret_cast<sqlite3_stmt**>(statement), tail);
}

std::int32_t APS5_VABI sqlite3_step_nid_postfix(GuestSqliteStatement* statement) {
    return sqlite3_step(Host(statement));
}

std::int32_t APS5_VABI sqlite3_reset_nid_postfix(GuestSqliteStatement* statement) {
    return sqlite3_reset(Host(statement));
}

std::int32_t APS5_VABI sqlite3_finalize_nid_postfix(GuestSqliteStatement* statement) {
    return sqlite3_finalize(Host(statement));
}

std::int32_t APS5_VABI sqlite3_clear_bindings_nid_postfix(GuestSqliteStatement* statement) {
    return sqlite3_clear_bindings(Host(statement));
}

std::int32_t APS5_VABI sqlite3_bind_blob_nid_postfix(GuestSqliteStatement* statement, std::int32_t index, const void* value, std::int32_t size, GuestSqliteDestructor destructor) {
    return sqlite3_bind_blob(Host(statement), index, value, size, Destructor(value, destructor));
}

std::int32_t APS5_VABI sqlite3_bind_text16_nid_postfix(GuestSqliteStatement* statement, std::int32_t index, const void* value, std::int32_t size, GuestSqliteDestructor destructor) {
    return sqlite3_bind_text16(Host(statement), index, value, size, Destructor(value, destructor));
}

std::int32_t APS5_VABI sqlite3_bind_double_nid_postfix(GuestSqliteStatement* statement, std::int32_t index, double value) {
    return sqlite3_bind_double(Host(statement), index, value);
}

std::int32_t APS5_VABI sqlite3_bind_int_nid_postfix(GuestSqliteStatement* statement, std::int32_t index, std::int32_t value) {
    return sqlite3_bind_int(Host(statement), index, value);
}

std::int32_t APS5_VABI sqlite3_bind_int64_nid_postfix(GuestSqliteStatement* statement, std::int32_t index, std::int64_t value) {
    return sqlite3_bind_int64(Host(statement), index, value);
}

std::int32_t APS5_VABI sqlite3_bind_null_nid_postfix(GuestSqliteStatement* statement, std::int32_t index) {
    return sqlite3_bind_null(Host(statement), index);
}

std::int32_t APS5_VABI sqlite3_bind_parameter_count_nid_postfix(GuestSqliteStatement* statement) {
    return sqlite3_bind_parameter_count(Host(statement));
}

const void* APS5_VABI sqlite3_column_blob_nid_postfix(GuestSqliteStatement* statement, std::int32_t column) {
    return sqlite3_column_blob(Host(statement), column);
}

std::int32_t APS5_VABI sqlite3_column_bytes_nid_postfix(GuestSqliteStatement* statement, std::int32_t column) {
    return sqlite3_column_bytes(Host(statement), column);
}

std::int32_t APS5_VABI sqlite3_column_bytes16_nid_postfix(GuestSqliteStatement* statement, std::int32_t column) {
    return sqlite3_column_bytes16(Host(statement), column);
}

const char* APS5_VABI sqlite3_column_decltype_nid_postfix(GuestSqliteStatement* statement, std::int32_t column) {
    return sqlite3_column_decltype(Host(statement), column);
}

double APS5_VABI sqlite3_column_double_nid_postfix(GuestSqliteStatement* statement, std::int32_t column) {
    return sqlite3_column_double(Host(statement), column);
}

std::int32_t APS5_VABI sqlite3_column_int_nid_postfix(GuestSqliteStatement* statement, std::int32_t column) {
    return sqlite3_column_int(Host(statement), column);
}

std::int64_t APS5_VABI sqlite3_column_int64_nid_postfix(GuestSqliteStatement* statement, std::int32_t column) {
    return sqlite3_column_int64(Host(statement), column);
}

const void* APS5_VABI sqlite3_column_name16_nid_postfix(GuestSqliteStatement* statement, std::int32_t column) {
    return sqlite3_column_name16(Host(statement), column);
}

const void* APS5_VABI sqlite3_column_text16_nid_postfix(GuestSqliteStatement* statement, std::int32_t column) {
    return sqlite3_column_text16(Host(statement), column);
}

std::int32_t APS5_VABI sqlite3_column_type_nid_postfix(GuestSqliteStatement* statement, std::int32_t column) {
    return sqlite3_column_type(Host(statement), column);
}

GuestSqliteValue* APS5_VABI sqlite3_column_value_nid_postfix(GuestSqliteStatement* statement, std::int32_t column) {
    return reinterpret_cast<GuestSqliteValue*>(sqlite3_column_value(Host(statement), column));
}

std::int32_t APS5_VABI sqlite3_data_count_nid_postfix(GuestSqliteStatement* statement) {
    return sqlite3_data_count(Host(statement));
}

const char* APS5_VABI sqlite3_sql_nid_postfix(GuestSqliteStatement* statement) {
    return sqlite3_sql(Host(statement));
}

std::int32_t APS5_VABI sqlite3_stmt_busy_nid_postfix(GuestSqliteStatement* statement) {
    return sqlite3_stmt_busy(Host(statement));
}

GuestSqliteStatement* APS5_VABI sqlite3_next_stmt_nid_postfix(GuestSqlite* database, GuestSqliteStatement* statement) {
    return reinterpret_cast<GuestSqliteStatement*>(sqlite3_next_stmt(Host(database), Host(statement)));
}

std::int32_t APS5_VABI sqlite3_errcode_nid_postfix(GuestSqlite* database) {
    return sqlite3_errcode(Host(database));
}

const char* APS5_VABI sqlite3_errmsg_nid_postfix(GuestSqlite* database) {
    return sqlite3_errmsg(Host(database));
}

const char* APS5_VABI sqlite3_errstr_nid_postfix(std::int32_t code) {
    return sqlite3_errstr(code);
}

std::int32_t APS5_VABI sqlite3_extended_result_codes_nid_postfix(GuestSqlite* database, std::int32_t enable) {
    return sqlite3_extended_result_codes(Host(database), enable);
}

std::int32_t APS5_VABI sqlite3_get_autocommit_nid_postfix(GuestSqlite* database) {
    return sqlite3_get_autocommit(Host(database));
}

void APS5_VABI sqlite3_interrupt_nid_postfix(GuestSqlite* database) {
    sqlite3_interrupt(Host(database));
}

std::int64_t APS5_VABI sqlite3_last_insert_rowid_nid_postfix(GuestSqlite* database) {
    return sqlite3_last_insert_rowid(Host(database));
}

std::int32_t APS5_VABI sqlite3_total_changes_nid_postfix(GuestSqlite* database) {
    return sqlite3_total_changes(Host(database));
}

std::int32_t APS5_VABI sqlite3_busy_timeout_nid_postfix(GuestSqlite* database, std::int32_t milliseconds) {
    Connection& connection = Find(Host(database));
    const int result = sqlite3_busy_timeout(Host(database), milliseconds);
    connection.busy = nullptr;
    connection.busyArgument = nullptr;
    return result;
}

std::int32_t APS5_VABI sqlite3_busy_handler_nid_postfix(GuestSqlite* database, GuestSqliteBusyHandler handler, void* argument) {
    Connection& connection = Find(Host(database));
    connection.busy = handler;
    connection.busyArgument = argument;
    return sqlite3_busy_handler(Host(database), handler ? HostBusy : nullptr, handler ? &connection : nullptr);
}

std::int32_t APS5_VABI sqlite3_set_authorizer_nid_postfix(GuestSqlite* database, GuestSqliteAuthorizer authorizer, void* argument) {
    Connection& connection = Find(Host(database));
    connection.authorizer = authorizer;
    connection.authorizerArgument = argument;
    return sqlite3_set_authorizer(Host(database), authorizer ? HostAuthorizer : nullptr, authorizer ? &connection : nullptr);
}

void* APS5_VABI sqlite3_wal_hook_nid_postfix(GuestSqlite* database, GuestSqliteWalHook hook, void* argument) {
    Connection& connection = Find(Host(database));
    const GuestSqliteWalHook previousHook = connection.walHook;
    void* const previousArgument = connection.walArgument;
    connection.walHook = hook;
    connection.walArgument = argument;
    void* const replaced = sqlite3_wal_hook(Host(database), hook ? HostWalHook : nullptr, hook ? &connection : argument);
    return replaced == &connection && previousHook ? previousArgument : replaced;
}

std::int32_t APS5_VABI sqlite3_wal_checkpoint_v2_nid_postfix(GuestSqlite* database, const char* name, std::int32_t mode, std::int32_t* log, std::int32_t* checkpointed) {
    return sqlite3_wal_checkpoint_v2(Host(database), name, mode, log, checkpointed);
}

std::int32_t APS5_VABI sqlite3_create_function_nid_postfix(GuestSqlite* database, const char* name, std::int32_t arguments, std::int32_t encoding, void* application,
                                                           GuestSqliteFunction function, GuestSqliteFunction step, GuestSqliteFinal final) {
    Connection& connection = Find(Host(database));
    auto record = std::make_unique<FunctionRecord>(FunctionRecord{function, step, final, application});
    const int result = sqlite3_create_function(Host(database), name, arguments, encoding, record.get(),
                                               function ? HostFunction : nullptr, step ? HostStep : nullptr, final ? HostFinal : nullptr);
    if (result == SQLITE_OK) connection.functions.push_back(std::move(record));
    return result;
}

std::int32_t APS5_VABI sqlite3_create_collation_v2_nid_postfix(GuestSqlite* database, const char* name, std::int32_t encoding, void* argument,
                                                               GuestSqliteCompare compare, GuestSqliteDestructor destroy) {
    Find(Host(database));
    auto* record = new CollationRecord{compare, destroy, argument};
    const int result = sqlite3_create_collation_v2(Host(database), name, encoding, record, compare ? HostCompare : nullptr, HostCollationDestroy);
    if (result != SQLITE_OK) delete record;
    return result;
}

void* APS5_VABI sqlite3_user_data_nid_postfix(GuestSqliteContext* context) {
    return static_cast<FunctionRecord*>(sqlite3_user_data(Host(context)))->application;
}

void APS5_VABI sqlite3_result_error_nid_postfix(GuestSqliteContext* context, const char* message, std::int32_t size) {
    sqlite3_result_error(Host(context), message, size);
}

double APS5_VABI sqlite3_value_double_nid_postfix(GuestSqliteValue* value) {
    return sqlite3_value_double(Host(value));
}

const void* APS5_VABI sqlite3_value_text16_nid_postfix(GuestSqliteValue* value) {
    return sqlite3_value_text16(Host(value));
}

std::int32_t APS5_VABI sqlite3_value_type_nid_postfix(GuestSqliteValue* value) {
    return sqlite3_value_type(Host(value));
}

}

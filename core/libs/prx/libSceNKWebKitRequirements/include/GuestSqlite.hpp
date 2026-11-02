#ifndef CORE_LIBS_PRX_LIBSCENKWEBKITREQUIREMENTS_INCLUDE_GUESTSQLITE_HPP
#define CORE_LIBS_PRX_LIBSCENKWEBKITREQUIREMENTS_INCLUDE_GUESTSQLITE_HPP

#include "prx/libc/include/general/VabiMacros.hpp"

#include <cstdint>

namespace NKWebKitRequirements {

struct GuestSqlite;
struct GuestSqliteStatement;
struct GuestSqliteValue;
struct GuestSqliteContext;

using GuestSqliteDestructor = void (APS5_VABI*)(void*);
using GuestSqliteBusyHandler = std::int32_t (APS5_VABI*)(void*, std::int32_t);
using GuestSqliteCompare = std::int32_t (APS5_VABI*)(void*, std::int32_t, const void*, std::int32_t, const void*);
using GuestSqliteFunction = void (APS5_VABI*)(GuestSqliteContext*, std::int32_t, GuestSqliteValue**);
using GuestSqliteFinal = void (APS5_VABI*)(GuestSqliteContext*);
using GuestSqliteAuthorizer = std::int32_t (APS5_VABI*)(void*, std::int32_t, const char*, const char*, const char*, const char*);
using GuestSqliteWalHook = std::int32_t (APS5_VABI*)(void*, GuestSqlite*, const char*, std::int32_t);

}

#endif

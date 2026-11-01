#include "prx/libSceJxr/libSceJxr.hpp"
#include <cstdio>
#include <cstdlib>
#include <stdexcept>

extern "C" int JxrCodecStateTest_nid_no_patch();

int main() {
    const int line = JxrCodecStateTest_nid_no_patch();
    if (line != 0) {
        std::fprintf(stderr, "JXR codec state: check failed at GuestJxrCodecStateReference.c:%d\n", line);
        return 1;
    }
    bool threw = false;
    try { getTilePos_nid_postfix(nullptr, 0, 0); } catch (const std::invalid_argument&) { threw = true; }
    if (!threw) {
        std::fprintf(stderr, "JXR codec state: null codec did not throw\n");
        return 1;
    }
    return 0;
}

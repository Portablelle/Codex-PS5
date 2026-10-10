// SPDX-License-Identifier: GPL-3.0-or-later
#include "platform.hpp"
extern "C" int sceSystemServiceLoadExec(const char*,const char* const*);

namespace botty::platform {
[[noreturn]] void exitApplication() noexcept {
    log("Application cleanup completed; requesting shell exit");
    const int result=sceSystemServiceLoadExec("exit",nullptr);
    if(result<0)log("Shell exit request failed - close using PS menu");
    // The shell owns title termination. Never fall through to CRT/loader teardown,
    // even if the request returns before the shell closes the process.
    for(;;)sleep(100000);
}
}

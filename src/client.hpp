// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "protocol.hpp"
namespace assistant {
class Client {
public:
    bool start();void stop();State snapshot();
    void login();void submit(std::string text);void interrupt();void decide(bool accept);
private:
    std::mutex mutex_;Protocol protocol_;bool stopping_=false;void* worker_=nullptr;
    static void* run(void* self);void loop();
};
}

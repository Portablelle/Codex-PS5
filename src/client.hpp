// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "protocol.hpp"
namespace assistant {
class Client {
public:
    bool start();void stop();State snapshot();
    void login();bool submit(std::string text);bool newConversation();void interrupt();void decide(bool accept);
private:
    std::mutex mutex_;Protocol protocol_;bool stopping_=false;void* worker_=nullptr;
    std::string saved_; // conversation stored on the console, owned by the worker once started
    static void* run(void* self);void loop();
};
}

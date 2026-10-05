// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "protocol.hpp"

namespace assistant {
enum class InGamePhase {idle,capturing,listening,transcribing,thinking,cancelling,speaking};
struct InGameCapabilities {bool overlay=false,capture=false,microphone=false,speech=false;};

// All calls belong to one service event loop. The adapter executes actions
// asynchronously; neither an application window nor a renderer owns this session.
class InGameSession {
public:
    explicit InGameSession(std::string captureDirectory="/data/codex-ps5/captures");
    Protocol engine{true};
    std::deque<Json> actions;
    InGamePhase phase() const {return phase_;}
    std::uint64_t request() const {return request_;}
    const std::string& status() const {return status_;}
    void capabilities(InGameCapabilities value){capabilities_=value;}
    void trigger(std::uint64_t nowMs);
    void cancel(std::uint64_t nowMs);
    void tick(std::uint64_t nowMs);
    bool adapterEvent(const Json& event,std::uint64_t nowMs);
    bool engineEvent(std::string_view line,std::uint64_t nowMs);
    void disconnected();
private:
    InGameCapabilities capabilities_;
    InGamePhase phase_=InGamePhase::idle;
    std::string directory_,image_,status_="Idle";
    std::uint64_t request_=0,deadline_=0;
    bool validDirectory_=false;
    bool interruptSent_=false;
    void action(const char* name,Json fields=Json::object());
    void transition(InGamePhase phase,const char* status,std::uint64_t deadline);
    void finish(std::string status);
    void syncEngine(std::uint64_t nowMs);
};

// No shortcut is bound by default. Feed every pad sample, including releases.
// This detects the entire chord becoming held, even across separate samples.
class InGameShortcut {
public:
    explicit InGameShortcut(std::uint32_t mask=0):mask_(mask){}
    bool sample(std::uint32_t buttons){
        const bool down=mask_ && (buttons&mask_)==mask_;
        const bool fired=down && !held_;held_=down;return fired;
    }
    void reset(){held_=false;}
private:
    std::uint32_t mask_=0;bool held_=false;
};
}

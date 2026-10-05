// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <atomic>
#include <mutex>
#include <string>
struct whisper_context;
namespace assistant {
struct VoiceState {bool recording=false,working=false;std::string status="Triangle: dictate",transcript;};
class Voice {
public:
    void toggle();void cancel();void shutdown();VoiceState snapshot();std::string takeTranscript();
private:
    std::mutex mutex_;VoiceState state_;std::atomic<bool> stop_{false},cancelled_{false};
    void* worker_=nullptr;whisper_context* context_=nullptr;
    static void* run(void*);void record();
};
}

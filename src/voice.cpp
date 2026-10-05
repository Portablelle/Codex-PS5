// SPDX-License-Identifier: GPL-3.0-or-later
// AudioIn routing based on BlackBearReloaded's GPL-3.0 microphone example.
#include "voice.hpp"
#include "language.hpp"
#include "platform.hpp"
#include "whisper.h"
#include <array>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <vector>
extern "C" {
int sceUserServiceGetInitialUser(int*);
int sceAudioInOpen(int,int,int,unsigned,unsigned,unsigned);
int sceAudioInInput(int,void*);
int sceAudioInClose(int);
int sceAudioInGetSilentState(int);
std::size_t ps5_opengl_heap_capacity(void);
std::size_t ps5_opengl_heap_live_bytes(void);
}
namespace assistant {
VoiceState Voice::snapshot(){std::lock_guard lock(mutex_);return state_;}
std::string Voice::takeTranscript(){std::lock_guard lock(mutex_);std::string s;state_.transcript.swap(s);return s;}
void Voice::toggle(){
    {std::lock_guard lock(mutex_);if(state_.recording){stop_=true;return;}if(state_.working)return;}
    if(worker_){botty::platform::joinWorker(worker_);worker_=nullptr;}
    stop_=false;cancelled_=false;
    {std::lock_guard lock(mutex_);state_.recording=true;state_.status="Recording locally (30 s max). Triangle: finish";}
    if(!botty::platform::startWorker(run,this,&worker_)){
        std::lock_guard lock(mutex_);state_.recording=false;state_.status="Unable to start microphone";
    }
}
void Voice::cancel(){cancelled_=true;stop_=true;}
void Voice::shutdown(){cancel();if(worker_)botty::platform::joinWorker(worker_);worker_=nullptr;if(context_)whisper_free(context_);context_=nullptr;}
void* Voice::run(void* p){static_cast<Voice*>(p)->record();return nullptr;}
static void quiet(enum ggml_log_level,const char*,void*) {} // no transcript logging
void Voice::record(){
    const auto finish=[&](std::string status){std::lock_guard lock(mutex_);state_.recording=state_.working=false;state_.status=std::move(status);};
    int user=-1;if(sceUserServiceGetInitialUser(&user)<0){finish("PS5 user unavailable");return;}
    const int mic=sceAudioInOpen(user,1,0,256,16000,0);
    if(mic<0){finish("Microphone unavailable. Check your controller microphone.");return;}
    std::vector<float> samples;samples.reserve(30*16000);std::array<short,256> block{};
    double energy=0;bool failed=false;
    while(!stop_ && samples.size()+block.size()<=30*16000){
        if(sceAudioInInput(mic,block.data())<0){failed=true;break;}
        for(short value:block){const float sample=value/32768.0f;samples.push_back(sample);energy+=sample*sample;}
    }
    (void)sceAudioInClose(mic);
    if(cancelled_){finish("Dictation cancelled");return;}
    if(failed){finish("Microphone capture error");return;}
    if(samples.size()<8000 || std::sqrt(energy/samples.size())<0.003){finish("No voice detected. Check the mute button.");return;}
    {std::lock_guard lock(mutex_);state_.recording=false;state_.working=true;state_.status="Transcribing locally on PS5...";}
    whisper_log_set(quiet,nullptr);
    ggml_log_set(quiet,nullptr);
    ggml_set_abort_callback([](const char*){std::abort();});
    if(!context_){
        const auto capacity=ps5_opengl_heap_capacity(),used=ps5_opengl_heap_live_bytes();
        if(used>capacity || capacity-used<768u*1024u*1024u){
            finish("Not enough memory for local dictation. You can still use the keyboard.");return;
        }
        botty::platform::log("Whisper model initialization started");
        auto params=whisper_context_default_params();params.use_gpu=false;params.flash_attn=true;
        FILE* file=std::fopen("/app0/assets/ggml-base.bin","rb");
        if(file){
            whisper_model_loader loader{};loader.context=file;
            loader.read=[](void* p,void* data,std::size_t size){return std::fread(data,1,size,static_cast<FILE*>(p));};
            loader.eof=[](void* p){return std::feof(static_cast<FILE*>(p))!=0;};
            loader.close=[](void* p){std::fclose(static_cast<FILE*>(p));};
            context_=whisper_init_with_params(&loader,params);
        }
    }
    if(!context_){finish("Whisper model missing or unreadable in assets/ggml-base.bin");return;}
    botty::platform::log("Whisper context initialized");
    auto params=whisper_full_default_params(WHISPER_SAMPLING_GREEDY);
    params.n_threads=4;params.language=language::code();params.translate=false;
    params.print_realtime=params.print_progress=params.print_timestamps=params.print_special=false;
    params.no_context=true;params.suppress_blank=true;params.suppress_nst=true;
    params.abort_callback=[](void* p){return static_cast<Voice*>(p)->cancelled_.load();};params.abort_callback_user_data=this;
    if(whisper_full(context_,params,samples.data(),static_cast<int>(samples.size()))!=0){finish(cancelled_?"Dictation cancelled":"Transcription failed");return;}
    botty::platform::log("Whisper transcription completed");
    std::string text;
    for(int i=0;i<whisper_full_n_segments(context_);++i){
        const char* segment=whisper_full_get_segment_text(context_,i);
        if(segment)text+=segment;if(text.size()>16384){finish("Dictation too long");return;}
    }
    if(cancelled_){finish("Dictation cancelled");return;}
    {std::lock_guard lock(mutex_);state_.transcript=std::move(text);state_.recording=state_.working=false;state_.status="Dictation ready: review, then press Options to send";}
}
}

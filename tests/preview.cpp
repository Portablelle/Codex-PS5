// Host-only UI preview. Simulated session; never contacts a console or account.
#include "../src/client.hpp"
#include "../src/voice.hpp"
#include "../src/platform.hpp"
extern "C" {
#include "../vendor/ps5-pad.h"
}
#include <cstdlib>
#include <cstdio>
#include <cstdint>
#include <sys/event.h>
namespace assistant {
bool Client::start(){
    protocol_.state.connected=protocol_.state.ready=protocol_.state.authenticated=true;
    protocol_.state.thread="preview";protocol_.state.status="Ready — simulated preview session";
    protocol_.state.response="I can help you organize PS5 files.\n\nDictate a request with △ or open the keyboard with □.\n\nWhen Codex asks for approval, review the command or change before confirming.";
    protocol_.state.messages={{false,"I can check the available space on mounted volumes.","intro"},
        {true,"How much space is left on my PS5 and external drive?",{}},
        {false,"- **PS5 internal storage** : **510.4 GiB** available.\n- **External drive** : **575.5 GiB** available.\n\nVolumes checked with `df -h`.","reply"}};
    if(std::getenv("PREVIEW_THINKING")){protocol_.state.messages.pop_back();protocol_.state.busy=true;protocol_.state.status="Codex is thinking...";}
    if(std::getenv("PREVIEW_SIGNIN")){protocol_.state.code="K7QF-29XM";protocol_.state.url="https://auth.openai.com/codex/device";protocol_.state.status="Sign in on your phone using the displayed code.";}
    if(std::getenv("PREVIEW_APPROVAL"))protocol_.state.approvals.push_back({"demo","item/commandExecution/requestApproval","mv /data/notes.txt /data/archives/notes.txt"});
    return true;
}
void Client::stop(){}State Client::snapshot(){return protocol_.state;}
void Client::login(){}void Client::submit(std::string){}void Client::interrupt(){}void Client::decide(bool){}
void Voice::toggle(){}void Voice::cancel(){}void Voice::shutdown(){}
VoiceState Voice::snapshot(){return {};}
std::string Voice::takeTranscript(){static bool first=true;if(first){first=false;return "List the files in /data and help me organize them.";}return {};}
}
namespace botty::platform {
int systemLanguage() noexcept {return std::getenv("PREVIEW_ENGLISH")?1:(std::getenv("PREVIEW_FRENCH")?2:1);}
std::uint64_t now() noexcept {return 0;}void sleep(unsigned) noexcept {}void log(const char*) noexcept {}
}
extern "C" {
struct Buffer {void* data;void* a;void* b;void* c;};static Buffer* buffers;
std::size_t sceKernelGetDirectMemorySize(){return 1ULL<<30;}
int sceKernelAllocateDirectMemory(std::int64_t,std::int64_t,std::size_t,std::size_t,int,std::int64_t* p){*p=0;return 0;}
int sceKernelMapDirectMemory(void** p,std::size_t n,int,int,std::int64_t,std::size_t){*p=std::calloc(1,n);return *p?0:-1;}
int sceKernelSendNotificationRequest(std::uint32_t,void*,std::size_t,int){return 0;}
int sceKernelUsleep(std::uint32_t){return 0;}int sceSystemServiceHideSplashScreen(){return 0;}
int sceKernelCreateEqueue(struct kevent** p,const char*){*p=reinterpret_cast<struct kevent*>(1);return 0;}
int sceKernelWaitEqueue(struct kevent*,struct kevent*,int,int* n,unsigned*){*n=1;return 0;}
int sceKernelDeleteEqueue(struct kevent*){return 0;}int sceVideoOutAddFlipEvent(struct kevent*,int,void*){return 0;}
int sceVideoOutDeleteFlipEvent(struct kevent*,int){return 0;}void sceVideoOutClose(int){}
int sceKernelMunmap(void*,std::size_t){return 0;}int sceKernelReleaseDirectMemory(std::int64_t,std::size_t){return 0;}
int sceVideoOutOpen(std::int32_t,std::int32_t,std::int32_t,const void*){return 1;}
int sceVideoOutSetFlipRate(std::int32_t,std::int32_t){return 0;}
int sceVideoOutWaitVblank(std::int32_t){return 0;}
bool ps5ObserveOwnedAllocation(const void* p) noexcept {return p!=nullptr;}
void sceVideoOutSetBufferAttribute2(void*,std::uint64_t,std::uint32_t,std::uint32_t,std::uint32_t,std::uint64_t,std::uint32_t,std::uint64_t){}
int sceVideoOutRegisterBuffers2(std::int32_t,std::int32_t,std::int32_t,Buffer* p,std::int32_t,void*,std::int32_t,void*){buffers=p;return 0;}
int sceVideoOutSubmitFlip(std::int32_t,std::int32_t i,std::uint32_t,std::int64_t){
    const char* path=std::getenv("PREVIEW_OUTPUT");FILE* f=std::fopen(path?path:"build/preview.ppm","wb");if(!f)std::exit(1);
    std::fprintf(f,"P6\n1920 1080\n255\n");auto* b=static_cast<unsigned char*>(buffers[i].data);
    for(unsigned y=0;y<1080;++y)for(unsigned x=0;x<1920;++x){
        unsigned off=((y<<4)&0x70)^((y<<5)&0xf00)^((y<<9)&0x1000)^((y<<8)&0x4000)^((x<<2)&0xc)^((x<<5)&0x380)^((x<<4)&0x400)^((x<<6)&0x800)^((x<<9)&0xa000);
        off+=(((y>>7)*15+(x>>7))<<16);std::fwrite(b+off,1,3,f);
    }
    std::fclose(f);std::exit(0);
}
int scePadInit(){return 0;}int scePadOpen(int,int,int,void*){return 1;}int scePadClose(int){return 0;}
int scePadRead(int,PS5_PadData* p,int){p[0]={};p[0].connected=1;p[0].buttons=std::getenv("PREVIEW_QUIT")?PS5_PAD_BUTTON_TOUCH_PAD:(std::getenv("PREVIEW_KEYBOARD")?PS5_PAD_BUTTON_SQUARE:0);return 1;}
int sceUserServiceInitialize(void*){return 0;}
int sceUserServiceGetInitialUser(int* p){*p=1;return 0;}
}

// Client worker against a fake engine on a socketpair, with the console storage stubbed.
#include "../src/client.hpp"
#include "../src/platform.hpp"
#include <atomic>
#include <cassert>
#include <chrono>
#include <iostream>
#include <thread>
#include <vector>
#include <poll.h>
#include <pthread.h>
#include <sys/socket.h>
#include <unistd.h>
using namespace assistant;
namespace {
int engine=-1;std::atomic<int> app{-1}; // -1: the engine is unavailable
std::mutex storage;std::string stored="saved-thread";std::vector<std::string> writes;std::atomic<int> failures{0};
}
namespace botty::platform {
int systemLanguage() noexcept {return 1;}
std::uint64_t now() noexcept {return static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now().time_since_epoch()).count());}
void sleep(unsigned us) noexcept {std::this_thread::sleep_for(std::chrono::microseconds(us));}
void log(const char*) noexcept {}
int connectLocal() noexcept {const int fd=app;return fd<0?-1:dup(fd);}
int send(int fd,const void* bytes,std::size_t length) noexcept {return static_cast<int>(::send(fd,bytes,length,0));}
int receive(int fd,void* bytes,std::size_t length) noexcept {
    pollfd p{fd,POLLIN,0};if(poll(&p,1,20)==0)return -2;
    return static_cast<int>(::recv(fd,bytes,length,0));
}
void closeSocket(int fd) noexcept {close(fd);}
bool startWorker(void* (*entry)(void*),void* context,void** handle) noexcept {
    auto* thread=new pthread_t;if(pthread_create(thread,nullptr,entry,context)){delete thread;return false;}
    *handle=thread;return true;
}
void joinWorker(void* handle) noexcept {auto* thread=static_cast<pthread_t*>(handle);pthread_join(*thread,nullptr);delete thread;}
std::string loadConversation(){std::lock_guard lock(storage);return stored;}
bool saveConversation(std::string_view thread) noexcept {
    std::lock_guard lock(storage);writes.emplace_back(thread);
    if(failures>0){--failures;return false;}
    stored=thread;return true;
}
}
namespace {
std::string buffered;
Json next(){ // next request from the client, skipping notifications
    for(;;){
        const auto newline=buffered.find('\n');
        if(newline!=std::string::npos){
            auto j=Json::parse(buffered.substr(0,newline));buffered.erase(0,newline+1);
            if(j.contains("id"))return j;continue;
        }
        pollfd p{engine,POLLIN,0};assert(poll(&p,1,10000)==1); // the client never stays silent that long
        char bytes[4096];const auto n=recv(engine,bytes,sizeof(bytes),0);assert(n>0);buffered.append(bytes,static_cast<std::size_t>(n));
    }
}
void reply(const Json& request,Json result,std::string_view after={}){
    const auto line=Json({{"id",request["id"]},{"result",std::move(result)}}).dump()+"\n"+std::string(after);
    assert(::send(engine,line.data(),line.size(),0)==static_cast<ssize_t>(line.size()));
}
template<class Condition> void wait(Condition condition){
    const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(10);
    while(!condition()){assert(std::chrono::steady_clock::now()<deadline);std::this_thread::sleep_for(std::chrono::milliseconds(5));}
}
}
int main(){
    int pair[2];assert(socketpair(AF_UNIX,SOCK_STREAM,0,pair)==0);engine=pair[0];app=pair[1];
    Client client;assert(client.start());
    auto request=next();assert(request["method"]=="initialize");reply(request,Json::object());
    request=next();assert(request["method"]=="account/read");reply(request,{{"account",{{"type","chatgpt"}}}});
    // The saved conversation is resumed, and nothing is written while it stays the same.
    request=next();assert(request["method"]=="thread/resume" && request["params"]["threadId"]=="saved-thread");
    reply(request,{{"thread",{{"id","saved-thread"},{"turns",Json::array()}}}});
    wait([&]{return client.snapshot().thread=="saved-thread";});
    {std::lock_guard lock(storage);assert(writes.empty());}
    // Leaving it must reach the storage even when the first writes fail.
    failures=2;assert(client.newConversation());
    request=next();assert(request["method"]=="thread/start");
    wait([&]{std::lock_guard lock(storage);return stored.empty();});
    {std::lock_guard lock(storage);assert(writes.size()==3 && writes[0].empty() && writes[2].empty());}
    reply(request,{{"thread",{{"id","fresh"}}}});
    wait([&]{return client.snapshot().thread=="fresh";});
    // The new thread is stored before its first turn is sent to the engine.
    assert(client.submit("Hello"));
    request=next();assert(request["method"]=="turn/start" && request["params"]["threadId"]=="fresh");
    {std::lock_guard lock(storage);assert(stored=="fresh");}
    reply(request,{{"turn",{{"id","t1"}}}});
    const auto done=Json({{"method","turn/completed"},{"params",{{"turn",{{"status","completed"}}}}}}).dump()+"\n";
    assert(::send(engine,done.data(),done.size(),0)>0);
    wait([&]{return !client.snapshot().busy;});
    const auto reconnect=[&]{
        auto request=next();assert(request["method"]=="initialize");reply(request,Json::object());
        request=next();assert(request["method"]=="account/read");reply(request,{{"account",{{"type","chatgpt"}}}});
        return next();
    };
    // A broken line ends the connection; the reconnection resumes the conversation, never a prompt.
    const std::string broken="not json\n";assert(::send(engine,broken.data(),broken.size(),0)>0);
    request=reconnect();assert(request["method"]=="thread/resume" && request["params"]["threadId"]=="fresh");
    // A conversation changed by the last lines before the connection ends is saved right away,
    // while no new connection can save it.
    app=-1;reply(request,{{"thread",{{"id","moved"},{"turns",Json::array()}}}},broken);
    wait([&]{std::lock_guard lock(storage);return stored=="moved";});
    app=pair[1];request=reconnect();assert(request["method"]=="thread/resume" && request["params"]["threadId"]=="moved");
    // When that write fails, the next connection resumes it from memory and saves it.
    failures=1;reply(request,{{"thread",{{"id","moved-again"},{"turns",Json::array()}}}},broken);
    request=reconnect();assert(request["method"]=="thread/resume" && request["params"]["threadId"]=="moved-again");
    wait([&]{std::lock_guard lock(storage);return stored=="moved-again";});
    // A resume whose answer ends the connection (e.g. too large) is not repeated in a loop:
    // the reconnection starts a new conversation and keeps the saved one for the next launch.
    assert(::send(engine,broken.data(),broken.size(),0)>0);
    request=reconnect();assert(request["method"]=="thread/start");
    reply(request,{{"thread",{{"id","replacement"}}}});
    wait([&]{return client.snapshot().thread=="replacement";});
    {std::lock_guard lock(storage);assert(stored=="moved-again");}
    client.stop();close(engine);close(pair[1]);
    std::cout<<"client tests passed\n";
}

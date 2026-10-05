// SPDX-License-Identifier: GPL-3.0-or-later
// Independent payload: owns Codex transport, session state and timeout handling.
#include "../src/ingame.hpp"
#include <algorithm>
#include <array>
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <arpa/inet.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <poll.h>
#include <signal.h>
#include <sys/socket.h>
#include <time.h>
#include <unistd.h>
using namespace assistant;
namespace {
constexpr std::size_t limit=1048576;
std::uint64_t nowMs(){
    timespec value{};clock_gettime(CLOCK_MONOTONIC,&value);
    return static_cast<std::uint64_t>(value.tv_sec)*1000+value.tv_nsec/1000000;
}
bool nonblocking(int fd){int flags=fcntl(fd,F_GETFL);return flags>=0 && fcntl(fd,F_SETFL,flags|O_NONBLOCK)==0;}
sockaddr_in address(unsigned port){
    sockaddr_in value{};value.sin_family=AF_INET;value.sin_port=htons(port);
    value.sin_addr.s_addr=htonl(INADDR_LOOPBACK);
#if defined(__FreeBSD__)
    value.sin_len=sizeof(value);
#endif
    return value;
}
struct Stream {
    int fd=-1,role=0; // 0: awaiting hello, 1: observer, 2: media adapter
    std::string input,output;std::size_t sent=0;std::uint64_t deadline=0,cleanupSent=0;
    void close(){if(fd>=0)::close(fd);fd=-1;input.clear();output.clear();sent=0;role=0;cleanupSent=0;}
    bool enqueue(std::string_view line){
        if(sent){output.erase(0,sent);sent=0;}
        if(output.size()+line.size()>limit)return false;
        output.append(line);return true;
    }
    bool write(short events){
        if(output.size()==sent || !(events&POLLOUT))return true;
        auto n=::send(fd,output.data()+sent,output.size()-sent,0);
        if(n>0){sent+=static_cast<std::size_t>(n);if(sent==output.size()){output.clear();sent=0;}}
        else if(n==0 || (errno!=EAGAIN && errno!=EWOULDBLOCK && errno!=EINTR))return false;
        return true;
    }
    bool read(short events){
        if(events&POLLIN){
            char bytes[8192];auto n=::recv(fd,bytes,sizeof(bytes),0);
            if(n>0){
                if(input.size()+static_cast<std::size_t>(n)>limit)return false;
                input.append(bytes,static_cast<std::size_t>(n));
            } else if(n==0 || (errno!=EAGAIN && errno!=EWOULDBLOCK && errno!=EINTR))return false;
        } else if(events&(POLLHUP|POLLERR|POLLNVAL))return false;
        return true;
    }
};
short interests(const Stream& stream){return POLLIN|(stream.output.size()>stream.sent?POLLOUT:0);}
const char* phaseName(InGamePhase phase){
    switch(phase){
        case InGamePhase::idle:return "idle";
        case InGamePhase::capturing:return "capturing";
        case InGamePhase::listening:return "listening";
        case InGamePhase::transcribing:return "transcribing";
        case InGamePhase::thinking:return "thinking";
        case InGamePhase::cancelling:return "cancelling";
        case InGamePhase::speaking:return "speaking";
    }
    return "unknown";
}
volatile sig_atomic_t stopped=0;
void stopSignal(int){stopped=1;}
bool boolean(const Json& object,const char* key){
    auto it=object.find(key);return it!=object.end() && it->is_boolean() && it->get<bool>();
}
struct Resident {
    InGameSession session;
    Stream engine;
    std::array<Stream,4> peers;
    std::deque<Json> cleanup;
    int adapter=-1;
    bool connecting=false,exiting=false,failed=false;
    std::uint64_t retryAt=0,sequence=0;
    std::string instance,lastStatus;
    Resident(std::string directory,std::string identity):session(directory+"/"+identity),instance(std::move(identity)){}
    ~Resident(){engine.close();for(auto& peer:peers)peer.close();}
    void lostEngine(std::uint64_t now){
        engine.close();connecting=false;retryAt=now+1000;session.disconnected();
    }
    void drop(unsigned slot,std::uint64_t now){
        if(static_cast<int>(slot)==adapter){
            adapter=-1;session.capabilities({});session.cancel(now);
        }
        peers[slot].close();
    }
    std::string status(){
        return Json({{"event","status"},{"instance",instance},{"request",session.request()},
            {"phase",phaseName(session.phase())},{"status",session.status()},
            {"engineConnected",session.engine.state.connected},{"ready",session.engine.state.ready && !session.engine.state.thread.empty()},
            {"authenticated",session.engine.state.authenticated},{"adapterConnected",adapter>=0}}).dump()+"\n";
    }
    bool message(unsigned slot,const Json& input,std::uint64_t now){
        auto& peer=peers[slot];if(!input.is_object())return false;
        const auto command=field(input,"command");
        if(peer.role==0){
            if(command!="hello")return false;
            const auto role=field(input,"role");
            if(role=="observer")peer.role=1;
            else if(role=="adapter" && adapter<0){peer.role=2;adapter=static_cast<int>(slot);}
            else return false;
            return peer.enqueue(status());
        }
        if(command=="status")return peer.enqueue(status());
        // Epoch checks prevent a restarted adapter from returning previous work.
        if(field(input,"instance")!=instance)return false;
        if(command=="stop"){
            if(session.phase()!=InGamePhase::idle || !cleanup.empty())return peer.enqueue("{\"event\":\"error\",\"message\":\"Cancel the active question and finish media cleanup before stopping\"}\n");
            exiting=true;return true;
        }
        if(peer.role!=2)return false;
        if(command=="ack"){
            auto it=input.find("sequence");
            if(it==input.end() || !it->is_number_integer() ||
               (!it->is_number_unsigned() && it->get<std::int64_t>()<0))return false;
            const auto number=it->get<std::uint64_t>();
            const auto found=std::find_if(cleanup.begin(),cleanup.end(),[&](const Json& action){return action["sequence"]==number;});
            if(found!=cleanup.end())cleanup.erase(found);
            return true; // duplicate acknowledgements are harmless
        }
        if(command=="capabilities"){
            auto it=input.find("capabilities");if(it==input.end() || !it->is_object())return false;
            session.capabilities({boolean(*it,"overlay"),boolean(*it,"capture"),boolean(*it,"microphone"),boolean(*it,"speech")});return true;
        }
        if(command=="trigger"){session.trigger(now);return true;}
        if(command=="cancel"){session.cancel(now);return true;}
        if(command=="adapter"){
            auto it=input.find("message");if(it==input.end() || !it->is_object())return false;
            if(!session.adapterEvent(*it,now))return peer.enqueue("{\"event\":\"ignored\",\"reason\":\"Stale request or wrong phase\"}\n");
            return true;
        }
        return false;
    }
    static bool isCleanup(const Json& action){
        const auto name=field(action,"action");
        return name=="cancelMedia" || name=="stopSpeech" || name=="releaseCapture" ||
            (name=="overlay" && action.contains("visible") && action["visible"]==false);
    }
    void drain(std::uint64_t now){
        // Replay only idempotent cleanup, in order, until acknowledged by the adapter.
        if(adapter>=0){
            for(const auto& action:cleanup){
                const auto number=action["sequence"].get<std::uint64_t>();
                if(number<=peers[adapter].cleanupSent)continue;
                if(!peers[adapter].enqueue(action.dump()+"\n")){drop(static_cast<unsigned>(adapter),now);break;}
                peers[adapter].cleanupSent=number;
            }
        }
        while(!session.actions.empty()){
            auto action=session.actions.front();session.actions.pop_front();
            const auto name=field(action,"action");
            if(name=="disconnectEngine"){lostEngine(now);continue;}
            action["instance"]=instance;action["sequence"]=++sequence;
            const bool retained=isCleanup(action);
            if(retained){
                cleanup.push_back(action);
                if(cleanup.size()>32){failed=exiting=true;return;} // never silently discard cleanup
            }
            if(adapter>=0){
                if(!peers[adapter].enqueue(action.dump()+"\n"))drop(static_cast<unsigned>(adapter),now);
                else if(retained)peers[adapter].cleanupSent=sequence;
            }
            // Capture, recording and speech work is never replayed on reconnect.
        }
        while(engine.fd>=0 && !connecting && !session.engine.outgoing.empty()){
            if(!engine.enqueue(session.engine.outgoing.front())){lostEngine(now);break;}
            session.engine.outgoing.pop_front();
        }
        auto snapshot=status();
        if(snapshot!=lastStatus){
            lastStatus=snapshot;
            for(unsigned i=0;i<peers.size();++i)if(peers[i].fd>=0 && peers[i].role && !peers[i].enqueue(snapshot))drop(i,now);
        }
    }
    void connectEngine(unsigned port,std::uint64_t now){
        if(engine.fd>=0 || now<retryAt || session.phase()!=InGamePhase::idle)return;
        int fd=::socket(AF_INET,SOCK_STREAM,0);
        if(fd<0){retryAt=now+1000;return;}
        auto addr=address(port);
        if(!nonblocking(fd)){::close(fd);retryAt=now+1000;return;}
        auto result=::connect(fd,reinterpret_cast<sockaddr*>(&addr),sizeof(addr));
        if(result<0 && errno!=EINPROGRESS){::close(fd);retryAt=now+1000;return;}
        engine.fd=fd;connecting=result<0;engine.deadline=now+1500;
        if(!connecting)session.engine.begin();
    }
};
unsigned portArgument(const char* input){
    if(!*input)return 0;for(const char* p=input;*p;++p)if(*p<'0' || *p>'9')return 0;
    char* end=nullptr;errno=0;auto value=std::strtoul(input,&end,10);
    return !errno && end && !*end && value>0 && value<=65535?static_cast<unsigned>(value):0;
}
}
int main(int argc,char** argv){
    unsigned adapterPort=49324,enginePort=49322;std::string directory="/data/codex-ps5/captures";
    for(int i=1;i<argc;++i){
        const std::string_view arg=argv[i];
        if(arg=="--help"){
            std::puts("ingame-service [--adapter-port PORT] [--engine-port PORT] [--capture-directory /PATH]\nLoopback only. No PS5 overlay/audio hooks are installed.");return 0;
        }
        if(i+1>=argc){std::fprintf(stderr,"Missing option value\n");return 2;}
        if(arg=="--adapter-port")adapterPort=portArgument(argv[++i]);
        else if(arg=="--engine-port")enginePort=portArgument(argv[++i]);
        else if(arg=="--capture-directory")directory=argv[++i];
        else {std::fprintf(stderr,"Unknown option\n");return 2;}
    }
    if(!adapterPort || !enginePort || adapterPort==enginePort || directory.empty() || directory.front()!='/')return 2;
    signal(SIGPIPE,SIG_IGN);signal(SIGINT,stopSignal);signal(SIGTERM,stopSignal);
    int listener=::socket(AF_INET,SOCK_STREAM,0);if(listener<0)return 1;
    auto addr=address(adapterPort);int reuse=1;
    if(setsockopt(listener,SOL_SOCKET,SO_REUSEADDR,&reuse,sizeof(reuse)) ||
       bind(listener,reinterpret_cast<sockaddr*>(&addr),sizeof(addr)) || listen(listener,4) || !nonblocking(listener)){
        ::close(listener);std::fprintf(stderr,"Resident listener unavailable\n");return 1;
    }
    const auto identity="resident-"+std::to_string(getpid())+"-"+std::to_string(nowMs());
    Resident resident(directory,identity);
    while(!stopped && !resident.exiting){
        auto now=nowMs();resident.session.tick(now);resident.drain(now);resident.connectEngine(enginePort,now);
        std::array<pollfd,6> fds{};fds[0]={listener,POLLIN,0};
        fds[1]={resident.engine.fd,static_cast<short>(resident.connecting?POLLOUT:interests(resident.engine)),0};
        for(unsigned i=0;i<resident.peers.size();++i)fds[i+2]={resident.peers[i].fd,interests(resident.peers[i]),0};
        if(poll(fds.data(),fds.size(),100)<0){if(errno==EINTR)continue;resident.failed=true;break;}
        now=nowMs();
        if(resident.engine.fd>=0){
            if(resident.connecting){
                if(fds[1].revents || now>=resident.engine.deadline){
                    int error=0;socklen_t size=sizeof(error);
                    if(now>=resident.engine.deadline || getsockopt(resident.engine.fd,SOL_SOCKET,SO_ERROR,&error,&size) || error)resident.lostEngine(now);
                    else {resident.connecting=false;resident.session.engine.begin();}
                }
            } else {
                if(!resident.engine.write(fds[1].revents) || !resident.engine.read(fds[1].revents))resident.lostEngine(now);
                else {
                    std::size_t end;
                    while((end=resident.engine.input.find('\n'))!=std::string::npos){
                        auto line=resident.engine.input.substr(0,end);resident.engine.input.erase(0,end+1);
                        if(!resident.session.engineEvent(line,now))break;
                    }
                }
            }
        }
        for(unsigned i=0;i<resident.peers.size() && !resident.exiting;++i){
            auto& peer=resident.peers[i];if(peer.fd<0)continue;
            if((!peer.role && now>=peer.deadline) || !peer.write(fds[i+2].revents) || !peer.read(fds[i+2].revents)){
                resident.drop(i,now);continue;
            }
            std::size_t end;
            while((end=peer.input.find('\n'))!=std::string::npos){
                auto line=peer.input.substr(0,end);peer.input.erase(0,end+1);
                auto input=Json::parse(line,nullptr,false);
                if(input.is_discarded() || !resident.message(i,input,now)){resident.drop(i,now);break;}
                // Deliver cleanup and generated actions before accepting the next command.
                resident.drain(now);
                if(resident.exiting || resident.peers[i].fd<0)break;
            }
        }
        if(!resident.exiting && (fds[0].revents&POLLIN)){
            int fd=accept(listener,nullptr,nullptr);
            if(fd>=0){
                auto it=std::find_if(resident.peers.begin(),resident.peers.end(),[](const Stream& s){return s.fd<0;});
                if(it==resident.peers.end() || !nonblocking(fd))::close(fd);
                else {it->fd=fd;it->deadline=now+2000;}
            }
        }
    }
    // Closing the owned engine transport terminates only this payload's engine.
    ::close(listener);return resident.failed?1:0;
}

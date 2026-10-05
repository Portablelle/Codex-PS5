// SPDX-License-Identifier: GPL-3.0-or-later
#include "client.hpp"
#include "platform.hpp"
namespace assistant {
namespace platform=botty::platform;
bool Client::start(){return platform::startWorker(run,this,&worker_);}
void Client::stop(){{std::lock_guard lock(mutex_);stopping_=true;}if(worker_)platform::joinWorker(worker_);worker_=nullptr;}
State Client::snapshot(){std::lock_guard lock(mutex_);return protocol_.state;}
void Client::changeLanguage(){std::lock_guard lock(mutex_);protocol_.changeLanguage();}
void Client::login(){std::lock_guard lock(mutex_);protocol_.login();}
void Client::submit(std::string text){std::lock_guard lock(mutex_);protocol_.submit(text);}
void Client::interrupt(){std::lock_guard lock(mutex_);protocol_.interrupt();}
void Client::decide(bool accept){std::lock_guard lock(mutex_);protocol_.decide(accept);}
void* Client::run(void* p){static_cast<Client*>(p)->loop();return nullptr;}
void Client::loop(){
    for(;;){
        {std::lock_guard lock(mutex_);if(stopping_)return;}
        int fd=platform::connectLocal();
        if(fd<0){
            {std::lock_guard lock(mutex_);
             const std::string status="Moteur absent : lance assistant-service dans Payload Manager";
             if(protocol_.state.status!=status){protocol_.state.status=status;++protocol_.state.revision;}}
            platform::sleep(500000);continue;
        }
        {std::lock_guard lock(mutex_);protocol_.begin();}
        std::string incoming;bool valid=true;
        while(valid){
            std::deque<std::string> queue;
            {std::lock_guard lock(mutex_);if(stopping_)break;queue.swap(protocol_.outgoing);}
            for(const auto& line:queue){
                std::size_t offset=0;
                while(offset<line.size()){
                    int n=platform::send(fd,line.data()+offset,line.size()-offset);
                    if(n<=0){valid=false;break;}offset+=static_cast<std::size_t>(n);
                }
                if(!valid)break;
            }
            if(!valid)break;
            char bytes[8192];int n=platform::receive(fd,bytes,sizeof(bytes));
            if(n==-2)continue; // timeout; lets pending UI actions reach the server
            if(n<=0)break;
            incoming.append(bytes,static_cast<std::size_t>(n));
            std::size_t newline;
            while((newline=incoming.find('\n'))!=std::string::npos){
                std::lock_guard lock(mutex_);
                if(!protocol_.ingest(std::string_view(incoming).substr(0,newline))){valid=false;break;}
                incoming.erase(0,newline+1);
            }
            if(incoming.size()>1048576)valid=false;
        }
        platform::closeSocket(fd);
        {std::lock_guard lock(mutex_);protocol_.disconnect();}
        platform::sleep(500000);
    }
}
}

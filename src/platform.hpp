// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
namespace botty::platform {
int systemLanguage() noexcept;
std::uint64_t now() noexcept;
void sleep(unsigned microseconds) noexcept;
void log(const char* message) noexcept;
[[noreturn]] void exitApplication() noexcept;
int connectLocal() noexcept;
int send(int socket,const void* bytes,std::size_t length) noexcept;
int receive(int socket,void* bytes,std::size_t length) noexcept;
void closeSocket(int socket) noexcept;
bool startWorker(void* (*entry)(void*),void* context,void** handle) noexcept;
void joinWorker(void* handle) noexcept;
// Thread ID of the last conversation, kept in the title's own storage; empty when none.
std::string loadConversation();
bool saveConversation(std::string_view thread) noexcept;
}

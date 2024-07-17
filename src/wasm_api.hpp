#pragma once
#include <cstdint>
#include <cstddef>

extern "C" {
extern void onConnectCount(size_t N);
extern void onConnect(const char*);
extern void onDisconnect(const char*);
extern void onChain(const char*);
extern void onMempoolAdd(const char*);
extern void onMempoolErase(const char*);
}

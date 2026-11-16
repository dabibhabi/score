#include <score/db/bar.hpp>
#include <cstddef>
#include <cstdio>
#include <pthread.h>
#include <string>
struct PackedRow { std::int32_t date_days; double open, high, low, close; std::int64_t volume; };
int main(){
  using score::db::Bar;
  std::printf("sizeof(std::string)=%zu sizeof(Bar)=%zu alignof(Bar)=%zu\n",sizeof(std::string),sizeof(Bar),alignof(Bar));
  std::printf("offsets: date=%zu open=%zu high=%zu low=%zu close=%zu volume=%zu\n",offsetof(Bar,date),offsetof(Bar,open),offsetof(Bar,high),offsetof(Bar,low),offsetof(Bar,close),offsetof(Bar,volume));
  std::printf("PackedRow sizeof=%zu offsets date=%zu open=%zu close=%zu volume=%zu\n",sizeof(PackedRow),offsetof(PackedRow,date_days),offsetof(PackedRow,open),offsetof(PackedRow,close),offsetof(PackedRow,volume));
  std::string s="2024-05-07"; std::printf("SSO: string data inside object? %d (capacity %zu)\n", (const char*)s.data()>=(const char*)&s && (const char*)s.data()<(const char*)&s+sizeof s, s.capacity());
  pthread_attr_t a; pthread_attr_init(&a); size_t st; pthread_attr_getstacksize(&a,&st); std::printf("default pthread stack size = %zu bytes (%zu KiB)\n",st,st/1024);
}

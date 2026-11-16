#include "codec_sketch.hpp"
#include <cstdio>
#include <cassert>
int main(){
  SketchJsonFrameCodec enc;
  Frame req{ nlohmann::json{{"op","var"},{"symbol","AAPL"},{"conf",0.95}} };
  auto bytes=enc.encode(req);
  std::string body=req.payload.dump();
  std::printf("dump: %s\nlen=%zu total=%zu\nhex:",body.c_str(),body.size(),bytes.size());
  for(size_t i=0;i<bytes.size();++i) std::printf("%s%02X",(i%16==0)?"\n  ":" ",bytes[i]);
  std::printf("\n");
  // exhaustive 3-way splits of a 2-frame stream
  Frame req2{ nlohmann::json{{"op","cvar"},{"symbol","MSFT"},{"conf",0.975}} };
  auto b2=enc.encode(req2); std::printf("frame2 dump: %s len=%zu\n",req2.payload.dump().c_str(),b2.size()-4);
  std::vector<std::uint8_t> stream=bytes; stream.insert(stream.end(),b2.begin(),b2.end());
  size_t L=stream.size(), cases=0;
  for(size_t i=0;i<=L;++i) for(size_t j=i;j<=L;++j){
    SketchJsonFrameCodec d; std::vector<Frame> got;
    auto pump=[&](size_t a,size_t b){ d.feed(stream.data()+a,b-a); while(auto f=d.try_decode()) got.push_back(*f); };
    pump(0,i); pump(i,j); pump(j,L);
    assert(got.size()==2 && got[0].payload==req.payload && got[1].payload==req2.payload);
    ++cases;
  }
  std::printf("exhaustive 3-way splits of %zu-byte stream: %zu cases OK\n",L,cases);
  // byte-at-a-time
  { SketchJsonFrameCodec d; int k=0; for(auto b:stream){ d.feed(&b,1); while(d.try_decode()) ++k; } std::printf("byte-at-a-time frames=%d\n",k); }
  // worked 3-read split example: 3 | 20 | rest
  { SketchJsonFrameCodec d; size_t cuts[]={0,3,23,bytes.size()};
    for(int r=0;r<3;++r){ d.feed(bytes.data()+cuts[r],cuts[r+1]-cuts[r]); auto f=d.try_decode();
      std::printf("read %d: +%zu bytes -> %s\n",r+1,cuts[r+1]-cuts[r], f? f->payload.dump().c_str():"nullopt"); } }
  // oversize
  { SketchJsonFrameCodec d; std::uint8_t h[4]={0x7F,0xFF,0xFF,0xFF}; d.feed(h,4);
    try{ d.try_decode(); std::printf("no throw?!\n"); }catch(const std::length_error& e){ std::printf("oversize header 0x7FFFFFFF -> length_error: %s\n",e.what()); } }
  std::printf("0x7FFFFFFF=%u, 0xFFFFFFFF=%u\n",0x7FFFFFFFu,0xFFFFFFFFu);
}

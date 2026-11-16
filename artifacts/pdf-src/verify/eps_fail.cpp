#include <score/statistics/risk.hpp>
#include <score/core/exceptions.hpp>
#include <cstdio>
#include <sys/select.h>
int main(){
  std::vector<double> r{0.00,0.01,0.02,0.03,0.04,-0.01,-0.02,-0.03,-0.04,-0.05};
  double c=0.90000000005;
  try{ std::printf("c=%.17g -> VaR=%.17g (true tail size floor((1-c)*10)=0, should throw)\n",c,risk::historical_var(r,c)); }
  catch(const score::ScoreException&e){std::printf("throws: %s\n",e.what());}
  try{ risk::historical_var(r,0.9500001);}catch(const score::ScoreException&e){std::printf("msg for c=0.9500001: %s\n",e.what());}
  std::printf("FD_SETSIZE=%d\n",FD_SETSIZE);
}

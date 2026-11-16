#include <score/statistics/risk.hpp>
#include <score/core/exceptions.hpp>
#include <cstdio>
#include <cmath>
#include <vector>
int main(){
  std::vector<double> r{0.00,0.01,0.02,0.03,0.04,-0.01,-0.02,-0.03,-0.04,-0.05};
  for (double c : {0.95,0.90,0.85,0.80,0.75,0.70,0.50}) {
    try { double v=risk::historical_var(r,c), cv=risk::historical_cvar(r,c);
      std::printf("c=%.2f VaR=%.17g CVaR=%.17g\n",c,v,cv);
    } catch(const score::ScoreException& e){ std::printf("c=%.2f THROWS %s\n",c,e.what()); }
  }
  // tail index raw
  for (double c : {0.95,0.90,0.80}) {
    double tf=1.0-c; double prod=tf*10.0;
    std::printf("c=%.2f 1-c=%.17g (1-c)*10=%.17g floor_noeps=%zu floor_eps=%zu\n",c,tf,prod,(size_t)prod,(size_t)(prod+1e-9));
  }
  // epsilon false positive
  double c=0.9+1e-10; double prod=(1.0-c)*10.0;
  std::printf("c=0.9+1e-10: (1-c)*10=%.17g noeps=%zu eps=%zu\n",prod,(size_t)prod,(size_t)(prod+1e-9));
  try{ std::printf(" var=%.17g\n",risk::historical_var(r,c)); }catch(const score::ScoreException&e){std::printf(" throws %s\n",e.what());}
  // all-positive returns -> clamp
  std::vector<double> pos{0.01,0.02,0.03,0.04,0.05,0.06,0.07,0.08,0.09,0.10};
  std::printf("all-positive VaR90=%.17g CVaR90=%.17g\n",risk::historical_var(pos,0.9),risk::historical_cvar(pos,0.9));
  try{ risk::historical_var({},0.9);}catch(const score::ScoreException&e){std::printf("empty: %s\n",e.what());}
  try{ risk::historical_var(r,1.0);}catch(const score::ScoreException&e){std::printf("c=1: %s\n",e.what());}
}

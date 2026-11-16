#include <score/statistics/risk.hpp>
#include <nlohmann/json.hpp>
#include <fstream>
#include <cstdio>
#include <cmath>
int main(){
  std::ifstream f("mock_closes.json"); auto j=nlohmann::json::parse(f);
  for (auto sym : {"AAPL","MSFT","TSLA"}) {
    std::vector<double> px=j[sym].get<std::vector<double>>();
    auto r=risk::log_returns(px); auto st=risk::calculate_risk_from_returns(r);
    std::printf("%s n_prices=%zu n_returns=%zu mean=%.6f std=%.6f\n",sym,px.size(),r.size(),st.mean_daily_return,st.daily_std_return);
    for(double c:{0.95,0.975,0.99}){
      size_t k=(size_t)((1.0-c)*r.size()+1e-9);
      std::printf("  c=%.3f k=%zu VaR=%.6f CVaR=%.6f  gaussVaR=%.6f\n",c,k,risk::historical_var(r,c),risk::historical_cvar(r,c),
        -(st.mean_daily_return - (c==0.95?1.6448536269514722:c==0.975?1.959963984540054:2.3263478740408408)*st.daily_std_return));
    }
    if(std::string(sym)=="AAPL"){ std::ofstream o("aapl_returns.txt"); for(double x:r) o<<x<<"\n"; }
  }
}

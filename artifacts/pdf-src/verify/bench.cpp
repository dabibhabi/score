// poll() vs kqueue readiness cost with n idle fds and 1 ready fd; plus pipe ping-pong.
#include <sys/event.h>
#include <sys/resource.h>
#include <poll.h>
#include <unistd.h>
#include <chrono>
#include <cstdio>
#include <thread>
#include <vector>
#include <algorithm>
using clk=std::chrono::steady_clock;
int main(){
  rlimit rl{20000,20000}; if(setrlimit(RLIMIT_NOFILE,&rl)) perror("setrlimit");
  std::vector<int> ns={1,10,100,250,500,1000,2000,4000,8000};
  std::printf("n,poll_us,kqueue_us\n");
  for(int n:ns){
    std::vector<int> rd(n),wr(n);
    for(int i=0;i<n;++i){int p[2]; if(pipe(p)){perror("pipe");return 1;} rd[i]=p[0];wr[i]=p[1];}
    char c='x'; write(wr[n-1],&c,1);  // exactly one ready fd
    std::vector<pollfd> pf(n); for(int i=0;i<n;++i) pf[i]={rd[i],POLLIN,0};
    const int R=2000; std::vector<double> tp,tk;
    for(int rep=0;rep<5;++rep){
      auto t0=clk::now(); for(int r=0;r<R;++r){ int k=poll(pf.data(),n,0); if(k!=1) return 2; }
      tp.push_back(std::chrono::duration<double,std::micro>(clk::now()-t0).count()/R);
    }
    int kq=kqueue(); std::vector<struct kevent> ch(n);
    for(int i=0;i<n;++i) EV_SET(&ch[i],rd[i],EVFILT_READ,EV_ADD,0,0,nullptr);
    kevent(kq,ch.data(),n,nullptr,0,nullptr);
    struct kevent ev[64]; timespec z{0,0};
    for(int rep=0;rep<5;++rep){
      auto t0=clk::now(); for(int r=0;r<R;++r){ int k=kevent(kq,nullptr,0,ev,64,&z); if(k!=1) return 3; }
      tk.push_back(std::chrono::duration<double,std::micro>(clk::now()-t0).count()/R);
    }
    std::sort(tp.begin(),tp.end()); std::sort(tk.begin(),tk.end());
    std::printf("%d,%.3f,%.3f\n",n,tp[2],tk[2]);
    close(kq); for(int i=0;i<n;++i){close(rd[i]);close(wr[i]);}
  }
  // ping-pong: two threads bounce a byte over two pipes -> 2 blocking handoffs per round trip
  int a[2],b[2]; pipe(a); pipe(b); const int N=200000;
  std::thread t([&]{ char x; for(int i=0;i<N;++i){ read(a[0],&x,1); write(b[1],&x,1);} });
  char x='p'; auto t0=clk::now();
  for(int i=0;i<N;++i){ write(a[1],&x,1); read(b[0],&x,1);} 
  double us=std::chrono::duration<double,std::micro>(clk::now()-t0).count(); t.join();
  std::printf("pingpong round trip: %.3f us (=> ~%.3f us per handoff)\n",us/N,us/N/2);
  // thread create+join cost
  t0=clk::now(); for(int i=0;i<20000;++i){ std::thread th([]{}); th.join(); }
  std::printf("thread create+join: %.3f us\n",std::chrono::duration<double,std::micro>(clk::now()-t0).count()/20000);
}

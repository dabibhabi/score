#include <cstdint>
#include <cstddef>
#include <stdexcept>
#include <cstdio>
// Sketch (not repository code): tail size from a confidence level in basis points.
std::size_t tail_index_bp(std::uint32_t conf_bp, std::size_t n) {
    if (conf_bp == 0 || conf_bp >= 10'000) throw std::domain_error("conf_bp must be in [1, 9999]");
    // exact: floor((1 - conf_bp/10^4) * n) == floor((10^4 - conf_bp) * n / 10^4)
    return static_cast<std::size_t>((std::uint64_t{10'000 - conf_bp} * n) / 10'000);
}
int main(){
  long bad=0, tot=0;
  for (std::uint32_t bp=1; bp<10000; ++bp) for (std::size_t n=1;n<=2000;++n){
    // reference with long double rational check: floor((10000-bp)*n/10000) computed by repeated subtraction-free integer division is the definition;
    std::uint64_t num=(10000-bp)*(std::uint64_t)n; std::size_t ref=0; while ((ref+1)*10000 <= num) ++ref; // brute force
    ++tot; if (tail_index_bp(bp,n)!=ref) ++bad; if (n>60) break; }
  std::printf("bp sketch: %ld cases, %ld mismatches; k(9000,10)=%zu k(9500,10)=%zu k(8000,10)=%zu k(9500,503)=%zu k(9750,503)=%zu k(9900,503)=%zu\n",tot,bad,
    tail_index_bp(9000,10),tail_index_bp(9500,10),tail_index_bp(8000,10),tail_index_bp(9500,503),tail_index_bp(9750,503),tail_index_bp(9900,503));
}

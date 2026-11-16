from fractions import Fraction as F
import numpy as np, math
# Patriot: 1/10 chopped to 24 bits after the binary point (Skeel's account)
x=F(1,10); chopped=F(math.floor(x*2**24),2**24)
err=x-chopped
print("chopped 0.1 =",chopped, float(chopped), " binary:", format(math.floor(x*2**24),'024b'))
print("error per tick =",float(err))
for h in [1,8,20,48,72,100]:
    ticks=h*3600*10
    print(f"  {h:3d} h: ticks={ticks} drift={float(err*ticks):.4f} s")
# 23 bits variant for comparison
c23=F(math.floor(x*2**23),2**23); print("23-bit chop err", float(x-c23), "x 3.6e6 =", float((x-c23)*3600000))
# VSE simulation: index starts at 1000, n updates of small random relative changes; truncate vs round to 3 dp
rng=np.random.default_rng(1982)
def sim(updates, mode):
    v=1000.0; vt=1000.0
    steps=rng.normal(0, 0.0002, size=updates)  # tiny relative moves per recomputation
    exact=1000.0
    out=[]
    for i,s in enumerate(steps):
        exact*= (1+s)
        t = vt*(1+s)
        vt = math.floor(t*1000)/1000 if mode=='trunc' else round(t*1000)/1000
        if i% (updates//20)==0: out.append((i,exact,vt))
    return exact, vt, out
rng=np.random.default_rng(1982); e,t,_=sim(22*21*3000,'trunc')
rng=np.random.default_rng(1982); e2,r,_=sim(22*21*3000,'round')
print("VSE sim updates",22*21*3000," exact",round(e,3)," truncated",t," rounded",r, " loss",round(e-t,3))
print("expected truncation loss ~ 0.0005*updates =",0.0005*22*21*3000)

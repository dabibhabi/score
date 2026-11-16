import math, numpy as np
from fractions import Fraction as F
from decimal import Decimal
print("product exact?", F((1.0-0.9)*10)==F(1.0-0.9)*10)
r=[0.00,0.01,0.02,0.03,0.04,-0.01,-0.02,-0.03,-0.04,-0.05]
s=sorted(r); n=len(r); print("sorted",s)
def repo(c):
    k=int((1.0-c)*n+1e-9)
    if k==0: return None
    return k, max(0.0,-s[k-1]), max(0.0,-sum(s[:k])/k)
def upper(c):  # -inf{x: F_n(x) > p}
    p=F(1)-F(str(c)); # exact decimal
    for x in s:
        if F(sum(1 for y in s if y<=x),n) > p: return -x
def lower(c):
    p=F(1)-F(str(c))
    for x in s:
        if F(sum(1 for y in s if y<=x),n) >= p: return -x
def es_at(c):  # Acerbi-Tasche sample ES
    p=F(1)-F(str(c)); np_=p*n; m=math.floor(np_)
    tot=sum(F(str(x)) for x in s[:m])
    if np_>m: tot+= (np_-m)*F(str(s[m]))
    return float(-tot/np_)
print(f"{'c':>5} {'k':>3} {'repoVaR':>8} {'repoCVaR':>9} {'upper':>7} {'lower(t1)':>9} {'np.inv_cdf':>10} {'AT-ES':>7}")
for c in [0.95,0.9,0.85,0.8,0.75,0.7,0.65,0.6,0.5]:
    rr=repo(c); inv=-np.quantile(s,1-c,method='inverted_cdf')
    print(f"{c:5.2f} {str(rr[0]) if rr else '-':>3} {rr[1] if rr else 'throw':>8} {round(rr[2],6) if rr else 'throw':>9} {upper(c):>7} {lower(c):>9} {inv:>10.4f} {es_at(c):>7.4f}")
# numpy method names available (Hyndman-Fan types)
for m in ['inverted_cdf','averaged_inverted_cdf','closest_observation','interpolated_inverted_cdf','hazen','weibull','linear','median_unbiased','normal_unbiased','lower','higher']:
    print(m, -np.quantile(s,0.1,method=m), -np.quantile(s,0.2,method=m), -np.quantile(s,0.15,method=m))
# loans counterexample
p=F(4,100); L=100
P0=(1-p)**2; P1=2*p*(1-p); P2=p**2
print("single: P(loss=100)=",p, "-> VaR95=0 since",p,"<",F(5,100))
print("sum: P(0)=",P0,float(P0)," P(100)=",P1,float(P1)," P(200)=",P2,float(P2), " P(sum>=100)=",P1+P2,float(P1+P2))
# VaR95 of loss sum: smallest l with P(Lsum>l)<=0.05
for l in [0,100,200]:
    print(" P(S>",l,")=",float(sum(pr for v,pr in [(0,P0),(100,P1),(200,P2)] if v>l)))
alpha=F(5,100)
def es_loss(dist):
    # average of worst alpha mass
    rem=alpha; tot=F(0)
    for v,pr in sorted(dist,reverse=True):
        take=min(rem,pr); tot+=take*v; rem-=take
        if rem==0: break
    return tot/alpha
print("ES95 single:",es_loss([(0,1-p),(100,p)]), " ES95 sum:",es_loss([(0,P0),(100,P1),(200,P2)]), float(es_loss([(0,P0),(100,P1),(200,P2)])))
# Monte Carlo check
rng=np.random.default_rng(7); N=2_000_000
a=(rng.random(N)<0.04)*100; b=(rng.random(N)<0.04)*100
for name,x in [("A",a),("B",b),("A+B",a+b)]:
    v=np.quantile(x,0.95,method='inverted_cdf'); print("MC",name,"VaR95",v, "P(x>0)",(x>0).mean())

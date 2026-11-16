import struct, math
from decimal import Decimal, getcontext
from fractions import Fraction
getcontext().prec=80
def bits(x):
    h=struct.unpack('>Q',struct.pack('>d',x))[0]
    b=format(h,'064b'); return h,b
for x in [0.9,0.1,1.0-0.9,0.05,1.0-0.95,0.2,1.0-0.8,0.95,0.8, 0.5]:
    h,b=bits(x); s=b[0]; e=b[1:12]; m=b[12:]
    E=int(e,2)
    print(f"{x!r:24} hex=0x{h:016X} sign={s} exp={e} (E={E}, unbiased {E-1023}) mant={m}")
    print("   exact =",Decimal(x)); print("   frac  =",Fraction(x))
print("1.0-0.9 =",repr(1.0-0.9), " == 0.09999999999999998:",1.0-0.9==0.09999999999999998)
print("Sterbenz exactness: Fraction(1)-Fraction(0.9) == Fraction(1.0-0.9):",Fraction(1)-Fraction(0.9)==Fraction(1.0-0.9))
print("err 0.9 =", Decimal(0.9)-Decimal('0.9'), " ulp(0.9)=",math.ulp(0.9), " 2^-53=",2**-53," 2^-54=",2**-54)
print("err 0.1 =", Decimal(0.1)-Decimal('0.1'), " ulp(0.1)=",math.ulp(0.1))
print("(1-0.9)*10 =",repr((1.0-0.9)*10), " exact product:",Decimal(1.0-0.9)*10)
print("nearest double below 1: ",repr(math.nextafter(1.0,0)))
print("0.1+0.2 =",repr(0.1+0.2))
# epsilon bound
for eps in [1e-9]:
    nmax = eps*2**51
    print("n bound eps*2^51 =",nmax)
# epsilon false positive: c = 0.9 + 5e-11
c=0.9+5e-11; p=(1.0-c)*10
print("c=0.9+5e-11:",repr(c)," (1-c)*10=",repr(p)," floor=",math.floor(p)," floor(+eps)=",math.floor(p+1e-9), " exact (1-c)*10 =",(1-Fraction(c))*10 < 1)
# scan of decimal confidence levels with up to 4 dp, n up to 5000: does eps floor equal exact floor?
bad=0;badnoeps=0;tot=0
for bp in range(1,10000):
    c=bp/10000.0; cdec=Fraction(bp,10000)
    for n in list(range(1,2001))+[10**4,10**5,10**6]:
        exact=math.floor((1-cdec)*n)
        got=int((1.0-c)*float(n)+1e-9); got0=int((1.0-c)*float(n))
        tot+=1; bad+= got!=exact; badnoeps+= got0!=exact
print("4-dp scan: total",tot,"eps mismatches",bad,"no-eps mismatches",badnoeps)

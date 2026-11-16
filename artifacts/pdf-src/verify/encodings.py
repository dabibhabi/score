# Varint / zig-zag / fixed-width encodings quoted in Chapter 4.
def varint(n):
    out=[]
    while True:
        b=n&0x7F; n>>=7
        if n: out.append(b|0x80)
        else: out.append(b); return bytes(out)
for v in [40,127,128,300,9500]: print(v, varint(v).hex(' ').upper())
zz=lambda v: (v<<1)^(v>>63)
print("zigzag -1 ->", zz(-1)&0xFFFFFFFFFFFFFFFF, " zigzag 1 ->", zz(1))
print("9500 u16 BE:", (9500).to_bytes(2,'big').hex(' ').upper(), " 9750:", (9750).to_bytes(2,'big').hex(' ').upper())
print("0xFFFFFFFF =", 0xFFFFFFFF, "bytes =", 0xFFFFFFFF/2**30, "GiB")

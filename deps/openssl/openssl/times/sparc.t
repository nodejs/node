gcc 2.7.2
Sparc 10 - Solaris 2.3 - 50mhz
SSLeay 0.7.3r 20-May-1997
built on Mon Jun  2 00:55:51 EST 1997
options:bn(64,32) md2(int) rc4(ptr,char) des(idx,cisc,16,long) idea(int) blowfish(ptr)
C flags:gcc -O3 -fomit-frame-pointer -mv8 -Wall
The 'numbers' are in 1000s of bytes per second processed.
type              8 bytes     64 bytes    256 bytes   1024 bytes   8192 bytes
md2                54.88k      154.52k      210.35k      231.08k      237.21k
md5               550.75k     2460.49k     4116.01k     4988.74k     5159.86k
sha               340.28k     1461.76k     2430.10k     2879.87k     2999.15k
sha1              307.27k     1298.41k     2136.26k     2540.07k     2658.28k
rc4              2652.21k     2805.24k     3301.63k     4003.98k     4071.18k
des cbc           811.78k      903.93k      914.19k      921.60k      932.29k
des ede3          328.21k      344.93k      349.64k      351.48k      345.07k
idea cbc          685.06k      727.42k      734.41k      730.11k      739.21k
rc2 cbc           718.59k      777.02k      781.96k      784.38k      782.60k
blowfish cbc     1268.85k     1520.64k     1568.88k     1587.54k     1591.98k
rsa  512 bits   0.037s   0.005
rsa 1024 bits   0.213s   0.006
rsa 2048 bits   1.471s   0.053
rsa 4096 bits  11.100s   0.202
dsa  512 bits   0.038s   0.074
dsa 1024 bits   0.128s   0.248
dsa 2048 bits   0.473s   0.959


Sparc Station LX
SSLeay 0.7.3 30-Apr-1997
built on Thu May  1 10:44:02 EST 1997
options:bn(64,32) md2(int) rc4(ptr,char) des(idx,cisc,16,long) idea(int) blowfish(ptr)
C flags:gcc -O3 -fomit-frame-pointer -mv8 -Wall
The 'numbers' are in 1000s of bytes per second processed.
type              8 bytes     64 bytes    256 bytes   1024 bytes   8192 bytes
md2                17.60k       48.72k       66.47k       72.70k       74.72k
md5               226.24k     1082.21k     1982.72k     2594.02k     2717.01k
sha                71.38k      320.71k      551.08k      677.76k      720.90k
sha1               63.08k      280.79k      473.86k      576.94k      608.94k
rc4              1138.30k     1257.67k     1304.49k     1377.78k     1364.42k
des cbc           265.34k      308.85k      314.28k      315.39k      317.20k
des ede3           83.23k       93.13k       94.04k       94.50k       94.63k
idea cbc          254.48k      274.26k      275.88k      274.68k      275.80k
rc2 cbc           328.27k      375.39k      381.43k      381.61k      380.83k
blowfish cbc      487.00k      498.02k      510.12k      515.41k      516.10k
rsa  512 bits   0.093s
rsa 1024 bits   0.537s
rsa 2048 bits   3.823s
rsa 4096 bits  28.650s


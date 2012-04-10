HPUX 887

SSLeay 0.7.3r 20-May-1997
built on Mon Jun  2 02:59:45 EST 1997
options:bn(32,32) md2(int) rc4(ptr,int) des(ptr,risc1,16,long) idea(int) blowfish(idx)
C flags:cc -DB_ENDIAN -D_HPUX_SOURCE -Aa -Ae +ESlit +O4 -Wl,-a,archive
The 'numbers' are in 1000s of bytes per second processed.
type              8 bytes     64 bytes    256 bytes   1024 bytes   8192 bytes
md2                58.99k      166.85k      225.07k      247.21k      253.76k
md5               639.22k     2726.98k     4477.25k     5312.69k     5605.20k
sha               381.08k     1661.49k     2793.84k     3368.86k     3581.23k
sha1              349.54k     1514.56k     2536.63k     3042.59k     3224.39k
rc4              2891.10k     4238.01k     4464.11k     4532.49k     4545.87k
des cbc           717.05k      808.76k      820.14k      821.97k      821.96k
des ede3          288.21k      303.50k      303.69k      305.82k      305.14k
idea cbc          325.83k      334.36k      335.89k      336.61k      333.43k
rc2 cbc           793.00k      915.81k      926.69k      933.28k      929.53k
blowfish cbc     1561.91k     2051.97k     2122.65k     2139.40k     2145.92k
rsa  512 bits   0.031s   0.004
rsa 1024 bits   0.164s   0.004
rsa 2048 bits   1.055s   0.037
rsa 4096 bits   7.600s   0.137
dsa  512 bits   0.029s   0.057
dsa 1024 bits   0.092s   0.177
dsa 2048 bits   0.325s   0.646

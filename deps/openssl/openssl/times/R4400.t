IRIX 5.3
R4400 200mhz
cc -O2
SSLeay 0.6.5a 06-Dec-1996
built on Mon Dec 23 11:51:11 EST 1996
options:bn(32,32) md2(int) rc4(ptr,int) des(ptr,risc2,16,long) idea(int)
C flags:cc -O2 -DTERMIOS -DB_ENDIAN
The 'numbers' are in 1000s of bytes per second processed.
type           8 bytes     64 bytes    256 bytes   1024 bytes   8192 bytes
md2            100.62k      280.25k      380.15k      416.02k      428.82k
md5            828.62k     3525.05k     6311.98k     7742.51k     8328.04k
sha            580.04k     2513.74k     4251.73k     5101.04k     5394.80k
sha1           520.23k     2382.94k     4107.82k     5024.62k     5362.56k
rc4           5871.53k     6323.08k     6357.49k     6392.04k     6305.45k
des cfb       1016.76k     1156.72k     1176.59k     1180.55k     1181.65k
des cbc       1016.38k     1303.81k     1349.10k     1359.41k     1356.62k
des ede3       607.39k      650.74k      655.11k      657.52k      654.18k
idea cfb      1296.10k     1348.66k     1353.80k     1358.75k     1355.40k
idea cbc      1453.90k     1554.68k     1567.84k     1569.89k     1573.57k
rc2 cfb       1199.86k     1251.69k     1253.57k     1259.56k     1251.31k
rc2 cbc       1334.60k     1428.55k     1441.89k     1445.42k     1441.45k
rsa  512 bits   0.024s
rsa 1024 bits   0.125s
rsa 2048 bits   0.806s
rsa 4096 bits   5.800s


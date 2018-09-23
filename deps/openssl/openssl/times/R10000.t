IRIX 6.2 - R10000 195mhz
SLeay 0.6.5a 06-Dec-1996
built on Tue Dec 24 03:51:45 EST 1996
options:bn(32,32) md2(int) rc4(ptr,int) des(ptr,risc2,16,long) idea(int)
C flags:cc -O2 -DTERMIOS -DB_ENDIAN
The 'numbers' are in 1000s of bytes per second processed.
type           8 bytes     64 bytes    256 bytes   1024 bytes   8192 bytes
md2            156.34k      424.03k      571.88k      628.88k      646.01k
md5           1885.02k     8181.72k    13440.53k    16020.60k    16947.54k
sha           1587.12k     7022.05k    11951.24k    14440.12k    15462.74k
sha1          1413.13k     6215.86k    10571.16k    12736.22k    13628.51k
rc4          10556.28k    11974.08k    12077.10k    12111.38k    12103.20k
des cfb       2977.71k     3252.27k     3284.36k     3302.66k     3290.54k
des cbc       3298.31k     3704.96k     3771.30k     3730.73k     3778.80k
des ede3      1278.28k     1328.82k     1342.66k     1339.82k     1343.27k
idea cfb      2843.34k     3138.04k     3180.95k     3176.46k     3188.54k
idea cbc      3115.21k     3558.03k     3590.61k     3591.24k     3601.18k
rc2 cfb       2006.66k     2133.33k     2149.03k     2159.36k     2149.71k
rc2 cbc       2167.07k     2315.30k     2338.05k     2329.34k     2333.90k
rsa  512 bits   0.008s
rsa 1024 bits   0.043s
rsa 2048 bits   0.280s
rsa 4096 bits   2.064s


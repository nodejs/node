Sparc 2000? - Solaris 2.5.1 - 167mhz Ultra sparc

SSLeay 0.7.3r 20-May-1997
built on Mon Jun  2 02:25:48 EST 1997
options:bn(64,32) md2(int) rc4(ptr,char) des(ptr,risc1,16,long) idea(int) blowfish(ptr)
C flags:cc cc -xtarget=ultra -xarch=v8plus -Xa -xO5 -Xa -DB_ENDIAN
The 'numbers' are in 1000s of bytes per second processed.
type              8 bytes     64 bytes    256 bytes   1024 bytes   8192 bytes
md2               135.23k      389.87k      536.66k      591.87k      603.48k
md5              1534.38k     6160.41k     9842.69k    11446.95k    11993.09k
sha              1178.30k     5020.74k     8532.22k    10275.50k    11010.05k
sha1             1114.22k     4703.94k     7703.81k     9236.14k     9756.67k
rc4             10818.03k    13327.57k    13711.10k    13810.69k    13836.29k
des cbc          3052.44k     3320.02k     3356.25k     3369.98k     3295.91k
des ede3         1310.32k     1359.98k     1367.47k     1362.94k     1362.60k
idea cbc         1749.52k     1833.13k     1844.74k     1848.32k     1848.66k
rc2 cbc          1950.25k     2053.23k     2064.21k     2072.58k     2072.58k
blowfish cbc     4927.16k     5659.75k     5762.73k     5797.55k     5805.40k
rsa  512 bits   0.021s   0.003
rsa 1024 bits   0.126s   0.003
rsa 2048 bits   0.888s   0.032
rsa 4096 bits   6.770s   0.122
dsa  512 bits   0.022s   0.043
dsa 1024 bits   0.076s   0.151
dsa 2048 bits   0.286s   0.574

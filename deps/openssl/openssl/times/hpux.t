HP-UX A.09.05 9000/712

SSLeay 0.6.6 14-Jan-1997
built on Tue Jan 14 16:36:31 WET 1997
options:bn(32,32) md2(int) rc4(ptr,int) des(ptr,risc1,16,long) idea(int) 
blowfish(idx)
C flags:cc -DB_ENDIAN -D_HPUX_SOURCE -Aa +ESlit +O2 -Wl,-a,archive
The 'numbers' are in 1000s of bytes per second processed.
type              8 bytes     64 bytes    256 bytes   1024 bytes   8192 bytes
md2                66.56k      184.92k      251.82k      259.86k      282.62k
md5               615.54k     2805.92k     4764.30k     5724.21k     6084.39k
sha               358.23k     1616.46k     2781.50k     3325.72k     3640.89k
sha1              327.50k     1497.98k     2619.44k     3220.26k     3460.85k
rc4              3500.47k     3890.99k     3943.81k     3883.74k     3900.02k
des cbc           742.65k      871.66k      887.15k      891.21k      895.40k
des ede3          302.42k      322.50k      324.46k      326.66k      326.05k
idea cbc          664.41k      755.87k      765.61k      772.70k      773.69k
rc2 cbc           798.78k      931.04k      947.69k      950.31k      952.04k
blowfish cbc     1353.32k     1932.29k     2021.93k     2047.02k     2053.66k
rsa  512 bits   0.059s
rsa 1024 bits   0.372s
rsa 2048 bits   2.697s
rsa 4096 bits  20.790s

SSLeay 0.6.6 14-Jan-1997
built on Tue Jan 14 15:37:30 WET 1997
options:bn(64,32) md2(int) rc4(ptr,int) des(ptr,risc1,16,long) idea(int) 
blowfish(idx)
C flags:gcc -DB_ENDIAN -O3
The 'numbers' are in 1000s of bytes per second processed.
type              8 bytes     64 bytes    256 bytes   1024 bytes   8192 bytes
md2                44.91k      122.57k      167.71k      183.89k      190.24k
md5               532.50k     2316.27k     3965.72k     4740.11k     5055.06k
sha               363.76k     1684.09k     2978.53k     3730.86k     3972.72k
sha1              385.76k     1743.53k     2997.69k     3650.74k     3899.08k
rc4              3178.84k     3621.31k     3672.71k     3684.01k     3571.54k
des cbc           733.00k      844.70k      863.28k      863.72k      868.73k
des ede3          289.99k      308.94k      310.11k      309.64k      312.08k
idea cbc          624.07k      713.91k      724.76k      723.35k      725.13k
rc2 cbc           704.34k      793.39k      804.25k      805.99k      782.63k
blowfish cbc     1371.24k     1823.66k     1890.05k     1915.51k     1920.12k
rsa  512 bits   0.030s
rsa 1024 bits   0.156s
rsa 2048 bits   1.113s
rsa 4096 bits   7.480s


HPUX B.10.01 V 9000/887 - HP92453-01 A.10.11 HP C Compiler
SSLeay 0.5.2 - -Aa +ESlit +Oall +O4 -Wl,-a,archive

HPUX A.09.04 B 9000/887

ssleay 0.5.1 gcc v 2.7.0 -O3 -mpa-risc-1-1
type           8 bytes     64 bytes    256 bytes   1024 bytes   8192 bytes
md2             53.00k      166.81k      205.66k      241.95k      242.20k
md5            743.22k     3128.44k     6031.85k     6142.07k     7025.26k
sha            481.30k     2008.24k     3361.31k     3985.07k     4180.74k
sha-1          463.60k     1916.15k     3139.24k     3786.27k     3997.70k
rc4           3708.61k     4125.16k     4547.53k     4206.21k     4390.07k
des cfb        665.91k      705.97k      698.48k      694.25k      666.08k
des cbc        679.80k      741.90k      769.85k      747.62k      719.47k
des ede3       264.31k      270.22k      265.63k      273.07k      273.07k
idea cfb       635.91k      673.40k      605.60k      699.53k      672.36k
idea cbc       705.85k      774.63k      750.60k      715.83k      721.50k
rsa  512 bits   0.066s
rsa 1024 bits   0.372s
rsa 2048 bits   2.177s
rsa 4096 bits  16.230s

HP92453-01 A.09.61 HP C Compiler
ssleay 0.5.1 cc -Ae +ESlit +Oall -Wl,-a,archive
type           8 bytes     64 bytes    256 bytes   1024 bytes   8192 bytes
md2             58.69k      163.30k      213.57k      230.40k      254.23k
md5            608.60k     2596.82k     3871.43k     4684.10k     4763.88k
sha            343.26k     1482.43k     2316.80k     2766.27k     2860.26k
sha-1          319.15k     1324.13k     2106.03k     2527.82k     2747.95k
rc4           2467.47k     3374.41k     3265.49k     3354.39k     3368.55k
des cfb        812.05k      814.90k      851.20k      819.20k      854.56k
des cbc        836.35k      994.06k      916.02k     1020.01k      988.14k
des ede3       369.78k      389.15k      401.01k      382.94k      408.03k
idea cfb       290.40k      298.06k      286.11k      296.92k      299.46k
idea cbc       301.30k      297.72k      304.34k      300.10k      309.70k
rsa  512 bits   0.350s
rsa 1024 bits   2.635s
rsa 2048 bits  19.930s


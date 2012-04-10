SSLeay-051 Alpha gcc -O3 64Bit (assember bn_mul)
type           8 bytes     64 bytes    256 bytes   1024 bytes   8192 bytes
md2             44.40k      121.56k      162.73k      179.20k      185.01k
md5            780.85k     3278.53k     5281.52k     6327.98k     6684.67k
sha            501.40k     2249.19k     3855.27k     4801.19k     5160.96k
sha-1          384.99k     1759.72k     3113.64k     3946.92k     4229.80k
rc4           3505.05k     3724.54k     3723.78k     3555.33k     3694.68k
des cfb        946.96k     1015.27k     1021.87k     1033.56k     1037.65k
des cbc       1001.24k     1220.20k     1243.31k     1272.73k     1265.87k
des ede3       445.34k      491.65k      500.53k      502.10k      502.44k
idea cfb       643.53k      667.49k      663.81k      666.28k      664.51k
idea cbc       650.42k      735.41k      733.27k      742.74k      745.47k
rsa  512 bits   0.031s
rsa 1024 bits   0.141s
rsa 2048 bits   0.844s
rsa 4096 bits   6.033s

SSLeay-051 Alpha cc -O2 64bit (assember bn_mul)
type           8 bytes     64 bytes    256 bytes   1024 bytes   8192 bytes
md2             45.37k      122.86k      165.97k      182.95k      188.42k
md5            842.42k     3629.93k     5916.76k     7039.17k     7364.61k
sha            498.93k     2197.23k     3895.60k     4756.48k     5132.13k
sha-1          382.02k     1757.21k     3112.53k     3865.23k     4128.77k
rc4           2975.25k     3049.33k     3180.97k     3214.68k     3424.26k
des cfb        901.55k      990.83k     1006.08k     1011.19k     1004.89k
des cbc        947.84k     1127.84k     1163.67k     1162.24k     1157.80k
des ede3       435.62k      485.57k      493.67k      491.52k      491.52k
idea cfb       629.31k      648.66k      647.77k      648.53k      649.90k
idea cbc       565.15k      608.00k      613.46k      613.38k      617.13k
rsa  512 bits   0.030s
rsa 1024 bits   0.141s
rsa 2048 bits   0.854s
rsa 4096 bits   6.067s

des cfb        718.28k      822.64k      833.11k      836.27k      841.05k
des cbc        806.10k      951.42k      975.83k      983.73k      991.23k
des ede3       329.50k      379.11k      387.95k      387.41k      388.33k

des cfb        871.62k      948.65k      951.81k      953.00k      955.58k
des cbc        953.60k     1174.27k     1206.70k     1216.10k     1216.44k
des ede3       349.34k      418.05k      427.26k      429.74k      431.45k




SSLeay-045c Alpha gcc -O3 64Bit
type           8 bytes     64 bytes    256 bytes   1024 bytes   8192 bytes
md2             44.95k      122.22k      164.27k      180.62k      184.66k
md5            808.71k     3371.95k     5415.68k     6385.66k     6684.67k
sha            493.68k     2162.05k     3725.82k     4552.02k     4838.74k
rc4           3317.32k     3649.09k     3728.30k     3744.09k     3691.86k
cfb des        996.45k     1050.77k     1058.30k     1059.16k     1064.96k
cbc des       1096.52k     1255.49k     1282.13k     1289.90k     1299.80k
ede3 des       482.14k      513.51k      518.66k      520.19k      521.39k
cfb idea       519.90k      533.40k      535.21k      535.55k      535.21k
cbc idea       619.34k      682.21k      688.04k      689.15k      690.86k
rsa  512 bits   0.050s
rsa 1024 bits   0.279s
rsa 2048 bits   1.908s
rsa 4096 bits  14.750s

type           8 bytes     64 bytes    256 bytes   1024 bytes   8192 bytes
md2             37.31k      102.77k      137.64k      151.55k      155.78k
md5            516.65k     2535.21k     4655.72k     5859.66k     6343.34k
rc4           3519.61k     3707.01k     3746.86k     3755.39k     3675.48k
cfb des        780.27k      894.68k      913.10k      921.26k      922.97k
cbc des        867.54k     1040.13k     1074.17k     1075.54k     1084.07k
ede3 des       357.19k      397.36k      398.08k      402.28k      401.41k
cbc idea       646.53k      686.44k      694.03k      691.20k      693.59k
rsa  512 bits   0.046s
rsa 1024 bits   0.270s
rsa 2048 bits   1.858s
rsa 4096 bits  14.350s

md2      C      37.83k      103.17k      137.90k      150.87k      155.37k
md2      L      37.30k      102.04k      139.01k      152.74k      155.78k
rc4       I   3532.24k     3718.08k     3750.83k     3768.78k     3694.59k
rc4      CI   2662.97k     2873.26k     2907.22k     2920.63k     2886.31k
rc4      LI   3514.63k     3738.72k     3747.41k     3752.96k     3708.49k
cbc idea S     619.01k      658.68k      661.50k      662.53k      663.55k
cbc idea  L    645.69k      684.22k      694.55k      692.57k      690.86k

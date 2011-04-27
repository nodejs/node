## os Module

Use `require('os')` to access this module.

### os.hostname()

Returns the hostname of the operating system.

### os.type()

Returns the operating system name.

### os.platform()

Returns the operating system platform.

### os.arch()

Returns the operating system CPU architecture.

### os.release()

Returns the operating system release.

### os.uptime()

Returns the system uptime in seconds.

### os.loadavg()

Returns an array containing the 1, 5, and 15 minute load averages.

### os.totalmem()

Returns the total amount of system memory in bytes.

### os.freemem()

Returns the amount of free system memory in bytes.

### os.cpus()

Returns an array of objects containing information about each CPU/core installed: model, speed (in MHz), and times (an object containing the number of CPU ticks spent in: user, nice, sys, idle, and irq).

Example inspection of os.cpus:

    [ { model: 'Intel(R) Core(TM) i7 CPU         860  @ 2.80GHz',
        speed: 2926,
        times:
         { user: 252020,
           nice: 0,
           sys: 30340,
           idle: 1070356870,
           irq: 0 } },
      { model: 'Intel(R) Core(TM) i7 CPU         860  @ 2.80GHz',
        speed: 2926,
        times:
         { user: 306960,
           nice: 0,
           sys: 26980,
           idle: 1071569080,
           irq: 0 } },
      { model: 'Intel(R) Core(TM) i7 CPU         860  @ 2.80GHz',
        speed: 2926,
        times:
         { user: 248450,
           nice: 0,
           sys: 21750,
           idle: 1070919370,
           irq: 0 } },
      { model: 'Intel(R) Core(TM) i7 CPU         860  @ 2.80GHz',
        speed: 2926,
        times:
         { user: 256880,
           nice: 0,
           sys: 19430,
           idle: 1070905480,
           irq: 20 } },
      { model: 'Intel(R) Core(TM) i7 CPU         860  @ 2.80GHz',
        speed: 2926,
        times:
         { user: 511580,
           nice: 20,
           sys: 40900,
           idle: 1070842510,
           irq: 0 } },
      { model: 'Intel(R) Core(TM) i7 CPU         860  @ 2.80GHz',
        speed: 2926,
        times:
         { user: 291660,
           nice: 0,
           sys: 34360,
           idle: 1070888000,
           irq: 10 } },
      { model: 'Intel(R) Core(TM) i7 CPU         860  @ 2.80GHz',
        speed: 2926,
        times:
         { user: 308260,
           nice: 0,
           sys: 55410,
           idle: 1071129970,
           irq: 880 } },
      { model: 'Intel(R) Core(TM) i7 CPU         860  @ 2.80GHz',
        speed: 2926,
        times:
         { user: 266450,
           nice: 1480,
           sys: 34920,
           idle: 1072572010,
           irq: 30 } } ]

### os.getNetworkInterfaces()

Get a list of network interfaces:

    { lo: { ip: '127.0.0.1', internal: true, ip6: '::1' },
      eth0: { ip6: 'fe80::f2de:f1ff:fe19:ae7', internal: false },
      wlan0: { ip: '10.0.1.118', internal: false, ip6: 'fe80::226:c7ff:fe7d:1602' },
      vboxnet0: {} }



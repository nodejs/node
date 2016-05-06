# OS

    Stability: 2 - Stable

Provides a few basic operating-system related utility functions.

Use `require('os')` to access this module.

## os.EOL
<!-- YAML
added: v0.7.8
-->

A constant defining the appropriate End-of-line marker for the operating
system.

## os.arch()
<!-- YAML
added: v0.5.0
-->

Returns the operating system CPU architecture. Possible values are `'x64'`,
`'arm'` and `'ia32'`. Returns the value of [`process.arch`][].

## os.cpus()
<!-- YAML
added: v0.3.3
-->

Returns an array of objects containing information about each CPU/core
installed: model, speed (in MHz), and times (an object containing the number of
milliseconds the CPU/core spent in: user, nice, sys, idle, and irq).

Example inspection of os.cpus:

```js
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
```

Note that since `nice` values are UNIX centric in Windows the `nice` values of
all processors are always 0.

## os.endianness()
<!-- YAML
added: v0.9.4
-->

Returns the endianness of the CPU. Possible values are `'BE'` for big endian
or `'LE'` for little endian.

## os.freemem()
<!-- YAML
added: v0.3.3
-->

Returns the amount of free system memory in bytes.

## os.homedir()
<!-- YAML
added: v2.3.0
-->

Returns the home directory of the current user.

## os.hostname()
<!-- YAML
added: v0.3.3
-->

Returns the hostname of the operating system.

## os.loadavg()
<!-- YAML
added: v0.3.3
-->

Returns an array containing the 1, 5, and 15 minute load averages.

The load average is a measure of system activity, calculated by the operating
system and expressed as a fractional number.  As a rule of thumb, the load
average should ideally be less than the number of logical CPUs in the system.

The load average is a very UNIX-y concept; there is no real equivalent on
Windows platforms.  That is why this function always returns `[0, 0, 0]` on
Windows.

## os.networkInterfaces()
<!-- YAML
added: v0.6.0
-->

Get a list of network interfaces:

```js
{ lo:
   [ { address: '127.0.0.1',
       netmask: '255.0.0.0',
       family: 'IPv4',
       mac: '00:00:00:00:00:00',
       internal: true },
     { address: '::1',
       netmask: 'ffff:ffff:ffff:ffff:ffff:ffff:ffff:ffff',
       family: 'IPv6',
       mac: '00:00:00:00:00:00',
       internal: true } ],
  eth0:
   [ { address: '192.168.1.108',
       netmask: '255.255.255.0',
       family: 'IPv4',
       mac: '01:02:03:0a:0b:0c',
       internal: false },
     { address: 'fe80::a00:27ff:fe4e:66a1',
       netmask: 'ffff:ffff:ffff:ffff::',
       family: 'IPv6',
       mac: '01:02:03:0a:0b:0c',
       internal: false } ] }
```

Note that due to the underlying implementation this will only return network
interfaces that have been assigned an address.

## os.platform()
<!-- YAML
added: v0.5.0
-->

Returns the operating system platform. Possible values are `'darwin'`,
`'freebsd'`, `'linux'`, `'sunos'` or `'win32'`. Returns the value of
[`process.platform`][].

## os.release()
<!-- YAML
added: v0.3.3
-->

Returns the operating system release.

## os.tmpdir()
<!-- YAML
added: v0.9.9
-->

Returns the operating system's default directory for temporary files.

## os.totalmem()
<!-- YAML
added: v0.3.3
-->

Returns the total amount of system memory in bytes.

## os.type()
<!-- YAML
added: v0.3.3
-->

Returns the operating system name. For example `'Linux'` on Linux, `'Darwin'`
on OS X and `'Windows_NT'` on Windows.

## os.uptime()
<!-- YAML
added: v0.3.3
-->

Returns the system uptime in seconds.

[`process.arch`]: process.html#process_process_arch
[`process.platform`]: process.html#process_process_platform

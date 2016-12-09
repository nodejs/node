# OS

> Stability: 2 - Stable

The `os` module provides a number of operating system-related utility methods.
It can be accessed using:

```js
const os = require('os');
```

## os.EOL
<!-- YAML
added: v0.7.8
-->

* {String}

A string constant defining the operating system-specific end-of-line marker:

* `\n` on POSIX
* `\r\n` on Windows

## os.arch()
<!-- YAML
added: v0.5.0
-->

The `os.arch()` method returns a string identifying the operating system CPU
architecture *for which the Node.js binary was compiled*.

The current possible values are: `'arm'`, `'arm64'`, `'ia32'`, `'mips'`,
`'mipsel'`, `'ppc'`, `'ppc64'`, `'s390'`, `'s390x'`, `'x32'`, `'x64'`,  and
`'x86'`.

Equivalent to [`process.arch`][].

## os.constants

* {Object}

Returns an object containing commonly used operating system specific constants
for error codes, process signals, and so on. The specific constants currently
defined are described in [OS Constants][].

## os.cpus()
<!-- YAML
added: v0.3.3
-->

* Returns: {Array}

The `os.cpus()` method returns an array of objects containing information about
each CPU/core installed.

The properties included on each object include:

* `model` {String}
* `speed` {number} (in MHz)
* `times` {Object}
  * `user` {number} The number of milliseconds the CPU has spent in user mode.
  * `nice` {number} The number of milliseconds the CPU has spent in nice mode.
  * `sys` {number} The number of milliseconds the CPU has spent in sys mode.
  * `idle` {number} The number of milliseconds the CPU has spent in idle mode.
  * `irq` {number} The number of milliseconds the CPU has spent in irq mode.

For example:

```js
[
  {
    model: 'Intel(R) Core(TM) i7 CPU         860  @ 2.80GHz',
    speed: 2926,
    times: {
      user: 252020,
      nice: 0,
      sys: 30340,
      idle: 1070356870,
      irq: 0
    }
  },
  {
    model: 'Intel(R) Core(TM) i7 CPU         860  @ 2.80GHz',
    speed: 2926,
    times: {
      user: 306960,
      nice: 0,
      sys: 26980,
      idle: 1071569080,
      irq: 0
    }
  },
  {
    model: 'Intel(R) Core(TM) i7 CPU         860  @ 2.80GHz',
    speed: 2926,
    times: {
      user: 248450,
      nice: 0,
      sys: 21750,
      idle: 1070919370,
      irq: 0
    }
  },
  {
    model: 'Intel(R) Core(TM) i7 CPU         860  @ 2.80GHz',
    speed: 2926,
    times: {
      user: 256880,
      nice: 0,
      sys: 19430,
      idle: 1070905480,
      irq: 20
    }
  },
  {
    model: 'Intel(R) Core(TM) i7 CPU         860  @ 2.80GHz',
    speed: 2926,
    times: {
      user: 511580,
      nice: 20,
      sys: 40900,
      idle: 1070842510,
      irq: 0
    }
  },
  {
    model: 'Intel(R) Core(TM) i7 CPU         860  @ 2.80GHz',
    speed: 2926,
    times: {
      user: 291660,
      nice: 0,
      sys: 34360,
      idle: 1070888000,
      irq: 10
    }
  },
  {
    model: 'Intel(R) Core(TM) i7 CPU         860  @ 2.80GHz',
    speed: 2926,
    times: {
      user: 308260,
      nice: 0,
      sys: 55410,
      idle: 1071129970,
      irq: 880
    }
  },
  {
    model: 'Intel(R) Core(TM) i7 CPU         860  @ 2.80GHz',
    speed: 2926,
    times: {
      user: 266450,
      nice: 1480,
      sys: 34920,
      idle: 1072572010,
      irq: 30
    }
  }
]
```

*Note*: Because `nice` values are UNIX-specific, on Windows the `nice` values of
all processors are always 0.

## os.endianness()
<!-- YAML
added: v0.9.4
-->

* Returns: {String}

The `os.endianness()` method returns a string identifying the endianness of the
CPU *for which the Node.js binary was compiled*.

Possible values are:

* `'BE'` for big endian
* `'LE'` for little endian.

## os.freemem()
<!-- YAML
added: v0.3.3
-->

* Returns: {Integer}

The `os.freemem()` method returns the amount of free system memory in bytes as
an integer.

## os.homedir()
<!-- YAML
added: v2.3.0
-->

* Returns: {String}

The `os.homedir()` method returns the home directory of the current user as a
string.

## os.hostname()
<!-- YAML
added: v0.3.3
-->

* Returns: {String}

The `os.hostname()` method returns the hostname of the operating system as a
string.

## os.loadavg()
<!-- YAML
added: v0.3.3
-->

* Returns: {Array}

The `os.loadavg()` method returns an array containing the 1, 5, and 15 minute
load averages.

The load average is a measure of system activity, calculated by the operating
system and expressed as a fractional number.  As a rule of thumb, the load
average should ideally be less than the number of logical CPUs in the system.

The load average is a UNIX-specific concept with no real equivalent on
Windows platforms. On Windows, the return value is always `[0, 0, 0]`.

## os.networkInterfaces()
<!-- YAML
added: v0.6.0
-->

* Returns: {Object}

The `os.networkInterfaces()` method returns an object containing only network
interfaces that have been assigned a network address.

Each key on the returned object identifies a network interface. The associated
value is an array of objects that each describe an assigned network address.

The properties available on the assigned network address object include:

* `address` {String} The assigned IPv4 or IPv6 address
* `netmask` {String} The IPv4 or IPv6 network mask
* `family` {String} Either `IPv4` or `IPv6`
* `mac` {String} The MAC address of the network interface
* `internal` {boolean} `true` if the network interface is a loopback or
  similar interface that is not remotely accessible; otherwise `false`
* `scopeid` {number} The numeric IPv6 scope ID (only specified when `family`
  is `IPv6`)

```js
{
  lo: [
    {
      address: '127.0.0.1',
      netmask: '255.0.0.0',
      family: 'IPv4',
      mac: '00:00:00:00:00:00',
      internal: true
    },
    {
      address: '::1',
      netmask: 'ffff:ffff:ffff:ffff:ffff:ffff:ffff:ffff',
      family: 'IPv6',
      mac: '00:00:00:00:00:00',
      internal: true
    }
  ],
  eth0: [
    {
      address: '192.168.1.108',
      netmask: '255.255.255.0',
      family: 'IPv4',
      mac: '01:02:03:0a:0b:0c',
      internal: false
    },
    {
      address: 'fe80::a00:27ff:fe4e:66a1',
      netmask: 'ffff:ffff:ffff:ffff::',
      family: 'IPv6',
      mac: '01:02:03:0a:0b:0c',
      internal: false
    }
  ]
}
```

## os.platform()
<!-- YAML
added: v0.5.0
-->

* Returns: {String}

The `os.platform()` method returns a string identifying the operating system
platform as set during compile time of Node.js.

Currently possible values are:

* `'aix'`
* `'darwin'`
* `'freebsd'`
* `'linux'`
* `'openbsd'`
* `'sunos'`
* `'win32'`

Equivalent to [`process.platform`][].

*Note*: The value `'android'` may also be returned if the Node.js is built on
the Android operating system. However, Android support in Node.js is considered
to be experimental at this time.

## os.release()
<!-- YAML
added: v0.3.3
-->

* Returns: {String}

The `os.release()` method returns a string identifying the operating system
release.

*Note*: On POSIX systems, the operating system release is determined by calling
uname(3). On Windows, `GetVersionExW()` is used. Please see
https://en.wikipedia.org/wiki/Uname#Examples for more information.

## os.tmpdir()
<!-- YAML
added: v0.9.9
-->

* Returns: {String}

The `os.tmpdir()` method returns a string specifying the operating system's
default directory for temporary files.

## os.totalmem()
<!-- YAML
added: v0.3.3
-->

* Returns: {Integer}

The `os.totalmem()` method returns the total amount of system memory in bytes
as an integer.

## os.type()
<!-- YAML
added: v0.3.3
-->

* Returns: {String}

The `os.type()` method returns a string identifying the operating system name
as returned by uname(3). For example `'Linux'` on Linux, `'Darwin'` on OS X and
`'Windows_NT'` on Windows.

Please see https://en.wikipedia.org/wiki/Uname#Examples for additional
information about the output of running uname(3) on various operating systems.

## os.uptime()
<!-- YAML
added: v0.3.3
-->

* Returns: {Integer}

The `os.uptime()` method returns the system uptime in number of seconds.

*Note*: Within Node.js' internals, this number is represented as a `double`.
However, fractional seconds are not returned and the value can typically be
treated as an integer.

## os.userInfo([options])
<!-- YAML
added: v6.0.0
-->

* `options` {Object}
  * `encoding` {String} Character encoding used to interpret resulting strings.
    If `encoding` is set to `'buffer'`, the `username`, `shell`, and `homedir`
    values will be `Buffer` instances. (Default: 'utf8')
* Returns: {Object}

The `os.userInfo()` method returns information about the currently effective
user -- on POSIX platforms, this is typically a subset of the password file. The
returned object includes the `username`, `uid`, `gid`, `shell`, and `homedir`.
On Windows, the `uid` and `gid` fields are `-1`, and `shell` is `null`.

The value of `homedir` returned by `os.userInfo()` is provided by the operating
system. This differs from the result of `os.homedir()`, which queries several
environment variables for the home directory before falling back to the
operating system response.

## OS Constants

The following constants are exported by `os.constants`. **Note:** Not all
constants will be available on every operating system.

### Signal Constants

The following signal constants are exported by `os.constants.signals`:

<table>
  <tr>
    <th>Constant</th>
    <th>Description</th>
  </tr>
  <tr>
    <td><code>SIGHUP</code></td>
    <td>Sent to indicate when a controlling terminal is closed or a parent
    process exits.</td>
  </tr>
  <tr>
    <td><code>SIGINT</code></td>
    <td>Sent to indicate when a user wishes to interrupt a process
    (`(Ctrl+C)`).</td>
  </tr>
  <tr>
    <td><code>SIGQUIT</code></td>
    <td>Sent to indicate when a user wishes to terminate a process and perform a
    core dump.</td>
  </tr>
  <tr>
    <td><code>SIGILL</code></td>
    <td>Sent to a process to notify that it has attempted to perform an illegal,
    malformed, unknown or privileged instruction.</td>
  </tr>
  <tr>
    <td><code>SIGTRAP</code></td>
    <td>Sent to a process when an exception has occurred.</td>
  </tr>
  <tr>
    <td><code>SIGABRT</code></td>
    <td>Sent to a process to request that it abort.</td>
  </tr>
  <tr>
    <td><code>SIGIOT</code></td>
    <td>Synonym for <code>SIGABRT</code></td>
  </tr>
  <tr>
    <td><code>SIGBUS</code></td>
    <td>Sent to a process to notify that it has caused a bus error.</td>
  </tr>
  <tr>
    <td><code>SIGFPE</code></td>
    <td>Sent to a process to notify that it has performed an illegal arithmetic
    operation.</td>
  </tr>
  <tr>
    <td><code>SIGKILL</code></td>
    <td>Sent to a process to terminate it immediately.</td>
  </tr>
  <tr>
    <td><code>SIGUSR1</code> <code>SIGUSR2</code></td>
    <td>Sent to a process to identify user-defined conditions.</td>
  </tr>
  <tr>
    <td><code>SIGSEGV</code></td>
    <td>Sent to a process to notify of a segmentation fault.</td>
  </tr>
  <tr>
    <td><code>SIGPIPE</code></td>
    <td>Sent to a process when it has attempted to write to a disconnected
    pipe.</td>
  </tr>
  <tr>
    <td><code>SIGALRM</code></td>
    <td>Sent to a process when a system timer elapses.</td>
  </tr>
  <tr>
    <td><code>SIGTERM</code></td>
    <td>Sent to a process to request termination.</td>
  </tr>
  <tr>
    <td><code>SIGCHLD</code></td>
    <td>Sent to a process when a child process terminates.</td>
  </tr>
  <tr>
    <td><code>SIGSTKFLT</code></td>
    <td>Sent to a process to indicate a stack fault on a coprocessor.</td>
  </tr>
  <tr>
    <td><code>SIGCONT</code></td>
    <td>Sent to instruct the operating system to continue a paused process.</td>
  </tr>
  <tr>
    <td><code>SIGSTOP</code></td>
    <td>Sent to instruct the operating system to halt a process.</td>
  </tr>
  <tr>
    <td><code>SIGTSTP</code></td>
    <td>Sent to a process to request it to stop.</td>
  </tr>
  <tr>
    <td><code>SIGBREAK</code></td>
    <td>Sent to indicate when a user wishes to interrupt a process.</td>
  </tr>
  <tr>
    <td><code>SIGTTIN</code></td>
    <td>Sent to a process when it reads from the TTY while in the
    background.</td>
  </tr>
  <tr>
    <td><code>SIGTTOU</code></td>
    <td>Sent to a process when it writes to the TTY while in the
    background.</td>
  </tr>
  <tr>
    <td><code>SIGURG</code></td>
    <td>Sent to a process when a socket has urgent data to read.</td>
  </tr>
  <tr>
    <td><code>SIGXCPU</code></td>
    <td>Sent to a process when it has exceeded its limit on CPU usage.</td>
  </tr>
  <tr>
    <td><code>SIGXFSZ</code></td>
    <td>Sent to a process when it grows a file larger than the maximum
    allowed.</td>
  </tr>
  <tr>
    <td><code>SIGVTALRM</code></td>
    <td>Sent to a process when a virtual timer has elapsed.</td>
  </tr>
  <tr>
    <td><code>SIGPROF</code></td>
    <td>Sent to a process when a system timer has elapsed.</td>
  </tr>
  <tr>
    <td><code>SIGWINCH</code></td>
    <td>Sent to a process when the controlling terminal has changed its
    size.</td>
  </tr>
  <tr>
    <td><code>SIGIO</code></td>
    <td>Sent to a process when I/O is available.</td>
  </tr>
  <tr>
    <td><code>SIGPOLL</code></td>
    <td>Synonym for <code>SIGIO</code></td>
  </tr>
  <tr>
    <td><code>SIGLOST</code></td>
    <td>Sent to a process when a file lock has been lost.</td>
  </tr>
  <tr>
    <td><code>SIGPWR</code></td>
    <td>Sent to a process to notify of a power failure.</td>
  </tr>
  <tr>
    <td><code>SIGINFO</code></td>
    <td>Synonym for <code>SIGPWR</code></td>
  </tr>
  <tr>
    <td><code>SIGSYS</code></td>
    <td>Sent to a process to notify of a bad argument.</td>
  </tr>
  <tr>
    <td><code>SIGUNUSED</code></td>
    <td>Synonym for <code>SIGSYS</code></td>
  </tr>
</table>

### Error Constants

The following error constants are exported by `os.constants.errno`:

#### POSIX Error Constants

<table>
  <tr>
    <th>Constant</th>
    <th>Description</th>
  </tr>
  <tr>
    <td><code>E2BIG</code></td>
    <td>Indicates that the list of arguments is longer than expected.</td>
  </tr>
  <tr>
    <td><code>EACCES</code></td>
    <td>Indicates that the operation did not have sufficient permissions.</td>
  </tr>
  <tr>
    <td><code>EADDRINUSE</code></td>
    <td>Indicates that the network address is already in use.</td>
  </tr>
  <tr>
    <td><code>EADDRNOTAVAIL</code></td>
    <td>Indicates that the network address is currently unavailable for
    use.</td>
  </tr>
  <tr>
    <td><code>EAFNOSUPPORT</code></td>
    <td>Indicates that the network address family is not supported.</td>
  </tr>
  <tr>
    <td><code>EAGAIN</code></td>
    <td>Indicates that there is currently no data available and to try the
    operation again later.</td>
  </tr>
  <tr>
    <td><code>EALREADY</code></td>
    <td>Indicates that the socket already has a pending connection in
    progress.</td>
  </tr>
  <tr>
    <td><code>EBADF</code></td>
    <td>Indicates that a file descriptor is not valid.</td>
  </tr>
  <tr>
    <td><code>EBADMSG</code></td>
    <td>Indicates an invalid data message.</td>
  </tr>
  <tr>
    <td><code>EBUSY</code></td>
    <td>Indicates that a device or resource is busy.</td>
  </tr>
  <tr>
    <td><code>ECANCELED</code></td>
    <td>Indicates that an operation was canceled.</td>
  </tr>
  <tr>
    <td><code>ECHILD</code></td>
    <td>Indicates that there are no child processes.</td>
  </tr>
  <tr>
    <td><code>ECONNABORTED</code></td>
    <td>Indicates that the network connection has been aborted.</td>
  </tr>
  <tr>
    <td><code>ECONNREFUSED</code></td>
    <td>Indicates that the network connection has been refused.</td>
  </tr>
  <tr>
    <td><code>ECONNRESET</code></td>
    <td>Indicates that the network connection has been reset.</td>
  </tr>
  <tr>
    <td><code>EDEADLK</code></td>
    <td>Indicates that a resource deadlock has been avoided.</td>
  </tr>
  <tr>
    <td><code>EDESTADDRREQ</code></td>
    <td>Indicates that a destination address is required.</td>
  </tr>
  <tr>
    <td><code>EDOM</code></td>
    <td>Indicates that an argument is out of the domain of the function.</td>
  </tr>
  <tr>
    <td><code>EDQUOT</code></td>
    <td>Indicates that the disk quota has been exceeded.</td>
  </tr>
  <tr>
    <td><code>EEXIST</code></td>
    <td>Indicates that the file already exists.</td>
  </tr>
  <tr>
    <td><code>EFAULT</code></td>
    <td>Indicates an invalid pointer address.</td>
  </tr>
  <tr>
    <td><code>EFBIG</code></td>
    <td>Indicates that the file is too large.</td>
  </tr>
  <tr>
    <td><code>EHOSTUNREACH</code></td>
    <td>Indicates that the host is unreachable.</td>
  </tr>
  <tr>
    <td><code>EIDRM</code></td>
    <td>Indicates that the identifier has been removed.</td>
  </tr>
  <tr>
    <td><code>EILSEQ</code></td>
    <td>Indicates an illegal byte sequence.</td>
  </tr>
  <tr>
    <td><code>EINPROGRESS</code></td>
    <td>Indicates that an operation is already in progress.</td>
  </tr>
  <tr>
    <td><code>EINTR</code></td>
    <td>Indicates that a function call was interrupted.</td>
  </tr>
  <tr>
    <td><code>EINVAL</code></td>
    <td>Indicates that an invalid argument was provided.</td>
  </tr>
  <tr>
    <td><code>EIO</code></td>
    <td>Indicates an otherwise unspecified I/O error.</td>
  </tr>
  <tr>
    <td><code>EISCONN</code></td>
    <td>Indicates that the socket is connected.</td>
  </tr>
  <tr>
    <td><code>EISDIR</code></td>
    <td>Indicates that the path is a directory.</td>
  </tr>
  <tr>
    <td><code>ELOOP</code></td>
    <td>Indicates too many levels of symbolic links in a path.</td>
  </tr>
  <tr>
    <td><code>EMFILE</code></td>
    <td>Indicates that there are too many open files.</td>
  </tr>
  <tr>
    <td><code>EMLINK</code></td>
    <td>Indicates that there are too many hard links to a file.</td>
  </tr>
  <tr>
    <td><code>EMSGSIZE</code></td>
    <td>Indicates that the provided message is too long.</td>
  </tr>
  <tr>
    <td><code>EMULTIHOP</code></td>
    <td>Indicates that a multihop was attempted.</td>
  </tr>
  <tr>
    <td><code>ENAMETOOLONG</code></td>
    <td>Indicates that the filename is too long.</td>
  </tr>
  <tr>
    <td><code>ENETDOWN</code></td>
    <td>Indicates that the network is down.</td>
  </tr>
  <tr>
    <td><code>ENETRESET</code></td>
    <td>Indicates that the connection has been aborted by the network.</td>
  </tr>
  <tr>
    <td><code>ENETUNREACH</code></td>
    <td>Indicates that the network is unreachable.</td>
  </tr>
  <tr>
    <td><code>ENFILE</code></td>
    <td>Indicates too many open files in the system.</td>
  </tr>
  <tr>
    <td><code>ENOBUFS</code></td>
    <td>Indicates that no buffer space is available.</td>
  </tr>
  <tr>
    <td><code>ENODATA</code></td>
    <td>Indicates that no message is available on the stream head read
    queue.</td>
  </tr>
  <tr>
    <td><code>ENODEV</code></td>
    <td>Indicates that there is no such device.</td>
  </tr>
  <tr>
    <td><code>ENOENT</code></td>
    <td>Indicates that there is no such file or directory.</td>
  </tr>
  <tr>
    <td><code>ENOEXEC</code></td>
    <td>Indicates an exec format error.</td>
  </tr>
  <tr>
    <td><code>ENOLCK</code></td>
    <td>Indicates that there are no locks available.</td>
  </tr>
  <tr>
    <td><code>ENOLINK</code></td>
    <td>Indications that a link has been severed.</td>
  </tr>
  <tr>
    <td><code>ENOMEM</code></td>
    <td>Indicates that there is not enough space.</td>
  </tr>
  <tr>
    <td><code>ENOMSG</code></td>
    <td>Indicates that there is no message of the desired type.</td>
  </tr>
  <tr>
    <td><code>ENOPROTOOPT</code></td>
    <td>Indicates that a given protocol is not available.</td>
  </tr>
  <tr>
    <td><code>ENOSPC</code></td>
    <td>Indicates that there is no space available on the device.</td>
  </tr>
  <tr>
    <td><code>ENOSR</code></td>
    <td>Indicates that there are no stream resources available.</td>
  </tr>
  <tr>
    <td><code>ENOSTR</code></td>
    <td>Indicates that a given resource is not a stream.</td>
  </tr>
  <tr>
    <td><code>ENOSYS</code></td>
    <td>Indicates that a function has not been implemented.</td>
  </tr>
  <tr>
    <td><code>ENOTCONN</code></td>
    <td>Indicates that the socket is not connected.</td>
  </tr>
  <tr>
    <td><code>ENOTDIR</code></td>
    <td>Indicates that the path is not a directory.</td>
  </tr>
  <tr>
    <td><code>ENOTEMPTY</code></td>
    <td>Indicates that the directory is not empty.</td>
  </tr>
  <tr>
    <td><code>ENOTSOCK</code></td>
    <td>Indicates that the given item is not a socket.</td>
  </tr>
  <tr>
    <td><code>ENOTSUP</code></td>
    <td>Indicates that a given operation is not supported.</td>
  </tr>
  <tr>
    <td><code>ENOTTY</code></td>
    <td>Indicates an inappropriate I/O control operation.</td>
  </tr>
  <tr>
    <td><code>ENXIO</code></td>
    <td>Indicates no such device or address.</td>
  </tr>
  <tr>
    <td><code>EOPNOTSUPP</code></td>
    <td>Indicates that an operation is not supported on the socket.
    Note that while `ENOTSUP` and `EOPNOTSUPP` have the same value on Linux,
    according to POSIX.1 these error values should be distinct.)</td>
  </tr>
  <tr>
    <td><code>EOVERFLOW</code></td>
    <td>Indicates that a value is too large to be stored in a given data
    type.</td>
  </tr>
  <tr>
    <td><code>EPERM</code></td>
    <td>Indicates that the operation is not permitted.</td>
  </tr>
  <tr>
    <td><code>EPIPE</code></td>
    <td>Indicates a broken pipe.</td>
  </tr>
  <tr>
    <td><code>EPROTO</code></td>
    <td>Indicates a protocol error.</td>
  </tr>
  <tr>
    <td><code>EPROTONOSUPPORT</code></td>
    <td>Indicates that a protocol is not supported.</td>
  </tr>
  <tr>
    <td><code>EPROTOTYPE</code></td>
    <td>Indicates the wrong type of protocol for a socket.</td>
  </tr>
  <tr>
    <td><code>ERANGE</code></td>
    <td>Indicates that the results are too large.</td>
  </tr>
  <tr>
    <td><code>EROFS</code></td>
    <td>Indicates that the file system is read only.</td>
  </tr>
  <tr>
    <td><code>ESPIPE</code></td>
    <td>Indicates an invalid seek operation.</td>
  </tr>
  <tr>
    <td><code>ESRCH</code></td>
    <td>Indicates that there is no such process.</td>
  </tr>
  <tr>
    <td><code>ESTALE</code></td>
    <td>Indicates that the file handle is stale.</td>
  </tr>
  <tr>
    <td><code>ETIME</code></td>
    <td>Indicates an expired timer.</td>
  </tr>
  <tr>
    <td><code>ETIMEDOUT</code></td>
    <td>Indicates that the connection timed out.</td>
  </tr>
  <tr>
    <td><code>ETXTBSY</code></td>
    <td>Indicates that a text file is busy.</td>
  </tr>
  <tr>
    <td><code>EWOULDBLOCK</code></td>
    <td>Indicates that the operation would block.</td>
  </tr>
  <tr>
    <td><code>EXDEV</code></td>
    <td>Indicates an improper link.
  </tr>
</table>

#### Windows Specific Error Constants

The following error codes are specific to the Windows operating system:

<table>
  <tr>
    <th>Constant</th>
    <th>Description</th>
  </tr>
  <tr>
    <td><code>WSAEINTR</code></td>
    <td>Indicates an interrupted function call.</td>
  </tr>
  <tr>
    <td><code>WSAEBADF</code></td>
    <td>Indicates an invalid file handle.</td>
  </tr>
  <tr>
    <td><code>WSAEACCES</code></td>
    <td>Indicates insufficient permissions to complete the operation.</td>
  </tr>
  <tr>
    <td><code>WSAEFAULT</code></td>
    <td>Indicates an invalid pointer address.</td>
  </tr>
  <tr>
    <td><code>WSAEINVAL</code></td>
    <td>Indicates that an invalid argument was passed.</td>
  </tr>
  <tr>
    <td><code>WSAEMFILE</code></td>
    <td>Indicates that there are too many open files.</td>
  </tr>
  <tr>
    <td><code>WSAEWOULDBLOCK</code></td>
    <td>Indicates that a resource is temporarily unavailable.</td>
  </tr>
  <tr>
    <td><code>WSAEINPROGRESS</code></td>
    <td>Indicates that an operation is currently in progress.</td>
  </tr>
  <tr>
    <td><code>WSAEALREADY</code></td>
    <td>Indicates that an operation is already in progress.</td>
  </tr>
  <tr>
    <td><code>WSAENOTSOCK</code></td>
    <td>Indicates that the resource is not a socket.</td>
  </tr>
  <tr>
    <td><code>WSAEDESTADDRREQ</code></td>
    <td>Indicates that a destination address is required.</td>
  </tr>
  <tr>
    <td><code>WSAEMSGSIZE</code></td>
    <td>Indicates that the message size is too long.</td>
  </tr>
  <tr>
    <td><code>WSAEPROTOTYPE</code></td>
    <td>Indicates the wrong protocol type for the socket.</td>
  </tr>
  <tr>
    <td><code>WSAENOPROTOOPT</code></td>
    <td>Indicates a bad protocol option.</td>
  </tr>
  <tr>
    <td><code>WSAEPROTONOSUPPORT</code></td>
    <td>Indicates that the protocol is not supported.</td>
  </tr>
  <tr>
    <td><code>WSAESOCKTNOSUPPORT</code></td>
    <td>Indicates that the socket type is not supported.</td>
  </tr>
  <tr>
    <td><code>WSAEOPNOTSUPP</code></td>
    <td>Indicates that the operation is not supported.</td>
  </tr>
  <tr>
    <td><code>WSAEPFNOSUPPORT</code></td>
    <td>Indicates that the protocol family is not supported.</td>
  </tr>
  <tr>
    <td><code>WSAEAFNOSUPPORT</code></td>
    <td>Indicates that the address family is not supported.</td>
  </tr>
  <tr>
    <td><code>WSAEADDRINUSE</code></td>
    <td>Indicates that the network address is already in use.</td>
  </tr>
  <tr>
    <td><code>WSAEADDRNOTAVAIL</code></td>
    <td>Indicates that the network address is not available.</td>
  </tr>
  <tr>
    <td><code>WSAENETDOWN</code></td>
    <td>Indicates that the network is down.</td>
  </tr>
  <tr>
    <td><code>WSAENETUNREACH</code></td>
    <td>Indicates that the network is unreachable.</td>
  </tr>
  <tr>
    <td><code>WSAENETRESET</code></td>
    <td>Indicates that the network connection has been reset.</td>
  </tr>
  <tr>
    <td><code>WSAECONNABORTED</code></td>
    <td>Indicates that the connection has been aborted.</td>
  </tr>
  <tr>
    <td><code>WSAECONNRESET</code></td>
    <td>Indicates that the connection has been reset by the peer.</td>
  </tr>
  <tr>
    <td><code>WSAENOBUFS</code></td>
    <td>Indicates that there is no buffer space available.</td>
  </tr>
  <tr>
    <td><code>WSAEISCONN</code></td>
    <td>Indicates that the socket is already connected.</td>
  </tr>
  <tr>
    <td><code>WSAENOTCONN</code></td>
    <td>Indicates that the socket is not connected.</td>
  </tr>
  <tr>
    <td><code>WSAESHUTDOWN</code></td>
    <td>Indicates that data cannot be sent after the socket has been
    shutdown.</td>
  </tr>
  <tr>
    <td><code>WSAETOOMANYREFS</code></td>
    <td>Indicates that there are too many references.</td>
  </tr>
  <tr>
    <td><code>WSAETIMEDOUT</code></td>
    <td>Indicates that the connection has timed out.</td>
  </tr>
  <tr>
    <td><code>WSAECONNREFUSED</code></td>
    <td>Indicates that the connection has been refused.</td>
  </tr>
  <tr>
    <td><code>WSAELOOP</code></td>
    <td>Indicates that a name cannot be translated.</td>
  </tr>
  <tr>
    <td><code>WSAENAMETOOLONG</code></td>
    <td>Indicates that a name was too long.</td>
  </tr>
  <tr>
    <td><code>WSAEHOSTDOWN</code></td>
    <td>Indicates that a network host is down.</td>
  </tr>
  <tr>
    <td><code>WSAEHOSTUNREACH</code></td>
    <td>Indicates that there is no route to a network host.</td>
  </tr>
  <tr>
    <td><code>WSAENOTEMPTY</code></td>
    <td>Indicates that the directory is not empty.</td>
  </tr>
  <tr>
    <td><code>WSAEPROCLIM</code></td>
    <td>Indicates that there are too many processes.</td>
  </tr>
  <tr>
    <td><code>WSAEUSERS</code></td>
    <td>Indicates that the user quota has been exceeded.</td>
  </tr>
  <tr>
    <td><code>WSAEDQUOT</code></td>
    <td>Indicates that the disk quota has been exceeded.</td>
  </tr>
  <tr>
    <td><code>WSAESTALE</code></td>
    <td>Indicates a stale file handle reference.</td>
  </tr>
  <tr>
    <td><code>WSAEREMOTE</code></td>
    <td>Indicates that the item is remote.</td>
  </tr>
  <tr>
    <td><code>WSASYSNOTREADY</code></td>
    <td>Indicates that the network subsystem is not ready.</td>
  </tr>
  <tr>
    <td><code>WSAVERNOTSUPPORTED</code></td>
    <td>Indicates that the winsock.dll version is out of range.</td>
  </tr>
  <tr>
    <td><code>WSANOTINITIALISED</code></td>
    <td>Indicates that successful WSAStartup has not yet been performed.</td>
  </tr>
  <tr>
    <td><code>WSAEDISCON</code></td>
    <td>Indicates that a graceful shutdown is in progress.</td>
  </tr>
  <tr>
    <td><code>WSAENOMORE</code></td>
    <td>Indicates that there are no more results.</td>
  </tr>
  <tr>
    <td><code>WSAECANCELLED</code></td>
    <td>Indicates that an operation has been canceled.</td>
  </tr>
  <tr>
    <td><code>WSAEINVALIDPROCTABLE</code></td>
    <td>Indicates that the procedure call table is invalid.</td>
  </tr>
  <tr>
    <td><code>WSAEINVALIDPROVIDER</code></td>
    <td>Indicates an invalid service provider.</td>
  </tr>
  <tr>
    <td><code>WSAEPROVIDERFAILEDINIT</code></td>
    <td>Indicates that the service provider failed to initialized.</td>
  </tr>
  <tr>
    <td><code>WSASYSCALLFAILURE</code></td>
    <td>Indicates a system call failure.</td>
  </tr>
  <tr>
    <td><code>WSASERVICE_NOT_FOUND</code></td>
    <td>Indicates that a service was not found.</td>
  </tr>
  <tr>
    <td><code>WSATYPE_NOT_FOUND</code></td>
    <td>Indicates that a class type was not found.</td>
  </tr>
  <tr>
    <td><code>WSA_E_NO_MORE</code></td>
    <td>Indicates that there are no more results.</td>
  </tr>
  <tr>
    <td><code>WSA_E_CANCELLED</code></td>
    <td>Indicates that the call was canceled.</td>
  </tr>
  <tr>
    <td><code>WSAEREFUSED</code></td>
    <td>Indicates that a database query was refused.</td>
  </tr>
</table>

### libuv Constants

<table>
  <tr>
    <th>Constant</th>
    <th>Description</th>
  </tr>
  <tr>
    <td><code>UV_UDP_REUSEADDR</code></td>
    <td></td>
  </tr>
</table>

[`process.arch`]: process.html#process_process_arch
[`process.platform`]: process.html#process_process_platform
[OS Constants]: #os_os_constants

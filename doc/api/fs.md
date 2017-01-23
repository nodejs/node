# File System

<!--introduced_in=v0.10.0-->

> Stability: 2 - Stable

<!--name=fs-->

File I/O is provided by simple wrappers around standard POSIX functions.  To
use this module do `require('fs')`. All the methods have asynchronous and
synchronous forms.

The asynchronous form always takes a completion callback as its last argument.
The arguments passed to the completion callback depend on the method, but the
first argument is always reserved for an exception. If the operation was
completed successfully, then the first argument will be `null` or `undefined`.

When using the synchronous form any exceptions are immediately thrown.
Exceptions may be handled using `try`/`catch`, or they may be allowed to
bubble up.

Here is an example of the asynchronous version:

```js
const fs = require('fs');

fs.unlink('/tmp/hello', (err) => {
  if (err) throw err;
  console.log('successfully deleted /tmp/hello');
});
```

Here is the synchronous version:

```js
const fs = require('fs');

fs.unlinkSync('/tmp/hello');
console.log('successfully deleted /tmp/hello');
```

With the asynchronous methods there is no guaranteed ordering. So the
following is prone to error:

```js
fs.rename('/tmp/hello', '/tmp/world', (err) => {
  if (err) throw err;
  console.log('renamed complete');
});
fs.stat('/tmp/world', (err, stats) => {
  if (err) throw err;
  console.log(`stats: ${JSON.stringify(stats)}`);
});
```

It could be that `fs.stat` is executed before `fs.rename`.
The correct way to do this is to chain the callbacks.

```js
fs.rename('/tmp/hello', '/tmp/world', (err) => {
  if (err) throw err;
  fs.stat('/tmp/world', (err, stats) => {
    if (err) throw err;
    console.log(`stats: ${JSON.stringify(stats)}`);
  });
});
```

In busy processes, the programmer is _strongly encouraged_ to use the
asynchronous versions of these calls. The synchronous versions will block
the entire process until they complete--halting all connections.

The relative path to a filename can be used. Remember, however, that this path
will be relative to `process.cwd()`.

While it is not recommended, most fs functions allow the callback argument to
be omitted, in which case a default callback is used that rethrows errors. To
get a trace to the original call site, set the `NODE_DEBUG` environment
variable:

*Note*: Omitting the callback function on asynchronous fs functions is
deprecated and may result in an error being thrown in the future.

```txt
$ cat script.js
function bad() {
  require('fs').readFile('/');
}
bad();

$ env NODE_DEBUG=fs node script.js
fs.js:88
        throw backtrace;
        ^
Error: EISDIR: illegal operation on a directory, read
    <stack trace.>
```

*Note:* On Windows Node.js follows the concept of per-drive working directory.
This behavior can be observed when using a drive path without a backslash. For
example `fs.readdirSync('c:\\')` can potentially return a different result than
`fs.readdirSync('c:')`. For more information, see
[this MSDN page][MSDN-Rel-Path].

## Threadpool Usage

Note that all file system APIs except `fs.FSWatcher()` and those that are
explicitly synchronous use libuv's threadpool, which can have surprising and
negative performance implications for some applications, see the
[`UV_THREADPOOL_SIZE`][] documentation for more information.

## WHATWG URL object support
<!-- YAML
added: v7.6.0
-->

> Stability: 1 - Experimental

For most `fs` module functions, the `path` or `filename` argument may be passed
as a WHATWG [`URL`][] object. Only [`URL`][] objects using the `file:` protocol
are supported.

```js
const fs = require('fs');
const { URL } = require('url');
const fileUrl = new URL('file:///tmp/hello');

fs.readFileSync(fileUrl);
```

*Note*: `file:` URLs are always absolute paths.

Using WHATWG [`URL`][] objects might introduce platform-specific behaviors.

On Windows, `file:` URLs with a hostname convert to UNC paths, while `file:`
URLs with drive letters convert to local absolute paths. `file:` URLs without a
hostname nor a drive letter will result in a throw :

```js
// On Windows :

// - WHATWG file URLs with hostname convert to UNC path
// file://hostname/p/a/t/h/file => \\hostname\p\a\t\h\file
fs.readFileSync(new URL('file://hostname/p/a/t/h/file'));

// - WHATWG file URLs with drive letters convert to absolute path
// file:///C:/tmp/hello => C:\tmp\hello
fs.readFileSync(new URL('file:///C:/tmp/hello'));

// - WHATWG file URLs without hostname must have a drive letters
fs.readFileSync(new URL('file:///notdriveletter/p/a/t/h/file'));
fs.readFileSync(new URL('file:///c/p/a/t/h/file'));
// TypeError [ERR_INVALID_FILE_URL_PATH]: File URL path must be absolute
```

*Note*: `file:` URLs with drive letters must use `:` as a separator just after
the drive letter. Using another separator will result in a throw.

On all other platforms, `file:` URLs with a hostname are unsupported and will
result in a throw:

```js
// On other platforms:

// - WHATWG file URLs with hostname are unsupported
// file://hostname/p/a/t/h/file => throw!
fs.readFileSync(new URL('file://hostname/p/a/t/h/file'));
// TypeError [ERR_INVALID_FILE_URL_PATH]: must be absolute

// - WHATWG file URLs convert to absolute path
// file:///tmp/hello => /tmp/hello
fs.readFileSync(new URL('file:///tmp/hello'));
```

A `file:` URL having encoded slash characters will result in a throw on all
platforms:

```js
// On Windows
fs.readFileSync(new URL('file:///C:/p/a/t/h/%2F'));
fs.readFileSync(new URL('file:///C:/p/a/t/h/%2f'));
/* TypeError [ERR_INVALID_FILE_URL_PATH]: File URL path must not include encoded
\ or / characters */

// On POSIX
fs.readFileSync(new URL('file:///p/a/t/h/%2F'));
fs.readFileSync(new URL('file:///p/a/t/h/%2f'));
/* TypeError [ERR_INVALID_FILE_URL_PATH]: File URL path must not include encoded
/ characters */
```
On Windows, `file:` URLs having encoded backslash will result in a throw:

```js
// On Windows
fs.readFileSync(new URL('file:///C:/path/%5C'));
fs.readFileSync(new URL('file:///C:/path/%5c'));
/* TypeError [ERR_INVALID_FILE_URL_PATH]: File URL path must not include encoded
\ or / characters */
```

## Buffer API
<!-- YAML
added: v6.0.0
-->

`fs` functions support passing and receiving paths as both strings
and Buffers. The latter is intended to make it possible to work with
filesystems that allow for non-UTF-8 filenames. For most typical
uses, working with paths as Buffers will be unnecessary, as the string
API converts to and from UTF-8 automatically.

*Note*: On certain file systems (such as NTFS and HFS+) filenames
will always be encoded as UTF-8. On such file systems, passing
non-UTF-8 encoded Buffers to `fs` functions will not work as expected.

## Class: fs.FSWatcher
<!-- YAML
added: v0.5.8
-->

Objects returned from [`fs.watch()`][] are of this type.

The `listener` callback provided to `fs.watch()` receives the returned FSWatcher's
`change` events.

The object itself emits these events:

### Event: 'change'
<!-- YAML
added: v0.5.8
-->

* `eventType` {string} The type of fs change
* `filename` {string|Buffer} The filename that changed (if relevant/available)

Emitted when something changes in a watched directory or file.
See more details in [`fs.watch()`][].

The `filename` argument may not be provided depending on operating system
support. If `filename` is provided, it will be provided as a `Buffer` if
`fs.watch()` is called with its `encoding` option set to `'buffer'`, otherwise
`filename` will be a string.

```js
// Example when handled through fs.watch listener
fs.watch('./tmp', { encoding: 'buffer' }, (eventType, filename) => {
  if (filename) {
    console.log(filename);
    // Prints: <Buffer ...>
  }
});
```

### Event: 'error'
<!-- YAML
added: v0.5.8
-->

* `error` {Error}

Emitted when an error occurs.

### watcher.close()
<!-- YAML
added: v0.5.8
-->

Stop watching for changes on the given `fs.FSWatcher`.

## Class: fs.ReadStream
<!-- YAML
added: v0.1.93
-->

`ReadStream` is a [Readable Stream][].

### Event: 'close'
<!-- YAML
added: v0.1.93
-->

Emitted when the `ReadStream`'s underlying file descriptor has been closed
using the `fs.close()` method.

### Event: 'open'
<!-- YAML
added: v0.1.93
-->

* `fd` {integer} Integer file descriptor used by the ReadStream.

Emitted when the ReadStream's file is opened.

### readStream.bytesRead
<!-- YAML
added: 6.4.0
-->

The number of bytes read so far.

### readStream.path
<!-- YAML
added: v0.1.93
-->

The path to the file the stream is reading from as specified in the first
argument to `fs.createReadStream()`. If `path` is passed as a string, then
`readStream.path` will be a string. If `path` is passed as a `Buffer`, then
`readStream.path` will be a `Buffer`.

## Class: fs.Stats
<!-- YAML
added: v0.1.21
changes:
  - version: REPLACEME
    pr-url: https://github.com/nodejs/node/pull/13173
    description: Added times as numbers.
-->

Objects returned from [`fs.stat()`][], [`fs.lstat()`][] and [`fs.fstat()`][] and
their synchronous counterparts are of this type.

 - `stats.isFile()`
 - `stats.isDirectory()`
 - `stats.isBlockDevice()`
 - `stats.isCharacterDevice()`
 - `stats.isSymbolicLink()` (only valid with [`fs.lstat()`][])
 - `stats.isFIFO()`
 - `stats.isSocket()`

For a regular file [`util.inspect(stats)`][] would return a string very
similar to this:

```console
Stats {
  dev: 2114,
  ino: 48064969,
  mode: 33188,
  nlink: 1,
  uid: 85,
  gid: 100,
  rdev: 0,
  size: 527,
  blksize: 4096,
  blocks: 8,
  atimeMs: 1318289051000.1,
  mtimeMs: 1318289051000.1,
  ctimeMs: 1318289051000.1,
  birthtimeMs: 1318289051000.1,
  atime: Mon, 10 Oct 2011 23:24:11 GMT,
  mtime: Mon, 10 Oct 2011 23:24:11 GMT,
  ctime: Mon, 10 Oct 2011 23:24:11 GMT,
  birthtime: Mon, 10 Oct 2011 23:24:11 GMT }
```

*Note*: `atimeMs`, `mtimeMs`, `ctimeMs`, `birthtimeMs` are [numbers][MDN-Number]
that hold the corresponding times in milliseconds. Their precision is platform
specific. `atime`, `mtime`, `ctime`, and `birthtime` are [`Date`][MDN-Date]
object alternate representations of the various times. The `Date` and number
values are not connected. Assigning a new number value, or mutating the `Date`
value, will not be reflected in the corresponding alternate representation.


### Stat Time Values

The times in the stat object have the following semantics:

* `atime` "Access Time" - Time when file data last accessed.  Changed
  by the mknod(2), utimes(2), and read(2) system calls.
* `mtime` "Modified Time" - Time when file data last modified.
  Changed by the mknod(2), utimes(2), and write(2) system calls.
* `ctime` "Change Time" - Time when file status was last changed
  (inode data modification).  Changed by the chmod(2), chown(2),
  link(2), mknod(2), rename(2), unlink(2), utimes(2),
  read(2), and write(2) system calls.
* `birthtime` "Birth Time" -  Time of file creation. Set once when the
  file is created.  On filesystems where birthtime is not available,
  this field may instead hold either the `ctime` or
  `1970-01-01T00:00Z` (ie, unix epoch timestamp `0`). Note that this
  value may be greater than `atime` or `mtime` in this case. On Darwin
  and other FreeBSD variants, also set if the `atime` is explicitly
  set to an earlier value than the current `birthtime` using the
  utimes(2) system call.

Prior to Node v0.12, the `ctime` held the `birthtime` on Windows
systems.  Note that as of v0.12, `ctime` is not "creation time", and
on Unix systems, it never was.

## Class: fs.WriteStream
<!-- YAML
added: v0.1.93
-->

`WriteStream` is a [Writable Stream][].

### Event: 'close'
<!-- YAML
added: v0.1.93
-->

Emitted when the `WriteStream`'s underlying file descriptor has been closed
using the `fs.close()` method.

### Event: 'open'
<!-- YAML
added: v0.1.93
-->

* `fd` {integer} Integer file descriptor used by the WriteStream.

Emitted when the WriteStream's file is opened.

### writeStream.bytesWritten
<!-- YAML
added: v0.4.7
-->

The number of bytes written so far. Does not include data that is still queued
for writing.

### writeStream.path
<!-- YAML
added: v0.1.93
-->

The path to the file the stream is writing to as specified in the first
argument to `fs.createWriteStream()`. If `path` is passed as a string, then
`writeStream.path` will be a string. If `path` is passed as a `Buffer`, then
`writeStream.path` will be a `Buffer`.

## fs.access(path[, mode], callback)
<!-- YAML
added: v0.11.15
changes:
  - version: v7.6.0
    pr-url: https://github.com/nodejs/node/pull/10739
    description: The `path` parameter can be a WHATWG `URL` object using `file:`
                 protocol. Support is currently still *experimental*.
  - version: v6.3.0
    pr-url: https://github.com/nodejs/node/pull/6534
    description: The constants like `fs.R_OK`, etc which were present directly
                 on `fs` were moved into `fs.constants` as a soft deprecation.
                 Thus for Node `< v6.3.0` use `fs` to access those constants, or
                 do something like `(fs.constants || fs).R_OK` to work with all
                 versions.
-->

* `path` {string|Buffer|URL}
* `mode` {integer} **Default:** `fs.constants.F_OK`
* `callback` {Function}

Tests a user's permissions for the file or directory specified by `path`.
The `mode` argument is an optional integer that specifies the accessibility
checks to be performed. The following constants define the possible values of
`mode`. It is possible to create a mask consisting of the bitwise OR of two or
more values.

- `fs.constants.F_OK` - `path` is visible to the calling process. This is useful
for determining if a file exists, but says nothing about `rwx` permissions.
Default if no `mode` is specified.
- `fs.constants.R_OK` - `path` can be read by the calling process.
- `fs.constants.W_OK` - `path` can be written by the calling process.
- `fs.constants.X_OK` - `path` can be executed by the calling process. This has
no effect on Windows (will behave like `fs.constants.F_OK`).

The final argument, `callback`, is a callback function that is invoked with
a possible error argument. If any of the accessibility checks fail, the error
argument will be populated. The following example checks if the file
`/etc/passwd` can be read and written by the current process.

```js
fs.access('/etc/passwd', fs.constants.R_OK | fs.constants.W_OK, (err) => {
  console.log(err ? 'no access!' : 'can read/write');
});
```

Using `fs.access()` to check for the accessibility of a file before calling
`fs.open()`, `fs.readFile()` or `fs.writeFile()` is not recommended. Doing
so introduces a race condition, since other processes may change the file's
state between the two calls. Instead, user code should open/read/write the
file directly and handle the error raised if the file is not accessible.

For example:


**write (NOT RECOMMENDED)**

```js
fs.access('myfile', (err) => {
  if (!err) {
    console.error('myfile already exists');
    return;
  }

  fs.open('myfile', 'wx', (err, fd) => {
    if (err) throw err;
    writeMyData(fd);
  });
});
```

**write (RECOMMENDED)**

```js
fs.open('myfile', 'wx', (err, fd) => {
  if (err) {
    if (err.code === 'EEXIST') {
      console.error('myfile already exists');
      return;
    }

    throw err;
  }

  writeMyData(fd);
});
```

**read (NOT RECOMMENDED)**

```js
fs.access('myfile', (err) => {
  if (err) {
    if (err.code === 'ENOENT') {
      console.error('myfile does not exist');
      return;
    }

    throw err;
  }

  fs.open('myfile', 'r', (err, fd) => {
    if (err) throw err;
    readMyData(fd);
  });
});
```

**read (RECOMMENDED)**

```js
fs.open('myfile', 'r', (err, fd) => {
  if (err) {
    if (err.code === 'ENOENT') {
      console.error('myfile does not exist');
      return;
    }

    throw err;
  }

  readMyData(fd);
});
```

The "not recommended" examples above check for accessibility and then use the
file; the "recommended" examples are better because they use the file directly
and handle the error, if any.

In general, check for the accessibility of a file only if the file won’t be
used directly, for example when its accessibility is a signal from another
process.

## fs.accessSync(path[, mode])
<!-- YAML
added: v0.11.15
changes:
  - version: v7.6.0
    pr-url: https://github.com/nodejs/node/pull/10739
    description: The `path` parameter can be a WHATWG `URL` object using `file:`
                 protocol. Support is currently still *experimental*.
-->

* `path` {string|Buffer|URL}
* `mode` {integer} **Default:** `fs.constants.F_OK`

Synchronous version of [`fs.access()`][]. This throws if any accessibility
checks fail, and does nothing otherwise.

## fs.appendFile(file, data[, options], callback)
<!-- YAML
added: v0.6.7
changes:
  - version: v7.0.0
    pr-url: https://github.com/nodejs/node/pull/7897
    description: The `callback` parameter is no longer optional. Not passing
                 it will emit a deprecation warning.
  - version: v7.0.0
    pr-url: https://github.com/nodejs/node/pull/7831
    description: The passed `options` object will never be modified.
  - version: v5.0.0
    pr-url: https://github.com/nodejs/node/pull/3163
    description: The `file` parameter can be a file descriptor now.
-->

* `file` {string|Buffer|number} filename or file descriptor
* `data` {string|Buffer}
* `options` {Object|string}
  * `encoding` {string|null} **Default:** `'utf8'`
  * `mode` {integer} **Default:** `0o666`
  * `flag` {string} **Default:** `'a'`
* `callback` {Function}

Asynchronously append data to a file, creating the file if it does not yet exist.
`data` can be a string or a buffer.

Example:

```js
fs.appendFile('message.txt', 'data to append', (err) => {
  if (err) throw err;
  console.log('The "data to append" was appended to file!');
});
```

If `options` is a string, then it specifies the encoding. Example:

```js
fs.appendFile('message.txt', 'data to append', 'utf8', callback);
```

Any specified file descriptor has to have been opened for appending.

*Note*: If a file descriptor is specified as the `file`, it will not be closed
automatically.

## fs.appendFileSync(file, data[, options])
<!-- YAML
added: v0.6.7
changes:
  - version: v7.0.0
    pr-url: https://github.com/nodejs/node/pull/7831
    description: The passed `options` object will never be modified.
  - version: v5.0.0
    pr-url: https://github.com/nodejs/node/pull/3163
    description: The `file` parameter can be a file descriptor now.
-->

* `file` {string|Buffer|number} filename or file descriptor
* `data` {string|Buffer}
* `options` {Object|string}
  * `encoding` {string|null} **Default:** `'utf8'`
  * `mode` {integer} **Default:** `0o666`
  * `flag` {string} **Default:** `'a'`

The synchronous version of [`fs.appendFile()`][]. Returns `undefined`.

## fs.chmod(path, mode, callback)
<!-- YAML
added: v0.1.30
changes:
  - version: v7.6.0
    pr-url: https://github.com/nodejs/node/pull/10739
    description: The `path` parameter can be a WHATWG `URL` object using `file:`
                 protocol. Support is currently still *experimental*.
  - version: v7.0.0
    pr-url: https://github.com/nodejs/node/pull/7897
    description: The `callback` parameter is no longer optional. Not passing
                 it will emit a deprecation warning.
-->

* `path` {string|Buffer|URL}
* `mode` {integer}
* `callback` {Function}

Asynchronous chmod(2). No arguments other than a possible exception are given
to the completion callback.

## fs.chmodSync(path, mode)
<!-- YAML
added: v0.6.7
changes:
  - version: v7.6.0
    pr-url: https://github.com/nodejs/node/pull/10739
    description: The `path` parameter can be a WHATWG `URL` object using `file:`
                 protocol. Support is currently still *experimental*.
-->

* `path` {string|Buffer|URL}
* `mode` {integer}

Synchronous chmod(2). Returns `undefined`.

## fs.chown(path, uid, gid, callback)
<!-- YAML
added: v0.1.97
changes:
  - version: v7.6.0
    pr-url: https://github.com/nodejs/node/pull/10739
    description: The `path` parameter can be a WHATWG `URL` object using `file:`
                 protocol. Support is currently still *experimental*.
  - version: v7.0.0
    pr-url: https://github.com/nodejs/node/pull/7897
    description: The `callback` parameter is no longer optional. Not passing
                 it will emit a deprecation warning.
-->

* `path` {string|Buffer|URL}
* `uid` {integer}
* `gid` {integer}
* `callback` {Function}

Asynchronous chown(2). No arguments other than a possible exception are given
to the completion callback.

## fs.chownSync(path, uid, gid)
<!-- YAML
added: v0.1.97
changes:
  - version: v7.6.0
    pr-url: https://github.com/nodejs/node/pull/10739
    description: The `path` parameter can be a WHATWG `URL` object using `file:`
                 protocol. Support is currently still *experimental*.
-->

* `path` {string|Buffer|URL}
* `uid` {integer}
* `gid` {integer}

Synchronous chown(2). Returns `undefined`.

## fs.close(fd, callback)
<!-- YAML
added: v0.0.2
changes:
  - version: v7.0.0
    pr-url: https://github.com/nodejs/node/pull/7897
    description: The `callback` parameter is no longer optional. Not passing
                 it will emit a deprecation warning.
-->

* `fd` {integer}
* `callback` {Function}

Asynchronous close(2).  No arguments other than a possible exception are given
to the completion callback.

## fs.closeSync(fd)
<!-- YAML
added: v0.1.21
-->

* `fd` {integer}

Synchronous close(2). Returns `undefined`.

## fs.constants

Returns an object containing commonly used constants for file system
operations. The specific constants currently defined are described in
[FS Constants][].

## fs.createReadStream(path[, options])
<!-- YAML
added: v0.1.31
changes:
  - version: v7.6.0
    pr-url: https://github.com/nodejs/node/pull/10739
    description: The `path` parameter can be a WHATWG `URL` object using
                 `file:` protocol. Support is currently still *experimental*.
  - version: v7.0.0
    pr-url: https://github.com/nodejs/node/pull/7831
    description: The passed `options` object will never be modified.
  - version: v2.3.0
    pr-url: https://github.com/nodejs/node/pull/1845
    description: The passed `options` object can be a string now.
-->

* `path` {string|Buffer|URL}
* `options` {string|Object}
  * `flags` {string}
  * `encoding` {string}
  * `fd` {integer}
  * `mode` {integer}
  * `autoClose` {boolean}
  * `start` {integer}
  * `end` {integer}

Returns a new [`ReadStream`][] object. (See [Readable Stream][]).

Be aware that, unlike the default value set for `highWaterMark` on a
readable stream (16 kb), the stream returned by this method has a
default value of 64 kb for the same parameter.

`options` is an object or string with the following defaults:

```js
const defaults = {
  flags: 'r',
  encoding: null,
  fd: null,
  mode: 0o666,
  autoClose: true
};
```

`options` can include `start` and `end` values to read a range of bytes from
the file instead of the entire file.  Both `start` and `end` are inclusive and
start counting at 0. If `fd` is specified and `start` is omitted or `undefined`,
`fs.createReadStream()` reads sequentially from the current file position.
The `encoding` can be any one of those accepted by [`Buffer`][].

If `fd` is specified, `ReadStream` will ignore the `path` argument and will use
the specified file descriptor. This means that no `'open'` event will be
emitted. Note that `fd` should be blocking; non-blocking `fd`s should be passed
to [`net.Socket`][].

If `autoClose` is false, then the file descriptor won't be closed, even if
there's an error. It is the application's responsibility to close it and make
sure there's no file descriptor leak. If `autoClose` is set to true (default
behavior), on `error` or `end` the file descriptor will be closed
automatically.

`mode` sets the file mode (permission and sticky bits), but only if the
file was created.

An example to read the last 10 bytes of a file which is 100 bytes long:

```js
fs.createReadStream('sample.txt', { start: 90, end: 99 });
```

If `options` is a string, then it specifies the encoding.

## fs.createWriteStream(path[, options])
<!-- YAML
added: v0.1.31
changes:
  - version: v7.6.0
    pr-url: https://github.com/nodejs/node/pull/10739
    description: The `path` parameter can be a WHATWG `URL` object using
                 `file:` protocol. Support is currently still *experimental*.
  - version: v7.0.0
    pr-url: https://github.com/nodejs/node/pull/7831
    description: The passed `options` object will never be modified.
  - version: v5.5.0
    pr-url: https://github.com/nodejs/node/pull/3679
    description: The `autoClose` option is supported now.
  - version: v2.3.0
    pr-url: https://github.com/nodejs/node/pull/1845
    description: The passed `options` object can be a string now.
-->

* `path` {string|Buffer|URL}
* `options` {string|Object}
  * `flags` {string}
  * `encoding` {string}
  * `fd` {integer}
  * `mode` {integer}
  * `autoClose` {boolean}
  * `start` {integer}

Returns a new [`WriteStream`][] object. (See [Writable Stream][]).

`options` is an object or string with the following defaults:

```js
const defaults = {
  flags: 'w',
  encoding: 'utf8',
  fd: null,
  mode: 0o666,
  autoClose: true
};
```

`options` may also include a `start` option to allow writing data at
some position past the beginning of the file.  Modifying a file rather
than replacing it may require a `flags` mode of `r+` rather than the
default mode `w`. The `encoding` can be any one of those accepted by
[`Buffer`][].

If `autoClose` is set to true (default behavior) on `error` or `end`
the file descriptor will be closed automatically. If `autoClose` is false,
then the file descriptor won't be closed, even if there's an error.
It is the application's responsibility to close it and make sure there's no
file descriptor leak.

Like [`ReadStream`][], if `fd` is specified, `WriteStream` will ignore the
`path` argument and will use the specified file descriptor. This means that no
`'open'` event will be emitted. Note that `fd` should be blocking; non-blocking
`fd`s should be passed to [`net.Socket`][].

If `options` is a string, then it specifies the encoding.

## fs.exists(path, callback)
<!-- YAML
added: v0.0.2
changes:
  - version: v7.6.0
    pr-url: https://github.com/nodejs/node/pull/10739
    description: The `path` parameter can be a WHATWG `URL` object using
                 `file:` protocol. Support is currently still *experimental*.
deprecated: v1.0.0
-->

> Stability: 0 - Deprecated: Use [`fs.stat()`][] or [`fs.access()`][] instead.

* `path` {string|Buffer|URL}
* `callback` {Function}

Test whether or not the given path exists by checking with the file system.
Then call the `callback` argument with either true or false.  Example:

```js
fs.exists('/etc/passwd', (exists) => {
  console.log(exists ? 'it\'s there' : 'no passwd!');
});
```

**Note that the parameter to this callback is not consistent with other
Node.js callbacks.** Normally, the first parameter to a Node.js callback is
an `err` parameter, optionally followed by other parameters. The
`fs.exists()` callback has only one boolean parameter. This is one reason
`fs.access()` is recommended instead of `fs.exists()`.

Using `fs.exists()` to check for the existence of a file before calling
`fs.open()`, `fs.readFile()` or `fs.writeFile()` is not recommended. Doing
so introduces a race condition, since other processes may change the file's
state between the two calls. Instead, user code should open/read/write the
file directly and handle the error raised if the file does not exist.

For example:

**write (NOT RECOMMENDED)**

```js
fs.exists('myfile', (exists) => {
  if (exists) {
    console.error('myfile already exists');
  } else {
    fs.open('myfile', 'wx', (err, fd) => {
      if (err) throw err;
      writeMyData(fd);
    });
  }
});
```

**write (RECOMMENDED)**

```js
fs.open('myfile', 'wx', (err, fd) => {
  if (err) {
    if (err.code === 'EEXIST') {
      console.error('myfile already exists');
      return;
    }

    throw err;
  }

  writeMyData(fd);
});
```

**read (NOT RECOMMENDED)**

```js
fs.exists('myfile', (exists) => {
  if (exists) {
    fs.open('myfile', 'r', (err, fd) => {
      readMyData(fd);
    });
  } else {
    console.error('myfile does not exist');
  }
});
```

**read (RECOMMENDED)**

```js
fs.open('myfile', 'r', (err, fd) => {
  if (err) {
    if (err.code === 'ENOENT') {
      console.error('myfile does not exist');
      return;
    }

    throw err;
  }

  readMyData(fd);
});
```

The "not recommended" examples above check for existence and then use the
file; the "recommended" examples are better because they use the file directly
and handle the error, if any.

In general, check for the existence of a file only if the file won’t be
used directly, for example when its existence is a signal from another
process.

## fs.existsSync(path)
<!-- YAML
added: v0.1.21
changes:
  - version: v7.6.0
    pr-url: https://github.com/nodejs/node/pull/10739
    description: The `path` parameter can be a WHATWG `URL` object using
                 `file:` protocol. Support is currently still *experimental*.
-->

* `path` {string|Buffer|URL}

Synchronous version of [`fs.exists()`][].
Returns `true` if the file exists, `false` otherwise.

Note that `fs.exists()` is deprecated, but `fs.existsSync()` is not.
(The `callback` parameter to `fs.exists()` accepts parameters that are
inconsistent with other Node.js callbacks. `fs.existsSync()` does not use
a callback.)

## fs.fchmod(fd, mode, callback)
<!-- YAML
added: v0.4.7
changes:
  - version: v7.0.0
    pr-url: https://github.com/nodejs/node/pull/7897
    description: The `callback` parameter is no longer optional. Not passing
                 it will emit a deprecation warning.
-->

* `fd` {integer}
* `mode` {integer}
* `callback` {Function}

Asynchronous fchmod(2). No arguments other than a possible exception
are given to the completion callback.

## fs.fchmodSync(fd, mode)
<!-- YAML
added: v0.4.7
-->

* `fd` {integer}
* `mode` {integer}

Synchronous fchmod(2). Returns `undefined`.

## fs.fchown(fd, uid, gid, callback)
<!-- YAML
added: v0.4.7
changes:
  - version: v7.0.0
    pr-url: https://github.com/nodejs/node/pull/7897
    description: The `callback` parameter is no longer optional. Not passing
                 it will emit a deprecation warning.
-->

* `fd` {integer}
* `uid` {integer}
* `gid` {integer}
* `callback` {Function}

Asynchronous fchown(2). No arguments other than a possible exception are given
to the completion callback.

## fs.fchownSync(fd, uid, gid)
<!-- YAML
added: v0.4.7
-->

* `fd` {integer}
* `uid` {integer}
* `gid` {integer}

Synchronous fchown(2). Returns `undefined`.

## fs.fdatasync(fd, callback)
<!-- YAML
added: v0.1.96
changes:
  - version: v7.0.0
    pr-url: https://github.com/nodejs/node/pull/7897
    description: The `callback` parameter is no longer optional. Not passing
                 it will emit a deprecation warning.
-->

* `fd` {integer}
* `callback` {Function}

Asynchronous fdatasync(2). No arguments other than a possible exception are
given to the completion callback.

## fs.fdatasyncSync(fd)
<!-- YAML
added: v0.1.96
-->

* `fd` {integer}

Synchronous fdatasync(2). Returns `undefined`.

## fs.fstat(fd, callback)
<!-- YAML
added: v0.1.95
changes:
  - version: v7.0.0
    pr-url: https://github.com/nodejs/node/pull/7897
    description: The `callback` parameter is no longer optional. Not passing
                 it will emit a deprecation warning.
-->

* `fd` {integer}
* `callback` {Function}

Asynchronous fstat(2). The callback gets two arguments `(err, stats)` where
`stats` is an [`fs.Stats`][] object. `fstat()` is identical to [`stat()`][],
except that the file to be stat-ed is specified by the file descriptor `fd`.

## fs.fstatSync(fd)
<!-- YAML
added: v0.1.95
-->

* `fd` {integer}

Synchronous fstat(2). Returns an instance of [`fs.Stats`][].

## fs.fsync(fd, callback)
<!-- YAML
added: v0.1.96
changes:
  - version: v7.0.0
    pr-url: https://github.com/nodejs/node/pull/7897
    description: The `callback` parameter is no longer optional. Not passing
                 it will emit a deprecation warning.
-->

* `fd` {integer}
* `callback` {Function}

Asynchronous fsync(2). No arguments other than a possible exception are given
to the completion callback.

## fs.fsyncSync(fd)
<!-- YAML
added: v0.1.96
-->

* `fd` {integer}

Synchronous fsync(2). Returns `undefined`.

## fs.ftruncate(fd[, len], callback)
<!-- YAML
added: v0.8.6
changes:
  - version: v7.0.0
    pr-url: https://github.com/nodejs/node/pull/7897
    description: The `callback` parameter is no longer optional. Not passing
                 it will emit a deprecation warning.
-->

* `fd` {integer}
* `len` {integer} **Default:** `0`
* `callback` {Function}

Asynchronous ftruncate(2). No arguments other than a possible exception are
given to the completion callback.

If the file referred to by the file descriptor was larger than `len` bytes, only
the first `len` bytes will be retained in the file.

For example, the following program retains only the first four bytes of the file

```js
console.log(fs.readFileSync('temp.txt', 'utf8'));
// Prints: Node.js

// get the file descriptor of the file to be truncated
const fd = fs.openSync('temp.txt', 'r+');

// truncate the file to first four bytes
fs.ftruncate(fd, 4, (err) => {
  assert.ifError(err);
  console.log(fs.readFileSync('temp.txt', 'utf8'));
});
// Prints: Node
```

If the file previously was shorter than `len` bytes, it is extended, and the
extended part is filled with null bytes ('\0'). For example,

```js
console.log(fs.readFileSync('temp.txt', 'utf-8'));
// Prints: Node.js

// get the file descriptor of the file to be truncated
const fd = fs.openSync('temp.txt', 'r+');

// truncate the file to 10 bytes, whereas the actual size is 7 bytes
fs.ftruncate(fd, 10, (err) => {
  assert.ifError(err);
  console.log(fs.readFileSync('temp.txt'));
});
// Prints: <Buffer 4e 6f 64 65 2e 6a 73 00 00 00>
// ('Node.js\0\0\0' in UTF8)
```

The last three bytes are null bytes ('\0'), to compensate the over-truncation.

## fs.ftruncateSync(fd[, len])
<!-- YAML
added: v0.8.6
-->

* `fd` {integer}
* `len` {integer} **Default:** `0`

Synchronous ftruncate(2). Returns `undefined`.

## fs.futimes(fd, atime, mtime, callback)
<!-- YAML
added: v0.4.2
changes:
  - version: v7.0.0
    pr-url: https://github.com/nodejs/node/pull/7897
    description: The `callback` parameter is no longer optional. Not passing
                 it will emit a deprecation warning.
  - version: v4.1.0
    pr-url: https://github.com/nodejs/node/pull/2387
    description: Numeric strings, `NaN` and `Infinity` are now allowed
                 time specifiers.
-->

* `fd` {integer}
* `atime` {integer}
* `mtime` {integer}
* `callback` {Function}

Change the file timestamps of a file referenced by the supplied file
descriptor.

*Note*: This function does not work on AIX versions before 7.1, it will return
the error `UV_ENOSYS`.

## fs.futimesSync(fd, atime, mtime)
<!-- YAML
added: v0.4.2
changes:
  - version: v4.1.0
    pr-url: https://github.com/nodejs/node/pull/2387
    description: Numeric strings, `NaN` and `Infinity` are now allowed
                 time specifiers.
-->

* `fd` {integer}
* `atime` {integer}
* `mtime` {integer}

Synchronous version of [`fs.futimes()`][]. Returns `undefined`.

## fs.lchmod(path, mode, callback)
<!-- YAML
deprecated: v0.4.7
changes:
  - version: v7.0.0
    pr-url: https://github.com/nodejs/node/pull/7897
    description: The `callback` parameter is no longer optional. Not passing
                 it will emit a deprecation warning.
-->

* `path` {string|Buffer}
* `mode` {integer}
* `callback` {Function}

Asynchronous lchmod(2). No arguments other than a possible exception
are given to the completion callback.

Only available on macOS.

## fs.lchmodSync(path, mode)
<!-- YAML
deprecated: v0.4.7
-->

* `path` {string|Buffer}
* `mode` {integer}

Synchronous lchmod(2). Returns `undefined`.

## fs.lchown(path, uid, gid, callback)
<!-- YAML
deprecated: v0.4.7
changes:
  - version: v7.0.0
    pr-url: https://github.com/nodejs/node/pull/7897
    description: The `callback` parameter is no longer optional. Not passing
                 it will emit a deprecation warning.
-->

* `path` {string|Buffer}
* `uid` {integer}
* `gid` {integer}
* `callback` {Function}

Asynchronous lchown(2). No arguments other than a possible exception are given
to the completion callback.

## fs.lchownSync(path, uid, gid)
<!-- YAML
deprecated: v0.4.7
-->

* `path` {string|Buffer}
* `uid` {integer}
* `gid` {integer}

Synchronous lchown(2). Returns `undefined`.

## fs.link(existingPath, newPath, callback)
<!-- YAML
added: v0.1.31
changes:
  - version: v7.6.0
    pr-url: https://github.com/nodejs/node/pull/10739
    description: The `existingPath` and `newPath` parameters can be WHATWG
                 `URL` objects using `file:` protocol. Support is currently
                 still *experimental*.
  - version: v7.0.0
    pr-url: https://github.com/nodejs/node/pull/7897
    description: The `callback` parameter is no longer optional. Not passing
                 it will emit a deprecation warning.
-->

* `existingPath` {string|Buffer|URL}
* `newPath` {string|Buffer|URL}
* `callback` {Function}

Asynchronous link(2). No arguments other than a possible exception are given to
the completion callback.

## fs.linkSync(existingPath, newPath)
<!-- YAML
added: v0.1.31
changes:
  - version: v7.6.0
    pr-url: https://github.com/nodejs/node/pull/10739
    description: The `existingPath` and `newPath` parameters can be WHATWG
                 `URL` objects using `file:` protocol. Support is currently
                 still *experimental*.
-->

* `existingPath` {string|Buffer|URL}
* `newPath` {string|Buffer|URL}

Synchronous link(2). Returns `undefined`.

## fs.lstat(path, callback)
<!-- YAML
added: v0.1.30
changes:
  - version: v7.6.0
    pr-url: https://github.com/nodejs/node/pull/10739
    description: The `path` parameter can be a WHATWG `URL` object using `file:`
                 protocol. Support is currently still *experimental*.
  - version: v7.0.0
    pr-url: https://github.com/nodejs/node/pull/7897
    description: The `callback` parameter is no longer optional. Not passing
                 it will emit a deprecation warning.
-->

* `path` {string|Buffer|URL}
* `callback` {Function}

Asynchronous lstat(2). The callback gets two arguments `(err, stats)` where
`stats` is a [`fs.Stats`][] object. `lstat()` is identical to `stat()`,
except that if `path` is a symbolic link, then the link itself is stat-ed,
not the file that it refers to.

## fs.lstatSync(path)
<!-- YAML
added: v0.1.30
changes:
  - version: v7.6.0
    pr-url: https://github.com/nodejs/node/pull/10739
    description: The `path` parameter can be a WHATWG `URL` object using `file:`
                 protocol. Support is currently still *experimental*.
-->

* `path` {string|Buffer|URL}

Synchronous lstat(2). Returns an instance of [`fs.Stats`][].

## fs.mkdir(path[, mode], callback)
<!-- YAML
added: v0.1.8
changes:
  - version: v7.6.0
    pr-url: https://github.com/nodejs/node/pull/10739
    description: The `path` parameter can be a WHATWG `URL` object using `file:`
                 protocol. Support is currently still *experimental*.
  - version: v7.0.0
    pr-url: https://github.com/nodejs/node/pull/7897
    description: The `callback` parameter is no longer optional. Not passing
                 it will emit a deprecation warning.
-->

* `path` {string|Buffer|URL}
* `mode` {integer} **Default:** `0o777`
* `callback` {Function}

Asynchronous mkdir(2). No arguments other than a possible exception are given
to the completion callback. `mode` defaults to `0o777`.

## fs.mkdirSync(path[, mode])
<!-- YAML
added: v0.1.21
changes:
  - version: v7.6.0
    pr-url: https://github.com/nodejs/node/pull/10739
    description: The `path` parameter can be a WHATWG `URL` object using `file:`
                 protocol. Support is currently still *experimental*.
-->

* `path` {string|Buffer|URL}
* `mode` {integer} **Default:** `0o777`

Synchronous mkdir(2). Returns `undefined`.

## fs.mkdtemp(prefix[, options], callback)
<!-- YAML
added: v5.10.0
changes:
  - version: v7.0.0
    pr-url: https://github.com/nodejs/node/pull/7897
    description: The `callback` parameter is no longer optional. Not passing
                 it will emit a deprecation warning.
  - version: v6.2.1
    pr-url: https://github.com/nodejs/node/pull/6828
    description: The `callback` parameter is optional now.
-->

* `prefix` {string}
* `options` {string|Object}
  * `encoding` {string} **Default:** `'utf8'`
* `callback` {Function}

Creates a unique temporary directory.

Generates six random characters to be appended behind a required
`prefix` to create a unique temporary directory.

The created folder path is passed as a string to the callback's second
parameter.

The optional `options` argument can be a string specifying an encoding, or an
object with an `encoding` property specifying the character encoding to use.

Example:

```js
fs.mkdtemp('/tmp/foo-', (err, folder) => {
  if (err) throw err;
  console.log(folder);
  // Prints: /tmp/foo-itXde2
});
```

*Note*: The `fs.mkdtemp()` method will append the six randomly selected
characters directly to the `prefix` string. For instance, given a directory
`/tmp`, if the intention is to create a temporary directory *within* `/tmp`,
the `prefix` *must* end with a trailing platform-specific path separator
(`require('path').sep`).

```js
// The parent directory for the new temporary directory
const tmpDir = '/tmp';

// This method is *INCORRECT*:
fs.mkdtemp(tmpDir, (err, folder) => {
  if (err) throw err;
  console.log(folder);
  // Will print something similar to `/tmpabc123`.
  // Note that a new temporary directory is created
  // at the file system root rather than *within*
  // the /tmp directory.
});

// This method is *CORRECT*:
const { sep } = require('path');
fs.mkdtemp(`${tmpDir}${sep}`, (err, folder) => {
  if (err) throw err;
  console.log(folder);
  // Will print something similar to `/tmp/abc123`.
  // A new temporary directory is created within
  // the /tmp directory.
});
```

## fs.mkdtempSync(prefix[, options])
<!-- YAML
added: v5.10.0
-->

* `prefix` {string}
* `options` {string|Object}
  * `encoding` {string} **Default:** `'utf8'`

The synchronous version of [`fs.mkdtemp()`][]. Returns the created
folder path.

The optional `options` argument can be a string specifying an encoding, or an
object with an `encoding` property specifying the character encoding to use.

## fs.open(path, flags[, mode], callback)
<!-- YAML
added: v0.0.2
changes:
  - version: v7.6.0
    pr-url: https://github.com/nodejs/node/pull/10739
    description: The `path` parameter can be a WHATWG `URL` object using `file:`
                 protocol. Support is currently still *experimental*.
-->

* `path` {string|Buffer|URL}
* `flags` {string|number}
* `mode` {integer} **Default:** `0o666`
* `callback` {Function}

Asynchronous file open. See open(2). `flags` can be:

* `'r'` - Open file for reading.
An exception occurs if the file does not exist.

* `'r+'` - Open file for reading and writing.
An exception occurs if the file does not exist.

* `'rs+'` - Open file for reading and writing in synchronous mode. Instructs
  the operating system to bypass the local file system cache.

  This is primarily useful for opening files on NFS mounts as it allows skipping
  the potentially stale local cache. It has a very real impact on I/O
  performance so using this flag is not recommended unless it is needed.

  Note that this doesn't turn `fs.open()` into a synchronous blocking call.
  If synchronous operation is desired `fs.openSync()` should be used.

* `'w'` - Open file for writing.
The file is created (if it does not exist) or truncated (if it exists).

* `'wx'` - Like `'w'` but fails if `path` exists.

* `'w+'` - Open file for reading and writing.
The file is created (if it does not exist) or truncated (if it exists).

* `'wx+'` - Like `'w+'` but fails if `path` exists.

* `'a'` - Open file for appending.
The file is created if it does not exist.

* `'ax'` - Like `'a'` but fails if `path` exists.

* `'a+'` - Open file for reading and appending.
The file is created if it does not exist.

* `'ax+'` - Like `'a+'` but fails if `path` exists.

`mode` sets the file mode (permission and sticky bits), but only if the file was
created. It defaults to `0o666`, readable and writable.

The callback gets two arguments `(err, fd)`.

The exclusive flag `'x'` (`O_EXCL` flag in open(2)) ensures that `path` is newly
created. On POSIX systems, `path` is considered to exist even if it is a symlink
to a non-existent file. The exclusive flag may or may not work with network file
systems.

`flags` can also be a number as documented by open(2); commonly used constants
are available from `fs.constants`.  On Windows, flags are translated to
their equivalent ones where applicable, e.g. `O_WRONLY` to `FILE_GENERIC_WRITE`,
or `O_EXCL|O_CREAT` to `CREATE_NEW`, as accepted by CreateFileW.

On Linux, positional writes don't work when the file is opened in append mode.
The kernel ignores the position argument and always appends the data to
the end of the file.

*Note*: The behavior of `fs.open()` is platform-specific for some flags. As
such, opening a directory on macOS and Linux with the `'a+'` flag - see example
below - will return an error. In contrast, on Windows and FreeBSD, a file
descriptor will be returned.

```js
// macOS and Linux
fs.open('<directory>', 'a+', (err, fd) => {
  // => [Error: EISDIR: illegal operation on a directory, open <directory>]
});

// Windows and FreeBSD
fs.open('<directory>', 'a+', (err, fd) => {
  // => null, <fd>
});
```

Some characters (`< > : " / \ | ? *`) are reserved under Windows as documented
by [Naming Files, Paths, and Namespaces][]. Under NTFS, if the filename contains
a colon, Node.js will open a file system stream, as described by
[this MSDN page][MSDN-Using-Streams].

Functions based on `fs.open()` exhibit this behavior as well. eg.
`fs.writeFile()`, `fs.readFile()`, etc.

## fs.openSync(path, flags[, mode])
<!-- YAML
added: v0.1.21
changes:
  - version: v7.6.0
    pr-url: https://github.com/nodejs/node/pull/10739
    description: The `path` parameter can be a WHATWG `URL` object using `file:`
                 protocol. Support is currently still *experimental*.
-->

* `path` {string|Buffer|URL}
* `flags` {string|number}
* `mode` {integer} **Default:** `0o666`

Synchronous version of [`fs.open()`][]. Returns an integer representing the file
descriptor.

## fs.read(fd, buffer, offset, length, position, callback)
<!-- YAML
added: v0.0.2
changes:
  - version: v7.4.0
    pr-url: https://github.com/nodejs/node/pull/10382
    description: The `buffer` parameter can now be a `Uint8Array`.
  - version: v6.0.0
    pr-url: https://github.com/nodejs/node/pull/4518
    description: The `length` parameter can now be `0`.
-->

* `fd` {integer}
* `buffer` {Buffer|Uint8Array}
* `offset` {integer}
* `length` {integer}
* `position` {integer}
* `callback` {Function}

Read data from the file specified by `fd`.

`buffer` is the buffer that the data will be written to.

`offset` is the offset in the buffer to start writing at.

`length` is an integer specifying the number of bytes to read.

`position` is an argument specifying where to begin reading from in the file.
If `position` is `null`, data will be read from the current file position,
and the file position will be updated.
If `position` is an integer, the file position will remain unchanged.

The callback is given the three arguments, `(err, bytesRead, buffer)`.

If this method is invoked as its [`util.promisify()`][]ed version, it returns
a Promise for an object with `bytesRead` and `buffer` properties.

## fs.readdir(path[, options], callback)
<!-- YAML
added: v0.1.8
changes:
  - version: v7.6.0
    pr-url: https://github.com/nodejs/node/pull/10739
    description: The `path` parameter can be a WHATWG `URL` object using `file:`
                 protocol. Support is currently still *experimental*.
  - version: v7.0.0
    pr-url: https://github.com/nodejs/node/pull/7897
    description: The `callback` parameter is no longer optional. Not passing
                 it will emit a deprecation warning.
  - version: v6.0.0
    pr-url: https://github.com/nodejs/node/pull/5616
    description: The `options` parameter was added.
-->

* `path` {string|Buffer|URL}
* `options` {string|Object}
  * `encoding` {string} **Default:** `'utf8'`
* `callback` {Function}

Asynchronous readdir(3).  Reads the contents of a directory.
The callback gets two arguments `(err, files)` where `files` is an array of
the names of the files in the directory excluding `'.'` and `'..'`.

The optional `options` argument can be a string specifying an encoding, or an
object with an `encoding` property specifying the character encoding to use for
the filenames passed to the callback. If the `encoding` is set to `'buffer'`,
the filenames returned will be passed as `Buffer` objects.

## fs.readdirSync(path[, options])
<!-- YAML
added: v0.1.21
changes:
  - version: v7.6.0
    pr-url: https://github.com/nodejs/node/pull/10739
    description: The `path` parameter can be a WHATWG `URL` object using `file:`
                 protocol. Support is currently still *experimental*.
-->

* `path` {string|Buffer|URL}
* `options` {string|Object}
  * `encoding` {string} **Default:** `'utf8'`

Synchronous readdir(3). Returns an array of filenames excluding `'.'` and
`'..'`.

The optional `options` argument can be a string specifying an encoding, or an
object with an `encoding` property specifying the character encoding to use for
the filenames passed to the callback. If the `encoding` is set to `'buffer'`,
the filenames returned will be passed as `Buffer` objects.

## fs.readFile(path[, options], callback)
<!-- YAML
added: v0.1.29
changes:
  - version: v7.6.0
    pr-url: https://github.com/nodejs/node/pull/10739
    description: The `path` parameter can be a WHATWG `URL` object using `file:`
                 protocol. Support is currently still *experimental*.
  - version: v7.0.0
    pr-url: https://github.com/nodejs/node/pull/7897
    description: The `callback` parameter is no longer optional. Not passing
                 it will emit a deprecation warning.
  - version: v5.1.0
    pr-url: https://github.com/nodejs/node/pull/3740
    description: The `callback` will always be called with `null` as the `error`
                 parameter in case of success.
  - version: v5.0.0
    pr-url: https://github.com/nodejs/node/pull/3163
    description: The `path` parameter can be a file descriptor now.
-->

* `path` {string|Buffer|URL|integer} filename or file descriptor
* `options` {Object|string}
  * `encoding` {string|null} **Default:** `null`
  * `flag` {string} **Default:** `'r'`
* `callback` {Function}

Asynchronously reads the entire contents of a file. Example:

```js
fs.readFile('/etc/passwd', (err, data) => {
  if (err) throw err;
  console.log(data);
});
```

The callback is passed two arguments `(err, data)`, where `data` is the
contents of the file.

If no encoding is specified, then the raw buffer is returned.

If `options` is a string, then it specifies the encoding. Example:

```js
fs.readFile('/etc/passwd', 'utf8', callback);
```
*Note*: When the path is a directory, the behavior of
`fs.readFile()` and [`fs.readFileSync()`][] is platform-specific. On macOS,
Linux, and Windows, an error will be returned. On FreeBSD, a representation
of the directory's contents will be returned.

```js
// macOS, Linux and Windows
fs.readFile('<directory>', (err, data) => {
  // => [Error: EISDIR: illegal operation on a directory, read <directory>]
});

//  FreeBSD
fs.readFile('<directory>', (err, data) => {
  // => null, <data>
});
```

Any specified file descriptor has to support reading.

*Note*: If a file descriptor is specified as the `path`, it will not be closed
automatically.

## fs.readFileSync(path[, options])
<!-- YAML
added: v0.1.8
changes:
  - version: v7.6.0
    pr-url: https://github.com/nodejs/node/pull/10739
    description: The `path` parameter can be a WHATWG `URL` object using `file:`
                 protocol. Support is currently still *experimental*.
  - version: v5.0.0
    pr-url: https://github.com/nodejs/node/pull/3163
    description: The `path` parameter can be a file descriptor now.
-->

* `path` {string|Buffer|URL|integer} filename or file descriptor
* `options` {Object|string}
  * `encoding` {string|null} **Default:** `null`
  * `flag` {string} **Default:** `'r'`

Synchronous version of [`fs.readFile()`][]. Returns the contents of the `path`.

If the `encoding` option is specified then this function returns a
string. Otherwise it returns a buffer.

*Note*: Similar to [`fs.readFile()`][], when the path is a directory, the
behavior of `fs.readFileSync()` is platform-specific.

```js
// macOS, Linux and Windows
fs.readFileSync('<directory>');
// => [Error: EISDIR: illegal operation on a directory, read <directory>]

//  FreeBSD
fs.readFileSync('<directory>'); // => null, <data>
```

## fs.readlink(path[, options], callback)
<!-- YAML
added: v0.1.31
changes:
  - version: v7.6.0
    pr-url: https://github.com/nodejs/node/pull/10739
    description: The `path` parameter can be a WHATWG `URL` object using `file:`
                 protocol. Support is currently still *experimental*.
  - version: v7.0.0
    pr-url: https://github.com/nodejs/node/pull/7897
    description: The `callback` parameter is no longer optional. Not passing
                 it will emit a deprecation warning.
-->

* `path` {string|Buffer|URL}
* `options` {string|Object}
  * `encoding` {string} **Default:** `'utf8'`
* `callback` {Function}

Asynchronous readlink(2). The callback gets two arguments `(err,
linkString)`.

The optional `options` argument can be a string specifying an encoding, or an
object with an `encoding` property specifying the character encoding to use for
the link path passed to the callback. If the `encoding` is set to `'buffer'`,
the link path returned will be passed as a `Buffer` object.

## fs.readlinkSync(path[, options])
<!-- YAML
added: v0.1.31
changes:
  - version: v7.6.0
    pr-url: https://github.com/nodejs/node/pull/10739
    description: The `path` parameter can be a WHATWG `URL` object using `file:`
                 protocol. Support is currently still *experimental*.
-->

* `path` {string|Buffer|URL}
* `options` {string|Object}
  * `encoding` {string} **Default:** `'utf8'`

Synchronous readlink(2). Returns the symbolic link's string value.

The optional `options` argument can be a string specifying an encoding, or an
object with an `encoding` property specifying the character encoding to use for
the link path passed to the callback. If the `encoding` is set to `'buffer'`,
the link path returned will be passed as a `Buffer` object.

## fs.readSync(fd, buffer, offset, length, position)
<!-- YAML
added: v0.1.21
changes:
  - version: v6.0.0
    pr-url: https://github.com/nodejs/node/pull/4518
    description: The `length` parameter can now be `0`.
-->

* `fd` {integer}
* `buffer` {string|Buffer|Uint8Array}
* `offset` {integer}
* `length` {integer}
* `position` {integer}

Synchronous version of [`fs.read()`][]. Returns the number of `bytesRead`.

## fs.realpath(path[, options], callback)
<!-- YAML
added: v0.1.31
changes:
  - version: v8.0.0
    pr-url: https://github.com/nodejs/node/pull/13028
    description: Pipe/Socket resolve support was added.
  - version: v7.6.0
    pr-url: https://github.com/nodejs/node/pull/10739
    description: The `path` parameter can be a WHATWG `URL` object using
                 `file:` protocol. Support is currently still *experimental*.
  - version: v7.0.0
    pr-url: https://github.com/nodejs/node/pull/7897
    description: The `callback` parameter is no longer optional. Not passing
                 it will emit a deprecation warning.
  - version: v6.4.0
    pr-url: https://github.com/nodejs/node/pull/7899
    description: Calling `realpath` now works again for various edge cases
                 on Windows.
  - version: v6.0.0
    pr-url: https://github.com/nodejs/node/pull/3594
    description: The `cache` parameter was removed.
-->

* `path` {string|Buffer|URL}
* `options` {string|Object}
  * `encoding` {string} **Default:** `'utf8'`
* `callback` {Function}

Asynchronous realpath(3). The `callback` gets two arguments `(err,
resolvedPath)`. May use `process.cwd` to resolve relative paths.

Only paths that can be converted to UTF8 strings are supported.

The optional `options` argument can be a string specifying an encoding, or an
object with an `encoding` property specifying the character encoding to use for
the path passed to the callback. If the `encoding` is set to `'buffer'`,
the path returned will be passed as a `Buffer` object.

*Note*: If `path` resolves to a socket or a pipe, the function will return a
system dependent name for that object.

## fs.realpathSync(path[, options])
<!-- YAML
added: v0.1.31
changes:
  - version: v8.0.0
    pr-url: https://github.com/nodejs/node/pull/13028
    description: Pipe/Socket resolve support was added.
  - version: v7.6.0
    pr-url: https://github.com/nodejs/node/pull/10739
    description: The `path` parameter can be a WHATWG `URL` object using
                 `file:` protocol. Support is currently still *experimental*.
  - version: v6.4.0
    pr-url: https://github.com/nodejs/node/pull/7899
    description: Calling `realpathSync` now works again for various edge cases
                 on Windows.
  - version: v6.0.0
    pr-url: https://github.com/nodejs/node/pull/3594
    description: The `cache` parameter was removed.
-->

* `path` {string|Buffer|URL}
* `options` {string|Object}
  * `encoding` {string} **Default:** `'utf8'`

Synchronous realpath(3). Returns the resolved path.

Only paths that can be converted to UTF8 strings are supported.

The optional `options` argument can be a string specifying an encoding, or an
object with an `encoding` property specifying the character encoding to use for
the returned value. If the `encoding` is set to `'buffer'`, the path returned
will be passed as a `Buffer` object.

*Note*: If `path` resolves to a socket or a pipe, the function will return a
system dependent name for that object.

## fs.rename(oldPath, newPath, callback)
<!-- YAML
added: v0.0.2
changes:
  - version: v7.6.0
    pr-url: https://github.com/nodejs/node/pull/10739
    description: The `oldPath` and `newPath` parameters can be WHATWG `URL`
                 objects using `file:` protocol. Support is currently still
                 *experimental*.
  - version: v7.0.0
    pr-url: https://github.com/nodejs/node/pull/7897
    description: The `callback` parameter is no longer optional. Not passing
                 it will emit a deprecation warning.
-->

* `oldPath` {string|Buffer|URL}
* `newPath` {string|Buffer|URL}
* `callback` {Function}

Asynchronous rename(2). No arguments other than a possible exception are given
to the completion callback.

## fs.renameSync(oldPath, newPath)
<!-- YAML
added: v0.1.21
changes:
  - version: v7.6.0
    pr-url: https://github.com/nodejs/node/pull/10739
    description: The `oldPath` and `newPath` parameters can be WHATWG `URL`
                 objects using `file:` protocol. Support is currently still
                 *experimental*.
-->

* `oldPath` {string|Buffer|URL}
* `newPath` {string|Buffer|URL}

Synchronous rename(2). Returns `undefined`.

## fs.rmdir(path, callback)
<!-- YAML
added: v0.0.2
changes:
  - version: v7.6.0
    pr-url: https://github.com/nodejs/node/pull/10739
    description: The `path` parameters can be a WHATWG `URL` object using
                 `file:` protocol. Support is currently still *experimental*.
  - version: v7.0.0
    pr-url: https://github.com/nodejs/node/pull/7897
    description: The `callback` parameter is no longer optional. Not passing
                 it will emit a deprecation warning.
-->

* `path` {string|Buffer|URL}
* `callback` {Function}

Asynchronous rmdir(2). No arguments other than a possible exception are given
to the completion callback.

*Note*: Using `fs.rmdir()` on a file (not a directory) results in an `ENOENT`
error on Windows and an `ENOTDIR` error on POSIX.

## fs.rmdirSync(path)
<!-- YAML
added: v0.1.21
changes:
  - version: v7.6.0
    pr-url: https://github.com/nodejs/node/pull/10739
    description: The `path` parameters can be a WHATWG `URL` object using
                 `file:` protocol. Support is currently still *experimental*.
-->

* `path` {string|Buffer|URL}

Synchronous rmdir(2). Returns `undefined`.

*Note*: Using `fs.rmdirSync()` on a file (not a directory) results in an `ENOENT`
error on Windows and an `ENOTDIR` error on POSIX.

## fs.stat(path, callback)
<!-- YAML
added: v0.0.2
changes:
  - version: v7.6.0
    pr-url: https://github.com/nodejs/node/pull/10739
    description: The `path` parameter can be a WHATWG `URL` object using `file:`
                 protocol. Support is currently still *experimental*.
  - version: v7.0.0
    pr-url: https://github.com/nodejs/node/pull/7897
    description: The `callback` parameter is no longer optional. Not passing
                 it will emit a deprecation warning.
-->

* `path` {string|Buffer|URL}
* `callback` {Function}

Asynchronous stat(2). The callback gets two arguments `(err, stats)` where
`stats` is an [`fs.Stats`][] object.

In case of an error, the `err.code` will be one of [Common System Errors][].

Using `fs.stat()` to check for the existence of a file before calling
`fs.open()`, `fs.readFile()` or `fs.writeFile()` is not recommended.
Instead, user code should open/read/write the file directly and handle the
error raised if the file is not available.

To check if a file exists without manipulating it afterwards, [`fs.access()`]
is recommended.

## fs.statSync(path)
<!-- YAML
added: v0.1.21
changes:
  - version: v7.6.0
    pr-url: https://github.com/nodejs/node/pull/10739
    description: The `path` parameter can be a WHATWG `URL` object using `file:`
                 protocol. Support is currently still *experimental*.
-->

* `path` {string|Buffer|URL}

Synchronous stat(2). Returns an instance of [`fs.Stats`][].

## fs.symlink(target, path[, type], callback)
<!-- YAML
added: v0.1.31
changes:
  - version: v7.6.0
    pr-url: https://github.com/nodejs/node/pull/10739
    description: The `target` and `path` parameters can be WHATWG `URL` objects
                 using `file:` protocol. Support is currently still
                 *experimental*.
-->

* `target` {string|Buffer|URL}
* `path` {string|Buffer|URL}
* `type` {string} **Default:** `'file'`
* `callback` {Function}

Asynchronous symlink(2). No arguments other than a possible exception are given
to the completion callback. The `type` argument can be set to `'dir'`,
`'file'`, or `'junction'` (default is `'file'`) and is only available on
Windows (ignored on other platforms). Note that Windows junction points require
the destination path to be absolute. When using `'junction'`, the `target`
argument will automatically be normalized to absolute path.

Here is an example below:

```js
fs.symlink('./foo', './new-port', callback);
```

It creates a symbolic link named "new-port" that points to "foo".

## fs.symlinkSync(target, path[, type])
<!-- YAML
added: v0.1.31
changes:
  - version: v7.6.0
    pr-url: https://github.com/nodejs/node/pull/10739
    description: The `target` and `path` parameters can be WHATWG `URL` objects
                 using `file:` protocol. Support is currently still
                 *experimental*.
-->

* `target` {string|Buffer|URL}
* `path` {string|Buffer|URL}
* `type` {string} **Default:** `'file'`

Synchronous symlink(2). Returns `undefined`.

## fs.truncate(path[, len], callback)
<!-- YAML
added: v0.8.6
changes:
  - version: v7.0.0
    pr-url: https://github.com/nodejs/node/pull/7897
    description: The `callback` parameter is no longer optional. Not passing
                 it will emit a deprecation warning.
-->

* `path` {string|Buffer}
* `len` {integer} **Default:** `0`
* `callback` {Function}

Asynchronous truncate(2). No arguments other than a possible exception are
given to the completion callback. A file descriptor can also be passed as the
first argument. In this case, `fs.ftruncate()` is called.

## fs.truncateSync(path[, len])
<!-- YAML
added: v0.8.6
-->

* `path` {string|Buffer}
* `len` {integer} **Default:** `0`

Synchronous truncate(2). Returns `undefined`. A file descriptor can also be
passed as the first argument. In this case, `fs.ftruncateSync()` is called.

## fs.unlink(path, callback)
<!-- YAML
added: v0.0.2
changes:
  - version: v7.6.0
    pr-url: https://github.com/nodejs/node/pull/10739
    description: The `path` parameter can be a WHATWG `URL` object using `file:`
                 protocol. Support is currently still *experimental*.
  - version: v7.0.0
    pr-url: https://github.com/nodejs/node/pull/7897
    description: The `callback` parameter is no longer optional. Not passing
                 it will emit a deprecation warning.
-->

* `path` {string|Buffer|URL}
* `callback` {Function}

Asynchronous unlink(2). No arguments other than a possible exception are given
to the completion callback.

## fs.unlinkSync(path)
<!-- YAML
added: v0.1.21
changes:
  - version: v7.6.0
    pr-url: https://github.com/nodejs/node/pull/10739
    description: The `path` parameter can be a WHATWG `URL` object using `file:`
                 protocol. Support is currently still *experimental*.
-->

* `path` {string|Buffer|URL}

Synchronous unlink(2). Returns `undefined`.

## fs.unwatchFile(filename[, listener])
<!-- YAML
added: v0.1.31
-->

* `filename` {string|Buffer}
* `listener` {Function|undefined} **Default:** `undefined`

Stop watching for changes on `filename`. If `listener` is specified, only that
particular listener is removed. Otherwise, *all* listeners are removed,
effectively stopping watching of `filename`.

Calling `fs.unwatchFile()` with a filename that is not being watched is a
no-op, not an error.

*Note*: [`fs.watch()`][] is more efficient than `fs.watchFile()` and
`fs.unwatchFile()`.  `fs.watch()` should be used instead of `fs.watchFile()`
and `fs.unwatchFile()` when possible.

## fs.utimes(path, atime, mtime, callback)
<!-- YAML
added: v0.4.2
changes:
  - version: v7.6.0
    pr-url: https://github.com/nodejs/node/pull/10739
    description: The `path` parameter can be a WHATWG `URL` object using `file:`
                 protocol. Support is currently still *experimental*.
  - version: v7.0.0
    pr-url: https://github.com/nodejs/node/pull/7897
    description: The `callback` parameter is no longer optional. Not passing
                 it will emit a deprecation warning.
  - version: v4.1.0
    pr-url: https://github.com/nodejs/node/pull/2387
    description: Numeric strings, `NaN` and `Infinity` are now allowed
                 time specifiers.
-->

* `path` {string|Buffer|URL}
* `atime` {integer}
* `mtime` {integer}
* `callback` {Function}

Change file timestamps of the file referenced by the supplied path.

*Note*: The arguments `atime` and `mtime` of the following related functions
follow these rules:

- The value should be a Unix timestamp in seconds. For example, `Date.now()`
  returns milliseconds, so it should be divided by 1000 before passing it in.
- If the value is a numeric string like `'123456789'`, the value will get
  converted to the corresponding number.
- If the value is `NaN`, `Infinity` or `-Infinity`, an Error will be thrown.

## fs.utimesSync(path, atime, mtime)
<!-- YAML
added: v0.4.2
changes:
  - version: v7.6.0
    pr-url: https://github.com/nodejs/node/pull/10739
    description: The `path` parameter can be a WHATWG `URL` object using `file:`
                 protocol. Support is currently still *experimental*.
  - version: v4.1.0
    pr-url: https://github.com/nodejs/node/pull/2387
    description: Numeric strings, `NaN` and `Infinity` are now allowed
                 time specifiers.
-->

* `path` {string|Buffer|URL}
* `atime` {integer}
* `mtime` {integer}

Synchronous version of [`fs.utimes()`][]. Returns `undefined`.

## fs.watch(filename[, options][, listener])
<!-- YAML
added: v0.5.10
changes:
  - version: v7.6.0
    pr-url: https://github.com/nodejs/node/pull/10739
    description: The `filename` parameter can be a WHATWG `URL` object using
                 `file:` protocol. Support is currently still *experimental*.
  - version: v7.0.0
    pr-url: https://github.com/nodejs/node/pull/7831
    description: The passed `options` object will never be modified.
-->

* `filename` {string|Buffer|URL}
* `options` {string|Object}
  * `persistent` {boolean} Indicates whether the process should continue to run
    as long as files are being watched. **Default:** `true`
  * `recursive` {boolean} Indicates whether all subdirectories should be
    watched, or only the current directory. This applies when a directory is
    specified, and only on supported platforms (See [Caveats][]). **Default:**
    `false`
  * `encoding` {string} Specifies the character encoding to be used for the
     filename passed to the listener. **Default:** `'utf8'`
* `listener` {Function|undefined} **Default:** `undefined`

Watch for changes on `filename`, where `filename` is either a file or a
directory.  The returned object is a [`fs.FSWatcher`][].

The second argument is optional. If `options` is provided as a string, it
specifies the `encoding`. Otherwise `options` should be passed as an object.

The listener callback gets two arguments `(eventType, filename)`.  `eventType` is either
`'rename'` or `'change'`, and `filename` is the name of the file which triggered
the event.

Note that on most platforms, `'rename'` is emitted whenever a filename appears
or disappears in the directory.

Also note the listener callback is attached to the `'change'` event fired by
[`fs.FSWatcher`][], but it is not the same thing as the `'change'` value of
`eventType`.

### Caveats

<!--type=misc-->

The `fs.watch` API is not 100% consistent across platforms, and is
unavailable in some situations.

The recursive option is only supported on macOS and Windows.

#### Availability

<!--type=misc-->

This feature depends on the underlying operating system providing a way
to be notified of filesystem changes.

* On Linux systems, this uses [`inotify`]
* On BSD systems, this uses [`kqueue`]
* On macOS, this uses [`kqueue`] for files and [`FSEvents`] for directories.
* On SunOS systems (including Solaris and SmartOS), this uses [`event ports`].
* On Windows systems, this feature depends on [`ReadDirectoryChangesW`].
* On Aix systems, this feature depends on [`AHAFS`], which must be enabled.

If the underlying functionality is not available for some reason, then
`fs.watch` will not be able to function. For example, watching files or
directories can be unreliable, and in some cases impossible, on network file
systems (NFS, SMB, etc), or host file systems when using virtualization software
such as Vagrant, Docker, etc.

It is still possible to use `fs.watchFile()`, which uses stat polling, but
this method is slower and less reliable.

#### Inodes

<!--type=misc-->

On Linux and macOS systems, `fs.watch()` resolves the path to an [inode][] and
watches the inode. If the watched path is deleted and recreated, it is assigned
a new inode. The watch will emit an event for the delete but will continue
watching the *original* inode. Events for the new inode will not be emitted.
This is expected behavior.

In AIX, save and close of a file being watched causes two notifications -
one for adding new content, and one for truncation. Moreover, save and
close operations on some platforms cause inode changes that force watch
operations to become invalid and ineffective. AIX retains inode for the
lifetime of a file, that way though this is different from Linux / macOS,
this improves the usability of file watching. This is expected behavior.

#### Filename Argument

<!--type=misc-->

Providing `filename` argument in the callback is only supported on Linux,
macOS, Windows, and AIX.  Even on supported platforms, `filename` is not always
guaranteed to be provided. Therefore, don't assume that `filename` argument is
always provided in the callback, and have some fallback logic if it is null.

```js
fs.watch('somedir', (eventType, filename) => {
  console.log(`event type is: ${eventType}`);
  if (filename) {
    console.log(`filename provided: ${filename}`);
  } else {
    console.log('filename not provided');
  }
});
```

## fs.watchFile(filename[, options], listener)
<!-- YAML
added: v0.1.31
changes:
  - version: v7.6.0
    pr-url: https://github.com/nodejs/node/pull/10739
    description: The `filename` parameter can be a WHATWG `URL` object using
                 `file:` protocol. Support is currently still *experimental*.
-->

* `filename` {string|Buffer|URL}
* `options` {Object}
  * `persistent` {boolean} **Default:** `true`
  * `interval` {integer} **Default:** `5007`
* `listener` {Function}

Watch for changes on `filename`. The callback `listener` will be called each
time the file is accessed.

The `options` argument may be omitted. If provided, it should be an object. The
`options` object may contain a boolean named `persistent` that indicates
whether the process should continue to run as long as files are being watched.
The `options` object may specify an `interval` property indicating how often the
target should be polled in milliseconds. The default is
`{ persistent: true, interval: 5007 }`.

The `listener` gets two arguments the current stat object and the previous
stat object:

```js
fs.watchFile('message.text', (curr, prev) => {
  console.log(`the current mtime is: ${curr.mtime}`);
  console.log(`the previous mtime was: ${prev.mtime}`);
});
```

These stat objects are instances of `fs.Stat`.

To be notified when the file was modified, not just accessed, it is necessary
to compare `curr.mtime` and `prev.mtime`.

*Note*: When an `fs.watchFile` operation results in an `ENOENT` error, it
will invoke the listener once, with all the fields zeroed (or, for dates, the
Unix Epoch). In Windows, `blksize` and `blocks` fields will be `undefined`,
instead of zero. If the file is created later on, the listener will be called
again, with the latest stat objects. This is a change in functionality since
v0.10.

*Note*: [`fs.watch()`][] is more efficient than `fs.watchFile` and
`fs.unwatchFile`. `fs.watch` should be used instead of `fs.watchFile` and
`fs.unwatchFile` when possible.

## fs.write(fd, buffer[, offset[, length[, position]]], callback)
<!-- YAML
added: v0.0.2
changes:
  - version: v7.4.0
    pr-url: https://github.com/nodejs/node/pull/10382
    description: The `buffer` parameter can now be a `Uint8Array`.
  - version: v7.2.0
    pr-url: https://github.com/nodejs/node/pull/7856
    description: The `offset` and `length` parameters are optional now.
  - version: v7.0.0
    pr-url: https://github.com/nodejs/node/pull/7897
    description: The `callback` parameter is no longer optional. Not passing
                 it will emit a deprecation warning.
-->

* `fd` {integer}
* `buffer` {Buffer|Uint8Array}
* `offset` {integer}
* `length` {integer}
* `position` {integer}
* `callback` {Function}

Write `buffer` to the file specified by `fd`.

`offset` determines the part of the buffer to be written, and `length` is
an integer specifying the number of bytes to write.

`position` refers to the offset from the beginning of the file where this data
should be written. If `typeof position !== 'number'`, the data will be written
at the current position. See pwrite(2).

The callback will be given three arguments `(err, bytesWritten, buffer)` where
`bytesWritten` specifies how many _bytes_ were written from `buffer`.

If this method is invoked as its [`util.promisify()`][]ed version, it returns
a Promise for an object with `bytesWritten` and `buffer` properties.

Note that it is unsafe to use `fs.write` multiple times on the same file
without waiting for the callback. For this scenario,
`fs.createWriteStream` is strongly recommended.

On Linux, positional writes don't work when the file is opened in append mode.
The kernel ignores the position argument and always appends the data to
the end of the file.

## fs.write(fd, string[, position[, encoding]], callback)
<!-- YAML
added: v0.11.5
changes:
  - version: v7.2.0
    pr-url: https://github.com/nodejs/node/pull/7856
    description: The `position` parameter is optional now.
  - version: v7.0.0
    pr-url: https://github.com/nodejs/node/pull/7897
    description: The `callback` parameter is no longer optional. Not passing
                 it will emit a deprecation warning.
-->

* `fd` {integer}
* `string` {string}
* `position` {integer}
* `encoding` {string}
* `callback` {Function}

Write `string` to the file specified by `fd`.  If `string` is not a string, then
the value will be coerced to one.

`position` refers to the offset from the beginning of the file where this data
should be written. If `typeof position !== 'number'` the data will be written at
the current position. See pwrite(2).

`encoding` is the expected string encoding.

The callback will receive the arguments `(err, written, string)` where `written`
specifies how many _bytes_ the passed string required to be written. Note that
bytes written is not the same as string characters. See [`Buffer.byteLength`][].

Unlike when writing `buffer`, the entire string must be written. No substring
may be specified. This is because the byte offset of the resulting data may not
be the same as the string offset.

Note that it is unsafe to use `fs.write` multiple times on the same file
without waiting for the callback. For this scenario,
`fs.createWriteStream` is strongly recommended.

On Linux, positional writes don't work when the file is opened in append mode.
The kernel ignores the position argument and always appends the data to
the end of the file.

## fs.writeFile(file, data[, options], callback)
<!-- YAML
added: v0.1.29
changes:
  - version: v7.4.0
    pr-url: https://github.com/nodejs/node/pull/10382
    description: The `data` parameter can now be a `Uint8Array`.
  - version: v7.0.0
    pr-url: https://github.com/nodejs/node/pull/7897
    description: The `callback` parameter is no longer optional. Not passing
                 it will emit a deprecation warning.
  - version: v5.0.0
    pr-url: https://github.com/nodejs/node/pull/3163
    description: The `file` parameter can be a file descriptor now.
-->

* `file` {string|Buffer|integer} filename or file descriptor
* `data` {string|Buffer|Uint8Array}
* `options` {Object|string}
  * `encoding` {string|null} **Default:** `'utf8'`
  * `mode` {integer} **Default:** `0o666`
  * `flag` {string} **Default:** `'w'`
* `callback` {Function}

Asynchronously writes data to a file, replacing the file if it already exists.
`data` can be a string or a buffer.

The `encoding` option is ignored if `data` is a buffer. It defaults
to `'utf8'`.

Example:

```js
fs.writeFile('message.txt', 'Hello Node.js', (err) => {
  if (err) throw err;
  console.log('The file has been saved!');
});
```

If `options` is a string, then it specifies the encoding. Example:

```js
fs.writeFile('message.txt', 'Hello Node.js', 'utf8', callback);
```

Any specified file descriptor has to support writing.

Note that it is unsafe to use `fs.writeFile` multiple times on the same file
without waiting for the callback. For this scenario,
`fs.createWriteStream` is strongly recommended.

*Note*: If a file descriptor is specified as the `file`, it will not be closed
automatically.

## fs.writeFileSync(file, data[, options])
<!-- YAML
added: v0.1.29
changes:
  - version: v7.4.0
    pr-url: https://github.com/nodejs/node/pull/10382
    description: The `data` parameter can now be a `Uint8Array`.
  - version: v5.0.0
    pr-url: https://github.com/nodejs/node/pull/3163
    description: The `file` parameter can be a file descriptor now.
-->

* `file` {string|Buffer|integer} filename or file descriptor
* `data` {string|Buffer|Uint8Array}
* `options` {Object|string}
  * `encoding` {string|null} **Default:** `'utf8'`
  * `mode` {integer} **Default:** `0o666`
  * `flag` {string} **Default:** `'w'`

The synchronous version of [`fs.writeFile()`][]. Returns `undefined`.

## fs.writeSync(fd, buffer[, offset[, length[, position]]])
<!-- YAML
added: v0.1.21
changes:
  - version: v7.4.0
    pr-url: https://github.com/nodejs/node/pull/10382
    description: The `buffer` parameter can now be a `Uint8Array`.
  - version: v7.2.0
    pr-url: https://github.com/nodejs/node/pull/7856
    description: The `offset` and `length` parameters are optional now.
-->

* `fd` {integer}
* `buffer` {Buffer|Uint8Array}
* `offset` {integer}
* `length` {integer}
* `position` {integer}

## fs.writeSync(fd, string[, position[, encoding]])
<!-- YAML
added: v0.11.5
changes:
  - version: v7.2.0
    pr-url: https://github.com/nodejs/node/pull/7856
    description: The `position` parameter is optional now.
-->

* `fd` {integer}
* `string` {string}
* `position` {integer}
* `encoding` {string}

Synchronous versions of [`fs.write()`][]. Returns the number of bytes written.

## FS Constants

The following constants are exported by `fs.constants`.

*Note*: Not every constant will be available on every operating system.

### File Access Constants

The following constants are meant for use with [`fs.access()`][].

<table>
  <tr>
    <th>Constant</th>
    <th>Description</th>
  </tr>
  <tr>
    <td><code>F_OK</code></td>
    <td>Flag indicating that the file is visible to the calling process.</td>
  </tr>
  <tr>
    <td><code>R_OK</code></td>
    <td>Flag indicating that the file can be read by the calling process.</td>
  </tr>
  <tr>
    <td><code>W_OK</code></td>
    <td>Flag indicating that the file can be written by the calling
    process.</td>
  </tr>
  <tr>
    <td><code>X_OK</code></td>
    <td>Flag indicating that the file can be executed by the calling
    process.</td>
  </tr>
</table>

### File Open Constants

The following constants are meant for use with `fs.open()`.

<table>
  <tr>
    <th>Constant</th>
    <th>Description</th>
  </tr>
  <tr>
    <td><code>O_RDONLY</code></td>
    <td>Flag indicating to open a file for read-only access.</td>
  </tr>
  <tr>
    <td><code>O_WRONLY</code></td>
    <td>Flag indicating to open a file for write-only access.</td>
  </tr>
  <tr>
    <td><code>O_RDWR</code></td>
    <td>Flag indicating to open a file for read-write access.</td>
  </tr>
  <tr>
    <td><code>O_CREAT</code></td>
    <td>Flag indicating to create the file if it does not already exist.</td>
  </tr>
  <tr>
    <td><code>O_EXCL</code></td>
    <td>Flag indicating that opening a file should fail if the
    <code>O_CREAT</code> flag is set and the file already exists.</td>
  </tr>
  <tr>
    <td><code>O_NOCTTY</code></td>
    <td>Flag indicating that if path identifies a terminal device, opening the
    path shall not cause that terminal to become the controlling terminal for
    the process (if the process does not already have one).</td>
  </tr>
  <tr>
    <td><code>O_TRUNC</code></td>
    <td>Flag indicating that if the file exists and is a regular file, and the
    file is opened successfully for write access, its length shall be truncated
    to zero.</td>
  </tr>
  <tr>
    <td><code>O_APPEND</code></td>
    <td>Flag indicating that data will be appended to the end of the file.</td>
  </tr>
  <tr>
    <td><code>O_DIRECTORY</code></td>
    <td>Flag indicating that the open should fail if the path is not a
    directory.</td>
  </tr>
  <tr>
  <td><code>O_NOATIME</code></td>
    <td>Flag indicating reading accesses to the file system will no longer
    result in an update to the `atime` information associated with the file.
    This flag is available on Linux operating systems only.</td>
  </tr>
  <tr>
    <td><code>O_NOFOLLOW</code></td>
    <td>Flag indicating that the open should fail if the path is a symbolic
    link.</td>
  </tr>
  <tr>
    <td><code>O_SYNC</code></td>
    <td>Flag indicating that the file is opened for synchronous I/O.</td>
  </tr>
  <tr>
    <td><code>O_SYMLINK</code></td>
    <td>Flag indicating to open the symbolic link itself rather than the
    resource it is pointing to.</td>
  </tr>
  <tr>
    <td><code>O_DIRECT</code></td>
    <td>When set, an attempt will be made to minimize caching effects of file
    I/O.</td>
  </tr>
  <tr>
    <td><code>O_NONBLOCK</code></td>
    <td>Flag indicating to open the file in nonblocking mode when possible.</td>
  </tr>
</table>

### File Type Constants

The following constants are meant for use with the [`fs.Stats`][] object's
`mode` property for determining a file's type.

<table>
  <tr>
    <th>Constant</th>
    <th>Description</th>
  </tr>
  <tr>
    <td><code>S_IFMT</code></td>
    <td>Bit mask used to extract the file type code.</td>
  </tr>
  <tr>
    <td><code>S_IFREG</code></td>
    <td>File type constant for a regular file.</td>
  </tr>
  <tr>
    <td><code>S_IFDIR</code></td>
    <td>File type constant for a directory.</td>
  </tr>
  <tr>
    <td><code>S_IFCHR</code></td>
    <td>File type constant for a character-oriented device file.</td>
  </tr>
  <tr>
    <td><code>S_IFBLK</code></td>
    <td>File type constant for a block-oriented device file.</td>
  </tr>
  <tr>
    <td><code>S_IFIFO</code></td>
    <td>File type constant for a FIFO/pipe.</td>
  </tr>
  <tr>
    <td><code>S_IFLNK</code></td>
    <td>File type constant for a symbolic link.</td>
  </tr>
  <tr>
    <td><code>S_IFSOCK</code></td>
    <td>File type constant for a socket.</td>
  </tr>
</table>

### File Mode Constants

The following constants are meant for use with the [`fs.Stats`][] object's
`mode` property for determining the access permissions for a file.

<table>
  <tr>
    <th>Constant</th>
    <th>Description</th>
  </tr>
  <tr>
    <td><code>S_IRWXU</code></td>
    <td>File mode indicating readable, writable and executable by owner.</td>
  </tr>
  <tr>
    <td><code>S_IRUSR</code></td>
    <td>File mode indicating readable by owner.</td>
  </tr>
  <tr>
    <td><code>S_IWUSR</code></td>
    <td>File mode indicating writable by owner.</td>
  </tr>
  <tr>
    <td><code>S_IXUSR</code></td>
    <td>File mode indicating executable by owner.</td>
  </tr>
  <tr>
    <td><code>S_IRWXG</code></td>
    <td>File mode indicating readable, writable and executable by group.</td>
  </tr>
  <tr>
    <td><code>S_IRGRP</code></td>
    <td>File mode indicating readable by group.</td>
  </tr>
  <tr>
    <td><code>S_IWGRP</code></td>
    <td>File mode indicating writable by group.</td>
  </tr>
  <tr>
    <td><code>S_IXGRP</code></td>
    <td>File mode indicating executable by group.</td>
  </tr>
  <tr>
    <td><code>S_IRWXO</code></td>
    <td>File mode indicating readable, writable and executable by others.</td>
  </tr>
  <tr>
    <td><code>S_IROTH</code></td>
    <td>File mode indicating readable by others.</td>
  </tr>
  <tr>
    <td><code>S_IWOTH</code></td>
    <td>File mode indicating writable by others.</td>
  </tr>
  <tr>
    <td><code>S_IXOTH</code></td>
    <td>File mode indicating executable by others.</td>
  </tr>
</table>


[`AHAFS`]: https://www.ibm.com/developerworks/aix/library/au-aix_event_infrastructure/
[`Buffer.byteLength`]: buffer.html#buffer_class_method_buffer_bytelength_string_encoding
[`Buffer`]: buffer.html#buffer_buffer
[`FSEvents`]: https://developer.apple.com/library/mac/documentation/Darwin/Conceptual/FSEvents_ProgGuide/Introduction/Introduction.html#//apple_ref/doc/uid/TP40005289-CH1-SW1
[`ReadDirectoryChangesW`]: https://msdn.microsoft.com/en-us/library/windows/desktop/aa365465%28v=vs.85%29.aspx
[`ReadStream`]: #fs_class_fs_readstream
[`URL`]: url.html#url_the_whatwg_url_api
[`UV_THREADPOOL_SIZE`]: cli.html#cli_uv_threadpool_size_size
[`WriteStream`]: #fs_class_fs_writestream
[`event ports`]: http://illumos.org/man/port_create
[`fs.FSWatcher`]: #fs_class_fs_fswatcher
[`fs.Stats`]: #fs_class_fs_stats
[`fs.access()`]: #fs_fs_access_path_mode_callback
[`fs.appendFile()`]: fs.html#fs_fs_appendfile_file_data_options_callback
[`fs.exists()`]: fs.html#fs_fs_exists_path_callback
[`fs.fstat()`]: #fs_fs_fstat_fd_callback
[`fs.futimes()`]: #fs_fs_futimes_fd_atime_mtime_callback
[`fs.lstat()`]: #fs_fs_lstat_path_callback
[`fs.mkdtemp()`]: #fs_fs_mkdtemp_prefix_options_callback
[`fs.open()`]: #fs_fs_open_path_flags_mode_callback
[`fs.read()`]: #fs_fs_read_fd_buffer_offset_length_position_callback
[`fs.readFile()`]: #fs_fs_readfile_path_options_callback
[`fs.readFileSync()`]: #fs_fs_readfilesync_path_options
[`fs.stat()`]: #fs_fs_stat_path_callback
[`fs.utimes()`]: #fs_fs_utimes_path_atime_mtime_callback
[`fs.watch()`]: #fs_fs_watch_filename_options_listener
[`fs.write()`]: #fs_fs_write_fd_buffer_offset_length_position_callback
[`fs.writeFile()`]: #fs_fs_writefile_file_data_options_callback
[`inotify`]: http://man7.org/linux/man-pages/man7/inotify.7.html
[`kqueue`]: https://www.freebsd.org/cgi/man.cgi?kqueue
[`net.Socket`]: net.html#net_class_net_socket
[`stat()`]: fs.html#fs_fs_stat_path_callback
[`util.inspect(stats)`]: util.html#util_util_inspect_object_options
[`util.promisify()`]: util.html#util_util_promisify_original
[Caveats]: #fs_caveats
[Common System Errors]: errors.html#errors_common_system_errors
[FS Constants]: #fs_fs_constants_1
[MDN-Date]: https://developer.mozilla.org/en/JavaScript/Reference/Global_Objects/Date
[MDN-Number]: https://developer.mozilla.org/en-US/docs/Web/JavaScript/Data_structures#Number_type
[MSDN-Rel-Path]: https://msdn.microsoft.com/en-us/library/windows/desktop/aa365247.aspx#fully_qualified_vs._relative_paths
[Readable Stream]: stream.html#stream_class_stream_readable
[Writable Stream]: stream.html#stream_class_stream_writable
[inode]: https://en.wikipedia.org/wiki/Inode
[Naming Files, Paths, and Namespaces]: https://msdn.microsoft.com/en-us/library/windows/desktop/aa365247(v=vs.85).aspx
[MSDN-Using-Streams]: https://msdn.microsoft.com/en-us/library/windows/desktop/bb540537.aspx

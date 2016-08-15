# File System

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
You can use try/catch to handle exceptions or allow them to bubble up.

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

Most fs functions let you omit the callback argument. If you do, a default
callback is used that rethrows errors. To get a trace to the original call
site, set the `NODE_DEBUG` environment variable:

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

## Buffer API
<!-- YAML
added: v6.0.0
-->

`fs` functions support passing and receiving paths as both strings
and Buffers. The latter is intended to make it possible to work with
filesystems that allow for non-UTF-8 filenames. For most typical
uses, working with paths as Buffers will be unnecessary, as the string
API converts to and from UTF-8 automatically.

*Note* that on certain file systems (such as NTFS and HFS+) filenames
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

* `eventType` {String} The type of fs change
* `filename` {String | Buffer} The filename that changed (if relevant/available)

Emitted when something changes in a watched directory or file.
See more details in [`fs.watch()`][].

The `filename` argument may not be provided depending on operating system
support. If `filename` is provided, it will be provided as a `Buffer` if
`fs.watch()` is called with it's `encoding` option set to `'buffer'`, otherwise
`filename` will be a string.

```js
// Example when handled through fs.watch listener
fs.watch('./tmp', {encoding: 'buffer'}, (eventType, filename) => {
  if (filename)
    console.log(filename);
    // Prints: <Buffer ...>
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

### Event: 'open'
<!-- YAML
added: v0.1.93
-->

* `fd` {Integer} Integer file descriptor used by the ReadStream.

Emitted when the ReadStream's file is opened.

### Event: 'close'
<!-- YAML
added: v0.1.93
-->

Emitted when the `ReadStream`'s underlying file descriptor has been closed
using the `fs.close()` method.

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
-->

Objects returned from [`fs.stat()`][], [`fs.lstat()`][] and [`fs.fstat()`][] and their
synchronous counterparts are of this type.

 - `stats.isFile()`
 - `stats.isDirectory()`
 - `stats.isBlockDevice()`
 - `stats.isCharacterDevice()`
 - `stats.isSymbolicLink()` (only valid with [`fs.lstat()`][])
 - `stats.isFIFO()`
 - `stats.isSocket()`

For a regular file [`util.inspect(stats)`][] would return a string very
similar to this:

```js
{
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
  atime: Mon, 10 Oct 2011 23:24:11 GMT,
  mtime: Mon, 10 Oct 2011 23:24:11 GMT,
  ctime: Mon, 10 Oct 2011 23:24:11 GMT,
  birthtime: Mon, 10 Oct 2011 23:24:11 GMT
}
```

Please note that `atime`, `mtime`, `birthtime`, and `ctime` are
instances of [`Date`][MDN-Date] object and to compare the values of
these objects you should use appropriate methods. For most general
uses [`getTime()`][MDN-Date-getTime] will return the number of
milliseconds elapsed since _1 January 1970 00:00:00 UTC_ and this
integer should be sufficient for any comparison, however there are
additional methods which can be used for displaying fuzzy information.
More details can be found in the [MDN JavaScript Reference][MDN-Date]
page.

### Stat Time Values

The times in the stat object have the following semantics:

* `atime` "Access Time" - Time when file data last accessed.  Changed
  by the `mknod(2)`, `utimes(2)`, and `read(2)` system calls.
* `mtime` "Modified Time" - Time when file data last modified.
  Changed by the `mknod(2)`, `utimes(2)`, and `write(2)` system calls.
* `ctime` "Change Time" - Time when file status was last changed
  (inode data modification).  Changed by the `chmod(2)`, `chown(2)`,
  `link(2)`, `mknod(2)`, `rename(2)`, `unlink(2)`, `utimes(2)`,
  `read(2)`, and `write(2)` system calls.
* `birthtime` "Birth Time" -  Time of file creation. Set once when the
  file is created.  On filesystems where birthtime is not available,
  this field may instead hold either the `ctime` or
  `1970-01-01T00:00Z` (ie, unix epoch timestamp `0`). Note that this
  value may be greater than `atime` or `mtime` in this case. On Darwin
  and other FreeBSD variants, also set if the `atime` is explicitly
  set to an earlier value than the current `birthtime` using the
  `utimes(2)` system call.

Prior to Node v0.12, the `ctime` held the `birthtime` on Windows
systems.  Note that as of v0.12, `ctime` is not "creation time", and
on Unix systems, it never was.

## Class: fs.WriteStream
<!-- YAML
added: v0.1.93
-->

`WriteStream` is a [Writable Stream][].

### Event: 'open'
<!-- YAML
added: v0.1.93
-->

* `fd` {Integer} Integer file descriptor used by the WriteStream.

Emitted when the WriteStream's file is opened.

### Event: 'close'
<!-- YAML
added: v0.1.93
-->

Emitted when the `WriteStream`'s underlying file descriptor has been closed
using the `fs.close()` method.

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
-->

* `path` {String | Buffer}
* `mode` {Integer}
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

## fs.accessSync(path[, mode])
<!-- YAML
added: v0.11.15
-->

* `path` {String | Buffer}
* `mode` {Integer}

Synchronous version of [`fs.access()`][]. This throws if any accessibility
checks fail, and does nothing otherwise.

## fs.appendFile(file, data[, options], callback)
<!-- YAML
added: v0.6.7
-->

* `file` {String | Buffer | Number} filename or file descriptor
* `data` {String | Buffer}
* `options` {Object | String}
  * `encoding` {String | Null} default = `'utf8'`
  * `mode` {Integer} default = `0o666`
  * `flag` {String} default = `'a'`
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

_Note: If a file descriptor is specified as the `file`, it will not be closed
automatically._

## fs.appendFileSync(file, data[, options])
<!-- YAML
added: v0.6.7
-->

* `file` {String | Buffer | Number} filename or file descriptor
* `data` {String | Buffer}
* `options` {Object | String}
  * `encoding` {String | Null} default = `'utf8'`
  * `mode` {Integer} default = `0o666`
  * `flag` {String} default = `'a'`

The synchronous version of [`fs.appendFile()`][]. Returns `undefined`.

## fs.chmod(path, mode, callback)
<!-- YAML
added: v0.1.30
-->

* `path` {String | Buffer}
* `mode` {Integer}
* `callback` {Function}

Asynchronous chmod(2). No arguments other than a possible exception are given
to the completion callback.

## fs.chmodSync(path, mode)
<!-- YAML
added: v0.6.7
-->

* `path` {String | Buffer}
* `mode` {Integer}

Synchronous chmod(2). Returns `undefined`.

## fs.chown(path, uid, gid, callback)
<!-- YAML
added: v0.1.97
-->

* `path` {String | Buffer}
* `uid` {Integer}
* `gid` {Integer}
* `callback` {Function}

Asynchronous chown(2). No arguments other than a possible exception are given
to the completion callback.

## fs.chownSync(path, uid, gid)
<!-- YAML
added: v0.1.97
-->

* `path` {String | Buffer}
* `uid` {Integer}
* `gid` {Integer}

Synchronous chown(2). Returns `undefined`.

## fs.close(fd, callback)
<!-- YAML
added: v0.0.2
-->

* `fd` {Integer}
* `callback` {Function}

Asynchronous close(2).  No arguments other than a possible exception are given
to the completion callback.

## fs.closeSync(fd)
<!-- YAML
added: v0.1.21
-->

* `fd` {Integer}

Synchronous close(2). Returns `undefined`.

## fs.constants

Returns an object containing commonly used constants for file system
operations. The specific constants currently defined are described in
[FS Constants][].

## fs.createReadStream(path[, options])
<!-- YAML
added: v0.1.31
-->

* `path` {String | Buffer}
* `options` {String | Object}
  * `flags` {String}
  * `encoding` {String}
  * `fd` {Integer}
  * `mode` {Integer}
  * `autoClose` {Boolean}
  * `start` {Integer}
  * `end` {Integer}

Returns a new [`ReadStream`][] object. (See [Readable Stream][]).

Be aware that, unlike the default value set for `highWaterMark` on a
readable stream (16 kb), the stream returned by this method has a
default value of 64 kb for the same parameter.

`options` is an object or string with the following defaults:

```js
{
  flags: 'r',
  encoding: null,
  fd: null,
  mode: 0o666,
  autoClose: true
}
```

`options` can include `start` and `end` values to read a range of bytes from
the file instead of the entire file.  Both `start` and `end` are inclusive and
start at 0. The `encoding` can be any one of those accepted by [`Buffer`][].

If `fd` is specified, `ReadStream` will ignore the `path` argument and will use
the specified file descriptor. This means that no `'open'` event will be
emitted. Note that `fd` should be blocking; non-blocking `fd`s should be passed
to [`net.Socket`][].

If `autoClose` is false, then the file descriptor won't be closed, even if
there's an error.  It is your responsibility to close it and make sure
there's no file descriptor leak.  If `autoClose` is set to true (default
behavior), on `error` or `end` the file descriptor will be closed
automatically.

`mode` sets the file mode (permission and sticky bits), but only if the
file was created.

An example to read the last 10 bytes of a file which is 100 bytes long:

```js
fs.createReadStream('sample.txt', {start: 90, end: 99});
```

If `options` is a string, then it specifies the encoding.

## fs.createWriteStream(path[, options])
<!-- YAML
added: v0.1.31
-->

* `path` {String | Buffer}
* `options` {String | Object}
  * `flags` {String}
  * `defaultEncoding` {String}
  * `fd` {Integer}
  * `mode` {Integer}
  * `autoClose` {Boolean}
  * `start` {Integer}

Returns a new [`WriteStream`][] object. (See [Writable Stream][]).

`options` is an object or string with the following defaults:

```js
{
  flags: 'w',
  defaultEncoding: 'utf8',
  fd: null,
  mode: 0o666,
  autoClose: true
}
```

`options` may also include a `start` option to allow writing data at
some position past the beginning of the file.  Modifying a file rather
than replacing it may require a `flags` mode of `r+` rather than the
default mode `w`. The `defaultEncoding` can be any one of those accepted by
[`Buffer`][].

If `autoClose` is set to true (default behavior) on `error` or `end`
the file descriptor will be closed automatically. If `autoClose` is false,
then the file descriptor won't be closed, even if there's an error.
It is your responsibility to close it and make sure
there's no file descriptor leak.

Like [`ReadStream`][], if `fd` is specified, `WriteStream` will ignore the
`path` argument and will use the specified file descriptor. This means that no
`'open'` event will be emitted. Note that `fd` should be blocking; non-blocking
`fd`s should be passed to [`net.Socket`][].

If `options` is a string, then it specifies the encoding.

## fs.exists(path, callback)
<!-- YAML
added: v0.0.2
deprecated: v1.0.0
-->

> Stability: 0 - Deprecated: Use [`fs.stat()`][] or [`fs.access()`][] instead.

* `path` {String | Buffer}
* `callback` {Function}

Test whether or not the given path exists by checking with the file system.
Then call the `callback` argument with either true or false.  Example:

```js
fs.exists('/etc/passwd', (exists) => {
  console.log(exists ? 'it\'s there' : 'no passwd!');
});
```

`fs.exists()` should not be used to check if a file exists before calling
`fs.open()`. Doing so introduces a race condition since other processes may
change the file's state between the two calls. Instead, user code should
call `fs.open()` directly and handle the error raised if the file is
non-existent.

## fs.existsSync(path)
<!-- YAML
added: v0.1.21
deprecated: v1.0.0
-->

> Stability: 0 - Deprecated: Use [`fs.statSync()`][] or [`fs.accessSync()`][]
> instead.

* `path` {String | Buffer}

Synchronous version of [`fs.exists()`][].
Returns `true` if the file exists, `false` otherwise.

## fs.fchmod(fd, mode, callback)
<!-- YAML
added: v0.4.7
-->

* `fd` {Integer}
* `mode` {Integer}
* `callback` {Function}

Asynchronous fchmod(2). No arguments other than a possible exception
are given to the completion callback.

## fs.fchmodSync(fd, mode)
<!-- YAML
added: v0.4.7
-->

* `fd` {Integer}
* `mode` {Integer}

Synchronous fchmod(2). Returns `undefined`.

## fs.fchown(fd, uid, gid, callback)
<!-- YAML
added: v0.4.7
-->

* `fd` {Integer}
* `uid` {Integer}
* `gid` {Integer}
* `callback` {Function}

Asynchronous fchown(2). No arguments other than a possible exception are given
to the completion callback.

## fs.fchownSync(fd, uid, gid)
<!-- YAML
added: v0.4.7
-->

* `fd` {Integer}
* `uid` {Integer}
* `gid` {Integer}

Synchronous fchown(2). Returns `undefined`.

## fs.fdatasync(fd, callback)
<!-- YAML
added: v0.1.96
-->

* `fd` {Integer}
* `callback` {Function}

Asynchronous fdatasync(2). No arguments other than a possible exception are
given to the completion callback.

## fs.fdatasyncSync(fd)
<!-- YAML
added: v0.1.96
-->

* `fd` {Integer}

Synchronous fdatasync(2). Returns `undefined`.

## fs.fstat(fd, callback)
<!-- YAML
added: v0.1.95
-->

* `fd` {Integer}
* `callback` {Function}

Asynchronous fstat(2). The callback gets two arguments `(err, stats)` where
`stats` is a [`fs.Stats`][] object. `fstat()` is identical to [`stat()`][],
except that the file to be stat-ed is specified by the file descriptor `fd`.

## fs.fstatSync(fd)
<!-- YAML
added: v0.1.95
-->

* `fd` {Integer}

Synchronous fstat(2). Returns an instance of `fs.Stats`.

## fs.fsync(fd, callback)
<!-- YAML
added: v0.1.96
-->

* `fd` {Integer}
* `callback` {Function}

Asynchronous fsync(2). No arguments other than a possible exception are given
to the completion callback.

## fs.fsyncSync(fd)
<!-- YAML
added: v0.1.96
-->

* `fd` {Integer}

Synchronous fsync(2). Returns `undefined`.

## fs.ftruncate(fd, len, callback)
<!-- YAML
added: v0.8.6
-->

* `fd` {Integer}
* `len` {Integer}
* `callback` {Function}

Asynchronous ftruncate(2). No arguments other than a possible exception are
given to the completion callback.

## fs.ftruncateSync(fd, len)
<!-- YAML
added: v0.8.6
-->

* `fd` {Integer}
* `len` {Integer}

Synchronous ftruncate(2). Returns `undefined`.

## fs.futimes(fd, atime, mtime, callback)
<!-- YAML
added: v0.4.2
-->

* `fd` {Integer}
* `atime` {Integer}
* `mtime` {Integer}
* `callback` {Function}

Change the file timestamps of a file referenced by the supplied file
descriptor.

## fs.futimesSync(fd, atime, mtime)
<!-- YAML
added: v0.4.2
-->

* `fd` {Integer}
* `atime` {Integer}
* `mtime` {Integer}

Synchronous version of [`fs.futimes()`][]. Returns `undefined`.

## fs.lchmod(path, mode, callback)
<!-- YAML
deprecated: v0.4.7
-->

* `path` {String | Buffer}
* `mode` {Integer}
* `callback` {Function}

Asynchronous lchmod(2). No arguments other than a possible exception
are given to the completion callback.

Only available on Mac OS X.

## fs.lchmodSync(path, mode)
<!-- YAML
deprecated: v0.4.7
-->

* `path` {String | Buffer}
* `mode` {Integer}

Synchronous lchmod(2). Returns `undefined`.

## fs.lchown(path, uid, gid, callback)
<!-- YAML
deprecated: v0.4.7
-->

* `path` {String | Buffer}
* `uid` {Integer}
* `gid` {Integer}
* `callback` {Function}

Asynchronous lchown(2). No arguments other than a possible exception are given
to the completion callback.

## fs.lchownSync(path, uid, gid)
<!-- YAML
deprecated: v0.4.7
-->

* `path` {String | Buffer}
* `uid` {Integer}
* `gid` {Integer}

Synchronous lchown(2). Returns `undefined`.

## fs.link(srcpath, dstpath, callback)
<!-- YAML
added: v0.1.31
-->

* `srcpath` {String | Buffer}
* `dstpath` {String | Buffer}
* `callback` {Function}

Asynchronous link(2). No arguments other than a possible exception are given to
the completion callback.

## fs.linkSync(srcpath, dstpath)
<!-- YAML
added: v0.1.31
-->

* `srcpath` {String | Buffer}
* `dstpath` {String | Buffer}

Synchronous link(2). Returns `undefined`.

## fs.lstat(path, callback)
<!-- YAML
added: v0.1.30
-->

* `path` {String | Buffer}
* `callback` {Function}

Asynchronous lstat(2). The callback gets two arguments `(err, stats)` where
`stats` is a [`fs.Stats`][] object. `lstat()` is identical to `stat()`,
except that if `path` is a symbolic link, then the link itself is stat-ed,
not the file that it refers to.

## fs.lstatSync(path)
<!-- YAML
added: v0.1.30
-->

* `path` {String | Buffer}

Synchronous lstat(2). Returns an instance of `fs.Stats`.

## fs.mkdir(path[, mode], callback)
<!-- YAML
added: v0.1.8
-->

* `path` {String | Buffer}
* `mode` {Integer}
* `callback` {Function}

Asynchronous mkdir(2). No arguments other than a possible exception are given
to the completion callback. `mode` defaults to `0o777`.

## fs.mkdirSync(path[, mode])
<!-- YAML
added: v0.1.21
-->

* `path` {String | Buffer}
* `mode` {Integer}

Synchronous mkdir(2). Returns `undefined`.

## fs.mkdtemp(prefix, callback)
<!-- YAML
added: v5.10.0
-->

Creates a unique temporary directory.

Generates six random characters to be appended behind a required
`prefix` to create a unique temporary directory.

The created folder path is passed as a string to the callback's second
parameter.

Example:

```js
fs.mkdtemp('/tmp/foo-', (err, folder) => {
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
const path = require('path');
fs.mkdtemp(tmpDir + path.sep, (err, folder) => {
  if (err) throw err;
  console.log(folder);
    // Will print something similar to `/tmp/abc123`.
    // A new temporary directory is created within
    // the /tmp directory.
});
```

## fs.mkdtempSync(prefix)
<!-- YAML
added: v5.10.0
-->

The synchronous version of [`fs.mkdtemp()`][]. Returns the created
folder path.

## fs.open(path, flags[, mode], callback)
<!-- YAML
added: v0.0.2
-->

* `path` {String | Buffer}
* `flags` {String | Number}
* `mode` {Integer}
* `callback` {Function}

Asynchronous file open. See open(2). `flags` can be:

* `'r'` - Open file for reading.
An exception occurs if the file does not exist.

* `'r+'` - Open file for reading and writing.
An exception occurs if the file does not exist.

* `'rs+'` - Open file for reading and writing in synchronous mode. Instructs
  the operating system to bypass the local file system cache.

  This is primarily useful for opening files on NFS mounts as it allows you to
  skip the potentially stale local cache. It has a very real impact on I/O
  performance so don't use this flag unless you need it.

  Note that this doesn't turn `fs.open()` into a synchronous blocking call.
  If that's what you want then you should be using `fs.openSync()`

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
created. It defaults to `0666`, readable and writable.

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

_Note: The behavior of `fs.open()` is platform specific for some flags. As such,
opening a directory on OS X and Linux with the `'a+'` flag - see example below -
will return an error. In contrast, on Windows and FreeBSD, a file descriptor
will be returned._

```js
// OS X and Linux
fs.open('<directory>', 'a+', (err, fd) => {
  // => [Error: EISDIR: illegal operation on a directory, open <directory>]
});

// Windows and FreeBSD
fs.open('<directory>', 'a+', (err, fd) => {
  // => null, <fd>
});
```

## fs.openSync(path, flags[, mode])
<!-- YAML
added: v0.1.21
-->

* `path` {String | Buffer}
* `flags` {String | Number}
* `mode` {Integer}

Synchronous version of [`fs.open()`][]. Returns an integer representing the file
descriptor.

## fs.read(fd, buffer, offset, length, position, callback)
<!-- YAML
added: v0.0.2
-->

* `fd` {Integer}
* `buffer` {String | Buffer}
* `offset` {Integer}
* `length` {Integer}
* `position` {Integer}
* `callback` {Function}

Read data from the file specified by `fd`.

`buffer` is the buffer that the data will be written to.

`offset` is the offset in the buffer to start writing at.

`length` is an integer specifying the number of bytes to read.

`position` is an integer specifying where to begin reading from in the file.
If `position` is `null`, data will be read from the current file position.

The callback is given the three arguments, `(err, bytesRead, buffer)`.

## fs.readdir(path[, options], callback)
<!-- YAML
added: v0.1.8
-->

* `path` {String | Buffer}
* `options` {String | Object}
  * `encoding` {String} default = `'utf8'`
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
-->

* `path` {String | Buffer}
* `options` {String | Object}
  * `encoding` {String} default = `'utf8'`

Synchronous readdir(3). Returns an array of filenames excluding `'.'` and
`'..'`.

The optional `options` argument can be a string specifying an encoding, or an
object with an `encoding` property specifying the character encoding to use for
the filenames passed to the callback. If the `encoding` is set to `'buffer'`,
the filenames returned will be passed as `Buffer` objects.

## fs.readFile(file[, options], callback)
<!-- YAML
added: v0.1.29
-->

* `file` {String | Buffer | Integer} filename or file descriptor
* `options` {Object | String}
  * `encoding` {String | Null} default = `null`
  * `flag` {String} default = `'r'`
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

Any specified file descriptor has to support reading.

_Note: If a file descriptor is specified as the `file`, it will not be closed
automatically._

## fs.readFileSync(file[, options])
<!-- YAML
added: v0.1.8
-->

* `file` {String | Buffer | Integer} filename or file descriptor
* `options` {Object | String}
  * `encoding` {String | Null} default = `null`
  * `flag` {String} default = `'r'`

Synchronous version of [`fs.readFile`][]. Returns the contents of the `file`.

If the `encoding` option is specified then this function returns a
string. Otherwise it returns a buffer.

## fs.readlink(path[, options], callback)
<!-- YAML
added: v0.1.31
-->

* `path` {String | Buffer}
* `options` {String | Object}
  * `encoding` {String} default = `'utf8'`
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
-->

* `path` {String | Buffer}
* `options` {String | Object}
  * `encoding` {String} default = `'utf8'`

Synchronous readlink(2). Returns the symbolic link's string value.

The optional `options` argument can be a string specifying an encoding, or an
object with an `encoding` property specifying the character encoding to use for
the link path passed to the callback. If the `encoding` is set to `'buffer'`,
the link path returned will be passed as a `Buffer` object.

## fs.readSync(fd, buffer, offset, length, position)
<!-- YAML
added: v0.1.21
-->

* `fd` {Integer}
* `buffer` {String | Buffer}
* `offset` {Integer}
* `length` {Integer}
* `position` {Integer}

Synchronous version of [`fs.read()`][]. Returns the number of `bytesRead`.

## fs.realpath(path[, options], callback)
<!-- YAML
added: v0.1.31
-->

* `path` {String | Buffer}
* `options` {String | Object}
  * `encoding` {String} default = `'utf8'`
* `callback` {Function}

Asynchronous realpath(3). The `callback` gets two arguments `(err,
resolvedPath)`. May use `process.cwd` to resolve relative paths.

Only paths that can be converted to UTF8 strings are supported. 

The optional `options` argument can be a string specifying an encoding, or an
object with an `encoding` property specifying the character encoding to use for
the path passed to the callback. If the `encoding` is set to `'buffer'`,
the path returned will be passed as a `Buffer` object.

## fs.realpathSync(path[, options])
<!-- YAML
added: v0.1.31
-->

* `path` {String | Buffer};
* `options` {String | Object}
  * `encoding` {String} default = `'utf8'`

Synchronous realpath(3). Returns the resolved path.

Only paths that can be converted to UTF8 strings are supported.

The optional `options` argument can be a string specifying an encoding, or an
object with an `encoding` property specifying the character encoding to use for
the returned value. If the `encoding` is set to `'buffer'`, the path returned
will be passed as a `Buffer` object.

## fs.rename(oldPath, newPath, callback)
<!-- YAML
added: v0.0.2
-->

* `oldPath` {String | Buffer}
* `newPath` {String | Buffer}
* `callback` {Function}

Asynchronous rename(2). No arguments other than a possible exception are given
to the completion callback.

## fs.renameSync(oldPath, newPath)
<!-- YAML
added: v0.1.21
-->

* `oldPath` {String | Buffer}
* `newPath` {String | Buffer}

Synchronous rename(2). Returns `undefined`.

## fs.rmdir(path, callback)
<!-- YAML
added: v0.0.2
-->

* `path` {String | Buffer}
* `callback` {Function}

Asynchronous rmdir(2). No arguments other than a possible exception are given
to the completion callback.

## fs.rmdirSync(path)
<!-- YAML
added: v0.1.21
-->

* `path` {String | Buffer}

Synchronous rmdir(2). Returns `undefined`.

## fs.stat(path, callback)
<!-- YAML
added: v0.0.2
-->

* `path` {String | Buffer}
* `callback` {Function}

Asynchronous stat(2). The callback gets two arguments `(err, stats)` where
`stats` is a [`fs.Stats`][] object.  See the [`fs.Stats`][] section for more
information.

## fs.statSync(path)
<!-- YAML
added: v0.1.21
-->

* `path` {String | Buffer}

Synchronous stat(2). Returns an instance of [`fs.Stats`][].

## fs.symlink(target, path[, type], callback)
<!-- YAML
added: v0.1.31
-->

* `target` {String | Buffer}
* `path` {String | Buffer}
* `type` {String}
* `callback` {Function}

Asynchronous symlink(2). No arguments other than a possible exception are given
to the completion callback. The `type` argument can be set to `'dir'`,
`'file'`, or `'junction'` (default is `'file'`) and is only available on
Windows (ignored on other platforms). Note that Windows junction points require
the destination path to be absolute. When using `'junction'`, the `target`
argument will automatically be normalized to absolute path.

Here is an example below:

```js
fs.symlink('./foo', './new-port');
```

It creates a symbolic link named "new-port" that points to "foo".

## fs.symlinkSync(target, path[, type])
<!-- YAML
added: v0.1.31
-->

* `target` {String | Buffer}
* `path` {String | Buffer}
* `type` {String}

Synchronous symlink(2). Returns `undefined`.

## fs.truncate(path, len, callback)
<!-- YAML
added: v0.8.6
-->

* `path` {String | Buffer}
* `len` {Integer}
* `callback` {Function}

Asynchronous truncate(2). No arguments other than a possible exception are
given to the completion callback. A file descriptor can also be passed as the
first argument. In this case, `fs.ftruncate()` is called.

## fs.truncateSync(path, len)
<!-- YAML
added: v0.8.6
-->

* `path` {String | Buffer}
* `len` {Integer}

Synchronous truncate(2). Returns `undefined`.

## fs.unlink(path, callback)
<!-- YAML
added: v0.0.2
-->

* `path` {String | Buffer}
* `callback` {Function}

Asynchronous unlink(2). No arguments other than a possible exception are given
to the completion callback.

## fs.unlinkSync(path)
<!-- YAML
added: v0.1.21
-->

* `path` {String | Buffer}

Synchronous unlink(2). Returns `undefined`.

## fs.unwatchFile(filename[, listener])
<!-- YAML
added: v0.1.31
-->

* `filename` {String | Buffer}
* `listener` {Function}

Stop watching for changes on `filename`. If `listener` is specified, only that
particular listener is removed. Otherwise, *all* listeners are removed and you
have effectively stopped watching `filename`.

Calling `fs.unwatchFile()` with a filename that is not being watched is a
no-op, not an error.

_Note: [`fs.watch()`][] is more efficient than `fs.watchFile()` and `fs.unwatchFile()`.
`fs.watch()` should be used instead of `fs.watchFile()` and `fs.unwatchFile()`
when possible._

## fs.utimes(path, atime, mtime, callback)
<!-- YAML
added: v0.4.2
-->

* `path` {String | Buffer}
* `atime` {Integer}
* `mtime` {Integer}
* `callback` {Function}

Change file timestamps of the file referenced by the supplied path.

Note: the arguments `atime` and `mtime` of the following related functions does
follow the below rules:

- If the value is a numberable string like `'123456789'`, the value would get
  converted to corresponding number.
- If the value is `NaN` or `Infinity`, the value would get converted to
  `Date.now()`.

## fs.utimesSync(path, atime, mtime)
<!-- YAML
added: v0.4.2
-->

* `path` {String | Buffer}
* `atime` {Integer}
* `mtime` {Integer}

Synchronous version of [`fs.utimes()`][]. Returns `undefined`.

## fs.watch(filename[, options][, listener])
<!-- YAML
added: v0.5.10
-->

* `filename` {String | Buffer}
* `options` {String | Object}
  * `persistent` {Boolean} Indicates whether the process should continue to run
    as long as files are being watched. default = `true`
  * `recursive` {Boolean} Indicates whether all subdirectories should be
    watched, or only the current directory. The applies when a directory is
    specified, and only on supported platforms (See [Caveats][]). default =
    `false`
  * `encoding` {String} Specifies the character encoding to be used for the
     filename passed to the listener. default = `'utf8'`
* `listener` {Function}

Watch for changes on `filename`, where `filename` is either a file or a
directory.  The returned object is a [`fs.FSWatcher`][].

The second argument is optional. If `options` is provided as a string, it
specifies the `encoding`. Otherwise `options` should be passed as an object.

The listener callback gets two arguments `(eventType, filename)`.  `eventType` is either
`'rename'` or `'change'`, and `filename` is the name of the file which triggered
the event.

Please note the listener callback is attached to the `'change'` event
fired by [`fs.FSWatcher`][], but they are not the same thing.

### Caveats

<!--type=misc-->

The `fs.watch` API is not 100% consistent across platforms, and is
unavailable in some situations.

The recursive option is only supported on OS X and Windows.

#### Availability

<!--type=misc-->

This feature depends on the underlying operating system providing a way
to be notified of filesystem changes.

* On Linux systems, this uses [`inotify`]
* On BSD systems, this uses [`kqueue`]
* On OS X, this uses [`kqueue`] for files and [`FSEvents`] for directories.
* On SunOS systems (including Solaris and SmartOS), this uses [`event ports`].
* On Windows systems, this feature depends on [`ReadDirectoryChangesW`].
* On Aix systems, this feature depends on [`AHAFS`], which must be enabled.

If the underlying functionality is not available for some reason, then
`fs.watch` will not be able to function. For example, watching files or
directories can be unreliable, and in some cases impossible, on network file
systems (NFS, SMB, etc), or host file systems when using virtualization software
such as Vagrant, Docker, etc.

You can still use `fs.watchFile`, which uses stat polling, but it is slower and
less reliable.

#### Inodes

<!--type=misc-->

On Linux and OS X systems, `fs.watch()` resolves the path to an [inode][] and
watches the inode. If the watched path is deleted and recreated, it is assigned
a new inode. The watch will emit an event for the delete but will continue
watching the *original* inode. Events for the new inode will not be emitted.
This is expected behavior.

#### Filename Argument

<!--type=misc-->

Providing `filename` argument in the callback is only supported on Linux and
Windows.  Even on supported platforms, `filename` is not always guaranteed to
be provided. Therefore, don't assume that `filename` argument is always
provided in the callback, and have some fallback logic if it is null.

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
-->

* `filename` {String | Buffer}
* `options` {Object}
  * `persistent` {Boolean}
  * `interval` {Integer}
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

If you want to be notified when the file was modified, not just accessed,
you need to compare `curr.mtime` and `prev.mtime`.

_Note: when an `fs.watchFile` operation results in an `ENOENT` error, it will
 invoke the listener once, with all the fields zeroed (or, for dates, the Unix
 Epoch). In Windows, `blksize` and `blocks` fields will be `undefined`, instead
 of zero. If the file is created later on, the listener will be called again,
 with the latest stat objects. This is a change in functionality since v0.10._

_Note: [`fs.watch()`][] is more efficient than `fs.watchFile` and
`fs.unwatchFile`. `fs.watch` should be used instead of `fs.watchFile` and
`fs.unwatchFile` when possible._

## fs.write(fd, buffer, offset, length[, position], callback)
<!-- YAML
added: v0.0.2
-->

* `fd` {Integer}
* `buffer` {String | Buffer}
* `offset` {Integer}
* `length` {Integer}
* `position` {Integer}
* `callback` {Function}

Write `buffer` to the file specified by `fd`.

`offset` and `length` determine the part of the buffer to be written.

`position` refers to the offset from the beginning of the file where this data
should be written. If `typeof position !== 'number'`, the data will be written
at the current position. See pwrite(2).

The callback will be given three arguments `(err, written, buffer)` where
`written` specifies how many _bytes_ were written from `buffer`.

Note that it is unsafe to use `fs.write` multiple times on the same file
without waiting for the callback. For this scenario,
`fs.createWriteStream` is strongly recommended.

On Linux, positional writes don't work when the file is opened in append mode.
The kernel ignores the position argument and always appends the data to
the end of the file.

## fs.write(fd, data[, position[, encoding]], callback)
<!-- YAML
added: v0.11.5
-->

* `fd` {Integer}
* `data` {String | Buffer}
* `position` {Integer}
* `encoding` {String}
* `callback` {Function}

Write `data` to the file specified by `fd`.  If `data` is not a Buffer instance
then the value will be coerced to a string.

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
-->

* `file` {String | Buffer | Integer} filename or file descriptor
* `data` {String | Buffer}
* `options` {Object | String}
  * `encoding` {String | Null} default = `'utf8'`
  * `mode` {Integer} default = `0o666`
  * `flag` {String} default = `'w'`
* `callback` {Function}

Asynchronously writes data to a file, replacing the file if it already exists.
`data` can be a string or a buffer.

The `encoding` option is ignored if `data` is a buffer. It defaults
to `'utf8'`.

Example:

```js
fs.writeFile('message.txt', 'Hello Node.js', (err) => {
  if (err) throw err;
  console.log('It\'s saved!');
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

_Note: If a file descriptor is specified as the `file`, it will not be closed
automatically._

## fs.writeFileSync(file, data[, options])
<!-- YAML
added: v0.1.29
-->

* `file` {String | Buffer | Integer} filename or file descriptor
* `data` {String | Buffer}
* `options` {Object | String}
  * `encoding` {String | Null} default = `'utf8'`
  * `mode` {Integer} default = `0o666`
  * `flag` {String} default = `'w'`

The synchronous version of [`fs.writeFile()`][]. Returns `undefined`.

## fs.writeSync(fd, buffer, offset, length[, position])
<!-- YAML
added: v0.1.21
-->

* `fd` {Integer}
* `buffer` {String | Buffer}
* `offset` {Integer}
* `length` {Integer}
* `position` {Integer}

## fs.writeSync(fd, data[, position[, encoding]])
<!-- YAML
added: v0.11.5
-->

* `fd` {Integer}
* `data` {String | Buffer}
* `position` {Integer}
* `encoding` {String}

Synchronous versions of [`fs.write()`][]. Returns the number of bytes written.

## FS Constants

The following constants are exported by `fs.constants`. **Note:** Not every
constant will be available on every operating system.

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

[`Buffer.byteLength`]: buffer.html#buffer_class_method_buffer_bytelength_string_encoding
[`Buffer`]: buffer.html#buffer_buffer
[Caveats]: #fs_caveats
[`fs.access()`]: #fs_fs_access_path_mode_callback
[`fs.accessSync()`]: #fs_fs_accesssync_path_mode
[`fs.appendFile()`]: fs.html#fs_fs_appendfile_file_data_options_callback
[`fs.exists()`]: fs.html#fs_fs_exists_path_callback
[`fs.fstat()`]: #fs_fs_fstat_fd_callback
[`fs.FSWatcher`]: #fs_class_fs_fswatcher
[`fs.futimes()`]: #fs_fs_futimes_fd_atime_mtime_callback
[`fs.lstat()`]: #fs_fs_lstat_path_callback
[`fs.mkdtemp()`]: #fs_fs_mkdtemp_prefix_callback
[`fs.open()`]: #fs_fs_open_path_flags_mode_callback
[`fs.read()`]: #fs_fs_read_fd_buffer_offset_length_position_callback
[`fs.readFile`]: #fs_fs_readfile_file_options_callback
[`fs.stat()`]: #fs_fs_stat_path_callback
[`fs.Stats`]: #fs_class_fs_stats
[`fs.statSync()`]: #fs_fs_statsync_path
[`fs.utimes()`]: #fs_fs_futimes_fd_atime_mtime_callback
[`fs.watch()`]: #fs_fs_watch_filename_options_listener
[`fs.write()`]: #fs_fs_write_fd_buffer_offset_length_position_callback
[`fs.writeFile()`]: #fs_fs_writefile_file_data_options_callback
[`net.Socket`]: net.html#net_class_net_socket
[`ReadStream`]: #fs_class_fs_readstream
[`stat()`]: fs.html#fs_fs_stat_path_callback
[`util.inspect(stats)`]: util.html#util_util_inspect_object_options
[`WriteStream`]: #fs_class_fs_writestream
[MDN-Date-getTime]: https://developer.mozilla.org/en/JavaScript/Reference/Global_Objects/Date/getTime
[MDN-Date]: https://developer.mozilla.org/en/JavaScript/Reference/Global_Objects/Date
[Readable Stream]: stream.html#stream_class_stream_readable
[Writable Stream]: stream.html#stream_class_stream_writable
[inode]: http://www.linux.org/threads/intro-to-inodes.4130
[FS Constants]: #fs_fs_constants
[`inotify`]: http://man7.org/linux/man-pages/man7/inotify.7.html
[`kqueue`]: https://www.freebsd.org/cgi/man.cgi?kqueue
[`FSEvents`]: https://developer.apple.com/library/mac/documentation/Darwin/Conceptual/FSEvents_ProgGuide/Introduction/Introduction.html#//apple_ref/doc/uid/TP40005289-CH1-SW1
[`event ports`]: http://illumos.org/man/port_create
[`ReadDirectoryChangesW`]: https://msdn.microsoft.com/en-us/library/windows/desktop/aa365465%28v=vs.85%29.aspx
[`AHAFS`]: https://www.ibm.com/developerworks/aix/library/au-aix_event_infrastructure/

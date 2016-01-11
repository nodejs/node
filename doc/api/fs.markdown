# File System

    Stability: 2 - Stable

<!--name=fs-->

A core capability of Node.js is the ability to interact with the file system.
This is accomplished via a set of simple wrappers around standard POSIX
functions implemented internally by the `libuv` library. These functions are
exposed by the `fs` module.

    const fs = require('fs');
    
    fs.rename('/tmp/hello', '/tmp/goodbye', (err) => {
      if (err) throw err;
      console.log('file renamed!');
    });

## Asynchronous vs. Synchronous I/O

Every I/O operation provided by the `fs` module is available in asynchronous
and synchronous forms.

With a few exceptions, the asynchronous forms follow the idiomatic Node.js
callback pattern in which methods take a callback function as the last
argument. When the operation completes or when an error occurs, the callback is
called. The first argument is generally always either an `Error` object or
`null` if the operation completed successfully. Additional arguments can be
passed to the callback depending on the method that was called. This pattern is
illustrated in the `fs.rename()` example above.

In the synchronous form, exceptions are immediately thrown and can be handled
using JavaScript's `try/catch`:

    const fs = require('fs');
    try {
      fs.renameSync('/tmp/hello', '/tmp/goodbye');
    } catch (err) {
      // Whoops, an error occurred
    }

When using asynchronous methods, there is no guaranteed order in which the I/O
operations will be completed. The following, for instance, is prone to error
because the call to `fs.stat()` could complete before the call to `fs.rename()`:

    fs.rename('/tmp/hello', '/tmp/world', (err) => {
      if (err) throw err;
      console.log('renamed complete');
    });
    fs.stat('/tmp/world', (err, stats) => {
      if (err) throw err;
      console.log(`stats: ${JSON.stringify(stats)}`);
    });

The correct way to accomplish the appropriate order of operations is to "chain"
the callback functions by calling `fs.stat()` within the callback function
passed to `fs.rename()`:

    fs.rename('/tmp/hello', '/tmp/world', (err) => {
      if (err) throw err;
      fs.stat('/tmp/world', (err, stats) => {
        if (err) throw err;
        console.log(`stats: ${JSON.stringify(stats)}`);
      });
    });

In busy processes, it is _strongly recommended_ to use the asynchronous I/O
methods as the synchronous versions will block the Node.js event loop until
they complete.

Most asynchronous functions allow the callback argument to be omitted. When
done so, a default callback is used that re-throws any errors that occur. This
is problematic because the reported stack trace will misreport the call site
where the actual error was thrown. To get a trace to the original call site,
set the `NODE_DEBUG` environment variable:

    $ cat script.js
    function bad() {
      require('fs').readFile('/');
    }
    bad();

    $ env NODE_DEBUG=fs node script.js
    fs.js:66
            throw err;
                  ^
    Error: EISDIR, read
        at rethrow (fs.js:61:21)
        at maybeCallback (fs.js:79:42)
        at Object.fs.readFile (fs.js:153:18)
        at bad (/path/to/script.js:2:17)
        at Object.<anonymous> (/path/to/script.js:5:1)
        <etc.>

## File paths and File descriptors

Most `fs` methods allow the use of relative or absolute file paths. When
relative paths are used, they are resolved relative to the current working
directory as reported by `process.cwd()`.

Some variants of the `fs` methods accept a file descriptor (fd) rather than
a path. The file descriptor itself is a simple integer ID that references an
internally managed open file handle. These are managed using the `fs.open()`
and `fs.close()` methods. In most cases, use of relative or absolute file
system paths will be sufficient.

Node.js interprets file system paths as UTF-8 character strings and there is
currently no built in mechanism for supporting file systems that use other
character encodings by default. On most systems assuming a default of UTF-8
is safe but issues have been encountered with filenames that use other
encodings. For more information, refer to the
[Working with Different Filesystems][] guide.

## File mode constants

Every file has a 'mode' that describes the permissions a user has on the file.  
The following constants define the possible modes.

- `fs.F_OK` - File is visible to the calling process. This is useful for
determining if a file exists, but says nothing about `rwx` permissions.
Default if no `mode` is specified.
- `fs.R_OK` - File can be read by the calling process.
- `fs.W_OK` - File can be written by the calling process.
- `fs.X_OK` - File can be executed by the calling process. This has no effect
on Windows (will behave like `fs.F_OK`).

These can be combined using a bit field. For instance, `fs.R_OK | fs.W_OK`
indicates that the file can be both written to and read by the calling
processes.

## Checking file permissions

The two methods `fs.access()` and `fs.accessSync()` test that the current
process has permission to access the file specified by `path`. `mode` is a bit
field with a combination of one or more [File mode constants][].

- `fs.access(path[, mode], callback)`
- `fs.accessSync(path[, mode])`

When the asynchronous `fs.access()` call completes, the `callback` function
will be called with a single `err` argument. If the current process has the
specified permissions, `err` will be `null`; otherwise `err` will be an `Error`
object.

    const fs = require('fs');
    fs.access('/etc/passwd', fs.R_OK | fs.W_OK, (err) => {
      console.log(err ? 'no access!' : 'can read/write');
    });

When the synchronous `fs.accessSync()` method completes successfully, it will
simply return. If the current process does not have the specified permissions,
an `Error` will be thrown that can be caught using `try/catch`.

    const fs = require('fs');
    try {
      fs.accessSync('/etc/passwd', fs.R_OK | fs.W_OK);
      console.log('can read/write');
    } catch(err) {
      console.log('no access!');
    }

## Checking to see if a file exists

The `fs.exists()`, `fs.stat()`, `fs.lstat()`, and `fs.fstat()` methods can
each be used to determine if a file exists.

### `fs.stat()`, `fs.lstat()`, and `fs.fstat()`

The `fs.stat()`, `fs.lstat()`, and `fs.fstat()` methods each create `fs.Stats`
objects describing a path or file descriptor (fd).

- `fs.stat(path, callback)`
- `fs.lstat(path, callback)`
- `fs.fstat(fd, callback)`
- `fs.statSync(path)`
- `fs.lstatSync(path)`
- `fs.fstatSync(fd)`

For example:

    const fs = require('fs');
    fs.stat('/tmp/example', (err, stats) => {
      if (err) throw err;
      console.log(JSON.stringify(stats,null,2));
    });

The variants differ in the way the first argument passed to the function
is handled. For `fs.stat()`, `fs.lstat()`, `fs.statSync()` and
`fs.lstatSync()`, the first argument is a file system path. Only `fs.stat()`
and `fs.statSync()` will follow symbolic links passed as the `path`. For
`fs.fstat()` and `fs.fstatSync()`, the first argument must be a file descriptor.

When the asynchronous `fs.stat()`, `fs.lstat()`, and `fs.fstat()` methods
complete, the `callback` function is called and passed two arguments: `err`
and `stats`. If the call was successful, `err` will be `null` and `stats` will
be an instance of the [`fs.Stats`][] object; otherwise `err` will be an `Error`
object and `stats` will be `undefined`.

When the synchronous `fs.statSync()`, `fs.lstatSync()`, and `fs.fstatSync()`
methods complete successfully, an `fs.Stats` object is returned. Any errors
that occur will be throw and can be caught using `try/catch`.

    const fs = require('fs');
    try {
      var stats = fs.statsSync('/tmp/example');
      console.log(JSON.stringify(stats,null,2));
    } catch (err) {
      console.log('whoops! could not get the file stats!');
    }

### `fs.exists()` and `fs.existsSync()`

    Stability: 0 - Deprecated: Use [`fs.stat()`][], [`fs.access()`][],
    [`fs.statSync()`][], or [`fs.accessSync()`][] instead.

The `fs.exists()` and `fs.existsSync()` methods test whether the given `path`
exists.

- `fs.exists(path, callback)`
- `fs.existsSync(path)`

For example:

    const fs = require('fs');
    fs.exists('/etc/passwd', (exists) => {
      console.log(exists ? 'it\'s there' : 'no passwd!');
    });

The `path` is a file system path.

When the asynchronous `fs.exists()` completes, the `callback` function is
called and passed a single boolean `exists` argument. The value is `true`
if the file exists, `false` if it does not.

When the synchronous `fs.existsSync()` method completes, a boolean will be
returned: `true` if the file exists, `false` if it does not.

    const fs = require('fs');
    console.log(
      fs.existsSync('/etc/passwd') ?
        'it\'s there' : 'no /etc/passwd');

### Avoiding race conditions

A race condition can be introduced by checking for the existence of a
file before attempting to open and read it because other processes may alter
the file's state between the time the existence check completes and the
attempt to read the file. A more reliable approach is to simply open the
file directly and handle any errors raised in the case the file does not
exist.

## Changing file permissions

The `fs.chmod()`, `fs.fchmod()`, `fs.lchmod()`, `fs.chmodSync()`,
`fs.fchmodSync()`, and `fs.lchmodSync()` methods modify the access
permissions for the specified path or file descriptor.

- `fs.chmod(path, mode, callback)`
- `fs.lchmod(path, mode, callback)`
- `fs.fchmod(fd, mode, callback)`
- `fs.chmodSync(path, mode)`
- `fs.lchmodSync(path, mode)`
- `fs.fchmodSync(fd, mode)`

For example:

    const fs = require('fs');
    fs.chmod('/tmp/example', fs.R_OK | fs.W_OK, (err) => {
      console.log(err ? 'chmod failed' : 'permissions changed!');
    });

The variants differ in the way the first argument passed to the function
are handled. For `fs.chmod()`, `fs.lchmod()`, `fs.chmodSync()`, and
`fs.lchmodSync()`, the first argument is a file system path. Only `fs.chmod()`
and `fs.chmodSync()` will follow symbolic links. For `fs.fchmod()` and
`fs.fchmodSync()`, the first argument must be a file descriptor.

The `mode` is a bit field with a combination of one or more
[File mode constants][].

When the asynchronous `fs.chmod()`, `fs.lchmod()` and `fs.fchmod()` methods
complete, the `callback` function will be called and passed a single `err`
argument. If the permissions are changed successfully, `err` will be `null`;
otherwise `err` will be an `Error` object.

When the synchronous `fs.chmodSync()`, `fs.lchmodSync()`, and `fs.fchmodSync()`
methods complete successfully, they simply return. Any errors that occur while
attempting to set the permissions are thrown and can be caught using
`try/catch`.

    const fs = require('fs');
    try {
      fs.chmodSync('/tmp/example', fs.R_OK | fs.W_OK);
      console.log('permissions changed!');
    } catch (err) {
      console.log('chmod failed!');
    }

## Changing file ownership (`chown`, `fchown`, and `lchown`)

The `fs.chown()`, `fs.fchown()`, `fs.lchown()`, `fs.chownSync()`,
`fs.fchownSync()`, and `fs.lchownSync()` methods change the user and group
ownership of a path or file descriptor.

- `fs.chown(path, uid, gid, callback)`
- `fs.lchown(path, uid, gid, callback)`
- `fs.fchown(fd, uid, gid, callback)`
- `fs.chownSync(path, uid, gid)`
- `fs.lchownSync(path, uid, gid)`
- `fs.fchownSync(fd, uid, gid)`

For example:

    fs.chown('/tmp/example', 1, 1, (err) => {
      console.log(err ? 'chown failed!' : 'ownership changed!');  
    });

The variants differ in the way the first argument passed to the function
is handled. For `fs.chown()`, `fs.lchown()`, `fs.chownSync()`, and
`fs.lchownSync()`, the first argument is a file system path. Only `fs.chown()`
and `fs.chownSync()` will follow symbolic links. For `fs.fchown()` and
`fs.fchownSync()`, the first argument must be a file descriptor.

The `uid` and `gid` specify the new user ID and group ID to assign to the file.

When the asynchronous `fs.chown()`, `fs.lchown()` and `fs.fchown()` methods
complete, the `callback` function will be called and passed a single `err`
argument. If the ownership was modified successfully, `err` will be `null`;
otherwise `err` will be an `Error` object.

When the synchronous `fs.chownSync()`, `fs.lchownSync()`, and `fs.fchownSync()`
methods complete successfully, they simply return. Any errors that occur while
attempting to change the ownership will be thrown and can be caught using
`try/catch`.

    const fs = require('fs');
    try {
      fs.chownSync('/tmp/example', 1, 1);
      console.log('ownership changed!');
    } catch (err) {
      console.log('chown failed!');
    }

## Opening and Closing file descriptors

All file system I/O operations are performed using file descriptors. The
`fs` module functions that take a file path will internally open file
descriptors for the I/O resources located at those paths then close those
automatically when the operation is complete.

The `fs.open()`, `fs.close()`, `fs.openSync()`, and `fs.closeSync()` methods
provide the ability to work with file descriptors directly.

### `fs.open()` and `fs.openSync()`

The `fs.open()` and `fs.openSync()` methods are used to open file descriptors.

- `fs.open(path, flags[, mode], callback)`
- `fs.openSync(path, flags[, mode])`

For example:

    const fs = require('fs');
    fs.open('/tmp/example', 'r', (err, fd) => {
      if (err) throw err;
      console.log(`The file was opened as ${fd}`);
      fs.close(fd);
    });

The `path` is the absolute or relative path to the file to open.

The `flags` argument is typically one of the following string values that
indicate how the file descriptor should be opened:

- `'r'` - Open file for reading. An exception occurs if the file does not exist.
- `'r+'` - Open file for reading and writing. An exception occurs if the file
  does not exist.
- `'rs'` - Open file for reading in synchronous mode. Instructs the operating
  system to bypass the local file system cache. This is primarily useful for
  opening files on NFS mounts as it allows you to skip the potentially stale
  local cache. However, using `'rs'` has a very real impact on I/O performance
  so this flag should be used only when necessary. *Note that this does not
  turn `fs.open()` into a synchronous blocking call. If synchronous open is
  required, then `fs.openSync()` should be used instead.*
- `'rs+'` - Open file for reading and writing, telling the operating system to
  open it synchronously. See notes for `'rs'` about using this with caution.
- `'w'` - Open file for writing. The file is created (if it does not exist) or
  truncated (if it exists).
- `'wx'` - Like `'w'` but fails if `path` exists.
- `'w+'` - Open file for reading and writing. The file is created (if it does
  not exist) or truncated (if it exists).
- `'wx+'` - Like `'w+'` but fails if `path` exists.
- `'a'` - Open file for appending. The file is created if it does not exist.
- `'ax'` - Like `'a'` but fails if `path` exists.
- `'a+'` - Open file for reading and appending. The file is created if it does
  not exist.
- `'ax+'` - Like `'a+'` but fails if `path` exists.
- `'x'` - Ensures that `path` is newly created. On POSIX systems, `path` is
  considered to exist even if it is a symlink to a non-existent file. The
  exclusive flag may or may not work with network file systems. The
  `'x'` flag is equivalent to the `O_EXCL` flag in POSIX open(2)).

The `flags` argument can also be a number as documented by open(2); commonly
used constants are available using the `constants` module
(`require('constants')`).  On Windows, flags are translated to their
Windows-specific equivalents where applicable, e.g. `O_WRONLY` is translated to
`FILE_GENERIC_WRITE`, while `O_EXCL|O_CREAT` is translated to `CREATE_NEW`, as
accepted by the Windows `CreateFileW` API.

The `mode` argument sets the file mode (permission and sticky bits), but only
if the file is created. The default mode is `0o666` (readable and writeable).
See [File mode constants][] for more information.

When the asynchronous `fs.open()` method completes, the `callback` function
is called and passed two arguments: `err` and `fd`. If the file was opened
successfully, `err` will be `null` and `fd` will be the opened file descriptor;
otherwise `err` will be an `Error` object and `fd` will be `undefined`.

When the synchronous `fs.openSync()` completes successfully, the method will
return the opened file descriptor. Any error that occurs while attempting to
open the file will be thrown and can be caught using `try/catch`.

    const fs = require('fs');
    var fd;
    try {
      fd = fs.openSync('/tmp/example', 'r');
      console.log('the file was opened!');
    } catch (err) {
      console.log('whoops! there was an error opening the file!');
    } finally {
      if (fd) fs.closeSync(fd);
    }

### `fs.close()` and `fs.closeSync()`

The `fs.close()` and `fs.closeSync()` methods close file descriptors previously
opened using the `fs.open()` or `fs.openSync()` methods.

- `fs.close(fd, callback)`
- `fs.closeSync(fd)`

For example:

    const fs = require('fs');
    fs.open('/tmp/example', 'r', (err, fd) => {
      if (err) throw err;
      console.log('file opened!');
      fs.close(fd, (err) => {
        if (err) throw err;
        console.log('file closed!');
      });
    });
    
When the asynchronous `fs.close()` completes, the `callback` function is called
and passed a single `err` argument. If closing the file was successful, `err`
will be `null`; otherwise `err` will be an `Error object`.

When the synchronous `fs.closeSync()` completes successfully, the method simply
returns. Any errors that occur while attempting to close the file descriptor
will be thrown and can be caught using `try/catch`.

    const fs = require('fs');
    try {
      var fd = fs.openSync('/tmp/example', 'r');
      fs.closeSync(fd);
    } catch (err) {
      console.log('there was an error!');
    }

## Reading and Writing files using streams

Readable and Writable streams are the primary mechanism for reading data from,
and writing data to, the file system.

### `fs.createReadStream(path[, options])`

The `fs.createReadStream()` method creates and returns a new
[`fs.ReadStream`][] object that can be used to read data from the file
system. See [Readable Streams][] for details.

    const fs = require('fs');
    const str = fs.createReadStream('/tmp/example', 'utf8');
    str.on('error', (err) => {
      console.error('There was an error reading from the file');  
    });
    str.on('readable', () => {
      var data = str.read();
      if (data) console.log(`Data: ${data}`);
    });

The `path` argument identifies the file to be read.

The optional `options` argument is used to specify properties that define
how the returned `fs.ReadStream` object operations. The value can be either
an object or a string.

- If `options` is a string, it specifies the character encoding to use when
  reading the file data.
- If `options` is an object, it may have the following properties:
  - `start` / `end`: specify a range of bytes to read from the file as opposed
    to reading the entire file. Both the `start` and `end` are inclusive offset
    positions that start at `0`.
  - `encoding` : the character encoding applied to the `ReadableStream`. Any
    encoding supported by [`Buffer`][] can be used.
  - `flags`: the specific flags to be used when the file descriptor is opened
    (See `fs.open()` or `fs.openSync()` for details).
  - `fd`: an open file descriptor. When specified, the `path` argument will be
     ignored and no `'open'` event will be emitted by the returned
     `fs.ReadStream`. Only blocking file descriptors should be used.
     Non-blocking file descriptors should be passed to [`net.Socket`][].
  - `autoClose`: When `true`, the `fs.ReadStream` will automatically close the
    file descriptor when the `fs.ReadStream` is done reading the file data. This
    is the default behavior. When `false`, the file descriptor will not be
    closed even if there is an error. It is the developer's responsibility to
    close the file descriptor to protect against possible memory leaks.
  - `mode`: sets the file mode (permission and sticky bits) of the file. Only
    applies if the `fs.createReadStream()` method creates the file. See
    [File mode constants][] for more information.

If `options` is not specified, it defaults to an object with the following
properties:

    {
      flags: 'r',
      encoding: null,
      fd: null,
      mode: 0o666,
      autoClose: true
    }

Like all instances of [`ReadableStream`][], the `fs.ReadStream` object returned
by `fs.createReadStream()` has a `highWaterMark` property that controls how
much data can be read from the stream at any given time. However, unlike most
`ReadableStream` instances that use a 16 kb `highWaterMark`, `fs.ReadStream`
instances uses a 64 kb `highWaterMark`.

### `fs.createWriteStream(path[, options])`

The `fs.createWriteStream()` method creates and returns a new
[`fs.WriteStream`][] object used to write data to the file system.
See [Writable Streams][] for details.

    const fs = require('fs');
    const str = fs.createWriteStream('/tmp/example');
    str.write('this is the file content');
    str.end();

The `path` argument defines how the returned `fs.WriteStream` operations. It's
value can be either an object or a string.

- If `options` is a string, it specifies the character encoding to use when
  writing the file data.
- If `options` is an object, it may have the following properties:
  - `start`: specifies an offset position past the beginning of the file where
    writing should begin.
  - `defaultEncoding` : the character encoding applied to the `WritableStream`.
    Any encoding supported by [`Buffer`][] can be used.
  - `flags`: the specific flags to be used when the file descriptor is opened
    (See `fs.open()` or `fs.openSync()` for details). Modifying a file rather
    than replacing it may require a `flags` mode of `r+` rather than the default
    mode `w`.
  - `fd`: an open file descriptor. When specified, the `path` argument will be
     ignored and no `'open'` event will be emitted by the returned
     `fs.WriteStream`. Only blocking file descriptors should be used.
     Non-blocking file descriptors should be passed to [`net.Socket`][].
  - `mode`: sets the file mode (permission and sticky bits) of the file. Only
    applies if the `fs.createWriteStream()` method creates the file. See
    [File mode constants][] for more information.

If `options` is not specified, it defaults to an object with the following
properties:

    {
      flags: 'w',
      defaultEncoding: 'utf8',
      fd: null,
      mode: 0o666
    }

## Reading and Writing files without streams

The `fs` module provides a number of additional low-level methods for reading
from, and writing data to files.

### `fs.appendFile()` and `fs.appendFileSync()`

The `fs.appendFile()` and `fs.appendFileSync()` methods append data to a
specified file, creating it if it does not yet exist.

- `fs.appendFile(file, data[, options], callback)`
- `fs.appendFileSync(file, data[, options])`

For example:

    const fs = require('fs');
    fs.appendFile('/tmp/example', 'data to append', (err) => {
      if (err) throw err;
      console.log('The "data to append" was appended to file!');
    });

The `file` is either a path or a file descriptor. The `data` can be a string
or a [`Buffer`][].

The optional `options` argument can be either a string or an object.

- If `options` is a string, then it specifies the character encoding of the
  `data`.
- If `options` is an object is may have the following properties:
  - `encoding`: the character encoding of the `data`.
  - `mode`: sets the file mode (permission and sticky bits) of the file. Only
    applies if the `fs.createWriteStream()` method creates the file. See
    [File mode constants][] for more information.
  - `flag`: the specific flags to be used when the file descriptor is opened
    (See `fs.open()` or `fs.openSync()` for details).
    
If `options` is not specified, it defaults to an object with the following
properties:

    {
      encoding: null,
      mode: 0o666,
      flag: 'a'
    }

If `file` is passed as a file descriptor, that fd has to have been
previously opened for appending (using `fs.open()` or `fs.openSync()`).
It is important to note such file descriptors will not be closed
automatically.

When the asynchronous `fs.appendFile()` completes, the `callback` function will
be called with a single `err` argument. If the append operation was successful,
`err` will be `null`; otherwise the `err` argument will be an `Error` object.

When the synchronous `fs.appendFileSync()` completes successfully, the method
simply returns. Any errors that occur while attempting to append to the file
will be thrown and can be caught using `try/catch`.

    const fs = require('fs');
    try {
      fs.appendFileSync('/tmp/example', 'data to append');
      console.log('data written');
    } catch (err) {
      console.log('whoops, there was an error');
    }

### `fs.read()` and `fs.readSync()`

The `fs.read()` and `fs.readSync()` methods read data from the file system.

- `fs.read(fd, buffer, offset, length, position, callback)`
- `fs.readSync(fd, buffer, offset, length, position)`

For example:

    const fs = require('fs');
    fs.open('/tmp/example', 'r', (err, fd) => {
      if (err) throw err;
      fs.read(fd, new Buffer(100), 0, 100, 0, (err, read, buffer) => {
        if (err) throw err;
        console.log(read);
        console.log(buffer.slice(0,read).toString());
      });
    });

The first argument must be a file descriptor (`fd`). The `buffer` argument is a
[`Buffer`][] instance that the data will be written to.

The `offset` and `length` arguments are the offset in the buffer to start
writing at and the number of bytes to read, respectively.

The `position` argument is an integer specifying an offset position where to
begin reading data in from the file. If `position` is `null`, data will be
read from the current file position as maintained internally by the `fs`
module.

When the asynchronous `fs.read()` method completes, the `callback` function is
called and passed three arguments: `err`, `bytesRead`, and `buffer`. If the
read was successful, `err` will be `null`, `bytesRead` will be the total number
of bytes read from the file, and `buffer` is a reference to the `Buffer` object
to which the data was written; otherwise `err` will be an `Error` object and
`bytesRead` and `buffer` will each be `undefined`.

When the synchronous `fs.ReadSync()` method completes, the number of bytes
read into the passed in `Buffer` instance will be returned. Any errors that
occur during the read operation will be thrown and can be caught using
`try/catch`.

    const fs = require('fs');
    var fd;
    try {
      fd = fs.openSync('/tmp/example', 'r');
      var buffer = new Buffer(100);
      var bytes = fs.readSync(fd, buffer, 0, 100, 0);
      console.log(bytes);
      console.log(buffer.slice(0,bytes).toString());
    } catch(err) {
      console.error(err);
      console.log('whoops! there was an error reading the data');
    } finally {
      if (fd) fs.closeSync(fd);
    }
 
### `fs.readFile()` and `fs.readFileSync()`

The `fs.readFile()` and `fs.readFileSync()` methods read the entire contents
of a file.
 
- `fs.readFile(file[, options], callback)`
- `fs.readFileSync(file[, options])`

For example:

    const fs = require('fs');
    fs.readFile('/etc/passwd', (err, data) => {
      if (err) throw err;
      console.log(data);
    });

The `file` is the path of the file to read.

The optional `options` argument can be either a string or object.

- If `options` is a string, it's value specifies the character encoding to use
  when reading the file data.
- If `options` is an object it may have the following properties:
  - `encoding`: specifies the character encoding to use when reading the data
  - `flag`: the specific flags to be used when the file descriptor is opened
    (See `fs.open()` or `fs.openSync()` for details).

If `options` is not specified, it defaults to an object with the following
properties:

    {
      encoding: null,
      flag: 'r'
    }

If an `encoding` is specified, the data is read from the file as a string. If
the `encoding` is not specified, the data is read into a new `Buffer` instance.

When the asynchronous `fs.readFile()` method completes, the `callback` function
will be called and passed two arguments: `err` and `data`. If the read
operation completed successfully, `err` will be `null` and `data` will be
either a string or a `Buffer`. If the read operation failed, `err` will be
an `Error` object and `data` will be `undefined`.

When the synchronous `fs.readFileSync()` method completes, the method returns
either a string or a `Buffer`. Any errors that occur during the read
operation will be thrown and can be caught using `try/catch`.

    const fs = require('fs');
    try {
      const data = fs.readFileSync('/etc/passwd', 'ascii');
      console.log(data);
    } catch (err) {
      console.log('whoops, there was an error reading the data!');
    }

### `fs.truncate()`, `fs.truncateSync()`, `fs.ftruncate()` and `fs.ftruncateSync()`

The `fs.truncate()` and `fs.truncateSync()` methods truncate a file to a
given length.

- `fs.truncate(file, len, callback)`
- `fs.truncateSync(file, len)`
- `fs.ftruncate(fd, len, callback)`
- `fs.ftruncateSync(fd, len)`

For example:

    const fs = require('fs');
    fs.truncate('/tmp/example', 2, (err) => {
      if (err) throw err;
      console.log('file truncated to 2 bytes');
    });

The `file` argument can be a path or an open and writable file descriptor. The
`len` argument is the number of bytes to truncate the file too.

In the `fs.ftruncate()` and `fs.ftruncateSync()` variations, the first `fd`
argument must be an open and writable file descriptor (see `fs.open()` for
details). Otherwise, these variations operate identically to `fs.truncate()`
and `fs.truncateSync()`.

For example:

    const fs = require('fs');
    fs.open('/tmp/example', 'w', (err, fd) => {
      if (err) throw err;
      fs.ftruncate(fd, 2, (err) => {
        if (err) throw err;
        console.log('file truncated to 2 bytes');
      });      
    });

When the asynchronous `fs.truncate()` method completes, the `callback` function
is called and passed a single `err` argument. If the truncate operation was
successful, `err` will be `null`; otherwise `err` will be an `Error` object.

If the synchronous `fs.truncateSync()` completes successfully, the method
simply returns. Any errors that occur during the truncate operation will be
thrown and can be caught using `try/catch`.

    const fs = require('fs');
    try {
      fs.truncateSync('/tmp/example', 2);
      console.log('file truncated to 2 bytes');
    } catch (err) {
      console.log('whoops! there was an error truncating the file');
    }

### `fs.write()` and `fs.writeSync()`

The `fs.write()` and `fs.writeSync()` methods write data to the file system.

- `fs.write(fd, buffer, offset, length[, position], callback)`
- `fs.write(fd, data[, position[, encoding]], callback)`
- `fs.writeSync(fd, buffer, offset, length[, position])`
- `fs.writeSync(fd, data[, position[, encoding]])`

There are two distinct ways of calling both the `fs.write()` and
`fs.writeSync()` methods. The first uses a `Buffer` as the source of the data  
and allows `offset` and `length` arguments to be passed in specifying a
specific range of data from the input to be written out. The second uses
strings for the data and allows a character `encoding` to be specified, but does
not provide the `offset` and `length` options.

#### Writing using `Buffer`, `offset` and `length`

- `fs.write(fd, buffer, offset, length[, position], callback)`
- `fs.writeSync(fd, buffer, offset, length[, position])`

For example:

    const fs = require('fs');
    fs.open('/tmp/example', 'w', (err, fd) => {
      if (err) throw err;
      fs.write(fd, new Buffer('the data'), 0, 8, 0, (err, bw, buf) => {
        if (err) throw err;
        console.log(`${bw} bytes written`);
      });
    });

The `fd` argument must be an open and writable file descriptor (see `fs.open()`
for more details).

The `buffer` argument is a [`Buffer`][] instance that contains the data that is
to be written to the file.

The `offset` and `length` arguments specify the offset position within the
`buffer` where the  data to be written begins and the total number of bytes to
write, respectively.

The `position` argument refers to the offset from the beginning of the file
where the data from `buffer` should be written. If the value of `position` is
not a number, the data will be written at the current position as maintained
internally by the `fs` module.

When the asynchronous `fs.write()` completes, the `callback` function will be
called and passed three arguments: `err`, `written`, and `buffer`. If the
write operation was successful, `err` will be `null`, `written` will indicate
the number of bytes written, and `buffer` will be a reference to `Buffer` from
which the data was written. If the write operation failed, `err` will be an
`Error` object and `written` and `buffer` will both be `undefined`.

Note that it is unsafe to use `fs.write()` multiple times on the same file
without waiting for the callback to be invoked. If multiple writes are required,
using the stream-based `fs.createWriteStream()` is strongly recommended.

When the synchronous `fs.writeSync()` completes, the method returns the total
number of bytes written. Any errors that occur during the write operation will
be thrown and can be caught using `try/catch`.

    const fs = require('fs');
    var fd;
    try {
      var fd = fs.openSync('/tmp/example', 'w');
      var w = fs.writeSync(fd, new Buffer('the data'), 0, 8, 0);
      console.log(`${w} bytes written!`);
    } catch (err) {
      console.log('whoops! there was an error writing the data!');
    } finally {
      if (fd) fs.closeSync(fd);
    }

Note that on Linux, positional writes don't work when the file is opened in
append mode (flag `'a'`, see `fs.open()` for details). The kernel ignores the
position argument and always appends the data to the end of the file.

#### Writing using strings and optional `encoding`

- `fs.write(fd, data[, position[, encoding]], callback)`
` `fs.writeSync(fd, data[, position[, encoding]])

For example,

    const fs = require('fs');
    fs.open('/tmp/example', 'w', (err, fd) => {
      if (err) throw err;
      fs.write(fd, 'the data', 0, 'ascii', (err, w, data) => {
        if (err) throw err;
        console.log(`${w} bytes written!`);
      });
    });

The `fd` argument must be an open and writable file descriptor (see `fs.open()`
for more details).

The `position` argument refers to the offset from the beginning of the file
where the data from `buffer` should be written. If the value of `position` is
not a number, the data will be written at the current position as maintained
internally by the `fs` module.

The `data` argument is the string of data to write to the file. The optional
`encoding` argument is the expected string encoding of the `data`.

Unlike when writing a [`Buffer`][], the entire data string will be written. No
substring may be specified.

When the asynchronous `fs.write()` completes, the `callback` function will be
called and passed three arguments: `err`, `written`, and `data`. If the
write operation was successful, `err` will be `null`, `written` will indicate
the number of bytes written, and `data` will be the string that was written.
If the write operation failed, `err` will be an `Error` object and `written`
and `data` will both be `undefined`.

Note that it is unsafe to use `fs.write()` multiple times on the same file
without waiting for the callback to be invoked. If multiple writes are required,
using the stream-based `fs.createWriteStream()` is strongly recommended.

When the synchronous `fs.writeSync()` completes, the method returns the total
number of bytes written. Any errors that occur during the write operation will
be thrown and can be caught using `try/catch`.

    const fs = require('fs');
    var fd;
    try {
      var fd = fs.openSync('/tmp/example', 'w');
      var w = fs.writeSync(fd, 'the data', 0, 'ascii');
      console.log(`${w} bytes written!`);
    } catch (err) {
      console.log('whoops! there was an error writing the data!');
    } finally {
      if (fd) fs.closeSync(fd);
    }

Note that on Linux, positional writes don't work when the file is opened in
append mode (flag `'a'`, see `fs.open()` for details). The kernel ignores the
position argument and always appends the data to the end of the file.

### `fs.writeFile()` and `fs.writeFileSync()`

The `fs.writeFile()` and `fs.writeFileSync()` methods each write data to a
file, replacing the file content if it already exists.

- `fs.writeFile(file, data[, options], callback)`
- `fs.writeFileSync(file, data[, options])`

For example:

    const fs = require('fs');
    fs.writeFile('/tmp/example', 'the data', 'ascii', (err) => {
      if (err) throw err;
      console.log('the data was written!');
    });

The `file` argument can be a filename or open and writable file descriptor
(see `fs.open()` for details).

The `data` argument can be either a string or a [`Buffer`][] instance.

The optional `options` argument can either be a string or an object.

- If `options` is a string, it's value specifies the character `encoding` of
  the data.
- If `options` is an object, it may have the following properties:
  - `encoding`: specifies the character encoding of the data.
  - `mode`: sets the mode (permissions and sticky) bits of the file but only
    if it is being created.
  - `flag`: the flag used when opening the file (see `fs.open()` for details).
  
If `options` is not specified, it defaults to an object with the following
properties:

    {
      encoding: 'utf8',
      mode: 0o666,
      flag: 'w'
    }

The `encoding` option is used only if the `data` is a string.

When the asynchronous `fs.writeFile()` completes, the `callback` function is
called and passed a single `err` argument. If the write operation was
successful, `err` will be `null`; otherwise `err` will be an `Error` object.

Note that it is unsafe to use `fs.writeFile()` multiple times on the same file
without waiting for the callback. When multiple writes to a single file are
required, use of the stream-based `fs.createWriteStream()` method is strongly
recommended.

When the synchronous `fs.writeFileSync()` completes successfully, the method
simply returns. Any errors that occur during the write operation will be thrown
and can be caught using `try/catch`.

    const fs = require('fs');
    try {
      fs.writeFileSync('/tmp/example', 'the data', 'ascii');
    } catch (err) {
      console.log('whoops! there was an error writing the data!');
    }
    
## Working with links

The `fs` module exposes a number of methods that make it possible to work
with file system links.

### `fs.link()` and `fs.linkSync()`

The `fs.link()` and `fs.linkSync()` methods are used to create file system
links.

- `fs.link(srcpath, dstpath, callback)`
- `fs.linkSync(srcpath, dstpath)`

For example:

    const fs = require('fs');
    fs.link('/tmp/src', '/tmp/dst', (err) => {
      if (err) throw err;
      console.log('the link was created!');
    });

The `srcpath` specifies the current file path to which a link is being created.
The `dstpath` specifies the file path of the newly created link.

When the asynchronous `fs.link()` completes, the `callback` function is called
and passed a single `err` argument. If the link was created successfully, the
`err` will be `null`; otherwise `err` will be an `Error` object.

When the synchronous `fs.linkSync()` completes successfully, the method simply
returns. Any errors that occur while creating the link will be thrown an can
be caught using `try/catch`.

### `fs.readLink() and `fs.readlinkSync()`

The `fs.readLink()` and `fs.readlinkSync()` methods return the path to which
a link refers.

- `fs.readlink(path, callback)`
- `fs.readlinkSync(path)`

For example:

    const fs = require('fs');
    fs.link('/tmp/src', '/tmp/dst', (err) => {
      if (err) throw err;
      fs.readlink('/tmp/dst', (err, path) => {
        if (err) throw err;
        console.log(path);
          // Prints: /tmp/src
      });
    });

The `path` is the file system path of the link.

When the asynchronous `fs.readlink()` completes, the `callback` function is
called and passed two arguments: `err` and `path`. If the call was successful,
`err` will be `null` and the `path` will be the path to which the link refers;
otherwise `err` will be an `Error` object and `path` will be undefined.

When the synchronous `fs.readlinkSync()` successfully completes, the path to
which the link refers will be returned. Any errors that occur while attempting
to read the link will be thrown and can be caught using `try/catch`.

### `fs.symlink()` and `fs.symlinkSync()`

The `fs.symlink()` and `fs.symlinkSync()` methods are used to create symbolic
links in the file system.

- `fs.symlink(target, path[, type], callback)`
- `fs.symlinkSync(target, path[, type])`

For example:

    const fs = require('fs');
    fs.symlink('/tmp/target', '/tmp/link', (err) => {
      if (err) throw err;
      console.log('the symbolic link was created!');
    });

The `target` specifies the current file path to which a symbolic link is being
created. The `path` specifies the file path of the newly created symbolic link.

The optional `type` argument can be one of `'dir'`, `'file'`, or `'junction'`
and is used only by the Windows platform (the argument is ignored on all other
operating systems). When not specified, the default is `'file'`. Because
Windows junction points require the destination path to be absolute, whenever
`type` is set to `'junction'`, the `target` will be automatically resolved to
an absolute path.

When the asynchronous `fs.symlink()` completes, the `callback` function is
called and passed a single `err` argument. If creating the symbolic link was
successful, `err` will be `null`; otherwise `err` will be an `Error` object.

When the synchronous `fs.symlinkSync()` successfully completes, the method
simply returns. Any errors that occur while creating the symbol link will be
thrown and can be caught using `try/catch`.

    const fs = require('fs');
    try {
      fs.symlinkSync('/tmp/target', '/tmp/link');
      console.log('the symbolic link was created!');
    } catch (err) {
      console.log('whoops! the symbolic link could not be created!');
    }

### `fs.unlink()` and `fs.unlinkSync()`

The `fs.unlink()` and `fs.unlinkSync()` methods delete links and files.

- `fs.unlink(path, callback)`
- `fs.unlinkSync(path)`

The `path` is the file system path that is to be deleted.

For example:

    const fs = require('fs');
    fs.unlink('/tmp/example', (err) => {
      if (err) throw err;
      console.log('the example was deleted');
    });

When the asynchronous `fs.unlink()` completes, the `callback` function is
called and passed a single `err` argument. If the unlink operation was
successful, the `err` will be `null`; otherwise `err` will be an `Error`
object.

Whe the synchronous `fs.unlinkSync()` completes successfully, the method
simply returns. Any errors that occur during the unlink operation will be
thrown and can be caught using `try/catch`.

    const fs = require('fs');
    try {
      fs.unlinkSync('/tmp/example');
      console.log('file unlinked!');
    } catch (err) {
      console.log('whoops! there was an error running unlink');
    }

## Working with directories

The `fs` module exposes a number of methods that make it possible to work
with file system directories.

### `fs.mkdir()` and `fs.mkdirSync()`

The `fs.mkdir()` and `fs.mkdirSync()` methods create directories.

- `fs.mkdir(path[, mode], callback)`
- `fs.mkdirSync(path[, mode])`

For example:

    const fs = require('fs');
    fs.mkdir('/foo', (err) => {
      if (err) throw err;
      console.log('the directory was created');
    });

The `path` is the file system path of the directory to create. If the `path`
already exists, or if any of the parent directories identified in the path do
not exist, the operation will fail and an error will be reported.

The `mode` argument is a bit field indicating the permissions to apply to the
newly created directory (see [`File mode constants`][] for details). If not
specified, the default `mode` is `0o777`.

When the asynchronous `fs.mkdir()` completes, the `callback` function will be
called and passed a single `err` argument. If the directory was created
successfully, `err` will be `null`; otherwise `err` will be an `Error` object.

When the synchronous `fs.mkdirSync()` completes successfully, the method simply
returns. Any errors that occur while attempting to create the directory will be
thrown and can be caught using `try/catch`.

    const fs = require('fs');
    try {
      fs.mkdirSync('/foo');
      console.log('the directory was created.');
    } catch (err) {
      console.log('whoops! the directory could not be created');
    }

### `fs.readdir()` and `fs.readdirSync()`

The `fs.readdir()` and `fs.readdirSync()` methods scan a file system directory
and return an array of filenames within that directory.

- `fs.readdir(path, callback)`
` `fs.readdirSync(path)

For example:

    const fs = require('fs');
    fs.readdir('/tmp', (err, files) => {
      if (err) throw err;
      console.log('Files: ', files);
    });

The `path` is the file system path of the directory whose files are to be
listed.

The methods will construct an array of names of files in the directory
excluding the special purpose `'.'` and `'..'` filenames.

When the asynchronous `fs.readdir()` completes, the `callback` function is
called and passed two arguments: `err` and `files`. If the directory was read
successfully, `err` will be `null` and `files` will be an array of filenames;
otherwise `err` will be an `Error` and `files` will be `undefined`.

When the synchronous `fs.readdirSync()` completes successfully, the array of
file names will be returned. Any errors that occur while attempting to list
the files will be thrown and can be caught using `try/catch`.

    const fs = require('fs');
    try {
      console.log('Files', fs.readdirSync('/tmp'));
    } catch (err) {
      console.log('whoops! there was an error listing the files!');
    }

On some operating systems (e.g. Linux) the array returned will be lexically
sorted by filename, while on others (e.g. Windows) the array will be unsorted.
This sorting behavior is automatic and cannot currently be switched off.

### `fs.rmdir()` and `fs.rmdirSync()`

The `fs.rmdir()` and `fs.rmdirSync()` methods remove directories from the
file system.

- `fs.rmdir(path, callback)`
- `fs.rmdirSync(path)`

For example:

    const fs = require('fs');
    fs.rmdir('/tmp/foo', (err) => {
      if (err) throw err;
      console.log('the directory was removed!');
    });

The `path` is the directory to be removed.

*The methods will not delete a directory unless it is empty. There is no
option for recursively deleting a directory and all of its content.*

When the asynchronous `fs.rmdir()` completes, the `callback` function is
called and passed a single `err` argument. If the directory was removed
successfully, `err` will be `null`; otherwise `err` will be an `Error`.

When the synchronous `fs.rmdirSync()` completes successfully the method simply
returns. Any errors that occur while attempting to remove the directory will be
thrown and can be caught using `try/catch`.

    const fs = require('fs');
    try {
      fs.rmdirSync('/tmp/foo');
      console.log('the directory was removed');
    } catch (err) {
      console.log('whoops! there was an error removing the directory!');
    }

## Renaming paths

The `fs` module exposes methods that allow files, directories and links to
be renamed or moved.

### `fs.rename()` and `fs.renameSync()`

The `fs.rename()` and `fs.renameSync()` methods are used to rename or move
files.

- `fs.rename(oldPath, newPath, callback)`
- `fs.renameSync(oldPath, newPath)`

For example:

    const fs = require('fs');
    fs.rename('/tmp/old', '/Users/sally/tmp/new', (err) => {
      if (err) throw err;
      console.log('the file was moved!');
    });

The `oldPath` argument is the current path of the file. The `newPath` argument
is the new path that is to be assigned.

When the asynchronous `fs.rename()` method completes, the `callback` function
is called with a single `err` argument. If the rename operation was successful,
`err` is `null`; otherwise `err` is an `Error` object.

When the synchronous `fs.renameSync()` completes successfully, the method
simply returns. Any errors that occur while attempting to rename the file
will be thrown and can be caught using `try/catch`.

## Miscellaneous other methods

A variety of other methods for manipulating the file system are provided.

### `fs.fsync()` and `fs.fsyncSync()`

The `fs.fsync()` and `fs.fsyncSync()` methods call the system level `fsync`
mechanism to flush the data for a given file descriptor to permanent device
storage. See [`fsync`][] for more information.

- `fs.fsync(fd, callback)`
- `fs.fsyncSync(fd)`

The `fd` must be an open file descriptor (see `fs.open()` for details).

When the asynchronous `fs.fsync()` method completes, the `callback` function
will be called with a single `err` argument. If the fsync operation was
successful, `err` will be `null`; otherwise `err` will be an `Error` object.

When the synchronous `fs.fsyncSync()` method completes successfully, the method
will simply return. Any errors that occur while attempting the fsync will be
thrown and can be caught using `try/catch`.

### `fs.realpath()` and `fs.realpathSync()`

The `fs.realpath()` and `fs.realpathSync()` methods resolve all symbolic links
and non-canonical file system paths to their canonical absolute paths.

- `fs.realpath(path[, cache], callback)`
- `fs.realpathSync(path[, cache])`

For example,

    const fs = require('fs');
    fs.realpath('../././../', (err, path) => {
      if (err) throw err;
      console.log(`the real path is ${path}`);
    });

The `path` argument is the non-canonical path that is to be resolved.

The optional `cache` argument is an object of mapped paths that can be used to
force a specific path resolution or to avoid costly additional `fs.stat()`
calls for known real paths.

    const fs = require('fs');
    var cache = {
      '/etc': '/private/etc'
    };
    fs.realpath('/etc/passwd', cache, (err, path) => {
      if (err) throw err;
      console.log(resolvedPath);
    });

When the asynchronous `fs.realpath()` method completes, the `callback` function
will be called and passed two argments: `err` and `path`. If resolving the
path was successful, `err` will be `null` and `path` will be the resolved path;
otherwise `err` will be an `Error` object and `path` will be `undefined`.

When the synchronous `fs.realpathSync()` method completes successfully, the
resolved path will be returned. Any errors that occur while resolving the path
will be thrown and can be caught using `try/catch`.

    const fs = require('fs');
    try {
      console.log(`the realpath path is ${fs.realpathSync('.././././')}`);
    } catch (err) {
      console.log('whoops! there was an error resolving the path');
    }

### `fs.utimes()`, `fs.utimesSync()`, `fs.futimes()`, and `fs.futimesSync()`

The `fs.utimes()` and `fs.utimesSync()` methods change file timestamps of the
file referenced by the supplied path.

- `fs.utimes(path, atime, mtime, callback)`
- `fs.utimesSync(path, atime, mtime)`
- `fs.futimes(fd, atime, mtime, callback)`
- `fs.futimesSync(fd, atime, mtime)`

The `path` argument is the file system path of the file whose timestamps are
being modified. The `fs.futimes()` and `fs.futimesSync()` variants require the
first argument to be an open file descriptor (see `fs.open()` for details).

If the values of `atime` and `mtime` are numberable strings such as
`'123456789'`, the value is coerced to the corresponding number. If the values
are `NaN` or `Infinity`, the values would are converted to `Date.now()`.

When the asynchronous `fs.utimes()` of `fs.futimes()` methods complete, the
`callback` function is called and passed a single `err` argument. If the
operation was successful, `err` will be `null`; otherwise `err` will be an
`Error` object.

When the synchronous `fs.utimesSync()` or `fs.futimesSync()` methods complete
successfully, the methods simply return. Any errors that occur while attempting
to set the timestamps will be thrown and can be caught using `try/catch`.


## Watching the file system for changes

The `fs` module provides mechanisms for watching the file system for changes.

For example:

    const fs = require('fs');
    fs.watch('/tmp/example', (event, filename) => {
      console.log(`${filename} was modified! The event was ${event}`);
    });

Support for the `fs.watch()` capability is not entirely consistent across
operating system platforms and is unavailable in some circumstances. This is
because the specific implementation of `fs.watch()` depends on the underlying
operating system providing a way to be notified of file system changes.

- On Linux systems, this uses `inotify`.
- On BSD systems, this uses `kqueue`.
- On OS X, this uses `kqueue` for files and 'FSEvents' for directories.
- On SunOS systems (including Solaris and SmartOS), this uses `event ports`.
- On Windows systems, this feature depends on `ReadDirectoryChangesW`.

If this underlying ability to be notified of changes by the operating system is
not available for some reason, then `fs.watch()` will not function.  For
instance, it is currently not possible to reliably watch files or directories
located on network file systems (NFS, SMB, etc.).

The `fs.watchFile()` method implements an alternative approach based on stat
polling and may be used in some situations, but it is slower and less reliable
than `fs.watch()`.

### `fs.watch()`

The `fs.watch()` method creates and returns an [`fs.FSWatcher`][] object that
watches for changes on `filename`, where `filename` is either a file or a
directory.

- `fs.watch(filename[, options][, listener])`

The optional `options` argument should be an object. The supported boolean
members are `persistent` and `recursive`:

- The `persistent` option indicates whether the Node.js process should continue
  to run as long as files are being watched.
- The `recursive` option indicates whether all subdirectories should be
  watched or only the current directory. This applies only when a directory is
  specified as the `filename`. The `recursive` option is only supported on OS X
  and Windows.

The default `options` are: `{ persistent: true, recursive: false }`.

When the `persistent` option is `true`, the Node.js process will not exit
until the `watcher.close()` method is called on the `fs.FSWatcher` object
returned by `fs.watch()`.

The `fs.FSWatcher` object returned is an [`EventEmitter`][]. The `listener`
function passed to `fs.watch()` will be registered on the `fs.FSWatcher`
objects `'change'` event. The callback is passed two arguments when invoked:
`event` and `filename`. The `event` argument will be either `'rename'` or
`'change'`.

The `filename` argument passed to the called is the name of the file which
triggered the event. This argument is supported only on Linux and Windows,
However, even on these supported platforms, `filename` is not always guaranteed
to be provided. Application code should not assume that the `filename` argument
will always be provided in the callback, and should have some fallback logic if
it is passed as `null`:

    const fs = require('fs');
    fs.watch('/tmp', (event, filename) => {
      console.log(`event is: ${event}`);
      if (filename) {
        console.log(`filename provided: ${filename}`);
      } else {
        console.log('filename not provided');
      }
    });

### `fs.watchFile()` and `fs.unwatchFile()`

The `fs.watchFile()` method instructs the `fs` module to begin watching for
changes on a given filename. The `fs.unwatchFile()` method instructs instructs
the module to stop watching for such changes.

The implementation of `fs.watchFile()` uses a stat polling model that is less
constrained by operating system capabilities, but also less efficient than the
implementation of `fs.watch()`. Whenever possible the `fs.watch()` method
should be preferred.

- `fs.watchFile(filename[, options], listener)`
- `fs.unwatchFile(filename[, listener])`

The `filename` argument is the file system path to be watched.

The `options` argument may be omitted. If provided, it should be an object. The
`options` object may contain a boolean named `persistent` that indicates
whether the Node.js process should continue to run as long as files are being
watched. The `options` object may specify an `interval` property indicating how
often the target should be polled in milliseconds.

The default `options` are: `{ persistent: true, interval: 5007 }`.

When the `persistent` option is `true`, the Node.js process will not exit
until `fs.unwatchFile()` is used to stop all watching listener functions.

Once a file is being watched, the `listener` function passed to `fs.watchFile()`
will be called every time an access or change event happens on the file. The
function is passed two arguments: `current` and `previous`. The `current` is
the current `fs.Stats` object description of the file (as returned by
`fs.stat()`) and the `previous` is the previous `fs.Stats` object prior to the
change.

    const fs = require('fs');
    fs.watchFile('message.text', (current, previous) => {
      console.log(`the current mtime is: ${current.mtime}`);
      console.log(`the previous mtime was: ${previous.mtime}`);
    });

Compare the `current.mtime` and `previous.mtime` properties on the two
`fs.Stats` objects to determine if the file has been modified.

The `fs.unwatchFile()` is used to stop watching the file. The `filename`
argument is the path of the file being watched. If the `listener` function
is not specified, then _all_ `listeners` currently watching the given `filename`
are removed; otherwise only the specified `listener` is removed.

Calling `fs.unwatchFile()` with a `filename` that is not being watched has no
effect.

#### Error handling

The `fs.watchFile()` method will throw an error if a `listener` function is
not provided. This can be caught using `try/catch`.

Note that when an `fs.watchFile()` operation results in an `ENOENT` error, or
if the watched file does not currently exist, the the `listener` function will
be invoked *once*, with all of the fields on the `current` and `previous`
`fs.Stats` files zeroed (or, for dates, the Unix Epoch). On Windows, the
`blksize` and `blocks` fields will be `undefined`, instead of zero. If the file
is created later on, the listener will be called again, with the latest stat
objects. This is a change in functionality since Node.js v0.10


## `fs` module utility classes

The `fs` module provides a number of utility classes that can be used to read
from, write to, or monitor the file system.

### Class: fs.FSWatcher

Instances of the `fs.FSWatcher` class are [`EventEmitter`][] objects that
monitor the file system and emit events when changes are detected.

The `fs.watch()` method is used to create `fs.FSWatcher` instances. The `new`
keyword is not to be used to create `fs.FSWatcher` objects.

#### Event: 'change'

* `event` {String} The type of fs change
* `filename` {String} The filename that changed (if relevant/available)

The `'change'` event is emitted when a change is detected in a watched
directory or file.

See more details in [`fs.watch()`][].

#### Event: 'error'

* `error` {Error object}

The `'error'` event is emitted when an error occurs while watching the file
system.

#### watcher.close()

Stop watching for changes on the given `fs.FSWatcher`.

### Class: fs.ReadStream

Instances of the `fs.ReadStream` class are [Readable Streams][] that can be
used to read data from the file system. Each `fs.ReadStream` object encapsulates
a single file descriptor (fd).

The `fs.createReadStream()` method is used to create `fs.ReadStream` instances.
The `new` keyword is not to be used to create `fs.ReadStream` objects.

#### Event: 'open'

* `fd` {Integer} file descriptor used by the ReadStream.

The `'open'` event is emitted when the `fs.ReadStream` file descriptor is
opened.

#### readStream.path

Returns the file system path from which this `fs.ReadStream` is reading data.

### Class: fs.Stats

The `fs.Stats` class is used to provide information about a file descriptor
(fd). Objects passed to the callbacks of [`fs.stat()`][], [`fs.lstat()`][] and
[`fs.fstat()`][] (and returned from their synchronous counterparts) are of this
type.

The specific properties available on an `fs.Stats` instance can vary somewhat
based on operating system.

- `stats.dev`: identifier of the device container
- `stats.mode`: description of the protection applied
- `stats.nlink`: the number of hard links
- `stats.uid`: the user ID of the owner
- `stats.gid`: the group ID of the owner
- `stats.rdev`: the device ID (if the file descriptor represents a special file)
- `stats.ino`: the inode number
- `stats.size`: the total size in bytes (if available)
- `stats.atim_msec`: the time of last access
- `stats.mtim_msec`: the time of last modification
- `stats.ctim_msec`: the time of last status change
- `stats.birthtim_msec`: the time of creation
- `stats.blksize`: the blocksize (POSIX only)
- `stats.blocks`: the number of blocks allocated (POSIX only)

For a regular file, calling [`util.inspect(stats)`][] would return a string
similar to the following:

    { dev: 2114,
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
      birthtime: Mon, 10 Oct 2011 23:24:11 GMT }

Note that `atime`, `mtime`, `birthtime`, and `ctime` are
instances of the JavaScript [`Date`][MDN-Date] object and that to
compare the values of these objects you should use appropriate methods.
For most general uses, [`getTime()`][MDN-Date-getTime] will return the
number of milliseconds elapsed since _1 January 1970 00:00:00 UTC_ and this
integer should be sufficient for most comparisons, however there are
additional methods which can be used for displaying fuzzy information.
More details can be found in the [MDN JavaScript Reference][MDN-Date]
page.

#### Stat Time Values

The times in the `fs.Stats` object have the following semantics:

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
  `1970-01-01T00:00Z` (ie, unix epoch timestamp `0`).  On Darwin and
  other FreeBSD variants, also set if the `atime` is explicitly set to
  an earlier value than the current `birthtime` using the `utimes(2)`
  system call.

Prior to Node v0.12, the `ctime` held the `birthtime` on Windows
systems.  Note that as of v0.12, `ctime` is not "creation time", and
on Unix systems, it never was.

#### stats.isBlockDevice()

Returns `true` if the file descriptor (fd) represents a block device.

#### stats.isCharacterDevice()

Returns `true` if the file descriptor (fd) represents a character device.

#### stats.isDirectory()

Returns `true` if the file descriptor (fd) represents a directory

#### stats.isFIFO()

Returns `true` if the file descriptor (fd) represents a FIFO named pipe.

#### stats.isFile()

Returns `true` if the file descriptor (fd) represents a single file.

#### stats.isSocket()

Returns `true` if the file descriptor (fd) represents a socket.

#### stats.isSymbolicLink()

Returns `true` if the file descriptor (fd) represents a symbolic link. This
method is only available on `fs.Stats` objects generated by the the `fs.lstat()`
or `fs.lstatSync()` methods.

### Class: fs.WriteStream

Instances of the `fs.WriteStream` class are [Writable Streams][] that can be
used to write data to the file system. Each `fs.WriteStream` object encapsulates
a single file descriptor (fd).

The `fs.createWriteStream()` method is used to create `fs.WriteStream`
instances. The `new` keyword is not to be used to create `fs.WriteStream`
objects.

#### Event: 'open'

* `fd` {Integer} file descriptor used by the WriteStream.

The `'open'` event is emitted when the WriteStream's file descriptor is opened.

#### writeStream.bytesWritten

Returns the number of bytes actually written to the file system so far. Note
that this does not include data that may still be queued for writing.

#### writeStream.path

Returns the file system path to which the `fs.WriteStream` is writing data.

[`Buffer.byteLength`]: buffer.html#buffer_class_method_buffer_bytelength_string_encoding
[`Buffer`]: buffer.html#buffer_buffer
[`fs.access()`]: fs.html#fs_checking_file_permissions
[`fs.accessSync()`]: fs.html#fs_checking_file_permissions
[`fs.appendFile()`]: fs.html#fs_fs_appendfile_and_fs_appendfilesync
[`fs.exists()`]: fs.html#fs_fs_exists_and_fs_existssync
[`fs.stat()`]: fs.html#fs_fs_stat_fs_lstat_and_fs_fstat
[`fs.statSync()`]: fs.html#fs_fs_stat_fs_lstat_and_fs_fstat
[`fs.fstat()`]: fs.html#fs_fs_stat_fs_lstat_and_fs_fstat
[`fs.lstat()`]: fs.html#fs_fs_stat_fs_lstat_and_fs_fstat
[`fs.FSWatcher`]: fs.html#fs_class_fs_fswatcher
[`fs.utimes()`]: fs.html#fs_fs_utimes_fs_utimessync_fs_futimes_and_fs_futimessync
[`fs.futimes()`]: fs.html#fs_fs_utimes_fs_utimessync_fs_futimes_and_fs_futimessync
[`fs.open()`]: fs.html#fs_fs_open_and_fs_opensync
[`fs.close()`]: fs.html#fs_fs_close_and_fs_closesync
[`fs.read()`]: fs.html#fs_fs_read_and_fs_readsync
[`fs.readFile`]: fs.html#fs_fs_readfile_and_fs_readfilesync
[`fs.Stats`]: fs.html#fs_class_fs_stats
[`fs.watch()`]: fs.html#fs_fs_watch
[`fs.write()`]: fs.html#fs_fs_write_and_fs_writesync
[`fs.writeFile()`]: fs.html#fs_fs_writefile_and_fs_writefilesync
[`net.Socket`]: net.html#net_class_net_socket
[`fs.ReadStream`]: fs.html#fs_class_fs_readstream
[`fs.WriteStream`]: fs.html#fs_class_fs_writestream
[`util.inspect(stats)`]: util.html#util_util_inspect_object_options
[MDN-Date-getTime]: https://developer.mozilla.org/en/JavaScript/Reference/Global_Objects/Date/getTime
[MDN-Date]: https://developer.mozilla.org/en/JavaScript/Reference/Global_Objects/Date
[Readable Streams]: stream.html#stream_class_stream_readable
[Writable Streams]: stream.html#stream_class_stream_writable
[File mode constants]: fs.html#fs_file_mode_constants
[`fsync`]: http://linux.die.net/man/2/fsync
[`EventEmitter`]: events.html
[Working with Different Filesystems]: https://github.com/nodejs/nodejs.org/blob/master/locale/en/docs/guides/working-with-different-filesystems.md

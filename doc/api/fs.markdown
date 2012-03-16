# File System

    Stability: 3 - Stable

<!--name=fs-->

File I/O is provided by simple wrappers around standard POSIX functions.  To
use this module do `require('fs')`. All the methods have asynchronous and
synchronous forms.

The asynchronous form always take a completion callback as its last argument.
The arguments passed to the completion callback depend on the method, but the
first argument is always reserved for an exception. If the operation was
completed successfully, then the first argument will be `null` or `undefined`.

When using the synchronous form any exceptions are immediately thrown.
You can use try/catch to handle exceptions or allow them to bubble up.

Here is an example of the asynchronous version:

    var fs = require('fs');

    fs.unlink('/tmp/hello', function (err) {
      if (err) throw err;
      console.log('successfully deleted /tmp/hello');
    });

Here is the synchronous version:

    var fs = require('fs');

    fs.unlinkSync('/tmp/hello')
    console.log('successfully deleted /tmp/hello');

With the asynchronous methods there is no guaranteed ordering. So the
following is prone to error:

    fs.rename('/tmp/hello', '/tmp/world', function (err) {
      if (err) throw err;
      console.log('renamed complete');
    });
    fs.stat('/tmp/world', function (err, stats) {
      if (err) throw err;
      console.log('stats: ' + JSON.stringify(stats));
    });

It could be that `fs.stat` is executed before `fs.rename`.
The correct way to do this is to chain the callbacks.

    fs.rename('/tmp/hello', '/tmp/world', function (err) {
      if (err) throw err;
      fs.stat('/tmp/world', function (err, stats) {
        if (err) throw err;
        console.log('stats: ' + JSON.stringify(stats));
      });
    });

In busy processes, the programmer is _strongly encouraged_ to use the
asynchronous versions of these calls. The synchronous versions will block
the entire process until they complete--halting all connections.

Relative path to filename can be used, remember however that this path will be relative
to `process.cwd()`.

## fs.rename(path1, path2, [callback])

Asynchronous rename(2). No arguments other than a possible exception are given
to the completion callback.

## fs.renameSync(path1, path2)

Synchronous rename(2).

## fs.truncate(fd, len, [callback])

Asynchronous ftruncate(2). No arguments other than a possible exception are
given to the completion callback.

## fs.truncateSync(fd, len)

Synchronous ftruncate(2).

## fs.chown(path, uid, gid, [callback])

Asynchronous chown(2). No arguments other than a possible exception are given
to the completion callback.

## fs.chownSync(path, uid, gid)

Synchronous chown(2).

## fs.fchown(fd, uid, gid, [callback])

Asynchronous fchown(2). No arguments other than a possible exception are given
to the completion callback.

## fs.fchownSync(fd, uid, gid)

Synchronous fchown(2).

## fs.lchown(path, uid, gid, [callback])

Asynchronous lchown(2). No arguments other than a possible exception are given
to the completion callback.

## fs.lchownSync(path, uid, gid)

Synchronous lchown(2).

## fs.chmod(path, mode, [callback])

Asynchronous chmod(2). No arguments other than a possible exception are given
to the completion callback.

## fs.chmodSync(path, mode)

Synchronous chmod(2).

## fs.fchmod(fd, mode, [callback])

Asynchronous fchmod(2). No arguments other than a possible exception
are given to the completion callback.

## fs.fchmodSync(fd, mode)

Synchronous fchmod(2).

## fs.lchmod(path, mode, [callback])

Asynchronous lchmod(2). No arguments other than a possible exception
are given to the completion callback.

## fs.lchmodSync(path, mode)

Synchronous lchmod(2).

## fs.stat(path, [callback])

Asynchronous stat(2). The callback gets two arguments `(err, stats)` where
`stats` is a [fs.Stats](#fs_class_fs_stats) object.  See the [fs.Stats](#fs_class_fs_stats)
section below for more information.

## fs.lstat(path, [callback])

Asynchronous lstat(2). The callback gets two arguments `(err, stats)` where
`stats` is a `fs.Stats` object. `lstat()` is identical to `stat()`, except that if
`path` is a symbolic link, then the link itself is stat-ed, not the file that it
refers to.

## fs.fstat(fd, [callback])

Asynchronous fstat(2). The callback gets two arguments `(err, stats)` where
`stats` is a `fs.Stats` object. `fstat()` is identical to `stat()`, except that
the file to be stat-ed is specified by the file descriptor `fd`.

## fs.statSync(path)

Synchronous stat(2). Returns an instance of `fs.Stats`.

## fs.lstatSync(path)

Synchronous lstat(2). Returns an instance of `fs.Stats`.

## fs.fstatSync(fd)

Synchronous fstat(2). Returns an instance of `fs.Stats`.

## fs.link(srcpath, dstpath, [callback])

Asynchronous link(2). No arguments other than a possible exception are given to
the completion callback.

## fs.linkSync(srcpath, dstpath)

Synchronous link(2).

## fs.symlink(linkdata, path, [type], [callback])

Asynchronous symlink(2). No arguments other than a possible exception are given
to the completion callback.
`type` argument can be either `'dir'` or `'file'` (default is `'file'`).  It is only 
used on Windows (ignored on other platforms).

## fs.symlinkSync(linkdata, path, [type])

Synchronous symlink(2).

## fs.readlink(path, [callback])

Asynchronous readlink(2). The callback gets two arguments `(err,
linkString)`.

## fs.readlinkSync(path)

Synchronous readlink(2). Returns the symbolic link's string value.

## fs.realpath(path, [callback])

Asynchronous realpath(2).  The callback gets two arguments `(err,
resolvedPath)`.  May use `process.cwd` to resolve relative paths.

## fs.realpathSync(path)

Synchronous realpath(2). Returns the resolved path.

## fs.unlink(path, [callback])

Asynchronous unlink(2). No arguments other than a possible exception are given
to the completion callback.

## fs.unlinkSync(path)

Synchronous unlink(2).

## fs.rmdir(path, [callback])

Asynchronous rmdir(2). No arguments other than a possible exception are given
to the completion callback.

## fs.rmdirSync(path)

Synchronous rmdir(2).

## fs.mkdir(path, [mode], [callback])

Asynchronous mkdir(2). No arguments other than a possible exception are given
to the completion callback. `mode` defaults to `0777`.

## fs.mkdirSync(path, [mode])

Synchronous mkdir(2).

## fs.readdir(path, [callback])

Asynchronous readdir(3).  Reads the contents of a directory.
The callback gets two arguments `(err, files)` where `files` is an array of
the names of the files in the directory excluding `'.'` and `'..'`.

## fs.readdirSync(path)

Synchronous readdir(3). Returns an array of filenames excluding `'.'` and
`'..'`.

## fs.close(fd, [callback])

Asynchronous close(2).  No arguments other than a possible exception are given
to the completion callback.

## fs.closeSync(fd)

Synchronous close(2).

## fs.open(path, flags, [mode], [callback])

Asynchronous file open. See open(2). `flags` can be:

* `'r'` - Open file for reading.
An exception occurs if the file does not exist.

* `'r+'` - Open file for reading and writing.
An exception occurs if the file does not exist.

* `'w'` - Open file for writing.
The file is created (if it does not exist) or truncated (if it exists).

* `'w+'` - Open file for reading and writing.
The file is created (if it does not exist) or truncated (if it exists).

* `'a'` - Open file for appending.
The file is created if it does not exist.

* `'a+'` - Open file for reading and appending.
The file is created if it does not exist.

`mode` defaults to `0666`. The callback gets two arguments `(err, fd)`.

## fs.openSync(path, flags, [mode])

Synchronous open(2).

## fs.utimes(path, atime, mtime, [callback])
## fs.utimesSync(path, atime, mtime)

Change file timestamps of the file referenced by the supplied path.

## fs.futimes(fd, atime, mtime, [callback])
## fs.futimesSync(fd, atime, mtime)

Change the file timestamps of a file referenced by the supplied file
descriptor.

## fs.fsync(fd, [callback])

Asynchronous fsync(2). No arguments other than a possible exception are given
to the completion callback.

## fs.fsyncSync(fd)

Synchronous fsync(2).

## fs.write(fd, buffer, offset, length, position, [callback])

Write `buffer` to the file specified by `fd`.

`offset` and `length` determine the part of the buffer to be written.

`position` refers to the offset from the beginning of the file where this data
should be written. If `position` is `null`, the data will be written at the
current position.
See pwrite(2).

The callback will be given three arguments `(err, written, buffer)` where `written`
specifies how many _bytes_ were written from `buffer`.

Note that it is unsafe to use `fs.write` multiple times on the same file
without waiting for the callback. For this scenario,
`fs.createWriteStream` is strongly recommended.

## fs.writeSync(fd, buffer, offset, length, position)

Synchronous version of buffer-based `fs.write()`. Returns the number of bytes
written.

## fs.writeSync(fd, str, position, [encoding])

Synchronous version of string-based `fs.write()`. `encoding` defaults to
`'utf8'`. Returns the number of _bytes_ written.

## fs.read(fd, buffer, offset, length, position, [callback])

Read data from the file specified by `fd`.

`buffer` is the buffer that the data will be written to.

`offset` is offset within the buffer where writing will start.

`length` is an integer specifying the number of bytes to read.

`position` is an integer specifying where to begin reading from in the file.
If `position` is `null`, data will be read from the current file position.

The callback is given the three arguments, `(err, bytesRead, buffer)`.

## fs.readSync(fd, buffer, offset, length, position)

Synchronous version of buffer-based `fs.read`. Returns the number of
`bytesRead`.

## fs.readSync(fd, length, position, encoding)

Synchronous version of string-based `fs.read`. Returns the number of
`bytesRead`.

## fs.readFile(filename, [encoding], [callback])

Asynchronously reads the entire contents of a file. Example:

    fs.readFile('/etc/passwd', function (err, data) {
      if (err) throw err;
      console.log(data);
    });

The callback is passed two arguments `(err, data)`, where `data` is the
contents of the file.

If no encoding is specified, then the raw buffer is returned.


## fs.readFileSync(filename, [encoding])

Synchronous version of `fs.readFile`. Returns the contents of the `filename`.

If `encoding` is specified then this function returns a string. Otherwise it
returns a buffer.


## fs.writeFile(filename, data, [encoding], [callback])

Asynchronously writes data to a file, replacing the file if it already exists.
`data` can be a string or a buffer. The `encoding` argument is ignored if
`data` is a buffer. It defaults to `'utf8'`.

Example:

    fs.writeFile('message.txt', 'Hello Node', function (err) {
      if (err) throw err;
      console.log('It\'s saved!');
    });

## fs.writeFileSync(filename, data, [encoding])

The synchronous version of `fs.writeFile`.

## fs.watchFile(filename, [options], listener)

    Stability: 2 - Unstable.  Use fs.watch instead, if available.

Watch for changes on `filename`. The callback `listener` will be called each
time the file is accessed.

The second argument is optional. The `options` if provided should be an object
containing two members a boolean, `persistent`, and `interval`. `persistent`
indicates whether the process should continue to run as long as files are
being watched. `interval` indicates how often the target should be polled,
in milliseconds. (On Linux systems with inotify, `interval` is ignored.) The
default is `{ persistent: true, interval: 0 }`.

The `listener` gets two arguments the current stat object and the previous
stat object:

    fs.watchFile('message.text', function (curr, prev) {
      console.log('the current mtime is: ' + curr.mtime);
      console.log('the previous mtime was: ' + prev.mtime);
    });

These stat objects are instances of `fs.Stat`.

If you want to be notified when the file was modified, not just accessed
you need to compare `curr.mtime` and `prev.mtime`.


## fs.unwatchFile(filename)

    Stability: 2 - Unstable.  Use fs.watch instead, if available.

Stop watching for changes on `filename`.

## fs.watch(filename, [options], listener)

    Stability: 2 - Unstable.  Not available on all platforms.

Watch for changes on `filename`, where `filename` is either a file or a
directory.  The returned object is a [fs.FSWatcher](#fs_class_fs_fswatcher).

The second argument is optional. The `options` if provided should be an object
containing a boolean member `persistent`, which indicates whether the process
should continue to run as long as files are being watched. The default is
`{ persistent: true }`.

The listener callback gets two arguments `(event, filename)`.  `event` is either
'rename' or 'change', and `filename` is the name of the file which triggered
the event.

### Caveats

<!--type=misc-->

The `fs.watch` API is not 100% consistent across platforms, and is
unavailable in some situations.

#### Availability

<!--type=misc-->

This feature depends on the underlying operating system providing a way
to be notified of filesystem changes.

* On Linux systems, this uses `inotify`.
* On BSD systems (including OS X), this uses `kqueue`.
* On SunOS systems (including Solaris and SmartOS), this uses `event ports`.
* On Windows systems, this feature depends on `ReadDirectoryChangesW`.

If the underlying functionality is not available for some reason, then
`fs.watch` will not be able to function.  You can still use
`fs.watchFile`, which uses stat polling, but it is slower and less
reliable.

#### Filename Argument

<!--type=misc-->

Providing `filename` argument in the callback is not supported
on every platform (currently it's only supported on Linux and Windows).  Even
on supported platforms `filename` is not always guaranteed to be provided.
Therefore, don't assume that `filename` argument is always provided in the
callback, and have some fallback logic if it is null.

    fs.watch('somedir', function (event, filename) {
      console.log('event is: ' + event);
      if (filename) {
        console.log('filename provided: ' + filename);
      } else {
        console.log('filename not provided');
      }
    });

## Class: fs.Stats

Objects returned from `fs.stat()`, `fs.lstat()` and `fs.fstat()` and their
synchronous counterparts are of this type.

 - `stats.isFile()`
 - `stats.isDirectory()`
 - `stats.isBlockDevice()`
 - `stats.isCharacterDevice()`
 - `stats.isSymbolicLink()` (only valid with  `fs.lstat()`)
 - `stats.isFIFO()`
 - `stats.isSocket()`

For a regular file `util.inspect(stats)` would return a string very
similar to this:

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
      ctime: Mon, 10 Oct 2011 23:24:11 GMT }

Please note that `atime`, `mtime` and `ctime` are instances
of [Date][MDN-Date] object and to compare the values of
these objects you should use appropriate methods. For most
general uses [getTime()][MDN-Date-getTime] will return
the number of milliseconds elapsed since _1 January 1970
00:00:00 UTC_ and this integer should be sufficient for
any comparison, however there additional methods which can
be used for displaying fuzzy information. More details can
be found in the [MDN JavaScript Reference][MDN-Date] page.

[MDN-Date]: https://developer.mozilla.org/en/JavaScript/Reference/Global_Objects/Date
[MDN-Date-getTime]: https://developer.mozilla.org/en/JavaScript/Reference/Global_Objects/Date/getTime


## fs.createReadStream(path, [options])

Returns a new ReadStream object (See `Readable Stream`).

`options` is an object with the following defaults:

    { flags: 'r',
      encoding: null,
      fd: null,
      mode: 0666,
      bufferSize: 64 * 1024
    }

`options` can include `start` and `end` values to read a range of bytes from
the file instead of the entire file.  Both `start` and `end` are inclusive and
start at 0.

An example to read the last 10 bytes of a file which is 100 bytes long:

    fs.createReadStream('sample.txt', {start: 90, end: 99});


## Class: fs.ReadStream

`ReadStream` is a [Readable Stream](stream.html#stream_readable_stream).

### Event: 'open'

* `fd` {Integer} file descriptor used by the ReadStream.

Emitted when the ReadStream's file is opened.


## fs.createWriteStream(path, [options])

Returns a new WriteStream object (See `Writable Stream`).

`options` is an object with the following defaults:

    { flags: 'w',
      encoding: null,
      mode: 0666 }

`options` may also include a `start` option to allow writing data at
some position past the beginning of the file.  Modifying a file rather
than replacing it may require a `flags` mode of `r+` rather than the
default mode `w`.

## fs.WriteStream

`WriteStream` is a [Writable Stream](stream.html#stream_writable_stream).

### Event: 'open'

* `fd` {Integer} file descriptor used by the ReadStream.

Emitted when the WriteStream's file is opened.

### file.bytesWritten

The number of bytes written so far. Does not include data that is still queued
for writing.

## Class: fs.FSWatcher

Objects returned from `fs.watch()` are of this type.

### watcher.close()

Stop watching for changes on the given `fs.FSWatcher`.

### Event: 'change'

* `event` {String} The type of fs change
* `filename` {String} The filename that changed (if relevant/available)

Emitted when something changes in a watched directory or file.
See more details in [fs.watch](#fs_fs_watch_filename_options_listener).

### Event: 'error'

* `error` {Error object}

Emitted when an error occurs.

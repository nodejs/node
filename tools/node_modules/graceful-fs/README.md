# graceful-fs

graceful-fs functions as a drop-in replacement for the fs module,
making various improvements.

The improvements are meant to normalize behavior across different
platforms and environments, and to make filesystem access more
resilient to errors.

## Improvements over [fs module](http://api.nodejs.org/fs.html)

graceful-fs:

* Queues up `open` and `readdir` calls, and retries them once
  something closes if there is an EMFILE error from too many file
  descriptors.
* fixes `lchmod` for Node versions prior to 0.6.2.
* implements `fs.lutimes` if possible. Otherwise it becomes a noop.
* ignores `EINVAL` and `EPERM` errors in `chown`, `fchown` or
  `lchown` if the user isn't root.
* makes `lchmod` and `lchown` become noops, if not available.
* retries reading a file if `read` results in EAGAIN error.

On Windows, it retries renaming a file for up to one second if `EACCESS`
or `EPERM` error occurs, likely because antivirus software has locked
the directory.

## USAGE

```javascript
// use just like fs
var fs = require('graceful-fs')

// now go and do stuff with it...
fs.readFileSync('some-file-or-whatever')
```

## Global Patching

If you want to patch the global fs module (or any other fs-like
module) you can do this:

```javascript
// Make sure to read the caveat below.
var realFs = require('fs')
var gracefulFs = require('graceful-fs')
gracefulFs.gracefulify(realFs)
```

This should only ever be done at the top-level application layer, in
order to delay on EMFILE errors from any fs-using dependencies.  You
should **not** do this in a library, because it can cause unexpected
delays in other parts of the program.

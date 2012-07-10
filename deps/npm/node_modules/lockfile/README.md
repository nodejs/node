# lockfile

A very polite lock file utility, which endeavors to not litter, and to
wait patiently for others.

## Usage

```javascript
var lockFile = require('lockfile')

// opts is optional, and defaults to {}
lockFile.lock('some-file.lock', opts, function (er, fd) {
  // if the er happens, then it failed to acquire a lock.
  // if there was not an error, then the fd is opened in
  // wx mode.  If you want to write something to it, go ahead.

  // do my stuff, free of interruptions
  // then, some time later, do:
  lockFile.unlock('some-file.lock', function (er) {
    // er means that an error happened, and is probably bad.
  })
})
```

## Methods

Sync methods return the value/throw the error, others don't.  Standard
node fs stuff.

All known locks are removed when the process exits.  Of course, it's
possible for certain types of failures to cause this to fail, but a best
effort is made to not be a litterbug.

### lockFile.lock(path, [opts], cb)

Acquire a file lock on the specified path.  Returns the FD.

### lockFile.lockSync(path, [opts])

Acquire a file lock on the specified path

### lockFile.unlock(path, cb)

Close and unlink the lockfile.

### lockFile.unlockSync(path)

Close and unlink the lockfile.

### lockFile.check(path, [opts], cb)

Check if the lockfile is locked and not stale.

Returns boolean.

### lockFile.checkSync(path, [opts], cb)

Check if the lockfile is locked and not stale.

Callback is called with `cb(error, isLocked)`.

## Options

### opts.wait

A number of milliseconds to wait for locks to expire before giving up.
Only used by lockFile.lock.  Relies on fs.watch.  If the lock is not
cleared by the time the wait expires, then it returns with the original
error.

### opts.stale

A number of milliseconds before locks are considered to have expired.

### opts.retries

Used by lock and lockSync.  Retry `n` number of times before giving up.

### opts.retryWait

Used by lock.  Wait `n` milliseconds before retrying.

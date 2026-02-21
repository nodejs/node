const fs = require('node:fs/promises')
const { rmdirSync } = require('node:fs')
const promiseRetry = require('promise-retry')
const { onExit } = require('signal-exit')

// a lockfile implementation inspired by the unmaintained proper-lockfile library
//
// similarities:
// - based on mkdir's atomicity
// - works across processes and even machines (via NFS)
// - cleans up after itself
// - detects compromised locks
//
// differences:
// - higher-level API (just a withLock function)
// - written in async/await style
// - uses mtime + inode for more reliable compromised lock detection
// - more ergonomic compromised lock handling (i.e. withLock will reject, and callbacks have access to an AbortSignal)
// - uses a more recent version of signal-exit

const touchInterval = 1_000
// mtime precision is platform dependent, so use a reasonably large threshold
const staleThreshold = 5_000

// track current locks and their cleanup functions
const currentLocks = new Map()

function cleanupLocks () {
  for (const [, cleanup] of currentLocks) {
    try {
      cleanup()
    } catch (err) {
      //
    }
  }
}

// clean up any locks that were not released normally
onExit(cleanupLocks)

/**
 * Acquire an advisory lock for the given path and hold it for the duration of the callback.
 *
 * The lock will be released automatically when the callback resolves or rejects.
 * Concurrent calls to withLock() for the same path will wait until the lock is released.
 */
async function withLock (lockPath, cb) {
  try {
    const signal = await acquireLock(lockPath)
    return await new Promise((resolve, reject) => {
      signal.addEventListener('abort', () => {
        reject(Object.assign(new Error('Lock compromised'), { code: 'ECOMPROMISED' }))
      });

      (async () => {
        try {
          resolve(await cb(signal))
        } catch (err) {
          reject(err)
        }
      })()
    })
  } finally {
    releaseLock(lockPath)
  }
}

function acquireLock (lockPath) {
  return promiseRetry({
    minTimeout: 100,
    maxTimeout: 5_000,
    // if another process legitimately holds the lock, wait for it to release; if it dies abnormally and the lock becomes stale, we'll acquire it automatically
    forever: true,
  }, async (retry) => {
    try {
      await fs.mkdir(lockPath)
    } catch (err) {
      if (err.code !== 'EEXIST' && err.code !== 'EBUSY' && err.code !== 'EPERM') {
        throw err
      }

      const status = await getLockStatus(lockPath)

      if (status === 'locked') {
        // let's see if we can acquire it on the next attempt 🤞
        return retry(err)
      }
      if (status === 'stale') {
        try {
          // there is a very tiny window where another process could also release the stale lock and acquire it before we release it here; the lock compromise checker should detect this and throw an error
          deleteLock(lockPath)
        } catch (e) {
          // on windows, EBUSY/EPERM can happen if another process is (re)creating the lock; maybe we can acquire it on a subsequent attempt 🤞
          if (e.code === 'EBUSY' || e.code === 'EPERM') {
            return retry(e)
          }
          throw e
        }
      }
      // immediately attempt to acquire the lock (no backoff)
      return await acquireLock(lockPath)
    }
    try {
      const signal = await maintainLock(lockPath)
      return signal
    } catch (err) {
      throw Object.assign(new Error('Lock compromised'), { code: 'ECOMPROMISED' })
    }
  })
}

function deleteLock (lockPath) {
  try {
    // synchronous, so we can call in an exit handler
    rmdirSync(lockPath)
  } catch (err) {
    if (err.code !== 'ENOENT') {
      throw err
    }
  }
}

function releaseLock (lockPath) {
  currentLocks.get(lockPath)?.()
  currentLocks.delete(lockPath)
}

async function getLockStatus (lockPath) {
  try {
    const stat = await fs.stat(lockPath)
    return (Date.now() - stat.mtimeMs > staleThreshold) ? 'stale' : 'locked'
  } catch (err) {
    if (err.code === 'ENOENT') {
      return 'unlocked'
    }
    throw err
  }
}

async function maintainLock (lockPath) {
  const controller = new AbortController()
  const stats = await fs.stat(lockPath)
  // fs.utimes operates on floating points seconds (directly, or via strings/Date objects), which may not match the underlying filesystem's mtime precision, meaning that we might read a slightly different mtime than we write. always round to the nearest second, since all filesystems support at least second precision
  let mtime = Math.round(stats.mtimeMs / 1000)
  const signal = controller.signal

  async function touchLock () {
    try {
      const currentStats = (await fs.stat(lockPath))
      const currentMtime = Math.round(currentStats.mtimeMs / 1000)
      if (currentStats.ino !== stats.ino || currentMtime !== mtime) {
        throw new Error('Lock compromised')
      }
      mtime = Math.round(Date.now() / 1000)
      // touch the lock, unless we just released it during this iteration
      if (currentLocks.has(lockPath)) {
        await fs.utimes(lockPath, mtime, mtime)
      }
    } catch (err) {
      // stats mismatch or other fs error means the lock was compromised
      controller.abort()
    }
  }

  const timeout = setInterval(touchLock, touchInterval)
  timeout.unref()
  function cleanup () {
    clearInterval(timeout)
    deleteLock(lockPath)
  }
  currentLocks.set(lockPath, cleanup)
  return signal
}

module.exports = withLock

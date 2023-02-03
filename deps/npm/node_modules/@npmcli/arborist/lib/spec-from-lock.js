const npa = require('npm-package-arg')

// extracted from npm v6 lib/install/realize-shrinkwrap-specifier.js
const specFromLock = (name, lock, where) => {
  try {
    if (lock.version) {
      const spec = npa.resolve(name, lock.version, where)
      if (lock.integrity || spec.type === 'git') {
        return spec
      }
    }
    if (lock.from) {
      // legacy metadata includes "from", but not integrity
      const spec = npa.resolve(name, lock.from, where)
      if (spec.registry && lock.version) {
        return npa.resolve(name, lock.version, where)
      } else if (!lock.resolved) {
        return spec
      }
    }
    if (lock.resolved) {
      return npa.resolve(name, lock.resolved, where)
    }
  } catch {
    // ignore errors
  }
  try {
    return npa.resolve(name, lock.version, where)
  } catch {
    return {}
  }
}

module.exports = specFromLock

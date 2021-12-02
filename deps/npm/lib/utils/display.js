const { inspect } = require('util')
const npmlog = require('npmlog')
const log = require('./log-shim.js')
const { explain } = require('./explain-eresolve.js')

const _logHandler = Symbol('logHandler')
const _eresolveWarn = Symbol('eresolveWarn')
const _log = Symbol('log')
const _npmlog = Symbol('npmlog')

class Display {
  constructor () {
    // pause by default until config is loaded
    this.on()
    log.pause()
  }

  on () {
    process.on('log', this[_logHandler])
  }

  off () {
    process.off('log', this[_logHandler])
    // Unbalanced calls to enable/disable progress
    // will leave change listeners on the tracker
    // This pretty much only happens in tests but
    // this removes the event emitter listener warnings
    log.tracker.removeAllListeners()
  }

  load (config) {
    const {
      color,
      timing,
      loglevel,
      unicode,
      progress,
      heading = 'npm',
    } = config

    // XXX: decouple timing from loglevel
    if (timing && loglevel === 'notice') {
      log.level = 'timing'
    } else {
      log.level = loglevel
    }

    log.heading = heading

    if (color) {
      log.enableColor()
    } else {
      log.disableColor()
    }

    if (unicode) {
      log.enableUnicode()
    } else {
      log.disableUnicode()
    }

    // if it's more than error, don't show progress
    const silent = log.levels[log.level] > log.levels.error
    if (progress && !silent) {
      log.enableProgress()
    } else {
      log.disableProgress()
    }

    // Resume displaying logs now that we have config
    log.resume()
  }

  log (...args) {
    this[_logHandler](...args)
  }

  [_logHandler] = (level, ...args) => {
    try {
      this[_log](level, ...args)
    } catch (ex) {
      try {
        // if it crashed once, it might again!
        this[_npmlog]('verbose', `attempt to log ${inspect(args)} crashed`, ex)
      } catch (ex2) {
        // eslint-disable-next-line no-console
        console.error(`attempt to log ${inspect(args)} crashed`, ex, ex2)
      }
    }
  }

  [_log] (...args) {
    return this[_eresolveWarn](...args) || this[_npmlog](...args)
  }

  // Explicitly call these on npmlog and not log shim
  // This is the final place we should call npmlog before removing it.
  [_npmlog] (level, ...args) {
    npmlog[level](...args)
  }

  // Also (and this is a really inexcusable kludge), we patch the
  // log.warn() method so that when we see a peerDep override
  // explanation from Arborist, we can replace the object with a
  // highly abbreviated explanation of what's being overridden.
  [_eresolveWarn] (level, heading, message, expl) {
    if (level === 'warn' &&
        heading === 'ERESOLVE' &&
        expl && typeof expl === 'object'
    ) {
      this[_npmlog](level, heading, message)
      this[_npmlog](level, '', explain(expl, log.useColor(), 2))
      // Return true to short circuit other log in chain
      return true
    }
  }
}

module.exports = Display

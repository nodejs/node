const { inspect } = require('util')
const npmlog = require('npmlog')
const log = require('./log-shim.js')
const { explain } = require('./explain-eresolve.js')

class Display {
  constructor () {
    // pause by default until config is loaded
    this.on()
    log.pause()
  }

  on () {
    process.on('log', this.#logHandler)
  }

  off () {
    process.off('log', this.#logHandler)
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
      silent,
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

    // if it's silent, don't show progress
    if (progress && !silent) {
      log.enableProgress()
    } else {
      log.disableProgress()
    }

    // Resume displaying logs now that we have config
    log.resume()
  }

  log (...args) {
    this.#logHandler(...args)
  }

  #logHandler = (level, ...args) => {
    try {
      this.#log(level, ...args)
    } catch (ex) {
      try {
        // if it crashed once, it might again!
        this.#npmlog('verbose', `attempt to log ${inspect(args)} crashed`, ex)
      } catch (ex2) {
        // eslint-disable-next-line no-console
        console.error(`attempt to log ${inspect(args)} crashed`, ex, ex2)
      }
    }
  }

  #log (...args) {
    return this.#eresolveWarn(...args) || this.#npmlog(...args)
  }

  // Explicitly call these on npmlog and not log shim
  // This is the final place we should call npmlog before removing it.
  #npmlog (level, ...args) {
    npmlog[level](...args)
  }

  // Also (and this is a really inexcusable kludge), we patch the
  // log.warn() method so that when we see a peerDep override
  // explanation from Arborist, we can replace the object with a
  // highly abbreviated explanation of what's being overridden.
  #eresolveWarn (level, heading, message, expl) {
    if (level === 'warn' &&
        heading === 'ERESOLVE' &&
        expl && typeof expl === 'object'
    ) {
      this.#npmlog(level, heading, message)
      this.#npmlog(level, '', explain(expl, log.useColor(), 2))
      // Return true to short circuit other log in chain
      return true
    }
  }
}

module.exports = Display

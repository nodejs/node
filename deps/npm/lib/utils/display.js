const { inspect } = require('util')
const npmlog = require('npmlog')
const log = require('./log-shim.js')
const { explain } = require('./explain-eresolve.js')

class Display {
  #chalk = null

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
      chalk,
      timing,
      loglevel,
      unicode,
      progress,
      silent,
      heading = 'npm',
    } = config

    this.#chalk = chalk

    // npmlog is still going away someday, so this is a hack to dynamically
    // set the loglevel of timing based on the timing flag, instead of making
    // a breaking change to npmlog. The result is that timing logs are never
    // shown except when the --timing flag is set. We also need to change
    // the index of the silly level since otherwise it is set to -Infinity
    // and we can't go any lower than that. silent is still set to Infinify
    // because we DO want silent to hide timing levels. This allows for the
    // special case of getting timing information while hiding all CLI output
    // in order to get perf information that might be affected by writing to
    // a terminal. XXX(npmlog): this will be removed along with npmlog
    log.levels.silly = -10000
    log.levels.timing = log.levels[loglevel] + (timing ? 1 : -1)

    log.level = loglevel
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
      this.#npmlog(level, '', explain(expl, this.#chalk, 2))
      // Return true to short circuit other log in chain
      return true
    }
  }
}

module.exports = Display

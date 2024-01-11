const { inspect } = require('util')
const npmlog = require('npmlog')
const log = require('./log-shim.js')
const { explain } = require('./explain-eresolve.js')

const originalCustomInspect = Symbol('npm.display.original.util.inspect.custom')

// These are most assuredly not a mistake
// https://eslint.org/docs/latest/rules/no-control-regex
/* eslint-disable no-control-regex */
// \x00 through \x1f, \x7f through \x9f, not including \x09 \x0a \x0b \x0d
const hasC01 = /[\x00-\x08\x0c\x0e-\x1f\x7f-\x9f]/
// Allows everything up to '[38;5;255m' in 8 bit notation
const allowedSGR = /^\[[0-9;]{0,8}m/
// '[38;5;255m'.length
const sgrMaxLen = 10

// Strips all ANSI C0 and C1 control characters (except for SGR up to 8 bit)
function stripC01 (str) {
  if (!hasC01.test(str)) {
    return str
  }
  let result = ''
  for (let i = 0; i < str.length; i++) {
    const char = str[i]
    const code = char.charCodeAt(0)
    if (!hasC01.test(char)) {
      // Most characters are in this set so continue early if we can
      result = `${result}${char}`
    } else if (code === 27 && allowedSGR.test(str.slice(i + 1, i + sgrMaxLen + 1))) {
      // \x1b with allowed SGR
      result = `${result}\x1b`
    } else if (code <= 31) {
      // escape all other C0 control characters besides \x7f
      result = `${result}^${String.fromCharCode(code + 64)}`
    } else {
      // hasC01 ensures this is now a C1 control character or \x7f
      result = `${result}^${String.fromCharCode(code - 64)}`
    }
  }
  return result
}

class Display {
  #chalk = null

  constructor () {
    // pause by default until config is loaded
    this.on()
    log.pause()
  }

  static clean (output) {
    if (typeof output === 'string') {
      // Strings are cleaned inline
      return stripC01(output)
    }
    if (!output || typeof output !== 'object') {
      // Numbers, booleans, null all end up here and don't need cleaning
      return output
    }
    // output && typeof output === 'object'
    // We can't use hasOwn et al for detecting the original but we can use it
    // for detecting the properties we set via defineProperty
    if (
      output[inspect.custom] &&
      (!Object.hasOwn(output, originalCustomInspect))
    ) {
      // Save the old one if we didn't already do it.
      Object.defineProperty(output, originalCustomInspect, {
        value: output[inspect.custom],
        writable: true,
      })
    }
    if (!Object.hasOwn(output, originalCustomInspect)) {
      // Put a dummy one in for when we run multiple times on the same object
      Object.defineProperty(output, originalCustomInspect, {
        value: function () {
          return this
        },
        writable: true,
      })
    }
    // Set the custom inspect to our own function
    Object.defineProperty(output, inspect.custom, {
      value: function () {
        const toClean = this[originalCustomInspect]()
        // Custom inspect can return things other than objects, check type again
        if (typeof toClean === 'string') {
          // Strings are cleaned inline
          return stripC01(toClean)
        }
        if (!toClean || typeof toClean !== 'object') {
          // Numbers, booleans, null all end up here and don't need cleaning
          return toClean
        }
        return stripC01(inspect(toClean, { customInspect: false }))
      },
      writable: true,
    })
    return output
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
    npmlog[level](...args.map(Display.clean))
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

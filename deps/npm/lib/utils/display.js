const proggy = require('proggy')
const { log, output, META } = require('proc-log')
const { explain } = require('./explain-eresolve.js')
const { formatWithOptions } = require('./format')

// This is the general approach to color:
// Eventually this will be exposed somewhere we can refer to these by name.
// Foreground colors only. Never set the background color.
/*
 * Black # (Don't use)
 * Red # Danger
 * Green # Success
 * Yellow # Warning
 * Blue # Accent
 * Magenta # Done
 * Cyan # Emphasis
 * White # (Don't use)
 */

// Translates log levels to chalk colors
const COLOR_PALETTE = ({ chalk: c }) => ({
  heading: c.bold,
  title: c.blueBright,
  timing: c.magentaBright,
  // loglevels
  error: c.red,
  warn: c.yellow,
  notice: c.cyanBright,
  http: c.green,
  info: c.cyan,
  verbose: c.blue,
  silly: c.blue.dim,
})

const LEVEL_OPTIONS = {
  silent: {
    index: 0,
  },
  error: {
    index: 1,
  },
  warn: {
    index: 2,
  },
  notice: {
    index: 3,
  },
  http: {
    index: 4,
  },
  info: {
    index: 5,
  },
  verbose: {
    index: 6,
  },
  silly: {
    index: 7,
  },
}

const LEVEL_METHODS = {
  ...LEVEL_OPTIONS,
  [log.KEYS.timing]: {
    show: ({ timing, index }) => !!timing && index !== 0,
  },
}

const tryJsonParse = (value) => {
  if (typeof value === 'string') {
    try {
      return JSON.parse(value)
    } catch {
      return {}
    }
  }
  return value
}

const setBlocking = (stream) => {
  // Copied from https://github.com/yargs/set-blocking
  // https://raw.githubusercontent.com/yargs/set-blocking/master/LICENSE.txt
  /* istanbul ignore next - we trust that this works */
  if (stream._handle && stream.isTTY && typeof stream._handle.setBlocking === 'function') {
    stream._handle.setBlocking(true)
  }
  return stream
}

const withMeta = (handler) => (level, ...args) => {
  let meta = {}
  const last = args.at(-1)
  if (last && typeof last === 'object' && Object.hasOwn(last, META)) {
    meta = args.pop()
  }
  return handler(level, meta, ...args)
}

class Display {
  #logState = {
    buffering: true,
    buffer: [],
  }

  #outputState = {
    buffering: true,
    buffer: [],
  }

  // colors
  #noColorChalk
  #stdoutChalk
  #stdoutColor
  #stderrChalk
  #stderrColor
  #logColors

  // progress
  #progress

  // options
  #levelIndex
  #timing
  #json
  #heading
  #silent

  // display streams
  #stdout
  #stderr

  constructor ({ stdout, stderr }) {
    this.#stdout = setBlocking(stdout)
    this.#stderr = setBlocking(stderr)

    // Handlers are set immediately so they can buffer all events
    process.on('log', this.#logHandler)
    process.on('output', this.#outputHandler)
  }

  off () {
    process.off('log', this.#logHandler)
    this.#logState.buffer.length = 0

    process.off('output', this.#outputHandler)
    this.#outputState.buffer.length = 0

    if (this.#progress) {
      this.#progress.stop()
    }
  }

  get chalk () {
    return {
      noColor: this.#noColorChalk,
      stdout: this.#stdoutChalk,
      stderr: this.#stderrChalk,
    }
  }

  async load ({
    heading,
    json,
    loglevel,
    progress,
    stderrColor,
    stdoutColor,
    timing,
    unicode,
  }) {
    // get createSupportsColor from chalk directly if this lands
    // https://github.com/chalk/chalk/pull/600
    const [{ Chalk }, { createSupportsColor }] = await Promise.all([
      import('chalk'),
      import('supports-color'),
    ])
    // we get the chalk level based on a null stream meaning chalk will only use
    // what it knows about the environment to get color support since we already
    // determined in our definitions that we want to show colors.
    const level = Math.max(createSupportsColor(null).level, 1)

    this.#noColorChalk = new Chalk({ level: 0 })

    this.#stdoutColor = stdoutColor
    this.#stdoutChalk = stdoutColor ? new Chalk({ level }) : this.#noColorChalk

    this.#stderrColor = stderrColor
    this.#stderrChalk = stderrColor ? new Chalk({ level }) : this.#noColorChalk

    this.#logColors = COLOR_PALETTE({ chalk: this.#stderrChalk })

    this.#levelIndex = LEVEL_OPTIONS[loglevel].index
    this.#timing = timing
    this.#json = json
    this.#heading = heading
    this.#silent = this.#levelIndex <= 0

    // Emit resume event on the logs which will flush output
    log.resume()
    output.flush()
    this.#startProgress({ progress, unicode })
  }

  // STREAM WRITES

  // Write formatted and (non-)colorized output to streams
  #stdoutWrite (options, ...args) {
    this.#stdout.write(formatWithOptions({ colors: this.#stdoutColor, ...options }, ...args))
  }

  #stderrWrite (options, ...args) {
    this.#stderr.write(formatWithOptions({ colors: this.#stderrColor, ...options }, ...args))
  }

  // HANDLERS

  // Arrow function assigned to a private class field so it can be passed
  // directly as a listener and still reference "this"
  #logHandler = withMeta((level, meta, ...args) => {
    if (level === log.KEYS.resume) {
      this.#logState.buffering = false
      this.#logState.buffer.forEach((item) => this.#tryWriteLog(...item))
      this.#logState.buffer.length = 0
      return
    }

    if (level === log.KEYS.pause) {
      this.#logState.buffering = true
      return
    }

    if (this.#logState.buffering) {
      this.#logState.buffer.push([level, meta, ...args])
      return
    }

    this.#tryWriteLog(level, meta, ...args)
  })

  // Arrow function assigned to a private class field so it can be passed
  // directly as a listener and still reference "this"
  #outputHandler = withMeta((level, meta, ...args) => {
    if (level === output.KEYS.flush) {
      this.#outputState.buffering = false

      if (meta.jsonError && this.#json) {
        const json = {}
        for (const item of this.#outputState.buffer) {
          // index 2 skips the level and meta
          Object.assign(json, tryJsonParse(item[2]))
        }
        this.#writeOutput(
          output.KEYS.standard,
          meta,
          JSON.stringify({ ...json, error: meta.jsonError }, null, 2)
        )
      } else {
        this.#outputState.buffer.forEach((item) => this.#writeOutput(...item))
      }

      this.#outputState.buffer.length = 0
      return
    }

    if (level === output.KEYS.buffer) {
      this.#outputState.buffer.push([output.KEYS.standard, meta, ...args])
      return
    }

    if (this.#outputState.buffering) {
      this.#outputState.buffer.push([level, meta, ...args])
      return
    }

    // HACK: if it looks like the banner and we are silent do not print it.
    // There's no other way to do this right now :(
    // eslint-disable-next-line max-len
    if (this.#silent && args.length === 1 && args[0].startsWith('\n> ') && args[0].endsWith('\n')) {
      return
    }

    this.#writeOutput(level, meta, ...args)
  })

  // OUTPUT

  #writeOutput (level, meta, ...args) {
    if (level === output.KEYS.standard) {
      this.#stdoutWrite({}, ...args)
      return
    }

    if (level === output.KEYS.error) {
      this.#stderrWrite({}, ...args)
    }
  }

  // LOGS

  #tryWriteLog (level, meta, ...args) {
    try {
      // Also (and this is a really inexcusable kludge), we patch the
      // log.warn() method so that when we see a peerDep override
      // explanation from Arborist, we can replace the object with a
      // highly abbreviated explanation of what's being overridden.
      // TODO: this could probably be moved to arborist now that display is refactored
      const [heading, message, expl] = args
      if (level === log.KEYS.warn && heading === 'ERESOLVE' && expl && typeof expl === 'object') {
        this.#writeLog(level, meta, heading, message)
        this.#writeLog(level, meta, '', explain(expl, this.#stderrChalk, 2))
        return
      }
      this.#writeLog(level, meta, ...args)
    } catch (ex) {
      try {
        // if it crashed once, it might again!
        this.#writeLog(log.KEYS.verbose, meta, '', `attempt to log crashed`, ...args, ex)
      } catch (ex2) {
        // This happens if the object has an inspect method that crashes so just console.error
        // with the errors but don't do anything else that might error again.
        // eslint-disable-next-line no-console
        console.error(`attempt to log crashed`, ex, ex2)
      }
    }
  }

  #writeLog (level, meta, ...args) {
    const levelOpts = LEVEL_METHODS[level]
    const show = levelOpts.show ?? (({ index }) => levelOpts.index <= index)
    const force = meta.force && !this.#silent

    if (force || show({ index: this.#levelIndex, timing: this.#timing })) {
      // this mutates the array so we can pass args directly to format later
      const title = args.shift()
      const prefix = [
        this.#logColors.heading(this.#heading),
        this.#logColors[level](level),
        title ? this.#logColors.title(title) : null,
      ]
      this.#stderrWrite({ prefix }, ...args)
    } else if (this.#progress) {
      // TODO: make this display a single log line of filtered messages
    }
  }

  // PROGRESS

  #startProgress ({ progress, unicode }) {
    if (!progress || this.#silent) {
      return
    }
    this.#progress = proggy.createClient({ normalize: true })
    // TODO: implement proggy trackers in arborist/doctor
    // TODO: listen to progress events here and build progress UI
    // TODO: see deprecated gauge package for what unicode chars were used
  }
}

module.exports = Display

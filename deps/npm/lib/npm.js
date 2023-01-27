const Arborist = require('@npmcli/arborist')
const EventEmitter = require('events')
const { resolve, dirname, join } = require('path')
const Config = require('@npmcli/config')
const chalk = require('chalk')
const which = require('which')
const fs = require('fs/promises')

// Patch the global fs module here at the app level
require('graceful-fs').gracefulify(require('fs'))

const { definitions, flatten, shorthands } = require('./utils/config/index.js')
const usage = require('./utils/npm-usage.js')
const LogFile = require('./utils/log-file.js')
const Timers = require('./utils/timers.js')
const Display = require('./utils/display.js')
const log = require('./utils/log-shim')
const replaceInfo = require('./utils/replace-info.js')
const updateNotifier = require('./utils/update-notifier.js')
const pkg = require('../package.json')
const cmdList = require('./utils/cmd-list.js')

class Npm extends EventEmitter {
  static get version () {
    return pkg.version
  }

  updateNotification = null
  loadErr = null
  argv = []

  #command = null
  #runId = new Date().toISOString().replace(/[.:]/g, '_')
  #loadPromise = null
  #tmpFolder = null
  #title = 'npm'
  #argvClean = []
  #chalk = null
  #npmRoot = null
  #warnedNonDashArg = false

  #outputBuffer = []
  #logFile = new LogFile()
  #display = new Display()
  #timers = new Timers({
    start: 'npm',
    listener: (name, ms) => {
      const args = ['timing', name, `Completed in ${ms}ms`]
      this.#logFile.log(...args)
      this.#display.log(...args)
    },
  })

  // all these options are only used by tests in order to make testing more
  // closely resemble real world usage. for now, npm has no programmatic API so
  // it is ok to add stuff here, but we should not rely on it more than
  // necessary. XXX: make these options not necessary by refactoring @npmcli/config
  //   - npmRoot: this is where npm looks for docs files and the builtin config
  //   - argv: this allows tests to extend argv in the same way the argv would
  //     be passed in via a CLI arg.
  //   - excludeNpmCwd: this is a hack to get @npmcli/config to stop walking up
  //     dirs to set a local prefix when it encounters the `npmRoot`. this
  //     allows tests created by tap inside this repo to not set the local
  //     prefix to `npmRoot` since that is the first dir it would encounter when
  //     doing implicit detection
  constructor ({ npmRoot = dirname(__dirname), argv = [], excludeNpmCwd = false } = {}) {
    super()
    this.#npmRoot = npmRoot
    this.config = new Config({
      npmPath: this.#npmRoot,
      definitions,
      flatten,
      shorthands,
      argv: [...process.argv, ...argv],
      excludeNpmCwd,
    })
  }

  get version () {
    return this.constructor.version
  }

  deref (c) {
    if (!c) {
      return
    }
    if (c.match(/[A-Z]/)) {
      c = c.replace(/([A-Z])/g, m => '-' + m.toLowerCase())
    }
    if (cmdList.plumbing.indexOf(c) !== -1) {
      return c
    }
    // first deref the abbrev, if there is one
    // then resolve any aliases
    // so `npm install-cl` will resolve to `install-clean` then to `ci`
    let a = cmdList.abbrevs[c]
    while (cmdList.aliases[a]) {
      a = cmdList.aliases[a]
    }
    return a
  }

  // Get an instantiated npm command
  // npm.command is already taken as the currently running command, a refactor
  // would be needed to change this
  async cmd (cmd) {
    await this.load()

    const cmdId = this.deref(cmd)
    if (!cmdId) {
      throw Object.assign(new Error(`Unknown command ${cmd}`), {
        code: 'EUNKNOWNCOMMAND',
      })
    }

    const Impl = require(`./commands/${cmdId}.js`)
    const command = new Impl(this)

    // since 'test', 'start', 'stop', etc. commands re-enter this function
    // to call the run-script command, we need to only set it one time.
    if (!this.#command) {
      this.#command = command
      process.env.npm_command = this.command
    }

    return command
  }

  // Call an npm command
  async exec (cmd, args = this.argv) {
    const command = await this.cmd(cmd)
    const timeEnd = this.time(`command:${cmd}`)

    // this is async but we dont await it, since its ok if it doesnt
    // finish before the command finishes running. it uses command and argv
    // so it must be initiated here, after the command name is set
    // eslint-disable-next-line promise/catch-or-return
    updateNotifier(this).then((msg) => (this.updateNotification = msg))

    // Options are prefixed by a hyphen-minus (-, \u2d).
    // Other dash-type chars look similar but are invalid.
    if (!this.#warnedNonDashArg) {
      const nonDashArgs = args.filter(a => /^[\u2010-\u2015\u2212\uFE58\uFE63\uFF0D]/.test(a))
      if (nonDashArgs.length) {
        this.#warnedNonDashArg = true
        log.error(
          'arg',
          'Argument starts with non-ascii dash, this is probably invalid:',
          nonDashArgs.join(', ')
        )
      }
    }

    return command.cmdExec(args).finally(timeEnd)
  }

  async load () {
    if (!this.#loadPromise) {
      this.#loadPromise = this.time('npm:load', () => this.#load().catch((er) => {
        this.loadErr = er
        throw er
      }))
    }
    return this.#loadPromise
  }

  get loaded () {
    return this.config.loaded
  }

  // This gets called at the end of the exit handler and
  // during any tests to cleanup all of our listeners
  // Everything in here should be synchronous
  unload () {
    this.#timers.off()
    this.#display.off()
    this.#logFile.off()
  }

  time (name, fn) {
    return this.#timers.time(name, fn)
  }

  writeTimingFile () {
    this.#timers.writeFile({
      id: this.#runId,
      command: this.#argvClean,
      logfiles: this.logFiles,
      version: this.version,
    })
  }

  get title () {
    return this.#title
  }

  set title (t) {
    process.title = t
    this.#title = t
  }

  async #load () {
    await this.time('npm:load:whichnode', async () => {
      // TODO should we throw here?
      const node = await which(process.argv[0]).catch(() => {})
      if (node && node.toUpperCase() !== process.execPath.toUpperCase()) {
        log.verbose('node symlink', node)
        process.execPath = node
        this.config.execPath = node
      }
    })

    await this.time('npm:load:configload', () => this.config.load())

    // mkdir this separately since the logs dir can be set to
    // a different location. if this fails, then we don't have
    // a cache dir, but we don't want to fail immediately since
    // the command might not need a cache dir (like `npm --version`)
    await this.time('npm:load:mkdirpcache', () =>
      fs.mkdir(this.cache, { recursive: true })
        .catch((e) => log.verbose('cache', `could not create cache: ${e}`)))

    // its ok if this fails. user might have specified an invalid dir
    // which we will tell them about at the end
    await this.time('npm:load:mkdirplogs', () =>
      fs.mkdir(this.logsDir, { recursive: true })
        .catch((e) => log.verbose('logfile', `could not create logs-dir: ${e}`)))

    // note: this MUST be shorter than the actual argv length, because it
    // uses the same memory, so node will truncate it if it's too long.
    this.time('npm:load:setTitle', () => {
      const { parsedArgv: { cooked, remain } } = this.config
      this.argv = remain
      // Secrets are mostly in configs, so title is set using only the positional args
      // to keep those from being leaked.
      this.title = ['npm'].concat(replaceInfo(remain)).join(' ').trim()
      // The cooked argv is also logged separately for debugging purposes. It is
      // cleaned as a best effort by replacing known secrets like basic auth
      // password and strings that look like npm tokens. XXX: for this to be
      // safer the config should create a sanitized version of the argv as it
      // has the full context of what each option contains.
      this.#argvClean = replaceInfo(cooked)
      log.verbose('title', this.title)
      log.verbose('argv', this.#argvClean.map(JSON.stringify).join(' '))
    })

    this.time('npm:load:display', () => {
      this.#display.load({
        // Use logColor since that is based on stderr
        color: this.logColor,
        progress: this.flatOptions.progress,
        silent: this.silent,
        timing: this.config.get('timing'),
        loglevel: this.config.get('loglevel'),
        unicode: this.config.get('unicode'),
        heading: this.config.get('heading'),
      })
      process.env.COLOR = this.color ? '1' : '0'
    })

    this.time('npm:load:logFile', () => {
      this.#logFile.load({
        path: this.logPath,
        logsMax: this.config.get('logs-max'),
      })
      log.verbose('logfile', this.#logFile.files[0] || 'no logfile created')
    })

    this.time('npm:load:timers', () =>
      this.#timers.load({
        path: this.config.get('timing') ? this.logPath : null,
      })
    )

    this.time('npm:load:configScope', () => {
      const configScope = this.config.get('scope')
      if (configScope && !/^@/.test(configScope)) {
        this.config.set('scope', `@${configScope}`, this.config.find('scope'))
      }
    })

    if (this.config.get('force')) {
      log.warn('using --force', 'Recommended protections disabled.')
    }
  }

  get isShellout () {
    return this.#command?.constructor?.isShellout
  }

  get command () {
    return this.#command?.name
  }

  get flatOptions () {
    const { flat } = this.config
    // the Arborist constructor is used almost everywhere we call pacote, it's
    // easiest to attach it to flatOptions so it goes everywhere without having
    // to touch every call
    flat.Arborist = Arborist
    flat.nodeVersion = process.version
    flat.npmVersion = pkg.version
    if (this.command) {
      flat.npmCommand = this.command
    }
    return flat
  }

  // color and logColor are a special derived values that takes into
  // consideration not only the config, but whether or not we are operating
  // in a tty with the associated output (stdout/stderr)
  get color () {
    return this.flatOptions.color
  }

  get logColor () {
    return this.flatOptions.logColor
  }

  get chalk () {
    if (!this.#chalk) {
      let level = chalk.level
      if (!this.color) {
        level = 0
      }
      this.#chalk = new chalk.Instance({ level })
    }
    return this.#chalk
  }

  get global () {
    return this.config.get('global') || this.config.get('location') === 'global'
  }

  get silent () {
    return this.flatOptions.silent
  }

  get lockfileVersion () {
    return 2
  }

  get unfinishedTimers () {
    return this.#timers.unfinished
  }

  get finishedTimers () {
    return this.#timers.finished
  }

  get started () {
    return this.#timers.started
  }

  get logFiles () {
    return this.#logFile.files
  }

  get logsDir () {
    return this.config.get('logs-dir') || join(this.cache, '_logs')
  }

  get logPath () {
    return resolve(this.logsDir, `${this.#runId}-`)
  }

  get timingFile () {
    return this.#timers.file
  }

  get npmRoot () {
    return this.#npmRoot
  }

  get cache () {
    return this.config.get('cache')
  }

  set cache (r) {
    this.config.set('cache', r)
  }

  get globalPrefix () {
    return this.config.globalPrefix
  }

  set globalPrefix (r) {
    this.config.globalPrefix = r
  }

  get localPrefix () {
    return this.config.localPrefix
  }

  set localPrefix (r) {
    this.config.localPrefix = r
  }

  get localPackage () {
    return this.config.localPackage
  }

  get globalDir () {
    return process.platform !== 'win32'
      ? resolve(this.globalPrefix, 'lib', 'node_modules')
      : resolve(this.globalPrefix, 'node_modules')
  }

  get localDir () {
    return resolve(this.localPrefix, 'node_modules')
  }

  get dir () {
    return this.global ? this.globalDir : this.localDir
  }

  get globalBin () {
    const b = this.globalPrefix
    return process.platform !== 'win32' ? resolve(b, 'bin') : b
  }

  get localBin () {
    return resolve(this.dir, '.bin')
  }

  get bin () {
    return this.global ? this.globalBin : this.localBin
  }

  get prefix () {
    return this.global ? this.globalPrefix : this.localPrefix
  }

  set prefix (r) {
    const k = this.global ? 'globalPrefix' : 'localPrefix'
    this[k] = r
  }

  get usage () {
    return usage(this)
  }

  // XXX add logging to see if we actually use this
  get tmp () {
    if (!this.#tmpFolder) {
      const rand = require('crypto').randomBytes(4).toString('hex')
      this.#tmpFolder = `npm-${process.pid}-${rand}`
    }
    return resolve(this.config.get('tmp'), this.#tmpFolder)
  }

  // output to stdout in a progress bar compatible way
  output (...msg) {
    log.clearProgress()
    // eslint-disable-next-line no-console
    console.log(...msg)
    log.showProgress()
  }

  outputBuffer (item) {
    this.#outputBuffer.push(item)
  }

  flushOutput (jsonError) {
    if (!jsonError && !this.#outputBuffer.length) {
      return
    }

    if (this.config.get('json')) {
      const jsonOutput = this.#outputBuffer.reduce((acc, item) => {
        if (typeof item === 'string') {
          // try to parse it as json in case its a string
          try {
            item = JSON.parse(item)
          } catch {
            return acc
          }
        }
        return { ...acc, ...item }
      }, {})
      this.output(JSON.stringify({ ...jsonOutput, ...jsonError }, null, 2))
    } else {
      for (const item of this.#outputBuffer) {
        this.output(item)
      }
    }

    this.#outputBuffer.length = 0
  }

  outputError (...msg) {
    log.clearProgress()
    // eslint-disable-next-line no-console
    console.error(...msg)
    log.showProgress()
  }
}
module.exports = Npm

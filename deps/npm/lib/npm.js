const EventEmitter = require('events')
const { resolve, dirname } = require('path')
const Config = require('@npmcli/config')

// Patch the global fs module here at the app level
require('graceful-fs').gracefulify(require('fs'))

const { definitions, flatten, shorthands } = require('./utils/config/index.js')
const { shellouts } = require('./utils/cmd-list.js')
const usage = require('./utils/npm-usage.js')

const which = require('which')

const deref = require('./utils/deref-command.js')
const LogFile = require('./utils/log-file.js')
const Timers = require('./utils/timers.js')
const Display = require('./utils/display.js')
const log = require('./utils/log-shim')
const replaceInfo = require('./utils/replace-info.js')

let warnedNonDashArg = false
const _load = Symbol('_load')
const _tmpFolder = Symbol('_tmpFolder')
const _title = Symbol('_title')
const pkg = require('../package.json')

class Npm extends EventEmitter {
  static get version () {
    return pkg.version
  }

  #unloaded = false
  #timers = null
  #logFile = null
  #display = null

  constructor () {
    super()
    this.command = null
    this.#logFile = new LogFile()
    this.#display = new Display()
    this.#timers = new Timers({
      start: 'npm',
      listener: (name, ms) => {
        const args = ['timing', name, `Completed in ${ms}ms`]
        this.#logFile.log(...args)
        this.#display.log(...args)
      },
    })
    this.config = new Config({
      npmPath: dirname(__dirname),
      definitions,
      flatten,
      shorthands,
    })
    this[_title] = process.title
    this.updateNotification = null
  }

  get version () {
    return this.constructor.version
  }

  get shelloutCommands () {
    return shellouts
  }

  deref (c) {
    return deref(c)
  }

  // Get an instantiated npm command
  // npm.command is already taken as the currently running command, a refactor
  // would be needed to change this
  async cmd (cmd) {
    await this.load()
    const command = this.deref(cmd)
    if (!command) {
      throw Object.assign(new Error(`Unknown command ${cmd}`), {
        code: 'EUNKNOWNCOMMAND',
      })
    }
    const Impl = require(`./commands/${command}.js`)
    const impl = new Impl(this)
    return impl
  }

  // Call an npm command
  async exec (cmd, args) {
    const command = await this.cmd(cmd)
    process.emit('time', `command:${cmd}`)

    // since 'test', 'start', 'stop', etc. commands re-enter this function
    // to call the run-script command, we need to only set it one time.
    if (!this.command) {
      process.env.npm_command = command.name
      this.command = command.name
    }

    // Options are prefixed by a hyphen-minus (-, \u2d).
    // Other dash-type chars look similar but are invalid.
    if (!warnedNonDashArg) {
      args
        .filter(arg => /^[\u2010-\u2015\u2212\uFE58\uFE63\uFF0D]/.test(arg))
        .forEach(arg => {
          warnedNonDashArg = true
          log.error(
            'arg',
            'Argument starts with non-ascii dash, this is probably invalid:',
            arg
          )
        })
    }

    const workspacesEnabled = this.config.get('workspaces')
    const implicitWorkspace = this.config.get('workspace', 'default').length > 0
    const workspacesFilters = this.config.get('workspace')
    if (workspacesEnabled === false && workspacesFilters.length > 0) {
      throw new Error('Can not use --no-workspaces and --workspace at the same time')
    }

    // only call execWorkspaces when we have workspaces explicitly set
    // or when it is implicit and not in our ignore list
    const filterByWorkspaces =
      (workspacesEnabled || workspacesFilters.length > 0)
      && (!implicitWorkspace || !command.ignoreImplicitWorkspace)
    // normally this would go in the constructor, but our tests don't
    // actually use a real npm object so this.npm.config isn't always
    // populated.  this is the compromise until we can make that a reality
    // and then move this into the constructor.
    command.workspaces = this.config.get('workspaces')
    command.workspacePaths = null
    // normally this would be evaluated in base-command#setWorkspaces, see
    // above for explanation
    command.includeWorkspaceRoot = this.config.get('include-workspace-root')

    if (this.config.get('usage')) {
      this.output(command.usage)
      return
    }
    if (filterByWorkspaces) {
      if (this.config.get('global')) {
        throw new Error('Workspaces not supported for global packages')
      }

      return command.execWorkspaces(args, this.config.get('workspace')).finally(() => {
        process.emit('timeEnd', `command:${cmd}`)
      })
    } else {
      return command.exec(args).finally(() => {
        process.emit('timeEnd', `command:${cmd}`)
      })
    }
  }

  async load () {
    if (!this.loadPromise) {
      process.emit('time', 'npm:load')
      this.loadPromise = new Promise((resolve, reject) => {
        this[_load]()
          .catch(er => er)
          .then(er => {
            this.loadErr = er
            if (!er && this.config.get('force')) {
              log.warn('using --force', 'Recommended protections disabled.')
            }

            process.emit('timeEnd', 'npm:load')
            if (er) {
              return reject(er)
            }
            resolve()
          })
      })
    }
    return this.loadPromise
  }

  get loaded () {
    return this.config.loaded
  }

  // This gets called at the end of the exit handler and
  // during any tests to cleanup all of our listeners
  // Everything in here should be synchronous
  unload () {
    // Track if we've already unloaded so we dont
    // write multiple timing files. This is only an
    // issue in tests right now since we unload
    // in both tap teardowns and the exit handler
    if (this.#unloaded) {
      return
    }
    this.#timers.off()
    this.#display.off()
    this.#logFile.off()
    if (this.loaded && this.config.get('timing')) {
      this.#timers.writeFile({
        command: process.argv.slice(2),
        // We used to only ever report a single log file
        // so to be backwards compatible report the last logfile
        // XXX: remove this in npm 9 or just keep it forever
        logfile: this.logFiles[this.logFiles.length - 1],
        logfiles: this.logFiles,
        version: this.version,
      })
    }
    this.#unloaded = true
  }

  get title () {
    return this[_title]
  }

  set title (t) {
    process.title = t
    this[_title] = t
  }

  async [_load] () {
    process.emit('time', 'npm:load:whichnode')
    let node
    try {
      node = which.sync(process.argv[0])
    } catch {
      // TODO should we throw here?
    }
    process.emit('timeEnd', 'npm:load:whichnode')
    if (node && node.toUpperCase() !== process.execPath.toUpperCase()) {
      log.verbose('node symlink', node)
      process.execPath = node
      this.config.execPath = node
    }

    process.emit('time', 'npm:load:configload')
    await this.config.load()
    process.emit('timeEnd', 'npm:load:configload')

    this.argv = this.config.parsedArgv.remain
    // note: this MUST be shorter than the actual argv length, because it
    // uses the same memory, so node will truncate it if it's too long.
    // if it's a token revocation, then the argv contains a secret, so
    // don't show that.  (Regrettable historical choice to put it there.)
    // Any other secrets are configs only, so showing only the positional
    // args keeps those from being leaked.
    process.emit('time', 'npm:load:setTitle')
    const tokrev = deref(this.argv[0]) === 'token' && this.argv[1] === 'revoke'
    this.title = tokrev
      ? 'npm token revoke' + (this.argv[2] ? ' ***' : '')
      : replaceInfo(['npm', ...this.argv].join(' '))
    process.emit('timeEnd', 'npm:load:setTitle')

    process.emit('time', 'npm:load:display')
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
    process.emit('timeEnd', 'npm:load:display')
    process.env.COLOR = this.color ? '1' : '0'

    process.emit('time', 'npm:load:logFile')
    this.#logFile.load({
      dir: resolve(this.cache, '_logs'),
      logsMax: this.config.get('logs-max'),
    })
    log.verbose('logfile', this.#logFile.files[0])
    process.emit('timeEnd', 'npm:load:logFile')

    process.emit('time', 'npm:load:timers')
    this.#timers.load({
      dir: this.cache,
    })
    process.emit('timeEnd', 'npm:load:timers')

    process.emit('time', 'npm:load:configScope')
    const configScope = this.config.get('scope')
    if (configScope && !/^@/.test(configScope)) {
      this.config.set('scope', `@${configScope}`, this.config.find('scope'))
    }
    process.emit('timeEnd', 'npm:load:configScope')
  }

  get flatOptions () {
    const { flat } = this.config
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

  get globalDir () {
    return process.platform !== 'win32'
      ? resolve(this.globalPrefix, 'lib', 'node_modules')
      : resolve(this.globalPrefix, 'node_modules')
  }

  get localDir () {
    return resolve(this.localPrefix, 'node_modules')
  }

  get dir () {
    return this.config.get('global') ? this.globalDir : this.localDir
  }

  get globalBin () {
    const b = this.globalPrefix
    return process.platform !== 'win32' ? resolve(b, 'bin') : b
  }

  get localBin () {
    return resolve(this.dir, '.bin')
  }

  get bin () {
    return this.config.get('global') ? this.globalBin : this.localBin
  }

  get prefix () {
    return this.config.get('global') ? this.globalPrefix : this.localPrefix
  }

  set prefix (r) {
    const k = this.config.get('global') ? 'globalPrefix' : 'localPrefix'
    this[k] = r
  }

  get usage () {
    return usage(this)
  }

  // XXX add logging to see if we actually use this
  get tmp () {
    if (!this[_tmpFolder]) {
      const rand = require('crypto').randomBytes(4).toString('hex')
      this[_tmpFolder] = `npm-${process.pid}-${rand}`
    }
    return resolve(this.config.get('tmp'), this[_tmpFolder])
  }

  // output to stdout in a progress bar compatible way
  output (...msg) {
    log.clearProgress()
    // eslint-disable-next-line no-console
    console.log(...msg)
    log.showProgress()
  }
}
module.exports = Npm

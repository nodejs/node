// The order of the code in this file is relevant, because a lot of things
// require('npm.js'), but also we need to use some of those modules.  So,
// we define and instantiate the singleton ahead of loading any modules
// required for its methods.

// these are all dependencies used in the ctor
const EventEmitter = require('events')
const { resolve, dirname } = require('path')
const Config = require('@npmcli/config')

// Patch the global fs module here at the app level
require('graceful-fs').gracefulify(require('fs'))

const procLogListener = require('./utils/proc-log-listener.js')

const proxyCmds = new Proxy({}, {
  get: (target, cmd) => {
    const actual = deref(cmd)
    if (actual && !Reflect.has(target, actual)) {
      const Impl = require(`./${actual}.js`)
      const impl = new Impl(npm)
      // Our existing npm.commands[x] act like a function with attributes, but
      // our modules have non-inumerable attributes so we can't just assign
      // them to an anonymous function like we used to.  This acts like that
      // old way of doing things, until we can make breaking changes to the
      // npm.commands[x] api
      target[actual] = new Proxy(
        (args, cb) => npm[_runCmd](actual, impl, args, cb),
        {
          get: (target, attr, receiver) => {
            return Reflect.get(impl, attr, receiver)
          },
        })
    }
    return target[actual]
  },
})

const { definitions, flatten, shorthands } = require('./utils/config/index.js')
const { shellouts } = require('./utils/cmd-list.js')
const usage = require('./utils/npm-usage.js')

let warnedNonDashArg = false
const _runCmd = Symbol('_runCmd')
const _load = Symbol('_load')
const _tmpFolder = Symbol('_tmpFolder')
const _title = Symbol('_title')
const npm = module.exports = new class extends EventEmitter {
  constructor () {
    super()
    require('./utils/perf.js')
    this.started = Date.now()
    this.command = null
    this.commands = proxyCmds
    procLogListener()
    process.emit('time', 'npm')
    this.version = require('../package.json').version
    this.config = new Config({
      npmPath: dirname(__dirname),
      definitions,
      flatten,
      shorthands,
    })
    this[_title] = process.title
    this.updateNotification = null
  }

  get shelloutCommands () {
    return shellouts
  }

  deref (c) {
    return deref(c)
  }

  // this will only ever be called with cmd set to the canonical command name
  [_runCmd] (cmd, impl, args, cb) {
    if (!this.loaded) {
      throw new Error(
        'Call npm.load(cb) before using this command.\n' +
        'See the README.md or bin/npm-cli.js for example usage.'
      )
    }

    process.emit('time', `command:${cmd}`)
    // since 'test', 'start', 'stop', etc. commands re-enter this function
    // to call the run-script command, we need to only set it one time.
    if (!this.command) {
      process.env.npm_command = cmd
      this.command = cmd
    }

    // Options are prefixed by a hyphen-minus (-, \u2d).
    // Other dash-type chars look similar but are invalid.
    if (!warnedNonDashArg) {
      args.filter(arg => /^[\u2010-\u2015\u2212\uFE58\uFE63\uFF0D]/.test(arg))
        .forEach(arg => {
          warnedNonDashArg = true
          log.error('arg', 'Argument starts with non-ascii dash, this is probably invalid:', arg)
        })
    }

    const workspacesEnabled = this.config.get('workspaces')
    const workspacesFilters = this.config.get('workspace')
    const filterByWorkspaces = workspacesEnabled || workspacesFilters.length > 0

    if (this.config.get('usage')) {
      this.output(impl.usage)
      cb()
    } else if (filterByWorkspaces) {
      if (this.config.get('global'))
        return cb(new Error('Workspaces not supported for global packages'))

      impl.execWorkspaces(args, this.config.get('workspace'), er => {
        process.emit('timeEnd', `command:${cmd}`)
        cb(er)
      })
    } else {
      impl.exec(args, er => {
        process.emit('timeEnd', `command:${cmd}`)
        cb(er)
      })
    }
  }

  // call with parsed CLI options and a callback when done loading
  // XXX promisify this and stop taking a callback
  load (cb) {
    if (!cb || typeof cb !== 'function')
      throw new TypeError('must call as: npm.load(callback)')

    this.once('load', cb)
    if (this.loaded || this.loadErr) {
      this.emit('load', this.loadErr)
      return
    }
    if (this.loading)
      return

    this.loading = true

    process.emit('time', 'npm:load')
    this.log.pause()
    return this[_load]().catch(er => er).then((er) => {
      this.loading = false
      this.loadErr = er
      if (!er && this.config.get('force'))
        this.log.warn('using --force', 'Recommended protections disabled.')

      process.emit('timeEnd', 'npm:load')
      this.emit('load', er)
    })
  }

  get loaded () {
    return this.config.loaded
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
    const node = await which(process.argv[0]).catch(er => null)
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
    this.title = tokrev ? 'npm token revoke' + (this.argv[2] ? ' ***' : '')
      : ['npm', ...this.argv].join(' ')
    process.emit('timeEnd', 'npm:load:setTitle')

    process.emit('time', 'npm:load:setupLog')
    this.color = setupLog(this.config)
    process.emit('timeEnd', 'npm:load:setupLog')
    process.env.COLOR = this.color ? '1' : '0'

    process.emit('time', 'npm:load:cleanupLog')
    cleanUpLogFiles(this.cache, this.config.get('logs-max'), log.warn)
    process.emit('timeEnd', 'npm:load:cleanupLog')

    log.resume()

    process.emit('time', 'npm:load:configScope')
    const configScope = this.config.get('scope')
    if (configScope && !/^@/.test(configScope))
      this.config.set('scope', `@${configScope}`, this.config.find('scope'))
    process.emit('timeEnd', 'npm:load:configScope')

    process.emit('time', 'npm:load:projectScope')
    this.projectScope = this.config.get('scope') ||
      getProjectScope(this.prefix)
    process.emit('timeEnd', 'npm:load:projectScope')
  }

  get flatOptions () {
    const { flat } = this.config
    if (this.command)
      flat.npmCommand = this.command
    return flat
  }

  get lockfileVersion () {
    return 2
  }

  get log () {
    return log
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
    return (this.config.get('global')) ? this.globalDir : this.localDir
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
    this.log.clearProgress()
    console.log(...msg)
    this.log.showProgress()
  }
}()

// now load everything required by the class methods

const log = require('npmlog')
const { promisify } = require('util')

const which = promisify(require('which'))

const deref = require('./utils/deref-command.js')
const setupLog = require('./utils/setup-log.js')
const cleanUpLogFiles = require('./utils/cleanup-log-files.js')
const getProjectScope = require('./utils/get-project-scope.js')

if (require.main === module)
  require('./cli.js')(process)

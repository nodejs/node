const { createHook, executionAsyncId } = require('async_hooks')
const { EventEmitter } = require('events')
const { homedir, tmpdir } = require('os')
const { dirname, join } = require('path')
const { promisify } = require('util')
const mkdirp = require('mkdirp-infer-owner')
const npmlog = require('npmlog')
const rimraf = promisify(require('rimraf'))
const t = require('tap')

let active = null
const chain = new Map()
const sandboxes = new Map()

// keep a reference to the real process
const _process = process

const processHook = createHook({
  init: (asyncId, type, triggerAsyncId, resource) => {
    // track parentage of asyncIds
    chain.set(asyncId, triggerAsyncId)
  },
  before: (asyncId) => {
    // find the nearest parent id that has a sandbox
    let parent = asyncId
    while (chain.has(parent) && !sandboxes.has(parent)) {
      parent = chain.get(parent)
    }

    process = sandboxes.has(parent)
      ? sandboxes.get(parent)
      : _process
  },
}).enable()

for (const level in npmlog.levels) {
  npmlog[`_${level}`] = npmlog[level]
  npmlog[level] = (...args) => {
    process._logs = process._logs || {}
    process._logs[level] = process._logs[level] || []
    process._logs[level].push(args)
    const _level = npmlog.level
    npmlog.level = 'silent'
    npmlog[`_${level}`](...args)
    npmlog.level = _level
  }
}

const _data = Symbol('sandbox.data')
const _dirs = Symbol('sandbox.dirs')
const _test = Symbol('sandbox.test')
const _mocks = Symbol('sandbox.mocks')
const _npm = Symbol('sandbox.npm')
const _parent = Symbol('sandbox.parent')
const _output = Symbol('sandbox.output')
const _proxy = Symbol('sandbox.proxy')
const _get = Symbol('sandbox.proxy.get')
const _set = Symbol('sandbox.proxy.set')

// these config keys can be redacted widely
const redactedDefaults = [
  'node-version',
  'npm-version',
  'tmp',
]

// we can't just replace these values everywhere because they're known to be
// very short strings that could be present all over the place, so we only
// replace them if they're located within quotes for now
const vagueRedactedDefaults = [
  'editor',
  'shell',
]

const normalize = (str) => str
  .replace(/\r\n/g, '\n') // normalize line endings (for ini)
  .replace(/[A-z]:\\/g, '\\') // turn windows roots to posix ones
  .replace(/\\+/g, '/') // replace \ with /

class Sandbox extends EventEmitter {
  constructor (test, options = {}) {
    super()

    this[_test] = test
    this[_mocks] = options.mocks || {}
    this[_data] = new Map()
    this[_output] = []
    const tempDir = `${test.testdirName}-sandbox`
    this[_dirs] = {
      temp: tempDir,
      global: options.global || join(tempDir, 'global'),
      home: options.home || join(tempDir, 'home'),
      project: options.project || join(tempDir, 'project'),
    }

    this[_proxy] = new Proxy(_process, {
      get: this[_get].bind(this),
      set: this[_set].bind(this),
    })
    this[_proxy].env = {}
    this[_proxy].argv = []

    test.cleanSnapshot = this.cleanSnapshot.bind(this)
    test.afterEach(() => this.reset())
    test.teardown(() => this.teardown())
  }

  get config () {
    return this[_npm] && this[_npm].config
  }

  get logs () {
    return this[_proxy]._logs
  }

  get global () {
    return this[_dirs].global
  }

  get home () {
    return this[_dirs].home
  }

  get project () {
    return this[_dirs].project
  }

  get process () {
    return this[_proxy]
  }

  get output () {
    return this[_output].map((line) => line.join(' ')).join('\n')
  }

  cleanSnapshot (snapshot) {
    let clean = normalize(snapshot)

    const viewer = _process.platform === 'win32'
      ? /"browser"([^:]+|$)/g
      : /"man"([^:]+|$)/g

    // the global prefix is platform dependent
    const realGlobalPrefix = _process.platform === 'win32'
      ? dirname(_process.execPath)
      : dirname(dirname(_process.execPath))

    const cache = _process.platform === 'win32'
      ? /\{HOME\}\/npm-cache(\r?\n|"|\/|$)/g
      : /\{HOME\}\/\.npm(\n|"|\/|$)/g

    // and finally replace some paths we know could be present
    clean = clean
      .replace(viewer, '"{VIEWER}"$1')
      .split(normalize(this[_proxy].execPath)).join('{EXECPATH}')
      .split(normalize(_process.execPath)).join('{REALEXECPATH}')
      .split(normalize(this.global)).join('{GLOBALPREFIX}')
      .split(normalize(realGlobalPrefix)).join('{REALGLOBALREFIX}')
      .split(normalize(this.project)).join('{LOCALPREFIX}')
      .split(normalize(this.home)).join('{HOME}')
      .replace(cache, '{CACHE}$1')
      .split(normalize(dirname(dirname(__dirname)))).join('{NPMDIR}')
      .split(normalize(tmpdir())).join('{TMP}')
      .split(normalize(homedir())).join('{REALHOME}')
      .split(this[_proxy].platform).join('{PLATFORM}')
      .split(this[_proxy].arch).join('{ARCH}')

    // We do the defaults after everything else so that they don't cause the
    // other cleaners to miss values we would have clobbered here.  For
    // instance if execPath is /home/user/.nvm/versions/node/1.0.0/bin/node,
    // and we replaced the node version first, the real execPath we're trying
    // to replace would no longer be represented, and be missed.
    if (this[_npm]) {
      // replace default config values with placeholders
      for (const name of redactedDefaults) {
        let value = this[_npm].config.defaults[name]
        clean = clean.split(value).join(`{${name.toUpperCase()}}`)
      }

      // replace vague default config values that are present within quotes
      // with placeholders
      for (const name of vagueRedactedDefaults) {
        const value = this[_npm].config.defaults[name]
        clean = clean.split(`"${value}"`).join(`"{${name.toUpperCase()}}"`)
      }
    }

    return clean
  }

  // test.afterEach hook
  reset () {
    this.removeAllListeners()
    this[_parent] = undefined
    this[_output] = []
    this[_data].clear()
    this[_proxy].env = {}
    this[_proxy].argv = []
    this[_npm] = undefined
  }

  // test.teardown hook
  teardown () {
    if (this[_parent]) {
      sandboxes.delete(this[_parent])
    }
    return rimraf(this[_dirs].temp).catch(() => null)
  }

  // proxy get handler
  [_get] (target, prop, receiver) {
    if (this[_data].has(prop)) {
      return this[_data].get(prop)
    }

    if (this[prop] !== undefined) {
      return Reflect.get(this, prop, this)
    }

    const actual = Reflect.get(target, prop, receiver)
    if (typeof actual === 'function') {
      // in node 10.1 there's an interesting bug where if a function on process
      // is called without explicitly forcing the 'this' arg to something, we
      // get 'Illegal invocation' errors. wrapping function properties in their
      // own proxy so that we can make sure the context is right fixes it
      return new Proxy(actual, {
        apply: (target, context, args) => {
          return Reflect.apply(target, _process, args)
        },
      })
    }

    return actual
  }

  // proxy set handler
  [_set] (target, prop, value) {
    if (prop === 'env') {
      value = {
        ...value,
        HOME: this.home,
      }
    }

    if (prop === 'argv') {
      value = [
        process.execPath,
        join(dirname(process.execPath), 'npm'),
        ...value,
      ]
    }

    return this[_data].set(prop, value)
  }

  async run (command, argv = []) {
    await Promise.all([
      mkdirp(this.project),
      mkdirp(this.home),
      mkdirp(this.global),
    ])

    // attach the sandbox process now, doing it after the promise above is
    // necessary to make sure that only async calls spawned as part of this
    // call to run will receive the sandbox. if we attach it too early, we
    // end up interfering with tap
    this[_parent] = executionAsyncId()
    this[_data].set('_asyncId', this[_parent])
    sandboxes.set(this[_parent], this[_proxy])
    process = this[_proxy]

    this[_proxy].argv = [
      '--prefix', this.project,
      '--userconfig', join(this.home, '.npmrc'),
      '--globalconfig', join(this.global, 'npmrc'),
      command,
      ...argv,
    ]

    this[_npm] = this[_test].mock('../../lib/npm.js', this[_mocks])
    this[_npm].output = (...args) => this[_output].push(args)
    await this[_npm].load()
    // in some node versions (later 10.x) our executionAsyncId at this point
    // will for some reason appear to have been triggered by a different parent
    // so immediately after load, if we can see that we lost our ancestry, we
    // fix it here with a hammer
    if (chain.get(executionAsyncId()) !== this[_parent]) {
      chain.set(executionAsyncId(), this[_parent])
      process = this[_proxy]
    }

    const cmd = this[_npm].argv.shift()
    const impl = this[_npm].commands[cmd]
    if (!impl) {
      throw new Error(`Unknown command: ${cmd}`)
    }

    return new Promise((resolve, reject) => {
      impl(this[_npm].argv, (err) => {
        if (err) {
          return reject(err)
        }

        return resolve()
      })
    })
  }

  async complete (command, argv, partial) {
    if (!Array.isArray(argv)) {
      partial = argv
      argv = []
    }

    await Promise.all([
      mkdirp(this.project),
      mkdirp(this.home),
      mkdirp(this.global),
    ])

    // attach the sandbox process now, doing it after the promise above is
    // necessary to make sure that only async calls spawned as part of this
    // call to run will receive the sandbox. if we attach it too early, we
    // end up interfering with tap
    this[_parent] = executionAsyncId()
    this[_data].set('_asyncId', this[_parent])
    sandboxes.set(this[_parent], this[_proxy])
    process = this[_proxy]

    this[_proxy].argv = [
      '--prefix', this.project,
      '--userconfig', join(this.home, '.npmrc'),
      '--globalconfig', join(this.global, 'npmrc'),
      command,
      ...argv,
    ]

    this[_npm] = this[_test].mock('../../lib/npm.js', this[_mocks])
    this[_npm].output = (...args) => this[_output].push(args)
    await this[_npm].load()
    // in some node versions (later 10.x) our executionAsyncId at this point
    // will for some reason appear to have been triggered by a different parent
    // so immediately after load, if we can see that we lost our ancestry, we
    // fix it here with a hammer
    if (chain.get(executionAsyncId()) !== this[_parent]) {
      chain.set(executionAsyncId(), this[_parent])
      process = this[_proxy]
    }

    const impl = this[_npm].commands[command]
    if (!impl) {
      throw new Error(`Unknown command: ${cmd}`)
    }

    return impl.completion({
      partialWord: partial,
      conf: {
        argv: {
          remain: ['npm', command, ...argv],
        },
      },
    })
  }
}

module.exports = Sandbox

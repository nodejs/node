const os = require('os')
const fs = require('fs').promises
const path = require('path')
const mockLogs = require('./mock-logs')
const mockGlobals = require('./mock-globals')
const log = require('../../lib/utils/log-shim')
const envConfigKeys = Object.keys(require('../../lib/utils/config/definitions.js'))

const RealMockNpm = (t, otherMocks = {}) => {
  const mock = {
    ...mockLogs(otherMocks),
    outputs: [],
    outputErrors: [],
    joinedOutput: () => mock.outputs.map(o => o.join(' ')).join('\n'),
  }

  const Npm = t.mock('../../lib/npm.js', {
    '../../lib/utils/update-notifier.js': async () => {},
    ...otherMocks,
    ...mock.logMocks,
  })

  mock.Npm = class MockNpm extends Npm {
    // lib/npm.js tests needs this to actually test the function!
    originalOutput (...args) {
      super.output(...args)
    }

    originalOutputError (...args) {
      super.outputError(...args)
    }

    output (...args) {
      mock.outputs.push(args)
    }

    outputError (...args) {
      mock.outputErrors.push(args)
    }
  }

  return mock
}

const setLoglevel = (t, loglevel, reset = true) => {
  if (t && reset) {
    const _level = log.level
    t.teardown(() => log.level = _level)
  }

  if (loglevel) {
    // Set log level on the npmlog singleton and shared across everything
    log.level = loglevel
  }
}

// Resolve some options to a function call with supplied args
const result = (fn, ...args) => typeof fn === 'function' ? fn(...args) : fn

const LoadMockNpm = async (t, {
  init = true,
  load = init,
  prefixDir = {},
  homeDir = {},
  cacheDir = {},
  globalPrefixDir = {},
  config = {},
  mocks = {},
  globals = null,
} = {}) => {
  // Mock some globals with their original values so they get torn down
  // back to the original at the end of the test since they are manipulated
  // by npm itself
  const npmConfigEnv = {}
  for (const key in process.env) {
    if (key.startsWith('npm_config_')) {
      npmConfigEnv[key] = undefined
    }
  }
  mockGlobals(t, {
    process: {
      title: process.title,
      execPath: process.execPath,
      env: {
        npm_command: process.env.npm_command,
        COLOR: process.env.COLOR,
        ...npmConfigEnv,
      },
    },
  })

  const { Npm, ...rest } = RealMockNpm(t, mocks)

  // We want to fail fast when writing tests. Default this to 0 unless it was
  // explicitly set in a test.
  config = { 'fetch-retries': 0, ...config }

  if (!init && load) {
    throw new Error('cant `load` without `init`')
  }

  // Set log level as early as possible since
  setLoglevel(t, config.loglevel)

  const dir = t.testdir({
    home: homeDir,
    prefix: prefixDir,
    cache: cacheDir,
    global: globalPrefixDir,
  })
  const prefix = path.join(dir, 'prefix')
  const cache = path.join(dir, 'cache')
  const globalPrefix = path.join(dir, 'global')
  const home = path.join(dir, 'home')

  // Set cache to testdir via env var so it is available when load is run
  // XXX: remove this for a solution where cache argv is passed in
  mockGlobals(t, {
    'process.env.HOME': home,
    'process.env.npm_config_cache': cache,
    ...(globals ? result(globals, { prefix, cache, home }) : {}),
    // Some configs don't work because they can't be set via npm.config.set until
    // config is loaded. But some config items are needed before that. So this is
    // an explicit set of configs that must be loaded as env vars.
    // XXX(npm9): make this possible by passing in argv directly to npm/config
    ...Object.entries(config)
      .filter(([k]) => envConfigKeys.includes(k))
      .reduce((acc, [k, v]) => {
        acc[`process.env.npm_config_${k.replace(/-/g, '_')}`] = v.toString()
        return acc
      }, {}),
  })

  const npm = init ? new Npm() : null
  t.teardown(() => {
    npm && npm.unload()
  })

  if (load) {
    await npm.load()
    for (const [k, v] of Object.entries(result(config, { npm, prefix, cache }))) {
      if (typeof v === 'object' && v.value && v.where) {
        npm.config.set(k, v.value, v.where)
      } else {
        npm.config.set(k, v)
      }
    }
    // Set global loglevel *again* since it possibly got reset during load
    // XXX: remove with npmlog
    setLoglevel(t, config.loglevel, false)
    npm.prefix = prefix
    npm.cache = cache
    npm.globalPrefix = globalPrefix
  }

  return {
    ...rest,
    Npm,
    npm,
    home,
    prefix,
    globalPrefix,
    testdir: dir,
    cache,
    debugFile: async () => {
      const readFiles = npm.logFiles.map(f => fs.readFile(f))
      const logFiles = await Promise.all(readFiles)
      return logFiles
        .flatMap((d) => d.toString().trim().split(os.EOL))
        .filter(Boolean)
        .join('\n')
    },
    timingFile: async () => {
      const data = await fs.readFile(path.resolve(cache, '_timing.json'), 'utf8')
      return JSON.parse(data) // XXX: this fails if multiple timings are written
    },
  }
}

const realConfig = require('../../lib/utils/config')

// Basic npm fixture that you can give a config object that acts like
// npm.config You still need a separate flatOptions. Tests should migrate to
// using the real npm mock above
class MockNpm {
  constructor (base = {}, t) {
    this._mockOutputs = []
    this.isMockNpm = true
    this.base = base

    const config = base.config || {}

    for (const attr in base) {
      if (attr !== 'config') {
        this[attr] = base[attr]
      }
    }

    this.flatOptions = base.flatOptions || {}
    this.config = {
      // for now just set `find` to what config.find should return
      // this works cause `find` is not an existing config entry
      find: (k) => ({ ...realConfig.defaults, ...config })[k],
      // for now isDefault is going to just return false if a value was defined
      isDefault: (k) => !Object.prototype.hasOwnProperty.call(config, k),
      get: (k) => ({ ...realConfig.defaults, ...config })[k],
      set: (k, v) => {
        config[k] = v
        // mock how real npm derives silent
        if (k === 'loglevel') {
          this.flatOptions.silent = v === 'silent'
          this.silent = v === 'silent'
        }
      },
      list: [{ ...realConfig.defaults, ...config }],
    }

    if (t && config.loglevel) {
      setLoglevel(t, config.loglevel)
    }

    if (config.loglevel) {
      this.config.set('loglevel', config.loglevel)
    }
  }

  get global () {
    return this.config.get('global') || this.config.get('location') === 'global'
  }

  output (...msg) {
    if (this.base.output) {
      return this.base.output(msg)
    }
    this._mockOutputs.push(msg)
  }
}

const FakeMockNpm = (base = {}, t) => {
  return new MockNpm(base, t)
}

module.exports = {
  fake: FakeMockNpm,
  load: LoadMockNpm,
}

const npmlog = require('npmlog')
const procLog = require('../../lib/utils/proc-log-listener.js')
procLog.reset()

// In theory we shouldn't have to do this if all the tests were tearing down
// their listeners properly, we're still getting warnings even though
// perfStop() and procLog.reset() is in the teardown script.  This silences the
// warnings for now
require('events').defaultMaxListeners = Infinity

const realLog = {}
for (const level in npmlog.levels)
  realLog[level] = npmlog[level]

const { title, execPath } = process

// Eventually this should default to having a prefix of an empty testdir, and
// awaiting npm.load() unless told not to (for npm tests for example).  Ideally
// the prefix of an empty dir is inferred rather than explicitly set
const RealMockNpm = (t, otherMocks = {}) => {
  const mock = {}
  mock.logs = []
  mock.outputs = []
  mock.joinedOutput = () => {
    return mock.outputs.map(o => o.join(' ')).join('\n')
  }
  const Npm = t.mock('../../lib/npm.js', otherMocks)
  class MockNpm extends Npm {
    constructor () {
      super()
      for (const level in npmlog.levels) {
        npmlog[level] = (...msg) => {
          mock.logs.push([level, ...msg])

          const l = npmlog.level
          npmlog.level = 'silent'
          realLog[level](...msg)
          npmlog.level = l
        }
      }
      // npm.js tests need this restored to actually test this function!
      mock.npmOutput = this.output
      this.output = (...msg) => mock.outputs.push(msg)
    }
  }
  mock.Npm = MockNpm
  t.afterEach(() => {
    mock.outputs.length = 0
    mock.logs.length = 0
  })

  t.teardown(() => {
    process.removeAllListeners('time')
    process.removeAllListeners('timeEnd')
    npmlog.record.length = 0
    for (const level in npmlog.levels)
      npmlog[level] = realLog[level]
    procLog.reset()
    process.title = title
    process.execPath = execPath
    delete process.env.npm_command
    delete process.env.COLOR
  })

  return mock
}

const realConfig = require('../../lib/utils/config')

// Basic npm fixture that you can give a config object that acts like
// npm.config You still need a separate flatOptions. Tests should migrate to
// using the real npm mock above
class MockNpm {
  constructor (base = {}) {
    this._mockOutputs = []
    this.isMockNpm = true
    this.base = base

    const config = base.config || {}

    for (const attr in base) {
      if (attr !== 'config')
        this[attr] = base[attr]
    }

    this.flatOptions = base.flatOptions || {}
    this.config = {
      // for now just set `find` to what config.find should return
      // this works cause `find` is not an existing config entry
      find: (k) => ({...realConfig.defaults, ...config})[k],
      get: (k) => ({...realConfig.defaults, ...config})[k],
      set: (k, v) => config[k] = v,
      list: [{ ...realConfig.defaults, ...config}],
    }
    if (!this.log) {
      this.log = {
        clearProgress: () => {},
        disableProgress: () => {},
        enableProgress: () => {},
        http: () => {},
        info: () => {},
        levels: [],
        notice: () => {},
        pause: () => {},
        silly: () => {},
        verbose: () => {},
        warn: () => {},
      }
    }
  }

  output (...msg) {
    if (this.base.output)
      return this.base.output(msg)
    this._mockOutputs.push(msg)
  }
}

const FakeMockNpm = (base = {}) => {
  return new MockNpm(base)
}

module.exports = {
  fake: FakeMockNpm,
  real: RealMockNpm,
}

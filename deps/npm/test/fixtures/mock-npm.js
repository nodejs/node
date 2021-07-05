const npmlog = require('npmlog')
const perf = require('../../lib/utils/perf.js')
perf.reset()
const procLog = require('../../lib/utils/proc-log-listener.js')
procLog.reset()

const realLog = {}
for (const level of ['silly', 'verbose', 'timing', 'notice', 'warn', 'error'])
  realLog[level] = npmlog[level]

const { title, execPath } = process

const RealMockNpm = (t, otherMocks = {}) => {
  t.teardown(() => {
    for (const level of ['silly', 'verbose', 'timing', 'notice', 'warn', 'error'])
      npmlog[level] = realLog[level]
    perf.reset()
    procLog.reset()
    process.title = title
    process.execPath = execPath
    delete process.env.npm_command
    delete process.env.COLOR
  })
  const logs = []
  const outputs = []
  const npm = t.mock('../../lib/npm.js', otherMocks)
  const command = async (command, args = []) => {
    return new Promise((resolve, reject) => {
      npm.commands[command](args, err => {
        if (err)
          return reject(err)
        return resolve()
      })
    })
  }
  for (const level of ['silly', 'verbose', 'timing', 'notice', 'warn', 'error']) {
    npmlog[level] = (...msg) => {
      logs.push([level, ...msg])
    }
  }
  npm.output = (...msg) => outputs.push(msg)
  return { npm, logs, outputs, command }
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
      if (attr !== 'config') {
        this[attr] = base[attr]
      }
    }

    this.flatOptions = base.flatOptions || {}
    this.config = {
      // for now just set `find` to what config.find should return
      // this works cause `find` is not an existing config entry
      find: (k) => ({...realConfig.defaults, ...config})[k],
      get: (k) => ({...realConfig.defaults, ...config})[k],
      set: (k, v) => config[k] = v,
      list: [{ ...realConfig.defaults, ...config}]
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

  output(...msg) {
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
  real: RealMockNpm
}

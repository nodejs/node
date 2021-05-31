// Basic npm fixture that you can give a config object that acts like
// npm.config You still need a separate flatOptions but this is the first step
// to eventually just using npm itself

const realConfig = require('../../lib/utils/config')

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

// TODO export MockNpm, and change tests to use new MockNpm()
module.exports = (base = {}) => {
  return new MockNpm(base)
}

// Basic npm fixture that you can give a config object that acts like
// npm.config You still need a separate flatOptions but this is the first step
// to eventually just using npm itself

const realConfig = require('../../lib/utils/config')

const mockLog = {
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
const mockNpm = (base = {}) => {
  const config = base.config || {}
  const flatOptions = base.flatOptions || {}
  return {
    log: mockLog,
    ...base,
    flatOptions,
    config: {
      // for now just set `find` to what config.find should return
      // this works cause `find` is not an existing config entry
      find: (k) => ({...realConfig.defaults, ...config})[k],
      get: (k) => ({...realConfig.defaults, ...config})[k],
      set: (k, v) => config[k] = v,
      list: [{ ...realConfig.defaults, ...config}]
    },
  }
}

module.exports = mockNpm

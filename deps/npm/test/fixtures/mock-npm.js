// Basic npm fixture that you can give a config object that acts like
// npm.config You still need a separate flatOptions but this is the first step
// to eventually just using npm itself

const mockNpm = (base = {}) => {
  const config = base.config || {}
  const flatOptions = base.flatOptions || {}
  return {
    ...base,
    flatOptions,
    config: {
      // for now just set `find` to what config.find should return
      // this works cause `find` is not an existing config entry
      find: (k) => config[k],
      get: (k) => config[k],
      set: (k, v) => config[k] = v,
      list: [config]
    },
  }
}

module.exports = mockNpm

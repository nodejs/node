const is = require('./is.js')
const { dirname } = require('path')

module.exports = async ({ cwd = process.cwd(), root } = {}) => {
  while (true) {
    if (await is({ cwd })) {
      return cwd
    }
    const next = dirname(cwd)
    if (cwd === root || cwd === next) {
      return null
    }
    cwd = next
  }
}

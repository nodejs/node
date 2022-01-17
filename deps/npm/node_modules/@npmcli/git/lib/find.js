const is = require('./is.js')
const { dirname } = require('path')

module.exports = async ({ cwd = process.cwd() } = {}) => {
  if (await is({ cwd })) {
    return cwd
  }
  while (cwd !== dirname(cwd)) {
    cwd = dirname(cwd)
    if (await is({ cwd })) {
      return cwd
    }
  }
  return null
}

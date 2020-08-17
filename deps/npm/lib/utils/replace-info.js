const URL = require('url').URL

// replaces auth info in an array
//  of arguments or in a strings
function replaceInfo (arg) {
  const isArray = Array.isArray(arg)
  const isString = typeof arg === 'string'

  if (!isArray && !isString) return arg

  const args = isString ? arg.split(' ') : arg
  const info = args.map(arg => {
    try {
      const url = new URL(arg)
      return url.password === '' ? arg : arg.replace(url.password, '***')
    } catch (e) { return arg }
  })

  return isString ? info.join(' ') : info
}

module.exports = replaceInfo

const URL = require('url').URL

// replaces auth info in an array of arguments or in a strings
function replaceInfo (arg) {
  const isArray = Array.isArray(arg)
  const isString = str => typeof str === 'string'

  if (!isArray && !isString(arg)) {
    return arg
  }

  const testUrlAndReplace = str => {
    try {
      const url = new URL(str)
      return url.password === '' ? str : str.replace(url.password, '***')
    } catch (e) {
      return str
    }
  }

  const args = isString(arg) ? arg.split(' ') : arg
  const info = args.map(a => {
    if (isString(a) && a.indexOf(' ') > -1) {
      return a.split(' ').map(testUrlAndReplace).join(' ')
    }

    return testUrlAndReplace(a)
  })

  return isString(arg) ? info.join(' ') : info
}

module.exports = replaceInfo

const { cleanUrl } = require('npm-registry-fetch')
const isString = (v) => typeof v === 'string'

// split on \s|= similar to how nopt parses options
const splitAndReplace = (str) => {
  // stateful regex, don't move out of this scope
  const splitChars = /[\s=]/g

  let match = null
  let result = ''
  let index = 0
  while (match = splitChars.exec(str)) {
    result += cleanUrl(str.slice(index, match.index)) + match[0]
    index = splitChars.lastIndex
  }

  return result + cleanUrl(str.slice(index))
}

// replaces auth info in an array of arguments or in a strings
function replaceInfo (arg) {
  if (isString(arg)) {
    return splitAndReplace(arg)
  } else if (Array.isArray(arg)) {
    return arg.map((a) => isString(a) ? splitAndReplace(a) : a)
  }

  return arg
}

module.exports = replaceInfo

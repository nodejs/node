const { URL } = require('url')

const REPLACE = '***'
const TOKEN_REGEX = /\bnpm_[a-zA-Z0-9]{36}\b/g
const GUID_REGEX = /\b[0-9a-f]{8}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{12}\b/g

const redact = (value) => {
  if (typeof value !== 'string' || !value) {
    return value
  }

  let urlValue
  try {
    urlValue = new URL(value)
  } catch {
    // If it's not a URL then we can ignore all errors
  }

  if (urlValue?.password) {
    urlValue.password = REPLACE
    value = urlValue.toString()
  }

  return value
    .replace(TOKEN_REGEX, `npm_${REPLACE}`)
    .replace(GUID_REGEX, REPLACE)
}

// split on \s|= similar to how nopt parses options
const splitAndRedact = (str) => {
  // stateful regex, don't move out of this scope
  const splitChars = /[\s=]/g

  let match = null
  let result = ''
  let index = 0
  while (match = splitChars.exec(str)) {
    result += redact(str.slice(index, match.index)) + match[0]
    index = splitChars.lastIndex
  }

  return result + redact(str.slice(index))
}

// replaces auth info in an array of arguments or in a strings
const redactLog = (arg) => {
  if (typeof arg === 'string') {
    return splitAndRedact(arg)
  } else if (Array.isArray(arg)) {
    return arg.map((a) => typeof a === 'string' ? splitAndRedact(a) : a)
  }

  return arg
}

module.exports = {
  redact,
  redactLog,
}

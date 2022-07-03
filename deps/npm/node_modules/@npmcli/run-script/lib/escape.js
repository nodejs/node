'use strict'

// eslint-disable-next-line max-len
// this code adapted from: https://blogs.msdn.microsoft.com/twistylittlepassagesallalike/2011/04/23/everyone-quotes-command-line-arguments-the-wrong-way/
const cmd = (input, doubleEscape) => {
  if (!input.length) {
    return '""'
  }

  let result
  if (!/[ \t\n\v"]/.test(input)) {
    result = input
  } else {
    result = '"'
    for (let i = 0; i <= input.length; ++i) {
      let slashCount = 0
      while (input[i] === '\\') {
        ++i
        ++slashCount
      }

      if (i === input.length) {
        result += '\\'.repeat(slashCount * 2)
        break
      }

      if (input[i] === '"') {
        result += '\\'.repeat(slashCount * 2 + 1)
        result += input[i]
      } else {
        result += '\\'.repeat(slashCount)
        result += input[i]
      }
    }
    result += '"'
  }

  // and finally, prefix shell meta chars with a ^
  result = result.replace(/[ !^&()<>|"]/g, '^$&')
  if (doubleEscape) {
    result = result.replace(/[ !^&()<>|"]/g, '^$&')
  }

  // except for % which is escaped with another %, and only once
  result = result.replace(/%/g, '%%')

  return result
}

const sh = (input) => {
  if (!input.length) {
    return `''`
  }

  if (!/[\t\n\r "#$&'()*;<>?\\`|~]/.test(input)) {
    return input
  }

  // replace single quotes with '\'' and wrap the whole result in a fresh set of quotes
  const result = `'${input.replace(/'/g, `'\\''`)}'`
    // if the input string already had single quotes around it, clean those up
    .replace(/^(?:'')+(?!$)/, '')
    .replace(/\\'''/g, `\\'`)

  return result
}

// disabling the no-control-regex rule for this line as we very specifically _do_ want to
// replace those characters if they somehow exist at this point, which is highly unlikely
// eslint-disable-next-line no-control-regex
const filename = (input) => input.replace(/[<>:"/\\|?*\x00-\x31]/g, '')

module.exports = {
  cmd,
  sh,
  filename,
}

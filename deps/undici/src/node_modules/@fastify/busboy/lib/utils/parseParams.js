'use strict'

const decodeText = require('./decodeText')

const RE_ENCODED = /%([a-fA-F0-9]{2})/g

function encodedReplacer (match, byte) {
  return String.fromCharCode(parseInt(byte, 16))
}

function parseParams (str) {
  const res = []
  let state = 'key'
  let charset = ''
  let inquote = false
  let escaping = false
  let p = 0
  let tmp = ''

  for (var i = 0, len = str.length; i < len; ++i) { // eslint-disable-line no-var
    const char = str[i]
    if (char === '\\' && inquote) {
      if (escaping) { escaping = false } else {
        escaping = true
        continue
      }
    } else if (char === '"') {
      if (!escaping) {
        if (inquote) {
          inquote = false
          state = 'key'
        } else { inquote = true }
        continue
      } else { escaping = false }
    } else {
      if (escaping && inquote) { tmp += '\\' }
      escaping = false
      if ((state === 'charset' || state === 'lang') && char === "'") {
        if (state === 'charset') {
          state = 'lang'
          charset = tmp.substring(1)
        } else { state = 'value' }
        tmp = ''
        continue
      } else if (state === 'key' &&
        (char === '*' || char === '=') &&
        res.length) {
        if (char === '*') { state = 'charset' } else { state = 'value' }
        res[p] = [tmp, undefined]
        tmp = ''
        continue
      } else if (!inquote && char === ';') {
        state = 'key'
        if (charset) {
          if (tmp.length) {
            tmp = decodeText(tmp.replace(RE_ENCODED, encodedReplacer),
              'binary',
              charset)
          }
          charset = ''
        } else if (tmp.length) {
          tmp = decodeText(tmp, 'binary', 'utf8')
        }
        if (res[p] === undefined) { res[p] = tmp } else { res[p][1] = tmp }
        tmp = ''
        ++p
        continue
      } else if (!inquote && (char === ' ' || char === '\t')) { continue }
    }
    tmp += char
  }
  if (charset && tmp.length) {
    tmp = decodeText(tmp.replace(RE_ENCODED, encodedReplacer),
      'binary',
      charset)
  } else if (tmp) {
    tmp = decodeText(tmp, 'binary', 'utf8')
  }

  if (res[p] === undefined) {
    if (tmp) { res[p] = tmp }
  } else { res[p][1] = tmp }

  return res
}

module.exports = parseParams

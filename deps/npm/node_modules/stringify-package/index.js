'use strict'

module.exports = stringifyPackage

const DEFAULT_INDENT = 2
const CRLF = '\r\n'
const LF = '\n'

function stringifyPackage (data, indent, newline) {
  const json = JSON.stringify(data, null, indent || DEFAULT_INDENT)

  if (newline === CRLF) {
    return json.replace(/\n/g, CRLF) + CRLF
  }

  return json + LF
}

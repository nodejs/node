'use strict'

module.exports = stringifyPackage

const DEFAULT_INDENT = 2
const CRLF = '\r\n'
const LF = '\n'

function stringifyPackage (data, indent, newline) {
  indent = indent || (indent === 0 ? 0 : DEFAULT_INDENT)
  const json = JSON.stringify(data, null, indent)

  if (newline === CRLF) {
    return json.replace(/\n/g, CRLF) + CRLF
  }

  return json + LF
}

'use strict'
var parseJsonWithErrors = require('json-parse-better-errors')
var parseJSON = module.exports = function (content) {
  return parseJsonWithErrors(stripBOM(content))
}

parseJSON.noExceptions = function (content) {
  try {
    return parseJSON(content)
  } catch (ex) {

  }
}

// from read-package-json
function stripBOM (content) {
  content = content.toString()
  // Remove byte order marker. This catches EF BB BF (the UTF-8 BOM)
  // because the buffer-to-string conversion in `fs.readFileSync()`
  // translates it to FEFF, the UTF-16 BOM.
  if (content.charCodeAt(0) === 0xFEFF) {
    content = content.slice(1)
  }
  return content
}

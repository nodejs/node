'use strict'

module.exports = function (content) {
  // Code also yanked from read-package-json.
  function stripBOM (content) {
    content = content.toString()
    // Remove byte order marker. This catches EF BB BF (the UTF-8 BOM)
    // because the buffer-to-string conversion in `fs.readFileSync()`
    // translates it to FEFF, the UTF-16 BOM.
    if (content.charCodeAt(0) === 0xFEFF) return content.slice(1)
    return content
  }

  return JSON.parse(stripBOM(content))
}

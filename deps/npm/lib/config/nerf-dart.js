var url = require("url")

module.exports = toNerfDart

/**
 * Maps a URL to an identifier.
 *
 * Name courtesy schiffertronix media LLC, a New Jersey corporation
 *
 * @param {String} uri The URL to be nerfed.
 *
 * @returns {String} A nerfed URL.
 */
function toNerfDart(uri) {
  var parsed = url.parse(uri)
  delete parsed.protocol
  delete parsed.auth
  delete parsed.query
  delete parsed.search
  delete parsed.hash

  return url.resolve(url.format(parsed), ".")
}

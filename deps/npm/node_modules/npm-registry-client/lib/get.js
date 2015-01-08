module.exports = get

var assert = require("assert")
  , url = require("url")

/*
 * This is meant to be overridden in specific implementations if you
 * want specialized behavior for metadata (i.e. caching).
 */
function get (uri, params, cb) {
  assert(typeof uri === "string", "must pass registry URI to get")
  assert(params && typeof params === "object", "must pass params to get")
  assert(typeof cb === "function", "must pass callback to get")

  var parsed = url.parse(uri)
  assert(
    parsed.protocol === "http:" || parsed.protocol === "https:",
    "must have a URL that starts with http: or https:"
  )

  this.request(uri, params, cb)
}

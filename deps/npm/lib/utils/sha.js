
var fs = require("graceful-fs")
  , crypto = require("crypto")
  , log = require("./log.js")
  , binding

try { binding = process.binding("crypto") }
catch (e) { binding = null }

exports.check = check
exports.get = get

function check (file, sum, cb) {
  if (!binding) {
    log.warn("crypto binding not found. Cannot verify shasum.", "shasum")
    return cb()
  }
  get(file, function (er, actual) {
    if (er) return log.er(cb, "Error getting shasum")(er)
    var expected = sum.toLowerCase().trim()
      , ok = actual === expected
    cb(ok ? null : new Error(
      "shasum check failed for "+file+"\n"
      +"Expected: "+expected+"\n"
      +"Actual:   "+actual))
  })
}

function get (file, cb) {
  if (!binding) {
    log.warn("crypto binding not found. Cannot verify shasum.", "shasum")
    return cb()
  }
  var h = crypto.createHash("sha1")
    , s = fs.createReadStream(file)
    , errState = null
  s.on("error", function (er) {
    if (errState) return
    log.silly(er.stack || er.message, "sha error")
    return cb(errState = er)
  }).on("data", function (chunk) {
    if (errState) return
    log.silly(chunk.length, "updated sha bytes")
    h.update(chunk)
  }).on("end", function () {
    if (errState) return
    var actual = h.digest("hex").toLowerCase().trim()
    log(actual+"\n"+file, "shasum")
    cb(null, actual)
  })
}


var fs = require("graceful-fs")
  , crypto = require("crypto")
  , log = require("npmlog")
  , binding

try { binding = process.binding("crypto") }
catch (e) {
  var er = new Error( "crypto binding not found.\n"
                    + "Please build node with openssl.\n"
                    + e.message )
  throw er
}

exports.check = check
exports.get = get

function check (file, sum, cb) {
  get(file, function (er, actual) {
    if (er) {
      log.error("shasum", "error getting shasum")
      return cb(er)
    }
    var expected = sum.toLowerCase().trim()
      , ok = actual === expected
    cb(ok ? null : new Error(
      "shasum check failed for "+file+"\n"
      +"Expected: "+expected+"\n"
      +"Actual:   "+actual))
  })
}

function get (file, cb) {
  var h = crypto.createHash("sha1")
    , s = fs.createReadStream(file)
    , errState = null
  s.on("error", function (er) {
    if (errState) return
    log.silly("shasum", "error", er)
    return cb(errState = er)
  }).on("data", function (chunk) {
    if (errState) return
    log.silly("shasum", "updated bytes", chunk.length)
    h.update(chunk)
  }).on("end", function () {
    if (errState) return
    var actual = h.digest("hex").toLowerCase().trim()
    log.info("shasum", actual+"\n"+file)
    cb(null, actual)
  })
}

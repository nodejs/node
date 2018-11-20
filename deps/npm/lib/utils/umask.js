var umask = require("umask")
var npmlog = require("npmlog")
var _fromString = umask.fromString

module.exports = umask

// fromString with logging callback
umask.fromString = function (val) {
  _fromString(val, function (err, result) {
    if (err) {
      npmlog.warn("invalid umask", err.message)
    }
    val = result
  })

  return val
}

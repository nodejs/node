module.exports = setUser

var path = require('path')
var fs = require('fs')

function setUser (cb) {
  var defaultConf = Object.getPrototypeOf(this.root)

  // If global, leave it as-is.
  // If not global, then set the user to the owner of the prefix folder.
  // Just set the default, so it can be overridden.
  if (this.get("global")) return cb()
  if (process.env.SUDO_UID) {
    defaultConf.user = +(process.env.SUDO_UID)
    return cb()
  }

  var prefix = path.resolve(this.get("prefix"))
  fs.stat(prefix, function (er, st) {
    defaultConf.user = st && st.uid
    return cb(er)
  })
}

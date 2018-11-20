module.exports = logout

var dezalgo = require("dezalgo")
var log = require("npmlog")

var npm = require("./npm.js")
var mapToRegistry = require("./utils/map-to-registry.js")

logout.usage = "npm logout [--registry] [--scope]"

function logout (args, cb) {
  npm.spinner.start()
  cb = dezalgo(cb)

  mapToRegistry("/", npm.config, function (err, uri, auth, normalized) {
    if (err) return cb(err)

    if (auth.token) {
      log.verbose("logout", "clearing session token for", normalized)
      npm.registry.logout(normalized, { auth: auth }, function (err) {
        if (err) return cb(err)

        npm.config.clearCredentialsByURI(normalized)
        npm.spinner.stop()
        npm.config.save("user", cb)
      })
    }
    else if (auth.username || auth.password) {
      log.verbose("logout", "clearing user credentials for", normalized)
      npm.config.clearCredentialsByURI(normalized)
      npm.spinner.stop()
      npm.config.save("user", cb)
    }
    else {
      cb(new Error(
        "Not logged in to", normalized + ",", "so can't log out."
      ))
    }
  })
}

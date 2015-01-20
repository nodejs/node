var npm = require("./npm.js")

module.exports = whoami

whoami.usage = "npm whoami\n(just prints username according to given registry)"

function whoami (args, silent, cb) {
  // FIXME: need tighter checking on this, but is a breaking change
  if (typeof cb !== "function") {
    cb = silent
    silent = false
  }

  var registry = npm.config.get("registry")
  if (!registry) return cb(new Error("no default registry set"))

  var auth = npm.config.getCredentialsByURI(registry)
  if (auth) {
    if (auth.username) {
      if (!silent) console.log(auth.username)
      return process.nextTick(cb.bind(this, null, auth.username))
    }
    else if (auth.token) {
      return npm.registry.whoami(registry, { auth : auth }, function (er, username) {
        if (er) return cb(er)

        if (!silent) console.log(username)
        cb(null, username)
      })
    }
  }

  // At this point, if they have a credentials object, it doesn't
  // have a token or auth in it.  Probably just the default
  // registry.
  var msg = "Not authed.  Run 'npm adduser'"
  if (!silent) console.log(msg)
  process.nextTick(cb.bind(this, null, msg))
}


module.exports = adduser

var registry = require("./utils/npm-registry-client/index.js")
  , ini = require("./utils/ini.js")
  , log = require("./utils/log.js")
  , npm = require("./npm.js")
  , read = require("read")
  , promiseChain = require("./utils/promise-chain.js")
  , crypto

try {
  crypto = process.binding("crypto") && require("crypto")
} catch (ex) {}

adduser.usage = "npm adduser\nThen enter stuff at the prompts"

function adduser (args, cb) {
  if (!crypto) return cb(new Error(
    "You must compile node with ssl support to use the adduser feature"))

  var u = { u : npm.config.get("username")
          , p : npm.config.get("_password")
          , e : npm.config.get("email")
          }
    , changed = false

  promiseChain(cb)
    (read, [{prompt: "Username: ", default: u.u}], function (un) {
      changed = u.u !== un
      u.u = un
    })
    (function (cb) {
      if (u.p && !changed) return cb(null, u.p)
      read({prompt: "Password: ", default: u.p, silent: true}, cb)
    }, [], function (pw) { u.p = pw })
    (read, [{prompt: "Email: ", default: u.e}], function (em) { u.e = em })
    (function (cb) {
      if (changed) npm.config.del("_auth")
      registry.adduser(u.u, u.p, u.e, function (er) {
        if (er) return cb(er)
        ini.set("username", u.u, "user")
        ini.set("_password", u.p, "user")
        ini.set("email", u.e, "user")
        log("Authorized user " + u.u, "adduser")
        ini.save("user", cb)
      })
    })
    ()
}

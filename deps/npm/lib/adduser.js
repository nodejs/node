
module.exports = adduser

var ini = require("./utils/ini.js")
  , log = require("npmlog")
  , npm = require("./npm.js")
  , registry = npm.registry
  , read = require("read")
  , crypto

try {
  crypto = process.binding("crypto") && require("crypto")
} catch (ex) {}

adduser.usage = "npm adduser\nThen enter stuff at the prompts"

function adduser (args, cb) {
  if (!crypto) return cb(new Error(
    "You must compile node with ssl support to use the adduser feature"))

  var c = { u : npm.config.get("username")
          , p : npm.config.get("_password")
          , e : npm.config.get("email")
          }
    , changed = false
    , u = {}
    , fns = [readUsername, readPassword, readEmail, save]

  loop()
  function loop (er) {
    if (er) return cb(er)
    var fn = fns.shift()
    if (fn) return fn(c, u, loop)
    cb()
  }
}

function readUsername (c, u, cb) {
  read({prompt: "Username: ", default: c.u}, function (er, un) {
    c.changed = c.u !== un
    u.u = un
    cb(er)
  })
}

function readPassword (c, u, cb) {
  if (!c.changed) {
    u.p = c.p
    return cb()
  }
  read({prompt: "Password: ", silent: true}, function (er, pw) {
    u.p = pw
    cb(er)
  })
}

function readEmail (c, u, cb) {
  read({prompt: "Email: ", default: c.e}, function (er, em) {
    u.e = em
    cb(er)
  })
}

function save (c, u, cb) {
  if (c.changed) {
    delete registry.auth
    delete registry.username
    delete registry.password
    registry.username = u.u
    registry.password = u.p
  }

  // save existing configs, but yank off for this PUT
  registry.adduser(u.u, u.p, u.e, function (er) {
    if (er) return cb(er)
    registry.username = u.u
    registry.password = u.p
    registry.email = u.e
    ini.set("username", u.u, "user")
    ini.set("_password", u.p, "user")
    ini.set("email", u.e, "user")
    log.info("adduser", "Authorized user %s", u.u)
    ini.save("user", cb)
  })
}

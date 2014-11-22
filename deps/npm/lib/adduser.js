
module.exports = adduser

var log = require("npmlog")
  , npm = require("./npm.js")
  , registry = npm.registry
  , read = require("read")
  , userValidate = require("npm-user-validate")
  , crypto

try {
  crypto = process.binding("crypto") && require("crypto")
} catch (ex) {}

adduser.usage = "npm adduser\nThen enter stuff at the prompts"

function adduser (args, cb) {
  npm.spinner.stop()
  if (!crypto) return cb(new Error(
    "You must compile node with ssl support to use the adduser feature"))

  var creds = npm.config.getCredentialsByURI(npm.config.get("registry"))
  var c = { u : creds.username || ""
          , p : creds.password || ""
          , e : creds.email || ""
          }
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
  var v = userValidate.username
  read({prompt: "Username: ", default: c.u || ""}, function (er, un) {
    if (er) {
      return cb(er.message === "cancelled" ? er.message : er)
    }

    // make sure it's valid.  we have to do this here, because
    // couchdb will only ever say "bad password" with a 401 when
    // you try to PUT a _users record that the validate_doc_update
    // rejects for *any* reason.

    if (!un) {
      return readUsername(c, u, cb)
    }

    var error = v(un)
    if (error) {
      log.warn(error.message)
      return readUsername(c, u, cb)
    }

    c.changed = c.u !== un
    u.u = un
    cb(er)
  })
}

function readPassword (c, u, cb) {
  var v = userValidate.pw

  var prompt
  if (c.p && !c.changed) {
    prompt = "Password: (or leave unchanged) "
  } else {
    prompt = "Password: "
  }

  read({prompt: prompt, silent: true}, function (er, pw) {
    if (er) {
      return cb(er.message === "cancelled" ? er.message : er)
    }

    if (!c.changed && pw === "") {
      // when the username was not changed,
      // empty response means "use the old value"
      pw = c.p
    }

    if (!pw) {
      return readPassword(c, u, cb)
    }

    var error = v(pw)
    if (error) {
      log.warn(error.message)
      return readPassword(c, u, cb)
    }

    c.changed = c.changed || c.p !== pw
    u.p = pw
    cb(er)
  })
}

function readEmail (c, u, cb) {
  var v = userValidate.email
  var r = { prompt: "Email: (this IS public) ", default: c.e || "" }
  read(r, function (er, em) {
    if (er) {
      return cb(er.message === "cancelled" ? er.message : er)
    }

    if (!em) {
      return readEmail(c, u, cb)
    }

    var error = v(em)
    if (error) {
      log.warn(error.message)
      return readEmail(c, u, cb)
    }

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
  npm.spinner.start()

  // save existing configs, but yank off for this PUT
  var uri   = npm.config.get("registry")
  var scope = npm.config.get("scope")

  // there may be a saved scope and no --registry (for login)
  if (scope) {
    if (scope.charAt(0) !== "@") scope = "@" + scope

    var scopedRegistry = npm.config.get(scope + ":registry")
    if (scopedRegistry) uri = scopedRegistry
  }

  registry.adduser(uri, u.u, u.p, u.e, function (er, doc) {
    npm.spinner.stop()
    if (er) return cb(er)

    registry.username = u.u
    registry.password = u.p
    registry.email = u.e

    // don't want this polluting the configuration
    npm.config.del("_token", "user")

    if (scope) npm.config.set(scope + ":registry", uri, "user")

    if (doc && doc.token) {
      npm.config.setCredentialsByURI(uri, {
        token : doc.token
      })
    }
    else {
      npm.config.setCredentialsByURI(uri, {
        username   : u.u,
        password   : u.p,
        email      : u.e,
        alwaysAuth : npm.config.get("always-auth")
      })
    }

    log.info("adduser", "Authorized user %s", u.u)
    npm.config.save("user", cb)
  })
}

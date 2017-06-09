var log = require('npmlog')
var npm = require('../npm.js')
var read = require('read')
var userValidate = require('npm-user-validate')
var output = require('../utils/output')
var chain = require('slide').chain

module.exports.login = function login (creds, registry, scope, cb) {
  var c = {
    u: creds.username || '',
    p: creds.password || '',
    e: creds.email || ''
  }
  var u = {}

  chain([
    [readUsername, c, u],
    [readPassword, c, u],
    [readEmail, c, u],
    [save, c, u, registry, scope]
  ], function (err, res) {
    cb(err, res && res[res.length - 1])
  })
}

function readUsername (c, u, cb) {
  var v = userValidate.username
  read({prompt: 'Username: ', default: c.u || ''}, function (er, un) {
    if (er) {
      return cb(er.message === 'cancelled' ? er.message : er)
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
    prompt = 'Password: (or leave unchanged) '
  } else {
    prompt = 'Password: '
  }

  read({prompt: prompt, silent: true}, function (er, pw) {
    if (er) {
      return cb(er.message === 'cancelled' ? er.message : er)
    }

    if (!c.changed && pw === '') {
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
  var r = { prompt: 'Email: (this IS public) ', default: c.e || '' }
  read(r, function (er, em) {
    if (er) {
      return cb(er.message === 'cancelled' ? er.message : er)
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

function save (c, u, registry, scope, cb) {
  var params = {
    auth: {
      username: u.u,
      password: u.p,
      email: u.e
    }
  }
  npm.registry.adduser(registry, params, function (er, doc) {
    if (er) return cb(er)

    var newCreds = (doc && doc.token)
    ? {
      token: doc.token
    }
    : {
      username: u.u,
      password: u.p,
      email: u.e,
      alwaysAuth: npm.config.get('always-auth')
    }

    log.info('adduser', 'Authorized user %s', u.u)
    var scopeMessage = scope ? ' to scope ' + scope : ''
    output('Logged in as %s%s on %s.', u.u, scopeMessage, registry)

    cb(null, newCreds)
  })
}

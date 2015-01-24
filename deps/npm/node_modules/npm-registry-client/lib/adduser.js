module.exports = adduser

var url = require("url")
var assert = require("assert")

function adduser (uri, params, cb) {
  assert(typeof uri === "string", "must pass registry URI to adduser")
  assert(
    params && typeof params === "object",
    "must pass params to adduser"
  )
  assert(typeof cb === "function", "must pass callback to adduser")

  assert(params.auth && typeof params.auth, "must pass auth to adduser")
  var auth = params.auth
  assert(typeof auth.username === "string", "must include username in auth")
  assert(typeof auth.password === "string", "must include password in auth")
  assert(typeof auth.email === "string", "must include email in auth")

  // normalize registry URL
  if (uri.slice(-1) !== "/") uri += "/"

  var username = auth.username.trim()
  var password = auth.password.trim()
  var email = auth.email.trim()

  // validation
  if (!username) return cb(new Error("No username supplied."))
  if (!password) return cb(new Error("No password supplied."))
  if (!email) return cb(new Error("No email address supplied."))
  if (!email.match(/^[^@]+@[^\.]+\.[^\.]+/)) {
    return cb(new Error("Please use a real email address."))
  }

  var userobj = {
    _id      : "org.couchdb.user:"+username,
    name     : username,
    password : password,
    email    : email,
    type     : "user",
    roles    : [],
    date     : new Date().toISOString()
  }

  var token = this.config.couchToken
  if (this.couchLogin) this.couchLogin.token = null

  cb = done.call(this, token, cb)

  var logObj = Object.keys(userobj).map(function (k) {
    if (k === "password") return [k, "XXXXX"]
    return [k, userobj[k]]
  }).reduce(function (s, kv) {
    s[kv[0]] = kv[1]
    return s
  }, {})

  this.log.verbose("adduser", "before first PUT", logObj)

  var client = this

  uri = url.resolve(uri, "-/user/org.couchdb.user:" + encodeURIComponent(username))
  var options = {
    method : "PUT",
    body   : userobj,
    auth   : auth
  }
  this.request(
    uri,
    options,
    function (error, data, json, response) {
      if (!error || !response || response.statusCode !== 409) {
        return cb(error, data, json, response)
      }

      client.log.verbose("adduser", "update existing user")
      return client.request(
        uri+"?write=true",
        { body : userobj, auth : auth },
        function (er, data, json, response) {
          if (er || data.error) {
            return cb(er, data, json, response)
          }
          Object.keys(data).forEach(function (k) {
            if (!userobj[k] || k === "roles") {
              userobj[k] = data[k]
            }
          })
          client.log.verbose("adduser", "userobj", logObj)
          client.request(uri+"/-rev/"+userobj._rev, options, cb)
        }
      )
    }
  )

  function done (token, cb) {
    return function (error, data, json, response) {
      if (!error && (!response || response.statusCode === 201)) {
        return cb(error, data, json, response)
      }

      // there was some kind of error, reinstate previous auth/token/etc.
      if (client.couchLogin) {
        client.couchLogin.token = token
        if (client.couchLogin.tokenSet) {
          client.couchLogin.tokenSet(token)
        }
      }

      client.log.verbose("adduser", "back", [error, data, json])
      if (!error) {
        error = new Error(
          (response && response.statusCode || "") + " " +
          "Could not create user\n" + JSON.stringify(data)
        )
      }

      if (response && (response.statusCode === 401 || response.statusCode === 403)) {
        client.log.warn("adduser", "Incorrect username or password\n" +
                                   "You can reset your account by visiting:\n" +
                                   "\n" +
                                   "    https://npmjs.org/forgot\n")
      }

      return cb(error)
    }
  }
}

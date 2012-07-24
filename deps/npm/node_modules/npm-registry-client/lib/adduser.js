
module.exports = adduser

var uuid = require("node-uuid")
  , crypto

try {
} catch (ex) {}

function sha (s) {
  return crypto.createHash("sha1").update(s).digest("hex")
}

function adduser (username, password, email, cb) {
  if (!crypto) crypto = require("crypto")

  password = ("" + (password || "")).trim()
  if (!password) return cb(new Error("No password supplied."))

  email = ("" + (email || "")).trim()
  if (!email) return cb(new Error("No email address supplied."))
  if (!email.match(/^[^@]+@[^\.]+\.[^\.]+/)) {
    return cb(new Error("Please use a real email address."))
  }

  if (password.indexOf(":") !== -1) return cb(new Error(
    "Sorry, ':' chars are not allowed in passwords.\n"+
    "See <https://issues.apache.org/jira/browse/COUCHDB-969> for why."))

  var salt = uuid()
    , userobj =
      { name : username
      , salt : salt
      , password_sha : sha(password + salt)
      , email : email
      , _id : 'org.couchdb.user:'+username
      , type : "user"
      , roles : []
      , date: new Date().toISOString()
      }

  // pluck off any other username/password/token.  it needs to be the
  // same as the user we're becoming now.  replace them on error.
  var pre = { username: this.username
            , password: this.password
            , auth: this.auth
            , token: this.token }

  this.token = null
  if (this.couchLogin) {
    this.couchLogin.token = null
  }
  this.username = null
  this.password = null
  this.auth = null

  cb = done.call(this, cb, pre)

  var logObj = Object.keys(userobj).map(function (k) {
    if (k === 'salt' || k === 'password_sha') return [k, 'XXXXX']
    return [k, userobj[k]]
  }).reduce(function (s, kv) {
    s[kv[0]] = kv[1]
    return s
  }, {})

  this.log.verbose("adduser", "before first PUT", logObj)

  this.request('PUT'
    , '/-/user/org.couchdb.user:'+encodeURIComponent(username)
    , userobj
    , function (error, data, json, response) {
        // if it worked, then we just created a new user, and all is well.
        // but if we're updating a current record, then it'll 409 first
        if (error && !this.auth) {
          // must be trying to re-auth on a new machine.
          // use this info as auth
          var b = new Buffer(username + ":" + password)
          this.auth = b.toString("base64")
          this.username = username
          this.password = password
        }

        if (!error || !response || response.statusCode !== 409) {
          return cb(error, data, json, response)
        }

        this.log.verbose("adduser", "update existing user")
        return this.request('GET'
          , '/-/user/org.couchdb.user:'+encodeURIComponent(username)
          , function (er, data, json, response) {
              if (er || data.error) {
                return cb(er, data, json, response)
              }
              Object.keys(data).forEach(function (k) {
                if (!userobj[k]) {
                  userobj[k] = data[k]
                }
              })
              this.log.verbose("adduser", "userobj", logObj)
              this.request('PUT'
                , '/-/user/org.couchdb.user:'+encodeURIComponent(username)
                  + "/-rev/" + userobj._rev
                , userobj
                , cb )
            }.bind(this))
      }.bind(this))
}

function done (cb, pre) {
  return function (error, data, json, response) {
    if (!error && (!response || response.statusCode === 201)) {
      return cb(error, data, json, response)
    }

    // there was some kind of error, re-instate previous auth/token/etc.
    this.token = pre.token
    if (this.couchLogin) {
      this.couchLogin.token = this.token
      if (this.couchLogin.tokenSet) {
        this.couchLogin.tokenSet(pre.token)
      }
    }
    this.username = pre.username
    this.password = pre.password
    this.auth = pre.auth

    this.log.verbose("adduser", "back", [error, data, json])
    if (!error) {
      error = new Error( (response && response.statusCode || "") + " "+
      "Could not create user\n"+JSON.stringify(data))
    }
    if (response
        && (response.statusCode === 401 || response.statusCode === 403)) {
      this.log.warn("adduser", "Incorrect username or password\n"
              +"You can reset your account by visiting:\n"
              +"\n"
              +"    http://admin.npmjs.org/reset\n")
    }

    return cb(error)
  }.bind(this)
}

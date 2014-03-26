module.exports = adduser

var crypto = require('crypto')

function sha (s) {
  return crypto.createHash("sha1").update(s).digest("hex")
}

function adduser (username, password, email, cb) {

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

  var salt = crypto.randomBytes(30).toString('hex')
    , userobj =
      { name : username
      , password : password
      , email : email
      , _id : 'org.couchdb.user:'+username
      , type : "user"
      , roles : []
      , date: new Date().toISOString()
      }

  // pluck off any other username/password/token.  it needs to be the
  // same as the user we're becoming now.  replace them on error.
  var pre = { username: this.conf.get('username')
            , password: this.conf.get('_password')
            , auth: this.conf.get('_auth')
            , token: this.conf.get('_token') }

  this.conf.del('_token')
  this.conf.del('username')
  this.conf.del('_auth')
  this.conf.del('_password')
  if (this.couchLogin) {
    this.couchLogin.token = null
  }

  cb = done.call(this, cb, pre)

  var logObj = Object.keys(userobj).map(function (k) {
    if (k === 'password') return [k, 'XXXXX']
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
        if (error && !this.conf.get('_auth')) {
          // must be trying to re-auth on a new machine.
          // use this info as auth
          var b = new Buffer(username + ":" + password)
          this.conf.set('_auth', b.toString("base64"))
          this.conf.set('username', username)
          this.conf.set('_password', password)
        }

        if (!error || !response || response.statusCode !== 409) {
          return cb(error, data, json, response)
        }

        this.log.verbose("adduser", "update existing user")
        return this.request('GET'
          , '/-/user/org.couchdb.user:'+encodeURIComponent(username) +
            '?write=true'
          , function (er, data, json, response) {
              if (er || data.error) {
                return cb(er, data, json, response)
              }
              Object.keys(data).forEach(function (k) {
                if (!userobj[k] || k === 'roles') {
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
    this.conf.set('_token', pre.token)
    if (this.couchLogin) {
      this.couchLogin.token = pre.token
      if (this.couchLogin.tokenSet) {
        this.couchLogin.tokenSet(pre.token)
      }
    }
    this.conf.set('username', pre.username)
    this.conf.set('_password', pre.password)
    this.conf.set('_auth', pre.auth)

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
              +"    https://npmjs.org/forgot\n")
    }

    return cb(error)
  }.bind(this)
}

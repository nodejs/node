
module.exports = adduser

var uuid = require("node-uuid")
  , request = require("./request.js")
  , log = require("../log.js")
  , npm = require("../../npm.js")
  , crypto

try {
  crypto = process.binding("crypto") && require("crypto")
} catch (ex) {}

function sha (s) {
  return crypto.createHash("sha1").update(s).digest("hex")
}

function adduser (username, password, email, cb) {
  if (!crypto) return cb(new Error(
    "You must compile node with ssl support to use the adduser feature"))

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
  cb = done(cb)
  log.verbose(userobj, "before first PUT")
  request.PUT
    ( '/-/user/org.couchdb.user:'+encodeURIComponent(username)
    , userobj
    , function (error, data, json, response) {
        // if it worked, then we just created a new user, and all is well.
        // but if we're updating a current record, then it'll 409 first
        if (error && !npm.config.get("_auth")) {
          // must be trying to re-auth on a new machine.
          // use this info as auth
          npm.config.set("username", username)
          npm.config.set("_password", password)
          var b = new Buffer(username + ":" + password)
          npm.config.set("_auth", b.toString("base64"))
        }
        if (!error || !response || response.statusCode !== 409) {
          return cb(error, data, json, response)
        }
        log.verbose("update existing user", "adduser")
        return request.GET
          ( '/-/user/org.couchdb.user:'+encodeURIComponent(username)
          , function (er, data, json, response) {
              userobj._rev = data._rev
              userobj.roles = data.roles
              log.verbose(userobj, "userobj")
              request.PUT
                ( '/-/user/org.couchdb.user:'+encodeURIComponent(username)
                  + "/-rev/" + userobj._rev
                , userobj
                , cb )
            }
          )
      }
    )
}

function done (cb) { return function (error, data, json, response) {
  if (!error && (!response || response.statusCode === 201)) {
    return cb(error, data, json, response)
  }
  log.verbose([error, data, json], "back from adduser")
  if (!error) {
    error = new Error( (response && response.statusCode || "") + " "+
    "Could not create user\n"+JSON.stringify(data))
  }
  if (response
      && (response.statusCode === 401 || response.statusCode === 403)) {
    log.warn("Incorrect username or password\n"
            +"You can reset your account by visiting:\n"
            +"\n"
            +"    http://admin.npmjs.org/reset\n")
  }

  return cb(error)
}}

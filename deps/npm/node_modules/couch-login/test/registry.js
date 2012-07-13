// Should be able to use this module to log into the registry, as well.

var tap = require('tap')
, CouchLogin = require('../couch-login.js')

// Yeah, go ahead and abuse my staging server, whatevs.

var auth = { name: 'testuser', password: 'test' }
, newAuth = { name: 'testuser', password: 'asdfasdf' }
, couch = new CouchLogin('https://staging.npmjs.org/')
, u = '/_users/org.couchdb.user:' + auth.name
, userRecordMarker

// simulate the 'must change password on next login' thing
newAuth.mustChangePass = true
auth.mustChangePass = false


function okStatus (t, res) {
  var x = { found: res.statusCode, wanted: 'around 200' }
  var r = res.statusCode
  x.ok = (r >= 200 && r < 300)
  return t.ok(x.ok, 'Status code should be 200-ish', x)
}

tap.test('login', function (t) {
  couch.login(auth, function (er, res, data) {
    t.ifError(er)
    if (er) return t.end()
    okStatus(t, res)
    t.deepEqual(data, { ok: true, name: 'testuser', roles: [] })
    t.ok(couch.token)
    t.deepEqual(couch.token,
      { AuthSession: couch.token && couch.token.AuthSession,
        version: '1',
        expires: couch.token && couch.token.expires,
        path: '/',
        httponly: true })
    t.ok(couch.token, 'has token')
    t.end()
  })
})

var userRecord
tap.test('get', function (t) {
  couch.get(u, function (er, res, data) {
    t.ifError(er)
    if (er) return t.end()
    t.ok(data, 'data')
    t.ok(couch.token, 'token')
    userRecord = data
    okStatus(t, res)
    t.end()
  })
})

var userRecordMarker = require('crypto').randomBytes(30).toString('base64')
tap.test('add key to user record', function (t) {
  userRecord.testingCouchLogin = userRecordMarker
  var revved = u + '?rev=' + userRecord._rev
  couch.put(revved, userRecord, function (er, res, data) {
    t.ifError(er)
    if (er) return t.end()
    okStatus(t, res)
    t.ok(data, 'data')
    t.ok(couch.token, 'token')
    // get again so we have the current rev
    couch.get(u, function (er, res, data) {
      t.ifError(er)
      if (er) return t.end()
      okStatus(t, res)
      t.ok(data)
      t.ok(userRecord)
      t.equal(data.testingCouchLogin, userRecord.testingCouchLogin)
      userRecord = data
      t.end()
    })
  })
})

tap.test('remove key', function (t) {
  var revved = u + '?rev=' + userRecord._rev
  delete userRecord.testingCouchLogin
  couch.put(revved, userRecord, function (er, res, data) {
    t.ifError(er)
    if (er) return t.end()
    okStatus(t, res)
    t.ok(couch.token, 'token')
    couch.get(u, function (er, res, data) {
      t.ifError(er)
      if (er) return t.end()
      okStatus(t, res)
      t.ok(data, 'data')
      t.ok(couch.token, 'token')
      t.equal(data.testingCouchLogin, undefined)
      userRecord = data
      t.end()
    })
  })
})

var crypto = require('crypto')
function sha (s) {
  return crypto.createHash("sha1").update(s).digest("hex")
}

tap.test('change password manually', function (t) {
  var revved = u + '?rev=' + userRecord._rev
  , newPass = newAuth.password
  , newSalt = 'test-salt-two'
  , newSha = sha(newPass + newSalt)

  userRecord.salt = newSalt
  userRecord.password_sha = newSha
  couch.put(revved, userRecord, function (er, res, data) {
    t.ifError(er)
    if (er) return t.end()
    okStatus(t, res)

    // changing password invalidates session.
    // need to re-login
    couch.login(newAuth, function (er, res, data) {
      t.ifError(er)
      if (er) return t.end()
      okStatus(t, res)

      couch.get(u, function (er, res, data) {
        t.ifError(er)
        if (er) return t.end()
        okStatus(t, res)
        t.ok(data, 'data')
        t.ok(couch.token, 'token')
        t.equal(data.testingCouchLogin, undefined)
        userRecord = data
        t.end()
      })
    })
  })
})

tap.test('change password back manually', function (t) {
  var revved = u + '?rev=' + userRecord._rev
  , newPass = auth.password
  , newSalt = 'test-salt'
  , newSha = sha(newPass + newSalt)

  userRecord.salt = newSalt
  userRecord.password_sha = newSha
  couch.put(revved, userRecord, function (er, res, data) {
    t.ifError(er)
    if (er) return t.end()
    okStatus(t, res)
    t.ok(data, 'data')
    t.ok(couch.token, 'token')

    couch.login(auth, function (er, res, data) {
      t.ifError(er)
      if (er) return t.end()
      okStatus(t, res)

      couch.get(u, function (er, res, data) {
        t.ifError(er)
        if (er) return t.end()
        okStatus(t, res)
        t.ok(data, 'data')
        t.ok(couch.token, 'token')
        userRecord = data
        t.end()
      })
    })
  })
})

tap.test('change password easy', function (t) {
  couch.changePass(newAuth, function (er, res, data) {
    t.ifError(er)
    if (er) return t.end()
    okStatus(t, res)

    couch.get(u, function (er, res, data) {
      t.ifError(er)
      if (er) return t.end()
      okStatus(t, res)
      t.ok(data, 'data')
      t.ok(couch.token, 'token')
      t.equal(data.testingCouchLogin, undefined)
      t.equal(data.mustChangePass, true)
      userRecord = data
      t.end()
    })
  })
})

tap.test('change password back easy', function (t) {
  couch.changePass(auth, function (er, res, data) {
    t.ifError(er)
    if (er) return t.end()
    okStatus(t, res)

    couch.get(u, function (er, res, data) {
      t.ifError(er)
      if (er) return t.end()
      okStatus(t, res)
      t.ok(data, 'data')
      t.ok(couch.token, 'token')
      t.equal(data.testingCouchLogin, undefined)
      t.equal(data.mustChangePass, false)
      userRecord = data
      t.end()
    })
  })
})


tap.test('logout', function (t) {
  couch.logout(function (er, res, data) {
    t.ifError(er)
    if (er) return t.end()
    okStatus(t, res)
    t.ok(data, 'data')
    t.notOk(couch.token, 'token')
    t.end()
  })
})

var signupUser = { name: 'test-user-signup', password: 'signup-test' }

tap.test('sign up as new user', function (t) {
  couch.signup(signupUser, function (er, res, data) {
    t.ifError(er)
    if (er) return t.end()
    okStatus(t, res)
    t.ok(data, 'data')
    t.has(data,
      { _id: 'org.couchdb.user:test-user-signup',
        name: 'test-user-signup',
        roles: [],
        type: 'user' })
    t.ok(data._rev, 'rev')
    t.ok(data.date, 'date')
    t.ok(data.password_sha, 'hash')
    t.ok(data.salt, 'salt')
    t.ok(couch.token, 'token')
    // now delete account
    var name = signupUser.name
    couch.deleteAccount(name, function (er, res, data) {
      t.ifError(er, 'should be no error deleting account')
      if (er) return t.end()
      okStatus(t, res)
      t.ok(data, 'data')
      t.end()
    })
  })
})

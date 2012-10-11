var test = require('tap').test
var CouchLogin = require('../couch-login.js')

var auth = { name: 'testuser', password: 'test' }
, newAuth = { name: 'testuser', password: 'asdf', mustChangePass: true }
, db = 'http://localhost:15985/'
, couch = new CouchLogin(db)
, u = '/_users/org.couchdb.user:' + auth.name
, admin = { name: 'admin', password: 'admin' }
, newUser = { name: 'testuser', password: 'test' }
, newUserCouch = null
, authToken = null

newUser.name += Math.floor(Math.random() * 1E9)

var okGlobal = Object.keys(global)

var adminCouch = new CouchLogin(db, 'basic')

function okStatus (t, res) {
  var x = { found: res.statusCode, wanted: 'around 200' }
  var r = res.statusCode
  x.ok = (r >= 200 && r < 300)
  return t.ok(x.ok, 'Status code should be 200-ish', x)
}

test('adminCouch login', function (t) {
  t.deepEqual(Object.keys(global), okGlobal)
  console.error('adminCouch login')
  adminCouch.login(admin, function (er, res, data) {
    if (er)
      throw er
    okStatus(t, res)
    t.ok(data)
    t.end()
  })
})

test('get the user data as admin', function (t) {
  t.deepEqual(Object.keys(global), okGlobal)
  console.error('2')
  adminCouch.get(u, function (er, res, data) {
    if (er)
      throw er
    okStatus(t, res)
    t.ok(data)
    t.end()
  })
})

test('admin user changes the password for non-admin user', function (t) {
  console.error(3)
  t.deepEqual(Object.keys(global), okGlobal)
  adminCouch.changePass(newAuth, function (er, res, data) {
    if (er)
      throw er
    okStatus(t, res)
    t.ok(data)
    t.end()
  })
})

test('testuser logs in', function (t) {
  console.error(4)
  t.deepEqual(Object.keys(global), okGlobal)
  couch.login(newAuth, function (er, res, data) {
    if (er)
      throw er
    okStatus(t, res)
    t.deepEqual(data, { ok: true, name: 'testuser', roles: [] })
    authToken = couch.token
    t.end()
  })
})

// test('testuser changes password', function (t) {
//   couch = new CouchLogin(db)
//   couch.token = authToken
//   couch.changePass(auth, function (er, res, data) {
//     if (er)
//       throw er
//     okStatus(t, res)
//     console.error(data)
//     t.ok(data)
//     t.end()
//   })
// })

test('new user signup', function (t) {
  t.deepEqual(Object.keys(global), okGlobal)
  newUserCouch = new CouchLogin(db)
  newUserCouch.signup(newUser, function (er, res, data) {
    if (er)
      throw er
    okStatus(t, res)
    console.error(data)
    t.ok(data)
    t.end()
  })
})

test('delete newUser account', function (t) {
  t.deepEqual(Object.keys(global), okGlobal)
  newUserCouch.deleteAccount(newUser.name, function (er, res, data) {
    if (er)
      throw er
    okStatus(t, res)
    console.error(data)
    t.ok(data)
    t.end()
  })
})

var test = require('tap').test
var request = require('request')
var parse = require('parse-json-response')

test('update profile', function (t) {

  var id = 'org.couchdb.user:user'
  var u = 'http://admin:admin@localhost:15986/_users/' + id

  request.get({
    url: u
  , json: true
  }, function (err, res, prof) {
    prof.github = 'user'
    prof.homepage = 'http://www.user.com'

    request.post({
      url: 'http://admin:admin@localhost:15986/_users/_design/scratch/_update/profile/' + id
    , body: prof
    , json: true
    }, function (err, res, body) {
      if (err) throw err

      request.get({
        url: u
      , json: true
      }, function (err, res, body) {

        t.same(body.github, prof.github)
        t.same(body.homepage, prof.homepage)
        t.end()
      })
    })
  })
})

test('update email', function (t) {

  var id = 'org.couchdb.user:user'
  var u = 'http://admin:admin@localhost:15986/_users/' + id

  request.get({
    url: u
  , json: true
  }, function (err, res, prof) {
    t.ifError(err, 'user metadata fetched')
    prof.email = 'new@email.com'

    request.post({
      url: 'http://admin:admin@localhost:15986/_users/_design/scratch/_update/email/' + id
    , body: prof
    , json: true
    }, function (err, res, body) {
      t.ifError(err, 'user email address changed')

      request.get({
        url: u
      , json: true
      }, function (err, res, body) {
        t.ifError(err, 'user record updated on second fetch')
        t.same(body.email, prof.email)
        t.end()
      })
    })
  })
})

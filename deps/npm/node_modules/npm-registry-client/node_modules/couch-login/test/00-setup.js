// start the couchdb spinning as a detached child process.
// the zz-teardown.js test kills it.
//
// localhost:15985 ==> couchdb
// 127.0.0.1:15985 ==> npm registry

var spawn = require('child_process').spawn
var test = require('tap').test
var path = require('path')
var fs = require('fs')
var request = require('request')

// run with the cwd of the main program.
var cwd = path.dirname(__dirname)

var conf = path.resolve(__dirname, 'fixtures', 'couch.ini')
var pidfile = path.resolve(__dirname, 'fixtures', 'pid')
var logfile = path.resolve(__dirname, 'fixtures', 'couch.log')
var started = /Apache CouchDB has started on http:\/\/127\.0\.0\.1:15985\/\n$/

test('start couch as a zombie child', function (t) {
  var fd = fs.openSync(pidfile, 'wx')

  try { fs.unlinkSync(logfile) } catch (er) {}

  var child = spawn('couchdb', ['-a', conf], {
    detached: true,
    stdio: 'ignore',
    cwd: cwd
  })
  child.unref()
  t.ok(child.pid)
  fs.writeSync(fd, child.pid + '\n')
  fs.closeSync(fd)

  // wait for it to create a log, give it 5 seconds
  var start = Date.now()
  fs.readFile(logfile, function R (er, log) {
    log = log ? log.toString() : ''
    if (!er && !log.match(started))
      er = new Error('not started yet')
    if (er) {
      if (Date.now() - start < 5000)
        return setTimeout(function () {
          fs.readFile(logfile, R)
        }, 100)
      else
        throw er
    }
    t.pass('relax')
    t.end()
  })
})

// set up the testuser account that we'll be using everywhere.
// first delete any existing one, so that we don't end up with
// some newer copy taking over.
test('create testuser', function (t) {
  var u = 'http://admin:admin@localhost:15985/_users/org.couchdb.user:testuser'
  var rev

  request.get({ url: u, json: true }, function (er, res, data) {
    if (er)
      throw er
    rev = data._rev
    if (res.statusCode === 404)
      put()
    else
      del()
  })

  function del () {
    request.del(u + '?rev=' + rev, function (er, res, data) {
      if (er)
        throw er
      put()
    })
  }

  function put () {
    request.put({
      url: u,
      body: {
        _id: 'org.couchdb.user:testuser',
        name: 'testuser',
        roles: [],
        type: 'user',
        password_sha: 'e23952b517695e6bb72ecf060e10bf1b35bf7e0b',
        salt: '83695c9b64d3b48b94c9dda0cd691e72',
        date: '2012-09-26T16:49:30.175Z'
      },
      json: true
    }, function (er, res, data) {
      if (er)
        throw er
      t.ok(data.ok, 'user created')
      t.end()
    })
  }
})

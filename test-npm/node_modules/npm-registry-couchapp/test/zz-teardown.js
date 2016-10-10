// kill the couchdb process that's running as a detached child process
// started by the 00-setup.js test

var fs = require('fs')
var rimraf = require('rimraf')
var test = require('tap').test
var path = require('path')
var pidfile = path.resolve(__dirname, 'fixtures', 'pid')
var _users = path.resolve(__dirname, 'fixtures', '_users.couch')
var db = path.resolve(__dirname, 'fixtures', 'registry.couch')
var log = path.resolve(__dirname, 'fixtures', 'couch.log')
var repl = path.resolve(__dirname, 'fixtures', '_replicator.couch')
var rdes = path.resolve(__dirname, 'fixtures', '.registry_design')
var udes = path.resolve(__dirname, 'fixtures', '._users_design')
var dotDelete = path.resolve(__dirname, 'fixtures', '.delete')
var urifile = path.resolve(__dirname, 'fixtures', 'couch.uri')

if (process.env.TRAVIS) {
  test('nocleanup', function (t) {
    t.pass('No cleanup on travis')
    t.end()
  })
}
else {
  test('cleanup', function (t) {
    try {
      var pid = fs.readFileSync(pidfile)
    } catch (er) {}

    if (pid) {
      try { process.kill(pid) } catch (er) {
        // ok if already killed
        t.equal(er.code, 'ESRCH')
      }
    }
    var files = [ pidfile, repl, log, _users, db, rdes, dotDelete, urifile ]
    if (!process.env.TRAVIS) {
        files.push(udes);
    }
    files.forEach(function(file) {
      rimraf.sync(file)
    })

    t.pass('couch is no more')
    t.end()
  })
}
// kill the couchdb process that's running as a detached child process
// started by the 00-setup.js test

var fs = require('fs')
var test = require('tap').test
var path = require('path')
var pidfile = path.resolve(__dirname, 'fixtures', 'pid')
var _users = path.resolve(__dirname, 'fixtures', '_users.couch')

test('kill all the users', function (t) {
  fs.unlinkSync(_users)
  t.pass('_users db deleted')
  t.end()
})

test('craigslist (well, how do you get rid of YOUR couches?)', function (t) {
  var pid = fs.readFileSync(pidfile)
  fs.unlinkSync(pidfile)
  process.kill(pid)
  t.pass('couch is no more')
  t.end()
})


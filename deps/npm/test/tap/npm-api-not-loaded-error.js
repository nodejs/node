var test = require('tap').test
var npm = require('../..')
var path = require('path')
var rimraf = require('rimraf')
var npmrc = path.join(__dirname, 'npmrc')
var fs = require('fs')

test('setup', function (t) {
  fs.writeFileSync(npmrc, 'foo = bar\n', 'ascii')
  t.end()
})

test('calling set/get on config pre-load should throw', function (t) {
  var threw = true
  try {
    npm.config.get('foo')
    threw = false
  } catch (er) {
    t.equal(er.message, 'npm.load() required')
  } finally {
    t.ok(threw, 'get before load should throw')
  }

  threw = true
  try {
    npm.config.set('foo', 'bar')
    threw = false
  } catch (er) {
    t.equal(er.message, 'npm.load() required')
  } finally {
    t.ok(threw, 'set before load should throw')
  }

  npm.load({ userconfig: npmrc }, function (er) {
    if (er) throw er

    t.equal(npm.config.get('foo'), 'bar')
    npm.config.set('foo', 'baz')
    t.equal(npm.config.get('foo'), 'baz')
    t.end()
  })
})

test('cleanup', function (t) {
  rimraf.sync(npmrc)
  t.end()
})

var mr = require('npm-registry-mock')
var test = require('tap').test

var common = require('../common-tap.js')
var basedir = common.pkg
var cachedir = common.cache

var server

var EXEC_OPTS = {
  cwd: basedir,
  stdio: [0, 'pipe', 2],
  env: common.newEnv().extend({
    npm_config_cache: cachedir
  })
}

var jashkenas = {
  name: 'jashkenas',
  email: 'jashkenas@gmail.com'
}

var othiym23 = {
  name: 'othiym23',
  email: 'forrest@npmjs.com'
}

var bcoe = {
  name: 'bcoe',
  email: 'ben@npmjs.com'
}

function shrt (user) {
  return user.name + ' <' + user.email + '>\n'
}

function mocks (server) {
  server.get('/-/user/org.couchdb.user:othiym23')
    .many().reply(200, othiym23)

  // test 1
  server.get('/underscore')
    .reply(200, { _id: 'underscore', _rev: 1, maintainers: [jashkenas] })
  server.put(
    '/underscore/-rev/1',
    { _id: 'underscore', _rev: 1, maintainers: [jashkenas, othiym23] },
    {}
  ).reply(200, { _id: 'underscore', _rev: 2, maintainers: [jashkenas, othiym23] })

  // test 2
  server.get('/@xxx%2fscoped')
    .reply(200, { _id: '@xxx/scoped', _rev: 1, maintainers: [bcoe] })
  server.put(
    '/@xxx%2fscoped/-rev/1',
    { _id: '@xxx/scoped', _rev: 1, maintainers: [bcoe, othiym23] },
    {}
  ).reply(200, { _id: '@xxx/scoped', _rev: 2, maintainers: [bcoe, othiym23] })

  // test 3
  server.get('/underscore')
    .reply(200, { _id: 'underscore', _rev: 2, maintainers: [jashkenas, othiym23] })

  // test 4
  server.get('/underscore')
    .reply(200, { _id: 'underscore', _rev: 2, maintainers: [jashkenas, othiym23] })
  server.put(
    '/underscore/-rev/2',
    { _id: 'underscore', _rev: 2, maintainers: [jashkenas] },
    {}
  ).reply(200, { _id: 'underscore', _rev: 3, maintainers: [jashkenas] })
}

test('setup', function (t) {
  mr({ port: common.port, plugin: mocks }, function (er, s) {
    server = s
    t.end()
  })
})

test('npm owner add', function (t) {
  common.npm(
    [
      '--loglevel', 'warn',
      '--registry', common.registry,
      'owner', 'add', 'othiym23', 'underscore'
    ],
    EXEC_OPTS,
    function (err, code, stdout, stderr) {
      t.ifError(err, 'npm owner add ran without error')
      t.notOk(code, 'npm owner add exited cleanly')
      t.notOk(stderr, 'npm owner add ran silently')
      t.equal(stdout, '+ othiym23 (underscore)\n', 'got expected add output')

      t.end()
    }
  )
})

test('npm owner add (scoped)', function (t) {
  common.npm(
    [
      '--loglevel', 'silent',
      '--registry', common.registry,
      'owner', 'add', 'othiym23', '@xxx/scoped'
    ],
    EXEC_OPTS,
    function (err, code, stdout, stderr) {
      t.ifError(err, 'npm owner add (scoped) ran without error')
      t.notOk(code, 'npm owner add (scoped) exited cleanly')
      t.notOk(stderr, 'npm owner add (scoped) ran silently')
      t.equal(stdout, '+ othiym23 (@xxx/scoped)\n', 'got expected scoped add output')

      t.end()
    }
  )
})

test('npm owner ls', function (t) {
  common.npm(
    [
      '--loglevel', 'silent',
      '--registry', common.registry,
      'owner', 'ls', 'underscore'
    ],
    EXEC_OPTS,
    function (err, code, stdout, stderr) {
      t.ifError(err, 'npm owner ls ran without error')
      t.notOk(code, 'npm owner ls exited cleanly')
      t.notOk(stderr, 'npm owner ls ran silently')
      t.equal(stdout, shrt(jashkenas) + shrt(othiym23), 'got expected ls output')

      t.end()
    }
  )
})

test('npm owner rm', function (t) {
  common.npm(
    [
      '--loglevel', 'silent',
      '--registry', common.registry,
      'owner', 'rm', 'othiym23', 'underscore'
    ],
    EXEC_OPTS,
    function (err, code, stdout, stderr) {
      t.ifError(err, 'npm owner rm ran without error')
      t.notOk(code, 'npm owner rm exited cleanly')
      t.notOk(stderr, 'npm owner rm ran silently')
      t.equal(stdout, '- othiym23 (underscore)\n', 'got expected rm output')

      t.end()
    }
  )
})

test('cleanup', function (t) {
  server.close()
  t.end()
})

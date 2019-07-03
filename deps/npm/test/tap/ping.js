var fs = require('fs')
var path = require('path')

var mkdirp = require('mkdirp')
var mr = require('npm-registry-mock')
var rimraf = require('rimraf')
var test = require('tap').test

var common = require('../common-tap.js')

var pkg = common.pkg
var opts = { cwd: pkg }

var outfile = path.join(pkg, '_npmrc')

var contents = function () {
}.toString().split('\n').slice(1, -1).join('\n')

var pingResponse = {
  host: 'registry.npmjs.org',
  ok: true,
  username: null,
  peer: 'example.com'
}

function mocks (server) {
  server.get('/-/ping?write=true').reply(200, JSON.stringify(pingResponse))
}

test('setup', function (t) {
  cleanup()
  setup()
  t.end()
})

test('npm ping', function (t) {
  mr({ port: common.port, plugin: mocks }, function (err, s) {
    if (err) throw err

    common.npm([
      'ping',
      '--registry', common.registry,
      '--loglevel', 'notice',
      '--userconfig', outfile
    ], opts, function (err, code, stdout, stderr) {
      s.close()
      t.ifError(err, 'command completed')
      t.notOk(code, 'exited OK')

      t.match(stderr, /PING/, 'ping notification output')
      t.match(stderr, /PONG/, 'pong response output')
      t.end()
    })
  })
})

test('npm ping --json', function (t) {
  mr({ port: common.port, plugin: mocks }, function (err, s) {
    if (err) throw err

    common.npm([
      'ping',
      '--json',
      '--registry', common.registry,
      '--loglevel', 'notice',
      '--userconfig', outfile
    ], opts, function (err, code, stdout, stderr) {
      s.close()
      t.ifError(err, 'command completed')
      t.notOk(code, 'exited OK')

      const json = JSON.parse(stdout.trim())
      t.similar(json, {
        registry: common.registry,
        details: pingResponse
      }, 'JSON info returned')
      t.equal(typeof json.time, 'number', 'got a timestamp')
      t.end()
    })
  })
})

test('cleanup', function (t) {
  cleanup()
  t.end()
})

function setup () {
  mkdirp.sync(pkg)
  fs.writeFileSync(outfile, contents)
}

function cleanup () {
  rimraf.sync(pkg)
}

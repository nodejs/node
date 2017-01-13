'use strict'
var http = require('http')
var which = require('which')
var mr = require('npm-registry-mock')
var test = require('tap').test
var common = require('../common-tap.js')
var npm = require('../../lib/npm.js')
var server
var node_url
var pingResponse = {
  host: 'registry.npmjs.org',
  ok: true,
  username: null,
  peer: 'example.com'
}
var npmResponse = {
  'dist-tags': {latest: '0.0.0'},
  'versions': {
    '0.0.0': {
      version: '0.0.0',
      dist: {
        shasum: '',
        tarball: ''
      }
    }
  }
}

test('setup', function (t) {
  var port = common.port + 1
  http.createServer(function (q, s) {
    s.end(JSON.stringify([{lts: true, version: '0.0.0'}]))
    this.close()
  }).listen(port, function () {
    node_url = 'http://localhost:' + port
    mr({port: common.port}, function (e, s) {
      t.ifError(e, 'registry mocked successfully')
      server = s
      server.get('/-/ping?write=true').reply(200, JSON.stringify(pingResponse))
      server.get('/npm').reply(200, JSON.stringify(npmResponse))
      npm.load({registry: common.registry, loglevel: 'silent'}, function (e) {
        t.ifError(e, 'npm loaded successfully')
        t.pass('all set up')
        t.done()
      })
    })
  })
})

test('npm doctor', function (t) {
  npm.commands.doctor({'node-url': node_url}, true, function (e, list) {
    t.ifError(e, 'npm loaded successfully')
    t.same(list.length, 9, 'list should have 9 prop')
    t.same(list[0][1], 'ok', 'npm ping')
    t.same(list[1][1], 'v' + npm.version, 'npm -v')
    t.same(list[2][1], process.version, 'node -v')
    t.same(list[3][1], common.registry + '/', 'npm config get registry')
    t.same(list[5][1], 'ok', 'Perms check on cached files')
    t.same(list[6][1], 'ok', 'Perms check on global node_modules')
    t.same(list[7][1], 'ok', 'Perms check on local node_modules')
    t.same(list[8][1], 'ok', 'Checksum cached files')
    which('git', function (e, resolvedPath) {
      t.ifError(e, 'git command is installed')
      t.same(list[4][1], resolvedPath, 'which git')
      server.close()
      t.done()
    })
  })
})

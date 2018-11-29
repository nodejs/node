'use strict'
/* eslint-disable camelcase */
const common = require('../common-tap.js')
const http = require('http')
const mr = require('npm-registry-mock')
const npm = require('../../lib/npm.js')
const path = require('path')
const Tacks = require('tacks')
const t = require('tap')
const which = require('which')

const Dir = Tacks.Dir
const File = Tacks.File

const ROOT = common.pkg
const CACHE = common.cache
const TMP = path.join(ROOT, 'tmp')
const PREFIX = path.join(ROOT, 'global-prefix')
const PKG = path.join(ROOT, 'pkg')

let server
let node_url
const pingResponse = {
  host: 'registry.npmjs.org',
  ok: true,
  username: null,
  peer: 'example.com'
}
const npmResponse = {
  name: 'npm',
  'dist-tags': {latest: '0.0.0'},
  'versions': {
    '0.0.0': {
      name: 'npm',
      version: '0.0.0',
      _shrinkwrap: null,
      _hasShrinkwrap: false,
      dist: {
        shasum: 'deadbeef',
        tarball: 'https://reg.eh/npm-0.0.0.tgz'
      }
    }
  }
}

let nodeServer

t.teardown(() => {
  if (server) {
    server.close()
  }
  if (nodeServer) {
    nodeServer.close()
  }
})

t.test('setup', (t) => {
  const port = common.port + 1
  nodeServer = http.createServer(function (q, s) {
    s.end(JSON.stringify([{lts: true, version: '0.0.0'}]))
  })
  nodeServer.listen(port, () => {
    node_url = 'http://localhost:' + port
    mr({port: common.port}, (err, s) => {
      t.ifError(err, 'registry mocked successfully')
      server = s
      server.get('/-/ping?write=true').reply(200, JSON.stringify(pingResponse))
      server.get('/npm').reply(200, JSON.stringify(npmResponse))
      const fixture = new Tacks(Dir({
        [path.basename(PKG)]: Dir({
          'package.json': File({name: 'foo', version: '1.0.0'})
        }),
        [path.basename(PREFIX)]: Dir({})
      }))
      fixture.create(ROOT)
      npm.load({
        registry: common.registry,
        loglevel: 'silent',
        cache: CACHE,
        tmp: TMP,
        prefix: PREFIX
      }, (err) => {
        t.ifError(err, 'npm loaded successfully')
        t.pass('all set up')
        t.done()
      })
    })
  })
})

t.test('npm doctor', function (t) {
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
    t.match(list[8][1], /^verified \d+ tarballs?$/, 'Cache verified')
    which('git', function (e, resolvedPath) {
      t.ifError(e, 'git command is installed')
      t.same(list[4][1], resolvedPath, 'which git')
      t.done()
    })
  })
})

t.test('npm doctor works without registry', function (t) {
  npm.config.set('registry', false)
  npm.commands.doctor({'node-url': node_url}, true, function (e, list) {
    t.ifError(e, 'npm loaded successfully')
    t.same(list.length, 9, 'list should have 9 prop')
    t.same(list[0][1], 'ok', 'npm ping')
    t.same(list[1][1], 'v' + npm.version, 'npm -v')
    t.same(list[2][1], process.version, 'node -v')
    t.same(list[3][1], '', 'no registry, but no crash')
    t.same(list[5][1], 'ok', 'Perms check on cached files')
    t.same(list[6][1], 'ok', 'Perms check on global node_modules')
    t.same(list[7][1], 'ok', 'Perms check on local node_modules')
    t.match(list[8][1], /^verified \d+ tarballs?$/, 'Cache verified')
    which('git', function (e, resolvedPath) {
      t.ifError(e, 'git command is installed')
      t.same(list[4][1], resolvedPath, 'which git')
      server.close()
      t.done()
    })
  })
})

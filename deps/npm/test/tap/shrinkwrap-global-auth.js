'use strict'

var fs = require('fs')
var path = require('path')
var writeFileSync = require('graceful-fs').writeFileSync

var mkdirp = require('mkdirp')
var mr = require('npm-registry-mock')
var osenv = require('osenv')
var rimraf = require('rimraf')
var ssri = require('ssri')
var test = require('tap').test

var common = require('../common-tap.js')

var pkg = path.resolve(__dirname, path.basename(__filename, '.js'))
var outfile = path.resolve(pkg, '_npmrc')

var modules = path.resolve(pkg, 'node_modules')
var tarballPath = '/scoped-underscore/-/scoped-underscore-1.3.1.tgz'
var tarballURL = common.registry + tarballPath
var tarball = path.resolve(__dirname, '../fixtures/scoped-underscore-1.3.1.tgz')
var tarballIntegrity = ssri.fromData(fs.readFileSync(tarball)).toString()

var server

function mocks (server) {
  var auth = 'Bearer 0xabad1dea'
  server.get(tarballPath, { authorization: auth }).replyWithFile(200, tarball)
  server.get(tarballPath).reply(401, {
    error: 'unauthorized',
    reason: 'You are not authorized to access this db.'
  })
}

test('setup', function (t) {
  mr({ port: common.port, plugin: mocks }, function (er, s) {
    server = s
    t.ok(s, 'set up mock registry')
    setup()
    t.end()
  })
})

test('authed npm install with shrinkwrapped global package', function (t) {
  common.npm(
    [
      'install',
      '--loglevel', 'error',
      '--json',
      '--fetch-retries', 0,
      '--userconfig', outfile,
      '--registry', common.registry
    ],
    {cwd: pkg, stdio: [0, 'pipe', 2]},
    function (err, code, stdout) {
      if (err) throw err
      t.equal(code, 0, 'npm install exited OK')
      try {
        var results = JSON.parse(stdout)
        t.match(results, {added: [{name: '@scoped/underscore', version: '1.3.1'}]}, '@scoped/underscore installed')
      } catch (ex) {
        console.error('#', ex)
        t.ifError(ex, 'stdout was valid JSON')
      }

      t.end()
    }
  )
})

test('cleanup', function (t) {
  server.close()
  cleanup()
  t.end()
})

var contents = 'registry=' + common.registry + '\n' +
               '_authToken=0xabad1dea\n' +
               '\'always-auth\'=true\n'

var json = {
  name: 'test-package-install',
  version: '1.0.0',
  dependencies: {
    '@scoped/underscore': '1.0.0'
  }
}

var shrinkwrap = {
  name: 'test-package-install',
  version: '1.0.0',
  lockfileVersion: 1,
  dependencies: {
    '@scoped/underscore': {
      resolved: tarballURL,
      integrity: tarballIntegrity,
      version: '1.3.1'
    }
  }
}

function setup () {
  cleanup()
  mkdirp.sync(modules)
  writeFileSync(path.resolve(pkg, 'package.json'), JSON.stringify(json, null, 2) + '\n')
  writeFileSync(outfile, contents)
  writeFileSync(
    path.resolve(pkg, 'npm-shrinkwrap.json'),
    JSON.stringify(shrinkwrap, null, 2) + '\n'
  )
}

function cleanup () {
  process.chdir(osenv.tmpdir())
  rimraf.sync(pkg)
}

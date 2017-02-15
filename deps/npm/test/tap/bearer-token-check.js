var resolve = require('path').resolve
var writeFileSync = require('graceful-fs').writeFileSync

var mkdirp = require('mkdirp')
var mr = require('npm-registry-mock')
var osenv = require('osenv')
var rimraf = require('rimraf')
var test = require('tap').test

var common = require('../common-tap.js')
var toNerfDart = require('../../lib/config/nerf-dart.js')

var pkg = resolve(__dirname, 'install-bearer-check')
var outfile = resolve(pkg, '_npmrc')
var modules = resolve(pkg, 'node_modules')
var tarballPath = '/scoped-underscore/-/scoped-underscore-1.3.1.tgz'
// needs to be a different hostname to verify tokens (not) being sent correctly
var tarballURL = 'http://127.0.0.1:' + common.port + tarballPath
var tarball = resolve(__dirname, '../fixtures/scoped-underscore-1.3.1.tgz')

var server

var EXEC_OPTS = { cwd: pkg }

function mocks (server) {
  var auth = 'Bearer 0xabad1dea'
  server.get(tarballPath, { authorization: auth }).reply(403, {
    error: 'token leakage',
    reason: 'This token should not be sent.'
  })
  server.get(tarballPath).replyWithFile(200, tarball)
}

test('setup', function (t) {
  mr({ port: common.port, plugin: mocks }, function (er, s) {
    server = s
    t.ok(s, 'set up mock registry')
    setup()
    t.end()
  })
})

test('authed npm install with tarball not on registry', function (t) {
  common.npm(
    [
      'install',
      '--loglevel', 'silent',
      '--json',
      '--fetch-retries', 0,
      '--registry', common.registry,
      '--userconfig', outfile
    ],
    EXEC_OPTS,
    function (err, code, stdout, stderr) {
      t.ifError(err, 'test runner executed without error')
      t.equal(code, 0, 'npm install exited OK')
      t.comment(stdout.trim())
      t.comment(stderr.trim())
      t.notOk(stderr, 'no output on stderr')
      try {
        var results = JSON.parse(stdout)
      } catch (ex) {
        t.ifError(ex, 'stdout was valid JSON')
      }

      if (results) {
        var installedversion = {
          'version': '1.3.1',
          'from': '>=1.3.1 <2',
          'resolved': tarballURL
        }
        t.isDeeply(results.dependencies['@scoped/underscore'], installedversion, '@scoped/underscore installed')
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

var contents = '@scoped:registry=' + common.registry + '\n' +
               toNerfDart(common.registry) + ':_authToken=0xabad1dea\n'

var json = {
  name: 'test-package-install',
  version: '1.0.0'
}

var shrinkwrap = {
  name: 'test-package-install',
  version: '1.0.0',
  dependencies: {
    '@scoped/underscore': {
      resolved: tarballURL,
      from: '>=1.3.1 <2',
      version: '1.3.1'
    }
  }
}

function setup () {
  cleanup()
  mkdirp.sync(modules)
  writeFileSync(resolve(pkg, 'package.json'), JSON.stringify(json, null, 2) + '\n')
  writeFileSync(outfile, contents)
  writeFileSync(
    resolve(pkg, 'npm-shrinkwrap.json'),
    JSON.stringify(shrinkwrap, null, 2) + '\n'
  )
}

function cleanup () {
  process.chdir(osenv.tmpdir())
  rimraf.sync(pkg)
}

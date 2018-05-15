var fs = require('fs')
var path = require('path')
var mkdirp = require('mkdirp')
var rimraf = require('rimraf')
var mr = require('npm-registry-mock')

var test = require('tap').test
var common = require('../common-tap.js')

var opts = { cwd: __dirname }
var pkg = path.resolve(__dirname, 'adduser-saml')
var fakeBrowser = path.resolve(pkg, '_script.sh')
var configfile = path.resolve(pkg, '_npmrc')
var outfile = path.resolve(pkg, '_outfile')
var ssoUri = common.registry + '/-/saml/foo'

common.pendIfWindows('This is trickier to convert without opening new shells')

function mocks (server) {
  server.filteringRequestBody(function (r) {
    if (r.match(/"_id":"org\.couchdb\.user:npm_saml_auth_dummy_user"/)) {
      return 'auth'
    } else {
      return 'invalid'
    }
  })
  server.post('/-/v1/login', 'invalid').reply(404, 'not found')
  server.put('/-/user/org.couchdb.user:npm_saml_auth_dummy_user', 'auth')
    .reply(201, { token: 'foo', sso: ssoUri })
}

test('setup', function (t) {
  mkdirp.sync(pkg)
  fs.writeFileSync(configfile, '')
  var s = '#!/usr/bin/env bash\n' +
          'echo \"$@\" > ' + outfile + '\n'
  fs.writeFileSync(fakeBrowser, s, 'ascii')
  fs.chmodSync(fakeBrowser, '0755')
  t.pass('made script')
  t.end()
})

test('npm login', function (t) {
  mr({ port: common.port, plugin: mocks }, function (er, s) {
    s.get(
      '/-/whoami', { authorization: 'Bearer foo' }
    ).max(1).reply(401, {})
    var runner = common.npm(
      [
        'login',
        '--registry', common.registry,
        '--auth-type=saml',
        '--loglevel', 'silent',
        '--userconfig', configfile,
        '--browser', fakeBrowser
      ],
      opts,
      function (err, code, stdout, stderr) {
        t.ifError(err, 'npm ran without issue')
        t.equal(code, 0, 'exited OK')
        t.notOk(stderr, 'no error output')
        stderr && t.comment('stderr - ', stderr)
        t.matches(stdout, /Logged in as igotauthed/,
          'successfully authenticated and output the given username')
        s.close()
        rimraf.sync(configfile)
        rimraf.sync(outfile)
        t.end()
      }
    )

    var buf = ''
    runner.stdout.on('data', function (chunk) {
      buf += chunk.toString('utf8')
      if (buf.match(/complete authentication/)) {
        s.get(
          '/-/whoami', { authorization: 'Bearer foo' }
        ).reply(200, { username: 'igotauthed' })
      }
    })
  })
})

test('cleanup', function (t) {
  rimraf.sync(pkg)
  t.pass('cleaned up')
  t.end()
})

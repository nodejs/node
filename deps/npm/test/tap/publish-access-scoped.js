var fs = require('fs')
var path = require('path')

var test = require('tap').test
var mkdirp = require('mkdirp')
var rimraf = require('rimraf')
var mr = require('npm-registry-mock')
var common = require('../common-tap')
var server

var pkg = common.pkg

test('setup', function (t) {
  mr({port: common.port, throwOnUnmatched: true}, function (err, s) {
    t.ifError(err, 'registry mocked successfully')
    t.pass('setup done')
    server = s
    t.end()
  })
})

test('scoped packages pass public access if set', function (t) {
  server.filteringRequestBody(function (body) {
    t.doesNotThrow(function () {
      var parsed = JSON.parse(body)
      t.equal(parsed.access, 'public', 'access level is correct')
    }, 'converted body back into object')
    return true
  }).put('/@bigco%2fpublish-access', true).reply(201, {ok: true})

  mkdirp(path.join(pkg, 'cache'), function () {
    fs.writeFile(
      path.join(pkg, 'package.json'),
      JSON.stringify({
        name: '@bigco/publish-access',
        version: '1.2.5',
        public: true
      }),
      'ascii',
      function (er) {
        t.ifError(er, 'package file written')
        common.npm(
          [
            'publish',
            '--access', 'public',
            '--cache', path.join(pkg, 'cache'),
            '--loglevel', 'silly',
            '--registry', common.registry
          ],
          {
            cwd: pkg
          },
          function (er) {
            t.ifError(er, 'published without error')

            server.done()
            t.end()
          }
        )
      }
    )
  })
})

test('cleanup', function (t) {
  process.chdir(__dirname)
  server.close()
  rimraf(pkg, function (er) {
    t.ifError(er)

    t.end()
  })
})

// verify that prepublishOnly runs _only_ on pack and publish
var common = require('../common-tap')
var test = require('tap').test
var fs = require('graceful-fs')
var join = require('path').join
var mkdirp = require('mkdirp')
var mr = require('npm-registry-mock')
var rimraf = require('rimraf')

var pkg = join(__dirname, 'prepublish_package')
var tmp = join(pkg, 'tmp')
var cache = join(pkg, 'cache')

var server

test('setup', function (t) {
  var n = 0
  cleanup()
  mkdirp(pkg, then())
  mkdirp(cache, then())
  mkdirp(tmp, then())
  function then () {
    n++
    return function (er) {
      if (er) throw er
      if (--n === 0) next()
    }
  }

  function next () {
    fs.writeFile(join(pkg, 'package.json'), JSON.stringify({
      name: 'npm-test-prepublish-only',
      version: '1.2.5',
      scripts: { prepublishOnly: 'echo ok' }
    }), 'ascii', function (er) {
      if (er) throw er

      mr({port: common.port, throwOnUnmatched: true}, function (err, s) {
        t.ifError(err, 'registry mocked successfully')
        server = s
        t.end()
      })
    })
  }
})

test('test', function (t) {
  server.filteringRequestBody(function () { return true })
        .put('/npm-test-prepublish-only', true)
        .reply(201, {ok: true})

  var env = {
    'npm_config_cache': cache,
    'npm_config_tmp': tmp,
    'npm_config_prefix': pkg,
    'npm_config_global': 'false'
  }

  for (var i in process.env) {
    if (!/^npm_config_/.test(i)) {
      env[i] = process.env[i]
    }
  }

  var configuration = [
    'progress=false',
    'registry=' + common.registry,
    '//localhost:1337/:username=username',
    '//localhost:1337/:_authToken=deadbeeffeed'
  ]
  var configFile = join(pkg, '.npmrc')

  fs.writeFileSync(configFile, configuration.join('\n') + '\n')
  common.npm(
    [
      'publish',
      '--loglevel', 'warn'
    ],
    {
      cwd: pkg,
      env: env
    },
    function (err, code, stdout, stderr) {
      t.equal(code, 0, 'pack finished successfully')
      t.ifErr(err, 'pack finished successfully')

      t.notOk(stderr, 'got stderr data:' + JSON.stringify('' + stderr))
      var c = stdout.trim()
      var regex = new RegExp(
        '> npm-test-prepublish-only@1.2.5 prepublishOnly [^\\r\\n]+\\r?\\n' +
        '> echo ok\\r?\\n' +
        '\\r?\\n' +
        'ok\\r?\\n' +
        '\\+ npm-test-prepublish-only@1.2.5', 'ig'
      )

      t.match(c, regex)
      t.end()
    }
  )
})

test('cleanup', function (t) {
  cleanup()
  server.close()
  t.pass('cleaned up')
  t.end()
})

function cleanup () {
  rimraf.sync(pkg)
}

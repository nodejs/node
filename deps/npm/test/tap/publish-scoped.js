var fs = require('fs')
var path = require('path')

var test = require('tap').test
var mkdirp = require('mkdirp')
var rimraf = require('rimraf')
var common = require('../common-tap')
var mr = require('npm-registry-mock')

var pkg = path.join(__dirname, 'prepublish_package')

var server

function setup () {
  cleanup()
  mkdirp.sync(path.join(pkg, 'cache'))

  fs.writeFileSync(
    path.join(pkg, 'package.json'),
    JSON.stringify({
      name: '@bigco/publish-organized',
      version: '1.2.5'
    }, null, 2),
    'ascii')
}

test('setup', function (t) {
  setup()
  mr({port: common.port, throwOnUnmatched: true}, function (err, s) {
    t.ifError(err, 'registry mocked successfully')
    server = s
    t.end()
  })
})

test('npm publish should honor scoping', function (t) {
  server.filteringRequestBody(verify)
        .put('/@bigco%2fpublish-organized', true)
        .reply(201, {ok: true})

  var configuration = [
    'progress=false',
    'cache=' + path.join(pkg, 'cache'),
    'registry=http://nonexistent.lvh.me',
    '//localhost:1337/:username=username',
    '//localhost:1337/:_password=' + new Buffer('password').toString('base64'),
    '//localhost:1337/:email=' + 'ogd@aoaioxxysz.net',
    '@bigco:registry=' + common.registry
  ]
  var configFile = path.join(pkg, '.npmrc')

  fs.writeFileSync(configFile, configuration.join('\n') + '\n')

  common.npm(['publish'], {'cwd': pkg}, function (err, code, stdout, stderr) {
    if (err) throw err
    t.is(code, 0, 'published without error')
    server.done()
    t.end()
  })

  function verify (body) {
    t.doesNotThrow(function () {
      var parsed = JSON.parse(body)
      var current = parsed.versions['1.2.5']
      t.equal(
        current._npmVersion,
        require(path.resolve(__dirname, '../../package.json')).version,
        'npm version is correct'
      )

      t.equal(
        current._nodeVersion,
        process.versions.node,
        'node version is correct'
      )
    }, 'converted body back into object')

    return true
  }
})

test('cleanup', function (t) {
  server.close()
  t.end()
  cleanup()
})

function cleanup () {
  process.chdir(__dirname)
  rimraf.sync(pkg)
}

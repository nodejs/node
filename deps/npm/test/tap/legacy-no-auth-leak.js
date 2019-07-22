'use strict'
var test = require('tap').test
var common = require('../common-tap.js')
var basepath = common.pkg
var Tacks = require('tacks')
var File = Tacks.File
var Dir = Tacks.Dir

var fixture = new Tacks(
  Dir({
    README: File(
      'just an npm test\n'
    ),
    'package.json': File({
      name: 'npm-test-no-auth-leak',
      version: '0.0.0',
      scripts: {
        test: 'node test.js'
      }
    }),
    '.npmrc': File(
      'auth=abc',
      'authCrypt=def',
      'password=xyz',
      '//registry.npmjs.org/:_authToken=nopenope'
    ),
    'test.js': File(
      'var authTokenKeys = Object.keys(process.env)\n' +
      '  .filter(function (key) { return /authToken/.test(key) })\n' +
      'console.log(JSON.stringify({\n' +
      '  password: process.env.npm_config__password || null,\n' +
      '  auth: process.env.npm_config__auth || null,\n' +
      '  authCrypt: process.env.npm_config__authCrypt || null ,\n' +
      '  authToken: authTokenKeys && process.env[authTokenKeys[0]] || null\n' +
      '}))'
    )
  })
)

test('setup', function (t) {
  setup()
  t.done()
})

test('no-auth-leak', function (t) {
  common.npm(['test'], {cwd: basepath}, function (err, code, stdout, stderr) {
    if (err) throw err
    t.is(code, 0, 'test ran ok')
    if (stderr) console.log(stderr)
    var matchResult = /^[^{]*(\{(?:.|\n)*\})[^}]*$/
    t.like(stdout, matchResult, 'got results with a JSON chunk in them')
    var stripped = stdout.replace(matchResult, '$1')
    var result = JSON.parse(stripped)
    t.is(result.password, null, 'password')
    t.is(result.auth, null, 'auth')
    t.is(result.authCrypt, null, 'authCrypt')
    t.is(result.authToken, null, 'authToken')
    t.end()
  })
})

test('cleanup', function (t) {
  cleanup()
  t.done()
})

function setup () {
  cleanup()
  fixture.create(basepath)
}

function cleanup () {
  fixture.remove(basepath)
}

var common = require('./common.js')
var test = require('tap').test
var reg = 'http://127.0.0.1:15986/'
var path = require('path')
var conf = path.resolve(__dirname, 'fixtures', 'npmrc')
var pkg = path.resolve(__dirname, 'fixtures/package')
var http = require('http')
var env = { PATH: process.env.PATH }

test('deprecate', function (t) {
  var c = common.npm([
    '--color=always',
    '--registry=' + reg,
    '--userconf=' + conf,
    'deprecate', 'package@0.0.2', 'message goes here'
  ], { env: env })

  c.stderr.pipe(process.stderr)

  var v = ''
  c.stdout.setEncoding('utf8')
  c.stdout.on('data', function(d) {
    v += d
  })
  c.on('close', function(code) {
    t.notOk(code)
    t.equal(v, '')
    t.end()
  })
})

test('get data after deprecation', function (t) {
  http.get('http://127.0.0.1:15986/package', function(res) {
    var c = ''
    res.setEncoding('utf8')
    res.on('data', function (d) {
      c += d
    })
    res.on('end', function () {
      c = JSON.parse(c)
      t.equal(c.versions['0.0.2'].deprecated, 'message goes here')
      t.end()
    })
  })
})

test('un-deprecate', function (t) {
  var c = common.npm([
    '--color=always',
    '--registry=' + reg,
    '--userconf=' + conf,
    'deprecate', 'package@0.0.2', ''
  ], { env: env })

  c.stderr.pipe(process.stderr)

  var v = ''
  c.stdout.setEncoding('utf8')
  c.stdout.on('data', function(d) {
    v += d
  })
  c.on('close', function(code) {
    t.notOk(code)
    t.equal(v, '')
    t.end()
  })
})

test('get data after un-deprecation', function (t) {
  http.get('http://127.0.0.1:15986/package', function(res) {
    var c = ''
    res.setEncoding('utf8')
    res.on('data', function (d) {
      c += d
    })
    res.on('end', function () {
      c = JSON.parse(c)
      t.notOk(c.versions['0.0.2'].deprecated)
      t.end()
    })
  })
})

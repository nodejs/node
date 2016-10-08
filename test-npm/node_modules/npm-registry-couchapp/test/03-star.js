var common = require('./common.js')
var test = require('tap').test
var reg = 'http://127.0.0.1:15986/'
var path = require('path')
var rimraf = require('rimraf')
var conf = path.resolve(__dirname, 'fixtures', 'npmrc')
var conf2 = path.resolve(__dirname, 'fixtures', 'npmrc2')
var conf3 = path.resolve(__dirname, 'fixtures', 'npmrc3')
var spawn = require('child_process').spawn
var pkg = path.resolve(__dirname, 'fixtures/package')
var http = require('http')
var env = { PATH: process.env.PATH }
var url = require('url')

test('non-owner can star package', function(t) {
  var c = common.npm([
    '--registry=' + reg,
    '--userconf=' + conf3,
    'star', 'package'
  ], { env: env })
  c.stderr.pipe(process.stderr)
  var out = ''
  c.stdout.setEncoding('utf8')
  c.stdout.on('data', function(d) {
    out += d
  })
  c.on('close', function(code) {
    t.notOk(code)
    t.equal(out, "★  package\n")
    t.end()
  })
})

test('non-owner can unstar package', function(t) {
  var c = common.npm([
    '--registry=' + reg,
    '--userconf=' + conf3,
    'unstar', 'package'
  ], { env: env })
  c.stderr.pipe(process.stderr)
  var out = ''
  c.stdout.setEncoding('utf8')
  c.stdout.on('data', function(d) {
    out += d
  })
  c.on('close', function(code) {
    t.notOk(code)
    t.equal(out, "☆  package\n")
    t.end()
  })
})

test('simulate old-style multi-readme doc', function(t) {
  http.get('http://localhost:15986/registry/package', function(res) {
    var j = ''
    res.setEncoding('utf8')
    res.on('data', function(d) { j += d })
    res.on('end', function() {
      var p = JSON.parse(j)
      p.versions['0.0.2'].readme = 'foo'
      p.versions['0.2.3-alpha'].readme = 'bar'
      var b = new Buffer(JSON.stringify(p))
      var u = url.parse('http://admin:admin@localhost:15986/registry/package')
      u.method = 'PUT'
      u.headers = {
        'content-type': 'application/json',
        'content-length': b.length,
        'connection': 'close'
      }
      http.request(u, function(res) {
        t.equal(res.statusCode, 201)
        t.end()
        res.resume()
      }).end(b)
    })
  })
})

test('non-owner can star package', function(t) {
  var c = common.npm([
    '--registry=' + reg,
    '--userconf=' + conf3,
    'star', 'package'
  ], { env: env })
  c.stderr.pipe(process.stderr)
  var out = ''
  c.stdout.setEncoding('utf8')
  c.stdout.on('data', function(d) {
    out += d
  })
  c.on('close', function(code) {
    t.notOk(code)
    t.equal(out, "★  package\n")
    t.end()
  })
})

test('non-owner can unstar package', function(t) {
  var c = common.npm([
    '--registry=' + reg,
    '--userconf=' + conf3,
    'unstar', 'package'
  ], { env: env })
  c.stderr.pipe(process.stderr)
  var out = ''
  c.stdout.setEncoding('utf8')
  c.stdout.on('data', function(d) {
    out += d
  })
  c.on('close', function(code) {
    t.notOk(code)
    t.equal(out, "☆  package\n")
    t.end()
  })
})


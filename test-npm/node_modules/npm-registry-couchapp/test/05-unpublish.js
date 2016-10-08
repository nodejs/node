// npm --userconfig=test/fixtures/npmrc unpublish -f package
// --registry=http://127.0.0.1:15986; ( echo "now unpublish"; cd
// test/fixtures/package/0.2.4/; npm publish
// --registry=http://127.0.0.1:15986/
// --userconfig=$HOME/dev/npm/npmjs.org/test/fixtures/npmrc3)

var common = require('./common.js')
var test = require('tap').test
var reg = 'http://127.0.0.1:15986/'
var path = require('path')
var conf = path.resolve(__dirname, 'fixtures', 'npmrc')
var conf3 = path.resolve(__dirname, 'fixtures', 'npmrc3')
var pkg = path.resolve(__dirname, 'fixtures/package')
var pkg002 = path.resolve(pkg, '0.0.2')
var pkg024 = path.resolve(pkg, '0.2.4')
var http = require('http')
var env = { PATH: process.env.PATH, npm_config_loglevel: "error" }

test('unpublish', function(t) {
  var c = common.npm([
    '--registry=' + reg,
    '--userconf=' + conf,
    'unpublish', '-f', 'package'
  ], { env: env })
  var v = ''
  c.stdout.setEncoding('utf8')
  c.stdout.on('data', function(d) {
    v += d
  })
  c.on('close', function(code) {
    t.notOk(code)
    t.equal(v, '- package\n')
    t.end()
  })
})

test('GET after unpublish', function(t) {
  var expect = {
    "_id": "package",
    "name": "package",
    "time": {
      "modified": "xx",
      "created": "xx",
      "0.0.2": "xx",
      "0.2.3-alpha": "xx",
      "0.2.3": "xx",
      "unpublished": {
        "name": "user",
        "time": "xx"
      }
    }
  }

  http.get(reg + 'package', function(res) {
    t.equal(res.statusCode, 404)
    var c = ''
    res.setEncoding('utf8')
    res.on('data', function(d) {
      c += d
    })
    res.on('end', function() {
      c = JSON.parse(c)
      // rev and time will be different
      t.like(c._rev, /[0-9]+-[0-9a-f]+$/)
      expect._rev = c._rev
      t.equal(c.name, c._id)
      t.same(Object.keys(c).sort().join(","),
             "_attachments,_id,_rev,name,time")
      t.same(Object.keys(c.time).sort().join(","),
             '0.0.2,0.2.3,0.2.3-alpha,created,modified,unpublished')
      t.equal(c.time.unpublished.name, "user")
      t.end()
    })
  })
})

test('fail to clobber', function(t) {
  var c = common.npm([
    '--registry=' + reg,
    '--userconf=' + conf3,
    '--force',
    'publish'
  ], { cwd: pkg002, env: env })
  c.on('close', function(code) {
    t.ok(code)
    t.end()
  })
})

test('publish new version as new user', function(t) {
  var c = common.npm([
    '--registry=' + reg,
    '--userconf=' + conf3,
    '--force',
    'publish'
  ], { cwd: pkg024, env: env })
  var v = ''
  c.stderr.pipe(process.stderr)
  c.stdout.setEncoding('utf8')
  c.stdout.on('data', function(d) {
    v += d
  })
  c.on('close', function(code) {
    t.notOk(code)
    t.equal('+ package@0.2.4\n', v)
    t.end()
  })
})

test('now unpublish the new version', function(t) {
  var c = common.npm([
    '--registry=' + reg,
    '--userconf=' + conf3,
    'unpublish', 'package@0.2.4'
  ], { env: env })
  var v = ''
  c.stdout.setEncoding('utf8')
  c.stdout.on('data', function(d) {
    v += d
  })
  c.on('close', function(code) {
    t.notOk(code)
    t.equal('- package@0.2.4\n', v)
    t.end()
  })
})

test('GET after unpublish', function(t) {
  var expect = {
    "_id": "package",
    "name": "package",
    "time": {
      "modified": "xx",
      "created": "xx",
      "0.0.2": "xx",
      "0.2.3-alpha": "xx",
      "0.2.3": "xx",
      "0.2.4": "xx",
      "unpublished": {
        "name": "third",
        "time": "xx"
      }
    }
  }

  http.get(reg + 'package', function(res) {
    t.equal(res.statusCode, 404)
    var c = ''
    res.setEncoding('utf8')
    res.on('data', function(d) {
      c += d
    })
    res.on('end', function() {
      c = JSON.parse(c)
      // rev and time will be different
      t.like(c._rev, /[0-9]+-[0-9a-f]+$/)
      expect._rev = c._rev
      t.equal(c.name, c._id)
      t.same(Object.keys(c).sort().join(","),
             "_attachments,_id,_rev,name,time")
      t.same(Object.keys(c.time).sort().join(","),
             '0.0.2,0.2.3,0.2.3-alpha,0.2.4,created,modified,unpublished')
      t.equal(c.time.unpublished.name, "third")
      t.equal(c.time.unpublished.description, "just an npm test, but with a **markdown** readme.")
      t.same(c.time.unpublished.maintainers, [{"name":"third","email":"3@example.com"}])
      t.same(c.time.unpublished.versions, ["0.2.4"])
      t.same(c.time.unpublished.tags, { latest: "0.2.4" })
      t.end()
    })
  })
})


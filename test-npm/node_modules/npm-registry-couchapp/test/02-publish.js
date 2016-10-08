var common = require('./common.js')
var test = require('tap').test
var reg = 'http://127.0.0.1:15986/'
var request = require('request')
var db = 'http://localhost:15986/registry/'
var fs = require('fs')
var path = require('path')
var rimraf = require('rimraf')
var conf = path.resolve(__dirname, 'fixtures', 'npmrc')
var conf2 = path.resolve(__dirname, 'fixtures', 'npmrc2')
var pkg = path.resolve(__dirname, 'fixtures/package')
var pkg002 = path.resolve(pkg, '0.0.2')
var pkg023a = path.resolve(pkg, '0.2.3alpha')
var pkg023 = path.resolve(pkg, '0.2.3')
var inst = path.resolve(__dirname, 'fixtures/install')
var http = require('http')
var maintainers = [
  {
    "name": "user",
    "email": "email@example.com"
  }
]
var url = require("url")

var expect = null

var version002 = {
  "name": "package",
  "version": "0.0.2",
  "description": "just an npm test",
  "_id": "package@0.0.2",
  "dist": {
    "tarball": "http://127.0.0.1:15986/package/-/package-0.0.2.tgz"
  },
  "_from": ".",
  "_npmUser": {
    "name": "user",
    "email": "email@example.com"
  },
  "maintainers": [
    {
      "name": "user",
      "email": "email@example.com"
    }
  ],
  "directories": {}
}

var version023a = {
  "name": "package",
  "version": "0.2.3-alpha",
  "description": "just an npm test, but with a **markdown** readme.",
  "_id": "package@0.2.3-alpha",
  "dist": {
    "tarball": "http://127.0.0.1:15986/package/-/package-0.2.3-alpha.tgz"
  },
  "_from": ".",
  "_npmUser": {
    "name": "user",
    "email": "email@example.com"
  },
  "maintainers": [
    {
      "name": "user",
      "email": "email@example.com"
    }
  ],
  "directories": {}
}

var version023 = {
  "name": "package",
  "version": "0.2.3",
  "description": "just an npm test, but with a **markdown** readme.",
  "_id": "package@0.2.3",
  "dist": {
    "tarball": "http://127.0.0.1:15986/package/-/package-0.2.3.tgz"
  },
  "_from": ".",
  "_npmUser": {
    "name": "other",
    "email": "other@example.com"
  },
  "maintainers": [
    {
      "name": "user",
      "email": "email@example.com"
    },
    {
      "name": "other",
      "email": "other@example.com"
    }
  ],
  "directories": {}
}

var time = {}
var npmVersion = null
var env = { PATH: process.env.PATH, npm_config_loglevel: "error" }

test('get npm version', function(t) {
  var c = common.npm([ '--version' ], { env: env })
  var v = ''
  c.stdout.on('data', function(d) {
    v += d
  })
  c.on('close', function(code) {
    npmVersion = v.trim()
    version002._npmVersion = npmVersion
    version023a._npmVersion = npmVersion
    version023._npmVersion = npmVersion
    t.notOk(code)
    t.end()
  })
})


test('first publish', function(t) {
  var c = common.npm([
    '--registry=' + reg,
    '--userconf=' + conf,
    'publish'
  ], { cwd: pkg002, env: env })
  c.stderr.pipe(process.stderr)
  var out = ''
  c.stdout.setEncoding('utf8')
  c.stdout.on('data', function(d) {
    out += d
  })
  c.on('close', function(code) {
    t.notOk(code)
    t.equal(out, "+ package@0.0.2\n")
    t.end()
  })
})

var urls = [
  'package/-/package-0.0.2.tgz',
  'npm/public/registry/package/_attachments/package-0.0.2.tgz',
  'npm/public/registry/p/package/_attachments/package-0.0.2.tgz'
]

urls.forEach(function(u) {
  test('attachment: ' + u, function(t) {
    r = url.parse(reg + u)
    r.method = 'HEAD'
    r.headers = {connection: 'close'}
    http.request(r, function(res) {
      t.equal(res.statusCode, 200)
      t.end()
    }).end()
  })
})

test('GET after publish', function(t) {
  expect = {
    "_id": "package",
    "name": "package",
    "description": "just an npm test",
    "dist-tags": {
      "latest": "0.0.2"
    },
    "versions": {
      "0.0.2": version002
    },
    "readme": "just an npm test\n",
    "maintainers": maintainers,
    "time": time,
    "readmeFilename": "README",
    "_attachments": {
      "package-0.0.2.tgz": {
        "content_type": "application/octet-stream",
        "revpos": 1,
        "stub": true
      }
    }
  }

  http.get(reg + 'package', function(res) {
    t.equal(res.statusCode, 200)
    var c = ''
    res.setEncoding('utf8')
    res.on('data', function(d) {
      c += d
    })
    res.on('end', function() {
      c = JSON.parse(c)
      // rev and time will be different
      t.like(c._rev, /1-[0-9a-f]+$/)
      expect._rev = c._rev
      expect.time['0.0.2'] = c.time['0.0.2']
      t.has(c, expect)
      t.end()
    })
  })
})

test('GET with x-forwarded-for', function (t) {
  var wanted = 'http://fooblz/registry/_design/scratch/_rewrite/package/-/package-0.0.2.tgz'

  var g = url.parse('http://localhost:15986/registry/_design/scratch/_rewrite/package')
  g.headers = {
    'x-forwarded-host': 'fooblz'
  }

  http.get(g, function(res) {
    t.equal(res.statusCode, 200)
    var c = ''
    res.setEncoding('utf8')
    res.on('data', function(d) {
      c += d
    })
    res.on('end', function() {
      c = JSON.parse(c)
      var actual = c.versions[c['dist-tags'].latest].dist.tarball
      t.equal(actual, wanted)
      t.end()
    })
  })
})

test('fail to clobber', function(t) {
  var c = common.npm([
    '--registry=' + reg,
    '--userconf=' + conf,
    'publish'
  ], { cwd: pkg002, env: env })
  c.on('close', function(code) {
    t.ok(code)
    t.end()
  })
})

test('fail to publish as other user', function(t) {
  var c = common.npm([
    '--registry=' + reg,
    '--userconf=' + conf2,
    'publish'
  ], { cwd: pkg023a, env: env })
  c.on('close', function(code) {
    t.ok(code)
    t.end()
  })
})

test('publish update as non-latest', function(t) {
  var c = common.npm([
    '--registry=' + reg,
    '--userconf=' + conf,
    '--tag=alpha',
    'publish'
  ], { cwd: pkg023a, env: env })
  c.stderr.pipe(process.stderr)
  var out = ''
  c.stdout.setEncoding('utf8')
  c.stdout.on('data', function(d) {
    out += d
  })
  c.on('close', function(code) {
    t.notOk(code)
    t.equal(out, "+ package@0.2.3-alpha\n")
    t.end()
  })
})

test('GET after update', function(t) {
  expect = {
    "_id": "package",
    "name": "package",
    "description": "just an npm test",
    "dist-tags": {
      "latest": "0.0.2",
      "alpha": "0.2.3-alpha"
    },
    "versions": {
      "0.0.2": version002,
      "0.2.3-alpha": version023a
    },
    "readme": "just an npm test\n",
    "maintainers": maintainers,
    "time": time,
    "readmeFilename": "README",
    "_attachments": {
      "package-0.0.2.tgz": {
        "content_type": "application/octet-stream",
        "revpos": 1,
        "stub": true
      },
      "package-0.2.3-alpha.tgz": {
        "content_type": "application/octet-stream",
        "revpos": 2,
        "stub": true
      }
    }
  }

  http.get(reg + 'package', function(res) {
    t.equal(res.statusCode, 200)
    var c = ''
    res.setEncoding('utf8')
    res.on('data', function(d) {
      c += d
    })
    res.on('end', function() {
      c = JSON.parse(c)
      // rev and time will be different
      t.like(c._rev, /2-[0-9a-f]+$/)
      expect._rev = c._rev
      time['0.2.3-alpha'] = c.time['0.2.3-alpha']
      t.has(c, expect)
      t.end()
    })
  })
})

test('add second publisher', function(t) {
  var c = common.npm([
    '--registry=' + reg,
    '--userconf=' + conf,
    'owner',
    'add',
    'other',
    'package'
  ], { env: env })
  c.stderr.pipe(process.stderr)
  c.on('close', function(code) {
    t.notOk(code)
    t.end()
  })
})

test('get after owner add', function(t) {
  http.get(reg + 'package', function(res) {
    t.equal(res.statusCode, 200)
    var c = ''
    res.setEncoding('utf8')
    res.on('data', function(d) {
      c += d
    })
    res.on('end', function() {
      c = JSON.parse(c)
      // rev and time will be different
      t.like(c._rev, /3-[0-9a-f]+$/)
      expect._rev = c._rev
      expect.maintainers.push({
        name: 'other',
        email: 'other@example.com'
      })
      expect.time = time
      t.has(c, expect)
      t.end()
    })
  })
})

test('other owner publish', function(t) {
  var c = common.npm([
    '--registry=' + reg,
    '--userconf=' + conf2,
    'publish'
  ], { cwd: pkg023, env: env })
  c.stderr.pipe(process.stderr)
  var out = ''
  c.stdout.setEncoding('utf8')
  c.stdout.on('data', function(d) {
    out += d
  })
  c.on('close', function(code) {
    t.notOk(code)
    t.equal(out, "+ package@0.2.3\n")
    t.end()
  })
})

test('get after other publish (and x-forwarded-proto)', function(t) {
  var r = url.parse(reg + 'package')
  r.headers = { 'x-forwarded-proto': 'foobar' }

  http.get(r, function(res) {
    t.equal(res.statusCode, 200)
    var c = ''
    res.setEncoding('utf8')
    res.on('data', function(d) {
      c += d
    })
    res.on('end', function() {
      c = JSON.parse(c)
      // rev and time will be different
      t.like(c._rev, /4-[0-9a-f]+$/)
      expect._rev = c._rev
      expect.description = "just an npm test, but with a **markdown** readme."
      expect.readmeFilename = "README.md"
      expect.readme = "just an npm test, but with a **markdown** readme.\n"
      expect['dist-tags'] = {
        "latest": "0.2.3",
        "alpha": "0.2.3-alpha"
      }
      expect.versions['0.2.3'] = version023
      expect._attachments["package-0.2.3.tgz"] = {
        "content_type" : "application/octet-stream",
        "revpos" : 4,
        "stub" : true
      }
      expect.time['0.2.3'] = c.time['0.2.3']
      // should get x-forwarded-proto back
      Object.keys(expect.versions).forEach(function(v) {
        var tgz = expect.versions[v].dist.tarball
        expect.versions[v].dist.tarball = tgz.replace(/^https?/, 'foobar')
      })
      t.has(c, expect)
      t.end()
    })
  })
})

test('install the thing we published', function(t) {
  rimraf.sync(path.resolve(inst, 'node_modules'))
  var c = common.npm([
    '--registry=' + reg,
    'install'
  ], { env: env, cwd: inst })
  c.stderr.pipe(process.stderr)
  var out = ''
  c.stdout.setEncoding('utf8')
  c.stdout.on('data', function(d) {
    out += d
  })
  c.on('close', function(code) {
    t.notOk(code)
    t.similar(out, /(└── )?package@0.2.3/)
    rimraf.sync(path.resolve(inst, 'node_modules'))
    t.end()
  })
})

test('remove all the tarballs', function(t) {
  http.get(db + 'package', function(res) {
    t.equal(res.statusCode, 200)
    var c = ''
    res.setEncoding('utf8')
    res.on('data', function (d) {
      c += d
    })
    res.on('end', function() {
      var doc = JSON.parse(c)
      doc._attachments = {}
      var body = new Buffer(JSON.stringify(doc), 'utf8')
      var p = url.parse(db + 'package')
      p.auth = 'admin:admin'
      p.headers = {
        'content-type': 'application/json',
        'content-length': body.length,
        connection: 'close'
      }
      p.method = 'PUT'
      http.request(p, function(res) {
        if (res.statusCode !== 201)
          res.pipe(process.stderr)
        else
          res.resume()
        t.equal(res.statusCode, 201)
        res.on('end', t.end.bind(t))
      }).end(body)
    })
  })
})

test('try to attach a new tarball (and fail)', function(t) {
  http.get(db + 'package', function(res) {
    t.equal(res.statusCode, 200)
    var c = ''
    res.setEncoding('utf8')
    res.on('data', function (d) {
      c += d
    })
    res.on('end', function() {
      var doc = JSON.parse(c)
      var rev = doc._rev
      var p = url.parse(db + 'package/package-0.2.3.tgz?rev=' + rev)
      body = new Buffer("this is the attachment data")
      p.auth = 'other:pass@:!%\''
      p.headers = {
        'content-type': 'application/octet-stream',
        'content-length': body.length,
        connection: 'close'
      }
      p.method = 'PUT'
      http.request(p, function(res) {
        if (res.statusCode !== 403)
          res.pipe(process.stderr)
        else
          res.resume()
        res.on('end', t.end.bind(t))
        t.equal(res.statusCode, 403)
      }).end(body)
    })
  })
})

test('admin user can publish scoped package', function(t) {
  var packageJson = JSON.parse(fs.readFileSync(path.resolve(__dirname, 'fixtures', 'scoped-package.json'), 'utf-8'))
  var url = 'http://admin:admin@localhost:15986/registry/_design/scratch/_update/package/' + packageJson.name

  request.put({
    url: url,
    body: packageJson,
    json: true
  }, function(err, resp, body) {
    t.equal(body.ok, "created new entry")
    t.end()
  })
})

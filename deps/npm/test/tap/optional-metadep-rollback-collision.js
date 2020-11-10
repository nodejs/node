'use strict'
/* eslint-disable camelcase */
var fs = require('graceful-fs')
var path = require('path')

var mkdirp = require('mkdirp')
var test = require('tap').test

var common = require('../common-tap.js')

var pkg = common.pkg
var deps = path.resolve(pkg, 'deps')
var opdep = path.resolve(pkg, 'node_modules', 'opdep')
var cache = common.cache
var createServer = require('http').createServer
var mr = require('npm-registry-mock')
var serverPort = 27991

var json = {
  name: 'optional-metadep-rollback-collision',
  version: '1.0.0',
  description: 'let\'s just see about that race condition',
  optionalDependencies: {
    opdep: 'file:./deps/opdep'
  }
}

var d1 = {
  name: 'd1',
  version: '1.0.0',
  description: 'I FAIL CONSTANTLY',
  scripts: {
    preinstall: 'sleep 1'
  },
  dependencies: {
    foo: 'http://localhost:' + serverPort + '/'
  }
}

var d2 = {
  name: 'd2',
  version: '1.0.0',
  description: 'how do you *really* know you exist?',
  scripts: {
    postinstall: 'node blart.js'
  },
  dependencies: {
    'request': '^0.9.0',
    mkdirp: '^0.3.5',
    wordwrap: '^0.0.2'
  }
}

var opdep_json = {
  name: 'opdep',
  version: '1.0.0',
  description: 'To explode, of course!',
  main: 'index.js',
  dependencies: {
    d1: 'file:../d1',
    d2: 'file:../d2'
  }
}

var blart = `
var rando = require('crypto').randomBytes
var resolve = require('path').resolve

var mkdirp = require('mkdirp')
var rimraf = require('rimraf')
var writeFile = require('graceful-fs').writeFile

var BASEDIR = resolve(__dirname, 'arena')

var keepItGoingLouder = {}
var writers = 0
var errors = 0

function gensym () { return rando(16).toString('hex') }

function writeAlmostForever (filename) {
  if (!keepItGoingLouder[filename]) {
    writers--
    if (writers < 1) return done()
  } else {
    writeFile(filename, keepItGoingLouder[filename], function (err) {
      if (err) errors++

      writeAlmostForever(filename)
    })
  }
}

function done () {
  rimraf(BASEDIR, function () {
    if (errors > 0) console.log('not ok - %d errors', errors)
    else console.log('ok')
  })
}

mkdirp(BASEDIR, function go () {
  for (var i = 0; i < 16; i++) {
    var filename = resolve(BASEDIR, gensym() + '.txt')

    keepItGoingLouder[filename] = ''
    for (var j = 0; j < 512; j++) keepItGoingLouder[filename] += filename

    writers++
    writeAlmostForever(filename)
  }

  setTimeout(function viktor () {
    // kill all the writers
    keepItGoingLouder = {}
  }, 3 * 1000)
})
`

test('setup', function (t) {
  const badServer = createServer(function (req, res) {
    setTimeout(function () {
      res.writeHead(404)
      res.end()
    }, 1000)
  }).listen(serverPort, () => t.parent.teardown(() => badServer.close()))

  mkdirp.sync(pkg)
  fs.writeFileSync(
    path.join(pkg, 'package.json'),
    JSON.stringify(json, null, 2)
  )

  mkdirp.sync(path.join(deps, 'd1'))
  fs.writeFileSync(
    path.join(deps, 'd1', 'package.json'),
    JSON.stringify(d1, null, 2)
  )

  mkdirp.sync(path.join(deps, 'd2'))
  fs.writeFileSync(
    path.join(deps, 'd2', 'package.json'),
    JSON.stringify(d2, null, 2)
  )
  fs.writeFileSync(path.join(deps, 'd2', 'blart.js'), blart)

  mkdirp.sync(path.join(deps, 'opdep'))
  fs.writeFileSync(
    path.join(deps, 'opdep', 'package.json'),
    JSON.stringify(opdep_json, null, 2)
  )
  mr({ port: common.port }, function (er, server) {
    t.parent.teardown(() => server.close())
    t.end()
  })
})

test('go go test racer', t => common.npm(
  [
    '--prefix', pkg,
    '--fetch-retries', '0',
    '--loglevel', 'error',
    '--no-progress',
    '--registry', common.registry,
    '--parseable',
    '--cache', cache,
    'install'
  ],
  {
    cwd: pkg,
    env: {
      PATH: process.env.PATH,
      Path: process.env.Path
    },
    stdio: 'pipe'
  }
).spread((code, stdout, stderr) => {
  t.comment(stdout.trim())
  t.comment(stderr.trim())
  t.is(code, 0, 'npm install exited with code 0')
  t.notOk(/not ok/.test(stdout), 'should not contain the string \'not ok\'')
}))

test('verify results', function (t) {
  t.throws(function () {
    fs.statSync(opdep)
  })
  t.end()
})

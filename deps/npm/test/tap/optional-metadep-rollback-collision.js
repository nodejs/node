var fs = require('graceful-fs')
var path = require('path')

var mkdirp = require('mkdirp')
var osenv = require('osenv')
var rimraf = require('rimraf')
var test = require('tap').test

var common = require('../common-tap.js')

var pkg = path.resolve(__dirname, 'optional-metadep-rollback-collision')
var deps = path.resolve(pkg, 'deps')
var opdep = path.resolve(pkg, 'node_modules', 'opdep')
var cache = path.resolve(pkg, 'cache')
var pidfile = path.resolve(pkg, 'child.pid')

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
    foo: 'http://localhost:8080/'
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
    'graceful-fs': '^3.0.2',
    mkdirp: '^0.5.0',
    rimraf: '^2.2.8'
  }
}

var opdep_json = {
  name: 'opdep',
  version: '1.0.0',
  description: 'To explode, of course!',
  main: 'index.js',
  scripts: {
    preinstall: 'node bad-server.js'
  },
  dependencies: {
    d1: 'file:../d1',
    d2: 'file:../d2'
  }
}

var badServer = function () { /*
var createServer = require('http').createServer
var spawn = require('child_process').spawn
var fs = require('fs')
var path = require('path')
var pidfile = path.resolve(__dirname, '..', '..', 'child.pid')

if (process.argv[2]) {
  console.log('ok')
  createServer(function (req, res) {
    setTimeout(function () {
      res.writeHead(404)
      res.end()
    }, 1000)
    this.close()
  }).listen(8080)
} else {
  var child = spawn(
    process.execPath,
    [__filename, 'whatever'],
    {
      stdio: [0, 1, 2],
      detached: true
    }
  )
  child.unref()

  // kill any prior children, if existing.
  try {
    var pid = +fs.readFileSync(pidfile)
    process.kill(pid, 'SIGKILL')
  } catch (er) {}

  fs.writeFileSync(pidfile, child.pid + '\n')
}
*/ }.toString().split('\n').slice(1, -1).join('\n')

var blart = function () { /*
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
*/ }.toString().split('\n').slice(1, -1).join('\n')
test('setup', function (t) {
  cleanup()

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
  fs.writeFileSync(path.join(deps, 'opdep', 'bad-server.js'), badServer)

  t.end()
})

test('go go test racer', function (t) {
  common.npm(
    [
      '--prefix', pkg,
      '--fetch-retries', '0',
      '--loglevel', 'silent',
      '--parseable',
      '--cache', cache,
      'install'
    ],
    {
      cwd: pkg,
      env: {
        PATH: process.env.PATH,
        Path: process.env.Path
      }
    },
    function (er, code, stdout, stderr) {
      t.ifError(er, 'install ran to completion without error')
      t.is(code, 0, 'npm install exited with code 0')
      t.comment(stdout.trim())
      // stdout should be empty, because we only have one, optional, dep and
      // if it fails we shouldn't try installing anything
      t.equal(stdout, '')
      t.notOk(/not ok/.test(stdout), 'should not contain the string \'not ok\'')
      t.end()
    }
  )
})

test('verify results', function (t) {
  t.throws(function () {
    fs.statSync(opdep)
  })
  t.end()
})

test('cleanup', function (t) {
  cleanup()
  t.end()
})

function cleanup () {
  process.chdir(osenv.tmpdir())
  try {
    var pid = +fs.readFileSync(pidfile)
    process.kill(pid, 'SIGKILL')
  } catch (er) {}

  rimraf.sync(pkg)
}

var test = require('tap').test
var path = require('path')
var fs = require('fs')
var rimraf = require('rimraf')
var common = require('../common-tap.js')
var Tacks = require('tacks')
var unixFormatPath = require('../../lib/utils/unix-format-path.js')
var File = Tacks.File
var Dir = Tacks.Dir

var testdir = path.resolve(__dirname, path.basename(__filename, '.js'))
var cachedir = path.resolve(testdir, 'cache')
var config = ['--cache=' + cachedir, '--loglevel=error']

var shrinkwrap = {
  name: 'shrinkwrap-local-dependency',
  version: '1.0.0',
  dependencies: {
    mod2: {
      version: 'file:' + unixFormatPath(path.join('mods', 'mod2')),
      dependencies: {
        mod1: {
          version: 'file:' + unixFormatPath(path.join('mods', 'mod1'))
        }
      }
    }
  }
}

var fixture = new Tacks(
  Dir({
    cache: Dir(),
    mods: Dir({
      mod1: Dir({
        'package.json': File({
          name: 'mod1',
          version: '1.0.0'
        })
      }),
      mod2: Dir({
        'package.json': File({
          name: 'mod2',
          version: '1.0.0',
          dependencies: {
            mod1: 'file:' + path.join('..', 'mod1')
          }
        })
      })
    }),
    'package.json': File({
      name: 'shrinkwrap-local-dependency',
      version: '1.0.0',
      dependencies: {
        mod2: 'file:' + path.join('mods', 'mod2')
      }
    })
  })
)

function setup () {
  cleanup()
  fixture.create(testdir)
}

function cleanNodeModules () {
  rimraf.sync(path.resolve(testdir, 'node_modules'))
}

function cleanup () {
  fixture.remove(testdir)
}

test('shrinkwrap uses resolved with file: on local deps', function (t) {
  setup()

  common.npm(config.concat(['install', '--legacy']), {cwd: testdir}, function (err, code, stdout, stderr) {
    if (err) throw err
    t.comment(stdout.trim())
    t.comment(stderr.trim())
    t.equal(code, 0, 'npm exited normally')

    common.npm(config.concat('shrinkwrap'), {cwd: testdir}, function (err, code, stdout, stderr) {
      if (err) throw err
      t.comment(stdout.trim())
      t.comment(stderr.trim())
      t.equal(code, 0, 'npm exited normally')
      var data = fs.readFileSync(path.join(testdir, 'npm-shrinkwrap.json'), { encoding: 'utf8' })
      t.like(
        JSON.parse(data).dependencies,
        shrinkwrap.dependencies,
        'shrinkwrap looks correct'
      )
      t.end()
    })
  })
})

function exists (file) {
  try {
    fs.statSync(file)
    return true
  } catch (ex) {
    return false
  }
}

test("'npm install' should install local packages from shrinkwrap", function (t) {
  cleanNodeModules()

  common.npm(config.concat(['install']), {cwd: testdir}, function (err, code, stdout, stderr) {
    if (err) throw err
    t.comment(stdout.trim())
    t.comment(stderr.trim())
    t.equal(code, 0, 'npm exited normally')
    t.ok(exists(path.join(testdir, 'node_modules', 'mod2')), 'mod2 exists')
    t.ok(exists(path.join(testdir, 'node_modules', 'mod2', 'node_modules', 'mod1')), 'mod1 exists')
    t.end()
  })
})

test('cleanup', function (t) {
  cleanup()
  t.end()
})

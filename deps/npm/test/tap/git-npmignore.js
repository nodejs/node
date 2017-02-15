var child_process = require('child_process')
var readdir = require('graceful-fs').readdirSync
var path = require('path')
var resolve = require('path').resolve

var rimraf = require('rimraf')
var test = require('tap').test
var which = require('which')

var common = require('../common-tap.js')
var escapeArg = require('../../lib/utils/escape-arg.js')
var Tacks = require('tacks')
var Dir = Tacks.Dir
var File = Tacks.File

var fixture = new Tacks(Dir({
  deps: Dir({
    gitch: Dir({
      '.npmignore': File(
        't.js\n'
      ),
      '.gitignore': File(
        'node_modules/\n'
      ),
      'a.js': File(
        "console.log('hi');"
      ),
      't.js': File(
        "require('tap').test(function (t) { t.pass('I am a test!'); t.end(); });"
      ),
      'package.json': File({
        name: 'gitch',
        version: '1.0.0',
        private: true,
        main: 'a.js'
      })
    })
  }),
  'node_modules': Dir({
  })
}))

var testdir = resolve(__dirname, path.basename(__filename, '.js'))
var dep = resolve(testdir, 'deps', 'gitch')
var packname = 'gitch-1.0.0.tgz'
var packed = resolve(testdir, packname)
var modules = resolve(testdir, 'node_modules')
var installed = resolve(modules, 'gitch')
var expected = [
  'a.js',
  'package.json',
  '.npmignore'
].sort()

var NPM_OPTS = { cwd: testdir }

function exec (todo, opts, cb) {
  console.log('    # EXEC:', todo)
  child_process.exec(todo, opts, cb)
}

test('setup', function (t) {
  setup(function (er) {
    t.ifError(er, 'setup ran OK')

    t.end()
  })
})

test('npm pack directly from directory', function (t) {
  packInstallTest(dep, t)
})

test('npm pack via git', function (t) {
  var urlPath = dep
    .replace(/\\/g, '/') // fixup slashes for Windows
    .replace(/^\/+/, '') // remove any leading slashes
  packInstallTest('git+file:///' + urlPath, t)
})

test('cleanup', function (t) {
  cleanup()

  t.end()
})

function packInstallTest (spec, t) {
  console.log('    # pack', spec)
  common.npm(
    [
      '--loglevel', 'error',
      'pack', spec
    ],
    NPM_OPTS,
    function (err, code, stdout, stderr) {
      if (err) throw err
      t.is(code, 0, 'npm pack exited cleanly')
      t.is(stderr, '', 'npm pack ran silently')
      t.is(stdout.trim(), packname, 'got expected package name')

      common.npm(
        [
          '--loglevel', 'error',
          'install', packed
        ],
        NPM_OPTS,
        function (err, code, stdout, stderr) {
          if (err) throw err
          t.is(code, 0, 'npm install exited cleanly')
          t.is(stderr, '', 'npm install ran silently')

          var actual = readdir(installed).sort()
          t.isDeeply(actual, expected, 'no unexpected files in packed directory')

          rimraf(packed, function () {
            t.end()
          })
        }
      )
    }
  )
}

function cleanup () {
  fixture.remove(testdir)
  rimraf.sync(testdir)
}

function setup (cb) {
  cleanup()

  fixture.create(testdir)

  common.npm(
    [
      '--loglevel', 'error',
      'cache', 'clean'
    ],
    NPM_OPTS,
    function (er, code, _, stderr) {
      if (er) return cb(er)
      if (code) return cb(new Error('npm cache nonzero exit: ' + code))
      if (stderr) return cb(new Error('npm cache clean error: ' + stderr))

      which('git', function found (er, gitPath) {
        if (er) return cb(er)

        var git = escapeArg(gitPath)

        exec(git + ' init', {cwd: dep}, init)

        function init (er, _, stderr) {
          if (er) return cb(er)
          if (stderr) return cb(new Error('git init error: ' + stderr))

          exec(git + " config user.name 'Phantom Faker'", {cwd: dep}, user)
        }

        function user (er, _, stderr) {
          if (er) return cb(er)
          if (stderr) return cb(new Error('git config error: ' + stderr))

          exec(git + ' config user.email nope@not.real', {cwd: dep}, email)
        }

        function email (er, _, stderr) {
          if (er) return cb(er)
          if (stderr) return cb(new Error('git config error: ' + stderr))

          exec(git + ' config core.autocrlf input', {cwd: dep}, autocrlf)
        }

        function autocrlf (er, _, stderr) {
          if (er) return cb(er)
          if (stderr) return cb(new Error('git config error: ' + stderr))

          exec(git + ' add .', {cwd: dep}, addAll)
        }

        function addAll (er, _, stderr) {
          if (er) return cb(er)
          if (stderr) return cb(new Error('git add . error: ' + stderr))

          exec(git + ' commit -m boot', {cwd: dep}, commit)
        }

        function commit (er, _, stderr) {
          if (er) return cb(er)
          if (stderr) return cb(new Error('git commit error: ' + stderr))
          cb()
        }
      })
    }
  )
}

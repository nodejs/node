var cat = require('graceful-fs').writeFileSync
var resolve = require('path').resolve

var mkdirp = require('mkdirp')
var Bluebird = require('bluebird')
var mr = Bluebird.promisify(require('npm-registry-mock'))
var rimraf = require('rimraf')
var test = require('tap').test
var tmpdir = require('osenv').tmpdir

var common = require('../common-tap.js')

var pkg = resolve(__dirname, 'ls-l-depth-0')
var dep = resolve(pkg, 'deps', 'glock')
var modules = resolve(pkg, 'node_modules')

var expected =
  '\n' +
  '│ ' + pkg + '\n' +
  '│ \n' +
  '└── glock@1.8.7\n' +
  '    an inexplicably hostile sample package\n' +
  '    git+https://github.com/npm/glo.ck.git\n' +
  '    https://glo.ck\n' +
  '    file:glock-1.8.7.tgz\n' +
  '\n'

var server

var EXEC_OPTS = { cwd: pkg }

var fixture = {
  'name': 'glock',
  'version': '1.8.7',
  'private': true,
  'description': 'an inexplicably hostile sample package',
  'homepage': 'https://glo.ck',
  'repository': 'https://github.com/npm/glo.ck',
  'dependencies': {
    'underscore': '1.5.1'
  }
}

var deppack

test('setup', function (t) {
  setup()
  return mr({ port: common.port }).then((s) => {
    server = s
    return common.npm(['pack', dep], EXEC_OPTS)
  }).spread((code, stdout) => {
    t.is(code, 0, 'pack')
    deppack = stdout.trim()
  })
})

test('#6311: npm ll --depth=0 duplicates listing', function (t) {
  common.npm(
    [
      '--loglevel', 'silent',
      '--registry', common.registry,
      '--parseable',
      'install', deppack
    ],
    EXEC_OPTS,
    function (err, code, stdout, stderr) {
      if (err) throw err
      t.notOk(code, 'npm install exited cleanly')
      t.is(stderr, '', 'npm install ran silently')
      t.equal(
        stdout.trim(),
        'add\tunderscore\t1.5.1\tnode_modules/underscore\t\t\n' +
        'add\tglock\t1.8.7\tnode_modules/glock',
        'got expected install output'
      )

      common.npm(
        [
          '--loglevel', 'silent',
          'ls', '--long',
          '--unicode=true',
          '--depth', '0'
        ],
        EXEC_OPTS,
        function (err, code, stdout, stderr) {
          if (err) throw err
          t.is(code, 0, 'npm ll exited cleanly')
          t.is(stderr, '', 'npm ll ran silently')
          t.equal(
            stdout,
            expected,
            'got expected package name'
          )

          t.end()
        }
      )
    }
  )
})

test('cleanup', function (t) {
  cleanup()
  server.close()

  t.end()
})

function cleanup () {
  process.chdir(tmpdir())
  rimraf.sync(pkg)
}

function setup () {
  cleanup()

  mkdirp.sync(modules)
  mkdirp.sync(dep)

  cat(resolve(dep, 'package.json'), JSON.stringify(fixture))
}

'use strict'
var path = require('path')
var test = require('tap').test
var Tacks = require('tacks')
var File = Tacks.File
var Symlink = Tacks.Symlink
var Dir = Tacks.Dir
var common = require('../common-tap.js')
var mr = require('npm-registry-mock')
var extend = Object.assign || require('util')._extend

var testdir = path.join(__dirname, path.basename(__filename, '.js'))
var bugdir = path.join(testdir, 'modules', 'bug')

// This is an absolutely minimal version of the optimist included with
// npm-registry-mock.
var optimist = Dir({
  'package.json': File({
    dependencies: {
      minimist: '~0.0.1',
      wordwrap: '~0.0.2'
    },
    name: 'optimist',
    version: '0.6.0'
  }),
  node_modules: Dir({
    minimist: Dir({
      'package.json': File({
        _shasum: 'd7aa327bcecf518f9106ac6b8f003fa3bcea8566',
        _resolve: 'foo',
        name: 'minimist',
        version: '0.0.5'
      })
    }),
    wordwrap: Dir({
      'package.json': File({
        _shasum: 'b79669bb42ecb409f83d583cad52ca17eaa1643f',
        _resolve: 'foo',
        name: 'wordwrap',
        version: '0.0.2'
      })
    })
  })
})

var fixture = new Tacks(
  Dir({
    cache: Dir({}),
    global: Dir({
      lib: Dir({
        node_modules: Dir({
          linked1: Symlink('../../../modules/linked1/'),
          linked2: Symlink('../../../modules/linked2/')
        })
      })
    }),
    modules: Dir({
      bug: Dir({
        node_modules: Dir({
          linked1: Symlink('../../../global/lib/node_modules/linked1'),
          linked2: Symlink('../../../global/lib/node_modules/linked2')
        }),
        'package.json': File({
          name: 'bug',
          version: '10800.0.0',
          devDependencies: {
            optimist: '0.6.0',
            linked1: '^1.0.0',
            linked2: '^1.0.0'
          }
        })
      }),
      linked1: Dir({
        'package.json': File({
          name: 'linked1',
          version: '1.0.0',
          devDependencies: {
            optimist: '0.6.0'
          }
        }),
        node_modules: Dir({
          optimist: optimist
        })
      }),
      linked2: Dir({
        'package.json': File({
          name: 'linked2',
          version: '1.0.0',
          devDependencies: {
            optimist: '0.6.0',
            linked1: '^1.0.0'
          }
        }),
        node_modules: Dir({
          linked1: Symlink('../../../global/lib/node_modules/linked1'),
          optimist: optimist
        })
      })
    })
  })
)

function setup () {
  cleanup()
  fixture.create(testdir)
}

function cleanup () {
  fixture.remove(testdir)
}

var server
test('setup', function (t) {
  setup()
  mr({port: common.port}, function (er, s) {
    t.ifError(er)
    server = s
    t.end()
  })
})

test('shared-linked', function (t) {
  var options = {
    cwd: bugdir,
    env: extend(extend({}, process.env), {
      npm_config_prefix: path.join(testdir, 'global')
    })
  }
  var config = [
    '--cache', path.join(testdir, 'cache'),
    '--registry', common.registry,
    '--unicode', 'false'
  ]

  common.npm(config.concat(['install', '--dry-run', '--parseable']), options, function (err, code, stdout, stderr) {
    if (err) throw err
    t.is(code, 0)
    var got = stdout.trim().replace(/\s+\n/g, '\n')
    var expected =
      'add\tminimist\t0.0.5\tnode_modules/minimist\n' +
      'add\twordwrap\t0.0.2\tnode_modules/wordwrap\n' +
      'add\toptimist\t0.6.0\tnode_modules/optimist'
    t.is(got, expected, 'just an optimist install please')
    server.done()
    t.end()
  })
})

test('cleanup', function (t) {
  if (server) server.close()
  cleanup()
  t.end()
})

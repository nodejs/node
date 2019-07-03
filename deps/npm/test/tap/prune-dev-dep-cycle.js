'use strict'
var fs = require('fs')
var test = require('tap').test
var Tacks = require('tacks')
var File = Tacks.File
var Dir = Tacks.Dir
var common = require('../common-tap.js')
var testdir = common.pkg

var fixture = new Tacks(
  Dir({
    node_modules: Dir({
      'a': Dir({
        'package.json': File({
          _requested: {
            rawSpec: 'file:///mods/a'
          },
          dependencies: {
            'b': 'file:///mods/b'
          },
          name: 'a',
          version: '1.0.0'
        })
      }),
      'b': Dir({
        'package.json': File({
          _requested: {
            rawSpec: 'file:///mods/b'
          },
          dependencies: {
            'a': 'file:///mods/a'
          },
          name: 'b',
          version: '1.0.0'
        })
      })
    }),
    'package.json': File({
      name: 'test',
      version: '1.0.0',
      devDependencies: {
        'a': 'file:///mods/a'
      }
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

test('setup', function (t) {
  setup()
  t.end()
})

function readdir (dir) {
  try {
    return fs.readdirSync(dir)
  } catch (ex) {
    if (ex.code === 'ENOENT') return []
    throw ex
  }
}

test('prune cycle in dev deps', function (t) {
  common.npm(['prune', '--production', '--json'], {cwd: testdir}, function (err, code, stdout, stderr) {
    if (err) throw err
    t.is(code, 0, 'prune finished successfully')
    t.like(JSON.parse(stdout), {removed: [{name: 'a'}, {name: 'b'}]}, 'removed the right modules')
    var dirs = readdir(testdir + '/node_modules').sort()
    t.same(dirs, [])
    t.end()
  })
})

test('cleanup', function (t) {
  cleanup()
  t.end()
})

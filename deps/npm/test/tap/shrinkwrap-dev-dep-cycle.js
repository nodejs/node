'use strict'
var fs = require('fs')
var path = require('path')
var test = require('tap').test
var Tacks = require('tacks')
var File = Tacks.File
var Dir = Tacks.Dir
var common = require('../common-tap.js')
var testdir = path.join(__dirname, path.basename(__filename, '.js'))

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

var expectedShrinkwrap = {
  name: 'test',
  version: '1.0.0',
  dependencies: {}
}

function setup () {
  cleanup()
  fixture.create(testdir)
}

function cleanup () {
  fixture.remove(testdir)
}

function readJson (file) {
  try {
    var contents = fs.readFileSync(file)
    return JSON.parse(contents)
  } catch (ex) {
    return ex
  }
}

test('setup', function (t) {
  setup()
  t.end()
})

test('shrinkwrap cycle in dev deps', function (t) {
  common.npm(['shrinkwrap', '--only=prod'], {cwd: testdir}, function (err, code, stdout, stderr) {
    if (err) throw err
    t.is(code, 0, 'result code = ok')
    t.comment(stdout.trim())
    t.comment(stderr.trim())
    var actualShrinkwrap = readJson(path.join(testdir, 'npm-shrinkwrap.json'))
    t.isDeeply(actualShrinkwrap, expectedShrinkwrap, 'shrinkwrap is right')
    t.end()
  })
})

test('cleanup', function (t) {
  cleanup()
  t.end()
})

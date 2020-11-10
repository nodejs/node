'use strict'
var test = require('tap').test
var Tacks = require('tacks')
var File = Tacks.File
var Dir = Tacks.Dir
var common = require('../common-tap.js')

var testdir = common.pkg

var fixture = new Tacks(Dir({
  'package.json': File(
    '{\n' +
      "'name': 'some-name',\n" +
      "'dependencies': {}\n" +
    '}'
  ),
  'node_modules': Dir()
}))

function setup () {
  fixture.create(testdir)
}

function cleanup () {
  fixture.remove(testdir)
}

test('setup', function (t) {
  cleanup()
  setup()
  t.end()
})

test('failing to parse package.json should be error', function (t) {
  common.npm(
    ['install'],
    {cwd: testdir},
    function (err, code, stdout, stderr) {
      if (err) throw err
      t.equal(code, 1, 'exit not ok')
      t.similar(stderr, /npm ERR! JSON.parse Failed to parse json/)
      t.end()
    }
  )
})

test('cleanup', function (t) {
  cleanup()
  t.end()
})

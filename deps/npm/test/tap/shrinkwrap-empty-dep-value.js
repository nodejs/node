'use strict'

const common = require('../common-tap.js')
const path = require('path')
const test = require('tap').test

const Tacks = require('tacks')
const File = Tacks.File
const Dir = Tacks.Dir

const basedir = common.pkg
const testdir = path.join(basedir, 'testdir')

const fixture = new Tacks(Dir({
  cache: Dir(),
  global: Dir(),
  tmp: Dir(),
  testdir: Dir({
    'npm-shrinkwrap.json': File({
      name: 'http-locks',
      version: '0.0.0',
      dependencies: {
        minimist: {}
      }
    }),
    'package.json': File({
      name: 'http-locks',
      version: '1.0.0',
      dependencies: {
        minimist: common.registry + '/minimist/-/minimist-0.0.5.tgz'
      }
    })
  })
}))

function setup () {
  cleanup()
  fixture.create(basedir)
}

function cleanup () {
  fixture.remove(basedir)
}

test('setup', function (t) {
  setup()
  t.done()
})

test('raises error to regenerate the shrinkwrap', function (t) {
  common.npm(['install'], {cwd: testdir}, function (err, code, stdout, stderr) {
    if (err) throw err
    t.match(
      stderr,
      'npm ERR! If using a shrinkwrap, regenerate with "npm shrinkwrap".',
      'returns message to regenerate shrinkwrap'
    )

    t.done()
  })
})

test('cleanup', function (t) {
  cleanup()
  t.done()
})

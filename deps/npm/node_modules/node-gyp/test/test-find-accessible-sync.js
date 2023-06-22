'use strict'

const { describe, it } = require('mocha')
const assert = require('assert')
const path = require('path')
const requireInject = require('require-inject')
const configure = requireInject('../lib/configure', {
  'graceful-fs': {
    closeSync: function () { return undefined },
    openSync: function (path) {
      if (readableFiles.some(function (f) { return f === path })) {
        return 0
      } else {
        var error = new Error('ENOENT - not found')
        throw error
      }
    }
  }
})

const dir = path.sep + 'testdir'
const readableFile = 'readable_file'
const anotherReadableFile = 'another_readable_file'
const readableFileInDir = 'somedir' + path.sep + readableFile
const readableFiles = [
  path.resolve(dir, readableFile),
  path.resolve(dir, anotherReadableFile),
  path.resolve(dir, readableFileInDir)
]

describe('find-accessible-sync', function () {
  it('find accessible - empty array', function () {
    var candidates = []
    var found = configure.test.findAccessibleSync('test', dir, candidates)
    assert.strictEqual(found, undefined)
  })

  it('find accessible - single item array, readable', function () {
    var candidates = [readableFile]
    var found = configure.test.findAccessibleSync('test', dir, candidates)
    assert.strictEqual(found, path.resolve(dir, readableFile))
  })

  it('find accessible - single item array, readable in subdir', function () {
    var candidates = [readableFileInDir]
    var found = configure.test.findAccessibleSync('test', dir, candidates)
    assert.strictEqual(found, path.resolve(dir, readableFileInDir))
  })

  it('find accessible - single item array, unreadable', function () {
    var candidates = ['unreadable_file']
    var found = configure.test.findAccessibleSync('test', dir, candidates)
    assert.strictEqual(found, undefined)
  })

  it('find accessible - multi item array, no matches', function () {
    var candidates = ['non_existent_file', 'unreadable_file']
    var found = configure.test.findAccessibleSync('test', dir, candidates)
    assert.strictEqual(found, undefined)
  })

  it('find accessible - multi item array, single match', function () {
    var candidates = ['non_existent_file', readableFile]
    var found = configure.test.findAccessibleSync('test', dir, candidates)
    assert.strictEqual(found, path.resolve(dir, readableFile))
  })

  it('find accessible - multi item array, return first match', function () {
    var candidates = ['non_existent_file', anotherReadableFile, readableFile]
    var found = configure.test.findAccessibleSync('test', dir, candidates)
    assert.strictEqual(found, path.resolve(dir, anotherReadableFile))
  })
})

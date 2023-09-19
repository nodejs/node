'use strict'

const { describe, it } = require('mocha')
const assert = require('assert')
const path = require('path')
const findNodeDirectory = require('../lib/find-node-directory')

const platforms = ['darwin', 'freebsd', 'linux', 'sunos', 'win32', 'aix', 'os400']

describe('find-node-directory', function () {
  // we should find the directory based on the directory
  // the script is running in and it should match the layout
  // in a build tree where npm is installed in
  // .... /deps/npm
  it('test find-node-directory - node install', function () {
    for (var next = 0; next < platforms.length; next++) {
      var processObj = { execPath: '/x/y/bin/node', platform: platforms[next] }
      assert.strictEqual(
        findNodeDirectory('/x/deps/npm/node_modules/node-gyp/lib', processObj),
        path.join('/x'))
    }
  })

  // we should find the directory based on the directory
  // the script is running in and it should match the layout
  // in an installed tree where npm is installed in
  // .... /lib/node_modules/npm or .../node_modules/npm
  // depending on the patform
  it('test find-node-directory - node build', function () {
    for (var next = 0; next < platforms.length; next++) {
      var processObj = { execPath: '/x/y/bin/node', platform: platforms[next] }
      if (platforms[next] === 'win32') {
        assert.strictEqual(
          findNodeDirectory('/y/node_modules/npm/node_modules/node-gyp/lib',
            processObj), path.join('/y'))
      } else {
        assert.strictEqual(
          findNodeDirectory('/y/lib/node_modules/npm/node_modules/node-gyp/lib',
            processObj), path.join('/y'))
      }
    }
  })

  // we should find the directory based on the execPath
  // for node and match because it was in the bin directory
  it('test find-node-directory - node in bin directory', function () {
    for (var next = 0; next < platforms.length; next++) {
      var processObj = { execPath: '/x/y/bin/node', platform: platforms[next] }
      assert.strictEqual(
        findNodeDirectory('/nothere/npm/node_modules/node-gyp/lib', processObj),
        path.join('/x/y'))
    }
  })

  // we should find the directory based on the execPath
  // for node and match because it was in the Release directory
  it('test find-node-directory - node in build release dir', function () {
    for (var next = 0; next < platforms.length; next++) {
      var processObj
      if (platforms[next] === 'win32') {
        processObj = { execPath: '/x/y/Release/node', platform: platforms[next] }
      } else {
        processObj = {
          execPath: '/x/y/out/Release/node',
          platform: platforms[next]
        }
      }

      assert.strictEqual(
        findNodeDirectory('/nothere/npm/node_modules/node-gyp/lib', processObj),
        path.join('/x/y'))
    }
  })

  // we should find the directory based on the execPath
  // for node and match because it was in the Debug directory
  it('test find-node-directory - node in Debug release dir', function () {
    for (var next = 0; next < platforms.length; next++) {
      var processObj
      if (platforms[next] === 'win32') {
        processObj = { execPath: '/a/b/Debug/node', platform: platforms[next] }
      } else {
        processObj = { execPath: '/a/b/out/Debug/node', platform: platforms[next] }
      }

      assert.strictEqual(
        findNodeDirectory('/nothere/npm/node_modules/node-gyp/lib', processObj),
        path.join('/a/b'))
    }
  })

  // we should not find it as it will not match based on the execPath nor
  // the directory from which the script is running
  it('test find-node-directory - not found', function () {
    for (var next = 0; next < platforms.length; next++) {
      var processObj = { execPath: '/x/y/z/y', platform: next }
      assert.strictEqual(findNodeDirectory('/a/b/c/d', processObj), '')
    }
  })

  // we should find the directory based on the directory
  // the script is running in and it should match the layout
  // in a build tree where npm is installed in
  // .... /deps/npm
  // same test as above but make sure additional directory entries
  // don't cause an issue
  it('test find-node-directory - node install', function () {
    for (var next = 0; next < platforms.length; next++) {
      var processObj = { execPath: '/x/y/bin/node', platform: platforms[next] }
      assert.strictEqual(
        findNodeDirectory('/x/y/z/a/b/c/deps/npm/node_modules/node-gyp/lib',
          processObj), path.join('/x/y/z/a/b/c'))
    }
  })
})

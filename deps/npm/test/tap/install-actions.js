'use strict'
var npm = require('../../lib/npm.js')
var log = require('npmlog')
var test = require('tap').test

var mockLog = {
  finish: function () {},
  silly: function () {}
}

var actions
test('setup', function (t) {
  npm.load(function () {
    log.disableProgress()
    actions = require('../../lib/install/actions.js').actions
    t.end()
  })
})

test('->optdep:a->dep:b', function (t) {
  var moduleA = {
    name: 'a',
    path: '/a',
    package: {
      scripts: {
        postinstall: 'false'
      },
      dependencies: {
        b: '*'
      }
    },
    isTop: true
  }
  var moduleB = {
    name: 'b',
    path: '/b',
    package: {},
    requires: [],
    requiredBy: [moduleA]
  }
  moduleA.requires = [moduleB]

  var tree = {
    path: '/',
    package: {
      optionalDependencies: {
        a: '*'
      }
    },
    children: [moduleA, moduleB],
    requires: [moduleA],
    isTop: true
  }
  moduleA.requiredBy = [tree]
  moduleA.parent = tree
  moduleB.parent = tree

  t.plan(3)
  return actions.postinstall('/', moduleA, mockLog).then(() => {
    throw new Error('was not supposed to succeed')
  }, (err) => {
    t.is(err && err.code, 'ELIFECYCLE', 'Lifecycle failed')
    t.ok(moduleA.failed, 'moduleA (optional dep) is marked failed')
    t.ok(moduleB.failed, 'moduleB (direct dep of moduleA) is marked as failed')
    t.end()
  })
})

test('->dep:b,->optdep:a->dep:b', function (t) {
  var moduleA = {
    name: 'a',
    path: '/',
    package: {
      scripts: {
        postinstall: 'false'
      },
      dependencies: {
        b: '*'
      }
    },
    isTop: false
  }
  var moduleB = {
    name: 'b',
    path: '/',
    package: {},
    requires: [],
    requiredBy: [moduleA],
    isTop: false
  }
  moduleA.requires = [moduleB]

  var tree = {
    path: '/',
    package: {
      dependencies: {
        b: '*'
      },
      optionalDependencies: {
        a: '*'
      }
    },
    children: [moduleA, moduleB],
    requires: [moduleA, moduleB],
    isTop: true
  }
  moduleA.requiredBy = [tree]
  moduleB.requiredBy.push(tree)
  moduleA.parent = tree
  moduleB.parent = tree

  t.plan(3)
  return actions.postinstall('/', moduleA, mockLog).then(() => {
    throw new Error('was not supposed to succeed')
  }, (err) => {
    t.ok(err && err.code === 'ELIFECYCLE', 'Lifecycle failed')
    t.ok(moduleA.failed, 'moduleA (optional dep) is marked failed')
    t.ok(!moduleB.failed, 'moduleB (direct dep of moduleA) is marked as failed')
    t.end()
  })
})

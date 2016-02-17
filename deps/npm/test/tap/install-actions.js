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
    path: '/',
    package: {
      scripts: {
        postinstall: 'false'
      },
      dependencies: {
        b: '*'
      }
    }
  }
  var moduleB = {
    name: 'b',
    path: '/',
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
    requires: [moduleA]
  }
  moduleA.requiredBy = [tree]

  t.plan(3)
  actions.postinstall('/', '/', moduleA, mockLog, function (er) {
    t.ok(er && er.code === 'ELIFECYCLE', 'Lifecycle failed')
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
    }
  }
  var moduleB = {
    name: 'b',
    path: '/',
    package: {},
    requires: [],
    requiredBy: [moduleA]
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
    requires: [moduleA, moduleB]
  }
  moduleA.requiredBy = [tree]
  moduleB.requiredBy.push(tree)

  t.plan(3)
  actions.postinstall('/', '/', moduleA, mockLog, function (er) {
    t.ok(er && er.code === 'ELIFECYCLE', 'Lifecycle failed')
    t.ok(moduleA.failed, 'moduleA (optional dep) is marked failed')
    t.ok(!moduleB.failed, 'moduleB (direct dep of moduleA) is marked as failed')
    t.end()
  })
})

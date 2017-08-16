'use strict'
var test = require('tap').test
var requireInject = require('require-inject')
var npa = require('npm-package-arg')

// we're just mocking to avoid having to call `npm.load`
var deps = requireInject('../../lib/install/deps.js', {
  '../../lib/npm.js': {
    config: {
      get: function (val) { return (val === 'global-style' || val === 'legacy-bundling') ? false : 'mock' }
    },
    limit: {
      fetch: 10
    }
  }
})

var earliestInstallable = deps.earliestInstallable

test('earliestInstallable should consider devDependencies', function (t) {
  var dep1 = {
    children: [],
    package: {
      name: 'dep1',
      dependencies: { dep2: '2.0.0' }
    },
    path: '/dep1',
    realpath: '/dep1'
  }

  // a library required by the base package
  var dep2 = {
    package: {
      name: 'dep2',
      version: '1.0.0'
    },
    path: '/dep2',
    realpath: '/dep2'
  }

  // an incompatible verson of dep2. required by dep1
  var dep2a = {
    package: {
      name: 'dep2',
      version: '2.0.0',
      _requested: npa('dep2@2.0.0')
    },
    parent: dep1,
    path: '/dep1/node_modules/dep2a',
    realpath: '/dep1/node_modules/dep2a'
  }

  var pkg = {
    isTop: true,
    children: [dep1, dep2],
    package: {
      name: 'pkg',
      dependencies: { dep1: '1.0.0' },
      devDependencies: { dep2: '1.0.0' }
    },
    path: '/',
    realpath: '/'
  }

  dep1.parent = pkg
  dep2a.parent = dep1
  dep2.parent = pkg

  var earliest = earliestInstallable(dep1, dep1, dep2a.package)
  t.isDeeply(earliest, dep1, 'should hoist package when an incompatible devDependency is present')
  t.end()
})

test('earliestInstallable should reuse shared prod/dev deps when they are identical', function (t) {
  var dep1 = {
    children: [],
    package: {
      name: 'dep1',
      dependencies: { dep2: '1.0.0' }
    },
    path: '/dep1',
    realpath: '/dep1'
  }

  var dep2 = {
    package: {
      name: 'dep2',
      version: '1.0.0',
      _requested: npa('dep2@^1.0.0')
    },
    path: '/dep2',
    realpath: '/dep2'
  }

  var pkg = {
    isTop: true,
    children: [dep1],
    package: {
      name: 'pkg',
      dependencies: { dep1: '1.0.0' },
      devDependencies: { dep2: '^1.0.0' }
    },
    path: '/',
    realpath: '/'
  }

  dep1.parent = pkg
  dep2.parent = pkg

  var earliest = earliestInstallable(dep1, dep1, dep2.package)
  t.isDeeply(earliest, pkg, 'should reuse identical shared dev/prod deps when installing both')
  t.end()
})

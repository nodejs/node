'use strict'
var test = require('tap').test
var requireInject = require('require-inject')
var asap = require('asap')

// we're just mocking to avoid having to call `npm.load`
var deps = requireInject('../../lib/install/deps.js', {
  '../../lib/npm.js': {
    config: {
      get: function () { return 'mock' }
    },
    limit: {
      fetch: 10
    }
  }
})

var childDependencySpecifier = deps._childDependencySpecifier

test('childDependencySpecifier', function (t) {
  var tree = {
    resolved: {},
    package: {
      name: 'bar',
      _requested: ''
    }
  }

  childDependencySpecifier(tree, 'foo', '^1.0.0', function () {
    t.deepEqual(tree.resolved, {
      foo: {
        '^1.0.0': {
          raw: 'foo@^1.0.0',
          escapedName: 'foo',
          scope: null,
          name: 'foo',
          rawSpec: '^1.0.0',
          spec: '>=1.0.0 <2.0.0',
          type: 'range'
        }
      }
    }, 'should populate resolved')

    var order = []

    childDependencySpecifier(tree, 'foo', '^1.0.0', function () {
      order.push(1)
      childDependencySpecifier(tree, 'foo', '^1.0.0', function () {
        order.push(3)
        t.deepEqual(order, [1, 2, 3], 'should yield nested callbacks')
        t.end()
      })
    })

    asap(function () {
      order.push(2)
    })
  })
})

'use strict'
var test = require('tap').test
var requireInject = require('require-inject')

// we're just mocking to avoid having to call `npm.load`
var deps = requireInject('../../lib/install/deps.js', {
  '../../lib/npm.js': {
    config: {
      get: function () { return 'mock' }
    }
  }
})

var removeObsoleteDep = deps._removeObsoleteDep

test('removeObsoleteDep', function (t) {
  var child1 = {requiredBy: []}
  var test1 = {
    removed: true,
    requires: [ child1 ]
  }
  removeObsoleteDep(test1)
  t.is(child1.removed, undefined, 'no recursion on deps flagged as removed already')

  var child2 = {requiredBy: []}
  var test2 = {
    requires: [ child2 ]
  }
  child2.requiredBy.push(test2)
  removeObsoleteDep(test2)
  t.is(child2.removed, true, 'required by no other modules, removing')

  var child3 = {requiredBy: ['NOTEMPTY']}
  var test3 = {
    requires: [ child3 ]
  }
  child3.requiredBy.push(test3)
  removeObsoleteDep(test3)
  t.is(child3.removed, undefined, 'required by other modules, keeping')
  t.done()
})

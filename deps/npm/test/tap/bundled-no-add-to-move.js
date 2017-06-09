'use strict'
var test = require('tap').test
var Node = require('../../lib/install/node.js').create
var diffTrees = require('../../lib/install/diff-trees.js')._diffTrees
var sortActions = require('../../lib/install/diff-trees.js').sortActions

var oldTree = Node({
  path: '/',
  location: '/',
  children: [
    Node({
      package: {name: 'one', version: '1.0.0'},
      path: '/node_modules/one',
      location: '/one'
    })
  ]
})
oldTree.children[0].requiredBy.push(oldTree)

var newTree = Node({
  path: '/',
  location: '/',
  children: [
    Node({
      package: {name: 'abc', version: '1.0.0'},
      path: '/node_modules/abc',
      location: '/abc',
      children: [
        Node({
          package: {name: 'one', version: '1.0.0'},
          fromBundle: true,
          path: '/node_modules/abc/node_modules/one',
          location: '/abc/one'
        })
      ]
    })
  ]
})
newTree.children[0].requiredBy.push(newTree)
newTree.children[0].children[0].requiredBy.push(newTree.children[0])

test('test', function (t) {
  var differences = sortActions(diffTrees(oldTree, newTree)).map(function (diff) { return diff[0] + diff[1].location })
  t.isDeeply(differences, ['add/abc/one', 'remove/one', 'add/abc'], 'bundled add/remove stays add/remove')
  t.end()
})

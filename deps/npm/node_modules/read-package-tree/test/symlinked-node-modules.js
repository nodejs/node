'use strict'
var path = require('path')
var test = require('tap').test
var rpt = require('../rpt.js')
var Tacks = require('tacks')
var File = Tacks.File
var Symlink = Tacks.Symlink
var Dir = Tacks.Dir

var workdir = path.join(__dirname, path.basename(__filename, '.js'))
var fixture = new Tacks(Dir({
  bar: Dir({
    'package.json': File({
      name: 'bar',
      version: '1.0.0'
    })
  }),
  'linked-node-modules': Dir({
    bar: Symlink('../bar'),
    foo: Dir({
      'package.json': File({
        name: 'foo',
        version: '1.0.0'
      })
    })
  }),
  example: Dir({
    node_modules: Symlink('../linked-node-modules/'),
    'package.json': File({
      name: 'example',
      version: '1.0.0',
    })
  })
}))

function setup () {
  cleanup()
  fixture.create(workdir)
}

function cleanup () {
  fixture.remove(workdir)
}

test('setup', function (t) {
  setup()
  t.done()
})
test('symlinked-node-modules', function (t) {
  rpt(path.join(workdir, 'example'), function (err, tree) {
    t.ifError(err)
    t.is(tree.children.length, 2)
    var childrenShouldBe = {
      'foo': {isLink: false},
      'bar': {isLink: true}
    }
    tree.children.forEach(function (child) {
      var name = child.package.name
      t.is(child.isLink, childrenShouldBe[name].isLink,
        'is' + (childrenShouldBe[name].isLink ? '' : 'Not') + 'Link ' +
        path.relative(workdir, child.path) + " + " +
        path.relative(workdir, child.realpath))
    })
    t.done()
  })
})
test('cleanup', function (t) {
  cleanup()
  t.done()
})
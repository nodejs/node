var test = require('tap').test

var normalize = require('../')
var fixer = normalize.fixer

test('mixedcase', function (t) {
  t.doesNotThrow(function () {
    fixer.fixNameField({name: 'foo'}, true)
  })

  t.doesNotThrow(function () {
    fixer.fixNameField({name: 'foo'}, false)
  })

  t.doesNotThrow(function () {
    fixer.fixNameField({name: 'foo'})
  })

  t.throws(function () {
    fixer.fixNameField({name: 'Foo'}, true)
  }, new Error('Invalid name: "Foo"'), 'should throw an error')

  t.throws(function () {
    fixer.fixNameField({name: 'Foo'}, {strict: true})
  }, new Error('Invalid name: "Foo"'), 'should throw an error')

  t.doesNotThrow(function () {
    fixer.fixNameField({name: 'Foo'}, {strict: true, allowLegacyCase: true})
  })

  t.end()
})

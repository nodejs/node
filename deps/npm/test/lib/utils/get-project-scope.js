const getProjectScope = require('../../../lib/utils/get-project-scope.js')
const t = require('tap')

t.test('package.json with scope', t => {
  const dir = t.testdir({
    'package.json': JSON.stringify({ name: '@foo/bar' }),
  })
  t.equal(getProjectScope(dir), '@foo')
  t.end()
})

t.test('package.json with slash, but no @', t => {
  const dir = t.testdir({
    'package.json': JSON.stringify({ name: 'foo/bar' }),
  })
  t.equal(getProjectScope(dir), '')
  t.end()
})

t.test('package.json without scope', t => {
  const dir = t.testdir({
    'package.json': JSON.stringify({ name: 'foo' }),
  })
  t.equal(getProjectScope(dir), '')
  t.end()
})

t.test('package.json without name', t => {
  const dir = t.testdir({
    'package.json': JSON.stringify({}),
  })
  t.equal(getProjectScope(dir), '')
  t.end()
})

t.test('package.json not JSON', t => {
  const dir = t.testdir({
    'package.json': 'hello',
  })
  t.equal(getProjectScope(dir), '')
  t.end()
})

t.test('no package.json', t => {
  const dir = t.testdir({})
  t.equal(getProjectScope(dir), '')
  t.end()
})

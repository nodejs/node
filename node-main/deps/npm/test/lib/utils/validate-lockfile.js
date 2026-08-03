const t = require('tap')
const validateLockfile = require('../../../lib/utils/validate-lockfile.js')

t.test('identical inventory for both idealTree and virtualTree', async t => {
  t.matchSnapshot(
    validateLockfile(
      new Map([
        ['foo', { name: 'foo', version: '1.0.0' }],
        ['bar', { name: 'bar', version: '2.0.0' }],
      ]),
      new Map([
        ['foo', { name: 'foo', version: '1.0.0' }],
        ['bar', { name: 'bar', version: '2.0.0' }],
      ])
    ),
    'should have no errors on identical inventories'
  )
})

t.test('extra inventory items on idealTree', async t => {
  t.matchSnapshot(
    validateLockfile(
      new Map([
        ['foo', { name: 'foo', version: '1.0.0' }],
        ['bar', { name: 'bar', version: '2.0.0' }],
      ]),
      new Map([
        ['foo', { name: 'foo', version: '1.0.0' }],
        ['bar', { name: 'bar', version: '2.0.0' }],
        ['baz', { name: 'baz', version: '3.0.0' }],
      ])
    ),
    'should have missing entries error'
  )
})

t.test('extra inventory items on virtualTree', async t => {
  t.matchSnapshot(
    validateLockfile(
      new Map([
        ['foo', { name: 'foo', version: '1.0.0' }],
        ['bar', { name: 'bar', version: '2.0.0' }],
        ['baz', { name: 'baz', version: '3.0.0' }],
      ]),
      new Map([
        ['foo', { name: 'foo', version: '1.0.0' }],
        ['bar', { name: 'bar', version: '2.0.0' }],
      ])
    ),
    'should have no errors if finding virtualTree extra items'
  )
})

t.test('mismatching versions on inventory', async t => {
  t.matchSnapshot(
    validateLockfile(
      new Map([
        ['foo', { name: 'foo', version: '1.0.0' }],
        ['bar', { name: 'bar', version: '2.0.0' }],
      ]),
      new Map([
        ['foo', { name: 'foo', version: '2.0.0' }],
        ['bar', { name: 'bar', version: '3.0.0' }],
      ])
    ),
    'should have errors for each mismatching version'
  )
})

t.test('missing virtualTree inventory', async t => {
  t.matchSnapshot(
    validateLockfile(
      new Map([]),
      new Map([
        ['foo', { name: 'foo', version: '1.0.0' }],
        ['bar', { name: 'bar', version: '2.0.0' }],
        ['baz', { name: 'baz', version: '3.0.0' }],
      ])
    ),
    'should have errors for each mismatching version'
  )
})

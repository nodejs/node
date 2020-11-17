const { test } = require('tap')
const requireInject = require('require-inject')

test('prefix', (t) => {
  t.plan(3)
  const dir = '/prefix/dir'

  const prefix = requireInject('../../lib/prefix.js', {
    '../../lib/npm.js': { prefix: dir },
    '../../lib/utils/output.js': (output) => {
      t.equal(output, dir, 'prints the correct directory')
    },
  })

  prefix([], (err) => {
    t.ifError(err, 'npm prefix')
    t.ok('should have printed directory')
  })
})

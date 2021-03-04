const { test } = require('tap')
const requireInject = require('require-inject')

test('prefix', (t) => {
  t.plan(3)
  const dir = '/prefix/dir'

  const Prefix = requireInject('../../lib/prefix.js', {
    '../../lib/utils/output.js': (output) => {
      t.equal(output, dir, 'prints the correct directory')
    },
  })
  const prefix = new Prefix({ prefix: dir })

  prefix.exec([], (err) => {
    t.ifError(err, 'npm prefix')
    t.ok('should have printed directory')
  })
})

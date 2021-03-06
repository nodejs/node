const { test } = require('tap')
const requireInject = require('require-inject')

test('root', (t) => {
  t.plan(3)
  const dir = '/root/dir'

  const Root = requireInject('../../lib/root.js', {
    '../../lib/utils/output.js': (output) => {
      t.equal(output, dir, 'prints the correct directory')
    },
  })
  const root = new Root({ dir })

  root.exec([], (err) => {
    t.ifError(err, 'npm root')
    t.ok('should have printed directory')
  })
})

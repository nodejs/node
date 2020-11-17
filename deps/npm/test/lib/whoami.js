const { test } = require('tap')
const requireInject = require('require-inject')

test('whoami', (t) => {
  t.plan(3)
  const whoami = requireInject('../../lib/whoami.js', {
    '../../lib/utils/get-identity.js': () => Promise.resolve('foo'),
    '../../lib/npm.js': { flatOptions: {} },
    '../../lib/utils/output.js': (output) => {
      t.equal(output, 'foo', 'should output the username')
    },
  })

  whoami([], (err) => {
    t.ifError(err, 'npm whoami')
    t.ok('should successfully print username')
  })
})

test('whoami json', (t) => {
  t.plan(3)
  const whoami = requireInject('../../lib/whoami.js', {
    '../../lib/utils/get-identity.js': () => Promise.resolve('foo'),
    '../../lib/npm.js': { flatOptions: { json: true } },
    '../../lib/utils/output.js': (output) => {
      t.equal(output, '"foo"', 'should output the username as json')
    },
  })

  whoami([], (err) => {
    t.ifError(err, 'npm whoami')
    t.ok('should successfully print username as json')
  })
})

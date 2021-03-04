const { test } = require('tap')
const requireInject = require('require-inject')

test('whoami', (t) => {
  t.plan(3)
  const Whoami = requireInject('../../lib/whoami.js', {
    '../../lib/utils/get-identity.js': () => Promise.resolve('foo'),
    '../../lib/utils/output.js': (output) => {
      t.equal(output, 'foo', 'should output the username')
    },
  })
  const whoami = new Whoami({ flatOptions: {} })

  whoami.exec([], (err) => {
    t.ifError(err, 'npm whoami')
    t.ok('should successfully print username')
  })
})

test('whoami json', (t) => {
  t.plan(3)
  const Whoami = requireInject('../../lib/whoami.js', {
    '../../lib/utils/get-identity.js': () => Promise.resolve('foo'),
    '../../lib/utils/output.js': (output) => {
      t.equal(output, '"foo"', 'should output the username as json')
    },
  })
  const whoami = new Whoami({ flatOptions: { json: true } })

  whoami.exec([], (err) => {
    t.ifError(err, 'npm whoami')
    t.ok('should successfully print username as json')
  })
})

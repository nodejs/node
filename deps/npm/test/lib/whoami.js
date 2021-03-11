const { test } = require('tap')
const requireInject = require('require-inject')

test('whoami', (t) => {
  t.plan(3)
  const Whoami = requireInject('../../lib/whoami.js', {
    '../../lib/utils/get-identity.js': () => Promise.resolve('foo'),
  })
  const whoami = new Whoami({
    flatOptions: {},
    output: (output) => {
      t.equal(output, 'foo', 'should output the username')
    },
  })

  whoami.exec([], (err) => {
    t.ifError(err, 'npm whoami')
    t.ok('should successfully print username')
  })
})

test('whoami json', (t) => {
  t.plan(3)
  const Whoami = requireInject('../../lib/whoami.js', {
    '../../lib/utils/get-identity.js': () => Promise.resolve('foo'),
  })
  const whoami = new Whoami({
    flatOptions: { json: true },
    output: (output) => {
      t.equal(output, '"foo"', 'should output the username as json')
    },
  })

  whoami.exec([], (err) => {
    t.ifError(err, 'npm whoami')
    t.ok('should successfully print username as json')
  })
})

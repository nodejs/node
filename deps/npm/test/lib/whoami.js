const { test } = require('tap')
const requireInject = require('require-inject')
const mockNpm = require('../fixtures/mock-npm')

test('whoami', (t) => {
  t.plan(3)
  const Whoami = requireInject('../../lib/whoami.js', {
    '../../lib/utils/get-identity.js': () => Promise.resolve('foo'),
  })
  const npm = mockNpm({
    config: { json: false },
    output: (output) => {
      t.equal(output, 'foo', 'should output the username')
    },
  })

  const whoami = new Whoami(npm)

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
  const npm = mockNpm({
    config: { json: true },
    output: (output) => {
      t.equal(output, '"foo"', 'should output the username')
    },
  })
  const whoami = new Whoami(npm)

  whoami.exec([], (err) => {
    t.ifError(err, 'npm whoami')
    t.ok('should successfully print username as json')
  })
})

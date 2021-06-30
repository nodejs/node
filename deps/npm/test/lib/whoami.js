const t = require('tap')
const { fake: mockNpm } = require('../fixtures/mock-npm')

t.test('whoami', (t) => {
  t.plan(3)
  const Whoami = t.mock('../../lib/whoami.js', {
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
    t.error(err, 'npm whoami')
    t.ok('should successfully print username')
  })
})

t.test('whoami json', (t) => {
  t.plan(3)
  const Whoami = t.mock('../../lib/whoami.js', {
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
    t.error(err, 'npm whoami')
    t.ok('should successfully print username as json')
  })
})

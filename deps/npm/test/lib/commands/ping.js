const t = require('tap')
const { fake: mockNpm } = require('../../fixtures/mock-npm')

t.test('pings', async t => {
  t.plan(6)

  const registry = 'https://registry.npmjs.org'
  let noticeCalls = 0
  const Ping = t.mock('../../../lib/commands/ping.js', {
    '../../../lib/utils/ping.js': function (spec) {
      t.equal(spec.registry, registry, 'passes flatOptions')
      return {}
    },
    'proc-log': {
      notice: (type, spec) => {
        ++noticeCalls
        if (noticeCalls === 1) {
          t.equal(type, 'PING', 'should log a PING')
          t.equal(spec, registry, 'should log the registry url')
        } else {
          t.equal(type, 'PONG', 'should log a PONG')
          t.match(spec, /\d+ms/, 'should log the elapsed milliseconds')
        }
      },
    },
  })
  const npm = mockNpm({
    config: { registry },
    flatOptions: { registry },
  })
  const ping = new Ping(npm)

  await ping.exec([])
  t.equal(noticeCalls, 2, 'should have logged 2 lines')
})

t.test('pings and logs details', async t => {
  t.plan(8)

  const registry = 'https://registry.npmjs.org'
  const details = { extra: 'data' }
  let noticeCalls = 0
  const Ping = t.mock('../../../lib/commands/ping.js', {
    '../../../lib/utils/ping.js': function (spec) {
      t.equal(spec.registry, registry, 'passes flatOptions')
      return details
    },
    'proc-log': {
      notice: (type, spec) => {
        ++noticeCalls
        if (noticeCalls === 1) {
          t.equal(type, 'PING', 'should log a PING')
          t.equal(spec, registry, 'should log the registry url')
        } else if (noticeCalls === 2) {
          t.equal(type, 'PONG', 'should log a PONG')
          t.match(spec, /\d+ms/, 'should log the elapsed milliseconds')
        } else {
          t.equal(type, 'PONG', 'should log a PONG')
          const parsed = JSON.parse(spec)
          t.match(parsed, details, 'should log JSON stringified details')
        }
      },
    },
  })
  const npm = mockNpm({
    config: { registry },
    flatOptions: { registry },
  })
  const ping = new Ping(npm)

  await ping.exec([])
  t.equal(noticeCalls, 3, 'should have logged 3 lines')
})

t.test('pings and returns json', async t => {
  t.plan(9)

  const registry = 'https://registry.npmjs.org'
  const details = { extra: 'data' }
  let noticeCalls = 0
  const Ping = t.mock('../../../lib/commands/ping.js', {
    '../../../lib/utils/ping.js': function (spec) {
      t.equal(spec.registry, registry, 'passes flatOptions')
      return details
    },
    'proc-log': {
      notice: (type, spec) => {
        ++noticeCalls
        if (noticeCalls === 1) {
          t.equal(type, 'PING', 'should log a PING')
          t.equal(spec, registry, 'should log the registry url')
        } else {
          t.equal(type, 'PONG', 'should log a PONG')
          t.match(spec, /\d+ms/, 'should log the elapsed milliseconds')
        }
      },
    },
  })
  const npm = mockNpm({
    config: { registry, json: true },
    flatOptions: { registry },
    output: function (spec) {
      const parsed = JSON.parse(spec)
      t.equal(parsed.registry, registry, 'returns the correct registry url')
      t.match(parsed.details, details, 'prints returned details')
      t.type(parsed.time, 'number', 'returns time as a number')
    },
  })
  const ping = new Ping(npm)

  await ping.exec([])
  t.equal(noticeCalls, 2, 'should have logged 2 lines')
})

const t = require('tap')
const { load: loadMockNpm } = require('../../fixtures/mock-npm.js')
const MockRegistry = require('../../fixtures/mock-registry.js')

t.test('no details', async t => {
  const { npm, logs, joinedOutput } = await loadMockNpm(t)
  const registry = new MockRegistry({
    tap: t,
    registry: npm.config.get('registry'),
  })
  registry.ping()
  await npm.exec('ping', [])
  t.match(logs.notice, [['PING', 'https://registry.npmjs.org/'], ['PONG', /[0-9]+ms/]])
  t.equal(joinedOutput(), '')
})

t.test('with details', async t => {
  const { npm, logs, joinedOutput } = await loadMockNpm(t)
  const registry = new MockRegistry({
    tap: t,
    registry: npm.config.get('registry'),
  })
  registry.ping({ body: { test: true } })
  await npm.exec('ping', [])
  t.match(logs.notice, [
    ['PING', 'https://registry.npmjs.org/'],
    ['PONG', /[0-9]+ms/],
    ['PONG', '{\n  "test": true\n}'],
  ])
  t.match(joinedOutput(), '')
})

t.test('valid json', async t => {
  const { npm, logs, joinedOutput } = await loadMockNpm(t, {
    config: { json: true },
  })
  const registry = new MockRegistry({
    tap: t,
    registry: npm.config.get('registry'),
  })
  registry.ping()
  await npm.exec('ping', [])
  t.match(logs.notice, [['PING', 'https://registry.npmjs.org/'], ['PONG', /[0-9]+ms/]])
  t.match(JSON.parse(joinedOutput()), {
    registry: npm.config.get('registry'),
    time: /[0-9]+/,
    details: {},
  })
})

t.test('invalid json', async t => {
  const { npm, logs, joinedOutput } = await loadMockNpm(t, {
    config: { json: true },
  })
  const registry = new MockRegistry({
    tap: t,
    registry: npm.config.get('registry'),
  })
  registry.ping({ body: '{not: real"json]' })
  await npm.exec('ping', [])
  t.match(logs.notice, [['PING', 'https://registry.npmjs.org/'], ['PONG', /[0-9]+ms/]])
  t.match(JSON.parse(joinedOutput()), {
    registry: npm.config.get('registry'),
    time: /[0-9]+/,
    details: {},
  })
})

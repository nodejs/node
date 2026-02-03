const t = require('tap')
const { load: loadMockNpm } = require('../../fixtures/mock-npm.js')
const MockRegistry = require('@npmcli/mock-registry')

const authToken = 'abcd1234'

const auth = {
  '//registry.npmjs.org/:_authToken': authToken,
}

const now = new Date().toISOString()
const tokens = [
  {
    key: 'abcd1234abcd1234',
    token: 'efgh5678efgh5678',
    cidr_whitelist: null,
    readonly: false,
    created: now,
    updated: now,
  },
  {
    key: 'abcd1256',
    token: 'hgfe8765',
    cidr_whitelist: ['192.168.1.1/32'],
    readonly: true,
    created: now,
    updated: now,
  },
]

t.test('completion', async t => {
  const { token } = await loadMockNpm(t, { command: 'token' })

  const testComp = (argv, expect) => {
    t.resolveMatch(token.completion({ conf: { argv: { remain: argv } } }), expect, argv.join(' '))
  }

  testComp(['npm', 'token'], ['list', 'revoke', 'create'])
  testComp(['npm', 'token', 'list'], [])
  testComp(['npm', 'token', 'revoke'], [])
  testComp(['npm', 'token', 'create'], [])

  t.rejects(token.completion({ conf: { argv: { remain: ['npm', 'token', 'foobar'] } } }), {
    message: 'foobar not recognize',
  })
})

t.test('token foobar', async t => {
  const { npm } = await loadMockNpm(t)

  await t.rejects(npm.exec('token', ['foobar']), /foobar is not a recognized subcommand/)
})

t.test('token list', async t => {
  const { npm, outputs } = await loadMockNpm(t, {
    config: { ...auth },
  })
  const registry = new MockRegistry({
    tap: t,
    registry: npm.config.get('registry'),
    authorization: authToken,
  })
  registry.getTokens(tokens)
  await npm.exec('token', [])
  t.strictSame(outputs, [
    `Token efgh5678efgh5678… with id abcd123 created ${now.slice(0, 10)}`,
    '',
    `Token hgfe8765… with id abcd125 created ${now.slice(0, 10)}`,
    'with IP whitelist: 192.168.1.1/32',
    '',
  ])
})

t.test('token list json output', async t => {
  const { npm, joinedOutput } = await loadMockNpm(t, {
    config: {
      ...auth,
      json: true,
    },
  })
  const registry = new MockRegistry({
    tap: t,
    registry: npm.config.get('registry'),
    authorization: authToken,
  })
  registry.getTokens(tokens)
  await npm.exec('token', ['list'])
  const parsed = JSON.parse(joinedOutput())
  t.match(parsed, tokens, 'prints the json parsed tokens')
})

t.test('token list parseable output', async t => {
  const { npm, outputs } = await loadMockNpm(t, {
    config: {
      ...auth,
      parseable: true,
    },
  })
  const registry = new MockRegistry({
    tap: t,
    registry: npm.config.get('registry'),
    authorization: authToken,
  })
  registry.getTokens(tokens)
  await npm.exec('token', [])
  t.strictSame(outputs, [
    'key\ttoken\tcreated\treadonly\tCIDR whitelist',
    `abcd1234abcd1234\tefgh5678efgh5678\t${now}\tfalse\t`,
    `abcd1256\thgfe8765\t${now}\ttrue\t192.168.1.1/32`,
  ])
})

t.test('token revoke single', async t => {
  const { npm, joinedOutput } = await loadMockNpm(t, {
    config: { ...auth },
  })
  const registry = new MockRegistry({
    tap: t,
    registry: npm.config.get('registry'),
    authorization: authToken,
  })

  registry.getTokens(tokens)
  registry.revokeToken(tokens[0].key)
  await npm.exec('token', ['rm', tokens[0].key.slice(0, 8)])

  t.equal(joinedOutput(), 'Removed 1 token')
})

t.test('token revoke multiple', async t => {
  const { npm, joinedOutput } = await loadMockNpm(t, {
    config: { ...auth },
  })
  const registry = new MockRegistry({
    tap: t,
    registry: npm.config.get('registry'),
    authorization: authToken,
  })

  registry.getTokens(tokens)
  registry.revokeToken(tokens[0].key)
  registry.revokeToken(tokens[1].key)
  await npm.exec('token', ['rm', tokens[0].key.slice(0, 8), tokens[1].key.slice(0, 8)])

  t.equal(joinedOutput(), 'Removed 2 tokens')
})

t.test('token revoke json output', async t => {
  const { npm, joinedOutput } = await loadMockNpm(t, {
    config: {
      ...auth,
      json: true,
    },
  })
  const registry = new MockRegistry({
    tap: t,
    registry: npm.config.get('registry'),
    authorization: authToken,
  })

  registry.getTokens(tokens)
  registry.revokeToken(tokens[0].key)
  await npm.exec('token', ['rm', tokens[0].key.slice(0, 8)])

  const parsed = JSON.parse(joinedOutput())
  t.same(parsed, [tokens[0].key], 'logs the token as json')
})

t.test('token revoke parseable output', async t => {
  const { npm, joinedOutput } = await loadMockNpm(t, {
    config: {
      ...auth,
      parseable: true,
    },
  })
  const registry = new MockRegistry({
    tap: t,
    registry: npm.config.get('registry'),
    authorization: authToken,
  })

  registry.getTokens(tokens)
  registry.revokeToken(tokens[0].key)
  await npm.exec('token', ['rm', tokens[0].key.slice(0, 8)])
  t.equal(joinedOutput(), tokens[0].key, 'logs the token as a string')
})

t.test('token revoke by token', async t => {
  const { npm, joinedOutput } = await loadMockNpm(t, {
    config: { ...auth },
  })
  const registry = new MockRegistry({
    tap: t,
    registry: npm.config.get('registry'),
    authorization: authToken,
  })
  registry.getTokens(tokens)
  registry.revokeToken(tokens[0].token)
  await npm.exec('token', ['rm', tokens[0].token])
  t.equal(joinedOutput(), 'Removed 1 token')
})

t.test('token revoke requires an id', async t => {
  const { npm } = await loadMockNpm(t)

  await t.rejects(npm.exec('token', ['rm']), {
    code: 'EUSAGE',
    message: '`<tokenKey>` argument is required',
  })
})

t.test('token revoke ambiguous id errors', async t => {
  const { npm } = await loadMockNpm(t, {
    config: { ...auth },
  })
  const registry = new MockRegistry({
    tap: t,
    registry: npm.config.get('registry'),
    authorization: authToken,
  })
  registry.getTokens(tokens)
  await t.rejects(npm.exec('token', ['rm', 'abcd']), {
    message: /Token ID "abcd" was ambiguous/,
  })
})

t.test('token revoke unknown token', async t => {
  const { npm } = await loadMockNpm(t, {
    config: { ...auth },
  })
  const registry = new MockRegistry({
    tap: t,
    registry: npm.config.get('registry'),
    authorization: authToken,
  })

  registry.getTokens(tokens)
  await t.rejects(npm.exec('token', ['rm', '0xnotreal']),
    'Unknown token id or value 0xnotreal'
  )
})

t.test('token create defaults', async t => {
  const { npm, outputs } = await loadMockNpm(t, {
    config: {
      ...auth,
      name: 'test-token',
      password: 'test-password',
    },
  })

  const registry = new MockRegistry({
    tap: t,
    registry: npm.config.get('registry'),
    authorization: authToken,
  })

  registry.createToken({
    name: 'test-token',
    password: 'test-password',
  }, {
    access: 'publish',
  })

  await npm.exec('token', ['create'])
  t.match(outputs, ['Created token n3wt0k3n'])
})

t.test('token create extra token attributes', async t => {
  const { npm, outputs } = await loadMockNpm(t, {
    config: {
      ...auth,
      'bypass-2fa': true,
      cidr: ['10.0.0.0/8', '192.168.1.0/24'],
      expires: 1000,
      name: 'extras-token',
      orgs: ['@npmcli'],
      'orgs-permission': 'read-write',
      packages: ['@npmcli/test-package'],
      'packages-and-scopes-permission': 'read-only',
      'packages-all': true,
      password: 'test-password',
      scopes: ['@npmcli'],
      'token-description': 'test token',
    },
  })
  const registry = new MockRegistry({
    tap: t,
    registry: npm.config.get('registry'),
    authorization: authToken,
  })

  const expires = new Date()
  registry.createToken({
    bypass_2fa: true,
    cidr_whitelist: ['10.0.0.0/8', '192.168.1.0/24'],
    description: 'test token',
    expires: 1000,
    name: 'extras-token',
    orgs_permission: 'read-write',
    orgs: ['@npmcli'],
    packages_all: true,
    packages_and_scopes_permission: 'read-only',
    packages: ['@npmcli/test-package'],
    password: 'test-password',
    scopes: ['@npmcli'],
  }, {
    cidr_whitelist: ['10.0.0.0/8', '192.168.1.0/24'],
    expires,
  })

  await npm.exec('token', ['create'])
  t.match(outputs, [
    'Created token n3wt0k3n',
    'with IP whitelist: 10.0.0.0/8,192.168.1.0/24',
    `expires: ${expires.toISOString()}`,
  ])
})

t.test('token create access.read-only', async t => {
  const { npm, outputs } = await loadMockNpm(t, {
    config: {
      ...auth,
      name: 'test-token',
      password: 'test-password',
    },
  })

  const registry = new MockRegistry({
    tap: t,
    registry: npm.config.get('registry'),
    authorization: authToken,
  })

  registry.createToken({
    name: 'test-token',
    password: 'test-password',
  }, {
    access: 'read-only',
  })

  await npm.exec('token', ['create'])
  t.match(outputs, ['Created token n3wt0k3n'])
})

t.test('token create readonly', async t => {
  const { npm, outputs } = await loadMockNpm(t, {
    config: {
      ...auth,
      name: 'test-token',
      password: 'test-password',
    },
  }, {
    readonly: true,
  })

  const registry = new MockRegistry({
    tap: t,
    registry: npm.config.get('registry'),
    authorization: authToken,
  })

  registry.createToken({
    name: 'test-token',
    password: 'test-password',
  }, {
    access: 'read-only',
  })

  await npm.exec('token', ['create'])
  t.match(outputs, ['Created token n3wt0k3n'])
})

t.test('token create json output', async t => {
  const { npm, joinedOutput } = await loadMockNpm(t, {
    config: {
      ...auth,
      json: true,
      name: 'test-token',
      password: 'test-password',
    },
  })

  const registry = new MockRegistry({
    tap: t,
    registry: npm.config.get('registry'),
    authorization: authToken,
  })

  const created = new Date()
  registry.createToken({
    name: 'test-token',
    password: 'test-password',
  }, {
    created,
    other: 'attr',
  })

  await npm.exec('token', ['create'])
  t.match(JSON.parse(joinedOutput()), {
    access: 'read-only',
    created: created.toISOString(),
    id: '0xdeadbeef',
    name: 'test-token',
    other: 'attr',
    password: 'test-password',
    token: 'n3wt0k3n',
  })
})

t.test('token create parseable output', async t => {
  const { npm, outputs } = await loadMockNpm(t, {
    config: {
      ...auth,
      parseable: true,
      name: 'test-token',
      password: 'test-password',
    },
  })

  const registry = new MockRegistry({
    tap: t,
    registry: npm.config.get('registry'),
    authorization: authToken,
  })

  const created = new Date()
  registry.createToken({
    name: 'test-token',
    password: 'test-password',
  }, {
    access: 'publish',
    created,
  })

  await npm.exec('token', ['create'])
  t.match(outputs, [
    'id\t0xdeadbeef',
    'token\tn3wt0k3n',
    `created\t${created.toISOString()}`,
    'access\tpublish',
    'name\ttest-token',
    'password\ttest-password',
  ])
})

t.test('token create ipv6 cidr', async t => {
  const { npm } = await loadMockNpm(t, {
    config: {
      ...auth,
      cidr: '::1/128',
      name: 'ipv6-test',
      access: 'read-only',
    },
  })
  await t.rejects(npm.exec('token', ['create']), {
    code: 'EINVALIDCIDR',
    message: /CIDR whitelist can only contain IPv4 addresses, ::1\/128 is IPv6/,
  })
})

t.test('token create invalid cidr', async t => {
  const { npm } = await loadMockNpm(t, {
    config: {
      ...auth,
      cidr: 'apple/cider',
      name: 'invalid-cidr-test',
      access: 'read-only',
    },
  })
  await t.rejects(npm.exec('token', ['create']), {
    code: 'EINVALIDCIDR',
    message: 'CIDR whitelist contains invalid CIDR entry: apple/cider',
  })
})

const t = require('tap')
const mockNpm = require('../../fixtures/mock-npm')

const mockToken = async (t, { profile, getCredentialsByURI, readUserInfo, ...opts } = {}) => {
  const mocks = {}

  if (profile) {
    mocks['npm-profile'] = profile
  }

  if (readUserInfo) {
    mocks['{LIB}/utils/read-user-info.js'] = readUserInfo
  }

  const mock = await mockNpm(t, {
    ...opts,
    command: 'token',
    mocks,
  })

  // XXX: replace with mock registry
  if (getCredentialsByURI) {
    mock.npm.config.getCredentialsByURI = getCredentialsByURI
  }

  return mock
}

t.test('completion', async t => {
  const { token } = await mockToken(t)

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
  const { token } = await mockToken(t)

  await t.rejects(token.exec(['foobar']), /foobar is not a recognized subcommand/)
})

t.test('token list', async t => {
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

  const { token, joinedOutput } = await mockToken(t, {
    config: { registry: 'https://registry.npmjs.org', otp: '123456' },
    getCredentialsByURI: uri => {
      t.equal(uri, 'https://registry.npmjs.org/', 'requests correct registry')
      return { token: 'thisisnotarealtoken' }
    },
    profile: {
      listTokens: conf => {
        t.same(conf.auth, { token: 'thisisnotarealtoken', otp: '123456' })
        return tokens
      },
    },
  })

  await token.exec([])

  const lines = joinedOutput().split(/\r?\n/)
  t.match(lines[3], ' abcd123 ', 'includes the trimmed key')
  t.match(lines[3], ' efgh56… ', 'includes the trimmed token')
  t.match(lines[3], ` ${now.slice(0, 10)} `, 'includes the trimmed creation timestamp')
  t.match(lines[3], ' no ', 'includes the "no" string for readonly state')
  t.match(lines[5], ' abcd125 ', 'includes the trimmed key')
  t.match(lines[5], ' hgfe87… ', 'includes the trimmed token')
  t.match(lines[5], ` ${now.slice(0, 10)} `, 'includes the trimmed creation timestamp')
  t.match(lines[5], ' yes ', 'includes the "no" string for readonly state')
  t.match(lines[5], ` ${tokens[1].cidr_whitelist.join(',')} `, 'includes the cidr whitelist')
})

t.test('token list json output', async t => {
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
  ]

  const { token, joinedOutput } = await mockToken(t, {
    config: { registry: 'https://registry.npmjs.org', json: true },
    getCredentialsByURI: uri => {
      t.equal(uri, 'https://registry.npmjs.org/', 'requests correct registry')
      return { username: 'foo', password: 'bar' }
    },
    profile: {
      listTokens: conf => {
        t.same(
          conf.auth,
          { basic: { username: 'foo', password: 'bar' } },
          'passes the correct auth'
        )
        return tokens
      },
    },

  })

  await token.exec(['list'])

  const parsed = JSON.parse(joinedOutput())
  t.match(parsed, tokens, 'prints the json parsed tokens')
})

t.test('token list parseable output', async t => {
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
      key: 'efgh5678ijkl9101',
      token: 'hgfe8765',
      cidr_whitelist: ['192.168.1.1/32'],
      readonly: true,
      created: now,
      updated: now,
    },
  ]

  const { token, joinedOutput } = await mockToken(t, {
    config: { registry: 'https://registry.npmjs.org', parseable: true },
    getCredentialsByURI: uri => {
      t.equal(uri, 'https://registry.npmjs.org/', 'requests correct registry')
      return { auth: Buffer.from('foo:bar').toString('base64') }
    },
    profile: {
      listTokens: conf => {
        t.same(
          conf.auth,
          { basic: { username: 'foo', password: 'bar' } },
          'passes the correct auth'
        )
        return tokens
      },
    },
  })

  await token.exec(['list'])

  const lines = joinedOutput().split(/\r?\n/)

  t.equal(
    lines[0],
    ['key', 'token', 'created', 'readonly', 'CIDR whitelist'].join('\t'),
    'prints header'
  )

  t.equal(
    lines[1],
    [tokens[0].key, tokens[0].token, tokens[0].created, tokens[0].readonly, ''].join('\t'),
    'prints token info'
  )

  t.equal(
    lines[2],
    [
      tokens[1].key,
      tokens[1].token,
      tokens[1].created,
      tokens[1].readonly,
      tokens[1].cidr_whitelist.join(','),
    ].join('\t'),
    'prints token info'
  )
})

t.test('token revoke', async t => {
  const { token, joinedOutput } = await mockToken(t, {
    config: { registry: 'https://registry.npmjs.org' },
    getCredentialsByURI: uri => {
      t.equal(uri, 'https://registry.npmjs.org/', 'requests correct registry')
      return {}
    },
    profile: {
      listTokens: conf => {
        t.same(conf.auth, {}, 'passes the correct empty auth')
        return Promise.resolve([{ key: 'abcd1234' }])
      },
      removeToken: key => {
        t.equal(key, 'abcd1234', 'deletes the correct token')
      },
    },
  })

  await token.exec(['rm', 'abcd'])

  t.equal(joinedOutput(), 'Removed 1 token')
})

t.test('token revoke multiple tokens', async t => {
  const { token, joinedOutput } = await mockToken(t, {
    config: { registry: 'https://registry.npmjs.org' },
    getCredentialsByURI: uri => {
      t.equal(uri, 'https://registry.npmjs.org/', 'requests correct registry')
      return { token: 'thisisnotarealtoken' }
    },
    profile: {
      listTokens: () => Promise.resolve([{ key: 'abcd1234' }, { key: 'efgh5678' }]),
      removeToken: key => {
        // this will run twice
        t.ok(['abcd1234', 'efgh5678'].includes(key), 'deletes the correct token')
      },
    },
  })

  await token.exec(['revoke', 'abcd', 'efgh'])

  t.equal(joinedOutput(), 'Removed 2 tokens')
})

t.test('token revoke json output', async t => {
  const { token, joinedOutput } = await mockToken(t, {
    config: { registry: 'https://registry.npmjs.org', json: true },
    getCredentialsByURI: uri => {
      t.equal(uri, 'https://registry.npmjs.org/', 'requests correct registry')
      return { token: 'thisisnotarealtoken' }
    },
    profile: {
      listTokens: () => Promise.resolve([{ key: 'abcd1234' }]),
      removeToken: key => {
        t.equal(key, 'abcd1234', 'deletes the correct token')
      },
    },

  })

  await token.exec(['delete', 'abcd'])

  const parsed = JSON.parse(joinedOutput())
  t.same(parsed, ['abcd1234'], 'logs the token as json')
})

t.test('token revoke parseable output', async t => {
  const { token, joinedOutput } = await mockToken(t, {
    config: { registry: 'https://registry.npmjs.org', parseable: true },
    getCredentialsByURI: uri => {
      t.equal(uri, 'https://registry.npmjs.org/', 'requests correct registry')
      return { token: 'thisisnotarealtoken' }
    },
    profile: {
      listTokens: () => Promise.resolve([{ key: 'abcd1234' }]),
      removeToken: key => {
        t.equal(key, 'abcd1234', 'deletes the correct token')
      },
    },
  })

  await token.exec(['remove', 'abcd'])

  t.equal(joinedOutput(), 'abcd1234', 'logs the token as a string')
})

t.test('token revoke by token', async t => {
  const { token, joinedOutput } = await mockToken(t, {
    config: { registry: 'https://registry.npmjs.org' },
    getCredentialsByURI: uri => {
      t.equal(uri, 'https://registry.npmjs.org/', 'requests correct registry')
      return { token: 'thisisnotarealtoken' }
    },
    profile: {
      listTokens: () => Promise.resolve([{ key: 'abcd1234', token: 'efgh5678' }]),
      removeToken: key => {
        t.equal(key, 'efgh5678', 'passes through user input')
      },
    },
  })

  await token.exec(['rm', 'efgh5678'])
  t.equal(joinedOutput(), 'Removed 1 token')
})

t.test('token revoke requires an id', async t => {
  const { token } = await mockToken(t)

  await t.rejects(token.exec(['rm']), /`<tokenKey>` argument is required/)
})

t.test('token revoke ambiguous id errors', async t => {
  const { token } = await mockToken(t, {
    config: { registry: 'https://registry.npmjs.org' },
    getCredentialsByURI: uri => {
      t.equal(uri, 'https://registry.npmjs.org/', 'requests correct registry')
      return { token: 'thisisnotarealtoken' }
    },
    profile: {
      listTokens: () => Promise.resolve([{ key: 'abcd1234' }, { key: 'abcd5678' }]),
    },
  })

  await t.rejects(token.exec(['rm', 'abcd']), /Token ID "abcd" was ambiguous/)
})

t.test('token revoke unknown id errors', async t => {
  const { token } = await mockToken(t, {
    config: { registry: 'https://registry.npmjs.org' },
    getCredentialsByURI: uri => {
      t.equal(uri, 'https://registry.npmjs.org/', 'requests correct registry')
      return { token: 'thisisnotarealtoken' }
    },
    profile: {
      listTokens: () => Promise.resolve([{ key: 'abcd1234' }]),
    },
  })

  await t.rejects(token.exec(['rm', 'efgh']), /Unknown token id or value "efgh"./)
})

t.test('token create', async t => {
  const now = new Date().toISOString()
  const password = 'thisisnotreallyapassword'

  const { token, joinedOutput } = await mockToken(t, {
    config: {
      registry: 'https://registry.npmjs.org',
      cidr: ['10.0.0.0/8', '192.168.1.0/24'],
    },
    getCredentialsByURI: uri => {
      t.equal(uri, 'https://registry.npmjs.org/', 'requests correct registry')
      return { token: 'thisisnotarealtoken' }
    },
    readUserInfo: {
      password: () => Promise.resolve(password),
    },
    profile: {
      createToken: (pw, readonly, cidr) => {
        t.equal(pw, password)
        t.equal(readonly, false)
        t.same(cidr, ['10.0.0.0/8', '192.168.1.0/24'], 'defaults to empty array')
        return {
          key: 'abcd1234',
          token: 'efgh5678',
          created: now,
          updated: now,
          readonly: false,
          cidr_whitelist: [],
        }
      },
    },

  })

  await token.exec(['create'])

  const lines = joinedOutput().split(/\r?\n/)
  t.match(lines[1], 'token')
  t.match(lines[1], 'efgh5678', 'prints the whole token')
  t.match(lines[3], 'created')
  t.match(lines[3], now, 'prints the correct timestamp')
  t.match(lines[5], 'readonly')
  t.match(lines[5], 'false', 'prints the readonly flag')
  t.match(lines[7], 'cidr_whitelist')
})

t.test('token create json output', async t => {
  const now = new Date().toISOString()
  const password = 'thisisnotreallyapassword'

  const { token } = await mockToken(t, {
    config: { registry: 'https://registry.npmjs.org', json: true },
    getCredentialsByURI: uri => {
      t.equal(uri, 'https://registry.npmjs.org/', 'requests correct registry')
      return { token: 'thisisnotarealtoken' }
    },
    readUserInfo: {
      password: () => Promise.resolve(password),
    },
    profile: {
      createToken: (pw, readonly, cidr) => {
        t.equal(pw, password)
        t.equal(readonly, false)
        t.same(cidr, [], 'defaults to empty array')
        return {
          key: 'abcd1234',
          token: 'efgh5678',
          created: now,
          updated: now,
          readonly: false,
          cidr_whitelist: [],
        }
      },
    },
    output: spec => {
      t.type(spec, 'string', 'outputs a string')
      const parsed = JSON.parse(spec)
      t.same(
        parsed,
        { token: 'efgh5678', created: now, readonly: false, cidr_whitelist: [] },
        'outputs the correct object'
      )
    },
  })

  await token.exec(['create'])
})

t.test('token create parseable output', async t => {
  const now = new Date().toISOString()
  const password = 'thisisnotreallyapassword'

  const { token, joinedOutput } = await mockToken(t, {
    config: { registry: 'https://registry.npmjs.org', parseable: true },
    getCredentialsByURI: uri => {
      t.equal(uri, 'https://registry.npmjs.org/', 'requests correct registry')
      return { token: 'thisisnotarealtoken' }
    },
    readUserInfo: {
      password: () => Promise.resolve(password),
    },
    profile: {
      createToken: (pw, readonly, cidr) => {
        t.equal(pw, password)
        t.equal(readonly, false)
        t.same(cidr, [], 'defaults to empty array')
        return {
          key: 'abcd1234',
          token: 'efgh5678',
          created: now,
          updated: now,
          readonly: false,
          cidr_whitelist: [],
        }
      },
    },
  })

  await token.exec(['create'])

  const spec = joinedOutput().split(/\r?\n/)

  t.match(spec[0], 'token\tefgh5678', 'prints the token')
  t.match(spec[1], `created\t${now}`, 'prints the created timestamp')
  t.match(spec[2], 'readonly\tfalse', 'prints the readonly flag')
  t.match(spec[3], 'cidr_whitelist\t', 'prints the cidr whitelist')
})

t.test('token create ipv6 cidr', async t => {
  const password = 'thisisnotreallyapassword'

  const { token } = await mockToken(t, {
    config: { registry: 'https://registry.npmjs.org', cidr: '::1/128' },
    getCredentialsByURI: uri => {
      t.equal(uri, 'https://registry.npmjs.org/', 'requests correct registry')
      return { token: 'thisisnotarealtoken' }
    },
    readUserInfo: {
      password: () => Promise.resolve(password),
    },
  })

  await t.rejects(
    token.exec(['create']),
    {
      code: 'EINVALIDCIDR',
      message: /CIDR whitelist can only contain IPv4 addresses, ::1\/128 is IPv6/,
    },
    'returns correct error'
  )
})

t.test('token create invalid cidr', async t => {
  const password = 'thisisnotreallyapassword'

  const { token } = await mockToken(t, {
    config: { registry: 'https://registry.npmjs.org', cidr: 'apple/cider' },
    getCredentialsByURI: uri => {
      t.equal(uri, 'https://registry.npmjs.org/', 'requests correct registry')
      return { token: 'thisisnotarealtoken' }
    },
    readUserInfo: {
      password: () => Promise.resolve(password),
    },
  })

  await t.rejects(
    token.exec(['create']),
    { code: 'EINVALIDCIDR', message: /CIDR whitelist contains invalid CIDR entry: apple\/cider/ },
    'returns correct error'
  )
})

const t = require('tap')

const mocks = {
  profile: {},
  output: () => {},
  readUserInfo: {},
}
const npm = {
  output: (...args) => mocks.output(...args),
}

const mockToken = (otherMocks) => t.mock('../../../lib/commands/token.js', {
  '../../../lib/utils/read-user-info.js': mocks.readUserInfo,
  'npm-profile': mocks.profile,
  ...otherMocks,
})

const tokenWithMocks = (options = {}) => {
  const { log, ...mockRequests } = options

  for (const mod in mockRequests) {
    if (mod === 'npm') {
      mockRequests.npm = { ...npm, ...mockRequests.npm }
    } else {
      if (typeof mockRequests[mod] === 'function') {
        mocks[mod] = mockRequests[mod]
      } else {
        for (const key in mockRequests[mod]) {
          mocks[mod][key] = mockRequests[mod][key]
        }
      }
    }
  }

  const reset = () => {
    for (const mod in mockRequests) {
      if (mod !== 'npm') {
        if (typeof mockRequests[mod] === 'function') {
          mocks[mod] = () => {}
        } else {
          for (const key in mockRequests[mod]) {
            delete mocks[mod][key]
          }
        }
      }
    }
  }

  const MockedToken = mockToken(log ? {
    'proc-log': {
      info: log.info,
    },
    npmlog: {
      gauge: log.gauge,
      newItem: log.newItem,
    },
  } : {})
  const token = new MockedToken(mockRequests.npm || npm)
  return [token, reset]
}

t.test('completion', t => {
  t.plan(5)

  const [token] = tokenWithMocks()

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
  t.plan(2)

  const [token, reset] = tokenWithMocks({
    log: {
      gauge: {
        show: name => {
          t.equal(name, 'token', 'shows a gauge')
        },
      },
    },
  })

  t.teardown(reset)

  await t.rejects(token.exec(['foobar']), /foobar is not a recognized subcommand/)
})

t.test('token list', async t => {
  t.plan(14)

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

  const [token, reset] = tokenWithMocks({
    npm: {
      flatOptions: { registry: 'https://registry.npmjs.org', otp: '123456' },
      config: {
        getCredentialsByURI: uri => {
          t.equal(uri, 'https://registry.npmjs.org', 'requests correct registry')
          return { token: 'thisisnotarealtoken' }
        },
      },
    },
    profile: {
      listTokens: conf => {
        t.same(conf.auth, { token: 'thisisnotarealtoken', otp: '123456' })
        return tokens
      },
    },
    log: {
      gauge: {
        show: name => {
          t.equal(name, 'token')
        },
      },
      info: (type, msg) => {
        t.equal(type, 'token')
        t.equal(msg, 'getting list')
      },
    },
    output: spec => {
      const lines = spec.split(/\r?\n/)
      t.match(lines[3], ' abcd123 ', 'includes the trimmed key')
      t.match(lines[3], ' efgh56… ', 'includes the trimmed token')
      t.match(lines[3], ` ${now.slice(0, 10)} `, 'includes the trimmed creation timestamp')
      t.match(lines[3], ' no ', 'includes the "no" string for readonly state')
      t.match(lines[5], ' abcd125 ', 'includes the trimmed key')
      t.match(lines[5], ' hgfe87… ', 'includes the trimmed token')
      t.match(lines[5], ` ${now.slice(0, 10)} `, 'includes the trimmed creation timestamp')
      t.match(lines[5], ' yes ', 'includes the "no" string for readonly state')
      t.match(lines[5], ` ${tokens[1].cidr_whitelist.join(',')} `, 'includes the cidr whitelist')
    },
  })

  t.teardown(reset)

  await token.exec([])
})

t.test('token list json output', async t => {
  t.plan(7)

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

  const [token, reset] = tokenWithMocks({
    npm: {
      flatOptions: { registry: 'https://registry.npmjs.org', json: true },
      config: {
        getCredentialsByURI: uri => {
          t.equal(uri, 'https://registry.npmjs.org', 'requests correct registry')
          return { username: 'foo', password: 'bar' }
        },
      },
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
    log: {
      gauge: {
        show: name => {
          t.equal(name, 'token')
        },
      },
      info: (type, msg) => {
        t.equal(type, 'token')
        t.equal(msg, 'getting list')
      },
    },
    output: spec => {
      t.type(spec, 'string', 'is called with a string')
      const parsed = JSON.parse(spec)
      t.match(parsed, tokens, 'prints the json parsed tokens')
    },
  })

  t.teardown(reset)

  await token.exec(['list'])
})

t.test('token list parseable output', async t => {
  t.plan(11)

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

  let callCount = 0

  const [token, reset] = tokenWithMocks({
    npm: {
      flatOptions: { registry: 'https://registry.npmjs.org', parseable: true },
      config: {
        getCredentialsByURI: uri => {
          t.equal(uri, 'https://registry.npmjs.org', 'requests correct registry')
          return { auth: Buffer.from('foo:bar').toString('base64') }
        },
      },
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
    log: {
      gauge: {
        show: name => {
          t.equal(name, 'token')
        },
      },
      info: (type, msg) => {
        t.equal(type, 'token')
        t.equal(msg, 'getting list')
      },
    },
    output: spec => {
      ++callCount
      t.type(spec, 'string', 'is called with a string')
      if (callCount === 1) {
        t.equal(
          spec,
          ['key', 'token', 'created', 'readonly', 'CIDR whitelist'].join('\t'),
          'prints header'
        )
      } else if (callCount === 2) {
        t.equal(
          spec,
          [tokens[0].key, tokens[0].token, tokens[0].created, tokens[0].readonly, ''].join('\t'),
          'prints token info'
        )
      } else {
        t.equal(
          spec,
          [
            tokens[1].key,
            tokens[1].token,
            tokens[1].created,
            tokens[1].readonly,
            tokens[1].cidr_whitelist.join(','),
          ].join('\t'),
          'prints token info'
        )
      }
    },
  })

  t.teardown(reset)

  await token.exec(['list'])
})

t.test('token revoke', async t => {
  t.plan(9)

  const [token, reset] = tokenWithMocks({
    npm: {
      flatOptions: { registry: 'https://registry.npmjs.org' },
      config: {
        getCredentialsByURI: uri => {
          t.equal(uri, 'https://registry.npmjs.org', 'requests correct registry')
          return {}
        },
      },
    },
    log: {
      gauge: {
        show: name => {
          t.equal(name, 'token', 'starts a gauge')
        },
      },
      newItem: (action, len) => {
        t.equal(action, 'removing tokens')
        t.equal(len, 0)
        return {
          info: (name, progress) => {
            t.equal(name, 'token')
            t.equal(progress, 'getting existing list')
          },
        }
      },
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
    output: spec => {
      t.equal(spec, 'Removed 1 token')
    },
  })

  t.teardown(reset)

  await token.exec(['rm', 'abcd'])
})

t.test('token revoke multiple tokens', async t => {
  t.plan(9)

  const [token, reset] = tokenWithMocks({
    npm: {
      flatOptions: { registry: 'https://registry.npmjs.org' },
      config: {
        getCredentialsByURI: uri => {
          t.equal(uri, 'https://registry.npmjs.org', 'requests correct registry')
          return { token: 'thisisnotarealtoken' }
        },
      },
    },
    log: {
      gauge: {
        show: name => {
          t.equal(name, 'token', 'starts a gauge')
        },
      },
      newItem: (action, len) => {
        t.equal(action, 'removing tokens')
        t.equal(len, 0)
        return {
          info: (name, progress) => {
            t.equal(name, 'token')
            t.equal(progress, 'getting existing list')
          },
        }
      },
    },
    profile: {
      listTokens: () => Promise.resolve([{ key: 'abcd1234' }, { key: 'efgh5678' }]),
      removeToken: key => {
        // this will run twice
        t.ok(['abcd1234', 'efgh5678'].includes(key), 'deletes the correct token')
      },
    },
    output: spec => {
      t.equal(spec, 'Removed 2 tokens')
    },
  })

  t.teardown(reset)

  await token.exec(['revoke', 'abcd', 'efgh'])
})

t.test('token revoke json output', async t => {
  t.plan(9)

  const [token, reset] = tokenWithMocks({
    npm: {
      flatOptions: { registry: 'https://registry.npmjs.org', json: true },
      config: {
        getCredentialsByURI: uri => {
          t.equal(uri, 'https://registry.npmjs.org', 'requests correct registry')
          return { token: 'thisisnotarealtoken' }
        },
      },
    },
    log: {
      gauge: {
        show: name => {
          t.equal(name, 'token', 'starts a gauge')
        },
      },
      newItem: (action, len) => {
        t.equal(action, 'removing tokens')
        t.equal(len, 0)
        return {
          info: (name, progress) => {
            t.equal(name, 'token')
            t.equal(progress, 'getting existing list')
          },
        }
      },
    },
    profile: {
      listTokens: () => Promise.resolve([{ key: 'abcd1234' }]),
      removeToken: key => {
        t.equal(key, 'abcd1234', 'deletes the correct token')
      },
    },
    output: spec => {
      t.type(spec, 'string', 'is given a string')
      const parsed = JSON.parse(spec)
      t.same(parsed, ['abcd1234'], 'logs the token as json')
    },
  })

  t.teardown(reset)

  await token.exec(['delete', 'abcd'])
})

t.test('token revoke parseable output', async t => {
  t.plan(8)

  const [token, reset] = tokenWithMocks({
    npm: {
      flatOptions: { registry: 'https://registry.npmjs.org', parseable: true },
      config: {
        getCredentialsByURI: uri => {
          t.equal(uri, 'https://registry.npmjs.org', 'requests correct registry')
          return { token: 'thisisnotarealtoken' }
        },
      },
    },
    log: {
      gauge: {
        show: name => {
          t.equal(name, 'token', 'starts a gauge')
        },
      },
      newItem: (action, len) => {
        t.equal(action, 'removing tokens')
        t.equal(len, 0)
        return {
          info: (name, progress) => {
            t.equal(name, 'token')
            t.equal(progress, 'getting existing list')
          },
        }
      },
    },
    profile: {
      listTokens: () => Promise.resolve([{ key: 'abcd1234' }]),
      removeToken: key => {
        t.equal(key, 'abcd1234', 'deletes the correct token')
      },
    },
    output: spec => {
      t.equal(spec, 'abcd1234', 'logs the token as a string')
    },
  })

  t.teardown(reset)

  await token.exec(['remove', 'abcd'])
})

t.test('token revoke by token', async t => {
  t.plan(8)

  const [token, reset] = tokenWithMocks({
    npm: {
      flatOptions: { registry: 'https://registry.npmjs.org' },
      config: {
        getCredentialsByURI: uri => {
          t.equal(uri, 'https://registry.npmjs.org', 'requests correct registry')
          return { token: 'thisisnotarealtoken' }
        },
      },
    },
    log: {
      gauge: {
        show: name => {
          t.equal(name, 'token', 'starts a gauge')
        },
      },
      newItem: (action, len) => {
        t.equal(action, 'removing tokens')
        t.equal(len, 0)
        return {
          info: (name, progress) => {
            t.equal(name, 'token')
            t.equal(progress, 'getting existing list')
          },
        }
      },
    },
    profile: {
      listTokens: () => Promise.resolve([{ key: 'abcd1234', token: 'efgh5678' }]),
      removeToken: key => {
        t.equal(key, 'efgh5678', 'passes through user input')
      },
    },
    output: spec => {
      t.equal(spec, 'Removed 1 token')
    },
  })

  t.teardown(reset)

  await token.exec(['rm', 'efgh5678'])
})

t.test('token revoke requires an id', async t => {
  t.plan(2)

  const [token, reset] = tokenWithMocks({
    log: {
      gauge: {
        show: name => {
          t.equal(name, 'token')
        },
      },
    },
  })

  t.teardown(reset)

  await t.rejects(token.exec(['rm']), /`<tokenKey>` argument is required/)
})

t.test('token revoke ambiguous id errors', async t => {
  t.plan(7)

  const [token, reset] = tokenWithMocks({
    npm: {
      flatOptions: { registry: 'https://registry.npmjs.org' },
      config: {
        getCredentialsByURI: uri => {
          t.equal(uri, 'https://registry.npmjs.org', 'requests correct registry')
          return { token: 'thisisnotarealtoken' }
        },
      },
    },
    log: {
      gauge: {
        show: name => {
          t.equal(name, 'token', 'starts a gauge')
        },
      },
      newItem: (action, len) => {
        t.equal(action, 'removing tokens')
        t.equal(len, 0)
        return {
          info: (name, progress) => {
            t.equal(name, 'token')
            t.equal(progress, 'getting existing list')
          },
        }
      },
    },
    profile: {
      listTokens: () => Promise.resolve([{ key: 'abcd1234' }, { key: 'abcd5678' }]),
    },
  })

  t.teardown(reset)

  await t.rejects(token.exec(['rm', 'abcd']), /Token ID "abcd" was ambiguous/)
})

t.test('token revoke unknown id errors', async t => {
  t.plan(7)

  const [token, reset] = tokenWithMocks({
    npm: {
      flatOptions: { registry: 'https://registry.npmjs.org' },
      config: {
        getCredentialsByURI: uri => {
          t.equal(uri, 'https://registry.npmjs.org', 'requests correct registry')
          return { token: 'thisisnotarealtoken' }
        },
      },
    },
    log: {
      gauge: {
        show: name => {
          t.equal(name, 'token', 'starts a gauge')
        },
      },
      newItem: (action, len) => {
        t.equal(action, 'removing tokens')
        t.equal(len, 0)
        return {
          info: (name, progress) => {
            t.equal(name, 'token')
            t.equal(progress, 'getting existing list')
          },
        }
      },
    },
    profile: {
      listTokens: () => Promise.resolve([{ key: 'abcd1234' }]),
    },
  })

  t.teardown(reset)

  await t.rejects(token.exec(['rm', 'efgh']), /Unknown token id or value "efgh"./)
})

t.test('token create', async t => {
  t.plan(14)

  const now = new Date().toISOString()
  const password = 'thisisnotreallyapassword'

  const [token, reset] = tokenWithMocks({
    npm: {
      flatOptions: {
        registry: 'https://registry.npmjs.org',
        cidr: ['10.0.0.0/8', '192.168.1.0/24'],
      },
      config: {
        getCredentialsByURI: uri => {
          t.equal(uri, 'https://registry.npmjs.org', 'requests correct registry')
          return { token: 'thisisnotarealtoken' }
        },
      },
    },
    log: {
      gauge: {
        show: name => {
          t.equal(name, 'token', 'starts a gauge')
        },
      },
      info: (name, message) => {
        t.equal(name, 'token')
        t.equal(message, 'creating')
      },
    },
    readUserInfo: {
      password: () => Promise.resolve(password),
    },
    profile: {
      createToken: (pw, readonly, cidr) => {
        t.equal(pw, password)
        t.equal(readonly, undefined)
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
    output: spec => {
      const lines = spec.split(/\r?\n/)
      t.match(lines[1], 'token')
      t.match(lines[1], 'efgh5678', 'prints the whole token')
      t.match(lines[3], 'created')
      t.match(lines[3], now, 'prints the correct timestamp')
      t.match(lines[5], 'readonly')
      t.match(lines[5], 'false', 'prints the readonly flag')
      t.match(lines[7], 'cidr_whitelist')
    },
  })

  t.teardown(reset)

  await token.exec(['create'])
})

t.test('token create json output', async t => {
  t.plan(9)

  const now = new Date().toISOString()
  const password = 'thisisnotreallyapassword'

  const [token, reset] = tokenWithMocks({
    npm: {
      flatOptions: { registry: 'https://registry.npmjs.org', json: true },
      config: {
        getCredentialsByURI: uri => {
          t.equal(uri, 'https://registry.npmjs.org', 'requests correct registry')
          return { token: 'thisisnotarealtoken' }
        },
      },
    },
    log: {
      gauge: {
        show: name => {
          t.equal(name, 'token', 'starts a gauge')
        },
      },
      info: (name, message) => {
        t.equal(name, 'token')
        t.equal(message, 'creating')
      },
    },
    readUserInfo: {
      password: () => Promise.resolve(password),
    },
    profile: {
      createToken: (pw, readonly, cidr) => {
        t.equal(pw, password)
        t.equal(readonly, undefined)
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

  t.teardown(reset)

  await token.exec(['create'])
})

t.test('token create parseable output', async t => {
  t.plan(11)

  const now = new Date().toISOString()
  const password = 'thisisnotreallyapassword'

  let callCount = 0
  const [token, reset] = tokenWithMocks({
    npm: {
      flatOptions: { registry: 'https://registry.npmjs.org', parseable: true },
      config: {
        getCredentialsByURI: uri => {
          t.equal(uri, 'https://registry.npmjs.org', 'requests correct registry')
          return { token: 'thisisnotarealtoken' }
        },
      },
    },
    log: {
      gauge: {
        show: name => {
          t.equal(name, 'token', 'starts a gauge')
        },
      },
      info: (name, message) => {
        t.equal(name, 'token')
        t.equal(message, 'creating')
      },
    },
    readUserInfo: {
      password: () => Promise.resolve(password),
    },
    profile: {
      createToken: (pw, readonly, cidr) => {
        t.equal(pw, password)
        t.equal(readonly, undefined)
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
      ++callCount
      if (callCount === 1) {
        t.match(spec, 'token\tefgh5678', 'prints the token')
      } else if (callCount === 2) {
        t.match(spec, `created\t${now}`, 'prints the created timestamp')
      } else if (callCount === 3) {
        t.match(spec, 'readonly\tfalse', 'prints the readonly flag')
      } else {
        t.match(spec, 'cidr_whitelist\t', 'prints the cidr whitelist')
      }
    },
  })

  t.teardown(reset)

  await token.exec(['create'])
})

t.test('token create ipv6 cidr', async t => {
  t.plan(3)

  const password = 'thisisnotreallyapassword'

  const [token, reset] = tokenWithMocks({
    npm: {
      flatOptions: { registry: 'https://registry.npmjs.org', cidr: '::1/128' },
      config: {
        getCredentialsByURI: uri => {
          t.equal(uri, 'https://registry.npmjs.org', 'requests correct registry')
          return { token: 'thisisnotarealtoken' }
        },
      },
    },
    log: {
      gauge: {
        show: name => {
          t.equal(name, 'token', 'starts a gauge')
        },
      },
    },
    readUserInfo: {
      password: () => Promise.resolve(password),
    },
  })

  t.teardown(reset)

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
  t.plan(3)

  const password = 'thisisnotreallyapassword'

  const [token, reset] = tokenWithMocks({
    npm: {
      flatOptions: { registry: 'https://registry.npmjs.org', cidr: 'apple/cider' },
      config: {
        getCredentialsByURI: uri => {
          t.equal(uri, 'https://registry.npmjs.org', 'requests correct registry')
          return { token: 'thisisnotarealtoken' }
        },
      },
    },
    log: {
      gauge: {
        show: name => {
          t.equal(name, 'token', 'starts a gauge')
        },
      },
    },
    readUserInfo: {
      password: () => Promise.resolve(password),
    },
  })

  t.teardown(reset)

  await t.rejects(
    token.exec(['create']),
    { code: 'EINVALIDCIDR', message: /CIDR whitelist contains invalid CIDR entry: apple\/cider/ },
    'returns correct error'
  )
})

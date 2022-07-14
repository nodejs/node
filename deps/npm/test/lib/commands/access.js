const t = require('tap')

const { load: loadMockNpm } = require('../../fixtures/mock-npm.js')
const MockRegistry = require('../../fixtures/mock-registry.js')

const auth = { '//registry.npmjs.org/:_authToken': 'test-auth-token' }

t.test('completion', async t => {
  const { npm } = await loadMockNpm(t)
  const access = await npm.cmd('access')
  const testComp = (argv, expect) => {
    const res = access.completion({ conf: { argv: { remain: argv } } })
    t.resolves(res, expect, argv.join(' '))
  }

  testComp(['npm', 'access'], [
    'public', 'restricted', 'grant', 'revoke', 'ls-packages',
    'ls-collaborators', 'edit', '2fa-required', '2fa-not-required',
  ])
  testComp(['npm', 'access', 'grant'], ['read-only', 'read-write'])
  testComp(['npm', 'access', 'grant', 'read-only'], [])
  testComp(['npm', 'access', 'public'], [])
  testComp(['npm', 'access', 'restricted'], [])
  testComp(['npm', 'access', 'revoke'], [])
  testComp(['npm', 'access', 'ls-packages'], [])
  testComp(['npm', 'access', 'ls-collaborators'], [])
  testComp(['npm', 'access', 'edit'], [])
  testComp(['npm', 'access', '2fa-required'], [])
  testComp(['npm', 'access', '2fa-not-required'], [])
  testComp(['npm', 'access', 'revoke'], [])

  await t.rejects(
    access.completion({ conf: { argv: { remain: ['npm', 'access', 'foobar'] } } }),
    { message: 'foobar not recognized' }
  )
})

t.test('subcommand required', async t => {
  const { npm } = await loadMockNpm(t)
  const access = await npm.cmd('access')
  await t.rejects(
    npm.exec('access', []),
    access.usageError('Subcommand is required.')
  )
})

t.test('unrecognized subcommand', async t => {
  const { npm } = await loadMockNpm(t)
  await t.rejects(
    npm.exec('access', ['blerg']),
    /blerg is not a recognized subcommand/,
    'should throw EUSAGE on missing subcommand'
  )
})

t.test('edit', async t => {
  const { npm } = await loadMockNpm(t)
  await t.rejects(
    npm.exec('access', ['edit', '@scoped/another']),
    /edit subcommand is not implemented yet/,
    'should throw not implemented yet error'
  )
})

t.test('access public on unscoped package', async t => {
  const { npm } = await loadMockNpm(t, {
    prefixDir: {
      'package.json': JSON.stringify({
        name: 'npm-access-public-pkg',
      }),
    },
  })
  await t.rejects(
    npm.exec('access', ['public']),
    /This command is only available for scoped packages/,
    'should throw scoped-restricted error'
  )
})

t.test('access public on scoped package', async t => {
  const name = '@scoped/npm-access-public-pkg'
  const { npm, joinedOutput } = await loadMockNpm(t, {
    config: {
      ...auth,
    },
    prefixDir: {
      'package.json': JSON.stringify({ name }),
    },
  })
  const registry = new MockRegistry({
    tap: t,
    registry: npm.config.get('registry'),
    authorization: 'test-auth-token',
  })
  registry.access({ spec: name, access: 'public' })
  await npm.exec('access', ['public'])
  t.equal(joinedOutput(), '')
})

t.test('access public on missing package.json', async t => {
  const { npm } = await loadMockNpm(t)
  await t.rejects(
    npm.exec('access', ['public']),
    /no package name passed to command and no package.json found/,
    'should throw no package.json found error'
  )
})

t.test('access public on invalid package.json', async t => {
  const { npm } = await loadMockNpm(t, {
    prefixDir: {
      'package.json': '{\n',
      node_modules: {},
    },
  })
  await t.rejects(
    npm.exec('access', ['public']),
    { code: 'EJSONPARSE' },
    'should throw failed to parse package.json'
  )
})

t.test('access restricted on unscoped package', async t => {
  const { npm } = await loadMockNpm(t, {
    prefixDir: {
      'package.json': JSON.stringify({
        name: 'npm-access-restricted-pkg',
      }),
    },
  })
  await t.rejects(
    npm.exec('access', ['public']),
    /This command is only available for scoped packages/,
    'should throw scoped-restricted error'
  )
})

t.test('access restricted on scoped package', async t => {
  const name = '@scoped/npm-access-restricted-pkg'
  const { npm, joinedOutput } = await loadMockNpm(t, {
    config: {
      ...auth,
    },
    prefixDir: {
      'package.json': JSON.stringify({ name }),
    },
  })
  const registry = new MockRegistry({
    tap: t,
    registry: npm.config.get('registry'),
    authorization: 'test-auth-token',
  })
  registry.access({ spec: name, access: 'restricted' })
  await npm.exec('access', ['restricted'])
  t.equal(joinedOutput(), '')
})

t.test('access restricted on missing package.json', async t => {
  const { npm } = await loadMockNpm(t)
  await t.rejects(
    npm.exec('access', ['restricted']),
    /no package name passed to command and no package.json found/,
    'should throw no package.json found error'
  )
})

t.test('access restricted on invalid package.json', async t => {
  const { npm } = await loadMockNpm(t, {
    prefixDir: {
      'package.json': '{\n',
      node_modules: {},
    },
  })
  await t.rejects(
    npm.exec('access', ['restricted']),
    { code: 'EJSONPARSE' },
    'should throw failed to parse package.json'
  )
})

t.test('access grant read-only', async t => {
  const { npm, joinedOutput } = await loadMockNpm(t, {
    config: {
      ...auth,
    },
  })
  const registry = new MockRegistry({
    tap: t,
    registry: npm.config.get('registry'),
    authorization: 'test-auth-token',
  })
  registry.grant({ spec: '@scoped/another', team: 'myorg:myteam', permissions: 'read-only' })
  await npm.exec('access', ['grant', 'read-only', 'myorg:myteam', '@scoped/another'])
  t.equal(joinedOutput(), '')
})

t.test('access grant read-write', async t => {
  const { npm, joinedOutput } = await loadMockNpm(t, {
    config: {
      ...auth,
    },
  })
  const registry = new MockRegistry({
    tap: t,
    registry: npm.config.get('registry'),
    authorization: 'test-auth-token',
  })
  registry.grant({ spec: '@scoped/another', team: 'myorg:myteam', permissions: 'read-write' })
  await npm.exec('access', ['grant', 'read-write', 'myorg:myteam', '@scoped/another'])
  t.equal(joinedOutput(), '')
})

t.test('access grant current cwd', async t => {
  const { npm, joinedOutput } = await loadMockNpm(t, {
    config: {
      ...auth,
    },
    prefixDir: {
      'package.json': JSON.stringify({
        name: 'yargs',
      }),
    },
  })
  const registry = new MockRegistry({
    tap: t,
    registry: npm.config.get('registry'),
    authorization: 'test-auth-token',
  })
  registry.grant({ spec: 'yargs', team: 'myorg:myteam', permissions: 'read-write' })
  await npm.exec('access', ['grant', 'read-write', 'myorg:myteam'])
  t.equal(joinedOutput(), '')
})

t.test('access grant others', async t => {
  const { npm } = await loadMockNpm(t)
  await t.rejects(
    npm.exec('access', [
      'grant',
      'rerere',
      'myorg:myteam',
      '@scoped/another',
    ]),
    /First argument must be either `read-only` or `read-write`./,
    'should throw unrecognized argument error'
  )
})

t.test('access grant missing team args', async t => {
  const { npm } = await loadMockNpm(t)
  await t.rejects(
    npm.exec('access', [
      'grant',
      'read-only',
      undefined,
      '@scoped/another',
    ]),
    /`<scope:team>` argument is required./,
    'should throw missing argument error'
  )
})

t.test('access grant malformed team arg', async t => {
  const { npm } = await loadMockNpm(t)
  await t.rejects(
    npm.exec('access', [
      'grant',
      'read-only',
      'foo',
      '@scoped/another',
    ]),
    /Second argument used incorrect format.\n/,
    'should throw malformed arg error'
  )
})

t.test('access 2fa-required', async t => {
  const { npm, joinedOutput } = await loadMockNpm(t, {
    config: {
      ...auth,
    },
  })
  const registry = new MockRegistry({
    tap: t,
    registry: npm.config.get('registry'),
    authorization: 'test-auth-token',
  })
  registry.access({ spec: '@scope/pkg', publishRequires2fa: true })
  await npm.exec('access', ['2fa-required', '@scope/pkg'])
  t.equal(joinedOutput(), '')
})

t.test('access 2fa-not-required', async t => {
  const { npm, joinedOutput } = await loadMockNpm(t, {
    config: {
      ...auth,
    },
  })
  const registry = new MockRegistry({
    tap: t,
    registry: npm.config.get('registry'),
    authorization: 'test-auth-token',
  })
  registry.access({ spec: '@scope/pkg', publishRequires2fa: false })
  await npm.exec('access', ['2fa-not-required', '@scope/pkg'])
  t.equal(joinedOutput(), '')
})

t.test('access revoke', async t => {
  const { npm, joinedOutput } = await loadMockNpm(t, {
    config: {
      ...auth,
    },
  })
  const registry = new MockRegistry({
    tap: t,
    registry: npm.config.get('registry'),
    authorization: 'test-auth-token',
  })
  registry.revoke({ spec: '@scoped/another', team: 'myorg:myteam' })
  await npm.exec('access', ['revoke', 'myorg:myteam', '@scoped/another'])
  t.equal(joinedOutput(), '')
})

t.test('access revoke missing team args', async t => {
  const { npm } = await loadMockNpm(t)
  await t.rejects(
    npm.exec('access', [
      'revoke',
      undefined,
      '@scoped/another',
    ]),
    /`<scope:team>` argument is required./,
    'should throw missing argument error'
  )
})

t.test('access revoke malformed team arg', async t => {
  const { npm } = await loadMockNpm(t)
  await t.rejects(
    npm.exec('access', [
      'revoke',
      'foo',
      '@scoped/another',
    ]),
    /First argument used incorrect format.\n/,
    'should throw malformed arg error'
  )
})

t.test('npm access ls-packages with no team', async t => {
  const { npm, joinedOutput } = await loadMockNpm(t, {
    config: {
      ...auth,
    },
  })
  const registry = new MockRegistry({
    tap: t,
    registry: npm.config.get('registry'),
    authorization: 'test-auth-token',
  })
  const team = 'foo'
  const packages = { 'test-package': 'read-write' }
  registry.whoami({ username: team })
  registry.lsPackages({ team, packages })
  await npm.exec('access', ['ls-packages'])
  t.match(JSON.parse(joinedOutput()), packages)
})

t.test('access ls-packages on team', async t => {
  const { npm, joinedOutput } = await loadMockNpm(t, {
    config: {
      ...auth,
    },
  })
  const registry = new MockRegistry({
    tap: t,
    registry: npm.config.get('registry'),
    authorization: 'test-auth-token',
  })
  const team = 'myorg:myteam'
  const packages = { 'test-package': 'read-write' }
  registry.lsPackages({ team, packages })
  await npm.exec('access', ['ls-packages', 'myorg:myteam'])
  t.match(JSON.parse(joinedOutput()), packages)
})

t.test('access ls-collaborators on current', async t => {
  const { npm, joinedOutput } = await loadMockNpm(t, {
    config: {
      ...auth,
    },
    prefixDir: {
      'package.json': JSON.stringify({
        name: 'yargs',
      }),
    },
  })
  const registry = new MockRegistry({
    tap: t,
    registry: npm.config.get('registry'),
    authorization: 'test-auth-token',
  })
  const collaborators = { 'test-user': 'read-write' }
  registry.lsCollaborators({ spec: 'yargs', collaborators })
  await npm.exec('access', ['ls-collaborators'])
  t.match(JSON.parse(joinedOutput()), collaborators)
})

t.test('access ls-collaborators on spec', async t => {
  const { npm, joinedOutput } = await loadMockNpm(t, {
    config: {
      ...auth,
    },
  })
  const registry = new MockRegistry({
    tap: t,
    registry: npm.config.get('registry'),
    authorization: 'test-auth-token',
  })
  const collaborators = { 'test-user': 'read-write' }
  registry.lsCollaborators({ spec: 'yargs', collaborators })
  await npm.exec('access', ['ls-collaborators', 'yargs'])
  t.match(JSON.parse(joinedOutput()), collaborators)
})

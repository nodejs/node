const t = require('tap')
const path = require('path')
const fs = require('fs')
const os = require('os')

const { load: loadMockNpm } = require('../../fixtures/mock-npm.js')
const mockGlobals = require('../../fixtures/mock-globals.js')
const MockRegistry = require('../../fixtures/mock-registry.js')
const stream = require('stream')

t.test('usage', async t => {
  const { npm } = await loadMockNpm(t)
  const adduser = await npm.cmd('adduser')
  t.match(adduser.usage, 'adduser', 'usage has command name in it')
})

t.test('simple login', async t => {
  const stdin = new stream.PassThrough()
  stdin.write('test-user\n')
  stdin.write('test-password\n')
  stdin.write('test-email@npmjs.org\n')
  mockGlobals(t, {
    'process.stdin': stdin,
    'process.stdout': new stream.PassThrough(), // to quiet readline
  }, { replace: true })
  const { npm, home } = await loadMockNpm(t, {
    homeDir: {
      // These all get cleaned up by config.setCredentialsByURI
      '.npmrc': [
        '_token=user',
        '_password=user',
        'username=user',
        '_auth=user',
        '_authtoken=user',
        '-authtoken=user',
        '_authToken=user',
        '//registry.npmjs.org/:-authtoken=user',
        '//registry.npmjs.org/:_authToken=user',
        '//registry.npmjs.org/:_authtoken=user',
        '//registry.npmjs.org/:always-auth=user',
        '//registry.npmjs.org/:email=test-email-old@npmjs.org',
      ].join('\n'),
    },
  })
  const registry = new MockRegistry({
    tap: t,
    registry: npm.config.get('registry'),
  })
  registry.couchlogin({
    username: 'test-user',
    password: 'test-password',
    email: 'test-email@npmjs.org',
    token: 'npm_test-token',
  })
  await npm.exec('adduser', [])
  t.same(npm.config.get('email'), 'test-email-old@npmjs.org')
  t.same(npm.config.get('//registry.npmjs.org/:_authToken'), 'npm_test-token')
  const rc = fs.readFileSync(path.join(home, '.npmrc'), 'utf8')
  t.same(
    rc.trim().split(os.EOL), [
      '//registry.npmjs.org/:_authToken=npm_test-token',
      'email=test-email-old@npmjs.org',
    ], 'should only have token and un-nerfed old email'
  )
})

t.test('bad auth type', async t => {
  const { npm } = await loadMockNpm(t, {
    config: {
      'auth-type': 'foo',
    },
  })
  await t.rejects(npm.exec('adduser', []), {
    message: 'no such auth module',
  })
})

t.test('scoped login', async t => {
  const stdin = new stream.PassThrough()
  stdin.write('test-user\n')
  stdin.write('test-password\n')
  stdin.write('test-email@npmjs.org\n')
  mockGlobals(t, {
    'process.stdin': stdin,
    'process.stdout': new stream.PassThrough(), // to quiet readline
  }, { replace: true })
  const { npm, home } = await loadMockNpm(t, {
    config: {
      scope: '@myscope',
    },
  })
  const registry = new MockRegistry({
    tap: t,
    registry: npm.config.get('registry'),
  })
  registry.couchlogin({
    username: 'test-user',
    password: 'test-password',
    email: 'test-email@npmjs.org',
    token: 'npm_test-token',
  })
  await npm.exec('adduser', [])
  t.same(npm.config.get('//registry.npmjs.org/:_authToken'), 'npm_test-token')
  t.same(npm.config.get('@myscope:registry'), 'https://registry.npmjs.org/')
  const rc = fs.readFileSync(path.join(home, '.npmrc'), 'utf8')
  t.same(
    rc.trim().split(os.EOL), [
      '//registry.npmjs.org/:_authToken=npm_test-token',
      '@myscope:registry=https://registry.npmjs.org/',
    ], 'should only have token and scope:registry')
})

t.test('scoped login with valid scoped registry config', async t => {
  const stdin = new stream.PassThrough()
  stdin.write('test-user\n')
  stdin.write('test-password\n')
  stdin.write('test-email@npmjs.org\n')
  mockGlobals(t, {
    'process.stdin': stdin,
    'process.stdout': new stream.PassThrough(), // to quiet readline
  }, { replace: true })
  const { npm, home } = await loadMockNpm(t, {
    homeDir: {
      '.npmrc': '@myscope:registry=https://diff-registry.npmjs.org',
    },
    config: {
      scope: '@myscope',
    },
  })
  const registry = new MockRegistry({
    tap: t,
    registry: 'https://diff-registry.npmjs.org',
  })
  registry.couchlogin({
    username: 'test-user',
    password: 'test-password',
    email: 'test-email@npmjs.org',
    token: 'npm_test-token',
  })
  await npm.exec('adduser', [])
  t.same(npm.config.get('//diff-registry.npmjs.org/:_authToken'), 'npm_test-token')
  t.same(npm.config.get('@myscope:registry'), 'https://diff-registry.npmjs.org')
  const rc = fs.readFileSync(path.join(home, '.npmrc'), 'utf8')
  t.same(rc.trim().split(os.EOL),
    [
      '@myscope:registry=https://diff-registry.npmjs.org',
      '//diff-registry.npmjs.org/:_authToken=npm_test-token',
    ], 'should only have token and scope:registry')
})

t.test('save config failure', async t => {
  const stdin = new stream.PassThrough()
  stdin.write('test-user\n')
  stdin.write('test-password\n')
  stdin.write('test-email@npmjs.org\n')
  mockGlobals(t, {
    'process.stdin': stdin,
    'process.stdout': new stream.PassThrough(), // to quiet readline
  }, { replace: true })
  const { npm } = await loadMockNpm(t, {
    homeDir: {
      '.npmrc': {},
    },
  })
  const registry = new MockRegistry({
    tap: t,
    registry: npm.config.get('registry'),
  })
  registry.couchlogin({
    username: 'test-user',
    password: 'test-password',
    email: 'test-email@npmjs.org',
    token: 'npm_test-token',
  })
  await t.rejects(npm.exec('adduser', []))
})

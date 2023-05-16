const t = require('tap')
const fs = require('fs')
const path = require('path')
const ini = require('ini')

const { load: loadMockNpm } = require('../../fixtures/mock-npm.js')
const mockGlobals = require('../../fixtures/mock-globals.js')
const MockRegistry = require('@npmcli/mock-registry')
const stream = require('stream')

t.test('usage', async t => {
  const { npm } = await loadMockNpm(t)
  const adduser = await npm.cmd('adduser')
  t.match(adduser.usage, 'adduser', 'usage has command name in it')
})

t.test('legacy', async t => {
  t.test('simple adduser', async t => {
    const stdin = new stream.PassThrough()
    stdin.write('test-user\n')
    stdin.write('test-password\n')
    stdin.write('test-email@npmjs.org\n')
    mockGlobals(t, {
      'process.stdin': stdin,
      'process.stdout': new stream.PassThrough(), // to quiet readline
    }, { replace: true })
    const { npm, home } = await loadMockNpm(t, {
      config: { 'auth-type': 'legacy' },
      homeDir: {
        '.npmrc': [
          '//registry.npmjs.org/:_authToken=user',
          '//registry.npmjs.org/:always-auth=user',
          '//registry.npmjs.org/:email=test-email-old@npmjs.org',
        ].join('\n'),
      },
    })
    const registry = new MockRegistry({
      tap: t,
      registry: npm.config.get('registry'),
    })
    registry.couchadduser({
      username: 'test-user',
      password: 'test-password',
      email: 'test-email@npmjs.org',
      token: 'npm_test-token',
    })
    await npm.exec('adduser', [])
    t.same(npm.config.get('email'), 'test-email-old@npmjs.org')
    t.same(npm.config.get('//registry.npmjs.org/:_authToken'), 'npm_test-token')
    const rc = ini.parse(fs.readFileSync(path.join(home, '.npmrc'), 'utf8'))
    t.same(rc, {
      '//registry.npmjs.org/:_authToken': 'npm_test-token',
      email: 'test-email-old@npmjs.org',
    }, 'should only have token and un-nerfed old email')
  })

  t.test('scoped adduser', async t => {
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
        'auth-type': 'legacy',
        scope: '@myscope',
      },
    })
    const registry = new MockRegistry({
      tap: t,
      registry: npm.config.get('registry'),
    })
    registry.couchadduser({
      username: 'test-user',
      password: 'test-password',
      email: 'test-email@npmjs.org',
      token: 'npm_test-token',
    })
    await npm.exec('adduser', [])
    t.same(npm.config.get('//registry.npmjs.org/:_authToken'), 'npm_test-token')
    t.same(npm.config.get('@myscope:registry'), 'https://registry.npmjs.org/')
    const rc = ini.parse(fs.readFileSync(path.join(home, '.npmrc'), 'utf8'))
    t.same(rc, {
      '//registry.npmjs.org/:_authToken': 'npm_test-token',
      '@myscope:registry': 'https://registry.npmjs.org/',
    }, 'should only have token and scope:registry')
  })

  t.test('scoped adduser with valid scoped registry config', async t => {
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
        'auth-type': 'legacy',
        scope: '@myscope',
      },
    })
    const registry = new MockRegistry({
      tap: t,
      registry: 'https://diff-registry.npmjs.org',
    })
    registry.couchadduser({
      username: 'test-user',
      password: 'test-password',
      email: 'test-email@npmjs.org',
      token: 'npm_test-token',
    })
    await npm.exec('adduser', [])
    t.same(npm.config.get('//diff-registry.npmjs.org/:_authToken'), 'npm_test-token')
    t.same(npm.config.get('@myscope:registry'), 'https://diff-registry.npmjs.org')
    const rc = ini.parse(fs.readFileSync(path.join(home, '.npmrc'), 'utf8'))
    t.same(rc, {
      '@myscope:registry': 'https://diff-registry.npmjs.org',
      '//diff-registry.npmjs.org/:_authToken': 'npm_test-token',
    }, 'should only have token and scope:registry')
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
      config: { 'auth-type': 'legacy' },
      homeDir: {
        '.npmrc': {},
      },
    })
    const registry = new MockRegistry({
      tap: t,
      registry: npm.config.get('registry'),
    })
    registry.couchadduser({
      username: 'test-user',
      password: 'test-password',
      email: 'test-email@npmjs.org',
      token: 'npm_test-token',
    })
    await t.rejects(npm.exec('adduser', []))
  })
  t.end()
})

t.test('web', t => {
  t.test('basic adduser', async t => {
    const { npm, home } = await loadMockNpm(t, {
      config: { 'auth-type': 'web' },
    })
    const registry = new MockRegistry({
      tap: t,
      registry: npm.config.get('registry'),
    })
    registry.webadduser({ token: 'npm_test-token' })
    await npm.exec('adduser', [])
    t.same(npm.config.get('//registry.npmjs.org/:_authToken'), 'npm_test-token')
    const rc = ini.parse(fs.readFileSync(path.join(home, '.npmrc'), 'utf8'))
    t.same(rc, {
      '//registry.npmjs.org/:_authToken': 'npm_test-token',
    })
  })

  t.test('server error', async t => {
    const { npm } = await loadMockNpm(t, {
      config: { 'auth-type': 'web' },
    })
    const registry = new MockRegistry({
      tap: t,
      registry: npm.config.get('registry'),
    })
    registry.nock.post(registry.fullPath('/-/v1/login'))
      .reply(503, {})
    await t.rejects(
      npm.exec('adduser', []),
      { message: /503/ }
    )
  })

  t.test('fallback', async t => {
    const stdin = new stream.PassThrough()
    stdin.write('test-user\n')
    stdin.write('test-password\n')
    stdin.write('test-email@npmjs.org\n')
    mockGlobals(t, {
      'process.stdin': stdin,
      'process.stdout': new stream.PassThrough(), // to quiet readline
    }, { replace: true })
    const { npm } = await loadMockNpm(t, {
      config: { 'auth-type': 'web' },
    })
    const registry = new MockRegistry({
      tap: t,
      registry: npm.config.get('registry'),
    })
    registry.nock.post(registry.fullPath('/-/v1/login'))
      .reply(404, {})
    registry.couchadduser({
      username: 'test-user',
      password: 'test-password',
      email: 'test-email@npmjs.org',
      token: 'npm_test-token',
    })
    await npm.exec('adduser', [])
    t.same(npm.config.get('//registry.npmjs.org/:_authToken'), 'npm_test-token')
  })
  t.end()
})

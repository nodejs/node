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
  const login = await npm.cmd('login')
  t.match(login.usage, 'login', 'usage has command name in it')
})

t.test('legacy', t => {
  t.test('basic login', async t => {
    const stdin = new stream.PassThrough()
    stdin.write('test-user\n')
    stdin.write('test-password\n')
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
    registry.couchlogin({
      username: 'test-user',
      password: 'test-password',
      token: 'npm_test-token',
    })
    await npm.exec('login', [])
    t.same(npm.config.get('//registry.npmjs.org/:_authToken'), 'npm_test-token')
    const rc = ini.parse(fs.readFileSync(path.join(home, '.npmrc'), 'utf8'))
    t.same(rc, {
      '//registry.npmjs.org/:_authToken': 'npm_test-token',
      email: 'test-email-old@npmjs.org',
    }, 'should only have token and un-nerfed old email')
  })

  t.test('scoped login default registry', async t => {
    const stdin = new stream.PassThrough()
    stdin.write('test-user\n')
    stdin.write('test-password\n')
    mockGlobals(t, {
      'process.stdin': stdin,
      'process.stdout': new stream.PassThrough(), // to quiet readline
    }, { replace: true })
    const { npm, home } = await loadMockNpm(t, {
      config: {
        'auth-type': 'legacy',
        scope: '@npmcli',
      },
    })
    const registry = new MockRegistry({
      tap: t,
      registry: npm.config.get('registry'),
    })
    registry.couchlogin({
      username: 'test-user',
      password: 'test-password',
      token: 'npm_test-token',
    })
    await npm.exec('login', [])
    t.same(npm.config.get('//registry.npmjs.org/:_authToken'), 'npm_test-token')
    t.same(npm.config.get('@npmcli:registry'), 'https://registry.npmjs.org/')
    const rc = ini.parse(fs.readFileSync(path.join(home, '.npmrc'), 'utf8'))
    t.same(rc, {
      '//registry.npmjs.org/:_authToken': 'npm_test-token',
      '@npmcli:registry': 'https://registry.npmjs.org/',
    }, 'should only have token and scope:registry')
  })

  t.test('scoped login scoped registry', async t => {
    const stdin = new stream.PassThrough()
    stdin.write('test-user\n')
    stdin.write('test-password\n')
    mockGlobals(t, {
      'process.stdin': stdin,
      'process.stdout': new stream.PassThrough(), // to quiet readline
    }, { replace: true })
    const { npm, home } = await loadMockNpm(t, {
      config: {
        'auth-type': 'legacy',
        scope: '@npmcli',
      },
      homeDir: {
        '.npmrc': '@npmcli:registry=https://diff-registry.npmjs.org',
      },
    })
    const registry = new MockRegistry({
      tap: t,
      registry: 'https://diff-registry.npmjs.org',
    })
    registry.couchlogin({
      username: 'test-user',
      password: 'test-password',
      token: 'npm_test-token',
    })
    await npm.exec('login', [])
    t.same(npm.config.get('//diff-registry.npmjs.org/:_authToken'), 'npm_test-token')
    t.same(npm.config.get('@npmcli:registry'), 'https://diff-registry.npmjs.org')
    const rc = ini.parse(fs.readFileSync(path.join(home, '.npmrc'), 'utf8'))
    t.same(rc, {
      '@npmcli:registry': 'https://diff-registry.npmjs.org',
      '//diff-registry.npmjs.org/:_authToken': 'npm_test-token',
    }, 'should only have token and scope:registry')
  })
  t.end()
})

t.test('web', t => {
  t.test('basic login', async t => {
    const { npm, home } = await loadMockNpm(t, {
      config: { 'auth-type': 'web' },
    })
    const registry = new MockRegistry({
      tap: t,
      registry: npm.config.get('registry'),
    })
    registry.weblogin({ token: 'npm_test-token' })
    await npm.exec('login', [])
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
      npm.exec('login', []),
      { message: /503/ }
    )
  })
  t.test('fallback', async t => {
    const stdin = new stream.PassThrough()
    stdin.write('test-user\n')
    stdin.write('test-password\n')
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
    registry.couchlogin({
      username: 'test-user',
      password: 'test-password',
      token: 'npm_test-token',
    })
    await npm.exec('login', [])
    t.same(npm.config.get('//registry.npmjs.org/:_authToken'), 'npm_test-token')
  })
  t.end()
})

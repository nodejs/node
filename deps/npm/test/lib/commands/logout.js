const t = require('tap')
const fs = require('fs/promises')
const npmFetch = require('npm-registry-fetch')
const mockNpm = require('../../fixtures/mock-npm')
const { join } = require('path')

const mockLogout = async (t, { userRc = [], ...npmOpts } = {}) => {
  let result = null

  const mock = await mockNpm(t, {
    command: 'logout',
    mocks: {
      // XXX: refactor to use mock registry
      'npm-registry-fetch': Object.assign(async (url, opts) => {
        result = { url, opts }
      }, npmFetch),
    },
    ...npmOpts,
    homeDir: {
      '.npmrc': userRc.join('\n'),
    },
  })

  return {
    ...mock,
    result: () => result,
    // get only the message portion of the verbose log from the command
    logMsg: () => mock.logs.verbose.find(l => l[0] === 'logout')[1],
    userRc: () => fs.readFile(join(mock.home, '.npmrc'), 'utf-8').then(r => r.trim()),
  }
}

t.test('token logout', async t => {
  const { logout, logMsg, result, userRc } = await mockLogout(t, {
    userRc: [
      '//registry.npmjs.org/:_authToken=@foo/',
      'other-config=true',
    ],
  })

  await logout.exec([])

  t.equal(
    logMsg(),
    'clearing token for https://registry.npmjs.org/',
    'should log message with correct registry'
  )

  t.match(
    result(),
    {
      url: '/-/user/token/%40foo%2F',
      opts: {
        registry: 'https://registry.npmjs.org/',
        scope: '',
        '//registry.npmjs.org/:_authToken': '@foo/',
        method: 'DELETE',
        ignoreBody: true,
      },
    },
    'should call npm-registry-fetch with expected values'
  )

  t.equal(await userRc(), 'other-config=true')
})

t.test('token scoped logout', async t => {
  const { logout, logMsg, result, userRc } = await mockLogout(t, {
    config: { scope: '@myscope' },
    userRc: [
      '//diff-registry.npmjs.com/:_authToken=@bar/',
      '//registry.npmjs.org/:_authToken=@foo/',
      '@myscope:registry=https://diff-registry.npmjs.com/',
    ],
  })

  await logout.exec([])

  t.equal(
    logMsg(),
    'clearing token for https://diff-registry.npmjs.com/',
    'should log message with correct registry'
  )

  t.match(
    result(),
    {
      url: '/-/user/token/%40bar%2F',
      opts: {
        registry: 'https://registry.npmjs.org/',
        '@myscope:registry': 'https://diff-registry.npmjs.com/',
        scope: '@myscope',
        '//registry.npmjs.org/:_authToken': '@foo/', // <- removed by npm-registry-fetch
        '//diff-registry.npmjs.com/:_authToken': '@bar/',
        method: 'DELETE',
        ignoreBody: true,
      },
    },
    'should call npm-registry-fetch with expected values'
  )

  t.equal(await userRc(), '//registry.npmjs.org/:_authToken=@foo/')
})

t.test('user/pass logout', async t => {
  const { logout, logMsg, userRc } = await mockLogout(t, {
    userRc: [
      '//registry.npmjs.org/:username=foo',
      '//registry.npmjs.org/:_password=bar',
      'other-config=true',
    ],
  })

  await logout.exec([])

  t.equal(
    logMsg(),
    'clearing user credentials for https://registry.npmjs.org/',
    'should log message with correct registry'
  )

  t.equal(await userRc(), 'other-config=true')
})

t.test('missing credentials', async t => {
  const { logout } = await mockLogout(t)

  await t.rejects(
    logout.exec([]),
    {
      code: 'ENEEDAUTH',
      message: /not logged in to https:\/\/registry.npmjs.org\/, so can't log out!/,
    },
    'should throw with expected error code'
  )
})

t.test('ignore invalid scoped registry config', async t => {
  const { logout, logMsg, result, userRc } = await mockLogout(t, {
    config: { scope: '@myscope' },
    userRc: [
      '//registry.npmjs.org/:_authToken=@foo/',
      'other-config=true',
    ],
  })

  await logout.exec([])

  t.equal(
    logMsg(),
    'clearing token for https://registry.npmjs.org/',
    'should log message with correct registry'
  )

  t.match(
    result(),
    {
      url: '/-/user/token/%40foo%2F',
      opts: {
        '//registry.npmjs.org/:_authToken': '@foo/',
        registry: 'https://registry.npmjs.org/',
        method: 'DELETE',
        ignoreBody: true,
      },
    },
    'should call npm-registry-fetch with expected values'
  )

  t.equal(await userRc(), 'other-config=true')
})

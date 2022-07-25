const t = require('tap')

let log = ''

const _flatOptions = {
  ssoType: 'oauth',
}
const token = '24528a24f240'
const SSO_URL = 'https://registry.npmjs.org/{SSO_URL}'
const profile = {}
const npmFetch = {}
const sso = t.mock('../../../lib/auth/sso.js', {
  'proc-log': {
    info: (...msgs) => {
      log += msgs.join(' ') + '\n'
    },
  },
  'npm-profile': profile,
  'npm-registry-fetch': npmFetch,
  '../../../lib/utils/open-url.js': async (npm, url, msg) => {
    if (!url) {
      throw Object.assign(
        new Error('failed open url'),
        { code: 'ERROR' }
      )
    }
  },
})

const npm = {
  flatOptions: _flatOptions,
}

t.test('empty login', async (t) => {
  _flatOptions.ssoType = false

  await t.rejects(
    sso(npm, {}),
    { message: 'Missing option: sso-type' },
    'should throw if no sso-type defined in flatOptions'
  )

  _flatOptions.ssoType = 'oauth'
  log = ''
})

t.test('simple login', async (t) => {
  t.plan(6)

  profile.loginCouch = (username, password, opts) => {
    t.equal(username, 'npm_oauth_auth_dummy_user', 'should use dummy user')
    t.equal(password, 'placeholder', 'should use dummy password')
    t.same(
      opts,
      {
        creds: {},
        registry: 'https://registry.npmjs.org/',
        scope: '',
        ssoType: 'oauth',
      },
      'should use dummy password'
    )

    return { token, sso: SSO_URL }
  }
  npmFetch.json = () => Promise.resolve({ username: 'foo' })

  const {
    message,
    newCreds,
  } = await sso(npm, {
    creds: {},
    registry: 'https://registry.npmjs.org/',
    scope: '',
  })

  t.equal(
    message,
    'Logged in as foo on https://registry.npmjs.org/.',
    'should have correct message result'
  )

  t.equal(
    log,
    'adduser Polling for validated SSO session\nadduser Authorized user foo\n',
    'should have correct logged info msg'
  )

  t.same(
    newCreds,
    { token },
    'should return expected resulting credentials'
  )

  log = ''
  delete profile.loginCouch
  delete npmFetch.json
})

t.test('polling retry', async (t) => {
  t.plan(3)

  profile.loginCouch = () => ({ token, sso: SSO_URL })
  npmFetch.json = () => {
    // assert expected values during retry
    npmFetch.json = (url, { registry, forceAuth: { token: expected } }) => {
      t.equal(
        url,
        '/-/whoami',
        'should reach for expected endpoint'
      )

      t.equal(
        registry,
        'https://registry.npmjs.org/',
        'should use expected registry value'
      )

      t.equal(
        expected,
        token,
        'should use expected token retrieved from initial loginCouch'
      )

      return Promise.resolve({ username: 'foo' })
    }

    // initial fetch returns retry code
    return Promise.reject(Object.assign(
      new Error('nothing yet'),
      { code: 'E401' }
    ))
  }

  await sso(npm, {
    creds: {},
    registry: 'https://registry.npmjs.org/',
    scope: '',
  })

  log = ''
  delete profile.loginCouch
  delete npmFetch.json
})

t.test('polling error', async (t) => {
  profile.loginCouch = () => ({ token, sso: SSO_URL })
  npmFetch.json = () => Promise.reject(Object.assign(
    new Error('unknown error'),
    { code: 'ERROR' }
  ))

  await t.rejects(
    sso(npm, {
      creds: {},
      registry: 'https://registry.npmjs.org/',
      scope: '',
    }),
    { message: 'unknown error', code: 'ERROR' },
    'should throw unknown error'
  )

  log = ''
  delete profile.loginCouch
  delete npmFetch.json
})

t.test('no token retrieved from loginCouch', async (t) => {
  profile.loginCouch = () => ({})

  await t.rejects(
    sso(npm, {
      creds: {},
      registry: 'https://registry.npmjs.org/',
      scope: '',
    }),
    { message: 'no SSO token returned' },
    'should throw no SSO token returned error'
  )

  log = ''
  delete profile.loginCouch
})

t.test('no sso url retrieved from loginCouch', async (t) => {
  profile.loginCouch = () => Promise.resolve({ token })

  await t.rejects(
    sso(npm, {
      creds: {},
      registry: 'https://registry.npmjs.org/',
      scope: '',
    }),
    { message: 'no SSO URL returned by services' },
    'should throw no SSO url returned error'
  )

  log = ''
  delete profile.loginCouch
})

t.test('scoped login', async (t) => {
  profile.loginCouch = () => ({ token, sso: SSO_URL })
  npmFetch.json = () => Promise.resolve({ username: 'foo' })

  const {
    message,
    newCreds,
  } = await sso(npm, {
    creds: {},
    registry: 'https://diff-registry.npmjs.org/',
    scope: 'myscope',
  })

  t.equal(
    message,
    'Logged in as foo to scope myscope on https://diff-registry.npmjs.org/.',
    'should have correct message result'
  )

  t.equal(
    log,
    'adduser Polling for validated SSO session\nadduser Authorized user foo\n',
    'should have correct logged info msg'
  )

  t.same(
    newCreds,
    { token },
    'should return expected resulting credentials'
  )

  log = ''
  delete profile.loginCouch
  delete npmFetch.json
})

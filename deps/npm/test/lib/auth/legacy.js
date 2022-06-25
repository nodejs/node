const t = require('tap')

let log = ''

const token = '24528a24f240'
const profile = {}
const read = {}
const legacy = t.mock('../../../lib/auth/legacy.js', {
  'proc-log': {
    info: (...msgs) => {
      log += msgs.join(' ')
    },
  },
  'npm-profile': profile,
  '../../../lib/utils/open-url-prompt.js': (_npm, url) => {
    if (!url) {
      throw Object.assign(new Error('failed open url'), { code: 'ERROR' })
    }
  },
  '../../../lib/utils/read-user-info.js': read,
})

const npm = {
  config: {
    get: () => null,
  },
}

t.test('login using username/password with token result', async (t) => {
  profile.login = () => {
    return { token }
  }

  const {
    message,
    newCreds,
  } = await legacy(npm, {
    creds: {
      username: 'u',
      password: 'p',
      email: 'u@npmjs.org',
      alwaysAuth: false,
    },
    registry: 'https://registry.npmjs.org/',
    scope: '',
  })

  t.equal(
    message,
    'Logged in as u on https://registry.npmjs.org/.',
    'should have correct message result'
  )

  t.equal(
    log,
    'login Authorized user u',
    'should have correct message result'
  )

  t.same(
    newCreds,
    { token },
    'should return expected obj from profile.login'
  )

  log = ''
  delete profile.login
})

t.test('login using username/password with user info result', async (t) => {
  profile.login = () => {
    return null
  }

  const {
    message,
    newCreds,
  } = await legacy(npm, {
    creds: {
      username: 'u',
      password: 'p',
      email: 'u@npmjs.org',
      alwaysAuth: false,
    },
    registry: 'https://registry.npmjs.org/',
    scope: '',
  })

  t.equal(
    message,
    'Logged in as u on https://registry.npmjs.org/.',
    'should have correct message result'
  )

  t.same(
    newCreds,
    {
      username: 'u',
      password: 'p',
      email: 'u@npmjs.org',
      alwaysAuth: false,
    },
    'should return used credentials'
  )

  log = ''
  delete profile.login
})

t.test('login otp requested', async (t) => {
  t.plan(5)

  profile.login = () => Promise.reject(Object.assign(
    new Error('needs otp'),
    { code: 'EOTP' }
  ))
  profile.loginCouch = (username, password, { otp }) => {
    t.equal(username, 'u', 'should use provided username to loginCouch')
    t.equal(password, 'p', 'should use provided password to loginCouch')
    t.equal(otp, '1234', 'should use provided otp code to loginCouch')

    return { token }
  }
  read.otp = () => Promise.resolve('1234')

  const {
    message,
    newCreds,
  } = await legacy(npm, {
    creds: {
      username: 'u',
      password: 'p',
      email: 'u@npmjs.org',
      alwaysAuth: false,
    },
    registry: 'https://registry.npmjs.org/',
    scope: '',
  })

  t.equal(
    message,
    'Logged in as u on https://registry.npmjs.org/.',
    'should have correct message result'
  )

  t.same(
    newCreds,
    { token },
    'should return token from loginCouch result'
  )

  log = ''
  delete profile.login
  delete profile.loginCouch
  delete read.otp
})

t.test('login missing basic credential info', async (t) => {
  profile.login = () => Promise.reject(Object.assign(
    new Error('missing info'),
    { code: 'ERROR' }
  ))

  await t.rejects(
    legacy(npm, {
      creds: {
        username: 'u',
        password: 'p',
      },
      registry: 'https://registry.npmjs.org/',
      scope: '',
    }),
    { code: 'ERROR' },
    'should throw server response error'
  )

  log = ''
  delete profile.login
})

t.test('create new user when user not found', async (t) => {
  t.plan(6)

  profile.login = () => Promise.reject(Object.assign(
    new Error('User does not exist'),
    { code: 'ERROR' }
  ))
  profile.adduserCouch = (username, email, password) => {
    t.equal(username, 'u', 'should use provided username to adduserCouch')
    t.equal(email, 'u@npmjs.org', 'should use provided email to adduserCouch')
    t.equal(password, 'p', 'should use provided password to adduserCouch')

    return { token }
  }

  const {
    message,
    newCreds,
  } = await legacy(npm, {
    creds: {
      username: 'u',
      password: 'p',
      email: 'u@npmjs.org',
      alwaysAuth: false,
    },
    registry: 'https://registry.npmjs.org/',
    scope: '',
  })

  t.equal(
    message,
    'Logged in as u on https://registry.npmjs.org/.',
    'should have correct message result'
  )

  t.equal(
    log,
    'login Authorized user u',
    'should have correct message result'
  )

  t.same(
    newCreds,
    { token },
    'should return expected obj from profile.login'
  )

  log = ''
  delete profile.adduserCouch
  delete profile.login
})

t.test('prompts for user info if required', async (t) => {
  t.plan(4)

  profile.login = async (opener, prompt, opts) => {
    t.equal(opts.creds.alwaysAuth, true, 'should have refs to creds if any')
    await opener('https://registry.npmjs.org/-/v1/login')
    const creds = await prompt(opts.creds)
    return creds
  }
  read.username = () => Promise.resolve('foo')
  read.password = () => Promise.resolve('pass')
  read.email = () => Promise.resolve('foo@npmjs.org')

  const {
    message,
    newCreds,
  } = await legacy(npm, {
    creds: {
      alwaysAuth: true,
    },
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
    'login Authorized user foo',
    'should have correct message result'
  )

  t.same(
    newCreds,
    {
      username: 'foo',
      password: 'pass',
      email: 'foo@npmjs.org',
      alwaysAuth: true,
    },
    'should return result from profile.login containing prompt info'
  )

  log = ''
  delete profile.login
  delete read.username
  delete read.password
  delete read.email
})

t.test('request otp when creating new user', async (t) => {
  t.plan(3)

  profile.login = () => Promise.reject(Object.assign(
    new Error('User does not exist'),
    { code: 'ERROR' }
  ))
  profile.adduserCouch = () => Promise.reject(Object.assign(
    new Error('needs otp'),
    { code: 'EOTP' }
  ))
  profile.loginCouch = (username, password, { otp }) => {
    t.equal(username, 'u', 'should use provided username to loginCouch')
    t.equal(password, 'p', 'should use provided password to loginCouch')
    t.equal(otp, '1234', 'should now use provided otp code to loginCouch')

    return { token }
  }
  read.otp = () => Promise.resolve('1234')

  await legacy(npm, {
    creds: {
      username: 'u',
      password: 'p',
      email: 'u@npmjs.org',
      alwaysAuth: false,
    },
    registry: 'https://registry.npmjs.org/',
    scope: '',
  })

  log = ''
  delete profile.adduserCouch
  delete profile.login
  delete profile.loginCouch
  delete read.otp
})

t.test('unknown error during user creation', async (t) => {
  profile.login = () => Promise.reject(Object.assign(
    new Error('missing info'),
    { code: 'ERROR' }
  ))
  profile.adduserCouch = () => Promise.reject(Object.assign(
    new Error('unkown error'),
    { code: 'ERROR' }
  ))

  await t.rejects(
    legacy(npm, {
      creds: {
        username: 'u',
        password: 'p',
        email: 'u@npmjs.org',
        alwaysAuth: false,
      },
      registry: 'https://registry.npmjs.org/',
      scope: '',
    }),
    { code: 'ERROR' },
    'should throw unknown error'
  )

  log = ''
  delete profile.adduserCouch
  delete profile.login
})

t.test('open url error', async (t) => {
  profile.login = async (opener, prompt, opts) => {
    await opener()
  }

  await t.rejects(
    legacy(npm, {
      creds: {
        username: 'u',
        password: 'p',
      },
      registry: 'https://registry.npmjs.org/',
      scope: '',
    }),
    { message: 'failed open url', code: 'ERROR' },
    'should throw unknown error'
  )

  log = ''
  delete profile.login
})

t.test('login no credentials provided', async (t) => {
  profile.login = () => ({ token })

  await legacy(npm, {
    creds: {
      username: undefined,
      password: undefined,
      email: undefined,
      alwaysAuth: undefined,
    },
    registry: 'https://registry.npmjs.org/',
    scope: '',
  })

  t.equal(
    log,
    'login Authorized',
    'should have correct message result'
  )

  log = ''
  delete profile.login
})

t.test('scoped login', async (t) => {
  profile.login = () => ({ token })

  const { message } = await legacy(npm, {
    creds: {
      username: 'u',
      password: 'p',
      email: 'u@npmjs.org',
      alwaysAuth: false,
    },
    registry: 'https://diff-registry.npmjs.org/',
    scope: 'myscope',
  })

  t.equal(
    message,
    'Logged in as u to scope myscope on https://diff-registry.npmjs.org/.',
    'should have correct message result'
  )

  t.equal(
    log,
    'login Authorized user u',
    'should have correct message result'
  )

  log = ''
  delete profile.login
})

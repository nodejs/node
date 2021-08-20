const t = require('tap')
const { fake: mockNpm } = require('../fixtures/mock-npm')

let result = ''
const config = {
  otp: '',
  json: false,
  parseable: false,
  registry: 'https://registry.npmjs.org/',
}
const flatOptions = {
  registry: 'https://registry.npmjs.org/',
}
const npm = mockNpm({
  config,
  flatOptions,
  output: (...msg) => {
    result = result ? `${result}\n${msg.join('\n')}` : msg.join('\n')
  },
})
const mocks = {
  ansistyles: { bright: a => a },
  npmlog: {
    gauge: { show () {} },
    info () {},
    notice () {},
    warn () {},
  },
  'npm-profile': {
    async get () {},
    async set () {},
    async createToken () {},
  },
  'qrcode-terminal': { generate: (url, cb) => cb() },
  'cli-table3': class extends Array {
    toString () {
      return this
        .filter(Boolean)
        .map(i => [...Object.entries(i)]
          .map(i => i.join(': ')))
        .join('\n')
    }
  },
  '../../lib/utils/pulse-till-done.js': {
    withPromise: async a => a,
  },
  '../../lib/utils/otplease.js': async (opts, fn) => fn(opts),
  '../../lib/utils/usage.js': () => 'usage instructions',
  '../../lib/utils/read-user-info.js': {
    async password () {},
    async otp () {},
  },
}
const userProfile = {
  tfa: { pending: false, mode: 'auth-and-writes' },
  name: 'foo',
  email: 'foo@github.com',
  email_verified: true,
  created: '2015-02-26T01:26:37.384Z',
  updated: '2020-08-12T16:19:35.326Z',
  cidr_whitelist: null,
  fullname: 'Foo Bar',
  homepage: 'https://github.com',
  freenode: 'foobar',
  twitter: 'https://twitter.com/npmjs',
  github: 'https://github.com/npm',
}

t.afterEach(() => {
  result = ''
  flatOptions.otp = ''
  config.json = false
  config.parseable = false
  config.registry = 'https://registry.npmjs.org/'
})

const Profile = t.mock('../../lib/profile.js', mocks)
const profile = new Profile(npm)

t.test('no args', t => {
  profile.exec([], err => {
    t.match(
      err,
      /usage instructions/,
      'should throw usage instructions'
    )
    t.end()
  })
})

t.test('profile get no args', t => {
  const npmProfile = {
    async get () {
      return userProfile
    },
  }

  const Profile = t.mock('../../lib/profile.js', {
    ...mocks,
    'npm-profile': npmProfile,
  })
  const profile = new Profile(npm)

  t.test('default output', t => {
    profile.exec(['get'], err => {
      if (err)
        throw err

      t.matchSnapshot(
        result,
        'should output table with contents'
      )
      t.end()
    })
  })

  t.test('--json', t => {
    config.json = true

    profile.exec(['get'], err => {
      if (err)
        throw err

      t.same(
        JSON.parse(result),
        userProfile,
        'should output json profile result'
      )
      t.end()
    })
  })

  t.test('--parseable', t => {
    config.parseable = true

    profile.exec(['get'], err => {
      if (err)
        throw err

      t.matchSnapshot(
        result,
        'should output all profile info as parseable result'
      )
      t.end()
    })
  })

  t.test('no tfa enabled', t => {
    const npmProfile = {
      async get () {
        return {
          ...userProfile,
          tfa: null,
        }
      },
    }

    const Profile = t.mock('../../lib/profile.js', {
      ...mocks,
      'npm-profile': npmProfile,
    })
    const profile = new Profile(npm)

    profile.exec(['get'], err => {
      if (err)
        throw err

      t.matchSnapshot(
        result,
        'should output expected profile values'
      )
      t.end()
    })
  })

  t.test('unverified email', t => {
    const npmProfile = {
      async get () {
        return {
          ...userProfile,
          email_verified: false,
        }
      },
    }

    const Profile = t.mock('../../lib/profile.js', {
      ...mocks,
      'npm-profile': npmProfile,
    })
    const profile = new Profile(npm)

    profile.exec(['get'], err => {
      if (err)
        throw err

      t.matchSnapshot(
        result,
        'should output table with contents'
      )
      t.end()
    })
  })

  t.test('profile has cidr_whitelist item', t => {
    const npmProfile = {
      async get () {
        return {
          ...userProfile,
          cidr_whitelist: ['192.168.1.1'],
        }
      },
    }

    const Profile = t.mock('../../lib/profile.js', {
      ...mocks,
      'npm-profile': npmProfile,
    })
    const profile = new Profile(npm)

    profile.exec(['get'], err => {
      if (err)
        throw err

      t.matchSnapshot(
        result,
        'should output table with contents'
      )
      t.end()
    })
  })

  t.end()
})

t.test('profile get <key>', t => {
  const npmProfile = {
    async get () {
      return userProfile
    },
  }

  const Profile = t.mock('../../lib/profile.js', {
    ...mocks,
    'npm-profile': npmProfile,
  })
  const profile = new Profile(npm)

  t.test('default output', t => {
    profile.exec(['get', 'name'], err => {
      if (err)
        throw err

      t.equal(
        result,
        'foo',
        'should output value result'
      )
      t.end()
    })
  })

  t.test('--json', t => {
    config.json = true

    profile.exec(['get', 'name'], err => {
      if (err)
        throw err

      t.same(
        JSON.parse(result),
        userProfile,
        'should output json profile result ignoring args filter'
      )
      t.end()
    })
  })

  t.test('--parseable', t => {
    config.parseable = true

    profile.exec(['get', 'name'], err => {
      if (err)
        throw err

      t.matchSnapshot(
        result,
        'should output parseable result value'
      )
      t.end()
    })
  })

  t.end()
})

t.test('profile get multiple args', t => {
  const npmProfile = {
    async get () {
      return userProfile
    },
  }

  const Profile = t.mock('../../lib/profile.js', {
    ...mocks,
    'npm-profile': npmProfile,
  })
  const profile = new Profile(npm)

  t.test('default output', t => {
    profile.exec(['get', 'name', 'email', 'github'], err => {
      if (err)
        throw err

      t.matchSnapshot(
        result,
        'should output all keys'
      )
      t.end()
    })
  })

  t.test('--json', t => {
    config.json = true

    profile.exec(['get', 'name', 'email', 'github'], err => {
      if (err)
        throw err

      t.same(
        JSON.parse(result),
        userProfile,
        'should output json profile result and ignore args'
      )
      t.end()
    })
  })

  t.test('--parseable', t => {
    config.parseable = true

    profile.exec(['get', 'name', 'email', 'github'], err => {
      if (err)
        throw err

      t.matchSnapshot(
        result,
        'should output parseable profile value results'
      )
      t.end()
    })
  })

  t.test('comma separated', t => {
    profile.exec(['get', 'name,email,github'], err => {
      if (err)
        throw err

      t.matchSnapshot(
        result,
        'should output all keys'
      )
      t.end()
    })
  })

  t.end()
})

t.test('profile set <key> <value>', t => {
  const npmProfile = t => ({
    async get () {
      return userProfile
    },
    async set (newUser, conf) {
      t.match(
        newUser,
        {
          fullname: 'Lorem Ipsum',
        },
        'should set new value to key'
      )
      return {
        ...userProfile,
        ...newUser,
      }
    },
  })

  t.test('no key', t => {
    profile.exec(['set'], err => {
      t.match(
        err,
        /npm profile set <prop> <value>/,
        'should throw proper usage message'
      )
      t.end()
    })
  })

  t.test('no value', t => {
    profile.exec(['set', 'email'], err => {
      t.match(
        err,
        /npm profile set <prop> <value>/,
        'should throw proper usage message'
      )
      t.end()
    })
  })

  t.test('set password', t => {
    profile.exec(['set', 'password', '1234'], err => {
      t.match(
        err,
        /Do not include your current or new passwords on the command line./,
        'should throw an error refusing to set password from args'
      )
      t.end()
    })
  })

  t.test('unwritable key', t => {
    profile.exec(['set', 'name', 'foo'], err => {
      t.match(
        err,
        /"name" is not a property we can set./,
        'should throw the unwritable key error'
      )
      t.end()
    })
  })

  t.test('writable key', t => {
    t.test('default output', t => {
      t.plan(2)

      const Profile = t.mock('../../lib/profile.js', {
        ...mocks,
        'npm-profile': npmProfile(t),
      })
      const profile = new Profile(npm)

      profile.exec(['set', 'fullname', 'Lorem Ipsum'], err => {
        if (err)
          throw err

        t.equal(
          result,
          'Set\nfullname\nto\nLorem Ipsum',
          'should output set key success msg'
        )
      })
    })

    t.test('--json', t => {
      t.plan(2)

      config.json = true

      const Profile = t.mock('../../lib/profile.js', {
        ...mocks,
        'npm-profile': npmProfile(t),
      })
      const profile = new Profile(npm)

      profile.exec(['set', 'fullname', 'Lorem Ipsum'], err => {
        if (err)
          throw err

        t.same(
          JSON.parse(result),
          {
            fullname: 'Lorem Ipsum',
          },
          'should output json set key success msg'
        )
      })
    })

    t.test('--parseable', t => {
      t.plan(2)

      config.parseable = true

      const Profile = t.mock('../../lib/profile.js', {
        ...mocks,
        'npm-profile': npmProfile(t),
      })
      const profile = new Profile(npm)

      profile.exec(['set', 'fullname', 'Lorem Ipsum'], err => {
        if (err)
          throw err

        t.matchSnapshot(
          result,
          'should output parseable set key success msg'
        )
      })
    })

    t.end()
  })

  t.test('write new email', t => {
    t.plan(3)

    const npmProfile = {
      async get () {
        return userProfile
      },
      async set (newUser, conf) {
        t.match(
          newUser,
          {
            email: 'foo@npmjs.com',
          },
          'should set new value to email'
        )
        t.match(
          conf,
          npm.flatOptions,
          'should forward flatOptions config'
        )
        return {
          ...userProfile,
          ...newUser,
        }
      },
    }

    const Profile = t.mock('../../lib/profile.js', {
      ...mocks,
      'npm-profile': npmProfile,
    })
    const profile = new Profile(npm)

    profile.exec(['set', 'email', 'foo@npmjs.com'], err => {
      if (err)
        throw err

      t.equal(
        result,
        'Set\nemail\nto\nfoo@npmjs.com',
        'should output set key success msg'
      )
    })
  })

  t.test('change password', t => {
    t.plan(6)

    const npmProfile = {
      async get () {
        return userProfile
      },
      async set (newUser, conf) {
        t.match(
          newUser,
          {
            password: {
              old: 'currentpassword1234',
              new: 'newpassword1234',
            },
          },
          'should set new password'
        )
        t.match(
          conf,
          npm.flatOptions,
          'should forward flatOptions config'
        )
        return {
          ...userProfile,
        }
      },
    }

    const readUserInfo = {
      async password (label) {
        if (label === 'Current password: ')
          t.ok('should interactively ask for password confirmation')
        else if (label === 'New password: ')
          t.ok('should interactively ask for new password')
        else if (label === '       Again:     ')
          t.ok('should interactively ask for new password confirmation')
        else
          throw new Error('Unexpected label: ' + label)

        return label === 'Current password: '
          ? 'currentpassword1234'
          : 'newpassword1234'
      },
    }

    const Profile = t.mock('../../lib/profile.js', {
      ...mocks,
      'npm-profile': npmProfile,
      '../../lib/utils/read-user-info.js': readUserInfo,
    })
    const profile = new Profile(npm)

    profile.exec(['set', 'password'], err => {
      if (err)
        throw err

      t.equal(
        result,
        'Set\npassword',
        'should output set password success msg'
      )
      t.end()
    })
  })

  t.test('password confirmation mismatch', t => {
    t.plan(3)
    let passwordPromptCount = 0

    const npmProfile = {
      async get () {
        return userProfile
      },
      async set (newUser, conf) {
        return {
          ...userProfile,
        }
      },
    }

    const readUserInfo = {
      async password (label) {
        passwordPromptCount++

        switch (label) {
          case 'Current password: ':
            return 'currentpassword1234'
          case 'New password: ':
            return passwordPromptCount < 3
              ? 'password-that-will-not-be-confirmed'
              : 'newpassword'
          case '       Again:     ':
            return 'newpassword'
          default:
            return 'password1234'
        }
      },
    }

    const npmlog = {
      gauge: {
        show () {},
      },
      warn (title, msg) {
        t.equal(title, 'profile', 'should use expected profile')
        t.equal(
          msg,
          'Passwords do not match, please try again.',
          'should log password mismatch message'
        )
      },
    }

    const Profile = t.mock('../../lib/profile.js', {
      ...mocks,
      npmlog,
      'npm-profile': npmProfile,
      '../../lib/utils/read-user-info.js': readUserInfo,
    })
    const profile = new Profile(npm)

    profile.exec(['set', 'password'], err => {
      if (err)
        throw err

      t.equal(
        result,
        'Set\npassword',
        'should output set password success msg'
      )
      t.end()
    })
  })

  t.end()
})

t.test('enable-2fa', t => {
  t.test('invalid args', t => {
    profile.exec(['enable-2fa', 'foo', 'bar'], err => {
      t.match(
        err,
        /npm profile enable-2fa \[auth-and-writes|auth-only\]/,
        'should throw usage error'
      )
      t.end()
    })
  })

  t.test('invalid two factor auth mode', t => {
    profile.exec(['enable-2fa', 'foo'], err => {
      t.match(
        err,
        /Invalid two-factor authentication mode "foo"/,
        'should throw invalid auth mode error'
      )
      t.end()
    })
  })

  t.test('no support for --json output', t => {
    config.json = true

    profile.exec(['enable-2fa', 'auth-only'], err => {
      t.match(
        err.message,
        'Enabling two-factor authentication is an interactive ' +
        'operation and JSON output mode is not available',
        'should throw no support msg'
      )
      t.end()
    })
  })

  t.test('no support for --parseable output', t => {
    config.parseable = true

    profile.exec(['enable-2fa', 'auth-only'], err => {
      t.match(
        err.message,
        'Enabling two-factor authentication is an interactive ' +
        'operation and parseable output mode is not available',
        'should throw no support msg'
      )
      t.end()
    })
  })

  t.test('no bearer tokens returned by registry', t => {
    t.plan(3)

    // mock legacy basic auth style
    npm.config.getCredentialsByURI = reg => {
      t.equal(reg, flatOptions.registry, 'should use expected registry')
      return { auth: Buffer.from('foo:bar').toString('base64') }
    }

    const npmProfile = {
      async createToken (pass) {
        t.match(pass, 'bar', 'should use password for basic auth')
        return {}
      },
    }

    const Profile = t.mock('../../lib/profile.js', {
      ...mocks,
      'npm-profile': npmProfile,
    })
    const profile = new Profile(npm)

    profile.exec(['enable-2fa', 'auth-only'], err => {
      t.match(
        err.message,
        'Your registry https://registry.npmjs.org/ does ' +
        'not seem to support bearer tokens. Bearer tokens ' +
        'are required for two-factor authentication',
        'should throw no support msg'
      )
    })
  })

  t.test('from basic username/password auth', t => {
    // mock legacy basic auth style with user/pass
    npm.config.getCredentialsByURI = () => {
      return { username: 'foo', password: 'bar' }
    }

    const npmProfile = {
      async createToken (pass) {
        return {}
      },
    }

    const Profile = t.mock('../../lib/profile.js', {
      ...mocks,
      'npm-profile': npmProfile,
    })
    const profile = new Profile(npm)

    profile.exec(['enable-2fa', 'auth-only'], err => {
      t.match(
        err.message,
        'Your registry https://registry.npmjs.org/ does ' +
        'not seem to support bearer tokens. Bearer tokens ' +
        'are required for two-factor authentication',
        'should throw no support msg'
      )
      t.end()
    })
  })

  t.test('no auth found', t => {
    npm.config.getCredentialsByURI = () => ({})

    const Profile = t.mock('../../lib/profile.js', {
      ...mocks,
    })
    const profile = new Profile(npm)

    profile.exec(['enable-2fa', 'auth-only'], err => {
      t.match(
        err.message,
        'You need to be logged in to registry ' +
        'https://registry.npmjs.org/ in order to enable 2fa'
      )
      t.end()
    })
  })

  t.test('from basic auth, asks for otp', t => {
    t.plan(10)

    // mock legacy basic auth style
    npm.config.getCredentialsByURI = (reg) => {
      t.equal(reg, flatOptions.registry, 'should use expected registry')
      return { auth: Buffer.from('foo:bar').toString('base64') }
    }
    npm.config.setCredentialsByURI = (registry, { token }) => {
      t.equal(registry, flatOptions.registry, 'should set expected registry')
      t.equal(token, 'token', 'should set expected token')
    }
    npm.config.save = (type) => {
      t.equal(type, 'user', 'should save to user config')
    }

    const npmProfile = {
      async createToken (pass) {
        t.match(pass, 'bar', 'should use password for basic auth')
        return { token: 'token' }
      },
      async get () {
        return userProfile
      },
      async set (newProfile, conf) {
        t.match(
          newProfile,
          {
            tfa: {
              mode: 'auth-only',
            },
          },
          'should set tfa mode'
        )
        t.match(
          conf,
          {
            ...npm.flatOptions,
            otp: '123456',
          },
          'should forward flatOptions config'
        )
        return {
          ...userProfile,
          tfa: null,
        }
      },
    }

    const readUserInfo = {
      async password () {
        t.ok('should interactively ask for password confirmation')
        return 'password1234'
      },
      async otp (label) {
        t.equal(
          label,
          'Enter one-time password from your authenticator app: ',
          'should ask for otp confirmation'
        )
        return '123456'
      },
    }

    const Profile = t.mock('../../lib/profile.js', {
      ...mocks,
      'npm-profile': npmProfile,
      '../../lib/utils/read-user-info.js': readUserInfo,
    })
    const profile = new Profile(npm)

    profile.exec(['enable-2fa', 'auth-only'], err => {
      if (err)
        throw err

      t.equal(
        result,
        'Two factor authentication mode changed to: auth-only',
        'should output success msg'
      )
    })
  })

  t.test('from token and set otp, retries on pending and verifies with qrcode', t => {
    t.plan(4)

    flatOptions.otp = '1234'

    npm.config.getCredentialsByURI = () => {
      return { token: 'token' }
    }

    let setCount = 0
    const npmProfile = {
      async get () {
        return {
          ...userProfile,
          tfa: {
            pending: true,
          },
        }
      },
      async set (newProfile, conf) {
        setCount++

        // when profile response shows that 2fa is pending the
        // first time calling npm-profile.set should reset 2fa
        if (setCount === 1) {
          t.match(
            newProfile,
            {
              tfa: {
                password: 'password1234',
                mode: 'disable',
              },
            },
            'should reset 2fa'
          )
        } else if (setCount === 2) {
          t.match(
            newProfile,
            {
              tfa: {
                mode: 'auth-only',
              },
            },
            'should set tfa mode approprietly in follow-up call'
          )
        } else if (setCount === 3) {
          t.match(
            newProfile,
            {
              tfa: ['123456'],
            },
            'should set tfa as otp code?'
          )
          return {
            ...userProfile,
            tfa: [
              '123456',
              '789101',
            ],
          }
        }

        return {
          ...userProfile,
          tfa: 'otpauth://foo?secret=1234',
        }
      },
    }

    const readUserInfo = {
      async password () {
        return 'password1234'
      },
      async otp (label) {
        return '123456'
      },
    }

    const qrcode = {
      // eslint-disable-next-line standard/no-callback-literal
      generate: (url, cb) => cb('qrcode'),
    }

    const Profile = t.mock('../../lib/profile.js', {
      ...mocks,
      'npm-profile': npmProfile,
      'qrcode-terminal': qrcode,
      '../../lib/utils/read-user-info.js': readUserInfo,
    })
    const profile = new Profile(npm)

    profile.exec(['enable-2fa', 'auth-only'], err => {
      if (err)
        throw err

      t.matchSnapshot(
        result,
        'should output 2fa enablement success msgs'
      )
    })
  })

  t.test('from token and set otp, retrieves invalid otp', t => {
    flatOptions.otp = '1234'

    npm.config.getCredentialsByURI = () => {
      return { token: 'token' }
    }

    const npmProfile = {
      async get () {
        return {
          ...userProfile,
          tfa: {
            pending: true,
          },
        }
      },
      async set (newProfile, conf) {
        return {
          ...userProfile,
          tfa: 'http://foo?secret=1234',
        }
      },
    }

    const readUserInfo = {
      async password () {
        return 'password1234'
      },
      async otp (label) {
        return '123456'
      },
    }

    const Profile = t.mock('../../lib/profile.js', {
      ...mocks,
      'npm-profile': npmProfile,
      '../../lib/utils/read-user-info.js': readUserInfo,
    })
    const profile = new Profile(npm)

    profile.exec(['enable-2fa', 'auth-only'], err => {
      t.match(
        err,
        /Unknown error enabling two-factor authentication./,
        'should throw invalid 2fa auth url error'
      )
      t.end()
    })
  })

  t.test('from token auth provides --otp config arg', t => {
    flatOptions.otp = '123456'
    flatOptions.otp = '123456'

    npm.config.getCredentialsByURI = (reg) => {
      return { token: 'token' }
    }

    const npmProfile = {
      async get () {
        return userProfile
      },
      async set (newProfile, conf) {
        return {
          ...userProfile,
          tfa: null,
        }
      },
    }

    const readUserInfo = {
      async password () {
        return 'password1234'
      },
      async otp () {
        throw new Error('should not ask for otp')
      },
    }

    const Profile = t.mock('../../lib/profile.js', {
      ...mocks,
      'npm-profile': npmProfile,
      '../../lib/utils/read-user-info.js': readUserInfo,
    })
    const profile = new Profile(npm)

    profile.exec(['enable-2fa', 'auth-and-writes'], err => {
      if (err)
        throw err

      t.equal(
        result,
        'Two factor authentication mode changed to: auth-and-writes',
        'should output success msg'
      )
      t.end()
    })
  })

  t.test('missing tfa from user profile', t => {
    npm.config.getCredentialsByURI = (reg) => {
      return { token: 'token' }
    }

    const npmProfile = {
      async get () {
        return {
          ...userProfile,
          tfa: undefined,
        }
      },
      async set (newProfile, conf) {
        return {
          ...userProfile,
          tfa: null,
        }
      },
    }

    const readUserInfo = {
      async password () {
        return 'password1234'
      },
      async otp () {
        return '123456'
      },
    }

    const Profile = t.mock('../../lib/profile.js', {
      ...mocks,
      'npm-profile': npmProfile,
      '../../lib/utils/read-user-info.js': readUserInfo,
    })
    const profile = new Profile(npm)

    profile.exec(['enable-2fa', 'auth-only'], err => {
      if (err)
        throw err

      t.equal(
        result,
        'Two factor authentication mode changed to: auth-only',
        'should output success msg'
      )
      t.end()
    })
  })

  t.test('defaults to auth-and-writes permission if no mode specified', t => {
    npm.config.getCredentialsByURI = (reg) => {
      return { token: 'token' }
    }

    const npmProfile = {
      async get () {
        return {
          ...userProfile,
          tfa: undefined,
        }
      },
      async set (newProfile, conf) {
        return {
          ...userProfile,
          tfa: null,
        }
      },
    }

    const readUserInfo = {
      async password () {
        return 'password1234'
      },
      async otp () {
        return '123456'
      },
    }

    const Profile = t.mock('../../lib/profile.js', {
      ...mocks,
      'npm-profile': npmProfile,
      '../../lib/utils/read-user-info.js': readUserInfo,
    })
    const profile = new Profile(npm)

    profile.exec(['enable-2fa'], err => {
      if (err)
        throw err

      t.equal(
        result,
        'Two factor authentication mode changed to: auth-and-writes',
        'should enable 2fa with auth-and-writes permission'
      )
      t.end()
    })
  })

  t.end()
})

t.test('disable-2fa', t => {
  t.test('no tfa enabled', t => {
    const npmProfile = {
      async get () {
        return {
          ...userProfile,
          tfa: null,
        }
      },
    }

    const Profile = t.mock('../../lib/profile.js', {
      ...mocks,
      'npm-profile': npmProfile,
    })
    const profile = new Profile(npm)

    profile.exec(['disable-2fa'], err => {
      if (err)
        throw err

      t.equal(
        result,
        'Two factor authentication not enabled.',
        'should output already disalbed msg'
      )
      t.end()
    })
  })

  t.test('requests otp', t => {
    const npmProfile = t => ({
      async get () {
        return userProfile
      },
      async set (newProfile, conf) {
        t.same(
          newProfile,
          {
            tfa: {
              password: 'password1234',
              mode: 'disable',
            },
          },
          'should send the new info for setting in profile'
        )
        t.match(
          conf,
          {
            ...npm.flatOptions,
            otp: '1234',
          },
          'should forward flatOptions config'
        )
      },
    })

    const readUserInfo = t => ({
      async password () {
        t.ok('should interactively ask for password confirmation')
        return 'password1234'
      },
      async otp (label) {
        t.equal(
          label,
          'Enter one-time password from your authenticator app: ',
          'should ask for otp confirmation'
        )
        return '1234'
      },
    })

    t.test('default output', t => {
      const Profile = t.mock('../../lib/profile.js', {
        ...mocks,
        'npm-profile': npmProfile(t),
        '../../lib/utils/read-user-info.js': readUserInfo(t),
      })
      const profile = new Profile(npm)

      profile.exec(['disable-2fa'], err => {
        if (err)
          throw err

        t.equal(
          result,
          'Two factor authentication disabled.',
          'should output already disabled msg'
        )
        t.end()
      })
    })

    t.test('--json', t => {
      config.json = true

      const Profile = t.mock('../../lib/profile.js', {
        ...mocks,
        'npm-profile': npmProfile(t),
        '../../lib/utils/read-user-info.js': readUserInfo(t),
      })
      const profile = new Profile(npm)

      profile.exec(['disable-2fa'], err => {
        if (err)
          throw err

        t.same(
          JSON.parse(result),
          { tfa: false },
          'should output json already disabled msg'
        )
        t.end()
      })
    })

    t.test('--parseable', t => {
      config.parseable = true

      const Profile = t.mock('../../lib/profile.js', {
        ...mocks,
        'npm-profile': npmProfile(t),
        '../../lib/utils/read-user-info.js': readUserInfo(t),
      })
      const profile = new Profile(npm)

      profile.exec(['disable-2fa'], err => {
        if (err)
          throw err

        t.equal(
          result,
          'tfa\tfalse',
          'should output parseable already disabled msg'
        )
        t.end()
      })
    })

    t.end()
  })

  t.test('--otp config already set', t => {
    t.plan(3)

    flatOptions.otp = '123456'

    const npmProfile = {
      async get () {
        return userProfile
      },
      async set (newProfile, conf) {
        t.same(
          newProfile,
          {
            tfa: {
              password: 'password1234',
              mode: 'disable',
            },
          },
          'should send the new info for setting in profile'
        )
        t.match(
          conf,
          {
            ...npm.flatOptions,
            otp: '123456',
          },
          'should forward flatOptions config'
        )
      },
    }

    const readUserInfo = {
      async password () {
        return 'password1234'
      },
      async otp (label) {
        throw new Error('should not ask for otp')
      },
    }

    const Profile = t.mock('../../lib/profile.js', {
      ...mocks,
      'npm-profile': npmProfile,
      '../../lib/utils/read-user-info.js': readUserInfo,
    })
    const profile = new Profile(npm)

    profile.exec(['disable-2fa'], err => {
      if (err)
        throw err

      t.equal(
        result,
        'Two factor authentication disabled.',
        'should output already disalbed msg'
      )
    })
  })

  t.end()
})

t.test('unknown subcommand', t => {
  profile.exec(['asfd'], err => {
    t.match(
      err,
      /Unknown profile command: asfd/,
      'should throw unknown cmd error'
    )
    t.end()
  })
})

t.test('completion', t => {
  const testComp = async ({ t, argv, expect, title }) => {
    t.resolveMatch(
      profile.completion({ conf: { argv: { remain: argv } } }),
      expect,
      title
    )
  }

  t.test('npm profile autocomplete', async t => {
    await testComp({
      t,
      argv: ['npm', 'profile'],
      expect: ['enable-2fa', 'disable-2fa', 'get', 'set'],
      title: 'should auto complete with subcommands',
    })

    t.end()
  })

  t.test('npm profile enable autocomplete', async t => {
    await testComp({
      t,
      argv: ['npm', 'profile', 'enable-2fa'],
      expect: ['auth-and-writes', 'auth-only'],
      title: 'should auto complete with auth types',
    })

    t.end()
  })

  t.test('npm profile <subcmd> no autocomplete', async t => {
    const noAutocompleteCmds = ['disable-2fa', 'disable-tfa', 'get', 'set']
    for (const subcmd of noAutocompleteCmds) {
      await testComp({
        t,
        argv: ['npm', 'profile', subcmd],
        expect: [],
        title: `${subcmd} should have no autocomplete`,
      })
    }

    t.end()
  })

  t.test('npm profile unknown subcommand autocomplete', async t => {
    t.rejects(
      profile.completion({ conf: { argv: { remain: ['npm', 'profile', 'asdf'] } } }),
      { message: 'asdf not recognized' }, 'should throw unknown cmd error'
    )
    t.end()
  })

  t.end()
})

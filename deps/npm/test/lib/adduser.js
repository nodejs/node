const t = require('tap')
const { getCredentialsByURI, setCredentialsByURI } =
  require('@npmcli/config').prototype

let result = ''

const _flatOptions = {
  authType: 'legacy',
  registry: 'https://registry.npmjs.org/',
  scope: '',
  fromFlatOptions: true,
}

let failSave = false
let deletedConfig = {}
let registryOutput = ''
let setConfig = {}
const authDummy = (npm, options) => {
  if (!options.fromFlatOptions)
    throw new Error('did not pass full flatOptions to auth function')

  return Promise.resolve({
    message: 'success',
    newCreds: {
      username: 'u',
      password: 'p',
      email: 'u@npmjs.org',
    },
  })
}

const deleteMock = (key, where) => {
  deletedConfig = {
    ...deletedConfig,
    [key]: where,
  }
}
const npm = {
  flatOptions: _flatOptions,
  config: {
    delete: deleteMock,
    get (key, where) {
      if (!where || where === 'user')
        return _flatOptions[key]
    },
    getCredentialsByURI,
    async save () {
      if (failSave)
        throw new Error('error saving user config')
    },
    set (key, value, where) {
      setConfig = {
        ...setConfig,
        [key]: {
          value,
          where,
        },
      }
    },
    setCredentialsByURI,
  },
  output: msg => {
    result = msg
  },
}

const AddUser = t.mock('../../lib/adduser.js', {
  npmlog: {
    disableProgress: () => null,
    notice: (_, msg) => {
      registryOutput = msg
    },
  },
  '../../lib/auth/legacy.js': authDummy,
})

const adduser = new AddUser(npm)

t.test('usage', (t) => {
  t.match(adduser.usage, 'adduser', 'usage has command name in it')
  t.end()
})
t.test('simple login', (t) => {
  adduser.exec([], (err) => {
    t.error(err, 'npm adduser')

    t.equal(
      registryOutput,
      'Log in on https://registry.npmjs.org/',
      'should have correct message result'
    )

    t.same(
      deletedConfig,
      {
        _token: 'user',
        _password: 'user',
        username: 'user',
        _auth: 'user',
        _authtoken: 'user',
        '-authtoken': 'user',
        _authToken: 'user',
        '//registry.npmjs.org/:-authtoken': 'user',
        '//registry.npmjs.org/:_authToken': 'user',
        '//registry.npmjs.org/:_authtoken': 'user',
        '//registry.npmjs.org/:always-auth': 'user',
        '//registry.npmjs.org/:email': 'user',
      },
      'should delete token in user config'
    )

    t.same(
      setConfig,
      {
        '//registry.npmjs.org/:_password': { value: 'cA==', where: 'user' },
        '//registry.npmjs.org/:username': { value: 'u', where: 'user' },
        email: { value: 'u@npmjs.org', where: 'user' },
      },
      'should set expected user configs'
    )

    t.equal(
      result,
      'success',
      'should output auth success msg'
    )

    registryOutput = ''
    deletedConfig = {}
    setConfig = {}
    result = ''
    t.end()
  })
})

t.test('bad auth type', (t) => {
  _flatOptions.authType = 'foo'

  adduser.exec([], (err) => {
    t.match(
      err,
      /Error: no such auth module/,
      'should throw bad auth type error'
    )

    _flatOptions.authType = 'legacy'
    deletedConfig = {}
    setConfig = {}
    result = ''
    t.end()
  })
})

t.test('scoped login', (t) => {
  _flatOptions.scope = '@myscope'

  adduser.exec([], (err) => {
    t.error(err, 'npm adduser')

    t.same(
      setConfig['@myscope:registry'],
      { value: 'https://registry.npmjs.org/', where: 'user' },
      'should set scoped registry config'
    )

    _flatOptions.scope = ''
    deletedConfig = {}
    setConfig = {}
    result = ''
    t.end()
  })
})

t.test('scoped login with valid scoped registry config', (t) => {
  _flatOptions['@myscope:registry'] = 'https://diff-registry.npmjs.com/'
  _flatOptions.scope = '@myscope'

  adduser.exec([], (err) => {
    t.error(err, 'npm adduser')

    t.same(
      setConfig['@myscope:registry'],
      { value: 'https://diff-registry.npmjs.com/', where: 'user' },
      'should keep scoped registry config'
    )

    delete _flatOptions['@myscope:registry']
    _flatOptions.scope = ''
    deletedConfig = {}
    setConfig = {}
    result = ''
    t.end()
  })
})

t.test('save config failure', (t) => {
  failSave = true

  adduser.exec([], (err) => {
    t.match(
      err,
      /error saving user config/,
      'should throw config.save error'
    )

    failSave = false
    deletedConfig = {}
    setConfig = {}
    result = ''
    t.end()
  })
})

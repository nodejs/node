const t = require('tap')

const { EventEmitter } = require('events')

const redactCwd = (path) => {
  const normalizePath = p => p
    .replace(/\\+/g, '/')
    .replace(/\r\n/g, '\n')
  const replaceCwd = p => p
    .replace(new RegExp(normalizePath(process.cwd()), 'g'), '{CWD}')
  const cleanupWinPaths = p => p
    .replace(normalizePath(process.execPath), '/path/to/node')
    .replace(normalizePath(process.env.HOME), '~/')

  return cleanupWinPaths(
    replaceCwd(
      normalizePath(path)
    )
  )
}

t.cleanSnapshot = (str) => redactCwd(str)

let result = ''

const configDefs = require('../../lib/utils/config')
const definitions = Object.entries(configDefs.definitions)
  .filter(([key, def]) => {
    return [
      'init-author-name',
      'init.author.name',
      'init-version',
      'init.version',
    ].includes(key)
  }).reduce((defs, [key, def]) => {
    defs[key] = def
    return defs
  }, {})

const defaults = {
  'init-author-name': '',
  'init-version': '1.0.0',
  'init.author.name': '',
  'init.version': '1.0.0',
}

const cliConfig = {
  editor: 'vi',
  json: false,
  long: false,
  global: false,
  cat: true,
  chai: true,
  dog: true,
}

const npm = {
  log: {
    warn: () => null,
    info: () => null,
    enableProgress: () => null,
    disableProgress: () => null,
  },
  config: {
    data: new Map(Object.entries({
      default: { data: defaults, source: 'default values' },
      global: { data: {}, source: '/etc/npmrc' },
      cli: { data: cliConfig, source: 'command line options' },
    })),
    get (key) {
      return cliConfig[key]
    },
    validate () {
      return true
    },
  },
  output: msg => {
    result = msg
  },
}

const usageUtil = () => 'usage instructions'

const mocks = {
  '../../lib/utils/config/index.js': { defaults, definitions },
  '../../lib/utils/usage.js': usageUtil,
}

const Config = t.mock('../../lib/config.js', mocks)
const config = new Config(npm)

t.test('config no args', t => {
  config.exec([], (err) => {
    t.match(err, /usage instructions/, 'should not error out on empty locations')
    t.end()
  })
})

t.test('config ignores workspaces', t => {
  npm.log.warn = (title, msg) => {
    t.equal(title, 'config', 'should warn with expected title')
    t.equal(
      msg,
      'This command does not support workspaces.',
      'should warn with unsupported option msg'
    )
  }
  config.execWorkspaces([], [], (err) => {
    t.match(err, /usage instructions/, 'should not error out when workspaces are defined')
    npm.log.warn = () => null
    t.end()
  })
})

t.test('config list', t => {
  t.plan(2)

  npm.config.find = () => 'cli'
  result = ''
  t.teardown(() => {
    result = ''
    delete npm.config.find
  })

  config.exec(['list'], (err) => {
    t.error(err, 'npm config list')
    t.matchSnapshot(result, 'should list configs')
  })
})

t.test('config list overrides', t => {
  t.plan(2)

  npm.config.data.set('user', {
    data: {
      'init.author.name': 'Foo',
      '//private-reg.npmjs.org/:_authThoken': 'f00ba1',
    },
    source: '~/.npmrc',
  })
  cliConfig['init.author.name'] = 'Bar'
  npm.config.find = () => 'cli'
  result = ''
  t.teardown(() => {
    result = ''
    npm.config.data.delete('user')
    delete cliConfig['init.author.name']
    delete npm.config.find
  })

  config.exec(['list'], (err) => {
    t.error(err, 'npm config list')
    t.matchSnapshot(result, 'should list overridden configs')
  })
})

t.test('config list --long', t => {
  t.plan(2)

  npm.config.find = key => key in cliConfig ? 'cli' : 'default'
  cliConfig.long = true
  result = ''
  t.teardown(() => {
    delete npm.config.find
    cliConfig.long = false
    result = ''
  })

  config.exec(['list'], (err) => {
    t.error(err, 'npm config list --long')
    t.matchSnapshot(result, 'should list all configs')
  })
})

t.test('config list --json', t => {
  t.plan(2)

  cliConfig.json = true
  result = ''
  npm.config.list = [{
    '//private-reg.npmjs.org/:_authThoken': 'f00ba1',
    ...npm.config.data.get('cli').data,
  }]
  const npmConfigGet = npm.config.get
  npm.config.get = key => npm.config.list[0][key]

  t.teardown(() => {
    delete npm.config.list
    cliConfig.json = false
    npm.config.get = npmConfigGet
    result = ''
  })

  config.exec(['list'], (err) => {
    t.error(err, 'npm config list --json')
    t.same(
      JSON.parse(result),
      {
        editor: 'vi',
        json: true,
        long: false,
        global: false,
        cat: true,
        chai: true,
        dog: true,
      },
      'should list configs usin json'
    )
  })
})

t.test('config delete no args', t => {
  config.exec(['delete'], (err) => {
    t.match(err, { message: '\nUsage: usage instructions' })
    t.end()
  })
})

t.test('config delete key', t => {
  t.plan(4)

  npm.config.delete = (key, where) => {
    t.equal(key, 'foo', 'should delete expected keyword')
    t.equal(where, 'user', 'should delete key from user config by default')
  }

  npm.config.save = where => {
    t.equal(where, 'user', 'should save user config post-delete')
  }

  config.exec(['delete', 'foo'], (err) => {
    t.error(err, 'npm config delete key')
  })

  t.teardown(() => {
    delete npm.config.delete
    delete npm.config.save
  })
})

t.test('config delete multiple key', t => {
  t.plan(6)

  const expect = [
    'foo',
    'bar',
  ]

  npm.config.delete = (key, where) => {
    t.equal(key, expect.shift(), 'should delete expected keyword')
    t.equal(where, 'user', 'should delete key from user config by default')
  }

  npm.config.save = where => {
    t.equal(where, 'user', 'should save user config post-delete')
  }

  config.exec(['delete', 'foo', 'bar'], (err) => {
    t.error(err, 'npm config delete keys')
  })

  t.teardown(() => {
    delete npm.config.delete
    delete npm.config.save
  })
})

t.test('config delete key --global', t => {
  t.plan(4)

  npm.config.delete = (key, where) => {
    t.equal(key, 'foo', 'should delete expected keyword from global configs')
    t.equal(where, 'global', 'should delete key from global config by default')
  }

  npm.config.save = where => {
    t.equal(where, 'global', 'should save global config post-delete')
  }

  cliConfig.global = true
  config.exec(['delete', 'foo'], (err) => {
    t.error(err, 'npm config delete key --global')
  })

  t.teardown(() => {
    cliConfig.global = false
    delete npm.config.delete
    delete npm.config.save
  })
})

t.test('config set no args', t => {
  config.exec(['set'], (err) => {
    t.match(err, { message: '\nUsage: usage instructions' })
    t.end()
  })
})

t.test('config set key', t => {
  t.plan(5)

  npm.config.set = (key, val, where) => {
    t.equal(key, 'foo', 'should set expected key to user config')
    t.equal(val, 'bar', 'should set expected value to user config')
    t.equal(where, 'user', 'should set key/val in user config by default')
  }

  npm.config.save = where => {
    t.equal(where, 'user', 'should save user config')
  }

  config.exec(['set', 'foo', 'bar'], (err) => {
    t.error(err, 'npm config set key')
  })

  t.teardown(() => {
    delete npm.config.set
    delete npm.config.save
  })
})

t.test('config set key=val', t => {
  t.plan(5)

  npm.config.set = (key, val, where) => {
    t.equal(key, 'foo', 'should set expected key to user config')
    t.equal(val, 'bar', 'should set expected value to user config')
    t.equal(where, 'user', 'should set key/val in user config by default')
  }

  npm.config.save = where => {
    t.equal(where, 'user', 'should save user config')
  }

  config.exec(['set', 'foo=bar'], (err) => {
    t.error(err, 'npm config set key')
  })

  t.teardown(() => {
    delete npm.config.set
    delete npm.config.save
  })
})

t.test('config set multiple keys', t => {
  t.plan(11)

  const expect = [
    ['foo', 'bar'],
    ['bar', 'baz'],
    ['asdf', ''],
  ]
  const args = ['foo', 'bar', 'bar=baz', 'asdf']

  npm.config.set = (key, val, where) => {
    const [expectKey, expectVal] = expect.shift()
    t.equal(key, expectKey, 'should set expected key to user config')
    t.equal(val, expectVal, 'should set expected value to user config')
    t.equal(where, 'user', 'should set key/val in user config by default')
  }

  npm.config.save = where => {
    t.equal(where, 'user', 'should save user config')
  }

  config.exec(['set', ...args], (err) => {
    t.error(err, 'npm config set key')
  })

  t.teardown(() => {
    delete npm.config.set
    delete npm.config.save
  })
})

t.test('config set key to empty value', t => {
  t.plan(5)

  npm.config.set = (key, val, where) => {
    t.equal(key, 'foo', 'should set expected key to user config')
    t.equal(val, '', 'should set "" to user config')
    t.equal(where, 'user', 'should set key/val in user config by default')
  }

  npm.config.save = where => {
    t.equal(where, 'user', 'should save user config')
  }

  config.exec(['set', 'foo'], (err) => {
    t.error(err, 'npm config set key to empty value')
  })

  t.teardown(() => {
    delete npm.config.set
    delete npm.config.save
  })
})

t.test('config set invalid key', t => {
  t.plan(3)

  const npmConfigValidate = npm.config.validate
  npm.config.save = () => null
  npm.config.set = () => null
  npm.config.validate = () => false
  npm.log.warn = (title, msg) => {
    t.equal(title, 'config', 'should warn with expected title')
    t.equal(msg, 'omitting invalid config values', 'should use expected msg')
  }
  t.teardown(() => {
    npm.config.validate = npmConfigValidate
    delete npm.config.save
    delete npm.config.set
    npm.log.warn = () => null
  })

  config.exec(['set', 'foo', 'bar'], (err) => {
    t.error(err, 'npm config set invalid key')
  })
})

t.test('config set key --global', t => {
  t.plan(5)

  npm.config.set = (key, val, where) => {
    t.equal(key, 'foo', 'should set expected key to global config')
    t.equal(val, 'bar', 'should set expected value to global config')
    t.equal(where, 'global', 'should set key/val in global config')
  }

  npm.config.save = where => {
    t.equal(where, 'global', 'should save global config')
  }

  cliConfig.global = true
  config.exec(['set', 'foo', 'bar'], (err) => {
    t.error(err, 'npm config set key --global')
  })

  t.teardown(() => {
    cliConfig.global = false
    delete npm.config.set
    delete npm.config.save
  })
})

t.test('config get no args', t => {
  t.plan(2)

  npm.config.find = () => 'cli'
  result = ''
  t.teardown(() => {
    result = ''
    delete npm.config.find
  })

  config.exec(['get'], (err) => {
    t.error(err, 'npm config get no args')
    t.matchSnapshot(result, 'should list configs on config get no args')
  })
})

t.test('config get key', t => {
  t.plan(2)

  const npmConfigGet = npm.config.get
  npm.config.get = (key) => {
    t.equal(key, 'foo', 'should use expected key')
    return 'bar'
  }

  npm.config.save = where => {
    throw new Error('should not save')
  }

  config.exec(['get', 'foo'], (err) => {
    t.error(err, 'npm config get key')
  })

  t.teardown(() => {
    npm.config.get = npmConfigGet
    delete npm.config.save
  })
})

t.test('config get multiple keys', t => {
  t.plan(4)

  const expect = [
    'foo',
    'bar',
  ]

  const npmConfigGet = npm.config.get
  npm.config.get = (key) => {
    t.equal(key, expect.shift(), 'should use expected key')
    return 'asdf'
  }

  npm.config.save = where => {
    throw new Error('should not save')
  }

  config.exec(['get', 'foo', 'bar'], (err) => {
    t.error(err, 'npm config get multiple keys')
    t.equal(result, 'foo=asdf\nbar=asdf')
  })

  t.teardown(() => {
    result = ''
    npm.config.get = npmConfigGet
    delete npm.config.save
  })
})

t.test('config get private key', t => {
  config.exec(['get', '//private-reg.npmjs.org/:_authThoken'], (err) => {
    t.match(
      err,
      /The \/\/private-reg.npmjs.org\/:_authThoken option is protected, and cannot be retrieved in this way/,
      'should throw unable to retrieve error'
    )
    t.end()
  })
})

t.test('config edit', t => {
  t.plan(12)
  const npmrc = `//registry.npmjs.org/:_authToken=0000000
init.author.name=Foo
sign-git-commit=true`
  npm.config.data.set('user', {
    source: '~/.npmrc',
  })
  npm.config.save = async where => {
    t.equal(where, 'user', 'should save to user config by default')
  }
  const editMocks = {
    ...mocks,
    'mkdirp-infer-owner': async () => null,
    fs: {
      readFile (path, encoding, cb) {
        cb(null, npmrc)
      },
      writeFile (file, data, encoding, cb) {
        t.equal(file, '~/.npmrc', 'should save to expected file location')
        t.matchSnapshot(data, 'should write config file')
        cb()
      },
    },
    child_process: {
      spawn: (bin, args) => {
        t.equal(bin, 'vi', 'should use default editor')
        t.strictSame(args, ['~/.npmrc'], 'should match user source data')
        const ee = new EventEmitter()
        process.nextTick(() => {
          ee.emit('exit', 0)
        })
        return ee
      },
    },
  }
  const Config = t.mock('../../lib/config.js', editMocks)
  const config = new Config(npm)

  config.exec(['edit'], (err) => {
    t.error(err, 'npm config edit')

    // test no config file result
    editMocks.fs.readFile = (p, e, cb) => {
      cb(new Error('ERR'))
    }
    const Config = t.mock('../../lib/config.js', editMocks)
    const config = new Config(npm)
    config.exec(['edit'], (err) => {
      t.error(err, 'npm config edit')
    })
  })

  t.teardown(() => {
    npm.config.data.delete('user')
    delete npm.config.save
  })
})

t.test('config edit --global', t => {
  t.plan(6)

  cliConfig.global = true
  const npmrc = 'init.author.name=Foo'
  npm.config.data.set('global', {
    source: '/etc/npmrc',
  })
  npm.config.save = async where => {
    t.equal(where, 'global', 'should save to global config')
  }
  const editMocks = {
    ...mocks,
    'mkdirp-infer-owner': async () => null,
    fs: {
      readFile (path, encoding, cb) {
        cb(null, npmrc)
      },
      writeFile (file, data, encoding, cb) {
        t.equal(file, '/etc/npmrc', 'should save to global file location')
        t.matchSnapshot(data, 'should write global config file')
        cb()
      },
    },
    child_process: {
      spawn: (bin, args, cb) => {
        t.equal(bin, 'vi', 'should use default editor')
        t.strictSame(args, ['/etc/npmrc'], 'should match global source data')
        const ee = new EventEmitter()
        process.nextTick(() => {
          ee.emit('exit', 137)
        })
        return ee
      },
    },
  }
  const Config = t.mock('../../lib/config.js', editMocks)
  const config = new Config(npm)
  config.exec(['edit'], (err) => {
    t.match(err, /exited with code: 137/, 'propagated exit code from editor')
  })

  t.teardown(() => {
    cliConfig.global = false
    npm.config.data.delete('user')
    delete npm.config.save
  })
})

t.test('completion', t => {
  const { completion } = config

  const testComp = (argv, expect) => {
    t.resolveMatch(completion({ conf: { argv: { remain: argv } } }), expect, argv.join(' '))
  }

  testComp(['npm', 'foo'], [])
  testComp(['npm', 'config'], ['get', 'set', 'delete', 'ls', 'rm', 'edit', 'list'])
  testComp(['npm', 'config', 'set', 'foo'], [])

  const possibleConfigKeys = [...Object.keys(definitions)]
  testComp(['npm', 'config', 'get'], possibleConfigKeys)
  testComp(['npm', 'config', 'set'], possibleConfigKeys)
  testComp(['npm', 'config', 'delete'], possibleConfigKeys)
  testComp(['npm', 'config', 'rm'], possibleConfigKeys)
  testComp(['npm', 'config', 'edit'], [])
  testComp(['npm', 'config', 'list'], [])
  testComp(['npm', 'config', 'ls'], [])

  const partial = completion({conf: { argv: { remain: ['npm', 'config'] } }, partialWord: 'l'})
  t.resolveMatch(partial, ['get', 'set', 'delete', 'ls', 'rm', 'edit'], 'npm config')

  t.end()
})

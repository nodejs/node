const t = require('tap')
const { fake: mockNpm } = require('../fixtures/mock-npm.js')

let result = ''
let readPackageNamePrefix = null
let readPackageNameResponse = null

const noop = () => null

const npm = mockNpm({
  output: (msg) => {
    result = result ? `${result}\n${msg}` : msg
  },
})

const npmFetch = { json: noop }
const npmlog = { error: noop, info: noop, verbose: noop }
const pacote = { packument: noop }

const mocks = {
  npmlog,
  'npm-registry-fetch': npmFetch,
  pacote,
  '../../lib/utils/otplease.js': async (opts, fn) => fn({ otp: '123456', opts }),
  '../../lib/utils/read-package-name.js': async (prefix) => {
    readPackageNamePrefix = prefix
    return readPackageNameResponse
  },
  '../../lib/utils/usage.js': () => 'usage instructions',
}

const npmcliMaintainers = [
  { email: 'quitlahok@gmail.com', name: 'nlf' },
  { email: 'ruyadorno@hotmail.com', name: 'ruyadorno' },
  { email: 'darcy@darcyclarke.me', name: 'darcyclarke' },
  { email: 'i@izs.me', name: 'isaacs' },
]

const Owner = t.mock('../../lib/owner.js', mocks)
const owner = new Owner(npm)

t.test('owner no args', t => {
  result = ''
  t.teardown(() => {
    result = ''
  })

  owner.exec([], err => {
    t.match(err, /usage instructions/, 'should not error out on empty locations')
    t.end()
  })
})

t.test('owner ls no args', t => {
  t.plan(5)

  result = ''

  readPackageNameResponse = '@npmcli/map-workspaces'
  pacote.packument = async (spec, opts) => {
    t.equal(spec.name, '@npmcli/map-workspaces', 'should use expect pkg name')
    t.match(
      opts,
      {
        ...npm.flatOptions,
        fullMetadata: true,
      },
      'should forward expected options to pacote.packument'
    )
    return { maintainers: npmcliMaintainers }
  }
  t.teardown(() => {
    npm.prefix = null
    result = ''
    pacote.packument = noop
    readPackageNameResponse = null
  })
  npm.prefix = 'test-npm-prefix'

  owner.exec(['ls'], err => {
    t.error(err, 'npm owner ls no args')
    t.matchSnapshot(result, 'should output owners of cwd package')
    t.equal(readPackageNamePrefix, 'test-npm-prefix', 'read-package-name gets npm.prefix')
  })
})

t.test('owner ls global', t => {
  t.teardown(() => {
    npm.config.set('global', false)
  })
  npm.config.set('global', true)

  owner.exec(['ls'], err => {
    t.match(err, /usage instructions/, 'should throw usage instructions if no cwd package available')
    t.end()
  })
})

t.test('owner ls no args no cwd package', t => {
  result = ''
  t.teardown(() => {
    result = ''
    npmlog.error = noop
  })

  owner.exec(['ls'], err => {
    t.match(err, /usage instructions/, 'should throw usage instructions if no cwd package available')
    t.end()
  })
})

t.test('owner ls fails to retrieve packument', t => {
  t.plan(4)

  result = ''
  readPackageNameResponse = '@npmcli/map-workspaces'
  pacote.packument = () => {
    throw new Error('ERR')
  }
  npmlog.error = (title, msg, pkgName) => {
    t.equal(title, 'owner ls', 'should list npm owner ls title')
    t.equal(msg, "Couldn't get owner data", 'should use expected msg')
    t.equal(pkgName, '@npmcli/map-workspaces', 'should use pkg name')
  }
  t.teardown(() => {
    result = ''
    npmlog.error = noop
    pacote.packument = noop
  })

  owner.exec(['ls'], err => {
    t.match(
      err,
      /ERR/,
      'should throw unknown error'
    )
  })
})

t.test('owner ls <pkg>', t => {
  t.plan(4)

  result = ''
  pacote.packument = async (spec, opts) => {
    t.equal(spec.name, '@npmcli/map-workspaces', 'should use expect pkg name')
    t.match(
      opts,
      {
        ...npm.flatOptions,
        fullMetadata: true,
      },
      'should forward expected options to pacote.packument'
    )
    return { maintainers: npmcliMaintainers }
  }
  t.teardown(() => {
    result = ''
    pacote.packument = noop
  })

  owner.exec(['ls', '@npmcli/map-workspaces'], err => {
    t.error(err, 'npm owner ls <pkg>')
    t.matchSnapshot(result, 'should output owners of <pkg>')
  })
})

t.test('owner ls <pkg> no maintainers', t => {
  result = ''
  pacote.packument = async (spec, opts) => {
    return { maintainers: [] }
  }
  t.teardown(() => {
    result = ''
    pacote.packument = noop
  })

  owner.exec(['ls', '@npmcli/map-workspaces'], err => {
    t.error(err, 'npm owner ls <pkg> no maintainers')
    t.equal(result, 'no admin found', 'should output no admint found msg')
    t.end()
  })
})

t.test('owner add <user> <pkg>', t => {
  t.plan(9)

  result = ''
  npmFetch.json = async (uri, opts) => {
    // retrieve user info from couchdb request
    if (uri === '/-/user/org.couchdb.user:foo') {
      t.ok('should request user info')
      t.match(opts, { ...npm.flatOptions }, 'should use expected opts')
      return {
        _id: 'org.couchdb.user:foo',
        email: 'foo@github.com',
        name: 'foo',
      }
    } else if (uri === '/@npmcli%2fmap-workspaces/-rev/1-foobaaa1') {
      t.ok('should put changed owner')
      t.match(opts, {
        ...npm.flatOptions,
        method: 'PUT',
        body: {
          _rev: '1-foobaaa1',
          maintainers: npmcliMaintainers,
        },
        otp: '123456',
        spec: {
          name: '@npmcli/map-workspaces',
        },
      }, 'should use expected opts')
      t.same(
        opts.body.maintainers,
        [
          ...npmcliMaintainers,
          {
            name: 'foo',
            email: 'foo@github.com',
          },
        ],
        'should contain expected new owners, adding requested user'
      )
      return {}
    } else
      t.fail(`unexpected fetch json call to uri: ${uri}`)
  }
  pacote.packument = async (spec, opts) => {
    t.equal(spec.name, '@npmcli/map-workspaces', 'should use expect pkg name')
    t.match(
      opts,
      {
        ...npm.flatOptions,
        fullMetadata: true,
      },
      'should forward expected options to pacote.packument'
    )
    return {
      _rev: '1-foobaaa1',
      maintainers: npmcliMaintainers,
    }
  }
  t.teardown(() => {
    result = ''
    npmFetch.json = noop
    pacote.packument = noop
  })

  owner.exec(['add', 'foo', '@npmcli/map-workspaces'], err => {
    t.error(err, 'npm owner add <user> <pkg>')
    t.equal(result, '+ foo (@npmcli/map-workspaces)', 'should output add result')
  })
})

t.test('owner add <user> cwd package', t => {
  result = ''
  readPackageNameResponse = '@npmcli/map-workspaces'
  npmFetch.json = async (uri, opts) => {
    // retrieve user info from couchdb request
    if (uri === '/-/user/org.couchdb.user:foo') {
      return {
        _id: 'org.couchdb.user:foo',
        email: 'foo@github.com',
        name: 'foo',
      }
    } else if (uri === '/@npmcli%2fmap-workspaces/-rev/1-foobaaa1')
      return {}
    else
      t.fail(`unexpected fetch json call to uri: ${uri}`)
  }
  pacote.packument = async (spec, opts) => ({
    _rev: '1-foobaaa1',
    maintainers: npmcliMaintainers,
  })
  t.teardown(() => {
    result = ''
    readPackageNameResponse = null
    npmFetch.json = noop
    pacote.packument = noop
  })

  owner.exec(['add', 'foo'], err => {
    t.error(err, 'npm owner add <user> cwd package')
    t.equal(result, '+ foo (@npmcli/map-workspaces)', 'should output add result')
    t.end()
  })
})

t.test('owner add <user> <pkg> already an owner', t => {
  t.plan(3)

  result = ''
  npmlog.info = (title, msg) => {
    t.equal(title, 'owner add', 'should use expected title')
    t.equal(
      msg,
      'Already a package owner: ruyadorno <ruyadorno@hotmail.com>',
      'should log already package owner info message'
    )
  }
  npmFetch.json = async (uri, opts) => {
    // retrieve user info from couchdb request
    if (uri === '/-/user/org.couchdb.user:ruyadorno') {
      return {
        _id: 'org.couchdb.user:ruyadorno',
        email: 'ruyadorno@hotmail.com',
        name: 'ruyadorno',
      }
    } else
      t.fail(`unexpected fetch json call to uri: ${uri}`)
  }
  pacote.packument = async (spec, opts) => {
    return {
      _rev: '1-foobaaa1',
      maintainers: npmcliMaintainers,
    }
  }
  t.teardown(() => {
    result = ''
    npmlog.info = noop
    npmFetch.json = noop
    pacote.packument = noop
  })

  owner.exec(['add', 'ruyadorno', '@npmcli/map-workspaces'], err => {
    t.error(err, 'npm owner add <user> <pkg> already an owner')
  })
})

t.test('owner add <user> <pkg> fails to retrieve user', t => {
  result = ''
  readPackageNameResponse =
  npmFetch.json = async (uri, opts) => {
    // retrieve borked user info from couchdb request
    if (uri === '/-/user/org.couchdb.user:foo')
      return { ok: false }
    else if (uri === '/@npmcli%2fmap-workspaces/-rev/1-foobaaa1')
      return {}
    else
      t.fail(`unexpected fetch json call to uri: ${uri}`)
  }
  pacote.packument = async (spec, opts) => ({
    _rev: '1-foobaaa1',
    maintainers: npmcliMaintainers,
  })
  t.teardown(() => {
    result = ''
    readPackageNameResponse = null
    npmFetch.json = noop
    pacote.packument = noop
  })

  owner.exec(['add', 'foo', '@npmcli/map-workspaces'], err => {
    t.match(
      err,
      /Error: Couldn't get user data for foo: {"ok":false}/,
      'should throw user data error'
    )
    t.equal(err.code, 'EOWNERUSER', 'should have expected error code')
    t.end()
  })
})

t.test('owner add <user> <pkg> fails to PUT updates', t => {
  result = ''
  npmFetch.json = async (uri, opts) => {
    // retrieve user info from couchdb request
    if (uri === '/-/user/org.couchdb.user:foo') {
      return {
        _id: 'org.couchdb.user:foo',
        email: 'foo@github.com',
        name: 'foo',
      }
    } else if (uri === '/@npmcli%2fmap-workspaces/-rev/1-foobaaa1') {
      return {
        error: {
          status: '418',
          message: "I'm a teapot",
        },
      }
    } else
      t.fail(`unexpected fetch json call to uri: ${uri}`)
  }
  pacote.packument = async (spec, opts) => ({
    _rev: '1-foobaaa1',
    maintainers: npmcliMaintainers,
  })
  t.teardown(() => {
    result = ''
    npmFetch.json = noop
    pacote.packument = noop
  })

  owner.exec(['add', 'foo', '@npmcli/map-workspaces'], err => {
    t.match(
      err.message,
      /Failed to update package/,
      'should throw failed to update package error'
    )
    t.equal(err.code, 'EOWNERMUTATE', 'should have expected error code')
    t.end()
  })
})

t.test('owner add <user> <pkg> fails to retrieve user info', t => {
  t.plan(3)

  result = ''
  npmlog.error = (title, msg) => {
    t.equal(title, 'owner mutate', 'should use expected title')
    t.equal(msg, 'Error getting user data for foo')
  }
  npmFetch.json = async (uri, opts) => {
    // retrieve user info from couchdb request
    if (uri === '/-/user/org.couchdb.user:foo') {
      throw Object.assign(
        new Error("I'm a teapot"),
        { status: 418 }
      )
    } else
      t.fail(`unexpected fetch json call to uri: ${uri}`)
  }
  pacote.packument = async (spec, opts) => ({
    _rev: '1-foobaaa1',
    maintainers: npmcliMaintainers,
  })
  t.teardown(() => {
    result = ''
    npmlog.error = noop
    npmFetch.json = noop
    pacote.packument = noop
  })

  owner.exec(['add', 'foo', '@npmcli/map-workspaces'], err => {
    t.match(
      err.message,
      "I'm a teapot",
      'should throw server error response'
    )
  })
})

t.test('owner add <user> <pkg> no previous maintainers property from server', t => {
  result = ''
  npmFetch.json = async (uri, opts) => {
    // retrieve user info from couchdb request
    if (uri === '/-/user/org.couchdb.user:foo') {
      return {
        _id: 'org.couchdb.user:foo',
        email: 'foo@github.com',
        name: 'foo',
      }
    } else if (uri === '/@npmcli%2fno-owners-pkg/-rev/1-foobaaa1')
      return {}
    else
      t.fail(`unexpected fetch json call to uri: ${uri}`)
  }
  pacote.packument = async (spec, opts) => {
    return {
      _rev: '1-foobaaa1',
      maintainers: null,
    }
  }
  t.teardown(() => {
    result = ''
    npmFetch.json = noop
    pacote.packument = noop
  })

  owner.exec(['add', 'foo', '@npmcli/no-owners-pkg'], err => {
    t.error(err, 'npm owner add <user> <pkg>')
    t.equal(result, '+ foo (@npmcli/no-owners-pkg)', 'should output add result')
    t.end()
  })
})

t.test('owner add no user', t => {
  result = ''
  t.teardown(() => {
    result = ''
  })

  owner.exec(['add'], err => {
    t.match(err, /usage instructions/, 'should throw usage instructions if user provided')
    t.end()
  })
})

t.test('owner add no pkg global', t => {
  t.teardown(() => {
    npm.config.set('global', false)
  })
  npm.config.set('global', true)

  owner.exec(['add', 'gar'], err => {
    t.match(err, /usage instructions/, 'should throw usage instructions if user provided')
    t.end()
  })
})

t.test('owner add <user> no cwd package', t => {
  result = ''
  t.teardown(() => {
    result = ''
  })

  owner.exec(['add', 'foo'], err => {
    t.match(err, /usage instructions/, 'should throw usage instructions if no user provided')
    t.end()
  })
})

t.test('owner rm <user> <pkg>', t => {
  t.plan(9)

  result = ''
  npmFetch.json = async (uri, opts) => {
    // retrieve user info from couchdb request
    if (uri === '/-/user/org.couchdb.user:ruyadorno') {
      t.ok('should request user info')
      t.match(opts, { ...npm.flatOptions }, 'should use expected opts')
      return {
        _id: 'org.couchdb.user:ruyadorno',
        email: 'ruyadorno@hotmail.com',
        name: 'ruyadorno',
      }
    } else if (uri === '/@npmcli%2fmap-workspaces/-rev/1-foobaaa1') {
      t.ok('should put changed owner')
      t.match(opts, {
        ...npm.flatOptions,
        method: 'PUT',
        body: {
          _rev: '1-foobaaa1',
        },
        otp: '123456',
        spec: {
          name: '@npmcli/map-workspaces',
        },
      }, 'should use expected opts')
      t.same(
        opts.body.maintainers,
        npmcliMaintainers.filter(m => m.name !== 'ruyadorno'),
        'should contain expected new owners, removing requested user'
      )
      return {}
    } else
      t.fail(`unexpected fetch json call to: ${uri}`)
  }
  pacote.packument = async (spec, opts) => {
    t.equal(spec.name, '@npmcli/map-workspaces', 'should use expect pkg name')
    t.match(
      opts,
      {
        ...npm.flatOptions,
        fullMetadata: true,
      },
      'should forward expected options to pacote.packument'
    )
    return {
      _rev: '1-foobaaa1',
      maintainers: npmcliMaintainers,
    }
  }
  t.teardown(() => {
    result = ''
    npmFetch.json = noop
    pacote.packument = noop
  })

  owner.exec(['rm', 'ruyadorno', '@npmcli/map-workspaces'], err => {
    t.error(err, 'npm owner rm <user> <pkg>')
    t.equal(result, '- ruyadorno (@npmcli/map-workspaces)', 'should output rm result')
  })
})

t.test('owner rm <user> <pkg> not a current owner', t => {
  t.plan(3)

  result = ''
  npmlog.info = (title, msg) => {
    t.equal(title, 'owner rm', 'should log expected title')
    t.equal(msg, 'Not a package owner: foo', 'should log.info not a package owner msg')
  }
  npmFetch.json = async (uri, opts) => {
    // retrieve user info from couchdb request
    if (uri === '/-/user/org.couchdb.user:foo') {
      return {
        _id: 'org.couchdb.user:foo',
        email: 'foo@github.com',
        name: 'foo',
      }
    } else if (uri === '/@npmcli%2fmap-workspaces/-rev/1-foobaaa1')
      return {}
    else
      t.fail(`unexpected fetch json call to: ${uri}`)
  }
  pacote.packument = async (spec, opts) => {
    return {
      _rev: '1-foobaaa1',
      maintainers: npmcliMaintainers,
    }
  }
  t.teardown(() => {
    result = ''
    npmlog.info = noop
    npmFetch.json = noop
    pacote.packument = noop
  })

  owner.exec(['rm', 'foo', '@npmcli/map-workspaces'], err => {
    t.error(err, 'npm owner rm <user> <pkg> not a current owner')
  })
})

t.test('owner rm <user> cwd package', t => {
  result = ''
  readPackageNameResponse = '@npmcli/map-workspaces'
  npmFetch.json = async (uri, opts) => {
    // retrieve user info from couchdb request
    if (uri === '/-/user/org.couchdb.user:ruyadorno') {
      return {
        _id: 'org.couchdb.user:ruyadorno',
        email: 'ruyadorno@hotmail.com',
        name: 'ruyadorno',
      }
    } else if (uri === '/@npmcli%2fmap-workspaces/-rev/1-foobaaa1')
      return {}
    else
      t.fail(`unexpected fetch json call to uri: ${uri}`)
  }
  pacote.packument = async (spec, opts) => ({
    _rev: '1-foobaaa1',
    maintainers: npmcliMaintainers,
  })
  t.teardown(() => {
    result = ''
    readPackageNameResponse = null
    npmFetch.json = noop
    pacote.packument = noop
  })

  owner.exec(['rm', 'ruyadorno'], err => {
    t.error(err, 'npm owner rm <user> cwd package')
    t.equal(result, '- ruyadorno (@npmcli/map-workspaces)', 'should output rm result')
    t.end()
  })
})

t.test('owner rm <user> only user', t => {
  result = ''
  readPackageNameResponse = 'ipt'
  npmFetch.json = async (uri, opts) => {
    // retrieve user info from couchdb request
    if (uri === '/-/user/org.couchdb.user:ruyadorno') {
      return {
        _id: 'org.couchdb.user:ruyadorno',
        email: 'ruyadorno@hotmail.com',
        name: 'ruyadorno',
      }
    } else
      t.fail(`unexpected fetch json call to uri: ${uri}`)
  }
  pacote.packument = async (spec, opts) => ({
    _rev: '1-foobaaa1',
    maintainers: [{
      name: 'ruyadorno',
      email: 'ruyadorno@hotmail.com',
    }],
  })
  t.teardown(() => {
    result = ''
    readPackageNameResponse = null
    npmFetch.json = noop
    pacote.packument = noop
  })

  owner.exec(['rm', 'ruyadorno'], err => {
    t.equal(
      err.message,
      'Cannot remove all owners of a package. Add someone else first.',
      'should throw unable to remove unique owner message'
    )
    t.equal(err.code, 'EOWNERRM', 'should have expected error code')
    t.end()
  })
})

t.test('owner rm no user', t => {
  result = ''
  t.teardown(() => {
    result = ''
  })

  owner.exec(['rm'], err => {
    t.match(err, /usage instructions/, 'should throw usage instructions if no user provided to rm')
    t.end()
  })
})

t.test('owner rm no pkg global', t => {
  t.teardown(() => {
    npm.config.set('global', false)
  })
  npm.config.set('global', true)

  owner.exec(['rm', 'gar'], err => {
    t.match(err, /usage instructions/, 'should throw usage instructions if user provided')
    t.end()
  })
})

t.test('owner rm <user> no cwd package', t => {
  result = ''
  t.teardown(() => {
    result = ''
  })

  owner.exec(['rm', 'foo'], err => {
    t.match(err, /usage instructions/, 'should throw usage instructions if no user provided to rm')
    t.end()
  })
})

t.test('completion', async t => {
  const testComp = async (argv, expect) => {
    const res = await owner.completion({ conf: { argv: { remain: argv } } })
    t.strictSame(res, expect, argv.join(' '))
  }

  await Promise.all([
    testComp(['npm', 'foo'], []),
    testComp(['npm', 'owner'], ['add', 'rm', 'ls']),
    testComp(['npm', 'owner', 'add'], []),
    testComp(['npm', 'owner', 'ls'], []),
    testComp(['npm', 'owner', 'rm', 'foo'], []),
  ])

  // npm owner rm completion is async
  t.test('completion npm owner rm', async t => {
    t.plan(2)
    readPackageNameResponse = '@npmcli/map-workspaces'
    pacote.packument = async spec => {
      t.equal(spec.name, readPackageNameResponse, 'should use package spec')
      return {
        maintainers: npmcliMaintainers,
      }
    }
    t.teardown(() => {
      readPackageNameResponse = null
      pacote.packument = noop
    })

    const res = await owner.completion({ conf: { argv: { remain: ['npm', 'owner', 'rm'] } } })
    t.strictSame(res,
      ['nlf', 'ruyadorno', 'darcyclarke', 'isaacs'],
      'should return list of current owners'
    )
  })

  t.test('completion npm owner rm no cwd package', async t => {
    const res = await owner.completion({ conf: { argv: { remain: ['npm', 'owner', 'rm'] } } })
    t.strictSame(res, [], 'should have no owners to autocomplete if not cwd package')
    t.end()
  })

  t.test('completion npm owner rm global', async t => {
    t.teardown(() => {
      npm.config.set('global', false)
    })
    npm.config.set('global', true)
    const res = await owner.completion({ conf: { argv: { remain: ['npm', 'owner', 'rm'] } } })
    t.strictSame(res, [], 'should have no owners to autocomplete if global')
    t.end()
  })

  t.test('completion npm owner rm no owners found', async t => {
    t.plan(2)
    readPackageNameResponse = '@npmcli/map-workspaces'
    pacote.packument = async spec => {
      t.equal(spec.name, readPackageNameResponse, 'should use package spec')
      return {
        maintainers: [],
      }
    }
    t.teardown(() => {
      readPackageNameResponse = null
      pacote.packument = noop
    })

    const res = await owner.completion({ conf: { argv: { remain: ['npm', 'owner', 'rm'] } } })
    t.strictSame(res, [], 'should return no owners if not found')
  })

  t.end()
})

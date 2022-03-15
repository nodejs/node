const t = require('tap')
const { fake: mockNpm } = require('../../fixtures/mock-npm')

const output = []
const npm = mockNpm({
  flatOptions: {
    json: false,
    parseable: false,
    unicode: false,
  },
  config: {
    loglevel: 'info',
  },
  output: msg => {
    output.push(msg)
  },
})

const pkgTypes = {
  semver: 'package',
  '@npmcli': 'scope',
  npm: 'owner',
}

const now = Date.now()
let hookResponse = null
let hookArgs = null
const libnpmhook = {
  add: async (pkg, uri, secret, opts) => {
    hookArgs = { pkg, uri, secret, opts }
    return { id: 1, name: pkg.replace(/^@/, ''), type: pkgTypes[pkg], endpoint: uri }
  },
  ls: async opts => {
    hookArgs = opts
    let id = 0
    if (hookResponse) {
      return hookResponse
    }

    return Object.keys(pkgTypes).map(name => ({
      id: ++id,
      name: name.replace(/^@/, ''),
      type: pkgTypes[name],
      endpoint: 'https://google.com',
      last_delivery: id % 2 === 0 ? now : undefined,
    }))
  },
  rm: async (id, opts) => {
    hookArgs = { id, opts }
    const pkg = Object.keys(pkgTypes)[0]
    return {
      id: 1,
      name: pkg.replace(/^@/, ''),
      type: pkgTypes[pkg],
      endpoint: 'https://google.com',
    }
  },
  update: async (id, uri, secret, opts) => {
    hookArgs = { id, uri, secret, opts }
    const pkg = Object.keys(pkgTypes)[0]
    return { id, name: pkg.replace(/^@/, ''), type: pkgTypes[pkg], endpoint: uri }
  },
}

const Hook = t.mock('../../../lib/commands/hook.js', {
  '../../../lib/utils/otplease.js': async (opts, fn) => fn(opts),
  libnpmhook,
})
const hook = new Hook(npm)

t.test('npm hook no args', async t => {
  await t.rejects(hook.exec([]), hook.usage, 'throws usage with no arguments')
})

t.test('npm hook add', async t => {
  t.teardown(() => {
    hookArgs = null
    output.length = 0
  })

  await hook.exec(['add', 'semver', 'https://google.com', 'some-secret'])

  t.match(
    hookArgs,
    {
      pkg: 'semver',
      uri: 'https://google.com',
      secret: 'some-secret',
      opts: npm.flatOptions,
    },
    'provided the correct arguments to libnpmhook'
  )
  t.strictSame(output, ['+ semver  ->  https://google.com'], 'prints the correct output')
})

t.test('npm hook add - unicode output', async t => {
  npm.flatOptions.unicode = true
  t.teardown(() => {
    npm.flatOptions.unicode = false
    hookArgs = null
    output.length = 0
  })

  await hook.exec(['add', 'semver', 'https://google.com', 'some-secret'])

  t.match(
    hookArgs,
    {
      pkg: 'semver',
      uri: 'https://google.com',
      secret: 'some-secret',
      opts: npm.flatOptions,
    },
    'provided the correct arguments to libnpmhook'
  )
  t.strictSame(output, ['+ semver  ➜  https://google.com'], 'prints the correct output')
})

t.test('npm hook add - json output', async t => {
  npm.flatOptions.json = true
  t.teardown(() => {
    npm.flatOptions.json = false
    hookArgs = null
    output.length = 0
  })

  await hook.exec(['add', '@npmcli', 'https://google.com', 'some-secret'])

  t.match(
    hookArgs,
    {
      pkg: '@npmcli',
      uri: 'https://google.com',
      secret: 'some-secret',
      opts: npm.flatOptions,
    },
    'provided the correct arguments to libnpmhook'
  )
  t.strictSame(
    JSON.parse(output[0]),
    {
      id: 1,
      name: 'npmcli',
      endpoint: 'https://google.com',
      type: 'scope',
    },
    'prints the correct json output'
  )
})

t.test('npm hook add - parseable output', async t => {
  npm.flatOptions.parseable = true
  t.teardown(() => {
    npm.flatOptions.parseable = false
    hookArgs = null
    output.length = 0
  })

  await hook.exec(['add', '@npmcli', 'https://google.com', 'some-secret'])

  t.match(
    hookArgs,
    {
      pkg: '@npmcli',
      uri: 'https://google.com',
      secret: 'some-secret',
      opts: npm.flatOptions,
    },
    'provided the correct arguments to libnpmhook'
  )
  t.strictSame(
    output[0].split(/\t/),
    ['id', 'name', 'type', 'endpoint'],
    'prints the correct parseable output headers'
  )
  t.strictSame(
    output[1].split(/\t/),
    ['1', 'npmcli', 'scope', 'https://google.com'],
    'prints the correct parseable values'
  )
})

t.test('npm hook add - silent output', async t => {
  npm.config.set('loglevel', 'silent')
  t.teardown(() => {
    npm.config.set('loglevel', 'info')
    hookArgs = null
    output.length = 0
  })

  await hook.exec(['add', '@npmcli', 'https://google.com', 'some-secret'])

  t.match(
    hookArgs,
    {
      pkg: '@npmcli',
      uri: 'https://google.com',
      secret: 'some-secret',
      opts: npm.flatOptions,
    },
    'provided the correct arguments to libnpmhook'
  )
  t.strictSame(output, [], 'printed no output')
})

t.test('npm hook ls', async t => {
  t.teardown(() => {
    hookArgs = null
    output.length = 0
  })

  await hook.exec(['ls'])

  t.match(
    hookArgs,
    {
      ...npm.flatOptions,
      package: undefined,
    },
    'received the correct arguments'
  )
  t.equal(output[0], 'You have 3 hooks configured.', 'prints the correct header')
  const out = require('../../../lib/utils/ansi-trim')(output[1])
  t.match(out, /semver.*https:\/\/google.com.*\n.*\n.*never triggered/, 'prints package hook')
  t.match(out, /@npmcli.*https:\/\/google.com.*\n.*\n.*triggered just now/, 'prints scope hook')
  t.match(out, /~npm.*https:\/\/google.com.*\n.*\n.*never triggered/, 'prints owner hook')
})

t.test('npm hook ls, no results', async t => {
  hookResponse = []
  t.teardown(() => {
    hookResponse = null
    hookArgs = null
    output.length = 0
  })

  await hook.exec(['ls'])

  t.match(
    hookArgs,
    {
      ...npm.flatOptions,
      package: undefined,
    },
    'received the correct arguments'
  )
  t.equal(output[0], "You don't have any hooks configured yet.", 'prints the correct result')
})

t.test('npm hook ls, single result', async t => {
  hookResponse = [
    {
      id: 1,
      name: 'semver',
      type: 'package',
      endpoint: 'https://google.com',
    },
  ]

  t.teardown(() => {
    hookResponse = null
    hookArgs = null
    output.length = 0
  })

  await hook.exec(['ls'])

  t.match(
    hookArgs,
    {
      ...npm.flatOptions,
      package: undefined,
    },
    'received the correct arguments'
  )
  t.equal(output[0], 'You have one hook configured.', 'prints the correct header')
  const out = require('../../../lib/utils/ansi-trim')(output[1])
  t.match(out, /semver.*https:\/\/google.com.*\n.*\n.*never triggered/, 'prints package hook')
})

t.test('npm hook ls - json output', async t => {
  npm.flatOptions.json = true
  t.teardown(() => {
    npm.flatOptions.json = false
    hookArgs = null
    output.length = 0
  })

  await hook.exec(['ls'])

  t.match(
    hookArgs,
    {
      ...npm.flatOptions,
      package: undefined,
    },
    'received the correct arguments'
  )
  const out = JSON.parse(output[0])
  t.match(
    out,
    [
      {
        id: 1,
        name: 'semver',
        type: 'package',
        endpoint: 'https://google.com',
      },
      {
        id: 2,
        name: 'npmcli',
        type: 'scope',
        endpoint: 'https://google.com',
      },
      {
        id: 3,
        name: 'npm',
        type: 'owner',
        endpoint: 'https://google.com',
      },
    ],
    'prints the correct output'
  )
})

t.test('npm hook ls - parseable output', async t => {
  npm.flatOptions.parseable = true
  t.teardown(() => {
    npm.flatOptions.parseable = false
    hookArgs = null
    output.length = 0
  })

  await hook.exec(['ls'])

  t.match(
    hookArgs,
    {
      ...npm.flatOptions,
      package: undefined,
    },
    'received the correct arguments'
  )
  t.strictSame(
    output.map(line => line.split(/\t/)),
    [
      ['id', 'name', 'type', 'endpoint', 'last_delivery'],
      ['1', 'semver', 'package', 'https://google.com', ''],
      ['2', 'npmcli', 'scope', 'https://google.com', `${now}`],
      ['3', 'npm', 'owner', 'https://google.com', ''],
    ],
    'prints the correct result'
  )
})

t.test('npm hook ls - silent output', async t => {
  npm.config.set('loglevel', 'silent')
  t.teardown(() => {
    npm.config.set('loglevel', 'info')
    hookArgs = null
    output.length = 0
  })

  await hook.exec(['ls'])

  t.match(
    hookArgs,
    {
      ...npm.flatOptions,
      package: undefined,
    },
    'received the correct arguments'
  )
  t.strictSame(output, [], 'printed no output')
})

t.test('npm hook rm', async t => {
  t.teardown(() => {
    hookArgs = null
    output.length = 0
  })

  await hook.exec(['rm', '1'])

  t.match(
    hookArgs,
    {
      id: '1',
      opts: npm.flatOptions,
    },
    'received the correct arguments'
  )
  t.strictSame(output, ['- semver  X  https://google.com'], 'printed the correct output')
})

t.test('npm hook rm - unicode output', async t => {
  npm.flatOptions.unicode = true
  t.teardown(() => {
    npm.flatOptions.unicode = false
    hookArgs = null
    output.length = 0
  })

  await hook.exec(['rm', '1'])

  t.match(
    hookArgs,
    {
      id: '1',
      opts: npm.flatOptions,
    },
    'received the correct arguments'
  )
  t.strictSame(output, ['- semver  ✘  https://google.com'], 'printed the correct output')
})

t.test('npm hook rm - silent output', async t => {
  npm.config.set('loglevel', 'silent')
  t.teardown(() => {
    npm.config.set('loglevel', 'info')
    hookArgs = null
    output.length = 0
  })

  await hook.exec(['rm', '1'])

  t.match(
    hookArgs,
    {
      id: '1',
      opts: npm.flatOptions,
    },
    'received the correct arguments'
  )
  t.strictSame(output, [], 'printed no output')
})

t.test('npm hook rm - json output', async t => {
  npm.flatOptions.json = true
  t.teardown(() => {
    npm.flatOptions.json = false
    hookArgs = null
    output.length = 0
  })

  await hook.exec(['rm', '1'])

  t.match(
    hookArgs,
    {
      id: '1',
      opts: npm.flatOptions,
    },
    'received the correct arguments'
  )
  t.strictSame(
    JSON.parse(output[0]),
    {
      id: 1,
      name: 'semver',
      type: 'package',
      endpoint: 'https://google.com',
    },
    'printed correct output'
  )
})

t.test('npm hook rm - parseable output', async t => {
  npm.flatOptions.parseable = true
  t.teardown(() => {
    npm.flatOptions.parseable = false
    hookArgs = null
    output.length = 0
  })

  await hook.exec(['rm', '1'])

  t.match(
    hookArgs,
    {
      id: '1',
      opts: npm.flatOptions,
    },
    'received the correct arguments'
  )
  t.strictSame(
    output.map(line => line.split(/\t/)),
    [
      ['id', 'name', 'type', 'endpoint'],
      ['1', 'semver', 'package', 'https://google.com'],
    ],
    'printed correct output'
  )
})

t.test('npm hook update', async t => {
  t.teardown(() => {
    hookArgs = null
    output.length = 0
  })

  await hook.exec(['update', '1', 'https://google.com', 'some-secret'])

  t.match(
    hookArgs,
    {
      id: '1',
      uri: 'https://google.com',
      secret: 'some-secret',
      opts: npm.flatOptions,
    },
    'received the correct arguments'
  )
  t.strictSame(output, ['+ semver  ->  https://google.com'], 'printed the correct output')
})

t.test('npm hook update - unicode', async t => {
  npm.flatOptions.unicode = true
  t.teardown(() => {
    npm.flatOptions.unicode = false
    hookArgs = null
    output.length = 0
  })

  await hook.exec(['update', '1', 'https://google.com', 'some-secret'])

  t.match(
    hookArgs,
    {
      id: '1',
      uri: 'https://google.com',
      secret: 'some-secret',
      opts: npm.flatOptions,
    },
    'received the correct arguments'
  )
  t.strictSame(output, ['+ semver  ➜  https://google.com'], 'printed the correct output')
})

t.test('npm hook update - json output', async t => {
  npm.flatOptions.json = true
  t.teardown(() => {
    npm.flatOptions.json = false
    hookArgs = null
    output.length = 0
  })

  await hook.exec(['update', '1', 'https://google.com', 'some-secret'])

  t.match(
    hookArgs,
    {
      id: '1',
      uri: 'https://google.com',
      secret: 'some-secret',
      opts: npm.flatOptions,
    },
    'received the correct arguments'
  )
  t.strictSame(
    JSON.parse(output[0]),
    {
      id: '1',
      name: 'semver',
      type: 'package',
      endpoint: 'https://google.com',
    },
    'printed the correct output'
  )
})

t.test('npm hook update - parseable output', async t => {
  npm.flatOptions.parseable = true
  t.teardown(() => {
    npm.flatOptions.parseable = false
    hookArgs = null
    output.length = 0
  })

  await hook.exec(['update', '1', 'https://google.com', 'some-secret'])

  t.match(
    hookArgs,
    {
      id: '1',
      uri: 'https://google.com',
      secret: 'some-secret',
      opts: npm.flatOptions,
    },
    'received the correct arguments'
  )
  t.strictSame(
    output.map(line => line.split(/\t/)),
    [
      ['id', 'name', 'type', 'endpoint'],
      ['1', 'semver', 'package', 'https://google.com'],
    ],
    'printed the correct output'
  )
})

t.test('npm hook update - silent output', async t => {
  npm.config.set('loglevel', 'silent')
  t.teardown(() => {
    npm.config.set('loglevel', 'info')
    hookArgs = null
    output.length = 0
  })

  await hook.exec(['update', '1', 'https://google.com', 'some-secret'])

  t.match(
    hookArgs,
    {
      id: '1',
      uri: 'https://google.com',
      secret: 'some-secret',
      opts: npm.flatOptions,
    },
    'received the correct arguments'
  )
  t.strictSame(output, [], 'printed no output')
})

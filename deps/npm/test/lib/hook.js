const t = require('tap')

const output = []
const npm = {
  flatOptions: {
    json: false,
    parseable: false,
    silent: false,
    loglevel: 'info',
    unicode: false,
  },
  output: (msg) => {
    output.push(msg)
  },
}

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
  ls: async (opts) => {
    hookArgs = opts
    let id = 0
    if (hookResponse)
      return hookResponse

    return Object.keys(pkgTypes).map((name) => ({
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
    return { id: 1, name: pkg.replace(/^@/, ''), type: pkgTypes[pkg], endpoint: 'https://google.com' }
  },
  update: async (id, uri, secret, opts) => {
    hookArgs = { id, uri, secret, opts }
    const pkg = Object.keys(pkgTypes)[0]
    return { id, name: pkg.replace(/^@/, ''), type: pkgTypes[pkg], endpoint: uri }
  },
}

const Hook = t.mock('../../lib/hook.js', {
  '../../lib/utils/otplease.js': async (opts, fn) => fn(opts),
  libnpmhook,
})
const hook = new Hook(npm)

t.test('npm hook no args', t => {
  return hook.exec([], (err) => {
    t.match(err, /npm hook add/, 'throws usage with no arguments')
    t.end()
  })
})

t.test('npm hook add', t => {
  t.teardown(() => {
    hookArgs = null
    output.length = 0
  })

  return hook.exec(['add', 'semver', 'https://google.com', 'some-secret'], (err) => {
    if (err)
      throw err

    t.strictSame(hookArgs, {
      pkg: 'semver',
      uri: 'https://google.com',
      secret: 'some-secret',
      opts: npm.flatOptions,
    }, 'provided the correct arguments to libnpmhook')
    t.strictSame(output, ['+ semver  ->  https://google.com'], 'prints the correct output')
    t.end()
  })
})

t.test('npm hook add - unicode output', t => {
  npm.flatOptions.unicode = true
  t.teardown(() => {
    npm.flatOptions.unicode = false
    hookArgs = null
    output.length = 0
  })

  return hook.exec(['add', 'semver', 'https://google.com', 'some-secret'], (err) => {
    if (err)
      throw err

    t.strictSame(hookArgs, {
      pkg: 'semver',
      uri: 'https://google.com',
      secret: 'some-secret',
      opts: npm.flatOptions,
    }, 'provided the correct arguments to libnpmhook')
    t.strictSame(output, ['+ semver  ➜  https://google.com'], 'prints the correct output')
    t.end()
  })
})

t.test('npm hook add - json output', t => {
  npm.flatOptions.json = true
  t.teardown(() => {
    npm.flatOptions.json = false
    hookArgs = null
    output.length = 0
  })

  return hook.exec(['add', '@npmcli', 'https://google.com', 'some-secret'], (err) => {
    if (err)
      throw err

    t.strictSame(hookArgs, {
      pkg: '@npmcli',
      uri: 'https://google.com',
      secret: 'some-secret',
      opts: npm.flatOptions,
    }, 'provided the correct arguments to libnpmhook')
    t.strictSame(JSON.parse(output[0]), {
      id: 1,
      name: 'npmcli',
      endpoint: 'https://google.com',
      type: 'scope',
    }, 'prints the correct json output')
    t.end()
  })
})

t.test('npm hook add - parseable output', t => {
  npm.flatOptions.parseable = true
  t.teardown(() => {
    npm.flatOptions.parseable = false
    hookArgs = null
    output.length = 0
  })

  return hook.exec(['add', '@npmcli', 'https://google.com', 'some-secret'], (err) => {
    if (err)
      throw err

    t.strictSame(hookArgs, {
      pkg: '@npmcli',
      uri: 'https://google.com',
      secret: 'some-secret',
      opts: npm.flatOptions,
    }, 'provided the correct arguments to libnpmhook')
    t.strictSame(output[0].split(/\t/), [
      'id', 'name', 'type', 'endpoint',
    ], 'prints the correct parseable output headers')
    t.strictSame(output[1].split(/\t/), [
      '1', 'npmcli', 'scope', 'https://google.com',
    ], 'prints the correct parseable values')
    t.end()
  })
})

t.test('npm hook add - silent output', t => {
  npm.flatOptions.silent = true
  t.teardown(() => {
    npm.flatOptions.silent = false
    hookArgs = null
    output.length = 0
  })

  return hook.exec(['add', '@npmcli', 'https://google.com', 'some-secret'], (err) => {
    if (err)
      throw err

    t.strictSame(hookArgs, {
      pkg: '@npmcli',
      uri: 'https://google.com',
      secret: 'some-secret',
      opts: npm.flatOptions,
    }, 'provided the correct arguments to libnpmhook')
    t.strictSame(output, [], 'printed no output')
    t.end()
  })
})

t.test('npm hook ls', t => {
  t.teardown(() => {
    hookArgs = null
    output.length = 0
  })

  return hook.exec(['ls'], (err) => {
    if (err)
      throw err

    t.strictSame(hookArgs, {
      ...npm.flatOptions,
      package: undefined,
    }, 'received the correct arguments')
    t.equal(output[0], 'You have 3 hooks configured.', 'prints the correct header')
    const out = require('../../lib/utils/ansi-trim')(output[1])
    t.match(out, /semver.*https:\/\/google.com.*\n.*\n.*never triggered/, 'prints package hook')
    t.match(out, /@npmcli.*https:\/\/google.com.*\n.*\n.*triggered just now/, 'prints scope hook')
    t.match(out, /~npm.*https:\/\/google.com.*\n.*\n.*never triggered/, 'prints owner hook')
    t.end()
  })
})

t.test('npm hook ls, no results', t => {
  hookResponse = []
  t.teardown(() => {
    hookResponse = null
    hookArgs = null
    output.length = 0
  })

  return hook.exec(['ls'], (err) => {
    if (err)
      throw err

    t.strictSame(hookArgs, {
      ...npm.flatOptions,
      package: undefined,
    }, 'received the correct arguments')
    t.equal(output[0], 'You don\'t have any hooks configured yet.', 'prints the correct result')
    t.end()
  })
})

t.test('npm hook ls, single result', t => {
  hookResponse = [{
    id: 1,
    name: 'semver',
    type: 'package',
    endpoint: 'https://google.com',
  }]

  t.teardown(() => {
    hookResponse = null
    hookArgs = null
    output.length = 0
  })

  return hook.exec(['ls'], (err) => {
    if (err)
      throw err

    t.strictSame(hookArgs, {
      ...npm.flatOptions,
      package: undefined,
    }, 'received the correct arguments')
    t.equal(output[0], 'You have one hook configured.', 'prints the correct header')
    const out = require('../../lib/utils/ansi-trim')(output[1])
    t.match(out, /semver.*https:\/\/google.com.*\n.*\n.*never triggered/, 'prints package hook')
    t.end()
  })
})

t.test('npm hook ls - json output', t => {
  npm.flatOptions.json = true
  t.teardown(() => {
    npm.flatOptions.json = false
    hookArgs = null
    output.length = 0
  })

  return hook.exec(['ls'], (err) => {
    if (err)
      throw err

    t.strictSame(hookArgs, {
      ...npm.flatOptions,
      package: undefined,
    }, 'received the correct arguments')
    const out = JSON.parse(output[0])
    t.match(out, [{
      id: 1,
      name: 'semver',
      type: 'package',
      endpoint: 'https://google.com',
    }, {
      id: 2,
      name: 'npmcli',
      type: 'scope',
      endpoint: 'https://google.com',
    }, {
      id: 3,
      name: 'npm',
      type: 'owner',
      endpoint: 'https://google.com',
    }], 'prints the correct output')
    t.end()
  })
})

t.test('npm hook ls - parseable output', t => {
  npm.flatOptions.parseable = true
  t.teardown(() => {
    npm.flatOptions.parseable = false
    hookArgs = null
    output.length = 0
  })

  return hook.exec(['ls'], (err) => {
    if (err)
      throw err

    t.strictSame(hookArgs, {
      ...npm.flatOptions,
      package: undefined,
    }, 'received the correct arguments')
    t.strictSame(output.map(line => line.split(/\t/)), [
      ['id', 'name', 'type', 'endpoint', 'last_delivery'],
      ['1', 'semver', 'package', 'https://google.com', ''],
      ['2', 'npmcli', 'scope', 'https://google.com', `${now}`],
      ['3', 'npm', 'owner', 'https://google.com', ''],
    ], 'prints the correct result')
    t.end()
  })
})

t.test('npm hook ls - silent output', t => {
  npm.flatOptions.silent = true
  t.teardown(() => {
    npm.flatOptions.silent = false
    hookArgs = null
    output.length = 0
  })

  return hook.exec(['ls'], (err) => {
    if (err)
      throw err

    t.strictSame(hookArgs, {
      ...npm.flatOptions,
      package: undefined,
    }, 'received the correct arguments')
    t.strictSame(output, [], 'printed no output')
    t.end()
  })
})

t.test('npm hook rm', t => {
  t.teardown(() => {
    hookArgs = null
    output.length = 0
  })

  return hook.exec(['rm', '1'], (err) => {
    if (err)
      throw err

    t.strictSame(hookArgs, {
      id: '1',
      opts: npm.flatOptions,
    }, 'received the correct arguments')
    t.strictSame(output, [
      '- semver  X  https://google.com',
    ], 'printed the correct output')
    t.end()
  })
})

t.test('npm hook rm - unicode output', t => {
  npm.flatOptions.unicode = true
  t.teardown(() => {
    npm.flatOptions.unicode = false
    hookArgs = null
    output.length = 0
  })

  return hook.exec(['rm', '1'], (err) => {
    if (err)
      throw err

    t.strictSame(hookArgs, {
      id: '1',
      opts: npm.flatOptions,
    }, 'received the correct arguments')
    t.strictSame(output, [
      '- semver  ✘  https://google.com',
    ], 'printed the correct output')
    t.end()
  })
})

t.test('npm hook rm - silent output', t => {
  npm.flatOptions.silent = true
  t.teardown(() => {
    npm.flatOptions.silent = false
    hookArgs = null
    output.length = 0
  })

  return hook.exec(['rm', '1'], (err) => {
    if (err)
      throw err

    t.strictSame(hookArgs, {
      id: '1',
      opts: npm.flatOptions,
    }, 'received the correct arguments')
    t.strictSame(output, [], 'printed no output')
    t.end()
  })
})

t.test('npm hook rm - json output', t => {
  npm.flatOptions.json = true
  t.teardown(() => {
    npm.flatOptions.json = false
    hookArgs = null
    output.length = 0
  })

  return hook.exec(['rm', '1'], (err) => {
    if (err)
      throw err

    t.strictSame(hookArgs, {
      id: '1',
      opts: npm.flatOptions,
    }, 'received the correct arguments')
    t.strictSame(JSON.parse(output[0]), {
      id: 1,
      name: 'semver',
      type: 'package',
      endpoint: 'https://google.com',
    }, 'printed correct output')
    t.end()
  })
})

t.test('npm hook rm - parseable output', t => {
  npm.flatOptions.parseable = true
  t.teardown(() => {
    npm.flatOptions.parseable = false
    hookArgs = null
    output.length = 0
  })

  return hook.exec(['rm', '1'], (err) => {
    if (err)
      throw err

    t.strictSame(hookArgs, {
      id: '1',
      opts: npm.flatOptions,
    }, 'received the correct arguments')
    t.strictSame(output.map(line => line.split(/\t/)), [
      ['id', 'name', 'type', 'endpoint'],
      ['1', 'semver', 'package', 'https://google.com'],
    ], 'printed correct output')
    t.end()
  })
})

t.test('npm hook update', t => {
  t.teardown(() => {
    hookArgs = null
    output.length = 0
  })

  return hook.exec(['update', '1', 'https://google.com', 'some-secret'], (err) => {
    if (err)
      throw err

    t.strictSame(hookArgs, {
      id: '1',
      uri: 'https://google.com',
      secret: 'some-secret',
      opts: npm.flatOptions,
    }, 'received the correct arguments')
    t.strictSame(output, [
      '+ semver  ->  https://google.com',
    ], 'printed the correct output')
    t.end()
  })
})

t.test('npm hook update - unicode', t => {
  npm.flatOptions.unicode = true
  t.teardown(() => {
    npm.flatOptions.unicode = false
    hookArgs = null
    output.length = 0
  })

  return hook.exec(['update', '1', 'https://google.com', 'some-secret'], (err) => {
    if (err)
      throw err

    t.strictSame(hookArgs, {
      id: '1',
      uri: 'https://google.com',
      secret: 'some-secret',
      opts: npm.flatOptions,
    }, 'received the correct arguments')
    t.strictSame(output, [
      '+ semver  ➜  https://google.com',
    ], 'printed the correct output')
    t.end()
  })
})

t.test('npm hook update - json output', t => {
  npm.flatOptions.json = true
  t.teardown(() => {
    npm.flatOptions.json = false
    hookArgs = null
    output.length = 0
  })

  return hook.exec(['update', '1', 'https://google.com', 'some-secret'], (err) => {
    if (err)
      throw err

    t.strictSame(hookArgs, {
      id: '1',
      uri: 'https://google.com',
      secret: 'some-secret',
      opts: npm.flatOptions,
    }, 'received the correct arguments')
    t.strictSame(JSON.parse(output[0]), {
      id: '1',
      name: 'semver',
      type: 'package',
      endpoint: 'https://google.com',
    }, 'printed the correct output')
    t.end()
  })
})

t.test('npm hook update - parseable output', t => {
  npm.flatOptions.parseable = true
  t.teardown(() => {
    npm.flatOptions.parseable = false
    hookArgs = null
    output.length = 0
  })

  return hook.exec(['update', '1', 'https://google.com', 'some-secret'], (err) => {
    if (err)
      throw err

    t.strictSame(hookArgs, {
      id: '1',
      uri: 'https://google.com',
      secret: 'some-secret',
      opts: npm.flatOptions,
    }, 'received the correct arguments')
    t.strictSame(output.map(line => line.split(/\t/)), [
      ['id', 'name', 'type', 'endpoint'],
      ['1', 'semver', 'package', 'https://google.com'],
    ], 'printed the correct output')
    t.end()
  })
})

t.test('npm hook update - silent output', t => {
  npm.flatOptions.silent = true
  t.teardown(() => {
    npm.flatOptions.silent = false
    hookArgs = null
    output.length = 0
  })

  return hook.exec(['update', '1', 'https://google.com', 'some-secret'], (err) => {
    if (err)
      throw err

    t.strictSame(hookArgs, {
      id: '1',
      uri: 'https://google.com',
      secret: 'some-secret',
      opts: npm.flatOptions,
    }, 'received the correct arguments')
    t.strictSame(output, [], 'printed no output')
    t.end()
  })
})

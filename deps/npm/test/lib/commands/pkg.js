const { resolve } = require('path')
const { readFileSync } = require('fs')
const t = require('tap')
const { fake: mockNpm } = require('../../fixtures/mock-npm')

const redactCwd = (path) => {
  const normalizePath = p => p
    .replace(/\\+/g, '/')
    .replace(/\r\n/g, '\n')
  return normalizePath(path)
    .replace(new RegExp(normalizePath(process.cwd()), 'g'), '{CWD}')
}

t.cleanSnapshot = (str) => redactCwd(str)

let OUTPUT = ''
const config = {
  global: false,
  force: false,
  'pkg-cast': 'string',
}
const npm = mockNpm({
  localPrefix: t.testdirName,
  config,
  output: (str) => {
    OUTPUT += str
  },
})

const Pkg = require('../../../lib/commands/pkg.js')
const pkg = new Pkg(npm)

const readPackageJson = (path) => {
  path = path || npm.localPrefix
  return JSON.parse(readFileSync(resolve(path, 'package.json'), 'utf8'))
}

t.afterEach(() => {
  config.global = false
  config.json = false
  npm.localPrefix = t.testdirName
  OUTPUT = ''
})

t.test('no args', async t => {
  await t.rejects(
    pkg.exec([]),
    { code: 'EUSAGE' },
    'should throw usage error'
  )
})

t.test('no global mode', async t => {
  config.global = true
  await t.rejects(
    pkg.exec(['get', 'foo']),
    { code: 'EPKGGLOBAL' },
    'should throw no global mode error'
  )
})

t.test('get no args', async t => {
  npm.localPrefix = t.testdir({
    'package.json': JSON.stringify({
      name: 'foo',
      version: '1.1.1',
    }),
  })

  await pkg.exec(['get'])

  t.strictSame(
    JSON.parse(OUTPUT),
    {
      name: 'foo',
      version: '1.1.1',
    },
    'should print package.json content'
  )
})

t.test('get single arg', async t => {
  npm.localPrefix = t.testdir({
    'package.json': JSON.stringify({
      name: 'foo',
      version: '1.1.1',
    }),
  })

  await pkg.exec(['get', 'version'])

  t.strictSame(
    JSON.parse(OUTPUT),
    '1.1.1',
    'should print retrieved package.json field'
  )
})

t.test('get nested arg', async t => {
  npm.localPrefix = t.testdir({
    'package.json': JSON.stringify({
      name: 'foo',
      version: '1.1.1',
      scripts: {
        test: 'node test.js',
      },
    }),
  })

  await pkg.exec(['get', 'scripts.test'])

  t.strictSame(
    JSON.parse(OUTPUT),
    'node test.js',
    'should print retrieved nested field'
  )
})

t.test('get array field', async t => {
  const files = [
    'index.js',
    'cli.js',
  ]
  npm.localPrefix = t.testdir({
    'package.json': JSON.stringify({
      name: 'foo',
      version: '1.1.1',
      files,
    }),
  })

  await pkg.exec(['get', 'files'])

  t.strictSame(
    JSON.parse(OUTPUT),
    files,
    'should print retrieved array field'
  )
})

t.test('get array item', async t => {
  const files = [
    'index.js',
    'cli.js',
  ]
  npm.localPrefix = t.testdir({
    'package.json': JSON.stringify({
      name: 'foo',
      version: '1.1.1',
      files,
    }),
  })

  await pkg.exec(['get', 'files[0]'])

  t.strictSame(
    JSON.parse(OUTPUT),
    'index.js',
    'should print retrieved array field'
  )
})

t.test('get array nested items notation', async t => {
  const contributors = [
    {
      name: 'Ruy',
      url: 'http://example.com/ruy',
    },
    {
      name: 'Gar',
      url: 'http://example.com/gar',
    },
  ]
  npm.localPrefix = t.testdir({
    'package.json': JSON.stringify({
      name: 'foo',
      version: '1.1.1',
      contributors,
    }),
  })

  await pkg.exec(['get', 'contributors.name'])
  t.strictSame(
    JSON.parse(OUTPUT),
    {
      'contributors[0].name': 'Ruy',
      'contributors[1].name': 'Gar',
    },
    'should print json result containing matching results'
  )
})

t.test('set no args', async t => {
  npm.localPrefix = t.testdir({
    'package.json': JSON.stringify({ name: 'foo' }),
  })
  await t.rejects(
    pkg.exec(['set']),
    { code: 'EUSAGE' },
    'should throw an error if no args'
  )
})

t.test('set missing value', async t => {
  npm.localPrefix = t.testdir({
    'package.json': JSON.stringify({ name: 'foo' }),
  })
  await t.rejects(
    pkg.exec(['set', 'key=']),
    { code: 'EUSAGE' },
    'should throw an error if missing value'
  )
})

t.test('set missing key', async t => {
  npm.localPrefix = t.testdir({
    'package.json': JSON.stringify({ name: 'foo' }),
  })
  await t.rejects(
    pkg.exec(['set', '=value']),
    { code: 'EUSAGE' },
    'should throw an error if missing key'
  )
})

t.test('set single field', async t => {
  const json = {
    name: 'foo',
    version: '1.1.1',
  }
  npm.localPrefix = t.testdir({
    'package.json': JSON.stringify(json),
  })

  await pkg.exec(['set', 'description=Awesome stuff'])
  t.strictSame(
    readPackageJson(),
    {
      ...json,
      description: 'Awesome stuff',
    },
    'should add single field to package.json'
  )
})

t.test('push to array syntax', async t => {
  const json = {
    name: 'foo',
    version: '1.1.1',
    keywords: [
      'foo',
    ],
  }
  npm.localPrefix = t.testdir({
    'package.json': JSON.stringify(json),
  })

  await pkg.exec(['set', 'keywords[]=bar', 'keywords[]=baz'])
  t.strictSame(
    readPackageJson(),
    {
      ...json,
      keywords: [
        'foo',
        'bar',
        'baz',
      ],
    },
    'should append to arrays using empty bracket syntax'
  )
})

t.test('set multiple fields', async t => {
  const json = {
    name: 'foo',
    version: '1.1.1',
  }
  npm.localPrefix = t.testdir({
    'package.json': JSON.stringify(json),
  })

  await pkg.exec(['set', 'bin.foo=foo.js', 'scripts.test=node test.js'])
  t.strictSame(
    readPackageJson(),
    {
      ...json,
      bin: {
        foo: 'foo.js',
      },
      scripts: {
        test: 'node test.js',
      },
    },
    'should add single field to package.json'
  )
})

t.test('set = separate value', async t => {
  const json = {
    name: 'foo',
    version: '1.1.1',
  }
  npm.localPrefix = t.testdir({
    'package.json': JSON.stringify(json),
  })

  await pkg.exec(['set', 'tap[test-env][0]=LC_ALL=sk'])
  t.strictSame(
    readPackageJson(),
    {
      ...json,
      tap: {
        'test-env': [
          'LC_ALL=sk',
        ],
      },
    },
    'should add single field to package.json'
  )
})

t.test('set --json', async t => {
  config.json = true
  npm.localPrefix = t.testdir({
    'package.json': JSON.stringify({
      name: 'foo',
      version: '1.1.1',
    }),
  })

  await pkg.exec(['set', 'private=true'])
  t.strictSame(
    readPackageJson(),
    {
      name: 'foo',
      version: '1.1.1',
      private: true,
    },
    'should add boolean field to package.json'
  )

  await pkg.exec(['set', 'tap.timeout=60'])
  t.strictSame(
    readPackageJson(),
    {
      name: 'foo',
      version: '1.1.1',
      private: true,
      tap: {
        timeout: 60,
      },
    },
    'should add number field to package.json'
  )

  await pkg.exec(['set', 'foo={ "bar": { "baz": "BAZ" } }'])
  t.strictSame(
    readPackageJson(),
    {
      name: 'foo',
      version: '1.1.1',
      private: true,
      tap: {
        timeout: 60,
      },
      foo: {
        bar: {
          baz: 'BAZ',
        },
      },
    },
    'should add object field to package.json'
  )

  await pkg.exec(['set', 'workspaces=["packages/*"]'])
  t.strictSame(
    readPackageJson(),
    {
      name: 'foo',
      version: '1.1.1',
      private: true,
      workspaces: [
        'packages/*',
      ],
      tap: {
        timeout: 60,
      },
      foo: {
        bar: {
          baz: 'BAZ',
        },
      },
    },
    'should add object field to package.json'
  )

  await pkg.exec(['set', 'description="awesome"'])
  t.strictSame(
    readPackageJson(),
    {
      name: 'foo',
      version: '1.1.1',
      description: 'awesome',
      private: true,
      workspaces: [
        'packages/*',
      ],
      tap: {
        timeout: 60,
      },
      foo: {
        bar: {
          baz: 'BAZ',
        },
      },
    },
    'should add object field to package.json'
  )
})

t.test('delete no args', async t => {
  npm.localPrefix = t.testdir({
    'package.json': JSON.stringify({ name: 'foo' }),
  })
  await t.rejects(
    pkg.exec(['delete']),
    { code: 'EUSAGE' },
    'should throw an error if deleting no args'
  )
})

t.test('delete invalid key', async t => {
  npm.localPrefix = t.testdir({
    'package.json': JSON.stringify({ name: 'foo' }),
  })
  await t.rejects(
    pkg.exec(['delete', '']),
    { code: 'EUSAGE' },
    'should throw an error if deleting invalid args'
  )
})

t.test('delete single field', async t => {
  npm.localPrefix = t.testdir({
    'package.json': JSON.stringify({
      name: 'foo',
      version: '1.0.0',
    }),
  })
  await pkg.exec(['delete', 'version'])
  t.strictSame(
    readPackageJson(),
    {
      name: 'foo',
    },
    'should delete single field from package.json'
  )
})

t.test('delete multiple field', async t => {
  npm.localPrefix = t.testdir({
    'package.json': JSON.stringify({
      name: 'foo',
      version: '1.0.0',
      description: 'awesome',
    }),
  })
  await pkg.exec(['delete', 'version', 'description'])
  t.strictSame(
    readPackageJson(),
    {
      name: 'foo',
    },
    'should delete multiple fields from package.json'
  )
})

t.test('delete nested field', async t => {
  npm.localPrefix = t.testdir({
    'package.json': JSON.stringify({
      name: 'foo',
      version: '1.0.0',
      info: {
        foo: {
          bar: [
            {
              baz: 'deleteme',
            },
          ],
        },
      },
    }),
  })
  await pkg.exec(['delete', 'info.foo.bar[0].baz'])
  t.strictSame(
    readPackageJson(),
    {
      name: 'foo',
      version: '1.0.0',
      info: {
        foo: {
          bar: [
            {},
          ],
        },
      },
    },
    'should delete nested fields from package.json'
  )
})

t.test('workspaces', async t => {
  npm.localPrefix = t.testdir({
    'package.json': JSON.stringify({
      name: 'root',
      version: '1.0.0',
      workspaces: [
        'packages/*',
      ],
    }),
    packages: {
      a: {
        'package.json': JSON.stringify({
          name: 'a',
          version: '1.0.0',
        }),
      },
      b: {
        'package.json': JSON.stringify({
          name: 'b',
          version: '1.2.3',
        }),
      },
    },
  })

  await pkg.execWorkspaces(['get', 'name', 'version'], [])

  t.strictSame(
    JSON.parse(OUTPUT),
    {
      a: {
        name: 'a',
        version: '1.0.0',
      },
      b: {
        name: 'b',
        version: '1.2.3',
      },
    },
    'should return expected result for configured workspaces'
  )

  await pkg.execWorkspaces(['set', 'funding=http://example.com'], [])

  t.strictSame(
    readPackageJson(resolve(npm.localPrefix, 'packages/a')),
    {
      name: 'a',
      version: '1.0.0',
      funding: 'http://example.com',
    },
    'should add field to workspace a'
  )

  t.strictSame(
    readPackageJson(resolve(npm.localPrefix, 'packages/b')),
    {
      name: 'b',
      version: '1.2.3',
      funding: 'http://example.com',
    },
    'should add field to workspace b'
  )

  await pkg.execWorkspaces(['delete', 'version'], [])
  t.strictSame(
    readPackageJson(resolve(npm.localPrefix, 'packages/a')),
    {
      name: 'a',
      funding: 'http://example.com',
    },
    'should delete version field from workspace a'
  )

  t.strictSame(
    readPackageJson(resolve(npm.localPrefix, 'packages/b')),
    {
      name: 'b',
      funding: 'http://example.com',
    },
    'should delete version field from workspace b'
  )
})

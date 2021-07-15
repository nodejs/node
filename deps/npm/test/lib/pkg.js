const { resolve } = require('path')
const { readFileSync } = require('fs')
const t = require('tap')
const { fake: mockNpm } = require('../fixtures/mock-npm')

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

const Pkg = require('../../lib/pkg.js')
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

t.test('no args', t => {
  pkg.exec([], err => {
    t.match(
      err,
      { code: 'EUSAGE' },
      'should throw usage error'
    )
    t.end()
  })
})

t.test('no global mode', t => {
  config.global = true
  pkg.exec(['get', 'foo'], err => {
    t.match(
      err,
      { code: 'EPKGGLOBAL' },
      'should throw no global mode error'
    )
    t.end()
  })
})

t.test('get no args', t => {
  npm.localPrefix = t.testdir({
    'package.json': JSON.stringify({
      name: 'foo',
      version: '1.1.1',
    }),
  })

  pkg.exec(['get'], err => {
    if (err)
      throw err

    t.strictSame(
      JSON.parse(OUTPUT),
      {
        name: 'foo',
        version: '1.1.1',
      },
      'should print package.json content'
    )
    t.end()
  })
})

t.test('get single arg', t => {
  npm.localPrefix = t.testdir({
    'package.json': JSON.stringify({
      name: 'foo',
      version: '1.1.1',
    }),
  })

  pkg.exec(['get', 'version'], err => {
    if (err)
      throw err

    t.strictSame(
      JSON.parse(OUTPUT),
      '1.1.1',
      'should print retrieved package.json field'
    )
    t.end()
  })
})

t.test('get nested arg', t => {
  npm.localPrefix = t.testdir({
    'package.json': JSON.stringify({
      name: 'foo',
      version: '1.1.1',
      scripts: {
        test: 'node test.js',
      },
    }),
  })

  pkg.exec(['get', 'scripts.test'], err => {
    if (err)
      throw err

    t.strictSame(
      JSON.parse(OUTPUT),
      'node test.js',
      'should print retrieved nested field'
    )
    t.end()
  })
})

t.test('get array field', t => {
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

  pkg.exec(['get', 'files'], err => {
    if (err)
      throw err

    t.strictSame(
      JSON.parse(OUTPUT),
      files,
      'should print retrieved array field'
    )
    t.end()
  })
})

t.test('get array item', t => {
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

  pkg.exec(['get', 'files[0]'], err => {
    if (err)
      throw err

    t.strictSame(
      JSON.parse(OUTPUT),
      'index.js',
      'should print retrieved array field'
    )
    t.end()
  })
})

t.test('get array nested items notation', t => {
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

  pkg.exec(['get', 'contributors.name'], err => {
    if (err)
      throw err

    t.strictSame(
      JSON.parse(OUTPUT),
      {
        'contributors[0].name': 'Ruy',
        'contributors[1].name': 'Gar',
      },
      'should print json result containing matching results'
    )
    t.end()
  })
})

t.test('set no args', t => {
  npm.localPrefix = t.testdir({
    'package.json': JSON.stringify({ name: 'foo' }),
  })
  pkg.exec(['set'], err => {
    t.match(
      err,
      { code: 'EPKGSET' },
      'should throw an error if no args'
    )

    t.end()
  })
})

t.test('set missing value', t => {
  npm.localPrefix = t.testdir({
    'package.json': JSON.stringify({ name: 'foo' }),
  })
  pkg.exec(['set', 'key='], err => {
    t.match(
      err,
      { code: 'EPKGSET' },
      'should throw an error if missing value'
    )

    t.end()
  })
})

t.test('set missing key', t => {
  npm.localPrefix = t.testdir({
    'package.json': JSON.stringify({ name: 'foo' }),
  })
  pkg.exec(['set', '=value'], err => {
    t.match(
      err,
      { code: 'EPKGSET' },
      'should throw an error if missing key'
    )

    t.end()
  })
})

t.test('set single field', t => {
  const json = {
    name: 'foo',
    version: '1.1.1',
  }
  npm.localPrefix = t.testdir({
    'package.json': JSON.stringify(json),
  })

  pkg.exec(['set', 'description=Awesome stuff'], err => {
    if (err)
      throw err

    t.strictSame(
      readPackageJson(),
      {
        ...json,
        description: 'Awesome stuff',
      },
      'should add single field to package.json'
    )
    t.end()
  })
})

t.test('push to array syntax', t => {
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

  pkg.exec(['set', 'keywords[]=bar', 'keywords[]=baz'], err => {
    if (err)
      throw err

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
    t.end()
  })
})

t.test('set multiple fields', t => {
  const json = {
    name: 'foo',
    version: '1.1.1',
  }
  npm.localPrefix = t.testdir({
    'package.json': JSON.stringify(json),
  })

  pkg.exec(['set', 'bin.foo=foo.js', 'scripts.test=node test.js'], err => {
    if (err)
      throw err

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
    t.end()
  })
})

t.test('set = separate value', t => {
  const json = {
    name: 'foo',
    version: '1.1.1',
  }
  npm.localPrefix = t.testdir({
    'package.json': JSON.stringify(json),
  })

  pkg.exec(['set', 'tap[test-env][0]=LC_ALL=sk'], err => {
    if (err)
      throw err

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
    t.end()
  })
})

t.test('set --json', async t => {
  config.json = true
  npm.localPrefix = t.testdir({
    'package.json': JSON.stringify({
      name: 'foo',
      version: '1.1.1',
    }),
  })

  await new Promise((res, rej) => {
    pkg.exec(['set', 'private=true'], err => {
      if (err)
        rej(err)

      t.strictSame(
        readPackageJson(),
        {
          name: 'foo',
          version: '1.1.1',
          private: true,
        },
        'should add boolean field to package.json'
      )
      res()
    })
  })

  await new Promise((res, rej) => {
    pkg.exec(['set', 'tap.timeout=60'], err => {
      if (err)
        rej(err)

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
      res()
    })
  })

  await new Promise((res, rej) => {
    pkg.exec(['set', 'foo={ "bar": { "baz": "BAZ" } }'], err => {
      if (err)
        rej(err)

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
      res()
    })
  })

  await new Promise((res, rej) => {
    pkg.exec(['set', 'workspaces=["packages/*"]'], err => {
      if (err)
        rej(err)

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
      res()
    })
  })

  await new Promise((res, rej) => {
    pkg.exec(['set', 'description="awesome"'], err => {
      if (err)
        rej(err)

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
      res()
    })
  })
})

t.test('delete no args', t => {
  npm.localPrefix = t.testdir({
    'package.json': JSON.stringify({ name: 'foo' }),
  })
  pkg.exec(['delete'], err => {
    t.match(
      err,
      { code: 'EPKGDELETE' },
      'should throw an error if deleting no args'
    )

    t.end()
  })
})

t.test('delete invalid key', t => {
  npm.localPrefix = t.testdir({
    'package.json': JSON.stringify({ name: 'foo' }),
  })
  pkg.exec(['delete', ''], err => {
    t.match(
      err,
      { code: 'EPKGDELETE' },
      'should throw an error if deleting invalid args'
    )

    t.end()
  })
})

t.test('delete single field', t => {
  npm.localPrefix = t.testdir({
    'package.json': JSON.stringify({
      name: 'foo',
      version: '1.0.0',
    }),
  })
  pkg.exec(['delete', 'version'], err => {
    if (err)
      throw err

    t.strictSame(
      readPackageJson(),
      {
        name: 'foo',
      },
      'should delete single field from package.json'
    )

    t.end()
  })
})

t.test('delete multiple field', t => {
  npm.localPrefix = t.testdir({
    'package.json': JSON.stringify({
      name: 'foo',
      version: '1.0.0',
      description: 'awesome',
    }),
  })
  pkg.exec(['delete', 'version', 'description'], err => {
    if (err)
      throw err

    t.strictSame(
      readPackageJson(),
      {
        name: 'foo',
      },
      'should delete multiple fields from package.json'
    )

    t.end()
  })
})

t.test('delete nested field', t => {
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
  pkg.exec(['delete', 'info.foo.bar[0].baz'], err => {
    if (err)
      throw err

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

    t.end()
  })
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

  await new Promise((res, rej) => {
    pkg.execWorkspaces(['get', 'name', 'version'], [], err => {
      if (err)
        rej(err)

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
      res()
    })
  })

  await new Promise((res, rej) => {
    pkg.execWorkspaces(['set', 'funding=http://example.com'], [], err => {
      if (err)
        rej(err)

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
      res()
    })
  })

  await new Promise((res, rej) => {
    pkg.execWorkspaces(['delete', 'version'], [], err => {
      if (err)
        rej(err)

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
      res()
    })
  })
})

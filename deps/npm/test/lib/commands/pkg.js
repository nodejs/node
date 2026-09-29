const { resolve } = require('node:path')
const { readFileSync } = require('node:fs')
const t = require('tap')
const { load: loadMockNpm } = require('../../fixtures/mock-npm')
const { cleanCwd } = require('../../fixtures/clean-snapshot')

t.cleanSnapshot = (str) => cleanCwd(str)

const readPackageJson = (prefix, dir = '') =>
  JSON.parse(readFileSync(resolve(prefix, dir, 'package.json'), 'utf8'))

t.test('no args', async t => {
  const { npm } = await loadMockNpm(t)

  await t.rejects(
    npm.exec('pkg'),
    { code: 'EUSAGE' },
    'should throw usage error'
  )
})

t.test('no global mode', async t => {
  const { npm } = await loadMockNpm(t, {
    config: {
      global: true,
    },
  })

  await t.rejects(
    npm.exec('pkg'),
    { code: 'EPKGGLOBAL' },
    'should throw no global mode error'
  )
})

t.test('get', t => {
  t.test('no args', async t => {
    const { npm, joinedOutput } = await loadMockNpm(t, {
      prefixDir: {
        'package.json': JSON.stringify({
          name: 'foo',
          version: '1.1.1',
        }),
      },
    })
    await npm.exec('pkg', ['get'])

    t.matchSnapshot(
      joinedOutput(),
      'should print package.json content'
    )
  })

  t.test('single arg', async t => {
    const { npm, joinedOutput } = await loadMockNpm(t, {
      prefixDir: {
        'package.json': JSON.stringify({
          name: 'foo',
          version: '1.1.1',
        }),
      },
    })

    await npm.exec('pkg', ['get', 'version'])

    t.matchSnapshot(
      joinedOutput(),
      'should print retrieved package.json field'
    )
  })

  t.test('non string', async t => {
    const { npm, joinedOutput } = await loadMockNpm(t, {
      prefixDir: {
        'package.json': JSON.stringify({
          name: 'foo',
          version: '1.1.1',
          dependencies: {
            '@npmcli/test': '*',
          },
        }),
      },
    })

    await npm.exec('pkg', ['get', 'dependencies'])

    t.matchSnapshot(
      joinedOutput(),
      'should print retrieved package.json field'
    )
  })
  t.test('multiple arg', async t => {
    const { npm, joinedOutput } = await loadMockNpm(t, {
      prefixDir: {
        'package.json': JSON.stringify({
          name: 'foo',
          version: '1.1.1',
        }),
      },
    })

    await npm.exec('pkg', ['get', 'name', 'version'])

    t.matchSnapshot(
      joinedOutput(),
      'should print retrieved package.json fields'
    )
  })

  t.test('multiple arg with only one arg existing', async t => {
    const { npm, joinedOutput } = await loadMockNpm(t, {
      prefixDir: {
        'package.json': JSON.stringify({
          name: 'foo',
        }),
      },
    })

    await npm.exec('pkg', ['get', 'name', 'version', 'dependencies'])

    t.matchSnapshot(
      joinedOutput(),
      'should print retrieved package.json field'
    )
  })

  t.test('multiple arg with empty value', async t => {
    const { npm, joinedOutput } = await loadMockNpm(t, {
      prefixDir: {
        'package.json': JSON.stringify({
          name: 'foo',
          author: '',
        }),
      },
    })

    await npm.exec('pkg', ['get', 'name', 'author'])

    t.matchSnapshot(
      joinedOutput(),
      'should print retrieved package.json field regardless of empty value'
    )
  })

  t.test('nested arg', async t => {
    const { npm, joinedOutput } = await loadMockNpm(t, {
      prefixDir: {
        'package.json': JSON.stringify({
          name: 'foo',
          version: '1.1.1',
          scripts: {
            test: 'node test.js',
          },
        }),
      },
    })

    await npm.exec('pkg', ['get', 'scripts.test'])

    t.matchSnapshot(
      joinedOutput(),
      'node test.js',
      'should print retrieved nested field'
    )
  })

  t.test('array field', async t => {
    const { npm, joinedOutput } = await loadMockNpm(t, {
      prefixDir: {
        'package.json': JSON.stringify({
          name: 'foo',
          version: '1.1.1',
          files: [
            'index.js',
            'cli.js',
          ],
        }),
      },
    })

    await npm.exec('pkg', ['get', 'files'])

    t.matchSnapshot(
      joinedOutput(),
      'should print retrieved array field'
    )
  })

  t.test('array item', async t => {
    const { npm, joinedOutput } = await loadMockNpm(t, {
      prefixDir: {
        'package.json': JSON.stringify({
          name: 'foo',
          version: '1.1.1',
          files: [
            'index.js',
            'cli.js',
          ],
        }),
      },
    })

    await npm.exec('pkg', ['get', 'files[0]'])

    t.matchSnapshot(
      joinedOutput(),
      'should print retrieved array field'
    )
  })

  t.test('json no args', async t => {
    const { npm, joinedOutput } = await loadMockNpm(t, {
      prefixDir: {
        'package.json': JSON.stringify({
          name: 'foo',
          version: '1.1.1',
        }),
      },
      config: {
        json: true,
      },
    })
    await npm.exec('pkg', ['get'])

    t.matchSnapshot(
      joinedOutput(),
      'should print package.json content'
    )
  })

  t.test('json with args', async t => {
    const { npm, joinedOutput } = await loadMockNpm(t, {
      prefixDir: {
        'package.json': JSON.stringify({
          name: 'foo',
          version: '1.1.1',
        }),
      },
      config: {
        json: true,
      },
    })
    await npm.exec('pkg', ['get', 'name'])

    t.matchSnapshot(
      joinedOutput(),
      'should print package.json content'
    )
  })

  t.test('get array nested items notation', async t => {
    const { npm, joinedOutput } = await loadMockNpm(t, {
      prefixDir: {
        'package.json': JSON.stringify({
          name: 'foo',
          version: '1.1.1',
          contributors: [
            {
              name: 'Ruy',
              url: 'http://example.com/ruy',
            },
            {
              name: 'Gar',
              url: 'http://example.com/gar',
            },
          ],
        }),
      },
    })

    await npm.exec('pkg', ['get', 'contributors.name'])
    t.matchSnapshot(
      joinedOutput(),
      'should print json result containing matching results'
    )
  })
  t.end()
})

t.test('set', t => {
  t.test('set no args', async t => {
    const { npm } = await loadMockNpm(t, {
      prefixDir: {
        'package.json': JSON.stringify({ name: 'foo' }),
      },
    })
    await t.rejects(
      npm.exec('pkg', ['set']),
      { code: 'EUSAGE' },
      'should throw an error if no args'
    )
  })

  t.test('set missing value', async t => {
    const { npm } = await loadMockNpm(t, {
      prefixDir: {
        'package.json': JSON.stringify({ name: 'foo' }),
      },
    })
    await t.rejects(
      npm.exec('pkg', ['set', 'key=']),
      { code: 'EUSAGE' },
      'should throw an error if missing value'
    )
  })

  t.test('set missing key', async t => {
    const { npm } = await loadMockNpm(t, {
      prefixDir: {
        'package.json': JSON.stringify({ name: 'foo' }),
      },
    })
    await t.rejects(
      npm.exec('pkg', ['set', '=value']),
      { code: 'EUSAGE' },
      'should throw an error if missing key'
    )
  })

  t.test('set single field', async t => {
    const json = {
      name: 'foo',
      version: '1.1.1',
    }
    const { npm } = await loadMockNpm(t, {
      prefixDir: {
        'package.json': JSON.stringify(json),
      },
    })

    await npm.exec('pkg', ['set', 'description=Awesome stuff'])
    t.matchSnapshot(
      readPackageJson(npm.prefix),
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
    const { npm } = await loadMockNpm(t, {
      prefixDir: {
        'package.json': JSON.stringify(json),
      },
    })

    await npm.exec('pkg', ['set', 'keywords[]=bar', 'keywords[]=baz'])
    t.matchSnapshot(
      readPackageJson(npm.prefix),
      'should append to arrays using empty bracket syntax'
    )
  })

  t.test('set multiple fields', async t => {
    const json = {
      name: 'foo',
      version: '1.1.1',
    }
    const { npm } = await loadMockNpm(t, {
      prefixDir: {
        'package.json': JSON.stringify(json),
      },
    })

    await npm.exec('pkg', ['set', 'bin.foo=foo.js', 'scripts.test=node test.js'])
    t.matchSnapshot(
      readPackageJson(npm.prefix),
      'should add single field to package.json'
    )
  })

  t.test('set = separate value', async t => {
    const json = {
      name: 'foo',
      version: '1.1.1',
    }
    const { npm } = await loadMockNpm(t, {
      prefixDir: {
        'package.json': JSON.stringify(json),
      },
    })

    await npm.exec('pkg', ['set', 'tap[test-env][0]=LC_ALL=sk'])
    t.matchSnapshot(
      readPackageJson(npm.prefix),
      'should add single field to package.json'
    )
  })

  t.test('set --json', async t => {
    const { npm } = await loadMockNpm(t, {
      prefixDir: {
        'package.json': JSON.stringify({
          name: 'foo',
          version: '1.1.1',
        }),
      },
      config: { json: true },
    })

    await npm.exec('pkg', ['set', 'private=true'])
    await npm.exec('pkg', ['set', 'tap.timeout=60'])
    await npm.exec('pkg', ['set', 'foo={ "bar": { "baz": "BAZ" } }'])
    await npm.exec('pkg', ['set', 'workspaces=["packages/*"]'])
    await npm.exec('pkg', ['set', 'description="awesome"'])
    t.matchSnapshot(
      readPackageJson(npm.prefix),
      'should add fields to package.json'
    )
  })
  t.end()
})

t.test('delete', t => {
  t.test('delete no args', async t => {
    const { npm } = await loadMockNpm(t, {
      prefixDir: {
        'package.json': JSON.stringify({ name: 'foo' }),
      },
    })
    await t.rejects(
      npm.exec('pkg', ['delete']),
      { code: 'EUSAGE' },
      'should throw an error if deleting no args'
    )
  })

  t.test('delete invalid key', async t => {
    const { npm } = await loadMockNpm(t, {
      prefixDir: {
        'package.json': JSON.stringify({ name: 'foo' }),
      },
    })
    await t.rejects(
      npm.exec('pkg', ['delete', '']),
      { code: 'EUSAGE' },
      'should throw an error if deleting invalid args'
    )
  })

  t.test('delete single field', async t => {
    const { npm } = await loadMockNpm(t, {
      prefixDir: {
        'package.json': JSON.stringify({
          name: 'foo',
          version: '1.0.0',
        }),
      },
    })
    await npm.exec('pkg', ['delete', 'version'])
    t.matchSnapshot(
      readPackageJson(npm.prefix),
      'should delete single field from package.json'
    )
  })

  t.test('delete multiple field', async t => {
    const { npm } = await loadMockNpm(t, {
      prefixDir: {
        'package.json': JSON.stringify({
          name: 'foo',
          version: '1.0.0',
          description: 'awesome',
        }),
      },
    })
    await npm.exec('pkg', ['delete', 'version', 'description'])
    t.matchSnapshot(
      readPackageJson(npm.prefix),
      'should delete multiple fields from package.json'
    )
  })

  t.test('delete nested field', async t => {
    const { npm } = await loadMockNpm(t, {
      prefixDir: {
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
      },
    })
    await npm.exec('pkg', ['delete', 'info.foo.bar[0].baz'])
    t.matchSnapshot(
      readPackageJson(npm.prefix),
      'should delete nested fields from package.json'
    )
  })
  t.end()
})

t.test('workspaces', async t => {
  const workspaceSetup = {
    prefixDir: {
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
    },
    config: { workspaces: true },
  }

  t.test('get', async t => {
    const { npm, joinedOutput } = await loadMockNpm(t, workspaceSetup)
    await npm.exec('pkg', ['get', 'name', 'version'])
    t.matchSnapshot(
      joinedOutput(),
      'should return expected result for configured workspaces'
    )
  })

  t.test('get json ', async t => {
    const { npm, joinedOutput } = await loadMockNpm(t, {
      ...workspaceSetup,
      config: {
        json: true,
        workspaces: true,
      },
    })
    await npm.exec('pkg', ['get', 'name', 'version'])
    t.matchSnapshot(
      joinedOutput(),
      'should return expected json result for configured workspaces'
    )
  })

  t.test('set', async t => {
    const { npm } = await loadMockNpm(t, workspaceSetup)

    await npm.exec('pkg', ['set', 'funding=http://example.com'])

    t.matchSnapshot(
      readPackageJson(npm.prefix, 'packages/a'),
      'should add field to workspace a'
    )

    t.matchSnapshot(
      readPackageJson(npm.prefix, 'packages/b'),
      'should add field to workspace b'
    )

    await npm.exec('pkg', ['delete', 'version'])

    t.matchSnapshot(
      readPackageJson(npm.prefix, 'packages/a'),
      'should delete version field from workspace a'
    )

    t.matchSnapshot(
      readPackageJson(npm.prefix, 'packages/b'),
      'should delete version field from workspace b'
    )
  })
})

t.test('single workspace', async t => {
  const workspaceSetup = {
    prefixDir: {
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
    },
    config: { workspace: ['packages/a'] },
  }

  t.test('multiple args', async t => {
    const { npm, joinedOutput } = await loadMockNpm(t, workspaceSetup)
    await npm.exec('pkg', ['get', 'name', 'version'])

    t.matchSnapshot(
      joinedOutput(),
      'should only return info for one workspace'
    )
  })

  t.test('single arg', async t => {
    const { npm, joinedOutput } = await loadMockNpm(t, workspaceSetup)
    await npm.exec('pkg', ['get', 'version'])

    t.matchSnapshot(
      joinedOutput(),
      'should only return info for one workspace'
    )
  })
})

t.test('fix', async t => {
  const { npm } = await loadMockNpm(t, {
    prefixDir: {
      'package.json': JSON.stringify({
        name: 'foo ',
        version: 'v1.1.1',
      }),
    },
  })

  await npm.exec('pkg', ['fix'])
  t.matchSnapshot(
    readPackageJson(npm.prefix),
    'fixes package.json issues'
  )
})

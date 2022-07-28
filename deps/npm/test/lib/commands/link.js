const t = require('tap')
const { resolve } = require('path')
const fs = require('fs')

const Arborist = require('@npmcli/arborist')
const { fake: mockNpm } = require('../../fixtures/mock-npm')

const redactCwd = (path) => {
  const normalizePath = p => p
    .replace(/\\+/g, '/')
    .replace(/\r\n/g, '\n')
  return normalizePath(path)
    .replace(new RegExp(normalizePath(process.cwd()), 'g'), '{CWD}')
}

t.cleanSnapshot = (str) => redactCwd(str)

const config = {}
const npm = mockNpm({
  globalDir: null,
  prefix: null,
  config,
})

const printLinks = async (opts) => {
  let res = ''
  const arb = new Arborist(opts)
  const tree = await arb.loadActual()
  const linkedItems = [...tree.inventory.values()]
    .sort((a, b) => a.pkgid.localeCompare(b.pkgid, 'en'))
  for (const item of linkedItems) {
    if (item.isLink) {
      res += `${item.path} -> ${item.target.path}\n`
    }
  }
  return res
}

const mocks = {
  '../../../lib/utils/reify-output.js': async () => {},
}

const Link = t.mock('../../../lib/commands/link.js', mocks)
const link = new Link(npm)

t.test('link to globalDir when in current working dir of pkg and no args', async t => {
  const testdir = t.testdir({
    'global-prefix': {
      lib: {
        node_modules: {
          a: {
            'package.json': JSON.stringify({
              name: 'a',
              version: '1.0.0',
            }),
          },
        },
      },
    },
    'test-pkg-link': {
      'package.json': JSON.stringify({
        name: 'test-pkg-link',
        version: '1.0.0',
      }),
    },
  })
  npm.globalDir = resolve(testdir, 'global-prefix', 'lib', 'node_modules')
  npm.prefix = resolve(testdir, 'test-pkg-link')

  await link.exec([])
  const links = await printLinks({
    path: resolve(npm.globalDir, '..'),
    global: true,
  })

  t.matchSnapshot(links, 'should create a global link to current pkg')
})

t.test('link ws to globalDir when workspace specified and no args', async t => {
  const testdir = t.testdir({
    'global-prefix': {
      lib: {
        node_modules: {
          a: {
            'package.json': JSON.stringify({
              name: 'a',
              version: '1.0.0',
            }),
          },
        },
      },
    },
    'test-pkg-link': {
      'package.json': JSON.stringify({
        name: 'test-pkg-link',
        version: '1.0.0',
        workspaces: ['packages/*'],
      }),
      packages: {
        a: {
          'package.json': JSON.stringify({
            name: 'a',
            version: '1.0.0',
          }),
        },
      },
    },
  })
  npm.globalDir = resolve(testdir, 'global-prefix', 'lib', 'node_modules')
  npm.prefix = resolve(testdir, 'test-pkg-link')
  npm.localPrefix = resolve(testdir, 'test-pkg-link')

  // link.workspaces = ['a']
  // link.workspacePaths = [resolve(testdir, 'test-pkg-link/packages/a')]
  await link.execWorkspaces([], ['a'])
  const links = await printLinks({
    path: resolve(npm.globalDir, '..'),
    global: true,
  })

  t.matchSnapshot(links, 'should create a global link to current pkg')
})

t.test('link global linked pkg to local nm when using args', async t => {
  const testdir = t.testdir({
    'global-prefix': {
      lib: {
        node_modules: {
          '@myscope': {
            foo: {
              'package.json': JSON.stringify({
                name: '@myscope/foo',
                version: '1.0.0',
              }),
            },
            bar: {
              'package.json': JSON.stringify({
                name: '@myscope/bar',
                version: '1.0.0',
              }),
            },
            linked: t.fixture('symlink', '../../../../scoped-linked'),
          },
          a: {
            'package.json': JSON.stringify({
              name: 'a',
              version: '1.0.0',
            }),
          },
          b: {
            'package.json': JSON.stringify({
              name: 'b',
              version: '1.0.0',
            }),
          },
          'test-pkg-link': t.fixture('symlink', '../../../test-pkg-link'),
        },
      },
    },
    'test-pkg-link': {
      'package.json': JSON.stringify({
        name: 'test-pkg-link',
        version: '1.0.0',
      }),
    },
    'link-me-too': {
      'package.json': JSON.stringify({
        name: 'link-me-too',
        version: '1.0.0',
      }),
    },
    'scoped-linked': {
      'package.json': JSON.stringify({
        name: '@myscope/linked',
        version: '1.0.0',
      }),
    },
    'my-project': {
      'package.json': JSON.stringify({
        name: 'my-project',
        version: '1.0.0',
        dependencies: {
          foo: '^1.0.0',
        },
      }),
      node_modules: {
        foo: {
          'package.json': JSON.stringify({
            name: 'foo',
            version: '1.0.0',
          }),
        },
      },
    },
  })
  npm.globalDir = resolve(testdir, 'global-prefix', 'lib', 'node_modules')
  npm.prefix = resolve(testdir, 'my-project')

  const _cwd = process.cwd()
  process.chdir(npm.prefix)

  // installs examples for:
  // - test-pkg-link: pkg linked to globalDir from local fs
  // - @myscope/linked: scoped pkg linked to globalDir from local fs
  // - @myscope/bar: prev installed scoped package available in globalDir
  // - a: prev installed package available in globalDir
  // - file:./link-me-too: pkg that needs to be reified in globalDir first
  await link.exec([
    'test-pkg-link',
    '@myscope/linked',
    '@myscope/bar',
    'a',
    'file:../link-me-too',
  ])
  process.chdir(_cwd)
  const links = await printLinks({
    path: npm.prefix,
  })

  t.matchSnapshot(links, 'should create a local symlink to global pkg')
})

t.test('link global linked pkg to local workspace using args', async t => {
  const testdir = t.testdir({
    'global-prefix': {
      lib: {
        node_modules: {
          '@myscope': {
            foo: {
              'package.json': JSON.stringify({
                name: '@myscope/foo',
                version: '1.0.0',
              }),
            },
            bar: {
              'package.json': JSON.stringify({
                name: '@myscope/bar',
                version: '1.0.0',
              }),
            },
            linked: t.fixture('symlink', '../../../../scoped-linked'),
          },
          a: {
            'package.json': JSON.stringify({
              name: 'a',
              version: '1.0.0',
            }),
          },
          b: {
            'package.json': JSON.stringify({
              name: 'b',
              version: '1.0.0',
            }),
          },
          'test-pkg-link': t.fixture('symlink', '../../../test-pkg-link'),
        },
      },
    },
    'test-pkg-link': {
      'package.json': JSON.stringify({
        name: 'test-pkg-link',
        version: '1.0.0',
      }),
    },
    'link-me-too': {
      'package.json': JSON.stringify({
        name: 'link-me-too',
        version: '1.0.0',
      }),
    },
    'scoped-linked': {
      'package.json': JSON.stringify({
        name: '@myscope/linked',
        version: '1.0.0',
      }),
    },
    'my-project': {
      'package.json': JSON.stringify({
        name: 'my-project',
        version: '1.0.0',
        workspaces: ['packages/*'],
      }),
      packages: {
        x: {
          'package.json': JSON.stringify({
            name: 'x',
            version: '1.0.0',
            dependencies: {
              foo: '^1.0.0',
            },
          }),
        },
      },
      node_modules: {
        foo: {
          'package.json': JSON.stringify({
            name: 'foo',
            version: '1.0.0',
          }),
        },
      },
    },
  })
  npm.globalDir = resolve(testdir, 'global-prefix', 'lib', 'node_modules')
  npm.prefix = resolve(testdir, 'my-project')
  npm.localPrefix = resolve(testdir, 'my-project')

  const _cwd = process.cwd()
  process.chdir(npm.prefix)

  // installs examples for:
  // - test-pkg-link: pkg linked to globalDir from local fs
  // - @myscope/linked: scoped pkg linked to globalDir from local fs
  // - @myscope/bar: prev installed scoped package available in globalDir
  // - a: prev installed package available in globalDir
  // - file:./link-me-too: pkg that needs to be reified in globalDir first
  await link.execWorkspaces([
    'test-pkg-link',
    '@myscope/linked',
    '@myscope/bar',
    'a',
    'file:../link-me-too',
  ], ['x'])
  process.chdir(_cwd)

  const links = await printLinks({
    path: npm.prefix,
  })

  t.matchSnapshot(links, 'should create a local symlink to global pkg')
})

t.test('link pkg already in global space', async t => {
  const testdir = t.testdir({
    'global-prefix': {
      lib: {
        node_modules: {
          '@myscope': {
            linked: t.fixture('symlink', '../../../../scoped-linked'),
          },
        },
      },
    },
    'scoped-linked': {
      'package.json': JSON.stringify({
        name: '@myscope/linked',
        version: '1.0.0',
      }),
    },
    'my-project': {
      'package.json': JSON.stringify({
        name: 'my-project',
        version: '1.0.0',
      }),
    },
  })
  npm.globalDir = resolve(testdir, 'global-prefix', 'lib', 'node_modules')
  npm.prefix = resolve(testdir, 'my-project')

  npm.config.find = () => 'default'

  const _cwd = process.cwd()
  process.chdir(npm.prefix)

  // installs examples for:
  // - test-pkg-link: pkg linked to globalDir from local fs
  // - @myscope/linked: scoped pkg linked to globalDir from local fs
  // - @myscope/bar: prev installed scoped package available in globalDir
  // - a: prev installed package available in globalDir
  // - file:./link-me-too: pkg that needs to be reified in globalDir first
  await link.exec(['@myscope/linked'])
  process.chdir(_cwd)
  npm.config.find = () => null

  const links = await printLinks({
    path: npm.prefix,
  })

  t.equal(
    require(resolve(testdir, 'my-project', 'package.json')).dependencies,
    undefined,
    'should not save to package.json upon linking'
  )

  t.matchSnapshot(links, 'should create a local symlink to global pkg')
})

t.test('link pkg already in global space when prefix is a symlink', async t => {
  const testdir = t.testdir({
    'global-prefix': t.fixture('symlink', './real-global-prefix'),
    'real-global-prefix': {
      lib: {
        node_modules: {
          '@myscope': {
            linked: t.fixture('symlink', '../../../../scoped-linked'),
          },
        },
      },
    },
    'scoped-linked': {
      'package.json': JSON.stringify({
        name: '@myscope/linked',
        version: '1.0.0',
      }),
    },
    'my-project': {
      'package.json': JSON.stringify({
        name: 'my-project',
        version: '1.0.0',
      }),
    },
  })
  npm.globalDir = resolve(testdir, 'global-prefix', 'lib', 'node_modules')
  npm.prefix = resolve(testdir, 'my-project')

  npm.config.find = () => 'default'

  const _cwd = process.cwd()
  process.chdir(npm.prefix)

  await link.exec(['@myscope/linked'])
  process.chdir(_cwd)
  npm.config.find = () => null

  const links = await printLinks({
    path: npm.prefix,
  })

  t.equal(
    require(resolve(testdir, 'my-project', 'package.json')).dependencies,
    undefined,
    'should not save to package.json upon linking'
  )

  t.matchSnapshot(links, 'should create a local symlink to global pkg')
})

t.test('should not prune dependencies when linking packages', async t => {
  const testdir = t.testdir({
    'global-prefix': {
      lib: {
        node_modules: {
          linked: t.fixture('symlink', '../../../linked'),
        },
      },
    },
    linked: {
      'package.json': JSON.stringify({
        name: 'linked',
        version: '1.0.0',
      }),
    },
    'my-project': {
      node_modules: {
        foo: {
          'package.json': JSON.stringify({ name: 'foo', version: '1.0.0' }),
        },
      },
      'package.json': JSON.stringify({
        name: 'my-project',
        version: '1.0.0',
      }),
    },
  })
  npm.globalDir = resolve(testdir, 'global-prefix', 'lib', 'node_modules')
  npm.prefix = resolve(testdir, 'my-project')

  const _cwd = process.cwd()
  process.chdir(npm.prefix)

  await link.exec(['linked'])

  t.ok(
    fs.statSync(resolve(testdir, 'my-project/node_modules/foo')),
    'should not prune any extraneous dep when running npm link'
  )
  process.chdir(_cwd)
})

t.test('completion', async t => {
  const testdir = t.testdir({
    'global-prefix': {
      lib: {
        node_modules: {
          foo: {},
          bar: {},
          lorem: {},
          ipsum: {},
        },
      },
    },
  })
  npm.globalDir = resolve(testdir, 'global-prefix', 'lib', 'node_modules')

  const words = await link.completion({})
  t.same(
    words,
    ['bar', 'foo', 'ipsum', 'lorem'],
    'should list all package names available in globalDir'
  )
})

t.test('--global option', async t => {
  t.teardown(() => {
    npm.config = _config
  })
  const _config = npm.config
  npm.config = { get () {
    return true
  } }
  await t.rejects(
    link.exec([]),
    /link should never be --global/,
    'should throw an useful error'
  )
})

t.test('hash character in working directory path', async t => {
  const testdir = t.testdir({
    'global-prefix': {
      lib: {
        node_modules: {
          a: {
            'package.json': JSON.stringify({
              name: 'a',
              version: '1.0.0',
            }),
          },
        },
      },
    },
    'i_like_#_in_my_paths': {
      'test-pkg-link': {
        'package.json': JSON.stringify({
          name: 'test-pkg-link',
          version: '1.0.0',
        }),
      },
    },
  })
  npm.globalDir = resolve(testdir, 'global-prefix', 'lib', 'node_modules')
  npm.prefix = resolve(testdir, 'i_like_#_in_my_paths', 'test-pkg-link')

  link.workspacePaths = null
  await link.exec([])
  const links = await printLinks({
    path: resolve(npm.globalDir, '..'),
    global: true,
  })

  t.matchSnapshot(links, 'should create a global link to current pkg, even within path with hash')
})

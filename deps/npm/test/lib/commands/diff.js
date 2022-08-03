const t = require('tap')
const { resolve, join } = require('path')
const { fake: mockNpm } = require('../../fixtures/mock-npm')

const noop = () => null
let libnpmdiff = noop

const config = {
  global: false,
  tag: 'latest',
  diff: [],
}
const flatOptions = {
  global: false,
  diffUnified: null,
  diffIgnoreAllSpace: false,
  diffNoPrefix: false,
  diffSrcPrefix: '',
  diffDstPrefix: '',
  diffText: false,
  savePrefix: '^',
}
const fooPath = t.testdir({
  'package.json': JSON.stringify({ name: 'foo', version: '1.0.0' }),
})
const npm = mockNpm({
  prefix: fooPath,
  config,
  flatOptions,
  output: noop,
})

const mocks = {
  'proc-log': { info: noop, verbose: noop },
  libnpmdiff: (...args) => libnpmdiff(...args),
  'npm-registry-fetch': async () => ({}),
}

t.afterEach(() => {
  config.global = false
  config.tag = 'latest'
  config.diff = []
  flatOptions.global = false
  flatOptions.diffUnified = null
  flatOptions.diffIgnoreAllSpace = false
  flatOptions.diffNoPrefix = false
  flatOptions.diffSrcPrefix = ''
  flatOptions.diffDstPrefix = ''
  flatOptions.diffText = false
  flatOptions.savePrefix = '^'
  npm.globalDir = fooPath
  npm.prefix = fooPath
  libnpmdiff = noop
  diff.prefix = undefined
  diff.top = undefined
})

const Diff = t.mock('../../../lib/commands/diff.js', mocks)
const diff = new Diff(npm)

t.test('no args', t => {
  t.test('in a project dir', async t => {
    t.plan(3)

    libnpmdiff = async ([a, b], opts) => {
      t.equal(a, 'foo@latest', 'should have default spec comparison')
      t.equal(b, `file:${fooPath}`, 'should compare to cwd')
      t.match(opts, npm.flatOptions, 'should forward flat options')
    }

    npm.prefix = fooPath
    await diff.exec([])
  })

  t.test('no args, missing package.json name in cwd', async t => {
    const path = t.testdir({})
    npm.prefix = path
    await t.rejects(
      diff.exec([]),
      /Needs multiple arguments to compare or run from a project dir./,
      'should throw EDIFF error msg'
    )
  })

  t.test('no args, bad package.json in cwd', async t => {
    const path = t.testdir({
      'package.json': '{invalid"json',
    })
    npm.prefix = path

    await t.rejects(
      diff.exec([]),
      /Needs multiple arguments to compare or run from a project dir./,
      'should throw EDIFF error msg'
    )
  })

  t.end()
})

t.test('single arg', t => {
  t.test('spec using cwd package name', async t => {
    t.plan(3)

    libnpmdiff = async ([a, b], opts) => {
      t.equal(a, 'foo@1.0.0', 'should forward single spec')
      t.equal(b, `file:${fooPath}`, 'should compare to cwd')
      t.match(opts, npm.flatOptions, 'should forward flat options')
    }

    config.diff = ['foo@1.0.0']
    npm.prefix = fooPath
    await diff.exec([])
  })

  t.test('unknown spec, no package.json', async t => {
    const path = t.testdir({})

    config.diff = ['foo@1.0.0']
    npm.prefix = path
    await t.rejects(
      diff.exec([]),
      /Needs multiple arguments to compare or run from a project dir./,
      'should throw usage error'
    )
  })

  t.test('spec using semver range', async t => {
    t.plan(3)

    libnpmdiff = async ([a, b], opts) => {
      t.equal(a, 'foo@~1.0.0', 'should forward single spec')
      t.equal(b, `file:${fooPath}`, 'should compare to cwd')
      t.match(opts, npm.flatOptions, 'should forward flat options')
    }

    config.diff = ['foo@~1.0.0']
    await diff.exec([])
  })

  t.test('version', async t => {
    t.plan(3)

    libnpmdiff = async ([a, b], opts) => {
      t.equal(a, 'foo@2.1.4', 'should convert to expected first spec')
      t.equal(b, `file:${fooPath}`, 'should compare to cwd')
      t.match(opts, npm.flatOptions, 'should forward flat options')
    }

    config.diff = ['2.1.4']
    await diff.exec([])
  })

  t.test('version, no package.json', async t => {
    const path = t.testdir({})
    npm.prefix = path
    config.diff = ['2.1.4']
    await t.rejects(
      diff.exec([]),
      /Needs multiple arguments to compare or run from a project dir./,
      'should throw an error message explaining usage'
    )
  })

  t.test('version, filtering by files', async t => {
    t.plan(3)

    libnpmdiff = async ([a, b], opts) => {
      t.equal(a, 'foo@2.1.4', 'should use expected spec')
      t.equal(b, `file:${fooPath}`, 'should compare to cwd')
      t.match(
        opts,
        {
          ...npm.flatOptions,
          diffFiles: ['./foo.js', './bar.js'],
        },
        'should forward flatOptions and diffFiles'
      )
    }

    config.diff = ['2.1.4']
    await diff.exec(['./foo.js', './bar.js'])
  })

  t.test('spec is not a dep', async t => {
    t.plan(2)

    const path = t.testdir({
      node_modules: {},
      'package.json': JSON.stringify({
        name: 'my-project',
      }),
    })

    libnpmdiff = async ([a, b], opts) => {
      t.equal(a, 'bar@1.0.0', 'should have current spec')
      t.equal(b, `file:${path}`, 'should compare to cwd')
    }

    config.diff = ['bar@1.0.0']
    npm.prefix = path

    await diff.exec([])
  })

  t.test('unknown package name', async t => {
    t.plan(3)

    const path = t.testdir({
      'package.json': JSON.stringify({
        name: 'my-project',
        dependencies: {
          bar: '^1.0.0',
        },
      }),
    })

    libnpmdiff = async ([a, b], opts) => {
      t.equal(a, 'simple-output@latest', 'should forward single spec')
      t.equal(b, `file:${path}`, 'should compare to cwd')
      t.match(opts, npm.flatOptions, 'should forward flat options')
    }

    config.diff = ['simple-output']
    npm.prefix = path
    await diff.exec([])
  })

  t.test('unknown package name, no package.json', async t => {
    const path = t.testdir({})

    config.diff = ['bar']
    npm.prefix = path
    await t.rejects(
      diff.exec([]),
      /Needs multiple arguments to compare or run from a project dir./,
      'should throw usage error'
    )
  })

  t.test('transform single direct dep name into spec comparison', async t => {
    t.plan(4)

    const path = t.testdir({
      node_modules: {
        bar: {
          'package.json': JSON.stringify({
            name: 'bar',
            version: '1.0.0',
          }),
        },
      },
      'package.json': JSON.stringify({
        name: 'my-project',
        dependencies: {
          bar: '^1.0.0',
        },
      }),
    })

    config.diff = ['bar']
    npm.prefix = path

    const Diff = t.mock('../../../lib/commands/diff.js', {
      ...mocks,
      pacote: {
        packument: spec => {
          t.equal(spec.name, 'bar', 'should have expected spec name')
        },
      },
      'npm-pick-manifest': (packument, target) => {
        t.equal(target, '^1.0.0', 'should use expected target')
        return { version: '1.8.10' }
      },
      libnpmdiff: async ([a, b], opts) => {
        t.equal(
          a,
          `bar@file:${resolve(path, 'node_modules/bar')}`,
          'should target local node_modules pkg'
        )
        t.equal(b, 'bar@1.8.10', 'should have possible semver range spec')
      },
    })
    const diff = new Diff(npm)

    await diff.exec([])
  })

  t.test('global space, transform single direct dep name', async t => {
    t.plan(4)

    const path = t.testdir({
      globalDir: {
        lib: {
          node_modules: {
            lorem: {
              'package.json': JSON.stringify({
                name: 'lorem',
                version: '2.0.0',
              }),
            },
          },
        },
      },
      project: {
        node_modules: {
          bar: {
            'package.json': JSON.stringify({
              name: 'bar',
              version: '1.0.0',
            }),
          },
        },
        'package.json': JSON.stringify({
          name: 'my-project',
          dependencies: {
            bar: '^1.0.0',
          },
        }),
      },
    })

    config.global = true
    flatOptions.global = true
    config.diff = ['lorem']
    npm.prefix = resolve(path, 'project')
    npm.globalDir = resolve(path, 'globalDir/lib/node_modules')

    const Diff = t.mock('../../../lib/commands/diff.js', {
      ...mocks,
      pacote: {
        packument: spec => {
          t.equal(spec.name, 'lorem', 'should have expected spec name')
        },
      },
      'npm-pick-manifest': (packument, target) => {
        t.equal(target, '*', 'should always want latest in global space')
        return { version: '2.1.0' }
      },
      libnpmdiff: async ([a, b], opts) => {
        t.equal(
          a,
          `lorem@file:${resolve(path, 'globalDir/lib/node_modules/lorem')}`,
          'should target local node_modules pkg'
        )
        t.equal(b, 'lorem@2.1.0', 'should have possible semver range spec')
      },
    })
    const diff = new Diff(npm)

    await diff.exec([])
  })

  t.test('transform single spec into spec comparison', async t => {
    t.plan(2)

    const path = t.testdir({
      node_modules: {
        bar: {
          'package.json': JSON.stringify({
            name: 'bar',
            version: '1.0.0',
          }),
        },
      },
      'package.json': JSON.stringify({
        name: 'my-project',
        dependencies: {
          bar: '^1.0.0',
        },
      }),
    })

    libnpmdiff = async ([a, b], opts) => {
      t.equal(
        a,
        `bar@file:${resolve(path, 'node_modules/bar')}`,
        'should target local node_modules pkg'
      )
      t.equal(b, 'bar@2.0.0', 'should have expected comparison spec')
    }

    config.diff = ['bar@2.0.0']
    npm.prefix = path

    await diff.exec([])
  })

  t.test('transform single spec from transitive deps', async t => {
    t.plan(4)

    const path = t.testdir({
      node_modules: {
        bar: {
          'package.json': JSON.stringify({
            name: 'bar',
            version: '1.0.0',
            dependencies: {
              lorem: '^2.0.0',
            },
          }),
        },
        lorem: {
          'package.json': JSON.stringify({
            name: 'lorem',
            version: '2.0.0',
          }),
        },
      },
      'package.json': JSON.stringify({
        name: 'my-project',
        dependencies: {
          bar: '^1.0.0',
        },
      }),
    })

    const Diff = t.mock('../../../lib/commands/diff.js', {
      ...mocks,
      pacote: {
        packument: spec => {
          t.equal(spec.name, 'lorem', 'should have expected spec name')
        },
      },
      'npm-pick-manifest': (packument, target) => {
        t.equal(target, '^2.0.0', 'should target first semver-range spec found')
        return { version: '2.2.2' }
      },
      libnpmdiff: async ([a, b], opts) => {
        t.equal(
          a,
          `lorem@file:${resolve(path, 'node_modules/lorem')}`,
          'should target local node_modules pkg'
        )
        t.equal(b, 'lorem@2.2.2', 'should have expected target spec')
      },
    })
    const diff = new Diff(npm)

    config.diff = ['lorem']
    npm.prefix = path

    await diff.exec([])
  })

  t.test('missing actual tree', async t => {
    t.plan(2)

    const path = t.testdir({
      'package.json': JSON.stringify({
        name: 'my-project',
      }),
    })

    const Diff = t.mock('../../../lib/commands/diff.js', {
      ...mocks,
      '@npmcli/arborist': class {
        constructor () {
          throw new Error('ERR')
        }
      },
      libnpmdiff: async ([a, b], opts) => {
        t.equal(a, 'lorem@latest', 'should target latest version of pkg name')
        t.equal(b, `file:${path}`, 'should target current cwd')
      },
    })
    const diff = new Diff(npm)

    config.diff = ['lorem']
    npm.prefix = path

    await diff.exec([])
  })

  t.test('unknown package name', async t => {
    t.plan(2)

    const path = t.testdir({
      'package.json': JSON.stringify({ version: '1.0.0' }),
    })
    libnpmdiff = async ([a, b], opts) => {
      t.equal(a, 'bar@latest', 'should target latest tag of name')
      t.equal(b, `file:${path}`, 'should compare to cwd')
    }

    config.diff = ['bar']
    npm.prefix = path

    await diff.exec([])
  })

  t.test('use project name in project dir', async t => {
    t.plan(2)

    libnpmdiff = async ([a, b], opts) => {
      t.equal(a, 'foo@latest', 'should target latest tag of name')
      t.equal(b, `file:${fooPath}`, 'should compare to cwd')
    }

    config.diff = ['foo']
    await diff.exec([])
  })

  t.test('dir spec type', async t => {
    t.plan(2)

    const otherPath = resolve('/path/to/other-dir')
    libnpmdiff = async ([a, b], opts) => {
      t.equal(a, `file:${otherPath}`, 'should target dir')
      t.equal(b, `file:${fooPath}`, 'should compare to cwd')
    }

    config.diff = [otherPath]
    await diff.exec([])
  })

  t.test('unsupported spec type', async t => {
    config.diff = ['git+https://github.com/user/foo']
    await t.rejects(
      diff.exec([]),
      /Spec type git not supported./,
      'should throw spec type not supported error.'
    )
  })

  t.end()
})

t.test('first arg is a qualified spec', t => {
  t.test('second arg is ALSO a qualified spec', async t => {
    t.plan(3)

    libnpmdiff = async ([a, b], opts) => {
      t.equal(a, 'bar@1.0.0', 'should set expected first spec')
      t.equal(b, 'bar@^2.0.0', 'should set expected second spec')
      t.match(opts, npm.flatOptions, 'should forward flat options')
    }

    config.diff = ['bar@1.0.0', 'bar@^2.0.0']
    await diff.exec([])
  })

  t.test('second arg is a known dependency name', async t => {
    t.plan(2)

    const path = t.testdir({
      node_modules: {
        bar: {
          'package.json': JSON.stringify({
            name: 'bar',
            version: '1.0.0',
          }),
        },
      },
      'package.json': JSON.stringify({
        name: 'my-project',
        dependencies: {
          bar: '^1.0.0',
        },
      }),
    })

    libnpmdiff = async ([a, b], opts) => {
      t.equal(a, 'bar@2.0.0', 'should set expected first spec')
      t.equal(
        b,
        `bar@file:${resolve(path, 'node_modules/bar')}`,
        'should target local node_modules pkg'
      )
    }

    npm.prefix = path
    config.diff = ['bar@2.0.0', 'bar']
    await diff.exec([])
  })

  t.test('second arg is a valid semver version', async t => {
    t.plan(2)

    config.diff = ['bar@1.0.0', '2.0.0']

    libnpmdiff = async ([a, b], opts) => {
      t.equal(a, 'bar@1.0.0', 'should set expected first spec')
      t.equal(b, 'bar@2.0.0', 'should use name from first arg')
    }

    await diff.exec([])
  })

  t.test('second arg is an unknown dependency name', async t => {
    t.plan(2)

    libnpmdiff = async ([a, b], opts) => {
      t.equal(a, 'bar@1.0.0', 'should set expected first spec')
      t.equal(b, 'bar-fork@latest', 'should target latest tag if not a dep')
    }

    config.diff = ['bar@1.0.0', 'bar-fork']
    await diff.exec([])
  })

  t.end()
})

t.test('first arg is a known dependency name', async t => {
  t.test('second arg is a qualified spec', t => {
    t.plan(2)

    const path = t.testdir({
      node_modules: {
        bar: {
          'package.json': JSON.stringify({
            name: 'bar',
            version: '1.0.0',
          }),
        },
      },
      'package.json': JSON.stringify({
        name: 'my-project',
        dependencies: {
          bar: '^1.0.0',
        },
      }),
    })

    libnpmdiff = async ([a, b], opts) => {
      t.equal(
        a,
        `bar@file:${resolve(path, 'node_modules/bar')}`,
        'should target local node_modules pkg'
      )
      t.equal(b, 'bar@2.0.0', 'should set expected second spec')
    }

    npm.prefix = path
    config.diff = ['bar', 'bar@2.0.0']
    diff.exec([], err => {
      if (err) {
        throw err
      }
    })
  })

  t.test('second arg is ALSO a known dependency', t => {
    t.plan(2)

    const path = t.testdir({
      node_modules: {
        bar: {
          'package.json': JSON.stringify({
            name: 'bar',
            version: '1.0.0',
          }),
        },
        'bar-fork': {
          'package.json': JSON.stringify({
            name: 'bar-fork',
            version: '1.0.0',
          }),
        },
      },
      'package.json': JSON.stringify({
        name: 'my-project',
        dependencies: {
          bar: '^1.0.0',
        },
      }),
    })

    libnpmdiff = async ([a, b], opts) => {
      t.equal(
        a,
        `bar@file:${resolve(path, 'node_modules/bar')}`,
        'should target local node_modules pkg'
      )
      t.equal(
        b,
        `bar-fork@file:${resolve(path, 'node_modules/bar-fork')}`,
        'should target fork local node_modules pkg'
      )
    }

    npm.prefix = path
    config.diff = ['bar', 'bar-fork']
    diff.exec([], err => {
      if (err) {
        throw err
      }
    })
  })

  t.test('second arg is a valid semver version', t => {
    t.plan(2)

    const path = t.testdir({
      node_modules: {
        bar: {
          'package.json': JSON.stringify({
            name: 'bar',
            version: '1.0.0',
          }),
        },
      },
      'package.json': JSON.stringify({
        name: 'my-project',
        dependencies: {
          bar: '^1.0.0',
        },
      }),
    })

    libnpmdiff = async ([a, b], opts) => {
      t.equal(
        a,
        `bar@file:${resolve(path, 'node_modules/bar')}`,
        'should target local node_modules pkg'
      )
      t.equal(b, 'bar@2.0.0', 'should use package name from first arg')
    }

    npm.prefix = path
    config.diff = ['bar', '2.0.0']
    diff.exec([], err => {
      if (err) {
        throw err
      }
    })
  })

  t.test('second arg is an unknown dependency name', async t => {
    t.plan(2)

    const path = t.testdir({
      node_modules: {
        bar: {
          'package.json': JSON.stringify({
            name: 'bar',
            version: '1.0.0',
          }),
        },
      },
      'package.json': JSON.stringify({
        name: 'my-project',
        dependencies: {
          bar: '^1.0.0',
        },
      }),
    })

    libnpmdiff = async ([a, b], opts) => {
      t.equal(
        a,
        `bar@file:${resolve(path, 'node_modules/bar')}`,
        'should target local node_modules pkg'
      )
      t.equal(b, 'bar-fork@latest', 'should set expected second spec')
    }

    npm.prefix = path
    config.diff = ['bar', 'bar-fork']
    await diff.exec([])
  })

  t.end()
})

t.test('first arg is a valid semver range', t => {
  t.test('second arg is a qualified spec', async t => {
    t.plan(2)

    config.diff = ['1.0.0', 'bar@2.0.0']

    libnpmdiff = async ([a, b], opts) => {
      t.equal(a, 'bar@1.0.0', 'should use name from second arg')
      t.equal(b, 'bar@2.0.0', 'should use expected spec')
    }

    await diff.exec([])
  })

  t.test('second arg is a known dependency', async t => {
    t.plan(2)

    const path = t.testdir({
      node_modules: {
        bar: {
          'package.json': JSON.stringify({
            name: 'bar',
            version: '2.0.0',
          }),
        },
      },
      'package.json': JSON.stringify({
        name: 'my-project',
        dependencies: {
          bar: '^1.0.0',
        },
      }),
    })

    libnpmdiff = async ([a, b], opts) => {
      t.equal(a, 'bar@1.0.0', 'should use name from second arg')
      t.equal(
        b,
        `bar@file:${resolve(path, 'node_modules/bar')}`,
        'should set expected second spec from nm'
      )
    }

    npm.prefix = path
    config.diff = ['1.0.0', 'bar']
    await diff.exec([])
  })

  t.test('second arg is ALSO a semver version', async t => {
    t.plan(2)

    libnpmdiff = async ([a, b], opts) => {
      t.equal(a, 'foo@1.0.0', 'should use name from project dir')
      t.equal(b, 'foo@2.0.0', 'should use name from project dir')
    }

    config.diff = ['1.0.0', '2.0.0']
    await diff.exec([])
  })

  t.test('second arg is ALSO a semver version BUT cwd not a project dir', async t => {
    const path = t.testdir({})
    config.diff = ['1.0.0', '2.0.0']
    npm.prefix = path
    await t.rejects(
      diff.exec([]),
      /Needs to be run from a project dir in order to diff two versions./,
      'should throw two versions need project dir error usage msg'
    )
  })

  t.test('second arg is an unknown dependency name', async t => {
    t.plan(2)

    libnpmdiff = async ([a, b], opts) => {
      t.equal(a, 'bar@1.0.0', 'should use name from second arg')
      t.equal(b, 'bar@latest', 'should compare against latest tag')
    }

    config.diff = ['1.0.0', 'bar']
    await diff.exec([])
  })

  t.test('second arg is a qualified spec, missing actual tree', async t => {
    t.plan(2)

    const path = t.testdir({
      'package.json': JSON.stringify({
        name: 'my-project',
      }),
    })

    const Diff = t.mock('../../../lib/commands/diff.js', {
      ...mocks,
      '@npmcli/arborist': class {
        constructor () {
          throw new Error('ERR')
        }
      },
      libnpmdiff: async ([a, b], opts) => {
        t.equal(a, 'lorem@1.0.0', 'should target latest version of pkg name')
        t.equal(b, 'lorem@2.0.0', 'should target expected spec')
      },
    })
    const diff = new Diff(npm)

    config.diff = ['1.0.0', 'lorem@2.0.0']
    npm.prefix = path

    await diff.exec([])
  })

  t.end()
})

t.test('first arg is an unknown dependency name', t => {
  t.test('second arg is a qualified spec', t => {
    t.plan(4)

    libnpmdiff = async ([a, b], opts) => {
      t.equal(a, 'bar@latest', 'should set expected first spec')
      t.equal(b, 'bar@2.0.0', 'should set expected second spec')
      t.match(opts, npm.flatOptions, 'should forward flat options')
      t.match(opts, { where: fooPath }, 'should forward pacote options')
    }

    config.diff = ['bar', 'bar@2.0.0']
    diff.exec([], err => {
      if (err) {
        throw err
      }
    })
  })

  t.test('second arg is a known dependency', t => {
    t.plan(2)

    const path = t.testdir({
      node_modules: {
        bar: {
          'package.json': JSON.stringify({
            name: 'bar',
            version: '2.0.0',
          }),
        },
      },
      'package.json': JSON.stringify({
        name: 'my-project',
        dependencies: {
          bar: '^1.0.0',
        },
      }),
    })

    libnpmdiff = async ([a, b], opts) => {
      t.equal(a, 'bar-fork@latest', 'should use latest tag')
      t.equal(
        b,
        `bar@file:${resolve(path, 'node_modules/bar')}`,
        'should target local node_modules pkg'
      )
    }

    npm.prefix = path
    config.diff = ['bar-fork', 'bar']
    diff.exec([], err => {
      if (err) {
        throw err
      }
    })
  })

  t.test('second arg is a valid semver version', t => {
    t.plan(2)

    libnpmdiff = async ([a, b], opts) => {
      t.equal(a, 'bar@latest', 'should use latest tag')
      t.equal(b, 'bar@^1.0.0', 'should use name from first arg')
    }

    config.diff = ['bar', '^1.0.0']
    diff.exec([], err => {
      if (err) {
        throw err
      }
    })
  })

  t.test('second arg is ALSO an unknown dependency name', t => {
    t.plan(2)

    libnpmdiff = async ([a, b], opts) => {
      t.equal(a, 'bar@latest', 'should use latest tag')
      t.equal(b, 'bar-fork@latest', 'should use latest tag')
    }

    config.diff = ['bar', 'bar-fork']
    diff.exec([], err => {
      if (err) {
        throw err
      }
    })
  })

  t.test('cwd not a project dir', t => {
    t.plan(2)

    const path = t.testdir({})
    libnpmdiff = async ([a, b], opts) => {
      t.equal(a, 'bar@latest', 'should use latest tag')
      t.equal(b, 'bar-fork@latest', 'should use latest tag')
    }

    config.diff = ['bar', 'bar-fork']
    npm.prefix = path

    diff.exec([], err => {
      if (err) {
        throw err
      }
    })
  })

  t.end()
})

t.test('various options', t => {
  t.test('using --name-only option', async t => {
    t.plan(1)

    flatOptions.diffNameOnly = true

    libnpmdiff = async ([a, b], opts) => {
      t.match(
        opts,
        {
          ...npm.flatOptions,
          diffNameOnly: true,
        },
        'should forward nameOnly=true option'
      )
    }

    await diff.exec([])
  })

  t.test('set files after both versions', async t => {
    t.plan(3)

    config.diff = ['2.1.4', '3.0.0']

    libnpmdiff = async ([a, b], opts) => {
      t.equal(a, 'foo@2.1.4', 'should use expected spec')
      t.equal(b, 'foo@3.0.0', 'should use expected spec')
      t.match(
        opts,
        {
          ...npm.flatOptions,
          diffFiles: ['./foo.js', './bar.js'],
        },
        'should forward diffFiles values'
      )
    }

    await diff.exec(['./foo.js', './bar.js'])
  })

  t.test('set files no diff args', async t => {
    t.plan(3)

    libnpmdiff = async ([a, b], opts) => {
      t.equal(a, 'foo@latest', 'should have default spec')
      t.equal(b, `file:${fooPath}`, 'should compare to cwd')
      t.match(
        opts,
        {
          ...npm.flatOptions,
          diffFiles: ['./foo.js', './bar.js'],
        },
        'should forward all remaining items as filenames'
      )
    }

    await diff.exec(['./foo.js', './bar.js'])
  })

  t.test('using diff option', async t => {
    t.plan(1)

    flatOptions.diffContext = 5
    flatOptions.diffIgnoreWhitespace = true
    flatOptions.diffNoPrefix = false
    flatOptions.diffSrcPrefix = 'foo/'
    flatOptions.diffDstPrefix = 'bar/'
    flatOptions.diffText = true

    libnpmdiff = async ([a, b], opts) => {
      t.match(
        opts,
        {
          ...npm.flatOptions,
          diffContext: 5,
          diffIgnoreWhitespace: true,
          diffNoPrefix: false,
          diffSrcPrefix: 'foo/',
          diffDstPrefix: 'bar/',
          diffText: true,
        },
        'should forward diff options'
      )
    }

    await diff.exec([])
  })

  t.end()
})

t.test('too many args', async t => {
  config.diff = ['a', 'b', 'c']
  await t.rejects(
    diff.exec([]),
    /Can't use more than two --diff arguments./,
    'should throw usage error'
  )
})

t.test('workspaces', t => {
  const path = t.testdir({
    'package.json': JSON.stringify({
      name: 'workspaces-test',
      version: '1.2.3-test',
      workspaces: ['workspace-a', 'workspace-b', 'workspace-c'],
    }),
    'workspace-a': {
      'package.json': JSON.stringify({
        name: 'workspace-a',
        version: '1.2.3-a',
      }),
    },
    'workspace-b': {
      'package.json': JSON.stringify({
        name: 'workspace-b',
        version: '1.2.3-b',
      }),
    },
    'workspace-c': JSON.stringify({
      'package.json': {
        name: 'workspace-n',
        version: '1.2.3-n',
      },
    }),
  })

  t.test('all workspaces', async t => {
    const diffCalls = []
    libnpmdiff = async ([a, b]) => {
      diffCalls.push([a, b])
    }
    npm.prefix = path
    npm.localPrefix = path
    await diff.execWorkspaces([], [])
    t.same(
      diffCalls,
      [
        ['workspace-a@latest', join(`file:${path}`, 'workspace-a')],
        ['workspace-b@latest', join(`file:${path}`, 'workspace-b')],
      ],
      'should call libnpmdiff with workspaces params'
    )
  })

  t.test('one workspace', async t => {
    const diffCalls = []
    libnpmdiff = async ([a, b]) => {
      diffCalls.push([a, b])
    }
    npm.prefix = path
    npm.localPrefix = path
    await diff.execWorkspaces([], ['workspace-a'])
    t.same(
      diffCalls,
      [['workspace-a@latest', join(`file:${path}`, 'workspace-a')]],
      'should call libnpmdiff with workspaces params'
    )
  })

  t.test('invalid workspace', async t => {
    libnpmdiff = () => {
      t.fail('should not call libnpmdiff')
    }
    npm.prefix = path
    npm.localPrefix = path
    await t.rejects(diff.execWorkspaces([], ['workspace-x']), /No workspaces found/)
    await t.rejects(diff.execWorkspaces([], ['workspace-x']), /workspace-x/)
  })
  t.end()
})

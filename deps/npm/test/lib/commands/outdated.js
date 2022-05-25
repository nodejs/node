const t = require('tap')
const { fake: mockNpm } = require('../../fixtures/mock-npm')

const packument = spec => {
  const mocks = {
    cat: {
      name: 'cat',
      'dist-tags': {
        latest: '1.0.1',
      },
      versions: {
        '1.0.1': {
          version: '1.0.1',
          dependencies: {
            dog: '2.0.0',
          },
        },
      },
    },
    chai: {
      name: 'chai',
      'dist-tags': {
        latest: '1.0.1',
      },
      versions: {
        '1.0.1': {
          version: '1.0.1',
        },
      },
    },
    dog: {
      name: 'dog',
      'dist-tags': {
        latest: '2.0.0',
      },
      versions: {
        '1.0.1': {
          version: '1.0.1',
        },
        '2.0.0': {
          version: '2.0.0',
        },
      },
    },
    theta: {
      name: 'theta',
      'dist-tags': {
        latest: '1.0.1',
      },
      versions: {
        '1.0.1': {
          version: '1.0.1',
        },
      },
    },
  }

  if (spec.name === 'eta') {
    throw new Error('There is an error with this package.')
  }

  if (!mocks[spec.name]) {
    const err = new Error()
    err.code = 'E404'
    throw err
  }

  return mocks[spec.name]
}

let logs
const output = (msg) => {
  logs = `${logs}\n${msg}`
}

const globalDir = t.testdir({
  node_modules: {
    cat: {
      'package.json': JSON.stringify({
        name: 'cat',
        version: '1.0.0',
      }, null, 2),
    },
  },
})

const outdated = (dir, opts) => {
  logs = ''
  const Outdated = t.mock('../../../lib/commands/outdated.js', {
    pacote: {
      packument,
    },
  })
  if (opts.config && opts.config.omit) {
    opts.flatOptions = {
      omit: opts.config.omit,
      ...opts.flatOptions,
    }
    delete opts.config.omit
  }
  const npm = mockNpm({
    ...opts,
    localPrefix: dir,
    prefix: dir,
    flatOptions: {
      workspacesEnabled: true,
      omit: [],
      ...opts.flatOptions,
    },
    globalDir: `${globalDir}/node_modules`,
    output,
  })
  return new Outdated(npm)
}

t.beforeEach(() => logs = '')

const { exitCode } = process

t.afterEach(() => process.exitCode = exitCode)

const redactCwd = (path) => {
  const normalizePath = p => p
    .replace(/\\+/g, '/')
    .replace(/\r\n/g, '\n')
  return normalizePath(path)
    .replace(new RegExp(normalizePath(process.cwd()), 'g'), '{CWD}')
}

t.cleanSnapshot = (str) => redactCwd(str)

t.test('should display outdated deps', t => {
  const testDir = t.testdir({
    'package.json': JSON.stringify({
      name: 'delta',
      version: '1.0.0',
      dependencies: {
        cat: '^1.0.0',
        dog: '^1.0.0',
        theta: '^1.0.0',
      },
      devDependencies: {
        zeta: '^1.0.0',
      },
      optionalDependencies: {
        lorem: '^1.0.0',
      },
      peerDependencies: {
        chai: '^1.0.0',
      },
    }, null, 2),
    node_modules: {
      cat: {
        'package.json': JSON.stringify({
          name: 'cat',
          version: '1.0.0',
          dependencies: {
            dog: '2.0.0',
          },
        }, null, 2),
        node_modules: {
          dog: {
            'package.json': JSON.stringify({
              name: 'dog',
              version: '2.0.0',
            }, null, 2),
          },
        },
      },
      chai: {
        'package.json': JSON.stringify({
          name: 'chai',
          version: '1.0.0',
        }, null, 2),
      },
      dog: {
        'package.json': JSON.stringify({
          name: 'dog',
          version: '1.0.1',
        }, null, 2),
      },
      zeta: {
        'package.json': JSON.stringify({
          name: 'zeta',
          version: '1.0.0',
        }, null, 2),
      },
    },
  })

  t.test('outdated global', async t => {
    await outdated(null, {
      config: { global: true },
    }).exec([])
    t.equal(process.exitCode, 1)
    t.matchSnapshot(logs)
  })

  t.test('outdated', async t => {
    await outdated(testDir, {
      config: {
        global: false,
      },
      color: true,
    }).exec([])
    t.equal(process.exitCode, 1)
    t.matchSnapshot(logs)
  })

  t.test('outdated --omit=dev', async t => {
    await outdated(testDir, {
      config: {
        global: false,
        omit: ['dev'],
      },
      color: true,
    }).exec([])
    t.equal(process.exitCode, 1)
    t.matchSnapshot(logs)
  })

  t.test('outdated --omit=dev --omit=peer', async t => {
    await outdated(testDir, {
      config: {
        global: false,
        omit: ['dev', 'peer'],
      },
      color: true,
    }).exec([])
    t.equal(process.exitCode, 1)
    t.matchSnapshot(logs)
  })

  t.test('outdated --omit=prod', async t => {
    await outdated(testDir, {
      config: {
        global: false,
        omit: ['prod'],
      },
      color: true,
    }).exec([])
    t.equal(process.exitCode, 1)
    t.matchSnapshot(logs)
  })

  t.test('outdated --long', async t => {
    await outdated(testDir, {
      config: {
        global: false,
        long: true,
      },
    }).exec([])
    t.equal(process.exitCode, 1)
    t.matchSnapshot(logs)
  })

  t.test('outdated --json', async t => {
    await outdated(testDir, {
      config: {
        global: false,
        json: true,
      },
    }).exec([])
    t.equal(process.exitCode, 1)
    t.matchSnapshot(logs)
  })

  t.test('outdated --json --long', async t => {
    await outdated(testDir, {
      config: {
        global: false,
        json: true,
        long: true,
      },
    }).exec([])
    t.equal(process.exitCode, 1)
    t.matchSnapshot(logs)
  })

  t.test('outdated --parseable', async t => {
    await outdated(testDir, {
      config: {
        global: false,
        parseable: true,
      },
    }).exec([])
    t.equal(process.exitCode, 1)
    t.matchSnapshot(logs)
  })

  t.test('outdated --parseable --long', async t => {
    await outdated(testDir, {
      config: {
        global: false,
        parseable: true,
        long: true,
      },
    }).exec([])
    t.equal(process.exitCode, 1)
    t.matchSnapshot(logs)
  })

  t.test('outdated --all', async t => {
    await outdated(testDir, {
      config: {
        all: true,
      },
    }).exec([])
    t.equal(process.exitCode, 1)
    t.matchSnapshot(logs)
  })

  t.test('outdated specific dep', async t => {
    await outdated(testDir, {
      config: {
        global: false,
      },
    }).exec(['cat'])
    t.equal(process.exitCode, 1)
    t.matchSnapshot(logs)
  })

  t.end()
})

t.test('should return if no outdated deps', async t => {
  const testDir = t.testdir({
    'package.json': JSON.stringify({
      name: 'delta',
      version: '1.0.0',
      dependencies: {
        cat: '^1.0.0',
      },
    }, null, 2),
    node_modules: {
      cat: {
        'package.json': JSON.stringify({
          name: 'cat',
          version: '1.0.1',
        }, null, 2),
      },
    },
  })

  await outdated(testDir, {
    config: {
      global: false,
    },
  }).exec([])
  t.equal(logs.length, 0, 'no logs')
})

t.test('throws if error with a dep', async t => {
  const testDir = t.testdir({
    'package.json': JSON.stringify({
      name: 'delta',
      version: '1.0.0',
      dependencies: {
        eta: '^1.0.0',
      },
    }, null, 2),
    node_modules: {
      eta: {
        'package.json': JSON.stringify({
          name: 'eta',
          version: '1.0.1',
        }, null, 2),
      },
    },
  })

  await t.rejects(
    outdated(testDir, {
      config: {
        global: false,
      },
    }).exec([]),
    'There is an error with this package.'
  )
})

t.test('should skip missing non-prod deps', async t => {
  const testDir = t.testdir({
    'package.json': JSON.stringify({
      name: 'delta',
      version: '1.0.0',
      devDependencies: {
        chai: '^1.0.0',
      },
    }, null, 2),
    node_modules: {},
  })

  await outdated(testDir, {
    config: {
      global: false,
    },
  }).exec([])
  t.equal(logs.length, 0, 'no logs')
})

t.test('should skip invalid pkg ranges', async t => {
  const testDir = t.testdir({
    'package.json': JSON.stringify({
      name: 'delta',
      version: '1.0.0',
      dependencies: {
        cat: '>=^2',
      },
    }, null, 2),
    node_modules: {
      cat: {
        'package.json': JSON.stringify({
          name: 'cat',
          version: '1.0.0',
        }, null, 2),
      },
    },
  })

  await outdated(testDir, {}).exec([])
  t.equal(logs.length, 0, 'no logs')
})

t.test('should skip git specs', async t => {
  const testDir = t.testdir({
    'package.json': JSON.stringify({
      name: 'delta',
      version: '1.0.0',
      dependencies: {
        cat: 'github:username/foo',
      },
    }, null, 2),
    node_modules: {
      cat: {
        'package.json': JSON.stringify({
          name: 'cat',
          version: '1.0.0',
        }, null, 2),
      },
    },
  })

  await outdated(testDir, {}).exec([])
  t.equal(logs.length, 0, 'no logs')
})

t.test('workspaces', async t => {
  const testDir = t.testdir({
    'package.json': JSON.stringify({
      name: 'workspaces-project',
      version: '1.0.0',
      workspaces: ['packages/*'],
      dependencies: {
        dog: '^1.0.0',
      },
    }),
    node_modules: {
      a: t.fixture('symlink', '../packages/a'),
      b: t.fixture('symlink', '../packages/b'),
      c: t.fixture('symlink', '../packages/c'),
      cat: {
        'package.json': JSON.stringify({
          name: 'cat',
          version: '1.0.0',
          dependencies: {
            dog: '2.0.0',
          },
        }),
        node_modules: {
          dog: {
            'package.json': JSON.stringify({
              name: 'dog',
              version: '2.0.0',
            }),
          },
        },
      },
      chai: {
        'package.json': JSON.stringify({
          name: 'chai',
          version: '1.0.0',
        }),
      },
      dog: {
        'package.json': JSON.stringify({
          name: 'dog',
          version: '1.0.1',
        }),
      },
      foo: {
        'package.json': JSON.stringify({
          name: 'foo',
          version: '1.0.0',
          dependencies: {
            chai: '^1.0.0',
          },
        }),
      },
      zeta: {
        'package.json': JSON.stringify({
          name: 'zeta',
          version: '1.0.0',
        }),
      },
    },
    packages: {
      a: {
        'package.json': JSON.stringify({
          name: 'a',
          version: '1.0.0',
          dependencies: {
            b: '^1.0.0',
            cat: '^1.0.0',
            foo: '^1.0.0',
          },
        }),
      },
      b: {
        'package.json': JSON.stringify({
          name: 'b',
          version: '1.0.0',
          dependencies: {
            zeta: '^1.0.0',
          },
        }),
      },
      c: {
        'package.json': JSON.stringify({
          name: 'c',
          version: '1.0.0',
          dependencies: {
            theta: '^1.0.0',
          },
        }),
      },
    },
  })

  await outdated(testDir, {}).exec([])

  t.matchSnapshot(logs, 'should display ws outdated deps human output')
  t.equal(process.exitCode, 1)

  await outdated(testDir, {
    flatOptions: {
      workspacesEnabled: false,
    },
  }).exec([])

  // TODO: This should display dog, but doesn't because arborist filters
  // workspace deps even if they're also root deps
  // This will be fixed in a future arborist version
  t.matchSnapshot(logs, 'should display only root outdated when ws disabled')

  await outdated(testDir, {
    config: {
      json: true,
    },
  }).exec([])
  t.matchSnapshot(logs, 'should display ws outdated deps json output')
  t.equal(process.exitCode, 1)

  await outdated(testDir, {
    config: {
      parseable: true,
    },
  }).exec([])

  t.matchSnapshot(logs, 'should display ws outdated deps parseable output')
  t.equal(process.exitCode, 1)

  await outdated(testDir, {
    config: {
      all: true,
    },
  }).exec([])

  t.matchSnapshot(logs, 'should display all dependencies')
  t.equal(process.exitCode, 1)

  await outdated(testDir, {
    color: true,
  }).exec([])

  t.matchSnapshot(logs, 'should highlight ws in dependend by section')
  t.equal(process.exitCode, 1)

  await outdated(testDir, {}).execWorkspaces([], ['a'])
  t.matchSnapshot(logs, 'should display results filtered by ws')
  t.equal(process.exitCode, 1)

  await outdated(testDir, {
    config: {
      json: true,
    },
  }).execWorkspaces([], ['a'])
  t.matchSnapshot(logs, 'should display json results filtered by ws')
  t.equal(process.exitCode, 1)

  await outdated(testDir, {
    config: {
      parseable: true,
    },
  }).execWorkspaces([], ['a'])
  t.matchSnapshot(logs, 'should display parseable results filtered by ws')
  t.equal(process.exitCode, 1)

  await outdated(testDir, {
    config: {
      all: true,
    },
  }).execWorkspaces([], ['a'])

  t.matchSnapshot(logs,
    'should display nested deps when filtering by ws and using --all')
  t.equal(process.exitCode, 1)

  await outdated(testDir, {}).execWorkspaces([], ['b'])
  t.matchSnapshot(logs,
    'should display no results if ws has no deps to display')

  await outdated(testDir, {}).execWorkspaces([], ['c'])
  t.matchSnapshot(logs,
    'should display missing deps when filtering by ws')
})

t.test('aliases', async t => {
  const testDir = t.testdir({
    'package.json': JSON.stringify({
      name: 'display-aliases',
      version: '1.0.0',
      dependencies: {
        cat: 'npm:dog@latest',
      },
    }),
    node_modules: {
      cat: {
        'package.json': JSON.stringify({
          name: 'dog',
          version: '1.0.0',
        }),
      },
    },
  })

  await outdated(testDir, {}).exec([])

  t.matchSnapshot(logs, 'should display aliased outdated dep output')
  t.equal(process.exitCode, 1)
})

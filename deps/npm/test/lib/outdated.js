const t = require('tap')
const { fake: mockNpm } = require('../fixtures/mock-npm')

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

  if (spec.name === 'eta')
    throw new Error('There is an error with this package.')

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
  const Outdated = t.mock('../../lib/outdated.js', {
    pacote: {
      packument,
    },
  })
  const npm = mockNpm({
    ...opts,
    localPrefix: dir,
    prefix: dir,
    globalDir: `${globalDir}/node_modules`,
    output,
  })
  return new Outdated(npm)
}

t.beforeEach(() => logs = '')

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

  t.test('outdated global', t => {
    outdated(null, {
      config: { global: true },
    }).exec([], () => {
      t.matchSnapshot(logs)
      t.end()
    })
  })

  t.test('outdated', t => {
    outdated(testDir, {
      config: {
        global: false,
      },
      color: true,
    }).exec([], () => {
      t.matchSnapshot(logs)
      t.end()
    })
  })

  t.test('outdated --omit=dev', t => {
    outdated(testDir, {
      config: {
        global: false,
        omit: ['dev'],
      },
      color: true,
    }).exec([], () => {
      t.matchSnapshot(logs)
      t.end()
    })
  })

  t.test('outdated --omit=dev --omit=peer', t => {
    outdated(testDir, {
      config: {
        global: false,
        omit: ['dev', 'peer'],
      },
      color: true,
    }).exec([], () => {
      t.matchSnapshot(logs)
      t.end()
    })
  })

  t.test('outdated --omit=prod', t => {
    outdated(testDir, {
      config: {
        global: false,
        omit: ['prod'],
      },
      color: true,
    }).exec([], () => {
      t.matchSnapshot(logs)
      t.end()
    })
  })

  t.test('outdated --long', t => {
    outdated(testDir, {
      config: {
        global: false,
        long: true,
      },
    }).exec([], () => {
      t.matchSnapshot(logs)
      t.end()
    })
  })

  t.test('outdated --json', t => {
    outdated(testDir, {
      config: {
        global: false,
        json: true,
      },
    }).exec([], () => {
      t.matchSnapshot(logs)
      t.end()
    })
  })

  t.test('outdated --json --long', t => {
    outdated(testDir, {
      config: {
        global: false,
        json: true,
        long: true,
      },
    }).exec([], () => {
      t.matchSnapshot(logs)
      t.end()
    })
  })

  t.test('outdated --parseable', t => {
    outdated(testDir, {
      config: {
        global: false,
        parseable: true,
      },
    }).exec([], () => {
      t.matchSnapshot(logs)
      t.end()
    })
  })

  t.test('outdated --parseable --long', t => {
    outdated(testDir, {
      config: {
        global: false,
        parseable: true,
        long: true,
      },
    }).exec([], () => {
      t.matchSnapshot(logs)
      t.end()
    })
  })

  t.test('outdated --all', t => {
    outdated(testDir, {
      config: {
        all: true,
      },
    }).exec([], () => {
      t.matchSnapshot(logs)
      t.end()
    })
  })

  t.test('outdated specific dep', t => {
    outdated(testDir, {
      config: {
        global: false,
      },
    }).exec(['cat'], () => {
      t.matchSnapshot(logs)
      t.end()
    })
  })

  t.end()
})

t.test('should return if no outdated deps', t => {
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

  outdated(testDir, {
    global: false,
  }).exec([], () => {
    t.equal(logs.length, 0, 'no logs')
    t.end()
  })
})

t.test('throws if error with a dep', t => {
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

  outdated(testDir, {
    global: false,
  }).exec([], (err) => {
    t.equal(err.message, 'There is an error with this package.')
    t.end()
  })
})

t.test('should skip missing non-prod deps', t => {
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

  outdated(testDir, {
    global: false,
  }).exec([], () => {
    t.equal(logs.length, 0, 'no logs')
    t.end()
  })
})

t.test('should skip invalid pkg ranges', t => {
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

  outdated(testDir, {}).exec([], () => {
    t.equal(logs.length, 0, 'no logs')
    t.end()
  })
})

t.test('should skip git specs', t => {
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

  outdated(testDir, {}).exec([], () => {
    t.equal(logs.length, 0, 'no logs')
    t.end()
  })
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

  await new Promise((res, rej) => {
    outdated(testDir, {}).exec([], err => {
      if (err)
        rej(err)

      t.matchSnapshot(logs, 'should display ws outdated deps human output')
      res()
    })
  })

  await new Promise((res, rej) => {
    outdated(testDir, {
      config: {
        json: true,
      },
    }).exec([], err => {
      if (err)
        rej(err)

      t.matchSnapshot(logs, 'should display ws outdated deps json output')
      res()
    })
  })

  await new Promise((res, rej) => {
    outdated(testDir, {
      config: {
        parseable: true,
      },
    }).exec([], err => {
      if (err)
        rej(err)

      t.matchSnapshot(logs, 'should display ws outdated deps parseable output')
      res()
    })
  })

  await new Promise((res, rej) => {
    outdated(testDir, {
      config: {
        all: true,
      },
    }).exec([], err => {
      if (err)
        rej(err)

      t.matchSnapshot(logs, 'should display all dependencies')
      res()
    })
  })

  await new Promise((res, rej) => {
    outdated(testDir, {
      color: true,
    }).exec([], err => {
      if (err)
        rej(err)

      t.matchSnapshot(logs, 'should highlight ws in dependend by section')
      res()
    })
  })

  await new Promise((res, rej) => {
    outdated(testDir, {}).execWorkspaces([], ['a'], err => {
      if (err)
        rej(err)

      t.matchSnapshot(logs, 'should display results filtered by ws')
      res()
    })
  })

  await new Promise((res, rej) => {
    outdated(testDir, {
      config: {
        json: true,
      },
    }).execWorkspaces([], ['a'], err => {
      if (err)
        rej(err)

      t.matchSnapshot(logs, 'should display json results filtered by ws')
      res()
    })
  })

  await new Promise((res, rej) => {
    outdated(testDir, {
      config: {
        parseable: true,
      },
    }).execWorkspaces([], ['a'], err => {
      if (err)
        rej(err)

      t.matchSnapshot(logs, 'should display parseable results filtered by ws')
      res()
    })
  })

  await new Promise((res, rej) => {
    outdated(testDir, {
      config: {
        all: true,
      },
    }).execWorkspaces([], ['a'], err => {
      if (err)
        rej(err)

      t.matchSnapshot(logs,
        'should display nested deps when filtering by ws and using --all')
      res()
    })
  })

  await new Promise((res, rej) => {
    outdated(testDir, {}).execWorkspaces([], ['b'], err => {
      if (err)
        rej(err)

      t.matchSnapshot(logs,
        'should display no results if ws has no deps to display')
      res()
    })
  })

  await new Promise((res, rej) => {
    outdated(testDir, {}).execWorkspaces([], ['c'], err => {
      if (err)
        rej(err)

      t.matchSnapshot(logs,
        'should display missing deps when filtering by ws')
      res()
    })
  })
})

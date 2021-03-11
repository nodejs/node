const t = require('tap')
const requireInject = require('require-inject')

const packument = spec => {
  const mocks = {
    alpha: {
      name: 'alpha',
      'dist-tags': {
        latest: '1.0.1',
      },
      versions: {
        '1.0.1': {
          version: '1.0.1',
          dependencies: {
            gamma: '2.0.0',
          },
        },
      },
    },
    beta: {
      name: 'beta',
      'dist-tags': {
        latest: '1.0.1',
      },
      versions: {
        '1.0.1': {
          version: '1.0.1',
        },
      },
    },
    gamma: {
      name: 'gamma',
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
    alpha: {
      'package.json': JSON.stringify({
        name: 'alpha',
        version: '1.0.0',
      }, null, 2),
    },
  },
})

const outdated = (dir, opts) => {
  const Outdated = requireInject('../../lib/outdated.js', {
    pacote: {
      packument,
    },
  })
  return new Outdated({
    prefix: dir,
    globalDir: `${globalDir}/node_modules`,
    flatOptions: opts,
    output,
  })
}

t.beforeEach((done) => {
  logs = ''
  done()
})

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
        alpha: '^1.0.0',
        gamma: '^1.0.0',
        theta: '^1.0.0',
      },
      devDependencies: {
        zeta: '^1.0.0',
      },
      optionalDependencies: {
        lorem: '^1.0.0',
      },
      peerDependencies: {
        beta: '^1.0.0',
      },
    }, null, 2),
    node_modules: {
      alpha: {
        'package.json': JSON.stringify({
          name: 'alpha',
          version: '1.0.0',
          dependencies: {
            gamma: '2.0.0',
          },
        }, null, 2),
        node_modules: {
          gamma: {
            'package.json': JSON.stringify({
              name: 'gamma',
              version: '2.0.0',
            }, null, 2),
          },
        },
      },
      beta: {
        'package.json': JSON.stringify({
          name: 'beta',
          version: '1.0.0',
        }, null, 2),
      },
      gamma: {
        'package.json': JSON.stringify({
          name: 'gamma',
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
      global: true,
    }).exec([], () => {
      t.matchSnapshot(logs)
      t.end()
    })
  })

  t.test('outdated', t => {
    outdated(testDir, {
      global: false,
      color: true,
    }).exec([], () => {
      t.matchSnapshot(logs)
      t.end()
    })
  })

  t.test('outdated --omit=dev', t => {
    outdated(testDir, {
      global: false,
      color: true,
      omit: ['dev'],
    }).exec([], () => {
      t.matchSnapshot(logs)
      t.end()
    })
  })

  t.test('outdated --omit=dev --omit=peer', t => {
    outdated(testDir, {
      global: false,
      color: true,
      omit: ['dev', 'peer'],
    }).exec([], () => {
      t.matchSnapshot(logs)
      t.end()
    })
  })

  t.test('outdated --omit=prod', t => {
    outdated(testDir, {
      global: false,
      color: true,
      omit: ['prod'],
    }).exec([], () => {
      t.matchSnapshot(logs)
      t.end()
    })
  })

  t.test('outdated --long', t => {
    outdated(testDir, {
      global: false,
      long: true,
    }).exec([], () => {
      t.matchSnapshot(logs)
      t.end()
    })
  })

  t.test('outdated --json', t => {
    outdated(testDir, {
      global: false,
      json: true,
    }).exec([], () => {
      t.matchSnapshot(logs)
      t.end()
    })
  })

  t.test('outdated --json --long', t => {
    outdated(testDir, {
      global: false,
      json: true,
      long: true,
    }).exec([], () => {
      t.matchSnapshot(logs)
      t.end()
    })
  })

  t.test('outdated --parseable', t => {
    outdated(testDir, {
      global: false,
      parseable: true,
    }).exec([], () => {
      t.matchSnapshot(logs)
      t.end()
    })
  })

  t.test('outdated --parseable --long', t => {
    outdated(testDir, {
      global: false,
      parseable: true,
      long: true,
    }).exec([], () => {
      t.matchSnapshot(logs)
      t.end()
    })
  })

  t.test('outdated --all', t => {
    outdated(testDir, {
      all: true,
    }).exec([], () => {
      t.matchSnapshot(logs)
      t.end()
    })
  })

  t.test('outdated specific dep', t => {
    outdated(testDir, {
      global: false,
    }).exec(['alpha'], () => {
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
        alpha: '^1.0.0',
      },
    }, null, 2),
    node_modules: {
      alpha: {
        'package.json': JSON.stringify({
          name: 'alpha',
          version: '1.0.1',
        }, null, 2),
      },
    },
  })

  outdated(testDir, {
    global: false,
  }).exec([], () => {
    t.equals(logs.length, 0, 'no logs')
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
    t.equals(err.message, 'There is an error with this package.')
    t.end()
  })
})

t.test('should skip missing non-prod deps', t => {
  const testDir = t.testdir({
    'package.json': JSON.stringify({
      name: 'delta',
      version: '1.0.0',
      devDependencies: {
        beta: '^1.0.0',
      },
    }, null, 2),
    node_modules: {},
  })

  outdated(testDir, {
    global: false,
  }).exec([], () => {
    t.equals(logs.length, 0, 'no logs')
    t.end()
  })
})

t.test('should skip invalid pkg ranges', t => {
  const testDir = t.testdir({
    'package.json': JSON.stringify({
      name: 'delta',
      version: '1.0.0',
      dependencies: {
        alpha: '>=^2',
      },
    }, null, 2),
    node_modules: {
      alpha: {
        'package.json': JSON.stringify({
          name: 'alpha',
          version: '1.0.0',
        }, null, 2),
      },
    },
  })

  outdated(testDir, {}).exec([], () => {
    t.equals(logs.length, 0, 'no logs')
    t.end()
  })
})

t.test('should skip git specs', t => {
  const testDir = t.testdir({
    'package.json': JSON.stringify({
      name: 'delta',
      version: '1.0.0',
      dependencies: {
        alpha: 'github:username/foo',
      },
    }, null, 2),
    node_modules: {
      alpha: {
        'package.json': JSON.stringify({
          name: 'alpha',
          version: '1.0.0',
        }, null, 2),
      },
    },
  })

  outdated(testDir, {}).exec([], () => {
    t.equals(logs.length, 0, 'no logs')
    t.end()
  })
})

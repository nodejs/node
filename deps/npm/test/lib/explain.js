const t = require('tap')
const npm = {
  prefix: null,
  color: true,
  flatOptions: {},
  output: (...args) => {
    OUTPUT.push(args)
  },
}
const { resolve } = require('path')

const OUTPUT = []

const Explain = t.mock('../../lib/explain.js', {

  // keep the snapshots pared down a bit, since this has its own tests.
  '../../lib/utils/explain-dep.js': {
    explainNode: (expl, depth, color) => {
      return `${expl.name}@${expl.version} depth=${depth} color=${color}`
    },
  },
})
const explain = new Explain(npm)

t.test('no args throws usage', t => {
  t.plan(1)
  explain.exec([], er => {
    t.equal(er, explain.usage)
    t.end()
  })
})

t.test('no match throws not found', t => {
  npm.prefix = t.testdir()
  t.plan(1)
  explain.exec(['foo@1.2.3', 'node_modules/baz'], er => {
    t.equal(er, 'No dependencies found matching foo@1.2.3, node_modules/baz')
  })
})

t.test('invalid package name throws not found', t => {
  npm.prefix = t.testdir()
  t.plan(1)
  const badName = ' not a valid package name '
  explain.exec([`${badName}@1.2.3`], er => {
    t.equal(er, `No dependencies found matching ${badName}@1.2.3`)
  })
})

t.test('explain some nodes', t => {
  t.afterEach(() => {
    OUTPUT.length = 0
    npm.flatOptions.json = false
  })

  npm.prefix = t.testdir({
    node_modules: {
      foo: {
        'package.json': JSON.stringify({
          name: 'foo',
          version: '1.2.3',
          dependencies: {
            bar: '*',
          },
        }),
      },
      bar: {
        'package.json': JSON.stringify({
          name: 'bar',
          version: '1.2.3',
        }),
      },
      baz: {
        'package.json': JSON.stringify({
          name: 'baz',
          version: '1.2.3',
          dependencies: {
            foo: '*',
            bar: '2',
          },
        }),
        node_modules: {
          bar: {
            'package.json': JSON.stringify({
              name: 'bar',
              version: '2.3.4',
            }),
          },
          extra: {
            'package.json': JSON.stringify({
              name: 'extra',
              version: '99.9999.999999',
              description: 'extraneous package',
            }),
          },
        },
      },
    },
    'package.json': JSON.stringify({
      dependencies: {
        baz: '1',
      },
    }),
  })

  t.test('works with the location', t => {
    const path = 'node_modules/foo'
    explain.exec([path], er => {
      if (er)
        throw er
      t.strictSame(OUTPUT, [['foo@1.2.3 depth=Infinity color=true']])
      t.end()
    })
  })
  t.test('works with a full actual path', t => {
    const path = resolve(npm.prefix, 'node_modules/foo')
    explain.exec([path], er => {
      if (er)
        throw er
      t.strictSame(OUTPUT, [['foo@1.2.3 depth=Infinity color=true']])
      t.end()
    })
  })

  t.test('finds all nodes by name', t => {
    explain.exec(['bar'], er => {
      if (er)
        throw er
      t.strictSame(OUTPUT, [[
        'bar@1.2.3 depth=Infinity color=true\n\n' +
        'bar@2.3.4 depth=Infinity color=true',
      ]])
      t.end()
    })
  })

  t.test('finds only nodes that match the spec', t => {
    explain.exec(['bar@1'], er => {
      if (er)
        throw er
      t.strictSame(OUTPUT, [['bar@1.2.3 depth=Infinity color=true']])
      t.end()
    })
  })

  t.test('finds extraneous nodes', t => {
    explain.exec(['extra'], er => {
      if (er)
        throw er
      t.strictSame(OUTPUT, [['extra@99.9999.999999 depth=Infinity color=true']])
      t.end()
    })
  })

  t.test('json output', t => {
    npm.flatOptions.json = true
    explain.exec(['node_modules/foo'], er => {
      if (er)
        throw er
      t.match(JSON.parse(OUTPUT[0][0]), [{
        name: 'foo',
        version: '1.2.3',
        dependents: Array,
      }])
      t.end()
    })
  })

  t.test('report if no nodes found', t => {
    t.plan(1)
    explain.exec(['asdf/foo/bar', 'quux@1.x'], er => {
      t.equal(er, 'No dependencies found matching asdf/foo/bar, quux@1.x')
      t.end()
    })
  })
  t.end()
})

t.test('workspaces', async t => {
  npm.localPrefix = npm.prefix = t.testdir({
    'package.json': JSON.stringify({
      name: 'workspaces-project',
      version: '1.0.0',
      workspaces: ['packages/*'],
      dependencies: {
        abbrev: '^1.0.0',
      },
    }),
    node_modules: {
      a: t.fixture('symlink', '../packages/a'),
      b: t.fixture('symlink', '../packages/b'),
      c: t.fixture('symlink', '../packages/c'),
      once: {
        'package.json': JSON.stringify({
          name: 'once',
          version: '1.0.0',
          dependencies: {
            wrappy: '2.0.0',
          },
        }),
      },
      abbrev: {
        'package.json': JSON.stringify({
          name: 'abbrev',
          version: '1.0.0',
        }),
      },
      wrappy: {
        'package.json': JSON.stringify({
          name: 'wrappy',
          version: '2.0.0',
        }),
      },
    },
    packages: {
      a: {
        'package.json': JSON.stringify({
          name: 'a',
          version: '1.0.0',
          dependencies: {
            once: '1.0.0',
          },
        }),
      },
      b: {
        'package.json': JSON.stringify({
          name: 'b',
          version: '1.0.0',
          dependencies: {
            abbrev: '^1.0.0',
          },
        }),
      },
      c: {
        'package.json': JSON.stringify({
          name: 'c',
          version: '1.0.0',
        }),
      },
    },
  })

  await new Promise((res, rej) => {
    explain.exec(['wrappy'], err => {
      if (err)
        rej(err)

      t.strictSame(
        OUTPUT,
        [['wrappy@2.0.0 depth=Infinity color=true']],
        'should explain workspaces deps'
      )
      OUTPUT.length = 0
      res()
    })
  })

  await new Promise((res, rej) => {
    explain.execWorkspaces(['wrappy'], ['a'], err => {
      if (err)
        rej(err)

      t.strictSame(
        OUTPUT,
        [
          ['wrappy@2.0.0 depth=Infinity color=true'],
        ],
        'should explain deps when filtering to a single ws'
      )
      OUTPUT.length = 0
      res()
    })
  })

  await new Promise((res, rej) => {
    explain.execWorkspaces(['abbrev'], [], err => {
      if (err)
        rej(err)

      t.strictSame(
        OUTPUT,
        [
          ['abbrev@1.0.0 depth=Infinity color=true'],
        ],
        'should explain deps of workspaces only'
      )
      OUTPUT.length = 0
      res()
    })
  })

  await new Promise((res, rej) => {
    explain.execWorkspaces(['abbrev'], ['a'], err => {
      t.equal(
        err,
        'No dependencies found matching abbrev',
        'should throw usage if dep not found within filtered ws'
      )

      res()
    })
  })
})

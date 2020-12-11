const t = require('tap')
const requireInject = require('require-inject')
const npm = {
  prefix: null,
  color: true,
  flatOptions: {},
}
const { resolve } = require('path')

const OUTPUT = []

const explain = requireInject('../../lib/explain.js', {
  '../../lib/npm.js': npm,

  '../../lib/utils/output.js': (...args) => {
    OUTPUT.push(args)
  },

  // keep the snapshots pared down a bit, since this has its own tests.
  '../../lib/utils/explain-dep.js': {
    explainNode: (expl, depth, color) => {
      return `${expl.name}@${expl.version} depth=${depth} color=${color}`
    },
  },
})

t.test('no args throws usage', async t => {
  t.plan(1)
  try {
    await explain([], er => {
      throw er
    })
  } catch (er) {
    t.equal(er, explain.usage)
  }
})

t.test('no match throws not found', async t => {
  npm.prefix = t.testdir()
  t.plan(1)
  try {
    await explain(['foo@1.2.3', 'node_modules/baz'], er => {
      throw er
    })
  } catch (er) {
    t.equal(er, 'No dependencies found matching foo@1.2.3, node_modules/baz')
  }
})

t.test('invalid package name throws not found', async t => {
  npm.prefix = t.testdir()
  t.plan(1)
  const badName = ' not a valid package name '
  try {
    await explain([`${badName}@1.2.3`], er => {
      throw er
    })
  } catch (er) {
    t.equal(er, `No dependencies found matching ${badName}@1.2.3`)
  }
})

t.test('explain some nodes', async t => {
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

  // works with either a full actual path or the location
  const p = 'node_modules/foo'
  for (const path of [p, resolve(npm.prefix, p)]) {
    await explain([path], er => {
      if (er)
        throw er
    })
    t.strictSame(OUTPUT, [['foo@1.2.3 depth=Infinity color=true']])
    OUTPUT.length = 0
  }

  // finds all nodes by name
  await explain(['bar'], er => {
    if (er)
      throw er
  })
  t.strictSame(OUTPUT, [[
    'bar@1.2.3 depth=Infinity color=true\n\n' +
    'bar@2.3.4 depth=Infinity color=true',
  ]])
  OUTPUT.length = 0

  // finds only nodes that match the spec
  await explain(['bar@1'], er => {
    if (er)
      throw er
  })
  t.strictSame(OUTPUT, [['bar@1.2.3 depth=Infinity color=true']])
  OUTPUT.length = 0

  // finds extraneous nodes
  await explain(['extra'], er => {
    if (er)
      throw er
  })
  t.strictSame(OUTPUT, [['extra@99.9999.999999 depth=Infinity color=true']])
  OUTPUT.length = 0

  npm.flatOptions.json = true
  await explain(['node_modules/foo'], er => {
    if (er)
      throw er
  })
  t.match(JSON.parse(OUTPUT[0][0]), [{
    name: 'foo',
    version: '1.2.3',
    dependents: Array,
  }])
  OUTPUT.length = 0
  npm.flatOptions.json = false

  t.test('report if no nodes found', async t => {
    t.plan(1)
    await explain(['asdf/foo/bar', 'quux@1.x'], er => {
      t.equal(er, 'No dependencies found matching asdf/foo/bar, quux@1.x')
    })
  })
})

const t = require('tap')
const { resolve } = require('path')
const { fake: mockNpm } = require('../fixtures/mock-npm')

const config = {
  depth: 0,
  global: false,
}
const noop = () => null
const npm = mockNpm({
  globalDir: '',
  log: noop,
  config,
  prefix: '',
})
const mocks = {
  npmlog: { warn () {} },
  '@npmcli/arborist': class {
    reify () {}
  },
  '../../lib/utils/reify-finish.js': noop,
  '../../lib/utils/usage.js': () => 'usage instructions',
}

t.afterEach(() => {
  npm.prefix = ''
  config.global = false
  npm.globalDir = ''
})

t.test('no args', t => {
  t.plan(3)

  npm.prefix = '/project/a'

  class Arborist {
    constructor (args) {
      t.same(
        args,
        {
          ...npm.flatOptions,
          path: npm.prefix,
          log: noop,
          workspaces: null,
        },
        'should call arborist contructor with expected args'
      )
    }

    reify ({ update }) {
      t.equal(update, true, 'should update all deps')
    }
  }

  const Update = t.mock('../../lib/update.js', {
    ...mocks,
    '../../lib/utils/reify-finish.js': (npm, arb) => {
      t.match(arb, Arborist, 'should reify-finish with arborist instance')
    },
    '@npmcli/arborist': Arborist,
  })
  const update = new Update(npm)

  update.exec([], err => {
    if (err)
      throw err
  })
})

t.test('with args', t => {
  t.plan(3)

  npm.prefix = '/project/a'

  class Arborist {
    constructor (args) {
      t.same(
        args,
        {
          ...npm.flatOptions,
          path: npm.prefix,
          log: noop,
          workspaces: null,
        },
        'should call arborist contructor with expected args'
      )
    }

    reify ({ update }) {
      t.same(update, ['ipt'], 'should update listed deps')
    }
  }

  const Update = t.mock('../../lib/update.js', {
    ...mocks,
    '../../lib/utils/reify-finish.js': (npm, arb) => {
      t.match(arb, Arborist, 'should reify-finish with arborist instance')
    },
    '@npmcli/arborist': Arborist,
  })
  const update = new Update(npm)

  update.exec(['ipt'], err => {
    if (err)
      throw err
  })
})

t.test('update --depth=<number>', t => {
  t.plan(2)

  npm.prefix = '/project/a'
  config.depth = 1

  const Update = t.mock('../../lib/update.js', {
    ...mocks,
    npmlog: {
      warn: (title, msg) => {
        t.equal(title, 'update', 'should print expected title')
        t.match(
          msg,
          /The --depth option no longer has any effect/,
          'should print expected warning message'
        )
      },
    },
  })
  const update = new Update(npm)

  update.exec([], err => {
    if (err)
      throw err
  })
})

t.test('update --global', t => {
  t.plan(2)

  const normalizePath = p => p.replace(/\\+/g, '/')
  const redactCwd = (path) => normalizePath(path)
    .replace(new RegExp(normalizePath(process.cwd()), 'g'), '{CWD}')

  npm.prefix = '/project/a'
  npm.globalDir = resolve(process.cwd(), 'global/lib/node_modules')
  config.global = true

  class Arborist {
    constructor (args) {
      const { path, ...opts } = args
      t.same(
        opts,
        { ...npm.flatOptions, log: noop, workspaces: undefined },
        'should call arborist contructor with expected options'
      )

      t.equal(
        redactCwd(path),
        '{CWD}/global/lib',
        'should run with expected prefix'
      )
    }

    reify () {}
  }

  const Update = t.mock('../../lib/update.js', {
    ...mocks,
    '@npmcli/arborist': Arborist,
  })
  const update = new Update(npm)

  update.exec([], err => {
    if (err)
      throw err
  })
})

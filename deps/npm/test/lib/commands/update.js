const t = require('tap')
const { resolve } = require('path')
const { fake: mockNpm } = require('../../fixtures/mock-npm')

const config = {
  depth: 0,
  global: false,
}
const noop = () => null
const npm = mockNpm({
  globalDir: '',
  config,
  prefix: '',
})
const mocks = {
  '@npmcli/arborist': class {
    reify () {}
  },
  '../../../lib/utils/reify-finish.js': noop,
}

t.afterEach(() => {
  npm.prefix = ''
  config.global = false
  npm.globalDir = ''
})

t.test('no args', async t => {
  t.plan(4)

  npm.prefix = '/project/a'

  class Arborist {
    constructor (args) {
      const { log, ...rest } = args
      t.same(
        rest,
        {
          ...npm.flatOptions,
          path: npm.prefix,
          save: false,
          workspaces: null,
        },
        'should call arborist contructor with expected args'
      )
    }

    reify ({ save, update }) {
      t.equal(save, false, 'should default to save=false')
      t.equal(update, true, 'should update all deps')
    }
  }

  const Update = t.mock('../../../lib/commands/update.js', {
    ...mocks,
    '../../../lib/utils/reify-finish.js': (npm, arb) => {
      t.match(arb, Arborist, 'should reify-finish with arborist instance')
    },
    '@npmcli/arborist': Arborist,
  })
  const update = new Update(npm)

  await update.exec([])
})

t.test('with args', async t => {
  t.plan(4)

  npm.prefix = '/project/a'
  config.save = true

  class Arborist {
    constructor (args) {
      const { log, ...rest } = args
      t.same(
        rest,
        {
          ...npm.flatOptions,
          path: npm.prefix,
          save: true,
          workspaces: null,
        },
        'should call arborist contructor with expected args'
      )
    }

    reify ({ save, update }) {
      t.equal(save, true, 'should pass save if manually set')
      t.same(update, ['ipt'], 'should update listed deps')
    }
  }

  const Update = t.mock('../../../lib/commands/update.js', {
    ...mocks,
    '../../../lib/utils/reify-finish.js': (npm, arb) => {
      t.match(arb, Arborist, 'should reify-finish with arborist instance')
    },
    '@npmcli/arborist': Arborist,
  })
  const update = new Update(npm)

  await update.exec(['ipt'])
})

t.test('update --depth=<number>', async t => {
  t.plan(2)

  npm.prefix = '/project/a'
  config.depth = 1

  const Update = t.mock('../../../lib/commands/update.js', {
    ...mocks,
    'proc-log': {
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

  await update.exec([])
})

t.test('update --global', async t => {
  t.plan(2)

  const normalizePath = p => p.replace(/\\+/g, '/')
  const redactCwd = (path) => normalizePath(path)
    .replace(new RegExp(normalizePath(process.cwd()), 'g'), '{CWD}')

  npm.prefix = '/project/a'
  npm.globalDir = resolve(process.cwd(), 'global/lib/node_modules')
  config.global = true

  class Arborist {
    constructor (args) {
      const { path, log, ...rest } = args
      t.same(
        rest,
        { ...npm.flatOptions, save: true, workspaces: undefined },
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

  const Update = t.mock('../../../lib/commands/update.js', {
    ...mocks,
    '@npmcli/arborist': Arborist,
  })
  const update = new Update(npm)

  await update.exec([])
})

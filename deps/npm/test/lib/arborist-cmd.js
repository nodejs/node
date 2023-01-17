const { resolve } = require('path')
const t = require('tap')
const { load: loadMockNpm } = require('../fixtures/mock-npm')
const tmock = require('../fixtures/tmock')

const mockArboristCmd = async (t, exec, workspace, { mocks = {}, ...opts } = {}) => {
  const ArboristCmd = tmock(t, '{LIB}/arborist-cmd.js', mocks)

  const config = (typeof workspace === 'function')
    ? (dirs) => ({ workspace: workspace(dirs) })
    : { workspace }

  const mock = await loadMockNpm(t, {
    config,
    prefixDir: {
      'package.json': JSON.stringify({
        name: 'simple-workspaces-list',
        version: '1.1.1',
        workspaces: [
          'a',
          'b',
          'group/*',
        ],
      }),
      node_modules: {
        abbrev: {
          'package.json': JSON.stringify({ name: 'abbrev', version: '1.1.1' }),
        },
        a: t.fixture('symlink', '../a'),
        b: t.fixture('symlink', '../b'),
      },
      a: {
        'package.json': JSON.stringify({ name: 'a', version: '1.0.0' }),
      },
      b: {
        'package.json': JSON.stringify({ name: 'b', version: '1.0.0' }),
      },
      group: {
        c: {
          'package.json': JSON.stringify({
            name: 'c',
            version: '1.0.0',
            dependencies: {
              abbrev: '^1.1.1',
            },
          }),
        },
        d: {
          'package.json': JSON.stringify({ name: 'd', version: '1.0.0' }),
        },
      },
    },
    ...opts,
  })

  let execArg
  class TestCmd extends ArboristCmd {
    async exec (arg) {
      execArg = arg
    }
  }

  const cmd = new TestCmd(mock.npm)
  if (exec) {
    await cmd.execWorkspaces(exec)
  }

  return { ...mock, cmd, getArg: () => execArg }
}

t.test('arborist-cmd', async t => {
  await t.test('single name', async t => {
    const { cmd, getArg } = await mockArboristCmd(t, ['foo'], 'a')

    t.same(cmd.workspaceNames, ['a'], 'should set array with single ws name')
    t.same(getArg(), ['foo'], 'should get received args')
  })

  await t.test('single path', async t => {
    const { cmd } = await mockArboristCmd(t, [], './a')

    t.same(cmd.workspaceNames, ['a'], 'should set array with single ws name')
  })

  await t.test('single full path', async t => {
    const { cmd } = await mockArboristCmd(t, [], ({ prefix }) => resolve(prefix, 'a'))

    t.same(cmd.workspaceNames, ['a'], 'should set array with single ws name')
  })

  await t.test('multiple names', async t => {
    const { cmd } = await mockArboristCmd(t, [], ['a', 'c'])

    t.same(cmd.workspaceNames, ['a', 'c'], 'should set array with single ws name')
  })

  await t.test('multiple paths', async t => {
    const { cmd } = await mockArboristCmd(t, [], ['./a', 'group/c'])

    t.same(cmd.workspaceNames, ['a', 'c'], 'should set array with single ws name')
  })

  await t.test('parent path', async t => {
    const { cmd } = await mockArboristCmd(t, [], './group')

    t.same(cmd.workspaceNames, ['c', 'd'], 'should set array with single ws name')
  })

  await t.test('parent path', async t => {
    const { cmd } = await mockArboristCmd(t, [], './group')

    t.same(cmd.workspaceNames, ['c', 'd'], 'should set array with single ws name')
  })

  await t.test('prefix inside cwd', async t => {
    const { npm, cmd, prefix } = await mockArboristCmd(t, null, ['a', 'c'], {
      chdir: (dirs) => dirs.testdir,
    })

    npm.localPrefix = prefix
    await cmd.execWorkspaces([])

    t.same(cmd.workspaceNames, ['a', 'c'], 'should set array with single ws name')
  })
})

t.test('handle getWorkspaces raising an error', async t => {
  const { cmd } = await mockArboristCmd(t, null, 'a', {
    mocks: {
      '{LIB}/workspaces/get-workspaces.js': async () => {
        throw new Error('oopsie')
      },
    },
  })

  await t.rejects(
    cmd.execWorkspaces(['foo']),
    { message: 'oopsie' }
  )
})

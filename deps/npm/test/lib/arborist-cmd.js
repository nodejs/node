const { resolve } = require('path')
const t = require('tap')
const ArboristCmd = require('../../lib/arborist-cmd.js')

t.test('arborist-cmd', async t => {
  const path = t.testdir({
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
  })

  class TestCmd extends ArboristCmd {}

  const cmd = new TestCmd()
  cmd.npm = { localPrefix: path }

  // check filtering for a single workspace name
  cmd.exec = async function (args) {
    t.same(this.workspaceNames, ['a'], 'should set array with single ws name')
    t.same(args, ['foo'], 'should get received args')
  }
  await cmd.execWorkspaces(['foo'], ['a'])

  // check filtering single workspace by path
  cmd.exec = async function (args) {
    t.same(this.workspaceNames, ['a'],
      'should set array with single ws name from path')
  }
  await cmd.execWorkspaces([], ['./a'])

  // check filtering single workspace by full path
  cmd.exec = function (args) {
    t.same(this.workspaceNames, ['a'],
      'should set array with single ws name from full path')
  }
  await cmd.execWorkspaces([], [resolve(path, './a')])

  // filtering multiple workspaces by name
  cmd.exec = async function (args) {
    t.same(this.workspaceNames, ['a', 'c'],
      'should set array with multiple listed ws names')
  }
  await cmd.execWorkspaces([], ['a', 'c'])

  // filtering multiple workspaces by path names
  cmd.exec = async function (args) {
    t.same(this.workspaceNames, ['a', 'c'],
      'should set array with multiple ws names from paths')
  }
  await cmd.execWorkspaces([], ['./a', 'group/c'])

  // filtering multiple workspaces by parent path name
  cmd.exec = async function (args) {
    t.same(this.workspaceNames, ['c', 'd'],
      'should set array with multiple ws names from a parent folder name')
  }
  await cmd.execWorkspaces([], ['./group'])
})

t.test('handle getWorkspaces raising an error', async t => {
  const ArboristCmd = t.mock('../../lib/arborist-cmd.js', {
    '../../lib/workspaces/get-workspaces.js': async () => {
      throw new Error('oopsie')
    },
  })
  class TestCmd extends ArboristCmd {}
  const cmd = new TestCmd()
  cmd.npm = {}

  await t.rejects(
    cmd.execWorkspaces(['foo'], ['a']),
    { message: 'oopsie' }
  )
})

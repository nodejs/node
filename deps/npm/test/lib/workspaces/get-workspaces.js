const { resolve } = require('path')
const t = require('tap')
const getWorkspaces = require('../../../lib/workspaces/get-workspaces.js')

const normalizePath = p => p
  .replace(/\\+/g, '/')
  .replace(/\r\n/g, '\n')

const cleanOutput = (str, path) => normalizePath(str)
  .replace(normalizePath(path), '{PATH}')

const clean = (res, path) => {
  const cleaned = new Map()
  for (const [key, value] of res.entries())
    cleaned.set(key, cleanOutput(value, path))
  return cleaned
}

t.test('get-workspaces', async t => {
  const path = t.testdir({
    packages: {
      a: {
        'package.json': JSON.stringify({
          name: 'a',
          version: '1.0.0',
          scripts: { glorp: 'echo a doing the glerp glop' },
        }),
      },
      b: {
        'package.json': JSON.stringify({
          name: 'b',
          version: '2.0.0',
          scripts: { glorp: 'echo b doing the glerp glop' },
        }),
      },
      c: {
        'package.json': JSON.stringify({
          name: 'c',
          version: '1.0.0',
          scripts: {
            test: 'exit 0',
            posttest: 'echo posttest',
            lorem: 'echo c lorem',
          },
        }),
      },
      d: {
        'package.json': JSON.stringify({
          name: 'd',
          version: '1.0.0',
          scripts: {
            test: 'exit 0',
            posttest: 'echo posttest',
          },
        }),
      },
      e: {
        'package.json': JSON.stringify({
          name: 'e',
          scripts: { test: 'exit 0', start: 'echo start something' },
        }),
      },
      noscripts: {
        'package.json': JSON.stringify({
          name: 'noscripts',
          version: '1.0.0',
        }),
      },
    },
    'package.json': JSON.stringify({
      name: 'x',
      version: '1.2.3',
      workspaces: ['packages/*'],
    }),
  })

  let workspaces

  workspaces = await getWorkspaces(['a', 'b'], { path })
  t.same(
    clean(workspaces, path),
    new Map(Object.entries({
      a: '{PATH}/packages/a',
      b: '{PATH}/packages/b',
    })),
    'should filter by package name'
  )

  workspaces = await getWorkspaces(['./packages/c'], { path })
  t.same(
    clean(workspaces, path),
    new Map(Object.entries({
      c: '{PATH}/packages/c',
    })),
    'should filter by package directory'
  )

  workspaces = await getWorkspaces(['packages/c'], { path })
  t.same(
    clean(workspaces, path),
    new Map(Object.entries({
      c: '{PATH}/packages/c',
    })),
    'should filter by rel package directory'
  )

  workspaces = await getWorkspaces([resolve(path, 'packages/c')], { path })
  t.same(
    clean(workspaces, path),
    new Map(Object.entries({
      c: '{PATH}/packages/c',
    })),
    'should filter by absolute package directory'
  )

  workspaces = await getWorkspaces(['packages'], { path })
  t.same(
    clean(workspaces, path),
    new Map(Object.entries({
      a: '{PATH}/packages/a',
      b: '{PATH}/packages/b',
      c: '{PATH}/packages/c',
      d: '{PATH}/packages/d',
      e: '{PATH}/packages/e',
      noscripts: '{PATH}/packages/noscripts',
    })),
    'should filter by parent directory name'
  )

  workspaces = await getWorkspaces(['./packages/'], { path })
  t.same(
    clean(workspaces, path),
    new Map(Object.entries({
      a: '{PATH}/packages/a',
      b: '{PATH}/packages/b',
      c: '{PATH}/packages/c',
      d: '{PATH}/packages/d',
      e: '{PATH}/packages/e',
      noscripts: '{PATH}/packages/noscripts',
    })),
    'should filter by parent directory path'
  )

  workspaces = await getWorkspaces([resolve(path, './packages')], { path })
  t.same(
    clean(workspaces, path),
    new Map(Object.entries({
      a: '{PATH}/packages/a',
      b: '{PATH}/packages/b',
      c: '{PATH}/packages/c',
      d: '{PATH}/packages/d',
      e: '{PATH}/packages/e',
      noscripts: '{PATH}/packages/noscripts',
    })),
    'should filter by absolute parent directory path'
  )

  workspaces = await getWorkspaces([], { path })
  t.same(
    clean(workspaces, path),
    new Map(Object.entries({
      a: '{PATH}/packages/a',
      b: '{PATH}/packages/b',
      c: '{PATH}/packages/c',
      d: '{PATH}/packages/d',
      e: '{PATH}/packages/e',
      noscripts: '{PATH}/packages/noscripts',
    })),
    'should return all workspaces if no filter set'
  )

  try {
    await getWorkspaces(['missing'], { path })
    throw new Error('missed throw')
  } catch (err) {
    t.match(
      err,
      /No workspaces found/,
      'should throw no workspaces found error'
    )
  }

  const unconfiguredWorkspaces = t.testdir({
    'package.json': JSON.stringify({
      name: 'no-configured-workspaces',
      version: '1.0.0',
    }),
  })
  try {
    await getWorkspaces([], { path: unconfiguredWorkspaces })
    throw new Error('missed throw')
  } catch (err) {
    t.match(
      err,
      /No workspaces found/,
      'should throw no workspaces found error'
    )
  }
})

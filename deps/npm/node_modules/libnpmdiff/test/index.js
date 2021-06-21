const { resolve } = require('path')

const t = require('tap')

const diff = require('../index.js')

const normalizePath = p => p
  .replace(/\\+/g, '/')
  .replace(/\r\n/g, '\n')

t.cleanSnapshot = (str) => normalizePath(str)
  .replace(normalizePath(process.execPath), 'node')

const json = (obj) => `${JSON.stringify(obj, null, 2)}\n`

t.test('compare two diff specs', async t => {
  const path = t.testdir({
    a1: {
      'package.json': json({
        name: 'a',
        version: '1.0.0',
      }),
      'index.js': 'module.exports =\n  "a1"\n',
    },
    a2: {
      'package.json': json({
        name: 'a',
        version: '2.0.0',
      }),
      'index.js': 'module.exports =\n  "a2"\n',
    },
  })

  const a = `file:${resolve(path, 'a1')}`
  const b = `file:${resolve(path, 'a2')}`

  t.resolveMatchSnapshot(diff([a, b], {}), 'should output expected diff')
})

t.test('using single arg', async t => {
  await t.rejects(
    diff(['abbrev@1.0.3']),
    /libnpmdiff needs two arguments to compare/,
    'should throw EDIFFARGS error'
  )
})

t.test('too many args', async t => {
  const args = ['abbrev@1.0.3', 'abbrev@1.0.4', 'abbrev@1.0.5']
  await t.rejects(
    diff(args),
    /libnpmdiff needs two arguments to compare/,
    'should output diff against cwd files'
  )
})

t.test('folder in node_modules', async t => {
  const path = t.testdir({
    node_modules: {
      a: {
        'package.json': json({
          name: 'a',
          version: '1.0.0',
          scripts: {
            prepare: `${process.execPath} prepare.js`,
          },
        }),
        'prepare.js': 'throw new Error("ERR")',
        node_modules: {
          b: {
            'package.json': json({
              name: 'b',
              version: '2.0.0',
              scripts: {
                prepare: `${process.execPath} prepare.js`,
              },
            }),
            'prepare.js': 'throw new Error("ERR")',
          },
        },
      },
    },
    packages: {
      a: {
        'package.json': json({
          name: 'a',
          version: '1.0.1',
          scripts: {
            prepare: `${process.execPath} prepare.js`,
          },
        }),
        'prepare.js': '',
      },
      b: {
        'package.json': json({
          name: 'b',
          version: '2.0.1',
          scripts: {
            prepare: `${process.execPath} prepare.js`,
          },
        }),
        'prepare.js': '',
      },
    },
    'package.json': json({
      name: 'my-project',
      version: '1.0.0',
    }),
  })

  t.test('top-level, absolute path', async t => {
    t.resolveMatchSnapshot(diff([
      `file:${resolve(path, 'node_modules/a')}`,
      `file:${resolve(path, 'packages/a')}`,
    ], { where: path }), 'should output expected diff')
  })
  t.test('top-level, relative path', async t => {
    const _cwd = process.cwd()
    process.chdir(path)
    t.teardown(() => {
      process.chdir(_cwd)
    })

    t.resolveMatchSnapshot(diff([
      'file:./node_modules/a',
      'file:./packages/a',
    ], { where: path }), 'should output expected diff')
  })
  t.test('nested, absolute path', async t => {
    t.resolveMatchSnapshot(diff([
      `file:${resolve(path, 'node_modules/a/node_modules/b')}`,
      `file:${resolve(path, 'packages/b')}`,
    ], { where: path}), 'should output expected diff')
  })
  t.test('nested, relative path', async t => {
    const _cwd = process.cwd()
    process.chdir(path)
    t.teardown(() => {
      process.chdir(_cwd)
    })

    t.resolveMatchSnapshot(diff([
      'file:./node_modules/a/node_modules/b',
      'file:./packages/b',
    ], { where: path }), 'should output expected diff')
  })
})

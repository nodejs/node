const { resolve } = require('path')

const t = require('tap')
const tar = require('tar')
const pacote = require('pacote')
pacote.tarball = () => {
  throw new Error('Failed to detect node_modules tarball')
}

const tarball = require('../lib/tarball.js')

const json = (obj) => `${JSON.stringify(obj, null, 2)}\n`

t.test('returns a tarball from node_modules', t => {
  t.plan(2)

  const path = t.testdir({
    node_modules: {
      a: {
        'package.json': json({
          name: 'a',
          version: '1.0.0',
          bin: { a: 'index.js' },
        }),
        'index.js': '',
      },
    },
  })

  const _cwd = process.cwd()
  process.chdir(path)
  t.teardown(() => {
    process.chdir(_cwd)
  })

  tarball({ bin: { a: 'index.js' }, _resolved: resolve(path, 'node_modules/a') }, { where: path })
    .then(res => {
      tar.list({
        filter: path => {
          t.match(
            path,
            /package.json|index.js/,
            'should return tarball with expected files'
          )
        },
      })
        .on('error', e => {
          throw e
        })
        .end(res)
    })
})

t.test('node_modules folder within a linked dir', async t => {
  const path = t.testdir({
    node_modules: {
      a: t.fixture('symlink', '../packages/a'),
    },
    packages: {
      a: {
        node_modules: {
          b: {
            'package.json': json({
              name: 'a',
              version: '1.0.0',
            }),
          },
        },
      },
    },
  })

  const link = await tarball({ _resolved: resolve(path, 'node_modules/a/node_modules/b') }, {})
  t.ok(link, 'should retrieve tarball from reading link')

  const target = await tarball({ _resolved: resolve(path, 'packages/a/node_modules/b') }, {})
  t.ok(target, 'should retrieve tarball from reading target')
})

t.test('pkg not in a node_modules folder', async t => {
  const path = t.testdir({
    packages: {
      a: {
        'package.json': json({
          name: 'a',
          version: '1.0.0',
        }),
      },
    },
  })

  t.throws(
    () => tarball({ _resolved: resolve(path, 'packages/a') }, {}),
    'should call regular pacote.tarball method instead'
  )
})

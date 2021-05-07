const t = require('tap')
const mockNpm = require('../../fixtures/mock-npm')

const config = {
  json: false,
  global: false,
}
const npm = mockNpm({ config })

const readLocalPackageName = require('../../../lib/utils/read-local-package.js')

t.test('read local package.json', async (t) => {
  npm.prefix = t.testdir({
    'package.json': JSON.stringify({
      name: 'my-local-package',
      version: '1.0.0',
    }),
  })
  const packageName = await readLocalPackageName(npm)
  t.equal(
    packageName,
    'my-local-package',
    'should retrieve current package name'
  )
})

t.test('read local scoped-package.json', async (t) => {
  npm.prefix = t.testdir({
    'package.json': JSON.stringify({
      name: '@my-scope/my-local-package',
      version: '1.0.0',
    }),
  })
  const packageName = await readLocalPackageName(npm)
  t.equal(
    packageName,
    '@my-scope/my-local-package',
    'should retrieve scoped package name'
  )
})

t.test('read using --global', async (t) => {
  npm.prefix = t.testdir({})
  config.global = true
  const packageName = await readLocalPackageName(npm)
  t.equal(
    packageName,
    undefined,
    'should not retrieve a package name'
  )
  config.global = false
})

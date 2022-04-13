const t = require('tap')

const readPackageName = require('../../../lib/utils/read-package-name.js')

t.test('read local package.json', async (t) => {
  const prefix = t.testdir({
    'package.json': JSON.stringify({
      name: 'my-local-package',
      version: '1.0.0',
    }),
  })
  const packageName = await readPackageName(prefix)
  t.equal(
    packageName,
    'my-local-package',
    'should retrieve current package name'
  )
})

t.test('read local scoped-package.json', async (t) => {
  const prefix = t.testdir({
    'package.json': JSON.stringify({
      name: '@my-scope/my-local-package',
      version: '1.0.0',
    }),
  })
  const packageName = await readPackageName(prefix)
  t.equal(
    packageName,
    '@my-scope/my-local-package',
    'should retrieve scoped package name'
  )
})

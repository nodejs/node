const requireInject = require('require-inject')
const { test } = require('tap')

let prefix
const _flatOptions = {
  json: false,
  global: false,
  get prefix () {
    return prefix
  },
}

const readLocalPackageName = requireInject('../../../lib/utils/read-local-package.js')
const npm = {
  flatOptions: _flatOptions,
}

test('read local package.json', async (t) => {
  prefix = t.testdir({
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

test('read local scoped-package.json', async (t) => {
  prefix = t.testdir({
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

test('read using --global', async (t) => {
  prefix = t.testdir({})
  _flatOptions.global = true
  const packageName = await readLocalPackageName(npm)
  t.equal(
    packageName,
    undefined,
    'should not retrieve a package name'
  )
  _flatOptions.global = false
})

const t = require('tap')
const { cleanNewlines } = require('../../fixtures/clean-snapshot')
const tmock = require('../../fixtures/tmock')

const npm = {
  config: {
    data: {
      get: () => builtinConfMock,
    },
  },
}

const builtinConfMock = {
  loadError: new Error('no builtin config'),
  raw: { hasBuiltinConfig: true, x: 'y', nested: { foo: 'bar' } },
}

const reifyOutput = () => {}

let expectWrite = false
const realFs = require('fs')
const fs = {
  ...realFs,
  promises: realFs.promises && {
    ...realFs.promises,
    writeFile: async (path, data) => {
      if (!expectWrite) {
        throw new Error('did not expect to write builtin config file')
      }
      return realFs.promises.writeFile(path, data)
    },
  },
}

const reifyFinish = tmock(t, '{LIB}/utils/reify-finish.js', {
  fs,
  '{LIB}/utils/reify-output.js': reifyOutput,
})

t.test('should not write if not global', async t => {
  expectWrite = false
  await reifyFinish(npm, {
    options: { global: false },
    actualTree: {},
  })
})

t.test('should not write if no global npm module', async t => {
  expectWrite = false
  await reifyFinish(npm, {
    options: { global: true },
    actualTree: {
      inventory: new Map(),
    },
  })
})

t.test('should not write if builtin conf had load error', async t => {
  expectWrite = false
  await reifyFinish(npm, {
    options: { global: true },
    actualTree: {
      inventory: new Map([['node_modules/npm', {}]]),
    },
  })
})

t.test('should write if everything above passes', async t => {
  expectWrite = true
  delete builtinConfMock.loadError
  const path = t.testdir()
  await reifyFinish(npm, {
    options: { global: true },
    actualTree: {
      inventory: new Map([['node_modules/npm', { path }]]),
    },
  })
  // windowwwwwwssss!!!!!
  const data = cleanNewlines(fs.readFileSync(`${path}/npmrc`, 'utf8'))
  t.matchSnapshot(data, 'written config')
})

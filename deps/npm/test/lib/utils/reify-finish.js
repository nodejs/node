const t = require('tap')

const npm = {
  config: {
    data: {
      get: () => builtinConfMock,
    },
  },
}

const builtinConfMock = {
  loadError: new Error('no builtin config'),
  raw: { hasBuiltinConfig: true, x: 'y', nested: { foo: 'bar' }},
}

const reifyOutput = () => {}

let expectWrite = false
const realFs = require('fs')
const fs = {
  ...realFs,
  promises: realFs.promises && {
    ...realFs.promises,
    writeFile: async (path, data) => {
      if (!expectWrite)
        throw new Error('did not expect to write builtin config file')
      return realFs.promises.writeFile(path, data)
    },
  },
}

const reifyFinish = t.mock('../../../lib/utils/reify-finish.js', {
  fs,
  '../../../lib/utils/reify-output.js': reifyOutput,
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
      inventory: new Map([['node_modules/npm', {path}]]),
    },
  })
  // windowwwwwwssss!!!!!
  const data = fs.readFileSync(`${path}/npmrc`, 'utf8').replace(/\r\n/g, '\n')
  t.matchSnapshot(data, 'written config')
})

t.test('works without fs.promises', async t => {
  t.doesNotThrow(() => t.mock('../../../lib/utils/reify-finish.js', {
    fs: { ...fs, promises: null },
    '../../../lib/npm.js': npm,
    '../../../lib/utils/reify-output.js': reifyOutput,
  }))
})

const t = require('tap')
const npm = require('../../../lib/npm.js')

const dym = require('../../../lib/utils/did-you-mean.js')
t.test('did-you-mean', t => {
  npm.load(err => {
    t.notOk(err)
    t.test('nistall', async t => {
      const result = await dym(npm, npm.localPrefix, 'nistall')
      t.match(result, 'npm install')
    })
    t.test('sttest', async t => {
      const result = await dym(npm, npm.localPrefix, 'sttest')
      t.match(result, 'npm test')
      t.match(result, 'npm run posttest')
    })
    t.test('npz', async t => {
      const result = await dym(npm, npm.localPrefix, 'npxx')
      t.match(result, 'npm exec npx')
    })
    t.test('qwuijbo', async t => {
      const result = await dym(npm, npm.localPrefix, 'qwuijbo')
      t.match(result, '')
    })
    t.end()
  })
})

t.test('missing bin and script properties', async t => {
  const path = t.testdir({
    'package.json': JSON.stringify({
      name: 'missing-bin',
    }),
  })

  const result = await dym(npm, path, 'nistall')
  t.match(result, 'npm install')
})

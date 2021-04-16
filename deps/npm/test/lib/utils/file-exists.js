const t = require('tap')
const fileExists = require('../../../lib/utils/file-exists.js')

t.test('returns true when arg is a file', async (t) => {
  const path = t.testdir({
    foo: 'just some file',
  })

  const result = await fileExists(`${path}/foo`)
  t.equal(result, true, 'file exists')
  t.end()
})

t.test('returns false when arg is not a file', async (t) => {
  const path = t.testdir({
    foo: {},
  })

  const result = await fileExists(`${path}/foo`)
  t.equal(result, false, 'file does not exist')
  t.end()
})

t.test('returns false when arg does not exist', async (t) => {
  const path = t.testdir()

  const result = await fileExists(`${path}/foo`)
  t.equal(result, false, 'file does not exist')
  t.end()
})

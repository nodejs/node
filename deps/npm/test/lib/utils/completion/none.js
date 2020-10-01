const t = require('tap')
const none = require('../../../../lib/utils/completion/none.js')
none({any:'thing'}, (er, res) => {
  t.equal(er, null)
  t.strictSame(res, [])
})

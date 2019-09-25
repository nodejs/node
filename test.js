const stream = require('stream')
const assert = require('assert')

const src = new stream.Readable()
src.push('a')
src.push(null)

let data = ''
const dst = new stream.Writable({
  write (chunk, encoding, cb) {
    setTimeout(() => {
      data += chunk
      cb()
    }, 1000)
  }
})

stream.pipeline(
  src,
  dst,
  err => {
    if (!err) {
      assert.strictEqual(data, 'a') // fails
    }
  }
)

process.nextTick(() => {
  dst.destroy()
})

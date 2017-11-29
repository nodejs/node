const MiniPass = require('./')
const butterfly = '🦋'
var mp = new MiniPass({ encoding: 'utf8' })
mp.on('data', chunk => {
  console.error('data %s', chunk)
})
var butterbuf = new Buffer([0xf0, 0x9f, 0xa6, 0x8b])
mp.write(butterbuf.slice(0, 1))
mp.write(butterbuf.slice(1, 2))
mp.write(butterbuf.slice(2, 3))
mp.write(butterbuf.slice(3, 4))
mp.end()

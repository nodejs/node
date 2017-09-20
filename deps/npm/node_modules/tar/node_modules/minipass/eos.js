const EE = require('events').EventEmitter
const eos = require('end-of-stream')
const ee = new EE()
ee.readable = ee.writable = true
eos(ee, er => {
  if (er)
    throw er
  console.log('stream ended')
})
ee.emit('finish')
ee.emit('close')
ee.emit('end')

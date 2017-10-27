'use strict'
const MiniPass = require('../..')

module.exports = class ExtendMiniPass extends MiniPass {
  constructor (opts) {
    super(opts)
  }
  write (data, encoding) {
    return super.write(data, encoding)
  }
}

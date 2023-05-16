const fs = require('./fs.js')
const getOptions = require('./common/get-options.js')
const withOwner = require('./with-owner.js')

const copyFile = async (src, dest, opts) => {
  const options = getOptions(opts, {
    copy: ['mode'],
    wrap: 'mode',
  })

  // the node core method as of 16.5.0 does not support the mode being in an
  // object, so we have to pass the mode value directly
  return withOwner(dest, () => fs.copyFile(src, dest, options.mode), opts)
}

module.exports = copyFile

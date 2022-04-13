const { dirname, sep } = require('path')

const fs = require('./fs.js')
const getOptions = require('./common/get-options.js')
const withOwner = require('./with-owner.js')

const mkdtemp = async (prefix, opts) => {
  const options = getOptions(opts, {
    copy: ['encoding'],
    wrap: 'encoding',
  })

  // mkdtemp relies on the trailing path separator to indicate if it should
  // create a directory inside of the prefix. if that's the case then the root
  // we infer ownership from is the prefix itself, otherwise it's the dirname
  // /tmp -> /tmpABCDEF, infers from /
  // /tmp/ -> /tmp/ABCDEF, infers from /tmp
  const root = prefix.endsWith(sep) ? prefix : dirname(prefix)

  return withOwner(root, () => fs.mkdtemp(prefix, options), opts)
}

module.exports = mkdtemp

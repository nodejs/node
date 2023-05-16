const getOptions = require('./common/get-options.js')
const owner = require('./common/owner-sync.js')

const withOwnerSync = (path, fn, opts) => {
  const options = getOptions(opts, {
    copy: ['owner'],
  })

  const { uid, gid } = owner.validate(path, options.owner)

  const result = fn({ uid, gid })

  owner.update(path, uid, gid)
  if (typeof result === 'string') {
    owner.update(result, uid, gid)
  }

  return result
}

module.exports = withOwnerSync

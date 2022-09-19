const getOptions = require('./common/get-options.js')
const owner = require('./common/owner.js')

const withOwner = async (path, fn, opts) => {
  const options = getOptions(opts, {
    copy: ['owner'],
  })

  const { uid, gid } = await owner.validate(path, options.owner)

  const result = await fn({ uid, gid })

  await Promise.all([
    owner.update(path, uid, gid),
    typeof result === 'string' ? owner.update(result, uid, gid) : null,
  ])

  return result
}

module.exports = withOwner

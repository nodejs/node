const log = require('npmlog')
const fetch = require('npm-registry-fetch')

const npm = require('./npm.js')
const output = require('./utils/output.js')
const getIdentity = require('./utils/get-identity.js')
const usageUtil = require('./utils/usage.js')
const completion = require('./utils/completion/none.js')

const usage = usageUtil('stars', 'npm stars [<user>]')

const cmd = (args, cb) => stars(args).then(() => cb()).catch(cb)

const stars = (args) => {
  return stars_(args).catch(er => {
    if (er.code === 'ENEEDAUTH')
      log.warn('stars', 'auth is required to look up your username')

    throw er
  })
}

const stars_ = async ([user = getIdentity(npm.flatOptions)]) => {
  const { rows } = await fetch.json('/-/_view/starredByUser', {
    ...npm.flatOptions,
    query: { key: `"${await user}"` },
  })

  if (rows.length === 0)
    log.warn('stars', 'user has not starred any packages')

  for (const row of rows)
    output(row.value)
}

module.exports = Object.assign(cmd, { usage, completion })

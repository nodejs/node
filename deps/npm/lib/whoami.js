const npm = require('./npm.js')
const output = require('./utils/output.js')
const getIdentity = require('./utils/get-identity.js')
const usageUtil = require('./utils/usage.js')

const cmd = (args, cb) => whoami(args).then(() => cb()).catch(cb)

const usage = usageUtil('whoami', 'npm whoami [--registry <registry>]\n(just prints username according to given registry)')

const whoami = async ([spec]) => {
  const opts = npm.flatOptions
  const username = await getIdentity(opts, spec)
  output(opts.json ? JSON.stringify(username) : username)
}

module.exports = Object.assign(cmd, { usage })

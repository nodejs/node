const fetch = require('npm-registry-fetch')
const log = require('npmlog')
const npa = require('npm-package-arg')

const npm = require('./npm.js')
const output = require('./utils/output.js')
const usageUtil = require('./utils/usage.js')
const getIdentity = require('./utils/get-identity')
const completion = require('./utils/completion/none.js')

const usage = usageUtil(
  'star',
  'npm star [<pkg>...]\n' +
  'npm unstar [<pkg>...]'
)

const cmd = (args, cb) => star(args).then(() => cb()).catch(cb)

const star = async args => {
  if (!args.length)
    throw new Error(usage)

  // if we're unstarring, then show an empty star image
  // otherwise, show the full star image
  const { unicode } = npm.flatOptions
  const unstar = npm.config.get('star.unstar')
  const full = unicode ? '\u2605 ' : '(*)'
  const empty = unicode ? '\u2606 ' : '( )'
  const show = unstar ? empty : full

  const pkgs = args.map(npa)
  for (const pkg of pkgs) {
    const [username, fullData] = await Promise.all([
      getIdentity(npm.flatOptions),
      fetch.json(pkg.escapedName, {
        ...npm.flatOptions,
        spec: pkg,
        query: { write: true },
        preferOnline: true,
      }),
    ])

    if (!username)
      throw new Error('You need to be logged in!')

    const body = {
      _id: fullData._id,
      _rev: fullData._rev,
      users: fullData.users || {},
    }

    if (!unstar) {
      log.info('star', 'starring', body._id)
      body.users[username] = true
      log.verbose('star', 'starring', body)
    } else {
      delete body.users[username]
      log.info('unstar', 'unstarring', body._id)
      log.verbose('unstar', 'unstarring', body)
    }

    const data = await fetch.json(pkg.escapedName, {
      ...npm.flatOptions,
      spec: pkg,
      method: 'PUT',
      body,
    })

    output(show + ' ' + pkg.name)
    log.verbose('star', data)
    return data
  }
}

module.exports = Object.assign(cmd, { completion, usage })

'use strict'

const fetch = require('npm-registry-fetch')
const log = require('npmlog')
const npa = require('npm-package-arg')
const npm = require('./npm.js')
const output = require('./utils/output.js')
const usage = require('./utils/usage.js')
const getItentity = require('./utils/get-identity')

star.usage = usage(
  'star',
  'npm star [<pkg>...]\n' +
  'npm unstar [<pkg>...]'
)

star.completion = function (opts, cb) {
  // FIXME: there used to be registry completion here, but it stopped making
  // sense somewhere around 50,000 packages on the registry
  cb()
}

module.exports = star
function star (args, cb) {
  const opts = npm.flatOptions
  return Promise.resolve().then(() => {
    if (!args.length) throw new Error(star.usage)
    // if we're unstarring, then show an empty star image
    // otherwise, show the full star image
    const unstar = /^un/.test(npm.command)
    const full = opts.unicode ? '\u2605 ' : '(*)'
    const empty = opts.unicode ? '\u2606 ' : '( )'
    const show = unstar ? empty : full
    return Promise.all(args.map(npa).map(pkg => {
      return Promise.all([
        getItentity(opts),
        fetch.json(pkg.escapedName, {
          ...opts,
          spec: pkg,
          query: { write: true },
          preferOnline: true
        })
      ]).then(([username, fullData]) => {
        if (!username) { throw new Error('You need to be logged in!') }
        const body = {
          _id: fullData._id,
          _rev: fullData._rev,
          users: fullData.users || {}
        }

        if (!unstar) {
          log.info('star', 'starring', body._id)
          body.users[username] = true
          log.verbose('star', 'starring', body)
        } else {
          delete body.users[username]
          log.info('star', 'unstarring', body._id)
          log.verbose('star', 'unstarring', body)
        }
        return fetch.json(pkg.escapedName, {
          ...opts,
          spec: pkg,
          method: 'PUT',
          body
        })
      }).then(data => {
        output(show + ' ' + pkg.name)
        log.verbose('star', data)
        return data
      })
    }))
  }).then(() => cb(), cb)
}

'use strict'

const BB = require('bluebird')

const npmConfig = require('./config/figgy-config.js')
const fetch = require('libnpm/fetch')
const log = require('npmlog')
const output = require('./utils/output.js')
const whoami = require('./whoami.js')

stars.usage = 'npm stars [<user>]'

module.exports = stars
function stars ([user], cb) {
  const opts = npmConfig()
  return BB.try(() => {
    return (user ? BB.resolve(user) : whoami([], true, () => {})).then(usr => {
      return fetch.json('/-/_view/starredByUser', opts.concat({
        query: {key: `"${usr}"`} // WHY. WHY THE ""?!
      }))
    }).then(data => data.rows).then(stars => {
      if (stars.length === 0) {
        log.warn('stars', 'user has not starred any packages.')
      } else {
        stars.forEach(s => output(s.value))
      }
    })
  }).catch(err => {
    if (err.code === 'ENEEDAUTH') {
      throw Object.assign(new Error("'npm starts' on your own user account requires auth"), {
        code: 'ENEEDAUTH'
      })
    } else {
      throw err
    }
  }).nodeify(cb)
}

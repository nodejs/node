module.exports = owner

const BB = require('bluebird')

const log = require('npmlog')
const npa = require('libnpm/parse-arg')
const npmConfig = require('./config/figgy-config.js')
const npmFetch = require('libnpm/fetch')
const output = require('./utils/output.js')
const otplease = require('./utils/otplease.js')
const packument = require('libnpm/packument')
const readLocalPkg = BB.promisify(require('./utils/read-local-package.js'))
const usage = require('./utils/usage')
const whoami = BB.promisify(require('./whoami.js'))

owner.usage = usage(
  'owner',
  'npm owner add <user> [<@scope>/]<pkg>' +
  '\nnpm owner rm <user> [<@scope>/]<pkg>' +
  '\nnpm owner ls [<@scope>/]<pkg>'
)

owner.completion = function (opts, cb) {
  const argv = opts.conf.argv.remain
  if (argv.length > 4) return cb()
  if (argv.length <= 2) {
    var subs = ['add', 'rm']
    if (opts.partialWord === 'l') subs.push('ls')
    else subs.push('ls', 'list')
    return cb(null, subs)
  }
  BB.try(() => {
    const opts = npmConfig()
    return whoami([], true).then(username => {
      const un = encodeURIComponent(username)
      let byUser, theUser
      switch (argv[2]) {
        case 'ls':
          // FIXME: there used to be registry completion here, but it stopped
          // making sense somewhere around 50,000 packages on the registry
          return
        case 'rm':
          if (argv.length > 3) {
            theUser = encodeURIComponent(argv[3])
            byUser = `/-/by-user/${theUser}|${un}`
            return npmFetch.json(byUser, opts).then(d => {
              return d[theUser].filter(
                // kludge for server adminery.
                p => un === 'isaacs' || d[un].indexOf(p) === -1
              )
            })
          }
          // else fallthrough
          /* eslint no-fallthrough:0 */
        case 'add':
          if (argv.length > 3) {
            theUser = encodeURIComponent(argv[3])
            byUser = `/-/by-user/${theUser}|${un}`
            return npmFetch.json(byUser, opts).then(d => {
              var mine = d[un] || []
              var theirs = d[theUser] || []
              return mine.filter(p => theirs.indexOf(p) === -1)
            })
          } else {
            // just list all users who aren't me.
            return npmFetch.json('/-/users', opts).then(list => {
              return Object.keys(list).filter(n => n !== un)
            })
          }

        default:
          return cb()
      }
    })
  }).nodeify(cb)
}

function UsageError () {
  throw Object.assign(new Error(owner.usage), {code: 'EUSAGE'})
}

function owner ([action, ...args], cb) {
  const opts = npmConfig()
  BB.try(() => {
    switch (action) {
      case 'ls': case 'list': return ls(args[0], opts)
      case 'add': return add(args[0], args[1], opts)
      case 'rm': case 'remove': return rm(args[0], args[1], opts)
      default: UsageError()
    }
  }).then(
    data => cb(null, data),
    err => err.code === 'EUSAGE' ? cb(err.message) : cb(err)
  )
}

function ls (pkg, opts) {
  if (!pkg) {
    return readLocalPkg().then(pkg => {
      if (!pkg) { UsageError() }
      return ls(pkg, opts)
    })
  }

  const spec = npa(pkg)
  return packument(spec, opts.concat({fullMetadata: true})).then(
    data => {
      var owners = data.maintainers
      if (!owners || !owners.length) {
        output('admin party!')
      } else {
        output(owners.map(o => `${o.name} <${o.email}>`).join('\n'))
      }
      return owners
    },
    err => {
      log.error('owner ls', "Couldn't get owner data", pkg)
      throw err
    }
  )
}

function add (user, pkg, opts) {
  if (!user) { UsageError() }
  if (!pkg) {
    return readLocalPkg().then(pkg => {
      if (!pkg) { UsageError() }
      return add(user, pkg, opts)
    })
  }
  log.verbose('owner add', '%s to %s', user, pkg)

  const spec = npa(pkg)
  return withMutation(spec, user, opts, (u, owners) => {
    if (!owners) owners = []
    for (var i = 0, l = owners.length; i < l; i++) {
      var o = owners[i]
      if (o.name === u.name) {
        log.info(
          'owner add',
          'Already a package owner: ' + o.name + ' <' + o.email + '>'
        )
        return false
      }
    }
    owners.push(u)
    return owners
  })
}

function rm (user, pkg, opts) {
  if (!user) { UsageError() }
  if (!pkg) {
    return readLocalPkg().then(pkg => {
      if (!pkg) { UsageError() }
      return add(user, pkg, opts)
    })
  }
  log.verbose('owner rm', '%s from %s', user, pkg)

  const spec = npa(pkg)
  return withMutation(spec, user, opts, function (u, owners) {
    let found = false
    const m = owners.filter(function (o) {
      var match = (o.name === user)
      found = found || match
      return !match
    })

    if (!found) {
      log.info('owner rm', 'Not a package owner: ' + user)
      return false
    }

    if (!m.length) {
      throw new Error(
        'Cannot remove all owners of a package.  Add someone else first.'
      )
    }

    return m
  })
}

function withMutation (spec, user, opts, mutation) {
  return BB.try(() => {
    if (user) {
      const uri = `/-/user/org.couchdb.user:${encodeURIComponent(user)}`
      return npmFetch.json(uri, opts).then(mutate_, err => {
        log.error('owner mutate', 'Error getting user data for %s', user)
        throw err
      })
    } else {
      return mutate_(null)
    }
  })

  function mutate_ (u) {
    if (user && (!u || u.error)) {
      throw new Error(
        "Couldn't get user data for " + user + ': ' + JSON.stringify(u)
      )
    }

    if (u) u = { name: u.name, email: u.email }
    return packument(spec, opts.concat({
      fullMetadata: true
    })).then(data => {
      // save the number of maintainers before mutation so that we can figure
      // out if maintainers were added or removed
      const beforeMutation = data.maintainers.length

      const m = mutation(u, data.maintainers)
      if (!m) return // handled
      if (m instanceof Error) throw m // error

      data = {
        _id: data._id,
        _rev: data._rev,
        maintainers: m
      }
      const dataPath = `/${spec.escapedName}/-rev/${encodeURIComponent(data._rev)}`
      return otplease(opts, opts => {
        const reqOpts = opts.concat({
          method: 'PUT',
          body: data,
          spec
        })
        return npmFetch.json(dataPath, reqOpts)
      }).then(data => {
        if (data.error) {
          throw new Error('Failed to update package metadata: ' + JSON.stringify(data))
        } else if (m.length > beforeMutation) {
          output('+ %s (%s)', user, spec.name)
        } else if (m.length < beforeMutation) {
          output('- %s (%s)', user, spec.name)
        }
        return data
      })
    })
  }
}

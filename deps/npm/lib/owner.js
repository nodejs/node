module.exports = owner

var npm = require('./npm.js')
var log = require('npmlog')
var mapToRegistry = require('./utils/map-to-registry.js')
var readLocalPkg = require('./utils/read-local-package.js')
var usage = require('./utils/usage')
var output = require('./utils/output.js')

owner.usage = usage(
  'owner',
  'npm owner add <user> [<@scope>/]<pkg>' +
  '\nnpm owner rm <user> [<@scope>/]<pkg>' +
  '\nnpm owner ls [<@scope>/]<pkg>'
)
owner.completion = function (opts, cb) {
  var argv = opts.conf.argv.remain
  if (argv.length > 4) return cb()
  if (argv.length <= 2) {
    var subs = ['add', 'rm']
    if (opts.partialWord === 'l') subs.push('ls')
    else subs.push('ls', 'list')
    return cb(null, subs)
  }

  npm.commands.whoami([], true, function (er, username) {
    if (er) return cb()

    var un = encodeURIComponent(username)
    var byUser, theUser
    switch (argv[2]) {
      case 'ls':
        // FIXME: there used to be registry completion here, but it stopped
        // making sense somewhere around 50,000 packages on the registry
        return cb()

      case 'rm':
        if (argv.length > 3) {
          theUser = encodeURIComponent(argv[3])
          byUser = '-/by-user/' + theUser + '|' + un
          return mapToRegistry(byUser, npm.config, function (er, uri, auth) {
            if (er) return cb(er)

            console.error(uri)
            npm.registry.get(uri, { auth: auth }, function (er, d) {
              if (er) return cb(er)
              // return the intersection
              return cb(null, d[theUser].filter(function (p) {
                // kludge for server adminery.
                return un === 'isaacs' || d[un].indexOf(p) === -1
              }))
            })
          })
        }
        // else fallthrough
        /*eslint no-fallthrough:0*/
      case 'add':
        if (argv.length > 3) {
          theUser = encodeURIComponent(argv[3])
          byUser = '-/by-user/' + theUser + '|' + un
          return mapToRegistry(byUser, npm.config, function (er, uri, auth) {
            if (er) return cb(er)

            console.error(uri)
            npm.registry.get(uri, { auth: auth }, function (er, d) {
              console.error(uri, er || d)
              // return mine that they're not already on.
              if (er) return cb(er)
              var mine = d[un] || []
              var theirs = d[theUser] || []
              return cb(null, mine.filter(function (p) {
                return theirs.indexOf(p) === -1
              }))
            })
          })
        }
        // just list all users who aren't me.
        return mapToRegistry('-/users', npm.config, function (er, uri, auth) {
          if (er) return cb(er)

          npm.registry.get(uri, { auth: auth }, function (er, list) {
            if (er) return cb()
            return cb(null, Object.keys(list).filter(function (n) {
              return n !== un
            }))
          })
        })

      default:
        return cb()
    }
  })
}

function owner (args, cb) {
  var action = args.shift()
  switch (action) {
    case 'ls': case 'list': return ls(args[0], cb)
    case 'add': return add(args[0], args[1], cb)
    case 'rm': case 'remove': return rm(args[0], args[1], cb)
    default: return unknown(action, cb)
  }
}

function ls (pkg, cb) {
  if (!pkg) {
    return readLocalPkg(function (er, pkg) {
      if (er) return cb(er)
      if (!pkg) return cb(owner.usage)
      ls(pkg, cb)
    })
  }

  mapToRegistry(pkg, npm.config, function (er, uri, auth) {
    if (er) return cb(er)

    npm.registry.get(uri, { auth: auth }, function (er, data) {
      var msg = ''
      if (er) {
        log.error('owner ls', "Couldn't get owner data", pkg)
        return cb(er)
      }
      var owners = data.maintainers
      if (!owners || !owners.length) {
        msg = 'admin party!'
      } else {
        msg = owners.map(function (o) {
          return o.name + ' <' + o.email + '>'
        }).join('\n')
      }
      output(msg)
      cb(er, owners)
    })
  })
}

function add (user, pkg, cb) {
  if (!user) return cb(owner.usage)
  if (!pkg) {
    return readLocalPkg(function (er, pkg) {
      if (er) return cb(er)
      if (!pkg) return cb(new Error(owner.usage))
      add(user, pkg, cb)
    })
  }

  log.verbose('owner add', '%s to %s', user, pkg)
  mutate(pkg, user, function (u, owners) {
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
  }, cb)
}

function rm (user, pkg, cb) {
  if (!pkg) {
    return readLocalPkg(function (er, pkg) {
      if (er) return cb(er)
      if (!pkg) return cb(new Error(owner.usage))
      rm(user, pkg, cb)
    })
  }

  log.verbose('owner rm', '%s from %s', user, pkg)
  mutate(pkg, user, function (u, owners) {
    var found = false
    var m = owners.filter(function (o) {
      var match = (o.name === user)
      found = found || match
      return !match
    })

    if (!found) {
      log.info('owner rm', 'Not a package owner: ' + user)
      return false
    }

    if (!m.length) {
      return new Error(
        'Cannot remove all owners of a package.  Add someone else first.'
      )
    }

    return m
  }, cb)
}

function mutate (pkg, user, mutation, cb) {
  if (user) {
    var byUser = '-/user/org.couchdb.user:' + user
    mapToRegistry(byUser, npm.config, function (er, uri, auth) {
      if (er) return cb(er)

      npm.registry.get(uri, { auth: auth }, mutate_)
    })
  } else {
    mutate_(null, null)
  }

  function mutate_ (er, u) {
    if (!er && user && (!u || u.error)) {
      er = new Error(
        "Couldn't get user data for " + user + ': ' + JSON.stringify(u)
      )
    }

    if (er) {
      log.error('owner mutate', 'Error getting user data for %s', user)
      return cb(er)
    }

    if (u) u = { name: u.name, email: u.email }
    mapToRegistry(pkg, npm.config, function (er, uri, auth) {
      if (er) return cb(er)

      npm.registry.get(uri, { auth: auth }, function (er, data) {
        if (er) {
          log.error('owner mutate', 'Error getting package data for %s', pkg)
          return cb(er)
        }

        // save the number of maintainers before mutation so that we can figure
        // out if maintainers were added or removed
        var beforeMutation = data.maintainers.length

        var m = mutation(u, data.maintainers)
        if (!m) return cb() // handled
        if (m instanceof Error) return cb(m) // error

        data = {
          _id: data._id,
          _rev: data._rev,
          maintainers: m
        }
        var dataPath = pkg.replace('/', '%2f') + '/-rev/' + data._rev
        mapToRegistry(dataPath, npm.config, function (er, uri, auth) {
          if (er) return cb(er)

          var params = {
            method: 'PUT',
            body: data,
            auth: auth
          }
          npm.registry.request(uri, params, function (er, data) {
            if (!er && data.error) {
              er = new Error('Failed to update package metadata: ' + JSON.stringify(data))
            }

            if (er) {
              log.error('owner mutate', 'Failed to update package metadata')
            } else if (m.length > beforeMutation) {
              output('+ %s (%s)', user, pkg)
            } else if (m.length < beforeMutation) {
              output('- %s (%s)', user, pkg)
            }

            cb(er, data)
          })
        })
      })
    })
  }
}

function unknown (action, cb) {
  cb('Usage: \n' + owner.usage)
}

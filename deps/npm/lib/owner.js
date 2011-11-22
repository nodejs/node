
module.exports = owner

owner.usage = "npm owner add <username> <pkg>"
            + "\nnpm owner rm <username> <pkg>"
            + "\nnpm owner ls <pkg>"

owner.completion = function (opts, cb) {
  var argv = opts.conf.argv.remain
  if (argv.length > 4) return cb()
  if (argv.length <= 2) {
    var subs = ["add", "rm"]
    if (opts.partialWord === "l") subs.push("ls")
    else subs.push("ls", "list")
    return cb(null, subs)
  }
  var un = encodeURIComponent(npm.config.get("username"))
  switch (argv[2]) {
    case "ls":
      if (argv.length > 3) return cb()
      else return registry.get("/-/short", cb)

    case "rm":
      if (argv.length > 3) {
        var theUser = encodeURIComponent(argv[3])
          , uri = "/-/by-user/"+theUser+"|"+un
        console.error(uri)
        return registry.get(uri, function (er, d) {
          if (er) return cb(er)
          // return the intersection
          return cb(null, d[theUser].filter(function (p) {
            // kludge for server adminery.
            return un === "isaacs" || d[un].indexOf(p) === -1
          }))
        })
      }
      // else fallthrough
    case "add":
      if (argv.length > 3) {
        var theUser = encodeURIComponent(argv[3])
          , uri = "/-/by-user/"+theUser+"|"+un
        console.error(uri)
        return registry.get(uri, function (er, d) {
          console.error(uri, er || d)
          // return mine that they're not already on.
          if (er) return cb(er)
          var mine = d[un] || []
            , theirs = d[theUser] || []
          return cb(null, mine.filter(function (p) {
            return theirs.indexOf(p) === -1
          }))
        })
      }
      // just list all users who aren't me.
      return registry.get("/-/users", function (er, list) {
        if (er) return cb()
        return cb(null, Object.keys(list).filter(function (n) {
          return n !== un
        }))
      })

    default:
      return cb()
  }
}

var registry = require("./utils/npm-registry-client/index.js")
  , get = registry.request.GET
  , put = registry.request.PUT
  , log = require("./utils/log.js")
  , output
  , npm = require("./npm.js")

function owner (args, cb) {
  var action = args.shift()
  switch (action) {
    case "ls": case "list": return ls(args[0], cb)
    case "add": return add(args[0], args[1], cb)
    case "rm": case "remove": return rm(args[0], args[1], cb)
    default: return unknown(action, cb)
  }
}

function ls (pkg, cb) {
  if (!pkg) return cb(owner.usage)
  get(pkg, function (er, data) {
    var msg = ""
    if (er) return log.er(cb, "Couldn't get owner data for "+pkg)(er)
    var owners = data.maintainers
    if (!owners || !owners.length) msg = "admin party!"
    else msg = owners.map(function (o) { return o.name +" <"+o.email+">" }).join("\n")
    output = output || require("./utils/output.js")
    output.write(msg, function (er) { cb(er, owners) })
  })
}

function add (user, pkg, cb) {
  if (!pkg) readLocalPkg(function (er, pkg) {
    if (er) return cb(er)
    if (!pkg) return cb(new Error(owner.usage))
    add(user, pkg, cb)
  })

  log.verbose(user+" to "+pkg, "owner add")
  mutate(pkg, user, function (u, owners) {
    if (!owners) owners = []
    for (var i = 0, l = owners.length; i < l; i ++) {
      var o = owners[i]
      if (o.name === u.name) {
        log( "Already a package owner: "+o.name+" <"+o.email+">"
           , "owner add"
           )
        return false
      }
    }
    owners.push(u)
    return owners
  }, cb)
}

function rm (user, pkg, cb) {
  if (!pkg) readLocalPkg(function (er, pkg) {
    if (er) return cb(er)
    if (!pkg) return cb(new Error(owner.usage))
    rm(user, pkg, cb)
  })

  log.verbose(user+" from "+pkg, "owner rm")
  mutate(pkg, null, function (u, owners) {
    var found = false
      , m = owners.filter(function (o) {
          var match = (o.name === user)
          found = found || match
          return !match
        })
    if (!found) {
      log("Not a package owner: "+user, "owner rm")
      return false
    }
    if (!m.length) return new Error(
      "Cannot remove all owners of a package.  Add someone else first.")
    return m
  }, cb)
}

function mutate (pkg, user, mutation, cb) {
  if (user) {
    get("/-/user/org.couchdb.user:"+user, mutate_)
  } else {
    mutate_(null, null)
  }

  function mutate_ (er, u) {
    if (er) return log.er(cb, "Error getting user data for "+user)(er)
    if (user && (!u || u.error)) return cb(new Error(
      "Couldn't get user data for "+user+": "+JSON.stringify(u)))
    if (u) u = { "name" : u.name, "email" : u.email }
    get("/"+pkg, function (er, data) {
      if (er) return log.er(cb, "Couldn't get package data for "+pkg)(er)
      var m = mutation(u, data.maintainers)
      if (!m) return cb() // handled
      if (m instanceof Error) return cb(m) // error
      data = { _id : data._id
             , _rev : data._rev
             , maintainers : m
             }
      put("/"+pkg+"/-rev/"+data._rev, data, function (er, data) {
        if (er) return log.er(cb, "Failed to update package metadata")(er)
        if (data.error) return cb(new Error(
          "Failed to update pacakge metadata: "+JSON.stringify(data)))
        cb(null, data)
      })
    })
  }
}

function readLocalPkg (cb) {
  if (npm.config.get("global")) return cb()
  var path = require("path")
    , readJson = require("./utils/read-json.js")
  readJson(path.resolve(npm.prefix, "package.json"), function (er, d) {
    return cb(er, d && d.name)
  })
}

function unknown (action, cb) {
  cb("Usage: \n"+owner.usage)
}

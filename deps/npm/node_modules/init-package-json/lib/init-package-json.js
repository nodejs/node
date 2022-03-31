
module.exports = init
module.exports.yes = yes

var PZ = require('promzard').PromZard
var path = require('path')
var def = require.resolve('./default-input.js')

var fs = require('fs')
var semver = require('semver')
var read = require('read')

// to validate the data object at the end as a worthwhile package
// and assign default values for things.
var readJson = require('read-package-json')

function yes (conf) {
  return !!(
    conf.get('yes') || conf.get('y') ||
    conf.get('force') || conf.get('f')
  )
}

function init (dir, input, config, cb) {
  if (typeof config === 'function') {
    cb = config
    config = {}
  }

  // accept either a plain-jane object, or a config object
  // with a "get" method.
  if (typeof config.get !== 'function') {
    var data = config
    config = {
      get: function (k) {
        return data[k]
      },
      toJSON: function () {
        return data
      },
    }
  }

  var packageFile = path.resolve(dir, 'package.json')
  input = path.resolve(input)
  var pkg
  var ctx = { yes: yes(config) }

  var es = readJson.extraSet
  readJson.extraSet = es.filter(function (fn) {
    return fn.name !== 'authors' && fn.name !== 'mans'
  })
  readJson(packageFile, function (er, d) {
    readJson.extraSet = es

    if (er) {
      pkg = {}
    } else {
      pkg = d
    }

    ctx.filename = packageFile
    ctx.dirname = path.dirname(packageFile)
    ctx.basename = path.basename(ctx.dirname)
    if (!pkg.version || !semver.valid(pkg.version)) {
      delete pkg.version
    }

    ctx.package = pkg
    ctx.config = config || {}

    // make sure that the input is valid.
    // if not, use the default
    var pz = new PZ(input, ctx)
    pz.backupFile = def
    pz.on('error', cb)
    pz.on('data', function (pzData) {
      Object.keys(pzData).forEach(function (k) {
        if (pzData[k] !== undefined && pzData[k] !== null) {
          pkg[k] = pzData[k]
        }
      })

      // only do a few of these.
      // no need for mans or contributors if they're in the files
      es = readJson.extraSet
      readJson.extraSet = es.filter(function (fn) {
        return fn.name !== 'authors' && fn.name !== 'mans'
      })
      readJson.extras(packageFile, pkg, function (extrasErr, pkgWithExtras) {
        if (extrasErr) {
          return cb(extrasErr, pkgWithExtras)
        }
        readJson.extraSet = es
        pkgWithExtras = unParsePeople(pkgWithExtras)
        // no need for the readme now.
        delete pkgWithExtras.readme
        delete pkgWithExtras.readmeFilename

        // really don't want to have this lying around in the file
        delete pkgWithExtras._id

        // ditto
        delete pkgWithExtras.gitHead

        // if the repo is empty, remove it.
        if (!pkgWithExtras.repository) {
          delete pkgWithExtras.repository
        }

        // readJson filters out empty descriptions, but init-package-json
        // traditionally leaves them alone
        if (!pkgWithExtras.description) {
          pkgWithExtras.description = pzData.description
        }

        var stringified = JSON.stringify(updateDeps(pkgWithExtras), null, 2) + '\n'
        function write (writeYes) {
          fs.writeFile(packageFile, stringified, 'utf8', function (writeFileErr) {
            if (!writeFileErr && writeYes && !config.get('silent')) {
              console.log('Wrote to %s:\n\n%s\n', packageFile, stringified)
            }
            return cb(writeFileErr, pkgWithExtras)
          })
        }
        if (ctx.yes) {
          return write(true)
        }
        console.log('About to write to %s:\n\n%s\n', packageFile, stringified)
        read({ prompt: 'Is this OK? ', default: 'yes' }, function (promptErr, ok) {
          if (promptErr) {
            return cb(promptErr)
          }
          if (!ok || ok.toLowerCase().charAt(0) !== 'y') {
            console.log('Aborted.')
          } else {
            return write()
          }
        })
      })
    })
  })
}

function updateDeps (depsData) {
  // optionalDependencies don't need to be repeated in two places
  if (depsData.dependencies) {
    if (depsData.optionalDependencies) {
      for (const name of Object.keys(depsData.optionalDependencies)) {
        delete depsData.dependencies[name]
      }
    }
    if (Object.keys(depsData.dependencies).length === 0) {
      delete depsData.dependencies
    }
  }

  return depsData
}

// turn the objects into somewhat more humane strings.
function unParsePeople (data) {
  if (data.author) {
    data.author = unParsePerson(data.author)
  }['maintainers', 'contributors'].forEach(function (set) {
    if (!Array.isArray(data[set])) {
      return
    }
    data[set] = data[set].map(unParsePerson)
  })
  return data
}

function unParsePerson (person) {
  if (typeof person === 'string') {
    return person
  }
  var name = person.name || ''
  var u = person.url || person.web
  var url = u ? (' (' + u + ')') : ''
  var e = person.email || person.mail
  var email = e ? (' <' + e + '>') : ''
  return name + email + url
}

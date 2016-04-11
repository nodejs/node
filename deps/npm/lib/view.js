// npm view [pkg [pkg ...]]

module.exports = view
view.usage = 'npm view [<@scope>/]<pkg>[@<version>] [<field>[.subfield]...]' +
             '\n\naliases: info, show, v'

var npm = require('./npm.js')
var readJson = require('read-package-json')
var log = require('npmlog')
var util = require('util')
var semver = require('semver')
var mapToRegistry = require('./utils/map-to-registry.js')
var npa = require('npm-package-arg')
var path = require('path')

view.completion = function (opts, cb) {
  if (opts.conf.argv.remain.length <= 2) {
    // FIXME: there used to be registry completion here, but it stopped making
    // sense somewhere around 50,000 packages on the registry
    return cb()
  }
  // have the package, get the fields.
  var tag = npm.config.get('tag')
  mapToRegistry(opts.conf.argv.remain[2], npm.config, function (er, uri, auth) {
    if (er) return cb(er)

    npm.registry.get(uri, { auth: auth }, function (er, d) {
      if (er) return cb(er)
      var dv = d.versions[d['dist-tags'][tag]]
      var fields = []
      d.versions = Object.keys(d.versions).sort(semver.compareLoose)
      fields = getFields(d).concat(getFields(dv))
      cb(null, fields)
    })
  })

  function getFields (d, f, pref) {
    f = f || []
    if (!d) return f
    pref = pref || []
    Object.keys(d).forEach(function (k) {
      if (k.charAt(0) === '_' || k.indexOf('.') !== -1) return
      var p = pref.concat(k).join('.')
      f.push(p)
      if (Array.isArray(d[k])) {
        d[k].forEach(function (val, i) {
          var pi = p + '[' + i + ']'
          if (val && typeof val === 'object') getFields(val, f, [p])
          else f.push(pi)
        })
        return
      }
      if (typeof d[k] === 'object') getFields(d[k], f, [p])
    })
    return f
  }
}

function view (args, silent, cb) {
  if (typeof cb !== 'function') {
    cb = silent
    silent = false
  }

  if (!args.length) args = ['.']

  var pkg = args.shift()
  var nv = npa(pkg)
  var name = nv.name
  var local = (name === '.' || !name)

  if (npm.config.get('global') && local) {
    return cb(new Error('Cannot use view command in global mode.'))
  }

  if (local) {
    var dir = npm.prefix
    readJson(path.resolve(dir, 'package.json'), function (er, d) {
      d = d || {}
      if (er && er.code !== 'ENOENT' && er.code !== 'ENOTDIR') return cb(er)
      if (!d.name) return cb(new Error('Invalid package.json'))

      var p = d.name
      nv = npa(p)
      if (pkg && ~pkg.indexOf('@')) {
        nv.rawSpec = pkg.split('@')[pkg.indexOf('@')]
      }

      fetchAndRead(nv, args, silent, cb)
    })
  } else {
    fetchAndRead(nv, args, silent, cb)
  }
}

function fetchAndRead (nv, args, silent, cb) {
  // get the data about this package
  var name = nv.name
  var version = nv.rawSpec || npm.config.get('tag')

  mapToRegistry(name, npm.config, function (er, uri, auth) {
    if (er) return cb(er)

    npm.registry.get(uri, { auth: auth }, function (er, data) {
      if (er) return cb(er)
      if (data['dist-tags'] && data['dist-tags'].hasOwnProperty(version)) {
        version = data['dist-tags'][version]
      }

      if (data.time && data.time.unpublished) {
        var u = data.time.unpublished
        er = new Error('Unpublished by ' + u.name + ' on ' + u.time)
        er.statusCode = 404
        er.code = 'E404'
        er.pkgid = data._id
        return cb(er, data)
      }

      var results = []
      var error = null
      var versions = data.versions || {}
      data.versions = Object.keys(versions).sort(semver.compareLoose)
      if (!args.length) args = ['']

      // remove readme unless we asked for it
      if (args.indexOf('readme') === -1) {
        delete data.readme
      }

      Object.keys(versions).forEach(function (v) {
        if (semver.satisfies(v, version, true)) {
          args.forEach(function (args) {
            // remove readme unless we asked for it
            if (args.indexOf('readme') !== -1) {
              delete versions[v].readme
            }
            results.push(showFields(data, versions[v], args))
          })
        }
      })
      results = results.reduce(reducer, {})
      var retval = results

      if (args.length === 1 && args[0] === '') {
        retval = cleanBlanks(retval)
        log.silly('cleanup', retval)
      }

      if (error || silent) cb(error, retval)
      else printData(results, data._id, cb.bind(null, error, retval))
    })
  })
}

function cleanBlanks (obj) {
  var clean = {}
  Object.keys(obj).forEach(function (version) {
    clean[version] = obj[version]['']
  })
  return clean
}

function reducer (l, r) {
  if (r) {
    Object.keys(r).forEach(function (v) {
      l[v] = l[v] || {}
      Object.keys(r[v]).forEach(function (t) {
        l[v][t] = r[v][t]
      })
    })
  }

  return l
}

// return whatever was printed
function showFields (data, version, fields) {
  var o = {}
  ;[data, version].forEach(function (s) {
    Object.keys(s).forEach(function (k) {
      o[k] = s[k]
    })
  })
  return search(o, fields.split('.'), version.version, fields)
}

function search (data, fields, version, title) {
  var field
  var tail = fields
  while (!field && fields.length) field = tail.shift()
  fields = [field].concat(tail)
  var o
  if (!field && !tail.length) {
    o = {}
    o[version] = {}
    o[version][title] = data
    return o
  }
  var index = field.match(/(.+)\[([^\]]+)\]$/)
  if (index) {
    field = index[1]
    index = index[2]
    if (data.field && data.field.hasOwnProperty(index)) {
      return search(data[field][index], tail, version, title)
    } else {
      field = field + '[' + index + ']'
    }
  }
  if (Array.isArray(data)) {
    if (data.length === 1) {
      return search(data[0], fields, version, title)
    }
    var results = []
    data.forEach(function (data, i) {
      var tl = title.length
      var newt = title.substr(0, tl - fields.join('.').length - 1) +
                 '[' + i + ']' + [''].concat(fields).join('.')
      results.push(search(data, fields.slice(), version, newt))
    })
    results = results.reduce(reducer, {})
    return results
  }
  if (!data.hasOwnProperty(field)) return undefined
  data = data[field]
  if (tail.length) {
    if (typeof data === 'object') {
      // there are more fields to deal with.
      return search(data, tail, version, title)
    } else {
      return new Error('Not an object: ' + data)
    }
  }
  o = {}
  o[version] = {}
  o[version][title] = data
  return o
}

function printData (data, name, cb) {
  var versions = Object.keys(data)
  var msg = ''
  var msgJson = []
  var includeVersions = versions.length > 1
  var includeFields

  versions.forEach(function (v) {
    var fields = Object.keys(data[v])
    includeFields = includeFields || (fields.length > 1)
    msgJson.push({})
    fields.forEach(function (f) {
      var d = cleanup(data[v][f])
      if (fields.length === 1 && npm.config.get('json')) {
        msgJson[msgJson.length - 1][f] = d
      }
      if (includeVersions || includeFields || typeof d !== 'string') {
        if (npm.config.get('json')) {
          msgJson[msgJson.length - 1][f] = d
        } else {
          d = util.inspect(d, false, 5, npm.color)
        }
      } else if (typeof d === 'string' && npm.config.get('json')) {
        d = JSON.stringify(d)
      }
      if (!npm.config.get('json')) {
        if (f && includeFields) f += ' = '
        if (d.indexOf('\n') !== -1) d = ' \n' + d
        msg += (includeVersions ? name + '@' + v + ' ' : '') +
               (includeFields ? f : '') + d + '\n'
      }
    })
  })

  if (msgJson.length && Object.keys(msgJson[0]).length === 1) {
    var k = Object.keys(msgJson[0])[0]
    msgJson = msgJson.map(function (m) { return m[k] })
  }

  if (!msg) {
    msg = JSON.stringify(msgJson[0], null, 2) + '\n'
  } else if (msgJson.length > 1) {
    msg = JSON.stringify(msgJson, null, 2) + '\n'
  }

  // preserve output symmetry by adding a whitespace-only line at the end if
  // there's one at the beginning
  if (/^\s*\n/.test(msg)) msg += '\n'

  // print directly to stdout to not unnecessarily add blank lines
  process.stdout.write(msg)

  cb(null, data)
}
function cleanup (data) {
  if (Array.isArray(data)) {
    return data.map(cleanup)
  }
  if (!data || typeof data !== 'object') return data

  if (typeof data.versions === 'object' &&
      data.versions &&
      !Array.isArray(data.versions)) {
    data.versions = Object.keys(data.versions || {})
  }

  var keys = Object.keys(data)
  keys.forEach(function (d) {
    if (d.charAt(0) === '_') delete data[d]
    else if (typeof data[d] === 'object') data[d] = cleanup(data[d])
  })
  keys = Object.keys(data)
  if (keys.length <= 3 &&
      data.name &&
      (keys.length === 1 ||
       keys.length === 3 && data.email && data.url ||
       keys.length === 2 && (data.email || data.url))) {
    data = unparsePerson(data)
  }
  return data
}
function unparsePerson (d) {
  if (typeof d === 'string') return d
  return d.name +
    (d.email ? ' <' + d.email + '>' : '') +
    (d.url ? ' (' + d.url + ')' : '')
}

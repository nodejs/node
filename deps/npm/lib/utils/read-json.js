
module.exports = readJson
readJson.processJson = processJson
readJson.unParsePeople = unParsePeople
readJson.parsePeople = parsePeople
readJson.clearCache = clearCache

var fs = require("graceful-fs")
  , semver = require("semver")
  , path = require("path")
  , log = require("npmlog")
  , npm = require("../npm.js")
  , cache = {}
  , timers = {}
  , loadPackageDefaults = require("./load-package-defaults.js")

function readJson (jsonFile, opts, cb) {
  if (typeof cb !== "function") cb = opts, opts = {}
  if (cache.hasOwnProperty(jsonFile)) {
    log.verbose("json from cache", jsonFile)
    return cb(null, cache[jsonFile])
  }
  log.verbose("read json", jsonFile)

  opts.file = jsonFile

  var wscript = null
    , contributors = null
    , serverjs = null
    , gypfile = null

  if (opts.gypfile !== null && opts.gypfile !== undefined) {
    gypfile = opts.gypfile
    next()
  } else {
    var pkgdir = path.dirname(jsonFile)

    function hasGyp (has) {
      gypfile = opts.gypfile = has
      next()
    }

    fs.readdir(pkgdir, function (er, gf) {
      // this would be weird.
      if (er) return hasGyp(false)

      // see if there are any *.gyp files in there.
      gf = gf.filter(function (f) {
        return f.match(/\.gyp$/)
      })
      gf = gf[0]
      return hasGyp(!!gf)
    })
  }

  if (opts.wscript !== null && opts.wscript !== undefined) {
    wscript = opts.wscript
    next()
  } else fs.readFile( path.join(path.dirname(jsonFile), "wscript")
                    , function (er, data) {
    if (er) opts.wscript = false
    else opts.wscript = !!(data.toString().match(/(^|\n)def build\b/)
                           && data.toString().match(/(^|\n)def configure\b/))
    wscript = opts.wscript
    next()
  })

  if (opts.contributors !== null && opts.contributors !== undefined) {
    contributors = opts.contributors
    next()
  } else fs.readFile( path.join(path.dirname(jsonFile), "AUTHORS")
                    , function (er, data) {
    if (er) opts.contributors = false
    else {
      data = data.toString().split(/\r?\n/).map(function (l) {
        l = l.trim().split("#").shift()
        return l
      }).filter(function (l) { return l })
      opts.contributors = data
    }
    contributors = opts.contributors
    next()
  })

  if (opts.serverjs !== null && opts.serverjs !== undefined) {
    serverjs = opts.serverjs
    next()
  } else fs.stat( path.join(path.dirname(jsonFile), "server.js")
                , function (er, st) {
    if (er) opts.serverjs = false
    else opts.serverjs = st.isFile()
    serverjs = opts.serverjs
    next()
  })

  function next () {
    if (wscript === null ||
        contributors === null ||
        gypfile === null ||
        serverjs === null) {
      return
    }

    // XXX this api here is insane.  being internal is no excuse.
    // please refactor.
    var thenLoad = processJson(opts, function (er, data) {
      if (er) return cb(er)
      var doLoad = !(jsonFile.indexOf(npm.cache) === 0 &&
                     path.basename(path.dirname(jsonFile)) !== "package")
      if (!doLoad) return cb(er, data)
      loadPackageDefaults(data, path.dirname(jsonFile), cb)
    })

    fs.readFile(jsonFile, function (er, data) {
      if (er && er.code === "ENOENT") {
        // single-file module, maybe?
        // check index.js for a /**package { ... } **/ section.
        var indexFile = path.resolve(path.dirname(jsonFile), "index.js")
        return fs.readFile(indexFile, function (er2, data) {
          // if this doesn't work, then die with the original error.
          if (er2) return cb(er)
          data = parseIndex(data)
          if (!data) return cb(er)
          thenLoad(null, data)
        })
      }
      thenLoad(er, data)
    })
  }
}

// sync. no io.
// /**package { "name": "foo", "version": "1.2.3", ... } **/
function parseIndex (data) {
  data = data.toString()
  data = data.split(/^\/\*\*package(?:\s|$)/m)
  if (data.length < 2) return null
  data = data[1]
  data = data.split(/\*\*\/$/m)
  if (data.length < 2) return null
  data = data[0]
  data = data.replace(/^\s*\*/mg, "")
  return data
}

function processJson (opts, cb) {
  if (typeof cb !== "function") cb = opts, opts = {}
  if (typeof cb !== "function") {
    var thing = cb, cb = null
    return P(null, thing)
  } else return P

  function P (er, thing) {
    if (er) {
      if (cb) return cb(er, thing)
      throw er
    }
    if (typeof thing === "object" && !Buffer.isBuffer(thing)) {
      return processObject(opts, cb)(er, thing)
    } else {
      return processJsonString(opts, cb)(er, thing)
    }
  }
}

function processJsonString (opts, cb) { return function (er, jsonString) {
  if (er) return cb(er, jsonString)
  jsonString += ""
  var json
  try {
    json = JSON.parse(jsonString)
  } catch (ex) {
    if (opts.file && opts.file.indexOf(npm.dir) === 0) {
      try {
        json = require("vm").runInNewContext("(\n"+jsonString+"\n)")
        log.error("Error parsing json", opts.file, ex)
      } catch (ex2) {
        return jsonParseFail(ex, opts.file, cb)
      }
    } else {
      return jsonParseFail(ex, opts.file, cb)
    }
  }
  return processObject(opts, cb)(er, json)
}}


function jsonParseFail (ex, file, cb) {
  var e = new Error(
    "Failed to parse json\n"+ex.message)
  e.code = "EJSONPARSE"
  e.file = file
  if (cb) return cb(e)
  throw e
}

// a warning for deprecated or likely-incorrect fields
var typoWarned = {}
function typoWarn (json) {
  if (typoWarned[json._id]) return
  typoWarned[json._id] = true

  if (json.modules) {
    log.verbose("package.json", "'modules' object is deprecated", json._id)
    delete json.modules
  }

  // http://registry.npmjs.org/-/fields
  var typos = { "dependancies": "dependencies"
              , "dependecies": "dependencies"
              , "depdenencies": "dependencies"
              , "devEependencies": "devDependencies"
              , "depends": "dependencies"
              , "dev-dependencies": "devDependencies"
              , "devDependences": "devDependencies"
              , "devDepenencies": "devDependencies"
              , "devdependencies": "devDependencies"
              , "repostitory": "repository"
              , "prefereGlobal": "preferGlobal"
              , "hompage": "homepage"
              , "hampage": "homepage" // XXX maybe not a typo, just delicious?
              , "autohr": "author"
              , "autor": "author"
              , "contributers": "contributors"
              , "publicationConfig": "publishConfig"
              }

  Object.keys(typos).forEach(function (d) {
    if (json.hasOwnProperty(d)) {
      log.warn( json._id, "package.json: '" + d + "' should probably be '"
              + typos[d] + "'" )
    }
  })

  // bugs typos
  var bugsTypos = { "web": "url"
                  , "name": "url"
                  }

  if (typeof json.bugs === "object") {
    // just go ahead and correct these.
    Object.keys(bugsTypos).forEach(function (d) {
      if (json.bugs.hasOwnProperty(d)) {
        json.bugs[ bugsTypos[d] ] = json.bugs[d]
        delete json.bugs[d]
      }
    })
  }

  // script typos
  var scriptTypos = { "server": "start" }
  if (json.scripts) Object.keys(scriptTypos).forEach(function (d) {
    if (json.scripts.hasOwnProperty(d)) {
      log.warn( json._id
              , "package.json: scripts['" + d + "'] should probably be "
              + "scripts['" + scriptTypos[d] + "']" )
    }
  })
}


function processObject (opts, cb) { return function (er, json) {
  // json._npmJsonOpts = opts
  if (npm.config.get("username")) {
    json._npmUser = { name: npm.config.get("username")
                    , email: npm.config.get("email") }
  }

  // slashes would be a security risk.
  // anything else will just fail harmlessly.
  if (!json.name) {
    var e = new Error("No 'name' field found in package.json")
    if (cb) return cb(e)
    throw e
  }
  json.name = json.name.trim()
  if (json.name.charAt(0) === "." || json.name.match(/[\/@\s\+%:]/)) {
    var msg = "Invalid name: "
            + JSON.stringify(json.name)
            + " may not start with '.' or contain %/@+: or whitespace"
      , e = new Error(msg)
    if (cb) return cb(e)
    throw e
  }
  if (json.name.toLowerCase() === "node_modules") {
    var msg = "Invalid package name: node_modules"
      , e = new Error(msg)
    if (cb) return cb(e)
    throw e
  }
  if (json.name.toLowerCase() === "favicon.ico") {
    var msg = "Sorry, favicon.ico is a picture, not a package."
      , e = new Error(msg)
    if (cb) return cb(e)
    throw e
  }

  if (json.repostories) {
    var msg = "'repositories' (plural) No longer supported.\n"
            + "Please pick one, and put it in the 'repository' field."
      , e = new Error(msg)
    // uncomment once this is no longer an issue.
    // if (cb) return cb(e)
    // throw e
    log.error("json", "incorrect json: "+json.name, msg)
    json.repostory = json.repositories[0]
    delete json.repositories
  }

  if (json.repository) {
    if (typeof json.repository === "string") {
      json.repository = { type : "git"
                        , url : json.repository }
    }
    var repo = json.repository.url || ""
    repo = repo.replace(/^(https?|git):\/\/[^\@]+\@github.com/
                       ,'$1://github.com')
    if (json.repository.type === "git"
        && ( repo.match(/^https?:\/\/github.com/)
          || repo.match(/github.com\/[^\/]+\/[^\/]+\/?$/)
             && !repo.match(/\.git$/)
        )) {
      repo = repo.replace(/^https?:\/\/github.com/, 'git://github.com')
      if (!repo.match(/\.git$/)) {
        repo = repo.replace(/\/?$/, '.git')
      }
    }
    if (repo.match(/github\.com\/[^\/]+\/[^\/]+\/?$/)
        && repo.match(/\.git\.git$/)) {
      log.warn(json._id, "Probably broken git url", repo)
    }
    json.repository.url = repo
  }

  var files = json.files
  if (files && !Array.isArray(files)) {
    log.warn(json._id, "Invalid 'files' member.  See 'npm help json'", files)
    delete json.files
  }

  var kw = json.keywords
  if (typeof kw === "string") {
    kw = kw.split(/,\s+/)
    json.keywords = kw
  }

  json._id = json.name+"@"+json.version

  var scripts = json.scripts || {}

  // if it has a bindings.gyp, then build with node-gyp
  if (opts.gypfile && !json.prebuilt) {
    log.verbose(json._id, "has bindings.gyp", [json.prebuilt, opts])
    if (!scripts.install && !scripts.preinstall) {
      scripts.install = "node-gyp rebuild"
      json.scripts = scripts
    }
  }

  // if it has a wscript, then build it.
  if (opts.wscript && !json.prebuilt) {
    log.verbose(json._id, "has wscript", [json.prebuilt, opts])
    if (!scripts.install && !scripts.preinstall) {
      // don't fail if it was unexpected, just try.
      scripts.preinstall = "node-waf clean || (exit 0); node-waf configure build"
      json.scripts = scripts
    }
  }

  // if it has an AUTHORS, then credit them
  if (opts.contributors && Array.isArray(opts.contributors)
      && opts.contributors.length) {
    json.contributors = opts.contributors
  }

  // if it has a server.js, then start it.
  if (opts.serverjs && !scripts.start) {
    scripts.start = "node server.js"
    json.scripts = scripts
  }

  if (!(semver.valid(json.version))) {
    var m
    if (!json.version) {
      m = "'version' field missing\n"
    } else {
      m = "Invalid 'version' field: "+json.version+"\n"
    }

    m += "'version' Must be X.Y.Z, with an optional trailing tag.\n"
      + "See the section on 'version' in `npm help json`"

    var e = new Error(m)
    if (cb) return cb(e)
    throw e
  }
  json.version = semver.clean(json.version)

  if (json.bin && typeof json.bin === "string") {
    var b = {}
    b[ json.name ] = json.bin
    json.bin = b
  }

  if (json.bundledDependencies && !json.bundleDependencies) {
    json.bundleDependencies = json.bundledDependencies
    delete json.bundledDependencies
  }

  if (json.bundleDependencies && !Array.isArray(json.bundleDependencies)) {
    var e = new Error("bundleDependencies must be an array.\n"
                     +"See `npm help json`")
    if (cb) return cb(e)
    throw e
  }

  if (json["dev-dependencies"] && !json.devDependencies) {
    json.devDependencies = json["dev-dependencies"]
    delete json["dev-dependencies"]
  }

  ; [ "dependencies"
    , "devDependencies"
    , "optionalDependencies"
    ].forEach(function (d) {
      json[d] = json.hasOwnProperty(d)
              ? depObjectify(json[d], d, json._id)
              : {}
    })

  // always merge optionals into deps
  Object.keys(json.optionalDependencies).forEach(function (d) {
    json.dependencies[d] = json.optionalDependencies[d]
  })

  if (opts.dev
      || npm.config.get("dev")
      || npm.config.get("npat")) {
    Object.keys(json.devDependencies || {}).forEach(function (d) {
      json.dependencies[d] = json.devDependencies[d]
    })
  }

  typoWarn(json)

  json = testEngine(json)
  json = parsePeople(unParsePeople(json))
  if ( json.bugs ) json.bugs = parsePerson(unParsePerson(json.bugs))
  json._npmVersion = npm.version
  json._nodeVersion = process.version
  if (opts.file) {
    log.verbose("caching json", opts.file)
    cache[opts.file] = json
    // arbitrary
    var keys = Object.keys(cache)
      , l = keys.length
    if (l > 10000) for (var i = 0; i < l - 5000; i ++) {
      delete cache[keys[i]]
    }
  }
  if (cb) cb(null,json)
  return json
}}

var depObjectifyWarn = {}
function depObjectify (deps, d, id) {
  if (!deps) return {}
  if (typeof deps === "string") {
    deps = deps.trim().split(/[\n\r\s\t ,]+/)
  }
  if (!Array.isArray(deps)) return deps
  var o = {}
  deps.forEach(function (d) {
    d = d.trim().split(/(:?[@\s><=])/)
    o[d.shift()] = d.join("").trim().replace(/^@/, "")
  })
  return o
}

function testEngine (json) {
  // if engines is empty, then assume that node is allowed.
  if ( !json.engines
      || Array.isArray(json.engines)
        && !json.engines.length
      || typeof json.engines === "object"
        && !Object.keys(json.engines).length
      ) {
    json.engines = { "node" : "*" }
  }
  if (typeof json.engines === "string") {
    if (semver.validRange(json.engines) !== null) {
      json.engines = { "node" : json.engines }
    } else json.engines = [ json.engines ]
  }

  var nodeVer = npm.config.get("node-version")
    , ok = false
  if (nodeVer) nodeVer = nodeVer.replace(/\+$/, '')
  if (Array.isArray(json.engines)) {
    // Packages/1.0 commonjs style, with an array.
    // hack it to just hang a "node" member with the version range,
    // then do the npm-style check below.
    for (var i = 0, l = json.engines.length; i < l; i ++) {
      var e = json.engines[i].trim()
      if (e.substr(0, 4) === "node") {
        json.engines.node = e.substr(4)
      } else if (e.substr(0, 3) === "npm") {
        json.engines.npm = e.substr(3)
      }
    }
  }
  if (json.engines.node === "") json.engines.node = "*"
  if (json.engines.node && null === semver.validRange(json.engines.node)) {
    log.warn( json._id
            , "Invalid range in engines.node.  Please see `npm help json`"
            , json.engines.node )
  }

  if (nodeVer) {
    json._engineSupported = semver.satisfies( nodeVer
                                            , json.engines.node || "null" )
  }
  if (json.engines.hasOwnProperty("npm") && json._engineSupported) {
    json._engineSupported = semver.satisfies(npm.version, json.engines.npm)
  }
  return json
}

function unParsePeople (json) { return parsePeople(json, true) }

function parsePeople (json, un) {
  var fn = un ? unParsePerson : parsePerson
  if (json.author) json.author = fn(json.author)
  ;["maintainers", "contributors"].forEach(function (set) {
    if (Array.isArray(json[set])) json[set] = json[set].map(fn)
  })
  return json
}

function unParsePerson (person) {
  if (typeof person === "string") return person
  var name = person.name || ""
    , u = person.url || person.web
    , url = u ? (" ("+u+")") : ""
    , e = person.email || person.mail
    , email = e ? (" <"+e+">") : ""
  return name+email+url
}

function parsePerson (person) {
  if (typeof person !== "string") return person
  var name = person.match(/^([^\(<]+)/)
    , url = person.match(/\(([^\)]+)\)/)
    , email = person.match(/<([^>]+)>/)
    , obj = {}
  if (name && name[0].trim()) obj.name = name[0].trim()
  if (email) obj.email = email[1]
  if (url) obj.url = url[1]
  return obj
}

function clearCache (prefix) {
  if (!prefix) {
    cache = {}
    return
  }
  Object.keys(cache).forEach(function (c) {
    if (c.indexOf(prefix) === 0) delete cache[c]
  })
}

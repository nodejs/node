// vim: set softtabstop=16 shiftwidth=16:

try {
                readJson.log = require("npmlog")
} catch (er) {
                readJson.log = {
                                info: function () {},
                                verbose: function () {},
                                warn: function () {}
                }
}


try {
                var fs = require("graceful-fs")
} catch (er) {
                var fs = require("fs")
}


module.exports = readJson

var LRU = require("lru-cache")
readJson.cache = new LRU({max: 1000})
var path = require("path")
var glob = require("glob")
var slide = require("slide")
var asyncMap = slide.asyncMap
var semver = require("semver")

// put more stuff on here to customize.
readJson.extraSet = [gypfile, wscript, serverjs, authors, readme, mans, bins]

var typoWarned = {}
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
            , "hampage": "homepage"
            , "autohr": "author"
            , "autor": "author"
            , "contributers": "contributors"
            , "publicationConfig": "publishConfig"
            }
var bugsTypos = { "web": "url", "name": "url" }
var scriptTypos = { "server": "start", "tests": "test" }
var depTypes = [ "dependencies"
               , "devDependencies"
               , "optionalDependencies" ]


function readJson (file, cb) {
                var c = readJson.cache.get(file)
                if (c) {
                                readJson.log.verbose("from cache", file)
                                cb = cb.bind(null, null, c)
                                return process.nextTick(cb);
                }
                readJson.log.verbose("read json", file)
                cb = (function (orig) { return function (er, data) {
                                if (data) readJson.cache.set(file, data);
                                return orig(er, data)
                } })(cb)
                readJson_(file, cb)
}


function readJson_ (file, cb) {
                fs.readFile(file, "utf8", function (er, d) {
                                parseJson(file, er, d, cb)
                })
}

function parseJson (file, er, d, cb) {
                if (er && er.code === "ENOENT") {
                                indexjs(file, er, cb)
                                return
                }
                if (er) return cb(er);
                try {
                                d = JSON.parse(d)
                } catch (er) {
                                d = parseIndex(d)
                                if (!d) return cb(parseError(er, file));
                }
                extras(file, d, cb)
}


function indexjs (file, er, cb) {
                if (path.basename(file) === "index.js") {
                                return cb(er);
                }
                var index = path.resolve(path.dirname(file), "index.js")
                fs.readFile(index, "utf8", function (er2, d) {
                                if (er2) return cb(er);
                                d = parseIndex(d)
                                if (!d) return cb(er);
                                extras(file, d, cb)
                })
}


readJson.extras = extras
function extras (file, data, cb) {
                asyncMap(readJson.extraSet, function (fn, cb) {
                                return fn(file, data, cb)
                }, function (er) {
                                if (er) return cb(er);
                                final(file, data, cb)
                })
}

function gypfile (file, data, cb) {
                var dir = path.dirname(file)
                var s = data.scripts || {}
                if (s.install || s.preinstall) {
                                return cb(null, data);
                }
                glob("*.gyp", { cwd: dir }, function (er, files) {
                                if (er) return cb(er);
                                gypfile_(file, data, files, cb)
                })
}

function gypfile_ (file, data, files, cb) {
                if (!files.length) return cb(null, data);
                var s = data.scripts || {}
                s.install = "node-gyp rebuild"
                data.scripts = s
                data.gypfile = true
                return cb(null, data);
}

function wscript (file, data, cb) {
                var dir = path.dirname(file)
                var s = data.scripts || {}
                if (s.install || s.preinstall) {
                                return cb(null, data);
                }
                glob("wscript", { cwd: dir }, function (er, files) {
                                if (er) return cb(er);
                                wscript_(file, data, files, cb)
                })
}
function wscript_ (file, data, files, cb) {
                if (!files.length || data.gypfile) return cb(null, data);
                var s = data.scripts || {}
                s.install = "node-waf clean ; node-waf configure build"
                data.scripts = s
                return cb(null, data);
}

function serverjs (file, data, cb) {
                var dir = path.dirname(file)
                var s = data.scripts || {}
                if (s.start) return cb(null, data)
                glob("server.js", { cwd: dir }, function (er, files) {
                                if (er) return cb(er);
                                serverjs_(file, data, files, cb)
                })
}
function serverjs_ (file, data, files, cb) {
                if (!files.length) return cb(null, data);
                var s = data.scripts || {}
                s.start = "node server.js"
                data.scripts = s
                return cb(null, data)
}

function authors (file, data, cb) {
                if (data.contributors) return cb(null, data);
                var af = path.resolve(path.dirname(file), "AUTHORS")
                fs.readFile(af, "utf8", function (er, ad) {
                                // ignore error.  just checking it.
                                if (er) return cb(null, data);
                                authors_(file, data, ad, cb)
                })
}
function authors_ (file, data, ad, cb) {
                ad = ad.split(/\r?\n/g).map(function (line) {
                                return line.replace(/^\s*#.*$/, '').trim()
                }).filter(function (line) {
                                return line
                })
                data.contributors = ad
                return cb(null, data)
}

var defDesc = "Unnamed repository; edit this file " +
              "'description' to name the repository."
function gitDescription (file, data, cb) {
                if (data.description) return cb(null, data);
                var dir = path.dirname(file)
                // just cuz it'd be nice if this file mattered...
                var gitDesc = path.resolve(dir, '.git/description')
                fs.readFile(gitDesc, 'utf8', function (er, desc) {
                                if (desc) desc = desc.trim()
                                if (!er && desc !== defDesc)
                                                data.description = desc
                                return cb(null, data)
                })
}

function readmeDescription (file, data) {
                if (data.description) return cb(null, data);
                var d = data.readme
                if (!d) return;
                // the first block of text before the first heading
                // that isn't the first line heading
                d = d.trim().split('\n')
                for (var s = 0; d[s] && d[s].trim().match(/^(#|$)/); s ++);
                var l = d.length
                for (var e = s + 1; e < l && d[e].trim(); e ++);
                data.description = d.slice(s, e).join(' ').trim()
}

function readme (file, data, cb) {
                if (data.readme) return cb(null, data);
                var dir = path.dirname(file)
                var globOpts = { cwd: dir, nocase: true, mark: true }
                glob("README?(.*)", globOpts, function (er, files) {
                                if (er) return cb(er);
                                // don't accept directories.
                                files = files.filter(function (file) {
                                                return !file.match(/\/$/)
                                })
                                if (!files.length) return cb();
                                var rm = path.resolve(dir, files[0])
                                readme_(file, data, rm, cb)
                })
}
function readme_(file, data, rm, cb) {
                var rmfn = path.basename(rm);
                fs.readFile(rm, "utf8", function (er, rm) {
                                // maybe not readable, or something.
                                if (er) return cb()
                                data.readme = rm
                                data.readmeFilename = rmfn
                                return cb(er, data)
                })
}

function mans (file, data, cb) {
                var m = data.directories && data.directories.man
                if (data.man || !m) return cb(null, data);
                m = path.resolve(path.dirname(file), m)
                glob("**/*.[0-9]", { cwd: m }, function (er, mans) {
                                if (er) return cb(er);
                                mans_(file, data, mans, cb)
                })
}
function mans_ (file, data, mans, cb) {
                var m = data.directories && data.directories.man
                data.man = mans.map(function (mf) {
                                return path.resolve(path.dirname(file), m, mf)
                })
                return cb(null, data)
}

function bins (file, data, cb) {
                if (Array.isArray(data.bin)) {
                                return bins_(file, data, data.bin, cb)
                }
                var m = data.directories && data.directories.bin
                if (data.bin || !m) return cb(null, data);
                m = path.resolve(path.dirname(file), m)
                glob("**", { cwd: m }, function (er, bins) {
                                if (er) return cb(er);
                                bins_(file, data, bins, cb)
                })
}
function bins_ (file, data, bins, cb) {
                var m = data.directories && data.directories.bin
                data.bin = bins.reduce(function (acc, mf) {
                                if (mf && mf.charAt(0) !== '.') {
                                                var f = path.basename(mf)
                                                acc[f] = path.join(m, mf)
                                }
                                return acc
                }, {})
                return cb(null, data)
}

function final (file, data, cb) {
                var ret = validName(file, data)
                if (ret !== true) return cb(ret);
                ret = validVersion(file, data)
                if (ret !== true) return cb(ret);

                data._id = data.name + "@" + data.version
                typoWarn(file, data)
                validRepo(file, data)
                validFiles(file, data)
                validBin(file, data)
                validMan(file, data)
                validBundled(file, data)
                objectifyDeps(file, data)
                unParsePeople(file, data)
                parsePeople(file, data)

                if (data.description &&
                    typeof data.description !== 'string') {
                                warn(file, data,
                                     "'description' field should be a string")
                                delete data.description
                }

                if (data.readme && !data.description)
                                readmeDescription(file, data)

                readJson.cache.set(file, data)
                cb(null, data)
}


// /**package { "name": "foo", "version": "1.2.3", ... } **/
function parseIndex (data) {
                data = data.split(/^\/\*\*package(?:\s|$)/m)
                if (data.length < 2) return null
                data = data[1]
                data = data.split(/\*\*\/$/m)
                if (data.length < 2) return null
                data = data[0]
                data = data.replace(/^\s*\*/mg, "")
                try {
                                return JSON.parse(data)
                } catch (er) {
                                return null
                }
}

function parseError (ex, file) {
                var e = new Error("Failed to parse json\n"+ex.message)
                e.code = "EJSONPARSE"
                e.file = file
                return e
}

// a warning for deprecated or likely-incorrect fields
function typoWarn (file, data) {
                if (data.modules) {
                                warn(file, data,
                                     "'modules' is deprecated")
                                delete data.modules
                }
                Object.keys(typos).forEach(function (d) {
                                checkTypo(file, data, d)
                })
                bugsTypoWarn(file, data)
                scriptTypoWarn(file, data)
                noreadmeWarn(file, data)
                typoWarned[data._id] = true
}

function noreadmeWarn (file, data) {
                if (data.readme) return;
                warn(file, data, "No README.md file found!")
                data.readme = "ERROR: No README.md file found!"
}

function checkTypo (file, data, d) {
                if (!data.hasOwnProperty(d)) return;
                warn(file, data,
                     "'" + d + "' should probably be '" + typos[d] + "'" )
}

function bugsTypoWarn (file, data) {
                var b = data.bugs
                if (!b || typeof b !== "object") return
                Object.keys(b).forEach(function (k) {
                                if (bugsTypos[k]) {
                                                b[bugsTypos[k]] = b[k]
                                                delete b[k]
                                }
                })
}

function scriptTypoWarn (file, data) {
                var s = data.scripts
                if (!s || typeof s !== "object") return
                Object.keys(s).forEach(function (k) {
                                if (scriptTypos[k]) {
                                                scriptWarn_(file, data, k)
                                }
                })
}
function scriptWarn_ (file, data, k) {
                warn(file, data, "scripts['" + k + "'] should probably " +
                     "be scripts['" + scriptTypos[k] + "']")
}

function validRepo (file, data) {
                if (data.repostories) {
                                warnRepositories(file, data)
                }
                if (!data.repository) return;
                if (typeof data.repository === "string") {
                                data.repository = {
                                                type: "git",
                                                url: data.repository
                                }
                }
                var r = data.repository.url || ""
                // use the non-private urls
                r = r.replace(/^(https?|git):\/\/[^\@]+\@github.com/,
                              '$1://github.com')
                r = r.replace(/^https?:\/\/github.com/,
                              'git://github.com')
                if (r.match(/github.com\/[^\/]+\/[^\/]+\.git\.git$/)) {
                                warn(file, data, "Probably broken git " +
                                     "url: " + r)
                }
}
function warnRepostories (file, data) {
                warn(file, data,
                     "'repositories' (plural) Not supported.\n" +
                     "Please pick one as the 'repository' field");
                data.repository = data.repositories[0]
}

function validFiles (file, data) {
                var files = data.files
                if (files && !Array.isArray(files)) {
                                warn(file, data, "Invalid 'files' member")
                                delete data.files
                }
}

function validBin (file, data) {
                if (!data.bin) return;
                if (typeof data.bin === "string") {
                                var b = {}
                                b[data.name] = data.bin
                                data.bin = b
                }
}

function validMan (file, data) {
                if (!data.man) return;
                if (typeof data.man === "string") {
                                data.man = [ data.man ]
                }
}

function validBundled (file, data) {
                var bdd = "bundledDependencies"
                var bd = "bundleDependencies"
                if (data[bdd] && !data[bd]) {
                                data[bd] = data[bdd]
                                delete data[bdd]
                }

                if (data[bd] && !Array.isArray(data[bd])) {
                                warn(file, data, "bundleDependencies " +
                                     "must be an array")
                }
}

function objectifyDeps (file, data) {
                depTypes.forEach(function (d) {
                                objectifyDep_(file, data, d)
                })

                var o = data.optionalDependencies
                if (!o) return;
                var d = data.dependencies || {}
                Object.keys(o).forEach(function (k) {
                                d[k] = o[k]
                })
                data.dependencies = d
}
function objectifyDep_ (file, data, type) {
                if (!data[type]) return;
                data[type] = depObjectify(file, data, data[type])
}
function depObjectify (file, data, deps) {
                if (!deps) return {}
                if (typeof deps === "string") {
                                deps = deps.trim().split(/[\n\r\s\t ,]+/)
                }
                if (!Array.isArray(deps)) return deps
                var o = {}
                deps.forEach(function (d) {
                                d = d.trim().split(/(:?[@\s><=])/)
                                var dn = d.shift()
                                var dv = d.join("")
                                dv = dv.trim()
                                dv = dv.replace(/^@/, "")
                                o[dn] = dv
                })
                return o
}


function warn (f, d, m) {
                if (typoWarned[d._id]) return;
                readJson.log.warn("package.json", d._id, m)
}


function validName (file, data) {
                if (!data.name) return new Error("No 'name' field")
                data.name = data.name.trim()
                if (data.name.charAt(0) === "." ||
                    data.name.match(/[\/@\s\+%:]/) ||
                    data.name.toLowerCase() === "node_modules" ||
                    data.name.toLowerCase() === "favicon.ico") {
                                return new Error("Invalid name: " +
                                                 JSON.stringify(data.name))
                }
                return true
}


function parseKeywords (file, data) {
                var kw = data.keywords
                if (typeof kw === "string") {
                                kw = kw.split(/,\s+/)
                                data.keywords = kw
                }
}

function validVersion (file, data) {
                var v = data.version
                if (!v) return new Error("no version");
                if (!semver.valid(v)) {
                                return new Error("invalid version: "+v)
                }
                data.version = semver.clean(data.version)
                return true
}
function unParsePeople (file, data) {
                return parsePeople(file, data, true)
}

function parsePeople (file, data, un) {
                var fn = un ? unParsePerson : parsePerson
                if (data.author) data.author = fn(data.author)
                ;["maintainers", "contributors"].forEach(function (set) {
                                if (!Array.isArray(data[set])) return;
                                data[set] = data[set].map(fn)
                })
                return data
}

function unParsePerson (person) {
                if (typeof person === "string") return person
                var name = person.name || ""
                var u = person.url || person.web
                var url = u ? (" ("+u+")") : ""
                var e = person.email || person.mail
                var email = e ? (" <"+e+">") : ""
                return name+email+url
}

function parsePerson (person) {
                if (typeof person !== "string") return person
                var name = person.match(/^([^\(<]+)/)
                var url = person.match(/\(([^\)]+)\)/)
                var email = person.match(/<([^>]+)>/)
                var obj = {}
                if (name && name[0].trim()) obj.name = name[0].trim()
                if (email) obj.email = email[1];
                if (url) obj.url = url[1];
                return obj
}

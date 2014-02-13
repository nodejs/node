var semver = require("semver")
var parseGitHubURL = require("github-url-from-git")
var depTypes = ["dependencies","devDependencies","optionalDependencies"]
var extractDescription = require("./extract_description")
var url = require("url")
var typos = require("./typos")
var coreModuleNames = require("./core_module_names")
var githubUserRepo = require("github-url-from-username-repo")
var warningMessages = require('./warning_messages.json')
var format = require('util').format

var fixer = module.exports = {
  // default warning function
  warn: function() {},

  fixRepositoryField: function(data) {
    if (data.repositories) {
      this.warn(warningMessages.repositories);
      data.repository = data.repositories[0]
    }
    if (!data.repository) return this.warn(warningMessages.missingRepository)
    if (typeof data.repository === "string") {
      data.repository = {
        type: "git",
        url: data.repository
      }
    }
    var r = data.repository.url || ""
    if (r) {
      var ghurl = parseGitHubURL(r)
      if (ghurl) {
        r = ghurl.replace(/^https?:\/\//, 'git://')
      } else if (githubUserRepo(r)) {
        // repo has 'user/reponame' filled in as repo
        data.repository.url = githubUserRepo(r)
      }
    }

    if (r.match(/github.com\/[^\/]+\/[^\/]+\.git\.git$/)) {
      this.warn(format(warningMessages.brokenGitUrl, r))
    }
  }

, fixTypos: function(data) {
    Object.keys(typos.topLevel).forEach(function (d) {
      if (data.hasOwnProperty(d)) {
        this.warn(makeTypoWarning(d, typos.topLevel[d]))
      }
    }, this)
  }

, fixScriptsField: function(data) {
    if (!data.scripts) return
    if (typeof data.scripts !== "object") {
      this.warn(warningMessages.nonObjectScripts)
      delete data.scripts
    }
    Object.keys(data.scripts).forEach(function (k) {
      if (typeof data.scripts[k] !== "string") {
        this.warn(warningMessages.nonStringScript)
        delete data.scripts[k]
      } else if (typos.script[k]) {
        this.warn(makeTypoWarning(k, typos.script[k], "scripts"))
      }
    }, this)
  }

, fixFilesField: function(data) {
    var files = data.files
    if (files && !Array.isArray(files)) {
      this.warn(warningMessages.nonArrayFiles)
      delete data.files
    } else if (data.files) {
      data.files = data.files.filter(function(file) {
        if (!file || typeof file !== "string") {
          this.warn(format(warningMessages.invalidFilename, file))
          return false
        } else {
          return true
        }
      }, this)
    }
  }

, fixBinField: function(data) {
    if (!data.bin) return;
    if (typeof data.bin === "string") {
      var b = {}
      b[data.name] = data.bin
      data.bin = b
    }
  }

, fixManField: function(data) {
    if (!data.man) return;
    if (typeof data.man === "string") {
      data.man = [ data.man ]
    }
  }
, fixBundleDependenciesField: function(data) {
    var bdd = "bundledDependencies"
    var bd = "bundleDependencies"
    if (data[bdd] && !data[bd]) {
      data[bd] = data[bdd]
      delete data[bdd]
    }
    if (data[bd] && !Array.isArray(data[bd])) {
      this.warn(warningMessages.nonArrayBundleDependencies)
      delete data[bd]
    } else if (data[bd]) {
      data[bd] = data[bd].filter(function(bd) {
        if (!bd || typeof bd !== 'string') {
          this.warn(format(nonStringBundleDependency, bd))
          return false
        } else {
          return true
        }
      }, this)
    }
  }

, fixDependencies: function(data, strict) {
    var loose = !strict
    objectifyDeps(data, this.warn)
    addOptionalDepsToDeps(data, this.warn)
    this.fixBundleDependenciesField(data)

    ;['dependencies','devDependencies'].forEach(function(deps) {
      if (!(deps in data)) return
      if (!data[deps] || typeof data[deps] !== "object") {
        this.warn(format(warningMessages.nonObjectDependencies, deps))
        delete data[deps]
        return
      }
      Object.keys(data[deps]).forEach(function (d) {
        var r = data[deps][d]
        if (typeof r !== 'string') {
          this.warn(format(nonStringDependency, d, JSON.stringify(r)))
          delete data[deps][d]
        }
        // "/" is not allowed as packagename for publishing, but for git-urls
        // normalize shorthand-urls
        if (githubUserRepo(data[deps][d])) {
          data[deps][d] = githubUserRepo(data[deps][d])
        }
      }, this)
    }, this)
  }

, fixModulesField: function (data) {
    if (data.modules) {
      this.warn(warningMessages.deprecatedModules)
      delete data.modules
    }
  }

, fixKeywordsField: function (data) {
    if (typeof data.keywords === "string") {
      data.keywords = data.keywords.split(/,\s+/)
    }
    if (data.keywords && !Array.isArray(data.keywords)) {
      delete data.keywords
      this.warn(warningMessages.nonArrayKeywords)
    } else if (data.keywords) {
      data.keywords = data.keywords.filter(function(kw) {
        if (typeof kw !== "string" || !kw) {
          this.warn(warningMessages.nonStringKeyword);
          return false
        } else {
          return true
        }
      }, this)
    }
  }

, fixVersionField: function(data, strict) {
    // allow "loose" semver 1.0 versions in non-strict mode
    // enforce strict semver 2.0 compliance in strict mode
    var loose = !strict
    if (!data.version) {
      data.version = ""
      return true
    }
    if (!semver.valid(data.version, loose)) {
      throw new Error('Invalid version: "'+ data.version + '"')
    }
    data.version = semver.clean(data.version, loose)
    return true
  }

, fixPeople: function(data) {
    modifyPeople(data, unParsePerson)
    modifyPeople(data, parsePerson)
  }

, fixNameField: function(data, strict) {
    if (!data.name && !strict) {
      data.name = ""
      return
    }
    if (typeof data.name !== "string") {
      throw new Error("name field must be a string.")
    }
    if (!strict)
      data.name = data.name.trim()
    ensureValidName(data.name, strict)
    if (coreModuleNames.indexOf(data.name) !== -1)
      this.warn(format(warningMessages.conflictingName, data.name))
  }


, fixDescriptionField: function (data) {
    if (data.description && typeof data.description !== 'string') {
      this.warn(warningMessages.nonStringDescription)
      delete data.description
    }
    if (data.readme && !data.description)
      data.description = extractDescription(data.readme)
      if(data.description === undefined) delete data.description;
    if (!data.description) this.warn(warningMessages.missingDescription)
  }

, fixReadmeField: function (data) {
    if (!data.readme) {
      this.warn(warningMessages.missingReadme)
      data.readme = "ERROR: No README data found!"
    }
  }

, fixBugsField: function(data) {
    if (!data.bugs && data.repository && data.repository.url) {
      var gh = parseGitHubURL(data.repository.url)
      if(gh) {
        if(gh.match(/^https:\/\/github.com\//))
          data.bugs = {url: gh + "/issues"}
        else // gist url
          data.bugs = {url: gh}
      }
    }
    else if(data.bugs) {
      var emailRe = /^.+@.*\..+$/
      if(typeof data.bugs == "string") {
        if(emailRe.test(data.bugs))
          data.bugs = {email:data.bugs}
        else if(url.parse(data.bugs).protocol)
          data.bugs = {url: data.bugs}
        else
          this.warn(warningMessages.nonEmailUrlBugsString)
      }
      else {
        bugsTypos(data.bugs, this.warn)
        var oldBugs = data.bugs
        data.bugs = {}
        if(oldBugs.url) {
          if(typeof(oldBugs.url) == "string" && url.parse(oldBugs.url).protocol)
            data.bugs.url = oldBugs.url
          else
            this.warn(warningMessages.nonUrlBugsUrlField)
        }
        if(oldBugs.email) {
          if(typeof(oldBugs.email) == "string" && emailRe.test(oldBugs.email))
            data.bugs.email = oldBugs.email
          else
            this.warn(warningMessages.nonEmailBugsEmailField)
        }
      }
      if(!data.bugs.email && !data.bugs.url) {
        delete data.bugs
        this.warn(warningMessages.emptyNormalizedBugs)
      }
    }
  }

, fixHomepageField: function(data) {
    if (!data.homepage && data.repository && data.repository.url) {
      var gh = parseGitHubURL(data.repository.url)
      if (gh)
          data.homepage = gh
      else
        return true
    } else if (!data.homepage)
      return true

    if(typeof data.homepage !== "string") {
      this.warn(warningMessages.nonUrlHomepage)
      return delete data.homepage
    }
    if(!url.parse(data.homepage).protocol) {
      this.warn(warningMessages.missingProtocolHomepage)
      data.homepage = "http://" + data.homepage
    }
  }
}

function ensureValidName (name, strict) {
  if (name.charAt(0) === "." ||
      name.match(/[\/@\s\+%:]/) ||
      name !== encodeURIComponent(name) ||
      (strict && name !== name.toLowerCase()) ||
      name.toLowerCase() === "node_modules" ||
      name.toLowerCase() === "favicon.ico") {
        throw new Error("Invalid name: " + JSON.stringify(name))
  }
}

function modifyPeople (data, fn) {
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

function addOptionalDepsToDeps (data, warn) {
  var o = data.optionalDependencies
  if (!o) return;
  var d = data.dependencies || {}
  Object.keys(o).forEach(function (k) {
    d[k] = o[k]
  })
  data.dependencies = d
}

function depObjectify (deps, type, warn) {
  if (!deps) return {}
  if (typeof deps === "string") {
    deps = deps.trim().split(/[\n\r\s\t ,]+/)
  }
  if (!Array.isArray(deps)) return deps
  warn(format(warningMessages.deprecatedArrayDependencies, type))
  var o = {}
  deps.filter(function (d) {
    return typeof d === "string"
  }).forEach(function(d) {
    d = d.trim().split(/(:?[@\s><=])/)
    var dn = d.shift()
    var dv = d.join("")
    dv = dv.trim()
    dv = dv.replace(/^@/, "")
    o[dn] = dv
  })
  return o
}

function objectifyDeps (data, warn) {
  depTypes.forEach(function (type) {
    if (!data[type]) return;
    data[type] = depObjectify(data[type], type, warn)
  })
}

function bugsTypos(bugs, warn) {
  if (!bugs) return
  Object.keys(bugs).forEach(function (k) {
    if (typos.bugs[k]) {
      warn(makeTypoWarning(k, typos.bugs[k], "bugs"))
      bugs[typos.bugs[k]] = bugs[k]
      delete bugs[k]
    }
  })
}

function makeTypoWarning (providedName, probableName, field) {
  if (field) {
    providedName = field + "['" + providedName + "']"
    probableName = field + "['" + probableName + "']"
  }
  return format(warningMessages.typo, providedName, probableName)
}
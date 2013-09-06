var semver = require("semver")
var parseGitHubURL = require("github-url-from-git")
var depTypes = ["dependencies","devDependencies","optionalDependencies"]
var extractDescription = require("./extract_description")
var url = require("url")
var typos = require("./typos")

var fixer = module.exports = {
  // default warning function
  warn: function() {},

  fixRepositoryField: function(data) {
    if (data.repositories) {
      this.warn("'repositories' (plural) Not supported.\n" +
           "Please pick one as the 'repository' field");
      data.repository = data.repositories[0]
    }
    if (!data.repository) return this.warn('No repository field.')
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
      } else if (/^[\w-]+\/[\w-]+$/.test(r)) {
        // repo has 'user/reponame' filled in as repo
        data.repository.url = "git://github.com/" + r
      }
    }

    if (r.match(/github.com\/[^\/]+\/[^\/]+\.git\.git$/)) {
      this.warn("Probably broken git url: " + r)
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
      this.warn("scripts must be an object")
      delete data.scripts
    }
    Object.keys(data.scripts).forEach(function (k) {
      if (typeof data.scripts[k] !== "string") {
        this.warn("script values must be string commands")
        delete data.scripts[k]
      } else if (typos.script[k]) {
        this.warn(makeTypoWarning(k, typos.script[k], "scripts"))
      }
    }, this)
  }

, fixFilesField: function(data) {
    var files = data.files
    if (files && !Array.isArray(files)) {
      this.warn("Invalid 'files' member")
      delete data.files
    } else if (data.files) {
      data.files = data.files.filter(function(file) {
        if (!file || typeof file !== "string") {
          this.warn("Invalid filename in 'files' list: " + file)
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
      this.warn("Invalid 'bundleDependencies' list. " +
                "Must be array of package names")
      delete data[bd]
    } else if (data[bd]) {
      data[bd] = data[bd].filter(function(bd) {
        if (!bd || typeof bd !== 'string') {
          this.warn("Invalid bundleDependencies member: " + bd)
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
        this.warn(deps + " field must be an object")
        delete data[deps]
        return
      }
      Object.keys(data[deps]).forEach(function (d) {
        var r = data[deps][d]
        if (typeof r !== 'string') {
          this.warn('Invalid dependency: ' + d + ' ' + JSON.stringify(r))
          delete data[deps][d]
        }
      }, this)
    }, this)
  }

, fixModulesField: function (data) {
    if (data.modules) {
      this.warn("modules field is deprecated")
      delete data.modules
    }
  }

, fixKeywordsField: function (data) {
    if (typeof data.keywords === "string") {
      data.keywords = data.keywords.split(/,\s+/)
    }
    if (data.keywords && !Array.isArray(data.keywords)) {
      delete data.keywords
      this.warn("keywords should be an array of strings")
    } else if (data.keywords) {
      data.keywords = data.keywords.filter(function(kw) {
        if (typeof kw !== "string" || !kw) {
          this.warn("keywords should be an array of strings");
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
  }
  

, fixDescriptionField: function (data) {
    if (data.description && typeof data.description !== 'string') {
      this.warn("'description' field should be a string")
      delete data.description
    }
    if (data.readme && !data.description)
      data.description = extractDescription(data.readme)
    if (!data.description) this.warn('No description')
  }
  
, fixReadmeField: function (data) {
    if (!data.readme) {
      this.warn("No README data")
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
          this.warn("Bug string field must be url, email, or {email,url}")
      }
      else {
        bugsTypos(data.bugs, this.warn)
        var oldBugs = data.bugs
        data.bugs = {}
        if(oldBugs.url) {
          if(typeof(oldBugs.url) == "string" && url.parse(oldBugs.url).protocol)
            data.bugs.url = oldBugs.url
          else
            this.warn("bugs.url field must be a string url. Deleted.")
        }
        if(oldBugs.email) {
          if(typeof(oldBugs.email) == "string" && emailRe.test(oldBugs.email))
            data.bugs.email = oldBugs.email
          else
            this.warn("bugs.email field must be a string email. Deleted.")
        }
      }
      if(!data.bugs.email && !data.bugs.url) {
        delete data.bugs
        this.warn("Normalized value of bugs field is an empty object. Deleted.")
      }
    }
  }

,  fixHomepageField: function(data) {
    if(!data.homepage) return true;
    if(typeof data.homepage !== "string") {
      this.warn("homepage field must be a string url. Deleted.")
      return delete data.homepage
    }
    if(!url.parse(data.homepage).protocol) {
      this.warn("homepage field must start with a protocol.")
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

function depObjectify (deps) {
  if (!deps) return {}
  if (typeof deps === "string") {
    deps = deps.trim().split(/[\n\r\s\t ,]+/)
  }
  if (!Array.isArray(deps)) return deps
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
    data[type] = depObjectify(data[type])
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
  return providedName + " should probably be " + probableName + "."
}

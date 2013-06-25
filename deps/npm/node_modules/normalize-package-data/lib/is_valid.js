// a warning for deprecated or likely-incorrect fields

module.exports = isValid

var typos = require("./typos")

function isValid (data, warnFunc) {
  var hasWarned = false
  function warn(msg) {
    hasWarned = true
    if(warnFunc) warnFunc(msg)
  }
  if (data.modules) warn("'modules' is deprecated")
  Object.keys(typos.topLevel).forEach(function (d) {
    if (data.hasOwnProperty(d)) {
      warn(makeTypoWarning(d, typos.topLevel[d]))
    }
  })
  checkBugsField(data.bugs, warn)
  checkScriptsField(data.scripts, warn)
  if (!data.repository) warn("No repository field.")
  if (!data.readme) warn("No readme data.")
  if (data.description && typeof data.description !== 'string') {
    warn("'description' field should be a string")
  }
  if (data[data.bundledDependencies] && 
      !Array.isArray(data.bundleDependencies)) {
    warn("bundleDependencies must be an array")
  }
  return !hasWarned
}

function checkBugsField (bugs, warn) {
  if (!bugs || typeof bugs !== "object") return
  Object.keys(bugs).forEach(function (k) {
    if (typos.bugs[k]) {
      bugs[typos.bugs[k]] = bugs[k]
      delete bugs[k]
    }
  })
}

function checkScriptsField (scripts, warn) {
  if (!scripts || typeof scripts !== "object") return
  Object.keys(scripts).forEach(function (k) {
    if (typos.script[k]) {
      warn(makeTypoWarning(k, typos.script[k], "scripts"))
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

var scopedPackagePattern = new RegExp("^(?:@([^/]+?)[/])?([^/]+?)$");
var builtins = require("builtins")
var blacklist = [
  "node_modules",
  "favicon.ico"
];

var validate = module.exports = function(name) {

  var warnings = []
  var errors = []

  if (name === null) {
    errors.push("name cannot be null")
    return done(warnings, errors)
  }

  if (name === undefined) {
    errors.push("name cannot be undefined")
    return done(warnings, errors)
  }

  if (typeof name !== "string") {
    errors.push("name must be a string")
    return done(warnings, errors)
  }

  if (!name.length) {
    errors.push("name length must be greater than zero")
  }

  if (name.match(/^\./)) {
    errors.push("name cannot start with a period")
  }

  if (name.match(/^_/)) {
    errors.push("name cannot start with an underscore")
  }

  if (name.trim() !== name) {
    errors.push("name cannot contain leading or trailing spaces")
  }

  // No funny business
  blacklist.forEach(function(blacklistedName){
    if (name.toLowerCase() === blacklistedName) {
      errors.push(blacklistedName + " is a blacklisted name")
    }
  })

  // Generate warnings for stuff that used to be allowed

  // core module names like http, events, util, etc
  builtins.forEach(function(builtin){
    if (name.toLowerCase() === builtin) {
      warnings.push(builtin + " is a core module name")
    }
  })

  // really-long-package-names-------------------------------such--length-----many---wow
  // the thisisareallyreallylongpackagenameitshouldpublishdowenowhavealimittothelengthofpackagenames-poch.
  if (name.length > 214) {
    warnings.push("name can no longer contain more than 214 characters")
  }

  // mIxeD CaSe nAMEs
  if (name.toLowerCase() !== name) {
    warnings.push("name can no longer contain capital letters")
  }

  if (encodeURIComponent(name) !== name) {

    // Maybe it's a scoped package name, like @user/package
    var nameMatch = name.match(scopedPackagePattern)
    if (nameMatch) {
      var user = nameMatch[1]
      var pkg = nameMatch[2]
      if (encodeURIComponent(user) === user && encodeURIComponent(pkg) === pkg) {
        return done(warnings, errors)
      }
    }

    errors.push("name can only contain URL-friendly characters")
  }

  return done(warnings, errors)

}

validate.scopedPackagePattern = scopedPackagePattern

var done = function (warnings, errors) {
  var result = {
    validForNewPackages: errors.length === 0 && warnings.length === 0,
    validForOldPackages: errors.length === 0,
    warnings: warnings,
    errors: errors
  }
  if (!result.warnings.length) delete result.warnings
  if (!result.errors.length) delete result.errors
  return result
}

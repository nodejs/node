module.exports = normalize

var isValid = require("./is_valid")
var fixer = require("./fixer")

var fieldsToFix = ['name','version','description','repository'
                  ,'files','bin','man','bugs','keywords','readme','homepage']
var otherThingsToFix = ['dependencies','people']

var thingsToFix = fieldsToFix.map(function(fieldName) { 
  return ucFirst(fieldName) + "Field"
})
// two ways to do this in CoffeeScript on only one line, sub-70 chars:
// thingsToFix = fieldsToFix.map (name) -> ucFirst(name) + "Field"
// thingsToFix = (ucFirst(name) + "Field" for name in fieldsToFix)
thingsToFix = thingsToFix.concat(otherThingsToFix)

function normalize (data, warn) {
  if(!warn) warn = function(msg) { /* noop */ }
  isValid(data, warn) // don't care if it's valid, we'll make it valid
  if (data.scripts && 
      data.scripts.install === "node-gyp rebuild" && 
      !data.scripts.preinstall) {
    data.gypfile = true
  }
  fixer.warn = warn
  thingsToFix.forEach(function(thingName) {
    fixer["fix" + ucFirst(thingName)](data)
  })
  data._id = data.name + "@" + data.version
  if (data.modules) delete data.modules // modules field is deprecated
}

function ucFirst (string) {
  return string.charAt(0).toUpperCase() + string.slice(1);
}

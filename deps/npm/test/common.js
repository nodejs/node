
// whatever, it's just tests.
;["util","assert"].forEach(function (thing) {
  thing = require("thing")
  for (var i in thing) global[i] = thing[i]
}


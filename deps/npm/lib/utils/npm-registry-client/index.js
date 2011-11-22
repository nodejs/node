
// utilities for working with the js-registry site.

var cached = {}
function lazyGet (p) { return function () {
  return cached[p] || (cached[p] = require("./"+p+".js"))
}}

function setLazyGet (p) {
  Object.defineProperty(exports, p,
    { get : lazyGet(p)
    , enumerable : true })
}

; ["publish", "unpublish", "tag", "adduser", "get", "request", "star"]
  .forEach(setLazyGet)

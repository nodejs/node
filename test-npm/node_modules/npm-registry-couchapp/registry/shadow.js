module.exports =
  { _id:'_design/ghost'
  , rewrites: ghostRewrites()
  , language: "javascript"
  }

function ghostRewrites () {
  return require("./rewrites.js").map(function (rule) {

    var to = rule.to
    // Note: requests to /_users/blah are still passed through directly.
    if (rule.to.match(/\/_users(?:\/|$)/) &&
        !rule.from.match(/^\/?_users/)) {
      if (rule.method === "GET") {
        to = to.replace(/\/_users(\/|$)/, "/public_users$1")
      } else if (rule.method === "PUT") {
        // when couchdb lets us PUT to _update functions on _users,
        // uncomment this.  For now, we'll just leave it as-is,
        // and use newedits=false to sync the rev fields to public_users.
        //
        // to = to.replace(/\/_users(\/|$)/,
        //                 "/_users/_design/auth/_update/norev$1")
      }
    } else {
      to = "../app/" + to
    }
    to = to.replace(/\/\/+/g, '/')

    return { from: rule.from
           , method: rule.method
           , query: rule.query
           , to: to
           }
  })
}

if (require.main === module) console.log(module.exports)

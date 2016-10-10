var ddoc = module.exports =
  { _id:'_design/scratch'
  , shows: require("./shows.js")
  , updates: require("./updates.js")
  , rewrites: require("./rewrites.js")
  , views: require("./views.js")
  , lists: require("./lists.js")
  , validate_doc_update: require("./validate_doc_update.js")
  , language: "javascript"
  }

if (process.env.DEPLOY_VERSION)
  ddoc.deploy_version = process.env.DEPLOY_VERSION
else
  throw new Error('Must set DEPLOY_VERSION env to `git describe` output')

var modules = require("./modules.js")
for (var i in modules) ddoc[i] = modules[i]

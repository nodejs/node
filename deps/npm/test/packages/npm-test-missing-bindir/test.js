
var assert = require("assert")
assert.equal(undefined, process.env.npm_config__password, "password exposed!")
assert.equal(undefined, process.env.npm_config__auth, "auth exposed!")
assert.equal(undefined, process.env.npm_config__authCrypt, "authCrypt exposed!")

#!/usr/bin/env node
var fs = require("fs")
  , path = require("path")
  , cli = path.resolve(__dirname, "..", "doc", "cli")
  , clidocs = null
  , api = path.resolve(__dirname, "..", "doc", "api")
  , apidocs = null
  , readme = path.resolve(__dirname, "..", "README.md")

fs.readdir(cli, done("cli"))
fs.readdir(api, done("api"))

function done (which) { return function (er, docs) {
  if (er) throw er
  if (which === "api") apidocs = docs
  else clidocs = docs

  if (apidocs && clidocs) next()
}}

function filter (d) {
  return d !== "index.md"
       && d.charAt(0) !== "."
       && d.match(/\.md$/)
}

function next () {
  console.log(
    "npm-index(1) -- Index of all npm documentation\n" +
    "==============================================\n")

  apidocs = apidocs.filter(filter).map(function (d) {
    return [3, path.resolve(api, d)]
  })

  clidocs = clidocs.filter(filter).map(function (d) {
    return [1, path.resolve(cli, d)]
  })

  writeLine([1, readme])

  console.log("# Command Line Documentation")

  clidocs.forEach(writeLine)

  console.log("# API Documentation")
  apidocs.forEach(writeLine)
}

function writeLine (sd) {
  var sxn = sd[0]
    , doc = sd[1]
    , d = path.basename(doc, ".md")
    , s = fs.lstatSync(doc)

  if (s.isSymbolicLink()) return

  var content = fs.readFileSync(doc, "utf8").split("\n")[0].split("--")[1]

  console.log("## npm-%s(%d)\n", d, sxn)
  console.log(content + "\n")
}

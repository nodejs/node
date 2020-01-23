#!/usr/bin/env node

var fs = require('fs')
var marked = require('marked-man')
var npm = require('../lib/npm.js')
var args = process.argv.slice(2)
var src = args[0]
var dest = args[1] || src

fs.readFile(src, 'utf8', function (err, data) {
  if (err) return console.log(err)

  function replacer (match, p1) {
    return 'npm help ' + p1.replace(/npm /, '')
  }

  var result = data.replace(/@VERSION@/g, npm.version)
    .replace(/---([\s\S]+)---/g, '')
    .replace(/\[([^\]]+)\]\(\/cli-commands\/([^)]+)\)/g, replacer)
    .replace(/\[([^\]]+)\]\(\/configuring-npm\/([^)]+)\)/g, replacer)
    .replace(/\[([^\]]+)\]\(\/using-npm\/([^)]+)\)/g, replacer)
    .replace(/(# .*)\s+(## (.*))/g, '$1 - $3')
    .trim()

  fs.writeFile(dest, marked(result), 'utf8', function (err) {
    if (err) return console.log(err)
  })
})

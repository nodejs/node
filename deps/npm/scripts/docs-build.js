#!/usr/bin/env node

var fs = require('fs')
var marked = require('marked-man')
var npm = require('../lib/npm.js')
var args = process.argv.slice(2)
var src = args[0]
var dest = args[1] || src

fs.readFile(src, 'utf8', function (err, data) {
  if (err) return console.log(err)

  function frontmatter (match, p1) {
    const fm = { }

    p1.split(/\r?\n/).forEach((kv) => {
      let result = kv.match(/^([^\s:]+):\s*(.*)/)
      if (result) {
        fm[result[1]] = result[2]
      }
    })

    return `# ${fm['title']}(${fm['section']}) - ${fm['description']}`
  }

  function replacer (match, p1) {
    return 'npm help ' + p1.replace(/npm /, '')
  }

  var result = data.replace(/@VERSION@/g, npm.version)
    .replace(/^---\n([\s\S]+\n)---/, frontmatter)
    .replace(/\[([^\]]+)\]\(\/commands\/([^)]+)\)/g, replacer)
    .replace(/\[([^\]]+)\]\(\/configuring-npm\/([^)]+)\)/g, replacer)
    .replace(/\[([^\]]+)\]\(\/using-npm\/([^)]+)\)/g, replacer)
    .trim()

  fs.writeFile(dest, marked(result), 'utf8', function (err) {
    if (err) return console.log(err)
  })
})

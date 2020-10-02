#!/usr/bin/env node

const dirs = []
let doSort = false
process.argv.slice(2).forEach(arg => {
  if (arg === '-h' || arg === '--help') {
    console.log('usage: npm-packlist [-s --sort] [directory, directory, ...]')
    process.exit(0)
  } else if (arg === '-s' || arg === '--sort')
    doSort = true
  else
    dirs.push(arg)
})

const sort = list => doSort ? list.sort((a, b) => a.localeCompare(b)) : list

const packlist = require('../')
if (!dirs.length)
  console.log(sort(packlist.sync({ path: process.cwd() })).join('\n'))
else
  dirs.forEach(path => {
    console.log(`> ${path}`)
    console.log(sort(packlist.sync({ path })).join('\n'))
  })

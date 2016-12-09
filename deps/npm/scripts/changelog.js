'use strict'
/*
Usage:

node scripts/changelog.js [comittish]

Generates changelog entries in our format as best as its able based on
commits starting at comittish, or if that's not passed, latest.

Ordinarily this is run via the gen-changelog shell script, which appends
the result to the changelog.

*/
const execSync = require('child_process').execSync
const branch = process.argv[2] || 'origin/latest'
const log = execSync(`git log --reverse --pretty='format:%h %H%d %s (%aN)%n%b%n---%n' ${branch}...`).toString().split(/\n/)

main()

function shortname (url) {
  let matched = url.match(/https:\/\/github.com\/([^/]+\/[^/]+)\/(?:pull|issues)\/(\d+)/)
  if (!matched) return false
  let repo = matched[1]
  let id = matched[2]
  if (repo !== 'npm/npm') {
    return `${repo}#${id}`
  } else {
    return `#${id}`
  }
}

function print_commit (c) {
  console.log(`* [\`${c.shortid}\`](https://github.com/npm/npm/commit/${c.fullid})`)
  if (c.fixes) {
    let label = shortname(c.fixes)
    if (label) {
      console.log(`  [${label}](${c.fixes})`)
    } else {
      console.log(`  [#${c.fixes}](https://github.com/npm/npm/issues/${c.fixes})`)
    }
  } else if (c.prurl) {
    let label = shortname(c.prurl)
    if (label) {
      console.log(`  [${label}](${c.prurl})`)
    } else {
      console.log(`  [#](${c.prurl})`)
    }
  }
  let msg = c.message
    .replace(/^\s+/mg, '')
    .replace(/^[-a-z]+: /, '')
    .replace(/^/mg, '  ')
    .replace(/\n$/, '')
  // backtickify package@version
    .replace(/^(\s*[^@\s]+@\d+[.]\d+[.]\d+)(\s*\S)/g, '$1:$2')
    .replace(/\b([^@\s]+@\d+[.]\d+[.]\d+)\b/g, '`$1`')
  // linkify commitids
    .replace(/\b([a-f0-9]{7,8})\b/g, '[`$1`](https://github.com/npm/npm/commit/$1)')
    .replace(/\b#(\d+)\b/g, '[#$1](https://github.com/npm/npm/issues/$1)')
  console.log(msg)
  if (c.credit) {
    c.credit.forEach(function (credit) {
      console.log(`  ([@${credit}](https://github.com/${credit}))`)
    })
  } else {
    console.log(`  ([@${c.author}](https://github.com/${c.author}))`)
  }
}

function main () {
  let commit
  log.forEach(function (line) {
    let m
    /*eslint no-cond-assign:0*/
    if (/^---$/.test(line)) {
      print_commit(commit)
    } else if (m = line.match(/^([a-f0-9]{7}) ([a-f0-9]+) (?:[(]([^)]+)[)] )?(.*?) [(](.*?)[)]/)) {
      commit = {
        shortid: m[1],
        fullid: m[2],
        branch: m[3],
        message: m[4],
        author: m[5],
        prurl: null,
        fixes: null,
        credit: null
      }
    } else if (m = line.match(/^PR-URL: (.*)/)) {
      commit.prurl = m[1]
    } else if (m = line.match(/^Credit: @(.*)/)) {
      if (!commit.credit) commit.credit = []
      commit.credit.push(m[1])
    } else if (m = line.match(/^Fixes: #?(.*?)/)) {
      commit.fixes = m[1]
    } else if (m = line.match(/^Reviewed-By: @(.*)/)) {
      commit.reviewed = m[1]
    } else if (/\S/.test(line)) {
      commit.message += `\n${line}`
    }
  })
}

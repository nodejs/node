'use strict'
/*
Usage:

node scripts/changelog.js [comittish]

Generates changelog entries in our format as best as its able based on
commits starting at comittish, or if that's not passed, master.

Ordinarily this is run via the gen-changelog shell script, which appends
the result to the changelog.

*/
const execSync = require('child_process').execSync
const branch = process.argv[2] || 'master'
const log = execSync(`git log --pretty='format:%h %H%d %s (%aN)%n%b%n---%n' ${branch}...`).toString().split(/\n/)
const authormap = {
  'Rebecca Turner': 'iarna',
  'Forrest L Norvell': 'othiym23',
  'Kyle Mitchell': 'kemitchell',
  'Chris Rebert': 'cvrebert',
  'Kat Marchán': 'zkat'
}

main()

function print_commit (c) {
  let m
  console.log(`* [\`${c.shortid}\`](https://github.com/npm/npm/commit/${c.fullid})`)
  if (c.fixes) {
    console.log(`  [#${c.fixes}](https://github.com/npm/npm/issues/${c.fixes})`)
  } else if (c.prurl && (m = c.prurl.match(/https:\/\/github.com\/([^/]+\/[^/]+)\/pull\/(\d+)/))) {
    let repo = m[1]
    let prid = m[2]
    if (repo !== 'npm/npm') {
      console.log(`  [${repo}#${prid}](${c.prurl})`)
    } else {
      console.log(`  [#${prid}](${c.prurl})`)
    }
  } else if (c.prurl) {
    console.log(`  [#](${c.prurl})`)
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
        author: authormap[m[5]] || m[5],
        prurl: null,
        fixes: null,
        credit: null
      }
    } else if (m = line.match(/^PR-URL: (.*)/)) {
      commit.prurl = m[1]
    } else if (m = line.match(/^Credit: @(.*)/)) {
      if (!commit.credit) commit.credit = []
      commit.credit.push(m[1])
    } else if (m = line.match(/^Fixes: #(.*)/)) {
      commit.fixes = m[1]
    } else if (m = line.match(/^Reviewed-By: @(.*)/)) {
      commit.reviewed = m[1]
    } else if (/\S/.test(line)) {
      commit.message += `\n${line}`
    }
  })
}

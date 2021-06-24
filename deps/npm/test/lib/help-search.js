const t = require('tap')
const { join } = require('path')
const { fake: mockNpm } = require('../fixtures/mock-npm')
const ansicolors = require('ansicolors')

const OUTPUT = []
const output = (msg) => {
  OUTPUT.push(msg)
}

const config = {
  long: false,
}
const npmHelpErr = null
const npm = mockNpm({
  color: false,
  config,
  flatOptions: {
    long: false,
  },
  usage: 'npm test usage',
  commands: {
    help: (args, cb) => {
      return cb(npmHelpErr)
    },
  },
  output,
})

let globRoot = null
const globDir = {
  'npm-exec.md': 'the exec command\nhelp has multiple lines of exec help\none of them references exec',
  'npm-something.md': 'another\ncommand you run\nthat\nreferences exec\nand has multiple lines\nwith no matches\nthat will be ignored\nand another line\nthat does have exec as well',
  'npm-run-script.md': 'the scripted run-script command runs scripts\nand has lines\nsome of which dont match the string run\nor script\nscript',
  'npm-install.md': 'does a thing in a script\nif a thing does not exist in a thing you run\nto install it and run it maybe in a script',
  'npm-help.md': 'will run the `help-search` command if you need to run it to help you search',
  'npm-help-search.md': 'is the help search command\nthat you get if you run help-search',
  'npm-useless.md': 'exec\nexec',
  'npm-more-useless.md': 'exec exec',
  'npm-extra-useless.md': 'exec\nexec\nexec',
}
const glob = (p, cb) =>
  cb(null, Object.keys(globDir).map((file) => join(globRoot, file)))

const HelpSearch = t.mock('../../lib/help-search.js', {
  glob,
})
const helpSearch = new HelpSearch(npm)

t.test('npm help-search', t => {
  globRoot = t.testdir(globDir)
  t.teardown(() => {
    OUTPUT.length = 0
    globRoot = null
  })

  return helpSearch.exec(['exec'], (err) => {
    if (err)
      throw err

    t.match(OUTPUT, /Top hits for "exec"/, 'outputs results')
    t.end()
  })
})

t.test('npm help-search multiple terms', t => {
  globRoot = t.testdir(globDir)
  t.teardown(() => {
    OUTPUT.length = 0
    globRoot = null
  })

  return helpSearch.exec(['run', 'script'], (err) => {
    if (err)
      throw err

    t.match(OUTPUT, /Top hits for/, 'outputs results')
    t.match(OUTPUT, /run:\d+ script:\d+/, 'shows hit counts for both terms')
    t.end()
  })
})

t.test('npm help-search long output', t => {
  globRoot = t.testdir(globDir)
  config.long = true
  t.teardown(() => {
    OUTPUT.length = 0
    config.long = false
    globRoot = null
  })

  return helpSearch.exec(['exec'], (err) => {
    if (err)
      throw err

    t.match(OUTPUT, /has multiple lines of exec help/, 'outputs detailed results')
    t.end()
  })
})

t.test('npm help-search long output with color', t => {
  globRoot = t.testdir(globDir)
  config.long = true
  npm.color = true
  t.teardown(() => {
    OUTPUT.length = 0
    config.long = false
    npm.color = false
    globRoot = null
  })

  return helpSearch.exec(['help-search'], (err) => {
    if (err)
      throw err

    const highlightedText = ansicolors.bgBlack(ansicolors.red('help-search'))
    t.equal(OUTPUT.some((line) => line.includes(highlightedText)), true, 'returned highlighted search terms')
    t.end()
  })
})

t.test('npm help-search no args', t => {
  return helpSearch.exec([], (err) => {
    t.notOk(err)
    t.match(OUTPUT, /npm help-search/, 'outputs usage')
    t.end()
  })
})

t.test('npm help-search no matches', t => {
  globRoot = t.testdir(globDir)
  t.teardown(() => {
    OUTPUT.length = 0
    globRoot = null
  })

  return helpSearch.exec(['asdfasdf'], (err) => {
    if (err)
      throw err

    t.match(OUTPUT, /No matches/)
    t.end()
  })
})

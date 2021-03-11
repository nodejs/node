const { test } = require('tap')
const { join } = require('path')
const requireInject = require('require-inject')
const ansicolors = require('ansicolors')

const OUTPUT = []
const output = (msg) => {
  OUTPUT.push(msg)
}

let npmHelpArgs = null
let npmHelpErr = null
const npm = {
  color: false,
  flatOptions: {
    long: false,
  },
  commands: {
    help: (args, cb) => {
      npmHelpArgs = args
      return cb(npmHelpErr)
    },
  },
  output,
}

let npmUsageArg = null
const npmUsage = (npm, arg) => {
  npmUsageArg = arg
}

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

const HelpSearch = requireInject('../../lib/help-search.js', {
  '../../lib/utils/npm-usage.js': npmUsage,
  glob,
})
const helpSearch = new HelpSearch(npm)

test('npm help-search', t => {
  globRoot = t.testdir(globDir)
  t.teardown(() => {
    OUTPUT.length = 0
    globRoot = null
  })

  return helpSearch.exec(['exec'], (err) => {
    if (err)
      throw err

    t.match(OUTPUT, /Top hits for/, 'outputs results')
    t.match(OUTPUT, /Did you mean this\?\n\s+exec/, 'matched command, so suggest it')
    t.end()
  })
})

test('npm help-search multiple terms', t => {
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

test('npm help-search single result prints full section', t => {
  globRoot = t.testdir(globDir)
  t.teardown(() => {
    OUTPUT.length = 0
    npmHelpArgs = null
    globRoot = null
  })

  return helpSearch.exec(['does not exist in'], (err) => {
    if (err)
      throw err

    t.strictSame(npmHelpArgs, ['npm-install'], 'identified the correct man page and called help with it')
    t.end()
  })
})

test('npm help-search single result propagates error', t => {
  globRoot = t.testdir(globDir)
  npmHelpErr = new Error('help broke')
  t.teardown(() => {
    OUTPUT.length = 0
    npmHelpArgs = null
    npmHelpErr = null
    globRoot = null
  })

  return helpSearch.exec(['does not exist in'], (err) => {
    t.strictSame(npmHelpArgs, ['npm-install'], 'identified the correct man page and called help with it')
    t.match(err, /help broke/, 'propagated the error from help')
    t.end()
  })
})

test('npm help-search long output', t => {
  globRoot = t.testdir(globDir)
  npm.flatOptions.long = true
  t.teardown(() => {
    OUTPUT.length = 0
    npm.flatOptions.long = false
    globRoot = null
  })

  return helpSearch.exec(['exec'], (err) => {
    if (err)
      throw err

    t.match(OUTPUT, /has multiple lines of exec help/, 'outputs detailed results')
    t.end()
  })
})

test('npm help-search long output with color', t => {
  globRoot = t.testdir(globDir)
  npm.flatOptions.long = true
  npm.color = true
  t.teardown(() => {
    OUTPUT.length = 0
    npm.flatOptions.long = false
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

test('npm help-search no args', t => {
  return helpSearch.exec([], (err) => {
    t.match(err, /npm help-search/, 'throws usage')
    t.end()
  })
})

test('npm help-search no matches', t => {
  globRoot = t.testdir(globDir)
  t.teardown(() => {
    OUTPUT.length = 0
    npmUsageArg = null
    globRoot = null
  })

  return helpSearch.exec(['asdfasdf'], (err) => {
    if (err)
      throw err

    t.equal(npmUsageArg, false, 'called npmUsage for no matches')
    t.end()
  })
})

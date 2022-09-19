const t = require('tap')
const { join } = require('path')
const { fake: mockNpm } = require('../../fixtures/mock-npm')
const chalk = require('chalk')

const OUTPUT = []
const output = msg => {
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
  exec: async () => {
    if (npmHelpErr) {
      throw npmHelpErr
    }
  },
  output,
})

let globRoot = null
const globDir = {
  'npm-exec.md':
    'the exec command\nhelp has multiple lines of exec help\none of them references exec',
  /* eslint-disable-next-line max-len */
  'npm-something.md': 'another\ncommand you run\nthat\nreferences exec\nand has multiple lines\nwith no matches\nthat will be ignored\nand another line\nthat does have exec as well',
  /* eslint-disable-next-line max-len */
  'npm-run-script.md': 'the scripted run-script command runs scripts\nand has lines\nsome of which dont match the string run\nor script\nscript',
  /* eslint-disable-next-line max-len */
  'npm-install.md': 'does a thing in a script\nif a thing does not exist in a thing you run\nto install it and run it maybe in a script',
  'npm-help.md': 'will run the `help-search` command if you need to run it to help you search',
  'npm-help-search.md': 'is the help search command\nthat you get if you run help-search',
  'npm-useless.md': 'exec\nexec',
  'npm-more-useless.md': 'exec exec',
  'npm-extra-useless.md': 'exec\nexec\nexec',
}
const glob = (p, cb) =>
  cb(
    null,
    Object.keys(globDir).map(file => join(globRoot, file))
  )

const HelpSearch = t.mock('../../../lib/commands/help-search.js', {
  glob,
})
const helpSearch = new HelpSearch(npm)

t.test('npm help-search', async t => {
  globRoot = t.testdir(globDir)
  t.teardown(() => {
    OUTPUT.length = 0
    globRoot = null
  })

  await helpSearch.exec(['exec'])

  t.match(OUTPUT, /Top hits for "exec"/, 'outputs results')
})

t.test('npm help-search multiple terms', async t => {
  globRoot = t.testdir(globDir)
  t.teardown(() => {
    OUTPUT.length = 0
    globRoot = null
  })

  await helpSearch.exec(['run', 'script'])

  t.match(OUTPUT, /Top hits for/, 'outputs results')
  t.match(OUTPUT, /run:\d+ script:\d+/, 'shows hit counts for both terms')
})

t.test('npm help-search long output', async t => {
  globRoot = t.testdir(globDir)
  config.long = true
  t.teardown(() => {
    OUTPUT.length = 0
    config.long = false
    globRoot = null
  })

  await helpSearch.exec(['exec'])

  t.match(OUTPUT, /has multiple lines of exec help/, 'outputs detailed results')
})

t.test('npm help-search long output with color', async t => {
  globRoot = t.testdir(globDir)
  config.long = true
  npm.color = true
  t.teardown(() => {
    OUTPUT.length = 0
    config.long = false
    npm.color = false
    globRoot = null
  })

  await helpSearch.exec(['help-search'])

  const highlightedText = chalk.bgBlack.red('help-search')
  t.equal(
    OUTPUT.some(line => line.includes(highlightedText)),
    true,
    'returned highlighted search terms'
  )
})

t.test('npm help-search no args', async t => {
  t.rejects(helpSearch.exec([]), /npm help-search/, 'outputs usage')
})

t.test('npm help-search no matches', async t => {
  globRoot = t.testdir(globDir)
  t.teardown(() => {
    OUTPUT.length = 0
    globRoot = null
  })

  await helpSearch.exec(['asdfasdf'])
  t.match(OUTPUT, /No matches/)
})

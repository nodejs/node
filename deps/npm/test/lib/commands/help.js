const t = require('tap')
const localeCompare = require('@isaacs/string-locale-compare')('en')
const { load: loadMockNpm } = require('../../fixtures/mock-npm.js')
const { cleanCwd } = require('../../fixtures/clean-snapshot')

const genManPages = (obj) => {
  const man = {}
  const resPages = new Set()

  for (const [section, pages] of Object.entries(obj)) {
    const num = parseInt(section, 10)
    man[`man${num}`] = {}

    const sectionPages = []
    for (const name of pages) {
      man[`man${num}`][`${name}.${section}`] = `.TH "${name.toUpperCase()}" "${num}"`
      sectionPages.push(name.replace(/^npm-/, ''))
    }

    // return a sorted list of uniq pages in order to test completion
    for (const p of sectionPages.sort(localeCompare)) {
      resPages.add(p)
    }
  }

  // man directory name is hardcoded in the command
  return { fixtures: { man }, pages: [...resPages.values()] }
}

const mockHelp = async (t, {
  man = {
    5: ['npmrc', 'install', 'package-json'],
    1: ['whoami', 'install', 'star', 'unstar', 'uninstall', 'unpublish'].map(p => `npm-${p}`),
    7: ['disputes', 'config'],
  },
  browser = false,
  woman = false,
  exec: execArgs = null,
  spawnErr,
  ...opts
} = {}) => {
  const config = {
    // always set viewer to test the same on all platforms
    viewer: browser ? 'browser' : woman ? 'woman' : 'man',
    ...opts.config,
  }

  let args = null
  const mockSpawn = async (...a) => {
    args = a
    if (spawnErr) {
      throw spawnErr
    }
  }
  mockSpawn.open = async (url) => args = [cleanCwd(decodeURI(url))]

  const manPages = genManPages(man)

  const { npm, ...rest } = await loadMockNpm(t, {
    npm: ({ other }) => ({ npmRoot: other }),
    mocks: { '@npmcli/promise-spawn': mockSpawn },
    otherDirs: { ...manPages.fixtures },
    config,
    ...opts,
  })

  const help = await npm.cmd('help')
  const exec = execArgs
    ? await npm.exec('help', execArgs)
    : (...a) => npm.exec('help', a)

  return {
    npm,
    help,
    exec,
    manPages: manPages.pages,
    getArgs: () => args,
    ...rest,
  }
}

t.test('npm help', async t => {
  const { exec, joinedOutput } = await mockHelp(t)
  await exec()

  t.match(joinedOutput(), 'npm <command>', 'showed npm usage')
})

t.test('npm help completion', async t => {
  const { help, manPages } = await mockHelp(t)

  const noArgs = await help.completion({ conf: { argv: { remain: [] } } })
  t.strictSame(noArgs, ['help', ...manPages], 'outputs available help pages')
  const threeArgs = await help.completion({ conf: { argv: { remain: ['one', 'two', 'three'] } } })
  t.strictSame(threeArgs, [], 'outputs no results when more than 2 args are provided')
})

t.test('npm help multiple args calls search', async t => {
  const { joinedOutput } = await mockHelp(t, { exec: ['run', 'script'] })

  t.match(joinedOutput(), 'No matches in help for: run script', 'calls help-search')
})

t.test('npm help no matches calls search', async t => {
  const { joinedOutput } = await mockHelp(t, { exec: ['asdfasdf'] })

  t.match(joinedOutput(), 'No matches in help for: asdfasdf', 'passed the args to help-search')
})

t.test('npm help whoami', async t => {
  const { getArgs } = await mockHelp(t, { exec: ['whoami'] })

  const [spawnBin, spawnArgs] = getArgs()
  t.equal(spawnBin, 'man', 'calls man by default')
  t.equal(spawnArgs.length, 1)
  t.match(spawnArgs[0], /npm-whoami\.1$/)
})

t.test('npm help 1 install', async t => {
  const { getArgs } = await mockHelp(t, {
    exec: ['1', 'install'],
    browser: true,
  })

  const [url] = getArgs()
  t.match(url, /commands\/npm-install.html$/, 'attempts to open the correct url')
  t.ok(url.startsWith('file:///'), 'opens with the correct uri schema')
})

t.test('npm help 5 install', async t => {
  const { getArgs } = await mockHelp(t, {
    exec: ['5', 'install'],
    browser: true,
  })

  const [url] = getArgs()
  t.match(url, /configuring-npm\/install.html$/, 'attempts to open the correct url')
})

t.test('npm help 7 config', async t => {
  const { getArgs } = await mockHelp(t, {
    exec: ['7', 'config'],
    browser: true,
  })

  const [url] = getArgs()
  t.match(url, /using-npm\/config.html$/, 'attempts to open the correct url')
})

t.test('npm help package.json redirects to package-json', async t => {
  const { getArgs } = await mockHelp(t, {
    exec: ['package.json'],
  })

  const [spawnBin, spawnArgs] = getArgs()
  t.equal(spawnBin, 'man', 'calls man by default')
  t.equal(spawnArgs.length, 1)
  t.match(spawnArgs[0], /package-json\.5$/)
})

t.test('npm help ?(un)star', async t => {
  const { getArgs } = await mockHelp(t, {
    exec: ['?(un)star'],
    woman: true,
  })

  const [spawnBin, spawnArgs] = getArgs()
  t.equal(spawnBin, 'emacsclient', 'maps woman to emacs correctly')
  t.equal(spawnArgs.length, 2)
  t.match(spawnArgs[1], /^\(woman-find-file '/)
  t.match(spawnArgs[1], /npm-star.1'\)$/)
})

t.test('npm help un*', async t => {
  const { getArgs } = await mockHelp(t, {
    exec: ['un*'],
  })

  const [spawnBin, spawnArgs] = getArgs()
  t.equal(spawnBin, 'man', 'calls man by default')
  t.equal(spawnArgs.length, 1)
  t.match(spawnArgs[0], /npm-uninstall\.1$/)
})

t.test('npm help - prefers lowest numbered npm prefixed help pages', async t => {
  const { getArgs } = await mockHelp(t, {
    man: {
      6: ['npm-install'],
      1: ['npm-install'],
      5: ['install'],
      7: ['npm-install'],
    },
    exec: ['install'],
  })

  const [spawnBin, spawnArgs] = getArgs()
  t.equal(spawnBin, 'man', 'calls man by default')
  t.equal(spawnArgs.length, 1)
  t.match(spawnArgs[0], /npm-install\.1$/)
})

t.test('npm help - works in the presence of strange man pages', async t => {
  const { getArgs } = await mockHelp(t, {
    man: {
      '1strange': ['config'],
      5: ['config'],
      '6ssl': ['config'],
    },
    exec: ['config'],
  })

  const [spawnBin, spawnArgs] = getArgs()
  t.equal(spawnBin, 'man', 'calls man by default')
  t.equal(spawnArgs.length, 1)
  t.match(spawnArgs[0], /config\.5$/)
})

t.test('rejects with code', async t => {
  const { exec } = await mockHelp(t, {
    spawnErr: Object.assign(new Error('errrrr'), { code: 'SPAWN_ERR' }),
  })

  await t.rejects(exec('whoami'), /help process exited with code: SPAWN_ERR/)
})

t.test('rejects with no code', async t => {
  const { exec } = await mockHelp(t, {
    spawnErr: new Error('errrrr'),
  })

  await t.rejects(exec('whoami'), /errrrr/)
})

const t = require('tap')
const { resolve } = require('path')
const mockGlobals = require('@npmcli/mock-globals')
const tmock = require('../../../fixtures/tmock')
const pkg = require('../../../../package.json')

// have to fake the node version, or else it'll only pass on this one
mockGlobals(t, { 'process.version': 'v14.8.0', 'process.env.NODE_ENV': undefined })

const mockDefs = (mocks = {}) => tmock(t, '{LIB}/utils/config/definitions.js', mocks)

const isWin = (isWindows) => ({ '{LIB}/utils/is-windows.js': { isWindows } })

t.test('basic flattening function camelCases from css-case', t => {
  const flat = {}
  const obj = { 'prefer-online': true }
  const definitions = mockDefs()
  definitions['prefer-online'].flatten('prefer-online', obj, flat)
  t.strictSame(flat, { preferOnline: true })
  t.end()
})

t.test('editor', t => {
  t.test('has EDITOR and VISUAL, use EDITOR', t => {
    mockGlobals(t, { 'process.env': { EDITOR: 'vim', VISUAL: 'mate' } })
    const defs = mockDefs()
    t.equal(defs.editor.default, 'vim')
    t.end()
  })
  t.test('has VISUAL but no EDITOR, use VISUAL', t => {
    mockGlobals(t, { 'process.env': { EDITOR: undefined, VISUAL: 'mate' } })
    const defs = mockDefs()
    t.equal(defs.editor.default, 'mate')
    t.end()
  })
  t.test('has neither EDITOR nor VISUAL, system specific', t => {
    mockGlobals(t, {
      'process.env': {
        EDITOR: undefined,
        VISUAL: undefined,
        SYSTEMROOT: 'C:\\Windows',
      },
    })
    const defsWin = mockDefs(isWin(true))
    t.equal(defsWin.editor.default, 'C:\\Windows\\notepad.exe')
    const defsNix = mockDefs(isWin(false))
    t.equal(defsNix.editor.default, 'vi')
    t.end()
  })
  t.end()
})

t.test('shell', t => {
  t.test('windows, env.ComSpec then cmd.exe', t => {
    mockGlobals(t, { 'process.env.ComSpec': 'command.com' })
    const defsComSpec = mockDefs(isWin(true))
    t.equal(defsComSpec.shell.default, 'command.com')
    mockGlobals(t, { 'process.env.ComSpec': undefined })
    const defsNoComSpec = mockDefs(isWin(true))
    t.equal(defsNoComSpec.shell.default, 'cmd')
    t.end()
  })

  t.test('nix, SHELL then sh', t => {
    mockGlobals(t, { 'process.env.SHELL': '/usr/local/bin/bash' })
    const defsShell = mockDefs(isWin(false))
    t.equal(defsShell.shell.default, '/usr/local/bin/bash')
    mockGlobals(t, { 'process.env.SHELL': undefined })
    const defsNoShell = mockDefs(isWin(false))
    t.equal(defsNoShell.shell.default, 'sh')
    t.end()
  })

  t.end()
})

t.test('local-address allowed types', t => {
  t.test('get list from os.networkInterfaces', t => {
    const os = {
      tmpdir: () => '/tmp',
      networkInterfaces: () => ({
        eth420: [{ address: '127.0.0.1' }],
        eth69: [{ address: 'no place like home' }],
      }),
    }
    const defs = mockDefs({ os })
    t.same(defs['local-address'].type, [
      null,
      '127.0.0.1',
      'no place like home',
    ])
    t.end()
  })
  t.test('handle os.networkInterfaces throwing', t => {
    const os = {
      tmpdir: () => '/tmp',
      networkInterfaces: () => {
        throw new Error('no network interfaces for some reason')
      },
    }
    const defs = mockDefs({ os })
    t.same(defs['local-address'].type, [null])
    t.end()
  })
  t.end()
})

t.test('unicode allowed?', t => {
  const setGlobal = (obj = {}) => mockGlobals(t, { 'process.env': obj })

  setGlobal({ LC_ALL: 'utf8', LC_CTYPE: 'UTF-8', LANG: 'Unicode utf-8' })

  const lcAll = mockDefs()
  t.equal(lcAll.unicode.default, true)
  setGlobal({ LC_ALL: 'no unicode for youUUUU!' })
  const noLcAll = mockDefs()
  t.equal(noLcAll.unicode.default, false)

  setGlobal({ LC_ALL: undefined })
  const lcCtype = mockDefs()
  t.equal(lcCtype.unicode.default, true)
  setGlobal({ LC_CTYPE: 'something other than unicode version 8' })
  const noLcCtype = mockDefs()
  t.equal(noLcCtype.unicode.default, false)

  setGlobal({ LC_CTYPE: undefined })
  const lang = mockDefs()
  t.equal(lang.unicode.default, true)
  setGlobal({ LANG: 'ISO-8859-1' })
  const noLang = mockDefs()
  t.equal(noLang.unicode.default, false)
  t.end()
})

t.test('cache', t => {
  mockGlobals(t, { 'process.env.LOCALAPPDATA': 'app/data/local' })
  const defsWinLocalAppData = mockDefs(isWin(true))
  t.equal(defsWinLocalAppData.cache.default, 'app/data/local/npm-cache')

  mockGlobals(t, { 'process.env.LOCALAPPDATA': undefined })
  const defsWinNoLocalAppData = mockDefs(isWin(true))
  t.equal(defsWinNoLocalAppData.cache.default, '~/npm-cache')

  const defsNix = mockDefs(isWin(false))
  t.equal(defsNix.cache.default, '~/.npm')

  const flat = {}
  defsNix.cache.flatten('cache', { cache: '/some/cache/value' }, flat)
  const { join } = require('path')
  t.equal(flat.cache, join('/some/cache/value', '_cacache'))
  t.equal(flat.npxCache, join('/some/cache/value', '_npx'))

  t.end()
})

t.test('flatteners that populate flat.omit array', t => {
  t.test('also', t => {
    const flat = {}
    const obj = {}

    // ignored if setting is not dev or development
    obj.also = 'ignored'
    mockDefs().also.flatten('also', obj, flat)
    t.strictSame(obj, { also: 'ignored', omit: [], include: [] }, 'nothing done')
    t.strictSame(flat, { omit: [] }, 'nothing done')

    obj.also = 'development'
    mockDefs().also.flatten('also', obj, flat)
    t.strictSame(obj, {
      also: 'development',
      omit: [],
      include: ['dev'],
    }, 'marked dev as included')
    t.strictSame(flat, { omit: [] }, 'nothing omitted, so nothing changed')

    obj.omit = ['dev', 'optional']
    obj.include = []
    mockDefs().also.flatten('also', obj, flat)
    t.strictSame(obj, {
      also: 'development',
      omit: ['optional'],
      include: ['dev'],
    }, 'marked dev as included')
    t.strictSame(flat, { omit: ['optional'] }, 'removed dev from omit')
    t.end()
  })

  t.test('include', t => {
    const flat = {}
    const obj = { include: ['dev'] }
    mockDefs().include.flatten('include', obj, flat)
    t.strictSame(flat, { omit: [] }, 'not omitting anything')
    obj.omit = ['optional', 'dev']
    mockDefs().include.flatten('include', obj, flat)
    t.strictSame(flat, { omit: ['optional'] }, 'only omitting optional')
    t.end()
  })

  t.test('omit', t => {
    const flat = {}
    const obj = { include: ['dev'], omit: ['dev', 'optional'] }
    mockDefs().omit.flatten('omit', obj, flat)
    t.strictSame(flat, { omit: ['optional'] }, 'do not omit what is included')

    mockGlobals(t, { 'process.env.NODE_ENV': 'production' })
    const defProdEnv = mockDefs()
    t.strictSame(defProdEnv.omit.default, ['dev'], 'omit dev in production')
    t.end()
  })

  t.test('only', t => {
    const flat = {}
    const obj = { only: 'asdf' }
    mockDefs().only.flatten('only', obj, flat)
    t.strictSame(flat, { omit: [] }, 'ignored if value is not production')

    obj.only = 'prod'
    mockDefs().only.flatten('only', obj, flat)
    t.strictSame(flat, { omit: ['dev'] }, 'omit dev when --only=prod')

    obj.include = ['dev']
    flat.omit = []
    mockDefs().only.flatten('only', obj, flat)
    t.strictSame(flat, { omit: [] }, 'do not omit when included')

    t.end()
  })

  t.test('optional', t => {
    const flat = {}
    const obj = { optional: null }

    mockDefs().optional.flatten('optional', obj, flat)
    t.strictSame(obj, {
      optional: null,
      omit: [],
      include: [],
    }, 'do nothing by default')
    t.strictSame(flat, { omit: [] }, 'do nothing by default')

    obj.optional = true
    mockDefs().optional.flatten('optional', obj, flat)
    t.strictSame(obj, {
      omit: [],
      optional: true,
      include: ['optional'],
    }, 'include optional when set')
    t.strictSame(flat, { omit: [] }, 'nothing to omit in flatOptions')

    delete obj.include
    obj.optional = false
    mockDefs().optional.flatten('optional', obj, flat)
    t.strictSame(obj, {
      omit: ['optional'],
      optional: false,
      include: [],
    }, 'omit optional when set false')
    t.strictSame(flat, { omit: ['optional'] }, 'omit optional when set false')

    t.end()
  })

  t.test('production', t => {
    const flat = {}
    const obj = { production: true }
    mockDefs().production.flatten('production', obj, flat)
    t.strictSame(obj, {
      production: true,
      omit: ['dev'],
      include: [],
    }, '--production sets --omit=dev')
    t.strictSame(flat, { omit: ['dev'] }, '--production sets --omit=dev')

    delete obj.omit
    obj.production = false
    delete flat.omit
    mockDefs().production.flatten('production', obj, flat)
    t.strictSame(obj, {
      production: false,
      include: ['dev'],
      omit: [],
    }, '--no-production explicitly includes dev')
    t.strictSame(flat, { omit: [] }, '--no-production has no effect')

    obj.production = true
    obj.include = ['dev']
    mockDefs().production.flatten('production', obj, flat)
    t.strictSame(obj, {
      production: true,
      include: ['dev'],
      omit: [],
    }, 'omit and include dev')
    t.strictSame(flat, { omit: [] }, 'do not omit dev when included')

    t.end()
  })

  t.test('dev', t => {
    const flat = {}
    const obj = { dev: true }
    mockDefs().dev.flatten('dev', obj, flat)
    t.strictSame(obj, {
      dev: true,
      omit: [],
      include: ['dev'],
    })
    t.end()
  })

  t.end()
})

t.test('cache-max', t => {
  const flat = {}
  const obj = { 'cache-max': 10342 }
  mockDefs()['cache-max'].flatten('cache-max', obj, flat)
  t.strictSame(flat, {}, 'no effect if not <= 0')
  obj['cache-max'] = 0
  mockDefs()['cache-max'].flatten('cache-max', obj, flat)
  t.strictSame(flat, { preferOnline: true }, 'preferOnline if <= 0')
  t.end()
})

t.test('cache-min', t => {
  const flat = {}
  const obj = { 'cache-min': 123 }
  mockDefs()['cache-min'].flatten('cache-min', obj, flat)
  t.strictSame(flat, {}, 'no effect if not >= 9999')
  obj['cache-min'] = 9999
  mockDefs()['cache-min'].flatten('cache-min', obj, flat)
  t.strictSame(flat, { preferOffline: true }, 'preferOffline if >=9999')
  t.end()
})

t.test('color', t => {
  const setTTY = (stream, value) => mockGlobals(t, { [`process.${stream}.isTTY`]: value })

  const flat = {}
  const obj = { color: 'always' }

  mockDefs().color.flatten('color', obj, flat)
  t.strictSame(flat, { color: true, logColor: true }, 'true when --color=always')

  obj.color = false
  mockDefs().color.flatten('color', obj, flat)
  t.strictSame(flat, { color: false, logColor: false }, 'true when --no-color')

  setTTY('stdout', false)
  setTTY('stderr', false)

  obj.color = true
  mockDefs().color.flatten('color', obj, flat)
  t.strictSame(flat, { color: false, logColor: false }, 'no color when stdout not tty')
  setTTY('stdout', true)
  mockDefs().color.flatten('color', obj, flat)
  t.strictSame(flat, { color: true, logColor: false }, '--color turns on color when stdout is tty')
  setTTY('stdout', false)

  obj.color = true
  mockDefs().color.flatten('color', obj, flat)
  t.strictSame(flat, { color: false, logColor: false }, 'no color when stderr not tty')
  setTTY('stderr', true)
  mockDefs().color.flatten('color', obj, flat)
  t.strictSame(flat, { color: false, logColor: true }, '--color turns on color when stderr is tty')
  setTTY('stderr', false)

  const setColor = (value) => mockGlobals(t, { 'process.env.NO_COLOR': value })

  setColor(undefined)
  const defsAllowColor = mockDefs()
  t.equal(defsAllowColor.color.default, true, 'default true when no NO_COLOR env')

  setColor('0')
  const defsNoColor0 = mockDefs()
  t.equal(defsNoColor0.color.default, true, 'default true when no NO_COLOR=0')

  setColor('1')
  const defsNoColor1 = mockDefs()
  t.equal(defsNoColor1.color.default, false, 'default false when no NO_COLOR=1')

  t.end()
})

t.test('progress', t => {
  const setEnv = ({ tty, term } = {}) => mockGlobals(t, {
    'process.stderr.isTTY': tty,
    'process.env.TERM': term,
  })

  const flat = {}

  mockDefs().progress.flatten('progress', {}, flat)
  t.strictSame(flat, { progress: false })

  setEnv({ tty: true, term: 'notdumb' })
  mockDefs().progress.flatten('progress', { progress: true }, flat)
  t.strictSame(flat, { progress: true })

  setEnv({ tty: false, term: 'notdumb' })
  mockDefs().progress.flatten('progress', { progress: true }, flat)
  t.strictSame(flat, { progress: false })

  setEnv({ tty: true, term: 'dumb' })
  mockDefs().progress.flatten('progress', { progress: true }, flat)
  t.strictSame(flat, { progress: false })

  t.end()
})

t.test('retry options', t => {
  const obj = {}
  // <config>: flat.retry[<option>]
  const mapping = {
    'fetch-retries': 'retries',
    'fetch-retry-factor': 'factor',
    'fetch-retry-maxtimeout': 'maxTimeout',
    'fetch-retry-mintimeout': 'minTimeout',
  }
  for (const [config, option] of Object.entries(mapping)) {
    const msg = `${config} -> retry.${option}`
    const flat = {}
    obj[config] = 99
    mockDefs()[config].flatten(config, obj, flat)
    t.strictSame(flat, { retry: { [option]: 99 } }, msg)
    delete obj[config]
  }
  t.end()
})

t.test('search options', t => {
  const vals = {
    description: 'test description',
    exclude: 'test search exclude',
    limit: 99,
    staleneess: 99,

  }
  const obj = {}
  // <config>: flat.search[<option>]
  const mapping = {
    description: 'description',
    searchexclude: 'exclude',
    searchlimit: 'limit',
    searchstaleness: 'staleness',
  }

  for (const [config, option] of Object.entries(mapping)) {
    const msg = `${config} -> search.${option}`
    const flat = {}
    obj[config] = vals[option]
    mockDefs()[config].flatten(config, obj, flat)
    t.strictSame(flat, { search: { limit: 20, [option]: vals[option] } }, msg)
    delete obj[config]
  }

  const flat = {}
  obj.searchopts = 'a=b&b=c'
  mockDefs().searchopts.flatten('searchopts', obj, flat)
  t.strictSame(flat, {
    search: {
      limit: 20,
      opts: Object.assign(Object.create(null), {
        a: 'b',
        b: 'c',
      }),
    },
  }, 'searchopts -> querystring.parse() -> search.opts')
  delete obj.searchopts

  t.end()
})

t.test('noProxy - array', t => {
  const obj = { noproxy: ['1.2.3.4,2.3.4.5', '3.4.5.6'] }
  const flat = {}
  mockDefs().noproxy.flatten('noproxy', obj, flat)
  t.strictSame(flat, { noProxy: '1.2.3.4,2.3.4.5,3.4.5.6' })
  t.end()
})

t.test('noProxy - string', t => {
  const obj = { noproxy: '1.2.3.4,2.3.4.5,3.4.5.6' }
  const flat = {}
  mockDefs().noproxy.flatten('noproxy', obj, flat)
  t.strictSame(flat, { noProxy: '1.2.3.4,2.3.4.5,3.4.5.6' })
  t.end()
})

t.test('maxSockets', t => {
  const obj = { maxsockets: 123 }
  const flat = {}
  mockDefs().maxsockets.flatten('maxsockets', obj, flat)
  t.strictSame(flat, { maxSockets: 123 })
  t.end()
})

t.test('scope', t => {
  const obj = { scope: 'asdf' }
  const flat = {}
  mockDefs().scope.flatten('scope', obj, flat)
  t.strictSame(flat, { scope: '@asdf', projectScope: '@asdf' }, 'prepend @ if needed')

  obj.scope = '@asdf'
  mockDefs().scope.flatten('scope', obj, flat)
  t.strictSame(flat, { scope: '@asdf', projectScope: '@asdf' }, 'leave untouched if has @')

  t.end()
})

t.test('strictSSL', t => {
  const obj = { 'strict-ssl': false }
  const flat = {}
  mockDefs()['strict-ssl'].flatten('strict-ssl', obj, flat)
  t.strictSame(flat, { strictSSL: false })
  obj['strict-ssl'] = true
  mockDefs()['strict-ssl'].flatten('strict-ssl', obj, flat)
  t.strictSame(flat, { strictSSL: true })
  t.end()
})

t.test('shrinkwrap/package-lock', t => {
  const obj = { shrinkwrap: false }
  const flat = {}
  mockDefs().shrinkwrap.flatten('shrinkwrap', obj, flat)
  t.strictSame(flat, { packageLock: false })
  obj.shrinkwrap = true
  mockDefs().shrinkwrap.flatten('shrinkwrap', obj, flat)
  t.strictSame(flat, { packageLock: true })

  delete obj.shrinkwrap
  obj['package-lock'] = false
  mockDefs()['package-lock'].flatten('package-lock', obj, flat)
  t.strictSame(flat, { packageLock: false })
  obj['package-lock'] = true
  mockDefs()['package-lock'].flatten('package-lock', obj, flat)
  t.strictSame(flat, { packageLock: true })

  t.end()
})

t.test('scriptShell', t => {
  const obj = { 'script-shell': null }
  const flat = {}
  mockDefs()['script-shell'].flatten('script-shell', obj, flat)
  t.ok(Object.prototype.hasOwnProperty.call(flat, 'scriptShell'),
    'should set it to undefined explicitly')
  t.strictSame(flat, { scriptShell: undefined }, 'no other fields')

  obj['script-shell'] = 'asdf'
  mockDefs()['script-shell'].flatten('script-shell', obj, flat)
  t.strictSame(flat, { scriptShell: 'asdf' }, 'sets if not falsey')

  t.end()
})

t.test('defaultTag', t => {
  const obj = { tag: 'next' }
  const flat = {}
  mockDefs().tag.flatten('tag', obj, flat)
  t.strictSame(flat, { defaultTag: 'next' })
  t.end()
})

t.test('timeout', t => {
  const obj = { 'fetch-timeout': 123 }
  const flat = {}
  mockDefs()['fetch-timeout'].flatten('fetch-timeout', obj, flat)
  t.strictSame(flat, { timeout: 123 })
  t.end()
})

t.test('saveType', t => {
  t.test('save-prod', t => {
    const obj = { 'save-prod': false }
    const flat = {}
    mockDefs()['save-prod'].flatten('save-prod', obj, flat)
    t.strictSame(flat, {}, 'no effect if false and missing')
    flat.saveType = 'prod'
    mockDefs()['save-prod'].flatten('save-prod', obj, flat)
    t.strictSame(flat, {}, 'remove if false and set to prod')
    flat.saveType = 'dev'
    mockDefs()['save-prod'].flatten('save-prod', obj, flat)
    t.strictSame(flat, { saveType: 'dev' }, 'ignore if false and not already prod')
    obj['save-prod'] = true
    mockDefs()['save-prod'].flatten('save-prod', obj, flat)
    t.strictSame(flat, { saveType: 'prod' }, 'set to prod if true')
    t.end()
  })

  t.test('save-dev', t => {
    const obj = { 'save-dev': false }
    const flat = {}
    mockDefs()['save-dev'].flatten('save-dev', obj, flat)
    t.strictSame(flat, {}, 'no effect if false and missing')
    flat.saveType = 'dev'
    mockDefs()['save-dev'].flatten('save-dev', obj, flat)
    t.strictSame(flat, {}, 'remove if false and set to dev')
    flat.saveType = 'prod'
    obj['save-dev'] = false
    mockDefs()['save-dev'].flatten('save-dev', obj, flat)
    t.strictSame(flat, { saveType: 'prod' }, 'ignore if false and not already dev')
    obj['save-dev'] = true
    mockDefs()['save-dev'].flatten('save-dev', obj, flat)
    t.strictSame(flat, { saveType: 'dev' }, 'set to dev if true')
    t.end()
  })

  t.test('save-bundle', t => {
    const obj = { 'save-bundle': true }
    const flat = {}
    mockDefs()['save-bundle'].flatten('save-bundle', obj, flat)
    t.strictSame(flat, { saveBundle: true }, 'set the saveBundle flag')

    obj['save-bundle'] = false
    mockDefs()['save-bundle'].flatten('save-bundle', obj, flat)
    t.strictSame(flat, { saveBundle: false }, 'unset the saveBundle flag')

    obj['save-bundle'] = true
    obj['save-peer'] = true
    mockDefs()['save-bundle'].flatten('save-bundle', obj, flat)
    t.strictSame(flat, { saveBundle: false }, 'false if save-peer is set')

    t.end()
  })

  t.test('save-peer', t => {
    const obj = { 'save-peer': false }
    const flat = {}
    mockDefs()['save-peer'].flatten('save-peer', obj, flat)
    t.strictSame(flat, {}, 'no effect if false and not yet set')

    obj['save-peer'] = true
    mockDefs()['save-peer'].flatten('save-peer', obj, flat)
    t.strictSame(flat, { saveType: 'peer' }, 'set saveType to peer if unset')

    flat.saveType = 'optional'
    mockDefs()['save-peer'].flatten('save-peer', obj, flat)
    t.strictSame(flat, { saveType: 'peerOptional' }, 'set to peerOptional if optional already')

    mockDefs()['save-peer'].flatten('save-peer', obj, flat)
    t.strictSame(flat, { saveType: 'peerOptional' }, 'no effect if already peerOptional')

    obj['save-peer'] = false
    mockDefs()['save-peer'].flatten('save-peer', obj, flat)
    t.strictSame(flat, { saveType: 'optional' }, 'switch peerOptional to optional if false')

    obj['save-peer'] = false
    flat.saveType = 'peer'
    mockDefs()['save-peer'].flatten('save-peer', obj, flat)
    t.strictSame(flat, {}, 'remove saveType if peer and setting false')

    t.end()
  })

  t.test('save-optional', t => {
    const obj = { 'save-optional': false }
    const flat = {}
    mockDefs()['save-optional'].flatten('save-optional', obj, flat)
    t.strictSame(flat, {}, 'no effect if false and not yet set')

    obj['save-optional'] = true
    mockDefs()['save-optional'].flatten('save-optional', obj, flat)
    t.strictSame(flat, { saveType: 'optional' }, 'set saveType to optional if unset')

    flat.saveType = 'peer'
    mockDefs()['save-optional'].flatten('save-optional', obj, flat)
    t.strictSame(flat, { saveType: 'peerOptional' }, 'set to peerOptional if peer already')

    mockDefs()['save-optional'].flatten('save-optional', obj, flat)
    t.strictSame(flat, { saveType: 'peerOptional' }, 'no effect if already peerOptional')

    obj['save-optional'] = false
    mockDefs()['save-optional'].flatten('save-optional', obj, flat)
    t.strictSame(flat, { saveType: 'peer' }, 'switch peerOptional to peer if false')

    flat.saveType = 'optional'
    mockDefs()['save-optional'].flatten('save-optional', obj, flat)
    t.strictSame(flat, {}, 'remove saveType if optional and setting false')

    t.end()
  })

  t.end()
})

t.test('cafile -> flat.ca', t => {
  const path = t.testdir({
    cafile: `
-----BEGIN CERTIFICATE-----
XXXX
XXXX
-----END CERTIFICATE-----
-----BEGIN CERTIFICATE-----
YYYY\r
YYYY\r
-----END CERTIFICATE-----
`,
  })
  const cafile = resolve(path, 'cafile')

  const obj = {}
  const flat = {}
  mockDefs().cafile.flatten('cafile', obj, flat)
  t.strictSame(flat, {}, 'no effect if no cafile set')
  obj.cafile = resolve(path, 'no/cafile/here')
  mockDefs().cafile.flatten('cafile', obj, flat)
  t.strictSame(flat, {}, 'no effect if cafile not found')
  obj.cafile = cafile
  mockDefs().cafile.flatten('cafile', obj, flat)
  t.strictSame(flat, {
    ca: [
      '-----BEGIN CERTIFICATE-----\nXXXX\nXXXX\n-----END CERTIFICATE-----',
      '-----BEGIN CERTIFICATE-----\nYYYY\nYYYY\n-----END CERTIFICATE-----',
    ],
  })
  t.test('error other than ENOENT gets thrown', t => {
    const poo = new Error('poo')
    const defnReadFileThrows = mockDefs({
      fs: {
        ...require('fs'),
        readFileSync: () => {
          throw poo
        },
      },
    })
    t.throws(() => defnReadFileThrows.cafile.flatten('cafile', obj, {}), poo)
    t.end()
  })

  t.end()
})

t.test('detect CI', t => {
  const defnNoCI = mockDefs({
    'ci-info': { isCI: false, name: null },
  })
  const defnCIFoo = mockDefs({
    'ci-info': { isCI: false, name: 'foo' },
  })
  t.equal(defnNoCI['ci-name'].default, null, 'null when not in CI')
  t.equal(defnCIFoo['ci-name'].default, 'foo', 'name of CI when in CI')
  t.end()
})

t.test('user-agent', t => {
  const obj = {
    'user-agent': mockDefs()['user-agent'].default,
  }
  const flat = {}
  const expectNoCI = `npm/${pkg.version} node/${process.version} ` +
    `${process.platform} ${process.arch} workspaces/false`
  mockDefs()['user-agent'].flatten('user-agent', obj, flat)
  t.equal(flat.userAgent, expectNoCI)
  t.equal(process.env.npm_config_user_agent, flat.userAgent, 'npm_user_config environment is set')
  t.not(obj['user-agent'], flat.userAgent, 'config user-agent template is not translated')

  obj['ci-name'] = 'foo'
  obj['user-agent'] = mockDefs()['user-agent'].default
  const expectCI = `${expectNoCI} ci/foo`
  mockDefs()['user-agent'].flatten('user-agent', obj, flat)
  t.equal(flat.userAgent, expectCI)
  t.equal(process.env.npm_config_user_agent, flat.userAgent, 'npm_user_config environment is set')
  t.not(obj['user-agent'], flat.userAgent, 'config user-agent template is not translated')

  delete obj['ci-name']
  obj.workspaces = true
  obj['user-agent'] = mockDefs()['user-agent'].default
  const expectWorkspaces = expectNoCI.replace('workspaces/false', 'workspaces/true')
  mockDefs()['user-agent'].flatten('user-agent', obj, flat)
  t.equal(flat.userAgent, expectWorkspaces)
  t.equal(process.env.npm_config_user_agent, flat.userAgent, 'npm_user_config environment is set')
  t.not(obj['user-agent'], flat.userAgent, 'config user-agent template is not translated')

  delete obj.workspaces
  obj.workspace = ['foo']
  obj['user-agent'] = mockDefs()['user-agent'].default
  mockDefs()['user-agent'].flatten('user-agent', obj, flat)
  t.equal(flat.userAgent, expectWorkspaces)
  t.equal(process.env.npm_config_user_agent, flat.userAgent, 'npm_user_config environment is set')
  t.not(obj['user-agent'], flat.userAgent, 'config user-agent template is not translated')
  t.end()
})

t.test('save-prefix', t => {
  const obj = {
    'save-exact': true,
    'save-prefix': '~1.2.3',
  }
  const flat = {}
  mockDefs()['save-prefix']
    .flatten('save-prefix', { ...obj, 'save-exact': true }, flat)
  t.strictSame(flat, { savePrefix: '' })
  mockDefs()['save-prefix']
    .flatten('save-prefix', { ...obj, 'save-exact': false }, flat)
  t.strictSame(flat, { savePrefix: '~1.2.3' })
  t.end()
})

t.test('save-exact', t => {
  const obj = {
    'save-exact': true,
    'save-prefix': '~1.2.3',
  }
  const flat = {}
  mockDefs()['save-exact']
    .flatten('save-exact', { ...obj, 'save-exact': true }, flat)
  t.strictSame(flat, { savePrefix: '' })
  mockDefs()['save-exact']
    .flatten('save-exact', { ...obj, 'save-exact': false }, flat)
  t.strictSame(flat, { savePrefix: '~1.2.3' })
  t.end()
})

t.test('location', t => {
  const obj = {
    global: true,
    location: 'user',
  }
  const flat = {}
  // the global flattener is what sets location, so run that
  mockDefs().global.flatten('global', obj, flat)
  mockDefs().location.flatten('location', obj, flat)
  // global = true sets location in both places to global
  t.strictSame(flat, { global: true, location: 'global' })
  // location here is still 'user' because flattening doesn't modify the object
  t.strictSame(obj, { global: true, location: 'user' })

  obj.global = false
  obj.location = 'user'
  delete flat.global
  delete flat.location

  mockDefs().global.flatten('global', obj, flat)
  mockDefs().location.flatten('location', obj, flat)
  // global = false leaves location unaltered
  t.strictSame(flat, { global: false, location: 'user' })
  t.strictSame(obj, { global: false, location: 'user' })
  t.end()
})

t.test('package-lock-only', t => {
  const obj = {
    'package-lock': false,
    'package-lock-only': true,
  }
  const flat = {}

  mockDefs()['package-lock-only'].flatten('package-lock-only', obj, flat)
  mockDefs()['package-lock'].flatten('package-lock', obj, flat)
  t.strictSame(flat, { packageLock: true, packageLockOnly: true })

  obj['package-lock-only'] = false
  delete flat.packageLock
  delete flat.packageLockOnly

  mockDefs()['package-lock-only'].flatten('package-lock-only', obj, flat)
  mockDefs()['package-lock'].flatten('package-lock', obj, flat)
  t.strictSame(flat, { packageLock: false, packageLockOnly: false })
  t.end()
})

t.test('workspaces', t => {
  const obj = {
    workspaces: true,
    'user-agent': mockDefs()['user-agent'].default,
  }
  const flat = {}
  mockDefs().workspaces.flatten('workspaces', obj, flat)
  t.match(flat.userAgent, /workspaces\/true/)
  t.end()
})

t.test('workspace', t => {
  const obj = {
    workspace: ['workspace-a'],
    'user-agent': mockDefs()['user-agent'].default,
  }
  const flat = {}
  mockDefs().workspace.flatten('workspaces', obj, flat)
  t.match(flat.userAgent, /workspaces\/true/)
  t.end()
})

t.test('workspaces derived', t => {
  const obj = {
    workspaces: ['a'],
    'user-agent': mockDefs()['user-agent'].default,
  }
  const flat = {}
  mockDefs().workspaces.flatten('workspaces', obj, flat)
  t.equal(flat.workspacesEnabled, true)
  obj.workspaces = null
  mockDefs().workspaces.flatten('workspaces', obj, flat)
  t.equal(flat.workspacesEnabled, true)
  obj.workspaces = false
  mockDefs().workspaces.flatten('workspaces', obj, flat)
  t.equal(flat.workspacesEnabled, false)
  t.end()
})

t.test('lockfile version', t => {
  const flat = {}
  mockDefs()['lockfile-version'].flatten('lockfile-version', {
    'lockfile-version': '3',
  }, flat)
  t.match(flat.lockfileVersion, 3, 'flattens to a number')
  t.end()
})

t.test('loglevel silent', t => {
  const flat = {}
  mockDefs().loglevel.flatten('loglevel', {
    loglevel: 'silent',
  }, flat)
  t.match(flat.silent, true, 'flattens to assign silent')
  t.end()
})

t.test('remap legacy-bundling', t => {
  const obj = { 'legacy-bundling': true }
  const flat = {}
  mockDefs()['legacy-bundling'].flatten('legacy-bundling', obj, flat)
  t.strictSame(flat, { installStrategy: 'nested' })
  t.end()
})

t.test('remap global-style', t => {
  const obj = { 'global-style': true }
  const flat = {}
  mockDefs()['global-style'].flatten('global-style', obj, flat)
  t.strictSame(flat, { installStrategy: 'shallow' })
  t.end()
})

t.test('otp changes auth-type', t => {
  const obj = { 'auth-type': 'web', otp: 123456 }
  const flat = {}
  mockDefs().otp.flatten('otp', obj, flat)
  t.strictSame(flat, { authType: 'legacy', otp: 123456 })
  t.strictSame(obj, { 'auth-type': 'legacy', otp: 123456 })
  t.end()
})

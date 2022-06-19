const t = require('tap')
const { resolve } = require('path')
const mockGlobals = require('../../../fixtures/mock-globals')

// have to fake the node version, or else it'll only pass on this one
mockGlobals(t, { 'process.version': 'v14.8.0', 'process.env.NODE_ENV': undefined })

// also fake the npm version, so that it doesn't get reset every time
const pkg = require('../../../../package.json')

// this is a pain to keep typing
const defpath = '../../../../lib/utils/config/definitions.js'

const definitions = require(defpath)

// Tie the definitions to a snapshot so that if they change we are forced to
// update snapshots, which rebuilds the docs
for (const key of Object.keys(definitions)) {
  t.matchSnapshot(definitions[key].describe(), `config description for ${key}`)
}

const isWin = '../../../../lib/utils/is-windows.js'

// snapshot these just so we note when they change
t.matchSnapshot(Object.keys(definitions), 'all config keys')
t.matchSnapshot(Object.keys(definitions).filter(d => d.flatten),
  'all config keys that are shared to flatOptions')

t.equal(definitions['npm-version'].default, pkg.version, 'npm-version default')
t.equal(definitions['node-version'].default, process.version, 'node-version default')

t.test('basic flattening function camelCases from css-case', t => {
  const flat = {}
  const obj = { 'prefer-online': true }
  definitions['prefer-online'].flatten('prefer-online', obj, flat)
  t.strictSame(flat, { preferOnline: true })
  t.end()
})

t.test('editor', t => {
  t.test('has EDITOR and VISUAL, use EDITOR', t => {
    mockGlobals(t, { 'process.env': { EDITOR: 'vim', VISUAL: 'mate' } })
    const defs = t.mock(defpath)
    t.equal(defs.editor.default, 'vim')
    t.end()
  })
  t.test('has VISUAL but no EDITOR, use VISUAL', t => {
    mockGlobals(t, { 'process.env': { EDITOR: undefined, VISUAL: 'mate' } })
    const defs = t.mock(defpath)
    t.equal(defs.editor.default, 'mate')
    t.end()
  })
  t.test('has neither EDITOR nor VISUAL, system specific', t => {
    mockGlobals(t, { 'process.env': { EDITOR: undefined, VISUAL: undefined } })
    const defsWin = t.mock(defpath, {
      [isWin]: { isWindows: true },
    })
    t.equal(defsWin.editor.default, 'notepad.exe')
    const defsNix = t.mock(defpath, {
      [isWin]: { isWindows: false },
    })
    t.equal(defsNix.editor.default, 'vi')
    t.end()
  })
  t.end()
})

t.test('shell', t => {
  t.test('windows, env.ComSpec then cmd.exe', t => {
    mockGlobals(t, { 'process.env.ComSpec': 'command.com' })
    const defsComSpec = t.mock(defpath, {
      [isWin]: { isWindows: true },
    })
    t.equal(defsComSpec.shell.default, 'command.com')
    mockGlobals(t, { 'process.env.ComSpec': undefined })
    const defsNoComSpec = t.mock(defpath, {
      [isWin]: { isWindows: true },
    })
    t.equal(defsNoComSpec.shell.default, 'cmd')
    t.end()
  })

  t.test('nix, SHELL then sh', t => {
    mockGlobals(t, { 'process.env.SHELL': '/usr/local/bin/bash' })
    const defsShell = t.mock(defpath, {
      [isWin]: { isWindows: false },
    })
    t.equal(defsShell.shell.default, '/usr/local/bin/bash')
    mockGlobals(t, { 'process.env.SHELL': undefined })
    const defsNoShell = t.mock(defpath, {
      [isWin]: { isWindows: false },
    })
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
    const defs = t.mock(defpath, { os })
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
    const defs = t.mock(defpath, { os })
    t.same(defs['local-address'].type, [null])
    t.end()
  })
  t.end()
})

t.test('unicode allowed?', t => {
  const setGlobal = (obj = {}) => mockGlobals(t, { 'process.env': obj })

  setGlobal({ LC_ALL: 'utf8', LC_CTYPE: 'UTF-8', LANG: 'Unicode utf-8' })

  const lcAll = t.mock(defpath)
  t.equal(lcAll.unicode.default, true)
  setGlobal({ LC_ALL: 'no unicode for youUUUU!' })
  const noLcAll = t.mock(defpath)
  t.equal(noLcAll.unicode.default, false)

  setGlobal({ LC_ALL: undefined })
  const lcCtype = t.mock(defpath)
  t.equal(lcCtype.unicode.default, true)
  setGlobal({ LC_CTYPE: 'something other than unicode version 8' })
  const noLcCtype = t.mock(defpath)
  t.equal(noLcCtype.unicode.default, false)

  setGlobal({ LC_CTYPE: undefined })
  const lang = t.mock(defpath)
  t.equal(lang.unicode.default, true)
  setGlobal({ LANG: 'ISO-8859-1' })
  const noLang = t.mock(defpath)
  t.equal(noLang.unicode.default, false)
  t.end()
})

t.test('cache', t => {
  mockGlobals(t, { 'process.env.LOCALAPPDATA': 'app/data/local' })
  const defsWinLocalAppData = t.mock(defpath, {
    [isWin]: { isWindows: true },
  })
  t.equal(defsWinLocalAppData.cache.default, 'app/data/local/npm-cache')

  mockGlobals(t, { 'process.env.LOCALAPPDATA': undefined })
  const defsWinNoLocalAppData = t.mock(defpath, {
    [isWin]: { isWindows: true },
  })
  t.equal(defsWinNoLocalAppData.cache.default, '~/npm-cache')

  const defsNix = t.mock(defpath, {
    [isWin]: { isWindows: false },
  })
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
    definitions.also.flatten('also', obj, flat)
    t.strictSame(obj, { also: 'ignored', omit: [], include: [] }, 'nothing done')
    t.strictSame(flat, { omit: [] }, 'nothing done')

    obj.also = 'development'
    definitions.also.flatten('also', obj, flat)
    t.strictSame(obj, {
      also: 'development',
      omit: [],
      include: ['dev'],
    }, 'marked dev as included')
    t.strictSame(flat, { omit: [] }, 'nothing omitted, so nothing changed')

    obj.omit = ['dev', 'optional']
    obj.include = []
    definitions.also.flatten('also', obj, flat)
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
    definitions.include.flatten('include', obj, flat)
    t.strictSame(flat, { omit: [] }, 'not omitting anything')
    obj.omit = ['optional', 'dev']
    definitions.include.flatten('include', obj, flat)
    t.strictSame(flat, { omit: ['optional'] }, 'only omitting optional')
    t.end()
  })

  t.test('omit', t => {
    const flat = {}
    const obj = { include: ['dev'], omit: ['dev', 'optional'] }
    definitions.omit.flatten('omit', obj, flat)
    t.strictSame(flat, { omit: ['optional'] }, 'do not omit what is included')

    mockGlobals(t, { 'process.env.NODE_ENV': 'production' })
    const defProdEnv = t.mock(defpath)
    t.strictSame(defProdEnv.omit.default, ['dev'], 'omit dev in production')
    t.end()
  })

  t.test('only', t => {
    const flat = {}
    const obj = { only: 'asdf' }
    definitions.only.flatten('only', obj, flat)
    t.strictSame(flat, { omit: [] }, 'ignored if value is not production')

    obj.only = 'prod'
    definitions.only.flatten('only', obj, flat)
    t.strictSame(flat, { omit: ['dev'] }, 'omit dev when --only=prod')

    obj.include = ['dev']
    flat.omit = []
    definitions.only.flatten('only', obj, flat)
    t.strictSame(flat, { omit: [] }, 'do not omit when included')

    t.end()
  })

  t.test('optional', t => {
    const flat = {}
    const obj = { optional: null }

    definitions.optional.flatten('optional', obj, flat)
    t.strictSame(obj, {
      optional: null,
      omit: [],
      include: [],
    }, 'do nothing by default')
    t.strictSame(flat, { omit: [] }, 'do nothing by default')

    obj.optional = true
    definitions.optional.flatten('optional', obj, flat)
    t.strictSame(obj, {
      omit: [],
      optional: true,
      include: ['optional'],
    }, 'include optional when set')
    t.strictSame(flat, { omit: [] }, 'nothing to omit in flatOptions')

    delete obj.include
    obj.optional = false
    definitions.optional.flatten('optional', obj, flat)
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
    definitions.production.flatten('production', obj, flat)
    t.strictSame(obj, {
      production: true,
      omit: ['dev'],
      include: [],
    }, '--production sets --omit=dev')
    t.strictSame(flat, { omit: ['dev'] }, '--production sets --omit=dev')

    delete obj.omit
    obj.production = false
    delete flat.omit
    definitions.production.flatten('production', obj, flat)
    t.strictSame(obj, {
      production: false,
      include: ['dev'],
      omit: [],
    }, '--no-production explicitly includes dev')
    t.strictSame(flat, { omit: [] }, '--no-production has no effect')

    obj.production = true
    obj.include = ['dev']
    definitions.production.flatten('production', obj, flat)
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
    definitions.dev.flatten('dev', obj, flat)
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
  definitions['cache-max'].flatten('cache-max', obj, flat)
  t.strictSame(flat, {}, 'no effect if not <= 0')
  obj['cache-max'] = 0
  definitions['cache-max'].flatten('cache-max', obj, flat)
  t.strictSame(flat, { preferOnline: true }, 'preferOnline if <= 0')
  t.end()
})

t.test('cache-min', t => {
  const flat = {}
  const obj = { 'cache-min': 123 }
  definitions['cache-min'].flatten('cache-min', obj, flat)
  t.strictSame(flat, {}, 'no effect if not >= 9999')
  obj['cache-min'] = 9999
  definitions['cache-min'].flatten('cache-min', obj, flat)
  t.strictSame(flat, { preferOffline: true }, 'preferOffline if >=9999')
  t.end()
})

t.test('color', t => {
  const setTTY = (stream, value) => mockGlobals(t, { [`process.${stream}.isTTY`]: value })

  const flat = {}
  const obj = { color: 'always' }

  definitions.color.flatten('color', obj, flat)
  t.strictSame(flat, { color: true, logColor: true }, 'true when --color=always')

  obj.color = false
  definitions.color.flatten('color', obj, flat)
  t.strictSame(flat, { color: false, logColor: false }, 'true when --no-color')

  setTTY('stdout', false)
  setTTY('stderr', false)

  obj.color = true
  definitions.color.flatten('color', obj, flat)
  t.strictSame(flat, { color: false, logColor: false }, 'no color when stdout not tty')
  setTTY('stdout', true)
  definitions.color.flatten('color', obj, flat)
  t.strictSame(flat, { color: true, logColor: false }, '--color turns on color when stdout is tty')
  setTTY('stdout', false)

  obj.color = true
  definitions.color.flatten('color', obj, flat)
  t.strictSame(flat, { color: false, logColor: false }, 'no color when stderr not tty')
  setTTY('stderr', true)
  definitions.color.flatten('color', obj, flat)
  t.strictSame(flat, { color: false, logColor: true }, '--color turns on color when stderr is tty')
  setTTY('stderr', false)

  const setColor = (value) => mockGlobals(t, { 'process.env.NO_COLOR': value })

  setColor(undefined)
  const defsAllowColor = t.mock(defpath)
  t.equal(defsAllowColor.color.default, true, 'default true when no NO_COLOR env')

  setColor('0')
  const defsNoColor0 = t.mock(defpath)
  t.equal(defsNoColor0.color.default, true, 'default true when no NO_COLOR=0')

  setColor('1')
  const defsNoColor1 = t.mock(defpath)
  t.equal(defsNoColor1.color.default, false, 'default false when no NO_COLOR=1')

  t.end()
})

t.test('progress', t => {
  const setEnv = ({ tty, term } = {}) => mockGlobals(t, {
    'process.stderr.isTTY': tty,
    'process.env.TERM': term,
  })

  const flat = {}

  definitions.progress.flatten('progress', {}, flat)
  t.strictSame(flat, { progress: false })

  setEnv({ tty: true, term: 'notdumb' })
  definitions.progress.flatten('progress', { progress: true }, flat)
  t.strictSame(flat, { progress: true })

  setEnv({ tty: false, term: 'notdumb' })
  definitions.progress.flatten('progress', { progress: true }, flat)
  t.strictSame(flat, { progress: false })

  setEnv({ tty: true, term: 'dumb' })
  definitions.progress.flatten('progress', { progress: true }, flat)
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
    definitions[config].flatten(config, obj, flat)
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
    definitions[config].flatten(config, obj, flat)
    t.strictSame(flat, { search: { limit: 20, [option]: vals[option] } }, msg)
    delete obj[config]
  }

  const flat = {}
  obj.searchopts = 'a=b&b=c'
  definitions.searchopts.flatten('searchopts', obj, flat)
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
  definitions.noproxy.flatten('noproxy', obj, flat)
  t.strictSame(flat, { noProxy: '1.2.3.4,2.3.4.5,3.4.5.6' })
  t.end()
})

t.test('noProxy - string', t => {
  const obj = { noproxy: '1.2.3.4,2.3.4.5,3.4.5.6' }
  const flat = {}
  definitions.noproxy.flatten('noproxy', obj, flat)
  t.strictSame(flat, { noProxy: '1.2.3.4,2.3.4.5,3.4.5.6' })
  t.end()
})

t.test('maxSockets', t => {
  const obj = { maxsockets: 123 }
  const flat = {}
  definitions.maxsockets.flatten('maxsockets', obj, flat)
  t.strictSame(flat, { maxSockets: 123 })
  t.end()
})

t.test('scope', t => {
  const obj = { scope: 'asdf' }
  const flat = {}
  definitions.scope.flatten('scope', obj, flat)
  t.strictSame(flat, { scope: '@asdf', projectScope: '@asdf' }, 'prepend @ if needed')

  obj.scope = '@asdf'
  definitions.scope.flatten('scope', obj, flat)
  t.strictSame(flat, { scope: '@asdf', projectScope: '@asdf' }, 'leave untouched if has @')

  t.end()
})

t.test('strictSSL', t => {
  const obj = { 'strict-ssl': false }
  const flat = {}
  definitions['strict-ssl'].flatten('strict-ssl', obj, flat)
  t.strictSame(flat, { strictSSL: false })
  obj['strict-ssl'] = true
  definitions['strict-ssl'].flatten('strict-ssl', obj, flat)
  t.strictSame(flat, { strictSSL: true })
  t.end()
})

t.test('shrinkwrap/package-lock', t => {
  const obj = { shrinkwrap: false }
  const flat = {}
  definitions.shrinkwrap.flatten('shrinkwrap', obj, flat)
  t.strictSame(flat, { packageLock: false })
  obj.shrinkwrap = true
  definitions.shrinkwrap.flatten('shrinkwrap', obj, flat)
  t.strictSame(flat, { packageLock: true })

  delete obj.shrinkwrap
  obj['package-lock'] = false
  definitions['package-lock'].flatten('package-lock', obj, flat)
  t.strictSame(flat, { packageLock: false })
  obj['package-lock'] = true
  definitions['package-lock'].flatten('package-lock', obj, flat)
  t.strictSame(flat, { packageLock: true })

  t.end()
})

t.test('scriptShell', t => {
  const obj = { 'script-shell': null }
  const flat = {}
  definitions['script-shell'].flatten('script-shell', obj, flat)
  t.ok(Object.prototype.hasOwnProperty.call(flat, 'scriptShell'),
    'should set it to undefined explicitly')
  t.strictSame(flat, { scriptShell: undefined }, 'no other fields')

  obj['script-shell'] = 'asdf'
  definitions['script-shell'].flatten('script-shell', obj, flat)
  t.strictSame(flat, { scriptShell: 'asdf' }, 'sets if not falsey')

  t.end()
})

t.test('defaultTag', t => {
  const obj = { tag: 'next' }
  const flat = {}
  definitions.tag.flatten('tag', obj, flat)
  t.strictSame(flat, { defaultTag: 'next' })
  t.end()
})

t.test('timeout', t => {
  const obj = { 'fetch-timeout': 123 }
  const flat = {}
  definitions['fetch-timeout'].flatten('fetch-timeout', obj, flat)
  t.strictSame(flat, { timeout: 123 })
  t.end()
})

t.test('saveType', t => {
  t.test('save-prod', t => {
    const obj = { 'save-prod': false }
    const flat = {}
    definitions['save-prod'].flatten('save-prod', obj, flat)
    t.strictSame(flat, {}, 'no effect if false and missing')
    flat.saveType = 'prod'
    definitions['save-prod'].flatten('save-prod', obj, flat)
    t.strictSame(flat, {}, 'remove if false and set to prod')
    flat.saveType = 'dev'
    definitions['save-prod'].flatten('save-prod', obj, flat)
    t.strictSame(flat, { saveType: 'dev' }, 'ignore if false and not already prod')
    obj['save-prod'] = true
    definitions['save-prod'].flatten('save-prod', obj, flat)
    t.strictSame(flat, { saveType: 'prod' }, 'set to prod if true')
    t.end()
  })

  t.test('save-dev', t => {
    const obj = { 'save-dev': false }
    const flat = {}
    definitions['save-dev'].flatten('save-dev', obj, flat)
    t.strictSame(flat, {}, 'no effect if false and missing')
    flat.saveType = 'dev'
    definitions['save-dev'].flatten('save-dev', obj, flat)
    t.strictSame(flat, {}, 'remove if false and set to dev')
    flat.saveType = 'prod'
    obj['save-dev'] = false
    definitions['save-dev'].flatten('save-dev', obj, flat)
    t.strictSame(flat, { saveType: 'prod' }, 'ignore if false and not already dev')
    obj['save-dev'] = true
    definitions['save-dev'].flatten('save-dev', obj, flat)
    t.strictSame(flat, { saveType: 'dev' }, 'set to dev if true')
    t.end()
  })

  t.test('save-bundle', t => {
    const obj = { 'save-bundle': true }
    const flat = {}
    definitions['save-bundle'].flatten('save-bundle', obj, flat)
    t.strictSame(flat, { saveBundle: true }, 'set the saveBundle flag')

    obj['save-bundle'] = false
    definitions['save-bundle'].flatten('save-bundle', obj, flat)
    t.strictSame(flat, { saveBundle: false }, 'unset the saveBundle flag')

    obj['save-bundle'] = true
    obj['save-peer'] = true
    definitions['save-bundle'].flatten('save-bundle', obj, flat)
    t.strictSame(flat, { saveBundle: false }, 'false if save-peer is set')

    t.end()
  })

  t.test('save-peer', t => {
    const obj = { 'save-peer': false }
    const flat = {}
    definitions['save-peer'].flatten('save-peer', obj, flat)
    t.strictSame(flat, {}, 'no effect if false and not yet set')

    obj['save-peer'] = true
    definitions['save-peer'].flatten('save-peer', obj, flat)
    t.strictSame(flat, { saveType: 'peer' }, 'set saveType to peer if unset')

    flat.saveType = 'optional'
    definitions['save-peer'].flatten('save-peer', obj, flat)
    t.strictSame(flat, { saveType: 'peerOptional' }, 'set to peerOptional if optional already')

    definitions['save-peer'].flatten('save-peer', obj, flat)
    t.strictSame(flat, { saveType: 'peerOptional' }, 'no effect if already peerOptional')

    obj['save-peer'] = false
    definitions['save-peer'].flatten('save-peer', obj, flat)
    t.strictSame(flat, { saveType: 'optional' }, 'switch peerOptional to optional if false')

    obj['save-peer'] = false
    flat.saveType = 'peer'
    definitions['save-peer'].flatten('save-peer', obj, flat)
    t.strictSame(flat, {}, 'remove saveType if peer and setting false')

    t.end()
  })

  t.test('save-optional', t => {
    const obj = { 'save-optional': false }
    const flat = {}
    definitions['save-optional'].flatten('save-optional', obj, flat)
    t.strictSame(flat, {}, 'no effect if false and not yet set')

    obj['save-optional'] = true
    definitions['save-optional'].flatten('save-optional', obj, flat)
    t.strictSame(flat, { saveType: 'optional' }, 'set saveType to optional if unset')

    flat.saveType = 'peer'
    definitions['save-optional'].flatten('save-optional', obj, flat)
    t.strictSame(flat, { saveType: 'peerOptional' }, 'set to peerOptional if peer already')

    definitions['save-optional'].flatten('save-optional', obj, flat)
    t.strictSame(flat, { saveType: 'peerOptional' }, 'no effect if already peerOptional')

    obj['save-optional'] = false
    definitions['save-optional'].flatten('save-optional', obj, flat)
    t.strictSame(flat, { saveType: 'peer' }, 'switch peerOptional to peer if false')

    flat.saveType = 'optional'
    definitions['save-optional'].flatten('save-optional', obj, flat)
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
  definitions.cafile.flatten('cafile', obj, flat)
  t.strictSame(flat, {}, 'no effect if no cafile set')
  obj.cafile = resolve(path, 'no/cafile/here')
  definitions.cafile.flatten('cafile', obj, flat)
  t.strictSame(flat, {}, 'no effect if cafile not found')
  obj.cafile = cafile
  definitions.cafile.flatten('cafile', obj, flat)
  t.strictSame(flat, {
    ca: [
      '-----BEGIN CERTIFICATE-----\nXXXX\nXXXX\n-----END CERTIFICATE-----',
      '-----BEGIN CERTIFICATE-----\nYYYY\nYYYY\n-----END CERTIFICATE-----',
    ],
  })
  t.test('error other than ENOENT gets thrown', t => {
    const poo = new Error('poo')
    const defnReadFileThrows = t.mock(defpath, {
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
  const defnNoCI = t.mock(defpath, {
    '@npmcli/ci-detect': () => false,
  })
  const defnCIFoo = t.mock(defpath, {
    '@npmcli/ci-detect': () => 'foo',
  })
  t.equal(defnNoCI['ci-name'].default, null, 'null when not in CI')
  t.equal(defnCIFoo['ci-name'].default, 'foo', 'name of CI when in CI')
  t.end()
})

t.test('user-agent', t => {
  const obj = {
    'user-agent': definitions['user-agent'].default,
    'npm-version': '1.2.3',
    'node-version': '9.8.7',
  }
  const flat = {}
  const expectNoCI = `npm/1.2.3 node/9.8.7 ` +
    `${process.platform} ${process.arch} workspaces/false`
  definitions['user-agent'].flatten('user-agent', obj, flat)
  t.equal(flat.userAgent, expectNoCI)
  t.equal(process.env.npm_config_user_agent, flat.userAgent, 'npm_user_config environment is set')
  t.not(obj['user-agent'], flat.userAgent, 'config user-agent template is not translated')

  obj['ci-name'] = 'foo'
  obj['user-agent'] = definitions['user-agent'].default
  const expectCI = `${expectNoCI} ci/foo`
  definitions['user-agent'].flatten('user-agent', obj, flat)
  t.equal(flat.userAgent, expectCI)
  t.equal(process.env.npm_config_user_agent, flat.userAgent, 'npm_user_config environment is set')
  t.not(obj['user-agent'], flat.userAgent, 'config user-agent template is not translated')

  delete obj['ci-name']
  obj.workspaces = true
  obj['user-agent'] = definitions['user-agent'].default
  const expectWorkspaces = expectNoCI.replace('workspaces/false', 'workspaces/true')
  definitions['user-agent'].flatten('user-agent', obj, flat)
  t.equal(flat.userAgent, expectWorkspaces)
  t.equal(process.env.npm_config_user_agent, flat.userAgent, 'npm_user_config environment is set')
  t.not(obj['user-agent'], flat.userAgent, 'config user-agent template is not translated')

  delete obj.workspaces
  obj.workspace = ['foo']
  obj['user-agent'] = definitions['user-agent'].default
  definitions['user-agent'].flatten('user-agent', obj, flat)
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
  definitions['save-prefix']
    .flatten('save-prefix', { ...obj, 'save-exact': true }, flat)
  t.strictSame(flat, { savePrefix: '' })
  definitions['save-prefix']
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
  definitions['save-exact']
    .flatten('save-exact', { ...obj, 'save-exact': true }, flat)
  t.strictSame(flat, { savePrefix: '' })
  definitions['save-exact']
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
  definitions.global.flatten('global', obj, flat)
  definitions.location.flatten('location', obj, flat)
  // global = true sets location in both places to global
  t.strictSame(flat, { global: true, location: 'global' })
  // location here is still 'user' because flattening doesn't modify the object
  t.strictSame(obj, { global: true, location: 'user' })

  obj.global = false
  obj.location = 'user'
  delete flat.global
  delete flat.location

  definitions.global.flatten('global', obj, flat)
  definitions.location.flatten('location', obj, flat)
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

  definitions['package-lock-only'].flatten('package-lock-only', obj, flat)
  definitions['package-lock'].flatten('package-lock', obj, flat)
  t.strictSame(flat, { packageLock: true, packageLockOnly: true })

  obj['package-lock-only'] = false
  delete flat.packageLock
  delete flat.packageLockOnly

  definitions['package-lock-only'].flatten('package-lock-only', obj, flat)
  definitions['package-lock'].flatten('package-lock', obj, flat)
  t.strictSame(flat, { packageLock: false, packageLockOnly: false })
  t.end()
})

t.test('workspaces', t => {
  const obj = {
    workspaces: true,
    'user-agent': definitions['user-agent'].default,
  }
  const flat = {}
  definitions.workspaces.flatten('workspaces', obj, flat)
  t.match(flat.userAgent, /workspaces\/true/)
  t.end()
})

t.test('workspace', t => {
  const obj = {
    workspace: ['workspace-a'],
    'user-agent': definitions['user-agent'].default,
  }
  const flat = {}
  definitions.workspace.flatten('workspaces', obj, flat)
  t.match(flat.userAgent, /workspaces\/true/)
  t.end()
})

t.test('workspaces derived', t => {
  const obj = {
    workspaces: ['a'],
    'user-agent': definitions['user-agent'].default,
  }
  const flat = {}
  definitions.workspaces.flatten('workspaces', obj, flat)
  t.equal(flat.workspacesEnabled, true)
  obj.workspaces = null
  definitions.workspaces.flatten('workspaces', obj, flat)
  t.equal(flat.workspacesEnabled, true)
  obj.workspaces = false
  definitions.workspaces.flatten('workspaces', obj, flat)
  t.equal(flat.workspacesEnabled, false)
  t.end()
})

t.test('lockfile version', t => {
  const flat = {}
  definitions['lockfile-version'].flatten('lockfile-version', {
    'lockfile-version': '3',
  }, flat)
  t.match(flat.lockfileVersion, 3, 'flattens to a number')
  t.end()
})

t.test('loglevel silent', t => {
  const flat = {}
  definitions.loglevel.flatten('loglevel', {
    loglevel: 'silent',
  }, flat)
  t.match(flat.silent, true, 'flattens to assign silent')
  t.end()
})

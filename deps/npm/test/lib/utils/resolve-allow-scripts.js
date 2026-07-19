const t = require('tap')
const mockNpm = require('../../fixtures/mock-npm')
const tmock = require('../../fixtures/tmock')

const loadResolver = (t) => tmock(t, '{LIB}/utils/resolve-allow-scripts.js')

// Helper that simulates config layering. `cliConfig` sets the value at
// the 'cli' source; `npmrcConfig` sets it at the 'user' source. mockNpm
// puts all `config` keys into the 'cli' source by default, so for npmrc
// tests we use an .npmrc file instead.

t.test('returns null when no policy is set anywhere', async t => {
  const { npm } = await mockNpm(t, {
    prefixDir: { 'package.json': JSON.stringify({ name: 'p' }) },
  })
  const resolveAllowScripts = loadResolver(t)
  const result = await resolveAllowScripts(npm)
  t.strictSame(result, { policy: null, source: null })
})

t.test('global install: skips package.json but still consults CLI', async t => {
  const { npm } = await mockNpm(t, {
    config: { global: true, 'allow-scripts': 'canvas' },
    prefixDir: { 'package.json': JSON.stringify({ name: 'p', allowScripts: { sharp: true } }) },
  })
  const resolveAllowScripts = loadResolver(t)
  const result = await resolveAllowScripts(npm)
  t.equal(result.source, 'cli')
  t.strictSame(result.policy, { canvas: true })
})

t.test('global install: skips package.json but still consults .npmrc', async t => {
  const { npm } = await mockNpm(t, {
    config: { global: true },
    homeDir: { '.npmrc': 'allow-scripts = canvas' },
    prefixDir: {
      'package.json': JSON.stringify({ name: 'p', allowScripts: { sharp: true } }),
    },
  })
  const resolveAllowScripts = loadResolver(t)
  const result = await resolveAllowScripts(npm)
  t.equal(result.source, '.npmrc')
  t.strictSame(result.policy, { canvas: true })
})

t.test('global install with no CLI or .npmrc returns null', async t => {
  const { npm } = await mockNpm(t, {
    config: { global: true },
    prefixDir: { 'package.json': JSON.stringify({ name: 'p', allowScripts: { x: true } }) },
  })
  const resolveAllowScripts = loadResolver(t)
  const result = await resolveAllowScripts(npm)
  t.strictSame(result, { policy: null, source: null })
})

t.test('reads from package.json when only package.json is set', async t => {
  const { npm } = await mockNpm(t, {
    prefixDir: {
      'package.json': JSON.stringify({
        name: 'p',
        allowScripts: { canvas: true, 'core-js': false, 'sharp@0.33.2': true },
      }),
    },
  })
  const resolveAllowScripts = loadResolver(t)
  const result = await resolveAllowScripts(npm)
  t.equal(result.source, 'package.json')
  t.strictSame(result.policy, { canvas: true, 'core-js': false, 'sharp@0.33.2': true })
})

t.test('--allow-scripts CLI flag is rejected in project-scoped installs', async t => {
  const mock = await mockNpm(t, {
    prefixDir: {
      'package.json': JSON.stringify({
        name: 'p',
        allowScripts: { sharp: true },
      }),
    },
    // mock-npm puts all config keys at the 'cli' source.
    config: { 'allow-scripts': 'canvas' },
  })
  const resolveAllowScripts = loadResolver(t)
  await t.rejects(
    resolveAllowScripts(mock.npm),
    { code: 'EALLOWSCRIPTS', message: /--allow-scripts is not allowed/ }
  )
})

t.test('--allow-scripts CLI flag is accepted in global installs (RFC layer 1 wins)', async t => {
  const mock = await mockNpm(t, {
    prefixDir: {
      'package.json': JSON.stringify({
        name: 'p',
        allowScripts: { sharp: true },
      }),
    },
    config: { 'allow-scripts': 'canvas', global: true },
  })
  const resolveAllowScripts = loadResolver(t)
  const result = await resolveAllowScripts(mock.npm)
  t.equal(result.source, 'cli')
  t.strictSame(result.policy, { canvas: true })
})

t.test('package.json wins over .npmrc setting (RFC layer 2 > layer 3)', async t => {
  // Put the allow-scripts setting in an .npmrc file so it loads at the
  // 'user' source, not 'cli'.
  const mock = await mockNpm(t, {
    prefixDir: {
      'package.json': JSON.stringify({
        name: 'p',
        allowScripts: { sharp: true },
      }),
      '.npmrc': 'allow-scripts = canvas',
    },
  })
  const resolveAllowScripts = loadResolver(t)
  const result = await resolveAllowScripts(mock.npm)
  t.equal(result.source, 'package.json')
  t.strictSame(result.policy, { sharp: true })
  t.match(
    mock.logs.warn.byTitle('allow-scripts'),
    [/\.npmrc allow-scripts setting is being ignored because package.json/]
  )
})

t.test('.npmrc setting is used when nothing higher is set', async t => {
  const { npm } = await mockNpm(t, {
    prefixDir: {
      'package.json': JSON.stringify({ name: 'p' }),
      '.npmrc': 'allow-scripts = canvas, sharp',
    },
  })
  const resolveAllowScripts = loadResolver(t)
  const result = await resolveAllowScripts(npm)
  t.equal(result.source, '.npmrc')
  t.strictSame(result.policy, { canvas: true, sharp: true })
})

t.test('--allow-scripts CLI flag is accepted via skipProjectConfig (npm exec)', async t => {
  const mock = await mockNpm(t, {
    prefixDir: {
      'package.json': JSON.stringify({ name: 'p' }),
      '.npmrc': 'allow-scripts = canvas',
    },
    config: { 'allow-scripts': 'sharp' },
  })
  const resolveAllowScripts = loadResolver(t)
  const result = await resolveAllowScripts(mock.npm, { skipProjectConfig: true })
  t.equal(result.source, 'cli')
  t.strictSame(result.policy, { sharp: true })
  t.match(
    mock.logs.warn.byTitle('allow-scripts'),
    [/\.npmrc allow-scripts setting is being ignored because --allow-scripts/]
  )
})

t.test('empty allowScripts object in package.json falls through to .npmrc', async t => {
  const { npm } = await mockNpm(t, {
    prefixDir: {
      'package.json': JSON.stringify({ name: 'p', allowScripts: {} }),
      '.npmrc': 'allow-scripts = canvas',
    },
  })
  const resolveAllowScripts = loadResolver(t)
  const result = await resolveAllowScripts(npm)
  t.equal(result.source, '.npmrc')
  t.strictSame(result.policy, { canvas: true })
})

t.test('missing package.json with .npmrc setting uses .npmrc', async t => {
  const { npm } = await mockNpm(t, {
    prefixDir: {
      '.npmrc': 'allow-scripts = canvas',
    },
  })
  const resolveAllowScripts = loadResolver(t)
  const result = await resolveAllowScripts(npm)
  t.equal(result.source, '.npmrc')
  t.strictSame(result.policy, { canvas: true })
})

t.test('reads from npm.prefix, not cwd, so workspace sub-installs find root policy', async t => {
  const { npm } = await mockNpm(t, {
    prefixDir: {
      'package.json': JSON.stringify({
        name: 'root',
        workspaces: ['packages/*'],
        allowScripts: { sharp: true },
      }),
      packages: {
        sub: { 'package.json': JSON.stringify({ name: 'sub' }) },
      },
    },
    chdir: ({ prefix }) => require('node:path').join(prefix, 'packages', 'sub'),
  })
  const resolveAllowScripts = loadResolver(t)
  const result = await resolveAllowScripts(npm)
  t.equal(result.source, 'package.json')
  t.strictSame(result.policy, { sharp: true })
})

t.test('drops package.json entries with forbidden semver ranges and warns', async t => {
  const mock = await mockNpm(t, {
    prefixDir: {
      'package.json': JSON.stringify({
        name: 'p',
        allowScripts: {
          'sharp@^0.33.0': true, // forbidden: caret range
          'canvas@~2.11.0': true, // forbidden: tilde range
          'core-js@>=3.0.0': true, // forbidden: gte range
          'good@1.2.3': true, // OK: exact pin
          'also-good': true, // OK: bare name
          'disjunction@1.0.0 || 2.0.0': true, // OK: exact disjunction
        },
      }),
    },
  })
  const resolveAllowScripts = loadResolver(t)
  const result = await resolveAllowScripts(mock.npm)
  t.equal(result.source, 'package.json')
  t.strictSame(result.policy, {
    'good@1.2.3': true,
    'also-good': true,
    'disjunction@1.0.0 || 2.0.0': true,
  })
  const warnings = mock.logs.warn.byTitle('allow-scripts')
  t.equal(warnings.filter(m => /semver ranges/.test(m)).length, 3)
})

t.test('drops package.json entries with dist-tag specs and warns', async t => {
  const mock = await mockNpm(t, {
    prefixDir: {
      'package.json': JSON.stringify({
        name: 'p',
        allowScripts: {
          'sharp@latest': true, // forbidden: dist-tag
          'canvas@next': true, // forbidden: dist-tag
          'good@1.2.3': true, // OK: exact pin
        },
      }),
    },
  })
  const resolveAllowScripts = loadResolver(t)
  const result = await resolveAllowScripts(mock.npm)
  t.equal(result.source, 'package.json')
  t.strictSame(result.policy, { 'good@1.2.3': true })
  const warnings = mock.logs.warn.byTitle('allow-scripts')
  t.equal(warnings.filter(m => /dist-tag specs/.test(m)).length, 2)
})

t.test('drops .npmrc forbidden ranges (and warns) but keeps valid entries', async t => {
  const mock = await mockNpm(t, {
    prefixDir: {
      'package.json': JSON.stringify({ name: 'p' }),
      '.npmrc': 'allow-scripts = canvas, sharp@^0.33.0, lodash@4.17.21',
    },
  })
  const resolveAllowScripts = loadResolver(t)
  const result = await resolveAllowScripts(mock.npm)
  t.equal(result.source, '.npmrc')
  t.strictSame(result.policy, { canvas: true, 'lodash@4.17.21': true })
  const warnings = mock.logs.warn.byTitle('allow-scripts')
  t.ok(warnings.some(m => /sharp@\^0\.33\.0/.test(m) && /semver ranges/.test(m)))
})

t.test('drops package.json entries that fail npa parse', async t => {
  const mock = await mockNpm(t, {
    prefixDir: {
      'package.json': JSON.stringify({
        name: 'p',
        allowScripts: {
          '@@@invalid@@@': true,
          good: true,
        },
      }),
    },
  })
  const resolveAllowScripts = loadResolver(t)
  const result = await resolveAllowScripts(mock.npm)
  t.equal(result.source, 'package.json')
  t.strictSame(result.policy, { good: true })
  t.ok(mock.logs.warn.byTitle('allow-scripts').some(m => /unparseable/.test(m)))
})

t.test('returns null when all package.json entries are dropped as invalid', async t => {
  const { npm } = await mockNpm(t, {
    prefixDir: {
      'package.json': JSON.stringify({
        name: 'p',
        allowScripts: { 'sharp@^0.33.0': true },
      }),
    },
  })
  const resolveAllowScripts = loadResolver(t)
  const result = await resolveAllowScripts(npm)
  t.strictSame(result, { policy: null, source: null })
})

t.test('skipProjectConfig: ignores package.json even when present', async t => {
  // Per RFC line 299, exec/npx consults only user/global .npmrc.
  const { npm } = await mockNpm(t, {
    prefixDir: {
      'package.json': JSON.stringify({
        name: 'p',
        allowScripts: { sharp: true },
      }),
      '.npmrc': 'allow-scripts = canvas',
    },
  })
  const resolveAllowScripts = loadResolver(t)
  const result = await resolveAllowScripts(npm, { skipProjectConfig: true })
  // package.json is skipped, falls through to .npmrc.
  t.equal(result.source, '.npmrc')
  t.strictSame(result.policy, { canvas: true })
})

t.test('skipProjectConfig: CLI still wins over .npmrc', async t => {
  const { npm } = await mockNpm(t, {
    prefixDir: {
      'package.json': JSON.stringify({
        name: 'p',
        allowScripts: { sharp: true },
      }),
      '.npmrc': 'allow-scripts = canvas',
    },
    config: { 'allow-scripts': 'lodash' },
  })
  const resolveAllowScripts = loadResolver(t)
  const result = await resolveAllowScripts(npm, { skipProjectConfig: true })
  t.equal(result.source, 'cli')
  t.strictSame(result.policy, { lodash: true })
})

t.test('skipProjectConfig: returns null when only package.json is set', async t => {
  const { npm } = await mockNpm(t, {
    prefixDir: {
      'package.json': JSON.stringify({
        name: 'p',
        allowScripts: { sharp: true },
      }),
    },
  })
  const resolveAllowScripts = loadResolver(t)
  const result = await resolveAllowScripts(npm, { skipProjectConfig: true })
  t.strictSame(result, { policy: null, source: null })
})

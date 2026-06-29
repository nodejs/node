const t = require('tap')
const { classifyUnusedEntries } = require('../../../lib/utils/allow-scripts-prune.js')

// Minimal registry node: `matches` derives name/version from the resolved URL.
const node = ({ name = 'pkg', version = '1.0.0' } = {}) => ({
  name,
  version,
  isRegistryDependency: true,
  resolved: `https://registry.npmjs.org/${name}/-/${name}-${version}.tgz`,
})

const withScripts = (overrides) => ({ node: node(overrides), hasScripts: true })
const noScripts = (overrides) => ({ node: node(overrides), hasScripts: false })

t.test('empty / nullish policy', t => {
  t.same(classifyUnusedEntries({}, []), { remaining: {}, removed: [] })
  t.same(classifyUnusedEntries(null, []), { remaining: {}, removed: [] })
  t.same(classifyUnusedEntries(undefined, [withScripts()]), { remaining: {}, removed: [] })
  t.end()
})

t.test('keeps entries that match an installed package with scripts', t => {
  const { remaining, removed } = classifyUnusedEntries(
    { canvas: true, 'esbuild@1.0.0': true },
    [withScripts({ name: 'canvas' }), withScripts({ name: 'esbuild', version: '1.0.0' })]
  )
  t.same(remaining, { canvas: true, 'esbuild@1.0.0': true })
  t.same(removed, [])
  t.end()
})

t.test('removes entries for packages no longer installed', t => {
  const { remaining, removed } = classifyUnusedEntries(
    { canvas: true, gone: true },
    [withScripts({ name: 'canvas' })]
  )
  t.same(remaining, { canvas: true })
  t.same(removed, [{ key: 'gone', value: true, reason: 'not-installed' }])
  t.end()
})

t.test('removes entries whose package no longer has install scripts', t => {
  const { remaining, removed } = classifyUnusedEntries(
    { canvas: true },
    [noScripts({ name: 'canvas' })]
  )
  t.same(remaining, {})
  t.same(removed, [{ key: 'canvas', value: true, reason: 'no-scripts' }])
  t.end()
})

t.test('a matching version with scripts keeps the entry even if another lacks them', t => {
  const { remaining, removed } = classifyUnusedEntries(
    { canvas: true },
    [noScripts({ name: 'canvas', version: '1.0.0' }), withScripts({ name: 'canvas', version: '2.0.0' })]
  )
  t.same(remaining, { canvas: true })
  t.same(removed, [])
  t.end()
})

t.test('version-pinned entry is unused when that exact version is not installed', t => {
  const { remaining, removed } = classifyUnusedEntries(
    { 'canvas@1.0.0': true },
    [withScripts({ name: 'canvas', version: '2.0.0' })]
  )
  t.same(remaining, {})
  t.same(removed, [{ key: 'canvas@1.0.0', value: true, reason: 'not-installed' }])
  t.end()
})

t.test('prunes unused deny (false) entries the same way', t => {
  const { remaining, removed } = classifyUnusedEntries(
    { 'denied-gone': false, 'denied-here': false },
    [withScripts({ name: 'denied-here' })]
  )
  t.same(remaining, { 'denied-here': false })
  t.same(removed, [{ key: 'denied-gone', value: false, reason: 'not-installed' }])
  t.end()
})

t.test('keeps unparseable keys untouched', t => {
  const { remaining, removed } = classifyUnusedEntries(
    { 'a b': true, gone: true },
    [withScripts({ name: 'canvas' })]
  )
  t.same(remaining, { 'a b': true })
  t.same(removed, [{ key: 'gone', value: true, reason: 'not-installed' }])
  t.end()
})

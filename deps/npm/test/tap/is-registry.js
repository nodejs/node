'use strict'
const test = require('tap').test
const npa = require('npm-package-arg')
const isRegistry = require('../../lib/utils/is-registry.js')

test('current npa', (t) => {
  t.is(Boolean(isRegistry(npa('foo@1.0.0'))), true, 'version')
  t.is(Boolean(isRegistry(npa('foo@^1.0.0'))), true, 'range')
  t.is(Boolean(isRegistry(npa('foo@test'))), true, 'tag')
  t.is(Boolean(isRegistry(npa('foo@git://foo.bar/test.git'))), false, 'git')
  t.is(Boolean(isRegistry(npa('foo@foo/bar'))), false, 'github')
  t.is(Boolean(isRegistry(npa('foo@http://example.com/example.tgz'))), false, 'remote')
  t.is(Boolean(isRegistry(npa('foo@file:example.tgz'))), false, 'file')
  t.is(Boolean(isRegistry(npa('foo@file:example/'))), false, 'dir')
  t.done()
})

test('legacy spec data', (t) => {
  t.is(Boolean(isRegistry({type: 'version'})), true, 'version')
  t.is(Boolean(isRegistry({type: 'range'})), true, 'range')
  t.is(Boolean(isRegistry({type: 'tag'})), true, 'tag')
  t.is(Boolean(isRegistry({type: 'git'})), false, 'git')
  t.is(Boolean(isRegistry({type: 'file'})), false, 'file')
  t.is(Boolean(isRegistry({type: 'directory'})), false, 'directory')
  t.is(Boolean(isRegistry({type: 'remote'})), false, 'remote')
  t.done()
})

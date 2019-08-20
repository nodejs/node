'use strict'

const figgyPudding = require('figgy-pudding')
const getStream = require('get-stream')
const qs = require('querystring')
const test = require('tap').test
const tnock = require('./util/tnock.js')

const OPTS = figgyPudding({ registry: {} })({
  registry: 'https://mock.reg/'
})

const REG = OPTS.registry
const search = require('../index.js')

test('basic test', t => {
  const query = qs.stringify({
    text: 'oo',
    size: 20,
    from: 0,
    quality: 0.65,
    popularity: 0.98,
    maintenance: 0.5
  })
  tnock(t, REG).get(`/-/v1/search?${query}`).once().reply(200, {
    objects: [
      { package: { name: 'cool', version: '1.0.0' } },
      { package: { name: 'foo', version: '2.0.0' } }
    ]
  })
  return search('oo', OPTS).then(results => {
    t.similar(results, [{
      name: 'cool',
      version: '1.0.0'
    }, {
      name: 'foo',
      version: '2.0.0'
    }], 'got back an array of search results')
  })
})

test('search.stream', t => {
  const query = qs.stringify({
    text: 'oo',
    size: 20,
    from: 0,
    quality: 0.65,
    popularity: 0.98,
    maintenance: 0.5
  })
  tnock(t, REG).get(`/-/v1/search?${query}`).once().reply(200, {
    objects: [
      { package: { name: 'cool', version: '1.0.0', date: new Date().toISOString() } },
      { package: { name: 'foo', version: '2.0.0' } }
    ]
  })
  return getStream.array(
    search.stream('oo', OPTS)
  ).then(results => {
    t.similar(results, [{
      name: 'cool',
      version: '1.0.0'
    }, {
      name: 'foo',
      version: '2.0.0'
    }], 'has a stream-based API function with identical results')
  })
})

test('accepts a limit option', t => {
  const query = qs.stringify({
    text: 'oo',
    size: 3,
    from: 0,
    quality: 0.65,
    popularity: 0.98,
    maintenance: 0.5
  })
  tnock(t, REG).get(`/-/v1/search?${query}`).once().reply(200, {
    objects: [
      { package: { name: 'cool', version: '1.0.0' } },
      { package: { name: 'cool', version: '1.0.0' } },
      { package: { name: 'cool', version: '1.0.0' } },
      { package: { name: 'cool', version: '1.0.0' } }
    ]
  })
  return search('oo', OPTS.concat({ limit: 3 })).then(results => {
    t.equal(results.length, 4, 'returns more results if endpoint does so')
  })
})

test('accepts a from option', t => {
  const query = qs.stringify({
    text: 'oo',
    size: 20,
    from: 1,
    quality: 0.65,
    popularity: 0.98,
    maintenance: 0.5
  })
  tnock(t, REG).get(`/-/v1/search?${query}`).once().reply(200, {
    objects: [
      { package: { name: 'cool', version: '1.0.0' } },
      { package: { name: 'cool', version: '1.1.0' } },
      { package: { name: 'cool', version: '1.0.0' } },
      { package: { name: 'cool', version: '1.0.0' } }
    ]
  })
  return search('oo', OPTS.concat({ from: 1 })).then(results => {
    t.equal(results.length, 4, 'returns more results if endpoint does so')
  })
})

test('accepts quality/mainenance/popularity options', t => {
  const query = qs.stringify({
    text: 'oo',
    size: 20,
    from: 0,
    quality: 1,
    popularity: 2,
    maintenance: 3
  })
  tnock(t, REG).get(`/-/v1/search?${query}`).once().reply(200, {
    objects: [
      { package: { name: 'cool', version: '1.0.0' } },
      { package: { name: 'cool', version: '1.0.0' } },
      { package: { name: 'cool', version: '1.0.0' } },
      { package: { name: 'cool', version: '1.0.0' } }
    ]
  })
  return search('oo', OPTS.concat({
    quality: 1,
    popularity: 2,
    maintenance: 3
  })).then(results => {
    t.equal(results.length, 4, 'returns more results if endpoint does so')
  })
})

test('sortBy: quality', t => {
  const query = qs.stringify({
    text: 'oo',
    size: 20,
    from: 0,
    quality: 1,
    popularity: 0,
    maintenance: 0
  })
  tnock(t, REG).get(`/-/v1/search?${query}`).once().reply(200, {
    objects: [
      { package: { name: 'cool', version: '1.0.0' } },
      { package: { name: 'cool', version: '1.0.0' } },
      { package: { name: 'cool', version: '1.0.0' } },
      { package: { name: 'cool', version: '1.0.0' } }
    ]
  })
  return search('oo', OPTS.concat({
    sortBy: 'quality'
  })).then(results => {
    t.equal(results.length, 4, 'returns more results if endpoint does so')
  })
})

test('sortBy: popularity', t => {
  const query = qs.stringify({
    text: 'oo',
    size: 20,
    from: 0,
    quality: 0,
    popularity: 1,
    maintenance: 0
  })
  tnock(t, REG).get(`/-/v1/search?${query}`).once().reply(200, {
    objects: [
      { package: { name: 'cool', version: '1.0.0' } },
      { package: { name: 'cool', version: '1.0.0' } },
      { package: { name: 'cool', version: '1.0.0' } },
      { package: { name: 'cool', version: '1.0.0' } }
    ]
  })
  return search('oo', OPTS.concat({
    sortBy: 'popularity'
  })).then(results => {
    t.equal(results.length, 4, 'returns more results if endpoint does so')
  })
})

test('sortBy: maintenance', t => {
  const query = qs.stringify({
    text: 'oo',
    size: 20,
    from: 0,
    quality: 0,
    popularity: 0,
    maintenance: 1
  })
  tnock(t, REG).get(`/-/v1/search?${query}`).once().reply(200, {
    objects: [
      { package: { name: 'cool', version: '1.0.0' } },
      { package: { name: 'cool', version: '1.0.0' } },
      { package: { name: 'cool', version: '1.0.0' } },
      { package: { name: 'cool', version: '1.0.0' } }
    ]
  })
  return search('oo', OPTS.concat({
    sortBy: 'maintenance'
  })).then(results => {
    t.equal(results.length, 4, 'returns more results if endpoint does so')
  })
})

test('sortBy: optimal', t => {
  const query = qs.stringify({
    text: 'oo',
    size: 20,
    from: 0,
    quality: 0.65,
    popularity: 0.98,
    maintenance: 0.5
  })
  tnock(t, REG).get(`/-/v1/search?${query}`).once().reply(200, {
    objects: [
      { package: { name: 'cool', version: '1.0.0' } },
      { package: { name: 'cool', version: '1.0.0' } },
      { package: { name: 'cool', version: '1.0.0' } },
      { package: { name: 'cool', version: '1.0.0' } }
    ]
  })
  return search('oo', OPTS.concat({
    sortBy: 'optimal'
  })).then(results => {
    t.equal(results.length, 4, 'returns more results if endpoint does so')
  })
})

test('detailed format', t => {
  const query = qs.stringify({
    text: 'oo',
    size: 20,
    from: 0,
    quality: 0,
    popularity: 0,
    maintenance: 1
  })
  const results = [
    {
      package: { name: 'cool', version: '1.0.0' },
      score: {
        final: 0.9237841281241451,
        detail: {
          quality: 0.9270640902288084,
          popularity: 0.8484861649808381,
          maintenance: 0.9962706951777409
        }
      },
      searchScore: 100000.914
    },
    {
      package: { name: 'ok', version: '2.0.0' },
      score: {
        final: 0.9237841281451,
        detail: {
          quality: 0.9270602288084,
          popularity: 0.8461649808381,
          maintenance: 0.9706951777409
        }
      },
      searchScore: 1000.91
    }
  ]
  tnock(t, REG).get(`/-/v1/search?${query}`).once().reply(200, {
    objects: results
  })
  return search('oo', OPTS.concat({
    sortBy: 'maintenance',
    detailed: true
  })).then(res => {
    t.deepEqual(res, results, 'return full-format results with opts.detailed')
  })
})

test('space-separates and URI-encodes multiple search params', t => {
  const query = qs.stringify({
    text: 'foo bar:baz quux?=',
    size: 1,
    from: 0,
    quality: 1,
    popularity: 2,
    maintenance: 3
  })
  tnock(t, REG).get(`/-/v1/search?${query}`).reply(200, { objects: [] })
  return search(['foo', 'bar:baz', 'quux?='], OPTS.concat({
    limit: 1,
    quality: 1,
    popularity: 2,
    maintenance: 3
  })).then(
    () => t.ok(true, 'sent parameters correctly urlencoded')
  )
})

'use strict'

const figgyPudding = require('figgy-pudding')
const getStream = require('get-stream')
const npmFetch = require('npm-registry-fetch')

const SearchOpts = figgyPudding({
  detailed: {default: false},
  limit: {default: 20},
  from: {default: 0},
  quality: {default: 0.65},
  popularity: {default: 0.98},
  maintenance: {default: 0.5},
  sortBy: {}
})

module.exports = search
function search (query, opts) {
  return getStream.array(search.stream(query, opts))
}
search.stream = searchStream
function searchStream (query, opts) {
  opts = SearchOpts(opts)
  switch (opts.sortBy) {
    case 'optimal': {
      opts = opts.concat({
        quality: 0.65,
        popularity: 0.98,
        maintenance: 0.5
      })
      break
    }
    case 'quality': {
      opts = opts.concat({
        quality: 1,
        popularity: 0,
        maintenance: 0
      })
      break
    }
    case 'popularity': {
      opts = opts.concat({
        quality: 0,
        popularity: 1,
        maintenance: 0
      })
      break
    }
    case 'maintenance': {
      opts = opts.concat({
        quality: 0,
        popularity: 0,
        maintenance: 1
      })
      break
    }
  }
  return npmFetch.json.stream('/-/v1/search', 'objects.*',
    opts.concat({
      query: {
        text: Array.isArray(query) ? query.join(' ') : query,
        size: opts.limit,
        quality: opts.quality,
        popularity: opts.popularity,
        maintenance: opts.maintenance
      },
      mapJson (obj) {
        if (obj.package.date) {
          obj.package.date = new Date(obj.package.date)
        }
        if (opts.detailed) {
          return obj
        } else {
          return obj.package
        }
      }
    })
  )
}

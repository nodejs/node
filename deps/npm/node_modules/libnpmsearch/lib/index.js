'use strict'

const npmFetch = require('npm-registry-fetch')

module.exports = search
function search (query, opts) {
  return search.stream(query, opts).collect()
}
search.stream = searchStream
function searchStream (query, opts = {}) {
  opts = {
    detailed: false,
    limit: 20,
    from: 0,
    quality: 0.65,
    popularity: 0.98,
    maintenance: 0.5,
    ...opts.opts, // this is to support the cli's --searchopts parameter
    ...opts,
  }

  switch (opts.sortBy) {
    case 'optimal': {
      opts.quality = 0.65
      opts.popularity = 0.98
      opts.maintenance = 0.5
      break
    }
    case 'quality': {
      opts.quality = 1
      opts.popularity = 0
      opts.maintenance = 0
      break
    }
    case 'popularity': {
      opts.quality = 0
      opts.popularity = 1
      opts.maintenance = 0
      break
    }
    case 'maintenance': {
      opts.quality = 0
      opts.popularity = 0
      opts.maintenance = 1
      break
    }
  }
  return npmFetch.json.stream('/-/v1/search', 'objects.*',
    {
      ...opts,
      query: {
        text: Array.isArray(query) ? query.join(' ') : query,
        size: opts.limit,
        from: opts.from,
        quality: opts.quality,
        popularity: opts.popularity,
        maintenance: opts.maintenance,
      },
      mapJSON: (obj) => {
        if (obj.package.date) {
          obj.package.date = new Date(obj.package.date)
        }
        if (opts.detailed) {
          return obj
        } else {
          return obj.package
        }
      },
    }
  )
}

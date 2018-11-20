# figgy-pudding [![npm version](https://img.shields.io/npm/v/figgy-pudding.svg)](https://npm.im/figgy-pudding) [![license](https://img.shields.io/npm/l/figgy-pudding.svg)](https://npm.im/figgy-pudding) [![Travis](https://img.shields.io/travis/zkat/figgy-pudding.svg)](https://travis-ci.org/zkat/figgy-pudding) [![AppVeyor](https://ci.appveyor.com/api/projects/status/github/zkat/figgy-pudding?svg=true)](https://ci.appveyor.com/project/zkat/figgy-pudding) [![Coverage Status](https://coveralls.io/repos/github/zkat/figgy-pudding/badge.svg?branch=latest)](https://coveralls.io/github/zkat/figgy-pudding?branch=latest)

# Death to the God Object! Now Bring Us Some Figgy Pudding!

[`figgy-pudding`](https://github.com/zkat/figgy-pudding) is a simple JavaScript library for managing and composing cascading options objects -- hiding what needs to be hidden from each layer, without having to do a lot of manual munging and passing of options.

## Install

`$ npm install figgy-pudding`

## Table of Contents

* [Example](#example)
* [Features](#features)
* [API](#api)
  * [`figgyPudding(spec)`](#figgy-pudding)
  * [`Opts(values)`](#opts)
    * [`opts.get()`](#opts-get)
    * [`opts.concat()`](#opts-concat)

### Example

```javascript
const puddin = require('figgyPudding')

const RequestOpts = puddin({
  follow: {
    default: true
  },
  streaming: {
    default: false
  },
  log: {
    default: require('npmlog')
  }
})

const MyAppOpts = puddin({
  log: {
    default: require('npmlog')
  },
  cache: {
    default: './cache'
  }
})

function start (opts) {
  opts = MyAppOpts(opts)
  initCache(opts.get('cache'))
  opts.get('streaming') // => undefined
  reqStuff('https://npm.im/figgy-pudding', opts)
}

function reqStuff (uri, opts) {
  opts = RequestOpts(opts)
  require('request').get(uri, opts) // can't see `cache`
}
```

### Features

* Hide options from layer that didn't ask for it
* Shared multi-layer options

### API

#### <a name="figgy-pudding"></a> `> figgyPudding({ key: { default: val } | String }, [opts])`

Defines an Options constructor that can be used to collect only the needed
options.

An optional `default` property for specs can be used to specify default values
if nothing was passed in.

If the value for a spec is a string, it will be treated as an alias to that
other key.

##### Example

```javascript
const MyAppOpts = figgyPudding({
  lg: 'log',
  log: {
    default: () => require('npmlog')
  },
  cache: {}
})
```

#### <a name="opts"></a> `> Opts(...providers)`

Instantiates an options object defined by `figgyPudding()`, which uses
`providers`, in order, to find requested properties.

Each provider can be either a plain object, a `Map`-like object (that is, one
with a `.get()` method) or another figgyPudding `Opts` object.

When nesting `Opts` objects, their properties will not become available to the
new object, but any further nested `Opts` that reference that property _will_ be
able to read from their grandparent, as long as they define that key. Default
values for nested `Opts` parents will be used, if found.

##### Example

```javascript
const ReqOpts = figgyPudding({
  follow: {}
})

const opts = ReqOpts({
  follow: true,
  log: require('npmlog')
})

opts.get('follow') // => true
opts.get('log') // => Error: ReqOpts does not define `log`

const MoreOpts = figgyPudding({
  log: {}
})
MoreOpts(opts).get('log') // => npmlog object (passed in from original plain obj)
MoreOpts(opts).get('follow') // => Error: MoreOpts does not define `follow`
```

#### <a name="opts-get"></a> `> opts.get(key)`

Gets a value from the options object.

##### Example

```js
const opts = MyOpts(config)
opts.get('foo') // value of `foo`
```

#### <a name="opts-concat"></a> `> opts.concat(...moreProviders)`

Creates a new opts object of the same type as `opts` with additional providers.
Providers further to the right shadow providers to the left, with properties in
the original `opts` being shadows by the new providers.

##### Example

```js
const opts = MyOpts({x: 1})
opts.get('x') // 1
opts.concat({x: 2}).get('x') // 2
opts.get('x') // 1 (original opts object left intact)
```

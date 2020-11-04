# minipass-json-stream

Like [JSONStream](http://npm.im/JSONStream), but using Minipass streams

## install

```
npm install minipass-json-stream
```

## example

```js

const request = require('request')
const JSONStream = require('minipass-json-stream')
const es = require('event-stream')

request({url: 'http://isaacs.couchone.com/registry/_all_docs'})
  .pipe(JSONStream.parse('rows.*'))
  .pipe(es.mapSync(function (data) {
    console.error(data)
    return data
  }))
```

## new JSONStream(options)

Create a new stream.  This is a [minipass](http://npm.im/minipass) stream
that is always set in `objectMode`.  It emits objects parsed out of
string/buffer JSON input that match the supplied `path` option.

## JSONStream.parse(path)

Return a new JSONStream object to stream values that match a path.

(Equivalent to `new JSONStream({path})`.)

``` js
JSONStream.parse('rows.*.doc')
```

The `..` operator is the recursive descent operator from
[JSONPath](http://goessner.net/articles/JsonPath/), which will match a
child at any depth (see examples below).

If your keys have keys that include `.` or `*` etc, use an array instead.
`['row', true, /^doc/]`.

If you use an array, `RegExp`s, booleans, and/or functions. The `..`
operator is also available in array representation, using `{recurse:
true}`.  any object that matches the path will be emitted as 'data' (and
`pipe`d down stream)

If `path` is empty or null, no 'data' events are emitted.

If you want to have keys emitted, you can prefix your `*` operator with
`$`: `obj.$*` - in this case the data passed to the stream is an object
with a `key` holding the key and a `value` property holding the data.

### Examples

query a couchdb view:

``` bash
curl -sS localhost:5984/tests/_all_docs&include_docs=true
```
you will get something like this:

``` js
{"total_rows":129,"offset":0,"rows":[
  { "id":"change1_0.6995461115147918"
  , "key":"change1_0.6995461115147918"
  , "value":{"rev":"1-e240bae28c7bb3667f02760f6398d508"}
  , "doc":{
      "_id":  "change1_0.6995461115147918"
    , "_rev": "1-e240bae28c7bb3667f02760f6398d508","hello":1}
  },
  { "id":"change2_0.6995461115147918"
  , "key":"change2_0.6995461115147918"
  , "value":{"rev":"1-13677d36b98c0c075145bb8975105153"}
  , "doc":{
      "_id":"change2_0.6995461115147918"
    , "_rev":"1-13677d36b98c0c075145bb8975105153"
    , "hello":2
    }
  },
]}
```

we are probably most interested in the `rows.*.doc`

create a `JSONStream` that parses the documents from the feed like this:

``` js
var stream = JSONStream.parse(['rows', true, 'doc']) //rows, ANYTHING, doc

stream.on('data', function(data) {
  console.log('received:', data);
});

//emits anything from _before_ the first match
stream.on('header', function (data) {
  console.log('header:', data) // => {"total_rows":129,"offset":0}
})
```

awesome!

In case you wanted the contents the doc emitted:

``` js
// equivalent to: 'rows.*.doc.$*'
var stream = JSONStream.parse([
  'rows',
  true,
  'doc',
  {emitKey: true}
]) //rows, ANYTHING, doc, items in docs with keys

stream.on('data', function(data) {
  console.log('key:', data.key);
  console.log('value:', data.value);
});
```

You can also emit the path:

``` js
var stream = JSONStream.parse([
  'rows',
  true,
  'doc',
  {emitPath: true}
]) //rows, ANYTHING, doc, items in docs with keys

stream.on('data', function(data) {
  console.log('path:', data.path);
  console.log('value:', data.value);
});
```

### recursive patterns (..)

`JSONStream.parse('docs..value')` 
(or `JSONStream.parse(['docs', {recurse: true}, 'value'])` using an array)
will emit every `value` object that is a child, grand-child, etc. of the 
`docs` object. In this example, it will match exactly 5 times at various depth
levels, emitting 0, 1, 2, 3 and 4 as results.

```js
{
  "total": 5,
  "docs": [
    {
      "key": {
        "value": 0,
        "some": "property"
      }
    },
    {"value": 1},
    {"value": 2},
    {"blbl": [{}, {"a":0, "b":1, "value":3}, 10]},
    {"value": 4}
  ]
}
```

## JSONStream.parse(pattern, map)

(Equivalent to `new JSONStream({ pattern, map })`)

provide a function that can be used to map or filter
the json output. `map` is passed the value at that node of the pattern,
if `map` return non-nullish (anything but `null` or `undefined`)
that value will be emitted in the stream. If it returns a nullish value,
nothing will be emitted.

`JSONStream` also emits `'header'` and `'footer'` events,
the `'header'` event contains anything in the output that was before
the first match, and the `'footer'`, is anything after the last match.

## Acknowlegements

This module is a fork of [JSONStream](http://npm.im/JSONStream) by Dominic
Tarr, modified and redistributed under the terms of the MIT license.

this module depends on https://github.com/creationix/jsonparse
by Tim Caswell

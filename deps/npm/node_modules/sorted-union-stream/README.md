# sorted-union-stream

Get the union of two sorted streams

```
npm install sorted-union-stream
```

[![build status](https://secure.travis-ci.org/mafintosh/sorted-union-stream.png)](http://travis-ci.org/mafintosh/sorted-union-stream)

## Usage

``` js
var union = require('sorted-union-stream')
var from = require('from2-array')

// es.readArray converts an array into a stream
var sorted1 = from.obj([1,10,24,42,43,50,55])
var sorted2 = from.obj([10,42,53,55,60])

// combine the two streams into a single sorted stream
var u = union(sorted1, sorted2)

u.on('data', function(data) {
  console.log(data)
})
u.on('end', function() {
  console.log('no more data')
})
```

Running the above example will print

```
1
10
24
42
43
50
53
55
60
no more data
```

## Streaming objects

If you are streaming objects sorting is based on `.key`.

If this property is not present you should add a `toKey` function as the third parameter.
`toKey` should return an key representation of the data that can be used to compare objects.

_The keys MUST be sorted_

``` js
var sorted1 = from.obj([{foo:'a'}, {foo:'b'}, {foo:'c'}])
var sorted2 = from.obj([{foo:'b'}, {foo:'d'}])

var u = union(sorted1, sorted2, function(data) {
  return data.foo // the foo property is sorted
})

union.on('data', function(data) {
  console.log(data)
});
```

Running the above will print

```
{foo:'a'}
{foo:'b'}
{foo:'c'}
{foo:'d'}
```

## License

MIT

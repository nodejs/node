# stream-iterate

Iterate through the values in a stream.

```
npm install stream-iterate
```

[![build status](http://img.shields.io/travis/mafintosh/stream-iterate.svg?style=flat)](http://travis-ci.org/mafintosh/stream-iterate)

## Usage

``` js
var iterate = require('stream-iterate')
var from = require('from2')

var stream = from.obj(['a', 'b', 'c'])

var read = iterate(stream)

read(function (err, data, next) {
  console.log(err, data)
  next()
})
```

If you don't call `next` and call `read` again the same `(err, value)` pair will be returned.

You can use this module to implement stuff like [a streaming merge sort](https://github.com/mafintosh/stream-iterate/blob/master/test.js#L5-L47).

## License

[MIT](LICENSE)

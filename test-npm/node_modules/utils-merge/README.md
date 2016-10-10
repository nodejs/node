# utils-merge

Merges the properties from a source object into a destination object.

## Install

    $ npm install utils-merge

## Usage

```javascript
var a = { foo: 'bar' }
  , b = { bar: 'baz' };

merge(a, b);
// => { foo: 'bar', bar: 'baz' }
```

## Tests

    $ npm install
    $ npm test

[![Build Status](https://secure.travis-ci.org/jaredhanson/utils-merge.png)](http://travis-ci.org/jaredhanson/utils-merge)

## Credits

  - [Jared Hanson](http://github.com/jaredhanson)

## License

[The MIT License](http://opensource.org/licenses/MIT)

Copyright (c) 2013 Jared Hanson <[http://jaredhanson.net/](http://jaredhanson.net/)>

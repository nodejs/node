self.Flatted = (function (exports) {
  'use strict';

  

  /*! (c) 2020 Andrea Giammarchi */
  var $parse = JSON.parse,
      $stringify = JSON.stringify;
  var keys = Object.keys;
  var Primitive = String; // it could be Number

  var primitive = 'string'; // it could be 'number'

  var ignore = {};
  var object = 'object';

  var noop = function noop(_, value) {
    return value;
  };

  var primitives = function primitives(value) {
    return value instanceof Primitive ? Primitive(value) : value;
  };

  var Primitives = function Primitives(_, value) {
    return typeof(value) === primitive ? new Primitive(value) : value;
  };

  var revive = function revive(input, parsed, output, $) {
    var lazy = [];

    for (var ke = keys(output), length = ke.length, y = 0; y < length; y++) {
      var k = ke[y];
      var value = output[k];

      if (value instanceof Primitive) {
        var tmp = input[value];

        if (typeof(tmp) === object && !parsed.has(tmp)) {
          parsed.add(tmp);
          output[k] = ignore;
          lazy.push({
            k: k,
            a: [input, parsed, tmp, $]
          });
        } else output[k] = $.call(output, k, tmp);
      } else if (output[k] !== ignore) output[k] = $.call(output, k, value);
    }

    for (var _length = lazy.length, i = 0; i < _length; i++) {
      var _lazy$i = lazy[i],
          _k = _lazy$i.k,
          a = _lazy$i.a;
      output[_k] = $.call(output, _k, revive.apply(null, a));
    }

    return output;
  };

  var set = function set(known, input, value) {
    var index = Primitive(input.push(value) - 1);
    known.set(value, index);
    return index;
  };

  var parse = function parse(text, reviver) {
    var input = $parse(text, Primitives).map(primitives);
    var value = input[0];
    var $ = reviver || noop;
    var tmp = typeof(value) === object && value ? revive(input, new Set(), value, $) : value;
    return $.call({
      '': tmp
    }, '', tmp);
  };
  var stringify = function stringify(value, replacer, space) {
    var $ = replacer && typeof(replacer) === object ? function (k, v) {
      return k === '' || -1 < replacer.indexOf(k) ? v : void 0;
    } : replacer || noop;
    var known = new Map();
    var input = [];
    var output = [];
    var i = +set(known, input, $.call({
      '': value
    }, '', value));
    var firstRun = !i;

    while (i < input.length) {
      firstRun = true;
      output[i] = $stringify(input[i++], replace, space);
    }

    return '[' + output.join(',') + ']';

    function replace(key, value) {
      if (firstRun) {
        firstRun = !firstRun;
        return value;
      }

      var after = $.call(this, key, value);

      switch (typeof(after)) {
        case object:
          if (after === null) return after;

        case primitive:
          return known.get(after) || set(known, input, after);
      }

      return after;
    }
  };

  exports.parse = parse;
  exports.stringify = stringify;

  return exports;

}({}));

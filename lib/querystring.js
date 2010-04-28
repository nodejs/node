// Query String Utilities

var QueryString = exports;

QueryString.unescape = function (str, decodeSpaces) {
  return decodeURIComponent(decodeSpaces ? str.replace(/\+/g, " ") : str);
};

QueryString.escape = function (str) {
  return encodeURIComponent(str);
};


var stack = [];
/**
 * <p>Converts an arbitrary value to a Query String representation.</p>
 *
 * <p>Objects with cyclical references will trigger an exception.</p>
 *
 * @method stringify
 * @param obj {Variant} any arbitrary value to convert to query string
 * @param sep {String} (optional) Character that should join param k=v pairs together. Default: "&"
 * @param eq  {String} (optional) Character that should join keys to their values. Default: "="
 * @param munge {Boolean} (optional) Indicate whether array/object params should be munged, PHP/Rails-style. Default: true
 * @param name {String} (optional) Name of the current key, for handling children recursively.
 * @static
 */
QueryString.stringify = function (obj, sep, eq, munge, name) {
  munge = typeof(munge) == "undefined" ? true : munge;
  sep = sep || "&";
  eq = eq || "=";
  if (isA(obj, null) || isA(obj, undefined) || typeof(obj) === 'function') {
    return name ? encodeURIComponent(name) + eq : '';
  }

  if (isBool(obj)) obj = +obj;
  if (isNumber(obj) || isString(obj)) {
    return encodeURIComponent(name) + eq + encodeURIComponent(obj);
  }
  if (isA(obj, [])) {
    var s = [];
    name = name+(munge ? '[]' : '');
    for (var i = 0, l = obj.length; i < l; i ++) {
      s.push( QueryString.stringify(obj[i], sep, eq, munge, name) );
    }
    return s.join(sep);
  }
  // now we know it's an object.

  // Check for cyclical references in nested objects
  for (var i = stack.length - 1; i >= 0; --i) if (stack[i] === obj) {
    throw new Error("querystring.stringify. Cyclical reference");
  }

  stack.push(obj);

  var s = [];
  var begin = name ? name + '[' : '';
  var end = name ? ']' : '';
  var keys = Object.keys(obj);
  for (var i = 0, l = keys.length; i < l; i++) {
    var key = keys[i];
    var n = begin + key + end;
    s.push(QueryString.stringify(obj[key], sep, eq, munge, n));
  }

  stack.pop();

  s = s.join(sep);
  if (!s && name) return name + "=";
  return s;
};

QueryString.parseQuery = QueryString.parse = function (qs, sep, eq) {
  return (qs || '')
    .split(sep||"&")
    .map(pieceParser(eq||"="))
    .reduce(mergeParams);
};

// Parse a key=val string.
// These can get pretty hairy
// example flow:
// parse(foo[bar][][bla]=baz)
// return parse(foo[bar][][bla],"baz")
// return parse(foo[bar][], {bla : "baz"})
// return parse(foo[bar], [{bla:"baz"}])
// return parse(foo, {bar:[{bla:"baz"}]})
// return {foo:{bar:[{bla:"baz"}]}}
var trimmerPattern = /^\s+|\s+$/g,
  slicerPattern = /(.*)\[([^\]]*)\]$/;
var pieceParser = function (eq) {
  return function parsePiece (key, val) {
    if (arguments.length !== 2) {
      // key=val, called from the map/reduce
       key = key.split(eq);
      return parsePiece(
        QueryString.unescape(key.shift(), true),
        QueryString.unescape(key.join(eq), true)
      );
    }
    key = key.replace(trimmerPattern, '');
    if (isString(val)) {
      val = val.replace(trimmerPattern, '');
      // convert numerals to numbers
      if (!isNaN(val)) {
        var numVal = +val;
        if (val === numVal.toString(10)) val = numVal;
      }
    }
    var sliced = slicerPattern.exec(key);
    if (!sliced) {
      var ret = {};
      if (key) ret[key] = val;
      return ret;
    }
    // ["foo[][bar][][baz]", "foo[][bar][]", "baz"]
    var tail = sliced[2], head = sliced[1];

    // array: key[]=val
    if (!tail) return parsePiece(head, [val]);

    // obj: key[subkey]=val
    var ret = {};
    ret[tail] = val;
    return parsePiece(head, ret);
  };
};

// the reducer function that merges each query piece together into one set of params
function mergeParams (params, addition) {
  return (
    // if it's uncontested, then just return the addition.
    (!params) ? addition
    // if the existing value is an array, then concat it.
    : (isA(params, [])) ? params.concat(addition)
    // if the existing value is not an array, and either are not objects, arrayify it.
    : (!isA(params, {}) || !isA(addition, {})) ? [params].concat(addition)
    // else merge them as objects, which is a little more complex
    : mergeObjects(params, addition)
  );
};

// Merge two *objects* together. If this is called, we've already ruled
// out the simple cases, and need to do a loop.
function mergeObjects (params, addition) {
  var keys = Object.keys(addition);
  for (var i = 0, l = keys.length; i < l; i++) {
    var key = keys[i];
    if (key) {
      params[key] = mergeParams(params[key], addition[key]);
    }
  }
  return params;
};

// duck typing
function isA (thing, canon) {
  return (
    // truthiness. you can feel it in your gut.
    (!thing === !canon)
    // typeof is usually "object"
    && typeof(thing) === typeof(canon)
    // check the constructor
    && Object.prototype.toString.call(thing) === Object.prototype.toString.call(canon)
  );
};
function isBool (thing) {
  return (
    typeof(thing) === "boolean"
    || isA(thing, new Boolean(thing))
  );
};
function isNumber (thing) {
  return (
    typeof(thing) === "number"
    || isA(thing, new Number(thing))
  ) && isFinite(thing);
};
function isString (thing) {
  return (
    typeof(thing) === "string"
    || isA(thing, new String(thing))
  );
};

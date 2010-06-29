// Query String Utilities

var QueryString = exports;
var urlDecode = process.binding("http_parser").urlDecode;

// a safe fast alternative to decodeURIComponent
QueryString.unescape = urlDecode;

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
QueryString.stringify = QueryString.encode = function (obj, sep, eq, munge, name) {
  munge = typeof munge == "undefined" || munge;
  sep = sep || "&";
  eq = eq || "=";
  if (obj == null || typeof obj == "function") {
    return name ? QueryString.escape(name) + eq : "";
  }

  if (isBool(obj)) {
    obj = +obj;
  }
  if (isNumber(obj) || isString(obj)) {
    return QueryString.escape(name) + eq + QueryString.escape(obj);
  }
  if (isA(obj, [])) {
    name = name + (munge ? "[]" : "");
    return obj.map(function (item) {
      return QueryString.stringify(item, sep, eq, munge, name);
    }).join(sep);
  }
  // now we know it's an object.

  // Check for cyclical references in nested objects
  for (var i = stack.length - 1; i >= 0; --i) if (stack[i] === obj) {
    throw new Error("querystring.stringify. Cyclical reference");
  }

  stack.push(obj);

  var begin = name ? name + "[" : "",
      end = name ? "]" : "",
      keys = Object.keys(obj),
      n,
      s = Object.keys(obj).map(function (key) {
        n = begin + key + end;
        return QueryString.stringify(obj[key], sep, eq, munge, n);
      }).join(sep);

  stack.pop();

  if (!s && name) {
    return name + "=";
  }
  return s;
};

// matches .xxxxx or [xxxxx] or ['xxxxx'] or ["xxxxx"] with optional [] at the end
var chunks = /(?:(?:^|\.)([^\[\(\.]+)(?=\[|\.|$|\()|\[([^"'][^\]]*?)\]|\["([^\]"]*?)"\]|\['([^\]']*?)'\])(\[\])?/g;
// Parse a key=val string.
QueryString.parse = QueryString.decode = function (qs, sep, eq) {
  var obj = {};
  String(qs).split(sep || "&").map(function (keyValue) {
    var res = obj,
      next,
      kv = keyValue.split(eq || "="),
      key = QueryString.unescape(kv.shift(), true),
      value = QueryString.unescape(kv.join(eq || "="), true);
    key.replace(chunks, function (all, name, nameInBrackets, nameIn2Quotes, nameIn1Quotes, isArray, offset) {
      var end = offset + all.length == key.length;
      name = name || nameInBrackets || nameIn2Quotes || nameIn1Quotes;
      next = end ? value : {};
      next = next && (+next == next ? +next : next);
      if (Array.isArray(res[name])) {
        res[name].push(next);
        res = next;
      } else {
        if (name in res) {
          if (isArray || end) {
            res = (res[name] = [res[name], next])[1];
          } else {
            res = res[name];
          }
        } else {
          if (isArray) {
            res = (res[name] = [next])[0];
          } else {
            res = res[name] = next;
          }
        }
      }
    });
  });
  return obj;
};

function isA (thing, canon) {
  // special case for null and undefined
  if (thing == null || canon == null) {
    return thing === canon;
  }
  return Object.getPrototypeOf(Object(thing)) == Object.getPrototypeOf(Object(canon));
}
function isBool (thing) {
  return isA(thing, true);
}
function isNumber (thing) {
  return isA(thing, 0) && isFinite(thing);
}
function isString (thing) {
  return isA(thing, "");
}
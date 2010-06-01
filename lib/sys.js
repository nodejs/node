var events = require('events');


exports.print = function () {
  for (var i = 0, len = arguments.length; i < len; ++i) {
    process.stdout.write(arguments[i]);
  }
};


exports.puts = function () {
  for (var i = 0, len = arguments.length; i < len; ++i) {
    process.stdout.write(arguments[i] + '\n');
  }
};


exports.debug = function (x) {
  process.binding('stdio').writeError("DEBUG: " + x + "\n");
};


var error = exports.error = function (x) {
  for (var i = 0, len = arguments.length; i < len; ++i) {
    process.binding('stdio').writeError(arguments[i] + '\n');
  }
};


/**
 * Echos the value of a value. Trys to print the value out
 * in the best way possible given the different types.
 *
 * @param {Object} value The object to print out
 * @param {Boolean} showHidden Flag that shows hidden (not enumerable)
 * properties of objects.
 */
exports.inspect = function (obj, showHidden, depth) {
  var seen = [];
  function format(value, recurseTimes) {
    // Provide a hook for user-specified inspect functions.
    // Check that value is an object with an inspect function on it
    if (value && typeof value.inspect === 'function' &&
        // Filter out the sys module, it's inspect function is special
        value !== exports &&
        // Also filter out any prototype objects using the circular check.
        !(value.constructor && value.constructor.prototype === value)) {
      return value.inspect(recurseTimes);
    }

    // Primitive types cannot have properties
    switch (typeof value) {
      case 'undefined': return 'undefined';
      case 'string':    return JSON.stringify(value).replace(/'/g, "\\'")
                                                    .replace(/\\"/g, '"')
                                                    .replace(/(^"|"$)/g, "'");
      case 'number':    return '' + value;
      case 'boolean':   return '' + value;
    }
    // For some reason typeof null is "object", so special case here.
    if (value === null) {
      return 'null';
    }

    // Look up the keys of the object.
    if (showHidden) {
      var keys = Object.getOwnPropertyNames(value).map(function (key) { return '' + key; });
    } else {
      var keys = Object.keys(value);
    }

    var visible_keys = Object.keys(value);

    // Functions without properties can be shortcutted.
    if (typeof value === 'function' && keys.length === 0) {
      if (isRegExp(value)) {
        return '' + value;
      } else {
        return '[Function]';
      }
    }

    // Dates without properties can be shortcutted
    if (isDate(value) && keys.length === 0) {
        return value.toUTCString();
    }

    var base, type, braces;
    // Determine the object type
    if (isArray(value)) {
      type = 'Array';
      braces = ["[", "]"];
    } else {
      type = 'Object';
      braces = ["{", "}"];
    }

    // Make functions say that they are functions
    if (typeof value === 'function') {
      base = (isRegExp(value)) ? ' ' + value : ' [Function]';
    } else {
      base = "";
    }

    // Make dates with properties first say the date
    if (isDate(value)) {
      base = ' ' + value.toUTCString();
    }

    seen.push(value);

    if (keys.length === 0) {
      return braces[0] + base + braces[1];
    }

    if (recurseTimes < 0) {
      if (isRegExp(value)) {
        return '' + value;
      } else {
        return "[Object]";
      }
    }

    output = keys.map(function (key) {
      var name, str;
      if (value.__lookupGetter__) {
        if (value.__lookupGetter__(key)) {
          if (value.__lookupSetter__(key)) {
            str = "[Getter/Setter]";
          } else {
            str = "[Getter]";
          }
        } else {
          if (value.__lookupSetter__(key)) {
            str = "[Setter]";
          }
        }
      }
      if (visible_keys.indexOf(key) < 0) {
        name = "[" + key + "]";
      }
      if (!str) {
        if (seen.indexOf(value[key]) < 0) {
          if ( recurseTimes === null) {
            str = format(value[key]);
          }
          else {
            str = format(value[key], recurseTimes - 1);
          }
          if (str.indexOf('\n') > -1) {
            if (isArray(value)) {
              str = str.split('\n').map(function(line) {
                return '  ' + line;
              }).join('\n').substr(2);
            }
            else {
              str = '\n' + str.split('\n').map(function(line) {
                return '   ' + line;
              }).join('\n');
            }
          }
        } else {
          str = '[Circular]';
        }
      }
      if (typeof name === 'undefined') {
        if (type === 'Array' && key.match(/^\d+$/)) {
          return str;
        }
        name = JSON.stringify('' + key);
        if (name.match(/^"([a-zA-Z_][a-zA-Z_0-9]*)"$/)) {
          name = name.substr(1, name.length-2);
        }
        else {
          name = name.replace(/'/g, "\\'")
                     .replace(/\\"/g, '"')
                     .replace(/(^"|"$)/g, "'");
        }
      }

      return name + ": " + str;
    });

    var numLinesEst = 0;
    var length = output.reduce(function(prev, cur) {
        numLinesEst++;
        if( cur.indexOf('\n') >= 0 ) {
          numLinesEst++;
        }
        return prev + cur.length + 1;
      },0);

    if (length > 50) {
      output = braces[0]
             + (base === '' ? '' : base + '\n ')
             + ' '
             + output.join('\n, ')
             + (numLinesEst > 1 ? '\n' : ' ')
             + braces[1]
             ;
    }
    else {
      output = braces[0] + base + ' ' + output.join(', ') + ' ' + braces[1];
    }

    return output;
  }
  return format(obj, (typeof depth === 'undefined' ? 2 : depth));
};


function isArray (ar) {
  return ar instanceof Array
      || Array.isArray(ar)
      || (ar && ar !== Object.prototype && isArray(ar.__proto__));
}


function isRegExp (re) {
  var s = ""+re;
  return re instanceof RegExp // easy case
      || typeof(re) === "function" // duck-type for context-switching evalcx case
      && re.constructor.name === "RegExp"
      && re.compile
      && re.test
      && re.exec
      && s.charAt(0) === "/"
      && s.substr(-1) === "/";
}


function isDate (d) {
  if (d instanceof Date) return true;
  if (typeof d !== "object") return false;
  var properties = Date.prototype && Object.getOwnPropertyNames(Date.prototype);
  var proto = d.__proto__ && Object.getOwnPropertyNames(d.__proto__);
  return JSON.stringify(proto) === JSON.stringify(properties);
}


var pWarning;

exports.p = function () {
  if (!pWarning) {
    pWarning = "sys.p will be removed in future versions of Node. Use sys.puts(sys.inspect()) instead.\n";
    exports.error(pWarning);
  }
  for (var i = 0, len = arguments.length; i < len; ++i) {
    error(exports.inspect(arguments[i]));
  }
};


function pad (n) {
  return n < 10 ? '0' + n.toString(10) : n.toString(10);
}


var months = ['Jan', 'Feb', 'Mar', 'Apr', 'May', 'Jun', 'Jul', 'Aug', 'Sep', 'Oct', 'Nov', 'Dec'];

// 26 Feb 16:19:34
function timestamp () {
  var d = new Date();
  return  [ d.getDate()
          , months[d.getMonth()]
          , [pad(d.getHours()), pad(d.getMinutes()), pad(d.getSeconds())].join(':')
          ].join(' ');
}


exports.log = function (msg) {
  exports.puts(timestamp() + ' - ' + msg.toString());
};


var execWarning;
exports.exec = function () {
  if (!execWarning) {
    execWarning = 'sys.exec has moved to the "child_process" module. Please update your source code.'
    error(execWarning);
  }
  return require('child_process').exec.apply(this, arguments);
};


/**
 * Inherit the prototype methods from one constructor into another.
 *
 * The Function.prototype.inherits from lang.js rewritten as a standalone
 * function (not on Function.prototype). NOTE: If this file is to be loaded
 * during bootstrapping this function needs to be revritten using some native
 * functions as prototype setup using normal JavaScript does not work as
 * expected during bootstrapping (see mirror.js in r114903).
 *
 * @param {function} ctor Constructor function which needs to inherit the
 *     prototype
 * @param {function} superCtor Constructor function to inherit prototype from
 */
exports.inherits = function (ctor, superCtor) {
  var tempCtor = function(){};
  tempCtor.prototype = superCtor.prototype;
  ctor.super_ = superCtor;
  ctor.prototype = new tempCtor();
  ctor.prototype.constructor = ctor;
};

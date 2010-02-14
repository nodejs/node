var events = require('events');

exports.print = function () {
  for (var i = 0, len = arguments.length; i < len; ++i) {
    process.stdio.write(arguments[i]);
  }
};

exports.puts = function () {
  for (var i = 0, len = arguments.length; i < len; ++i) {
    process.stdio.write(arguments[i] + '\n');
  }
};

exports.debug = function (x) {
  process.stdio.writeError("DEBUG: " + x + "\n");
};

exports.error = function (x) {
  for (var i = 0, len = arguments.length; i < len; ++i) {
    process.stdio.writeError(arguments[i] + '\n');
  }
};

/**
 * Echos the value of a value. Trys to print the value out
 * in the best way possible given the different types.
 *
 * @param {Object} value The object to print out
 * @param {Boolean} showHidden Flag that shows hidden (not enumerable) properties of objects.
 */
exports.inspect = function (obj, showHidden) {
  var seen = [];
  function format(value) {
    // Primitive types cannot have properties
    switch (typeof value) {
      case 'undefined': return 'undefined';
      case 'string':    return JSON.stringify(value);
      case 'number':    return '' + value;
      case 'boolean':   return '' + value;
    }
    // For some reason typeof null is "object", so special case here.
    if (value === null) {
      return 'null';
    }

    // Look up the keys of the object.
    var keys = showHidden ? Object.getOwnPropertyNames(value).map(function (key) {
      return '' + key;
    }) : Object.keys(value);
    var visible_keys = Object.keys(value);

    // Functions without properties can be shortcutted.
    if (typeof value === 'function' && keys.length === 0) {
      if (value instanceof RegExp) {
        return '' + value;
      } else {
        return '[Function]';
      }
    }

    // Dates without properties can be shortcutted
    if (value instanceof Date && keys.length === 0) {
        return value.toUTCString();
    }

    var base, type, braces;
    // Determine the object type
    if (value instanceof Array) {
      type = 'Array';
      braces = ["[", "]"];
    } else {
      type = 'Object';
      braces = ["{", "}"];
    }

    // Make functions say that they are functions
    if (typeof value === 'function') {
      base = (value instanceof RegExp) ? ' ' + value : ' [Function]';
    } else {
      base = "";
    }
    
    // Make dates with properties first say the date
    if (value instanceof Date) {
      base = ' ' + value.toUTCString();
    }

    seen.push(value);

    if (keys.length === 0) {
      return braces[0] + base + braces[1];
    }

    return braces[0] + base + "\n" + (keys.map(function (key) {
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
          str = format(value[key]);
        } else {
          str = '[Circular]';
        }
      }
      if (typeof name === 'undefined') {
        if (type === 'Array' && key.match(/^\d+$/)) {
          return str;
        }
        name = JSON.stringify('' + key);
      }

      return name + ": " + str;
    }).join(",\n")).split("\n").map(function (line) {
      return ' ' + line;
    }).join('\n') + "\n" + braces[1];
  }
  return format(obj);
};

exports.p = function () {
  for (var i = 0, len = arguments.length; i < len; ++i) {
    exports.error(exports.inspect(arguments[i]));
  }
};

exports.exec = function (command) {
  var child = process.createChildProcess("/bin/sh", ["-c", command]);
  var stdout = "";
  var stderr = "";
  var promise = new events.Promise();

  child.addListener("output", function (chunk) {
    if (chunk) stdout += chunk;
  });

  child.addListener("error", function (chunk) {
    if (chunk) stderr += chunk;
  });

  child.addListener("exit", function (code) {
    if (code == 0) {
      promise.emitSuccess(stdout, stderr);
    } else {
      promise.emitError(code, stdout, stderr);
    }
  });

  return promise;
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
exports.inherits = process.inherits;


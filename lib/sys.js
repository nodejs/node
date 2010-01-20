var events = require('events');

exports.print = function (x) {
  process.stdio.write(x);
};

exports.puts = function (x) {
  process.stdio.write(x + "\n");
};

exports.debug = function (x) {
  process.stdio.writeError("DEBUG: " + x + "\n");
};

exports.error = function (x) {
  process.stdio.writeError(x + "\n");
};

/**
 * Echos the value of a value. Trys to print the value out
 * in the best way possible given the different types.
 *
 * @param {Object} value The object to print out
 */
exports.inspect = function (value) {
  return formatter(value, '', []);
};

exports.p = function (x) {
  exports.error(exports.inspect(x));
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

/**
 * A recursive function to format an object - used by inspect.
 *
 * @param {Object} value
 *   the value to format
 * @param {String} indent
 *   the indent level of any nested objects, since they are formatted over
 *   more than one line
 * @param {Array} parents
 *   contains all objects above the current one in the heirachy, used to
 *   prevent getting stuck in a loop on circular references
 */
var formatter = function(value, indent, parents) {
  switch(typeof(value)) {
    case 'string':    return JSON.stringify(value);
    case 'number':    return '' + value;
    case 'function':  return '[Function]';
    case 'boolean':   return '' + value;
    case 'undefined': return 'undefined';
    case 'object':
      if (value == null) return 'null';
      if (parents.indexOf(value) >= 0) return '[Circular]';
      parents.push(value);

      if (value instanceof Array && Object.keys(value).length === value.length) {
        return formatObject(value, indent, parents, '[]', function(x, f) {
          return f(value[x]);
        });
      } else {
        return formatObject(value, indent, parents, '{}', function(x, f) {
          var child;
          if (value.__lookupGetter__(x)) {
            if (value.__lookupSetter__(x)) {
              child = "[Getter/Setter]";
            } else {
              child = "[Getter]";
            }
          } else {
            if (value.__lookupSetter__(x)) {
              child = "[Setter]";
            } else {
              child = f(value[x]);
            }
          }
          return f(x) + ': ' + child;
        });
      }
      return buffer;
    default:
      throw('inspect unimplemented for ' + typeof(value));
  }
}

/**
 * Helper function for formatting either an array or an object, used internally by formatter
 */
var formatObject = function(obj, indent, parents, parenthesis, entryFormatter) {
  var buffer = parenthesis[0];
  var values = [];
  var x;

  var localFormatter = function(value) {
    return formatter(value, indent + ' ', parents);
  };
  for (x in obj) {
    values.push(indent + ' ' + entryFormatter(x, localFormatter));
  }
  if (values.length > 0) {
    buffer += "\n" + values.join(",\n") + "\n" + indent;
  }
  buffer += parenthesis[1];
  return buffer;
}

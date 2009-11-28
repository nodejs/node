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
  if (value === 0) return "0";
  if (value === false) return "false";
  if (value === "") return '""';
  if (typeof(value) == "function") return "[Function]";
  if (value === undefined) return;
  
  try {
    return JSON.stringify(value, undefined, 1);
  } catch (e) {
    // TODO make this recusrive and do a partial JSON output of object.
    if (e.message.search("circular")) {
      return "[Circular Object]";
    } else {
      throw e;
    }
  }
};

exports.p = function (x) {
  exports.error(exports.inspect(x));
};

exports.exec = function (command) {
  var child = process.createChildProcess("/bin/sh", ["-c", command]);
  var stdout = "";
  var stderr = "";
  var promise = new process.Promise();

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
exports.inherits = function (ctor, superCtor) {
  var tempCtor = function(){};
  tempCtor.prototype = superCtor.prototype;
  ctor.super_ = superCtor.prototype;
  ctor.prototype = new tempCtor();
  ctor.prototype.constructor = ctor;
};

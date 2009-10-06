exports.print = function (x) {
  node.stdio.write(x);
};

exports.puts = function (x) {
  node.stdio.write(x.toString() + "\n");
};

exports.debug = function (x) {
  node.stdio.writeError("DEBUG: " + x.toString() + "\n");
};

exports.error = function (x) {
  node.stdio.writeError(x.toString() + "\n");
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
    return JSON.stringify(value);
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
  // TODO TODO TODO
  // The following line needs to be replaced with proper command line
  // parsing. at the moment quoted strings will not be picked up as a single
  // argument.
  var args = command.split(/\s+/);

  var file = args.shift();

  var child = node.createChildProcess(file, args);
  var stdout = "";
  var stderr = "";
  var promise = new node.Promise();

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

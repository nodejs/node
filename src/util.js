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
node.inherits = function (ctor, superCtor) {
  var tempCtor = function(){};
  tempCtor.prototype = superCtor.prototype;
  ctor.super_ = superCtor.prototype;
  ctor.prototype = new tempCtor();
  ctor.prototype.constructor = ctor;
};

node.assert = function (x, msg) {
  if (!(x)) throw new Error(msg || "assertion error");
};

node.cat = function(location, encoding) {
  var url_re = new RegExp("^http:\/\/");
  var f = url_re.exec(location) ? node.http.cat : node.fs.cat;
  return f(location, encoding);
};

node.path = new function () {
  this.join = function () {
    var joined = "";
    for (var i = 0; i < arguments.length; i++) {
      var part = arguments[i].toString();

      /* Some logic to shorten paths */
      if (part === ".") continue;
      while (/^\.\//.exec(part)) part = part.replace(/^\.\//, "");

      if (i === 0) {
        part = part.replace(/\/*$/, "/");
      } else if (i === arguments.length - 1) {
        part = part.replace(/^\/*/, "");
      } else {
        part = part.replace(/^\/*/, "").replace(/\/*$/, "/");
      }
      joined += part;
    }
    return joined;
  };

  this.dirname = function (path) {
    if (path.charAt(0) !== "/") path = "./" + path;
    var parts = path.split("/");
    return parts.slice(0, parts.length-1).join("/");
  };

  this.filename = function (path) {
    if (path.charAt(0) !== "/") path = "./" + path;
    var parts = path.split("/");
    return parts[parts.length-1];
  };
};

print = function (x) {
  node.stdio.write(x);
};

puts = function (x) {
  print(x.toString() + "\n");
};

node.debug = function (x) {
  node.stdio.writeError("DEBUG: " + x.toString() + "\n");
};

node.error = function (x) {
  node.stdio.writeError(x.toString() + "\n");
};

/**
 * Echos the value of a value. Trys to print the value out
 * in the best way possible given the different types.
 * 
 * @param {Object} value The object to print out
 */
node.inspect = function (value) {
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

p = function (x) {
  node.error(node.inspect(x));
};

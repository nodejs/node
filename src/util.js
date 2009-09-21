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

p = function (x) {
  if (x === null) {
    node.error("null");
  } else if (x === NaN) {
    node.error("NaN");
  } else {
    node.error(JSON.stringify(x) || "undefined");
  }
};

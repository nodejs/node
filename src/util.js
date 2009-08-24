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

// This is useful for dealing with raw encodings.
node.encodeUtf8 = function (array) {
  var string = "";
  var i = 0;
  var c = c1 = c2 = 0;

  while (i < array.length) {
    c = array[i];

    if (c < 128) {
      string += String.fromCharCode(c);
      i++;
    } else if ((c > 191) && (c < 224)) {
      c2 = array[i+1];
      string += String.fromCharCode(((c & 31) << 6) | (c2 & 63));
      i += 2;
    } else {
      c2 = array[i+1];
      c3 = array[i+2];
      string += String.fromCharCode(((c & 15) << 12) | ((c2 & 63) << 6) | (c3 & 63));
      i += 3;
    }
  }

  return string;
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
  return node.stdio.write(x);
};

puts = function (x) {
  return print(x.toString() + "\n");
};

p = function (x) {
  return puts(JSON.stringify(x));
};

node.debug = function (x) {
  return node.stdio.writeError("DEBUG: " + x.toString() + "\n");
};

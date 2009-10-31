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
process.inherits = function (ctor, superCtor) {
  var tempCtor = function(){};
  tempCtor.prototype = superCtor.prototype;
  ctor.super_ = superCtor.prototype;
  ctor.prototype = new tempCtor();
  ctor.prototype.constructor = ctor;
};

process.assert = function (x, msg) {
  if (!(x)) throw new Error(msg || "assertion error");
};

process.cat = function(location, encoding) {
  var url_re = new RegExp("^http:\/\/");
  if (url_re.exec(location)) {
    throw new Error("process.cat for http urls is temporarally disabled.");
  }
  //var f = url_re.exec(location) ? process.http.cat : process.fs.cat;
  //return f(location, encoding);
  return process.fs.cat(location, encoding);
};




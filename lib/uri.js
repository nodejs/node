/**
 * uri.js
 * A URI parser, compliant with assorted RFCs, providing parsing and resolution utilities.
 **/

/*
Blatantly stolen with permission from Narwhal, which did the same from Chiron,
and bits taken from parseUri 1.2.1 (c) 2007 Steven Levithan <stevenlevithan.com> MIT License,
and probably also plagiarizing http://code.google.com/p/js-uri/ in some ways.
Most lines have been changed, please don't blame any of the above persons for
any errors in this file.

*/

exports.parse = uri_parse;
exports.format = uri_format;
exports.resolve = uri_resolve;
exports.resolveObject = uri_resolveObject;
exports.decode = uri_decode;

/****
Decode a URI, replacing + with space
*/
function uri_decode (s) {
  return decodeURIComponent(s.replace(/\+/g, ' '));
}

/**** expressionKeys
  members of a parsed URI object that you get
  from evaluting the strict regular expression.
*/
var expressionKeys = [
    "url",
    "protocol",
    "authorityRoot",
    "authority",
      "userInfo",
        "user",
        "password",
      "domain",
      "port",
    "path",
      "root",
      "directory",
      "file",
    "query",
    "anchor"
  ],
  strictExpression = new RegExp( /* url */
    "^" +
    "(?:" +
      "([^:/?#]+):" + /* protocol */
    ")?" +
    "(?:" +
      "(//)" + /* authorityRoot */
      "(" + /* authority */
        "(?:" +
          "(" + /* userInfo */
            "([^:@/]*)" + /* user */
            ":?" +
            "([^@/]*)" + /* password */
          ")?" +
          "@" +
        ")?" +
        "([^:/?#]*)" + /* domain */
        "(?::(\\d*))?" + /* port */
      ")" +
    ")?" +
    "(" + /* path */
      "(/?)" + /* root */
      "((?:[^?#/]*/)*)" +
      "([^?#]*)" + /* file */
    ")" +
    "(?:\\?([^#]*))?" + /* query */
    "(?:#(.*))?" /*anchor */
  );

/**** parse
  a URI parser function that uses the `strictExpression`
  and `expressionKeys` and returns an `Object`
  mapping all `keys` to values.
*/
function uri_parse (url) {
  var items = {},
    parts = strictExpression.exec(url);
  
  for (var i = 0; i < parts.length; i++) {
    items[expressionKeys[i]] = parts[i] ? parts[i] : "";
  }

  items.root = (items.root || items.authorityRoot) ? '/' : '';

  items.directories = items.directory.split("/");
  if (items.directories[items.directories.length - 1] == "") {
    items.directories.pop();
  }

  /* normalize */
  items.directories = require("path").normalizeArray(items.directories);

  items.domains = items.domain.split(".");

  return items;
};


/**** format
  accepts a parsed URI object and returns
  the corresponding string.
*/
function uri_format (object) {
  if (typeof(object) == 'undefined')
    throw new Error("UrlError: URL undefined for urls#format");
  if (object instanceof String || typeof(object) === 'string')
    return object;
  
  var domain =
    object.domains ?
    object.domains.join(".") :
    object.domain;
  var userInfo = (
      object.user ||
      object.password 
    ) ? (
      (object.user || "") + 
      (object.password ? ":" + object.password : "") 
    ) :
    object.userInfo;
  var authority = object.authority || ((
      userInfo ||
      domain ||
      object.port
    ) ? (
      (userInfo ? userInfo + "@" : "") +
      (domain || "") + 
      (object.port ? ":" + object.port : "")
    ) : "");
  
  var directory =
    object.directories ?
    object.directories.join("/") :
    object.directory;
  var path =
    object.path ? object.path.substr(1) : (
      (directory || object.file) ? (
        (directory ? directory + "/" : "") +
        (object.file || "")
      ) : "");
  var authorityRoot = 
    object.authorityRoot
    || authority ? "//" : "";
    
  return object.url = ((
    (object.protocol ? object.protocol + ":" : "") +
    (authorityRoot) +
    (authority) +
    (object.root || (authority && path) ? "/" : "") +
    (path ? path : "") +
    (object.query ? "?" + object.query : "") +
    (object.anchor ? "#" + object.anchor : "")
  ) || object.url || "");
};

function uri_resolveObject (source, relative) {
  if (!source) return relative;
  
  // parse a string, or get new objects
  source = uri_parse(uri_format(source));
  relative = uri_parse(uri_format(relative));
  
  if (relative.url === "") return source;
  
  // links to xyz:... from abc:... are always absolute.
  if (relative.protocol && source.protocol && relative.protocol !== source.protocol) {
    return relative;
  }
  
  // if there's an authority root, but no protocol, then keep the current protocol
  if (relative.authorityRoot && !relative.protocol) {
    relative.protocol = source.protocol;
  }
  // if we have an authority root, then it's absolute
  if (relative.authorityRoot) return relative;

  
  // at this point, we start doing the tricky stuff
  // we know that relative doesn't have an authority, but might have root,
  // path, file, query, etc.
  // also, the directory segments might contain .. or .
  // start mutating "source", and then return that.

  // relative urls that start with / are absolute to the authority/protocol
  if (relative.root) {
    [
      "path", "root", "directory", "directories", "file", "query", "anchor"
    ].forEach(function (part) { source[part] = relative[part] });
  } else {
    // if you have /a/b/c and you're going to x/y/z, then that's /a/b/x/y/z
    // if you have /a/b/c/ and you're going ot x/y/z, then that's /a/b/c/x/y/z
    // if you have /a/b/c and you're going to ?foo, then that's /a/b/c?foo
    if (relative.file || relative.directory) {
      source.file = relative.file;
      source.query = relative.query;
      source.anchor = relative.anchor;
    }
    if (relative.query) source.query = relative.query;
    if (relative.query || relative.anchor) source.anchor = relative.anchor;
    
    // just append the dirs. we'll sort out .. and . later
    source.directories = source.directories.concat(relative.directories);
  }
  
  // back up "file" to the first non-dot
  // one step for ., two for ..
  var file = source.file;
  while (file === ".." || file === ".") {
    if (file === "..") source.directories.pop();
    file = source.directories.pop();
  }
  source.file = file || "";
  
  // walk over the dirs, replacing a/b/c/.. with a/b/ and a/b/c/. with a/b/c
  var dirs = [];
  source.directories.forEach(function (dir, i) {
    if (dir === "..") dirs.pop();
    else if (dir !== "." && dir !== "") dirs.push(dir);
  });
  
  // now construct path/etc.
  source.directories = dirs;
  source.directory = dirs.concat("").join("/");
  source.path = source.root + source.directory + source.file;
  source.url = uri_format(source);
  return source;
};

/**** resolve
  returns a URL resovled to a relative URL from a source URL.
*/
function uri_resolve (source, relative) {
  return uri_format(uri_resolveObject(source, relative));
};


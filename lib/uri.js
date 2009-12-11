/**
 * uri.js
 * A URI parser, compliant with assorted RFCs, providing parsing and resolution utilities.
 **/

exports.parse = uri_parse;
exports.format = uri_format;
exports.resolve = uri_resolve;
exports.resolveObject = uri_resolveObject;


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
  var directories = [];
  for (var i = 0; i < items.directories.length; i++) {
    var directory = items.directories[i];
    if (directory == '.') {
    } else if (directory == '..') {
      if (directories.length && directories[directories.length - 1] != '..')
        directories.pop();
      else
        directories.push('..');
    } else {
      directories.push(directory);
    }
  }
  items.directories = directories;

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
  var authority = (
      userInfo ||
      domain ||
      object.port
    ) ? (
      (userInfo ? userInfo + "@" : "") +
      (domain || "") + 
      (object.port ? ":" + object.port : "")
    ) :
    object.authority || "";
    
  var directory =
    object.directories ?
    object.directories.join("/") :
    object.directory;
  var path =
    directory || object.file ?
    (
      (directory ? directory + "/" : "") +
      (object.file || "")
    ) :
    object.path;
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

/**** resolveObject
  returns an object representing a URL resolved from
  a relative location and a source location.
*/
function uri_resolveObject (source, relative) {
  if (!source) 
    return relative;

  source = uri_parse(source);
  relative = uri_parse(relative);

  if (relative.url == "")
    return source;

  delete source.url;
  delete source.authority;
  delete source.domain;
  delete source.userInfo;
  delete source.path;
  delete source.directory;

  if (
    relative.protocol && relative.protocol != source.protocol ||
    relative.authority && relative.authority != source.authority
  ) {
    source = relative;
  } else {
    if (relative.root) {
      source.directories = relative.directories;
    } else {

      var directories = relative.directories;
      for (var i = 0; i < directories.length; i++) {
        var directory = directories[i];
        if (directory == ".") {
        } else if (directory == "..") {
          if (source.directories.length) {
            source.directories.pop();
          } else {
            source.directories.push('..');
          }
        } else {
          source.directories.push(directory);
        }
      }

      if (relative.file == ".") {
        relative.file = "";
      } else if (relative.file == "..") {
        source.directories.pop();
        relative.file = "";
      }
    }
  }

  if (relative.root)
    source.root = relative.root;
  if (relative.protcol)
    source.protocol = relative.protocol;
  if (!(!relative.path && relative.anchor))
    source.file = relative.file;
  source.query = relative.query;
  source.anchor = relative.anchor;

  return source;
};


/**** resolve
  returns a URL resovled to a relative URL from a source URL.
*/
function uri_resolve (source, relative) {
  return uri_format(uri_resolveObject(source, relative));
};


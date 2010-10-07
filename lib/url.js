exports.parse = urlParse;
exports.resolve = urlResolve;
exports.resolveObject = urlResolveObject;
exports.format = urlFormat;

// define these here so at least they only have to be compiled once on the first module load.
var protocolPattern = /^([a-z0-9]+:)/,
  portPattern = /:[0-9]+$/,
  nonHostChars = ["/", "?", ";", "#"],
  hostlessProtocol = {
    "file":true,
    "file:":true
  },
  slashedProtocol = {
    "http":true, "https":true, "ftp":true, "gopher":true, "file":true,
    "http:":true, "https:":true, "ftp:":true, "gopher:":true, "file:":true
  },
  path = require("path"), // internal module, guaranteed to be loaded already.
  querystring = require('querystring');

function urlParse (url, parseQueryString, slashesDenoteHost) {
  if (url && typeof(url) === "object" && url.href) return url;

  var out = { href : url },
    rest = url;

  var proto = protocolPattern.exec(rest);
  if (proto) {
    proto = proto[0];
    out.protocol = proto;
    rest = rest.substr(proto.length);
  }

  // figure out if it's got a host
  // user@server is *always* interpreted as a hostname, and url
  // resolution will treat //foo/bar as host=foo,path=bar because that's
  // how the browser resolves relative URLs.
  if (slashesDenoteHost || proto || rest.match(/^\/\/[^@\/]+@[^@\/]+/)) {
    var slashes = rest.substr(0, 2) === "//";
    if (slashes && !(proto && hostlessProtocol[proto])) {
      rest = rest.substr(2);
      out.slashes = true;
    }
  }
  if (!hostlessProtocol[proto] && (slashes || (proto && !slashedProtocol[proto]))) {
    // there's a hostname.
    // the first instance of /, ?, ;, or # ends the host.
    // don't enforce full RFC correctness, just be unstupid about it.
    var firstNonHost = -1;
    for (var i = 0, l = nonHostChars.length; i < l; i ++) {
      var index = rest.indexOf(nonHostChars[i]);
      if (index !== -1 && (firstNonHost < 0 || index < firstNonHost)) firstNonHost = index;
    }
    if (firstNonHost !== -1) {
      out.host = rest.substr(0, firstNonHost);
      rest = rest.substr(firstNonHost);
    } else {
      out.host = rest;
      rest = "";
    }

    // pull out the auth and port.
    var p = parseHost(out.host);
    var keys = Object.keys(p);
    for (var i = 0, l = keys.length; i < l; i++) {
      var key = keys[i];
      out[key] = p[key];
    }
    // we've indicated that there is a hostname, so even if it's empty, it has to be present.
    out.hostname = out.hostname || "";
  }

  // now rest is set to the post-host stuff.
  // chop off from the tail first.
  var hash = rest.indexOf("#");
  if (hash !== -1) {
    // got a fragment string.
    out.hash = rest.substr(hash);
    rest = rest.slice(0, hash);
  }
  var qm = rest.indexOf("?");
  if (qm !== -1) {
    out.search = rest.substr(qm);
    out.query = rest.substr(qm+1);
    if (parseQueryString) {
      out.query = querystring.parse(out.query);
    }
    rest = rest.slice(0, qm);
  }
  if (rest) out.pathname = rest;

  return out;
};

// format a parsed object into a url string
function urlFormat (obj) {
  // ensure it's an object, and not a string url. If it's an obj, this is a no-op.
  // this way, you can call url_format() on strings to clean up potentially wonky urls.
  if (typeof(obj) === "string") obj = urlParse(obj);

  var protocol = obj.protocol || "",
    host = (obj.host !== undefined) ? obj.host
      : obj.hostname !== undefined ? (
        (obj.auth ? obj.auth + "@" : "")
        + obj.hostname
        + (obj.port ? ":" + obj.port : "")
      )
      : false,
    pathname = obj.pathname || "",
    search = obj.search || (
      obj.query && ( "?" + (
        typeof(obj.query) === "object"
        ? querystring.stringify(obj.query)
        : String(obj.query)
      ))
    ) || "",
    hash = obj.hash || "";

  if (protocol && protocol.substr(-1) !== ":") protocol += ":";

  // only the slashedProtocols get the //.  Not mailto:, xmpp:, etc.
  // unless they had them to begin with.
  if (obj.slashes || (!protocol || slashedProtocol[protocol]) && host !== false) {
    host = "//" + (host || "");
    if (pathname && pathname.charAt(0) !== "/") pathname = "/" + pathname;
  } else if (!host) host = "";

  if (hash && hash.charAt(0) !== "#") hash = "#" + hash;
  if (search && search.charAt(0) !== "?") search = "?" + search;

  return protocol + host + pathname + search + hash;
};

function urlResolve (source, relative) {
  return urlFormat(urlResolveObject(source, relative));
};

function urlResolveObject (source, relative) {
  if (!source) return relative;

  source = urlParse(urlFormat(source), false, true);
  relative = urlParse(urlFormat(relative), false, true);

  // hash is always overridden, no matter what.
  source.hash = relative.hash;

  if (relative.href === "") return source;

  // hrefs like //foo/bar always cut to the protocol.
  if (relative.slashes && !relative.protocol) {
    relative.protocol = source.protocol;
    return relative;
  }

  if (relative.protocol && relative.protocol !== source.protocol) {
    // if it's a known url protocol, then changing the protocol does weird things
    // first, if it's not file:, then we MUST have a host, and if there was a path
    // to begin with, then we MUST have a path.
    // if it is file:, then the host is dropped, because that's known to be hostless.
    // anything else is assumed to be absolute.

    if (!slashedProtocol[relative.protocol]) return relative;

    source.protocol = relative.protocol;
    if (!relative.host && !hostlessProtocol[relative.protocol]) {
      var relPath = (relative.pathname || "").split("/");
      while (relPath.length && !(relative.host = relPath.shift()));
      if (!relative.host) relative.host = "";
      if (relPath[0] !== "") relPath.unshift("");
      if (relPath.length < 2) relPath.unshift("");
      relative.pathname = relPath.join("/");
    }
    source.pathname = relative.pathname;
    source.search = relative.search;
    source.query = relative.query;
    source.host = relative.host || "";
    delete source.auth;
    delete source.hostname;
    source.port = relative.port;
    return source;
  }

  var isSourceAbs = (source.pathname && source.pathname.charAt(0) === "/"),
    isRelAbs = (
      relative.host !== undefined
      || relative.pathname && relative.pathname.charAt(0) === "/"
    ),
    mustEndAbs = (isRelAbs || isSourceAbs || (source.host && relative.pathname)),
    removeAllDots = mustEndAbs,
    srcPath = source.pathname && source.pathname.split("/") || [],
    relPath = relative.pathname && relative.pathname.split("/") || [],
    psychotic = source.protocol && !slashedProtocol[source.protocol] && source.host !== undefined;

  // if the url is a non-slashed url, then relative links like ../.. should be able
  // to crawl up to the hostname, as well.  This is strange.
  // source.protocol has already been set by now.
  // Later on, put the first path part into the host field.
  if ( psychotic ) {

    delete source.hostname;
    delete source.auth;
    delete source.port;
    if (source.host) {
      if (srcPath[0] === "") srcPath[0] = source.host;
      else srcPath.unshift(source.host);
    }
    delete source.host;

    if (relative.protocol) {
      delete relative.hostname;
      delete relative.auth;
      delete relative.port;
      if (relative.host) {
        if (relPath[0] === "") relPath[0] = relative.host;
        else relPath.unshift(relative.host);
      }
      delete relative.host;
    }
    mustEndAbs = mustEndAbs && (relPath[0] === "" || srcPath[0] === "");
  }

  if (isRelAbs) {
    // it's absolute.
    source.host = (relative.host || relative.host === "") ? relative.host : source.host;
    source.search = relative.search;
    source.query = relative.query;
    srcPath = relPath;
    // fall through to the dot-handling below.
  } else if (relPath.length) {
    // it's relative
    // throw away the existing file, and take the new path instead.
    if (!srcPath) srcPath = [];
    srcPath.pop();
    srcPath = srcPath.concat(relPath);
    source.search = relative.search;
    source.query = relative.query;
  } else if ("search" in relative) {
    // just pull out the search.
    // like href="?foo".
    // Put this after the other two cases because it simplifies the booleans
    if (psychotic) {
      source.host = srcPath.shift();
    }
    source.search = relative.search;
    source.query = relative.query;
    return source;
  }
  if (!srcPath.length) {
    // no path at all.  easy.
    // we've already handled the other stuff above.
    delete source.pathname;
    return source;
  }

  // resolve dots.
  // if a url ENDs in . or .., then it must get a trailing slash.
  // however, if it ends in anything else non-slashy, then it must NOT get a trailing slash.
  var last = srcPath.slice(-1)[0];
  var hasTrailingSlash = (
    (source.host || relative.host) && (last === "." || last === "..")
    || last === ""
  );

  // Figure out if this has to end up as an absolute url, or should continue to be relative.
  srcPath = path.normalizeArray(srcPath, true);
  if (srcPath.length === 1 && srcPath[0] === ".") srcPath = [];
  if (mustEndAbs || removeAllDots) {
    // all dots must go.
    var dirs = [];
    srcPath.forEach(function (dir, i) {
      if (dir === "..") dirs.pop();
      else if (dir !== ".") dirs.push(dir);
    });

    if (mustEndAbs && dirs[0] !== "") {
      dirs.unshift("");
    }
    srcPath = dirs;
  }
  if (hasTrailingSlash && (srcPath.length < 2 || srcPath.slice(-1)[0] !== "")) srcPath.push("");

  // put the host back
  if ( psychotic ) source.host = srcPath[0] === "" ? "" : srcPath.shift();

  mustEndAbs = mustEndAbs || (source.host && srcPath.length);

  if (mustEndAbs && srcPath[0] !== "") srcPath.unshift("");

  source.pathname = srcPath.join("/");

  return source;
};

function parseHost (host) {
  var out = {};
  var at = host.indexOf("@");
  if (at !== -1) {
    out.auth = host.substr(0, at);
    host = host.substr(at+1); // drop the @
  }
  var port = portPattern.exec(host);
  if (port) {
    port = port[0];
    out.port = port.substr(1);
    host = host.substr(0, host.length - port.length);
  }
  if (host) out.hostname = host;
  return out;
}

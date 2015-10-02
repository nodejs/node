/*
  TODO:
    - Reuse sockets (this means maintaining a global state object)? This could
      especially help with minimizing the overhead of queries over TCP
      connections
      - This could include allowing multiple questions per query, although that
        kind of reuse would only happen if we had a user-facing API to allow
        such kind of queries
      - This could also be realized by using some kind of queueing mechanism to
        avoid creating too many sockets
    - Global query Buffer cache? This is already done locally inside the
      main resolver to avoid creating a new Buffer every time we need to retry
      the same query on a different protocol or nameserver
    - Add file (and registry, on Windows) watchers for updates to system DNS
      settings
      - If this is implemented, how should this interact with settings
        overridden by users (e.g. via setServers())?
    - Support EDNS, DNSSEC
    - Allow complete override of system-level DNS options in resolve() instead
      (or in addition to?) being able to append to existing options
      (e.g. system has `use_vc` set, but user wants to un-set that for a
      particular resolve())

  NOTES:
    - PR #744 mentioned c-ares could return *mixed* IPv4 and IPv6 when
      `family === 0`, but this is not true (it's even stated in c-ares' TODO),
      so this implementation does not return mixed results either
*/
'use strict';

const TCPSocket = require('net').Socket;
const UDPSocket = require('dgram').Socket;

const isIP = require('net').isIP; // TODO: move implementation from cares_wrap
const NSSReqWrap = process.binding('nss_wrap').NSSReqWrap;
const NSSModule = process.binding('nss_module').NSSModule;

const cp = require('child_process');
const os = require('os');
const fs = require('fs');
const pathJoin = require('path').join;

const NSS_REQ_ERR_SUCCESS = 0;
const NSS_REQ_ERR_NOTFOUND = 1;
const NSS_REQ_ERR_UNAVAIL = 2;
const NSS_REQ_ERR_TRYAGAIN = 3;
const NSS_REQ_ERR_INTERNAL = 4;
const NSS_REQ_ERR_MALLOC = 5;
const HOSTNAME = os.hostname();
const REGEX_NSSWITCH_ACTION =
    /^(!)?(success|notfound|unavail|tryagain)=(return|continue)$/i;
const REGEX_ARRAY = /^\{?(.*)\}?$/;
const REGEX_OPTS =
    /^(ndots|timeout|attempts|rotate|inet6|use-vc)(?::(\d{1,2}))?$/;
const REGEX_TYPES = /^(nameserver|domain|search|sortlist|options|lookup) /;
const REGEX_WHITESPACE = /[ \f\t\v]+/g;
const REGEX_COMMENT = /[#;].*$/;
const LOOKUPNS = ['dns'];
const QTYPE_A = 1;
const QTYPE_PTR = 12;
const QTYPE_AAAA = 28;
const FLAG_HAS_IPv4 = 0x1;
const FLAG_HAS_IPv6 = 0x2;
const FLAG_HAS_IPv4_IPv6 = (FLAG_HAS_IPv4 | FLAG_HAS_IPv6);
const V4MAPPED = 0x08;
const ADDRCONFIG = 0x20;
const FLAG_RESOLVE_NSONLY = 0x01; // No local hosts file lookup
const FLAG_RESOLVE_NOSEARCH = 0x02; // No domain searching
const FLAG_RESOLVE_NOALIASES = 0x04; // No HOSTALIASES checking
const FLAG_RESOLVE_IGNTC = 0x08; // Ignore truncation errors (UDP)
const FLAG_RESOLVE_USEVC = 0x10; // Always use TCP a.k.a "Virtual Circuit"
const DNS_DEFAULTS = {
  // TODO: improve default nameserver for IPv6-only systems
  nameserver: [ '127.0.0.1' ],
  search: (~HOSTNAME.indexOf('.')
           ? [ HOSTNAME.slice(HOSTNAME.indexOf('.') + 1) ] : null),
  sortlist: null,
  options: {
    attempts: 2, // how many tries before giving up
    inet6: false, // query AAAA before A and transform IPv4 to IPv4-mapped IPv6
    ndots: 1,
    rotate: false, // round-robin nameservers instead of always trying first one
    singleRequest: false, // perform IPv4 and IPv6 lookups sequentially?
    timeout: 5000, // ms
    use_vc: false // force TCP for all DNS queries
  },
  lookup: [ 'files', 'dns' ]
};

// Map DNS RCODE values to sensible string values
const RCODE_V2S = {
  // RFC 1035
  0: 'NOERROR', // No error
  1: 'FORMERR', // Format error
  2: 'SERVFAIL', // Server failure
  3: 'NXDOMAIN', // Non-existent domain
  4: 'NOTIMP', // Not implemented
  5: 'REFUSED', // Query refused
};
// Map DNS RCODE strings to c-ares
const RCODE_S2CARES = {
  'FORMERR': 'EFORMERR',
  'SERVFAIL': 'ESERVFAIL',
  'NXDOMAIN': 'ENOTFOUND',
  'NOTIMP': 'ENOTIMP',
  'REFUSED': 'EREFUSED'
};
// Map question/query type numbers to question-specific data and methods
const QTYPE_V2O = {
  1: {
    // RFC 1035 - IPv4 host address
    name: 'A',
    parse: function(rr, buf, p, end) {
      if (p + 3 >= end)
        return false;

      var addr = buf[p++] + '.' + buf[p++] + '.' + buf[p++] + '.' + buf[p++];

      rr.data = addr;

      return p;
    }
  },
  2: {
    // RFC 1035 - An authoritative name server
    name: 'NS',
    parse: nameRDataParser
  },
  5: {
    // RFC 1035 - The canonical name for an alias
    name: 'CNAME',
    parse: nameRDataParser
  },
  6: {
    // RFC 1035 - Marks the start of a zone of authority
    name: 'SOA',
    parse: function(rr, buf, p, end) {
      var mname;
      var rname;
      var serial;
      var refresh;
      var retry;
      var expire;
      var minimum;

      // MNAME
      var ret = parseName(buf, p, end);
      if (!Array.isArray(ret))
        return false;
      p = ret[0];
      mname = ret[1];

      // RNAME
      ret = parseName(buf, p, end);
      if (!Array.isArray(ret))
        return false;
      p = ret[0];
      rname = ret[1];

      // SERIAL
      if (p + 3 >= end)
        return false;
      serial = (buf[p++] * 16777216) + (buf[p++] * 65536) + (buf[p++] * 256) +
               buf[p++];

      // REFRESH
      if (p + 3 >= end)
        return false;
      refresh = (buf[p++] * 16777216) + (buf[p++] * 65536) + (buf[p++] * 256) +
                buf[p++];

      // RETRY
      if (p + 3 >= end)
        return false;
      retry = (buf[p++] * 16777216) + (buf[p++] * 65536) + (buf[p++] * 256) +
              buf[p++];

      // EXPIRE
      if (p + 3 >= end)
        return false;
      expire = (buf[p++] * 16777216) + (buf[p++] * 65536) + (buf[p++] * 256) +
               buf[p++];

      // MINIMUM
      if (p + 3 >= end)
        return false;
      minimum = (buf[p++] * 16777216) + (buf[p++] * 65536) + (buf[p++] * 256) +
                buf[p++];

      rr.data = {
        nsname: mname,
        hostmaster: rname,
        serial: serial,
        refresh: refresh,
        retry: retry,
        expire: expire,
        minttl: minimum
      };

      return p;
    }
  },
  12: {
    // RFC 1035 - A domain name pointer, used for reverse lookups
    name: 'PTR',
    parse: nameRDataParser
  },
  15: {
    // RFC 1035 - Mail exchange
    name: 'MX',
    parse: function(rr, buf, p, end) {
      if (p + 1 >= end)
        return false;

      // PREFERENCE
      var pref = (buf[p++] << 8) + buf[p++];

      // EXCHANGE
      var ret = parseName(buf, p, end);
      if (!Array.isArray(ret))
        return false;

      rr.data = { priority: pref, exchange: ret[1] };

      return ret[0];
    }
  },
  16: {
    // RFC 1035 - Text strings
    name: 'TXT',
    parse: function(rr, buf, p, end) {
      var texts = [];
      var len;

      while (p < end) {
        len = buf[p++];
        if (p + (len - 1) >= end)
          return false;

        texts.push(buf.toString('binary', p, p += len));
      }

      rr.data = texts;

      return end;
    }
  },
  28: {
    // RFC 3596 - IPv6 host address
    name: 'AAAA',
    parse: function(rr, buf, p, end) {
      // 128-bit value for IPv6 address
      if (p + 15 >= end)
        return false;

      // This implementation is more or less based on the inet_ntop from glibc

      var words = new Array(16);
      var str = '';
      var zbase = -1;
      var zlen = 0;
      var zbestbase = -1;
      var zbestlen = 0;

      for (var i = 0; p < end; ++i)
        words[i] = (buf[p++] << 8) + buf[p++];

      for (var i = 0; i < 8; ++i) {
        if (words[i] === 0) {
          if (zbase === -1) {
            zbase = i;
            zlen = 1;
          } else {
            ++zlen;
          }
        } else if (zbase !== -1) {
          if (zbestbase === -1 || zlen > zbestlen) {
            zbestbase = zbase;
            zbestlen = zlen;
          }
          zbase = -1;
        }
      }
      if (zbase !== -1) {
        if (zbestbase === -1 || zlen > zbestlen) {
          zbestbase = zbase;
          zbestlen = zlen;
        }
      }
      if (zbestbase !== -1 && zbestlen < 2)
        zbestbase = -1;

      for (var i = 0; i < 8; ++i) {
        // Are we inside the best run of 0x00's?
        if (zbestbase !== -1 && i >= zbestbase && i < (zbestbase + zbestlen)) {
          if (i === zbestbase)
            str += ':';
          continue;
        }
        // Are we following an initial run of 0x00s or any real hex?
        if (i !== 0)
          str += ':';
        // Is this address an encapsulated IPv4?
        if (i === 6 && zbestbase === 0 &&
            (zbestlen === 6 || (zbestlen === 5 && words[5] === 0xffff))) {
          str += (words[6] >>> 8) + '.' + (words[6] & 0xFF) + '.' +
                 (words[7] >>> 8) + '.' + (words[7] & 0xFF);
          break;
        }
        str += words[i].toString(16);
      }
      // Was it a trailing run of 0x00's?
      if (zbestbase !== -1 && (zbestbase + zbestlen) === 8)
        str += ':';

      rr.data = str;

      return end;
    }
  },
  29: {
    // RFC 1876 - Geographical location
    name: 'LOC',
    parse: function(rr, buf, p, end) {
      if (p + 15 >= end || buf[p++] !== 0)
        return false;

      var size = buf[p++];
      var horizPrec = buf[p++];
      var vertPrec = buf[p++];
      var latitude = (buf[p++] * 16777216) + (buf[p++] * 65536) +
                     (buf[p++] * 256) + buf[p++];
      var longitude = (buf[p++] * 16777216) + (buf[p++] * 65536) +
                      (buf[p++] * 256) + buf[p++];
      var altitude = (buf[p++] * 16777216) + (buf[p++] * 65536) +
                     (buf[p++] * 256) + buf[p++];

      var sizeBase = ((size & 0xF0) >> 4);
      var sizePower = (size & 0xF);
      if (sizeBase > 9 || sizePower > 9) // RFC says these values are undefined
        size = NaN;
      else
        size = sizeBase * Math.pow(10, sizePower);

      rr.data = {
        size: size,
        horizPrec: horizPrec,
        vertPrec: vertPrec,
        latitude: latitude,
        longitude: longitude,
        altitude: altitude
      };

      return end;
    }
  },
  33: {
    // RFC 2782 - Service locator
    name: 'SRV',
    parse: function(rr, buf, p, end) {
      if (p + 5 >= end)
        return false;

      var priority = (buf[p++] << 8) + buf[p++];

      var weight = (buf[p++] << 8) + buf[p++];

      var port = (buf[p++] << 8) + buf[p++];

      var ret = parseName(buf, p, end);
      if (!Array.isArray(ret)) // Error
        return false;

      rr.data = {
        priority: priority,
        weight: weight,
        port: port,
        name: ret[1]
      };

      return ret[0];
    }
  },
  35: {
    // RFC 3403 - Naming Authority Pointer
    name: 'NAPTR',
    parse: function(rr, buf, p, end) {
      if (p + 7 >= end)
        return false;

      var order = (buf[p++] << 8) + buf[p++];
      var preference = (buf[p++] << 8) + buf[p++];
      var len = buf[p++];
      // c-ares (at least) does not seem to interpret the character string
      // fields as UTF-8 as described in RFC 3403, so we emulate c-ares here ...
      var flags = '';
      var service = '';
      var regexp = '';
      var replacement;
      var ret;

      if (len) {
        if (p + (len - 1) >= end)
          return false;
        flags = buf.toString('binary', p, p += len);
      }

      if (p >= end)
        return false;
      len = buf[p++];

      if (len) {
        if (p + (len - 1) >= end)
          return false;
        service = buf.toString('binary', p, p += len);
      }

      if (p >= end)
        return false;
      len = buf[p++];

      if (len) {
        if (p + (len - 1) >= end)
          return false;
        regexp = buf.toString('binary', p, p += len);
      }

      if (p >= end)
        return false;
      ret = parseName(buf, p, end);
      if (!Array.isArray(ret)) // Error
        return false;
      replacement = ret[1];

      rr.data = {
        flags: flags,
        service: service,
        regexp: regexp,
        replacement: replacement,
        order: order,
        preference: preference
      };

      return ret[0];
    }
  },
  44: {
    // RFC 4255 / 6594 / 7479 - Secure shell fingerprint
    name: 'SSHFP',
    parse: function(rr, buf, p, end) {
      if (p + 2 >= end)
        return false;

      var algorithm = buf[p++];
      var fpType = buf[p++];

      switch (algorithm) {
        case 0: // RFC 4255 - Reserved
          return false;
        case 1: // RFC 4255
          algorithm = 'RSA';
          break;
        case 2: // RFC 4255
          algorithm = 'DSA';
          break;
        case 3: // RFC 6594
          algorithm = 'ECDSA';
          break;
        case 4: // RFC 7479
          algorithm = 'Ed25519';
          break;
      }

      switch (fpType) {
        case 0: // RFC 4255 - Reserved
          return false;
        case 1: // RFC 4255
          if ((end - p) < 40)
            return false;
          fpType = 'SHA1';
          break;
        case 2: // RFC 6594
          if ((end - p) < 64)
            return false;
          fpType = 'SHA256';
          break;
      }

      rr.data = {
        algorithm: algorithm,
        fpType: fpType,
        fp: buf.toString('hex', p, end)
      };

      return end;
    }
  }
};
// Several RDATA layouts all consist merely of a domain name, so we extract that
// functionality for better reuse
function nameRDataParser(rr, buf, p, end) {
  var ret = parseName(buf, p, end);
  if (!Array.isArray(ret))
    return false;
  rr.data = ret[1];
  return ret[0];
}

// Map question/query type strings to their numeric DNS protocol values
// This is used to validate user input in the actual resolver
var QTYPE_S2V = {
  '*': 255 // All cached records
};
(function() {
  var keys = Object.keys(QTYPE_V2O);
  for (var i = 0; i < keys.length; ++i) {
    var key = keys[i];
    QTYPE_S2V[QTYPE_V2O[key].name] = parseInt(key, 10);
  }
})();

// This stores the path to the databases directory on Windows that is evaluated
// only once at startup. Typically this is "%SystemRoot%\system32\drivers\etc"
var win32_dbPath;

// This stores nss module instances for nss modules other than 'dns' and 'files'
var nssModules = {};

// This saves the current index into the dns server list for when the `rotate`
// config option is enabled
var srvRotateIdx = -1;

// The current DNS configuration. This is populated at startup but can be
// modified by user-facing functions during runtime (e.g. setServers())
var dnsConfig = {
  nameserver: null,
  search: null,
  sortlist: null,
  options: {
    attempts: null,
    inet6: null,
    ndots: null,
    rotate: null,
    singleRequest: null,
    timeout: null,
    use_vc: null
  },
  lookup: null
};

exports.setServers = function(servers) {
  if (!Array.isArray(servers))
    throw new Error('servers argument must be an array of addresses');

  var newSet = [];

  for (var i = 0, serv; i < servers.length; ++i) {
    serv = servers[i];
    var ver = isIP(serv);

    if (ver) {
      newSet.push([ver, serv]);
      continue;
    }

    var match = serv.match(/\[(.*)\](:\d+)?/);

    // we have an IPv6 in brackets
    if (match) {
      ver = isIP(match[1]);
      if (ver) {
        newSet.push([ver, match[1]]);
        continue;
      }
    }

    var s = serv.split(/:\d+$/)[0];
    ver = isIP(s);

    if (ver) {
      newSet.push([ver, s]);
      continue;
    }

    throw new Error('IP address is not properly formatted: ' + serv);
  }

  dnsConfig.nameserver = newSet;
};

exports.getServers = function() {
  var nameservers = dnsConfig.nameserver;
  var ret = new Array(nameservers.length);
  for (var i = 0; i < nameservers.length; ++i)
    ret[i] = nameservers[i][1];
  return ret;
};

// lookup(hostname, [options,] callback)
exports.lookup = function lookup(hostname, options, callback) {
  var triedIPv4 = false; // State for `family === 0` case
  var isCustomType = false;
  var hints = 0;
  var family = -1;
  var all = false;
  var flags = 0;
  var type;

  // Parse arguments
  if (hostname && typeof hostname !== 'string') {
    throw new TypeError('invalid arguments: ' +
                        'hostname must be a string or falsey');
  } else if (typeof options === 'function') {
    callback = options;
    family = 0;
    // This should match glibc behavior
    hints = (ADDRCONFIG | V4MAPPED);
  } else if (typeof callback !== 'function') {
    throw new TypeError('invalid arguments: callback must be passed');
  } else if (options !== null && typeof options === 'object') {
    if (typeof options.hints === 'number')
      hints = (options.hints >>> 0);
    else if (options.family === undefined) // This should match glibc behavior
      hints = (ADDRCONFIG | V4MAPPED);
    family = (options.family >>> 0);
    all = (options.all === true);
    if (typeof options.flags === 'number')
      flags = (options.flags >>> 0);
    if (typeof options.type === 'string') {
      if (QTYPE_S2V[options.type] === undefined) {
        throw new Error(
          'invalid argument: type must be a supported record type'
        );
      }
      if (options.type !== 'A' && options.type !== 'AAAA')
        isCustomType = true;
      type = options.type;
    }

    if (hints !== 0 &&
        hints !== ADDRCONFIG &&
        hints !== V4MAPPED &&
        hints !== (ADDRCONFIG | V4MAPPED)) {
      throw new TypeError('invalid argument: hints must use valid flags');
    }
  } else {
    family = options >>> 0;
  }

  var inet6 = dnsConfig.options.inet6;
  var v4mapped = ((hints & V4MAPPED) > 0 || inet6);
  var addrconfig = (hints & ADDRCONFIG) > 0;
  var addrtypes = FLAG_HAS_IPv4_IPv6;

  if (!isCustomType) {
    if (family !== 0 && family !== 4 && family !== 6)
      throw new TypeError('invalid argument: family must be 4 or 6');

    if (hostname) {
      var matchedFamily = isIP(hostname);
      if (matchedFamily) {
        // TODO: Return IPv4-mapped IPv6 address if `v4mapped === true` and
        // `matchedFamily === 4` ?
        if (all) {
          process.nextTick(callback,
                           null,
                           [{address: hostname, family: matchedFamily}]);
        } else {
          process.nextTick(callback, null, hostname, matchedFamily);
        }
        return;
      }
    }

    // If we're limiting the type of query to what network interface address
    // types are available, we need to get those address types from the OS now
    if (addrconfig) {
      addrtypes = getIfaceAddrTypes();
      if (family === 0) {
        if (addrtypes & FLAG_HAS_IPv4)
          family = 4;
        else
          family = 6;
      }
    }

    // Check that we have a hostname AND that we have a network interface
    // address type that matches the type of IP address we're looking up (if
    // applicable)
    if (!hostname ||
        (addrconfig && family === 4 && !(addrtypes & FLAG_HAS_IPv4)) ||
        (addrconfig && family !== 4 && !(addrtypes & FLAG_HAS_IPv6))) {
      if (all)
        process.nextTick(callback, null, []);
      else
        process.nextTick(callback, null, null, family === 6 ? 6 : 4);
      return;
    }
  }

  if (!type) {
    if (family === 4)
      type = 'A';
    else if (family === 6 || dnsConfig.options.singleRequest)
      type = 'AAAA';
    else {
      type = 'AAAA';
      triedIPv4 = true;
      var origCallback = callback;
      var results = { 4: null, 6: null };
      callback = function(err, addrs) {
        // AAAA records
        results[6] = err || addrs;
        finalcb();
      };
      var finalcb = function() {
        var res4 = results[4];
        var res6 = results[6];
        if (res4 && res6) {
          var error4 = (res4 instanceof Error);
          var error6 = (res6 instanceof Error);
          if (error4 && error6)
            return origCallback(inet6 ? res6 : res4);

          var res;
          if (all && !error4 && !error6)
            res = (inet6 ? res6.concat(res4) : res4.concat(res6));
          else if (!error6 && (inet6 || error4))
            res = res6;
          else
            res = res4;

          origCallback(null, res);
        }
      };
      var newOpts;
      if (typeof options === 'object') {
        newOpts = {
          family: 4,
          hints: hints,
          all: all,
          flags: flags
        };
      } else 
        newOpts = 4;
      lookup(hostname, newOpts, function(err, addrs) {
        // A records
        results[4] = err || addrs;
        finalcb();
      });
    }
  }

  resolve(hostname, type, flags, function resolvecb(err, rep) {
    if (err) {
      // Try A records for `hostname` only if all of the following are true:
      //  - User did not request an explicit record type (A or AAAA) OR user
      //    explicitly requested AAAA and will accept IPv4-mapped IPv6 addresses
      //  - We haven't already tried for A records
      //  - We either aren't restricting record types to the types of network
      //    interface addresses currently enabled on the system OR we have at
      //    least one IPv4 (non-loopback and non-link-local) address assigned to
      //    an enabled network interface
      //  - User did not explicitly request a non-A/AAAA record type
      if ((family === 0 || (family === 6 && v4mapped)) && !triedIPv4 &&
          (!addrconfig || (addrtypes & FLAG_HAS_IPv4)) && !isCustomType) {
        triedIPv4 = true;
        return resolve(hostname, 'A', 0, resolvecb);
      }
      return callback(err);
    }

    var answers = rep.answers;
    if (answers.length === 0) {
      if (inet6 && family !== 4 && !triedIPv4) {
        triedIPv4 = true;
        return resolve(hostname, 'A', 0, resolvecb);
      }
      // Emulate old DNS behavior of sending an Error on "empty" responses
      // instead of sending an empty array like with dns.resolve*()
      return resolvecb(errnoException('ENOTFOUND', hostname));
    }

    if (!isCustomType) {
      var needv4Mapped = (v4mapped &&
                          (family === 6 || (family === 0 && inet6)) &&
                          triedIPv4);
      var addr;
      if (family === 0)
        family = (triedIPv4 ? 4 : 6);
      if (all) {
        for (var i = 0; i < answers.length; ++i) {
          addr = answers[i].data;
          if (needv4Mapped)
            addr = '::ffff:' + addr;
          answers[i] = {
            address: addr,
            family: family
          };
        }
        callback(null, addrs);
      } else {
        addr = answers[0].data;
        if (needv4Mapped)
          addr = '::ffff:' + addr;
        callback(null, addr, family);
      }
    } else {
      if (all) {
        for (var i = 0; i < answers.length; ++i)
          answers[i] = answers[i].data;
        callback(null, answers);
      } else {
        callback(null, answers[0].data);
      }
    }
  });
};

function resolver(type) {
  var returnFirst = (type === 'SOA');
  return function query(name, callback) {
    if (typeof name !== 'string')
      throw new Error('Name must be a string');
    else if (typeof callback !== 'function')
      throw new Error('Callback must be a function');

    resolve(name, type, FLAG_RESOLVE_NSONLY, function resolvecb(err, rep) {
      if (err)
        return callback(err);

      var answers = rep.answers;
      var addrs;

      if (returnFirst)
        addrs = (answers.length ? answers[0].data : null);
      else {
        addrs = new Array(answers.length);
        for (var i = 0; i < answers.length; ++i)
          addrs[i] = answers[i].data;
      }

      callback(null, addrs);
    });
  };
}

var resolveMap = {};
(function() {
  var keys = Object.keys(QTYPE_V2O);
  for (var i = 0; i < keys.length; ++i) {
    var key = keys[i];
    var name = QTYPE_V2O[key].name;
    var exportName = 'resolve' + name[0] + name.slice(1).toLowerCase();
    if (name === 'A')
      exportName = 'resolve4';
    else if (name === 'AAAA')
      exportName = 'resolve6';
    else if (name === 'PTR')
      exportName = 'reverse';
    exports[exportName] = resolveMap[name] = resolver(name);
  }
  exports.resolveAll = resolveMap['*'] = resolver('*');
})();

exports.resolve = function(hostname, type, callback_) {
  var resolver;
  var callback;
  if (typeof type === 'string') {
    resolver = resolveMap[type];
    callback = callback_;
  } else if (typeof type === 'function') {
    resolver = exports.resolve4;
    callback = type;
  } else {
    throw new Error('Type must be a string');
  }

  if (typeof resolver === 'function')
    resolver(hostname, callback);
  else
    throw new Error('Unknown type "' + type + '"');
};

exports.lookupService = function(addr, port, callback) {
  if (arguments.length < 3)
    throw new Error('missing arguments');

  if (isIP(addr) === 0)
    throw new TypeError('addr needs to be a valid IP address');
  if (typeof port !== 'number') {
    port = parseInt(port, 10);
    if (port !== port)
      throw new TypeError('invalid port');
  }
  if (!isFinite(port))
    throw new TypeError('invalid port');

  exports.lookup(addr, { type: 'PTR' }, function(err, hostname) {
    var host = (err || !hostname ? addr : hostname);
    var svcPath;
    if (process.platform === 'win32') {
      if (!win32_dbPath)
        return callback(err, host, port);
      svcPath = pathJoin(win32_dbPath, 'services');
    } else {
      // TODO: check /etc/nsswitch.conf for additional/alternate services file
      // path
      svcPath = '/etc/services';
    }
    searchServicesFile(svcPath, port, function(result) {
      callback(err, host, result || port);
    });
  });
};

function errnoException(err, hostname) {
  var ex;
  if (typeof err === 'string') {
    ex = new Error(err + (hostname ? ' ' + hostname : ''));
    ex.code = err;
    ex.errno = err;
  } else {
    ex = err;
  }
  if (hostname)
    ex.hostname = hostname;
  return ex;
}

// Error codes
exports.NODATA = 'ENODATA';
exports.FORMERR = 'EFORMERR';
exports.SERVFAIL = 'ESERVFAIL';
exports.NOTFOUND = 'ENOTFOUND';
exports.NOTIMP = 'ENOTIMP';
exports.REFUSED = 'EREFUSED';
exports.BADQUERY = 'EBADQUERY';
exports.BADNAME = 'EBADNAME';
exports.BADFAMILY = 'EBADFAMILY';
exports.BADRESP = 'EBADRESP';
exports.CONNREFUSED = 'ECONNREFUSED';
exports.TIMEOUT = 'ETIMEOUT';
exports.EOF = 'EOF';
exports.FILE = 'EFILE';
exports.NOMEM = 'ENOMEM';
exports.DESTRUCTION = 'EDESTRUCTION';
exports.BADSTR = 'EBADSTR';
exports.BADFLAGS = 'EBADFLAGS';
exports.NONAME = 'ENONAME';
exports.BADHINTS = 'EBADHINTS';
exports.NOTINITIALIZED = 'ENOTINITIALIZED';
exports.LOADIPHLPAPI = 'ELOADIPHLPAPI';
exports.ADDRGETNETWORKPARAMS = 'EADDRGETNETWORKPARAMS';
exports.CANCELLED = 'ECANCELLED';

// Hints for dns.lookup()
exports.V4MAPPED = V4MAPPED;
exports.ADDRCONFIG = ADDRCONFIG;

// Flags for dns.lookup() that are passed directly to resolve()
exports.FLAG_RESOLVE_NSONLY = FLAG_RESOLVE_NSONLY;
exports.FLAG_RESOLVE_NOSEARCH = FLAG_RESOLVE_NOSEARCH;
exports.FLAG_RESOLVE_NOALIASES = FLAG_RESOLVE_NOALIASES;
exports.FLAG_RESOLVE_IGNTC = FLAG_RESOLVE_IGNTC;
exports.FLAG_RESOLVE_USEVC = FLAG_RESOLVE_USEVC;



// The main resolver implementation
function resolve(name, type, flags, cb) {
  var rotate = dnsConfig.options.rotate;
  var nameservers = (!rotate ? dnsConfig.nameserver : null);
  var nsCount = 0;
  var nsIdx = -1;
  var TCPOnly = (dnsConfig.options.use_vc || (flags & FLAG_RESOLVE_USEVC));
  var attempts = dnsConfig.options.attempts;
  var lookups = ((flags & FLAG_RESOLVE_NSONLY) ? LOOKUPNS : dnsConfig.lookup);
  var lookupIdx = 0;
  var timeout = dnsConfig.options.timeout;
  var search = dnsConfig.search;
  var optNDots = dnsConfig.options.ndots;
  var searchIdx = 0;
  var cbCalled = false;
  var triedAsIs = false;
  var retryingOnTCP = false;
  var triedTCP = false;
  var reqIsPtr = false;
  var sawEmptyAnswers = false;
  var reqBufCache = {};
  var tcpBuf = null;
  var tcpBufLen = 0;
  var tcpLen = -0;
  var tcpSendLenBuf = null;
  var nameserver;
  var nssReq;
  var lastStatus;
  var ndots;
  var qtype;
  var qtypeName;
  var curName;
  var socket;
  var reqBuf;
  var timer;

  // For DNS nameserver requests
  qtype = QTYPE_S2V[type];
  if (!qtype)
    return process.nextTick(cb, errnoException(exports.BADQUERY, name));
  reqIsPtr = (qtype === QTYPE_PTR);
  qtypeName = type;
  if (typeof name !== 'string')
    return process.nextTick(cb, errnoException(exports.BADQUERY, name));
  if (name.charCodeAt(name.length - 1) === 46) { // '.'
    name = name.slice(0, -1);
    flags |= FLAG_RESOLVE_NOSEARCH;
  }

  ndots = -1;
  if (!reqIsPtr) {
    // Count dots in domain name
    var i = 0;
    ndots = 0;
    while ((i = name.indexOf('.', i)) > -1) {
      ++ndots;
      ++i;
    }

    if (ndots === 0 && process.env.HOSTALIASES &&
        !(flags & FLAG_RESOLVE_NOALIASES)) {
      // Domain name could be an alias, so check first before trying to resolve
      return searchHostAliases(name, function(newName) {
        resolve(newName, type, flags | FLAG_RESOLVE_NOALIASES, cb);
      });
    }
  }

  nextName();

  // Check that everything is OK with our initial request before trying anything
  if (cbCalled)
    return;

  nextLookup(false);



  function doCb(err, ret, async) {
    if (cbCalled)
      return;
    cbCalled = true;

    // Convert actual DNS error names to c-ares-compatible error names. The
    // error name conversion is done here at the very end to make it much easier
    // to switch to passing actual DNS error names to the user in the future.
    if (!err && ret && ret.rcode !== 'NOERROR') {
      ret.rcode = RCODE_S2CARES[ret.rcode] || ret.rcode;
      err = errnoException(ret.rcode, name);
      ret = undefined;
    }

    if (async)
      process.nextTick(cb, err, ret);
    else
      cb(err, ret);
  }

  // Cleanup on socket errors or query timeouts
  function cleanup(err) {
    // For nsswitch actions
    lastStatus = NSS_REQ_ERR_UNAVAIL;

    if (socket instanceof UDPSocket)
      socket.close();
    else {
      tcpBuf = null;
      tcpBufLen = 0;
      tcpLen = -0;
      socket.removeListener('data', tcpOnData);
      socket.destroy();
    }
  }

  function udpMsgHandler(msg, rinfo) {
    if (rinfo.address !== nameserver[1] || rinfo.port !== 53)
      return;
    clearTimeout(timer);
    socket.removeListener('message', udpMsgHandler);
    socket.removeListener('error', cleanup);
    socket.removeListener('close', nextLookup);
    socket.close();

    onReply(parseReply(qtypeName, msg));
  }

  function udpListening() {
    var ns = nameserver;
    timer = setTimeout(cleanup, timeout);
    socket.send(reqBuf, 0, reqBuf.length, 53, ns[1]);
  }

  function tcpOnData(data) {
    if (tcpLen === 0 && (1 / tcpLen) < 0) {
      // We have no length bytes yet
      if (data.length === 1) {
        tcpLen = -data[0];
        return;
      } else if (data.length === 2) {
        tcpLen = (data[0] << 8) + data[1];
        return;
      }
      tcpLen = (data[0] << 8) + data[1];
      data = data.slice(2);
    } else if (tcpLen < 0) {
      // We have one length byte
      tcpLen = ((tcpLen * -1) << 8) + data[0];
      if (data.length === 1)
        return;
      else
        data = data.slice(1);
    }

    tcpBufLen += data.length;
    if (tcpBufLen > tcpLen) {
      // We received too much from the server
      return cleanup();
    }
    if (!tcpBuf)
      tcpBuf = [ data ];
    else
      tcpBuf.push(data);

    if (tcpBufLen === tcpLen) {
      // Complete packet

      clearTimeout(timer);
      socket.removeListener('data', tcpOnData);
      socket.removeListener('error', cleanup);
      socket.removeListener('close', tcpOnClose);
      socket.end();

      var msg = Buffer.concat(tcpBuf, tcpBufLen);
      tcpBuf = null;
      tcpBufLen = 0;
      tcpLen = -0;

      onReply(parseReply(qtypeName, msg));
    }
  }

  function tcpOnConnect() {
    // Lazy create 2-byte `reqBuf` length Buffer
    if (!tcpSendLenBuf)
      tcpSendLenBuf = new Buffer(2);

    tcpSendLenBuf[0] = ((reqBuf.length >>> 8) & 0xFF);
    tcpSendLenBuf[1] = (reqBuf.length & 0xFF);

    socket.cork();
    socket.write(tcpSendLenBuf);
    socket.write(reqBuf);
    socket.uncork();
  }

  function tcpOnClose() {
    retryingOnTCP = false;
    nextLookup(false);
  }

  // Receives and interprets the result from parseReply()
  function onReply(rep) {
    if (rep === false) {
      // Parse or similar critical error

      // For nsswitch actions
      lastStatus = NSS_REQ_ERR_UNAVAIL;

      // Reset some state
      searchIdx = 0;
      triedAsIs = false;
    } else {
      // We got an actual reply

      // For nsswitch actions
      if (rep.rcode === 'NOERROR')
        lastStatus = NSS_REQ_ERR_SUCCESS;
      else if (rep.rcode === 'SERVFAIL')
        lastStatus = NSS_REQ_ERR_UNAVAIL;
      else
        lastStatus = NSS_REQ_ERR_NOTFOUND;

      if (rep.rcode === 'NOTIMP' || rep.rcode === 'SERVFAIL' ||
          rep.rcode === 'REFUSED' || (!rep.answers.length && !rep.truncated)) {
        if (!sawEmptyAnswers && rep.rcode !== 'NOTIMP' &&
            rep.rcode !== 'SERVFAIL' && rep.rcode !== 'REFUSED')
          sawEmptyAnswers = true;

        if (reqIsPtr || !nextName()) {
          // No more alternate names to try for the current server

          if (rep.rcode === 'NOERROR')
            return doCb(null, rep, false);

          searchIdx = 0;
          triedAsIs = false;
        } else {
          // Make sure we retry on TCP if we are already on TCP
          // TODO: revert back to UDP if we are on TCP because of a truncated
          // response? Either way we must use TCP if `TCPOnly === true`
          triedTCP = false;

          // Try next name with current server
          return nextLookup(true);
        }
      } else {
        if (rep.truncated && !(flags & FLAG_RESOLVE_IGNTC) && !retryingOnTCP) {
          retryingOnTCP = true;
          // Retry query on TCP with current parameters
          return nextLookup(true);
        }

        // Stop searching

        // If we saw a non-fatal error response with an empty answer set before,
        // make sure we do not treat the last response as an error (if there was
        // one)
        if (rep.rcode !== 'NOERROR' && sawEmptyAnswers)
          rep.rcode = 'NOERROR';

        if (rep.answers.length) {
          var sortlist = dnsConfig.sortlist;
          if (sortlist && sortlist.length) {
            if (qtype === QTYPE_A)
              rep.answers.sort(addressSortIPv4);
            else if (qtype === QTYPE_AAAA)
              rep.answers.sort(addressSortIPv6);
          }
        }

        return doCb(null, rep, false);
      }
    }

    // Advance to next server
    nextLookup(false);
  }

  // Determine the next domain name to be tried
  function nextName() {
    var domain;
    if ((!search || ndots === -1 || ndots >= optNDots ||
         searchIdx === search.length || (flags & FLAG_RESOLVE_NOSEARCH)) &&
        !triedAsIs) {
      triedAsIs = true;
      reqBuf = reqBufCache[name];
      if (!reqBuf) {
        reqBuf = reqToBuf(qtype, name);
        if (!Buffer.isBuffer(reqBuf)) // Error
          return doCb(reqBuf, undefined, true);
        reqBufCache[name] = reqBuf;
      }
      curName = name;
      return true;
    } else if (!(flags & FLAG_RESOLVE_NOSEARCH) && ndots > -1 && search &&
               (domain = search[searchIdx++])) {
      // Try the next domain
      var newName = name + '.' + domain;
      if (newName.charCodeAt(name.length - 1) === 46) // '.'
        newName = newName.slice(0, -1);
      reqBuf = reqBufCache[newName];
      if (!reqBuf) {
        reqBuf = reqToBuf(qtype, newName);
        if (!Buffer.isBuffer(reqBuf)) // Error
          return doCb(reqBuf, undefined, true);
        reqBufCache[newName] = reqBuf;
      }
      curName = newName;
      return true;
    }
    return false;
  }

  // Try the next lookup method, nameserver, or retry on a different protocol
  // (e.g. TCP)
  function nextLookup(retry) {
    var lookupMethod = lookups[lookupIdx];

    clearTimeout(timer);

    if (!lookupMethod)
      return doCb(errnoException('ENOTFOUND', name), undefined, false);

    switch (lookupMethod) {
      case 'dns':
        if (!retry) {
          // Advance to the next nameserver
          if (rotate) {
            if (nsCount++ >= dnsConfig.nameserver.length)
              nameserver = null;
            else {
              srvRotateIdx = (srvRotateIdx + 1) % dnsConfig.nameserver.length;
              nameserver = dnsConfig.nameserver[srvRotateIdx];
            }
          } else
            nameserver = nameservers[++nsIdx];

          triedTCP = false;
          if (TCPOnly)
            retryingOnTCP = true;
        }

        if (!nameserver) {
          // No more nameservers left, try next lookup method if we have
          // exhaused all retries
          if (attempts-- === 0)
            ++lookupIdx;
          else {
            nsCount = 0;
            nsIdx = -1;
          }
          return nextLookup(false);
        }

        if (retryingOnTCP) {
          if (triedTCP)
            return nextLookup(false);
          triedTCP = true;
          timer = setTimeout(cleanup, timeout);
          socket = new TCPSocket();
          socket.on('connect', tcpOnConnect);
          socket.on('data', tcpOnData);
          socket.on('error', cleanup);
          socket.on('close', tcpOnClose);
          socket.connect(53, nameserver[1]);
        } else {
          socket = new UDPSocket(nameserver[0] === 4 ? 'udp4' : 'udp6');
          socket.on('listening', udpListening);
          socket.on('message', udpMsgHandler);
          socket.on('error', cleanup);
          socket.on('close', nextLookup);
          socket.bind(0);
        }
        break;
      case 'files':
        // File lookups can only answer A, AAAA, and PTR questions
        if (reqIsPtr || qtype === QTYPE_A || qtype === QTYPE_AAAA) {
          fileLookup(name, qtype, function(ret) {
            if (ret !== false) {
              return doCb(null, {
                rcode: 'NOERROR',
                authoritative: true,
                truncated: false,
                recurseAvail: true,
                authenticated: false,
                answers: [{
                  name: name,
                  type: type,
                  ttl: 0,
                  data: ret
                }],
                authorities: [],
                additional: []
              }, false);
            }
            ++lookupIdx;
            nextLookup(false);
          });
        } else {
          ++lookupIdx;
          nextLookup(false);
        }
        break;
      default:
        var m;
        if (typeof lookupMethod !== 'string') {
          // nsswitch action(s)
          for (var a = 0; a < lookupMethod.length; ++a) {
            var pair = lookupMethod[a];
            var not = pair[0];
            var status = pair[1];
            var action = pair[2];
            if (/*(status === 'success' &&
                 ((!not && lastStatus === NSS_REQ_ERR_SUCCESS) ||
                  (not && lastStatus !== NSS_REQ_ERR_SUCCESS))) ||*/
                (status === 'notfound' &&
                 ((!not && lastStatus === NSS_REQ_ERR_NOTFOUND) ||
                  (not && lastStatus !== NSS_REQ_ERR_NOTFOUND))) ||
                (status === 'unavail' &&
                 ((!not && lastStatus === NSS_REQ_ERR_UNAVAIL) ||
                  (not && lastStatus !== NSS_REQ_ERR_UNAVAIL))) ||
                (status === 'tryagain' &&
                 ((!not && lastStatus === NSS_REQ_ERR_TRYAGAIN) ||
                  (not && lastStatus !== NSS_REQ_ERR_TRYAGAIN)))) {
              if (action === 'return') {
                if (lastStatus === NSS_REQ_ERR_NOTFOUND)
                  doCb(errnoException('ENOTFOUND', name), undefined, false);
                else if (lastStatus === NSS_REQ_ERR_UNAVAIL)
                  doCb(errnoException('ESERVFAIL', name), undefined, false);
                else if (lastStatus === NSS_REQ_ERR_TRYAGAIN)
                  doCb(errnoException('ESERVFAIL', name), undefined, false);
                return;
              }
            }
          }
          ++lookupIdx;
          nextLookup(false);
          return;
        }
        // Some custom nss module, like 'mdns4' for example
        if (reqIsPtr || qtype === QTYPE_A || qtype === QTYPE_AAAA) {
          var nssModule = nssModules[lookupMethod];
          if (!nssModule ||
              (reqIsPtr && !nssModule.hasByAddr) ||
              (!reqIsPtr && !nssModule.hasByName)) {
            ++lookupIdx;
            nextLookup(false);
            return;
          }
          if (!nssReq) {
            if (reqIsPtr)
              nssReq = new NSSReqWrap(name, 0);
            else
              nssReq = new NSSReqWrap(name, -1);
            nssReq.oncomplete = function(status, results) {
              lastStatus = status;
              if (lastStatus === NSS_REQ_ERR_SUCCESS) {
                if (reqIsPtr) {
                  results = [{
                    name: name,
                    type: type,
                    ttl: 0,  // TODO: fill in when available
                    data: results
                  }];
                } else {
                  for (var i = 0; i < results.length; ++i) {
                    results[i] = {
                      name: name,
                      type: type,
                      ttl: 0, // TODO: fill in when available
                      data: results[i]
                    };
                  }
                }
                doCb(null, {
                  rcode: 'NOERROR',
                  authoritative: true,
                  truncated: false,
                  recurseAvail: true,
                  authenticated: false,
                  answers: results,
                  authorities: [],
                  additional: []
                }, false);
              } else {
                ++lookupIdx;
                nextLookup(false);
              }
            };
          }
          if (reqIsPtr)
            nssModule.queryAddr(nssReq);
          else
            nssModule.queryName(nssReq, (qtype === QTYPE_A ? 4 : 6));
        } else {
          ++lookupIdx;
          nextLookup(false);
        }
    }
  }
}

// Create a properly formatted Buffer from DNS request parameters
function reqToBuf(qtype, name) {
  var origName = name;
  var namelen = 1; // Account for terminating length byte
  var buf;

  // Convert IP addresses to the appropriate reversed dotted notation
  if (qtype === QTYPE_PTR)
    name = createPtrAddr(name);

  name = name.split('.');

  for (var i = 0; i < name.length; ++i) {
    if (name[i].length > 63)
      return errnoException(exports.NOTFOUND, origName);
    // Label length byte + label content
    namelen += 1 + Buffer.byteLength(name[i], 'ascii');
  }
  if (namelen > 255)
    return errnoException(exports.NOTFOUND, origName);

  buf = new Buffer(12 + namelen + 2 + 2);

  // ID
  // We currently use 0 for all DNS queries because we do not use the same UDP
  // socket for multiple (outstanding) queries
  buf[0] = buf[1] = 0;

  // QR + OPCODE + AA + TC + RD
  //  0        0    -    -    1
  buf[2] = 1;

  // RA + Z + AD + CD + RCODE
  //  -   0    0    0       -
  buf[3] = 0;

  // # Questions (1 for now)
  buf[4] = 0;
  buf[5] = 1;

  // # Answer RRs
  buf[6] = buf[7] = 0;

  // # Authority RRs
  buf[8] = buf[9] = 0;

  // # Additional RRs
  buf[10] = buf[11] = 0;

  // Question:
  //  - QNAME
  var p = 12;
  for (var i = 0; i < name.length; ++i) {
    buf[p++] = name[i].length;
    buf.write(name[i], p, p += name[i].length, 'ascii');
  }
  buf[p++] = 0; // Terminating length byte

  //  - QTYPE
  buf[p++] = 0;
  buf[p++] = qtype;

  //  - QCLASS
  buf[p++] = 0;
  buf[p++] = 1; // Internet

  return buf;
}

// Convert an IPv4 or IPv6 address to its appropriate reverse dotted notation
// for reverse lookups
function createPtrAddr(name) {
  var ipType = isIP(name);

  if (ipType === 4) {
    var octets = name.split('.').reverse();
    // Remove any leading zeros
    for (var i = 0; i < 4; ++i)
      octets[i] = parseInt(octets[i], 10);
    return octets.join('.') + '.in-addr.arpa';
  } else if (ipType === 6) {
    var nibbles = new Array(32); // Individual hex digits
    var chunks = name.split(':');

    // Convert a possible embedded IPv4 address to IPv6 format
    var lastChunk = chunks[chunks.length - 1];
    if (lastChunk.indexOf('.') > -1) {
      var octets = lastChunk.split('.');
      chunks[chunks.length - 1] = ((parseInt(octets[0], 10) << 8) +
                                   parseInt(octets[1], 10)).toString(16);
      chunks.push(((parseInt(octets[2], 10) << 8) +
                   parseInt(octets[3], 10)).toString(16));
    }

    // Count existing bits in case of '::' to help us know how many 0's to add
    var bits = 0;
    var zeros;
    for (var i = 0; i < chunks.length; ++i) {
      if (chunks[i].length)
        bits += 16;
    }
    zeros = (128 - bits) / 4;

    // Fill in nibbles
    //var lastChunkIdx = chunks.length - 1;
    for (var i = 0, n = 31; i < chunks.length; ++i) {
      var val = chunks[i];
      var fill = (4 - val.length);
      if (fill === 4 && zeros > -1) {
        // '::'
        for (var j = 0; j < zeros; ++j, --n)
          nibbles[n] = '0';
        zeros = -1;
        if (!chunks[i + 1].length)
          ++i; // Skip over next empty string if ending on '::'
        /*++i; // Skip over next empty string
        // Check that we're not skipping over a valid chunk
        if (i === lastChunkIdx)
          --i;*/
      } else {
        // Fill in leading zeros first
        for (var j = 0; j < fill; ++j, --n)
          nibbles[n] = '0';

        for (var j = 0; j < val.length; ++j, --n)
          nibbles[n] = val[j];
      }
    }

    var ret = nibbles.join('.') + '.ip6.arpa';
    return ret;
  } else if (!/\.(?:in-addr|ip6)\.arpa$/i.test(name)) {
    throw errnoException('EINVAL', name);
  }

  // `name` already contains the appropriate suffix, so we assume the rest of
  // it is formatted correctly
  return name;
}

// Parse a raw reply from a nameserver
function parseReply(qtypename, buf) {
  var buflen = buf.length;
  var p = 0;
  var flags1;
  var AA;
  var TC;
  var flags2;
  var RA;
  var AD;
  var RCODE;
  var nquestions;
  var answers;
  var authorities;
  var additional;
  var ret;

  if (buf.length < 12)
    return false;

  // Check ID field, we're currently only using 0 for any DNS query
  if (buf[p++] !== 0 || buf[p++] !== 0)
    return false;

  // QR + OPCODE + AA + TC + RD
  flags1 = buf[p++];
  if (!(flags1 & 0x80)) // QR should be 1 for a response
    return false;
  if (flags1 & 0x78) // OPCODE should be 0 for standard query
    return false;
  AA = ((flags1 & 0x04) > 0);
  TC = ((flags1 & 0x02) > 0);

  // RA + Z + AD + CD + RCODE
  flags2 = buf[p++];
  RA = ((flags2 & 0x80) > 0);
  AD = ((flags2 & 0x20) > 0);
  RCODE = RCODE_V2S[(flags2 & 0x0F)] || 'UNKNOWN';

  // # Questions
  nquestions = (buf[p++] << 8) + buf[p++];

  // # Answer RRs
  answers = new Array((buf[p++] << 8) + buf[p++]);

  // # Authority RRs
  authorities = new Array((buf[p++] << 8) + buf[p++]);

  // # Additional RRs
  additional = new Array((buf[p++] << 8) + buf[p++]);

  // Question:
  // For responses, this contains a copy of the original question(s), so we skip
  // it altogether
  for (var i = 0; i < nquestions; ++i) {
    ret = parseName(buf, p, buflen);
    if (!Array.isArray(ret)) // Error
      return false;
    p = ret[0];
    p += 4; // Skip over QTYPE and QCLASS too
  }

  // Answers
  for (var i = 0, alen = answers.length; i < alen; ++i) {
    p = parseRR(buf, p, answers, i);
    if (typeof p !== 'number') // Error
      return false;
    // Check for unsupported, irrelevant, and/or duplicate answers
    var ans = answers[i];
    var shouldRemove = (ans === undefined || ans.type !== qtypename);
    if (!shouldRemove) {
      for (var j = 0; j < i; ++j) {
        if (deepEqual(answers[j], ans)) {
          shouldRemove = true;
          break;
        }
      }
    }
    if (shouldRemove) {
      spliceOne(answers, i);
      --alen;
      --i;
    }
  }

  // Authorities
  for (var i = 0; i < authorities.length; ++i) {
    p = parseRR(buf, p, authorities, i);
    if (typeof p !== 'number') // Error
      return false;
  }

  // Additional
  for (var i = 0; i < additional.length; ++i) {
    p = parseRR(buf, p, additional, i);
    if (typeof p !== 'number') // Error
      return false;
  }

  return {
    rcode: RCODE,
    authoritative: AA,
    truncated: TC,
    recurseAvail: RA,
    authenticated: AD,
    answers: answers,
    authorities: authorities,
    additional: additional
  };
}

// Parse an individual Resource Record from a nameserver reply
function parseRR(buf, p, storage, stidx) {
  var buflen = buf.length;
  var typeObj;
  var name;
  var type;
  var cls;
  var ttl;
  var rdlen;
  var end;
  var rr;
  var ret;

  // NAME
  ret = parseName(buf, p, buflen);
  if (!Array.isArray(ret)) // Error
    return false;
  p = ret[0];
  name = ret[1];

  if (p + 9 >= buflen) // `buf` too small
    return false;

  // TYPE
  type = (buf[p++] << 8) + buf[p++];

  // CLASS
  cls = (buf[p++] << 8) + buf[p++];

  // TTL
  ttl = (buf[p++] * 16777216) + (buf[p++] * 65536) + (buf[p++] * 256) +
        buf[p++];

  // RDLENGTH
  rdlen = (buf[p++] << 8) + buf[p++];

  end = p + rdlen;
  if (end > buflen) // `buf` too small
    return false;

  // Skip RRs of classes other than Internet
  if (cls !== 1) // 1 === Internet
    return end;

  // Skip RRs that have a type that we do not support
  typeObj = QTYPE_V2O[type];
  if (!typeObj)
    return end;

  rr = {
    name: name,
    type: typeObj.name,
    ttl: ttl,
    data: null
  };

  // RDATA
  if (rdlen > 0) {
    p = typeObj.parse(rr, buf, p, end);
    if (p === false) // Error
      return false;
  }

  // Append the result directly to the caller's storage array to avoid having to
  // return an array containing the result *and* the current position
  storage[stidx] = rr;

  return p;
}

// Parse a domain name string
function parseName(buf, p, end) {
  var visited = [];
  var name = '';
  var oldp;
  var len;

  while (len = buf[p++]) {
    // 63 is the max length for a non-pointer length byte (00111111)
    if (len > 63) {
      // Pointer
      if ((len & 192) === 192) {
        // Both of the 2 leftmost bits are set, this is the type of pointer
        // described by RFC 1035

        if (p >= end)
          return false;

        // If this is our first pointer encounter, make sure we store the
        // current position in the Buffer so that we know where to continue from
        // once we reach the last pointer
        if (!oldp)
          oldp = p + 1;

        p = ((len & 63) << 8) + buf[p];

        if (p >= end)
          return false;

        // Check for a pointer loop
        if (visited.indexOf(p) > -1)
          return false;
        visited.push(p);
      } else {
        // Unknown pointer type (e.g. EDNS defined a new binary label/pointer
        // type (now deprecated as of RFC 6891))
        return false;
      }
    } else {
      // Simple label
      if (p + (len - 1) >= end)
        return false;
      if (name)
        name += '.';
      name += buf.toString('ascii', p, p += len);
    }
  }

  // Reset our position if we followed a pointer at some point
  if (oldp)
    p = oldp;

  // Remove root from parsed name
  if (name[name.length - 1] === '.')
    name = name.slice(0, -1);

  return [ p, name ];
}

// Utility function for merging only properties unset on one object that are
// set on another object.
function fillIn(base, values) {
  var keys = Object.keys(base);
  for (var i = 0, key; i < keys.length; ++i) {
    key = keys[i];
    if (typeof base[key] === 'object' && base[key] !== null &&
        typeof values[key] === 'object' && values[key] !== null)
      fillIn(base[key], values[key]);
    else if (base[key] === null) {
      if (values[key] !== null && values[key] !== undefined)
        base[key] = values[key];
    }
  }
}

// Perform a lookup in the local hosts file
function fileLookup(str, qtype, cb) {
  if (process.platform === 'win32') {
    if (!win32_dbPath)
      return process.nextTick(cb, false);
    searchHostsFile(pathJoin(win32_dbPath, 'hosts'), str, qtype, cb);
  } else {
    // *nix
    searchHostsFile('/etc/hosts', str, qtype, cb);
  }
}

// This performs the actual search through a local hosts file, for either normal
// or reverse lookups
function searchHostsFile(path, str, qtype, cb) {
  var ret = false;
  var ipver = (qtype === QTYPE_A ? 4 : qtype === QTYPE_AAAA ? 6 : 0);
  var reverse = (isIP(str) !== 0);
  var data = new Buffer(128);
  var regexp = new RegExp(regexpEscape(str), 'i');
  var last = '';
  var addrParsed = (reverse ? parseAddrMask(str, true) : null);
  var lines;
  var host;

  function closecb(err) {
    cb(ret);
  }
  fs.open(path, 'r', function(err, fd) {
    if (err)
      return closecb();
    fs.read(fd, data, 0, data.length, null, function readcb(err, nb) {
      if (err)
        return fs.close(fd, closecb);

      if (nb) {
        lines = (last + data.toString('utf8', 0, nb)).split(/\r?\n/g);
        last = lines[lines.length - 1];
        for (var i = 0; i < lines.length - 1; ++i) {
          host = parseHostLine(lines[i]);
          if (host !== false) {
            if (reverse &&
                addrsEqual(parseAddrMask(host[0], true), addrParsed)) {
              ret = host[1];
              return fs.close(fd, closecb);
            } else if (!reverse) {
              for (var k = 1, hostval; k < host.length; ++k) {
                hostval = host[k];
                if (hostval.length !== str.length)
                  continue;
                if (hostval.match(regexp) !== null) {
                  // We found a matching hostname, but is the first value on the
                  // line a real IP address and is the right IP version we're
                  // searching for?
                  var hostipver = isIP(host[0]);
                  if (hostipver === ipver || (hostipver && ipver === 0)) {
                    ret = host[0];
                    return fs.close(fd, closecb);
                  }
                }
              }
            }
          }
        }
        fs.read(fd, data, 0, data.length, null, readcb);
      } else {
        // If the file didn't end on a line ending, we interpret what was left
        // in our buffer as a line
        if (ret === false && last) {
          host = parseHostLine(last);
          if (host !== false) {
            if (reverse &&
                addrsEqual(parseAddrMask(host[0], true), addrParsed)) {
              ret = host[1];
            } else if (!reverse) {
              for (var k = 1, hostval; k < host.length; ++k) {
                hostval = host[k];
                if (hostval.length !== str.length)
                  continue;
                if (hostval.match(regexp) !== null) {
                  var hostipver = isIP(host[0]);
                  if (hostipver === ipver || (hostipver && ipver === 0)) {
                    ret = host[0];
                    break;
                  }
                }
              }
            }
          }
        }
        fs.close(fd, closecb);
      }
    });
  });
}

// Byte/Word-wise equality check of two IP addresses
function addrsEqual(a, b) {
  if (a === false || b === false || a.length !== b.length)
    return false;
  for (var i = 0; i < a.length; ++i) {
    if (a[i] !== b[i])
      return false;
  }
  return true;
}

function regexpEscape(str) {
  return str.replace(/[-[\]{}()*+?.,\\^$|#\s]/g, '\\$&');
}

function parseHostLine(line) {
  line = line.replace(/#.*$/, '').trim();
  var fields = line.split(REGEX_WHITESPACE);
  if (fields.length < 2)
    return false;
  return fields;
}

// This searches a services file for the service name for a particular port
// number
function searchServicesFile(path, port, cb) {
  var ret = false;
  var data = new Buffer(128);
  var last = '';
  var lines;

  function closecb(err) {
    cb(ret);
  }
  fs.open(path, 'r', function(err, fd) {
    if (err)
      return closecb();
    fs.read(fd, data, 0, data.length, null, function readcb(err, nb) {
      if (err)
        return fs.close(fd, closecb);

      if (nb) {
        lines = (last + data.toString('ascii', 0, nb)).split(/\r?\n/g);
        last = lines[lines.length - 1];
        for (var i = 0; i < lines.length - 1; ++i) {
          ret = checkPort(lines[i], port);
          if (ret !== false)
            return fs.close(fd, closecb);
        }
        fs.read(fd, data, 0, data.length, null, readcb);
      } else {
        if (ret === false && last)
          ret = checkPort(last, port);
        fs.close(fd, closecb);
      }
    });
  });
}

function checkPort(line, port) {
  var m = /^([A-Za-z0-9\-]+)[ \t]+(\d+)\/[A-Za-z]+/.exec(line);
  if (m && m[2] == port)
    return m[1];
  return false;
}

// Helper function currently used to check if two DNS answers are equal
function deepEqual(a, b) {
  if (typeof a === 'object' && typeof b === 'object' &&
      a !== null && b !== null) {
    var keysA = Object.keys(a);
    var keysB = Object.keys(b);
    var alen = keysA.length;
    var blen = keysB.length;
    if (alen !== blen)
      return false;
    for (var i = 0; i < alen; ++i) {
      if (!deepEqual(a[keysA[i]], b[keysB[i]]))
        return false;
    }
    return true;
  }
  return (a === b);
}

// Parses strings of the format: <address>[/netmask]
// Into [parsedAddr, [parsedFullNetMask]]
function parseAddrMask(network, addrOnly) {
  var ret;
  var ipver;

  if (!addrOnly) {
    // Check for explicit netmask
    if (network.indexOf('/') > -1) {
      network = network.split('/', 2);
      ipver = isIP(network[0]);
      var mask = network[1];
      if (ipver === 4) {
        if (/^\d{1,2}$/.test(mask)) {
          // CIDR notation
          mask = parseInt(mask, 10);
          if (mask >= 0 && mask <= 32) {
            if (mask > 0)
              mask = (0xFFFFFFFF ^ (1 << 32 - mask) - 1);
            network[1] = [mask >>> 24, (mask >>> 16) & 0xFF,
                          (mask >>> 8) & 0xFF, mask & 0xFF];
            ret = network;
          }
        } else if (isIP(network[1]) === 4) {
          // Dotted notation
          var octets = mask.split('.');
          for (var k = 0; k < 4; ++k)
            octets[k] = parseInt(octets[k], 10);
          network[1] = octets;
          ret = network;
        }
      } else if (ipver === 6) {
        // CIDR notation
        if (/^\d{1,3}$/.test(mask)) {
          mask = parseInt(mask, 10);
          if (mask >= 0 && mask <= 128) {
            var newMask = [0, 0, 0, 0, 0, 0, 0, 0];
            var idx = 0;
            while (mask && idx < 8) {
              var bits = (mask > 16 ? 16 : mask);
              newMask[idx++] = (0xFFFF ^ (1 << 16 - bits) - 1);
              mask -= bits;
            }
            network[1] = newMask;
            ret = network;
          }
        }
      }
    } else {
      // No explicit netmask given, use default
      ipver = isIP(network);
      if (ipver === 4) {
        var octet = parseInt(network.split('.')[0], 10);
        if (octet >= 0 && octet < 128)
          ret = [network, [255, 0, 0, 0]];
        else if (octet >= 128 && octet < 192)
          ret = [network, [255, 255, 0, 0]];
        else if (octet >= 192 && octet < 224)
          ret = [network, [255, 255, 255, 0]];
      } else if (ipver === 6) {
        // Use "standard" mask of /64 for IPv6
        ret = [network, [65535, 65535, 65535, 65535, 0, 0, 0, 0]];
      }
    }
  }

  // Now parse address for convenience
  if (ret || addrOnly) {
    var addr;
    if (typeof network === 'string') {
      addr = network;
      ipver = isIP(addr);
    } else {
      addr = network[0];
    }
    if (ipver === 4) {
      addr = addr.split('.');
      for (var k = 0; k < 4; ++k)
        addr[k] = parseInt(addr[k], 10);
    } else if (ipver === 6) {
      var chunks = addr.split(':');
      var a = 0;
      addr = new Array(8);

      // Convert a possible embedded IPv4 dotted decimals to IPv6 format
      var lastChunk = chunks[chunks.length - 1];
      if (lastChunk.indexOf('.') > -1) {
        var octets = lastChunk.split('.');
        chunks[chunks.length - 1] = ((parseInt(octets[0], 10) << 8) +
                                     parseInt(octets[1], 10)).toString(16);
        chunks.push(((parseInt(octets[2], 10) << 8) +
                     parseInt(octets[3], 10)).toString(16));
      }

      // Count existing bits in case of '::' to help us know how many zero words
      // to add
      var bits = 0;
      for (var i = 0; i < chunks.length; ++i) {
        if (chunks[i].length)
          bits += 16;
      }
      var zeros = (128 - bits) / 16;

      // Parse chunks
      for (var i = 0; i < chunks.length; ++i) {
        var val = chunks[i];
        if (val.length === 0 && zeros > -1) {
          // '::'
          for (var j = 0; j < zeros; ++j)
            addr[a++] = 0;
          zeros = -1;
          if (!chunks[i + 1].length)
            ++i; // Skip over next empty string if ending on '::'
        } else {
          addr[a++] = parseInt(val, 16);
        }
      }
    }
    if (addrOnly)
      ret = addr;
    else if (typeof network === 'string')
      ret[0] = addr;
    else
      network[0] = addr;
  }

  return ret;
}

// Compares two IPv4 addresses according to a system-defined sortlist
function addressSortIPv4(a, b) {
  var sortlist = dnsConfig.sortlist;
  var ai = -1;
  var bi = -1;

  a = parseAddrMask(a, true);
  b = parseAddrMask(b, true);

  // Find matching network/mask for both addresses
  for (var i = 0; i < sortlist.length; ++i) {
    var network = sortlist[i][0];
    var mask = sortlist[i][1];
    var matchedA = true;
    var matchedB = true;
    if (network.length !== 4)
      continue;

    for (var j = 0; j < 4; ++j) {
      if (matchedA && ai === -1 && (a[j] & mask[j]) !== network[j])
        matchedA = false;
      if (matchedB && bi === -1 && (b[j] & mask[j]) !== network[j])
        matchedB = false;
      if (!matchedA && !matchedB)
        break;
    }

    if (matchedA && ai === -1)
      ai = i;
    if (matchedB && bi === -1)
      bi = i;
    if (ai > -1 && b > -1)
      break;
  }

  if (ai === bi) {
    var d = a[0] - b[0];
    if (!d) {
      d = a[1] - b[1];
      if (!d) {
        d = a[2] - b[2];
        if (!d)
          return (a[3] - b[3]);
      }
    }
    return d;
  } else if (ai < 0) {
    return 1;
  } else if (bi < 0) {
    return -1;
  }
  return ai - bi;
}

// Compares two IPv6 addresses according to a system-defined sortlist
function addressSortIPv6(a, b) {
  var sortlist = dnsConfig.sortlist;
  var ai = -1;
  var bi = -1;

  a = parseAddrMask(a, true);
  b = parseAddrMask(b, true);

  // Find matching network/mask for both addresses
  for (var i = 0; i < sortlist.length; ++i) {
    var network = sortlist[i][0];
    var mask = sortlist[i][1];
    var matchedA = true;
    var matchedB = true;
    if (network.length !== 8)
      continue;

    for (var j = 0; j < 8; ++j) {
      if (matchedA && ai === -1 && (a[j] & mask[j]) !== network[j])
        matchedA = false;
      if (matchedB && bi === -1 && (b[j] & mask[j]) !== network[j])
        matchedB = false;
      if (!matchedA && !matchedB)
        break;
    }

    if (matchedA && ai === -1)
      ai = i;
    if (matchedB && bi === -1)
      bi = i;
    if (ai > -1 && b > -1)
      break;
  }

  if (ai === bi) {
    var i = 0;
    var d;
    while (i < 8) {
      d = a[i] - b[i];
      if (d)
        return d;
      ++i;
    }
    return 0;
  } else if (ai < 0) {
    return 1;
  } else if (bi < 0) {
    return -1;
  }
  return ai - bi;
}

// Check to see what types of interface addresses currently exist in the system.
// This is used when the ADDRCONFIG flag is set to determine what kind of
// IP addresses should be returned to the user
function getIfaceAddrTypes() {
  var ret = 0;
  var interfaces = os.networkInterfaces();
  var ifkeys = Object.keys(interfaces);
  var hasIPv6LL = false;

  for (var i = 0, addrs; i < ifkeys.length; ++i) {
    addrs = interfaces[ifkeys[i]];
    for (var j = 0, addr; j < addrs.length; ++j) {
      addr = addrs[j];
      // Do not consider loopback addresses
      if (!addr.internal) {
        switch (addr.family) {
          case 'IPv4':
            // Also do not consider link-local addresses
            if (addr.address.slice(0, 8) !== '169.254.')
              ret |= FLAG_HAS_IPv4;
            break;
          case 'IPv6':
            if (addr.address.slice(0, 6) === 'fe80::')
              hasIPv6LL = true;
            else
              ret |= FLAG_HAS_IPv6;
            break;
        }
      }
    }
    if (ret === FLAG_HAS_IPv4_IPv6) // No need to search any more
      break;
  }

  // Only consider IPv6 link-local addresses if there are no IPv4 addresses
  // See: https://fedoraproject.org/wiki/Networking/NameResolution/ADDRCONFIG
  if (ret === 0 && hasIPv6LL)
    ret = FLAG_HAS_IPv6;

  return ret;
}

function spliceOne(list, index) {
  for (var k = index + 1, n = list.length; k < n; index += 1, k += 1)
    list[index] = list[k];
  list.pop();
}


// Platform-specific helper methods:
function win32_wmiDNSInfo() {
  // TODO: check AppendToMultiLabelName registry value before setting
  // `DNSDomainSuffixSearchOrder`
  //   http://blogs.technet.com/b/networking/archive/2009/04/16/dns-client-name-
  //   resolution-behavior-in-windows-vista-vs-windows-xp.aspx
  var ret = false;
  var cmd = [
    'wmic',
    'nicconfig',
    'where "MACAddress is not null and IPEnabled = true"',
    'get',
    ['DNSDomain',
     'DNSDomainSuffixSearchOrder',
     'DNSServerSearchOrder',
     'IPConnectionMetric'
    ].join(', '),
    '/format:csv'
  ].join(' ');

  try {
    var r = execSync(cmd, { encoding: 'utf8' });
    var lines = r.split(/\r?\n/g);
    var path = 'HKLM\\System\\CurrentControlSet\\Services\\Tcpip\\Parameters';
    var header;
    var dnsinfo;
    for (var i = 0, line; i < lines.length; ++i) {
      line = lines[i].trim();
      if (line) {
        line = line.split(',');
        if (!header)
          header = line;
        else {
          var row = makeObj(header, line);

          if (row.DNSServerSearchOrder) {
            // There is at least one DNS server set for this connection

            row.IPConnectionMetric = parseInt(row.IPConnectionMetric, 10);
            // Only choose the parameters from the connection with the lowest
            // connection metric
            if (!dnsinfo || row.IPConnectionMetric < dnsinfo.IPConnectionMetric)
              dnsinfo = row;
          }
        }
      }
    }
    if (dnsinfo) {
      var ns = REGEX_ARRAY.exec(dnsinfo.DNSServerSearchOrder)[1].split(';');
      var search = null;
      if (dnsinfo.DNSDomainSuffixSearchOrder)
        dnsinfo.DNSDomainSuffixSearchOrder = null;
      if (dnsinfo.DNSDomainSuffixSearchOrder)
        search = dnsinfo.DNSDomainSuffixSearchOrder;
      else {
        var primarySuffix = win32_regQuery(path, 'NV Domain', false);
        if (dnsinfo.DNSDomain) {
          search = dnsinfo.DNSDomain;
          if (primarySuffix)
            search = primarySuffix + ';' + search;
        } else if (primarySuffix) {
          search = primarySuffix;
        }
      }
      if (search)
        search = REGEX_ARRAY.exec(search)[1].split(';');
      ret = {
        nameserver: ns,
        search: search,
        sortlist: null,
        options: {
          // Windows always tries a multi-label name as-is before using suffixes
          ndots: 1,
          // TODO: support `timeout` via DNSQueryTimeouts registry value?
          //   https://technet.microsoft.com/library/Cc977482
          timeout: null,
          attempts: null,
          rotate: null,
          inet6: null,
          use_vc: null
        },
        // This is the order that both c-ares (on Windows) and the Windows DNS
        // client use
        lookup: ['files', 'dns']
      };
    }
  } catch (ex) {}

  return ret;
}

// Used by the WMI-based DNS settings retrieval method on Windows for creating
// an object from CSV output
function makeObj(keys, vals) {
  var obj = {};
  for (var i = 0; i < keys.length; ++i) {
    if (vals[i][0] === '{')
      vals[i] = vals[i].slice(1, -1);
    obj[keys[i]] = vals[i];
  }
  return obj;
}

function win32_getRegNameServers() {
  var r;
  var path;

  // Try global DNS servers first
  path = 'HKLM\\System\\CurrentControlSet\\Services\\Tcpip\\Parameters';
  r = win32_regQuery(path, 'NameServer', false);
  if (r !== false)
    return r.split(/[ \f\t\v,]+/g);
  r = win32_regQuery(path, 'DhcpNameServer', false);
  if (r !== false)
    return r.split(/[ \f\t\v,]+/g);

  // Next try to find the first interface DNS address
  // TODO: make this smarter/better rather than just using first interface?
  path += '\\Interfaces';
  r = win32_regQuery(path, 'NameServer', true);
  if (r !== false)
    return r.split(/[ \f\t\v,]+/g);
  r = win32_regQuery(path, 'DhcpNameServer', true);
  if (r !== false)
    return r.split(/[ \f\t\v,]+/g);

  return false;
}

function win32_regQuery(path, key, recurse) {
  var cmd = [
    'reg',
    'query',
    path,
    (recurse ? '/s ' : '') + '/v ' + key,
    '2>NUL',
    '|',
    'findstr ' + key
  ].join(' ');

  try {
    var r = execSync(cmd, { encoding: 'utf8' });
    r = /^([^ \t]+)[ \t]+([^ \t]+)[ \t]+(.+)$/mg.exec(r.trim());
    if (r && r[3])
      return r[3];
  } catch (ex) {}
  return false;
}

function win32_expandEnvironmentStrings(str) {
  var envVars = Object.keys(process.env);
  return str.replace(/%([^%]+)%/g, function(match, varName) {
    varName = varName.toUpperCase();
    for (var i = 0; i < envVars.length; ++i) {
      if (varName === envVars[i].toUpperCase()) {
        return process.env[envVars[i]];
      }
    }
    return match;
  });
}

function android_getNameServers() {
  var servers = false;
  for (var i = 0; i < 8; ++i) {
    var cmd = 'getprop net.dns' + (i + 1);

    try {
      var r = execSync(cmd, { encoding: 'utf8' });
      r = r.trim();
      if (r) {
        if (!servers)
          servers = [ r ];
        else
          servers.push(r);
      }
    } catch (ex) {}
  }
  return servers;
}

function parseResolvConf() {
  var ret = false;
  var lines;

  try {
    lines = fs.readFileSync('/etc/resolv.conf', 'utf8').split(/\r?\n/g);
    ret = {
      nameserver: null,
      search: null,
      sortlist: null,
      options: {
        ndots: null,
        timeout: null,
        attempts: null,
        rotate: null,
        inet6: null,
        use_vc: null
      },
      lookup: null
    };
    for (var i = 0; i < lines.length; ++i) {
      var line = lines[i].replace(REGEX_COMMENT, '')
                         .trim()
                         .replace(REGEX_WHITESPACE, ' ');
      var m = REGEX_TYPES.exec(line);
      if (m) {
        var val = line.slice(m[0].length);
        if (!val.length)
          continue;
        switch (m[1]) {
          case 'nameserver':
            if (!isIP(val))
              continue;
            if (!ret.nameserver)
              ret.nameserver = [val];
            else if (ret.nameserver.length < 3)
              ret.nameserver.push(val);
            break;
          case 'domain':
            ret.search = [val];
            break;
          case 'search':
            val = val.split(' ');
            if (val.length > 6)
              val = val.slice(0, 6);
            ret.search = val;
            break;
          case 'sortlist':
            val = val.split(' ');
            if (val.length > 10)
              val = val.slice(0, 10);
            for (var j = 0; j < val.length; ++j) {
              var network = parseAddrMask(val[j], false);
              if (network !== undefined)
                val[j] = network;
              else {
                spliceOne(val, j);
                --j;
              }
            }
            if (val.length)
              ret.sortlist = val;
            break;
          case 'options':
            fillIn(ret.options, parseResolvOptions(val.split(' ')));
            break;
          case 'lookup':
            val = val.split(' ');
            for (var i = 0, loc; i < val.length; ++i) {
              loc = val[i].toLowerCase();
              if (loc === 'bind')
                loc = 'dns';
              else if (loc === 'file')
                loc = 'files';
              if (loc === 'dns' || loc === 'files') {
                if (!ret.lookup)
                  ret.lookup = [loc];
                else if (ret.lookup.indexOf(loc) === -1)
                  ret.lookup.push(loc);
              }
            }
            break;
        }
      }
    }
  } catch (ex) {}

  return ret;
}

function parseResolvOptions(options) {
  var ret = {
    attempts: null,
    inet6: null,
    ndots: null,
    rotate: null,
    singleRequest: null,
    timeout: null,
    use_vc: null
  };
  for (var j = 0; j < options.length; ++j) {
    m = REGEX_OPTS.exec(options[i]);
    if (m) {
      val = m[2];
      switch (m[1]) {
        case 'attempts':
          if (val) {
            val = parseInt(val, 10);
            if (val <= 5)
              ret.attempts = val;
          }
          break;
        case 'inet6':
          ret.inet6 = true;
          break;
        case 'ndots':
          if (val) {
            val = parseInt(val, 10);
            if (val <= 15)
              ret.ndots = val;
          }
          break;
        case 'rotate':
          ret.rotate = true;
          break;
        case 'single-request':
          ret.singleRequest = true;
          break;
        case 'timeout':
          if (val) {
            val = parseInt(val, 10);
            if (val <= 30)
              ret.timeout = val;
          }
          break;
        case 'use-vc':
          ret.use_vc = true;
          break;
      }
    }
  }
  return ret;
}

function parseNsswitchConf() {
  // TODO: search for explicitly defined services file path too
  var ret = false;
  var seen = [];

  try {
    var data = fs.readFileSync('/etc/nsswitch.conf', 'utf8');
    var m = /^hosts:[ \f\t\v]*(.+)$/m.exec(data);
    if (m) {
      var order = m[1].replace(REGEX_COMMENT, '').trim();
      order = order.match(/(?:\[[^\]]+\])|(?:[^ \f\t\v,]+)/g);
      for (var i = 0, loc; i < order.length; ++i) {
        loc = order[i].toLowerCase();
        if (loc && loc[0] === '[' && loc[loc.length - 1] === ']') {
          // Parse [!]status=action pair(s)
          loc = loc.split(' ');
          for (var j = 0; j < loc.length; ++j) {
            m = REGEX_NSSWITCH_ACTION.exec(loc[j]);
            if (m)
              loc[j] = [ m[1] === '!', m[2], m[3] ];
            else {
              spliceOne(loc, j);
              --j;
            }
          }
        } else if (seen.indexOf(loc) !== -1)
          continue;
        if (typeof loc === 'string' && loc !== 'dns' && loc !== 'files')
          loc = order[i];
        if (!ret)
          ret = [loc];
        else
          ret.push(loc);
      }
    }
  } catch (ex) {}

  if (ret)
    ret = { lookup: ret };
  return ret;
}

function parseHostConf() {
  var ret = false;

  try {
    var data = fs.readFileSync('/etc/host.conf', 'utf8');
    var m = /^order[ \f\t\v]+(.+)$/m.exec(data);
    if (m) {
      var order = m[1].replace(REGEX_COMMENT, '')
                      .trim().split(REGEX_WHITESPACE);
      for (var i = 0, loc; i < order.length; ++i) {
        loc = order[i].toLowerCase();
        if (loc === 'bind' && (!ret || ret.indexOf('dns') === -1)) {
          if (!ret)
            ret = ['dns'];
          else
            ret.push('dns');
        } else if (loc === 'hosts' && (!ret || ret.indexOf('files') === -1)) {
          if (!ret)
            ret = ['files'];
          else
            ret.push('files');
        }
      }
    }
  } catch (ex) {}

  if (ret)
    ret = { lookup: ret };

  return ret;
}

function searchHostAliases(name, cb) {
  var ret = false;
  var regexp = new RegExp(
    '^' + regexpEscape(name) + '[ \\t]+([A-Za-z0-9\\-\\.]+)', 'i'
  );
  var data = new Buffer(128);
  var last = '';
  var lines;
  var m;

  function closecb(err) {
    cb(ret || name);
  }
  fs.open(process.env.HOSTALIASES, 'r', function(err, fd) {
    if (err)
      return closecb();
    fs.read(fd, data, 0, data.length, null, function readcb(err, nb) {
      if (err)
        return fs.close(fd, closecb);

      if (nb) {
        lines = (last + data.toString('ascii', 0, nb)).split(/\r?\n/g);
        last = lines[lines.length - 1];
        for (var i = 0; i < lines.length - 1; ++i) {
          m = regexp.exec(lines[i]);
          if (m) {
            ret = m[1];
            return fs.close(fd, closecb);
          }
        }
        fs.read(fd, data, 0, data.length, null, readcb);
      } else {
        if (ret === false && last) {
          m = regexp.exec(last);
          if (m)
            ret = m[1];
        }
        fs.close(fd, closecb);
      }
    });
  });
}

// By default child_process.execSync() writes stderr data directly to the parent
// process's stderr. We don't want that, so we use our own wrapper ...
function execSync(cmd, opts) {
  opts.stdio = ['pipe', 'pipe', 'ignore'];
  return cp.execSync(cmd, opts);
}


// Initialize DNS settings
(function initDNS() {
  var r;

  if (process.platform === 'win32') {
    // Store the DatabasePath to avoid looking it up every time in
    // lookupService() and fileLookup()
    var path = 'HKLM\\System\\CurrentControlSet\\Services\\Tcpip\\Parameters';
    win32_dbPath = win32_regQuery(path, 'DataBasePath', false);
    if (win32_dbPath)
      win32_dbPath = win32_expandEnvironmentStrings(win32_dbPath);
    // TODO: create fallbacks for win32_dbPath if it's not set in registry for
    //       some reason (e.g. try "%SystemRoot%\system32\drivers\etc" then
    //       "C:\Windows\system32\drivers\etc")?

    // Try WMI first
    r = win32_wmiDNSInfo();
    if (r !== false)
      fillIn(dnsConfig, r);
    else {
      // Try registry next, resulting in only nameserver info for now ...
      r = win32_getRegNameServers();
      if (r !== false)
        fillIn(dnsConfig, { nameserver: r });
    }
  } else if (process.platform === 'android') {
    // Only nameserver info for now ...
    r = android_getNameServers();
    if (r !== false)
      fillIn(dnsConfig, { nameserver: r });
  } else {
    // *nix

    // Check environment
    if (process.env.LOCALDOMAIN) {
      r = process.env.LOCALDOMAIN.split(/[ \t]+/);
      if (r.length > 6)
        r = r.slice(0, 6);
      if (r.length && dnsConfig.search === null)
        dnsConfig.search = r;
    }
    if (process.env.RES_OPTIONS)
      fillIn(dnsConfig.options, parseResolvOptions(process.env.RES_OPTIONS));

    r = parseResolvConf();
    if (r !== false)
      fillIn(dnsConfig, r);

    if (!dnsConfig.lookup) {
      r = parseNsswitchConf();
      if (r !== false)
        fillIn(dnsConfig, r);
    }

    if (!dnsConfig.lookup) {
      r = parseHostConf();
      if (r !== false)
        fillIn(dnsConfig, r);
    }
  }

  // Set missing parameters to some sensible defaults
  fillIn(dnsConfig, DNS_DEFAULTS);

  if (process.platform !== 'win32') {
    // Load any non-dns/files nss modules in case we need to try them during a
    // request
    for (var i = 0; i < dnsConfig.lookup.length; ++i) {
      var loc = dnsConfig.lookup[i];
      if (typeof loc === 'string' && loc !== 'dns' && loc !== 'files') {
        try {
          nssModules[loc] = new NSSModule(loc);
        } catch (ex) {
          // If we get here, it either means the module couldn't be loaded or
          // there were no host resolution functions exported by the module
          spliceOne(dnsConfig.lookup, i);
          --i;
        }
      }
    }
  }

  // Reformat nameservers to cache IP type
  for (var i = 0; i < dnsConfig.nameserver.length; ++i) {
    dnsConfig.nameserver[i] = [ isIP(dnsConfig.nameserver[i]),
                                dnsConfig.nameserver[i] ];
  }
})();

'use strict';

const {
  ArrayPrototypeFilter,
  ArrayPrototypeMap,
  ArrayPrototypeSort,
  JSONParse,
  StringPrototypeReplace,
  Symbol,
} = primordials;

const {
  DNSWrap,
  GetNameInfoReqWrap,
  getnameinfo,
} = internalBinding('dns');
const { Buffer } = require('buffer');
const { dnsException } = require('internal/errors');
const { AsyncResource } = require('async_hooks');

const kResource = Symbol('kResource');
const kOnComplete = Symbol('kOnComplete');

class ReqWrap {
  constructor(name) {
    this[kResource] = new AsyncResource(name);
    this[kOnComplete] = undefined;
  }

  set oncomplete(f) {
    this[kOnComplete] = function(...args) {
      try {
        return this[kResource].runInAsyncScope(f, this, ...args);
      } finally {
        this[kResource].emitDestroy();
      }
    };
  }

  get oncomplete() {
    return this[kOnComplete];
  }
}

class QueryReqWrap extends ReqWrap {
  constructor() {
    super('QUERYWRAP');
  }
}

class GetAddrInfoReqWrap extends ReqWrap {
  constructor() {
    super('GETADDRINFOREQWRAP');
  }
}

function unwrapBindata(s, isFQDN = false) {
  if (typeof s !== 'string') {
    s = Buffer.from(s).toString();
  }
  if (isFQDN) {
    s = StringPrototypeReplace(s, /\.$/, '');
  }
  return s;
}

const DNS_MAP_FNS = {};
const DNS_MAP_TYPES = {};
[
  // https://www.iana.org/assignments/dns-parameters/dns-parameters.xhtml
  ['Any', 255, (answer, req) => {
    let mapped = DNS_MAP_FNS[answer.type](answer, { ttl: true });
    if (typeof mapped === 'string') {
      mapped = { value: mapped };
    } else if (DNS_MAP_TYPES[answer.type] === 'TXT') {
      mapped = { entries: mapped };
    }
    mapped.type = DNS_MAP_TYPES[answer.type];
    return mapped;
  }],
  ['A', 1, (answer, req) => {
    if (req.ttl) {
      return {
        ttl: answer.ttl,
        address: unwrapBindata(answer.rdata.ipv4_address, true),
      };
    }
    return unwrapBindata(answer.rdata.ipv4_address);
  }],
  ['Aaaa', 28, (answer, req) => {
    if (req.ttl) {
      return {
        ttl: answer.ttl,
        address: unwrapBindata(answer.rdata.ipv6_address, true),
      };
    }
    return unwrapBindata(answer.rdata.ipv6_address);
  }],
  ['Caa', 257, (answer) => {
    return {
      critical: answer.rdata.flags,
      [unwrapBindata(answer.rdata.tag)]: unwrapBindata(answer.rdata.value),
    };
  }],
  ['Cname', 5, (answer) => {
    return answer.rdata.cname;
  }],
  ['Mx', 15, (answer) => {
    return {
      priority: answer.rdata.preference,
      exchange: unwrapBindata(answer.rdata.exchange, true),
    };
  }],
  ['Ns', 2, (answer) => {
    return unwrapBindata(answer.rdata.nsdname, true);
  }],
  ['Txt', 16, (answer) => {
    return ArrayPrototypeMap(
      answer.rdata.txt_strings,
      (s) => unwrapBindata(s, true));
  }],
  ['Srv', 33, (answer) => {
    return {
      priority: answer.rdata.priority,
      weight: answer.rdata.weight,
      port: answer.rdata.port,
      name: unwrapBindata(answer.rdata.target),
    };
  }],
  ['Ptr', 12, (answer) => {
    return unwrapBindata(answer.rdata.ptrdname, true);
  }],
  ['Naptr', 35, (answer) => {
    return {
      order: answer.rdata.order,
      preference: answer.rdata.preference,
      flags: answer.rdata.flags,
      service: answer.rdata.service,
      regexp: unwrapBindata(answer.rdata.regexp),
      replacement: unwrapBindata(answer.rdata.replacement),
    };
  }],
  ['Soa', 6, (answer) => {
    return {
      nsname: unwrapBindata(answer.rdata.mname, true),
      hostmaster: unwrapBindata(answer.rdata.rname, true),
      serial: answer.rdata.serial,
      refresh: answer.rdata.refresh,
      retry: answer.rdata.retry,
      expire: answer.rdata.expire,
      minttl: answer.rdata.minimum,
    };
  }],
].forEach(({ 0: name, 1: rr, 2: mapfn }) => {
  DNS_MAP_TYPES[rr] = name.toUpperCase();
  DNS_MAP_FNS[rr] = mapfn;
  const bindingName = `query${name}`;
  DNSWrap.prototype[bindingName] = function(req, hostname) {
    this.getGeneral(hostname, rr)
      .then((json) => {
        const data = JSONParse(json);
        let mapped;
        if (name === 'Txt') {
          mapped = ArrayPrototypeFilter(
            ArrayPrototypeMap(
              ArrayPrototypeFilter(
                data.replies_tree[0].answer,
                (a) => a.type === 16),
              (a) => mapfn(a, req)),
            (a) => a.length > 0);
        } else {
          mapped = ArrayPrototypeMap(
            data.replies_tree[0].answer,
            (a) => mapfn(a, req),
          );
        }
        if (name === 'Soa') {
          mapped = mapped[0];
        }
        return mapped;
      }, (e) => {
        throw dnsException(e, bindingName, name);
      })
      .then((v) => {
        req.oncomplete(null, v);
      }, (e) => {
        req.oncomplete(e);
      });
  };
});

DNSWrap.prototype.getHostByAddr = function(req, address) {
  this.getHostnames(address)
    .then((v) => {
      req.oncomplete(null, v);
    })
    .catch((e) => {
      req.oncomplete(dnsException(e, 'getHostByAddr', address));
    });
};

let implicitResolver;
function getImplicitResolver() {
  if (!implicitResolver) {
    implicitResolver = new DNSWrap(-1);
  }
  return implicitResolver;
}

function getaddrinfo(req, hostname, family, hints, verbatim) {
  getImplicitResolver()
    .getAddresses(hostname)
    .then((json) => {
      const data = JSONParse(json);
      let addresses = data.just_address_answers;
      if (!verbatim) {
        ArrayPrototypeSort(addresses, (a, b) => {
          if (a.address_type === b.address_type) {
            return 0;
          }
          if (a.address_type === 'IPv4' && b.address_type === 'IPv6') {
            return -1;
          }
          return 1;
        });
      }
      if (family !== 0) {
        const type = { 4: 'IPv4', 6: 'IPv6' }[family];
        addresses = ArrayPrototypeFilter(
          addresses, (a) => a.address_type === type);
      }
      return ArrayPrototypeMap(addresses, (a) => a.address_data);
    }, (e) => {
      throw dnsException(e, 'getaddrinfo', hostname);
    })
    .then((v) => {
      req.oncomplete(null, v);
    }, (e) => {
      req.oncomplete(e);
    });
}

module.exports = {
  QueryReqWrap,
  GetAddrInfoReqWrap,
  GetNameInfoReqWrap,
  getaddrinfo,
  getnameinfo,
};

"use strict";

Object.defineProperty(exports, "__esModule", {
  value: true
});
exports.moduleResolve = moduleResolve;
exports.resolve = resolve;

function _url() {
  const data = require("url");

  _url = function () {
    return data;
  };

  return data;
}

function _fs() {
  const data = _interopRequireWildcard(require("fs"), true);

  _fs = function () {
    return data;
  };

  return data;
}

function _path() {
  const data = require("path");

  _path = function () {
    return data;
  };

  return data;
}

function _assert() {
  const data = require("assert");

  _assert = function () {
    return data;
  };

  return data;
}

function _util() {
  const data = require("util");

  _util = function () {
    return data;
  };

  return data;
}

function _getRequireWildcardCache(nodeInterop) { if (typeof WeakMap !== "function") return null; var cacheBabelInterop = new WeakMap(); var cacheNodeInterop = new WeakMap(); return (_getRequireWildcardCache = function (nodeInterop) { return nodeInterop ? cacheNodeInterop : cacheBabelInterop; })(nodeInterop); }

function _interopRequireWildcard(obj, nodeInterop) { if (!nodeInterop && obj && obj.__esModule) { return obj; } if (obj === null || typeof obj !== "object" && typeof obj !== "function") { return { default: obj }; } var cache = _getRequireWildcardCache(nodeInterop); if (cache && cache.has(obj)) { return cache.get(obj); } var newObj = {}; var hasPropertyDescriptor = Object.defineProperty && Object.getOwnPropertyDescriptor; for (var key in obj) { if (key !== "default" && Object.prototype.hasOwnProperty.call(obj, key)) { var desc = hasPropertyDescriptor ? Object.getOwnPropertyDescriptor(obj, key) : null; if (desc && (desc.get || desc.set)) { Object.defineProperty(newObj, key, desc); } else { newObj[key] = obj[key]; } } } newObj.default = obj; if (cache) { cache.set(obj, newObj); } return newObj; }

function asyncGeneratorStep(gen, resolve, reject, _next, _throw, key, arg) { try { var info = gen[key](arg); var value = info.value; } catch (error) { reject(error); return; } if (info.done) { resolve(value); } else { Promise.resolve(value).then(_next, _throw); } }

function _asyncToGenerator(fn) { return function () { var self = this, args = arguments; return new Promise(function (resolve, reject) { var gen = fn.apply(self, args); function _next(value) { asyncGeneratorStep(gen, resolve, reject, _next, _throw, "next", value); } function _throw(err) { asyncGeneratorStep(gen, resolve, reject, _next, _throw, "throw", err); } _next(undefined); }); }; }

function createCommonjsModule(fn) {
  var module = {
    exports: {}
  };
  return fn(module, module.exports), module.exports;
}

const SEMVER_SPEC_VERSION = '2.0.0';
const MAX_LENGTH$2 = 256;
const MAX_SAFE_INTEGER$1 = Number.MAX_SAFE_INTEGER || 9007199254740991;
const MAX_SAFE_COMPONENT_LENGTH = 16;
var constants = {
  SEMVER_SPEC_VERSION,
  MAX_LENGTH: MAX_LENGTH$2,
  MAX_SAFE_INTEGER: MAX_SAFE_INTEGER$1,
  MAX_SAFE_COMPONENT_LENGTH
};
const debug = typeof process === 'object' && process.env && process.env.NODE_DEBUG && /\bsemver\b/i.test(process.env.NODE_DEBUG) ? (...args) => console.error('SEMVER', ...args) : () => {};
var debug_1 = debug;
var re_1 = createCommonjsModule(function (module, exports) {
  const {
    MAX_SAFE_COMPONENT_LENGTH
  } = constants;
  exports = module.exports = {};
  const re = exports.re = [];
  const src = exports.src = [];
  const t = exports.t = {};
  let R = 0;

  const createToken = (name, value, isGlobal) => {
    const index = R++;
    debug_1(index, value);
    t[name] = index;
    src[index] = value;
    re[index] = new RegExp(value, isGlobal ? 'g' : undefined);
  };

  createToken('NUMERICIDENTIFIER', '0|[1-9]\\d*');
  createToken('NUMERICIDENTIFIERLOOSE', '[0-9]+');
  createToken('NONNUMERICIDENTIFIER', '\\d*[a-zA-Z-][a-zA-Z0-9-]*');
  createToken('MAINVERSION', `(${src[t.NUMERICIDENTIFIER]})\\.` + `(${src[t.NUMERICIDENTIFIER]})\\.` + `(${src[t.NUMERICIDENTIFIER]})`);
  createToken('MAINVERSIONLOOSE', `(${src[t.NUMERICIDENTIFIERLOOSE]})\\.` + `(${src[t.NUMERICIDENTIFIERLOOSE]})\\.` + `(${src[t.NUMERICIDENTIFIERLOOSE]})`);
  createToken('PRERELEASEIDENTIFIER', `(?:${src[t.NUMERICIDENTIFIER]}|${src[t.NONNUMERICIDENTIFIER]})`);
  createToken('PRERELEASEIDENTIFIERLOOSE', `(?:${src[t.NUMERICIDENTIFIERLOOSE]}|${src[t.NONNUMERICIDENTIFIER]})`);
  createToken('PRERELEASE', `(?:-(${src[t.PRERELEASEIDENTIFIER]}(?:\\.${src[t.PRERELEASEIDENTIFIER]})*))`);
  createToken('PRERELEASELOOSE', `(?:-?(${src[t.PRERELEASEIDENTIFIERLOOSE]}(?:\\.${src[t.PRERELEASEIDENTIFIERLOOSE]})*))`);
  createToken('BUILDIDENTIFIER', '[0-9A-Za-z-]+');
  createToken('BUILD', `(?:\\+(${src[t.BUILDIDENTIFIER]}(?:\\.${src[t.BUILDIDENTIFIER]})*))`);
  createToken('FULLPLAIN', `v?${src[t.MAINVERSION]}${src[t.PRERELEASE]}?${src[t.BUILD]}?`);
  createToken('FULL', `^${src[t.FULLPLAIN]}$`);
  createToken('LOOSEPLAIN', `[v=\\s]*${src[t.MAINVERSIONLOOSE]}${src[t.PRERELEASELOOSE]}?${src[t.BUILD]}?`);
  createToken('LOOSE', `^${src[t.LOOSEPLAIN]}$`);
  createToken('GTLT', '((?:<|>)?=?)');
  createToken('XRANGEIDENTIFIERLOOSE', `${src[t.NUMERICIDENTIFIERLOOSE]}|x|X|\\*`);
  createToken('XRANGEIDENTIFIER', `${src[t.NUMERICIDENTIFIER]}|x|X|\\*`);
  createToken('XRANGEPLAIN', `[v=\\s]*(${src[t.XRANGEIDENTIFIER]})` + `(?:\\.(${src[t.XRANGEIDENTIFIER]})` + `(?:\\.(${src[t.XRANGEIDENTIFIER]})` + `(?:${src[t.PRERELEASE]})?${src[t.BUILD]}?` + `)?)?`);
  createToken('XRANGEPLAINLOOSE', `[v=\\s]*(${src[t.XRANGEIDENTIFIERLOOSE]})` + `(?:\\.(${src[t.XRANGEIDENTIFIERLOOSE]})` + `(?:\\.(${src[t.XRANGEIDENTIFIERLOOSE]})` + `(?:${src[t.PRERELEASELOOSE]})?${src[t.BUILD]}?` + `)?)?`);
  createToken('XRANGE', `^${src[t.GTLT]}\\s*${src[t.XRANGEPLAIN]}$`);
  createToken('XRANGELOOSE', `^${src[t.GTLT]}\\s*${src[t.XRANGEPLAINLOOSE]}$`);
  createToken('COERCE', `${'(^|[^\\d])' + '(\\d{1,'}${MAX_SAFE_COMPONENT_LENGTH}})` + `(?:\\.(\\d{1,${MAX_SAFE_COMPONENT_LENGTH}}))?` + `(?:\\.(\\d{1,${MAX_SAFE_COMPONENT_LENGTH}}))?` + `(?:$|[^\\d])`);
  createToken('COERCERTL', src[t.COERCE], true);
  createToken('LONETILDE', '(?:~>?)');
  createToken('TILDETRIM', `(\\s*)${src[t.LONETILDE]}\\s+`, true);
  exports.tildeTrimReplace = '$1~';
  createToken('TILDE', `^${src[t.LONETILDE]}${src[t.XRANGEPLAIN]}$`);
  createToken('TILDELOOSE', `^${src[t.LONETILDE]}${src[t.XRANGEPLAINLOOSE]}$`);
  createToken('LONECARET', '(?:\\^)');
  createToken('CARETTRIM', `(\\s*)${src[t.LONECARET]}\\s+`, true);
  exports.caretTrimReplace = '$1^';
  createToken('CARET', `^${src[t.LONECARET]}${src[t.XRANGEPLAIN]}$`);
  createToken('CARETLOOSE', `^${src[t.LONECARET]}${src[t.XRANGEPLAINLOOSE]}$`);
  createToken('COMPARATORLOOSE', `^${src[t.GTLT]}\\s*(${src[t.LOOSEPLAIN]})$|^$`);
  createToken('COMPARATOR', `^${src[t.GTLT]}\\s*(${src[t.FULLPLAIN]})$|^$`);
  createToken('COMPARATORTRIM', `(\\s*)${src[t.GTLT]}\\s*(${src[t.LOOSEPLAIN]}|${src[t.XRANGEPLAIN]})`, true);
  exports.comparatorTrimReplace = '$1$2$3';
  createToken('HYPHENRANGE', `^\\s*(${src[t.XRANGEPLAIN]})` + `\\s+-\\s+` + `(${src[t.XRANGEPLAIN]})` + `\\s*$`);
  createToken('HYPHENRANGELOOSE', `^\\s*(${src[t.XRANGEPLAINLOOSE]})` + `\\s+-\\s+` + `(${src[t.XRANGEPLAINLOOSE]})` + `\\s*$`);
  createToken('STAR', '(<|>)?=?\\s*\\*');
  createToken('GTE0', '^\\s*>=\\s*0\.0\.0\\s*$');
  createToken('GTE0PRE', '^\\s*>=\\s*0\.0\.0-0\\s*$');
});
const opts = ['includePrerelease', 'loose', 'rtl'];

const parseOptions = options => !options ? {} : typeof options !== 'object' ? {
  loose: true
} : opts.filter(k => options[k]).reduce((options, k) => {
  options[k] = true;
  return options;
}, {});

var parseOptions_1 = parseOptions;
const numeric = /^[0-9]+$/;

const compareIdentifiers$1 = (a, b) => {
  const anum = numeric.test(a);
  const bnum = numeric.test(b);

  if (anum && bnum) {
    a = +a;
    b = +b;
  }

  return a === b ? 0 : anum && !bnum ? -1 : bnum && !anum ? 1 : a < b ? -1 : 1;
};

const rcompareIdentifiers = (a, b) => compareIdentifiers$1(b, a);

var identifiers = {
  compareIdentifiers: compareIdentifiers$1,
  rcompareIdentifiers
};
const {
  MAX_LENGTH: MAX_LENGTH$1,
  MAX_SAFE_INTEGER
} = constants;
const {
  re: re$4,
  t: t$4
} = re_1;
const {
  compareIdentifiers
} = identifiers;

class SemVer {
  constructor(version, options) {
    options = parseOptions_1(options);

    if (version instanceof SemVer) {
      if (version.loose === !!options.loose && version.includePrerelease === !!options.includePrerelease) {
        return version;
      } else {
        version = version.version;
      }
    } else if (typeof version !== 'string') {
      throw new TypeError(`Invalid Version: ${version}`);
    }

    if (version.length > MAX_LENGTH$1) {
      throw new TypeError(`version is longer than ${MAX_LENGTH$1} characters`);
    }

    debug_1('SemVer', version, options);
    this.options = options;
    this.loose = !!options.loose;
    this.includePrerelease = !!options.includePrerelease;
    const m = version.trim().match(options.loose ? re$4[t$4.LOOSE] : re$4[t$4.FULL]);

    if (!m) {
      throw new TypeError(`Invalid Version: ${version}`);
    }

    this.raw = version;
    this.major = +m[1];
    this.minor = +m[2];
    this.patch = +m[3];

    if (this.major > MAX_SAFE_INTEGER || this.major < 0) {
      throw new TypeError('Invalid major version');
    }

    if (this.minor > MAX_SAFE_INTEGER || this.minor < 0) {
      throw new TypeError('Invalid minor version');
    }

    if (this.patch > MAX_SAFE_INTEGER || this.patch < 0) {
      throw new TypeError('Invalid patch version');
    }

    if (!m[4]) {
      this.prerelease = [];
    } else {
      this.prerelease = m[4].split('.').map(id => {
        if (/^[0-9]+$/.test(id)) {
          const num = +id;

          if (num >= 0 && num < MAX_SAFE_INTEGER) {
            return num;
          }
        }

        return id;
      });
    }

    this.build = m[5] ? m[5].split('.') : [];
    this.format();
  }

  format() {
    this.version = `${this.major}.${this.minor}.${this.patch}`;

    if (this.prerelease.length) {
      this.version += `-${this.prerelease.join('.')}`;
    }

    return this.version;
  }

  toString() {
    return this.version;
  }

  compare(other) {
    debug_1('SemVer.compare', this.version, this.options, other);

    if (!(other instanceof SemVer)) {
      if (typeof other === 'string' && other === this.version) {
        return 0;
      }

      other = new SemVer(other, this.options);
    }

    if (other.version === this.version) {
      return 0;
    }

    return this.compareMain(other) || this.comparePre(other);
  }

  compareMain(other) {
    if (!(other instanceof SemVer)) {
      other = new SemVer(other, this.options);
    }

    return compareIdentifiers(this.major, other.major) || compareIdentifiers(this.minor, other.minor) || compareIdentifiers(this.patch, other.patch);
  }

  comparePre(other) {
    if (!(other instanceof SemVer)) {
      other = new SemVer(other, this.options);
    }

    if (this.prerelease.length && !other.prerelease.length) {
      return -1;
    } else if (!this.prerelease.length && other.prerelease.length) {
      return 1;
    } else if (!this.prerelease.length && !other.prerelease.length) {
      return 0;
    }

    let i = 0;

    do {
      const a = this.prerelease[i];
      const b = other.prerelease[i];
      debug_1('prerelease compare', i, a, b);

      if (a === undefined && b === undefined) {
        return 0;
      } else if (b === undefined) {
        return 1;
      } else if (a === undefined) {
        return -1;
      } else if (a === b) {
        continue;
      } else {
        return compareIdentifiers(a, b);
      }
    } while (++i);
  }

  compareBuild(other) {
    if (!(other instanceof SemVer)) {
      other = new SemVer(other, this.options);
    }

    let i = 0;

    do {
      const a = this.build[i];
      const b = other.build[i];
      debug_1('prerelease compare', i, a, b);

      if (a === undefined && b === undefined) {
        return 0;
      } else if (b === undefined) {
        return 1;
      } else if (a === undefined) {
        return -1;
      } else if (a === b) {
        continue;
      } else {
        return compareIdentifiers(a, b);
      }
    } while (++i);
  }

  inc(release, identifier) {
    switch (release) {
      case 'premajor':
        this.prerelease.length = 0;
        this.patch = 0;
        this.minor = 0;
        this.major++;
        this.inc('pre', identifier);
        break;

      case 'preminor':
        this.prerelease.length = 0;
        this.patch = 0;
        this.minor++;
        this.inc('pre', identifier);
        break;

      case 'prepatch':
        this.prerelease.length = 0;
        this.inc('patch', identifier);
        this.inc('pre', identifier);
        break;

      case 'prerelease':
        if (this.prerelease.length === 0) {
          this.inc('patch', identifier);
        }

        this.inc('pre', identifier);
        break;

      case 'major':
        if (this.minor !== 0 || this.patch !== 0 || this.prerelease.length === 0) {
          this.major++;
        }

        this.minor = 0;
        this.patch = 0;
        this.prerelease = [];
        break;

      case 'minor':
        if (this.patch !== 0 || this.prerelease.length === 0) {
          this.minor++;
        }

        this.patch = 0;
        this.prerelease = [];
        break;

      case 'patch':
        if (this.prerelease.length === 0) {
          this.patch++;
        }

        this.prerelease = [];
        break;

      case 'pre':
        if (this.prerelease.length === 0) {
          this.prerelease = [0];
        } else {
          let i = this.prerelease.length;

          while (--i >= 0) {
            if (typeof this.prerelease[i] === 'number') {
              this.prerelease[i]++;
              i = -2;
            }
          }

          if (i === -1) {
            this.prerelease.push(0);
          }
        }

        if (identifier) {
          if (this.prerelease[0] === identifier) {
            if (isNaN(this.prerelease[1])) {
              this.prerelease = [identifier, 0];
            }
          } else {
            this.prerelease = [identifier, 0];
          }
        }

        break;

      default:
        throw new Error(`invalid increment argument: ${release}`);
    }

    this.format();
    this.raw = this.version;
    return this;
  }

}

var semver$1 = SemVer;
const {
  MAX_LENGTH
} = constants;
const {
  re: re$3,
  t: t$3
} = re_1;

const parse = (version, options) => {
  options = parseOptions_1(options);

  if (version instanceof semver$1) {
    return version;
  }

  if (typeof version !== 'string') {
    return null;
  }

  if (version.length > MAX_LENGTH) {
    return null;
  }

  const r = options.loose ? re$3[t$3.LOOSE] : re$3[t$3.FULL];

  if (!r.test(version)) {
    return null;
  }

  try {
    return new semver$1(version, options);
  } catch (er) {
    return null;
  }
};

var parse_1 = parse;

const valid$1 = (version, options) => {
  const v = parse_1(version, options);
  return v ? v.version : null;
};

var valid_1 = valid$1;

const clean = (version, options) => {
  const s = parse_1(version.trim().replace(/^[=v]+/, ''), options);
  return s ? s.version : null;
};

var clean_1 = clean;

const inc = (version, release, options, identifier) => {
  if (typeof options === 'string') {
    identifier = options;
    options = undefined;
  }

  try {
    return new semver$1(version, options).inc(release, identifier).version;
  } catch (er) {
    return null;
  }
};

var inc_1 = inc;

const compare = (a, b, loose) => new semver$1(a, loose).compare(new semver$1(b, loose));

var compare_1 = compare;

const eq = (a, b, loose) => compare_1(a, b, loose) === 0;

var eq_1 = eq;

const diff = (version1, version2) => {
  if (eq_1(version1, version2)) {
    return null;
  } else {
    const v1 = parse_1(version1);
    const v2 = parse_1(version2);
    const hasPre = v1.prerelease.length || v2.prerelease.length;
    const prefix = hasPre ? 'pre' : '';
    const defaultResult = hasPre ? 'prerelease' : '';

    for (const key in v1) {
      if (key === 'major' || key === 'minor' || key === 'patch') {
        if (v1[key] !== v2[key]) {
          return prefix + key;
        }
      }
    }

    return defaultResult;
  }
};

var diff_1 = diff;

const major = (a, loose) => new semver$1(a, loose).major;

var major_1 = major;

const minor = (a, loose) => new semver$1(a, loose).minor;

var minor_1 = minor;

const patch = (a, loose) => new semver$1(a, loose).patch;

var patch_1 = patch;

const prerelease = (version, options) => {
  const parsed = parse_1(version, options);
  return parsed && parsed.prerelease.length ? parsed.prerelease : null;
};

var prerelease_1 = prerelease;

const rcompare = (a, b, loose) => compare_1(b, a, loose);

var rcompare_1 = rcompare;

const compareLoose = (a, b) => compare_1(a, b, true);

var compareLoose_1 = compareLoose;

const compareBuild = (a, b, loose) => {
  const versionA = new semver$1(a, loose);
  const versionB = new semver$1(b, loose);
  return versionA.compare(versionB) || versionA.compareBuild(versionB);
};

var compareBuild_1 = compareBuild;

const sort = (list, loose) => list.sort((a, b) => compareBuild_1(a, b, loose));

var sort_1 = sort;

const rsort = (list, loose) => list.sort((a, b) => compareBuild_1(b, a, loose));

var rsort_1 = rsort;

const gt = (a, b, loose) => compare_1(a, b, loose) > 0;

var gt_1 = gt;

const lt = (a, b, loose) => compare_1(a, b, loose) < 0;

var lt_1 = lt;

const neq = (a, b, loose) => compare_1(a, b, loose) !== 0;

var neq_1 = neq;

const gte = (a, b, loose) => compare_1(a, b, loose) >= 0;

var gte_1 = gte;

const lte = (a, b, loose) => compare_1(a, b, loose) <= 0;

var lte_1 = lte;

const cmp = (a, op, b, loose) => {
  switch (op) {
    case '===':
      if (typeof a === 'object') a = a.version;
      if (typeof b === 'object') b = b.version;
      return a === b;

    case '!==':
      if (typeof a === 'object') a = a.version;
      if (typeof b === 'object') b = b.version;
      return a !== b;

    case '':
    case '=':
    case '==':
      return eq_1(a, b, loose);

    case '!=':
      return neq_1(a, b, loose);

    case '>':
      return gt_1(a, b, loose);

    case '>=':
      return gte_1(a, b, loose);

    case '<':
      return lt_1(a, b, loose);

    case '<=':
      return lte_1(a, b, loose);

    default:
      throw new TypeError(`Invalid operator: ${op}`);
  }
};

var cmp_1 = cmp;
const {
  re: re$2,
  t: t$2
} = re_1;

const coerce = (version, options) => {
  if (version instanceof semver$1) {
    return version;
  }

  if (typeof version === 'number') {
    version = String(version);
  }

  if (typeof version !== 'string') {
    return null;
  }

  options = options || {};
  let match = null;

  if (!options.rtl) {
    match = version.match(re$2[t$2.COERCE]);
  } else {
    let next;

    while ((next = re$2[t$2.COERCERTL].exec(version)) && (!match || match.index + match[0].length !== version.length)) {
      if (!match || next.index + next[0].length !== match.index + match[0].length) {
        match = next;
      }

      re$2[t$2.COERCERTL].lastIndex = next.index + next[1].length + next[2].length;
    }

    re$2[t$2.COERCERTL].lastIndex = -1;
  }

  if (match === null) return null;
  return parse_1(`${match[2]}.${match[3] || '0'}.${match[4] || '0'}`, options);
};

var coerce_1 = coerce;

var iterator = function (Yallist) {
  Yallist.prototype[Symbol.iterator] = function* () {
    for (let walker = this.head; walker; walker = walker.next) {
      yield walker.value;
    }
  };
};

var yallist = Yallist;
Yallist.Node = Node;
Yallist.create = Yallist;

function Yallist(list) {
  var self = this;

  if (!(self instanceof Yallist)) {
    self = new Yallist();
  }

  self.tail = null;
  self.head = null;
  self.length = 0;

  if (list && typeof list.forEach === 'function') {
    list.forEach(function (item) {
      self.push(item);
    });
  } else if (arguments.length > 0) {
    for (var i = 0, l = arguments.length; i < l; i++) {
      self.push(arguments[i]);
    }
  }

  return self;
}

Yallist.prototype.removeNode = function (node) {
  if (node.list !== this) {
    throw new Error('removing node which does not belong to this list');
  }

  var next = node.next;
  var prev = node.prev;

  if (next) {
    next.prev = prev;
  }

  if (prev) {
    prev.next = next;
  }

  if (node === this.head) {
    this.head = next;
  }

  if (node === this.tail) {
    this.tail = prev;
  }

  node.list.length--;
  node.next = null;
  node.prev = null;
  node.list = null;
  return next;
};

Yallist.prototype.unshiftNode = function (node) {
  if (node === this.head) {
    return;
  }

  if (node.list) {
    node.list.removeNode(node);
  }

  var head = this.head;
  node.list = this;
  node.next = head;

  if (head) {
    head.prev = node;
  }

  this.head = node;

  if (!this.tail) {
    this.tail = node;
  }

  this.length++;
};

Yallist.prototype.pushNode = function (node) {
  if (node === this.tail) {
    return;
  }

  if (node.list) {
    node.list.removeNode(node);
  }

  var tail = this.tail;
  node.list = this;
  node.prev = tail;

  if (tail) {
    tail.next = node;
  }

  this.tail = node;

  if (!this.head) {
    this.head = node;
  }

  this.length++;
};

Yallist.prototype.push = function () {
  for (var i = 0, l = arguments.length; i < l; i++) {
    push(this, arguments[i]);
  }

  return this.length;
};

Yallist.prototype.unshift = function () {
  for (var i = 0, l = arguments.length; i < l; i++) {
    unshift(this, arguments[i]);
  }

  return this.length;
};

Yallist.prototype.pop = function () {
  if (!this.tail) {
    return undefined;
  }

  var res = this.tail.value;
  this.tail = this.tail.prev;

  if (this.tail) {
    this.tail.next = null;
  } else {
    this.head = null;
  }

  this.length--;
  return res;
};

Yallist.prototype.shift = function () {
  if (!this.head) {
    return undefined;
  }

  var res = this.head.value;
  this.head = this.head.next;

  if (this.head) {
    this.head.prev = null;
  } else {
    this.tail = null;
  }

  this.length--;
  return res;
};

Yallist.prototype.forEach = function (fn, thisp) {
  thisp = thisp || this;

  for (var walker = this.head, i = 0; walker !== null; i++) {
    fn.call(thisp, walker.value, i, this);
    walker = walker.next;
  }
};

Yallist.prototype.forEachReverse = function (fn, thisp) {
  thisp = thisp || this;

  for (var walker = this.tail, i = this.length - 1; walker !== null; i--) {
    fn.call(thisp, walker.value, i, this);
    walker = walker.prev;
  }
};

Yallist.prototype.get = function (n) {
  for (var i = 0, walker = this.head; walker !== null && i < n; i++) {
    walker = walker.next;
  }

  if (i === n && walker !== null) {
    return walker.value;
  }
};

Yallist.prototype.getReverse = function (n) {
  for (var i = 0, walker = this.tail; walker !== null && i < n; i++) {
    walker = walker.prev;
  }

  if (i === n && walker !== null) {
    return walker.value;
  }
};

Yallist.prototype.map = function (fn, thisp) {
  thisp = thisp || this;
  var res = new Yallist();

  for (var walker = this.head; walker !== null;) {
    res.push(fn.call(thisp, walker.value, this));
    walker = walker.next;
  }

  return res;
};

Yallist.prototype.mapReverse = function (fn, thisp) {
  thisp = thisp || this;
  var res = new Yallist();

  for (var walker = this.tail; walker !== null;) {
    res.push(fn.call(thisp, walker.value, this));
    walker = walker.prev;
  }

  return res;
};

Yallist.prototype.reduce = function (fn, initial) {
  var acc;
  var walker = this.head;

  if (arguments.length > 1) {
    acc = initial;
  } else if (this.head) {
    walker = this.head.next;
    acc = this.head.value;
  } else {
    throw new TypeError('Reduce of empty list with no initial value');
  }

  for (var i = 0; walker !== null; i++) {
    acc = fn(acc, walker.value, i);
    walker = walker.next;
  }

  return acc;
};

Yallist.prototype.reduceReverse = function (fn, initial) {
  var acc;
  var walker = this.tail;

  if (arguments.length > 1) {
    acc = initial;
  } else if (this.tail) {
    walker = this.tail.prev;
    acc = this.tail.value;
  } else {
    throw new TypeError('Reduce of empty list with no initial value');
  }

  for (var i = this.length - 1; walker !== null; i--) {
    acc = fn(acc, walker.value, i);
    walker = walker.prev;
  }

  return acc;
};

Yallist.prototype.toArray = function () {
  var arr = new Array(this.length);

  for (var i = 0, walker = this.head; walker !== null; i++) {
    arr[i] = walker.value;
    walker = walker.next;
  }

  return arr;
};

Yallist.prototype.toArrayReverse = function () {
  var arr = new Array(this.length);

  for (var i = 0, walker = this.tail; walker !== null; i++) {
    arr[i] = walker.value;
    walker = walker.prev;
  }

  return arr;
};

Yallist.prototype.slice = function (from, to) {
  to = to || this.length;

  if (to < 0) {
    to += this.length;
  }

  from = from || 0;

  if (from < 0) {
    from += this.length;
  }

  var ret = new Yallist();

  if (to < from || to < 0) {
    return ret;
  }

  if (from < 0) {
    from = 0;
  }

  if (to > this.length) {
    to = this.length;
  }

  for (var i = 0, walker = this.head; walker !== null && i < from; i++) {
    walker = walker.next;
  }

  for (; walker !== null && i < to; i++, walker = walker.next) {
    ret.push(walker.value);
  }

  return ret;
};

Yallist.prototype.sliceReverse = function (from, to) {
  to = to || this.length;

  if (to < 0) {
    to += this.length;
  }

  from = from || 0;

  if (from < 0) {
    from += this.length;
  }

  var ret = new Yallist();

  if (to < from || to < 0) {
    return ret;
  }

  if (from < 0) {
    from = 0;
  }

  if (to > this.length) {
    to = this.length;
  }

  for (var i = this.length, walker = this.tail; walker !== null && i > to; i--) {
    walker = walker.prev;
  }

  for (; walker !== null && i > from; i--, walker = walker.prev) {
    ret.push(walker.value);
  }

  return ret;
};

Yallist.prototype.splice = function (start, deleteCount, ...nodes) {
  if (start > this.length) {
    start = this.length - 1;
  }

  if (start < 0) {
    start = this.length + start;
  }

  for (var i = 0, walker = this.head; walker !== null && i < start; i++) {
    walker = walker.next;
  }

  var ret = [];

  for (var i = 0; walker && i < deleteCount; i++) {
    ret.push(walker.value);
    walker = this.removeNode(walker);
  }

  if (walker === null) {
    walker = this.tail;
  }

  if (walker !== this.head && walker !== this.tail) {
    walker = walker.prev;
  }

  for (var i = 0; i < nodes.length; i++) {
    walker = insert(this, walker, nodes[i]);
  }

  return ret;
};

Yallist.prototype.reverse = function () {
  var head = this.head;
  var tail = this.tail;

  for (var walker = head; walker !== null; walker = walker.prev) {
    var p = walker.prev;
    walker.prev = walker.next;
    walker.next = p;
  }

  this.head = tail;
  this.tail = head;
  return this;
};

function insert(self, node, value) {
  var inserted = node === self.head ? new Node(value, null, node, self) : new Node(value, node, node.next, self);

  if (inserted.next === null) {
    self.tail = inserted;
  }

  if (inserted.prev === null) {
    self.head = inserted;
  }

  self.length++;
  return inserted;
}

function push(self, item) {
  self.tail = new Node(item, self.tail, null, self);

  if (!self.head) {
    self.head = self.tail;
  }

  self.length++;
}

function unshift(self, item) {
  self.head = new Node(item, null, self.head, self);

  if (!self.tail) {
    self.tail = self.head;
  }

  self.length++;
}

function Node(value, prev, next, list) {
  if (!(this instanceof Node)) {
    return new Node(value, prev, next, list);
  }

  this.list = list;
  this.value = value;

  if (prev) {
    prev.next = this;
    this.prev = prev;
  } else {
    this.prev = null;
  }

  if (next) {
    next.prev = this;
    this.next = next;
  } else {
    this.next = null;
  }
}

try {
  iterator(Yallist);
} catch (er) {}

const MAX = Symbol('max');
const LENGTH = Symbol('length');
const LENGTH_CALCULATOR = Symbol('lengthCalculator');
const ALLOW_STALE = Symbol('allowStale');
const MAX_AGE = Symbol('maxAge');
const DISPOSE = Symbol('dispose');
const NO_DISPOSE_ON_SET = Symbol('noDisposeOnSet');
const LRU_LIST = Symbol('lruList');
const CACHE = Symbol('cache');
const UPDATE_AGE_ON_GET = Symbol('updateAgeOnGet');

const naiveLength = () => 1;

class LRUCache {
  constructor(options) {
    if (typeof options === 'number') options = {
      max: options
    };
    if (!options) options = {};
    if (options.max && (typeof options.max !== 'number' || options.max < 0)) throw new TypeError('max must be a non-negative number');
    this[MAX] = options.max || Infinity;
    const lc = options.length || naiveLength;
    this[LENGTH_CALCULATOR] = typeof lc !== 'function' ? naiveLength : lc;
    this[ALLOW_STALE] = options.stale || false;
    if (options.maxAge && typeof options.maxAge !== 'number') throw new TypeError('maxAge must be a number');
    this[MAX_AGE] = options.maxAge || 0;
    this[DISPOSE] = options.dispose;
    this[NO_DISPOSE_ON_SET] = options.noDisposeOnSet || false;
    this[UPDATE_AGE_ON_GET] = options.updateAgeOnGet || false;
    this.reset();
  }

  set max(mL) {
    if (typeof mL !== 'number' || mL < 0) throw new TypeError('max must be a non-negative number');
    this[MAX] = mL || Infinity;
    trim(this);
  }

  get max() {
    return this[MAX];
  }

  set allowStale(allowStale) {
    this[ALLOW_STALE] = !!allowStale;
  }

  get allowStale() {
    return this[ALLOW_STALE];
  }

  set maxAge(mA) {
    if (typeof mA !== 'number') throw new TypeError('maxAge must be a non-negative number');
    this[MAX_AGE] = mA;
    trim(this);
  }

  get maxAge() {
    return this[MAX_AGE];
  }

  set lengthCalculator(lC) {
    if (typeof lC !== 'function') lC = naiveLength;

    if (lC !== this[LENGTH_CALCULATOR]) {
      this[LENGTH_CALCULATOR] = lC;
      this[LENGTH] = 0;
      this[LRU_LIST].forEach(hit => {
        hit.length = this[LENGTH_CALCULATOR](hit.value, hit.key);
        this[LENGTH] += hit.length;
      });
    }

    trim(this);
  }

  get lengthCalculator() {
    return this[LENGTH_CALCULATOR];
  }

  get length() {
    return this[LENGTH];
  }

  get itemCount() {
    return this[LRU_LIST].length;
  }

  rforEach(fn, thisp) {
    thisp = thisp || this;

    for (let walker = this[LRU_LIST].tail; walker !== null;) {
      const prev = walker.prev;
      forEachStep(this, fn, walker, thisp);
      walker = prev;
    }
  }

  forEach(fn, thisp) {
    thisp = thisp || this;

    for (let walker = this[LRU_LIST].head; walker !== null;) {
      const next = walker.next;
      forEachStep(this, fn, walker, thisp);
      walker = next;
    }
  }

  keys() {
    return this[LRU_LIST].toArray().map(k => k.key);
  }

  values() {
    return this[LRU_LIST].toArray().map(k => k.value);
  }

  reset() {
    if (this[DISPOSE] && this[LRU_LIST] && this[LRU_LIST].length) {
      this[LRU_LIST].forEach(hit => this[DISPOSE](hit.key, hit.value));
    }

    this[CACHE] = new Map();
    this[LRU_LIST] = new yallist();
    this[LENGTH] = 0;
  }

  dump() {
    return this[LRU_LIST].map(hit => isStale(this, hit) ? false : {
      k: hit.key,
      v: hit.value,
      e: hit.now + (hit.maxAge || 0)
    }).toArray().filter(h => h);
  }

  dumpLru() {
    return this[LRU_LIST];
  }

  set(key, value, maxAge) {
    maxAge = maxAge || this[MAX_AGE];
    if (maxAge && typeof maxAge !== 'number') throw new TypeError('maxAge must be a number');
    const now = maxAge ? Date.now() : 0;
    const len = this[LENGTH_CALCULATOR](value, key);

    if (this[CACHE].has(key)) {
      if (len > this[MAX]) {
        del(this, this[CACHE].get(key));
        return false;
      }

      const node = this[CACHE].get(key);
      const item = node.value;

      if (this[DISPOSE]) {
        if (!this[NO_DISPOSE_ON_SET]) this[DISPOSE](key, item.value);
      }

      item.now = now;
      item.maxAge = maxAge;
      item.value = value;
      this[LENGTH] += len - item.length;
      item.length = len;
      this.get(key);
      trim(this);
      return true;
    }

    const hit = new Entry(key, value, len, now, maxAge);

    if (hit.length > this[MAX]) {
      if (this[DISPOSE]) this[DISPOSE](key, value);
      return false;
    }

    this[LENGTH] += hit.length;
    this[LRU_LIST].unshift(hit);
    this[CACHE].set(key, this[LRU_LIST].head);
    trim(this);
    return true;
  }

  has(key) {
    if (!this[CACHE].has(key)) return false;
    const hit = this[CACHE].get(key).value;
    return !isStale(this, hit);
  }

  get(key) {
    return get(this, key, true);
  }

  peek(key) {
    return get(this, key, false);
  }

  pop() {
    const node = this[LRU_LIST].tail;
    if (!node) return null;
    del(this, node);
    return node.value;
  }

  del(key) {
    del(this, this[CACHE].get(key));
  }

  load(arr) {
    this.reset();
    const now = Date.now();

    for (let l = arr.length - 1; l >= 0; l--) {
      const hit = arr[l];
      const expiresAt = hit.e || 0;
      if (expiresAt === 0) this.set(hit.k, hit.v);else {
        const maxAge = expiresAt - now;

        if (maxAge > 0) {
          this.set(hit.k, hit.v, maxAge);
        }
      }
    }
  }

  prune() {
    this[CACHE].forEach((value, key) => get(this, key, false));
  }

}

const get = (self, key, doUse) => {
  const node = self[CACHE].get(key);

  if (node) {
    const hit = node.value;

    if (isStale(self, hit)) {
      del(self, node);
      if (!self[ALLOW_STALE]) return undefined;
    } else {
      if (doUse) {
        if (self[UPDATE_AGE_ON_GET]) node.value.now = Date.now();
        self[LRU_LIST].unshiftNode(node);
      }
    }

    return hit.value;
  }
};

const isStale = (self, hit) => {
  if (!hit || !hit.maxAge && !self[MAX_AGE]) return false;
  const diff = Date.now() - hit.now;
  return hit.maxAge ? diff > hit.maxAge : self[MAX_AGE] && diff > self[MAX_AGE];
};

const trim = self => {
  if (self[LENGTH] > self[MAX]) {
    for (let walker = self[LRU_LIST].tail; self[LENGTH] > self[MAX] && walker !== null;) {
      const prev = walker.prev;
      del(self, walker);
      walker = prev;
    }
  }
};

const del = (self, node) => {
  if (node) {
    const hit = node.value;
    if (self[DISPOSE]) self[DISPOSE](hit.key, hit.value);
    self[LENGTH] -= hit.length;
    self[CACHE].delete(hit.key);
    self[LRU_LIST].removeNode(node);
  }
};

class Entry {
  constructor(key, value, length, now, maxAge) {
    this.key = key;
    this.value = value;
    this.length = length;
    this.now = now;
    this.maxAge = maxAge || 0;
  }

}

const forEachStep = (self, fn, node, thisp) => {
  let hit = node.value;

  if (isStale(self, hit)) {
    del(self, node);
    if (!self[ALLOW_STALE]) hit = undefined;
  }

  if (hit) fn.call(thisp, hit.value, hit.key, self);
};

var lruCache = LRUCache;

class Range {
  constructor(range, options) {
    options = parseOptions_1(options);

    if (range instanceof Range) {
      if (range.loose === !!options.loose && range.includePrerelease === !!options.includePrerelease) {
        return range;
      } else {
        return new Range(range.raw, options);
      }
    }

    if (range instanceof comparator) {
      this.raw = range.value;
      this.set = [[range]];
      this.format();
      return this;
    }

    this.options = options;
    this.loose = !!options.loose;
    this.includePrerelease = !!options.includePrerelease;
    this.raw = range;
    this.set = range.split(/\s*\|\|\s*/).map(range => this.parseRange(range.trim())).filter(c => c.length);

    if (!this.set.length) {
      throw new TypeError(`Invalid SemVer Range: ${range}`);
    }

    if (this.set.length > 1) {
      const first = this.set[0];
      this.set = this.set.filter(c => !isNullSet(c[0]));
      if (this.set.length === 0) this.set = [first];else if (this.set.length > 1) {
        for (const c of this.set) {
          if (c.length === 1 && isAny(c[0])) {
            this.set = [c];
            break;
          }
        }
      }
    }

    this.format();
  }

  format() {
    this.range = this.set.map(comps => {
      return comps.join(' ').trim();
    }).join('||').trim();
    return this.range;
  }

  toString() {
    return this.range;
  }

  parseRange(range) {
    range = range.trim();
    const memoOpts = Object.keys(this.options).join(',');
    const memoKey = `parseRange:${memoOpts}:${range}`;
    const cached = cache.get(memoKey);
    if (cached) return cached;
    const loose = this.options.loose;
    const hr = loose ? re$1[t$1.HYPHENRANGELOOSE] : re$1[t$1.HYPHENRANGE];
    range = range.replace(hr, hyphenReplace(this.options.includePrerelease));
    debug_1('hyphen replace', range);
    range = range.replace(re$1[t$1.COMPARATORTRIM], comparatorTrimReplace);
    debug_1('comparator trim', range, re$1[t$1.COMPARATORTRIM]);
    range = range.replace(re$1[t$1.TILDETRIM], tildeTrimReplace);
    range = range.replace(re$1[t$1.CARETTRIM], caretTrimReplace);
    range = range.split(/\s+/).join(' ');
    const compRe = loose ? re$1[t$1.COMPARATORLOOSE] : re$1[t$1.COMPARATOR];
    const rangeList = range.split(' ').map(comp => parseComparator(comp, this.options)).join(' ').split(/\s+/).map(comp => replaceGTE0(comp, this.options)).filter(this.options.loose ? comp => !!comp.match(compRe) : () => true).map(comp => new comparator(comp, this.options));
    rangeList.length;
    const rangeMap = new Map();

    for (const comp of rangeList) {
      if (isNullSet(comp)) return [comp];
      rangeMap.set(comp.value, comp);
    }

    if (rangeMap.size > 1 && rangeMap.has('')) rangeMap.delete('');
    const result = [...rangeMap.values()];
    cache.set(memoKey, result);
    return result;
  }

  intersects(range, options) {
    if (!(range instanceof Range)) {
      throw new TypeError('a Range is required');
    }

    return this.set.some(thisComparators => {
      return isSatisfiable(thisComparators, options) && range.set.some(rangeComparators => {
        return isSatisfiable(rangeComparators, options) && thisComparators.every(thisComparator => {
          return rangeComparators.every(rangeComparator => {
            return thisComparator.intersects(rangeComparator, options);
          });
        });
      });
    });
  }

  test(version) {
    if (!version) {
      return false;
    }

    if (typeof version === 'string') {
      try {
        version = new semver$1(version, this.options);
      } catch (er) {
        return false;
      }
    }

    for (let i = 0; i < this.set.length; i++) {
      if (testSet(this.set[i], version, this.options)) {
        return true;
      }
    }

    return false;
  }

}

var range = Range;
const cache = new lruCache({
  max: 1000
});
const {
  re: re$1,
  t: t$1,
  comparatorTrimReplace,
  tildeTrimReplace,
  caretTrimReplace
} = re_1;

const isNullSet = c => c.value === '<0.0.0-0';

const isAny = c => c.value === '';

const isSatisfiable = (comparators, options) => {
  let result = true;
  const remainingComparators = comparators.slice();
  let testComparator = remainingComparators.pop();

  while (result && remainingComparators.length) {
    result = remainingComparators.every(otherComparator => {
      return testComparator.intersects(otherComparator, options);
    });
    testComparator = remainingComparators.pop();
  }

  return result;
};

const parseComparator = (comp, options) => {
  debug_1('comp', comp, options);
  comp = replaceCarets(comp, options);
  debug_1('caret', comp);
  comp = replaceTildes(comp, options);
  debug_1('tildes', comp);
  comp = replaceXRanges(comp, options);
  debug_1('xrange', comp);
  comp = replaceStars(comp, options);
  debug_1('stars', comp);
  return comp;
};

const isX = id => !id || id.toLowerCase() === 'x' || id === '*';

const replaceTildes = (comp, options) => comp.trim().split(/\s+/).map(comp => {
  return replaceTilde(comp, options);
}).join(' ');

const replaceTilde = (comp, options) => {
  const r = options.loose ? re$1[t$1.TILDELOOSE] : re$1[t$1.TILDE];
  return comp.replace(r, (_, M, m, p, pr) => {
    debug_1('tilde', comp, _, M, m, p, pr);
    let ret;

    if (isX(M)) {
      ret = '';
    } else if (isX(m)) {
      ret = `>=${M}.0.0 <${+M + 1}.0.0-0`;
    } else if (isX(p)) {
      ret = `>=${M}.${m}.0 <${M}.${+m + 1}.0-0`;
    } else if (pr) {
      debug_1('replaceTilde pr', pr);
      ret = `>=${M}.${m}.${p}-${pr} <${M}.${+m + 1}.0-0`;
    } else {
      ret = `>=${M}.${m}.${p} <${M}.${+m + 1}.0-0`;
    }

    debug_1('tilde return', ret);
    return ret;
  });
};

const replaceCarets = (comp, options) => comp.trim().split(/\s+/).map(comp => {
  return replaceCaret(comp, options);
}).join(' ');

const replaceCaret = (comp, options) => {
  debug_1('caret', comp, options);
  const r = options.loose ? re$1[t$1.CARETLOOSE] : re$1[t$1.CARET];
  const z = options.includePrerelease ? '-0' : '';
  return comp.replace(r, (_, M, m, p, pr) => {
    debug_1('caret', comp, _, M, m, p, pr);
    let ret;

    if (isX(M)) {
      ret = '';
    } else if (isX(m)) {
      ret = `>=${M}.0.0${z} <${+M + 1}.0.0-0`;
    } else if (isX(p)) {
      if (M === '0') {
        ret = `>=${M}.${m}.0${z} <${M}.${+m + 1}.0-0`;
      } else {
        ret = `>=${M}.${m}.0${z} <${+M + 1}.0.0-0`;
      }
    } else if (pr) {
      debug_1('replaceCaret pr', pr);

      if (M === '0') {
        if (m === '0') {
          ret = `>=${M}.${m}.${p}-${pr} <${M}.${m}.${+p + 1}-0`;
        } else {
          ret = `>=${M}.${m}.${p}-${pr} <${M}.${+m + 1}.0-0`;
        }
      } else {
        ret = `>=${M}.${m}.${p}-${pr} <${+M + 1}.0.0-0`;
      }
    } else {
      debug_1('no pr');

      if (M === '0') {
        if (m === '0') {
          ret = `>=${M}.${m}.${p}${z} <${M}.${m}.${+p + 1}-0`;
        } else {
          ret = `>=${M}.${m}.${p}${z} <${M}.${+m + 1}.0-0`;
        }
      } else {
        ret = `>=${M}.${m}.${p} <${+M + 1}.0.0-0`;
      }
    }

    debug_1('caret return', ret);
    return ret;
  });
};

const replaceXRanges = (comp, options) => {
  debug_1('replaceXRanges', comp, options);
  return comp.split(/\s+/).map(comp => {
    return replaceXRange(comp, options);
  }).join(' ');
};

const replaceXRange = (comp, options) => {
  comp = comp.trim();
  const r = options.loose ? re$1[t$1.XRANGELOOSE] : re$1[t$1.XRANGE];
  return comp.replace(r, (ret, gtlt, M, m, p, pr) => {
    debug_1('xRange', comp, ret, gtlt, M, m, p, pr);
    const xM = isX(M);
    const xm = xM || isX(m);
    const xp = xm || isX(p);
    const anyX = xp;

    if (gtlt === '=' && anyX) {
      gtlt = '';
    }

    pr = options.includePrerelease ? '-0' : '';

    if (xM) {
      if (gtlt === '>' || gtlt === '<') {
        ret = '<0.0.0-0';
      } else {
        ret = '*';
      }
    } else if (gtlt && anyX) {
      if (xm) {
        m = 0;
      }

      p = 0;

      if (gtlt === '>') {
        gtlt = '>=';

        if (xm) {
          M = +M + 1;
          m = 0;
          p = 0;
        } else {
          m = +m + 1;
          p = 0;
        }
      } else if (gtlt === '<=') {
        gtlt = '<';

        if (xm) {
          M = +M + 1;
        } else {
          m = +m + 1;
        }
      }

      if (gtlt === '<') pr = '-0';
      ret = `${gtlt + M}.${m}.${p}${pr}`;
    } else if (xm) {
      ret = `>=${M}.0.0${pr} <${+M + 1}.0.0-0`;
    } else if (xp) {
      ret = `>=${M}.${m}.0${pr} <${M}.${+m + 1}.0-0`;
    }

    debug_1('xRange return', ret);
    return ret;
  });
};

const replaceStars = (comp, options) => {
  debug_1('replaceStars', comp, options);
  return comp.trim().replace(re$1[t$1.STAR], '');
};

const replaceGTE0 = (comp, options) => {
  debug_1('replaceGTE0', comp, options);
  return comp.trim().replace(re$1[options.includePrerelease ? t$1.GTE0PRE : t$1.GTE0], '');
};

const hyphenReplace = incPr => ($0, from, fM, fm, fp, fpr, fb, to, tM, tm, tp, tpr, tb) => {
  if (isX(fM)) {
    from = '';
  } else if (isX(fm)) {
    from = `>=${fM}.0.0${incPr ? '-0' : ''}`;
  } else if (isX(fp)) {
    from = `>=${fM}.${fm}.0${incPr ? '-0' : ''}`;
  } else if (fpr) {
    from = `>=${from}`;
  } else {
    from = `>=${from}${incPr ? '-0' : ''}`;
  }

  if (isX(tM)) {
    to = '';
  } else if (isX(tm)) {
    to = `<${+tM + 1}.0.0-0`;
  } else if (isX(tp)) {
    to = `<${tM}.${+tm + 1}.0-0`;
  } else if (tpr) {
    to = `<=${tM}.${tm}.${tp}-${tpr}`;
  } else if (incPr) {
    to = `<${tM}.${tm}.${+tp + 1}-0`;
  } else {
    to = `<=${to}`;
  }

  return `${from} ${to}`.trim();
};

const testSet = (set, version, options) => {
  for (let i = 0; i < set.length; i++) {
    if (!set[i].test(version)) {
      return false;
    }
  }

  if (version.prerelease.length && !options.includePrerelease) {
    for (let i = 0; i < set.length; i++) {
      debug_1(set[i].semver);

      if (set[i].semver === comparator.ANY) {
        continue;
      }

      if (set[i].semver.prerelease.length > 0) {
        const allowed = set[i].semver;

        if (allowed.major === version.major && allowed.minor === version.minor && allowed.patch === version.patch) {
          return true;
        }
      }
    }

    return false;
  }

  return true;
};

const ANY$2 = Symbol('SemVer ANY');

class Comparator {
  static get ANY() {
    return ANY$2;
  }

  constructor(comp, options) {
    options = parseOptions_1(options);

    if (comp instanceof Comparator) {
      if (comp.loose === !!options.loose) {
        return comp;
      } else {
        comp = comp.value;
      }
    }

    debug_1('comparator', comp, options);
    this.options = options;
    this.loose = !!options.loose;
    this.parse(comp);

    if (this.semver === ANY$2) {
      this.value = '';
    } else {
      this.value = this.operator + this.semver.version;
    }

    debug_1('comp', this);
  }

  parse(comp) {
    const r = this.options.loose ? re[t.COMPARATORLOOSE] : re[t.COMPARATOR];
    const m = comp.match(r);

    if (!m) {
      throw new TypeError(`Invalid comparator: ${comp}`);
    }

    this.operator = m[1] !== undefined ? m[1] : '';

    if (this.operator === '=') {
      this.operator = '';
    }

    if (!m[2]) {
      this.semver = ANY$2;
    } else {
      this.semver = new semver$1(m[2], this.options.loose);
    }
  }

  toString() {
    return this.value;
  }

  test(version) {
    debug_1('Comparator.test', version, this.options.loose);

    if (this.semver === ANY$2 || version === ANY$2) {
      return true;
    }

    if (typeof version === 'string') {
      try {
        version = new semver$1(version, this.options);
      } catch (er) {
        return false;
      }
    }

    return cmp_1(version, this.operator, this.semver, this.options);
  }

  intersects(comp, options) {
    if (!(comp instanceof Comparator)) {
      throw new TypeError('a Comparator is required');
    }

    if (!options || typeof options !== 'object') {
      options = {
        loose: !!options,
        includePrerelease: false
      };
    }

    if (this.operator === '') {
      if (this.value === '') {
        return true;
      }

      return new range(comp.value, options).test(this.value);
    } else if (comp.operator === '') {
      if (comp.value === '') {
        return true;
      }

      return new range(this.value, options).test(comp.semver);
    }

    const sameDirectionIncreasing = (this.operator === '>=' || this.operator === '>') && (comp.operator === '>=' || comp.operator === '>');
    const sameDirectionDecreasing = (this.operator === '<=' || this.operator === '<') && (comp.operator === '<=' || comp.operator === '<');
    const sameSemVer = this.semver.version === comp.semver.version;
    const differentDirectionsInclusive = (this.operator === '>=' || this.operator === '<=') && (comp.operator === '>=' || comp.operator === '<=');
    const oppositeDirectionsLessThan = cmp_1(this.semver, '<', comp.semver, options) && (this.operator === '>=' || this.operator === '>') && (comp.operator === '<=' || comp.operator === '<');
    const oppositeDirectionsGreaterThan = cmp_1(this.semver, '>', comp.semver, options) && (this.operator === '<=' || this.operator === '<') && (comp.operator === '>=' || comp.operator === '>');
    return sameDirectionIncreasing || sameDirectionDecreasing || sameSemVer && differentDirectionsInclusive || oppositeDirectionsLessThan || oppositeDirectionsGreaterThan;
  }

}

var comparator = Comparator;
const {
  re,
  t
} = re_1;

const satisfies = (version, range$1, options) => {
  try {
    range$1 = new range(range$1, options);
  } catch (er) {
    return false;
  }

  return range$1.test(version);
};

var satisfies_1 = satisfies;

const toComparators = (range$1, options) => new range(range$1, options).set.map(comp => comp.map(c => c.value).join(' ').trim().split(' '));

var toComparators_1 = toComparators;

const maxSatisfying = (versions, range$1, options) => {
  let max = null;
  let maxSV = null;
  let rangeObj = null;

  try {
    rangeObj = new range(range$1, options);
  } catch (er) {
    return null;
  }

  versions.forEach(v => {
    if (rangeObj.test(v)) {
      if (!max || maxSV.compare(v) === -1) {
        max = v;
        maxSV = new semver$1(max, options);
      }
    }
  });
  return max;
};

var maxSatisfying_1 = maxSatisfying;

const minSatisfying = (versions, range$1, options) => {
  let min = null;
  let minSV = null;
  let rangeObj = null;

  try {
    rangeObj = new range(range$1, options);
  } catch (er) {
    return null;
  }

  versions.forEach(v => {
    if (rangeObj.test(v)) {
      if (!min || minSV.compare(v) === 1) {
        min = v;
        minSV = new semver$1(min, options);
      }
    }
  });
  return min;
};

var minSatisfying_1 = minSatisfying;

const minVersion = (range$1, loose) => {
  range$1 = new range(range$1, loose);
  let minver = new semver$1('0.0.0');

  if (range$1.test(minver)) {
    return minver;
  }

  minver = new semver$1('0.0.0-0');

  if (range$1.test(minver)) {
    return minver;
  }

  minver = null;

  for (let i = 0; i < range$1.set.length; ++i) {
    const comparators = range$1.set[i];
    let setMin = null;
    comparators.forEach(comparator => {
      const compver = new semver$1(comparator.semver.version);

      switch (comparator.operator) {
        case '>':
          if (compver.prerelease.length === 0) {
            compver.patch++;
          } else {
            compver.prerelease.push(0);
          }

          compver.raw = compver.format();

        case '':
        case '>=':
          if (!setMin || gt_1(compver, setMin)) {
            setMin = compver;
          }

          break;

        case '<':
        case '<=':
          break;

        default:
          throw new Error(`Unexpected operation: ${comparator.operator}`);
      }
    });
    if (setMin && (!minver || gt_1(minver, setMin))) minver = setMin;
  }

  if (minver && range$1.test(minver)) {
    return minver;
  }

  return null;
};

var minVersion_1 = minVersion;

const validRange = (range$1, options) => {
  try {
    return new range(range$1, options).range || '*';
  } catch (er) {
    return null;
  }
};

var valid = validRange;
const {
  ANY: ANY$1
} = comparator;

const outside = (version, range$1, hilo, options) => {
  version = new semver$1(version, options);
  range$1 = new range(range$1, options);
  let gtfn, ltefn, ltfn, comp, ecomp;

  switch (hilo) {
    case '>':
      gtfn = gt_1;
      ltefn = lte_1;
      ltfn = lt_1;
      comp = '>';
      ecomp = '>=';
      break;

    case '<':
      gtfn = lt_1;
      ltefn = gte_1;
      ltfn = gt_1;
      comp = '<';
      ecomp = '<=';
      break;

    default:
      throw new TypeError('Must provide a hilo val of "<" or ">"');
  }

  if (satisfies_1(version, range$1, options)) {
    return false;
  }

  for (let i = 0; i < range$1.set.length; ++i) {
    const comparators = range$1.set[i];
    let high = null;
    let low = null;
    comparators.forEach(comparator$1 => {
      if (comparator$1.semver === ANY$1) {
        comparator$1 = new comparator('>=0.0.0');
      }

      high = high || comparator$1;
      low = low || comparator$1;

      if (gtfn(comparator$1.semver, high.semver, options)) {
        high = comparator$1;
      } else if (ltfn(comparator$1.semver, low.semver, options)) {
        low = comparator$1;
      }
    });

    if (high.operator === comp || high.operator === ecomp) {
      return false;
    }

    if ((!low.operator || low.operator === comp) && ltefn(version, low.semver)) {
      return false;
    } else if (low.operator === ecomp && ltfn(version, low.semver)) {
      return false;
    }
  }

  return true;
};

var outside_1 = outside;

const gtr = (version, range, options) => outside_1(version, range, '>', options);

var gtr_1 = gtr;

const ltr = (version, range, options) => outside_1(version, range, '<', options);

var ltr_1 = ltr;

const intersects = (r1, r2, options) => {
  r1 = new range(r1, options);
  r2 = new range(r2, options);
  return r1.intersects(r2);
};

var intersects_1 = intersects;

var simplify = (versions, range, options) => {
  const set = [];
  let min = null;
  let prev = null;
  const v = versions.sort((a, b) => compare_1(a, b, options));

  for (const version of v) {
    const included = satisfies_1(version, range, options);

    if (included) {
      prev = version;
      if (!min) min = version;
    } else {
      if (prev) {
        set.push([min, prev]);
      }

      prev = null;
      min = null;
    }
  }

  if (min) set.push([min, null]);
  const ranges = [];

  for (const [min, max] of set) {
    if (min === max) ranges.push(min);else if (!max && min === v[0]) ranges.push('*');else if (!max) ranges.push(`>=${min}`);else if (min === v[0]) ranges.push(`<=${max}`);else ranges.push(`${min} - ${max}`);
  }

  const simplified = ranges.join(' || ');
  const original = typeof range.raw === 'string' ? range.raw : String(range);
  return simplified.length < original.length ? simplified : range;
};

const {
  ANY
} = comparator;

const subset = (sub, dom, options = {}) => {
  if (sub === dom) return true;
  sub = new range(sub, options);
  dom = new range(dom, options);
  let sawNonNull = false;

  OUTER: for (const simpleSub of sub.set) {
    for (const simpleDom of dom.set) {
      const isSub = simpleSubset(simpleSub, simpleDom, options);
      sawNonNull = sawNonNull || isSub !== null;
      if (isSub) continue OUTER;
    }

    if (sawNonNull) return false;
  }

  return true;
};

const simpleSubset = (sub, dom, options) => {
  if (sub === dom) return true;

  if (sub.length === 1 && sub[0].semver === ANY) {
    if (dom.length === 1 && dom[0].semver === ANY) return true;else if (options.includePrerelease) sub = [new comparator('>=0.0.0-0')];else sub = [new comparator('>=0.0.0')];
  }

  if (dom.length === 1 && dom[0].semver === ANY) {
    if (options.includePrerelease) return true;else dom = [new comparator('>=0.0.0')];
  }

  const eqSet = new Set();
  let gt, lt;

  for (const c of sub) {
    if (c.operator === '>' || c.operator === '>=') gt = higherGT(gt, c, options);else if (c.operator === '<' || c.operator === '<=') lt = lowerLT(lt, c, options);else eqSet.add(c.semver);
  }

  if (eqSet.size > 1) return null;
  let gtltComp;

  if (gt && lt) {
    gtltComp = compare_1(gt.semver, lt.semver, options);
    if (gtltComp > 0) return null;else if (gtltComp === 0 && (gt.operator !== '>=' || lt.operator !== '<=')) return null;
  }

  for (const eq of eqSet) {
    if (gt && !satisfies_1(eq, String(gt), options)) return null;
    if (lt && !satisfies_1(eq, String(lt), options)) return null;

    for (const c of dom) {
      if (!satisfies_1(eq, String(c), options)) return false;
    }

    return true;
  }

  let higher, lower;
  let hasDomLT, hasDomGT;
  let needDomLTPre = lt && !options.includePrerelease && lt.semver.prerelease.length ? lt.semver : false;
  let needDomGTPre = gt && !options.includePrerelease && gt.semver.prerelease.length ? gt.semver : false;

  if (needDomLTPre && needDomLTPre.prerelease.length === 1 && lt.operator === '<' && needDomLTPre.prerelease[0] === 0) {
    needDomLTPre = false;
  }

  for (const c of dom) {
    hasDomGT = hasDomGT || c.operator === '>' || c.operator === '>=';
    hasDomLT = hasDomLT || c.operator === '<' || c.operator === '<=';

    if (gt) {
      if (needDomGTPre) {
        if (c.semver.prerelease && c.semver.prerelease.length && c.semver.major === needDomGTPre.major && c.semver.minor === needDomGTPre.minor && c.semver.patch === needDomGTPre.patch) {
          needDomGTPre = false;
        }
      }

      if (c.operator === '>' || c.operator === '>=') {
        higher = higherGT(gt, c, options);
        if (higher === c && higher !== gt) return false;
      } else if (gt.operator === '>=' && !satisfies_1(gt.semver, String(c), options)) return false;
    }

    if (lt) {
      if (needDomLTPre) {
        if (c.semver.prerelease && c.semver.prerelease.length && c.semver.major === needDomLTPre.major && c.semver.minor === needDomLTPre.minor && c.semver.patch === needDomLTPre.patch) {
          needDomLTPre = false;
        }
      }

      if (c.operator === '<' || c.operator === '<=') {
        lower = lowerLT(lt, c, options);
        if (lower === c && lower !== lt) return false;
      } else if (lt.operator === '<=' && !satisfies_1(lt.semver, String(c), options)) return false;
    }

    if (!c.operator && (lt || gt) && gtltComp !== 0) return false;
  }

  if (gt && hasDomLT && !lt && gtltComp !== 0) return false;
  if (lt && hasDomGT && !gt && gtltComp !== 0) return false;
  if (needDomGTPre || needDomLTPre) return false;
  return true;
};

const higherGT = (a, b, options) => {
  if (!a) return b;
  const comp = compare_1(a.semver, b.semver, options);
  return comp > 0 ? a : comp < 0 ? b : b.operator === '>' && a.operator === '>=' ? b : a;
};

const lowerLT = (a, b, options) => {
  if (!a) return b;
  const comp = compare_1(a.semver, b.semver, options);
  return comp < 0 ? a : comp > 0 ? b : b.operator === '<' && a.operator === '<=' ? b : a;
};

var subset_1 = subset;
var semver = {
  re: re_1.re,
  src: re_1.src,
  tokens: re_1.t,
  SEMVER_SPEC_VERSION: constants.SEMVER_SPEC_VERSION,
  SemVer: semver$1,
  compareIdentifiers: identifiers.compareIdentifiers,
  rcompareIdentifiers: identifiers.rcompareIdentifiers,
  parse: parse_1,
  valid: valid_1,
  clean: clean_1,
  inc: inc_1,
  diff: diff_1,
  major: major_1,
  minor: minor_1,
  patch: patch_1,
  prerelease: prerelease_1,
  compare: compare_1,
  rcompare: rcompare_1,
  compareLoose: compareLoose_1,
  compareBuild: compareBuild_1,
  sort: sort_1,
  rsort: rsort_1,
  gt: gt_1,
  lt: lt_1,
  eq: eq_1,
  neq: neq_1,
  gte: gte_1,
  lte: lte_1,
  cmp: cmp_1,
  coerce: coerce_1,
  Comparator: comparator,
  Range: range,
  satisfies: satisfies_1,
  toComparators: toComparators_1,
  maxSatisfying: maxSatisfying_1,
  minSatisfying: minSatisfying_1,
  minVersion: minVersion_1,
  validRange: valid,
  outside: outside_1,
  gtr: gtr_1,
  ltr: ltr_1,
  intersects: intersects_1,
  simplifyRange: simplify,
  subset: subset_1
};

var builtins = function ({
  version = process.version,
  experimental = false
} = {}) {
  var coreModules = ['assert', 'buffer', 'child_process', 'cluster', 'console', 'constants', 'crypto', 'dgram', 'dns', 'domain', 'events', 'fs', 'http', 'https', 'module', 'net', 'os', 'path', 'punycode', 'querystring', 'readline', 'repl', 'stream', 'string_decoder', 'sys', 'timers', 'tls', 'tty', 'url', 'util', 'vm', 'zlib'];
  if (semver.lt(version, '6.0.0')) coreModules.push('freelist');
  if (semver.gte(version, '1.0.0')) coreModules.push('v8');
  if (semver.gte(version, '1.1.0')) coreModules.push('process');
  if (semver.gte(version, '8.0.0')) coreModules.push('inspector');
  if (semver.gte(version, '8.1.0')) coreModules.push('async_hooks');
  if (semver.gte(version, '8.4.0')) coreModules.push('http2');
  if (semver.gte(version, '8.5.0')) coreModules.push('perf_hooks');
  if (semver.gte(version, '10.0.0')) coreModules.push('trace_events');

  if (semver.gte(version, '10.5.0') && (experimental || semver.gte(version, '12.0.0'))) {
    coreModules.push('worker_threads');
  }

  if (semver.gte(version, '12.16.0') && experimental) {
    coreModules.push('wasi');
  }

  return coreModules;
};

const reader = {
  read
};

function read(jsonPath) {
  return find(_path().dirname(jsonPath));
}

function find(dir) {
  try {
    const string = _fs().default.readFileSync(_path().toNamespacedPath(_path().join(dir, 'package.json')), 'utf8');

    return {
      string
    };
  } catch (error) {
    if (error.code === 'ENOENT') {
      const parent = _path().dirname(dir);

      if (dir !== parent) return find(parent);
      return {
        string: undefined
      };
    }

    throw error;
  }
}

const isWindows = process.platform === 'win32';
const own$1 = {}.hasOwnProperty;
const codes = {};
const messages = new Map();
const nodeInternalPrefix = '__node_internal_';
let userStackTraceLimit;
codes.ERR_INVALID_MODULE_SPECIFIER = createError('ERR_INVALID_MODULE_SPECIFIER', (request, reason, base = undefined) => {
  return `Invalid module "${request}" ${reason}${base ? ` imported from ${base}` : ''}`;
}, TypeError);
codes.ERR_INVALID_PACKAGE_CONFIG = createError('ERR_INVALID_PACKAGE_CONFIG', (path, base, message) => {
  return `Invalid package config ${path}${base ? ` while importing ${base}` : ''}${message ? `. ${message}` : ''}`;
}, Error);
codes.ERR_INVALID_PACKAGE_TARGET = createError('ERR_INVALID_PACKAGE_TARGET', (pkgPath, key, target, isImport = false, base = undefined) => {
  const relError = typeof target === 'string' && !isImport && target.length > 0 && !target.startsWith('./');

  if (key === '.') {
    _assert()(isImport === false);

    return `Invalid "exports" main target ${JSON.stringify(target)} defined ` + `in the package config ${pkgPath}package.json${base ? ` imported from ${base}` : ''}${relError ? '; targets must start with "./"' : ''}`;
  }

  return `Invalid "${isImport ? 'imports' : 'exports'}" target ${JSON.stringify(target)} defined for '${key}' in the package config ${pkgPath}package.json${base ? ` imported from ${base}` : ''}${relError ? '; targets must start with "./"' : ''}`;
}, Error);
codes.ERR_MODULE_NOT_FOUND = createError('ERR_MODULE_NOT_FOUND', (path, base, type = 'package') => {
  return `Cannot find ${type} '${path}' imported from ${base}`;
}, Error);
codes.ERR_PACKAGE_IMPORT_NOT_DEFINED = createError('ERR_PACKAGE_IMPORT_NOT_DEFINED', (specifier, packagePath, base) => {
  return `Package import specifier "${specifier}" is not defined${packagePath ? ` in package ${packagePath}package.json` : ''} imported from ${base}`;
}, TypeError);
codes.ERR_PACKAGE_PATH_NOT_EXPORTED = createError('ERR_PACKAGE_PATH_NOT_EXPORTED', (pkgPath, subpath, base = undefined) => {
  if (subpath === '.') return `No "exports" main defined in ${pkgPath}package.json${base ? ` imported from ${base}` : ''}`;
  return `Package subpath '${subpath}' is not defined by "exports" in ${pkgPath}package.json${base ? ` imported from ${base}` : ''}`;
}, Error);
codes.ERR_UNSUPPORTED_DIR_IMPORT = createError('ERR_UNSUPPORTED_DIR_IMPORT', "Directory import '%s' is not supported " + 'resolving ES modules imported from %s', Error);
codes.ERR_UNKNOWN_FILE_EXTENSION = createError('ERR_UNKNOWN_FILE_EXTENSION', 'Unknown file extension "%s" for %s', TypeError);
codes.ERR_INVALID_ARG_VALUE = createError('ERR_INVALID_ARG_VALUE', (name, value, reason = 'is invalid') => {
  let inspected = (0, _util().inspect)(value);

  if (inspected.length > 128) {
    inspected = `${inspected.slice(0, 128)}...`;
  }

  const type = name.includes('.') ? 'property' : 'argument';
  return `The ${type} '${name}' ${reason}. Received ${inspected}`;
}, TypeError);
codes.ERR_UNSUPPORTED_ESM_URL_SCHEME = createError('ERR_UNSUPPORTED_ESM_URL_SCHEME', url => {
  let message = 'Only file and data URLs are supported by the default ESM loader';

  if (isWindows && url.protocol.length === 2) {
    message += '. On Windows, absolute paths must be valid file:// URLs';
  }

  message += `. Received protocol '${url.protocol}'`;
  return message;
}, Error);

function createError(sym, value, def) {
  messages.set(sym, value);
  return makeNodeErrorWithCode(def, sym);
}

function makeNodeErrorWithCode(Base, key) {
  return NodeError;

  function NodeError(...args) {
    const limit = Error.stackTraceLimit;
    if (isErrorStackTraceLimitWritable()) Error.stackTraceLimit = 0;
    const error = new Base();
    if (isErrorStackTraceLimitWritable()) Error.stackTraceLimit = limit;
    const message = getMessage(key, args, error);
    Object.defineProperty(error, 'message', {
      value: message,
      enumerable: false,
      writable: true,
      configurable: true
    });
    Object.defineProperty(error, 'toString', {
      value() {
        return `${this.name} [${key}]: ${this.message}`;
      },

      enumerable: false,
      writable: true,
      configurable: true
    });
    addCodeToName(error, Base.name, key);
    error.code = key;
    return error;
  }
}

const addCodeToName = hideStackFrames(function (error, name, code) {
  error = captureLargerStackTrace(error);
  error.name = `${name} [${code}]`;
  error.stack;

  if (name === 'SystemError') {
    Object.defineProperty(error, 'name', {
      value: name,
      enumerable: false,
      writable: true,
      configurable: true
    });
  } else {
    delete error.name;
  }
});

function isErrorStackTraceLimitWritable() {
  const desc = Object.getOwnPropertyDescriptor(Error, 'stackTraceLimit');

  if (desc === undefined) {
    return Object.isExtensible(Error);
  }

  return own$1.call(desc, 'writable') ? desc.writable : desc.set !== undefined;
}

function hideStackFrames(fn) {
  const hidden = nodeInternalPrefix + fn.name;
  Object.defineProperty(fn, 'name', {
    value: hidden
  });
  return fn;
}

const captureLargerStackTrace = hideStackFrames(function (error) {
  const stackTraceLimitIsWritable = isErrorStackTraceLimitWritable();

  if (stackTraceLimitIsWritable) {
    userStackTraceLimit = Error.stackTraceLimit;
    Error.stackTraceLimit = Number.POSITIVE_INFINITY;
  }

  Error.captureStackTrace(error);
  if (stackTraceLimitIsWritable) Error.stackTraceLimit = userStackTraceLimit;
  return error;
});

function getMessage(key, args, self) {
  const message = messages.get(key);

  if (typeof message === 'function') {
    _assert()(message.length <= args.length, `Code: ${key}; The provided arguments length (${args.length}) does not ` + `match the required ones (${message.length}).`);

    return Reflect.apply(message, self, args);
  }

  const expectedLength = (message.match(/%[dfijoOs]/g) || []).length;

  _assert()(expectedLength === args.length, `Code: ${key}; The provided arguments length (${args.length}) does not ` + `match the required ones (${expectedLength}).`);

  if (args.length === 0) return message;
  args.unshift(message);
  return Reflect.apply(_util().format, null, args);
}

const {
  ERR_UNKNOWN_FILE_EXTENSION
} = codes;
const extensionFormatMap = {
  __proto__: null,
  '.cjs': 'commonjs',
  '.js': 'module',
  '.mjs': 'module'
};

function defaultGetFormat(url) {
  if (url.startsWith('node:')) {
    return {
      format: 'builtin'
    };
  }

  const parsed = new (_url().URL)(url);

  if (parsed.protocol === 'data:') {
    const {
      1: mime
    } = /^([^/]+\/[^;,]+)[^,]*?(;base64)?,/.exec(parsed.pathname) || [null, null];
    const format = mime === 'text/javascript' ? 'module' : null;
    return {
      format
    };
  }

  if (parsed.protocol === 'file:') {
    const ext = _path().extname(parsed.pathname);

    let format;

    if (ext === '.js') {
      format = getPackageType(parsed.href) === 'module' ? 'module' : 'commonjs';
    } else {
      format = extensionFormatMap[ext];
    }

    if (!format) {
      throw new ERR_UNKNOWN_FILE_EXTENSION(ext, (0, _url().fileURLToPath)(url));
    }

    return {
      format: format || null
    };
  }

  return {
    format: null
  };
}

const listOfBuiltins = builtins();
const {
  ERR_INVALID_MODULE_SPECIFIER,
  ERR_INVALID_PACKAGE_CONFIG,
  ERR_INVALID_PACKAGE_TARGET,
  ERR_MODULE_NOT_FOUND,
  ERR_PACKAGE_IMPORT_NOT_DEFINED,
  ERR_PACKAGE_PATH_NOT_EXPORTED,
  ERR_UNSUPPORTED_DIR_IMPORT,
  ERR_UNSUPPORTED_ESM_URL_SCHEME,
  ERR_INVALID_ARG_VALUE
} = codes;
const own = {}.hasOwnProperty;
const DEFAULT_CONDITIONS = Object.freeze(['node', 'import']);
const DEFAULT_CONDITIONS_SET = new Set(DEFAULT_CONDITIONS);
const invalidSegmentRegEx = /(^|\\|\/)(\.\.?|node_modules)(\\|\/|$)/;
const patternRegEx = /\*/g;
const encodedSepRegEx = /%2f|%2c/i;
const emittedPackageWarnings = new Set();
const packageJsonCache = new Map();

function emitFolderMapDeprecation(match, pjsonUrl, isExports, base) {
  const pjsonPath = (0, _url().fileURLToPath)(pjsonUrl);
  if (emittedPackageWarnings.has(pjsonPath + '|' + match)) return;
  emittedPackageWarnings.add(pjsonPath + '|' + match);
  process.emitWarning(`Use of deprecated folder mapping "${match}" in the ${isExports ? '"exports"' : '"imports"'} field module resolution of the package at ${pjsonPath}${base ? ` imported from ${(0, _url().fileURLToPath)(base)}` : ''}.\n` + `Update this package.json to use a subpath pattern like "${match}*".`, 'DeprecationWarning', 'DEP0148');
}

function emitLegacyIndexDeprecation(url, packageJsonUrl, base, main) {
  const {
    format
  } = defaultGetFormat(url.href);
  if (format !== 'module') return;
  const path = (0, _url().fileURLToPath)(url.href);
  const pkgPath = (0, _url().fileURLToPath)(new (_url().URL)('.', packageJsonUrl));
  const basePath = (0, _url().fileURLToPath)(base);
  if (main) process.emitWarning(`Package ${pkgPath} has a "main" field set to ${JSON.stringify(main)}, ` + `excluding the full filename and extension to the resolved file at "${path.slice(pkgPath.length)}", imported from ${basePath}.\n Automatic extension resolution of the "main" field is` + 'deprecated for ES modules.', 'DeprecationWarning', 'DEP0151');else process.emitWarning(`No "main" or "exports" field defined in the package.json for ${pkgPath} resolving the main entry point "${path.slice(pkgPath.length)}", imported from ${basePath}.\nDefault "index" lookups for the main are deprecated for ES modules.`, 'DeprecationWarning', 'DEP0151');
}

function getConditionsSet(conditions) {
  if (conditions !== undefined && conditions !== DEFAULT_CONDITIONS) {
    if (!Array.isArray(conditions)) {
      throw new ERR_INVALID_ARG_VALUE('conditions', conditions, 'expected an array');
    }

    return new Set(conditions);
  }

  return DEFAULT_CONDITIONS_SET;
}

function tryStatSync(path) {
  try {
    return (0, _fs().statSync)(path);
  } catch (_unused) {
    return new (_fs().Stats)();
  }
}

function getPackageConfig(path, specifier, base) {
  const existing = packageJsonCache.get(path);

  if (existing !== undefined) {
    return existing;
  }

  const source = reader.read(path).string;

  if (source === undefined) {
    const packageConfig = {
      pjsonPath: path,
      exists: false,
      main: undefined,
      name: undefined,
      type: 'none',
      exports: undefined,
      imports: undefined
    };
    packageJsonCache.set(path, packageConfig);
    return packageConfig;
  }

  let packageJson;

  try {
    packageJson = JSON.parse(source);
  } catch (error) {
    throw new ERR_INVALID_PACKAGE_CONFIG(path, (base ? `"${specifier}" from ` : '') + (0, _url().fileURLToPath)(base || specifier), error.message);
  }

  const {
    exports,
    imports,
    main,
    name,
    type
  } = packageJson;
  const packageConfig = {
    pjsonPath: path,
    exists: true,
    main: typeof main === 'string' ? main : undefined,
    name: typeof name === 'string' ? name : undefined,
    type: type === 'module' || type === 'commonjs' ? type : 'none',
    exports,
    imports: imports && typeof imports === 'object' ? imports : undefined
  };
  packageJsonCache.set(path, packageConfig);
  return packageConfig;
}

function getPackageScopeConfig(resolved) {
  let packageJsonUrl = new (_url().URL)('./package.json', resolved);

  while (true) {
    const packageJsonPath = packageJsonUrl.pathname;
    if (packageJsonPath.endsWith('node_modules/package.json')) break;
    const packageConfig = getPackageConfig((0, _url().fileURLToPath)(packageJsonUrl), resolved);
    if (packageConfig.exists) return packageConfig;
    const lastPackageJsonUrl = packageJsonUrl;
    packageJsonUrl = new (_url().URL)('../package.json', packageJsonUrl);
    if (packageJsonUrl.pathname === lastPackageJsonUrl.pathname) break;
  }

  const packageJsonPath = (0, _url().fileURLToPath)(packageJsonUrl);
  const packageConfig = {
    pjsonPath: packageJsonPath,
    exists: false,
    main: undefined,
    name: undefined,
    type: 'none',
    exports: undefined,
    imports: undefined
  };
  packageJsonCache.set(packageJsonPath, packageConfig);
  return packageConfig;
}

function fileExists(url) {
  return tryStatSync((0, _url().fileURLToPath)(url)).isFile();
}

function legacyMainResolve(packageJsonUrl, packageConfig, base) {
  let guess;

  if (packageConfig.main !== undefined) {
    guess = new (_url().URL)(`./${packageConfig.main}`, packageJsonUrl);
    if (fileExists(guess)) return guess;
    const tries = [`./${packageConfig.main}.js`, `./${packageConfig.main}.json`, `./${packageConfig.main}.node`, `./${packageConfig.main}/index.js`, `./${packageConfig.main}/index.json`, `./${packageConfig.main}/index.node`];
    let i = -1;

    while (++i < tries.length) {
      guess = new (_url().URL)(tries[i], packageJsonUrl);
      if (fileExists(guess)) break;
      guess = undefined;
    }

    if (guess) {
      emitLegacyIndexDeprecation(guess, packageJsonUrl, base, packageConfig.main);
      return guess;
    }
  }

  const tries = ['./index.js', './index.json', './index.node'];
  let i = -1;

  while (++i < tries.length) {
    guess = new (_url().URL)(tries[i], packageJsonUrl);
    if (fileExists(guess)) break;
    guess = undefined;
  }

  if (guess) {
    emitLegacyIndexDeprecation(guess, packageJsonUrl, base, packageConfig.main);
    return guess;
  }

  throw new ERR_MODULE_NOT_FOUND((0, _url().fileURLToPath)(new (_url().URL)('.', packageJsonUrl)), (0, _url().fileURLToPath)(base));
}

function finalizeResolution(resolved, base) {
  if (encodedSepRegEx.test(resolved.pathname)) throw new ERR_INVALID_MODULE_SPECIFIER(resolved.pathname, 'must not include encoded "/" or "\\" characters', (0, _url().fileURLToPath)(base));
  const path = (0, _url().fileURLToPath)(resolved);
  const stats = tryStatSync(path.endsWith('/') ? path.slice(-1) : path);

  if (stats.isDirectory()) {
    const error = new ERR_UNSUPPORTED_DIR_IMPORT(path, (0, _url().fileURLToPath)(base));
    error.url = String(resolved);
    throw error;
  }

  if (!stats.isFile()) {
    throw new ERR_MODULE_NOT_FOUND(path || resolved.pathname, base && (0, _url().fileURLToPath)(base), 'module');
  }

  return resolved;
}

function throwImportNotDefined(specifier, packageJsonUrl, base) {
  throw new ERR_PACKAGE_IMPORT_NOT_DEFINED(specifier, packageJsonUrl && (0, _url().fileURLToPath)(new (_url().URL)('.', packageJsonUrl)), (0, _url().fileURLToPath)(base));
}

function throwExportsNotFound(subpath, packageJsonUrl, base) {
  throw new ERR_PACKAGE_PATH_NOT_EXPORTED((0, _url().fileURLToPath)(new (_url().URL)('.', packageJsonUrl)), subpath, base && (0, _url().fileURLToPath)(base));
}

function throwInvalidSubpath(subpath, packageJsonUrl, internal, base) {
  const reason = `request is not a valid subpath for the "${internal ? 'imports' : 'exports'}" resolution of ${(0, _url().fileURLToPath)(packageJsonUrl)}`;
  throw new ERR_INVALID_MODULE_SPECIFIER(subpath, reason, base && (0, _url().fileURLToPath)(base));
}

function throwInvalidPackageTarget(subpath, target, packageJsonUrl, internal, base) {
  target = typeof target === 'object' && target !== null ? JSON.stringify(target, null, '') : `${target}`;
  throw new ERR_INVALID_PACKAGE_TARGET((0, _url().fileURLToPath)(new (_url().URL)('.', packageJsonUrl)), subpath, target, internal, base && (0, _url().fileURLToPath)(base));
}

function resolvePackageTargetString(target, subpath, match, packageJsonUrl, base, pattern, internal, conditions) {
  if (subpath !== '' && !pattern && target[target.length - 1] !== '/') throwInvalidPackageTarget(match, target, packageJsonUrl, internal, base);

  if (!target.startsWith('./')) {
    if (internal && !target.startsWith('../') && !target.startsWith('/')) {
      let isURL = false;

      try {
        new (_url().URL)(target);
        isURL = true;
      } catch (_unused2) {}

      if (!isURL) {
        const exportTarget = pattern ? target.replace(patternRegEx, subpath) : target + subpath;
        return packageResolve(exportTarget, packageJsonUrl, conditions);
      }
    }

    throwInvalidPackageTarget(match, target, packageJsonUrl, internal, base);
  }

  if (invalidSegmentRegEx.test(target.slice(2))) throwInvalidPackageTarget(match, target, packageJsonUrl, internal, base);
  const resolved = new (_url().URL)(target, packageJsonUrl);
  const resolvedPath = resolved.pathname;
  const packagePath = new (_url().URL)('.', packageJsonUrl).pathname;
  if (!resolvedPath.startsWith(packagePath)) throwInvalidPackageTarget(match, target, packageJsonUrl, internal, base);
  if (subpath === '') return resolved;
  if (invalidSegmentRegEx.test(subpath)) throwInvalidSubpath(match + subpath, packageJsonUrl, internal, base);
  if (pattern) return new (_url().URL)(resolved.href.replace(patternRegEx, subpath));
  return new (_url().URL)(subpath, resolved);
}

function isArrayIndex(key) {
  const keyNumber = Number(key);
  if (`${keyNumber}` !== key) return false;
  return keyNumber >= 0 && keyNumber < 0xffffffff;
}

function resolvePackageTarget(packageJsonUrl, target, subpath, packageSubpath, base, pattern, internal, conditions) {
  if (typeof target === 'string') {
    return resolvePackageTargetString(target, subpath, packageSubpath, packageJsonUrl, base, pattern, internal, conditions);
  }

  if (Array.isArray(target)) {
    const targetList = target;
    if (targetList.length === 0) return null;
    let lastException;
    let i = -1;

    while (++i < targetList.length) {
      const targetItem = targetList[i];
      let resolved;

      try {
        resolved = resolvePackageTarget(packageJsonUrl, targetItem, subpath, packageSubpath, base, pattern, internal, conditions);
      } catch (error) {
        lastException = error;
        if (error.code === 'ERR_INVALID_PACKAGE_TARGET') continue;
        throw error;
      }

      if (resolved === undefined) continue;

      if (resolved === null) {
        lastException = null;
        continue;
      }

      return resolved;
    }

    if (lastException === undefined || lastException === null) {
      return lastException;
    }

    throw lastException;
  }

  if (typeof target === 'object' && target !== null) {
    const keys = Object.getOwnPropertyNames(target);
    let i = -1;

    while (++i < keys.length) {
      const key = keys[i];

      if (isArrayIndex(key)) {
        throw new ERR_INVALID_PACKAGE_CONFIG((0, _url().fileURLToPath)(packageJsonUrl), base, '"exports" cannot contain numeric property keys.');
      }
    }

    i = -1;

    while (++i < keys.length) {
      const key = keys[i];

      if (key === 'default' || conditions && conditions.has(key)) {
        const conditionalTarget = target[key];
        const resolved = resolvePackageTarget(packageJsonUrl, conditionalTarget, subpath, packageSubpath, base, pattern, internal, conditions);
        if (resolved === undefined) continue;
        return resolved;
      }
    }

    return undefined;
  }

  if (target === null) {
    return null;
  }

  throwInvalidPackageTarget(packageSubpath, target, packageJsonUrl, internal, base);
}

function isConditionalExportsMainSugar(exports, packageJsonUrl, base) {
  if (typeof exports === 'string' || Array.isArray(exports)) return true;
  if (typeof exports !== 'object' || exports === null) return false;
  const keys = Object.getOwnPropertyNames(exports);
  let isConditionalSugar = false;
  let i = 0;
  let j = -1;

  while (++j < keys.length) {
    const key = keys[j];
    const curIsConditionalSugar = key === '' || key[0] !== '.';

    if (i++ === 0) {
      isConditionalSugar = curIsConditionalSugar;
    } else if (isConditionalSugar !== curIsConditionalSugar) {
      throw new ERR_INVALID_PACKAGE_CONFIG((0, _url().fileURLToPath)(packageJsonUrl), base, '"exports" cannot contain some keys starting with \'.\' and some not.' + ' The exports object must either be an object of package subpath keys' + ' or an object of main entry condition name keys only.');
    }
  }

  return isConditionalSugar;
}

function packageExportsResolve(packageJsonUrl, packageSubpath, packageConfig, base, conditions) {
  let exports = packageConfig.exports;
  if (isConditionalExportsMainSugar(exports, packageJsonUrl, base)) exports = {
    '.': exports
  };

  if (own.call(exports, packageSubpath)) {
    const target = exports[packageSubpath];
    const resolved = resolvePackageTarget(packageJsonUrl, target, '', packageSubpath, base, false, false, conditions);
    if (resolved === null || resolved === undefined) throwExportsNotFound(packageSubpath, packageJsonUrl, base);
    return {
      resolved,
      exact: true
    };
  }

  let bestMatch = '';
  const keys = Object.getOwnPropertyNames(exports);
  let i = -1;

  while (++i < keys.length) {
    const key = keys[i];

    if (key[key.length - 1] === '*' && packageSubpath.startsWith(key.slice(0, -1)) && packageSubpath.length >= key.length && key.length > bestMatch.length) {
      bestMatch = key;
    } else if (key[key.length - 1] === '/' && packageSubpath.startsWith(key) && key.length > bestMatch.length) {
      bestMatch = key;
    }
  }

  if (bestMatch) {
    const target = exports[bestMatch];
    const pattern = bestMatch[bestMatch.length - 1] === '*';
    const subpath = packageSubpath.slice(bestMatch.length - (pattern ? 1 : 0));
    const resolved = resolvePackageTarget(packageJsonUrl, target, subpath, bestMatch, base, pattern, false, conditions);
    if (resolved === null || resolved === undefined) throwExportsNotFound(packageSubpath, packageJsonUrl, base);
    if (!pattern) emitFolderMapDeprecation(bestMatch, packageJsonUrl, true, base);
    return {
      resolved,
      exact: pattern
    };
  }

  throwExportsNotFound(packageSubpath, packageJsonUrl, base);
}

function packageImportsResolve(name, base, conditions) {
  if (name === '#' || name.startsWith('#/')) {
    const reason = 'is not a valid internal imports specifier name';
    throw new ERR_INVALID_MODULE_SPECIFIER(name, reason, (0, _url().fileURLToPath)(base));
  }

  let packageJsonUrl;
  const packageConfig = getPackageScopeConfig(base);

  if (packageConfig.exists) {
    packageJsonUrl = (0, _url().pathToFileURL)(packageConfig.pjsonPath);
    const imports = packageConfig.imports;

    if (imports) {
      if (own.call(imports, name)) {
        const resolved = resolvePackageTarget(packageJsonUrl, imports[name], '', name, base, false, true, conditions);
        if (resolved !== null) return {
          resolved,
          exact: true
        };
      } else {
        let bestMatch = '';
        const keys = Object.getOwnPropertyNames(imports);
        let i = -1;

        while (++i < keys.length) {
          const key = keys[i];

          if (key[key.length - 1] === '*' && name.startsWith(key.slice(0, -1)) && name.length >= key.length && key.length > bestMatch.length) {
            bestMatch = key;
          } else if (key[key.length - 1] === '/' && name.startsWith(key) && key.length > bestMatch.length) {
            bestMatch = key;
          }
        }

        if (bestMatch) {
          const target = imports[bestMatch];
          const pattern = bestMatch[bestMatch.length - 1] === '*';
          const subpath = name.slice(bestMatch.length - (pattern ? 1 : 0));
          const resolved = resolvePackageTarget(packageJsonUrl, target, subpath, bestMatch, base, pattern, true, conditions);

          if (resolved !== null) {
            if (!pattern) emitFolderMapDeprecation(bestMatch, packageJsonUrl, false, base);
            return {
              resolved,
              exact: pattern
            };
          }
        }
      }
    }
  }

  throwImportNotDefined(name, packageJsonUrl, base);
}

function getPackageType(url) {
  const packageConfig = getPackageScopeConfig(url);
  return packageConfig.type;
}

function parsePackageName(specifier, base) {
  let separatorIndex = specifier.indexOf('/');
  let validPackageName = true;
  let isScoped = false;

  if (specifier[0] === '@') {
    isScoped = true;

    if (separatorIndex === -1 || specifier.length === 0) {
      validPackageName = false;
    } else {
      separatorIndex = specifier.indexOf('/', separatorIndex + 1);
    }
  }

  const packageName = separatorIndex === -1 ? specifier : specifier.slice(0, separatorIndex);
  let i = -1;

  while (++i < packageName.length) {
    if (packageName[i] === '%' || packageName[i] === '\\') {
      validPackageName = false;
      break;
    }
  }

  if (!validPackageName) {
    throw new ERR_INVALID_MODULE_SPECIFIER(specifier, 'is not a valid package name', (0, _url().fileURLToPath)(base));
  }

  const packageSubpath = '.' + (separatorIndex === -1 ? '' : specifier.slice(separatorIndex));
  return {
    packageName,
    packageSubpath,
    isScoped
  };
}

function packageResolve(specifier, base, conditions) {
  const {
    packageName,
    packageSubpath,
    isScoped
  } = parsePackageName(specifier, base);
  const packageConfig = getPackageScopeConfig(base);

  if (packageConfig.exists) {
    const packageJsonUrl = (0, _url().pathToFileURL)(packageConfig.pjsonPath);

    if (packageConfig.name === packageName && packageConfig.exports !== undefined && packageConfig.exports !== null) {
      return packageExportsResolve(packageJsonUrl, packageSubpath, packageConfig, base, conditions).resolved;
    }
  }

  let packageJsonUrl = new (_url().URL)('./node_modules/' + packageName + '/package.json', base);
  let packageJsonPath = (0, _url().fileURLToPath)(packageJsonUrl);
  let lastPath;

  do {
    const stat = tryStatSync(packageJsonPath.slice(0, -13));

    if (!stat.isDirectory()) {
      lastPath = packageJsonPath;
      packageJsonUrl = new (_url().URL)((isScoped ? '../../../../node_modules/' : '../../../node_modules/') + packageName + '/package.json', packageJsonUrl);
      packageJsonPath = (0, _url().fileURLToPath)(packageJsonUrl);
      continue;
    }

    const packageConfig = getPackageConfig(packageJsonPath, specifier, base);
    if (packageConfig.exports !== undefined && packageConfig.exports !== null) return packageExportsResolve(packageJsonUrl, packageSubpath, packageConfig, base, conditions).resolved;
    if (packageSubpath === '.') return legacyMainResolve(packageJsonUrl, packageConfig, base);
    return new (_url().URL)(packageSubpath, packageJsonUrl);
  } while (packageJsonPath.length !== lastPath.length);

  throw new ERR_MODULE_NOT_FOUND(packageName, (0, _url().fileURLToPath)(base));
}

function isRelativeSpecifier(specifier) {
  if (specifier[0] === '.') {
    if (specifier.length === 1 || specifier[1] === '/') return true;

    if (specifier[1] === '.' && (specifier.length === 2 || specifier[2] === '/')) {
      return true;
    }
  }

  return false;
}

function shouldBeTreatedAsRelativeOrAbsolutePath(specifier) {
  if (specifier === '') return false;
  if (specifier[0] === '/') return true;
  return isRelativeSpecifier(specifier);
}

function moduleResolve(specifier, base, conditions) {
  let resolved;

  if (shouldBeTreatedAsRelativeOrAbsolutePath(specifier)) {
    resolved = new (_url().URL)(specifier, base);
  } else if (specifier[0] === '#') {
    ({
      resolved
    } = packageImportsResolve(specifier, base, conditions));
  } else {
    try {
      resolved = new (_url().URL)(specifier);
    } catch (_unused3) {
      resolved = packageResolve(specifier, base, conditions);
    }
  }

  return finalizeResolution(resolved, base);
}

function defaultResolve(specifier, context = {}) {
  const {
    parentURL
  } = context;
  let parsed;

  try {
    parsed = new (_url().URL)(specifier);

    if (parsed.protocol === 'data:') {
      return {
        url: specifier
      };
    }
  } catch (_unused4) {}

  if (parsed && parsed.protocol === 'node:') return {
    url: specifier
  };
  if (parsed && parsed.protocol !== 'file:' && parsed.protocol !== 'data:') throw new ERR_UNSUPPORTED_ESM_URL_SCHEME(parsed);

  if (listOfBuiltins.includes(specifier)) {
    return {
      url: 'node:' + specifier
    };
  }

  if (parentURL.startsWith('data:')) {
    new (_url().URL)(specifier, parentURL);
  }

  const conditions = getConditionsSet(context.conditions);
  let url = moduleResolve(specifier, new (_url().URL)(parentURL), conditions);
  const urlPath = (0, _url().fileURLToPath)(url);
  const real = (0, _fs().realpathSync)(urlPath);
  const old = url;
  url = (0, _url().pathToFileURL)(real + (urlPath.endsWith(_path().sep) ? '/' : ''));
  url.search = old.search;
  url.hash = old.hash;
  return {
    url: `${url}`
  };
}

function resolve(_x, _x2) {
  return _resolve.apply(this, arguments);
}

function _resolve() {
  _resolve = _asyncToGenerator(function* (specifier, parent) {
    if (!parent) {
      throw new Error('Please pass `parent`: `import-meta-resolve` cannot ponyfill that');
    }

    try {
      return defaultResolve(specifier, {
        parentURL: parent
      }).url;
    } catch (error) {
      return error.code === 'ERR_UNSUPPORTED_DIR_IMPORT' ? error.url : Promise.reject(error);
    }
  });
  return _resolve.apply(this, arguments);
}
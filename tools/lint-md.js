'use strict';

function _interopDefault (ex) { return (ex && (typeof ex === 'object') && 'default' in ex) ? ex['default'] : ex; }

var util = _interopDefault(require('util'));
var os = _interopDefault(require('os'));
var tty = _interopDefault(require('tty'));
var path = _interopDefault(require('path'));
var module$1 = _interopDefault(require('module'));
var fs = _interopDefault(require('fs'));
var assert = _interopDefault(require('assert'));
var events = _interopDefault(require('events'));
var stream = _interopDefault(require('stream'));

var vfileStatistics = statistics;

/* Get stats for a file, list of files, or list of messages. */
function statistics(files) {
  var result = {true: 0, false: 0, null: 0};

  count(files);

  return {
    fatal: result.true,
    nonfatal: result.false + result.null,
    warn: result.false,
    info: result.null,
    total: result.true + result.false + result.null
  }

  function count(value) {
    if (value) {
      if (value[0] && value[0].messages) {
        /* Multiple vfiles */
        countInAll(value);
      } else {
        /* One vfile / messages */
        countAll(value.messages || value);
      }
    }
  }

  function countInAll(files) {
    var length = files.length;
    var index = -1;

    while (++index < length) {
      count(files[index].messages);
    }
  }

  function countAll(messages) {
    var length = messages.length;
    var index = -1;
    var fatal;

    while (++index < length) {
      fatal = messages[index].fatal;
      result[fatal === null || fatal === undefined ? null : Boolean(fatal)]++;
    }
  }
}

var slice = [].slice;

var wrap_1 = wrap;

/* Wrap `fn`.  Can be sync or async; return a promise,
 * receive a completion handler, return new values and
 * errors. */
function wrap(fn, callback) {
  var invoked;

  return wrapped

  function wrapped() {
    var params = slice.call(arguments, 0);
    var callback = fn.length > params.length;
    var result;

    if (callback) {
      params.push(done);
    }

    try {
      result = fn.apply(null, params);
    } catch (err) {
      /* Well, this is quite the pickle.  `fn` received
       * a callback and invoked it (thus continuing the
       * pipeline), but later also threw an error.
       * We’re not about to restart the pipeline again,
       * so the only thing left to do is to throw the
       * thing instead. */
      if (callback && invoked) {
        throw err
      }

      return done(err)
    }

    if (!callback) {
      if (result && typeof result.then === 'function') {
        result.then(then, done);
      } else if (result instanceof Error) {
        done(result);
      } else {
        then(result);
      }
    }
  }

  /* Invoke `next`, only once. */
  function done() {
    if (!invoked) {
      invoked = true;

      callback.apply(null, arguments);
    }
  }

  /* Invoke `done` with one value.
   * Tracks if an error is passed, too. */
  function then(value) {
    done(null, value);
  }
}

var trough_1 = trough;

trough.wrap = wrap_1;

var slice$1 = [].slice;

/* Create new middleware. */
function trough() {
  var fns = [];
  var middleware = {};

  middleware.run = run;
  middleware.use = use;

  return middleware

  /* Run `fns`.  Last argument must be
   * a completion handler. */
  function run() {
    var index = -1;
    var input = slice$1.call(arguments, 0, -1);
    var done = arguments[arguments.length - 1];

    if (typeof done !== 'function') {
      throw new Error('Expected function as last argument, not ' + done)
    }

    next.apply(null, [null].concat(input));

    /* Run the next `fn`, if any. */
    function next(err) {
      var fn = fns[++index];
      var params = slice$1.call(arguments, 0);
      var values = params.slice(1);
      var length = input.length;
      var pos = -1;

      if (err) {
        done(err);
        return
      }

      /* Copy non-nully input into values. */
      while (++pos < length) {
        if (values[pos] === null || values[pos] === undefined) {
          values[pos] = input[pos];
        }
      }

      input = values;

      /* Next or done. */
      if (fn) {
        wrap_1(fn, next).apply(null, input);
      } else {
        done.apply(null, [null].concat(input));
      }
    }
  }

  /* Add `fn` to the list. */
  function use(fn) {
    if (typeof fn !== 'function') {
      throw new Error('Expected `fn` to be a function, not ' + fn)
    }

    fns.push(fn);

    return middleware
  }
}

function commonjsRequire () {
	throw new Error('Dynamic requires are not currently supported by rollup-plugin-commonjs');
}

function unwrapExports (x) {
	return x && x.__esModule && Object.prototype.hasOwnProperty.call(x, 'default') ? x['default'] : x;
}

function createCommonjsModule(fn, module) {
	return module = { exports: {} }, fn(module, module.exports), module.exports;
}

function isNothing(subject) {
  return (typeof subject === 'undefined') || (subject === null);
}


function isObject(subject) {
  return (typeof subject === 'object') && (subject !== null);
}


function toArray(sequence) {
  if (Array.isArray(sequence)) return sequence;
  else if (isNothing(sequence)) return [];

  return [ sequence ];
}


function extend(target, source) {
  var index, length, key, sourceKeys;

  if (source) {
    sourceKeys = Object.keys(source);

    for (index = 0, length = sourceKeys.length; index < length; index += 1) {
      key = sourceKeys[index];
      target[key] = source[key];
    }
  }

  return target;
}


function repeat(string, count) {
  var result = '', cycle;

  for (cycle = 0; cycle < count; cycle += 1) {
    result += string;
  }

  return result;
}


function isNegativeZero(number) {
  return (number === 0) && (Number.NEGATIVE_INFINITY === 1 / number);
}


var isNothing_1      = isNothing;
var isObject_1       = isObject;
var toArray_1        = toArray;
var repeat_1         = repeat;
var isNegativeZero_1 = isNegativeZero;
var extend_1         = extend;

var common = {
	isNothing: isNothing_1,
	isObject: isObject_1,
	toArray: toArray_1,
	repeat: repeat_1,
	isNegativeZero: isNegativeZero_1,
	extend: extend_1
};

// YAML error class. http://stackoverflow.com/questions/8458984
//
function YAMLException(reason, mark) {
  // Super constructor
  Error.call(this);

  this.name = 'YAMLException';
  this.reason = reason;
  this.mark = mark;
  this.message = (this.reason || '(unknown reason)') + (this.mark ? ' ' + this.mark.toString() : '');

  // Include stack trace in error object
  if (Error.captureStackTrace) {
    // Chrome and NodeJS
    Error.captureStackTrace(this, this.constructor);
  } else {
    // FF, IE 10+ and Safari 6+. Fallback for others
    this.stack = (new Error()).stack || '';
  }
}


// Inherit from Error
YAMLException.prototype = Object.create(Error.prototype);
YAMLException.prototype.constructor = YAMLException;


YAMLException.prototype.toString = function toString(compact) {
  var result = this.name + ': ';

  result += this.reason || '(unknown reason)';

  if (!compact && this.mark) {
    result += ' ' + this.mark.toString();
  }

  return result;
};


var exception = YAMLException;

function Mark(name, buffer, position, line, column) {
  this.name     = name;
  this.buffer   = buffer;
  this.position = position;
  this.line     = line;
  this.column   = column;
}


Mark.prototype.getSnippet = function getSnippet(indent, maxLength) {
  var head, start, tail, end, snippet;

  if (!this.buffer) return null;

  indent = indent || 4;
  maxLength = maxLength || 75;

  head = '';
  start = this.position;

  while (start > 0 && '\x00\r\n\x85\u2028\u2029'.indexOf(this.buffer.charAt(start - 1)) === -1) {
    start -= 1;
    if (this.position - start > (maxLength / 2 - 1)) {
      head = ' ... ';
      start += 5;
      break;
    }
  }

  tail = '';
  end = this.position;

  while (end < this.buffer.length && '\x00\r\n\x85\u2028\u2029'.indexOf(this.buffer.charAt(end)) === -1) {
    end += 1;
    if (end - this.position > (maxLength / 2 - 1)) {
      tail = ' ... ';
      end -= 5;
      break;
    }
  }

  snippet = this.buffer.slice(start, end);

  return common.repeat(' ', indent) + head + snippet + tail + '\n' +
         common.repeat(' ', indent + this.position - start + head.length) + '^';
};


Mark.prototype.toString = function toString(compact) {
  var snippet, where = '';

  if (this.name) {
    where += 'in "' + this.name + '" ';
  }

  where += 'at line ' + (this.line + 1) + ', column ' + (this.column + 1);

  if (!compact) {
    snippet = this.getSnippet();

    if (snippet) {
      where += ':\n' + snippet;
    }
  }

  return where;
};


var mark = Mark;

var TYPE_CONSTRUCTOR_OPTIONS = [
  'kind',
  'resolve',
  'construct',
  'instanceOf',
  'predicate',
  'represent',
  'defaultStyle',
  'styleAliases'
];

var YAML_NODE_KINDS = [
  'scalar',
  'sequence',
  'mapping'
];

function compileStyleAliases(map) {
  var result = {};

  if (map !== null) {
    Object.keys(map).forEach(function (style) {
      map[style].forEach(function (alias) {
        result[String(alias)] = style;
      });
    });
  }

  return result;
}

function Type(tag, options) {
  options = options || {};

  Object.keys(options).forEach(function (name) {
    if (TYPE_CONSTRUCTOR_OPTIONS.indexOf(name) === -1) {
      throw new exception('Unknown option "' + name + '" is met in definition of "' + tag + '" YAML type.');
    }
  });

  // TODO: Add tag format check.
  this.tag          = tag;
  this.kind         = options['kind']         || null;
  this.resolve      = options['resolve']      || function () { return true; };
  this.construct    = options['construct']    || function (data) { return data; };
  this.instanceOf   = options['instanceOf']   || null;
  this.predicate    = options['predicate']    || null;
  this.represent    = options['represent']    || null;
  this.defaultStyle = options['defaultStyle'] || null;
  this.styleAliases = compileStyleAliases(options['styleAliases'] || null);

  if (YAML_NODE_KINDS.indexOf(this.kind) === -1) {
    throw new exception('Unknown kind "' + this.kind + '" is specified for "' + tag + '" YAML type.');
  }
}

var type = Type;

/*eslint-disable max-len*/






function compileList(schema, name, result) {
  var exclude = [];

  schema.include.forEach(function (includedSchema) {
    result = compileList(includedSchema, name, result);
  });

  schema[name].forEach(function (currentType) {
    result.forEach(function (previousType, previousIndex) {
      if (previousType.tag === currentType.tag && previousType.kind === currentType.kind) {
        exclude.push(previousIndex);
      }
    });

    result.push(currentType);
  });

  return result.filter(function (type$$1, index) {
    return exclude.indexOf(index) === -1;
  });
}


function compileMap(/* lists... */) {
  var result = {
        scalar: {},
        sequence: {},
        mapping: {},
        fallback: {}
      }, index, length;

  function collectType(type$$1) {
    result[type$$1.kind][type$$1.tag] = result['fallback'][type$$1.tag] = type$$1;
  }

  for (index = 0, length = arguments.length; index < length; index += 1) {
    arguments[index].forEach(collectType);
  }
  return result;
}


function Schema(definition) {
  this.include  = definition.include  || [];
  this.implicit = definition.implicit || [];
  this.explicit = definition.explicit || [];

  this.implicit.forEach(function (type$$1) {
    if (type$$1.loadKind && type$$1.loadKind !== 'scalar') {
      throw new exception('There is a non-scalar type in the implicit list of a schema. Implicit resolving of such types is not supported.');
    }
  });

  this.compiledImplicit = compileList(this, 'implicit', []);
  this.compiledExplicit = compileList(this, 'explicit', []);
  this.compiledTypeMap  = compileMap(this.compiledImplicit, this.compiledExplicit);
}


Schema.DEFAULT = null;


Schema.create = function createSchema() {
  var schemas, types;

  switch (arguments.length) {
    case 1:
      schemas = Schema.DEFAULT;
      types = arguments[0];
      break;

    case 2:
      schemas = arguments[0];
      types = arguments[1];
      break;

    default:
      throw new exception('Wrong number of arguments for Schema.create function');
  }

  schemas = common.toArray(schemas);
  types = common.toArray(types);

  if (!schemas.every(function (schema) { return schema instanceof Schema; })) {
    throw new exception('Specified list of super schemas (or a single Schema object) contains a non-Schema object.');
  }

  if (!types.every(function (type$$1) { return type$$1 instanceof type; })) {
    throw new exception('Specified list of YAML types (or a single Type object) contains a non-Type object.');
  }

  return new Schema({
    include: schemas,
    explicit: types
  });
};


var schema = Schema;

var str = new type('tag:yaml.org,2002:str', {
  kind: 'scalar',
  construct: function (data) { return data !== null ? data : ''; }
});

var seq = new type('tag:yaml.org,2002:seq', {
  kind: 'sequence',
  construct: function (data) { return data !== null ? data : []; }
});

var map = new type('tag:yaml.org,2002:map', {
  kind: 'mapping',
  construct: function (data) { return data !== null ? data : {}; }
});

var failsafe = new schema({
  explicit: [
    str,
    seq,
    map
  ]
});

function resolveYamlNull(data) {
  if (data === null) return true;

  var max = data.length;

  return (max === 1 && data === '~') ||
         (max === 4 && (data === 'null' || data === 'Null' || data === 'NULL'));
}

function constructYamlNull() {
  return null;
}

function isNull(object) {
  return object === null;
}

var _null = new type('tag:yaml.org,2002:null', {
  kind: 'scalar',
  resolve: resolveYamlNull,
  construct: constructYamlNull,
  predicate: isNull,
  represent: {
    canonical: function () { return '~';    },
    lowercase: function () { return 'null'; },
    uppercase: function () { return 'NULL'; },
    camelcase: function () { return 'Null'; }
  },
  defaultStyle: 'lowercase'
});

function resolveYamlBoolean(data) {
  if (data === null) return false;

  var max = data.length;

  return (max === 4 && (data === 'true' || data === 'True' || data === 'TRUE')) ||
         (max === 5 && (data === 'false' || data === 'False' || data === 'FALSE'));
}

function constructYamlBoolean(data) {
  return data === 'true' ||
         data === 'True' ||
         data === 'TRUE';
}

function isBoolean(object) {
  return Object.prototype.toString.call(object) === '[object Boolean]';
}

var bool = new type('tag:yaml.org,2002:bool', {
  kind: 'scalar',
  resolve: resolveYamlBoolean,
  construct: constructYamlBoolean,
  predicate: isBoolean,
  represent: {
    lowercase: function (object) { return object ? 'true' : 'false'; },
    uppercase: function (object) { return object ? 'TRUE' : 'FALSE'; },
    camelcase: function (object) { return object ? 'True' : 'False'; }
  },
  defaultStyle: 'lowercase'
});

function isHexCode(c) {
  return ((0x30/* 0 */ <= c) && (c <= 0x39/* 9 */)) ||
         ((0x41/* A */ <= c) && (c <= 0x46/* F */)) ||
         ((0x61/* a */ <= c) && (c <= 0x66/* f */));
}

function isOctCode(c) {
  return ((0x30/* 0 */ <= c) && (c <= 0x37/* 7 */));
}

function isDecCode(c) {
  return ((0x30/* 0 */ <= c) && (c <= 0x39/* 9 */));
}

function resolveYamlInteger(data) {
  if (data === null) return false;

  var max = data.length,
      index = 0,
      hasDigits = false,
      ch;

  if (!max) return false;

  ch = data[index];

  // sign
  if (ch === '-' || ch === '+') {
    ch = data[++index];
  }

  if (ch === '0') {
    // 0
    if (index + 1 === max) return true;
    ch = data[++index];

    // base 2, base 8, base 16

    if (ch === 'b') {
      // base 2
      index++;

      for (; index < max; index++) {
        ch = data[index];
        if (ch === '_') continue;
        if (ch !== '0' && ch !== '1') return false;
        hasDigits = true;
      }
      return hasDigits && ch !== '_';
    }


    if (ch === 'x') {
      // base 16
      index++;

      for (; index < max; index++) {
        ch = data[index];
        if (ch === '_') continue;
        if (!isHexCode(data.charCodeAt(index))) return false;
        hasDigits = true;
      }
      return hasDigits && ch !== '_';
    }

    // base 8
    for (; index < max; index++) {
      ch = data[index];
      if (ch === '_') continue;
      if (!isOctCode(data.charCodeAt(index))) return false;
      hasDigits = true;
    }
    return hasDigits && ch !== '_';
  }

  // base 10 (except 0) or base 60

  // value should not start with `_`;
  if (ch === '_') return false;

  for (; index < max; index++) {
    ch = data[index];
    if (ch === '_') continue;
    if (ch === ':') break;
    if (!isDecCode(data.charCodeAt(index))) {
      return false;
    }
    hasDigits = true;
  }

  // Should have digits and should not end with `_`
  if (!hasDigits || ch === '_') return false;

  // if !base60 - done;
  if (ch !== ':') return true;

  // base60 almost not used, no needs to optimize
  return /^(:[0-5]?[0-9])+$/.test(data.slice(index));
}

function constructYamlInteger(data) {
  var value = data, sign = 1, ch, base, digits = [];

  if (value.indexOf('_') !== -1) {
    value = value.replace(/_/g, '');
  }

  ch = value[0];

  if (ch === '-' || ch === '+') {
    if (ch === '-') sign = -1;
    value = value.slice(1);
    ch = value[0];
  }

  if (value === '0') return 0;

  if (ch === '0') {
    if (value[1] === 'b') return sign * parseInt(value.slice(2), 2);
    if (value[1] === 'x') return sign * parseInt(value, 16);
    return sign * parseInt(value, 8);
  }

  if (value.indexOf(':') !== -1) {
    value.split(':').forEach(function (v) {
      digits.unshift(parseInt(v, 10));
    });

    value = 0;
    base = 1;

    digits.forEach(function (d) {
      value += (d * base);
      base *= 60;
    });

    return sign * value;

  }

  return sign * parseInt(value, 10);
}

function isInteger(object) {
  return (Object.prototype.toString.call(object)) === '[object Number]' &&
         (object % 1 === 0 && !common.isNegativeZero(object));
}

var int_1 = new type('tag:yaml.org,2002:int', {
  kind: 'scalar',
  resolve: resolveYamlInteger,
  construct: constructYamlInteger,
  predicate: isInteger,
  represent: {
    binary:      function (obj) { return obj >= 0 ? '0b' + obj.toString(2) : '-0b' + obj.toString(2).slice(1); },
    octal:       function (obj) { return obj >= 0 ? '0'  + obj.toString(8) : '-0'  + obj.toString(8).slice(1); },
    decimal:     function (obj) { return obj.toString(10); },
    /* eslint-disable max-len */
    hexadecimal: function (obj) { return obj >= 0 ? '0x' + obj.toString(16).toUpperCase() :  '-0x' + obj.toString(16).toUpperCase().slice(1); }
  },
  defaultStyle: 'decimal',
  styleAliases: {
    binary:      [ 2,  'bin' ],
    octal:       [ 8,  'oct' ],
    decimal:     [ 10, 'dec' ],
    hexadecimal: [ 16, 'hex' ]
  }
});

var YAML_FLOAT_PATTERN = new RegExp(
  // 2.5e4, 2.5 and integers
  '^(?:[-+]?(?:0|[1-9][0-9_]*)(?:\\.[0-9_]*)?(?:[eE][-+]?[0-9]+)?' +
  // .2e4, .2
  // special case, seems not from spec
  '|\\.[0-9_]+(?:[eE][-+]?[0-9]+)?' +
  // 20:59
  '|[-+]?[0-9][0-9_]*(?::[0-5]?[0-9])+\\.[0-9_]*' +
  // .inf
  '|[-+]?\\.(?:inf|Inf|INF)' +
  // .nan
  '|\\.(?:nan|NaN|NAN))$');

function resolveYamlFloat(data) {
  if (data === null) return false;

  if (!YAML_FLOAT_PATTERN.test(data) ||
      // Quick hack to not allow integers end with `_`
      // Probably should update regexp & check speed
      data[data.length - 1] === '_') {
    return false;
  }

  return true;
}

function constructYamlFloat(data) {
  var value, sign, base, digits;

  value  = data.replace(/_/g, '').toLowerCase();
  sign   = value[0] === '-' ? -1 : 1;
  digits = [];

  if ('+-'.indexOf(value[0]) >= 0) {
    value = value.slice(1);
  }

  if (value === '.inf') {
    return (sign === 1) ? Number.POSITIVE_INFINITY : Number.NEGATIVE_INFINITY;

  } else if (value === '.nan') {
    return NaN;

  } else if (value.indexOf(':') >= 0) {
    value.split(':').forEach(function (v) {
      digits.unshift(parseFloat(v, 10));
    });

    value = 0.0;
    base = 1;

    digits.forEach(function (d) {
      value += d * base;
      base *= 60;
    });

    return sign * value;

  }
  return sign * parseFloat(value, 10);
}


var SCIENTIFIC_WITHOUT_DOT = /^[-+]?[0-9]+e/;

function representYamlFloat(object, style) {
  var res;

  if (isNaN(object)) {
    switch (style) {
      case 'lowercase': return '.nan';
      case 'uppercase': return '.NAN';
      case 'camelcase': return '.NaN';
    }
  } else if (Number.POSITIVE_INFINITY === object) {
    switch (style) {
      case 'lowercase': return '.inf';
      case 'uppercase': return '.INF';
      case 'camelcase': return '.Inf';
    }
  } else if (Number.NEGATIVE_INFINITY === object) {
    switch (style) {
      case 'lowercase': return '-.inf';
      case 'uppercase': return '-.INF';
      case 'camelcase': return '-.Inf';
    }
  } else if (common.isNegativeZero(object)) {
    return '-0.0';
  }

  res = object.toString(10);

  // JS stringifier can build scientific format without dots: 5e-100,
  // while YAML requires dot: 5.e-100. Fix it with simple hack

  return SCIENTIFIC_WITHOUT_DOT.test(res) ? res.replace('e', '.e') : res;
}

function isFloat(object) {
  return (Object.prototype.toString.call(object) === '[object Number]') &&
         (object % 1 !== 0 || common.isNegativeZero(object));
}

var float_1 = new type('tag:yaml.org,2002:float', {
  kind: 'scalar',
  resolve: resolveYamlFloat,
  construct: constructYamlFloat,
  predicate: isFloat,
  represent: representYamlFloat,
  defaultStyle: 'lowercase'
});

var json = new schema({
  include: [
    failsafe
  ],
  implicit: [
    _null,
    bool,
    int_1,
    float_1
  ]
});

var core = new schema({
  include: [
    json
  ]
});

var YAML_DATE_REGEXP = new RegExp(
  '^([0-9][0-9][0-9][0-9])'          + // [1] year
  '-([0-9][0-9])'                    + // [2] month
  '-([0-9][0-9])$');                   // [3] day

var YAML_TIMESTAMP_REGEXP = new RegExp(
  '^([0-9][0-9][0-9][0-9])'          + // [1] year
  '-([0-9][0-9]?)'                   + // [2] month
  '-([0-9][0-9]?)'                   + // [3] day
  '(?:[Tt]|[ \\t]+)'                 + // ...
  '([0-9][0-9]?)'                    + // [4] hour
  ':([0-9][0-9])'                    + // [5] minute
  ':([0-9][0-9])'                    + // [6] second
  '(?:\\.([0-9]*))?'                 + // [7] fraction
  '(?:[ \\t]*(Z|([-+])([0-9][0-9]?)' + // [8] tz [9] tz_sign [10] tz_hour
  '(?::([0-9][0-9]))?))?$');           // [11] tz_minute

function resolveYamlTimestamp(data) {
  if (data === null) return false;
  if (YAML_DATE_REGEXP.exec(data) !== null) return true;
  if (YAML_TIMESTAMP_REGEXP.exec(data) !== null) return true;
  return false;
}

function constructYamlTimestamp(data) {
  var match, year, month, day, hour, minute, second, fraction = 0,
      delta = null, tz_hour, tz_minute, date;

  match = YAML_DATE_REGEXP.exec(data);
  if (match === null) match = YAML_TIMESTAMP_REGEXP.exec(data);

  if (match === null) throw new Error('Date resolve error');

  // match: [1] year [2] month [3] day

  year = +(match[1]);
  month = +(match[2]) - 1; // JS month starts with 0
  day = +(match[3]);

  if (!match[4]) { // no hour
    return new Date(Date.UTC(year, month, day));
  }

  // match: [4] hour [5] minute [6] second [7] fraction

  hour = +(match[4]);
  minute = +(match[5]);
  second = +(match[6]);

  if (match[7]) {
    fraction = match[7].slice(0, 3);
    while (fraction.length < 3) { // milli-seconds
      fraction += '0';
    }
    fraction = +fraction;
  }

  // match: [8] tz [9] tz_sign [10] tz_hour [11] tz_minute

  if (match[9]) {
    tz_hour = +(match[10]);
    tz_minute = +(match[11] || 0);
    delta = (tz_hour * 60 + tz_minute) * 60000; // delta in mili-seconds
    if (match[9] === '-') delta = -delta;
  }

  date = new Date(Date.UTC(year, month, day, hour, minute, second, fraction));

  if (delta) date.setTime(date.getTime() - delta);

  return date;
}

function representYamlTimestamp(object /*, style*/) {
  return object.toISOString();
}

var timestamp = new type('tag:yaml.org,2002:timestamp', {
  kind: 'scalar',
  resolve: resolveYamlTimestamp,
  construct: constructYamlTimestamp,
  instanceOf: Date,
  represent: representYamlTimestamp
});

function resolveYamlMerge(data) {
  return data === '<<' || data === null;
}

var merge = new type('tag:yaml.org,2002:merge', {
  kind: 'scalar',
  resolve: resolveYamlMerge
});

/*eslint-disable no-bitwise*/

var NodeBuffer;

try {
  // A trick for browserified version, to not include `Buffer` shim
  var _require = commonjsRequire;
  NodeBuffer = _require('buffer').Buffer;
} catch (__) {}




// [ 64, 65, 66 ] -> [ padding, CR, LF ]
var BASE64_MAP = 'ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/=\n\r';


function resolveYamlBinary(data) {
  if (data === null) return false;

  var code, idx, bitlen = 0, max = data.length, map = BASE64_MAP;

  // Convert one by one.
  for (idx = 0; idx < max; idx++) {
    code = map.indexOf(data.charAt(idx));

    // Skip CR/LF
    if (code > 64) continue;

    // Fail on illegal characters
    if (code < 0) return false;

    bitlen += 6;
  }

  // If there are any bits left, source was corrupted
  return (bitlen % 8) === 0;
}

function constructYamlBinary(data) {
  var idx, tailbits,
      input = data.replace(/[\r\n=]/g, ''), // remove CR/LF & padding to simplify scan
      max = input.length,
      map = BASE64_MAP,
      bits = 0,
      result = [];

  // Collect by 6*4 bits (3 bytes)

  for (idx = 0; idx < max; idx++) {
    if ((idx % 4 === 0) && idx) {
      result.push((bits >> 16) & 0xFF);
      result.push((bits >> 8) & 0xFF);
      result.push(bits & 0xFF);
    }

    bits = (bits << 6) | map.indexOf(input.charAt(idx));
  }

  // Dump tail

  tailbits = (max % 4) * 6;

  if (tailbits === 0) {
    result.push((bits >> 16) & 0xFF);
    result.push((bits >> 8) & 0xFF);
    result.push(bits & 0xFF);
  } else if (tailbits === 18) {
    result.push((bits >> 10) & 0xFF);
    result.push((bits >> 2) & 0xFF);
  } else if (tailbits === 12) {
    result.push((bits >> 4) & 0xFF);
  }

  // Wrap into Buffer for NodeJS and leave Array for browser
  if (NodeBuffer) {
    // Support node 6.+ Buffer API when available
    return NodeBuffer.from ? NodeBuffer.from(result) : new NodeBuffer(result);
  }

  return result;
}

function representYamlBinary(object /*, style*/) {
  var result = '', bits = 0, idx, tail,
      max = object.length,
      map = BASE64_MAP;

  // Convert every three bytes to 4 ASCII characters.

  for (idx = 0; idx < max; idx++) {
    if ((idx % 3 === 0) && idx) {
      result += map[(bits >> 18) & 0x3F];
      result += map[(bits >> 12) & 0x3F];
      result += map[(bits >> 6) & 0x3F];
      result += map[bits & 0x3F];
    }

    bits = (bits << 8) + object[idx];
  }

  // Dump tail

  tail = max % 3;

  if (tail === 0) {
    result += map[(bits >> 18) & 0x3F];
    result += map[(bits >> 12) & 0x3F];
    result += map[(bits >> 6) & 0x3F];
    result += map[bits & 0x3F];
  } else if (tail === 2) {
    result += map[(bits >> 10) & 0x3F];
    result += map[(bits >> 4) & 0x3F];
    result += map[(bits << 2) & 0x3F];
    result += map[64];
  } else if (tail === 1) {
    result += map[(bits >> 2) & 0x3F];
    result += map[(bits << 4) & 0x3F];
    result += map[64];
    result += map[64];
  }

  return result;
}

function isBinary(object) {
  return NodeBuffer && NodeBuffer.isBuffer(object);
}

var binary = new type('tag:yaml.org,2002:binary', {
  kind: 'scalar',
  resolve: resolveYamlBinary,
  construct: constructYamlBinary,
  predicate: isBinary,
  represent: representYamlBinary
});

var _hasOwnProperty = Object.prototype.hasOwnProperty;
var _toString       = Object.prototype.toString;

function resolveYamlOmap(data) {
  if (data === null) return true;

  var objectKeys = [], index, length, pair, pairKey, pairHasKey,
      object = data;

  for (index = 0, length = object.length; index < length; index += 1) {
    pair = object[index];
    pairHasKey = false;

    if (_toString.call(pair) !== '[object Object]') return false;

    for (pairKey in pair) {
      if (_hasOwnProperty.call(pair, pairKey)) {
        if (!pairHasKey) pairHasKey = true;
        else return false;
      }
    }

    if (!pairHasKey) return false;

    if (objectKeys.indexOf(pairKey) === -1) objectKeys.push(pairKey);
    else return false;
  }

  return true;
}

function constructYamlOmap(data) {
  return data !== null ? data : [];
}

var omap = new type('tag:yaml.org,2002:omap', {
  kind: 'sequence',
  resolve: resolveYamlOmap,
  construct: constructYamlOmap
});

var _toString$1 = Object.prototype.toString;

function resolveYamlPairs(data) {
  if (data === null) return true;

  var index, length, pair, keys, result,
      object = data;

  result = new Array(object.length);

  for (index = 0, length = object.length; index < length; index += 1) {
    pair = object[index];

    if (_toString$1.call(pair) !== '[object Object]') return false;

    keys = Object.keys(pair);

    if (keys.length !== 1) return false;

    result[index] = [ keys[0], pair[keys[0]] ];
  }

  return true;
}

function constructYamlPairs(data) {
  if (data === null) return [];

  var index, length, pair, keys, result,
      object = data;

  result = new Array(object.length);

  for (index = 0, length = object.length; index < length; index += 1) {
    pair = object[index];

    keys = Object.keys(pair);

    result[index] = [ keys[0], pair[keys[0]] ];
  }

  return result;
}

var pairs = new type('tag:yaml.org,2002:pairs', {
  kind: 'sequence',
  resolve: resolveYamlPairs,
  construct: constructYamlPairs
});

var _hasOwnProperty$1 = Object.prototype.hasOwnProperty;

function resolveYamlSet(data) {
  if (data === null) return true;

  var key, object = data;

  for (key in object) {
    if (_hasOwnProperty$1.call(object, key)) {
      if (object[key] !== null) return false;
    }
  }

  return true;
}

function constructYamlSet(data) {
  return data !== null ? data : {};
}

var set = new type('tag:yaml.org,2002:set', {
  kind: 'mapping',
  resolve: resolveYamlSet,
  construct: constructYamlSet
});

var default_safe = new schema({
  include: [
    core
  ],
  implicit: [
    timestamp,
    merge
  ],
  explicit: [
    binary,
    omap,
    pairs,
    set
  ]
});

function resolveJavascriptUndefined() {
  return true;
}

function constructJavascriptUndefined() {
  /*eslint-disable no-undefined*/
  return undefined;
}

function representJavascriptUndefined() {
  return '';
}

function isUndefined(object) {
  return typeof object === 'undefined';
}

var _undefined = new type('tag:yaml.org,2002:js/undefined', {
  kind: 'scalar',
  resolve: resolveJavascriptUndefined,
  construct: constructJavascriptUndefined,
  predicate: isUndefined,
  represent: representJavascriptUndefined
});

function resolveJavascriptRegExp(data) {
  if (data === null) return false;
  if (data.length === 0) return false;

  var regexp = data,
      tail   = /\/([gim]*)$/.exec(data),
      modifiers = '';

  // if regexp starts with '/' it can have modifiers and must be properly closed
  // `/foo/gim` - modifiers tail can be maximum 3 chars
  if (regexp[0] === '/') {
    if (tail) modifiers = tail[1];

    if (modifiers.length > 3) return false;
    // if expression starts with /, is should be properly terminated
    if (regexp[regexp.length - modifiers.length - 1] !== '/') return false;
  }

  return true;
}

function constructJavascriptRegExp(data) {
  var regexp = data,
      tail   = /\/([gim]*)$/.exec(data),
      modifiers = '';

  // `/foo/gim` - tail can be maximum 4 chars
  if (regexp[0] === '/') {
    if (tail) modifiers = tail[1];
    regexp = regexp.slice(1, regexp.length - modifiers.length - 1);
  }

  return new RegExp(regexp, modifiers);
}

function representJavascriptRegExp(object /*, style*/) {
  var result = '/' + object.source + '/';

  if (object.global) result += 'g';
  if (object.multiline) result += 'm';
  if (object.ignoreCase) result += 'i';

  return result;
}

function isRegExp(object) {
  return Object.prototype.toString.call(object) === '[object RegExp]';
}

var regexp = new type('tag:yaml.org,2002:js/regexp', {
  kind: 'scalar',
  resolve: resolveJavascriptRegExp,
  construct: constructJavascriptRegExp,
  predicate: isRegExp,
  represent: representJavascriptRegExp
});

var esprima;

// Browserified version does not have esprima
//
// 1. For node.js just require module as deps
// 2. For browser try to require mudule via external AMD system.
//    If not found - try to fallback to window.esprima. If not
//    found too - then fail to parse.
//
try {
  // workaround to exclude package from browserify list.
  var _require$1 = commonjsRequire;
  esprima = _require$1('esprima');
} catch (_) {
  /*global window */
  if (typeof window !== 'undefined') esprima = window.esprima;
}



function resolveJavascriptFunction(data) {
  if (data === null) return false;

  try {
    var source = '(' + data + ')',
        ast    = esprima.parse(source, { range: true });

    if (ast.type                    !== 'Program'             ||
        ast.body.length             !== 1                     ||
        ast.body[0].type            !== 'ExpressionStatement' ||
        (ast.body[0].expression.type !== 'ArrowFunctionExpression' &&
          ast.body[0].expression.type !== 'FunctionExpression')) {
      return false;
    }

    return true;
  } catch (err) {
    return false;
  }
}

function constructJavascriptFunction(data) {
  /*jslint evil:true*/

  var source = '(' + data + ')',
      ast    = esprima.parse(source, { range: true }),
      params = [],
      body;

  if (ast.type                    !== 'Program'             ||
      ast.body.length             !== 1                     ||
      ast.body[0].type            !== 'ExpressionStatement' ||
      (ast.body[0].expression.type !== 'ArrowFunctionExpression' &&
        ast.body[0].expression.type !== 'FunctionExpression')) {
    throw new Error('Failed to resolve function');
  }

  ast.body[0].expression.params.forEach(function (param) {
    params.push(param.name);
  });

  body = ast.body[0].expression.body.range;

  // Esprima's ranges include the first '{' and the last '}' characters on
  // function expressions. So cut them out.
  if (ast.body[0].expression.body.type === 'BlockStatement') {
    /*eslint-disable no-new-func*/
    return new Function(params, source.slice(body[0] + 1, body[1] - 1));
  }
  // ES6 arrow functions can omit the BlockStatement. In that case, just return
  // the body.
  /*eslint-disable no-new-func*/
  return new Function(params, 'return ' + source.slice(body[0], body[1]));
}

function representJavascriptFunction(object /*, style*/) {
  return object.toString();
}

function isFunction(object) {
  return Object.prototype.toString.call(object) === '[object Function]';
}

var _function = new type('tag:yaml.org,2002:js/function', {
  kind: 'scalar',
  resolve: resolveJavascriptFunction,
  construct: constructJavascriptFunction,
  predicate: isFunction,
  represent: representJavascriptFunction
});

var default_full = schema.DEFAULT = new schema({
  include: [
    default_safe
  ],
  explicit: [
    _undefined,
    regexp,
    _function
  ]
});

/*eslint-disable max-len,no-use-before-define*/








var _hasOwnProperty$2 = Object.prototype.hasOwnProperty;


var CONTEXT_FLOW_IN   = 1;
var CONTEXT_FLOW_OUT  = 2;
var CONTEXT_BLOCK_IN  = 3;
var CONTEXT_BLOCK_OUT = 4;


var CHOMPING_CLIP  = 1;
var CHOMPING_STRIP = 2;
var CHOMPING_KEEP  = 3;


var PATTERN_NON_PRINTABLE         = /[\x00-\x08\x0B\x0C\x0E-\x1F\x7F-\x84\x86-\x9F\uFFFE\uFFFF]|[\uD800-\uDBFF](?![\uDC00-\uDFFF])|(?:[^\uD800-\uDBFF]|^)[\uDC00-\uDFFF]/;
var PATTERN_NON_ASCII_LINE_BREAKS = /[\x85\u2028\u2029]/;
var PATTERN_FLOW_INDICATORS       = /[,\[\]\{\}]/;
var PATTERN_TAG_HANDLE            = /^(?:!|!!|![a-z\-]+!)$/i;
var PATTERN_TAG_URI               = /^(?:!|[^,\[\]\{\}])(?:%[0-9a-f]{2}|[0-9a-z\-#;\/\?:@&=\+\$,_\.!~\*'\(\)\[\]])*$/i;


function is_EOL(c) {
  return (c === 0x0A/* LF */) || (c === 0x0D/* CR */);
}

function is_WHITE_SPACE(c) {
  return (c === 0x09/* Tab */) || (c === 0x20/* Space */);
}

function is_WS_OR_EOL(c) {
  return (c === 0x09/* Tab */) ||
         (c === 0x20/* Space */) ||
         (c === 0x0A/* LF */) ||
         (c === 0x0D/* CR */);
}

function is_FLOW_INDICATOR(c) {
  return c === 0x2C/* , */ ||
         c === 0x5B/* [ */ ||
         c === 0x5D/* ] */ ||
         c === 0x7B/* { */ ||
         c === 0x7D/* } */;
}

function fromHexCode(c) {
  var lc;

  if ((0x30/* 0 */ <= c) && (c <= 0x39/* 9 */)) {
    return c - 0x30;
  }

  /*eslint-disable no-bitwise*/
  lc = c | 0x20;

  if ((0x61/* a */ <= lc) && (lc <= 0x66/* f */)) {
    return lc - 0x61 + 10;
  }

  return -1;
}

function escapedHexLen(c) {
  if (c === 0x78/* x */) { return 2; }
  if (c === 0x75/* u */) { return 4; }
  if (c === 0x55/* U */) { return 8; }
  return 0;
}

function fromDecimalCode(c) {
  if ((0x30/* 0 */ <= c) && (c <= 0x39/* 9 */)) {
    return c - 0x30;
  }

  return -1;
}

function simpleEscapeSequence(c) {
  /* eslint-disable indent */
  return (c === 0x30/* 0 */) ? '\x00' :
        (c === 0x61/* a */) ? '\x07' :
        (c === 0x62/* b */) ? '\x08' :
        (c === 0x74/* t */) ? '\x09' :
        (c === 0x09/* Tab */) ? '\x09' :
        (c === 0x6E/* n */) ? '\x0A' :
        (c === 0x76/* v */) ? '\x0B' :
        (c === 0x66/* f */) ? '\x0C' :
        (c === 0x72/* r */) ? '\x0D' :
        (c === 0x65/* e */) ? '\x1B' :
        (c === 0x20/* Space */) ? ' ' :
        (c === 0x22/* " */) ? '\x22' :
        (c === 0x2F/* / */) ? '/' :
        (c === 0x5C/* \ */) ? '\x5C' :
        (c === 0x4E/* N */) ? '\x85' :
        (c === 0x5F/* _ */) ? '\xA0' :
        (c === 0x4C/* L */) ? '\u2028' :
        (c === 0x50/* P */) ? '\u2029' : '';
}

function charFromCodepoint(c) {
  if (c <= 0xFFFF) {
    return String.fromCharCode(c);
  }
  // Encode UTF-16 surrogate pair
  // https://en.wikipedia.org/wiki/UTF-16#Code_points_U.2B010000_to_U.2B10FFFF
  return String.fromCharCode(
    ((c - 0x010000) >> 10) + 0xD800,
    ((c - 0x010000) & 0x03FF) + 0xDC00
  );
}

var simpleEscapeCheck = new Array(256); // integer, for fast access
var simpleEscapeMap = new Array(256);
for (var i = 0; i < 256; i++) {
  simpleEscapeCheck[i] = simpleEscapeSequence(i) ? 1 : 0;
  simpleEscapeMap[i] = simpleEscapeSequence(i);
}


function State(input, options) {
  this.input = input;

  this.filename  = options['filename']  || null;
  this.schema    = options['schema']    || default_full;
  this.onWarning = options['onWarning'] || null;
  this.legacy    = options['legacy']    || false;
  this.json      = options['json']      || false;
  this.listener  = options['listener']  || null;

  this.implicitTypes = this.schema.compiledImplicit;
  this.typeMap       = this.schema.compiledTypeMap;

  this.length     = input.length;
  this.position   = 0;
  this.line       = 0;
  this.lineStart  = 0;
  this.lineIndent = 0;

  this.documents = [];

  /*
  this.version;
  this.checkLineBreaks;
  this.tagMap;
  this.anchorMap;
  this.tag;
  this.anchor;
  this.kind;
  this.result;*/

}


function generateError(state, message) {
  return new exception(
    message,
    new mark(state.filename, state.input, state.position, state.line, (state.position - state.lineStart)));
}

function throwError(state, message) {
  throw generateError(state, message);
}

function throwWarning(state, message) {
  if (state.onWarning) {
    state.onWarning.call(null, generateError(state, message));
  }
}


var directiveHandlers = {

  YAML: function handleYamlDirective(state, name, args) {

    var match, major, minor;

    if (state.version !== null) {
      throwError(state, 'duplication of %YAML directive');
    }

    if (args.length !== 1) {
      throwError(state, 'YAML directive accepts exactly one argument');
    }

    match = /^([0-9]+)\.([0-9]+)$/.exec(args[0]);

    if (match === null) {
      throwError(state, 'ill-formed argument of the YAML directive');
    }

    major = parseInt(match[1], 10);
    minor = parseInt(match[2], 10);

    if (major !== 1) {
      throwError(state, 'unacceptable YAML version of the document');
    }

    state.version = args[0];
    state.checkLineBreaks = (minor < 2);

    if (minor !== 1 && minor !== 2) {
      throwWarning(state, 'unsupported YAML version of the document');
    }
  },

  TAG: function handleTagDirective(state, name, args) {

    var handle, prefix;

    if (args.length !== 2) {
      throwError(state, 'TAG directive accepts exactly two arguments');
    }

    handle = args[0];
    prefix = args[1];

    if (!PATTERN_TAG_HANDLE.test(handle)) {
      throwError(state, 'ill-formed tag handle (first argument) of the TAG directive');
    }

    if (_hasOwnProperty$2.call(state.tagMap, handle)) {
      throwError(state, 'there is a previously declared suffix for "' + handle + '" tag handle');
    }

    if (!PATTERN_TAG_URI.test(prefix)) {
      throwError(state, 'ill-formed tag prefix (second argument) of the TAG directive');
    }

    state.tagMap[handle] = prefix;
  }
};


function captureSegment(state, start, end, checkJson) {
  var _position, _length, _character, _result;

  if (start < end) {
    _result = state.input.slice(start, end);

    if (checkJson) {
      for (_position = 0, _length = _result.length; _position < _length; _position += 1) {
        _character = _result.charCodeAt(_position);
        if (!(_character === 0x09 ||
              (0x20 <= _character && _character <= 0x10FFFF))) {
          throwError(state, 'expected valid JSON character');
        }
      }
    } else if (PATTERN_NON_PRINTABLE.test(_result)) {
      throwError(state, 'the stream contains non-printable characters');
    }

    state.result += _result;
  }
}

function mergeMappings(state, destination, source, overridableKeys) {
  var sourceKeys, key, index, quantity;

  if (!common.isObject(source)) {
    throwError(state, 'cannot merge mappings; the provided source object is unacceptable');
  }

  sourceKeys = Object.keys(source);

  for (index = 0, quantity = sourceKeys.length; index < quantity; index += 1) {
    key = sourceKeys[index];

    if (!_hasOwnProperty$2.call(destination, key)) {
      destination[key] = source[key];
      overridableKeys[key] = true;
    }
  }
}

function storeMappingPair(state, _result, overridableKeys, keyTag, keyNode, valueNode, startLine, startPos) {
  var index, quantity;

  keyNode = String(keyNode);

  if (_result === null) {
    _result = {};
  }

  if (keyTag === 'tag:yaml.org,2002:merge') {
    if (Array.isArray(valueNode)) {
      for (index = 0, quantity = valueNode.length; index < quantity; index += 1) {
        mergeMappings(state, _result, valueNode[index], overridableKeys);
      }
    } else {
      mergeMappings(state, _result, valueNode, overridableKeys);
    }
  } else {
    if (!state.json &&
        !_hasOwnProperty$2.call(overridableKeys, keyNode) &&
        _hasOwnProperty$2.call(_result, keyNode)) {
      state.line = startLine || state.line;
      state.position = startPos || state.position;
      throwError(state, 'duplicated mapping key');
    }
    _result[keyNode] = valueNode;
    delete overridableKeys[keyNode];
  }

  return _result;
}

function readLineBreak(state) {
  var ch;

  ch = state.input.charCodeAt(state.position);

  if (ch === 0x0A/* LF */) {
    state.position++;
  } else if (ch === 0x0D/* CR */) {
    state.position++;
    if (state.input.charCodeAt(state.position) === 0x0A/* LF */) {
      state.position++;
    }
  } else {
    throwError(state, 'a line break is expected');
  }

  state.line += 1;
  state.lineStart = state.position;
}

function skipSeparationSpace(state, allowComments, checkIndent) {
  var lineBreaks = 0,
      ch = state.input.charCodeAt(state.position);

  while (ch !== 0) {
    while (is_WHITE_SPACE(ch)) {
      ch = state.input.charCodeAt(++state.position);
    }

    if (allowComments && ch === 0x23/* # */) {
      do {
        ch = state.input.charCodeAt(++state.position);
      } while (ch !== 0x0A/* LF */ && ch !== 0x0D/* CR */ && ch !== 0);
    }

    if (is_EOL(ch)) {
      readLineBreak(state);

      ch = state.input.charCodeAt(state.position);
      lineBreaks++;
      state.lineIndent = 0;

      while (ch === 0x20/* Space */) {
        state.lineIndent++;
        ch = state.input.charCodeAt(++state.position);
      }
    } else {
      break;
    }
  }

  if (checkIndent !== -1 && lineBreaks !== 0 && state.lineIndent < checkIndent) {
    throwWarning(state, 'deficient indentation');
  }

  return lineBreaks;
}

function testDocumentSeparator(state) {
  var _position = state.position,
      ch;

  ch = state.input.charCodeAt(_position);

  // Condition state.position === state.lineStart is tested
  // in parent on each call, for efficiency. No needs to test here again.
  if ((ch === 0x2D/* - */ || ch === 0x2E/* . */) &&
      ch === state.input.charCodeAt(_position + 1) &&
      ch === state.input.charCodeAt(_position + 2)) {

    _position += 3;

    ch = state.input.charCodeAt(_position);

    if (ch === 0 || is_WS_OR_EOL(ch)) {
      return true;
    }
  }

  return false;
}

function writeFoldedLines(state, count) {
  if (count === 1) {
    state.result += ' ';
  } else if (count > 1) {
    state.result += common.repeat('\n', count - 1);
  }
}


function readPlainScalar(state, nodeIndent, withinFlowCollection) {
  var preceding,
      following,
      captureStart,
      captureEnd,
      hasPendingContent,
      _line,
      _lineStart,
      _lineIndent,
      _kind = state.kind,
      _result = state.result,
      ch;

  ch = state.input.charCodeAt(state.position);

  if (is_WS_OR_EOL(ch)      ||
      is_FLOW_INDICATOR(ch) ||
      ch === 0x23/* # */    ||
      ch === 0x26/* & */    ||
      ch === 0x2A/* * */    ||
      ch === 0x21/* ! */    ||
      ch === 0x7C/* | */    ||
      ch === 0x3E/* > */    ||
      ch === 0x27/* ' */    ||
      ch === 0x22/* " */    ||
      ch === 0x25/* % */    ||
      ch === 0x40/* @ */    ||
      ch === 0x60/* ` */) {
    return false;
  }

  if (ch === 0x3F/* ? */ || ch === 0x2D/* - */) {
    following = state.input.charCodeAt(state.position + 1);

    if (is_WS_OR_EOL(following) ||
        withinFlowCollection && is_FLOW_INDICATOR(following)) {
      return false;
    }
  }

  state.kind = 'scalar';
  state.result = '';
  captureStart = captureEnd = state.position;
  hasPendingContent = false;

  while (ch !== 0) {
    if (ch === 0x3A/* : */) {
      following = state.input.charCodeAt(state.position + 1);

      if (is_WS_OR_EOL(following) ||
          withinFlowCollection && is_FLOW_INDICATOR(following)) {
        break;
      }

    } else if (ch === 0x23/* # */) {
      preceding = state.input.charCodeAt(state.position - 1);

      if (is_WS_OR_EOL(preceding)) {
        break;
      }

    } else if ((state.position === state.lineStart && testDocumentSeparator(state)) ||
               withinFlowCollection && is_FLOW_INDICATOR(ch)) {
      break;

    } else if (is_EOL(ch)) {
      _line = state.line;
      _lineStart = state.lineStart;
      _lineIndent = state.lineIndent;
      skipSeparationSpace(state, false, -1);

      if (state.lineIndent >= nodeIndent) {
        hasPendingContent = true;
        ch = state.input.charCodeAt(state.position);
        continue;
      } else {
        state.position = captureEnd;
        state.line = _line;
        state.lineStart = _lineStart;
        state.lineIndent = _lineIndent;
        break;
      }
    }

    if (hasPendingContent) {
      captureSegment(state, captureStart, captureEnd, false);
      writeFoldedLines(state, state.line - _line);
      captureStart = captureEnd = state.position;
      hasPendingContent = false;
    }

    if (!is_WHITE_SPACE(ch)) {
      captureEnd = state.position + 1;
    }

    ch = state.input.charCodeAt(++state.position);
  }

  captureSegment(state, captureStart, captureEnd, false);

  if (state.result) {
    return true;
  }

  state.kind = _kind;
  state.result = _result;
  return false;
}

function readSingleQuotedScalar(state, nodeIndent) {
  var ch,
      captureStart, captureEnd;

  ch = state.input.charCodeAt(state.position);

  if (ch !== 0x27/* ' */) {
    return false;
  }

  state.kind = 'scalar';
  state.result = '';
  state.position++;
  captureStart = captureEnd = state.position;

  while ((ch = state.input.charCodeAt(state.position)) !== 0) {
    if (ch === 0x27/* ' */) {
      captureSegment(state, captureStart, state.position, true);
      ch = state.input.charCodeAt(++state.position);

      if (ch === 0x27/* ' */) {
        captureStart = state.position;
        state.position++;
        captureEnd = state.position;
      } else {
        return true;
      }

    } else if (is_EOL(ch)) {
      captureSegment(state, captureStart, captureEnd, true);
      writeFoldedLines(state, skipSeparationSpace(state, false, nodeIndent));
      captureStart = captureEnd = state.position;

    } else if (state.position === state.lineStart && testDocumentSeparator(state)) {
      throwError(state, 'unexpected end of the document within a single quoted scalar');

    } else {
      state.position++;
      captureEnd = state.position;
    }
  }

  throwError(state, 'unexpected end of the stream within a single quoted scalar');
}

function readDoubleQuotedScalar(state, nodeIndent) {
  var captureStart,
      captureEnd,
      hexLength,
      hexResult,
      tmp,
      ch;

  ch = state.input.charCodeAt(state.position);

  if (ch !== 0x22/* " */) {
    return false;
  }

  state.kind = 'scalar';
  state.result = '';
  state.position++;
  captureStart = captureEnd = state.position;

  while ((ch = state.input.charCodeAt(state.position)) !== 0) {
    if (ch === 0x22/* " */) {
      captureSegment(state, captureStart, state.position, true);
      state.position++;
      return true;

    } else if (ch === 0x5C/* \ */) {
      captureSegment(state, captureStart, state.position, true);
      ch = state.input.charCodeAt(++state.position);

      if (is_EOL(ch)) {
        skipSeparationSpace(state, false, nodeIndent);

        // TODO: rework to inline fn with no type cast?
      } else if (ch < 256 && simpleEscapeCheck[ch]) {
        state.result += simpleEscapeMap[ch];
        state.position++;

      } else if ((tmp = escapedHexLen(ch)) > 0) {
        hexLength = tmp;
        hexResult = 0;

        for (; hexLength > 0; hexLength--) {
          ch = state.input.charCodeAt(++state.position);

          if ((tmp = fromHexCode(ch)) >= 0) {
            hexResult = (hexResult << 4) + tmp;

          } else {
            throwError(state, 'expected hexadecimal character');
          }
        }

        state.result += charFromCodepoint(hexResult);

        state.position++;

      } else {
        throwError(state, 'unknown escape sequence');
      }

      captureStart = captureEnd = state.position;

    } else if (is_EOL(ch)) {
      captureSegment(state, captureStart, captureEnd, true);
      writeFoldedLines(state, skipSeparationSpace(state, false, nodeIndent));
      captureStart = captureEnd = state.position;

    } else if (state.position === state.lineStart && testDocumentSeparator(state)) {
      throwError(state, 'unexpected end of the document within a double quoted scalar');

    } else {
      state.position++;
      captureEnd = state.position;
    }
  }

  throwError(state, 'unexpected end of the stream within a double quoted scalar');
}

function readFlowCollection(state, nodeIndent) {
  var readNext = true,
      _line,
      _tag     = state.tag,
      _result,
      _anchor  = state.anchor,
      following,
      terminator,
      isPair,
      isExplicitPair,
      isMapping,
      overridableKeys = {},
      keyNode,
      keyTag,
      valueNode,
      ch;

  ch = state.input.charCodeAt(state.position);

  if (ch === 0x5B/* [ */) {
    terminator = 0x5D;/* ] */
    isMapping = false;
    _result = [];
  } else if (ch === 0x7B/* { */) {
    terminator = 0x7D;/* } */
    isMapping = true;
    _result = {};
  } else {
    return false;
  }

  if (state.anchor !== null) {
    state.anchorMap[state.anchor] = _result;
  }

  ch = state.input.charCodeAt(++state.position);

  while (ch !== 0) {
    skipSeparationSpace(state, true, nodeIndent);

    ch = state.input.charCodeAt(state.position);

    if (ch === terminator) {
      state.position++;
      state.tag = _tag;
      state.anchor = _anchor;
      state.kind = isMapping ? 'mapping' : 'sequence';
      state.result = _result;
      return true;
    } else if (!readNext) {
      throwError(state, 'missed comma between flow collection entries');
    }

    keyTag = keyNode = valueNode = null;
    isPair = isExplicitPair = false;

    if (ch === 0x3F/* ? */) {
      following = state.input.charCodeAt(state.position + 1);

      if (is_WS_OR_EOL(following)) {
        isPair = isExplicitPair = true;
        state.position++;
        skipSeparationSpace(state, true, nodeIndent);
      }
    }

    _line = state.line;
    composeNode(state, nodeIndent, CONTEXT_FLOW_IN, false, true);
    keyTag = state.tag;
    keyNode = state.result;
    skipSeparationSpace(state, true, nodeIndent);

    ch = state.input.charCodeAt(state.position);

    if ((isExplicitPair || state.line === _line) && ch === 0x3A/* : */) {
      isPair = true;
      ch = state.input.charCodeAt(++state.position);
      skipSeparationSpace(state, true, nodeIndent);
      composeNode(state, nodeIndent, CONTEXT_FLOW_IN, false, true);
      valueNode = state.result;
    }

    if (isMapping) {
      storeMappingPair(state, _result, overridableKeys, keyTag, keyNode, valueNode);
    } else if (isPair) {
      _result.push(storeMappingPair(state, null, overridableKeys, keyTag, keyNode, valueNode));
    } else {
      _result.push(keyNode);
    }

    skipSeparationSpace(state, true, nodeIndent);

    ch = state.input.charCodeAt(state.position);

    if (ch === 0x2C/* , */) {
      readNext = true;
      ch = state.input.charCodeAt(++state.position);
    } else {
      readNext = false;
    }
  }

  throwError(state, 'unexpected end of the stream within a flow collection');
}

function readBlockScalar(state, nodeIndent) {
  var captureStart,
      folding,
      chomping       = CHOMPING_CLIP,
      didReadContent = false,
      detectedIndent = false,
      textIndent     = nodeIndent,
      emptyLines     = 0,
      atMoreIndented = false,
      tmp,
      ch;

  ch = state.input.charCodeAt(state.position);

  if (ch === 0x7C/* | */) {
    folding = false;
  } else if (ch === 0x3E/* > */) {
    folding = true;
  } else {
    return false;
  }

  state.kind = 'scalar';
  state.result = '';

  while (ch !== 0) {
    ch = state.input.charCodeAt(++state.position);

    if (ch === 0x2B/* + */ || ch === 0x2D/* - */) {
      if (CHOMPING_CLIP === chomping) {
        chomping = (ch === 0x2B/* + */) ? CHOMPING_KEEP : CHOMPING_STRIP;
      } else {
        throwError(state, 'repeat of a chomping mode identifier');
      }

    } else if ((tmp = fromDecimalCode(ch)) >= 0) {
      if (tmp === 0) {
        throwError(state, 'bad explicit indentation width of a block scalar; it cannot be less than one');
      } else if (!detectedIndent) {
        textIndent = nodeIndent + tmp - 1;
        detectedIndent = true;
      } else {
        throwError(state, 'repeat of an indentation width identifier');
      }

    } else {
      break;
    }
  }

  if (is_WHITE_SPACE(ch)) {
    do { ch = state.input.charCodeAt(++state.position); }
    while (is_WHITE_SPACE(ch));

    if (ch === 0x23/* # */) {
      do { ch = state.input.charCodeAt(++state.position); }
      while (!is_EOL(ch) && (ch !== 0));
    }
  }

  while (ch !== 0) {
    readLineBreak(state);
    state.lineIndent = 0;

    ch = state.input.charCodeAt(state.position);

    while ((!detectedIndent || state.lineIndent < textIndent) &&
           (ch === 0x20/* Space */)) {
      state.lineIndent++;
      ch = state.input.charCodeAt(++state.position);
    }

    if (!detectedIndent && state.lineIndent > textIndent) {
      textIndent = state.lineIndent;
    }

    if (is_EOL(ch)) {
      emptyLines++;
      continue;
    }

    // End of the scalar.
    if (state.lineIndent < textIndent) {

      // Perform the chomping.
      if (chomping === CHOMPING_KEEP) {
        state.result += common.repeat('\n', didReadContent ? 1 + emptyLines : emptyLines);
      } else if (chomping === CHOMPING_CLIP) {
        if (didReadContent) { // i.e. only if the scalar is not empty.
          state.result += '\n';
        }
      }

      // Break this `while` cycle and go to the function's epilogue.
      break;
    }

    // Folded style: use fancy rules to handle line breaks.
    if (folding) {

      // Lines starting with white space characters (more-indented lines) are not folded.
      if (is_WHITE_SPACE(ch)) {
        atMoreIndented = true;
        // except for the first content line (cf. Example 8.1)
        state.result += common.repeat('\n', didReadContent ? 1 + emptyLines : emptyLines);

      // End of more-indented block.
      } else if (atMoreIndented) {
        atMoreIndented = false;
        state.result += common.repeat('\n', emptyLines + 1);

      // Just one line break - perceive as the same line.
      } else if (emptyLines === 0) {
        if (didReadContent) { // i.e. only if we have already read some scalar content.
          state.result += ' ';
        }

      // Several line breaks - perceive as different lines.
      } else {
        state.result += common.repeat('\n', emptyLines);
      }

    // Literal style: just add exact number of line breaks between content lines.
    } else {
      // Keep all line breaks except the header line break.
      state.result += common.repeat('\n', didReadContent ? 1 + emptyLines : emptyLines);
    }

    didReadContent = true;
    detectedIndent = true;
    emptyLines = 0;
    captureStart = state.position;

    while (!is_EOL(ch) && (ch !== 0)) {
      ch = state.input.charCodeAt(++state.position);
    }

    captureSegment(state, captureStart, state.position, false);
  }

  return true;
}

function readBlockSequence(state, nodeIndent) {
  var _line,
      _tag      = state.tag,
      _anchor   = state.anchor,
      _result   = [],
      following,
      detected  = false,
      ch;

  if (state.anchor !== null) {
    state.anchorMap[state.anchor] = _result;
  }

  ch = state.input.charCodeAt(state.position);

  while (ch !== 0) {

    if (ch !== 0x2D/* - */) {
      break;
    }

    following = state.input.charCodeAt(state.position + 1);

    if (!is_WS_OR_EOL(following)) {
      break;
    }

    detected = true;
    state.position++;

    if (skipSeparationSpace(state, true, -1)) {
      if (state.lineIndent <= nodeIndent) {
        _result.push(null);
        ch = state.input.charCodeAt(state.position);
        continue;
      }
    }

    _line = state.line;
    composeNode(state, nodeIndent, CONTEXT_BLOCK_IN, false, true);
    _result.push(state.result);
    skipSeparationSpace(state, true, -1);

    ch = state.input.charCodeAt(state.position);

    if ((state.line === _line || state.lineIndent > nodeIndent) && (ch !== 0)) {
      throwError(state, 'bad indentation of a sequence entry');
    } else if (state.lineIndent < nodeIndent) {
      break;
    }
  }

  if (detected) {
    state.tag = _tag;
    state.anchor = _anchor;
    state.kind = 'sequence';
    state.result = _result;
    return true;
  }
  return false;
}

function readBlockMapping(state, nodeIndent, flowIndent) {
  var following,
      allowCompact,
      _line,
      _pos,
      _tag          = state.tag,
      _anchor       = state.anchor,
      _result       = {},
      overridableKeys = {},
      keyTag        = null,
      keyNode       = null,
      valueNode     = null,
      atExplicitKey = false,
      detected      = false,
      ch;

  if (state.anchor !== null) {
    state.anchorMap[state.anchor] = _result;
  }

  ch = state.input.charCodeAt(state.position);

  while (ch !== 0) {
    following = state.input.charCodeAt(state.position + 1);
    _line = state.line; // Save the current line.
    _pos = state.position;

    //
    // Explicit notation case. There are two separate blocks:
    // first for the key (denoted by "?") and second for the value (denoted by ":")
    //
    if ((ch === 0x3F/* ? */ || ch === 0x3A/* : */) && is_WS_OR_EOL(following)) {

      if (ch === 0x3F/* ? */) {
        if (atExplicitKey) {
          storeMappingPair(state, _result, overridableKeys, keyTag, keyNode, null);
          keyTag = keyNode = valueNode = null;
        }

        detected = true;
        atExplicitKey = true;
        allowCompact = true;

      } else if (atExplicitKey) {
        // i.e. 0x3A/* : */ === character after the explicit key.
        atExplicitKey = false;
        allowCompact = true;

      } else {
        throwError(state, 'incomplete explicit mapping pair; a key node is missed; or followed by a non-tabulated empty line');
      }

      state.position += 1;
      ch = following;

    //
    // Implicit notation case. Flow-style node as the key first, then ":", and the value.
    //
    } else if (composeNode(state, flowIndent, CONTEXT_FLOW_OUT, false, true)) {

      if (state.line === _line) {
        ch = state.input.charCodeAt(state.position);

        while (is_WHITE_SPACE(ch)) {
          ch = state.input.charCodeAt(++state.position);
        }

        if (ch === 0x3A/* : */) {
          ch = state.input.charCodeAt(++state.position);

          if (!is_WS_OR_EOL(ch)) {
            throwError(state, 'a whitespace character is expected after the key-value separator within a block mapping');
          }

          if (atExplicitKey) {
            storeMappingPair(state, _result, overridableKeys, keyTag, keyNode, null);
            keyTag = keyNode = valueNode = null;
          }

          detected = true;
          atExplicitKey = false;
          allowCompact = false;
          keyTag = state.tag;
          keyNode = state.result;

        } else if (detected) {
          throwError(state, 'can not read an implicit mapping pair; a colon is missed');

        } else {
          state.tag = _tag;
          state.anchor = _anchor;
          return true; // Keep the result of `composeNode`.
        }

      } else if (detected) {
        throwError(state, 'can not read a block mapping entry; a multiline key may not be an implicit key');

      } else {
        state.tag = _tag;
        state.anchor = _anchor;
        return true; // Keep the result of `composeNode`.
      }

    } else {
      break; // Reading is done. Go to the epilogue.
    }

    //
    // Common reading code for both explicit and implicit notations.
    //
    if (state.line === _line || state.lineIndent > nodeIndent) {
      if (composeNode(state, nodeIndent, CONTEXT_BLOCK_OUT, true, allowCompact)) {
        if (atExplicitKey) {
          keyNode = state.result;
        } else {
          valueNode = state.result;
        }
      }

      if (!atExplicitKey) {
        storeMappingPair(state, _result, overridableKeys, keyTag, keyNode, valueNode, _line, _pos);
        keyTag = keyNode = valueNode = null;
      }

      skipSeparationSpace(state, true, -1);
      ch = state.input.charCodeAt(state.position);
    }

    if (state.lineIndent > nodeIndent && (ch !== 0)) {
      throwError(state, 'bad indentation of a mapping entry');
    } else if (state.lineIndent < nodeIndent) {
      break;
    }
  }

  //
  // Epilogue.
  //

  // Special case: last mapping's node contains only the key in explicit notation.
  if (atExplicitKey) {
    storeMappingPair(state, _result, overridableKeys, keyTag, keyNode, null);
  }

  // Expose the resulting mapping.
  if (detected) {
    state.tag = _tag;
    state.anchor = _anchor;
    state.kind = 'mapping';
    state.result = _result;
  }

  return detected;
}

function readTagProperty(state) {
  var _position,
      isVerbatim = false,
      isNamed    = false,
      tagHandle,
      tagName,
      ch;

  ch = state.input.charCodeAt(state.position);

  if (ch !== 0x21/* ! */) return false;

  if (state.tag !== null) {
    throwError(state, 'duplication of a tag property');
  }

  ch = state.input.charCodeAt(++state.position);

  if (ch === 0x3C/* < */) {
    isVerbatim = true;
    ch = state.input.charCodeAt(++state.position);

  } else if (ch === 0x21/* ! */) {
    isNamed = true;
    tagHandle = '!!';
    ch = state.input.charCodeAt(++state.position);

  } else {
    tagHandle = '!';
  }

  _position = state.position;

  if (isVerbatim) {
    do { ch = state.input.charCodeAt(++state.position); }
    while (ch !== 0 && ch !== 0x3E/* > */);

    if (state.position < state.length) {
      tagName = state.input.slice(_position, state.position);
      ch = state.input.charCodeAt(++state.position);
    } else {
      throwError(state, 'unexpected end of the stream within a verbatim tag');
    }
  } else {
    while (ch !== 0 && !is_WS_OR_EOL(ch)) {

      if (ch === 0x21/* ! */) {
        if (!isNamed) {
          tagHandle = state.input.slice(_position - 1, state.position + 1);

          if (!PATTERN_TAG_HANDLE.test(tagHandle)) {
            throwError(state, 'named tag handle cannot contain such characters');
          }

          isNamed = true;
          _position = state.position + 1;
        } else {
          throwError(state, 'tag suffix cannot contain exclamation marks');
        }
      }

      ch = state.input.charCodeAt(++state.position);
    }

    tagName = state.input.slice(_position, state.position);

    if (PATTERN_FLOW_INDICATORS.test(tagName)) {
      throwError(state, 'tag suffix cannot contain flow indicator characters');
    }
  }

  if (tagName && !PATTERN_TAG_URI.test(tagName)) {
    throwError(state, 'tag name cannot contain such characters: ' + tagName);
  }

  if (isVerbatim) {
    state.tag = tagName;

  } else if (_hasOwnProperty$2.call(state.tagMap, tagHandle)) {
    state.tag = state.tagMap[tagHandle] + tagName;

  } else if (tagHandle === '!') {
    state.tag = '!' + tagName;

  } else if (tagHandle === '!!') {
    state.tag = 'tag:yaml.org,2002:' + tagName;

  } else {
    throwError(state, 'undeclared tag handle "' + tagHandle + '"');
  }

  return true;
}

function readAnchorProperty(state) {
  var _position,
      ch;

  ch = state.input.charCodeAt(state.position);

  if (ch !== 0x26/* & */) return false;

  if (state.anchor !== null) {
    throwError(state, 'duplication of an anchor property');
  }

  ch = state.input.charCodeAt(++state.position);
  _position = state.position;

  while (ch !== 0 && !is_WS_OR_EOL(ch) && !is_FLOW_INDICATOR(ch)) {
    ch = state.input.charCodeAt(++state.position);
  }

  if (state.position === _position) {
    throwError(state, 'name of an anchor node must contain at least one character');
  }

  state.anchor = state.input.slice(_position, state.position);
  return true;
}

function readAlias(state) {
  var _position, alias,
      ch;

  ch = state.input.charCodeAt(state.position);

  if (ch !== 0x2A/* * */) return false;

  ch = state.input.charCodeAt(++state.position);
  _position = state.position;

  while (ch !== 0 && !is_WS_OR_EOL(ch) && !is_FLOW_INDICATOR(ch)) {
    ch = state.input.charCodeAt(++state.position);
  }

  if (state.position === _position) {
    throwError(state, 'name of an alias node must contain at least one character');
  }

  alias = state.input.slice(_position, state.position);

  if (!state.anchorMap.hasOwnProperty(alias)) {
    throwError(state, 'unidentified alias "' + alias + '"');
  }

  state.result = state.anchorMap[alias];
  skipSeparationSpace(state, true, -1);
  return true;
}

function composeNode(state, parentIndent, nodeContext, allowToSeek, allowCompact) {
  var allowBlockStyles,
      allowBlockScalars,
      allowBlockCollections,
      indentStatus = 1, // 1: this>parent, 0: this=parent, -1: this<parent
      atNewLine  = false,
      hasContent = false,
      typeIndex,
      typeQuantity,
      type,
      flowIndent,
      blockIndent;

  if (state.listener !== null) {
    state.listener('open', state);
  }

  state.tag    = null;
  state.anchor = null;
  state.kind   = null;
  state.result = null;

  allowBlockStyles = allowBlockScalars = allowBlockCollections =
    CONTEXT_BLOCK_OUT === nodeContext ||
    CONTEXT_BLOCK_IN  === nodeContext;

  if (allowToSeek) {
    if (skipSeparationSpace(state, true, -1)) {
      atNewLine = true;

      if (state.lineIndent > parentIndent) {
        indentStatus = 1;
      } else if (state.lineIndent === parentIndent) {
        indentStatus = 0;
      } else if (state.lineIndent < parentIndent) {
        indentStatus = -1;
      }
    }
  }

  if (indentStatus === 1) {
    while (readTagProperty(state) || readAnchorProperty(state)) {
      if (skipSeparationSpace(state, true, -1)) {
        atNewLine = true;
        allowBlockCollections = allowBlockStyles;

        if (state.lineIndent > parentIndent) {
          indentStatus = 1;
        } else if (state.lineIndent === parentIndent) {
          indentStatus = 0;
        } else if (state.lineIndent < parentIndent) {
          indentStatus = -1;
        }
      } else {
        allowBlockCollections = false;
      }
    }
  }

  if (allowBlockCollections) {
    allowBlockCollections = atNewLine || allowCompact;
  }

  if (indentStatus === 1 || CONTEXT_BLOCK_OUT === nodeContext) {
    if (CONTEXT_FLOW_IN === nodeContext || CONTEXT_FLOW_OUT === nodeContext) {
      flowIndent = parentIndent;
    } else {
      flowIndent = parentIndent + 1;
    }

    blockIndent = state.position - state.lineStart;

    if (indentStatus === 1) {
      if (allowBlockCollections &&
          (readBlockSequence(state, blockIndent) ||
           readBlockMapping(state, blockIndent, flowIndent)) ||
          readFlowCollection(state, flowIndent)) {
        hasContent = true;
      } else {
        if ((allowBlockScalars && readBlockScalar(state, flowIndent)) ||
            readSingleQuotedScalar(state, flowIndent) ||
            readDoubleQuotedScalar(state, flowIndent)) {
          hasContent = true;

        } else if (readAlias(state)) {
          hasContent = true;

          if (state.tag !== null || state.anchor !== null) {
            throwError(state, 'alias node should not have any properties');
          }

        } else if (readPlainScalar(state, flowIndent, CONTEXT_FLOW_IN === nodeContext)) {
          hasContent = true;

          if (state.tag === null) {
            state.tag = '?';
          }
        }

        if (state.anchor !== null) {
          state.anchorMap[state.anchor] = state.result;
        }
      }
    } else if (indentStatus === 0) {
      // Special case: block sequences are allowed to have same indentation level as the parent.
      // http://www.yaml.org/spec/1.2/spec.html#id2799784
      hasContent = allowBlockCollections && readBlockSequence(state, blockIndent);
    }
  }

  if (state.tag !== null && state.tag !== '!') {
    if (state.tag === '?') {
      for (typeIndex = 0, typeQuantity = state.implicitTypes.length; typeIndex < typeQuantity; typeIndex += 1) {
        type = state.implicitTypes[typeIndex];

        // Implicit resolving is not allowed for non-scalar types, and '?'
        // non-specific tag is only assigned to plain scalars. So, it isn't
        // needed to check for 'kind' conformity.

        if (type.resolve(state.result)) { // `state.result` updated in resolver if matched
          state.result = type.construct(state.result);
          state.tag = type.tag;
          if (state.anchor !== null) {
            state.anchorMap[state.anchor] = state.result;
          }
          break;
        }
      }
    } else if (_hasOwnProperty$2.call(state.typeMap[state.kind || 'fallback'], state.tag)) {
      type = state.typeMap[state.kind || 'fallback'][state.tag];

      if (state.result !== null && type.kind !== state.kind) {
        throwError(state, 'unacceptable node kind for !<' + state.tag + '> tag; it should be "' + type.kind + '", not "' + state.kind + '"');
      }

      if (!type.resolve(state.result)) { // `state.result` updated in resolver if matched
        throwError(state, 'cannot resolve a node with !<' + state.tag + '> explicit tag');
      } else {
        state.result = type.construct(state.result);
        if (state.anchor !== null) {
          state.anchorMap[state.anchor] = state.result;
        }
      }
    } else {
      throwError(state, 'unknown tag !<' + state.tag + '>');
    }
  }

  if (state.listener !== null) {
    state.listener('close', state);
  }
  return state.tag !== null ||  state.anchor !== null || hasContent;
}

function readDocument(state) {
  var documentStart = state.position,
      _position,
      directiveName,
      directiveArgs,
      hasDirectives = false,
      ch;

  state.version = null;
  state.checkLineBreaks = state.legacy;
  state.tagMap = {};
  state.anchorMap = {};

  while ((ch = state.input.charCodeAt(state.position)) !== 0) {
    skipSeparationSpace(state, true, -1);

    ch = state.input.charCodeAt(state.position);

    if (state.lineIndent > 0 || ch !== 0x25/* % */) {
      break;
    }

    hasDirectives = true;
    ch = state.input.charCodeAt(++state.position);
    _position = state.position;

    while (ch !== 0 && !is_WS_OR_EOL(ch)) {
      ch = state.input.charCodeAt(++state.position);
    }

    directiveName = state.input.slice(_position, state.position);
    directiveArgs = [];

    if (directiveName.length < 1) {
      throwError(state, 'directive name must not be less than one character in length');
    }

    while (ch !== 0) {
      while (is_WHITE_SPACE(ch)) {
        ch = state.input.charCodeAt(++state.position);
      }

      if (ch === 0x23/* # */) {
        do { ch = state.input.charCodeAt(++state.position); }
        while (ch !== 0 && !is_EOL(ch));
        break;
      }

      if (is_EOL(ch)) break;

      _position = state.position;

      while (ch !== 0 && !is_WS_OR_EOL(ch)) {
        ch = state.input.charCodeAt(++state.position);
      }

      directiveArgs.push(state.input.slice(_position, state.position));
    }

    if (ch !== 0) readLineBreak(state);

    if (_hasOwnProperty$2.call(directiveHandlers, directiveName)) {
      directiveHandlers[directiveName](state, directiveName, directiveArgs);
    } else {
      throwWarning(state, 'unknown document directive "' + directiveName + '"');
    }
  }

  skipSeparationSpace(state, true, -1);

  if (state.lineIndent === 0 &&
      state.input.charCodeAt(state.position)     === 0x2D/* - */ &&
      state.input.charCodeAt(state.position + 1) === 0x2D/* - */ &&
      state.input.charCodeAt(state.position + 2) === 0x2D/* - */) {
    state.position += 3;
    skipSeparationSpace(state, true, -1);

  } else if (hasDirectives) {
    throwError(state, 'directives end mark is expected');
  }

  composeNode(state, state.lineIndent - 1, CONTEXT_BLOCK_OUT, false, true);
  skipSeparationSpace(state, true, -1);

  if (state.checkLineBreaks &&
      PATTERN_NON_ASCII_LINE_BREAKS.test(state.input.slice(documentStart, state.position))) {
    throwWarning(state, 'non-ASCII line breaks are interpreted as content');
  }

  state.documents.push(state.result);

  if (state.position === state.lineStart && testDocumentSeparator(state)) {

    if (state.input.charCodeAt(state.position) === 0x2E/* . */) {
      state.position += 3;
      skipSeparationSpace(state, true, -1);
    }
    return;
  }

  if (state.position < (state.length - 1)) {
    throwError(state, 'end of the stream or a document separator is expected');
  } else {
    return;
  }
}


function loadDocuments(input, options) {
  input = String(input);
  options = options || {};

  if (input.length !== 0) {

    // Add tailing `\n` if not exists
    if (input.charCodeAt(input.length - 1) !== 0x0A/* LF */ &&
        input.charCodeAt(input.length - 1) !== 0x0D/* CR */) {
      input += '\n';
    }

    // Strip BOM
    if (input.charCodeAt(0) === 0xFEFF) {
      input = input.slice(1);
    }
  }

  var state = new State(input, options);

  // Use 0 as string terminator. That significantly simplifies bounds check.
  state.input += '\0';

  while (state.input.charCodeAt(state.position) === 0x20/* Space */) {
    state.lineIndent += 1;
    state.position += 1;
  }

  while (state.position < (state.length - 1)) {
    readDocument(state);
  }

  return state.documents;
}


function loadAll(input, iterator, options) {
  var documents = loadDocuments(input, options), index, length;

  if (typeof iterator !== 'function') {
    return documents;
  }

  for (index = 0, length = documents.length; index < length; index += 1) {
    iterator(documents[index]);
  }
}


function load(input, options) {
  var documents = loadDocuments(input, options);

  if (documents.length === 0) {
    /*eslint-disable no-undefined*/
    return undefined;
  } else if (documents.length === 1) {
    return documents[0];
  }
  throw new exception('expected a single document in the stream, but found more');
}


function safeLoadAll(input, output, options) {
  if (typeof output === 'function') {
    loadAll(input, output, common.extend({ schema: default_safe }, options));
  } else {
    return loadAll(input, common.extend({ schema: default_safe }, options));
  }
}


function safeLoad(input, options) {
  return load(input, common.extend({ schema: default_safe }, options));
}


var loadAll_1     = loadAll;
var load_1        = load;
var safeLoadAll_1 = safeLoadAll;
var safeLoad_1    = safeLoad;

var loader = {
	loadAll: loadAll_1,
	load: load_1,
	safeLoadAll: safeLoadAll_1,
	safeLoad: safeLoad_1
};

/*eslint-disable no-use-before-define*/






var _toString$2       = Object.prototype.toString;
var _hasOwnProperty$3 = Object.prototype.hasOwnProperty;

var CHAR_TAB                  = 0x09; /* Tab */
var CHAR_LINE_FEED            = 0x0A; /* LF */
var CHAR_SPACE                = 0x20; /* Space */
var CHAR_EXCLAMATION          = 0x21; /* ! */
var CHAR_DOUBLE_QUOTE         = 0x22; /* " */
var CHAR_SHARP                = 0x23; /* # */
var CHAR_PERCENT              = 0x25; /* % */
var CHAR_AMPERSAND            = 0x26; /* & */
var CHAR_SINGLE_QUOTE         = 0x27; /* ' */
var CHAR_ASTERISK             = 0x2A; /* * */
var CHAR_COMMA                = 0x2C; /* , */
var CHAR_MINUS                = 0x2D; /* - */
var CHAR_COLON                = 0x3A; /* : */
var CHAR_GREATER_THAN         = 0x3E; /* > */
var CHAR_QUESTION             = 0x3F; /* ? */
var CHAR_COMMERCIAL_AT        = 0x40; /* @ */
var CHAR_LEFT_SQUARE_BRACKET  = 0x5B; /* [ */
var CHAR_RIGHT_SQUARE_BRACKET = 0x5D; /* ] */
var CHAR_GRAVE_ACCENT         = 0x60; /* ` */
var CHAR_LEFT_CURLY_BRACKET   = 0x7B; /* { */
var CHAR_VERTICAL_LINE        = 0x7C; /* | */
var CHAR_RIGHT_CURLY_BRACKET  = 0x7D; /* } */

var ESCAPE_SEQUENCES = {};

ESCAPE_SEQUENCES[0x00]   = '\\0';
ESCAPE_SEQUENCES[0x07]   = '\\a';
ESCAPE_SEQUENCES[0x08]   = '\\b';
ESCAPE_SEQUENCES[0x09]   = '\\t';
ESCAPE_SEQUENCES[0x0A]   = '\\n';
ESCAPE_SEQUENCES[0x0B]   = '\\v';
ESCAPE_SEQUENCES[0x0C]   = '\\f';
ESCAPE_SEQUENCES[0x0D]   = '\\r';
ESCAPE_SEQUENCES[0x1B]   = '\\e';
ESCAPE_SEQUENCES[0x22]   = '\\"';
ESCAPE_SEQUENCES[0x5C]   = '\\\\';
ESCAPE_SEQUENCES[0x85]   = '\\N';
ESCAPE_SEQUENCES[0xA0]   = '\\_';
ESCAPE_SEQUENCES[0x2028] = '\\L';
ESCAPE_SEQUENCES[0x2029] = '\\P';

var DEPRECATED_BOOLEANS_SYNTAX = [
  'y', 'Y', 'yes', 'Yes', 'YES', 'on', 'On', 'ON',
  'n', 'N', 'no', 'No', 'NO', 'off', 'Off', 'OFF'
];

function compileStyleMap(schema, map) {
  var result, keys, index, length, tag, style, type;

  if (map === null) return {};

  result = {};
  keys = Object.keys(map);

  for (index = 0, length = keys.length; index < length; index += 1) {
    tag = keys[index];
    style = String(map[tag]);

    if (tag.slice(0, 2) === '!!') {
      tag = 'tag:yaml.org,2002:' + tag.slice(2);
    }
    type = schema.compiledTypeMap['fallback'][tag];

    if (type && _hasOwnProperty$3.call(type.styleAliases, style)) {
      style = type.styleAliases[style];
    }

    result[tag] = style;
  }

  return result;
}

function encodeHex(character) {
  var string, handle, length;

  string = character.toString(16).toUpperCase();

  if (character <= 0xFF) {
    handle = 'x';
    length = 2;
  } else if (character <= 0xFFFF) {
    handle = 'u';
    length = 4;
  } else if (character <= 0xFFFFFFFF) {
    handle = 'U';
    length = 8;
  } else {
    throw new exception('code point within a string may not be greater than 0xFFFFFFFF');
  }

  return '\\' + handle + common.repeat('0', length - string.length) + string;
}

function State$1(options) {
  this.schema       = options['schema'] || default_full;
  this.indent       = Math.max(1, (options['indent'] || 2));
  this.skipInvalid  = options['skipInvalid'] || false;
  this.flowLevel    = (common.isNothing(options['flowLevel']) ? -1 : options['flowLevel']);
  this.styleMap     = compileStyleMap(this.schema, options['styles'] || null);
  this.sortKeys     = options['sortKeys'] || false;
  this.lineWidth    = options['lineWidth'] || 80;
  this.noRefs       = options['noRefs'] || false;
  this.noCompatMode = options['noCompatMode'] || false;
  this.condenseFlow = options['condenseFlow'] || false;

  this.implicitTypes = this.schema.compiledImplicit;
  this.explicitTypes = this.schema.compiledExplicit;

  this.tag = null;
  this.result = '';

  this.duplicates = [];
  this.usedDuplicates = null;
}

// Indents every line in a string. Empty lines (\n only) are not indented.
function indentString(string, spaces) {
  var ind = common.repeat(' ', spaces),
      position = 0,
      next = -1,
      result = '',
      line,
      length = string.length;

  while (position < length) {
    next = string.indexOf('\n', position);
    if (next === -1) {
      line = string.slice(position);
      position = length;
    } else {
      line = string.slice(position, next + 1);
      position = next + 1;
    }

    if (line.length && line !== '\n') result += ind;

    result += line;
  }

  return result;
}

function generateNextLine(state, level) {
  return '\n' + common.repeat(' ', state.indent * level);
}

function testImplicitResolving(state, str) {
  var index, length, type;

  for (index = 0, length = state.implicitTypes.length; index < length; index += 1) {
    type = state.implicitTypes[index];

    if (type.resolve(str)) {
      return true;
    }
  }

  return false;
}

// [33] s-white ::= s-space | s-tab
function isWhitespace(c) {
  return c === CHAR_SPACE || c === CHAR_TAB;
}

// Returns true if the character can be printed without escaping.
// From YAML 1.2: "any allowed characters known to be non-printable
// should also be escaped. [However,] This isn’t mandatory"
// Derived from nb-char - \t - #x85 - #xA0 - #x2028 - #x2029.
function isPrintable(c) {
  return  (0x00020 <= c && c <= 0x00007E)
      || ((0x000A1 <= c && c <= 0x00D7FF) && c !== 0x2028 && c !== 0x2029)
      || ((0x0E000 <= c && c <= 0x00FFFD) && c !== 0xFEFF /* BOM */)
      ||  (0x10000 <= c && c <= 0x10FFFF);
}

// Simplified test for values allowed after the first character in plain style.
function isPlainSafe(c) {
  // Uses a subset of nb-char - c-flow-indicator - ":" - "#"
  // where nb-char ::= c-printable - b-char - c-byte-order-mark.
  return isPrintable(c) && c !== 0xFEFF
    // - c-flow-indicator
    && c !== CHAR_COMMA
    && c !== CHAR_LEFT_SQUARE_BRACKET
    && c !== CHAR_RIGHT_SQUARE_BRACKET
    && c !== CHAR_LEFT_CURLY_BRACKET
    && c !== CHAR_RIGHT_CURLY_BRACKET
    // - ":" - "#"
    && c !== CHAR_COLON
    && c !== CHAR_SHARP;
}

// Simplified test for values allowed as the first character in plain style.
function isPlainSafeFirst(c) {
  // Uses a subset of ns-char - c-indicator
  // where ns-char = nb-char - s-white.
  return isPrintable(c) && c !== 0xFEFF
    && !isWhitespace(c) // - s-white
    // - (c-indicator ::=
    // “-” | “?” | “:” | “,” | “[” | “]” | “{” | “}”
    && c !== CHAR_MINUS
    && c !== CHAR_QUESTION
    && c !== CHAR_COLON
    && c !== CHAR_COMMA
    && c !== CHAR_LEFT_SQUARE_BRACKET
    && c !== CHAR_RIGHT_SQUARE_BRACKET
    && c !== CHAR_LEFT_CURLY_BRACKET
    && c !== CHAR_RIGHT_CURLY_BRACKET
    // | “#” | “&” | “*” | “!” | “|” | “>” | “'” | “"”
    && c !== CHAR_SHARP
    && c !== CHAR_AMPERSAND
    && c !== CHAR_ASTERISK
    && c !== CHAR_EXCLAMATION
    && c !== CHAR_VERTICAL_LINE
    && c !== CHAR_GREATER_THAN
    && c !== CHAR_SINGLE_QUOTE
    && c !== CHAR_DOUBLE_QUOTE
    // | “%” | “@” | “`”)
    && c !== CHAR_PERCENT
    && c !== CHAR_COMMERCIAL_AT
    && c !== CHAR_GRAVE_ACCENT;
}

// Determines whether block indentation indicator is required.
function needIndentIndicator(string) {
  var leadingSpaceRe = /^\n* /;
  return leadingSpaceRe.test(string);
}

var STYLE_PLAIN   = 1;
var STYLE_SINGLE  = 2;
var STYLE_LITERAL = 3;
var STYLE_FOLDED  = 4;
var STYLE_DOUBLE  = 5;

// Determines which scalar styles are possible and returns the preferred style.
// lineWidth = -1 => no limit.
// Pre-conditions: str.length > 0.
// Post-conditions:
//    STYLE_PLAIN or STYLE_SINGLE => no \n are in the string.
//    STYLE_LITERAL => no lines are suitable for folding (or lineWidth is -1).
//    STYLE_FOLDED => a line > lineWidth and can be folded (and lineWidth != -1).
function chooseScalarStyle(string, singleLineOnly, indentPerLevel, lineWidth, testAmbiguousType) {
  var i;
  var char;
  var hasLineBreak = false;
  var hasFoldableLine = false; // only checked if shouldTrackWidth
  var shouldTrackWidth = lineWidth !== -1;
  var previousLineBreak = -1; // count the first line correctly
  var plain = isPlainSafeFirst(string.charCodeAt(0))
          && !isWhitespace(string.charCodeAt(string.length - 1));

  if (singleLineOnly) {
    // Case: no block styles.
    // Check for disallowed characters to rule out plain and single.
    for (i = 0; i < string.length; i++) {
      char = string.charCodeAt(i);
      if (!isPrintable(char)) {
        return STYLE_DOUBLE;
      }
      plain = plain && isPlainSafe(char);
    }
  } else {
    // Case: block styles permitted.
    for (i = 0; i < string.length; i++) {
      char = string.charCodeAt(i);
      if (char === CHAR_LINE_FEED) {
        hasLineBreak = true;
        // Check if any line can be folded.
        if (shouldTrackWidth) {
          hasFoldableLine = hasFoldableLine ||
            // Foldable line = too long, and not more-indented.
            (i - previousLineBreak - 1 > lineWidth &&
             string[previousLineBreak + 1] !== ' ');
          previousLineBreak = i;
        }
      } else if (!isPrintable(char)) {
        return STYLE_DOUBLE;
      }
      plain = plain && isPlainSafe(char);
    }
    // in case the end is missing a \n
    hasFoldableLine = hasFoldableLine || (shouldTrackWidth &&
      (i - previousLineBreak - 1 > lineWidth &&
       string[previousLineBreak + 1] !== ' '));
  }
  // Although every style can represent \n without escaping, prefer block styles
  // for multiline, since they're more readable and they don't add empty lines.
  // Also prefer folding a super-long line.
  if (!hasLineBreak && !hasFoldableLine) {
    // Strings interpretable as another type have to be quoted;
    // e.g. the string 'true' vs. the boolean true.
    return plain && !testAmbiguousType(string)
      ? STYLE_PLAIN : STYLE_SINGLE;
  }
  // Edge case: block indentation indicator can only have one digit.
  if (indentPerLevel > 9 && needIndentIndicator(string)) {
    return STYLE_DOUBLE;
  }
  // At this point we know block styles are valid.
  // Prefer literal style unless we want to fold.
  return hasFoldableLine ? STYLE_FOLDED : STYLE_LITERAL;
}

// Note: line breaking/folding is implemented for only the folded style.
// NB. We drop the last trailing newline (if any) of a returned block scalar
//  since the dumper adds its own newline. This always works:
//    • No ending newline => unaffected; already using strip "-" chomping.
//    • Ending newline    => removed then restored.
//  Importantly, this keeps the "+" chomp indicator from gaining an extra line.
function writeScalar(state, string, level, iskey) {
  state.dump = (function () {
    if (string.length === 0) {
      return "''";
    }
    if (!state.noCompatMode &&
        DEPRECATED_BOOLEANS_SYNTAX.indexOf(string) !== -1) {
      return "'" + string + "'";
    }

    var indent = state.indent * Math.max(1, level); // no 0-indent scalars
    // As indentation gets deeper, let the width decrease monotonically
    // to the lower bound min(state.lineWidth, 40).
    // Note that this implies
    //  state.lineWidth ≤ 40 + state.indent: width is fixed at the lower bound.
    //  state.lineWidth > 40 + state.indent: width decreases until the lower bound.
    // This behaves better than a constant minimum width which disallows narrower options,
    // or an indent threshold which causes the width to suddenly increase.
    var lineWidth = state.lineWidth === -1
      ? -1 : Math.max(Math.min(state.lineWidth, 40), state.lineWidth - indent);

    // Without knowing if keys are implicit/explicit, assume implicit for safety.
    var singleLineOnly = iskey
      // No block styles in flow mode.
      || (state.flowLevel > -1 && level >= state.flowLevel);
    function testAmbiguity(string) {
      return testImplicitResolving(state, string);
    }

    switch (chooseScalarStyle(string, singleLineOnly, state.indent, lineWidth, testAmbiguity)) {
      case STYLE_PLAIN:
        return string;
      case STYLE_SINGLE:
        return "'" + string.replace(/'/g, "''") + "'";
      case STYLE_LITERAL:
        return '|' + blockHeader(string, state.indent)
          + dropEndingNewline(indentString(string, indent));
      case STYLE_FOLDED:
        return '>' + blockHeader(string, state.indent)
          + dropEndingNewline(indentString(foldString(string, lineWidth), indent));
      case STYLE_DOUBLE:
        return '"' + escapeString(string, lineWidth) + '"';
      default:
        throw new exception('impossible error: invalid scalar style');
    }
  }());
}

// Pre-conditions: string is valid for a block scalar, 1 <= indentPerLevel <= 9.
function blockHeader(string, indentPerLevel) {
  var indentIndicator = needIndentIndicator(string) ? String(indentPerLevel) : '';

  // note the special case: the string '\n' counts as a "trailing" empty line.
  var clip =          string[string.length - 1] === '\n';
  var keep = clip && (string[string.length - 2] === '\n' || string === '\n');
  var chomp = keep ? '+' : (clip ? '' : '-');

  return indentIndicator + chomp + '\n';
}

// (See the note for writeScalar.)
function dropEndingNewline(string) {
  return string[string.length - 1] === '\n' ? string.slice(0, -1) : string;
}

// Note: a long line without a suitable break point will exceed the width limit.
// Pre-conditions: every char in str isPrintable, str.length > 0, width > 0.
function foldString(string, width) {
  // In folded style, $k$ consecutive newlines output as $k+1$ newlines—
  // unless they're before or after a more-indented line, or at the very
  // beginning or end, in which case $k$ maps to $k$.
  // Therefore, parse each chunk as newline(s) followed by a content line.
  var lineRe = /(\n+)([^\n]*)/g;

  // first line (possibly an empty line)
  var result = (function () {
    var nextLF = string.indexOf('\n');
    nextLF = nextLF !== -1 ? nextLF : string.length;
    lineRe.lastIndex = nextLF;
    return foldLine(string.slice(0, nextLF), width);
  }());
  // If we haven't reached the first content line yet, don't add an extra \n.
  var prevMoreIndented = string[0] === '\n' || string[0] === ' ';
  var moreIndented;

  // rest of the lines
  var match;
  while ((match = lineRe.exec(string))) {
    var prefix = match[1], line = match[2];
    moreIndented = (line[0] === ' ');
    result += prefix
      + (!prevMoreIndented && !moreIndented && line !== ''
        ? '\n' : '')
      + foldLine(line, width);
    prevMoreIndented = moreIndented;
  }

  return result;
}

// Greedy line breaking.
// Picks the longest line under the limit each time,
// otherwise settles for the shortest line over the limit.
// NB. More-indented lines *cannot* be folded, as that would add an extra \n.
function foldLine(line, width) {
  if (line === '' || line[0] === ' ') return line;

  // Since a more-indented line adds a \n, breaks can't be followed by a space.
  var breakRe = / [^ ]/g; // note: the match index will always be <= length-2.
  var match;
  // start is an inclusive index. end, curr, and next are exclusive.
  var start = 0, end, curr = 0, next = 0;
  var result = '';

  // Invariants: 0 <= start <= length-1.
  //   0 <= curr <= next <= max(0, length-2). curr - start <= width.
  // Inside the loop:
  //   A match implies length >= 2, so curr and next are <= length-2.
  while ((match = breakRe.exec(line))) {
    next = match.index;
    // maintain invariant: curr - start <= width
    if (next - start > width) {
      end = (curr > start) ? curr : next; // derive end <= length-2
      result += '\n' + line.slice(start, end);
      // skip the space that was output as \n
      start = end + 1;                    // derive start <= length-1
    }
    curr = next;
  }

  // By the invariants, start <= length-1, so there is something left over.
  // It is either the whole string or a part starting from non-whitespace.
  result += '\n';
  // Insert a break if the remainder is too long and there is a break available.
  if (line.length - start > width && curr > start) {
    result += line.slice(start, curr) + '\n' + line.slice(curr + 1);
  } else {
    result += line.slice(start);
  }

  return result.slice(1); // drop extra \n joiner
}

// Escapes a double-quoted string.
function escapeString(string) {
  var result = '';
  var char, nextChar;
  var escapeSeq;

  for (var i = 0; i < string.length; i++) {
    char = string.charCodeAt(i);
    // Check for surrogate pairs (reference Unicode 3.0 section "3.7 Surrogates").
    if (char >= 0xD800 && char <= 0xDBFF/* high surrogate */) {
      nextChar = string.charCodeAt(i + 1);
      if (nextChar >= 0xDC00 && nextChar <= 0xDFFF/* low surrogate */) {
        // Combine the surrogate pair and store it escaped.
        result += encodeHex((char - 0xD800) * 0x400 + nextChar - 0xDC00 + 0x10000);
        // Advance index one extra since we already used that char here.
        i++; continue;
      }
    }
    escapeSeq = ESCAPE_SEQUENCES[char];
    result += !escapeSeq && isPrintable(char)
      ? string[i]
      : escapeSeq || encodeHex(char);
  }

  return result;
}

function writeFlowSequence(state, level, object) {
  var _result = '',
      _tag    = state.tag,
      index,
      length;

  for (index = 0, length = object.length; index < length; index += 1) {
    // Write only valid elements.
    if (writeNode(state, level, object[index], false, false)) {
      if (index !== 0) _result += ',' + (!state.condenseFlow ? ' ' : '');
      _result += state.dump;
    }
  }

  state.tag = _tag;
  state.dump = '[' + _result + ']';
}

function writeBlockSequence(state, level, object, compact) {
  var _result = '',
      _tag    = state.tag,
      index,
      length;

  for (index = 0, length = object.length; index < length; index += 1) {
    // Write only valid elements.
    if (writeNode(state, level + 1, object[index], true, true)) {
      if (!compact || index !== 0) {
        _result += generateNextLine(state, level);
      }

      if (state.dump && CHAR_LINE_FEED === state.dump.charCodeAt(0)) {
        _result += '-';
      } else {
        _result += '- ';
      }

      _result += state.dump;
    }
  }

  state.tag = _tag;
  state.dump = _result || '[]'; // Empty sequence if no valid values.
}

function writeFlowMapping(state, level, object) {
  var _result       = '',
      _tag          = state.tag,
      objectKeyList = Object.keys(object),
      index,
      length,
      objectKey,
      objectValue,
      pairBuffer;

  for (index = 0, length = objectKeyList.length; index < length; index += 1) {
    pairBuffer = state.condenseFlow ? '"' : '';

    if (index !== 0) pairBuffer += ', ';

    objectKey = objectKeyList[index];
    objectValue = object[objectKey];

    if (!writeNode(state, level, objectKey, false, false)) {
      continue; // Skip this pair because of invalid key;
    }

    if (state.dump.length > 1024) pairBuffer += '? ';

    pairBuffer += state.dump + (state.condenseFlow ? '"' : '') + ':' + (state.condenseFlow ? '' : ' ');

    if (!writeNode(state, level, objectValue, false, false)) {
      continue; // Skip this pair because of invalid value.
    }

    pairBuffer += state.dump;

    // Both key and value are valid.
    _result += pairBuffer;
  }

  state.tag = _tag;
  state.dump = '{' + _result + '}';
}

function writeBlockMapping(state, level, object, compact) {
  var _result       = '',
      _tag          = state.tag,
      objectKeyList = Object.keys(object),
      index,
      length,
      objectKey,
      objectValue,
      explicitPair,
      pairBuffer;

  // Allow sorting keys so that the output file is deterministic
  if (state.sortKeys === true) {
    // Default sorting
    objectKeyList.sort();
  } else if (typeof state.sortKeys === 'function') {
    // Custom sort function
    objectKeyList.sort(state.sortKeys);
  } else if (state.sortKeys) {
    // Something is wrong
    throw new exception('sortKeys must be a boolean or a function');
  }

  for (index = 0, length = objectKeyList.length; index < length; index += 1) {
    pairBuffer = '';

    if (!compact || index !== 0) {
      pairBuffer += generateNextLine(state, level);
    }

    objectKey = objectKeyList[index];
    objectValue = object[objectKey];

    if (!writeNode(state, level + 1, objectKey, true, true, true)) {
      continue; // Skip this pair because of invalid key.
    }

    explicitPair = (state.tag !== null && state.tag !== '?') ||
                   (state.dump && state.dump.length > 1024);

    if (explicitPair) {
      if (state.dump && CHAR_LINE_FEED === state.dump.charCodeAt(0)) {
        pairBuffer += '?';
      } else {
        pairBuffer += '? ';
      }
    }

    pairBuffer += state.dump;

    if (explicitPair) {
      pairBuffer += generateNextLine(state, level);
    }

    if (!writeNode(state, level + 1, objectValue, true, explicitPair)) {
      continue; // Skip this pair because of invalid value.
    }

    if (state.dump && CHAR_LINE_FEED === state.dump.charCodeAt(0)) {
      pairBuffer += ':';
    } else {
      pairBuffer += ': ';
    }

    pairBuffer += state.dump;

    // Both key and value are valid.
    _result += pairBuffer;
  }

  state.tag = _tag;
  state.dump = _result || '{}'; // Empty mapping if no valid pairs.
}

function detectType(state, object, explicit) {
  var _result, typeList, index, length, type, style;

  typeList = explicit ? state.explicitTypes : state.implicitTypes;

  for (index = 0, length = typeList.length; index < length; index += 1) {
    type = typeList[index];

    if ((type.instanceOf  || type.predicate) &&
        (!type.instanceOf || ((typeof object === 'object') && (object instanceof type.instanceOf))) &&
        (!type.predicate  || type.predicate(object))) {

      state.tag = explicit ? type.tag : '?';

      if (type.represent) {
        style = state.styleMap[type.tag] || type.defaultStyle;

        if (_toString$2.call(type.represent) === '[object Function]') {
          _result = type.represent(object, style);
        } else if (_hasOwnProperty$3.call(type.represent, style)) {
          _result = type.represent[style](object, style);
        } else {
          throw new exception('!<' + type.tag + '> tag resolver accepts not "' + style + '" style');
        }

        state.dump = _result;
      }

      return true;
    }
  }

  return false;
}

// Serializes `object` and writes it to global `result`.
// Returns true on success, or false on invalid object.
//
function writeNode(state, level, object, block, compact, iskey) {
  state.tag = null;
  state.dump = object;

  if (!detectType(state, object, false)) {
    detectType(state, object, true);
  }

  var type = _toString$2.call(state.dump);

  if (block) {
    block = (state.flowLevel < 0 || state.flowLevel > level);
  }

  var objectOrArray = type === '[object Object]' || type === '[object Array]',
      duplicateIndex,
      duplicate;

  if (objectOrArray) {
    duplicateIndex = state.duplicates.indexOf(object);
    duplicate = duplicateIndex !== -1;
  }

  if ((state.tag !== null && state.tag !== '?') || duplicate || (state.indent !== 2 && level > 0)) {
    compact = false;
  }

  if (duplicate && state.usedDuplicates[duplicateIndex]) {
    state.dump = '*ref_' + duplicateIndex;
  } else {
    if (objectOrArray && duplicate && !state.usedDuplicates[duplicateIndex]) {
      state.usedDuplicates[duplicateIndex] = true;
    }
    if (type === '[object Object]') {
      if (block && (Object.keys(state.dump).length !== 0)) {
        writeBlockMapping(state, level, state.dump, compact);
        if (duplicate) {
          state.dump = '&ref_' + duplicateIndex + state.dump;
        }
      } else {
        writeFlowMapping(state, level, state.dump);
        if (duplicate) {
          state.dump = '&ref_' + duplicateIndex + ' ' + state.dump;
        }
      }
    } else if (type === '[object Array]') {
      if (block && (state.dump.length !== 0)) {
        writeBlockSequence(state, level, state.dump, compact);
        if (duplicate) {
          state.dump = '&ref_' + duplicateIndex + state.dump;
        }
      } else {
        writeFlowSequence(state, level, state.dump);
        if (duplicate) {
          state.dump = '&ref_' + duplicateIndex + ' ' + state.dump;
        }
      }
    } else if (type === '[object String]') {
      if (state.tag !== '?') {
        writeScalar(state, state.dump, level, iskey);
      }
    } else {
      if (state.skipInvalid) return false;
      throw new exception('unacceptable kind of an object to dump ' + type);
    }

    if (state.tag !== null && state.tag !== '?') {
      state.dump = '!<' + state.tag + '> ' + state.dump;
    }
  }

  return true;
}

function getDuplicateReferences(object, state) {
  var objects = [],
      duplicatesIndexes = [],
      index,
      length;

  inspectNode(object, objects, duplicatesIndexes);

  for (index = 0, length = duplicatesIndexes.length; index < length; index += 1) {
    state.duplicates.push(objects[duplicatesIndexes[index]]);
  }
  state.usedDuplicates = new Array(length);
}

function inspectNode(object, objects, duplicatesIndexes) {
  var objectKeyList,
      index,
      length;

  if (object !== null && typeof object === 'object') {
    index = objects.indexOf(object);
    if (index !== -1) {
      if (duplicatesIndexes.indexOf(index) === -1) {
        duplicatesIndexes.push(index);
      }
    } else {
      objects.push(object);

      if (Array.isArray(object)) {
        for (index = 0, length = object.length; index < length; index += 1) {
          inspectNode(object[index], objects, duplicatesIndexes);
        }
      } else {
        objectKeyList = Object.keys(object);

        for (index = 0, length = objectKeyList.length; index < length; index += 1) {
          inspectNode(object[objectKeyList[index]], objects, duplicatesIndexes);
        }
      }
    }
  }
}

function dump(input, options) {
  options = options || {};

  var state = new State$1(options);

  if (!state.noRefs) getDuplicateReferences(input, state);

  if (writeNode(state, 0, input, true, true)) return state.dump + '\n';

  return '';
}

function safeDump(input, options) {
  return dump(input, common.extend({ schema: default_safe }, options));
}

var dump_1     = dump;
var safeDump_1 = safeDump;

var dumper = {
	dump: dump_1,
	safeDump: safeDump_1
};

function deprecated(name) {
  return function () {
    throw new Error('Function ' + name + ' is deprecated and cannot be used.');
  };
}


var Type$2                = type;
var Schema$2              = schema;
var FAILSAFE_SCHEMA     = failsafe;
var JSON_SCHEMA         = json;
var CORE_SCHEMA         = core;
var DEFAULT_SAFE_SCHEMA$1 = default_safe;
var DEFAULT_FULL_SCHEMA$1 = default_full;
var load$1                = loader.load;
var loadAll$1             = loader.loadAll;
var safeLoad$1            = loader.safeLoad;
var safeLoadAll$1         = loader.safeLoadAll;
var dump$1                = dumper.dump;
var safeDump$1            = dumper.safeDump;
var YAMLException$2       = exception;

// Deprecated schema names from JS-YAML 2.0.x
var MINIMAL_SCHEMA = failsafe;
var SAFE_SCHEMA    = default_safe;
var DEFAULT_SCHEMA = default_full;

// Deprecated functions from JS-YAML 1.x.x
var scan           = deprecated('scan');
var parse          = deprecated('parse');
var compose        = deprecated('compose');
var addConstructor = deprecated('addConstructor');

var jsYaml = {
	Type: Type$2,
	Schema: Schema$2,
	FAILSAFE_SCHEMA: FAILSAFE_SCHEMA,
	JSON_SCHEMA: JSON_SCHEMA,
	CORE_SCHEMA: CORE_SCHEMA,
	DEFAULT_SAFE_SCHEMA: DEFAULT_SAFE_SCHEMA$1,
	DEFAULT_FULL_SCHEMA: DEFAULT_FULL_SCHEMA$1,
	load: load$1,
	loadAll: loadAll$1,
	safeLoad: safeLoad$1,
	safeLoadAll: safeLoadAll$1,
	dump: dump$1,
	safeDump: safeDump$1,
	YAMLException: YAMLException$2,
	MINIMAL_SCHEMA: MINIMAL_SCHEMA,
	SAFE_SCHEMA: SAFE_SCHEMA,
	DEFAULT_SCHEMA: DEFAULT_SCHEMA,
	scan: scan,
	parse: parse,
	compose: compose,
	addConstructor: addConstructor
};

var jsYaml$2 = jsYaml;

var isArrayish = function isArrayish(obj) {
	if (!obj) {
		return false;
	}

	return obj instanceof Array || Array.isArray(obj) ||
		(obj.length >= 0 && obj.splice instanceof Function);
};

var errorEx = function errorEx(name, properties) {
	if (!name || name.constructor !== String) {
		properties = name || {};
		name = Error.name;
	}

	var errorExError = function ErrorEXError(message) {
		if (!this) {
			return new ErrorEXError(message);
		}

		message = message instanceof Error
			? message.message
			: (message || this.message);

		Error.call(this, message);
		Error.captureStackTrace(this, errorExError);

		this.name = name;

		Object.defineProperty(this, 'message', {
			configurable: true,
			enumerable: false,
			get: function () {
				var newMessage = message.split(/\r?\n/g);

				for (var key in properties) {
					if (!properties.hasOwnProperty(key)) {
						continue;
					}

					var modifier = properties[key];

					if ('message' in modifier) {
						newMessage = modifier.message(this[key], newMessage) || newMessage;
						if (!isArrayish(newMessage)) {
							newMessage = [newMessage];
						}
					}
				}

				return newMessage.join('\n');
			},
			set: function (v) {
				message = v;
			}
		});

		var overwrittenStack = null;

		var stackDescriptor = Object.getOwnPropertyDescriptor(this, 'stack');
		var stackGetter = stackDescriptor.get;
		var stackValue = stackDescriptor.value;
		delete stackDescriptor.value;
		delete stackDescriptor.writable;

		stackDescriptor.set = function (newstack) {
			overwrittenStack = newstack;
		};

		stackDescriptor.get = function () {
			var stack = (overwrittenStack || ((stackGetter)
				? stackGetter.call(this)
				: stackValue)).split(/\r?\n+/g);

			// starting in Node 7, the stack builder caches the message.
			// just replace it.
			if (!overwrittenStack) {
				stack[0] = this.name + ': ' + this.message;
			}

			var lineCount = 1;
			for (var key in properties) {
				if (!properties.hasOwnProperty(key)) {
					continue;
				}

				var modifier = properties[key];

				if ('line' in modifier) {
					var line = modifier.line(this[key]);
					if (line) {
						stack.splice(lineCount++, 0, '    ' + line);
					}
				}

				if ('stack' in modifier) {
					modifier.stack(this[key], stack);
				}
			}

			return stack.join('\n');
		};

		Object.defineProperty(this, 'stack', stackDescriptor);
	};

	if (Object.setPrototypeOf) {
		Object.setPrototypeOf(errorExError.prototype, Error.prototype);
		Object.setPrototypeOf(errorExError, Error);
	} else {
		util.inherits(errorExError, Error);
	}

	return errorExError;
};

errorEx.append = function (str, def) {
	return {
		message: function (v, message) {
			v = v || def;

			if (v) {
				message[0] += ' ' + str.replace('%s', v.toString());
			}

			return message;
		}
	};
};

errorEx.line = function (str, def) {
	return {
		line: function (v) {
			v = v || def;

			if (v) {
				return str.replace('%s', v.toString());
			}

			return null;
		}
	};
};

var errorEx_1 = errorEx;

var jsonParseBetterErrors = parseJson;
function parseJson (txt, reviver, context) {
  context = context || 20;
  try {
    return JSON.parse(txt, reviver)
  } catch (e) {
    if (typeof txt !== 'string') {
      const isEmptyArray = Array.isArray(txt) && txt.length === 0;
      const errorMessage = 'Cannot parse ' +
      (isEmptyArray ? 'an empty array' : String(txt));
      throw new TypeError(errorMessage)
    }
    const syntaxErr = e.message.match(/^Unexpected token.*position\s+(\d+)/i);
    const errIdx = syntaxErr
    ? +syntaxErr[1]
    : e.message.match(/^Unexpected end of JSON.*/i)
    ? txt.length - 1
    : null;
    if (errIdx != null) {
      const start = errIdx <= context
      ? 0
      : errIdx - context;
      const end = errIdx + context >= txt.length
      ? txt.length
      : errIdx + context;
      e.message += ` while parsing near '${
        start === 0 ? '' : '...'
      }${txt.slice(start, end)}${
        end === txt.length ? '' : '...'
      }'`;
    } else {
      e.message += ` while parsing '${txt.slice(0, context * 2)}'`;
    }
    throw e
  }
}

var parseJson$1 = createCommonjsModule(function (module) {
const JSONError = errorEx_1('JSONError', {
	fileName: errorEx_1.append('in %s')
});

module.exports = (input, reviver, filename) => {
	if (typeof reviver === 'string') {
		filename = reviver;
		reviver = null;
	}

	try {
		try {
			return JSON.parse(input, reviver);
		} catch (err) {
			jsonParseBetterErrors(input, reviver);

			throw err;
		}
	} catch (err) {
		err.message = err.message.replace(/\n/g, '');

		const jsonErr = new JSONError(err);
		if (filename) {
			jsonErr.fileName = filename;
		}

		throw jsonErr;
	}
};
});

/**
 * Helpers.
 */

var s = 1000;
var m = s * 60;
var h = m * 60;
var d = h * 24;
var y = d * 365.25;

/**
 * Parse or format the given `val`.
 *
 * Options:
 *
 *  - `long` verbose formatting [false]
 *
 * @param {String|Number} val
 * @param {Object} [options]
 * @throws {Error} throw an error if val is not a non-empty string or a number
 * @return {String|Number}
 * @api public
 */

var ms = function(val, options) {
  options = options || {};
  var type = typeof val;
  if (type === 'string' && val.length > 0) {
    return parse$1(val);
  } else if (type === 'number' && isNaN(val) === false) {
    return options.long ? fmtLong(val) : fmtShort(val);
  }
  throw new Error(
    'val is not a non-empty string or a valid number. val=' +
      JSON.stringify(val)
  );
};

/**
 * Parse the given `str` and return milliseconds.
 *
 * @param {String} str
 * @return {Number}
 * @api private
 */

function parse$1(str) {
  str = String(str);
  if (str.length > 100) {
    return;
  }
  var match = /^((?:\d+)?\.?\d+) *(milliseconds?|msecs?|ms|seconds?|secs?|s|minutes?|mins?|m|hours?|hrs?|h|days?|d|years?|yrs?|y)?$/i.exec(
    str
  );
  if (!match) {
    return;
  }
  var n = parseFloat(match[1]);
  var type = (match[2] || 'ms').toLowerCase();
  switch (type) {
    case 'years':
    case 'year':
    case 'yrs':
    case 'yr':
    case 'y':
      return n * y;
    case 'days':
    case 'day':
    case 'd':
      return n * d;
    case 'hours':
    case 'hour':
    case 'hrs':
    case 'hr':
    case 'h':
      return n * h;
    case 'minutes':
    case 'minute':
    case 'mins':
    case 'min':
    case 'm':
      return n * m;
    case 'seconds':
    case 'second':
    case 'secs':
    case 'sec':
    case 's':
      return n * s;
    case 'milliseconds':
    case 'millisecond':
    case 'msecs':
    case 'msec':
    case 'ms':
      return n;
    default:
      return undefined;
  }
}

/**
 * Short format for `ms`.
 *
 * @param {Number} ms
 * @return {String}
 * @api private
 */

function fmtShort(ms) {
  if (ms >= d) {
    return Math.round(ms / d) + 'd';
  }
  if (ms >= h) {
    return Math.round(ms / h) + 'h';
  }
  if (ms >= m) {
    return Math.round(ms / m) + 'm';
  }
  if (ms >= s) {
    return Math.round(ms / s) + 's';
  }
  return ms + 'ms';
}

/**
 * Long format for `ms`.
 *
 * @param {Number} ms
 * @return {String}
 * @api private
 */

function fmtLong(ms) {
  return plural(ms, d, 'day') ||
    plural(ms, h, 'hour') ||
    plural(ms, m, 'minute') ||
    plural(ms, s, 'second') ||
    ms + ' ms';
}

/**
 * Pluralization helper.
 */

function plural(ms, n, name) {
  if (ms < n) {
    return;
  }
  if (ms < n * 1.5) {
    return Math.floor(ms / n) + ' ' + name;
  }
  return Math.ceil(ms / n) + ' ' + name + 's';
}

var debug = createCommonjsModule(function (module, exports) {
/**
 * This is the common logic for both the Node.js and web browser
 * implementations of `debug()`.
 *
 * Expose `debug()` as the module.
 */

exports = module.exports = createDebug.debug = createDebug['default'] = createDebug;
exports.coerce = coerce;
exports.disable = disable;
exports.enable = enable;
exports.enabled = enabled;
exports.humanize = ms;

/**
 * Active `debug` instances.
 */
exports.instances = [];

/**
 * The currently active debug mode names, and names to skip.
 */

exports.names = [];
exports.skips = [];

/**
 * Map of special "%n" handling functions, for the debug "format" argument.
 *
 * Valid key names are a single, lower or upper-case letter, i.e. "n" and "N".
 */

exports.formatters = {};

/**
 * Select a color.
 * @param {String} namespace
 * @return {Number}
 * @api private
 */

function selectColor(namespace) {
  var hash = 0, i;

  for (i in namespace) {
    hash  = ((hash << 5) - hash) + namespace.charCodeAt(i);
    hash |= 0; // Convert to 32bit integer
  }

  return exports.colors[Math.abs(hash) % exports.colors.length];
}

/**
 * Create a debugger with the given `namespace`.
 *
 * @param {String} namespace
 * @return {Function}
 * @api public
 */

function createDebug(namespace) {

  var prevTime;

  function debug() {
    // disabled?
    if (!debug.enabled) return;

    var self = debug;

    // set `diff` timestamp
    var curr = +new Date();
    var ms$$1 = curr - (prevTime || curr);
    self.diff = ms$$1;
    self.prev = prevTime;
    self.curr = curr;
    prevTime = curr;

    // turn the `arguments` into a proper Array
    var args = new Array(arguments.length);
    for (var i = 0; i < args.length; i++) {
      args[i] = arguments[i];
    }

    args[0] = exports.coerce(args[0]);

    if ('string' !== typeof args[0]) {
      // anything else let's inspect with %O
      args.unshift('%O');
    }

    // apply any `formatters` transformations
    var index = 0;
    args[0] = args[0].replace(/%([a-zA-Z%])/g, function(match, format) {
      // if we encounter an escaped % then don't increase the array index
      if (match === '%%') return match;
      index++;
      var formatter = exports.formatters[format];
      if ('function' === typeof formatter) {
        var val = args[index];
        match = formatter.call(self, val);

        // now we need to remove `args[index]` since it's inlined in the `format`
        args.splice(index, 1);
        index--;
      }
      return match;
    });

    // apply env-specific formatting (colors, etc.)
    exports.formatArgs.call(self, args);

    var logFn = debug.log || exports.log || console.log.bind(console);
    logFn.apply(self, args);
  }

  debug.namespace = namespace;
  debug.enabled = exports.enabled(namespace);
  debug.useColors = exports.useColors();
  debug.color = selectColor(namespace);
  debug.destroy = destroy;

  // env-specific initialization logic for debug instances
  if ('function' === typeof exports.init) {
    exports.init(debug);
  }

  exports.instances.push(debug);

  return debug;
}

function destroy () {
  var index = exports.instances.indexOf(this);
  if (index !== -1) {
    exports.instances.splice(index, 1);
    return true;
  } else {
    return false;
  }
}

/**
 * Enables a debug mode by namespaces. This can include modes
 * separated by a colon and wildcards.
 *
 * @param {String} namespaces
 * @api public
 */

function enable(namespaces) {
  exports.save(namespaces);

  exports.names = [];
  exports.skips = [];

  var i;
  var split = (typeof namespaces === 'string' ? namespaces : '').split(/[\s,]+/);
  var len = split.length;

  for (i = 0; i < len; i++) {
    if (!split[i]) continue; // ignore empty strings
    namespaces = split[i].replace(/\*/g, '.*?');
    if (namespaces[0] === '-') {
      exports.skips.push(new RegExp('^' + namespaces.substr(1) + '$'));
    } else {
      exports.names.push(new RegExp('^' + namespaces + '$'));
    }
  }

  for (i = 0; i < exports.instances.length; i++) {
    var instance = exports.instances[i];
    instance.enabled = exports.enabled(instance.namespace);
  }
}

/**
 * Disable debug output.
 *
 * @api public
 */

function disable() {
  exports.enable('');
}

/**
 * Returns true if the given mode name is enabled, false otherwise.
 *
 * @param {String} name
 * @return {Boolean}
 * @api public
 */

function enabled(name) {
  if (name[name.length - 1] === '*') {
    return true;
  }
  var i, len;
  for (i = 0, len = exports.skips.length; i < len; i++) {
    if (exports.skips[i].test(name)) {
      return false;
    }
  }
  for (i = 0, len = exports.names.length; i < len; i++) {
    if (exports.names[i].test(name)) {
      return true;
    }
  }
  return false;
}

/**
 * Coerce `val`.
 *
 * @param {Mixed} val
 * @return {Mixed}
 * @api private
 */

function coerce(val) {
  if (val instanceof Error) return val.stack || val.message;
  return val;
}
});

var debug_1 = debug.coerce;
var debug_2 = debug.disable;
var debug_3 = debug.enable;
var debug_4 = debug.enabled;
var debug_5 = debug.humanize;
var debug_6 = debug.instances;
var debug_7 = debug.names;
var debug_8 = debug.skips;
var debug_9 = debug.formatters;

var browser = createCommonjsModule(function (module, exports) {
/**
 * This is the web browser implementation of `debug()`.
 *
 * Expose `debug()` as the module.
 */

exports = module.exports = debug;
exports.log = log;
exports.formatArgs = formatArgs;
exports.save = save;
exports.load = load;
exports.useColors = useColors;
exports.storage = 'undefined' != typeof chrome
               && 'undefined' != typeof chrome.storage
                  ? chrome.storage.local
                  : localstorage();

/**
 * Colors.
 */

exports.colors = [
  '#0000CC', '#0000FF', '#0033CC', '#0033FF', '#0066CC', '#0066FF', '#0099CC',
  '#0099FF', '#00CC00', '#00CC33', '#00CC66', '#00CC99', '#00CCCC', '#00CCFF',
  '#3300CC', '#3300FF', '#3333CC', '#3333FF', '#3366CC', '#3366FF', '#3399CC',
  '#3399FF', '#33CC00', '#33CC33', '#33CC66', '#33CC99', '#33CCCC', '#33CCFF',
  '#6600CC', '#6600FF', '#6633CC', '#6633FF', '#66CC00', '#66CC33', '#9900CC',
  '#9900FF', '#9933CC', '#9933FF', '#99CC00', '#99CC33', '#CC0000', '#CC0033',
  '#CC0066', '#CC0099', '#CC00CC', '#CC00FF', '#CC3300', '#CC3333', '#CC3366',
  '#CC3399', '#CC33CC', '#CC33FF', '#CC6600', '#CC6633', '#CC9900', '#CC9933',
  '#CCCC00', '#CCCC33', '#FF0000', '#FF0033', '#FF0066', '#FF0099', '#FF00CC',
  '#FF00FF', '#FF3300', '#FF3333', '#FF3366', '#FF3399', '#FF33CC', '#FF33FF',
  '#FF6600', '#FF6633', '#FF9900', '#FF9933', '#FFCC00', '#FFCC33'
];

/**
 * Currently only WebKit-based Web Inspectors, Firefox >= v31,
 * and the Firebug extension (any Firefox version) are known
 * to support "%c" CSS customizations.
 *
 * TODO: add a `localStorage` variable to explicitly enable/disable colors
 */

function useColors() {
  // NB: In an Electron preload script, document will be defined but not fully
  // initialized. Since we know we're in Chrome, we'll just detect this case
  // explicitly
  if (typeof window !== 'undefined' && window.process && window.process.type === 'renderer') {
    return true;
  }

  // Internet Explorer and Edge do not support colors.
  if (typeof navigator !== 'undefined' && navigator.userAgent && navigator.userAgent.toLowerCase().match(/(edge|trident)\/(\d+)/)) {
    return false;
  }

  // is webkit? http://stackoverflow.com/a/16459606/376773
  // document is undefined in react-native: https://github.com/facebook/react-native/pull/1632
  return (typeof document !== 'undefined' && document.documentElement && document.documentElement.style && document.documentElement.style.WebkitAppearance) ||
    // is firebug? http://stackoverflow.com/a/398120/376773
    (typeof window !== 'undefined' && window.console && (window.console.firebug || (window.console.exception && window.console.table))) ||
    // is firefox >= v31?
    // https://developer.mozilla.org/en-US/docs/Tools/Web_Console#Styling_messages
    (typeof navigator !== 'undefined' && navigator.userAgent && navigator.userAgent.toLowerCase().match(/firefox\/(\d+)/) && parseInt(RegExp.$1, 10) >= 31) ||
    // double check webkit in userAgent just in case we are in a worker
    (typeof navigator !== 'undefined' && navigator.userAgent && navigator.userAgent.toLowerCase().match(/applewebkit\/(\d+)/));
}

/**
 * Map %j to `JSON.stringify()`, since no Web Inspectors do that by default.
 */

exports.formatters.j = function(v) {
  try {
    return JSON.stringify(v);
  } catch (err) {
    return '[UnexpectedJSONParseError]: ' + err.message;
  }
};


/**
 * Colorize log arguments if enabled.
 *
 * @api public
 */

function formatArgs(args) {
  var useColors = this.useColors;

  args[0] = (useColors ? '%c' : '')
    + this.namespace
    + (useColors ? ' %c' : ' ')
    + args[0]
    + (useColors ? '%c ' : ' ')
    + '+' + exports.humanize(this.diff);

  if (!useColors) return;

  var c = 'color: ' + this.color;
  args.splice(1, 0, c, 'color: inherit');

  // the final "%c" is somewhat tricky, because there could be other
  // arguments passed either before or after the %c, so we need to
  // figure out the correct index to insert the CSS into
  var index = 0;
  var lastC = 0;
  args[0].replace(/%[a-zA-Z%]/g, function(match) {
    if ('%%' === match) return;
    index++;
    if ('%c' === match) {
      // we only are interested in the *last* %c
      // (the user may have provided their own)
      lastC = index;
    }
  });

  args.splice(lastC, 0, c);
}

/**
 * Invokes `console.log()` when available.
 * No-op when `console.log` is not a "function".
 *
 * @api public
 */

function log() {
  // this hackery is required for IE8/9, where
  // the `console.log` function doesn't have 'apply'
  return 'object' === typeof console
    && console.log
    && Function.prototype.apply.call(console.log, console, arguments);
}

/**
 * Save `namespaces`.
 *
 * @param {String} namespaces
 * @api private
 */

function save(namespaces) {
  try {
    if (null == namespaces) {
      exports.storage.removeItem('debug');
    } else {
      exports.storage.debug = namespaces;
    }
  } catch(e) {}
}

/**
 * Load `namespaces`.
 *
 * @return {String} returns the previously persisted debug modes
 * @api private
 */

function load() {
  var r;
  try {
    r = exports.storage.debug;
  } catch(e) {}

  // If debug isn't set in LS, and we're in Electron, try to load $DEBUG
  if (!r && typeof process !== 'undefined' && 'env' in process) {
    r = process.env.DEBUG;
  }

  return r;
}

/**
 * Enable namespaces listed in `localStorage.debug` initially.
 */

exports.enable(load());

/**
 * Localstorage attempts to return the localstorage.
 *
 * This is necessary because safari throws
 * when a user disables cookies/localstorage
 * and you attempt to access it.
 *
 * @return {LocalStorage}
 * @api private
 */

function localstorage() {
  try {
    return window.localStorage;
  } catch (e) {}
}
});

var browser_1 = browser.log;
var browser_2 = browser.formatArgs;
var browser_3 = browser.save;
var browser_4 = browser.load;
var browser_5 = browser.useColors;
var browser_6 = browser.storage;
var browser_7 = browser.colors;

var hasFlag = function (flag, argv) {
	argv = argv || process.argv;

	var terminatorPos = argv.indexOf('--');
	var prefix = /^-{1,2}/.test(flag) ? '' : '--';
	var pos = argv.indexOf(prefix + flag);

	return pos !== -1 && (terminatorPos === -1 ? true : pos < terminatorPos);
};

var supportsColor = createCommonjsModule(function (module) {
const env = process.env;

const support = level => {
	if (level === 0) {
		return false;
	}

	return {
		level,
		hasBasic: true,
		has256: level >= 2,
		has16m: level >= 3
	};
};

let supportLevel = (() => {
	if (hasFlag('no-color') ||
		hasFlag('no-colors') ||
		hasFlag('color=false')) {
		return 0;
	}

	if (hasFlag('color=16m') ||
		hasFlag('color=full') ||
		hasFlag('color=truecolor')) {
		return 3;
	}

	if (hasFlag('color=256')) {
		return 2;
	}

	if (hasFlag('color') ||
		hasFlag('colors') ||
		hasFlag('color=true') ||
		hasFlag('color=always')) {
		return 1;
	}

	if (process.stdout && !process.stdout.isTTY) {
		return 0;
	}

	if (process.platform === 'win32') {
		// Node.js 7.5.0 is the first version of Node.js to include a patch to
		// libuv that enables 256 color output on Windows. Anything earlier and it
		// won't work. However, here we target Node.js 8 at minimum as it is an LTS
		// release, and Node.js 7 is not. Windows 10 build 10586 is the first Windows
		// release that supports 256 colors.
		const osRelease = os.release().split('.');
		if (
			Number(process.versions.node.split('.')[0]) >= 8 &&
			Number(osRelease[0]) >= 10 &&
			Number(osRelease[2]) >= 10586
		) {
			return 2;
		}

		return 1;
	}

	if ('CI' in env) {
		if (['TRAVIS', 'CIRCLECI', 'APPVEYOR', 'GITLAB_CI'].some(sign => sign in env) || env.CI_NAME === 'codeship') {
			return 1;
		}

		return 0;
	}

	if ('TEAMCITY_VERSION' in env) {
		return /^(9\.(0*[1-9]\d*)\.|\d{2,}\.)/.test(env.TEAMCITY_VERSION) ? 1 : 0;
	}

	if ('TERM_PROGRAM' in env) {
		const version = parseInt((env.TERM_PROGRAM_VERSION || '').split('.')[0], 10);

		switch (env.TERM_PROGRAM) {
			case 'iTerm.app':
				return version >= 3 ? 3 : 2;
			case 'Hyper':
				return 3;
			case 'Apple_Terminal':
				return 2;
			// No default
		}
	}

	if (/-256(color)?$/i.test(env.TERM)) {
		return 2;
	}

	if (/^screen|^xterm|^vt100|^rxvt|color|ansi|cygwin|linux/i.test(env.TERM)) {
		return 1;
	}

	if ('COLORTERM' in env) {
		return 1;
	}

	if (env.TERM === 'dumb') {
		return 0;
	}

	return 0;
})();

if ('FORCE_COLOR' in env) {
	supportLevel = parseInt(env.FORCE_COLOR, 10) === 0 ? 0 : (supportLevel || 1);
}

module.exports = process && support(supportLevel);
});

var node = createCommonjsModule(function (module, exports) {
/**
 * Module dependencies.
 */




/**
 * This is the Node.js implementation of `debug()`.
 *
 * Expose `debug()` as the module.
 */

exports = module.exports = debug;
exports.init = init;
exports.log = log;
exports.formatArgs = formatArgs;
exports.save = save;
exports.load = load;
exports.useColors = useColors;

/**
 * Colors.
 */

exports.colors = [ 6, 2, 3, 4, 5, 1 ];

try {
  var supportsColor$$1 = supportsColor;
  if (supportsColor$$1 && supportsColor$$1.level >= 2) {
    exports.colors = [
      20, 21, 26, 27, 32, 33, 38, 39, 40, 41, 42, 43, 44, 45, 56, 57, 62, 63, 68,
      69, 74, 75, 76, 77, 78, 79, 80, 81, 92, 93, 98, 99, 112, 113, 128, 129, 134,
      135, 148, 149, 160, 161, 162, 163, 164, 165, 166, 167, 168, 169, 170, 171,
      172, 173, 178, 179, 184, 185, 196, 197, 198, 199, 200, 201, 202, 203, 204,
      205, 206, 207, 208, 209, 214, 215, 220, 221
    ];
  }
} catch (err) {
  // swallow - we only care if `supports-color` is available; it doesn't have to be.
}

/**
 * Build up the default `inspectOpts` object from the environment variables.
 *
 *   $ DEBUG_COLORS=no DEBUG_DEPTH=10 DEBUG_SHOW_HIDDEN=enabled node script.js
 */

exports.inspectOpts = Object.keys(process.env).filter(function (key) {
  return /^debug_/i.test(key);
}).reduce(function (obj, key) {
  // camel-case
  var prop = key
    .substring(6)
    .toLowerCase()
    .replace(/_([a-z])/g, function (_, k) { return k.toUpperCase() });

  // coerce string value into JS value
  var val = process.env[key];
  if (/^(yes|on|true|enabled)$/i.test(val)) val = true;
  else if (/^(no|off|false|disabled)$/i.test(val)) val = false;
  else if (val === 'null') val = null;
  else val = Number(val);

  obj[prop] = val;
  return obj;
}, {});

/**
 * Is stdout a TTY? Colored output is enabled when `true`.
 */

function useColors() {
  return 'colors' in exports.inspectOpts
    ? Boolean(exports.inspectOpts.colors)
    : tty.isatty(process.stderr.fd);
}

/**
 * Map %o to `util.inspect()`, all on a single line.
 */

exports.formatters.o = function(v) {
  this.inspectOpts.colors = this.useColors;
  return util.inspect(v, this.inspectOpts)
    .split('\n').map(function(str) {
      return str.trim()
    }).join(' ');
};

/**
 * Map %o to `util.inspect()`, allowing multiple lines if needed.
 */

exports.formatters.O = function(v) {
  this.inspectOpts.colors = this.useColors;
  return util.inspect(v, this.inspectOpts);
};

/**
 * Adds ANSI color escape codes if enabled.
 *
 * @api public
 */

function formatArgs(args) {
  var name = this.namespace;
  var useColors = this.useColors;

  if (useColors) {
    var c = this.color;
    var colorCode = '\u001b[3' + (c < 8 ? c : '8;5;' + c);
    var prefix = '  ' + colorCode + ';1m' + name + ' ' + '\u001b[0m';

    args[0] = prefix + args[0].split('\n').join('\n' + prefix);
    args.push(colorCode + 'm+' + exports.humanize(this.diff) + '\u001b[0m');
  } else {
    args[0] = getDate() + name + ' ' + args[0];
  }
}

function getDate() {
  if (exports.inspectOpts.hideDate) {
    return '';
  } else {
    return new Date().toISOString() + ' ';
  }
}

/**
 * Invokes `util.format()` with the specified arguments and writes to stderr.
 */

function log() {
  return process.stderr.write(util.format.apply(util, arguments) + '\n');
}

/**
 * Save `namespaces`.
 *
 * @param {String} namespaces
 * @api private
 */

function save(namespaces) {
  if (null == namespaces) {
    // If you set a process.env field to null or undefined, it gets cast to the
    // string 'null' or 'undefined'. Just delete instead.
    delete process.env.DEBUG;
  } else {
    process.env.DEBUG = namespaces;
  }
}

/**
 * Load `namespaces`.
 *
 * @return {String} returns the previously persisted debug modes
 * @api private
 */

function load() {
  return process.env.DEBUG;
}

/**
 * Init logic for `debug` instances.
 *
 * Create a new `inspectOpts` object in case `useColors` is set
 * differently for a particular `debug` instance.
 */

function init (debug$$1) {
  debug$$1.inspectOpts = {};

  var keys = Object.keys(exports.inspectOpts);
  for (var i = 0; i < keys.length; i++) {
    debug$$1.inspectOpts[keys[i]] = exports.inspectOpts[keys[i]];
  }
}

/**
 * Enable namespaces listed in `process.env.DEBUG` initially.
 */

exports.enable(load());
});

var node_1 = node.init;
var node_2 = node.log;
var node_3 = node.formatArgs;
var node_4 = node.save;
var node_5 = node.load;
var node_6 = node.useColors;
var node_7 = node.colors;
var node_8 = node.inspectOpts;

var src = createCommonjsModule(function (module) {
/**
 * Detect Electron renderer process, which is node, but we should
 * treat as a browser.
 */

if (typeof process === 'undefined' || process.type === 'renderer') {
  module.exports = browser;
} else {
  module.exports = node;
}
});

var resolveFrom_1 = createCommonjsModule(function (module) {
const resolveFrom = (fromDir, moduleId, silent) => {
	if (typeof fromDir !== 'string') {
		throw new TypeError(`Expected \`fromDir\` to be of type \`string\`, got \`${typeof fromDir}\``);
	}

	if (typeof moduleId !== 'string') {
		throw new TypeError(`Expected \`moduleId\` to be of type \`string\`, got \`${typeof moduleId}\``);
	}

	try {
		fromDir = fs.realpathSync(fromDir);
	} catch (err) {
		if (err.code === 'ENOENT') {
			fromDir = path.resolve(fromDir);
		} else if (silent) {
			return null;
		} else {
			throw err;
		}
	}

	const fromFile = path.join(fromDir, 'noop.js');

	const resolveFileName = () => module$1._resolveFilename(moduleId, {
		id: fromFile,
		filename: fromFile,
		paths: module$1._nodeModulePaths(fromDir)
	});

	if (silent) {
		try {
			return resolveFileName();
		} catch (err) {
			return null;
		}
	}

	return resolveFileName();
};

module.exports = (fromDir, moduleId) => resolveFrom(fromDir, moduleId);
module.exports.silent = (fromDir, moduleId) => resolveFrom(fromDir, moduleId, true);
});

var resolveFrom_2 = resolveFrom_1.silent;

var ini = createCommonjsModule(function (module, exports) {
exports.parse = exports.decode = decode;

exports.stringify = exports.encode = encode;

exports.safe = safe;
exports.unsafe = unsafe;

var eol = typeof process !== 'undefined' &&
  process.platform === 'win32' ? '\r\n' : '\n';

function encode (obj, opt) {
  var children = [];
  var out = '';

  if (typeof opt === 'string') {
    opt = {
      section: opt,
      whitespace: false
    };
  } else {
    opt = opt || {};
    opt.whitespace = opt.whitespace === true;
  }

  var separator = opt.whitespace ? ' = ' : '=';

  Object.keys(obj).forEach(function (k, _, __) {
    var val = obj[k];
    if (val && Array.isArray(val)) {
      val.forEach(function (item) {
        out += safe(k + '[]') + separator + safe(item) + '\n';
      });
    } else if (val && typeof val === 'object') {
      children.push(k);
    } else {
      out += safe(k) + separator + safe(val) + eol;
    }
  });

  if (opt.section && out.length) {
    out = '[' + safe(opt.section) + ']' + eol + out;
  }

  children.forEach(function (k, _, __) {
    var nk = dotSplit(k).join('\\.');
    var section = (opt.section ? opt.section + '.' : '') + nk;
    var child = encode(obj[k], {
      section: section,
      whitespace: opt.whitespace
    });
    if (out.length && child.length) {
      out += eol;
    }
    out += child;
  });

  return out
}

function dotSplit (str) {
  return str.replace(/\1/g, '\u0002LITERAL\\1LITERAL\u0002')
    .replace(/\\\./g, '\u0001')
    .split(/\./).map(function (part) {
      return part.replace(/\1/g, '\\.')
      .replace(/\2LITERAL\\1LITERAL\2/g, '\u0001')
    })
}

function decode (str) {
  var out = {};
  var p = out;
  var section = null;
  //          section     |key      = value
  var re = /^\[([^\]]*)\]$|^([^=]+)(=(.*))?$/i;
  var lines = str.split(/[\r\n]+/g);

  lines.forEach(function (line, _, __) {
    if (!line || line.match(/^\s*[;#]/)) return
    var match = line.match(re);
    if (!match) return
    if (match[1] !== undefined) {
      section = unsafe(match[1]);
      p = out[section] = out[section] || {};
      return
    }
    var key = unsafe(match[2]);
    var value = match[3] ? unsafe(match[4]) : true;
    switch (value) {
      case 'true':
      case 'false':
      case 'null': value = JSON.parse(value);
    }

    // Convert keys with '[]' suffix to an array
    if (key.length > 2 && key.slice(-2) === '[]') {
      key = key.substring(0, key.length - 2);
      if (!p[key]) {
        p[key] = [];
      } else if (!Array.isArray(p[key])) {
        p[key] = [p[key]];
      }
    }

    // safeguard against resetting a previously defined
    // array by accidentally forgetting the brackets
    if (Array.isArray(p[key])) {
      p[key].push(value);
    } else {
      p[key] = value;
    }
  });

  // {a:{y:1},"a.b":{x:2}} --> {a:{y:1,b:{x:2}}}
  // use a filter to return the keys that have to be deleted.
  Object.keys(out).filter(function (k, _, __) {
    if (!out[k] ||
      typeof out[k] !== 'object' ||
      Array.isArray(out[k])) {
      return false
    }
    // see if the parent section is also an object.
    // if so, add it to that, and mark this one for deletion
    var parts = dotSplit(k);
    var p = out;
    var l = parts.pop();
    var nl = l.replace(/\\\./g, '.');
    parts.forEach(function (part, _, __) {
      if (!p[part] || typeof p[part] !== 'object') p[part] = {};
      p = p[part];
    });
    if (p === out && nl === l) {
      return false
    }
    p[nl] = out[k];
    return true
  }).forEach(function (del, _, __) {
    delete out[del];
  });

  return out
}

function isQuoted (val) {
  return (val.charAt(0) === '"' && val.slice(-1) === '"') ||
    (val.charAt(0) === "'" && val.slice(-1) === "'")
}

function safe (val) {
  return (typeof val !== 'string' ||
    val.match(/[=\r\n]/) ||
    val.match(/^\[/) ||
    (val.length > 1 &&
     isQuoted(val)) ||
    val !== val.trim())
      ? JSON.stringify(val)
      : val.replace(/;/g, '\\;').replace(/#/g, '\\#')
}

function unsafe (val, doUnesc) {
  val = (val || '').trim();
  if (isQuoted(val)) {
    // remove the single quotes before calling JSON.parse
    if (val.charAt(0) === "'") {
      val = val.substr(1, val.length - 2);
    }
    try { val = JSON.parse(val); } catch (_) {}
  } else {
    // walk the val to find the first not-escaped ; character
    var esc = false;
    var unesc = '';
    for (var i = 0, l = val.length; i < l; i++) {
      var c = val.charAt(i);
      if (esc) {
        if ('\\;#'.indexOf(c) !== -1) {
          unesc += c;
        } else {
          unesc += '\\' + c;
        }
        esc = false;
      } else if (';#'.indexOf(c) !== -1) {
        break
      } else if (c === '\\') {
        esc = true;
      } else {
        unesc += c;
      }
    }
    if (esc) {
      unesc += '\\';
    }
    return unesc.trim()
  }
  return val
}
});

var ini_1 = ini.parse;
var ini_2 = ini.decode;
var ini_3 = ini.stringify;
var ini_4 = ini.encode;
var ini_5 = ini.safe;
var ini_6 = ini.unsafe;

var singleComment = 1;
var multiComment = 2;

function stripWithoutWhitespace() {
	return '';
}

function stripWithWhitespace(str, start, end) {
	return str.slice(start, end).replace(/\S/g, ' ');
}

var stripJsonComments = function (str, opts) {
	opts = opts || {};

	var currentChar;
	var nextChar;
	var insideString = false;
	var insideComment = false;
	var offset = 0;
	var ret = '';
	var strip = opts.whitespace === false ? stripWithoutWhitespace : stripWithWhitespace;

	for (var i = 0; i < str.length; i++) {
		currentChar = str[i];
		nextChar = str[i + 1];

		if (!insideComment && currentChar === '"') {
			var escaped = str[i - 1] === '\\' && str[i - 2] !== '\\';
			if (!escaped) {
				insideString = !insideString;
			}
		}

		if (insideString) {
			continue;
		}

		if (!insideComment && currentChar + nextChar === '//') {
			ret += str.slice(offset, i);
			offset = i;
			insideComment = singleComment;
			i++;
		} else if (insideComment === singleComment && currentChar + nextChar === '\r\n') {
			i++;
			insideComment = false;
			ret += strip(str, offset, i);
			offset = i;
			continue;
		} else if (insideComment === singleComment && currentChar === '\n') {
			insideComment = false;
			ret += strip(str, offset, i);
			offset = i;
		} else if (!insideComment && currentChar + nextChar === '/*') {
			ret += str.slice(offset, i);
			offset = i;
			insideComment = multiComment;
			i++;
			continue;
		} else if (insideComment === multiComment && currentChar + nextChar === '*/') {
			i++;
			insideComment = false;
			ret += strip(str, offset, i + 1);
			offset = i + 1;
			continue;
		}
	}

	return ret + (insideComment ? strip(str.substr(offset)) : str.substr(offset));
};

var utils = createCommonjsModule(function (module, exports) {
var parse = exports.parse = function (content) {

  //if it ends in .json or starts with { then it must be json.
  //must be done this way, because ini accepts everything.
  //can't just try and parse it and let it throw if it's not ini.
  //everything is ini. even json with a syntax error.

  if(/^\s*{/.test(content))
    return JSON.parse(stripJsonComments(content))
  return ini.parse(content)

};

var file = exports.file = function () {
  var args = [].slice.call(arguments).filter(function (arg) { return arg != null });

  //path.join breaks if it's a not a string, so just skip this.
  for(var i in args)
    if('string' !== typeof args[i])
      return

  var file = path.join.apply(null, args);
  try {
    return fs.readFileSync(file,'utf-8')
  } catch (err) {
    return
  }
};

var json = exports.json = function () {
  var content = file.apply(null, arguments);
  return content ? parse(content) : null
};

var env = exports.env = function (prefix, env) {
  env = env || process.env;
  var obj = {};
  var l = prefix.length;
  for(var k in env) {
    if(k.toLowerCase().indexOf(prefix.toLowerCase()) === 0) {

      var keypath = k.substring(l).split('__');

      // Trim empty strings from keypath array
      var _emptyStringIndex;
      while ((_emptyStringIndex=keypath.indexOf('')) > -1) {
        keypath.splice(_emptyStringIndex, 1);
      }

      var cursor = obj;
      keypath.forEach(function _buildSubObj(_subkey,i){

        // (check for _subkey first so we ignore empty strings)
        // (check for cursor to avoid assignment to primitive objects)
        if (!_subkey || typeof cursor !== 'object')
          return

        // If this is the last key, just stuff the value in there
        // Assigns actual value from env variable to final key
        // (unless it's just an empty string- in that case use the last valid key)
        if (i === keypath.length-1)
          cursor[_subkey] = env[k];


        // Build sub-object if nothing already exists at the keypath
        if (cursor[_subkey] === undefined)
          cursor[_subkey] = {};

        // Increment cursor used to track the object at the current depth
        cursor = cursor[_subkey];

      });

    }

  }

  return obj
};

var find = exports.find = function () {
  var rel = path.join.apply(null, [].slice.call(arguments));

  function find(start, rel) {
    var file = path.join(start, rel);
    try {
      fs.statSync(file);
      return file
    } catch (err) {
      if(path.dirname(start) !== start) // root
        return find(path.dirname(start), rel)
    }
  }
  return find(process.cwd(), rel)
};
});

var utils_1 = utils.parse;
var utils_2 = utils.file;
var utils_3 = utils.json;
var utils_4 = utils.env;
var utils_5 = utils.find;

var deepExtend_1 = createCommonjsModule(function (module) {
/*!
 * @description Recursive object extending
 * @author Viacheslav Lotsmanov <lotsmanov89@gmail.com>
 * @license MIT
 *
 * The MIT License (MIT)
 *
 * Copyright (c) 2013-2018 Viacheslav Lotsmanov
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy of
 * this software and associated documentation files (the "Software"), to deal in
 * the Software without restriction, including without limitation the rights to
 * use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of
 * the Software, and to permit persons to whom the Software is furnished to do so,
 * subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
 * FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
 * COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
 * IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

function isSpecificValue(val) {
	return (
		val instanceof Buffer
		|| val instanceof Date
		|| val instanceof RegExp
	) ? true : false;
}

function cloneSpecificValue(val) {
	if (val instanceof Buffer) {
		var x = Buffer.alloc
			? Buffer.alloc(val.length)
			: new Buffer(val.length);
		val.copy(x);
		return x;
	} else if (val instanceof Date) {
		return new Date(val.getTime());
	} else if (val instanceof RegExp) {
		return new RegExp(val);
	} else {
		throw new Error('Unexpected situation');
	}
}

/**
 * Recursive cloning array.
 */
function deepCloneArray(arr) {
	var clone = [];
	arr.forEach(function (item, index) {
		if (typeof item === 'object' && item !== null) {
			if (Array.isArray(item)) {
				clone[index] = deepCloneArray(item);
			} else if (isSpecificValue(item)) {
				clone[index] = cloneSpecificValue(item);
			} else {
				clone[index] = deepExtend({}, item);
			}
		} else {
			clone[index] = item;
		}
	});
	return clone;
}

function safeGetProperty(object, property) {
	return property === '__proto__' ? undefined : object[property];
}

/**
 * Extening object that entered in first argument.
 *
 * Returns extended object or false if have no target object or incorrect type.
 *
 * If you wish to clone source object (without modify it), just use empty new
 * object as first argument, like this:
 *   deepExtend({}, yourObj_1, [yourObj_N]);
 */
var deepExtend = module.exports = function (/*obj_1, [obj_2], [obj_N]*/) {
	if (arguments.length < 1 || typeof arguments[0] !== 'object') {
		return false;
	}

	if (arguments.length < 2) {
		return arguments[0];
	}

	var target = arguments[0];

	// convert arguments to array and cut off target object
	var args = Array.prototype.slice.call(arguments, 1);

	var val, src;

	args.forEach(function (obj) {
		// skip argument if isn't an object, is null, or is an array
		if (typeof obj !== 'object' || obj === null || Array.isArray(obj)) {
			return;
		}

		Object.keys(obj).forEach(function (key) {
			src = safeGetProperty(target, key); // source value
			val = safeGetProperty(obj, key); // new value

			// recursion prevention
			if (val === target) {
				return;

			/**
			 * if new value isn't object then just overwrite by new value
			 * instead of extending.
			 */
			} else if (typeof val !== 'object' || val === null) {
				target[key] = val;
				return;

			// just clone arrays (and recursive clone objects inside)
			} else if (Array.isArray(val)) {
				target[key] = deepCloneArray(val);
				return;

			// custom cloning and overwrite for specific objects
			} else if (isSpecificValue(val)) {
				target[key] = cloneSpecificValue(val);
				return;

			// overwrite by new value if source isn't object or array
			} else if (typeof src !== 'object' || src === null || Array.isArray(src)) {
				target[key] = deepExtend({}, val);
				return;

			// source value and new value is objects both, extending...
			} else {
				target[key] = deepExtend(src, val);
				return;
			}
		});
	});

	return target;
};
});

var minimist = function (args, opts) {
    if (!opts) opts = {};

    var flags = { bools : {}, strings : {}, unknownFn: null };

    if (typeof opts['unknown'] === 'function') {
        flags.unknownFn = opts['unknown'];
    }

    if (typeof opts['boolean'] === 'boolean' && opts['boolean']) {
      flags.allBools = true;
    } else {
      [].concat(opts['boolean']).filter(Boolean).forEach(function (key) {
          flags.bools[key] = true;
      });
    }

    var aliases = {};
    Object.keys(opts.alias || {}).forEach(function (key) {
        aliases[key] = [].concat(opts.alias[key]);
        aliases[key].forEach(function (x) {
            aliases[x] = [key].concat(aliases[key].filter(function (y) {
                return x !== y;
            }));
        });
    });

    [].concat(opts.string).filter(Boolean).forEach(function (key) {
        flags.strings[key] = true;
        if (aliases[key]) {
            flags.strings[aliases[key]] = true;
        }
     });

    var defaults = opts['default'] || {};

    var argv = { _ : [] };
    Object.keys(flags.bools).forEach(function (key) {
        setArg(key, defaults[key] === undefined ? false : defaults[key]);
    });

    var notFlags = [];

    if (args.indexOf('--') !== -1) {
        notFlags = args.slice(args.indexOf('--')+1);
        args = args.slice(0, args.indexOf('--'));
    }

    function argDefined(key, arg) {
        return (flags.allBools && /^--[^=]+$/.test(arg)) ||
            flags.strings[key] || flags.bools[key] || aliases[key];
    }

    function setArg (key, val, arg) {
        if (arg && flags.unknownFn && !argDefined(key, arg)) {
            if (flags.unknownFn(arg) === false) return;
        }

        var value = !flags.strings[key] && isNumber(val)
            ? Number(val) : val;
        setKey(argv, key.split('.'), value);

        (aliases[key] || []).forEach(function (x) {
            setKey(argv, x.split('.'), value);
        });
    }

    function setKey (obj, keys, value) {
        var o = obj;
        keys.slice(0,-1).forEach(function (key) {
            if (o[key] === undefined) o[key] = {};
            o = o[key];
        });

        var key = keys[keys.length - 1];
        if (o[key] === undefined || flags.bools[key] || typeof o[key] === 'boolean') {
            o[key] = value;
        }
        else if (Array.isArray(o[key])) {
            o[key].push(value);
        }
        else {
            o[key] = [ o[key], value ];
        }
    }

    function aliasIsBoolean(key) {
      return aliases[key].some(function (x) {
          return flags.bools[x];
      });
    }

    for (var i = 0; i < args.length; i++) {
        var arg = args[i];

        if (/^--.+=/.test(arg)) {
            // Using [\s\S] instead of . because js doesn't support the
            // 'dotall' regex modifier. See:
            // http://stackoverflow.com/a/1068308/13216
            var m = arg.match(/^--([^=]+)=([\s\S]*)$/);
            var key = m[1];
            var value = m[2];
            if (flags.bools[key]) {
                value = value !== 'false';
            }
            setArg(key, value, arg);
        }
        else if (/^--no-.+/.test(arg)) {
            var key = arg.match(/^--no-(.+)/)[1];
            setArg(key, false, arg);
        }
        else if (/^--.+/.test(arg)) {
            var key = arg.match(/^--(.+)/)[1];
            var next = args[i + 1];
            if (next !== undefined && !/^-/.test(next)
            && !flags.bools[key]
            && !flags.allBools
            && (aliases[key] ? !aliasIsBoolean(key) : true)) {
                setArg(key, next, arg);
                i++;
            }
            else if (/^(true|false)$/.test(next)) {
                setArg(key, next === 'true', arg);
                i++;
            }
            else {
                setArg(key, flags.strings[key] ? '' : true, arg);
            }
        }
        else if (/^-[^-]+/.test(arg)) {
            var letters = arg.slice(1,-1).split('');

            var broken = false;
            for (var j = 0; j < letters.length; j++) {
                var next = arg.slice(j+2);

                if (next === '-') {
                    setArg(letters[j], next, arg);
                    continue;
                }

                if (/[A-Za-z]/.test(letters[j]) && /=/.test(next)) {
                    setArg(letters[j], next.split('=')[1], arg);
                    broken = true;
                    break;
                }

                if (/[A-Za-z]/.test(letters[j])
                && /-?\d+(\.\d*)?(e-?\d+)?$/.test(next)) {
                    setArg(letters[j], next, arg);
                    broken = true;
                    break;
                }

                if (letters[j+1] && letters[j+1].match(/\W/)) {
                    setArg(letters[j], arg.slice(j+2), arg);
                    broken = true;
                    break;
                }
                else {
                    setArg(letters[j], flags.strings[letters[j]] ? '' : true, arg);
                }
            }

            var key = arg.slice(-1)[0];
            if (!broken && key !== '-') {
                if (args[i+1] && !/^(-|--)[^-]/.test(args[i+1])
                && !flags.bools[key]
                && (aliases[key] ? !aliasIsBoolean(key) : true)) {
                    setArg(key, args[i+1], arg);
                    i++;
                }
                else if (args[i+1] && /true|false/.test(args[i+1])) {
                    setArg(key, args[i+1] === 'true', arg);
                    i++;
                }
                else {
                    setArg(key, flags.strings[key] ? '' : true, arg);
                }
            }
        }
        else {
            if (!flags.unknownFn || flags.unknownFn(arg) !== false) {
                argv._.push(
                    flags.strings['_'] || !isNumber(arg) ? arg : Number(arg)
                );
            }
            if (opts.stopEarly) {
                argv._.push.apply(argv._, args.slice(i + 1));
                break;
            }
        }
    }

    Object.keys(defaults).forEach(function (key) {
        if (!hasKey(argv, key.split('.'))) {
            setKey(argv, key.split('.'), defaults[key]);

            (aliases[key] || []).forEach(function (x) {
                setKey(argv, x.split('.'), defaults[key]);
            });
        }
    });

    if (opts['--']) {
        argv['--'] = new Array();
        notFlags.forEach(function(key) {
            argv['--'].push(key);
        });
    }
    else {
        notFlags.forEach(function(key) {
            argv._.push(key);
        });
    }

    return argv;
};

function hasKey (obj, keys) {
    var o = obj;
    keys.slice(0,-1).forEach(function (key) {
        o = (o[key] || {});
    });

    var key = keys[keys.length - 1];
    return key in o;
}

function isNumber (x) {
    if (typeof x === 'number') return true;
    if (/^0x[0-9a-f]+$/i.test(x)) return true;
    return /^[-+]?(?:\d+(?:\.\d*)?|\.\d+)(e[-+]?\d+)?$/.test(x);
}

var join = path.join;

var etc = '/etc';
var win = process.platform === "win32";
var home = win
           ? process.env.USERPROFILE
           : process.env.HOME;

var rc = function (name, defaults, argv, parse) {
  if('string' !== typeof name)
    throw new Error('rc(name): name *must* be string')
  if(!argv)
    argv = minimist(process.argv.slice(2));
  defaults = (
      'string' === typeof defaults
    ? utils.json(defaults) : defaults
    ) || {};

  parse = parse || utils.parse;

  var env = utils.env(name + '_');

  var configs = [defaults];
  var configFiles = [];
  function addConfigFile (file) {
    if (configFiles.indexOf(file) >= 0) return
    var fileConfig = utils.file(file);
    if (fileConfig) {
      configs.push(parse(fileConfig));
      configFiles.push(file);
    }
  }

  // which files do we look at?
  if (!win)
   [join(etc, name, 'config'),
    join(etc, name + 'rc')].forEach(addConfigFile);
  if (home)
   [join(home, '.config', name, 'config'),
    join(home, '.config', name),
    join(home, '.' + name, 'config'),
    join(home, '.' + name + 'rc')].forEach(addConfigFile);
  addConfigFile(utils.find('.'+name+'rc'));
  if (env.config) addConfigFile(env.config);
  if (argv.config) addConfigFile(argv.config);

  return deepExtend_1.apply(null, configs.concat([
    env,
    argv,
    configFiles.length ? {configs: configFiles, config: configFiles[configFiles.length - 1]} : undefined,
  ]))
};

function homedir() {
	var env = process.env;
	var home = env.HOME;
	var user = env.LOGNAME || env.USER || env.LNAME || env.USERNAME;

	if (process.platform === 'win32') {
		return env.USERPROFILE || env.HOMEDRIVE + env.HOMEPATH || home || null;
	}

	if (process.platform === 'darwin') {
		return home || (user ? '/Users/' + user : null);
	}

	if (process.platform === 'linux') {
		return home || (process.getuid() === 0 ? '/root' : (user ? '/home/' + user : null));
	}

	return home || null;
}

var osHomedir = typeof os.homedir === 'function' ? os.homedir : homedir;

var home$1 = osHomedir();

var untildify = function (str) {
	if (typeof str !== 'string') {
		throw new TypeError('Expected a string');
	}

	return home$1 ? str.replace(/^~($|\/|\\)/, home$1 + '$1') : str;
};

var shellsubstitute = function (s, vars) {
  return s.replace(/(\\*)(\$([_a-z0-9]+)|\${([_a-z0-9]+)})/ig, function (_, escape, varExpression, variable, bracedVariable) {
    if (!(escape.length % 2)) {
      return escape.substring(Math.ceil(escape.length / 2)) + (vars[variable || bracedVariable] || '');
    } else {
      return escape.substring(1) + varExpression;
    }
  });
};

var npmPrefix = function () {
  var rcPrefix = rc('npm', null, []).prefix;

  if (rcPrefix) {
    return untildify(shellsubstitute(rcPrefix, process.env));
  }
  else if (process.platform == 'win32') {
    return path.dirname(process.execPath);
  }
  else {
    return path.resolve(process.execPath, '../..');
  }
};

var resolve = resolveFrom_1.silent;
var npmPrefix$2 = npmPrefix();

var loadPlugin_1 = loadPlugin;
loadPlugin.resolve = resolvePlugin;

var electron = process.versions.electron !== undefined;
var argv = process.argv[1] || /* istanbul ignore next */ '';
var nvm = process.env.NVM_BIN;
var globally = electron || argv.indexOf(npmPrefix$2) === 0;
var windows = process.platform === 'win32';
var prefix = windows ? /* istanbul ignore next */ '' : 'lib';
var globals = path.resolve(npmPrefix$2, prefix, 'node_modules');

/* istanbul ignore next - If we’re in Electron, we’re running in a modified
 * Node that cannot really install global node modules.  To find the actual
 * modules, the user has to either set `prefix` in their `.npmrc` (which is
 * picked up by `npm-prefix`).  Most people don’t do that, and some use NVM
 * instead to manage different versions of Node.  Luckily NVM leaks some
 * environment variables that we can pick up on to try and detect the actual
 * modules. */
if (electron && nvm && !fs.existsSync(globals)) {
  globals = path.resolve(nvm, '..', prefix, 'node_modules');
}

/* Load the plug-in found using `resolvePlugin`. */
function loadPlugin(name, options) {
  return commonjsRequire(resolvePlugin(name, options) || name)
}

/* Find a plugin.
 *
 * See also:
 * <https://docs.npmjs.com/files/folders#node-modules>
 * <https://github.com/sindresorhus/resolve-from>
 *
 * Uses the standard node module loading strategy to find $name
 * in each given `cwd` (and optionally the global node_modules
 * directory).
 *
 * If a prefix is given and $name is not a path, `$prefix-$name`
 * is also searched (preferring these over non-prefixed modules). */
function resolvePlugin(name, options) {
  var settings = options || {};
  var prefix = settings.prefix;
  var cwd = settings.cwd;
  var filePath;
  var sources;
  var length;
  var index;
  var plugin;

  if (cwd && typeof cwd === 'object') {
    sources = cwd.concat();
  } else {
    sources = [cwd || process.cwd()];
  }

  /* Non-path. */
  if (name.indexOf(path.sep) === -1 && name.charAt(0) !== '.') {
    if (settings.global == null ? globally : settings.global) {
      sources.push(globals);
    }

    /* Unprefix module. */
    if (prefix) {
      prefix = prefix.charAt(prefix.length - 1) === '-' ? prefix : prefix + '-';

      if (name.slice(0, prefix.length) !== prefix) {
        plugin = prefix + name;
      }
    }
  }

  length = sources.length;
  index = -1;

  while (++index < length) {
    cwd = sources[index];
    filePath = (plugin && resolve(cwd, plugin)) || resolve(cwd, name);

    if (filePath) {
      return filePath
    }
  }

  return null
}

var format = createCommonjsModule(function (module) {
//
// format - printf-like string formatting for JavaScript
// github.com/samsonjs/format
// @_sjs
//
// Copyright 2010 - 2013 Sami Samhuri <sami@samhuri.net>
//
// MIT License
// http://sjs.mit-license.org
//

(function() {

  //// Export the API
  var namespace;

  // CommonJS / Node module
  {
    namespace = module.exports = format;
  }

  namespace.format = format;
  namespace.vsprintf = vsprintf;

  if (typeof console !== 'undefined' && typeof console.log === 'function') {
    namespace.printf = printf;
  }

  function printf(/* ... */) {
    console.log(format.apply(null, arguments));
  }

  function vsprintf(fmt, replacements) {
    return format.apply(null, [fmt].concat(replacements));
  }

  function format(fmt) {
    var argIndex = 1 // skip initial format argument
      , args = [].slice.call(arguments)
      , i = 0
      , n = fmt.length
      , result = ''
      , c
      , escaped = false
      , arg
      , tmp
      , leadingZero = false
      , precision
      , nextArg = function() { return args[argIndex++]; }
      , slurpNumber = function() {
          var digits = '';
          while (/\d/.test(fmt[i])) {
            digits += fmt[i++];
            c = fmt[i];
          }
          return digits.length > 0 ? parseInt(digits) : null;
        };
    for (; i < n; ++i) {
      c = fmt[i];
      if (escaped) {
        escaped = false;
        if (c == '.') {
          leadingZero = false;
          c = fmt[++i];
        }
        else if (c == '0' && fmt[i + 1] == '.') {
          leadingZero = true;
          i += 2;
          c = fmt[i];
        }
        else {
          leadingZero = true;
        }
        precision = slurpNumber();
        switch (c) {
        case 'b': // number in binary
          result += parseInt(nextArg(), 10).toString(2);
          break;
        case 'c': // character
          arg = nextArg();
          if (typeof arg === 'string' || arg instanceof String)
            result += arg;
          else
            result += String.fromCharCode(parseInt(arg, 10));
          break;
        case 'd': // number in decimal
          result += parseInt(nextArg(), 10);
          break;
        case 'f': // floating point number
          tmp = String(parseFloat(nextArg()).toFixed(precision || 6));
          result += leadingZero ? tmp : tmp.replace(/^0/, '');
          break;
        case 'j': // JSON
          result += JSON.stringify(nextArg());
          break;
        case 'o': // number in octal
          result += '0' + parseInt(nextArg(), 10).toString(8);
          break;
        case 's': // string
          result += nextArg();
          break;
        case 'x': // lowercase hexadecimal
          result += '0x' + parseInt(nextArg(), 10).toString(16);
          break;
        case 'X': // uppercase hexadecimal
          result += '0x' + parseInt(nextArg(), 10).toString(16).toUpperCase();
          break;
        default:
          result += c;
          break;
        }
      } else if (c === '%') {
        escaped = true;
      } else {
        result += c;
      }
    }
    return result;
  }

}());
});

var fault = create(Error);

var fault_1 = fault;

fault.eval = create(EvalError);
fault.range = create(RangeError);
fault.reference = create(ReferenceError);
fault.syntax = create(SyntaxError);
fault.type = create(TypeError);
fault.uri = create(URIError);

fault.create = create;

/* Create a new `EConstructor`, with the formatted
 * `format` as a first argument. */
function create(EConstructor) {
  FormattedError.displayName = EConstructor.displayName || EConstructor.name;

  return FormattedError

  function FormattedError(format$$1) {
    if (format$$1) {
      format$$1 = format.apply(null, arguments);
    }

    return new EConstructor(format$$1)
  }
}

var immutable = extend$1;

var hasOwnProperty = Object.prototype.hasOwnProperty;

function extend$1() {
    var target = {};

    for (var i = 0; i < arguments.length; i++) {
        var source = arguments[i];

        for (var key in source) {
            if (hasOwnProperty.call(source, key)) {
                target[key] = source[key];
            }
        }
    }

    return target
}

var isObject$1 = function isObject(x) {
	return typeof x === "object" && x !== null;
};

var toString = Object.prototype.toString;

var xIsString = isString;

function isString(obj) {
    return toString.call(obj) === "[object String]"
}

var xIsFunction = function isFunction (fn) {
  return Object.prototype.toString.call(fn) === '[object Function]'
};

var debug$2 = src('unified-engine:find-up');



var findUp = FindUp;

var read = fs.readFile;
var resolve$1 = path.resolve;
var relative = path.relative;
var join$1 = path.join;
var dirname = path.dirname;

FindUp.prototype.load = load$2;

function FindUp(options) {
  var self = this;
  var fp = options.filePath;

  self.cache = {};
  self.cwd = options.cwd;
  self.detect = options.detect;
  self.names = options.names;
  self.create = options.create;

  if (fp) {
    self.givenFilePath = resolve$1(options.cwd, fp);
  }
}

function load$2(filePath, callback) {
  var self = this;
  var cache = self.cache;
  var givenFilePath = self.givenFilePath;
  var givenFile = self.givenFile;
  var names = self.names;
  var create = self.create;
  var cwd = self.cwd;
  var parent;

  if (givenFilePath) {
    if (givenFile) {
      apply(callback, givenFile);
    } else {
      givenFile = [callback];
      self.givenFile = givenFile;
      debug$2('Checking given file `%s`', givenFilePath);
      read(givenFilePath, loadGiven);
    }

    return;
  }

  if (!self.detect) {
    return callback();
  }

  filePath = resolve$1(cwd, filePath);
  parent = dirname(filePath);

  if (parent in cache) {
    apply(callback, cache[parent]);
  } else {
    cache[parent] = [callback];
    find(parent);
  }

  function loadGiven(err, buf) {
    var cbs = self.givenFile;
    var result;

    if (err) {
      result = fault_1('Cannot read given file `%s`\n%s', relative(cwd, givenFilePath), err.stack);
      result.code = 'ENOENT';
      result.path = err.path;
      result.syscall = err.syscall;
    } else {
      try {
        result = create(buf, givenFilePath);
        debug$2('Read given file `%s`', givenFilePath);
      } catch (err) {
        result = fault_1('Cannot parse given file `%s`\n%s', relative(cwd, givenFilePath), err.stack);
        debug$2(err.message);
      }
    }

    givenFile = result;
    self.givenFile = result;
    applyAll(cbs, result);
  }

  function find(directory) {
    var index = -1;
    var length = names.length;

    next();

    function next() {
      var parent;

      /* Try to read the next file. We don’t use `readdir` because on
       * huge directories, that could be *very* slow. */
      if (++index < length) {
        read(join$1(directory, names[index]), done);
      } else {
        parent = dirname(directory);

        if (directory === parent) {
          debug$2('No files found for `%s`', filePath);
          found();
        } else if (parent in cache) {
          apply(found, cache[parent]);
        } else {
          cache[parent] = [found];
          find(parent);
        }
      }
    }

    function done(err, buf) {
      var name = names[index];
      var fp = join$1(directory, name);
      var contents;

      /* istanbul ignore if - Hard to test. */
      if (err) {
        if (err.code === 'ENOENT') {
          return next();
        }

        err = fault_1('Cannot read file `%s`\n%s', relative(cwd, fp), err.message);
        debug$2(err.message);
        return found(err);
      }

      try {
        contents = create(buf, fp);
      } catch (err) {
        return found(fault_1('Cannot parse file `%s`\n%s', relative(cwd, fp), err.message));
      }

      /* istanbul ignore else - maybe used in the future. */
      if (contents) {
        debug$2('Read file `%s`', fp);
        found(null, contents);
      } else {
        next();
      }
    }

    function found(err, result) {
      var cbs = cache[directory];
      cache[directory] = err || result;
      applyAll(cbs, err || result);
    }
  }

  function applyAll(cbs, result) {
    var index = cbs.length;

    while (index--) {
      apply(cbs[index], result);
    }
  }

  function apply(cb, result) {
    if (isObject$1(result) && xIsFunction(result[0])) {
      result.push(cb);
    } else if (result instanceof Error) {
      cb(result);
    } else {
      cb(null, result);
    }
  }
}

var configuration = createCommonjsModule(function (module) {
var debug = src('unified-engine:configuration');
var resolve = loadPlugin_1.resolve;







module.exports = Config;

var own = {}.hasOwnProperty;
var extname = path.extname;
var basename = path.basename;
var dirname = path.dirname;
var relative = path.relative;

var loaders = {
  '.json': loadJSON,
  '.js': loadScript,
  '.yaml': loadYAML,
  '.yml': loadYAML
};

var defaultLoader = loadJSON;

Config.prototype.load = load;

function Config(options) {
  var rcName = options.rcName;
  var packageField = options.packageField;
  var names = [];

  this.cwd = options.cwd;
  this.packageField = options.packageField;
  this.pluginPrefix = options.pluginPrefix;
  this.configTransform = options.configTransform;
  this.defaultConfig = options.defaultConfig;

  if (rcName) {
    names.push(rcName, rcName + '.js', rcName + '.yml', rcName + '.yaml');
    debug('Looking for `%s` configuration files', names);
  }

  if (packageField) {
    names.push('package.json');
    debug('Looking for `%s` fields in `package.json` files', packageField);
  }

  this.given = {settings: options.settings, plugins: options.plugins};
  this.create = create.bind(this);

  this.findUp = new findUp({
    filePath: options.rcPath,
    cwd: options.cwd,
    detect: options.detectConfig,
    names: names,
    create: this.create
  });
}

function load(filePath, callback) {
  var searchPath = filePath || path.resolve(this.cwd, 'stdin.js');
  var self = this;

  self.findUp.load(searchPath, done);

  function done(err, res) {
    if (err || res) {
      return callback(err, res);
    }

    callback(null, self.create());
  }
}

function create(buf, filePath) {
  var self = this;
  var transform = self.configTransform;
  var defaults = self.defaultConfig;
  var fn = (filePath && loaders[extname(filePath)]) || defaultLoader;
  var options = {prefix: self.pluginPrefix, cwd: self.cwd};
  var result = {settings: {}, plugins: []};
  var contents = buf ? fn.apply(self, arguments) : undefined;

  if (transform && contents !== undefined) {
    contents = transform(contents, filePath);
  }

  /* Exit if we did find a `package.json`, but it doesn’t have configuration. */
  if (buf && contents === undefined && basename(filePath) === 'package.json') {
    return;
  }

  if (contents === undefined) {
    if (defaults) {
      merge(result, defaults, null, immutable(options, {root: self.cwd}));
    }
  } else {
    merge(result, contents, null, immutable(options, {root: dirname(filePath)}));
  }

  merge(result, self.given, null, immutable(options, {root: self.cwd}));

  return result;
}

/* Basically `Module.prototype.load`, but for a buffer instead
 * of a filepath. */
function loadScript(buf, filePath) {
  var submodule = module$1._cache[filePath];

  if (!submodule) {
    submodule = new module$1(filePath, module);
    submodule.filename = filePath;
    submodule.paths = module$1._nodeModulePaths(dirname(filePath));
    submodule._compile(String(buf), filePath);
    submodule.loaded = true;
    module$1._cache[filePath] = submodule;
  }

  return submodule.exports;
}

function loadYAML(buf, filePath) {
  return jsYaml$2.safeLoad(buf, {filename: basename(filePath)});
}

function loadJSON(buf, filePath) {
  var result = parseJson$1(buf, filePath);

  if (basename(filePath) === 'package.json') {
    result = result[this.packageField];
  }

  return result;
}

function merge(target, raw, val, options) {
  var root = options.root;
  var cwd = options.cwd;
  var prefix = options.prefix;

  if (isObject$1(raw)) {
    addPreset(raw);
  } else {
    throw new Error('Expected preset, not `' + raw + '`');
  }

  return target;

  function addPreset(result) {
    var plugins = result.plugins;

    if (plugins === null || plugins === undefined) {
      /* Empty. */
    } else if (isObject$1(plugins)) {
      if ('length' in plugins) {
        addEach(plugins);
      } else {
        addIn(plugins);
      }
    } else {
      throw new Error('Expected a list or object of plugins, not `' + plugins + '`');
    }

    target.settings = immutable(target.settings, result.settings);
  }

  function addEach(result) {
    var length = result.length;
    var index = -1;
    var value;

    while (++index < length) {
      value = result[index];

      if (isObject$1(value) && 'length' in value) {
        use.apply(null, value);
      } else {
        use(value);
      }
    }
  }

  function addIn(result) {
    var key;

    for (key in result) {
      use(key, result[key]);
    }
  }

  function use(usable, value) {
    if (xIsString(usable)) {
      addModule(usable, value);
    } else if (xIsFunction(usable)) {
      addPlugin(usable, value);
    } else {
      merge(target, usable, value, options);
    }
  }

  function addModule(id, value) {
    var fp = resolve(id, {cwd: root, prefix: prefix});
    var res;

    if (fp) {
      try {
        res = commonjsRequire(fp); // eslint-disable-line import/no-dynamic-require
      } catch (err) {
        throw fault_1('Cannot parse script `%s`\n%s', relative(root, fp), err.stack);
      }

      try {
        if (xIsFunction(res)) {
          addPlugin(res, value);
        } else {
          merge(target, res, value, immutable(options, {root: dirname(fp)}));
        }
      } catch (err) {
        throw fault_1('Error: Expected preset or plugin, not %s, at `%s`', res, relative(root, fp));
      }
    } else {
      fp = relative(cwd, path.resolve(root, id));
      addPlugin(failingModule(fp, new Error('Could not find module `' + id + '`')), value);
    }
  }

  function addPlugin(result, value) {
    var entry = find(target.plugins, result);

    if (entry) {
      reconfigure(entry, value);
    } else {
      target.plugins.push([result, value]);
    }
  }
}

function reconfigure(entry, value) {
  if (value !== false && entry[1] !== false && isObject$1(value)) {
    value = immutable(entry[1], value);
  }

  entry[1] = value;
}

function find(entries, plugin) {
  var length = entries.length;
  var index = -1;
  var entry;

  while (++index < length) {
    entry = entries[index];

    if (entry[0] === plugin) {
      return entry;
    }
  }
}

function failingModule(id, err) {
  var cache = failingModule.cache || (failingModule.cache = {});
  var submodule = own.call(cache, id) ? cache[id] : (cache[id] = fail);
  return submodule;
  function fail() {
    throw err;
  }
}
});

var configure_1 = configure;

function configure(context, settings) {
  context.configuration = new configuration(settings);
}

var _createClass = function () { function defineProperties(target, props) { for (var i = 0; i < props.length; i++) { var descriptor = props[i]; descriptor.enumerable = descriptor.enumerable || false; descriptor.configurable = true; if ("value" in descriptor) descriptor.writable = true; Object.defineProperty(target, descriptor.key, descriptor); } } return function (Constructor, protoProps, staticProps) { if (protoProps) defineProperties(Constructor.prototype, protoProps); if (staticProps) defineProperties(Constructor, staticProps); return Constructor; }; }();

function _classCallCheck(instance, Constructor) { if (!(instance instanceof Constructor)) { throw new TypeError("Cannot call a class as a function"); } }

var ignore = function () {
  return new IgnoreBase();
};

// A simple implementation of make-array
function make_array(subject) {
  return Array.isArray(subject) ? subject : [subject];
}

var REGEX_BLANK_LINE = /^\s+$/;
var REGEX_LEADING_EXCAPED_EXCLAMATION = /^\\\!/;
var REGEX_LEADING_EXCAPED_HASH = /^\\#/;
var SLASH = '/';
var KEY_IGNORE = typeof Symbol !== 'undefined' ? Symbol.for('node-ignore')
/* istanbul ignore next */
: 'node-ignore';

var IgnoreBase = function () {
  function IgnoreBase() {
    _classCallCheck(this, IgnoreBase);

    this._rules = [];
    this[KEY_IGNORE] = true;
    this._initCache();
  }

  _createClass(IgnoreBase, [{
    key: '_initCache',
    value: function _initCache() {
      this._cache = {};
    }

    // @param {Array.<string>|string|Ignore} pattern

  }, {
    key: 'add',
    value: function add(pattern) {
      this._added = false;

      if (typeof pattern === 'string') {
        pattern = pattern.split(/\r?\n/g);
      }

      make_array(pattern).forEach(this._addPattern, this);

      // Some rules have just added to the ignore,
      // making the behavior changed.
      if (this._added) {
        this._initCache();
      }

      return this;
    }

    // legacy

  }, {
    key: 'addPattern',
    value: function addPattern(pattern) {
      return this.add(pattern);
    }
  }, {
    key: '_addPattern',
    value: function _addPattern(pattern) {
      // #32
      if (pattern && pattern[KEY_IGNORE]) {
        this._rules = this._rules.concat(pattern._rules);
        this._added = true;
        return;
      }

      if (this._checkPattern(pattern)) {
        var rule = this._createRule(pattern);
        this._added = true;
        this._rules.push(rule);
      }
    }
  }, {
    key: '_checkPattern',
    value: function _checkPattern(pattern) {
      // > A blank line matches no files, so it can serve as a separator for readability.
      return pattern && typeof pattern === 'string' && !REGEX_BLANK_LINE.test(pattern)

      // > A line starting with # serves as a comment.
      && pattern.indexOf('#') !== 0;
    }
  }, {
    key: 'filter',
    value: function filter(paths) {
      var _this = this;

      return make_array(paths).filter(function (path$$1) {
        return _this._filter(path$$1);
      });
    }
  }, {
    key: 'createFilter',
    value: function createFilter() {
      var _this2 = this;

      return function (path$$1) {
        return _this2._filter(path$$1);
      };
    }
  }, {
    key: 'ignores',
    value: function ignores(path$$1) {
      return !this._filter(path$$1);
    }
  }, {
    key: '_createRule',
    value: function _createRule(pattern) {
      var origin = pattern;
      var negative = false;

      // > An optional prefix "!" which negates the pattern;
      if (pattern.indexOf('!') === 0) {
        negative = true;
        pattern = pattern.substr(1);
      }

      pattern = pattern
      // > Put a backslash ("\") in front of the first "!" for patterns that begin with a literal "!", for example, `"\!important!.txt"`.
      .replace(REGEX_LEADING_EXCAPED_EXCLAMATION, '!')
      // > Put a backslash ("\") in front of the first hash for patterns that begin with a hash.
      .replace(REGEX_LEADING_EXCAPED_HASH, '#');

      var regex = make_regex(pattern, negative);

      return {
        origin: origin,
        pattern: pattern,
        negative: negative,
        regex: regex
      };
    }

    // @returns `Boolean` true if the `path` is NOT ignored

  }, {
    key: '_filter',
    value: function _filter(path$$1, slices) {
      if (!path$$1) {
        return false;
      }

      if (path$$1 in this._cache) {
        return this._cache[path$$1];
      }

      if (!slices) {
        // path/to/a.js
        // ['path', 'to', 'a.js']
        slices = path$$1.split(SLASH);
      }

      slices.pop();

      return this._cache[path$$1] = slices.length
      // > It is not possible to re-include a file if a parent directory of that file is excluded.
      // If the path contains a parent directory, check the parent first
      ? this._filter(slices.join(SLASH) + SLASH, slices) && this._test(path$$1)

      // Or only test the path
      : this._test(path$$1);
    }

    // @returns {Boolean} true if a file is NOT ignored

  }, {
    key: '_test',
    value: function _test(path$$1) {
      // Explicitly define variable type by setting matched to `0`
      var matched = 0;

      this._rules.forEach(function (rule) {
        // if matched = true, then we only test negative rules
        // if matched = false, then we test non-negative rules
        if (!(matched ^ rule.negative)) {
          matched = rule.negative ^ rule.regex.test(path$$1);
        }
      });

      return !matched;
    }
  }]);

  return IgnoreBase;
}();

// > If the pattern ends with a slash,
// > it is removed for the purpose of the following description,
// > but it would only find a match with a directory.
// > In other words, foo/ will match a directory foo and paths underneath it,
// > but will not match a regular file or a symbolic link foo
// >  (this is consistent with the way how pathspec works in general in Git).
// '`foo/`' will not match regular file '`foo`' or symbolic link '`foo`'
// -> ignore-rules will not deal with it, because it costs extra `fs.stat` call
//      you could use option `mark: true` with `glob`

// '`foo/`' should not continue with the '`..`'


var DEFAULT_REPLACER_PREFIX = [

// > Trailing spaces are ignored unless they are quoted with backslash ("\")
[
// (a\ ) -> (a )
// (a  ) -> (a)
// (a \ ) -> (a  )
/\\?\s+$/, function (match) {
  return match.indexOf('\\') === 0 ? ' ' : '';
}],

// replace (\ ) with ' '
[/\\\s/g, function () {
  return ' ';
}],

// Escape metacharacters
// which is written down by users but means special for regular expressions.

// > There are 12 characters with special meanings:
// > - the backslash \,
// > - the caret ^,
// > - the dollar sign $,
// > - the period or dot .,
// > - the vertical bar or pipe symbol |,
// > - the question mark ?,
// > - the asterisk or star *,
// > - the plus sign +,
// > - the opening parenthesis (,
// > - the closing parenthesis ),
// > - and the opening square bracket [,
// > - the opening curly brace {,
// > These special characters are often called "metacharacters".
[/[\\\^$.|?*+()\[{]/g, function (match) {
  return '\\' + match;
}],

// leading slash
[

// > A leading slash matches the beginning of the pathname.
// > For example, "/*.c" matches "cat-file.c" but not "mozilla-sha1/sha1.c".
// A leading slash matches the beginning of the pathname
/^\//, function () {
  return '^';
}],

// replace special metacharacter slash after the leading slash
[/\//g, function () {
  return '\\/';
}], [
// > A leading "**" followed by a slash means match in all directories.
// > For example, "**/foo" matches file or directory "foo" anywhere,
// > the same as pattern "foo".
// > "**/foo/bar" matches file or directory "bar" anywhere that is directly under directory "foo".
// Notice that the '*'s have been replaced as '\\*'
/^\^*\\\*\\\*\\\//,

// '**/foo' <-> 'foo'
function () {
  return '^(?:.*\\/)?';
}]];

var DEFAULT_REPLACER_SUFFIX = [
// starting
[
// there will be no leading '/' (which has been replaced by section "leading slash")
// If starts with '**', adding a '^' to the regular expression also works
/^(?=[^\^])/, function () {
  return !/\/(?!$)/.test(this)
  // > If the pattern does not contain a slash /, Git treats it as a shell glob pattern
  // Actually, if there is only a trailing slash, git also treats it as a shell glob pattern
  ? '(?:^|\\/)'

  // > Otherwise, Git treats the pattern as a shell glob suitable for consumption by fnmatch(3)
  : '^';
}],

// two globstars
[
// Use lookahead assertions so that we could match more than one `'/**'`
/\\\/\\\*\\\*(?=\\\/|$)/g,

// Zero, one or several directories
// should not use '*', or it will be replaced by the next replacer

// Check if it is not the last `'/**'`
function (match, index, str) {
  return index + 6 < str.length

  // case: /**/
  // > A slash followed by two consecutive asterisks then a slash matches zero or more directories.
  // > For example, "a/**/b" matches "a/b", "a/x/b", "a/x/y/b" and so on.
  // '/**/'
  ? '(?:\\/[^\\/]+)*'

  // case: /**
  // > A trailing `"/**"` matches everything inside.

  // #21: everything inside but it should not include the current folder
  : '\\/.+';
}],

// intermediate wildcards
[
// Never replace escaped '*'
// ignore rule '\*' will match the path '*'

// 'abc.*/' -> go
// 'abc.*'  -> skip this rule
/(^|[^\\]+)\\\*(?=.+)/g,

// '*.js' matches '.js'
// '*.js' doesn't match 'abc'
function (match, p1) {
  return p1 + '[^\\/]*';
}],

// trailing wildcard
[/(\^|\\\/)?\\\*$/, function (match, p1) {
  return (p1
  // '\^':
  // '/*' does not match ''
  // '/*' does not match everything

  // '\\\/':
  // 'abc/*' does not match 'abc/'
  ? p1 + '[^/]+'

  // 'a*' matches 'a'
  // 'a*' matches 'aa'
  : '[^/]*') + '(?=$|\\/$)';
}], [
// unescape
/\\\\\\/g, function () {
  return '\\';
}]];

var POSITIVE_REPLACERS = [].concat(DEFAULT_REPLACER_PREFIX, [

// 'f'
// matches
// - /f(end)
// - /f/
// - (start)f(end)
// - (start)f/
// doesn't match
// - oof
// - foo
// pseudo:
// -> (^|/)f(/|$)

// ending
[
// 'js' will not match 'js.'
// 'ab' will not match 'abc'
/(?:[^*\/])$/,

// 'js*' will not match 'a.js'
// 'js/' will not match 'a.js'
// 'js' will match 'a.js' and 'a.js/'
function (match) {
  return match + '(?=$|\\/)';
}]], DEFAULT_REPLACER_SUFFIX);

var NEGATIVE_REPLACERS = [].concat(DEFAULT_REPLACER_PREFIX, [

// #24, #38
// The MISSING rule of [gitignore docs](https://git-scm.com/docs/gitignore)
// A negative pattern without a trailing wildcard should not
// re-include the things inside that directory.

// eg:
// ['node_modules/*', '!node_modules']
// should ignore `node_modules/a.js`
[/(?:[^*])$/, function (match) {
  return match + '(?=$|\\/$)';
}]], DEFAULT_REPLACER_SUFFIX);

// A simple cache, because an ignore rule only has only one certain meaning
var cache = {};

// @param {pattern}
function make_regex(pattern, negative) {
  var r = cache[pattern];
  if (r) {
    return r;
  }

  var replacers = negative ? NEGATIVE_REPLACERS : POSITIVE_REPLACERS;

  var source = replacers.reduce(function (prev, current) {
    return prev.replace(current[0], current[1].bind(pattern));
  }, pattern);

  return cache[pattern] = new RegExp(source, 'i');
}

// Windows
// --------------------------------------------------------------
/* istanbul ignore if  */
if (
// Detect `process` so that it can run in browsers.
typeof process !== 'undefined' && (process.env && process.env.IGNORE_TEST_WIN32 || process.platform === 'win32')) {

  var filter = IgnoreBase.prototype._filter;
  var make_posix = function make_posix(str) {
    return (/^\\\\\?\\/.test(str) || /[^\x00-\x80]+/.test(str) ? str : str.replace(/\\/g, '/')
    );
  };

  IgnoreBase.prototype._filter = function (path$$1, slices) {
    path$$1 = make_posix(path$$1);
    return filter.call(this, path$$1, slices);
  };
}

var ignore$2 = Ignore;

Ignore.prototype.check = check;

var dirname$1 = path.dirname;
var relative$1 = path.relative;
var resolve$2 = path.resolve;

function Ignore(options) {
  this.cwd = options.cwd;

  this.findUp = new findUp({
    filePath: options.ignorePath,
    cwd: options.cwd,
    detect: options.detectIgnore,
    names: options.ignoreName ? [options.ignoreName] : [],
    create: create$1
  });
}

function check(filePath, callback) {
  var self = this;

  self.findUp.load(filePath, done);

  function done(err, ignore$$1) {
    var normal;

    if (err) {
      callback(err);
    } else if (ignore$$1) {
      normal = relative$1(ignore$$1.filePath, resolve$2(self.cwd, filePath));
      callback(null, normal ? ignore$$1.ignores(normal) : false);
    } else {
      callback(null, false);
    }
  }
}

function create$1(buf, filePath) {
  var ignore$$1 = ignore().add(String(buf));
  ignore$$1.filePath = dirname$1(filePath);
  return ignore$$1;
}

// Copyright Joyent, Inc. and other Node contributors.
//
// Permission is hereby granted, free of charge, to any person obtaining a
// copy of this software and associated documentation files (the
// "Software"), to deal in the Software without restriction, including
// without limitation the rights to use, copy, modify, merge, publish,
// distribute, sublicense, and/or sell copies of the Software, and to permit
// persons to whom the Software is furnished to do so, subject to the
// following conditions:
//
// The above copyright notice and this permission notice shall be included
// in all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
// OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
// MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN
// NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
// DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR
// OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE
// USE OR OTHER DEALINGS IN THE SOFTWARE.


var isWindows = process.platform === 'win32';


// JavaScript implementation of realpath, ported from node pre-v6

var DEBUG = process.env.NODE_DEBUG && /fs/.test(process.env.NODE_DEBUG);

function rethrow() {
  // Only enable in debug mode. A backtrace uses ~1000 bytes of heap space and
  // is fairly slow to generate.
  var callback;
  if (DEBUG) {
    var backtrace = new Error;
    callback = debugCallback;
  } else
    callback = missingCallback;

  return callback;

  function debugCallback(err) {
    if (err) {
      backtrace.message = err.message;
      err = backtrace;
      missingCallback(err);
    }
  }

  function missingCallback(err) {
    if (err) {
      if (process.throwDeprecation)
        throw err;  // Forgot a callback but don't know where? Use NODE_DEBUG=fs
      else if (!process.noDeprecation) {
        var msg = 'fs: missing callback ' + (err.stack || err.message);
        if (process.traceDeprecation)
          console.trace(msg);
        else
          console.error(msg);
      }
    }
  }
}

function maybeCallback(cb) {
  return typeof cb === 'function' ? cb : rethrow();
}

var normalize = path.normalize;

// Regexp that finds the next partion of a (partial) path
// result is [base_with_slash, base], e.g. ['somedir/', 'somedir']
if (isWindows) {
  var nextPartRe = /(.*?)(?:[\/\\]+|$)/g;
} else {
  var nextPartRe = /(.*?)(?:[\/]+|$)/g;
}

// Regex to find the device root, including trailing slash. E.g. 'c:\\'.
if (isWindows) {
  var splitRootRe = /^(?:[a-zA-Z]:|[\\\/]{2}[^\\\/]+[\\\/][^\\\/]+)?[\\\/]*/;
} else {
  var splitRootRe = /^[\/]*/;
}

var realpathSync = function realpathSync(p, cache) {
  // make p is absolute
  p = path.resolve(p);

  if (cache && Object.prototype.hasOwnProperty.call(cache, p)) {
    return cache[p];
  }

  var original = p,
      seenLinks = {},
      knownHard = {};

  // current character position in p
  var pos;
  // the partial path so far, including a trailing slash if any
  var current;
  // the partial path without a trailing slash (except when pointing at a root)
  var base;
  // the partial path scanned in the previous round, with slash
  var previous;

  start();

  function start() {
    // Skip over roots
    var m = splitRootRe.exec(p);
    pos = m[0].length;
    current = m[0];
    base = m[0];
    previous = '';

    // On windows, check that the root exists. On unix there is no need.
    if (isWindows && !knownHard[base]) {
      fs.lstatSync(base);
      knownHard[base] = true;
    }
  }

  // walk down the path, swapping out linked pathparts for their real
  // values
  // NB: p.length changes.
  while (pos < p.length) {
    // find the next part
    nextPartRe.lastIndex = pos;
    var result = nextPartRe.exec(p);
    previous = current;
    current += result[0];
    base = previous + result[1];
    pos = nextPartRe.lastIndex;

    // continue if not a symlink
    if (knownHard[base] || (cache && cache[base] === base)) {
      continue;
    }

    var resolvedLink;
    if (cache && Object.prototype.hasOwnProperty.call(cache, base)) {
      // some known symbolic link.  no need to stat again.
      resolvedLink = cache[base];
    } else {
      var stat = fs.lstatSync(base);
      if (!stat.isSymbolicLink()) {
        knownHard[base] = true;
        if (cache) cache[base] = base;
        continue;
      }

      // read the link if it wasn't read before
      // dev/ino always return 0 on windows, so skip the check.
      var linkTarget = null;
      if (!isWindows) {
        var id = stat.dev.toString(32) + ':' + stat.ino.toString(32);
        if (seenLinks.hasOwnProperty(id)) {
          linkTarget = seenLinks[id];
        }
      }
      if (linkTarget === null) {
        fs.statSync(base);
        linkTarget = fs.readlinkSync(base);
      }
      resolvedLink = path.resolve(previous, linkTarget);
      // track this, if given a cache.
      if (cache) cache[base] = resolvedLink;
      if (!isWindows) seenLinks[id] = linkTarget;
    }

    // resolve the link, then start over
    p = path.resolve(resolvedLink, p.slice(pos));
    start();
  }

  if (cache) cache[original] = p;

  return p;
};


var realpath = function realpath(p, cache, cb) {
  if (typeof cb !== 'function') {
    cb = maybeCallback(cache);
    cache = null;
  }

  // make p is absolute
  p = path.resolve(p);

  if (cache && Object.prototype.hasOwnProperty.call(cache, p)) {
    return process.nextTick(cb.bind(null, null, cache[p]));
  }

  var original = p,
      seenLinks = {},
      knownHard = {};

  // current character position in p
  var pos;
  // the partial path so far, including a trailing slash if any
  var current;
  // the partial path without a trailing slash (except when pointing at a root)
  var base;
  // the partial path scanned in the previous round, with slash
  var previous;

  start();

  function start() {
    // Skip over roots
    var m = splitRootRe.exec(p);
    pos = m[0].length;
    current = m[0];
    base = m[0];
    previous = '';

    // On windows, check that the root exists. On unix there is no need.
    if (isWindows && !knownHard[base]) {
      fs.lstat(base, function(err) {
        if (err) return cb(err);
        knownHard[base] = true;
        LOOP();
      });
    } else {
      process.nextTick(LOOP);
    }
  }

  // walk down the path, swapping out linked pathparts for their real
  // values
  function LOOP() {
    // stop if scanned past end of path
    if (pos >= p.length) {
      if (cache) cache[original] = p;
      return cb(null, p);
    }

    // find the next part
    nextPartRe.lastIndex = pos;
    var result = nextPartRe.exec(p);
    previous = current;
    current += result[0];
    base = previous + result[1];
    pos = nextPartRe.lastIndex;

    // continue if not a symlink
    if (knownHard[base] || (cache && cache[base] === base)) {
      return process.nextTick(LOOP);
    }

    if (cache && Object.prototype.hasOwnProperty.call(cache, base)) {
      // known symbolic link.  no need to stat again.
      return gotResolvedLink(cache[base]);
    }

    return fs.lstat(base, gotStat);
  }

  function gotStat(err, stat) {
    if (err) return cb(err);

    // if not a symlink, skip to the next path part
    if (!stat.isSymbolicLink()) {
      knownHard[base] = true;
      if (cache) cache[base] = base;
      return process.nextTick(LOOP);
    }

    // stat & read the link if not read before
    // call gotTarget as soon as the link target is known
    // dev/ino always return 0 on windows, so skip the check.
    if (!isWindows) {
      var id = stat.dev.toString(32) + ':' + stat.ino.toString(32);
      if (seenLinks.hasOwnProperty(id)) {
        return gotTarget(null, seenLinks[id], base);
      }
    }
    fs.stat(base, function(err) {
      if (err) return cb(err);

      fs.readlink(base, function(err, target) {
        if (!isWindows) seenLinks[id] = target;
        gotTarget(err, target);
      });
    });
  }

  function gotTarget(err, target, base) {
    if (err) return cb(err);

    var resolvedLink = path.resolve(previous, target);
    if (cache) cache[base] = resolvedLink;
    gotResolvedLink(resolvedLink);
  }

  function gotResolvedLink(resolvedLink) {
    // resolve the link, then start over
    p = path.resolve(resolvedLink, p.slice(pos));
    start();
  }
};

var old = {
	realpathSync: realpathSync,
	realpath: realpath
};

var fs_realpath = realpath$1;
realpath$1.realpath = realpath$1;
realpath$1.sync = realpathSync$1;
realpath$1.realpathSync = realpathSync$1;
realpath$1.monkeypatch = monkeypatch;
realpath$1.unmonkeypatch = unmonkeypatch;


var origRealpath = fs.realpath;
var origRealpathSync = fs.realpathSync;

var version = process.version;
var ok = /^v[0-5]\./.test(version);


function newError (er) {
  return er && er.syscall === 'realpath' && (
    er.code === 'ELOOP' ||
    er.code === 'ENOMEM' ||
    er.code === 'ENAMETOOLONG'
  )
}

function realpath$1 (p, cache, cb) {
  if (ok) {
    return origRealpath(p, cache, cb)
  }

  if (typeof cache === 'function') {
    cb = cache;
    cache = null;
  }
  origRealpath(p, cache, function (er, result) {
    if (newError(er)) {
      old.realpath(p, cache, cb);
    } else {
      cb(er, result);
    }
  });
}

function realpathSync$1 (p, cache) {
  if (ok) {
    return origRealpathSync(p, cache)
  }

  try {
    return origRealpathSync(p, cache)
  } catch (er) {
    if (newError(er)) {
      return old.realpathSync(p, cache)
    } else {
      throw er
    }
  }
}

function monkeypatch () {
  fs.realpath = realpath$1;
  fs.realpathSync = realpathSync$1;
}

function unmonkeypatch () {
  fs.realpath = origRealpath;
  fs.realpathSync = origRealpathSync;
}

var concatMap = function (xs, fn) {
    var res = [];
    for (var i = 0; i < xs.length; i++) {
        var x = fn(xs[i], i);
        if (isArray(x)) res.push.apply(res, x);
        else res.push(x);
    }
    return res;
};

var isArray = Array.isArray || function (xs) {
    return Object.prototype.toString.call(xs) === '[object Array]';
};

var balancedMatch = balanced;
function balanced(a, b, str) {
  if (a instanceof RegExp) a = maybeMatch(a, str);
  if (b instanceof RegExp) b = maybeMatch(b, str);

  var r = range(a, b, str);

  return r && {
    start: r[0],
    end: r[1],
    pre: str.slice(0, r[0]),
    body: str.slice(r[0] + a.length, r[1]),
    post: str.slice(r[1] + b.length)
  };
}

function maybeMatch(reg, str) {
  var m = str.match(reg);
  return m ? m[0] : null;
}

balanced.range = range;
function range(a, b, str) {
  var begs, beg, left, right, result;
  var ai = str.indexOf(a);
  var bi = str.indexOf(b, ai + 1);
  var i = ai;

  if (ai >= 0 && bi > 0) {
    begs = [];
    left = str.length;

    while (i >= 0 && !result) {
      if (i == ai) {
        begs.push(i);
        ai = str.indexOf(a, i + 1);
      } else if (begs.length == 1) {
        result = [ begs.pop(), bi ];
      } else {
        beg = begs.pop();
        if (beg < left) {
          left = beg;
          right = bi;
        }

        bi = str.indexOf(b, i + 1);
      }

      i = ai < bi && ai >= 0 ? ai : bi;
    }

    if (begs.length) {
      result = [ left, right ];
    }
  }

  return result;
}

var braceExpansion = expandTop;

var escSlash = '\0SLASH'+Math.random()+'\0';
var escOpen = '\0OPEN'+Math.random()+'\0';
var escClose = '\0CLOSE'+Math.random()+'\0';
var escComma = '\0COMMA'+Math.random()+'\0';
var escPeriod = '\0PERIOD'+Math.random()+'\0';

function numeric(str) {
  return parseInt(str, 10) == str
    ? parseInt(str, 10)
    : str.charCodeAt(0);
}

function escapeBraces(str) {
  return str.split('\\\\').join(escSlash)
            .split('\\{').join(escOpen)
            .split('\\}').join(escClose)
            .split('\\,').join(escComma)
            .split('\\.').join(escPeriod);
}

function unescapeBraces(str) {
  return str.split(escSlash).join('\\')
            .split(escOpen).join('{')
            .split(escClose).join('}')
            .split(escComma).join(',')
            .split(escPeriod).join('.');
}


// Basically just str.split(","), but handling cases
// where we have nested braced sections, which should be
// treated as individual members, like {a,{b,c},d}
function parseCommaParts(str) {
  if (!str)
    return [''];

  var parts = [];
  var m = balancedMatch('{', '}', str);

  if (!m)
    return str.split(',');

  var pre = m.pre;
  var body = m.body;
  var post = m.post;
  var p = pre.split(',');

  p[p.length-1] += '{' + body + '}';
  var postParts = parseCommaParts(post);
  if (post.length) {
    p[p.length-1] += postParts.shift();
    p.push.apply(p, postParts);
  }

  parts.push.apply(parts, p);

  return parts;
}

function expandTop(str) {
  if (!str)
    return [];

  // I don't know why Bash 4.3 does this, but it does.
  // Anything starting with {} will have the first two bytes preserved
  // but *only* at the top level, so {},a}b will not expand to anything,
  // but a{},b}c will be expanded to [a}c,abc].
  // One could argue that this is a bug in Bash, but since the goal of
  // this module is to match Bash's rules, we escape a leading {}
  if (str.substr(0, 2) === '{}') {
    str = '\\{\\}' + str.substr(2);
  }

  return expand(escapeBraces(str), true).map(unescapeBraces);
}

function embrace(str) {
  return '{' + str + '}';
}
function isPadded(el) {
  return /^-?0\d/.test(el);
}

function lte(i, y) {
  return i <= y;
}
function gte(i, y) {
  return i >= y;
}

function expand(str, isTop) {
  var expansions = [];

  var m = balancedMatch('{', '}', str);
  if (!m || /\$$/.test(m.pre)) return [str];

  var isNumericSequence = /^-?\d+\.\.-?\d+(?:\.\.-?\d+)?$/.test(m.body);
  var isAlphaSequence = /^[a-zA-Z]\.\.[a-zA-Z](?:\.\.-?\d+)?$/.test(m.body);
  var isSequence = isNumericSequence || isAlphaSequence;
  var isOptions = m.body.indexOf(',') >= 0;
  if (!isSequence && !isOptions) {
    // {a},b}
    if (m.post.match(/,.*\}/)) {
      str = m.pre + '{' + m.body + escClose + m.post;
      return expand(str);
    }
    return [str];
  }

  var n;
  if (isSequence) {
    n = m.body.split(/\.\./);
  } else {
    n = parseCommaParts(m.body);
    if (n.length === 1) {
      // x{{a,b}}y ==> x{a}y x{b}y
      n = expand(n[0], false).map(embrace);
      if (n.length === 1) {
        var post = m.post.length
          ? expand(m.post, false)
          : [''];
        return post.map(function(p) {
          return m.pre + n[0] + p;
        });
      }
    }
  }

  // at this point, n is the parts, and we know it's not a comma set
  // with a single entry.

  // no need to expand pre, since it is guaranteed to be free of brace-sets
  var pre = m.pre;
  var post = m.post.length
    ? expand(m.post, false)
    : [''];

  var N;

  if (isSequence) {
    var x = numeric(n[0]);
    var y = numeric(n[1]);
    var width = Math.max(n[0].length, n[1].length);
    var incr = n.length == 3
      ? Math.abs(numeric(n[2]))
      : 1;
    var test = lte;
    var reverse = y < x;
    if (reverse) {
      incr *= -1;
      test = gte;
    }
    var pad = n.some(isPadded);

    N = [];

    for (var i = x; test(i, y); i += incr) {
      var c;
      if (isAlphaSequence) {
        c = String.fromCharCode(i);
        if (c === '\\')
          c = '';
      } else {
        c = String(i);
        if (pad) {
          var need = width - c.length;
          if (need > 0) {
            var z = new Array(need + 1).join('0');
            if (i < 0)
              c = '-' + z + c.slice(1);
            else
              c = z + c;
          }
        }
      }
      N.push(c);
    }
  } else {
    N = concatMap(n, function(el) { return expand(el, false) });
  }

  for (var j = 0; j < N.length; j++) {
    for (var k = 0; k < post.length; k++) {
      var expansion = pre + N[j] + post[k];
      if (!isTop || isSequence || expansion)
        expansions.push(expansion);
    }
  }

  return expansions;
}

var minimatch_1 = minimatch;
minimatch.Minimatch = Minimatch;

var path$2 = { sep: '/' };
try {
  path$2 = path;
} catch (er) {}

var GLOBSTAR = minimatch.GLOBSTAR = Minimatch.GLOBSTAR = {};


var plTypes = {
  '!': { open: '(?:(?!(?:', close: '))[^/]*?)'},
  '?': { open: '(?:', close: ')?' },
  '+': { open: '(?:', close: ')+' },
  '*': { open: '(?:', close: ')*' },
  '@': { open: '(?:', close: ')' }
};

// any single thing other than /
// don't need to escape / when using new RegExp()
var qmark = '[^/]';

// * => any number of characters
var star = qmark + '*?';

// ** when dots are allowed.  Anything goes, except .. and .
// not (^ or / followed by one or two dots followed by $ or /),
// followed by anything, any number of times.
var twoStarDot = '(?:(?!(?:\\\/|^)(?:\\.{1,2})($|\\\/)).)*?';

// not a ^ or / followed by a dot,
// followed by anything, any number of times.
var twoStarNoDot = '(?:(?!(?:\\\/|^)\\.).)*?';

// characters that need to be escaped in RegExp.
var reSpecials = charSet('().*{}+?[]^$\\!');

// "abc" -> { a:true, b:true, c:true }
function charSet (s) {
  return s.split('').reduce(function (set, c) {
    set[c] = true;
    return set
  }, {})
}

// normalizes slashes.
var slashSplit = /\/+/;

minimatch.filter = filter$1;
function filter$1 (pattern, options) {
  options = options || {};
  return function (p, i, list) {
    return minimatch(p, pattern, options)
  }
}

function ext (a, b) {
  a = a || {};
  b = b || {};
  var t = {};
  Object.keys(b).forEach(function (k) {
    t[k] = b[k];
  });
  Object.keys(a).forEach(function (k) {
    t[k] = a[k];
  });
  return t
}

minimatch.defaults = function (def) {
  if (!def || !Object.keys(def).length) return minimatch

  var orig = minimatch;

  var m = function minimatch (p, pattern, options) {
    return orig.minimatch(p, pattern, ext(def, options))
  };

  m.Minimatch = function Minimatch (pattern, options) {
    return new orig.Minimatch(pattern, ext(def, options))
  };

  return m
};

Minimatch.defaults = function (def) {
  if (!def || !Object.keys(def).length) return Minimatch
  return minimatch.defaults(def).Minimatch
};

function minimatch (p, pattern, options) {
  if (typeof pattern !== 'string') {
    throw new TypeError('glob pattern string required')
  }

  if (!options) options = {};

  // shortcut: comments match nothing.
  if (!options.nocomment && pattern.charAt(0) === '#') {
    return false
  }

  // "" only matches ""
  if (pattern.trim() === '') return p === ''

  return new Minimatch(pattern, options).match(p)
}

function Minimatch (pattern, options) {
  if (!(this instanceof Minimatch)) {
    return new Minimatch(pattern, options)
  }

  if (typeof pattern !== 'string') {
    throw new TypeError('glob pattern string required')
  }

  if (!options) options = {};
  pattern = pattern.trim();

  // windows support: need to use /, not \
  if (path$2.sep !== '/') {
    pattern = pattern.split(path$2.sep).join('/');
  }

  this.options = options;
  this.set = [];
  this.pattern = pattern;
  this.regexp = null;
  this.negate = false;
  this.comment = false;
  this.empty = false;

  // make the set of regexps etc.
  this.make();
}

Minimatch.prototype.debug = function () {};

Minimatch.prototype.make = make;
function make () {
  // don't do it more than once.
  if (this._made) return

  var pattern = this.pattern;
  var options = this.options;

  // empty patterns and comments match nothing.
  if (!options.nocomment && pattern.charAt(0) === '#') {
    this.comment = true;
    return
  }
  if (!pattern) {
    this.empty = true;
    return
  }

  // step 1: figure out negation, etc.
  this.parseNegate();

  // step 2: expand braces
  var set = this.globSet = this.braceExpand();

  if (options.debug) this.debug = console.error;

  this.debug(this.pattern, set);

  // step 3: now we have a set, so turn each one into a series of path-portion
  // matching patterns.
  // These will be regexps, except in the case of "**", which is
  // set to the GLOBSTAR object for globstar behavior,
  // and will not contain any / characters
  set = this.globParts = set.map(function (s) {
    return s.split(slashSplit)
  });

  this.debug(this.pattern, set);

  // glob --> regexps
  set = set.map(function (s, si, set) {
    return s.map(this.parse, this)
  }, this);

  this.debug(this.pattern, set);

  // filter out everything that didn't compile properly.
  set = set.filter(function (s) {
    return s.indexOf(false) === -1
  });

  this.debug(this.pattern, set);

  this.set = set;
}

Minimatch.prototype.parseNegate = parseNegate;
function parseNegate () {
  var pattern = this.pattern;
  var negate = false;
  var options = this.options;
  var negateOffset = 0;

  if (options.nonegate) return

  for (var i = 0, l = pattern.length
    ; i < l && pattern.charAt(i) === '!'
    ; i++) {
    negate = !negate;
    negateOffset++;
  }

  if (negateOffset) this.pattern = pattern.substr(negateOffset);
  this.negate = negate;
}

// Brace expansion:
// a{b,c}d -> abd acd
// a{b,}c -> abc ac
// a{0..3}d -> a0d a1d a2d a3d
// a{b,c{d,e}f}g -> abg acdfg acefg
// a{b,c}d{e,f}g -> abdeg acdeg abdeg abdfg
//
// Invalid sets are not expanded.
// a{2..}b -> a{2..}b
// a{b}c -> a{b}c
minimatch.braceExpand = function (pattern, options) {
  return braceExpand(pattern, options)
};

Minimatch.prototype.braceExpand = braceExpand;

function braceExpand (pattern, options) {
  if (!options) {
    if (this instanceof Minimatch) {
      options = this.options;
    } else {
      options = {};
    }
  }

  pattern = typeof pattern === 'undefined'
    ? this.pattern : pattern;

  if (typeof pattern === 'undefined') {
    throw new TypeError('undefined pattern')
  }

  if (options.nobrace ||
    !pattern.match(/\{.*\}/)) {
    // shortcut. no need to expand.
    return [pattern]
  }

  return braceExpansion(pattern)
}

// parse a component of the expanded set.
// At this point, no pattern may contain "/" in it
// so we're going to return a 2d array, where each entry is the full
// pattern, split on '/', and then turned into a regular expression.
// A regexp is made at the end which joins each array with an
// escaped /, and another full one which joins each regexp with |.
//
// Following the lead of Bash 4.1, note that "**" only has special meaning
// when it is the *only* thing in a path portion.  Otherwise, any series
// of * is equivalent to a single *.  Globstar behavior is enabled by
// default, and can be disabled by setting options.noglobstar.
Minimatch.prototype.parse = parse$2;
var SUBPARSE = {};
function parse$2 (pattern, isSub) {
  if (pattern.length > 1024 * 64) {
    throw new TypeError('pattern is too long')
  }

  var options = this.options;

  // shortcuts
  if (!options.noglobstar && pattern === '**') return GLOBSTAR
  if (pattern === '') return ''

  var re = '';
  var hasMagic = !!options.nocase;
  var escaping = false;
  // ? => one single character
  var patternListStack = [];
  var negativeLists = [];
  var stateChar;
  var inClass = false;
  var reClassStart = -1;
  var classStart = -1;
  // . and .. never match anything that doesn't start with .,
  // even when options.dot is set.
  var patternStart = pattern.charAt(0) === '.' ? '' // anything
  // not (start or / followed by . or .. followed by / or end)
  : options.dot ? '(?!(?:^|\\\/)\\.{1,2}(?:$|\\\/))'
  : '(?!\\.)';
  var self = this;

  function clearStateChar () {
    if (stateChar) {
      // we had some state-tracking character
      // that wasn't consumed by this pass.
      switch (stateChar) {
        case '*':
          re += star;
          hasMagic = true;
        break
        case '?':
          re += qmark;
          hasMagic = true;
        break
        default:
          re += '\\' + stateChar;
        break
      }
      self.debug('clearStateChar %j %j', stateChar, re);
      stateChar = false;
    }
  }

  for (var i = 0, len = pattern.length, c
    ; (i < len) && (c = pattern.charAt(i))
    ; i++) {
    this.debug('%s\t%s %s %j', pattern, i, re, c);

    // skip over any that are escaped.
    if (escaping && reSpecials[c]) {
      re += '\\' + c;
      escaping = false;
      continue
    }

    switch (c) {
      case '/':
        // completely not allowed, even escaped.
        // Should already be path-split by now.
        return false

      case '\\':
        clearStateChar();
        escaping = true;
      continue

      // the various stateChar values
      // for the "extglob" stuff.
      case '?':
      case '*':
      case '+':
      case '@':
      case '!':
        this.debug('%s\t%s %s %j <-- stateChar', pattern, i, re, c);

        // all of those are literals inside a class, except that
        // the glob [!a] means [^a] in regexp
        if (inClass) {
          this.debug('  in class');
          if (c === '!' && i === classStart + 1) c = '^';
          re += c;
          continue
        }

        // if we already have a stateChar, then it means
        // that there was something like ** or +? in there.
        // Handle the stateChar, then proceed with this one.
        self.debug('call clearStateChar %j', stateChar);
        clearStateChar();
        stateChar = c;
        // if extglob is disabled, then +(asdf|foo) isn't a thing.
        // just clear the statechar *now*, rather than even diving into
        // the patternList stuff.
        if (options.noext) clearStateChar();
      continue

      case '(':
        if (inClass) {
          re += '(';
          continue
        }

        if (!stateChar) {
          re += '\\(';
          continue
        }

        patternListStack.push({
          type: stateChar,
          start: i - 1,
          reStart: re.length,
          open: plTypes[stateChar].open,
          close: plTypes[stateChar].close
        });
        // negation is (?:(?!js)[^/]*)
        re += stateChar === '!' ? '(?:(?!(?:' : '(?:';
        this.debug('plType %j %j', stateChar, re);
        stateChar = false;
      continue

      case ')':
        if (inClass || !patternListStack.length) {
          re += '\\)';
          continue
        }

        clearStateChar();
        hasMagic = true;
        var pl = patternListStack.pop();
        // negation is (?:(?!js)[^/]*)
        // The others are (?:<pattern>)<type>
        re += pl.close;
        if (pl.type === '!') {
          negativeLists.push(pl);
        }
        pl.reEnd = re.length;
      continue

      case '|':
        if (inClass || !patternListStack.length || escaping) {
          re += '\\|';
          escaping = false;
          continue
        }

        clearStateChar();
        re += '|';
      continue

      // these are mostly the same in regexp and glob
      case '[':
        // swallow any state-tracking char before the [
        clearStateChar();

        if (inClass) {
          re += '\\' + c;
          continue
        }

        inClass = true;
        classStart = i;
        reClassStart = re.length;
        re += c;
      continue

      case ']':
        //  a right bracket shall lose its special
        //  meaning and represent itself in
        //  a bracket expression if it occurs
        //  first in the list.  -- POSIX.2 2.8.3.2
        if (i === classStart + 1 || !inClass) {
          re += '\\' + c;
          escaping = false;
          continue
        }

        // handle the case where we left a class open.
        // "[z-a]" is valid, equivalent to "\[z-a\]"
        if (inClass) {
          // split where the last [ was, make sure we don't have
          // an invalid re. if so, re-walk the contents of the
          // would-be class to re-translate any characters that
          // were passed through as-is
          // TODO: It would probably be faster to determine this
          // without a try/catch and a new RegExp, but it's tricky
          // to do safely.  For now, this is safe and works.
          var cs = pattern.substring(classStart + 1, i);
          try {

          } catch (er) {
            // not a valid class!
            var sp = this.parse(cs, SUBPARSE);
            re = re.substr(0, reClassStart) + '\\[' + sp[0] + '\\]';
            hasMagic = hasMagic || sp[1];
            inClass = false;
            continue
          }
        }

        // finish up the class.
        hasMagic = true;
        inClass = false;
        re += c;
      continue

      default:
        // swallow any state char that wasn't consumed
        clearStateChar();

        if (escaping) {
          // no need
          escaping = false;
        } else if (reSpecials[c]
          && !(c === '^' && inClass)) {
          re += '\\';
        }

        re += c;

    } // switch
  } // for

  // handle the case where we left a class open.
  // "[abc" is valid, equivalent to "\[abc"
  if (inClass) {
    // split where the last [ was, and escape it
    // this is a huge pita.  We now have to re-walk
    // the contents of the would-be class to re-translate
    // any characters that were passed through as-is
    cs = pattern.substr(classStart + 1);
    sp = this.parse(cs, SUBPARSE);
    re = re.substr(0, reClassStart) + '\\[' + sp[0];
    hasMagic = hasMagic || sp[1];
  }

  // handle the case where we had a +( thing at the *end*
  // of the pattern.
  // each pattern list stack adds 3 chars, and we need to go through
  // and escape any | chars that were passed through as-is for the regexp.
  // Go through and escape them, taking care not to double-escape any
  // | chars that were already escaped.
  for (pl = patternListStack.pop(); pl; pl = patternListStack.pop()) {
    var tail = re.slice(pl.reStart + pl.open.length);
    this.debug('setting tail', re, pl);
    // maybe some even number of \, then maybe 1 \, followed by a |
    tail = tail.replace(/((?:\\{2}){0,64})(\\?)\|/g, function (_, $1, $2) {
      if (!$2) {
        // the | isn't already escaped, so escape it.
        $2 = '\\';
      }

      // need to escape all those slashes *again*, without escaping the
      // one that we need for escaping the | character.  As it works out,
      // escaping an even number of slashes can be done by simply repeating
      // it exactly after itself.  That's why this trick works.
      //
      // I am sorry that you have to see this.
      return $1 + $1 + $2 + '|'
    });

    this.debug('tail=%j\n   %s', tail, tail, pl, re);
    var t = pl.type === '*' ? star
      : pl.type === '?' ? qmark
      : '\\' + pl.type;

    hasMagic = true;
    re = re.slice(0, pl.reStart) + t + '\\(' + tail;
  }

  // handle trailing things that only matter at the very end.
  clearStateChar();
  if (escaping) {
    // trailing \\
    re += '\\\\';
  }

  // only need to apply the nodot start if the re starts with
  // something that could conceivably capture a dot
  var addPatternStart = false;
  switch (re.charAt(0)) {
    case '.':
    case '[':
    case '(': addPatternStart = true;
  }

  // Hack to work around lack of negative lookbehind in JS
  // A pattern like: *.!(x).!(y|z) needs to ensure that a name
  // like 'a.xyz.yz' doesn't match.  So, the first negative
  // lookahead, has to look ALL the way ahead, to the end of
  // the pattern.
  for (var n = negativeLists.length - 1; n > -1; n--) {
    var nl = negativeLists[n];

    var nlBefore = re.slice(0, nl.reStart);
    var nlFirst = re.slice(nl.reStart, nl.reEnd - 8);
    var nlLast = re.slice(nl.reEnd - 8, nl.reEnd);
    var nlAfter = re.slice(nl.reEnd);

    nlLast += nlAfter;

    // Handle nested stuff like *(*.js|!(*.json)), where open parens
    // mean that we should *not* include the ) in the bit that is considered
    // "after" the negated section.
    var openParensBefore = nlBefore.split('(').length - 1;
    var cleanAfter = nlAfter;
    for (i = 0; i < openParensBefore; i++) {
      cleanAfter = cleanAfter.replace(/\)[+*?]?/, '');
    }
    nlAfter = cleanAfter;

    var dollar = '';
    if (nlAfter === '' && isSub !== SUBPARSE) {
      dollar = '$';
    }
    var newRe = nlBefore + nlFirst + nlAfter + dollar + nlLast;
    re = newRe;
  }

  // if the re is not "" at this point, then we need to make sure
  // it doesn't match against an empty path part.
  // Otherwise a/* will match a/, which it should not.
  if (re !== '' && hasMagic) {
    re = '(?=.)' + re;
  }

  if (addPatternStart) {
    re = patternStart + re;
  }

  // parsing just a piece of a larger pattern.
  if (isSub === SUBPARSE) {
    return [re, hasMagic]
  }

  // skip the regexp for non-magical patterns
  // unescape anything in it, though, so that it'll be
  // an exact match against a file etc.
  if (!hasMagic) {
    return globUnescape(pattern)
  }

  var flags = options.nocase ? 'i' : '';
  try {
    var regExp = new RegExp('^' + re + '$', flags);
  } catch (er) {
    // If it was an invalid regular expression, then it can't match
    // anything.  This trick looks for a character after the end of
    // the string, which is of course impossible, except in multi-line
    // mode, but it's not a /m regex.
    return new RegExp('$.')
  }

  regExp._glob = pattern;
  regExp._src = re;

  return regExp
}

minimatch.makeRe = function (pattern, options) {
  return new Minimatch(pattern, options || {}).makeRe()
};

Minimatch.prototype.makeRe = makeRe;
function makeRe () {
  if (this.regexp || this.regexp === false) return this.regexp

  // at this point, this.set is a 2d array of partial
  // pattern strings, or "**".
  //
  // It's better to use .match().  This function shouldn't
  // be used, really, but it's pretty convenient sometimes,
  // when you just want to work with a regex.
  var set = this.set;

  if (!set.length) {
    this.regexp = false;
    return this.regexp
  }
  var options = this.options;

  var twoStar = options.noglobstar ? star
    : options.dot ? twoStarDot
    : twoStarNoDot;
  var flags = options.nocase ? 'i' : '';

  var re = set.map(function (pattern) {
    return pattern.map(function (p) {
      return (p === GLOBSTAR) ? twoStar
      : (typeof p === 'string') ? regExpEscape(p)
      : p._src
    }).join('\\\/')
  }).join('|');

  // must match entire pattern
  // ending in a * or ** will make it less strict.
  re = '^(?:' + re + ')$';

  // can match anything, as long as it's not this.
  if (this.negate) re = '^(?!' + re + ').*$';

  try {
    this.regexp = new RegExp(re, flags);
  } catch (ex) {
    this.regexp = false;
  }
  return this.regexp
}

minimatch.match = function (list, pattern, options) {
  options = options || {};
  var mm = new Minimatch(pattern, options);
  list = list.filter(function (f) {
    return mm.match(f)
  });
  if (mm.options.nonull && !list.length) {
    list.push(pattern);
  }
  return list
};

Minimatch.prototype.match = match;
function match (f, partial) {
  this.debug('match', f, this.pattern);
  // short-circuit in the case of busted things.
  // comments, etc.
  if (this.comment) return false
  if (this.empty) return f === ''

  if (f === '/' && partial) return true

  var options = this.options;

  // windows: need to use /, not \
  if (path$2.sep !== '/') {
    f = f.split(path$2.sep).join('/');
  }

  // treat the test path as a set of pathparts.
  f = f.split(slashSplit);
  this.debug(this.pattern, 'split', f);

  // just ONE of the pattern sets in this.set needs to match
  // in order for it to be valid.  If negating, then just one
  // match means that we have failed.
  // Either way, return on the first hit.

  var set = this.set;
  this.debug(this.pattern, 'set', set);

  // Find the basename of the path by looking for the last non-empty segment
  var filename;
  var i;
  for (i = f.length - 1; i >= 0; i--) {
    filename = f[i];
    if (filename) break
  }

  for (i = 0; i < set.length; i++) {
    var pattern = set[i];
    var file = f;
    if (options.matchBase && pattern.length === 1) {
      file = [filename];
    }
    var hit = this.matchOne(file, pattern, partial);
    if (hit) {
      if (options.flipNegate) return true
      return !this.negate
    }
  }

  // didn't get any hits.  this is success if it's a negative
  // pattern, failure otherwise.
  if (options.flipNegate) return false
  return this.negate
}

// set partial to true to test if, for example,
// "/a/b" matches the start of "/*/b/*/d"
// Partial means, if you run out of file before you run
// out of pattern, then that's fine, as long as all
// the parts match.
Minimatch.prototype.matchOne = function (file, pattern, partial) {
  var options = this.options;

  this.debug('matchOne',
    { 'this': this, file: file, pattern: pattern });

  this.debug('matchOne', file.length, pattern.length);

  for (var fi = 0,
      pi = 0,
      fl = file.length,
      pl = pattern.length
      ; (fi < fl) && (pi < pl)
      ; fi++, pi++) {
    this.debug('matchOne loop');
    var p = pattern[pi];
    var f = file[fi];

    this.debug(pattern, p, f);

    // should be impossible.
    // some invalid regexp stuff in the set.
    if (p === false) return false

    if (p === GLOBSTAR) {
      this.debug('GLOBSTAR', [pattern, p, f]);

      // "**"
      // a/**/b/**/c would match the following:
      // a/b/x/y/z/c
      // a/x/y/z/b/c
      // a/b/x/b/x/c
      // a/b/c
      // To do this, take the rest of the pattern after
      // the **, and see if it would match the file remainder.
      // If so, return success.
      // If not, the ** "swallows" a segment, and try again.
      // This is recursively awful.
      //
      // a/**/b/**/c matching a/b/x/y/z/c
      // - a matches a
      // - doublestar
      //   - matchOne(b/x/y/z/c, b/**/c)
      //     - b matches b
      //     - doublestar
      //       - matchOne(x/y/z/c, c) -> no
      //       - matchOne(y/z/c, c) -> no
      //       - matchOne(z/c, c) -> no
      //       - matchOne(c, c) yes, hit
      var fr = fi;
      var pr = pi + 1;
      if (pr === pl) {
        this.debug('** at the end');
        // a ** at the end will just swallow the rest.
        // We have found a match.
        // however, it will not swallow /.x, unless
        // options.dot is set.
        // . and .. are *never* matched by **, for explosively
        // exponential reasons.
        for (; fi < fl; fi++) {
          if (file[fi] === '.' || file[fi] === '..' ||
            (!options.dot && file[fi].charAt(0) === '.')) return false
        }
        return true
      }

      // ok, let's see if we can swallow whatever we can.
      while (fr < fl) {
        var swallowee = file[fr];

        this.debug('\nglobstar while', file, fr, pattern, pr, swallowee);

        // XXX remove this slice.  Just pass the start index.
        if (this.matchOne(file.slice(fr), pattern.slice(pr), partial)) {
          this.debug('globstar found match!', fr, fl, swallowee);
          // found a match.
          return true
        } else {
          // can't swallow "." or ".." ever.
          // can only swallow ".foo" when explicitly asked.
          if (swallowee === '.' || swallowee === '..' ||
            (!options.dot && swallowee.charAt(0) === '.')) {
            this.debug('dot detected!', file, fr, pattern, pr);
            break
          }

          // ** swallows a segment, and continue.
          this.debug('globstar swallow a segment, and continue');
          fr++;
        }
      }

      // no match was found.
      // However, in partial mode, we can't say this is necessarily over.
      // If there's more *pattern* left, then
      if (partial) {
        // ran out of file
        this.debug('\n>>> no match, partial?', file, fr, pattern, pr);
        if (fr === fl) return true
      }
      return false
    }

    // something other than **
    // non-magic patterns just have to match exactly
    // patterns with magic have been turned into regexps.
    var hit;
    if (typeof p === 'string') {
      if (options.nocase) {
        hit = f.toLowerCase() === p.toLowerCase();
      } else {
        hit = f === p;
      }
      this.debug('string match', p, f, hit);
    } else {
      hit = f.match(p);
      this.debug('pattern match', p, f, hit);
    }

    if (!hit) return false
  }

  // Note: ending in / means that we'll get a final ""
  // at the end of the pattern.  This can only match a
  // corresponding "" at the end of the file.
  // If the file ends in /, then it can only match a
  // a pattern that ends in /, unless the pattern just
  // doesn't have any more for it. But, a/b/ should *not*
  // match "a/b/*", even though "" matches against the
  // [^/]*? pattern, except in partial mode, where it might
  // simply not be reached yet.
  // However, a/b/ should still satisfy a/*

  // now either we fell off the end of the pattern, or we're done.
  if (fi === fl && pi === pl) {
    // ran out of pattern and filename at the same time.
    // an exact hit!
    return true
  } else if (fi === fl) {
    // ran out of file, but still had pattern left.
    // this is ok if we're doing the match as part of
    // a glob fs traversal.
    return partial
  } else if (pi === pl) {
    // ran out of pattern, still have file left.
    // this is only acceptable if we're on the very last
    // empty segment of a file with a trailing slash.
    // a/* should match a/b/
    var emptyFileEnd = (fi === fl - 1) && (file[fi] === '');
    return emptyFileEnd
  }

  // should be unreachable.
  throw new Error('wtf?')
};

// replace stuff like \* with *
function globUnescape (s) {
  return s.replace(/\\(.)/g, '$1')
}

function regExpEscape (s) {
  return s.replace(/[-[\]{}()*+?.,\\^$|#\s]/g, '\\$&')
}

var inherits_browser = createCommonjsModule(function (module) {
if (typeof Object.create === 'function') {
  // implementation from standard node.js 'util' module
  module.exports = function inherits(ctor, superCtor) {
    ctor.super_ = superCtor;
    ctor.prototype = Object.create(superCtor.prototype, {
      constructor: {
        value: ctor,
        enumerable: false,
        writable: true,
        configurable: true
      }
    });
  };
} else {
  // old school shim for old browsers
  module.exports = function inherits(ctor, superCtor) {
    ctor.super_ = superCtor;
    var TempCtor = function () {};
    TempCtor.prototype = superCtor.prototype;
    ctor.prototype = new TempCtor();
    ctor.prototype.constructor = ctor;
  };
}
});

var inherits = createCommonjsModule(function (module) {
try {
  var util$$1 = util;
  if (typeof util$$1.inherits !== 'function') throw '';
  module.exports = util$$1.inherits;
} catch (e) {
  module.exports = inherits_browser;
}
});

function posix(path$$1) {
	return path$$1.charAt(0) === '/';
}

function win32(path$$1) {
	// https://github.com/nodejs/node/blob/b3fcc245fb25539909ef1d5eaa01dbf92e168633/lib/path.js#L56
	var splitDeviceRe = /^([a-zA-Z]:|[\\\/]{2}[^\\\/]+[\\\/]+[^\\\/]+)?([\\\/])?([\s\S]*?)$/;
	var result = splitDeviceRe.exec(path$$1);
	var device = result[1] || '';
	var isUnc = Boolean(device && device.charAt(1) !== ':');

	// UNC paths are always absolute
	return Boolean(result[2] || isUnc);
}

var pathIsAbsolute = process.platform === 'win32' ? win32 : posix;
var posix_1 = posix;
var win32_1 = win32;

pathIsAbsolute.posix = posix_1;
pathIsAbsolute.win32 = win32_1;

var alphasort_1 = alphasort;
var alphasorti_1 = alphasorti;
var setopts_1 = setopts;
var ownProp_1 = ownProp;
var makeAbs_1 = makeAbs;
var finish_1 = finish;
var mark_1 = mark$2;
var isIgnored_1 = isIgnored;
var childrenIgnored_1 = childrenIgnored;

function ownProp (obj, field) {
  return Object.prototype.hasOwnProperty.call(obj, field)
}




var Minimatch$1 = minimatch_1.Minimatch;

function alphasorti (a, b) {
  return a.toLowerCase().localeCompare(b.toLowerCase())
}

function alphasort (a, b) {
  return a.localeCompare(b)
}

function setupIgnores (self, options) {
  self.ignore = options.ignore || [];

  if (!Array.isArray(self.ignore))
    self.ignore = [self.ignore];

  if (self.ignore.length) {
    self.ignore = self.ignore.map(ignoreMap);
  }
}

// ignore patterns are always in dot:true mode.
function ignoreMap (pattern) {
  var gmatcher = null;
  if (pattern.slice(-3) === '/**') {
    var gpattern = pattern.replace(/(\/\*\*)+$/, '');
    gmatcher = new Minimatch$1(gpattern, { dot: true });
  }

  return {
    matcher: new Minimatch$1(pattern, { dot: true }),
    gmatcher: gmatcher
  }
}

function setopts (self, pattern, options) {
  if (!options)
    options = {};

  // base-matching: just use globstar for that.
  if (options.matchBase && -1 === pattern.indexOf("/")) {
    if (options.noglobstar) {
      throw new Error("base matching requires globstar")
    }
    pattern = "**/" + pattern;
  }

  self.silent = !!options.silent;
  self.pattern = pattern;
  self.strict = options.strict !== false;
  self.realpath = !!options.realpath;
  self.realpathCache = options.realpathCache || Object.create(null);
  self.follow = !!options.follow;
  self.dot = !!options.dot;
  self.mark = !!options.mark;
  self.nodir = !!options.nodir;
  if (self.nodir)
    self.mark = true;
  self.sync = !!options.sync;
  self.nounique = !!options.nounique;
  self.nonull = !!options.nonull;
  self.nosort = !!options.nosort;
  self.nocase = !!options.nocase;
  self.stat = !!options.stat;
  self.noprocess = !!options.noprocess;
  self.absolute = !!options.absolute;

  self.maxLength = options.maxLength || Infinity;
  self.cache = options.cache || Object.create(null);
  self.statCache = options.statCache || Object.create(null);
  self.symlinks = options.symlinks || Object.create(null);

  setupIgnores(self, options);

  self.changedCwd = false;
  var cwd = process.cwd();
  if (!ownProp(options, "cwd"))
    self.cwd = cwd;
  else {
    self.cwd = path.resolve(options.cwd);
    self.changedCwd = self.cwd !== cwd;
  }

  self.root = options.root || path.resolve(self.cwd, "/");
  self.root = path.resolve(self.root);
  if (process.platform === "win32")
    self.root = self.root.replace(/\\/g, "/");

  // TODO: is an absolute `cwd` supposed to be resolved against `root`?
  // e.g. { cwd: '/test', root: __dirname } === path.join(__dirname, '/test')
  self.cwdAbs = pathIsAbsolute(self.cwd) ? self.cwd : makeAbs(self, self.cwd);
  if (process.platform === "win32")
    self.cwdAbs = self.cwdAbs.replace(/\\/g, "/");
  self.nomount = !!options.nomount;

  // disable comments and negation in Minimatch.
  // Note that they are not supported in Glob itself anyway.
  options.nonegate = true;
  options.nocomment = true;

  self.minimatch = new Minimatch$1(pattern, options);
  self.options = self.minimatch.options;
}

function finish (self) {
  var nou = self.nounique;
  var all = nou ? [] : Object.create(null);

  for (var i = 0, l = self.matches.length; i < l; i ++) {
    var matches = self.matches[i];
    if (!matches || Object.keys(matches).length === 0) {
      if (self.nonull) {
        // do like the shell, and spit out the literal glob
        var literal = self.minimatch.globSet[i];
        if (nou)
          all.push(literal);
        else
          all[literal] = true;
      }
    } else {
      // had matches
      var m = Object.keys(matches);
      if (nou)
        all.push.apply(all, m);
      else
        m.forEach(function (m) {
          all[m] = true;
        });
    }
  }

  if (!nou)
    all = Object.keys(all);

  if (!self.nosort)
    all = all.sort(self.nocase ? alphasorti : alphasort);

  // at *some* point we statted all of these
  if (self.mark) {
    for (var i = 0; i < all.length; i++) {
      all[i] = self._mark(all[i]);
    }
    if (self.nodir) {
      all = all.filter(function (e) {
        var notDir = !(/\/$/.test(e));
        var c = self.cache[e] || self.cache[makeAbs(self, e)];
        if (notDir && c)
          notDir = c !== 'DIR' && !Array.isArray(c);
        return notDir
      });
    }
  }

  if (self.ignore.length)
    all = all.filter(function(m) {
      return !isIgnored(self, m)
    });

  self.found = all;
}

function mark$2 (self, p) {
  var abs = makeAbs(self, p);
  var c = self.cache[abs];
  var m = p;
  if (c) {
    var isDir = c === 'DIR' || Array.isArray(c);
    var slash = p.slice(-1) === '/';

    if (isDir && !slash)
      m += '/';
    else if (!isDir && slash)
      m = m.slice(0, -1);

    if (m !== p) {
      var mabs = makeAbs(self, m);
      self.statCache[mabs] = self.statCache[abs];
      self.cache[mabs] = self.cache[abs];
    }
  }

  return m
}

// lotta situps...
function makeAbs (self, f) {
  var abs = f;
  if (f.charAt(0) === '/') {
    abs = path.join(self.root, f);
  } else if (pathIsAbsolute(f) || f === '') {
    abs = f;
  } else if (self.changedCwd) {
    abs = path.resolve(self.cwd, f);
  } else {
    abs = path.resolve(f);
  }

  if (process.platform === 'win32')
    abs = abs.replace(/\\/g, '/');

  return abs
}


// Return true, if pattern ends with globstar '**', for the accompanying parent directory.
// Ex:- If node_modules/** is the pattern, add 'node_modules' to ignore list along with it's contents
function isIgnored (self, path$$1) {
  if (!self.ignore.length)
    return false

  return self.ignore.some(function(item) {
    return item.matcher.match(path$$1) || !!(item.gmatcher && item.gmatcher.match(path$$1))
  })
}

function childrenIgnored (self, path$$1) {
  if (!self.ignore.length)
    return false

  return self.ignore.some(function(item) {
    return !!(item.gmatcher && item.gmatcher.match(path$$1))
  })
}

var common$3 = {
	alphasort: alphasort_1,
	alphasorti: alphasorti_1,
	setopts: setopts_1,
	ownProp: ownProp_1,
	makeAbs: makeAbs_1,
	finish: finish_1,
	mark: mark_1,
	isIgnored: isIgnored_1,
	childrenIgnored: childrenIgnored_1
};

var sync = globSync;
globSync.GlobSync = GlobSync;




var setopts$1 = common$3.setopts;
var ownProp$1 = common$3.ownProp;
var childrenIgnored$1 = common$3.childrenIgnored;
var isIgnored$1 = common$3.isIgnored;

function globSync (pattern, options) {
  if (typeof options === 'function' || arguments.length === 3)
    throw new TypeError('callback provided to sync glob\n'+
                        'See: https://github.com/isaacs/node-glob/issues/167')

  return new GlobSync(pattern, options).found
}

function GlobSync (pattern, options) {
  if (!pattern)
    throw new Error('must provide pattern')

  if (typeof options === 'function' || arguments.length === 3)
    throw new TypeError('callback provided to sync glob\n'+
                        'See: https://github.com/isaacs/node-glob/issues/167')

  if (!(this instanceof GlobSync))
    return new GlobSync(pattern, options)

  setopts$1(this, pattern, options);

  if (this.noprocess)
    return this

  var n = this.minimatch.set.length;
  this.matches = new Array(n);
  for (var i = 0; i < n; i ++) {
    this._process(this.minimatch.set[i], i, false);
  }
  this._finish();
}

GlobSync.prototype._finish = function () {
  assert(this instanceof GlobSync);
  if (this.realpath) {
    var self = this;
    this.matches.forEach(function (matchset, index) {
      var set = self.matches[index] = Object.create(null);
      for (var p in matchset) {
        try {
          p = self._makeAbs(p);
          var real = fs_realpath.realpathSync(p, self.realpathCache);
          set[real] = true;
        } catch (er) {
          if (er.syscall === 'stat')
            set[self._makeAbs(p)] = true;
          else
            throw er
        }
      }
    });
  }
  common$3.finish(this);
};


GlobSync.prototype._process = function (pattern, index, inGlobStar) {
  assert(this instanceof GlobSync);

  // Get the first [n] parts of pattern that are all strings.
  var n = 0;
  while (typeof pattern[n] === 'string') {
    n ++;
  }
  // now n is the index of the first one that is *not* a string.

  // See if there's anything else
  var prefix;
  switch (n) {
    // if not, then this is rather simple
    case pattern.length:
      this._processSimple(pattern.join('/'), index);
      return

    case 0:
      // pattern *starts* with some non-trivial item.
      // going to readdir(cwd), but not include the prefix in matches.
      prefix = null;
      break

    default:
      // pattern has some string bits in the front.
      // whatever it starts with, whether that's 'absolute' like /foo/bar,
      // or 'relative' like '../baz'
      prefix = pattern.slice(0, n).join('/');
      break
  }

  var remain = pattern.slice(n);

  // get the list of entries.
  var read;
  if (prefix === null)
    read = '.';
  else if (pathIsAbsolute(prefix) || pathIsAbsolute(pattern.join('/'))) {
    if (!prefix || !pathIsAbsolute(prefix))
      prefix = '/' + prefix;
    read = prefix;
  } else
    read = prefix;

  var abs = this._makeAbs(read);

  //if ignored, skip processing
  if (childrenIgnored$1(this, read))
    return

  var isGlobStar = remain[0] === minimatch_1.GLOBSTAR;
  if (isGlobStar)
    this._processGlobStar(prefix, read, abs, remain, index, inGlobStar);
  else
    this._processReaddir(prefix, read, abs, remain, index, inGlobStar);
};


GlobSync.prototype._processReaddir = function (prefix, read, abs, remain, index, inGlobStar) {
  var entries = this._readdir(abs, inGlobStar);

  // if the abs isn't a dir, then nothing can match!
  if (!entries)
    return

  // It will only match dot entries if it starts with a dot, or if
  // dot is set.  Stuff like @(.foo|.bar) isn't allowed.
  var pn = remain[0];
  var negate = !!this.minimatch.negate;
  var rawGlob = pn._glob;
  var dotOk = this.dot || rawGlob.charAt(0) === '.';

  var matchedEntries = [];
  for (var i = 0; i < entries.length; i++) {
    var e = entries[i];
    if (e.charAt(0) !== '.' || dotOk) {
      var m;
      if (negate && !prefix) {
        m = !e.match(pn);
      } else {
        m = e.match(pn);
      }
      if (m)
        matchedEntries.push(e);
    }
  }

  var len = matchedEntries.length;
  // If there are no matched entries, then nothing matches.
  if (len === 0)
    return

  // if this is the last remaining pattern bit, then no need for
  // an additional stat *unless* the user has specified mark or
  // stat explicitly.  We know they exist, since readdir returned
  // them.

  if (remain.length === 1 && !this.mark && !this.stat) {
    if (!this.matches[index])
      this.matches[index] = Object.create(null);

    for (var i = 0; i < len; i ++) {
      var e = matchedEntries[i];
      if (prefix) {
        if (prefix.slice(-1) !== '/')
          e = prefix + '/' + e;
        else
          e = prefix + e;
      }

      if (e.charAt(0) === '/' && !this.nomount) {
        e = path.join(this.root, e);
      }
      this._emitMatch(index, e);
    }
    // This was the last one, and no stats were needed
    return
  }

  // now test all matched entries as stand-ins for that part
  // of the pattern.
  remain.shift();
  for (var i = 0; i < len; i ++) {
    var e = matchedEntries[i];
    var newPattern;
    if (prefix)
      newPattern = [prefix, e];
    else
      newPattern = [e];
    this._process(newPattern.concat(remain), index, inGlobStar);
  }
};


GlobSync.prototype._emitMatch = function (index, e) {
  if (isIgnored$1(this, e))
    return

  var abs = this._makeAbs(e);

  if (this.mark)
    e = this._mark(e);

  if (this.absolute) {
    e = abs;
  }

  if (this.matches[index][e])
    return

  if (this.nodir) {
    var c = this.cache[abs];
    if (c === 'DIR' || Array.isArray(c))
      return
  }

  this.matches[index][e] = true;

  if (this.stat)
    this._stat(e);
};


GlobSync.prototype._readdirInGlobStar = function (abs) {
  // follow all symlinked directories forever
  // just proceed as if this is a non-globstar situation
  if (this.follow)
    return this._readdir(abs, false)

  var entries;
  var lstat;
  try {
    lstat = fs.lstatSync(abs);
  } catch (er) {
    if (er.code === 'ENOENT') {
      // lstat failed, doesn't exist
      return null
    }
  }

  var isSym = lstat && lstat.isSymbolicLink();
  this.symlinks[abs] = isSym;

  // If it's not a symlink or a dir, then it's definitely a regular file.
  // don't bother doing a readdir in that case.
  if (!isSym && lstat && !lstat.isDirectory())
    this.cache[abs] = 'FILE';
  else
    entries = this._readdir(abs, false);

  return entries
};

GlobSync.prototype._readdir = function (abs, inGlobStar) {
  if (inGlobStar && !ownProp$1(this.symlinks, abs))
    return this._readdirInGlobStar(abs)

  if (ownProp$1(this.cache, abs)) {
    var c = this.cache[abs];
    if (!c || c === 'FILE')
      return null

    if (Array.isArray(c))
      return c
  }

  try {
    return this._readdirEntries(abs, fs.readdirSync(abs))
  } catch (er) {
    this._readdirError(abs, er);
    return null
  }
};

GlobSync.prototype._readdirEntries = function (abs, entries) {
  // if we haven't asked to stat everything, then just
  // assume that everything in there exists, so we can avoid
  // having to stat it a second time.
  if (!this.mark && !this.stat) {
    for (var i = 0; i < entries.length; i ++) {
      var e = entries[i];
      if (abs === '/')
        e = abs + e;
      else
        e = abs + '/' + e;
      this.cache[e] = true;
    }
  }

  this.cache[abs] = entries;

  // mark and cache dir-ness
  return entries
};

GlobSync.prototype._readdirError = function (f, er) {
  // handle errors, and cache the information
  switch (er.code) {
    case 'ENOTSUP': // https://github.com/isaacs/node-glob/issues/205
    case 'ENOTDIR': // totally normal. means it *does* exist.
      var abs = this._makeAbs(f);
      this.cache[abs] = 'FILE';
      if (abs === this.cwdAbs) {
        var error = new Error(er.code + ' invalid cwd ' + this.cwd);
        error.path = this.cwd;
        error.code = er.code;
        throw error
      }
      break

    case 'ENOENT': // not terribly unusual
    case 'ELOOP':
    case 'ENAMETOOLONG':
    case 'UNKNOWN':
      this.cache[this._makeAbs(f)] = false;
      break

    default: // some unusual error.  Treat as failure.
      this.cache[this._makeAbs(f)] = false;
      if (this.strict)
        throw er
      if (!this.silent)
        console.error('glob error', er);
      break
  }
};

GlobSync.prototype._processGlobStar = function (prefix, read, abs, remain, index, inGlobStar) {

  var entries = this._readdir(abs, inGlobStar);

  // no entries means not a dir, so it can never have matches
  // foo.txt/** doesn't match foo.txt
  if (!entries)
    return

  // test without the globstar, and with every child both below
  // and replacing the globstar.
  var remainWithoutGlobStar = remain.slice(1);
  var gspref = prefix ? [ prefix ] : [];
  var noGlobStar = gspref.concat(remainWithoutGlobStar);

  // the noGlobStar pattern exits the inGlobStar state
  this._process(noGlobStar, index, false);

  var len = entries.length;
  var isSym = this.symlinks[abs];

  // If it's a symlink, and we're in a globstar, then stop
  if (isSym && inGlobStar)
    return

  for (var i = 0; i < len; i++) {
    var e = entries[i];
    if (e.charAt(0) === '.' && !this.dot)
      continue

    // these two cases enter the inGlobStar state
    var instead = gspref.concat(entries[i], remainWithoutGlobStar);
    this._process(instead, index, true);

    var below = gspref.concat(entries[i], remain);
    this._process(below, index, true);
  }
};

GlobSync.prototype._processSimple = function (prefix, index) {
  // XXX review this.  Shouldn't it be doing the mounting etc
  // before doing stat?  kinda weird?
  var exists = this._stat(prefix);

  if (!this.matches[index])
    this.matches[index] = Object.create(null);

  // If it doesn't exist, then just mark the lack of results
  if (!exists)
    return

  if (prefix && pathIsAbsolute(prefix) && !this.nomount) {
    var trail = /[\/\\]$/.test(prefix);
    if (prefix.charAt(0) === '/') {
      prefix = path.join(this.root, prefix);
    } else {
      prefix = path.resolve(this.root, prefix);
      if (trail)
        prefix += '/';
    }
  }

  if (process.platform === 'win32')
    prefix = prefix.replace(/\\/g, '/');

  // Mark this as a match
  this._emitMatch(index, prefix);
};

// Returns either 'DIR', 'FILE', or false
GlobSync.prototype._stat = function (f) {
  var abs = this._makeAbs(f);
  var needDir = f.slice(-1) === '/';

  if (f.length > this.maxLength)
    return false

  if (!this.stat && ownProp$1(this.cache, abs)) {
    var c = this.cache[abs];

    if (Array.isArray(c))
      c = 'DIR';

    // It exists, but maybe not how we need it
    if (!needDir || c === 'DIR')
      return c

    if (needDir && c === 'FILE')
      return false

    // otherwise we have to stat, because maybe c=true
    // if we know it exists, but not what it is.
  }

  var stat = this.statCache[abs];
  if (!stat) {
    var lstat;
    try {
      lstat = fs.lstatSync(abs);
    } catch (er) {
      if (er && (er.code === 'ENOENT' || er.code === 'ENOTDIR')) {
        this.statCache[abs] = false;
        return false
      }
    }

    if (lstat && lstat.isSymbolicLink()) {
      try {
        stat = fs.statSync(abs);
      } catch (er) {
        stat = lstat;
      }
    } else {
      stat = lstat;
    }
  }

  this.statCache[abs] = stat;

  var c = true;
  if (stat)
    c = stat.isDirectory() ? 'DIR' : 'FILE';

  this.cache[abs] = this.cache[abs] || c;

  if (needDir && c === 'FILE')
    return false

  return c
};

GlobSync.prototype._mark = function (p) {
  return common$3.mark(this, p)
};

GlobSync.prototype._makeAbs = function (f) {
  return common$3.makeAbs(this, f)
};

// Returns a wrapper function that returns a wrapped callback
// The wrapper function should do some stuff, and return a
// presumably different callback function.
// This makes sure that own properties are retained, so that
// decorations and such are not lost along the way.
var wrappy_1 = wrappy;
function wrappy (fn, cb) {
  if (fn && cb) return wrappy(fn)(cb)

  if (typeof fn !== 'function')
    throw new TypeError('need wrapper function')

  Object.keys(fn).forEach(function (k) {
    wrapper[k] = fn[k];
  });

  return wrapper

  function wrapper() {
    var args = new Array(arguments.length);
    for (var i = 0; i < args.length; i++) {
      args[i] = arguments[i];
    }
    var ret = fn.apply(this, args);
    var cb = args[args.length-1];
    if (typeof ret === 'function' && ret !== cb) {
      Object.keys(cb).forEach(function (k) {
        ret[k] = cb[k];
      });
    }
    return ret
  }
}

var once_1 = wrappy_1(once);
var strict = wrappy_1(onceStrict);

once.proto = once(function () {
  Object.defineProperty(Function.prototype, 'once', {
    value: function () {
      return once(this)
    },
    configurable: true
  });

  Object.defineProperty(Function.prototype, 'onceStrict', {
    value: function () {
      return onceStrict(this)
    },
    configurable: true
  });
});

function once (fn) {
  var f = function () {
    if (f.called) return f.value
    f.called = true;
    return f.value = fn.apply(this, arguments)
  };
  f.called = false;
  return f
}

function onceStrict (fn) {
  var f = function () {
    if (f.called)
      throw new Error(f.onceError)
    f.called = true;
    return f.value = fn.apply(this, arguments)
  };
  var name = fn.name || 'Function wrapped with `once`';
  f.onceError = name + " shouldn't be called more than once";
  f.called = false;
  return f
}

once_1.strict = strict;

var reqs = Object.create(null);


var inflight_1 = wrappy_1(inflight);

function inflight (key, cb) {
  if (reqs[key]) {
    reqs[key].push(cb);
    return null
  } else {
    reqs[key] = [cb];
    return makeres(key)
  }
}

function makeres (key) {
  return once_1(function RES () {
    var cbs = reqs[key];
    var len = cbs.length;
    var args = slice$2(arguments);

    // XXX It's somewhat ambiguous whether a new callback added in this
    // pass should be queued for later execution if something in the
    // list of callbacks throws, or if it should just be discarded.
    // However, it's such an edge case that it hardly matters, and either
    // choice is likely as surprising as the other.
    // As it happens, we do go ahead and schedule it for later execution.
    try {
      for (var i = 0; i < len; i++) {
        cbs[i].apply(null, args);
      }
    } finally {
      if (cbs.length > len) {
        // added more in the interim.
        // de-zalgo, just in case, but don't call again.
        cbs.splice(0, len);
        process.nextTick(function () {
          RES.apply(null, args);
        });
      } else {
        delete reqs[key];
      }
    }
  })
}

function slice$2 (args) {
  var length = args.length;
  var array = [];

  for (var i = 0; i < length; i++) array[i] = args[i];
  return array
}

// Approach:
//
// 1. Get the minimatch set
// 2. For each pattern in the set, PROCESS(pattern, false)
// 3. Store matches per-set, then uniq them
//
// PROCESS(pattern, inGlobStar)
// Get the first [n] items from pattern that are all strings
// Join these together.  This is PREFIX.
//   If there is no more remaining, then stat(PREFIX) and
//   add to matches if it succeeds.  END.
//
// If inGlobStar and PREFIX is symlink and points to dir
//   set ENTRIES = []
// else readdir(PREFIX) as ENTRIES
//   If fail, END
//
// with ENTRIES
//   If pattern[n] is GLOBSTAR
//     // handle the case where the globstar match is empty
//     // by pruning it out, and testing the resulting pattern
//     PROCESS(pattern[0..n] + pattern[n+1 .. $], false)
//     // handle other cases.
//     for ENTRY in ENTRIES (not dotfiles)
//       // attach globstar + tail onto the entry
//       // Mark that this entry is a globstar match
//       PROCESS(pattern[0..n] + ENTRY + pattern[n .. $], true)
//
//   else // not globstar
//     for ENTRY in ENTRIES (not dotfiles, unless pattern[n] is dot)
//       Test ENTRY against pattern[n]
//       If fails, continue
//       If passes, PROCESS(pattern[0..n] + item + pattern[n+1 .. $])
//
// Caveat:
//   Cache all stats and readdirs results to minimize syscall.  Since all
//   we ever care about is existence and directory-ness, we can just keep
//   `true` for files, and [children,...] for directories, or `false` for
//   things that don't exist.

var glob_1 = glob;




var EE = events.EventEmitter;





var setopts$2 = common$3.setopts;
var ownProp$2 = common$3.ownProp;


var childrenIgnored$2 = common$3.childrenIgnored;
var isIgnored$2 = common$3.isIgnored;



function glob (pattern, options, cb) {
  if (typeof options === 'function') cb = options, options = {};
  if (!options) options = {};

  if (options.sync) {
    if (cb)
      throw new TypeError('callback provided to sync glob')
    return sync(pattern, options)
  }

  return new Glob$1(pattern, options, cb)
}

glob.sync = sync;
var GlobSync$1 = glob.GlobSync = sync.GlobSync;

// old api surface
glob.glob = glob;

function extend$2 (origin, add) {
  if (add === null || typeof add !== 'object') {
    return origin
  }

  var keys = Object.keys(add);
  var i = keys.length;
  while (i--) {
    origin[keys[i]] = add[keys[i]];
  }
  return origin
}

glob.hasMagic = function (pattern, options_) {
  var options = extend$2({}, options_);
  options.noprocess = true;

  var g = new Glob$1(pattern, options);
  var set = g.minimatch.set;

  if (!pattern)
    return false

  if (set.length > 1)
    return true

  for (var j = 0; j < set[0].length; j++) {
    if (typeof set[0][j] !== 'string')
      return true
  }

  return false
};

glob.Glob = Glob$1;
inherits(Glob$1, EE);
function Glob$1 (pattern, options, cb) {
  if (typeof options === 'function') {
    cb = options;
    options = null;
  }

  if (options && options.sync) {
    if (cb)
      throw new TypeError('callback provided to sync glob')
    return new GlobSync$1(pattern, options)
  }

  if (!(this instanceof Glob$1))
    return new Glob$1(pattern, options, cb)

  setopts$2(this, pattern, options);
  this._didRealPath = false;

  // process each pattern in the minimatch set
  var n = this.minimatch.set.length;

  // The matches are stored as {<filename>: true,...} so that
  // duplicates are automagically pruned.
  // Later, we do an Object.keys() on these.
  // Keep them as a list so we can fill in when nonull is set.
  this.matches = new Array(n);

  if (typeof cb === 'function') {
    cb = once_1(cb);
    this.on('error', cb);
    this.on('end', function (matches) {
      cb(null, matches);
    });
  }

  var self = this;
  this._processing = 0;

  this._emitQueue = [];
  this._processQueue = [];
  this.paused = false;

  if (this.noprocess)
    return this

  if (n === 0)
    return done()

  var sync$$1 = true;
  for (var i = 0; i < n; i ++) {
    this._process(this.minimatch.set[i], i, false, done);
  }
  sync$$1 = false;

  function done () {
    --self._processing;
    if (self._processing <= 0) {
      if (sync$$1) {
        process.nextTick(function () {
          self._finish();
        });
      } else {
        self._finish();
      }
    }
  }
}

Glob$1.prototype._finish = function () {
  assert(this instanceof Glob$1);
  if (this.aborted)
    return

  if (this.realpath && !this._didRealpath)
    return this._realpath()

  common$3.finish(this);
  this.emit('end', this.found);
};

Glob$1.prototype._realpath = function () {
  if (this._didRealpath)
    return

  this._didRealpath = true;

  var n = this.matches.length;
  if (n === 0)
    return this._finish()

  var self = this;
  for (var i = 0; i < this.matches.length; i++)
    this._realpathSet(i, next);

  function next () {
    if (--n === 0)
      self._finish();
  }
};

Glob$1.prototype._realpathSet = function (index, cb) {
  var matchset = this.matches[index];
  if (!matchset)
    return cb()

  var found = Object.keys(matchset);
  var self = this;
  var n = found.length;

  if (n === 0)
    return cb()

  var set = this.matches[index] = Object.create(null);
  found.forEach(function (p, i) {
    // If there's a problem with the stat, then it means that
    // one or more of the links in the realpath couldn't be
    // resolved.  just return the abs value in that case.
    p = self._makeAbs(p);
    fs_realpath.realpath(p, self.realpathCache, function (er, real) {
      if (!er)
        set[real] = true;
      else if (er.syscall === 'stat')
        set[p] = true;
      else
        self.emit('error', er); // srsly wtf right here

      if (--n === 0) {
        self.matches[index] = set;
        cb();
      }
    });
  });
};

Glob$1.prototype._mark = function (p) {
  return common$3.mark(this, p)
};

Glob$1.prototype._makeAbs = function (f) {
  return common$3.makeAbs(this, f)
};

Glob$1.prototype.abort = function () {
  this.aborted = true;
  this.emit('abort');
};

Glob$1.prototype.pause = function () {
  if (!this.paused) {
    this.paused = true;
    this.emit('pause');
  }
};

Glob$1.prototype.resume = function () {
  if (this.paused) {
    this.emit('resume');
    this.paused = false;
    if (this._emitQueue.length) {
      var eq = this._emitQueue.slice(0);
      this._emitQueue.length = 0;
      for (var i = 0; i < eq.length; i ++) {
        var e = eq[i];
        this._emitMatch(e[0], e[1]);
      }
    }
    if (this._processQueue.length) {
      var pq = this._processQueue.slice(0);
      this._processQueue.length = 0;
      for (var i = 0; i < pq.length; i ++) {
        var p = pq[i];
        this._processing--;
        this._process(p[0], p[1], p[2], p[3]);
      }
    }
  }
};

Glob$1.prototype._process = function (pattern, index, inGlobStar, cb) {
  assert(this instanceof Glob$1);
  assert(typeof cb === 'function');

  if (this.aborted)
    return

  this._processing++;
  if (this.paused) {
    this._processQueue.push([pattern, index, inGlobStar, cb]);
    return
  }

  //console.error('PROCESS %d', this._processing, pattern)

  // Get the first [n] parts of pattern that are all strings.
  var n = 0;
  while (typeof pattern[n] === 'string') {
    n ++;
  }
  // now n is the index of the first one that is *not* a string.

  // see if there's anything else
  var prefix;
  switch (n) {
    // if not, then this is rather simple
    case pattern.length:
      this._processSimple(pattern.join('/'), index, cb);
      return

    case 0:
      // pattern *starts* with some non-trivial item.
      // going to readdir(cwd), but not include the prefix in matches.
      prefix = null;
      break

    default:
      // pattern has some string bits in the front.
      // whatever it starts with, whether that's 'absolute' like /foo/bar,
      // or 'relative' like '../baz'
      prefix = pattern.slice(0, n).join('/');
      break
  }

  var remain = pattern.slice(n);

  // get the list of entries.
  var read;
  if (prefix === null)
    read = '.';
  else if (pathIsAbsolute(prefix) || pathIsAbsolute(pattern.join('/'))) {
    if (!prefix || !pathIsAbsolute(prefix))
      prefix = '/' + prefix;
    read = prefix;
  } else
    read = prefix;

  var abs = this._makeAbs(read);

  //if ignored, skip _processing
  if (childrenIgnored$2(this, read))
    return cb()

  var isGlobStar = remain[0] === minimatch_1.GLOBSTAR;
  if (isGlobStar)
    this._processGlobStar(prefix, read, abs, remain, index, inGlobStar, cb);
  else
    this._processReaddir(prefix, read, abs, remain, index, inGlobStar, cb);
};

Glob$1.prototype._processReaddir = function (prefix, read, abs, remain, index, inGlobStar, cb) {
  var self = this;
  this._readdir(abs, inGlobStar, function (er, entries) {
    return self._processReaddir2(prefix, read, abs, remain, index, inGlobStar, entries, cb)
  });
};

Glob$1.prototype._processReaddir2 = function (prefix, read, abs, remain, index, inGlobStar, entries, cb) {

  // if the abs isn't a dir, then nothing can match!
  if (!entries)
    return cb()

  // It will only match dot entries if it starts with a dot, or if
  // dot is set.  Stuff like @(.foo|.bar) isn't allowed.
  var pn = remain[0];
  var negate = !!this.minimatch.negate;
  var rawGlob = pn._glob;
  var dotOk = this.dot || rawGlob.charAt(0) === '.';

  var matchedEntries = [];
  for (var i = 0; i < entries.length; i++) {
    var e = entries[i];
    if (e.charAt(0) !== '.' || dotOk) {
      var m;
      if (negate && !prefix) {
        m = !e.match(pn);
      } else {
        m = e.match(pn);
      }
      if (m)
        matchedEntries.push(e);
    }
  }

  //console.error('prd2', prefix, entries, remain[0]._glob, matchedEntries)

  var len = matchedEntries.length;
  // If there are no matched entries, then nothing matches.
  if (len === 0)
    return cb()

  // if this is the last remaining pattern bit, then no need for
  // an additional stat *unless* the user has specified mark or
  // stat explicitly.  We know they exist, since readdir returned
  // them.

  if (remain.length === 1 && !this.mark && !this.stat) {
    if (!this.matches[index])
      this.matches[index] = Object.create(null);

    for (var i = 0; i < len; i ++) {
      var e = matchedEntries[i];
      if (prefix) {
        if (prefix !== '/')
          e = prefix + '/' + e;
        else
          e = prefix + e;
      }

      if (e.charAt(0) === '/' && !this.nomount) {
        e = path.join(this.root, e);
      }
      this._emitMatch(index, e);
    }
    // This was the last one, and no stats were needed
    return cb()
  }

  // now test all matched entries as stand-ins for that part
  // of the pattern.
  remain.shift();
  for (var i = 0; i < len; i ++) {
    var e = matchedEntries[i];
    if (prefix) {
      if (prefix !== '/')
        e = prefix + '/' + e;
      else
        e = prefix + e;
    }
    this._process([e].concat(remain), index, inGlobStar, cb);
  }
  cb();
};

Glob$1.prototype._emitMatch = function (index, e) {
  if (this.aborted)
    return

  if (isIgnored$2(this, e))
    return

  if (this.paused) {
    this._emitQueue.push([index, e]);
    return
  }

  var abs = pathIsAbsolute(e) ? e : this._makeAbs(e);

  if (this.mark)
    e = this._mark(e);

  if (this.absolute)
    e = abs;

  if (this.matches[index][e])
    return

  if (this.nodir) {
    var c = this.cache[abs];
    if (c === 'DIR' || Array.isArray(c))
      return
  }

  this.matches[index][e] = true;

  var st = this.statCache[abs];
  if (st)
    this.emit('stat', e, st);

  this.emit('match', e);
};

Glob$1.prototype._readdirInGlobStar = function (abs, cb) {
  if (this.aborted)
    return

  // follow all symlinked directories forever
  // just proceed as if this is a non-globstar situation
  if (this.follow)
    return this._readdir(abs, false, cb)

  var lstatkey = 'lstat\0' + abs;
  var self = this;
  var lstatcb = inflight_1(lstatkey, lstatcb_);

  if (lstatcb)
    fs.lstat(abs, lstatcb);

  function lstatcb_ (er, lstat) {
    if (er && er.code === 'ENOENT')
      return cb()

    var isSym = lstat && lstat.isSymbolicLink();
    self.symlinks[abs] = isSym;

    // If it's not a symlink or a dir, then it's definitely a regular file.
    // don't bother doing a readdir in that case.
    if (!isSym && lstat && !lstat.isDirectory()) {
      self.cache[abs] = 'FILE';
      cb();
    } else
      self._readdir(abs, false, cb);
  }
};

Glob$1.prototype._readdir = function (abs, inGlobStar, cb) {
  if (this.aborted)
    return

  cb = inflight_1('readdir\0'+abs+'\0'+inGlobStar, cb);
  if (!cb)
    return

  //console.error('RD %j %j', +inGlobStar, abs)
  if (inGlobStar && !ownProp$2(this.symlinks, abs))
    return this._readdirInGlobStar(abs, cb)

  if (ownProp$2(this.cache, abs)) {
    var c = this.cache[abs];
    if (!c || c === 'FILE')
      return cb()

    if (Array.isArray(c))
      return cb(null, c)
  }

  fs.readdir(abs, readdirCb(this, abs, cb));
};

function readdirCb (self, abs, cb) {
  return function (er, entries) {
    if (er)
      self._readdirError(abs, er, cb);
    else
      self._readdirEntries(abs, entries, cb);
  }
}

Glob$1.prototype._readdirEntries = function (abs, entries, cb) {
  if (this.aborted)
    return

  // if we haven't asked to stat everything, then just
  // assume that everything in there exists, so we can avoid
  // having to stat it a second time.
  if (!this.mark && !this.stat) {
    for (var i = 0; i < entries.length; i ++) {
      var e = entries[i];
      if (abs === '/')
        e = abs + e;
      else
        e = abs + '/' + e;
      this.cache[e] = true;
    }
  }

  this.cache[abs] = entries;
  return cb(null, entries)
};

Glob$1.prototype._readdirError = function (f, er, cb) {
  if (this.aborted)
    return

  // handle errors, and cache the information
  switch (er.code) {
    case 'ENOTSUP': // https://github.com/isaacs/node-glob/issues/205
    case 'ENOTDIR': // totally normal. means it *does* exist.
      var abs = this._makeAbs(f);
      this.cache[abs] = 'FILE';
      if (abs === this.cwdAbs) {
        var error = new Error(er.code + ' invalid cwd ' + this.cwd);
        error.path = this.cwd;
        error.code = er.code;
        this.emit('error', error);
        this.abort();
      }
      break

    case 'ENOENT': // not terribly unusual
    case 'ELOOP':
    case 'ENAMETOOLONG':
    case 'UNKNOWN':
      this.cache[this._makeAbs(f)] = false;
      break

    default: // some unusual error.  Treat as failure.
      this.cache[this._makeAbs(f)] = false;
      if (this.strict) {
        this.emit('error', er);
        // If the error is handled, then we abort
        // if not, we threw out of here
        this.abort();
      }
      if (!this.silent)
        console.error('glob error', er);
      break
  }

  return cb()
};

Glob$1.prototype._processGlobStar = function (prefix, read, abs, remain, index, inGlobStar, cb) {
  var self = this;
  this._readdir(abs, inGlobStar, function (er, entries) {
    self._processGlobStar2(prefix, read, abs, remain, index, inGlobStar, entries, cb);
  });
};


Glob$1.prototype._processGlobStar2 = function (prefix, read, abs, remain, index, inGlobStar, entries, cb) {
  //console.error('pgs2', prefix, remain[0], entries)

  // no entries means not a dir, so it can never have matches
  // foo.txt/** doesn't match foo.txt
  if (!entries)
    return cb()

  // test without the globstar, and with every child both below
  // and replacing the globstar.
  var remainWithoutGlobStar = remain.slice(1);
  var gspref = prefix ? [ prefix ] : [];
  var noGlobStar = gspref.concat(remainWithoutGlobStar);

  // the noGlobStar pattern exits the inGlobStar state
  this._process(noGlobStar, index, false, cb);

  var isSym = this.symlinks[abs];
  var len = entries.length;

  // If it's a symlink, and we're in a globstar, then stop
  if (isSym && inGlobStar)
    return cb()

  for (var i = 0; i < len; i++) {
    var e = entries[i];
    if (e.charAt(0) === '.' && !this.dot)
      continue

    // these two cases enter the inGlobStar state
    var instead = gspref.concat(entries[i], remainWithoutGlobStar);
    this._process(instead, index, true, cb);

    var below = gspref.concat(entries[i], remain);
    this._process(below, index, true, cb);
  }

  cb();
};

Glob$1.prototype._processSimple = function (prefix, index, cb) {
  // XXX review this.  Shouldn't it be doing the mounting etc
  // before doing stat?  kinda weird?
  var self = this;
  this._stat(prefix, function (er, exists) {
    self._processSimple2(prefix, index, er, exists, cb);
  });
};
Glob$1.prototype._processSimple2 = function (prefix, index, er, exists, cb) {

  //console.error('ps2', prefix, exists)

  if (!this.matches[index])
    this.matches[index] = Object.create(null);

  // If it doesn't exist, then just mark the lack of results
  if (!exists)
    return cb()

  if (prefix && pathIsAbsolute(prefix) && !this.nomount) {
    var trail = /[\/\\]$/.test(prefix);
    if (prefix.charAt(0) === '/') {
      prefix = path.join(this.root, prefix);
    } else {
      prefix = path.resolve(this.root, prefix);
      if (trail)
        prefix += '/';
    }
  }

  if (process.platform === 'win32')
    prefix = prefix.replace(/\\/g, '/');

  // Mark this as a match
  this._emitMatch(index, prefix);
  cb();
};

// Returns either 'DIR', 'FILE', or false
Glob$1.prototype._stat = function (f, cb) {
  var abs = this._makeAbs(f);
  var needDir = f.slice(-1) === '/';

  if (f.length > this.maxLength)
    return cb()

  if (!this.stat && ownProp$2(this.cache, abs)) {
    var c = this.cache[abs];

    if (Array.isArray(c))
      c = 'DIR';

    // It exists, but maybe not how we need it
    if (!needDir || c === 'DIR')
      return cb(null, c)

    if (needDir && c === 'FILE')
      return cb()

    // otherwise we have to stat, because maybe c=true
    // if we know it exists, but not what it is.
  }

  var stat = this.statCache[abs];
  if (stat !== undefined) {
    if (stat === false)
      return cb(null, stat)
    else {
      var type = stat.isDirectory() ? 'DIR' : 'FILE';
      if (needDir && type === 'FILE')
        return cb()
      else
        return cb(null, type, stat)
    }
  }

  var self = this;
  var statcb = inflight_1('stat\0' + abs, lstatcb_);
  if (statcb)
    fs.lstat(abs, statcb);

  function lstatcb_ (er, lstat) {
    if (lstat && lstat.isSymbolicLink()) {
      // If it's a symlink, then treat it as the target, unless
      // the target does not exist, then treat it as a file.
      return fs.stat(abs, function (er, stat) {
        if (er)
          self._stat2(f, abs, null, lstat, cb);
        else
          self._stat2(f, abs, er, stat, cb);
      })
    } else {
      self._stat2(f, abs, er, lstat, cb);
    }
  }
};

Glob$1.prototype._stat2 = function (f, abs, er, stat, cb) {
  if (er && (er.code === 'ENOENT' || er.code === 'ENOTDIR')) {
    this.statCache[abs] = false;
    return cb()
  }

  var needDir = f.slice(-1) === '/';
  this.statCache[abs] = stat;

  if (abs.slice(-1) === '/' && stat && !stat.isDirectory())
    return cb(null, false, stat)

  var c = true;
  if (stat)
    c = stat.isDirectory() ? 'DIR' : 'FILE';
  this.cache[abs] = this.cache[abs] || c;

  if (needDir && c === 'FILE')
    return cb()

  return cb(null, c, stat)
};

/*!
 * Determine if an object is a Buffer
 *
 * @author   Feross Aboukhadijeh <https://feross.org>
 * @license  MIT
 */

// The _isBuffer check is for Safari 5-7 support, because it's missing
// Object.prototype.constructor. Remove this eventually
var isBuffer_1 = function (obj) {
  return obj != null && (isBuffer(obj) || isSlowBuffer(obj) || !!obj._isBuffer)
};

function isBuffer (obj) {
  return !!obj.constructor && typeof obj.constructor.isBuffer === 'function' && obj.constructor.isBuffer(obj)
}

// For Node v0.10 support. Remove this eventually.
function isSlowBuffer (obj) {
  return typeof obj.readFloatLE === 'function' && typeof obj.slice === 'function' && isBuffer(obj.slice(0, 0))
}

var own = {}.hasOwnProperty;

var unistUtilStringifyPosition = stringify;

function stringify(value) {
  /* Nothing. */
  if (!value || typeof value !== 'object') {
    return null
  }

  /* Node. */
  if (own.call(value, 'position') || own.call(value, 'type')) {
    return position(value.position)
  }

  /* Position. */
  if (own.call(value, 'start') || own.call(value, 'end')) {
    return position(value)
  }

  /* Point. */
  if (own.call(value, 'line') || own.call(value, 'column')) {
    return point(value)
  }

  /* ? */
  return null
}

function point(point) {
  if (!point || typeof point !== 'object') {
    point = {};
  }

  return index(point.line) + ':' + index(point.column)
}

function position(pos) {
  if (!pos || typeof pos !== 'object') {
    pos = {};
  }

  return point(pos.start) + '-' + point(pos.end)
}

function index(value) {
  return value && typeof value === 'number' ? value : 1
}

var vfileMessage = VMessage;

/* Inherit from `Error#`. */
function VMessagePrototype() {}
VMessagePrototype.prototype = Error.prototype;
VMessage.prototype = new VMessagePrototype();

/* Message properties. */
var proto = VMessage.prototype;

proto.file = '';
proto.name = '';
proto.reason = '';
proto.message = '';
proto.stack = '';
proto.fatal = null;
proto.column = null;
proto.line = null;

/* Construct a new VMessage.
 *
 * Note: We cannot invoke `Error` on the created context,
 * as that adds readonly `line` and `column` attributes on
 * Safari 9, thus throwing and failing the data. */
function VMessage(reason, position, origin) {
  var parts;
  var range;
  var location;

  if (typeof position === 'string') {
    origin = position;
    position = null;
  }

  parts = parseOrigin(origin);
  range = unistUtilStringifyPosition(position) || '1:1';

  location = {
    start: {line: null, column: null},
    end: {line: null, column: null}
  };

  /* Node. */
  if (position && position.position) {
    position = position.position;
  }

  if (position) {
    /* Position. */
    if (position.start) {
      location = position;
      position = position.start;
    } else {
      /* Point. */
      location.start = position;
    }
  }

  if (reason.stack) {
    this.stack = reason.stack;
    reason = reason.message;
  }

  this.message = reason;
  this.name = range;
  this.reason = reason;
  this.line = position ? position.line : null;
  this.column = position ? position.column : null;
  this.location = location;
  this.source = parts[0];
  this.ruleId = parts[1];
}

function parseOrigin(origin) {
  var result = [null, null];
  var index;

  if (typeof origin === 'string') {
    index = origin.indexOf(':');

    if (index === -1) {
      result[1] = origin;
    } else {
      result[0] = origin.slice(0, index);
      result[1] = origin.slice(index + 1);
    }
  }

  return result
}

function replaceExt(npath, ext) {
  if (typeof npath !== 'string') {
    return npath;
  }

  if (npath.length === 0) {
    return npath;
  }

  var nFileName = path.basename(npath, path.extname(npath)) + ext;
  return path.join(path.dirname(npath), nFileName);
}

var replaceExt_1 = replaceExt;

var core$2 = VFile;

var own$1 = {}.hasOwnProperty;
var proto$1 = VFile.prototype;

proto$1.toString = toString$1;

/* Order of setting (least specific to most), we need this because
 * otherwise `{stem: 'a', path: '~/b.js'}` would throw, as a path
 * is needed before a stem can be set. */
var order = [
  'history',
  'path',
  'basename',
  'stem',
  'extname',
  'dirname'
];

/* Construct a new file. */
function VFile(options) {
  var prop;
  var index;
  var length;

  if (!options) {
    options = {};
  } else if (typeof options === 'string' || isBuffer_1(options)) {
    options = {contents: options};
  } else if ('message' in options && 'messages' in options) {
    return options;
  }

  if (!(this instanceof VFile)) {
    return new VFile(options);
  }

  this.data = {};
  this.messages = [];
  this.history = [];
  this.cwd = process.cwd();

  /* Set path related properties in the correct order. */
  index = -1;
  length = order.length;

  while (++index < length) {
    prop = order[index];

    if (own$1.call(options, prop)) {
      this[prop] = options[prop];
    }
  }

  /* Set non-path related properties. */
  for (prop in options) {
    if (order.indexOf(prop) === -1) {
      this[prop] = options[prop];
    }
  }
}

/* Access full path (`~/index.min.js`). */
Object.defineProperty(proto$1, 'path', {
  get: function () {
    return this.history[this.history.length - 1];
  },
  set: function (path$$1) {
    assertNonEmpty(path$$1, 'path');

    if (path$$1 !== this.path) {
      this.history.push(path$$1);
    }
  }
});

/* Access parent path (`~`). */
Object.defineProperty(proto$1, 'dirname', {
  get: function () {
    return typeof this.path === 'string' ? path.dirname(this.path) : undefined;
  },
  set: function (dirname) {
    assertPath(this.path, 'dirname');
    this.path = path.join(dirname || '', this.basename);
  }
});

/* Access basename (`index.min.js`). */
Object.defineProperty(proto$1, 'basename', {
  get: function () {
    return typeof this.path === 'string' ? path.basename(this.path) : undefined;
  },
  set: function (basename) {
    assertNonEmpty(basename, 'basename');
    assertPart(basename, 'basename');
    this.path = path.join(this.dirname || '', basename);
  }
});

/* Access extname (`.js`). */
Object.defineProperty(proto$1, 'extname', {
  get: function () {
    return typeof this.path === 'string' ? path.extname(this.path) : undefined;
  },
  set: function (extname) {
    var ext = extname || '';

    assertPart(ext, 'extname');
    assertPath(this.path, 'extname');

    if (ext) {
      if (ext.charAt(0) !== '.') {
        throw new Error('`extname` must start with `.`');
      }

      if (ext.indexOf('.', 1) !== -1) {
        throw new Error('`extname` cannot contain multiple dots');
      }
    }

    this.path = replaceExt_1(this.path, ext);
  }
});

/* Access stem (`index.min`). */
Object.defineProperty(proto$1, 'stem', {
  get: function () {
    return typeof this.path === 'string' ? path.basename(this.path, this.extname) : undefined;
  },
  set: function (stem) {
    assertNonEmpty(stem, 'stem');
    assertPart(stem, 'stem');
    this.path = path.join(this.dirname || '', stem + (this.extname || ''));
  }
});

/* Get the value of the file. */
function toString$1(encoding) {
  var value = this.contents || '';
  return isBuffer_1(value) ? value.toString(encoding) : String(value);
}

/* Assert that `part` is not a path (i.e., does
 * not contain `path.sep`). */
function assertPart(part, name) {
  if (part.indexOf(path.sep) !== -1) {
    throw new Error('`' + name + '` cannot be a path: did not expect `' + path.sep + '`');
  }
}

/* Assert that `part` is not empty. */
function assertNonEmpty(part, name) {
  if (!part) {
    throw new Error('`' + name + '` cannot be empty');
  }
}

/* Assert `path` exists. */
function assertPath(path$$1, name) {
  if (!path$$1) {
    throw new Error('Setting `' + name + '` requires `path` to be set too');
  }
}

var vfile = core$2;

var proto$2 = core$2.prototype;

proto$2.message = message;
proto$2.info = info;
proto$2.fail = fail;

/* Slight backwards compatibility.  Remove in the future. */
proto$2.warn = message;

/* Create a message with `reason` at `position`.
 * When an error is passed in as `reason`, copies the stack. */
function message(reason, position, origin) {
  var filePath = this.path;
  var message = new vfileMessage(reason, position, origin);

  if (filePath) {
    message.name = filePath + ':' + message.name;
    message.file = filePath;
  }

  message.fatal = false;

  this.messages.push(message);

  return message;
}

/* Fail. Creates a vmessage, associates it with the file,
 * and throws it. */
function fail() {
  var message = this.message.apply(this, arguments);

  message.fatal = true;

  throw message;
}

/* Info. Creates a vmessage, associates it with the file,
 * and marks the fatality as null. */
function info() {
  var message = this.message.apply(this, arguments);

  message.fatal = null;

  return message;
}

var core$4 = toVFile;

/* Create a virtual file from a description.
 * If `options` is a string or a buffer, it’s used as the
 * path.  In all other cases, the options are passed through
 * to `vfile()`. */
function toVFile(options) {
  if (typeof options === 'string' || isBuffer_1(options)) {
    options = {path: String(options)};
  }

  return vfile(options);
}

var fs_1 = core$4;

core$4.read = read$1;
core$4.readSync = readSync;
core$4.write = write;
core$4.writeSync = writeSync;

/* Create a virtual file and read it in, asynchronously. */
function read$1(description, options, callback) {
  var file = core$4(description);

  if (!callback && xIsFunction(options)) {
    callback = options;
    options = null;
  }

  if (!callback) {
    return new Promise(executor);
  }

  executor(null, callback);

  function executor(resolve, reject) {
    fs.readFile(file.path, options, done);

    function done(err, res) {
      if (err) {
        reject(err);
      } else {
        file.contents = res;

        if (resolve) {
          resolve(file);
        } else {
          callback(null, file);
        }
      }
    }
  }
}

/* Create a virtual file and read it in, synchronously. */
function readSync(description, options) {
  var file = core$4(description);
  file.contents = fs.readFileSync(file.path, options);
  return file;
}

/* Create a virtual file and write it out, asynchronously. */
function write(description, options, callback) {
  var file = core$4(description);

  /* Weird, right? Otherwise `fs` doesn’t accept it. */
  if (!callback && xIsFunction(options)) {
    callback = options;
    options = undefined;
  }

  if (!callback) {
    return new Promise(executor);
  }

  executor(null, callback);

  function executor(resolve, reject) {
    fs.writeFile(file.path, file.contents || '', options, done);

    function done(err) {
      if (err) {
        reject(err);
      } else if (resolve) {
        resolve();
      } else {
        callback();
      }
    }
  }
}

/* Create a virtual file and write it out, synchronously. */
function writeSync(description, options) {
  var file = core$4(description);
  fs.writeFileSync(file.path, file.contents || '', options);
}

var toVfile = fs_1;

/* Expose. */
var isHidden = hidden;

/* Check if `filename` is hidden (starts with a dot). */
function hidden(filename) {
  if (typeof filename !== 'string') {
    throw new Error('Expected string');
  }

  return filename.charAt(0) === '.';
}

var readdir = fs.readdir;
var stat = fs.stat;
var join$2 = path.join;
var relative$2 = path.relative;
var resolve$3 = path.resolve;
var basename = path.basename;
var extname = path.extname;
var magic = glob_1.hasMagic;

var finder = find;

/* Search `patterns`, a mix of globs, paths, and files. */
function find(input, options, callback) {
  expand$2(input, options, done);

  function done(err, result) {
    /* istanbul ignore if - glob errors are unusual.
     * other errors are on the vfile results. */
    if (err) {
      callback(err);
    } else {
      callback(null, {oneFileMode: oneFileMode(result), files: result.output});
    }
  }
}

/* Expand the given glob patterns, search given and found
 * directories, and map to vfiles. */
function expand$2(input, options, next) {
  var cwd = options.cwd;
  var paths = [];
  var actual = 0;
  var expected = 0;
  var failed;

  input.forEach(each);

  if (!expected) {
    search(paths, options, done);
  }

  function each(file) {
    if (xIsString(file)) {
      if (magic(file)) {
        expected++;
        glob_1(file, {cwd: cwd}, one);
      } else {
        /* `relative` to make the paths canonical. */
        file = relative$2(cwd, resolve$3(cwd, file)) || '.';
        paths.push(file);
      }
    } else {
      file.cwd = cwd;
      file.path = relative$2(cwd, file.path);
      file.history = [file.path];
      paths.push(file);
    }
  }

  function one(err, files) {
    /* istanbul ignore if - glob errors are unusual. */
    if (failed) {
      return;
    }

    /* istanbul ignore if - glob errors are unusual. */
    if (err) {
      failed = true;
      done(err);
    } else {
      actual++;
      paths = paths.concat(files);

      if (actual === expected) {
        search(paths, options, done);
      }
    }
  }

  function done(err, files) {
    /* istanbul ignore if - `search` currently does not give errors. */
    if (err) {
      next(err);
    } else {
      next(null, {input: paths, output: files});
    }
  }
}

/* Search `paths`. */
function search(input, options, next) {
  var cwd = options.cwd;
  var silent = options.silentlyIgnore;
  var nested = options.nested;
  var extensions = options.extensions;
  var files = [];
  var expected = 0;
  var actual = 0;

  input.forEach(each);

  if (!expected) {
    next(null, files);
  }

  return each;

  function each(file) {
    var part = base(file);

    if (nested && (isHidden(part) || part === 'node_modules')) {
      return;
    }

    expected++;

    statAndIgnore(file, options, handle);

    function handle(err, result) {
      var ignored = result && result.ignored;
      var dir = result && result.stats && result.stats.isDirectory();

      if (ignored && (nested || silent)) {
        return one(null, []);
      }

      if (!ignored && dir) {
        return readdir(resolve$3(cwd, filePath(file)), directory);
      }

      if (nested && !dir && extensions.length !== 0 && extensions.indexOf(extname(file)) === -1) {
        return one(null, []);
      }

      file = toVfile(file);
      file.cwd = cwd;

      if (ignored) {
        try {
          file.fail('Cannot process specified file: it’s ignored');
        } catch (err) {}
      }

      if (err && err.code === 'ENOENT') {
        try {
          file.fail(err.syscall === 'stat' ? 'No such file or directory' : err);
        } catch (err) {}
      }

      one(null, [file]);
    }

    function directory(err, basenames) {
      var file;

      /* istanbul ignore if - Should not happen often: the directory
       * is `stat`ed first, which was ok, but reading it is not. */
      if (err) {
        file = toVfile(filePath(file));
        file.cwd = cwd;

        try {
          file.fail('Cannot read directory');
        } catch (err) {}

        one(null, [file]);
      } else {
        search(basenames.map(concat), immutable(options, {nested: true}), one);
      }
    }

    /* Error is never given. Always given `results`. */
    function one(_, results) {
      /* istanbul ignore else - always given. */
      if (results) {
        files = files.concat(results);
      }

      actual++;

      if (actual === expected) {
        next(null, files);
      }
    }

    function concat(value) {
      return join$2(filePath(file), value);
    }
  }
}

function statAndIgnore(file, options, callback) {
  var ignore = options.ignore;
  var fp = resolve$3(options.cwd, filePath(file));
  var expected = 1;
  var actual = 0;
  var stats;
  var ignored;

  if (!file.contents) {
    expected++;
    stat(fp, handleStat);
  }

  ignore.check(fp, handleIgnore);

  function handleStat(err, value) {
    stats = value;
    one(err);
  }

  function handleIgnore(err, value) {
    ignored = value;
    one(err);
  }

  function one(err) {
    actual++;

    if (err) {
      callback(err);
      actual = -1;
    } else if (actual === expected) {
      callback(null, {stats: stats, ignored: ignored});
    }
  }
}

function base(file) {
  return xIsString(file) ? basename(file) : file.basename;
}

function filePath(file) {
  return xIsString(file) ? file : file.path;
}

function oneFileMode(result) {
  return result.output.length === 1 &&
    result.input.length === 1 &&
    result.output[0].path === result.input[0];
}

var fileSystem_1 = fileSystem;

/* Find files from the file-system. */
function fileSystem(context, settings, next) {
  var input = context.files;
  var skip = settings.silentlyIgnore;
  var ignore = new ignore$2({
    cwd: settings.cwd,
    detectIgnore: settings.detectIgnore,
    ignoreName: settings.ignoreName,
    ignorePath: settings.ignorePath
  });

  if (input.length === 0) {
    return next();
  }

  finder(input, {
    cwd: settings.cwd,
    extensions: settings.extensions,
    silentlyIgnore: skip,
    ignore: ignore
  }, found);

  function found(err, result) {
    var output = result.files;

    /* Sort alphabetically. Everything’s unique so we don’t care
     * about cases where left and right are equal. */
    output.sort(function (left, right) {
      return left.path < right.path ? -1 : 1;
    });

    /* Mark as given.  This allows outputting files,
     * which can be pretty dangerous, so it’s “hidden”. */
    output.forEach(function (file) {
      file.data.unifiedEngineGiven = true;
    });

    context.files = output;

    /* If `out` wasn’t set, detect it based on
     * whether one file was given. */
    if (settings.out === null || settings.out === undefined) {
      settings.out = result.oneFileMode;
    }

    next(err);
  }
}

var toString$2 = Object.prototype.toString;

var isModern = (
  typeof Buffer.alloc === 'function' &&
  typeof Buffer.allocUnsafe === 'function' &&
  typeof Buffer.from === 'function'
);

function isArrayBuffer (input) {
  return toString$2.call(input).slice(8, -1) === 'ArrayBuffer'
}

function fromArrayBuffer (obj, byteOffset, length) {
  byteOffset >>>= 0;

  var maxLength = obj.byteLength - byteOffset;

  if (maxLength < 0) {
    throw new RangeError("'offset' is out of bounds")
  }

  if (length === undefined) {
    length = maxLength;
  } else {
    length >>>= 0;

    if (length > maxLength) {
      throw new RangeError("'length' is out of bounds")
    }
  }

  return isModern
    ? Buffer.from(obj.slice(byteOffset, byteOffset + length))
    : new Buffer(new Uint8Array(obj.slice(byteOffset, byteOffset + length)))
}

function fromString (string, encoding) {
  if (typeof encoding !== 'string' || encoding === '') {
    encoding = 'utf8';
  }

  if (!Buffer.isEncoding(encoding)) {
    throw new TypeError('"encoding" must be a valid string encoding')
  }

  return isModern
    ? Buffer.from(string, encoding)
    : new Buffer(string, encoding)
}

function bufferFrom (value, encodingOrOffset, length) {
  if (typeof value === 'number') {
    throw new TypeError('"value" argument must not be a number')
  }

  if (isArrayBuffer(value)) {
    return fromArrayBuffer(value, encodingOrOffset, length)
  }

  if (typeof value === 'string') {
    return fromString(value, encodingOrOffset)
  }

  return isModern
    ? Buffer.from(value)
    : new Buffer(value)
}

var bufferFrom_1 = bufferFrom;

var typedarray = createCommonjsModule(function (module, exports) {
var undefined = (void 0); // Paranoia

// Beyond this value, index getters/setters (i.e. array[0], array[1]) are so slow to
// create, and consume so much memory, that the browser appears frozen.
var MAX_ARRAY_LENGTH = 1e5;

// Approximations of internal ECMAScript conversion functions
var ECMAScript = (function() {
  // Stash a copy in case other scripts modify these
  var opts = Object.prototype.toString,
      ophop = Object.prototype.hasOwnProperty;

  return {
    // Class returns internal [[Class]] property, used to avoid cross-frame instanceof issues:
    Class: function(v) { return opts.call(v).replace(/^\[object *|\]$/g, ''); },
    HasProperty: function(o, p) { return p in o; },
    HasOwnProperty: function(o, p) { return ophop.call(o, p); },
    IsCallable: function(o) { return typeof o === 'function'; },
    ToInt32: function(v) { return v >> 0; },
    ToUint32: function(v) { return v >>> 0; }
  };
}());

// Snapshot intrinsics
var LN2 = Math.LN2,
    abs = Math.abs,
    floor = Math.floor,
    log = Math.log,
    min = Math.min,
    pow = Math.pow,
    round = Math.round;

// ES5: lock down object properties
function configureProperties(obj) {
  if (getOwnPropNames && defineProp) {
    var props = getOwnPropNames(obj), i;
    for (i = 0; i < props.length; i += 1) {
      defineProp(obj, props[i], {
        value: obj[props[i]],
        writable: false,
        enumerable: false,
        configurable: false
      });
    }
  }
}

// emulate ES5 getter/setter API using legacy APIs
// http://blogs.msdn.com/b/ie/archive/2010/09/07/transitioning-existing-code-to-the-es5-getter-setter-apis.aspx
// (second clause tests for Object.defineProperty() in IE<9 that only supports extending DOM prototypes, but
// note that IE<9 does not support __defineGetter__ or __defineSetter__ so it just renders the method harmless)
var defineProp;
if (Object.defineProperty && (function() {
      try {
        Object.defineProperty({}, 'x', {});
        return true;
      } catch (e) {
        return false;
      }
    })()) {
  defineProp = Object.defineProperty;
} else {
  defineProp = function(o, p, desc) {
    if (!o === Object(o)) throw new TypeError("Object.defineProperty called on non-object");
    if (ECMAScript.HasProperty(desc, 'get') && Object.prototype.__defineGetter__) { Object.prototype.__defineGetter__.call(o, p, desc.get); }
    if (ECMAScript.HasProperty(desc, 'set') && Object.prototype.__defineSetter__) { Object.prototype.__defineSetter__.call(o, p, desc.set); }
    if (ECMAScript.HasProperty(desc, 'value')) { o[p] = desc.value; }
    return o;
  };
}

var getOwnPropNames = Object.getOwnPropertyNames || function (o) {
  if (o !== Object(o)) throw new TypeError("Object.getOwnPropertyNames called on non-object");
  var props = [], p;
  for (p in o) {
    if (ECMAScript.HasOwnProperty(o, p)) {
      props.push(p);
    }
  }
  return props;
};

// ES5: Make obj[index] an alias for obj._getter(index)/obj._setter(index, value)
// for index in 0 ... obj.length
function makeArrayAccessors(obj) {
  if (!defineProp) { return; }

  if (obj.length > MAX_ARRAY_LENGTH) throw new RangeError("Array too large for polyfill");

  function makeArrayAccessor(index) {
    defineProp(obj, index, {
      'get': function() { return obj._getter(index); },
      'set': function(v) { obj._setter(index, v); },
      enumerable: true,
      configurable: false
    });
  }

  var i;
  for (i = 0; i < obj.length; i += 1) {
    makeArrayAccessor(i);
  }
}

// Internal conversion functions:
//    pack<Type>()   - take a number (interpreted as Type), output a byte array
//    unpack<Type>() - take a byte array, output a Type-like number

function as_signed(value, bits) { var s = 32 - bits; return (value << s) >> s; }
function as_unsigned(value, bits) { var s = 32 - bits; return (value << s) >>> s; }

function packI8(n) { return [n & 0xff]; }
function unpackI8(bytes) { return as_signed(bytes[0], 8); }

function packU8(n) { return [n & 0xff]; }
function unpackU8(bytes) { return as_unsigned(bytes[0], 8); }

function packU8Clamped(n) { n = round(Number(n)); return [n < 0 ? 0 : n > 0xff ? 0xff : n & 0xff]; }

function packI16(n) { return [(n >> 8) & 0xff, n & 0xff]; }
function unpackI16(bytes) { return as_signed(bytes[0] << 8 | bytes[1], 16); }

function packU16(n) { return [(n >> 8) & 0xff, n & 0xff]; }
function unpackU16(bytes) { return as_unsigned(bytes[0] << 8 | bytes[1], 16); }

function packI32(n) { return [(n >> 24) & 0xff, (n >> 16) & 0xff, (n >> 8) & 0xff, n & 0xff]; }
function unpackI32(bytes) { return as_signed(bytes[0] << 24 | bytes[1] << 16 | bytes[2] << 8 | bytes[3], 32); }

function packU32(n) { return [(n >> 24) & 0xff, (n >> 16) & 0xff, (n >> 8) & 0xff, n & 0xff]; }
function unpackU32(bytes) { return as_unsigned(bytes[0] << 24 | bytes[1] << 16 | bytes[2] << 8 | bytes[3], 32); }

function packIEEE754(v, ebits, fbits) {

  var bias = (1 << (ebits - 1)) - 1,
      s, e, f, ln,
      i, bits, str, bytes;

  function roundToEven(n) {
    var w = floor(n), f = n - w;
    if (f < 0.5)
      return w;
    if (f > 0.5)
      return w + 1;
    return w % 2 ? w + 1 : w;
  }

  // Compute sign, exponent, fraction
  if (v !== v) {
    // NaN
    // http://dev.w3.org/2006/webapi/WebIDL/#es-type-mapping
    e = (1 << ebits) - 1; f = pow(2, fbits - 1); s = 0;
  } else if (v === Infinity || v === -Infinity) {
    e = (1 << ebits) - 1; f = 0; s = (v < 0) ? 1 : 0;
  } else if (v === 0) {
    e = 0; f = 0; s = (1 / v === -Infinity) ? 1 : 0;
  } else {
    s = v < 0;
    v = abs(v);

    if (v >= pow(2, 1 - bias)) {
      e = min(floor(log(v) / LN2), 1023);
      f = roundToEven(v / pow(2, e) * pow(2, fbits));
      if (f / pow(2, fbits) >= 2) {
        e = e + 1;
        f = 1;
      }
      if (e > bias) {
        // Overflow
        e = (1 << ebits) - 1;
        f = 0;
      } else {
        // Normalized
        e = e + bias;
        f = f - pow(2, fbits);
      }
    } else {
      // Denormalized
      e = 0;
      f = roundToEven(v / pow(2, 1 - bias - fbits));
    }
  }

  // Pack sign, exponent, fraction
  bits = [];
  for (i = fbits; i; i -= 1) { bits.push(f % 2 ? 1 : 0); f = floor(f / 2); }
  for (i = ebits; i; i -= 1) { bits.push(e % 2 ? 1 : 0); e = floor(e / 2); }
  bits.push(s ? 1 : 0);
  bits.reverse();
  str = bits.join('');

  // Bits to bytes
  bytes = [];
  while (str.length) {
    bytes.push(parseInt(str.substring(0, 8), 2));
    str = str.substring(8);
  }
  return bytes;
}

function unpackIEEE754(bytes, ebits, fbits) {

  // Bytes to bits
  var bits = [], i, j, b, str,
      bias, s, e, f;

  for (i = bytes.length; i; i -= 1) {
    b = bytes[i - 1];
    for (j = 8; j; j -= 1) {
      bits.push(b % 2 ? 1 : 0); b = b >> 1;
    }
  }
  bits.reverse();
  str = bits.join('');

  // Unpack sign, exponent, fraction
  bias = (1 << (ebits - 1)) - 1;
  s = parseInt(str.substring(0, 1), 2) ? -1 : 1;
  e = parseInt(str.substring(1, 1 + ebits), 2);
  f = parseInt(str.substring(1 + ebits), 2);

  // Produce number
  if (e === (1 << ebits) - 1) {
    return f !== 0 ? NaN : s * Infinity;
  } else if (e > 0) {
    // Normalized
    return s * pow(2, e - bias) * (1 + f / pow(2, fbits));
  } else if (f !== 0) {
    // Denormalized
    return s * pow(2, -(bias - 1)) * (f / pow(2, fbits));
  } else {
    return s < 0 ? -0 : 0;
  }
}

function unpackF64(b) { return unpackIEEE754(b, 11, 52); }
function packF64(v) { return packIEEE754(v, 11, 52); }
function unpackF32(b) { return unpackIEEE754(b, 8, 23); }
function packF32(v) { return packIEEE754(v, 8, 23); }


//
// 3 The ArrayBuffer Type
//

(function() {

  /** @constructor */
  var ArrayBuffer = function ArrayBuffer(length) {
    length = ECMAScript.ToInt32(length);
    if (length < 0) throw new RangeError('ArrayBuffer size is not a small enough positive integer');

    this.byteLength = length;
    this._bytes = [];
    this._bytes.length = length;

    var i;
    for (i = 0; i < this.byteLength; i += 1) {
      this._bytes[i] = 0;
    }

    configureProperties(this);
  };

  exports.ArrayBuffer = exports.ArrayBuffer || ArrayBuffer;

  //
  // 4 The ArrayBufferView Type
  //

  // NOTE: this constructor is not exported
  /** @constructor */
  var ArrayBufferView = function ArrayBufferView() {
    //this.buffer = null;
    //this.byteOffset = 0;
    //this.byteLength = 0;
  };

  //
  // 5 The Typed Array View Types
  //

  function makeConstructor(bytesPerElement, pack, unpack) {
    // Each TypedArray type requires a distinct constructor instance with
    // identical logic, which this produces.

    var ctor;
    ctor = function(buffer, byteOffset, length) {
      var array, sequence, i, s;

      if (!arguments.length || typeof arguments[0] === 'number') {
        // Constructor(unsigned long length)
        this.length = ECMAScript.ToInt32(arguments[0]);
        if (length < 0) throw new RangeError('ArrayBufferView size is not a small enough positive integer');

        this.byteLength = this.length * this.BYTES_PER_ELEMENT;
        this.buffer = new ArrayBuffer(this.byteLength);
        this.byteOffset = 0;
      } else if (typeof arguments[0] === 'object' && arguments[0].constructor === ctor) {
        // Constructor(TypedArray array)
        array = arguments[0];

        this.length = array.length;
        this.byteLength = this.length * this.BYTES_PER_ELEMENT;
        this.buffer = new ArrayBuffer(this.byteLength);
        this.byteOffset = 0;

        for (i = 0; i < this.length; i += 1) {
          this._setter(i, array._getter(i));
        }
      } else if (typeof arguments[0] === 'object' &&
                 !(arguments[0] instanceof ArrayBuffer || ECMAScript.Class(arguments[0]) === 'ArrayBuffer')) {
        // Constructor(sequence<type> array)
        sequence = arguments[0];

        this.length = ECMAScript.ToUint32(sequence.length);
        this.byteLength = this.length * this.BYTES_PER_ELEMENT;
        this.buffer = new ArrayBuffer(this.byteLength);
        this.byteOffset = 0;

        for (i = 0; i < this.length; i += 1) {
          s = sequence[i];
          this._setter(i, Number(s));
        }
      } else if (typeof arguments[0] === 'object' &&
                 (arguments[0] instanceof ArrayBuffer || ECMAScript.Class(arguments[0]) === 'ArrayBuffer')) {
        // Constructor(ArrayBuffer buffer,
        //             optional unsigned long byteOffset, optional unsigned long length)
        this.buffer = buffer;

        this.byteOffset = ECMAScript.ToUint32(byteOffset);
        if (this.byteOffset > this.buffer.byteLength) {
          throw new RangeError("byteOffset out of range");
        }

        if (this.byteOffset % this.BYTES_PER_ELEMENT) {
          // The given byteOffset must be a multiple of the element
          // size of the specific type, otherwise an exception is raised.
          throw new RangeError("ArrayBuffer length minus the byteOffset is not a multiple of the element size.");
        }

        if (arguments.length < 3) {
          this.byteLength = this.buffer.byteLength - this.byteOffset;

          if (this.byteLength % this.BYTES_PER_ELEMENT) {
            throw new RangeError("length of buffer minus byteOffset not a multiple of the element size");
          }
          this.length = this.byteLength / this.BYTES_PER_ELEMENT;
        } else {
          this.length = ECMAScript.ToUint32(length);
          this.byteLength = this.length * this.BYTES_PER_ELEMENT;
        }

        if ((this.byteOffset + this.byteLength) > this.buffer.byteLength) {
          throw new RangeError("byteOffset and length reference an area beyond the end of the buffer");
        }
      } else {
        throw new TypeError("Unexpected argument type(s)");
      }

      this.constructor = ctor;

      configureProperties(this);
      makeArrayAccessors(this);
    };

    ctor.prototype = new ArrayBufferView();
    ctor.prototype.BYTES_PER_ELEMENT = bytesPerElement;
    ctor.prototype._pack = pack;
    ctor.prototype._unpack = unpack;
    ctor.BYTES_PER_ELEMENT = bytesPerElement;

    // getter type (unsigned long index);
    ctor.prototype._getter = function(index) {
      if (arguments.length < 1) throw new SyntaxError("Not enough arguments");

      index = ECMAScript.ToUint32(index);
      if (index >= this.length) {
        return undefined;
      }

      var bytes = [], i, o;
      for (i = 0, o = this.byteOffset + index * this.BYTES_PER_ELEMENT;
           i < this.BYTES_PER_ELEMENT;
           i += 1, o += 1) {
        bytes.push(this.buffer._bytes[o]);
      }
      return this._unpack(bytes);
    };

    // NONSTANDARD: convenience alias for getter: type get(unsigned long index);
    ctor.prototype.get = ctor.prototype._getter;

    // setter void (unsigned long index, type value);
    ctor.prototype._setter = function(index, value) {
      if (arguments.length < 2) throw new SyntaxError("Not enough arguments");

      index = ECMAScript.ToUint32(index);
      if (index >= this.length) {
        return undefined;
      }

      var bytes = this._pack(value), i, o;
      for (i = 0, o = this.byteOffset + index * this.BYTES_PER_ELEMENT;
           i < this.BYTES_PER_ELEMENT;
           i += 1, o += 1) {
        this.buffer._bytes[o] = bytes[i];
      }
    };

    // void set(TypedArray array, optional unsigned long offset);
    // void set(sequence<type> array, optional unsigned long offset);
    ctor.prototype.set = function(index, value) {
      if (arguments.length < 1) throw new SyntaxError("Not enough arguments");
      var array, sequence, offset, len,
          i, s, d,
          byteOffset, byteLength, tmp;

      if (typeof arguments[0] === 'object' && arguments[0].constructor === this.constructor) {
        // void set(TypedArray array, optional unsigned long offset);
        array = arguments[0];
        offset = ECMAScript.ToUint32(arguments[1]);

        if (offset + array.length > this.length) {
          throw new RangeError("Offset plus length of array is out of range");
        }

        byteOffset = this.byteOffset + offset * this.BYTES_PER_ELEMENT;
        byteLength = array.length * this.BYTES_PER_ELEMENT;

        if (array.buffer === this.buffer) {
          tmp = [];
          for (i = 0, s = array.byteOffset; i < byteLength; i += 1, s += 1) {
            tmp[i] = array.buffer._bytes[s];
          }
          for (i = 0, d = byteOffset; i < byteLength; i += 1, d += 1) {
            this.buffer._bytes[d] = tmp[i];
          }
        } else {
          for (i = 0, s = array.byteOffset, d = byteOffset;
               i < byteLength; i += 1, s += 1, d += 1) {
            this.buffer._bytes[d] = array.buffer._bytes[s];
          }
        }
      } else if (typeof arguments[0] === 'object' && typeof arguments[0].length !== 'undefined') {
        // void set(sequence<type> array, optional unsigned long offset);
        sequence = arguments[0];
        len = ECMAScript.ToUint32(sequence.length);
        offset = ECMAScript.ToUint32(arguments[1]);

        if (offset + len > this.length) {
          throw new RangeError("Offset plus length of array is out of range");
        }

        for (i = 0; i < len; i += 1) {
          s = sequence[i];
          this._setter(offset + i, Number(s));
        }
      } else {
        throw new TypeError("Unexpected argument type(s)");
      }
    };

    // TypedArray subarray(long begin, optional long end);
    ctor.prototype.subarray = function(start, end) {
      function clamp(v, min, max) { return v < min ? min : v > max ? max : v; }

      start = ECMAScript.ToInt32(start);
      end = ECMAScript.ToInt32(end);

      if (arguments.length < 1) { start = 0; }
      if (arguments.length < 2) { end = this.length; }

      if (start < 0) { start = this.length + start; }
      if (end < 0) { end = this.length + end; }

      start = clamp(start, 0, this.length);
      end = clamp(end, 0, this.length);

      var len = end - start;
      if (len < 0) {
        len = 0;
      }

      return new this.constructor(
        this.buffer, this.byteOffset + start * this.BYTES_PER_ELEMENT, len);
    };

    return ctor;
  }

  var Int8Array = makeConstructor(1, packI8, unpackI8);
  var Uint8Array = makeConstructor(1, packU8, unpackU8);
  var Uint8ClampedArray = makeConstructor(1, packU8Clamped, unpackU8);
  var Int16Array = makeConstructor(2, packI16, unpackI16);
  var Uint16Array = makeConstructor(2, packU16, unpackU16);
  var Int32Array = makeConstructor(4, packI32, unpackI32);
  var Uint32Array = makeConstructor(4, packU32, unpackU32);
  var Float32Array = makeConstructor(4, packF32, unpackF32);
  var Float64Array = makeConstructor(8, packF64, unpackF64);

  exports.Int8Array = exports.Int8Array || Int8Array;
  exports.Uint8Array = exports.Uint8Array || Uint8Array;
  exports.Uint8ClampedArray = exports.Uint8ClampedArray || Uint8ClampedArray;
  exports.Int16Array = exports.Int16Array || Int16Array;
  exports.Uint16Array = exports.Uint16Array || Uint16Array;
  exports.Int32Array = exports.Int32Array || Int32Array;
  exports.Uint32Array = exports.Uint32Array || Uint32Array;
  exports.Float32Array = exports.Float32Array || Float32Array;
  exports.Float64Array = exports.Float64Array || Float64Array;
}());

//
// 6 The DataView View Type
//

(function() {
  function r(array, index) {
    return ECMAScript.IsCallable(array.get) ? array.get(index) : array[index];
  }

  var IS_BIG_ENDIAN = (function() {
    var u16array = new(exports.Uint16Array)([0x1234]),
        u8array = new(exports.Uint8Array)(u16array.buffer);
    return r(u8array, 0) === 0x12;
  }());

  // Constructor(ArrayBuffer buffer,
  //             optional unsigned long byteOffset,
  //             optional unsigned long byteLength)
  /** @constructor */
  var DataView = function DataView(buffer, byteOffset, byteLength) {
    if (arguments.length === 0) {
      buffer = new exports.ArrayBuffer(0);
    } else if (!(buffer instanceof exports.ArrayBuffer || ECMAScript.Class(buffer) === 'ArrayBuffer')) {
      throw new TypeError("TypeError");
    }

    this.buffer = buffer || new exports.ArrayBuffer(0);

    this.byteOffset = ECMAScript.ToUint32(byteOffset);
    if (this.byteOffset > this.buffer.byteLength) {
      throw new RangeError("byteOffset out of range");
    }

    if (arguments.length < 3) {
      this.byteLength = this.buffer.byteLength - this.byteOffset;
    } else {
      this.byteLength = ECMAScript.ToUint32(byteLength);
    }

    if ((this.byteOffset + this.byteLength) > this.buffer.byteLength) {
      throw new RangeError("byteOffset and length reference an area beyond the end of the buffer");
    }

    configureProperties(this);
  };

  function makeGetter(arrayType) {
    return function(byteOffset, littleEndian) {

      byteOffset = ECMAScript.ToUint32(byteOffset);

      if (byteOffset + arrayType.BYTES_PER_ELEMENT > this.byteLength) {
        throw new RangeError("Array index out of range");
      }
      byteOffset += this.byteOffset;

      var uint8Array = new exports.Uint8Array(this.buffer, byteOffset, arrayType.BYTES_PER_ELEMENT),
          bytes = [], i;
      for (i = 0; i < arrayType.BYTES_PER_ELEMENT; i += 1) {
        bytes.push(r(uint8Array, i));
      }

      if (Boolean(littleEndian) === Boolean(IS_BIG_ENDIAN)) {
        bytes.reverse();
      }

      return r(new arrayType(new exports.Uint8Array(bytes).buffer), 0);
    };
  }

  DataView.prototype.getUint8 = makeGetter(exports.Uint8Array);
  DataView.prototype.getInt8 = makeGetter(exports.Int8Array);
  DataView.prototype.getUint16 = makeGetter(exports.Uint16Array);
  DataView.prototype.getInt16 = makeGetter(exports.Int16Array);
  DataView.prototype.getUint32 = makeGetter(exports.Uint32Array);
  DataView.prototype.getInt32 = makeGetter(exports.Int32Array);
  DataView.prototype.getFloat32 = makeGetter(exports.Float32Array);
  DataView.prototype.getFloat64 = makeGetter(exports.Float64Array);

  function makeSetter(arrayType) {
    return function(byteOffset, value, littleEndian) {

      byteOffset = ECMAScript.ToUint32(byteOffset);
      if (byteOffset + arrayType.BYTES_PER_ELEMENT > this.byteLength) {
        throw new RangeError("Array index out of range");
      }

      // Get bytes
      var typeArray = new arrayType([value]),
          byteArray = new exports.Uint8Array(typeArray.buffer),
          bytes = [], i, byteView;

      for (i = 0; i < arrayType.BYTES_PER_ELEMENT; i += 1) {
        bytes.push(r(byteArray, i));
      }

      // Flip if necessary
      if (Boolean(littleEndian) === Boolean(IS_BIG_ENDIAN)) {
        bytes.reverse();
      }

      // Write them
      byteView = new exports.Uint8Array(this.buffer, byteOffset, arrayType.BYTES_PER_ELEMENT);
      byteView.set(bytes);
    };
  }

  DataView.prototype.setUint8 = makeSetter(exports.Uint8Array);
  DataView.prototype.setInt8 = makeSetter(exports.Int8Array);
  DataView.prototype.setUint16 = makeSetter(exports.Uint16Array);
  DataView.prototype.setInt16 = makeSetter(exports.Int16Array);
  DataView.prototype.setUint32 = makeSetter(exports.Uint32Array);
  DataView.prototype.setInt32 = makeSetter(exports.Int32Array);
  DataView.prototype.setFloat32 = makeSetter(exports.Float32Array);
  DataView.prototype.setFloat64 = makeSetter(exports.Float64Array);

  exports.DataView = exports.DataView || DataView;

}());
});

var typedarray_1 = typedarray.ArrayBuffer;
var typedarray_2 = typedarray.Int8Array;
var typedarray_3 = typedarray.Uint8Array;
var typedarray_4 = typedarray.Uint8ClampedArray;
var typedarray_5 = typedarray.Int16Array;
var typedarray_6 = typedarray.Uint16Array;
var typedarray_7 = typedarray.Int32Array;
var typedarray_8 = typedarray.Uint32Array;
var typedarray_9 = typedarray.Float32Array;
var typedarray_10 = typedarray.Float64Array;
var typedarray_11 = typedarray.DataView;

var Writable = stream.Writable;



if (typeof Uint8Array === 'undefined') {
  var U8 = typedarray.Uint8Array;
} else {
  var U8 = Uint8Array;
}

function ConcatStream(opts, cb) {
  if (!(this instanceof ConcatStream)) return new ConcatStream(opts, cb)

  if (typeof opts === 'function') {
    cb = opts;
    opts = {};
  }
  if (!opts) opts = {};

  var encoding = opts.encoding;
  var shouldInferEncoding = false;

  if (!encoding) {
    shouldInferEncoding = true;
  } else {
    encoding =  String(encoding).toLowerCase();
    if (encoding === 'u8' || encoding === 'uint8') {
      encoding = 'uint8array';
    }
  }

  Writable.call(this, { objectMode: true });

  this.encoding = encoding;
  this.shouldInferEncoding = shouldInferEncoding;

  if (cb) this.on('finish', function () { cb(this.getBody()); });
  this.body = [];
}

var concatStream = ConcatStream;
inherits(ConcatStream, Writable);

ConcatStream.prototype._write = function(chunk, enc, next) {
  this.body.push(chunk);
  next();
};

ConcatStream.prototype.inferEncoding = function (buff) {
  var firstBuffer = buff === undefined ? this.body[0] : buff;
  if (Buffer.isBuffer(firstBuffer)) return 'buffer'
  if (typeof Uint8Array !== 'undefined' && firstBuffer instanceof Uint8Array) return 'uint8array'
  if (Array.isArray(firstBuffer)) return 'array'
  if (typeof firstBuffer === 'string') return 'string'
  if (Object.prototype.toString.call(firstBuffer) === "[object Object]") return 'object'
  return 'buffer'
};

ConcatStream.prototype.getBody = function () {
  if (!this.encoding && this.body.length === 0) return []
  if (this.shouldInferEncoding) this.encoding = this.inferEncoding();
  if (this.encoding === 'array') return arrayConcat(this.body)
  if (this.encoding === 'string') return stringConcat(this.body)
  if (this.encoding === 'buffer') return bufferConcat(this.body)
  if (this.encoding === 'uint8array') return u8Concat(this.body)
  return this.body
};

function isArrayish$3 (arr) {
  return /Array\]$/.test(Object.prototype.toString.call(arr))
}

function isBufferish (p) {
  return typeof p === 'string' || isArrayish$3(p) || (p && typeof p.subarray === 'function')
}

function stringConcat (parts) {
  var strings = [];
  for (var i = 0; i < parts.length; i++) {
    var p = parts[i];
    if (typeof p === 'string') {
      strings.push(p);
    } else if (Buffer.isBuffer(p)) {
      strings.push(p);
    } else if (isBufferish(p)) {
      strings.push(bufferFrom_1(p));
    } else {
      strings.push(bufferFrom_1(String(p)));
    }
  }
  if (Buffer.isBuffer(parts[0])) {
    strings = Buffer.concat(strings);
    strings = strings.toString('utf8');
  } else {
    strings = strings.join('');
  }
  return strings
}

function bufferConcat (parts) {
  var bufs = [];
  for (var i = 0; i < parts.length; i++) {
    var p = parts[i];
    if (Buffer.isBuffer(p)) {
      bufs.push(p);
    } else if (isBufferish(p)) {
      bufs.push(bufferFrom_1(p));
    } else {
      bufs.push(bufferFrom_1(String(p)));
    }
  }
  return Buffer.concat(bufs)
}

function arrayConcat (parts) {
  var res = [];
  for (var i = 0; i < parts.length; i++) {
    res.push.apply(res, parts[i]);
  }
  return res
}

function u8Concat (parts) {
  var len = 0;
  for (var i = 0; i < parts.length; i++) {
    if (typeof parts[i] === 'string') {
      parts[i] = bufferFrom_1(parts[i]);
    }
    len += parts[i].length;
  }
  var u8 = new U8(len);
  for (var i = 0, offset = 0; i < parts.length; i++) {
    var part = parts[i];
    for (var j = 0; j < part.length; j++) {
      u8[offset++] = part[j];
    }
  }
  return u8
}

var debug$3 = src('unified-engine:file-set-pipeline:stdin');



var stdin_1 = stdin;

function stdin(context, settings, next) {
  var streamIn = settings.streamIn;
  var err;

  if (settings.files && settings.files.length !== 0) {
    debug$3('Ignoring `streamIn`');

    if (settings.filePath) {
      err = new Error(
        'Do not pass both `--file-path` and real files.\n' +
        'Did you mean to pass stdin instead of files?'
      );
    }

    next(err);

    return;
  }

  if (streamIn.isTTY) {
    debug$3('Cannot read from `tty` stream');
    next(new Error('No input'));

    return;
  }

  debug$3('Reading from `streamIn`');

  streamIn.pipe(concatStream({encoding: 'string'}, read));

  function read(value) {
    var file = toVfile(settings.filePath || undefined);

    debug$3('Read from `streamIn`');

    file.cwd = settings.cwd;
    file.contents = value;
    file.data.unifiedEngineGiven = true;
    file.data.unifiedEngineStreamIn = true;

    context.files = [file];

    /* If `out` wasn’t set, set `out`. */
    settings.out = settings.out === null || settings.out === undefined ? true : settings.out;

    next();
  }
}

var inherits$3 = util.inherits;




var fileSet = FileSet;

/* FileSet constructor. */
function FileSet() {
  var self = this;

  self.files = [];
  self.origins = [];

  self.expected = 0;
  self.actual = 0;

  self.pipeline = trough_1();
  self.plugins = [];

  events.init.call(self);

  self.on('one', one.bind(self));
}

/* Events. */
inherits$3(FileSet, events.EventEmitter);

/* Expose methods. */
FileSet.prototype.valueOf = valueOf;
FileSet.prototype.use = use;
FileSet.prototype.add = add;

/* Create an array representation of `fileSet`. */
function valueOf() {
  return this.files;
}

/* Attach middleware to the pipeline on `fileSet`. */
function use(plugin) {
  var self = this;
  var pipeline = self.pipeline;
  var duplicate = false;

  if (plugin && plugin.pluginId) {
    duplicate = self.plugins.some(function (fn) {
      return fn.pluginId === plugin.pluginId;
    });
  }

  if (!duplicate && self.plugins.indexOf(plugin) !== -1) {
    duplicate = true;
  }

  if (!duplicate) {
    self.plugins.push(plugin);
    pipeline.use(plugin);
  }

  return this;
}

/* Add a file to be processed.
 *
 * Ignores duplicate files (based on the `filePath` at time
 * of addition).
 *
 * Only runs `file-pipeline` on files which have not
 * `failed` before addition. */
function add(file) {
  var self = this;
  var origin;

  if (xIsString(file)) {
    file = toVfile(file);
  }

  /* Prevent files from being added multiple times. */
  origin = file.history[0];

  if (self.origins.indexOf(origin) !== -1) {
    return self;
  }

  self.origins.push(origin);

  /* Add. */
  self.valueOf().push(file);
  self.expected++;

  /* Force an asynchronous operation.
   * This ensures that files which fall through
   * the file pipeline immediately (e.g., when
   * already fatally failed) still queue up
   * correctly. */
  setImmediate(function () {
    self.emit('add', file);
  });

  return self;
}

/* Utility invoked when a single file has completed it's
 * pipeline, triggering `done` when all files are complete. */
function one() {
  var self = this;

  self.actual++;

  if (self.actual >= self.expected) {
    self.emit('done');
  }
}

var debug$4 = src('unified-engine:file-pipeline:read');


var read_1 = read$2;

var resolve$4 = path.resolve;
var readFile = fs.readFile;

/* Fill a file with its contents when not already filled. */
function read$2(context, file, fileSet, next) {
  var filePath = file.path;

  if (file.contents || file.data.unifiedEngineStreamIn) {
    debug$4('Not reading file `%s` with contents', filePath);
    next();
  } else if (vfileStatistics(file).fatal) {
    debug$4('Not reading failed file `%s`', filePath);
    next();
  } else {
    filePath = resolve$4(context.cwd, filePath);

    debug$4('Reading `%s` in `%s`', filePath, 'utf8');

    readFile(filePath, 'utf8', function (err, contents) {
      debug$4('Read `%s` (err: %s)', filePath, err);

      file.contents = contents || '';

      next(err);
    });
  }
}

var fnName = function (fn) {
	if (typeof fn !== 'function') {
		throw new TypeError('Expected a function');
	}

	return fn.displayName || fn.name || (/function ([^\(]+)?\(/.exec(fn.toString()) || [])[1] || null;
};

/**
 * Has own property.
 *
 * @type {Function}
 */

var has = Object.prototype.hasOwnProperty;

/**
 * To string.
 *
 * @type {Function}
 */

var toString$3 = Object.prototype.toString;

/**
 * Test whether a value is "empty".
 *
 * @param {Mixed} val
 * @return {Boolean}
 */

function isEmpty(val) {
  // Null and Undefined...
  if (val == null) return true

  // Booleans...
  if ('boolean' == typeof val) return false

  // Numbers...
  if ('number' == typeof val) return val === 0

  // Strings...
  if ('string' == typeof val) return val.length === 0

  // Functions...
  if ('function' == typeof val) return val.length === 0

  // Arrays...
  if (Array.isArray(val)) return val.length === 0

  // Errors...
  if (val instanceof Error) return val.message === ''

  // Objects...
  if (val.toString == toString$3) {
    switch (val.toString()) {

      // Maps, Sets, Files and Errors...
      case '[object File]':
      case '[object Map]':
      case '[object Set]': {
        return val.size === 0
      }

      // Plain objects...
      case '[object Object]': {
        for (var key in val) {
          if (has.call(val, key)) return false
        }

        return true
      }
    }
  }

  // Anything else...
  return false
}

/**
 * Export `isEmpty`.
 *
 * @type {Function}
 */

var lib = isEmpty;

var debug$5 = src('unified-engine:file-pipeline:configure');





var configure_1$2 = configure$1;

/* Collect configuration for a file based on the context. */
function configure$1(context, file, fileSet, next) {
  var config = context.configuration;
  var processor = context.processor;

  if (vfileStatistics(file).fatal) {
    return next();
  }

  config.load(file.path, handleConfiguration);

  function handleConfiguration(err, configuration) {
    var plugins;
    var options;
    var plugin;
    var length;
    var index;
    var name;

    if (err) {
      return next(err);
    }

    /* Store configuration on the context object. */
    debug$5('Using settings `%j`', configuration.settings);
    processor.data('settings', configuration.settings);

    plugins = configuration.plugins;
    length = plugins.length;
    index = -1;

    debug$5('Using `%d` plugins', length);

    while (++index < length) {
      plugin = plugins[index][0];
      options = plugins[index][1];

      if (options === false) {
        continue;
      }

      /* Allow for default arguments in es2020. */
      if (options === null || (isObject$1(options) && lib(options))) {
        options = undefined;
      }

      name = fnName(plugin) || 'function';
      debug$5('Using plug-in `%s`, with options `%j`', name, options);

      try {
        processor.use(plugin, options, fileSet);
      } catch (err) {
        /* istanbul ignore next - Shouldn’t happen anymore! */
        return next(err);
      }
    }

    next();
  }
}

var debug$6 = src('unified-engine:file-pipeline:parse');



var parse_1 = parse$3;

/* Fill a file with a tree. */
function parse$3(context, file) {
  var message;

  if (vfileStatistics(file).fatal) {
    return;
  }

  if (context.treeIn) {
    debug$6('Not parsing already parsed document');

    try {
      context.tree = parseJson$1(file.toString());
    } catch (err) {
      message = file.message(new Error('Cannot read file as JSON\n' + err.message));
      message.fatal = true;
    }

    /* Add the preferred extension to ensure the file, when compiled, is
     * correctly recognized. Only add it if there’s a path — not if the
     * file is for example stdin. */
    if (file.path) {
      file.extname = context.extensions[0];
    }

    file.contents = '';

    return;
  }

  debug$6('Parsing `%s`', file.path);

  context.tree = context.processor.parse(file);

  debug$6('Parsed document');
}

var debug$7 = src('unified-engine:file-pipeline:transform');


var transform_1 = transform;

/* Transform the tree associated with a file with
 * configured plug-ins. */
function transform(context, file, fileSet, next) {
  if (vfileStatistics(file).fatal) {
    next();
    return;
  }

  debug$7('Transforming document `%s`', file.path);

  context.processor.run(context.tree, file, function (err, node) {
    debug$7('Transformed document (error: %s)', err);
    context.tree = node;
    next(err);
  });
}

var debug$8 = src('unified-engine:file-pipeline:queue');



var queue_1 = queue;

/* Queue all files which came this far.
 * When the last file gets here, run the file-set pipeline
 * and flush the queue. */
function queue(context, file, fileSet, next) {
  var origin = file.history[0];
  var map = fileSet.complete;
  var complete = true;

  if (!map) {
    map = {};
    fileSet.complete = map;
  }

  debug$8('Queueing `%s`', origin);

  map[origin] = next;

  fileSet.valueOf().forEach(each);

  if (!complete) {
    debug$8('Not flushing: some files cannot be flushed');
    return;
  }

  fileSet.complete = {};

  fileSet.pipeline.run(fileSet, done);

  function each(file) {
    var key = file.history[0];

    if (vfileStatistics(file).fatal) {
      return;
    }

    if (xIsFunction(map[key])) {
      debug$8('`%s` can be flushed', key);
    } else {
      debug$8('Interupting flush: `%s` is not finished', key);
      complete = false;
    }
  }

  function done(err) {
    debug$8('Flushing: all files can be flushed');

    /* Flush. */
    for (origin in map) {
      map[origin](err);
    }
  }
}

/* Detect color support. */

var color = true;

try {
  color = 'inspect' in util;
} catch (err) {
  /* istanbul ignore next - browser */
  color = false;
}

var unistUtilInspect = color ? inspect : /* istanbul ignore next */ noColor;

inspect.color = inspect;
noColor.color = inspect;
inspect.noColor = noColor;
noColor.noColor = noColor;

var dim = ansiColor(2, 22);
var yellow = ansiColor(33, 39);
var green = ansiColor(32, 39);

/* Define ANSII color removal functionality. */
var COLOR_EXPRESSION = new RegExp(
  '(?:' +
    '(?:\\u001b\\[)|' +
    '\\u009b' +
    ')' +
    '(?:' +
    '(?:[0-9]{1,3})?(?:(?:;[0-9]{0,3})*)?[A-M|f-m]' +
    ')|' +
    '\\u001b[A-M]',
  'g'
);

/* Standard keys defined by unist:
 * https://github.com/syntax-tree/unist.
 * We don’t ignore `data` though. */
var ignore$4 = ['type', 'value', 'children', 'position'];

/* Inspects a node, without using color. */
function noColor(node, pad) {
  return stripColor(inspect(node, pad))
}

/* Inspects a node. */
function inspect(node, pad) {
  var result;
  var children;
  var index;
  var length;

  if (node && Boolean(node.length) && typeof node !== 'string') {
    length = node.length;
    index = -1;
    result = [];

    while (++index < length) {
      result[index] = inspect(node[index]);
    }

    return result.join('\n')
  }

  if (!node || !node.type) {
    return String(node)
  }

  result = [formatNode(node)];
  children = node.children;
  length = children && children.length;
  index = -1;

  if (!length) {
    return result[0]
  }

  if (!pad || typeof pad === 'number') {
    pad = '';
  }

  while (++index < length) {
    node = children[index];

    if (index === length - 1) {
      result.push(formatNesting(pad + '└─ ') + inspect(node, pad + '   '));
    } else {
      result.push(formatNesting(pad + '├─ ') + inspect(node, pad + '│  '));
    }
  }

  return result.join('\n')
}

/* Colored nesting formatter. */
function formatNesting(value) {
  return dim(value)
}

/* Compile a single position. */
function compile(pos) {
  var values = [];

  if (!pos) {
    return null
  }

  values = [[pos.line || 1, pos.column || 1].join(':')];

  if ('offset' in pos) {
    values.push(String(pos.offset || 0));
  }

  return values
}

/* Compile a location. */
function stringify$2(start, end) {
  var values = [];
  var positions = [];
  var offsets = [];

  add(start);
  add(end);

  if (positions.length !== 0) {
    values.push(positions.join('-'));
  }

  if (offsets.length !== 0) {
    values.push(offsets.join('-'));
  }

  return values.join(', ')

  /* Add a position. */
  function add(position) {
    var tuple = compile(position);

    if (tuple) {
      positions.push(tuple[0]);

      if (tuple[1]) {
        offsets.push(tuple[1]);
      }
    }
  }
}

/* Colored node formatter. */
function formatNode(node) {
  var log = node.type;
  var location = node.position || {};
  var position = stringify$2(location.start, location.end);
  var key;
  var values = [];
  var value;

  if (node.children) {
    log += dim('[') + yellow(node.children.length) + dim(']');
  } else if (typeof node.value === 'string') {
    log += dim(': ') + green(JSON.stringify(node.value));
  }

  if (position) {
    log += ' (' + position + ')';
  }

  for (key in node) {
    value = node[key];

    if (
      ignore$4.indexOf(key) !== -1 ||
      value === null ||
      value === undefined ||
      (typeof value === 'object' && lib(value))
    ) {
      continue
    }

    values.push('[' + key + '=' + JSON.stringify(value) + ']');
  }

  if (values.length !== 0) {
    log += ' ' + values.join('');
  }

  return log
}

/* Remove ANSI colour from `value`. */
function stripColor(value) {
  return value.replace(COLOR_EXPRESSION, '')
}

/* Factory to wrap values in ANSI colours. */
function ansiColor(open, close) {
  return color

  function color(value) {
    return '\u001B[' + open + 'm' + value + '\u001B[' + close + 'm'
  }
}

var debug$9 = src('unified-engine:file-pipeline:stringify');



var stringify_1 = stringify$3;

/* Stringify a tree. */
function stringify$3(context, file) {
  var processor = context.processor;
  var tree = context.tree;
  var value;

  if (vfileStatistics(file).fatal) {
    debug$9('Not compiling failed document');
    return;
  }

  if (!context.output && !context.out && !context.alwaysStringify) {
    debug$9('Not compiling document without output settings');
    return;
  }

  debug$9('Compiling `%s`', file.path);

  if (context.inspect) {
    /* Add a `txt` extension if there’s a path. */
    if (file.path) {
      file.extname = '.txt';
    }

    value = unistUtilInspect[context.color ? 'color' : 'noColor'](tree) + '\n';
  } else if (context.treeOut) {
    /* Add a `json` extension to ensure the file is correctly seen as JSON.
     * Only add it if there’s a path — not if the file is for example stdin. */
    if (file.path) {
      file.extname = '.json';
    }

    /* Add the line break to create a valid UNIX file. */
    value = JSON.stringify(tree, null, 2) + '\n';
  } else {
    value = processor.stringify(tree, file);
  }

  file.contents = value;

  debug$9('Compiled document');
}

var debug$10 = src('unified-engine:file-pipeline:copy');


var copy_1 = copy;

var stat$1 = fs.stat;
var dirname$2 = path.dirname;
var resolve$5 = path.resolve;
var relative$3 = path.relative;

/* Move a file. */
function copy(context, file, fileSet, next) {
  var output = context.output;
  var multi = fileSet.expected > 1;
  var outpath = output;
  var currentPath = file.path;

  if (!xIsString(outpath)) {
    debug$10('Not copying');
    return next();
  }

  outpath = resolve$5(context.cwd, outpath);

  debug$10('Copying `%s`', currentPath);

  stat$1(outpath, onstatfile);

  function onstatfile(err, stats) {
    if (err) {
      if (err.code !== 'ENOENT' || output.charAt(output.length - 1) === path.sep) {
        return next(new Error('Cannot read output directory. Error:\n' + err.message));
      }

      stat$1(dirname$2(outpath), onstatparent);
    } else {
      done(stats.isDirectory());
    }
  }

  /* This is either given an error, or the parent exists which
   * is a directory, but we should keep the basename of the
   * given file. */
  function onstatparent(err) {
    if (err) {
      next(new Error('Cannot read parent directory. Error:\n' + err.message));
    } else {
      done(false);
    }
  }

  function done(directory) {
    if (!directory && multi) {
      return next(new Error('Cannot write multiple files to single output: ' + outpath));
    }

    file[directory ? 'dirname' : 'path'] = relative$3(file.cwd, outpath);

    debug$10('Copying document from %s to %s', currentPath, file.path);

    next();
  }
}

var debug$11 = src('unified-engine:file-pipeline:stdout');


var stdout_1 = stdout;

/* Write a virtual file to `streamOut`.
 * Ignored when `output` is given, more than one file
 * was processed, or `out` is false. */
function stdout(context, file, fileSet, next) {
  if (!file.data.unifiedEngineGiven) {
    debug$11('Ignoring programmatically added file');
    next();
  } else if (vfileStatistics(file).fatal || context.output || !context.out) {
    debug$11('Ignoring writing to `streamOut`');
    next();
  } else {
    debug$11('Writing document to `streamOut`');
    context.streamOut.write(file.toString(), next);
  }
}

var debug$12 = src('unified-engine:file-pipeline:file-system');

var fileSystem_1$2 = fileSystem$1;

var writeFile = fs.writeFile;
var resolve$6 = path.resolve;

/* Write a virtual file to the file-system.
 * Ignored when `output` is not given. */
function fileSystem$1(context, file, fileSet, next) {
  var destinationPath;

  if (!context.output) {
    debug$12('Ignoring writing to file-system');
    return next();
  }

  if (!file.data.unifiedEngineGiven) {
    debug$12('Ignoring programmatically added file');
    return next();
  }

  destinationPath = file.path;

  if (!destinationPath) {
    debug$12('Cannot write file without a `destinationPath`');
    return next(new Error('Cannot write file without an output path '));
  }

  destinationPath = resolve$6(context.cwd, destinationPath);
  debug$12('Writing document to `%s`', destinationPath);

  file.stored = true;

  writeFile(destinationPath, file.toString(), next);
}

/* Expose: This pipeline ensures each of the pipes
 * always runs: even if the read pipe fails,
 * queue and write trigger. */
var filePipeline = trough_1()
  .use(chunk(trough_1().use(read_1).use(configure_1$2).use(parse_1).use(transform_1)))
  .use(chunk(trough_1().use(queue_1)))
  .use(chunk(trough_1().use(stringify_1).use(copy_1).use(stdout_1).use(fileSystem_1$2)));

/* Factory to run a pipe. Wraps a pipe to trigger an
 * error on the `file` in `context`, but still call
 * `next`. */
function chunk(pipe) {
  return run;

  /* Run the bound bound pipe and handles any errors. */
  function run(context, file, fileSet, next) {
    pipe.run(context, file, fileSet, one);

    function one(err) {
      var messages = file.messages;
      var index;

      if (err) {
        index = messages.indexOf(err);

        if (index === -1) {
          err = file.message(err);
          index = messages.length - 1;
        }

        messages[index].fatal = true;
      }

      next();
    }
  }
}

var transform_1$2 = transform$2;

/* Transform all files. */
function transform$2(context, settings, next) {
  var fileSet$$1 = new fileSet();

  context.fileSet = fileSet$$1;

  fileSet$$1.on('add', add).on('done', next);

  if (context.files.length === 0) {
    next();
  } else {
    context.files.forEach(fileSet$$1.add, fileSet$$1);
  }

  function add(file) {
    filePipeline.run({
      configuration: context.configuration,
      processor: settings.processor(),
      cwd: settings.cwd,
      extensions: settings.extensions,
      pluginPrefix: settings.pluginPrefix,
      treeIn: settings.treeIn,
      treeOut: settings.treeOut,
      inspect: settings.inspect,
      color: settings.color,
      out: settings.out,
      output: settings.output,
      streamOut: settings.streamOut,
      alwaysStringify: settings.alwaysStringify
    }, file, fileSet$$1, done);

    function done(err) {
      /* istanbul ignore next - doesn’t occur as all
       * failures in `filePipeLine` are failed on each
       * file.  Still, just to ensure things work in
       * the future, we add an extra check. */
      if (err) {
        err = file.message(err);
        err.fatal = true;
      }

      fileSet$$1.emit('one', file);
    }
  }
}

var ansiRegex = function () {
	return /[\u001b\u009b][[()#;?]*(?:[0-9]{1,4}(?:;[0-9]{0,4})*)?[0-9A-PRZcf-nqry=><]/g;
};

var ansiRegex$2 = ansiRegex();

var stripAnsi = function (str) {
	return typeof str === 'string' ? str.replace(ansiRegex$2, '') : str;
};

/* eslint-disable babel/new-cap, xo/throw-new-error */
var codePointAt = function (str, pos) {
	if (str === null || str === undefined) {
		throw TypeError();
	}

	str = String(str);

	var size = str.length;
	var i = pos ? Number(pos) : 0;

	if (Number.isNaN(i)) {
		i = 0;
	}

	if (i < 0 || i >= size) {
		return undefined;
	}

	var first = str.charCodeAt(i);

	if (first >= 0xD800 && first <= 0xDBFF && size > i + 1) {
		var second = str.charCodeAt(i + 1);

		if (second >= 0xDC00 && second <= 0xDFFF) {
			return ((first - 0xD800) * 0x400) + second - 0xDC00 + 0x10000;
		}
	}

	return first;
};

var numberIsNan = Number.isNaN || function (x) {
	return x !== x;
};

var isFullwidthCodePoint = function (x) {
	if (numberIsNan(x)) {
		return false;
	}

	// https://github.com/nodejs/io.js/blob/cff7300a578be1b10001f2d967aaedc88aee6402/lib/readline.js#L1369

	// code points are derived from:
	// http://www.unix.org/Public/UNIDATA/EastAsianWidth.txt
	if (x >= 0x1100 && (
		x <= 0x115f ||  // Hangul Jamo
		0x2329 === x || // LEFT-POINTING ANGLE BRACKET
		0x232a === x || // RIGHT-POINTING ANGLE BRACKET
		// CJK Radicals Supplement .. Enclosed CJK Letters and Months
		(0x2e80 <= x && x <= 0x3247 && x !== 0x303f) ||
		// Enclosed CJK Letters and Months .. CJK Unified Ideographs Extension A
		0x3250 <= x && x <= 0x4dbf ||
		// CJK Unified Ideographs .. Yi Radicals
		0x4e00 <= x && x <= 0xa4c6 ||
		// Hangul Jamo Extended-A
		0xa960 <= x && x <= 0xa97c ||
		// Hangul Syllables
		0xac00 <= x && x <= 0xd7a3 ||
		// CJK Compatibility Ideographs
		0xf900 <= x && x <= 0xfaff ||
		// Vertical Forms
		0xfe10 <= x && x <= 0xfe19 ||
		// CJK Compatibility Forms .. Small Form Variants
		0xfe30 <= x && x <= 0xfe6b ||
		// Halfwidth and Fullwidth Forms
		0xff01 <= x && x <= 0xff60 ||
		0xffe0 <= x && x <= 0xffe6 ||
		// Kana Supplement
		0x1b000 <= x && x <= 0x1b001 ||
		// Enclosed Ideographic Supplement
		0x1f200 <= x && x <= 0x1f251 ||
		// CJK Unified Ideographs Extension B .. Tertiary Ideographic Plane
		0x20000 <= x && x <= 0x3fffd)) {
		return true;
	}

	return false;
};

// https://github.com/nodejs/io.js/blob/cff7300a578be1b10001f2d967aaedc88aee6402/lib/readline.js#L1345
var stringWidth = function (str) {
	if (typeof str !== 'string' || str.length === 0) {
		return 0;
	}

	var width = 0;

	str = stripAnsi(str);

	for (var i = 0; i < str.length; i++) {
		var code = codePointAt(str, i);

		// ignore control characters
		if (code <= 0x1f || (code >= 0x7f && code <= 0x9f)) {
			continue;
		}

		// surrogates
		if (code >= 0x10000) {
			i++;
		}

		if (isFullwidthCodePoint(code)) {
			width += 2;
		} else {
			width++;
		}
	}

	return width;
};

/*!
 * repeat-string <https://github.com/jonschlinkert/repeat-string>
 *
 * Copyright (c) 2014-2015, Jon Schlinkert.
 * Licensed under the MIT License.
 */

/**
 * Results cache
 */

var res = '';
var cache$1;

/**
 * Expose `repeat`
 */

var repeatString = repeat$1;

/**
 * Repeat the given `string` the specified `number`
 * of times.
 *
 * **Example:**
 *
 * ```js
 * var repeat = require('repeat-string');
 * repeat('A', 5);
 * //=> AAAAA
 * ```
 *
 * @param {String} `string` The string to repeat
 * @param {Number} `number` The number of times to repeat the string
 * @return {String} Repeated string
 * @api public
 */

function repeat$1(str, num) {
  if (typeof str !== 'string') {
    throw new TypeError('expected a string');
  }

  // cover common, quick use cases
  if (num === 1) return str;
  if (num === 2) return str + str;

  var max = str.length * num;
  if (cache$1 !== str || typeof cache$1 === 'undefined') {
    cache$1 = str;
    res = '';
  } else if (res.length >= max) {
    return res.substr(0, max);
  }

  while (max > res.length && num > 1) {
    if (num & 1) {
      res += str;
    }

    num >>= 1;
    str += str;
  }

  res += str;
  res = res.substr(0, max);
  return res;
}

var supported = supportsColor.hasBasic;





var vfileReporter = reporter;

/* Check which characters should be used. */
var windows$1 = process.platform === 'win32';
/* `log-symbols` without chalk: */
/* istanbul ignore next - Windows. */
var chars = windows$1 ? {error: '×', warning: '‼'} : {error: '✖', warning: '⚠'};

/* Match trailing white-space. */
var trailing = /\s*$/;

/* Default filename. */
var DEFAULT = '<stdin>';

var noop = {open: '', close: ''};

var colors = {
  underline: {open: '\u001b[4m', close: '\u001b[24m'},
  red: {open: '\u001b[31m', close: '\u001b[39m'},
  yellow: {open: '\u001b[33m', close: '\u001b[39m'},
  green: {open: '\u001b[32m', close: '\u001b[39m'}
};

var noops = {
  underline: noop,
  red: noop,
  yellow: noop,
  green: noop
};

var labels = {
  true: 'error',
  false: 'warning',
  null: 'info',
  undefined: 'info'
};

/* Report a file’s messages. */
function reporter(files, options) {
  var settings = options || {};
  var one;

  if (!files) {
    return '';
  }

  /* Error. */
  if ('name' in files && 'message' in files) {
    return String(files.stack || files);
  }

  /* One file. */
  if (!('length' in files)) {
    one = true;
    files = [files];
  }

  return compile$1(parse$5(filter$2(files, settings), settings), one, settings);
}

function filter$2(files, options) {
  var result = [];
  var length = files.length;
  var index = -1;
  var file;

  if (!options.quiet && !options.silent) {
    return files.concat();
  }

  while (++index < length) {
    file = files[index];

    if (applicable(file, options).length !== 0) {
      result.push(file);
    }
  }

  return result;
}

function parse$5(files, options) {
  var length = files.length;
  var index = -1;
  var rows = [];
  var all = [];
  var locationSize = 0;
  var labelSize = 0;
  var reasonSize = 0;
  var ruleIdSize = 0;
  var file;
  var destination;
  var origin;
  var messages;
  var offset;
  var count;
  var message;
  var loc;
  var reason;
  var label;
  var id;

  while (++index < length) {
    file = files[index];
    destination = current(file);
    origin = file.history[0] || destination;
    messages = applicable(file, options).sort(comparator);

    if (rows.length !== 0 && rows[rows.length - 1].type !== 'header') {
      rows.push({type: 'separator'});
    }

    rows.push({
      type: 'header',
      origin: origin,
      destination: destination,
      name: origin || options.defaultName || DEFAULT,
      stored: Boolean(file.stored),
      moved: Boolean(file.stored && destination !== origin),
      stats: vfileStatistics(messages)
    });

    offset = -1;
    count = messages.length;

    while (++offset < count) {
      message = messages[offset];
      id = message.ruleId || '';
      reason = message.stack || message.message;
      loc = message.location;
      loc = unistUtilStringifyPosition(loc.end.line && loc.end.column ? loc : loc.start);

      if (options.verbose && message.note) {
        reason += '\n' + message.note;
      }

      label = labels[message.fatal];

      rows.push({
        location: loc,
        label: label,
        reason: reason,
        ruleId: id,
        source: message.source
      });

      locationSize = Math.max(realLength(loc), locationSize);
      labelSize = Math.max(realLength(label), labelSize);
      reasonSize = Math.max(realLength(reason), reasonSize);
      ruleIdSize = Math.max(realLength(id), ruleIdSize);
    }

    all = all.concat(messages);
  }

  return {
    rows: rows,
    statistics: vfileStatistics(all),
    location: locationSize,
    label: labelSize,
    reason: reasonSize,
    ruleId: ruleIdSize
  };
}

function compile$1(map, one, options) {
  var enabled = options.color;
  var all = map.statistics;
  var rows = map.rows;
  var length = rows.length;
  var index = -1;
  var lines = [];
  var row;
  var line;
  var style;
  var color;

  if (enabled === null || enabled === undefined) {
    enabled = supported;
  }

  style = enabled ? colors : noops;

  while (++index < length) {
    row = rows[index];

    if (row.type === 'separator') {
      lines.push('');
    } else if (row.type === 'header') {
      if (one && !options.defaultName && !row.origin) {
        line = '';
      } else {
        color = style[row.stats.fatal ? 'red' : (row.stats.total ? 'yellow' : 'green')];
        line = style.underline.open + color.open + row.name + color.close + style.underline.close;
        line += row.moved ? ' > ' + row.destination : '';
      }

      if (!row.stats.total) {
        line += line ? ': ' : '';

        if (row.stored) {
          line += style.yellow.open + 'written' + style.yellow.close;
        } else {
          line += 'no issues found';
        }
      }

      if (line) {
        lines.push(line);
      }
    } else {
      color = style[row.label === 'error' ? 'red' : 'yellow'];

      lines.push([
        '',
        padLeft(row.location, map.location),
        padRight(color.open + row.label + color.close, map.label),
        padRight(row.reason, map.reason),
        padRight(row.ruleId, map.ruleId),
        row.source || ''
      ].join('  ').replace(trailing, ''));
    }
  }

  if (all.fatal || all.warn) {
    line = [];

    if (all.fatal) {
      line.push([
        style.red.open + chars.error + style.red.close,
        all.fatal,
        plural$1(labels.true, all.fatal)
      ].join(' '));
    }

    if (all.warn) {
      line.push([
        style.yellow.open + chars.warning + style.yellow.close,
        all.warn,
        plural$1(labels.false, all.warn)
      ].join(' '));
    }

    line = line.join(', ');

    if (all.total !== all.fatal && all.total !== all.warn) {
      line = all.total + ' messages (' + line + ')';
    }

    lines.push('', line);
  }

  return lines.join('\n');
}

function applicable(file, options) {
  var messages = file.messages;
  var length = messages.length;
  var index = -1;
  var result = [];

  if (options.silent) {
    while (++index < length) {
      if (messages[index].fatal) {
        result.push(messages[index]);
      }
    }
  } else {
    result = messages.concat();
  }

  return result;
}

/* Get the length of `value`, ignoring ANSI sequences. */
function realLength(value) {
  var length = value.indexOf('\n');
  return stringWidth(length === -1 ? value : value.slice(0, length));
}

/* Pad `value` on the left. */
function padLeft(value, minimum) {
  return repeatString(' ', minimum - realLength(value)) + value;
}

/* Pad `value` on the Right. */
function padRight(value, minimum) {
  return value + repeatString(' ', minimum - realLength(value));
}

/* Comparator. */
function comparator(a, b) {
  return check$1(a, b, 'line') || check$1(a, b, 'column') || -1;
}

/* Compare a single property. */
function check$1(a, b, property) {
  return (a[property] || 0) - (b[property] || 0);
}

function current(file) {
  /* istanbul ignore if - Previous `vfile` version. */
  if (file.filePath) {
    return file.filePath();
  }

  return file.path;
}

function plural$1(value, count) {
  return count === 1 ? value : value + 's';
}

var log_1 = log;

var prefix$1 = 'vfile-reporter';

function log(context, settings, next) {
  var reporter = settings.reporter || vfileReporter;
  var diagnostics;

  if (xIsString(reporter)) {
    try {
      reporter = loadPlugin_1(reporter, {cwd: settings.cwd, prefix: prefix$1});
    } catch (err) {
      next(new Error('Could not find reporter `' + reporter + '`'));
      return;
    }
  }

  diagnostics = reporter(context.files.filter(given), immutable(settings.reporterOptions, {
    quiet: settings.quiet,
    silent: settings.silent,
    color: settings.color
  }));

  if (diagnostics) {
    if (diagnostics.charAt(diagnostics.length - 1) !== '\n') {
      diagnostics += '\n';
    }

    settings.streamError.write(diagnostics, next);
  } else {
    next();
  }
}

function given(file) {
  return file.data.unifiedEngineGiven;
}

var fileSetPipeline = trough_1()
  .use(configure_1)
  .use(fileSystem_1)
  .use(stdin_1)
  .use(transform_1$2)
  .use(log_1);

var PassThrough = stream.PassThrough;



var lib$2 = run;

/* Run the file set pipeline once.
 * `callback` is invoked with a fatal error,
 * or with a status code (`0` on success, `1` on failure). */
function run(options, callback) {
  var settings = {};
  var stdin = new PassThrough();
  var tree;
  var detectConfig;
  var hasConfig;
  var detectIgnore;
  var hasIgnore;

  try {
    stdin = process.stdin;
  } catch (err) {
    /* Obscure bug in Node (seen on windows):
     * - https://github.com/nodejs/node/blob/f856234/lib/internal/
     *   process/stdio.js#L82;
     * - https://github.com/AtomLinter/linter-markdown/pull/85.
     */
  }

  if (!callback) {
    throw new Error('Missing `callback`');
  }

  if (!options || !options.processor) {
    return next(new Error('Missing `processor`'));
  }

  /* Processor. */
  settings.processor = options.processor;

  /* Path to run as. */
  settings.cwd = options.cwd || process.cwd();

  /* Input. */
  settings.files = options.files || [];
  settings.extensions = (options.extensions || []).map(function (extension) {
    return extension.charAt(0) === '.' ? extension : '.' + extension;
  });

  settings.filePath = options.filePath || null;
  settings.streamIn = options.streamIn || stdin;

  /* Output. */
  settings.streamOut = options.streamOut || process.stdout;
  settings.streamError = options.streamError || process.stderr;
  settings.alwaysStringify = options.alwaysStringify;
  settings.output = options.output;
  settings.out = options.out;

  /* Null overwrites config settings, `undefined` doesn’t. */
  if (settings.output === null || settings.output === undefined) {
    settings.output = undefined;
  }

  if (settings.output && settings.out) {
    return next(new Error('Cannot accept both `output` and `out`'));
  }

  /* Process phase management. */
  tree = options.tree || false;

  settings.treeIn = options.treeIn;
  settings.treeOut = options.treeOut;
  settings.inspect = options.inspect;

  if (settings.treeIn === null || settings.treeIn === undefined) {
    settings.treeIn = tree;
  }

  if (settings.treeOut === null || settings.treeOut === undefined) {
    settings.treeOut = tree;
  }

  /* Configuration. */
  detectConfig = options.detectConfig;
  hasConfig = Boolean(options.rcName || options.packageField);

  if (detectConfig && !hasConfig) {
    return next(new Error(
      'Missing `rcName` or `packageField` with `detectConfig`'
    ));
  }

  settings.detectConfig = detectConfig === null || detectConfig === undefined ? hasConfig : detectConfig;
  settings.rcName = options.rcName || null;
  settings.rcPath = options.rcPath || null;
  settings.packageField = options.packageField || null;
  settings.settings = options.settings || {};
  settings.configTransform = options.configTransform;
  settings.defaultConfig = options.defaultConfig;

  /* Ignore. */
  detectIgnore = options.detectIgnore;
  hasIgnore = Boolean(options.ignoreName);

  settings.detectIgnore = detectIgnore === null || detectIgnore === undefined ? hasIgnore : detectIgnore;
  settings.ignoreName = options.ignoreName || null;
  settings.ignorePath = options.ignorePath || null;
  settings.silentlyIgnore = Boolean(options.silentlyIgnore);

  if (detectIgnore && !hasIgnore) {
    return next(new Error('Missing `ignoreName` with `detectIgnore`'));
  }

  /* Plug-ins. */
  settings.pluginPrefix = options.pluginPrefix || null;
  settings.plugins = options.plugins || {};

  /* Reporting. */
  settings.reporter = options.reporter || null;
  settings.reporterOptions = options.reporterOptions || null;
  settings.color = options.color || false;
  settings.silent = options.silent || false;
  settings.quiet = options.quiet || false;
  settings.frail = options.frail || false;

  /* Process. */
  fileSetPipeline.run({files: options.files || []}, settings, next);

  function next(err, context) {
    var stats = vfileStatistics((context || {}).files);
    var failed = Boolean(settings.frail ? stats.fatal || stats.warn : stats.fatal);

    if (err) {
      callback(err);
    } else {
      callback(null, failed ? 1 : 0, context);
    }
  }
}

var textTable = function (rows_, opts) {
    if (!opts) opts = {};
    var hsep = opts.hsep === undefined ? '  ' : opts.hsep;
    var align = opts.align || [];
    var stringLength = opts.stringLength
        || function (s) { return String(s).length; };

    var dotsizes = reduce(rows_, function (acc, row) {
        forEach(row, function (c, ix) {
            var n = dotindex(c);
            if (!acc[ix] || n > acc[ix]) acc[ix] = n;
        });
        return acc;
    }, []);

    var rows = map$2(rows_, function (row) {
        return map$2(row, function (c_, ix) {
            var c = String(c_);
            if (align[ix] === '.') {
                var index = dotindex(c);
                var size = dotsizes[ix] + (/\./.test(c) ? 1 : 2)
                    - (stringLength(c) - index);
                return c + Array(size).join(' ');
            }
            else return c;
        });
    });

    var sizes = reduce(rows, function (acc, row) {
        forEach(row, function (c, ix) {
            var n = stringLength(c);
            if (!acc[ix] || n > acc[ix]) acc[ix] = n;
        });
        return acc;
    }, []);

    return map$2(rows, function (row) {
        return map$2(row, function (c, ix) {
            var n = (sizes[ix] - stringLength(c)) || 0;
            var s = Array(Math.max(n + 1, 1)).join(' ');
            if (align[ix] === 'r' || align[ix] === '.') {
                return s + c;
            }
            if (align[ix] === 'c') {
                return Array(Math.ceil(n / 2 + 1)).join(' ')
                    + c + Array(Math.floor(n / 2 + 1)).join(' ')
                ;
            }

            return c + s;
        }).join(hsep).replace(/\s+$/, '');
    }).join('\n');
};

function dotindex (c) {
    var m = /\.[^.]*$/.exec(c);
    return m ? m.index + 1 : c.length;
}

function reduce (xs, f, init) {
    if (xs.reduce) return xs.reduce(f, init);
    var i = 0;
    var acc = arguments.length >= 3 ? init : xs[i++];
    for (; i < xs.length; i++) {
        f(acc, xs[i], i);
    }
    return acc;
}

function forEach (xs, f) {
    if (xs.forEach) return xs.forEach(f);
    for (var i = 0; i < xs.length; i++) {
        f.call(xs, xs[i], i);
    }
}

function map$2 (xs, f) {
    if (xs.map) return xs.map(f);
    var res = [];
    for (var i = 0; i < xs.length; i++) {
        res.push(f.call(xs, xs[i], i));
    }
    return res;
}

var camelcase = createCommonjsModule(function (module) {
const preserveCamelCase = input => {
	let isLastCharLower = false;
	let isLastCharUpper = false;
	let isLastLastCharUpper = false;

	for (let i = 0; i < input.length; i++) {
		const c = input[i];

		if (isLastCharLower && /[a-zA-Z]/.test(c) && c.toUpperCase() === c) {
			input = input.slice(0, i) + '-' + input.slice(i);
			isLastCharLower = false;
			isLastLastCharUpper = isLastCharUpper;
			isLastCharUpper = true;
			i++;
		} else if (isLastCharUpper && isLastLastCharUpper && /[a-zA-Z]/.test(c) && c.toLowerCase() === c) {
			input = input.slice(0, i - 1) + '-' + input.slice(i - 1);
			isLastLastCharUpper = isLastCharUpper;
			isLastCharUpper = false;
			isLastCharLower = true;
		} else {
			isLastCharLower = c.toLowerCase() === c;
			isLastLastCharUpper = isLastCharUpper;
			isLastCharUpper = c.toUpperCase() === c;
		}
	}

	return input;
};

module.exports = (input, options) => {
	options = Object.assign({
		pascalCase: false
	}, options);

	const postProcess = x => options.pascalCase ? x.charAt(0).toUpperCase() + x.slice(1) : x;

	if (Array.isArray(input)) {
		input = input.map(x => x.trim())
			.filter(x => x.length)
			.join('-');
	} else {
		input = input.trim();
	}

	if (input.length === 0) {
		return '';
	}

	if (input.length === 1) {
		return options.pascalCase ? input.toUpperCase() : input.toLowerCase();
	}

	if (/^[a-z\d]+$/.test(input)) {
		return postProcess(input);
	}

	const hasUpperCase = input !== input.toLowerCase();

	if (hasUpperCase) {
		input = preserveCamelCase(input);
	}

	input = input
		.replace(/^[_.\- ]+/, '')
		.toLowerCase()
		.replace(/[_.\- ]+(\w|$)/g, (m, p1) => p1.toUpperCase());

	return postProcess(input);
};
});

var unicode = createCommonjsModule(function (module, exports) {
Object.defineProperty(exports,"__esModule",{value:true});var Space_Separator=exports.Space_Separator=/[\u1680\u2000-\u200A\u202F\u205F\u3000]/;var ID_Start=exports.ID_Start=/[\xAA\xB5\xBA\xC0-\xD6\xD8-\xF6\xF8-\u02C1\u02C6-\u02D1\u02E0-\u02E4\u02EC\u02EE\u0370-\u0374\u0376\u0377\u037A-\u037D\u037F\u0386\u0388-\u038A\u038C\u038E-\u03A1\u03A3-\u03F5\u03F7-\u0481\u048A-\u052F\u0531-\u0556\u0559\u0561-\u0587\u05D0-\u05EA\u05F0-\u05F2\u0620-\u064A\u066E\u066F\u0671-\u06D3\u06D5\u06E5\u06E6\u06EE\u06EF\u06FA-\u06FC\u06FF\u0710\u0712-\u072F\u074D-\u07A5\u07B1\u07CA-\u07EA\u07F4\u07F5\u07FA\u0800-\u0815\u081A\u0824\u0828\u0840-\u0858\u08A0-\u08B4\u08B6-\u08BD\u0904-\u0939\u093D\u0950\u0958-\u0961\u0971-\u0980\u0985-\u098C\u098F\u0990\u0993-\u09A8\u09AA-\u09B0\u09B2\u09B6-\u09B9\u09BD\u09CE\u09DC\u09DD\u09DF-\u09E1\u09F0\u09F1\u0A05-\u0A0A\u0A0F\u0A10\u0A13-\u0A28\u0A2A-\u0A30\u0A32\u0A33\u0A35\u0A36\u0A38\u0A39\u0A59-\u0A5C\u0A5E\u0A72-\u0A74\u0A85-\u0A8D\u0A8F-\u0A91\u0A93-\u0AA8\u0AAA-\u0AB0\u0AB2\u0AB3\u0AB5-\u0AB9\u0ABD\u0AD0\u0AE0\u0AE1\u0AF9\u0B05-\u0B0C\u0B0F\u0B10\u0B13-\u0B28\u0B2A-\u0B30\u0B32\u0B33\u0B35-\u0B39\u0B3D\u0B5C\u0B5D\u0B5F-\u0B61\u0B71\u0B83\u0B85-\u0B8A\u0B8E-\u0B90\u0B92-\u0B95\u0B99\u0B9A\u0B9C\u0B9E\u0B9F\u0BA3\u0BA4\u0BA8-\u0BAA\u0BAE-\u0BB9\u0BD0\u0C05-\u0C0C\u0C0E-\u0C10\u0C12-\u0C28\u0C2A-\u0C39\u0C3D\u0C58-\u0C5A\u0C60\u0C61\u0C80\u0C85-\u0C8C\u0C8E-\u0C90\u0C92-\u0CA8\u0CAA-\u0CB3\u0CB5-\u0CB9\u0CBD\u0CDE\u0CE0\u0CE1\u0CF1\u0CF2\u0D05-\u0D0C\u0D0E-\u0D10\u0D12-\u0D3A\u0D3D\u0D4E\u0D54-\u0D56\u0D5F-\u0D61\u0D7A-\u0D7F\u0D85-\u0D96\u0D9A-\u0DB1\u0DB3-\u0DBB\u0DBD\u0DC0-\u0DC6\u0E01-\u0E30\u0E32\u0E33\u0E40-\u0E46\u0E81\u0E82\u0E84\u0E87\u0E88\u0E8A\u0E8D\u0E94-\u0E97\u0E99-\u0E9F\u0EA1-\u0EA3\u0EA5\u0EA7\u0EAA\u0EAB\u0EAD-\u0EB0\u0EB2\u0EB3\u0EBD\u0EC0-\u0EC4\u0EC6\u0EDC-\u0EDF\u0F00\u0F40-\u0F47\u0F49-\u0F6C\u0F88-\u0F8C\u1000-\u102A\u103F\u1050-\u1055\u105A-\u105D\u1061\u1065\u1066\u106E-\u1070\u1075-\u1081\u108E\u10A0-\u10C5\u10C7\u10CD\u10D0-\u10FA\u10FC-\u1248\u124A-\u124D\u1250-\u1256\u1258\u125A-\u125D\u1260-\u1288\u128A-\u128D\u1290-\u12B0\u12B2-\u12B5\u12B8-\u12BE\u12C0\u12C2-\u12C5\u12C8-\u12D6\u12D8-\u1310\u1312-\u1315\u1318-\u135A\u1380-\u138F\u13A0-\u13F5\u13F8-\u13FD\u1401-\u166C\u166F-\u167F\u1681-\u169A\u16A0-\u16EA\u16EE-\u16F8\u1700-\u170C\u170E-\u1711\u1720-\u1731\u1740-\u1751\u1760-\u176C\u176E-\u1770\u1780-\u17B3\u17D7\u17DC\u1820-\u1877\u1880-\u1884\u1887-\u18A8\u18AA\u18B0-\u18F5\u1900-\u191E\u1950-\u196D\u1970-\u1974\u1980-\u19AB\u19B0-\u19C9\u1A00-\u1A16\u1A20-\u1A54\u1AA7\u1B05-\u1B33\u1B45-\u1B4B\u1B83-\u1BA0\u1BAE\u1BAF\u1BBA-\u1BE5\u1C00-\u1C23\u1C4D-\u1C4F\u1C5A-\u1C7D\u1C80-\u1C88\u1CE9-\u1CEC\u1CEE-\u1CF1\u1CF5\u1CF6\u1D00-\u1DBF\u1E00-\u1F15\u1F18-\u1F1D\u1F20-\u1F45\u1F48-\u1F4D\u1F50-\u1F57\u1F59\u1F5B\u1F5D\u1F5F-\u1F7D\u1F80-\u1FB4\u1FB6-\u1FBC\u1FBE\u1FC2-\u1FC4\u1FC6-\u1FCC\u1FD0-\u1FD3\u1FD6-\u1FDB\u1FE0-\u1FEC\u1FF2-\u1FF4\u1FF6-\u1FFC\u2071\u207F\u2090-\u209C\u2102\u2107\u210A-\u2113\u2115\u2119-\u211D\u2124\u2126\u2128\u212A-\u212D\u212F-\u2139\u213C-\u213F\u2145-\u2149\u214E\u2160-\u2188\u2C00-\u2C2E\u2C30-\u2C5E\u2C60-\u2CE4\u2CEB-\u2CEE\u2CF2\u2CF3\u2D00-\u2D25\u2D27\u2D2D\u2D30-\u2D67\u2D6F\u2D80-\u2D96\u2DA0-\u2DA6\u2DA8-\u2DAE\u2DB0-\u2DB6\u2DB8-\u2DBE\u2DC0-\u2DC6\u2DC8-\u2DCE\u2DD0-\u2DD6\u2DD8-\u2DDE\u2E2F\u3005-\u3007\u3021-\u3029\u3031-\u3035\u3038-\u303C\u3041-\u3096\u309D-\u309F\u30A1-\u30FA\u30FC-\u30FF\u3105-\u312D\u3131-\u318E\u31A0-\u31BA\u31F0-\u31FF\u3400-\u4DB5\u4E00-\u9FD5\uA000-\uA48C\uA4D0-\uA4FD\uA500-\uA60C\uA610-\uA61F\uA62A\uA62B\uA640-\uA66E\uA67F-\uA69D\uA6A0-\uA6EF\uA717-\uA71F\uA722-\uA788\uA78B-\uA7AE\uA7B0-\uA7B7\uA7F7-\uA801\uA803-\uA805\uA807-\uA80A\uA80C-\uA822\uA840-\uA873\uA882-\uA8B3\uA8F2-\uA8F7\uA8FB\uA8FD\uA90A-\uA925\uA930-\uA946\uA960-\uA97C\uA984-\uA9B2\uA9CF\uA9E0-\uA9E4\uA9E6-\uA9EF\uA9FA-\uA9FE\uAA00-\uAA28\uAA40-\uAA42\uAA44-\uAA4B\uAA60-\uAA76\uAA7A\uAA7E-\uAAAF\uAAB1\uAAB5\uAAB6\uAAB9-\uAABD\uAAC0\uAAC2\uAADB-\uAADD\uAAE0-\uAAEA\uAAF2-\uAAF4\uAB01-\uAB06\uAB09-\uAB0E\uAB11-\uAB16\uAB20-\uAB26\uAB28-\uAB2E\uAB30-\uAB5A\uAB5C-\uAB65\uAB70-\uABE2\uAC00-\uD7A3\uD7B0-\uD7C6\uD7CB-\uD7FB\uF900-\uFA6D\uFA70-\uFAD9\uFB00-\uFB06\uFB13-\uFB17\uFB1D\uFB1F-\uFB28\uFB2A-\uFB36\uFB38-\uFB3C\uFB3E\uFB40\uFB41\uFB43\uFB44\uFB46-\uFBB1\uFBD3-\uFD3D\uFD50-\uFD8F\uFD92-\uFDC7\uFDF0-\uFDFB\uFE70-\uFE74\uFE76-\uFEFC\uFF21-\uFF3A\uFF41-\uFF5A\uFF66-\uFFBE\uFFC2-\uFFC7\uFFCA-\uFFCF\uFFD2-\uFFD7\uFFDA-\uFFDC]|\uD800[\uDC00-\uDC0B\uDC0D-\uDC26\uDC28-\uDC3A\uDC3C\uDC3D\uDC3F-\uDC4D\uDC50-\uDC5D\uDC80-\uDCFA\uDD40-\uDD74\uDE80-\uDE9C\uDEA0-\uDED0\uDF00-\uDF1F\uDF30-\uDF4A\uDF50-\uDF75\uDF80-\uDF9D\uDFA0-\uDFC3\uDFC8-\uDFCF\uDFD1-\uDFD5]|\uD801[\uDC00-\uDC9D\uDCB0-\uDCD3\uDCD8-\uDCFB\uDD00-\uDD27\uDD30-\uDD63\uDE00-\uDF36\uDF40-\uDF55\uDF60-\uDF67]|\uD802[\uDC00-\uDC05\uDC08\uDC0A-\uDC35\uDC37\uDC38\uDC3C\uDC3F-\uDC55\uDC60-\uDC76\uDC80-\uDC9E\uDCE0-\uDCF2\uDCF4\uDCF5\uDD00-\uDD15\uDD20-\uDD39\uDD80-\uDDB7\uDDBE\uDDBF\uDE00\uDE10-\uDE13\uDE15-\uDE17\uDE19-\uDE33\uDE60-\uDE7C\uDE80-\uDE9C\uDEC0-\uDEC7\uDEC9-\uDEE4\uDF00-\uDF35\uDF40-\uDF55\uDF60-\uDF72\uDF80-\uDF91]|\uD803[\uDC00-\uDC48\uDC80-\uDCB2\uDCC0-\uDCF2]|\uD804[\uDC03-\uDC37\uDC83-\uDCAF\uDCD0-\uDCE8\uDD03-\uDD26\uDD50-\uDD72\uDD76\uDD83-\uDDB2\uDDC1-\uDDC4\uDDDA\uDDDC\uDE00-\uDE11\uDE13-\uDE2B\uDE80-\uDE86\uDE88\uDE8A-\uDE8D\uDE8F-\uDE9D\uDE9F-\uDEA8\uDEB0-\uDEDE\uDF05-\uDF0C\uDF0F\uDF10\uDF13-\uDF28\uDF2A-\uDF30\uDF32\uDF33\uDF35-\uDF39\uDF3D\uDF50\uDF5D-\uDF61]|\uD805[\uDC00-\uDC34\uDC47-\uDC4A\uDC80-\uDCAF\uDCC4\uDCC5\uDCC7\uDD80-\uDDAE\uDDD8-\uDDDB\uDE00-\uDE2F\uDE44\uDE80-\uDEAA\uDF00-\uDF19]|\uD806[\uDCA0-\uDCDF\uDCFF\uDEC0-\uDEF8]|\uD807[\uDC00-\uDC08\uDC0A-\uDC2E\uDC40\uDC72-\uDC8F]|\uD808[\uDC00-\uDF99]|\uD809[\uDC00-\uDC6E\uDC80-\uDD43]|[\uD80C\uD81C-\uD820\uD840-\uD868\uD86A-\uD86C\uD86F-\uD872][\uDC00-\uDFFF]|\uD80D[\uDC00-\uDC2E]|\uD811[\uDC00-\uDE46]|\uD81A[\uDC00-\uDE38\uDE40-\uDE5E\uDED0-\uDEED\uDF00-\uDF2F\uDF40-\uDF43\uDF63-\uDF77\uDF7D-\uDF8F]|\uD81B[\uDF00-\uDF44\uDF50\uDF93-\uDF9F\uDFE0]|\uD821[\uDC00-\uDFEC]|\uD822[\uDC00-\uDEF2]|\uD82C[\uDC00\uDC01]|\uD82F[\uDC00-\uDC6A\uDC70-\uDC7C\uDC80-\uDC88\uDC90-\uDC99]|\uD835[\uDC00-\uDC54\uDC56-\uDC9C\uDC9E\uDC9F\uDCA2\uDCA5\uDCA6\uDCA9-\uDCAC\uDCAE-\uDCB9\uDCBB\uDCBD-\uDCC3\uDCC5-\uDD05\uDD07-\uDD0A\uDD0D-\uDD14\uDD16-\uDD1C\uDD1E-\uDD39\uDD3B-\uDD3E\uDD40-\uDD44\uDD46\uDD4A-\uDD50\uDD52-\uDEA5\uDEA8-\uDEC0\uDEC2-\uDEDA\uDEDC-\uDEFA\uDEFC-\uDF14\uDF16-\uDF34\uDF36-\uDF4E\uDF50-\uDF6E\uDF70-\uDF88\uDF8A-\uDFA8\uDFAA-\uDFC2\uDFC4-\uDFCB]|\uD83A[\uDC00-\uDCC4\uDD00-\uDD43]|\uD83B[\uDE00-\uDE03\uDE05-\uDE1F\uDE21\uDE22\uDE24\uDE27\uDE29-\uDE32\uDE34-\uDE37\uDE39\uDE3B\uDE42\uDE47\uDE49\uDE4B\uDE4D-\uDE4F\uDE51\uDE52\uDE54\uDE57\uDE59\uDE5B\uDE5D\uDE5F\uDE61\uDE62\uDE64\uDE67-\uDE6A\uDE6C-\uDE72\uDE74-\uDE77\uDE79-\uDE7C\uDE7E\uDE80-\uDE89\uDE8B-\uDE9B\uDEA1-\uDEA3\uDEA5-\uDEA9\uDEAB-\uDEBB]|\uD869[\uDC00-\uDED6\uDF00-\uDFFF]|\uD86D[\uDC00-\uDF34\uDF40-\uDFFF]|\uD86E[\uDC00-\uDC1D\uDC20-\uDFFF]|\uD873[\uDC00-\uDEA1]|\uD87E[\uDC00-\uDE1D]/;var ID_Continue=exports.ID_Continue=/[\xAA\xB5\xBA\xC0-\xD6\xD8-\xF6\xF8-\u02C1\u02C6-\u02D1\u02E0-\u02E4\u02EC\u02EE\u0300-\u0374\u0376\u0377\u037A-\u037D\u037F\u0386\u0388-\u038A\u038C\u038E-\u03A1\u03A3-\u03F5\u03F7-\u0481\u0483-\u0487\u048A-\u052F\u0531-\u0556\u0559\u0561-\u0587\u0591-\u05BD\u05BF\u05C1\u05C2\u05C4\u05C5\u05C7\u05D0-\u05EA\u05F0-\u05F2\u0610-\u061A\u0620-\u0669\u066E-\u06D3\u06D5-\u06DC\u06DF-\u06E8\u06EA-\u06FC\u06FF\u0710-\u074A\u074D-\u07B1\u07C0-\u07F5\u07FA\u0800-\u082D\u0840-\u085B\u08A0-\u08B4\u08B6-\u08BD\u08D4-\u08E1\u08E3-\u0963\u0966-\u096F\u0971-\u0983\u0985-\u098C\u098F\u0990\u0993-\u09A8\u09AA-\u09B0\u09B2\u09B6-\u09B9\u09BC-\u09C4\u09C7\u09C8\u09CB-\u09CE\u09D7\u09DC\u09DD\u09DF-\u09E3\u09E6-\u09F1\u0A01-\u0A03\u0A05-\u0A0A\u0A0F\u0A10\u0A13-\u0A28\u0A2A-\u0A30\u0A32\u0A33\u0A35\u0A36\u0A38\u0A39\u0A3C\u0A3E-\u0A42\u0A47\u0A48\u0A4B-\u0A4D\u0A51\u0A59-\u0A5C\u0A5E\u0A66-\u0A75\u0A81-\u0A83\u0A85-\u0A8D\u0A8F-\u0A91\u0A93-\u0AA8\u0AAA-\u0AB0\u0AB2\u0AB3\u0AB5-\u0AB9\u0ABC-\u0AC5\u0AC7-\u0AC9\u0ACB-\u0ACD\u0AD0\u0AE0-\u0AE3\u0AE6-\u0AEF\u0AF9\u0B01-\u0B03\u0B05-\u0B0C\u0B0F\u0B10\u0B13-\u0B28\u0B2A-\u0B30\u0B32\u0B33\u0B35-\u0B39\u0B3C-\u0B44\u0B47\u0B48\u0B4B-\u0B4D\u0B56\u0B57\u0B5C\u0B5D\u0B5F-\u0B63\u0B66-\u0B6F\u0B71\u0B82\u0B83\u0B85-\u0B8A\u0B8E-\u0B90\u0B92-\u0B95\u0B99\u0B9A\u0B9C\u0B9E\u0B9F\u0BA3\u0BA4\u0BA8-\u0BAA\u0BAE-\u0BB9\u0BBE-\u0BC2\u0BC6-\u0BC8\u0BCA-\u0BCD\u0BD0\u0BD7\u0BE6-\u0BEF\u0C00-\u0C03\u0C05-\u0C0C\u0C0E-\u0C10\u0C12-\u0C28\u0C2A-\u0C39\u0C3D-\u0C44\u0C46-\u0C48\u0C4A-\u0C4D\u0C55\u0C56\u0C58-\u0C5A\u0C60-\u0C63\u0C66-\u0C6F\u0C80-\u0C83\u0C85-\u0C8C\u0C8E-\u0C90\u0C92-\u0CA8\u0CAA-\u0CB3\u0CB5-\u0CB9\u0CBC-\u0CC4\u0CC6-\u0CC8\u0CCA-\u0CCD\u0CD5\u0CD6\u0CDE\u0CE0-\u0CE3\u0CE6-\u0CEF\u0CF1\u0CF2\u0D01-\u0D03\u0D05-\u0D0C\u0D0E-\u0D10\u0D12-\u0D3A\u0D3D-\u0D44\u0D46-\u0D48\u0D4A-\u0D4E\u0D54-\u0D57\u0D5F-\u0D63\u0D66-\u0D6F\u0D7A-\u0D7F\u0D82\u0D83\u0D85-\u0D96\u0D9A-\u0DB1\u0DB3-\u0DBB\u0DBD\u0DC0-\u0DC6\u0DCA\u0DCF-\u0DD4\u0DD6\u0DD8-\u0DDF\u0DE6-\u0DEF\u0DF2\u0DF3\u0E01-\u0E3A\u0E40-\u0E4E\u0E50-\u0E59\u0E81\u0E82\u0E84\u0E87\u0E88\u0E8A\u0E8D\u0E94-\u0E97\u0E99-\u0E9F\u0EA1-\u0EA3\u0EA5\u0EA7\u0EAA\u0EAB\u0EAD-\u0EB9\u0EBB-\u0EBD\u0EC0-\u0EC4\u0EC6\u0EC8-\u0ECD\u0ED0-\u0ED9\u0EDC-\u0EDF\u0F00\u0F18\u0F19\u0F20-\u0F29\u0F35\u0F37\u0F39\u0F3E-\u0F47\u0F49-\u0F6C\u0F71-\u0F84\u0F86-\u0F97\u0F99-\u0FBC\u0FC6\u1000-\u1049\u1050-\u109D\u10A0-\u10C5\u10C7\u10CD\u10D0-\u10FA\u10FC-\u1248\u124A-\u124D\u1250-\u1256\u1258\u125A-\u125D\u1260-\u1288\u128A-\u128D\u1290-\u12B0\u12B2-\u12B5\u12B8-\u12BE\u12C0\u12C2-\u12C5\u12C8-\u12D6\u12D8-\u1310\u1312-\u1315\u1318-\u135A\u135D-\u135F\u1380-\u138F\u13A0-\u13F5\u13F8-\u13FD\u1401-\u166C\u166F-\u167F\u1681-\u169A\u16A0-\u16EA\u16EE-\u16F8\u1700-\u170C\u170E-\u1714\u1720-\u1734\u1740-\u1753\u1760-\u176C\u176E-\u1770\u1772\u1773\u1780-\u17D3\u17D7\u17DC\u17DD\u17E0-\u17E9\u180B-\u180D\u1810-\u1819\u1820-\u1877\u1880-\u18AA\u18B0-\u18F5\u1900-\u191E\u1920-\u192B\u1930-\u193B\u1946-\u196D\u1970-\u1974\u1980-\u19AB\u19B0-\u19C9\u19D0-\u19D9\u1A00-\u1A1B\u1A20-\u1A5E\u1A60-\u1A7C\u1A7F-\u1A89\u1A90-\u1A99\u1AA7\u1AB0-\u1ABD\u1B00-\u1B4B\u1B50-\u1B59\u1B6B-\u1B73\u1B80-\u1BF3\u1C00-\u1C37\u1C40-\u1C49\u1C4D-\u1C7D\u1C80-\u1C88\u1CD0-\u1CD2\u1CD4-\u1CF6\u1CF8\u1CF9\u1D00-\u1DF5\u1DFB-\u1F15\u1F18-\u1F1D\u1F20-\u1F45\u1F48-\u1F4D\u1F50-\u1F57\u1F59\u1F5B\u1F5D\u1F5F-\u1F7D\u1F80-\u1FB4\u1FB6-\u1FBC\u1FBE\u1FC2-\u1FC4\u1FC6-\u1FCC\u1FD0-\u1FD3\u1FD6-\u1FDB\u1FE0-\u1FEC\u1FF2-\u1FF4\u1FF6-\u1FFC\u203F\u2040\u2054\u2071\u207F\u2090-\u209C\u20D0-\u20DC\u20E1\u20E5-\u20F0\u2102\u2107\u210A-\u2113\u2115\u2119-\u211D\u2124\u2126\u2128\u212A-\u212D\u212F-\u2139\u213C-\u213F\u2145-\u2149\u214E\u2160-\u2188\u2C00-\u2C2E\u2C30-\u2C5E\u2C60-\u2CE4\u2CEB-\u2CF3\u2D00-\u2D25\u2D27\u2D2D\u2D30-\u2D67\u2D6F\u2D7F-\u2D96\u2DA0-\u2DA6\u2DA8-\u2DAE\u2DB0-\u2DB6\u2DB8-\u2DBE\u2DC0-\u2DC6\u2DC8-\u2DCE\u2DD0-\u2DD6\u2DD8-\u2DDE\u2DE0-\u2DFF\u2E2F\u3005-\u3007\u3021-\u302F\u3031-\u3035\u3038-\u303C\u3041-\u3096\u3099\u309A\u309D-\u309F\u30A1-\u30FA\u30FC-\u30FF\u3105-\u312D\u3131-\u318E\u31A0-\u31BA\u31F0-\u31FF\u3400-\u4DB5\u4E00-\u9FD5\uA000-\uA48C\uA4D0-\uA4FD\uA500-\uA60C\uA610-\uA62B\uA640-\uA66F\uA674-\uA67D\uA67F-\uA6F1\uA717-\uA71F\uA722-\uA788\uA78B-\uA7AE\uA7B0-\uA7B7\uA7F7-\uA827\uA840-\uA873\uA880-\uA8C5\uA8D0-\uA8D9\uA8E0-\uA8F7\uA8FB\uA8FD\uA900-\uA92D\uA930-\uA953\uA960-\uA97C\uA980-\uA9C0\uA9CF-\uA9D9\uA9E0-\uA9FE\uAA00-\uAA36\uAA40-\uAA4D\uAA50-\uAA59\uAA60-\uAA76\uAA7A-\uAAC2\uAADB-\uAADD\uAAE0-\uAAEF\uAAF2-\uAAF6\uAB01-\uAB06\uAB09-\uAB0E\uAB11-\uAB16\uAB20-\uAB26\uAB28-\uAB2E\uAB30-\uAB5A\uAB5C-\uAB65\uAB70-\uABEA\uABEC\uABED\uABF0-\uABF9\uAC00-\uD7A3\uD7B0-\uD7C6\uD7CB-\uD7FB\uF900-\uFA6D\uFA70-\uFAD9\uFB00-\uFB06\uFB13-\uFB17\uFB1D-\uFB28\uFB2A-\uFB36\uFB38-\uFB3C\uFB3E\uFB40\uFB41\uFB43\uFB44\uFB46-\uFBB1\uFBD3-\uFD3D\uFD50-\uFD8F\uFD92-\uFDC7\uFDF0-\uFDFB\uFE00-\uFE0F\uFE20-\uFE2F\uFE33\uFE34\uFE4D-\uFE4F\uFE70-\uFE74\uFE76-\uFEFC\uFF10-\uFF19\uFF21-\uFF3A\uFF3F\uFF41-\uFF5A\uFF66-\uFFBE\uFFC2-\uFFC7\uFFCA-\uFFCF\uFFD2-\uFFD7\uFFDA-\uFFDC]|\uD800[\uDC00-\uDC0B\uDC0D-\uDC26\uDC28-\uDC3A\uDC3C\uDC3D\uDC3F-\uDC4D\uDC50-\uDC5D\uDC80-\uDCFA\uDD40-\uDD74\uDDFD\uDE80-\uDE9C\uDEA0-\uDED0\uDEE0\uDF00-\uDF1F\uDF30-\uDF4A\uDF50-\uDF7A\uDF80-\uDF9D\uDFA0-\uDFC3\uDFC8-\uDFCF\uDFD1-\uDFD5]|\uD801[\uDC00-\uDC9D\uDCA0-\uDCA9\uDCB0-\uDCD3\uDCD8-\uDCFB\uDD00-\uDD27\uDD30-\uDD63\uDE00-\uDF36\uDF40-\uDF55\uDF60-\uDF67]|\uD802[\uDC00-\uDC05\uDC08\uDC0A-\uDC35\uDC37\uDC38\uDC3C\uDC3F-\uDC55\uDC60-\uDC76\uDC80-\uDC9E\uDCE0-\uDCF2\uDCF4\uDCF5\uDD00-\uDD15\uDD20-\uDD39\uDD80-\uDDB7\uDDBE\uDDBF\uDE00-\uDE03\uDE05\uDE06\uDE0C-\uDE13\uDE15-\uDE17\uDE19-\uDE33\uDE38-\uDE3A\uDE3F\uDE60-\uDE7C\uDE80-\uDE9C\uDEC0-\uDEC7\uDEC9-\uDEE6\uDF00-\uDF35\uDF40-\uDF55\uDF60-\uDF72\uDF80-\uDF91]|\uD803[\uDC00-\uDC48\uDC80-\uDCB2\uDCC0-\uDCF2]|\uD804[\uDC00-\uDC46\uDC66-\uDC6F\uDC7F-\uDCBA\uDCD0-\uDCE8\uDCF0-\uDCF9\uDD00-\uDD34\uDD36-\uDD3F\uDD50-\uDD73\uDD76\uDD80-\uDDC4\uDDCA-\uDDCC\uDDD0-\uDDDA\uDDDC\uDE00-\uDE11\uDE13-\uDE37\uDE3E\uDE80-\uDE86\uDE88\uDE8A-\uDE8D\uDE8F-\uDE9D\uDE9F-\uDEA8\uDEB0-\uDEEA\uDEF0-\uDEF9\uDF00-\uDF03\uDF05-\uDF0C\uDF0F\uDF10\uDF13-\uDF28\uDF2A-\uDF30\uDF32\uDF33\uDF35-\uDF39\uDF3C-\uDF44\uDF47\uDF48\uDF4B-\uDF4D\uDF50\uDF57\uDF5D-\uDF63\uDF66-\uDF6C\uDF70-\uDF74]|\uD805[\uDC00-\uDC4A\uDC50-\uDC59\uDC80-\uDCC5\uDCC7\uDCD0-\uDCD9\uDD80-\uDDB5\uDDB8-\uDDC0\uDDD8-\uDDDD\uDE00-\uDE40\uDE44\uDE50-\uDE59\uDE80-\uDEB7\uDEC0-\uDEC9\uDF00-\uDF19\uDF1D-\uDF2B\uDF30-\uDF39]|\uD806[\uDCA0-\uDCE9\uDCFF\uDEC0-\uDEF8]|\uD807[\uDC00-\uDC08\uDC0A-\uDC36\uDC38-\uDC40\uDC50-\uDC59\uDC72-\uDC8F\uDC92-\uDCA7\uDCA9-\uDCB6]|\uD808[\uDC00-\uDF99]|\uD809[\uDC00-\uDC6E\uDC80-\uDD43]|[\uD80C\uD81C-\uD820\uD840-\uD868\uD86A-\uD86C\uD86F-\uD872][\uDC00-\uDFFF]|\uD80D[\uDC00-\uDC2E]|\uD811[\uDC00-\uDE46]|\uD81A[\uDC00-\uDE38\uDE40-\uDE5E\uDE60-\uDE69\uDED0-\uDEED\uDEF0-\uDEF4\uDF00-\uDF36\uDF40-\uDF43\uDF50-\uDF59\uDF63-\uDF77\uDF7D-\uDF8F]|\uD81B[\uDF00-\uDF44\uDF50-\uDF7E\uDF8F-\uDF9F\uDFE0]|\uD821[\uDC00-\uDFEC]|\uD822[\uDC00-\uDEF2]|\uD82C[\uDC00\uDC01]|\uD82F[\uDC00-\uDC6A\uDC70-\uDC7C\uDC80-\uDC88\uDC90-\uDC99\uDC9D\uDC9E]|\uD834[\uDD65-\uDD69\uDD6D-\uDD72\uDD7B-\uDD82\uDD85-\uDD8B\uDDAA-\uDDAD\uDE42-\uDE44]|\uD835[\uDC00-\uDC54\uDC56-\uDC9C\uDC9E\uDC9F\uDCA2\uDCA5\uDCA6\uDCA9-\uDCAC\uDCAE-\uDCB9\uDCBB\uDCBD-\uDCC3\uDCC5-\uDD05\uDD07-\uDD0A\uDD0D-\uDD14\uDD16-\uDD1C\uDD1E-\uDD39\uDD3B-\uDD3E\uDD40-\uDD44\uDD46\uDD4A-\uDD50\uDD52-\uDEA5\uDEA8-\uDEC0\uDEC2-\uDEDA\uDEDC-\uDEFA\uDEFC-\uDF14\uDF16-\uDF34\uDF36-\uDF4E\uDF50-\uDF6E\uDF70-\uDF88\uDF8A-\uDFA8\uDFAA-\uDFC2\uDFC4-\uDFCB\uDFCE-\uDFFF]|\uD836[\uDE00-\uDE36\uDE3B-\uDE6C\uDE75\uDE84\uDE9B-\uDE9F\uDEA1-\uDEAF]|\uD838[\uDC00-\uDC06\uDC08-\uDC18\uDC1B-\uDC21\uDC23\uDC24\uDC26-\uDC2A]|\uD83A[\uDC00-\uDCC4\uDCD0-\uDCD6\uDD00-\uDD4A\uDD50-\uDD59]|\uD83B[\uDE00-\uDE03\uDE05-\uDE1F\uDE21\uDE22\uDE24\uDE27\uDE29-\uDE32\uDE34-\uDE37\uDE39\uDE3B\uDE42\uDE47\uDE49\uDE4B\uDE4D-\uDE4F\uDE51\uDE52\uDE54\uDE57\uDE59\uDE5B\uDE5D\uDE5F\uDE61\uDE62\uDE64\uDE67-\uDE6A\uDE6C-\uDE72\uDE74-\uDE77\uDE79-\uDE7C\uDE7E\uDE80-\uDE89\uDE8B-\uDE9B\uDEA1-\uDEA3\uDEA5-\uDEA9\uDEAB-\uDEBB]|\uD869[\uDC00-\uDED6\uDF00-\uDFFF]|\uD86D[\uDC00-\uDF34\uDF40-\uDFFF]|\uD86E[\uDC00-\uDC1D\uDC20-\uDFFF]|\uD873[\uDC00-\uDEA1]|\uD87E[\uDC00-\uDE1D]|\uDB40[\uDD00-\uDDEF]/;
});

unwrapExports(unicode);
var unicode_1 = unicode.Space_Separator;
var unicode_2 = unicode.ID_Start;
var unicode_3 = unicode.ID_Continue;

var util$1 = createCommonjsModule(function (module, exports) {
Object.defineProperty(exports,'__esModule',{value:true});exports.isSpaceSeparator=isSpaceSeparator;exports.isIdStartChar=isIdStartChar;exports.isIdContinueChar=isIdContinueChar;exports.isDigit=isDigit;exports.isHexDigit=isHexDigit;var unicode$$1=_interopRequireWildcard(unicode);function _interopRequireWildcard(obj){if(obj&&obj.__esModule){return obj}else{var newObj={};if(obj!=null){for(var key in obj){if(Object.prototype.hasOwnProperty.call(obj,key))newObj[key]=obj[key];}}newObj.default=obj;return newObj}}function isSpaceSeparator(c){return unicode$$1.Space_Separator.test(c)}function isIdStartChar(c){return c>='a'&&c<='z'||c>='A'&&c<='Z'||c==='$'||c==='_'||unicode$$1.ID_Start.test(c)}function isIdContinueChar(c){return c>='a'&&c<='z'||c>='A'&&c<='Z'||c>='0'&&c<='9'||c==='$'||c==='_'||c==='\u200C'||c==='\u200D'||unicode$$1.ID_Continue.test(c)}function isDigit(c){return /[0-9]/.test(c)}function isHexDigit(c){return /[0-9A-Fa-f]/.test(c)}
});

unwrapExports(util$1);
var util_1 = util$1.isSpaceSeparator;
var util_2 = util$1.isIdStartChar;
var util_3 = util$1.isIdContinueChar;
var util_4 = util$1.isDigit;
var util_5 = util$1.isHexDigit;

var parse_1$2 = createCommonjsModule(function (module, exports) {
Object.defineProperty(exports,'__esModule',{value:true});var _typeof=typeof Symbol==='function'&&typeof Symbol.iterator==='symbol'?function(obj){return typeof obj}:function(obj){return obj&&typeof Symbol==='function'&&obj.constructor===Symbol&&obj!==Symbol.prototype?'symbol':typeof obj};exports.default=parse;var util$$1=_interopRequireWildcard(util$1);function _interopRequireWildcard(obj){if(obj&&obj.__esModule){return obj}else{var newObj={};if(obj!=null){for(var key in obj){if(Object.prototype.hasOwnProperty.call(obj,key))newObj[key]=obj[key];}}newObj.default=obj;return newObj}}var source=void 0;var parseState=void 0;var stack=void 0;var pos=void 0;var line=void 0;var column=void 0;var token=void 0;var key=void 0;var root=void 0;function parse(text,reviver){source=String(text);parseState='start';stack=[];pos=0;line=1;column=0;token=undefined;key=undefined;root=undefined;do{token=lex();parseStates[parseState]();}while(token.type!=='eof');if(typeof reviver==='function'){return internalize({'':root},'',reviver)}return root}function internalize(holder,name,reviver){var value=holder[name];if(value!=null&&(typeof value==='undefined'?'undefined':_typeof(value))==='object'){for(var _key in value){var replacement=internalize(value,_key,reviver);if(replacement===undefined){delete value[_key];}else{value[_key]=replacement;}}}return reviver.call(holder,name,value)}var lexState=void 0;var buffer=void 0;var doubleQuote=void 0;var _sign=void 0;var c=void 0;function lex(){lexState='default';buffer='';doubleQuote=false;_sign=1;for(;;){c=peek();var _token=lexStates[lexState]();if(_token){return _token}}}function peek(){if(source[pos]){return String.fromCodePoint(source.codePointAt(pos))}}function read(){var c=peek();if(c==='\n'){line++;column=0;}else if(c){column+=c.length;}else{column++;}if(c){pos+=c.length;}return c}var lexStates={default:function _default(){switch(c){case'\t':case'\x0B':case'\f':case' ':case'\xA0':case'\uFEFF':case'\n':case'\r':case'\u2028':case'\u2029':read();return;case'/':read();lexState='comment';return;case undefined:read();return newToken('eof');}if(util$$1.isSpaceSeparator(c)){read();return}return lexStates[parseState]()},comment:function comment(){switch(c){case'*':read();lexState='multiLineComment';return;case'/':read();lexState='singleLineComment';return;}throw invalidChar(read())},multiLineComment:function multiLineComment(){switch(c){case'*':read();lexState='multiLineCommentAsterisk';return;case undefined:throw invalidChar(read());}read();},multiLineCommentAsterisk:function multiLineCommentAsterisk(){switch(c){case'*':read();return;case'/':read();lexState='default';return;case undefined:throw invalidChar(read());}read();lexState='multiLineComment';},singleLineComment:function singleLineComment(){switch(c){case'\n':case'\r':case'\u2028':case'\u2029':read();lexState='default';return;case undefined:read();return newToken('eof');}read();},value:function value(){switch(c){case'{':case'[':return newToken('punctuator',read());case'n':read();literal('ull');return newToken('null',null);case't':read();literal('rue');return newToken('boolean',true);case'f':read();literal('alse');return newToken('boolean',false);case'-':case'+':if(read()==='-'){_sign=-1;}lexState='sign';return;case'.':buffer=read();lexState='decimalPointLeading';return;case'0':buffer=read();lexState='zero';return;case'1':case'2':case'3':case'4':case'5':case'6':case'7':case'8':case'9':buffer=read();lexState='decimalInteger';return;case'I':read();literal('nfinity');return newToken('numeric',Infinity);case'N':read();literal('aN');return newToken('numeric',NaN);case'"':case'\'':doubleQuote=read()==='"';buffer='';lexState='string';return;}throw invalidChar(read())},identifierNameStartEscape:function identifierNameStartEscape(){if(c!=='u'){throw invalidChar(read())}read();var u=unicodeEscape();switch(u){case'$':case'_':break;default:if(!util$$1.isIdStartChar(u)){throw invalidIdentifier()}break;}buffer+=u;lexState='identifierName';},identifierName:function identifierName(){switch(c){case'$':case'_':case'\u200C':case'\u200D':buffer+=read();return;case'\\':read();lexState='identifierNameEscape';return;}if(util$$1.isIdContinueChar(c)){buffer+=read();return}return newToken('identifier',buffer)},identifierNameEscape:function identifierNameEscape(){if(c!=='u'){throw invalidChar(read())}read();var u=unicodeEscape();switch(u){case'$':case'_':case'\u200C':case'\u200D':break;default:if(!util$$1.isIdContinueChar(u)){throw invalidIdentifier()}break;}buffer+=u;lexState='identifierName';},sign:function sign(){switch(c){case'.':buffer=read();lexState='decimalPointLeading';return;case'0':buffer=read();lexState='zero';return;case'1':case'2':case'3':case'4':case'5':case'6':case'7':case'8':case'9':buffer=read();lexState='decimalInteger';return;case'I':read();literal('nfinity');return newToken('numeric',_sign*Infinity);case'N':read();literal('aN');return newToken('numeric',NaN);}throw invalidChar(read())},zero:function zero(){switch(c){case'.':buffer+=read();lexState='decimalPoint';return;case'e':case'E':buffer+=read();lexState='decimalExponent';return;case'x':case'X':buffer+=read();lexState='hexadecimal';return;}return newToken('numeric',_sign*0)},decimalInteger:function decimalInteger(){switch(c){case'.':buffer+=read();lexState='decimalPoint';return;case'e':case'E':buffer+=read();lexState='decimalExponent';return;}if(util$$1.isDigit(c)){buffer+=read();return}return newToken('numeric',_sign*Number(buffer))},decimalPointLeading:function decimalPointLeading(){if(util$$1.isDigit(c)){buffer+=read();lexState='decimalFraction';return}throw invalidChar(read())},decimalPoint:function decimalPoint(){switch(c){case'e':case'E':buffer+=read();lexState='decimalExponent';return;}if(util$$1.isDigit(c)){buffer+=read();lexState='decimalFraction';return}return newToken('numeric',_sign*Number(buffer))},decimalFraction:function decimalFraction(){switch(c){case'e':case'E':buffer+=read();lexState='decimalExponent';return;}if(util$$1.isDigit(c)){buffer+=read();return}return newToken('numeric',_sign*Number(buffer))},decimalExponent:function decimalExponent(){switch(c){case'+':case'-':buffer+=read();lexState='decimalExponentSign';return;}if(util$$1.isDigit(c)){buffer+=read();lexState='decimalExponentInteger';return}throw invalidChar(read())},decimalExponentSign:function decimalExponentSign(){if(util$$1.isDigit(c)){buffer+=read();lexState='decimalExponentInteger';return}throw invalidChar(read())},decimalExponentInteger:function decimalExponentInteger(){if(util$$1.isDigit(c)){buffer+=read();return}return newToken('numeric',_sign*Number(buffer))},hexadecimal:function hexadecimal(){if(util$$1.isHexDigit(c)){buffer+=read();lexState='hexadecimalInteger';return}throw invalidChar(read())},hexadecimalInteger:function hexadecimalInteger(){if(util$$1.isHexDigit(c)){buffer+=read();return}return newToken('numeric',_sign*Number(buffer))},string:function string(){switch(c){case'\\':read();buffer+=escape();return;case'"':if(doubleQuote){read();return newToken('string',buffer)}buffer+=read();return;case'\'':if(!doubleQuote){read();return newToken('string',buffer)}buffer+=read();return;case'\n':case'\r':throw invalidChar(read());case'\u2028':case'\u2029':separatorChar(c);break;case undefined:throw invalidChar(read());}buffer+=read();},start:function start(){switch(c){case'{':case'[':return newToken('punctuator',read());}lexState='value';},beforePropertyName:function beforePropertyName(){switch(c){case'$':case'_':buffer=read();lexState='identifierName';return;case'\\':read();lexState='identifierNameStartEscape';return;case'}':return newToken('punctuator',read());case'"':case'\'':doubleQuote=read()==='"';lexState='string';return;}if(util$$1.isIdStartChar(c)){buffer+=read();lexState='identifierName';return}throw invalidChar(read())},afterPropertyName:function afterPropertyName(){if(c===':'){return newToken('punctuator',read())}throw invalidChar(read())},beforePropertyValue:function beforePropertyValue(){lexState='value';},afterPropertyValue:function afterPropertyValue(){switch(c){case',':case'}':return newToken('punctuator',read());}throw invalidChar(read())},beforeArrayValue:function beforeArrayValue(){if(c===']'){return newToken('punctuator',read())}lexState='value';},afterArrayValue:function afterArrayValue(){switch(c){case',':case']':return newToken('punctuator',read());}throw invalidChar(read())},end:function end(){throw invalidChar(read())}};function newToken(type,value){return{type:type,value:value,line:line,column:column}}function literal(s){var _iteratorNormalCompletion=true;var _didIteratorError=false;var _iteratorError=undefined;try{for(var _iterator=s[Symbol.iterator](),_step;!(_iteratorNormalCompletion=(_step=_iterator.next()).done);_iteratorNormalCompletion=true){var _c=_step.value;var p=peek();if(p!==_c){throw invalidChar(read())}read();}}catch(err){_didIteratorError=true;_iteratorError=err;}finally{try{if(!_iteratorNormalCompletion&&_iterator.return){_iterator.return();}}finally{if(_didIteratorError){throw _iteratorError}}}}function escape(){var c=peek();switch(c){case'b':read();return'\b';case'f':read();return'\f';case'n':read();return'\n';case'r':read();return'\r';case't':read();return'\t';case'v':read();return'\x0B';case'0':read();if(util$$1.isDigit(peek())){throw invalidChar(read())}return'\0';case'x':read();return hexEscape();case'u':read();return unicodeEscape();case'\n':case'\u2028':case'\u2029':read();return'';case'\r':read();if(peek()==='\n'){read();}return'';case'1':case'2':case'3':case'4':case'5':case'6':case'7':case'8':case'9':throw invalidChar(read());case undefined:throw invalidChar(read());}return read()}function hexEscape(){var buffer='';var c=peek();if(!util$$1.isHexDigit(c)){throw invalidChar(read())}buffer+=read();c=peek();if(!util$$1.isHexDigit(c)){throw invalidChar(read())}buffer+=read();return String.fromCodePoint(parseInt(buffer,16))}function unicodeEscape(){var buffer='';var count=4;while(count-->0){var _c2=peek();if(!util$$1.isHexDigit(_c2)){throw invalidChar(read())}buffer+=read();}return String.fromCodePoint(parseInt(buffer,16))}var parseStates={start:function start(){if(token.type==='eof'){throw invalidEOF()}push();},beforePropertyName:function beforePropertyName(){switch(token.type){case'identifier':case'string':key=token.value;parseState='afterPropertyName';return;case'punctuator':pop();return;case'eof':throw invalidEOF();}},afterPropertyName:function afterPropertyName(){if(token.type==='eof'){throw invalidEOF()}parseState='beforePropertyValue';},beforePropertyValue:function beforePropertyValue(){if(token.type==='eof'){throw invalidEOF()}push();},beforeArrayValue:function beforeArrayValue(){if(token.type==='eof'){throw invalidEOF()}if(token.type==='punctuator'&&token.value===']'){pop();return}push();},afterPropertyValue:function afterPropertyValue(){if(token.type==='eof'){throw invalidEOF()}switch(token.value){case',':parseState='beforePropertyName';return;case'}':pop();}},afterArrayValue:function afterArrayValue(){if(token.type==='eof'){throw invalidEOF()}switch(token.value){case',':parseState='beforeArrayValue';return;case']':pop();}},end:function end(){}};function push(){var value=void 0;switch(token.type){case'punctuator':switch(token.value){case'{':value={};break;case'[':value=[];break;}break;case'null':case'boolean':case'numeric':case'string':value=token.value;break;}if(root===undefined){root=value;}else{var parent=stack[stack.length-1];if(Array.isArray(parent)){parent.push(value);}else{parent[key]=value;}}if(value!==null&&(typeof value==='undefined'?'undefined':_typeof(value))==='object'){stack.push(value);if(Array.isArray(value)){parseState='beforeArrayValue';}else{parseState='beforePropertyName';}}else{var current=stack[stack.length-1];if(current==null){parseState='end';}else if(Array.isArray(current)){parseState='afterArrayValue';}else{parseState='afterPropertyValue';}}}function pop(){stack.pop();var current=stack[stack.length-1];if(current==null){parseState='end';}else if(Array.isArray(current)){parseState='afterArrayValue';}else{parseState='afterPropertyValue';}}function invalidChar(c){if(c===undefined){return syntaxError('JSON5: invalid end of input at '+line+':'+column)}return syntaxError('JSON5: invalid character \''+formatChar(c)+'\' at '+line+':'+column)}function invalidEOF(){return syntaxError('JSON5: invalid end of input at '+line+':'+column)}function invalidIdentifier(){column-=5;return syntaxError('JSON5: invalid identifier character at '+line+':'+column)}function separatorChar(c){console.warn('JSON5: \''+c+'\' is not valid ECMAScript; consider escaping');}function formatChar(c){var replacements={'\'':'\\\'','"':'\\"','\\':'\\\\','\b':'\\b','\f':'\\f','\n':'\\n','\r':'\\r','\t':'\\t','\x0B':'\\v','\0':'\\0','\u2028':'\\u2028','\u2029':'\\u2029'};if(replacements[c]){return replacements[c]}if(c<' '){var hexString=c.charCodeAt(0).toString(16);return'\\x'+('00'+hexString).substring(hexString.length)}return c}function syntaxError(message){var err=new SyntaxError(message);err.lineNumber=line;err.columnNumber=column;return err}module.exports=exports['default'];
});

unwrapExports(parse_1$2);

var stringify_1$2 = createCommonjsModule(function (module, exports) {
Object.defineProperty(exports,'__esModule',{value:true});var _typeof=typeof Symbol==='function'&&typeof Symbol.iterator==='symbol'?function(obj){return typeof obj}:function(obj){return obj&&typeof Symbol==='function'&&obj.constructor===Symbol&&obj!==Symbol.prototype?'symbol':typeof obj};exports.default=stringify;var util$$1=_interopRequireWildcard(util$1);function _interopRequireWildcard(obj){if(obj&&obj.__esModule){return obj}else{var newObj={};if(obj!=null){for(var key in obj){if(Object.prototype.hasOwnProperty.call(obj,key))newObj[key]=obj[key];}}newObj.default=obj;return newObj}}function stringify(value,replacer,space){var stack=[];var indent='';var propertyList=void 0;var replacerFunc=void 0;var gap='';var quote=void 0;if(replacer!=null&&(typeof replacer==='undefined'?'undefined':_typeof(replacer))==='object'&&!Array.isArray(replacer)){space=replacer.space;quote=replacer.quote;replacer=replacer.replacer;}if(typeof replacer==='function'){replacerFunc=replacer;}else if(Array.isArray(replacer)){propertyList=[];var _iteratorNormalCompletion=true;var _didIteratorError=false;var _iteratorError=undefined;try{for(var _iterator=replacer[Symbol.iterator](),_step;!(_iteratorNormalCompletion=(_step=_iterator.next()).done);_iteratorNormalCompletion=true){var v=_step.value;var item=void 0;if(typeof v==='string'){item=v;}else if(typeof v==='number'||v instanceof String||v instanceof Number){item=String(v);}if(item!==undefined&&propertyList.indexOf(item)<0){propertyList.push(item);}}}catch(err){_didIteratorError=true;_iteratorError=err;}finally{try{if(!_iteratorNormalCompletion&&_iterator.return){_iterator.return();}}finally{if(_didIteratorError){throw _iteratorError}}}}if(space instanceof Number){space=Number(space);}else if(space instanceof String){space=String(space);}if(typeof space==='number'){if(space>0){space=Math.min(10,Math.floor(space));gap='          '.substr(0,space);}}else if(typeof space==='string'){gap=space.substr(0,10);}return serializeProperty('',{'':value});function serializeProperty(key,holder){var value=holder[key];if(value!=null){if(typeof value.toJSON5==='function'){value=value.toJSON5(key);}else if(typeof value.toJSON==='function'){value=value.toJSON(key);}}if(replacerFunc){value=replacerFunc.call(holder,key,value);}if(value instanceof Number){value=Number(value);}else if(value instanceof String){value=String(value);}else if(value instanceof Boolean){value=value.valueOf();}switch(value){case null:return'null';case true:return'true';case false:return'false';}if(typeof value==='string'){return quoteString(value,false)}if(typeof value==='number'){return String(value)}if((typeof value==='undefined'?'undefined':_typeof(value))==='object'){return Array.isArray(value)?serializeArray(value):serializeObject(value)}return undefined}function quoteString(value){var quotes={'\'':0.1,'"':0.2};var replacements={'\'':'\\\'','"':'\\"','\\':'\\\\','\b':'\\b','\f':'\\f','\n':'\\n','\r':'\\r','\t':'\\t','\x0B':'\\v','\0':'\\0','\u2028':'\\u2028','\u2029':'\\u2029'};var product='';var _iteratorNormalCompletion2=true;var _didIteratorError2=false;var _iteratorError2=undefined;try{for(var _iterator2=value[Symbol.iterator](),_step2;!(_iteratorNormalCompletion2=(_step2=_iterator2.next()).done);_iteratorNormalCompletion2=true){var c=_step2.value;switch(c){case'\'':case'"':quotes[c]++;product+=c;continue;}if(replacements[c]){product+=replacements[c];continue}if(c<' '){var hexString=c.charCodeAt(0).toString(16);product+='\\x'+('00'+hexString).substring(hexString.length);continue}product+=c;}}catch(err){_didIteratorError2=true;_iteratorError2=err;}finally{try{if(!_iteratorNormalCompletion2&&_iterator2.return){_iterator2.return();}}finally{if(_didIteratorError2){throw _iteratorError2}}}var quoteChar=quote||Object.keys(quotes).reduce(function(a,b){return quotes[a]<quotes[b]?a:b});product=product.replace(new RegExp(quoteChar,'g'),replacements[quoteChar]);return quoteChar+product+quoteChar}function serializeObject(value){if(stack.indexOf(value)>=0){throw TypeError('Converting circular structure to JSON5')}stack.push(value);var stepback=indent;indent=indent+gap;var keys=propertyList||Object.keys(value);var partial=[];var _iteratorNormalCompletion3=true;var _didIteratorError3=false;var _iteratorError3=undefined;try{for(var _iterator3=keys[Symbol.iterator](),_step3;!(_iteratorNormalCompletion3=(_step3=_iterator3.next()).done);_iteratorNormalCompletion3=true){var key=_step3.value;var propertyString=serializeProperty(key,value);if(propertyString!==undefined){var member=serializeKey(key)+':';if(gap!==''){member+=' ';}member+=propertyString;partial.push(member);}}}catch(err){_didIteratorError3=true;_iteratorError3=err;}finally{try{if(!_iteratorNormalCompletion3&&_iterator3.return){_iterator3.return();}}finally{if(_didIteratorError3){throw _iteratorError3}}}var final=void 0;if(partial.length===0){final='{}';}else{var properties=void 0;if(gap===''){properties=partial.join(',');final='{'+properties+'}';}else{var separator=',\n'+indent;properties=partial.join(separator);final='{\n'+indent+properties+',\n'+stepback+'}';}}stack.pop();indent=stepback;return final}function serializeKey(key){if(key.length===0){return quoteString(key,true)}var firstChar=String.fromCodePoint(key.codePointAt(0));if(!util$$1.isIdStartChar(firstChar)){return quoteString(key,true)}for(var i=firstChar.length;i<key.length;i++){if(!util$$1.isIdContinueChar(String.fromCodePoint(key.codePointAt(i)))){return quoteString(key,true)}}return key}function serializeArray(value){if(stack.indexOf(value)>=0){throw TypeError('Converting circular structure to JSON5')}stack.push(value);var stepback=indent;indent=indent+gap;var partial=[];for(var i=0;i<value.length;i++){var propertyString=serializeProperty(String(i),value);partial.push(propertyString!==undefined?propertyString:'null');}var final=void 0;if(partial.length===0){final='[]';}else{if(gap===''){var properties=partial.join(',');final='['+properties+']';}else{var separator=',\n'+indent;var _properties=partial.join(separator);final='[\n'+indent+_properties+',\n'+stepback+']';}}stack.pop();indent=stepback;return final}}module.exports=exports['default'];
});

unwrapExports(stringify_1$2);

var lib$4 = createCommonjsModule(function (module, exports) {
Object.defineProperty(exports,'__esModule',{value:true});var _parse2=_interopRequireDefault(parse_1$2);var _stringify2=_interopRequireDefault(stringify_1$2);function _interopRequireDefault(obj){return obj&&obj.__esModule?obj:{default:obj}}exports.default={parse:_parse2.default,stringify:_stringify2.default};module.exports=exports['default'];
});

unwrapExports(lib$4);

var schema$2 = [
  {
    "long": "help",
    "description": "output usage information",
    "short": "h",
    "type": "boolean",
    "default": false
  },
  {
    "long": "version",
    "description": "output version number",
    "short": "v",
    "type": "boolean",
    "default": false
  },
  {
    "long": "output",
    "description": "specify output location",
    "short": "o",
    "value": "[path]"
  },
  {
    "long": "rc-path",
    "description": "specify configuration file",
    "short": "r",
    "type": "string",
    "value": "<path>"
  },
  {
    "long": "ignore-path",
    "description": "specify ignore file",
    "short": "i",
    "type": "string",
    "value": "<path>"
  },
  {
    "long": "setting",
    "description": "specify settings",
    "short": "s",
    "type": "string",
    "value": "<settings>"
  },
  {
    "long": "ext",
    "description": "specify extensions",
    "short": "e",
    "type": "string",
    "value": "<extensions>"
  },
  {
    "long": "use",
    "description": "use plugins",
    "short": "u",
    "type": "string",
    "value": "<plugins>"
  },
  {
    "long": "watch",
    "description": "watch for changes and reprocess",
    "short": "w",
    "type": "boolean",
    "default": false
  },
  {
    "long": "quiet",
    "description": "output only warnings and errors",
    "short": "q",
    "type": "boolean",
    "default": false
  },
  {
    "long": "silent",
    "description": "output only errors",
    "short": "S",
    "type": "boolean",
    "default": false
  },
  {
    "long": "frail",
    "description": "exit with 1 on warnings",
    "short": "f",
    "type": "boolean",
    "default": false
  },
  {
    "long": "tree",
    "description": "specify input and output as syntax tree",
    "short": "t",
    "type": "boolean",
    "default": false
  },
  {
    "long": "report",
    "description": "specify reporter",
    "type": "string",
    "value": "<reporter>"
  },
  {
    "long": "file-path",
    "description": "specify path to process as",
    "type": "string",
    "value": "<path>"
  },
  {
    "long": "tree-in",
    "description": "specify input as syntax tree",
    "type": "boolean"
  },
  {
    "long": "tree-out",
    "description": "output syntax tree",
    "type": "boolean"
  },
  {
    "long": "inspect",
    "description": "output formatted syntax tree",
    "type": "boolean"
  },
  {
    "long": "stdout",
    "description": "specify writing to stdout",
    "type": "boolean",
    "truelike": true
  },
  {
    "long": "color",
    "description": "specify color in report",
    "type": "boolean",
    "default": true
  },
  {
    "long": "config",
    "description": "search for configuration files",
    "type": "boolean",
    "default": true
  },
  {
    "long": "ignore",
    "description": "search for ignore files",
    "type": "boolean",
    "default": true
  }
]
;

var schema$3 = Object.freeze({
	default: schema$2
});

var schema$4 = ( schema$3 && schema$2 ) || schema$3;

var options_1 = options;

/* Schema for `minimist`. */
var minischema = {
  unknown: handleUnknownArgument,
  default: {},
  alias: {},
  string: [],
  boolean: []
};

schema$4.forEach(addEach);

/* Parse CLI options. */
function options(flags, configuration) {
  var extension = configuration.extensions[0];
  var name = configuration.name;
  var config = toCamelCase(minimist(flags, minischema));
  var help;
  var ext;
  var report;

  schema$4.forEach(function(option) {
    if (option.type === 'string' && config[option.long] === '') {
      throw fault_1('Missing value:%s', inspect$2(option).join(' '))
    }
  });

  ext = extensions(config.ext);
  report = reporter$1(config.report);

  help = [
    inspectAll(schema$4),
    '',
    'Examples:',
    '',
    '  # Process `input.' + extension + '`',
    '  $ ' + name + ' input.' + extension + ' -o output.' + extension,
    '',
    '  # Pipe',
    '  $ ' + name + ' < input.' + extension + ' > output.' + extension,
    '',
    '  # Rewrite all applicable files',
    '  $ ' + name + ' . -o'
  ].join('\n');

  return {
    helpMessage: help,
    /* “hidden” feature, makes testing easier. */
    cwd: configuration.cwd,
    processor: configuration.processor,
    help: config.help,
    version: config.version,
    files: config._,
    watch: config.watch,
    extensions: ext.length ? ext : configuration.extensions,
    output: config.output,
    out: config.stdout,
    tree: config.tree,
    treeIn: config.treeIn,
    treeOut: config.treeOut,
    inspect: config.inspect,
    rcName: configuration.rcName,
    packageField: configuration.packageField,
    rcPath: config.rcPath,
    detectConfig: config.config,
    settings: settings(config.setting),
    ignoreName: configuration.ignoreName,
    ignorePath: config.ignorePath,
    detectIgnore: config.ignore,
    pluginPrefix: configuration.pluginPrefix,
    plugins: plugins(config.use),
    reporter: report[0],
    reporterOptions: report[1],
    color: config.color,
    silent: config.silent,
    quiet: config.quiet,
    frail: config.frail
  }
}

function addEach(option) {
  var value = option.default;

  minischema.default[option.long] = value === undefined ? null : value;

  if (option.type in minischema) {
    minischema[option.type].push(option.long);
  }

  if (option.short) {
    minischema.alias[option.short] = option.long;
  }
}

/* Parse `extensions`. */
function extensions(value) {
  return flatten(normalize$1(value).map(splitList))
}

/* Parse `plugins`. */
function plugins(value) {
  var result = {};

  normalize$1(value)
    .map(splitOptions)
    .forEach(function(value) {
      result[value[0]] = value[1] ? parseConfig(value[1], {}) : null;
    });

  return result
}

/* Parse `reporter`: only one is accepted. */
function reporter$1(value) {
  var all = normalize$1(value)
    .map(splitOptions)
    .map(function(value) {
      return [value[0], value[1] ? parseConfig(value[1], {}) : null]
    });

  return all[all.length - 1] || []
}

/* Parse `settings`. */
function settings(value) {
  var cache = {};

  normalize$1(value).forEach(function(value) {
    parseConfig(value, cache);
  });

  return cache
}

/* Parse configuration. */
function parseConfig(flags, cache) {
  var flag;
  var message;

  try {
    flags = toCamelCase(parseJSON(flags));
  } catch (err) {
    /* Fix position */
    message = err.message.replace(/at(?= position)/, 'around');

    throw fault_1('Cannot parse `%s` as JSON: %s', flags, message)
  }

  for (flag in flags) {
    cache[flag] = flags[flag];
  }

  return cache
}

/* Handle an unknown flag. */
function handleUnknownArgument(flag) {
  /* Glob. */
  if (flag.charAt(0) !== '-') {
    return
  }

  /* Long options. Always unknown. */
  if (flag.charAt(1) === '-') {
    throw fault_1('Unknown option `%s`, expected:\n%s', flag, inspectAll(schema$4))
  }

  /* Short options. Can be grouped. */
  flag
    .slice(1)
    .split('')
    .forEach(each);

  function each(key) {
    var length = schema$4.length;
    var index = -1;
    var option;

    while (++index < length) {
      option = schema$4[index];

      if (option.short === key) {
        return
      }
    }

    throw fault_1(
      'Unknown short option `-%s`, expected:\n%s',
      key,
      inspectAll(schema$4.filter(short))
    )
  }

  function short(option) {
    return option.short
  }
}

/* Inspect all `options`. */
function inspectAll(options) {
  return textTable(options.map(inspect$2))
}

/* Inspect one `option`. */
function inspect$2(option) {
  var description = option.description;
  var long = option.long;

  if (option.default === true || option.truelike) {
    description += ' (on by default)';
    long = '[no-]' + long;
  }

  return [
    '',
    option.short ? '-' + option.short : '',
    '--' + long + (option.value ? ' ' + option.value : ''),
    description
  ]
}

/* Normalize `value`. */
function normalize$1(value) {
  if (!value) {
    return []
  }

  if (typeof value === 'string') {
    return [value]
  }

  return flatten(value.map(normalize$1))
}

/* Flatten `values`. */
function flatten(values) {
  return [].concat.apply([], values)
}

function splitOptions(value) {
  return value.split('=')
}

function splitList(value) {
  return value.split(',')
}

/* Transform the keys on an object to camel-case,
 * recursivly. */
function toCamelCase(object) {
  var result = {};
  var value;
  var key;

  for (key in object) {
    value = object[key];

    if (value && typeof value === 'object' && !('length' in value)) {
      value = toCamelCase(value);
    }

    result[camelcase(key)] = value;
  }

  return result
}

/* Parse a (lazy?) JSON config. */
function parseJSON(value) {
  return lib$4.parse('{' + value + '}')
}

var markdownExtensions = [
	"md",
	"markdown",
	"mdown",
	"mkdn",
	"mkd",
	"mdwn",
	"mkdown",
	"ron"
]
;

var markdownExtensions$1 = Object.freeze({
	default: markdownExtensions
});

var require$$0$15 = ( markdownExtensions$1 && markdownExtensions ) || markdownExtensions$1;

var markdownExtensions$2 = require$$0$15;

var hasOwn = Object.prototype.hasOwnProperty;
var toStr = Object.prototype.toString;
var defineProperty = Object.defineProperty;
var gOPD = Object.getOwnPropertyDescriptor;

var isArray$2 = function isArray(arr) {
	if (typeof Array.isArray === 'function') {
		return Array.isArray(arr);
	}

	return toStr.call(arr) === '[object Array]';
};

var isPlainObject = function isPlainObject(obj) {
	if (!obj || toStr.call(obj) !== '[object Object]') {
		return false;
	}

	var hasOwnConstructor = hasOwn.call(obj, 'constructor');
	var hasIsPrototypeOf = obj.constructor && obj.constructor.prototype && hasOwn.call(obj.constructor.prototype, 'isPrototypeOf');
	// Not own constructor property must be Object
	if (obj.constructor && !hasOwnConstructor && !hasIsPrototypeOf) {
		return false;
	}

	// Own properties are enumerated firstly, so to speed up,
	// if last one is own, then all properties are own.
	var key;
	for (key in obj) { /**/ }

	return typeof key === 'undefined' || hasOwn.call(obj, key);
};

// If name is '__proto__', and Object.defineProperty is available, define __proto__ as an own property on target
var setProperty = function setProperty(target, options) {
	if (defineProperty && options.name === '__proto__') {
		defineProperty(target, options.name, {
			enumerable: true,
			configurable: true,
			value: options.newValue,
			writable: true
		});
	} else {
		target[options.name] = options.newValue;
	}
};

// Return undefined instead of __proto__ if '__proto__' is not an own property
var getProperty = function getProperty(obj, name) {
	if (name === '__proto__') {
		if (!hasOwn.call(obj, name)) {
			return void 0;
		} else if (gOPD) {
			// In early versions of node, obj['__proto__'] is buggy when obj has
			// __proto__ as an own property. Object.getOwnPropertyDescriptor() works.
			return gOPD(obj, name).value;
		}
	}

	return obj[name];
};

var extend$3 = function extend() {
	var options, name, src, copy, copyIsArray, clone;
	var target = arguments[0];
	var i = 1;
	var length = arguments.length;
	var deep = false;

	// Handle a deep copy situation
	if (typeof target === 'boolean') {
		deep = target;
		target = arguments[1] || {};
		// skip the boolean and the target
		i = 2;
	}
	if (target == null || (typeof target !== 'object' && typeof target !== 'function')) {
		target = {};
	}

	for (; i < length; ++i) {
		options = arguments[i];
		// Only deal with non-null/undefined values
		if (options != null) {
			// Extend the base object
			for (name in options) {
				src = getProperty(target, name);
				copy = getProperty(options, name);

				// Prevent never-ending loop
				if (target !== copy) {
					// Recurse if we're merging plain objects or arrays
					if (deep && copy && (isPlainObject(copy) || (copyIsArray = isArray$2(copy)))) {
						if (copyIsArray) {
							copyIsArray = false;
							clone = src && isArray$2(src) ? src : [];
						} else {
							clone = src && isPlainObject(src) ? src : {};
						}

						// Never move original objects, clone them
						setProperty(target, { name: name, newValue: extend(deep, clone, copy) });

					// Don't bring in undefined values
					} else if (typeof copy !== 'undefined') {
						setProperty(target, { name: name, newValue: copy });
					}
				}
			}
		}
	}

	// Return the modified object
	return target;
};

var bail_1 = bail;

function bail(err) {
  if (err) {
    throw err
  }
}

var toString$4 = Object.prototype.toString;

var isPlainObj = function (x) {
	var prototype;
	return toString$4.call(x) === '[object Object]' && (prototype = Object.getPrototypeOf(x), prototype === null || prototype === Object.getPrototypeOf({}));
};

/* Dependencies. */







/* Expose a frozen processor. */
var unified_1 = unified().freeze();

var slice$3 = [].slice;
var own$2 = {}.hasOwnProperty;

/* Process pipeline. */
var pipeline = trough_1()
  .use(pipelineParse)
  .use(pipelineRun)
  .use(pipelineStringify);

function pipelineParse(p, ctx) {
  ctx.tree = p.parse(ctx.file);
}

function pipelineRun(p, ctx, next) {
  p.run(ctx.tree, ctx.file, done);

  function done(err, tree, file) {
    if (err) {
      next(err);
    } else {
      ctx.tree = tree;
      ctx.file = file;
      next();
    }
  }
}

function pipelineStringify(p, ctx) {
  ctx.file.contents = p.stringify(ctx.tree, ctx.file);
}

/* Function to create the first processor. */
function unified() {
  var attachers = [];
  var transformers = trough_1();
  var namespace = {};
  var frozen = false;
  var freezeIndex = -1;

  /* Data management. */
  processor.data = data;

  /* Lock. */
  processor.freeze = freeze;

  /* Plug-ins. */
  processor.attachers = attachers;
  processor.use = use;

  /* API. */
  processor.parse = parse;
  processor.stringify = stringify;
  processor.run = run;
  processor.runSync = runSync;
  processor.process = process;
  processor.processSync = processSync;

  /* Expose. */
  return processor

  /* Create a new processor based on the processor
   * in the current scope. */
  function processor() {
    var destination = unified();
    var length = attachers.length;
    var index = -1;

    while (++index < length) {
      destination.use.apply(null, attachers[index]);
    }

    destination.data(extend$3(true, {}, namespace));

    return destination
  }

  /* Freeze: used to signal a processor that has finished
   * configuration.
   *
   * For example, take unified itself.  It’s frozen.
   * Plug-ins should not be added to it.  Rather, it should
   * be extended, by invoking it, before modifying it.
   *
   * In essence, always invoke this when exporting a
   * processor. */
  function freeze() {
    var values;
    var plugin;
    var options;
    var transformer;

    if (frozen) {
      return processor
    }

    while (++freezeIndex < attachers.length) {
      values = attachers[freezeIndex];
      plugin = values[0];
      options = values[1];
      transformer = null;

      if (options === false) {
        continue
      }

      if (options === true) {
        values[1] = undefined;
      }

      transformer = plugin.apply(processor, values.slice(1));

      if (typeof transformer === 'function') {
        transformers.use(transformer);
      }
    }

    frozen = true;
    freezeIndex = Infinity;

    return processor
  }

  /* Data management.
   * Getter / setter for processor-specific informtion. */
  function data(key, value) {
    if (xIsString(key)) {
      /* Set `key`. */
      if (arguments.length === 2) {
        assertUnfrozen('data', frozen);

        namespace[key] = value;

        return processor
      }

      /* Get `key`. */
      return (own$2.call(namespace, key) && namespace[key]) || null
    }

    /* Set space. */
    if (key) {
      assertUnfrozen('data', frozen);
      namespace = key;
      return processor
    }

    /* Get space. */
    return namespace
  }

  /* Plug-in management.
   *
   * Pass it:
   * *   an attacher and options,
   * *   a preset,
   * *   a list of presets, attachers, and arguments (list
   *     of attachers and options). */
  function use(value) {
    var settings;

    assertUnfrozen('use', frozen);

    if (value === null || value === undefined) {
      /* Empty */
    } else if (typeof value === 'function') {
      addPlugin.apply(null, arguments);
    } else if (typeof value === 'object') {
      if ('length' in value) {
        addList(value);
      } else {
        addPreset(value);
      }
    } else {
      throw new Error('Expected usable value, not `' + value + '`')
    }

    if (settings) {
      namespace.settings = extend$3(namespace.settings || {}, settings);
    }

    return processor

    function addPreset(result) {
      addList(result.plugins);

      if (result.settings) {
        settings = extend$3(settings || {}, result.settings);
      }
    }

    function add(value) {
      if (typeof value === 'function') {
        addPlugin(value);
      } else if (typeof value === 'object') {
        if ('length' in value) {
          addPlugin.apply(null, value);
        } else {
          addPreset(value);
        }
      } else {
        throw new Error('Expected usable value, not `' + value + '`')
      }
    }

    function addList(plugins) {
      var length;
      var index;

      if (plugins === null || plugins === undefined) {
        /* Empty */
      } else if (typeof plugins === 'object' && 'length' in plugins) {
        length = plugins.length;
        index = -1;

        while (++index < length) {
          add(plugins[index]);
        }
      } else {
        throw new Error('Expected a list of plugins, not `' + plugins + '`')
      }
    }

    function addPlugin(plugin, value) {
      var entry = find(plugin);

      if (entry) {
        if (isPlainObj(entry[1]) && isPlainObj(value)) {
          value = extend$3(entry[1], value);
        }

        entry[1] = value;
      } else {
        attachers.push(slice$3.call(arguments));
      }
    }
  }

  function find(plugin) {
    var length = attachers.length;
    var index = -1;
    var entry;

    while (++index < length) {
      entry = attachers[index];

      if (entry[0] === plugin) {
        return entry
      }
    }
  }

  /* Parse a file (in string or VFile representation)
   * into a Unist node using the `Parser` on the
   * processor. */
  function parse(doc) {
    var file = vfile(doc);
    var Parser;

    freeze();
    Parser = processor.Parser;
    assertParser('parse', Parser);

    if (newable(Parser)) {
      return new Parser(String(file), file).parse()
    }

    return Parser(String(file), file) // eslint-disable-line new-cap
  }

  /* Run transforms on a Unist node representation of a file
   * (in string or VFile representation), async. */
  function run(node, file, cb) {
    assertNode(node);
    freeze();

    if (!cb && typeof file === 'function') {
      cb = file;
      file = null;
    }

    if (!cb) {
      return new Promise(executor)
    }

    executor(null, cb);

    function executor(resolve, reject) {
      transformers.run(node, vfile(file), done);

      function done(err, tree, file) {
        tree = tree || node;
        if (err) {
          reject(err);
        } else if (resolve) {
          resolve(tree);
        } else {
          cb(null, tree, file);
        }
      }
    }
  }

  /* Run transforms on a Unist node representation of a file
   * (in string or VFile representation), sync. */
  function runSync(node, file) {
    var complete = false;
    var result;

    run(node, file, done);

    assertDone('runSync', 'run', complete);

    return result

    function done(err, tree) {
      complete = true;
      bail_1(err);
      result = tree;
    }
  }

  /* Stringify a Unist node representation of a file
   * (in string or VFile representation) into a string
   * using the `Compiler` on the processor. */
  function stringify(node, doc) {
    var file = vfile(doc);
    var Compiler;

    freeze();
    Compiler = processor.Compiler;
    assertCompiler('stringify', Compiler);
    assertNode(node);

    if (newable(Compiler)) {
      return new Compiler(node, file).compile()
    }

    return Compiler(node, file) // eslint-disable-line new-cap
  }

  /* Parse a file (in string or VFile representation)
   * into a Unist node using the `Parser` on the processor,
   * then run transforms on that node, and compile the
   * resulting node using the `Compiler` on the processor,
   * and store that result on the VFile. */
  function process(doc, cb) {
    freeze();
    assertParser('process', processor.Parser);
    assertCompiler('process', processor.Compiler);

    if (!cb) {
      return new Promise(executor)
    }

    executor(null, cb);

    function executor(resolve, reject) {
      var file = vfile(doc);

      pipeline.run(processor, {file: file}, done);

      function done(err) {
        if (err) {
          reject(err);
        } else if (resolve) {
          resolve(file);
        } else {
          cb(null, file);
        }
      }
    }
  }

  /* Process the given document (in string or VFile
   * representation), sync. */
  function processSync(doc) {
    var complete = false;
    var file;

    freeze();
    assertParser('processSync', processor.Parser);
    assertCompiler('processSync', processor.Compiler);
    file = vfile(doc);

    process(file, done);

    assertDone('processSync', 'process', complete);

    return file

    function done(err) {
      complete = true;
      bail_1(err);
    }
  }
}

/* Check if `func` is a constructor. */
function newable(value) {
  return typeof value === 'function' && keys(value.prototype)
}

/* Check if `value` is an object with keys. */
function keys(value) {
  var key;
  for (key in value) {
    return true
  }
  return false
}

/* Assert a parser is available. */
function assertParser(name, Parser) {
  if (typeof Parser !== 'function') {
    throw new Error('Cannot `' + name + '` without `Parser`')
  }
}

/* Assert a compiler is available. */
function assertCompiler(name, Compiler) {
  if (typeof Compiler !== 'function') {
    throw new Error('Cannot `' + name + '` without `Compiler`')
  }
}

/* Assert the processor is not frozen. */
function assertUnfrozen(name, frozen) {
  if (frozen) {
    throw new Error(
      [
        'Cannot invoke `' + name + '` on a frozen processor.\nCreate a new ',
        'processor first, by invoking it: use `processor()` instead of ',
        '`processor`.'
      ].join('')
    )
  }
}

/* Assert `node` is a Unist node. */
function assertNode(node) {
  if (!node || !xIsString(node.type)) {
    throw new Error('Expected node, got `' + node + '`')
  }
}

/* Assert that `complete` is `true`. */
function assertDone(name, asyncName, complete) {
  if (!complete) {
    throw new Error(
      '`' + name + '` finished async. Use `' + asyncName + '` instead'
    )
  }
}

var unherit_1 = unherit;

/* Create a custom constructor which can be modified
 * without affecting the original class. */
function unherit(Super) {
  var result;
  var key;
  var value;

  inherits(Of, Super);
  inherits(From, Of);

  /* Clone values. */
  result = Of.prototype;

  for (key in result) {
    value = result[key];

    if (value && typeof value === 'object') {
      result[key] = 'concat' in value ? value.concat() : immutable(value);
    }
  }

  return Of

  /* Constructor accepting a single argument,
   * which itself is an `arguments` object. */
  function From(parameters) {
    return Super.apply(this, parameters)
  }

  /* Constructor accepting variadic arguments. */
  function Of() {
    if (!(this instanceof Of)) {
      return new From(arguments)
    }

    return Super.apply(this, arguments)
  }
}

var stateToggle = factory;

/* Construct a state `toggler`: a function which inverses
 * `property` in context based on its current value.
 * The by `toggler` returned function restores that value. */
function factory(key, state, ctx) {
  return enter

  function enter() {
    var context = ctx || this;
    var current = context[key];

    context[key] = !state;

    return exit

    function exit() {
      context[key] = current;
    }
  }
}

/* Expose. */
var vfileLocation = factory$1;

/* Factory. */
function factory$1(file) {
  var contents = indices(String(file));

  return {
    toPosition: offsetToPositionFactory(contents),
    toOffset: positionToOffsetFactory(contents)
  }
}

/* Factory to get the line and column-based `position` for
 * `offset` in the bound indices. */
function offsetToPositionFactory(indices) {
  return offsetToPosition

  /* Get the line and column-based `position` for
   * `offset` in the bound indices. */
  function offsetToPosition(offset) {
    var index = -1;
    var length = indices.length;

    if (offset < 0) {
      return {}
    }

    while (++index < length) {
      if (indices[index] > offset) {
        return {
          line: index + 1,
          column: offset - (indices[index - 1] || 0) + 1,
          offset: offset
        }
      }
    }

    return {}
  }
}

/* Factory to get the `offset` for a line and column-based
 * `position` in the bound indices. */
function positionToOffsetFactory(indices) {
  return positionToOffset

  /* Get the `offset` for a line and column-based
   * `position` in the bound indices. */
  function positionToOffset(position) {
    var line = position && position.line;
    var column = position && position.column;

    if (!isNaN(line) && !isNaN(column) && line - 1 in indices) {
      return (indices[line - 2] || 0) + column - 1 || 0
    }

    return -1
  }
}

/* Get indices of line-breaks in `value`. */
function indices(value) {
  var result = [];
  var index = value.indexOf('\n');

  while (index !== -1) {
    result.push(index + 1);
    index = value.indexOf('\n', index + 1);
  }

  result.push(value.length + 1);

  return result
}

var _unescape = factory$2;

/* Factory to de-escape a value, based on a list at `key`
 * in `ctx`. */
function factory$2(ctx, key) {
  return unescape;

  /* De-escape a string using the expression at `key`
   * in `ctx`. */
  function unescape(value) {
    var prev = 0;
    var index = value.indexOf('\\');
    var escape = ctx[key];
    var queue = [];
    var character;

    while (index !== -1) {
      queue.push(value.slice(prev, index));
      prev = index + 1;
      character = value.charAt(prev);

      /* If the following character is not a valid escape,
       * add the slash. */
      if (!character || escape.indexOf(character) === -1) {
        queue.push('\\');
      }

      index = value.indexOf('\\', prev);
    }

    queue.push(value.slice(prev));

    return queue.join('');
  }
}

const AEli = "Æ";
const AElig = "Æ";
const AM = "&";
const AMP = "&";
const Aacut = "Á";
const Aacute = "Á";
const Abreve = "Ă";
const Acir = "Â";
const Acirc = "Â";
const Acy = "А";
const Afr = "𝔄";
const Agrav = "À";
const Agrave = "À";
const Alpha = "Α";
const Amacr = "Ā";
const And = "⩓";
const Aogon = "Ą";
const Aopf = "𝔸";
const ApplyFunction = "⁡";
const Arin = "Å";
const Aring = "Å";
const Ascr = "𝒜";
const Assign = "≔";
const Atild = "Ã";
const Atilde = "Ã";
const Aum = "Ä";
const Auml = "Ä";
const Backslash = "∖";
const Barv = "⫧";
const Barwed = "⌆";
const Bcy = "Б";
const Because = "∵";
const Bernoullis = "ℬ";
const Beta = "Β";
const Bfr = "𝔅";
const Bopf = "𝔹";
const Breve = "˘";
const Bscr = "ℬ";
const Bumpeq = "≎";
const CHcy = "Ч";
const COP = "©";
const COPY = "©";
const Cacute = "Ć";
const Cap = "⋒";
const CapitalDifferentialD = "ⅅ";
const Cayleys = "ℭ";
const Ccaron = "Č";
const Ccedi = "Ç";
const Ccedil = "Ç";
const Ccirc = "Ĉ";
const Cconint = "∰";
const Cdot = "Ċ";
const Cedilla = "¸";
const CenterDot = "·";
const Cfr = "ℭ";
const Chi = "Χ";
const CircleDot = "⊙";
const CircleMinus = "⊖";
const CirclePlus = "⊕";
const CircleTimes = "⊗";
const ClockwiseContourIntegral = "∲";
const CloseCurlyDoubleQuote = "”";
const CloseCurlyQuote = "’";
const Colon = "∷";
const Colone = "⩴";
const Congruent = "≡";
const Conint = "∯";
const ContourIntegral = "∮";
const Copf = "ℂ";
const Coproduct = "∐";
const CounterClockwiseContourIntegral = "∳";
const Cross = "⨯";
const Cscr = "𝒞";
const Cup = "⋓";
const CupCap = "≍";
const DD = "ⅅ";
const DDotrahd = "⤑";
const DJcy = "Ђ";
const DScy = "Ѕ";
const DZcy = "Џ";
const Dagger = "‡";
const Darr = "↡";
const Dashv = "⫤";
const Dcaron = "Ď";
const Dcy = "Д";
const Del = "∇";
const Delta = "Δ";
const Dfr = "𝔇";
const DiacriticalAcute = "´";
const DiacriticalDot = "˙";
const DiacriticalDoubleAcute = "˝";
const DiacriticalGrave = "`";
const DiacriticalTilde = "˜";
const Diamond = "⋄";
const DifferentialD = "ⅆ";
const Dopf = "𝔻";
const Dot = "¨";
const DotDot = "⃜";
const DotEqual = "≐";
const DoubleContourIntegral = "∯";
const DoubleDot = "¨";
const DoubleDownArrow = "⇓";
const DoubleLeftArrow = "⇐";
const DoubleLeftRightArrow = "⇔";
const DoubleLeftTee = "⫤";
const DoubleLongLeftArrow = "⟸";
const DoubleLongLeftRightArrow = "⟺";
const DoubleLongRightArrow = "⟹";
const DoubleRightArrow = "⇒";
const DoubleRightTee = "⊨";
const DoubleUpArrow = "⇑";
const DoubleUpDownArrow = "⇕";
const DoubleVerticalBar = "∥";
const DownArrow = "↓";
const DownArrowBar = "⤓";
const DownArrowUpArrow = "⇵";
const DownBreve = "̑";
const DownLeftRightVector = "⥐";
const DownLeftTeeVector = "⥞";
const DownLeftVector = "↽";
const DownLeftVectorBar = "⥖";
const DownRightTeeVector = "⥟";
const DownRightVector = "⇁";
const DownRightVectorBar = "⥗";
const DownTee = "⊤";
const DownTeeArrow = "↧";
const Downarrow = "⇓";
const Dscr = "𝒟";
const Dstrok = "Đ";
const ENG = "Ŋ";
const ET = "Ð";
const ETH = "Ð";
const Eacut = "É";
const Eacute = "É";
const Ecaron = "Ě";
const Ecir = "Ê";
const Ecirc = "Ê";
const Ecy = "Э";
const Edot = "Ė";
const Efr = "𝔈";
const Egrav = "È";
const Egrave = "È";
const Element = "∈";
const Emacr = "Ē";
const EmptySmallSquare = "◻";
const EmptyVerySmallSquare = "▫";
const Eogon = "Ę";
const Eopf = "𝔼";
const Epsilon = "Ε";
const Equal = "⩵";
const EqualTilde = "≂";
const Equilibrium = "⇌";
const Escr = "ℰ";
const Esim = "⩳";
const Eta = "Η";
const Eum = "Ë";
const Euml = "Ë";
const Exists = "∃";
const ExponentialE = "ⅇ";
const Fcy = "Ф";
const Ffr = "𝔉";
const FilledSmallSquare = "◼";
const FilledVerySmallSquare = "▪";
const Fopf = "𝔽";
const ForAll = "∀";
const Fouriertrf = "ℱ";
const Fscr = "ℱ";
const GJcy = "Ѓ";
const G = ">";
const GT = ">";
const Gamma = "Γ";
const Gammad = "Ϝ";
const Gbreve = "Ğ";
const Gcedil = "Ģ";
const Gcirc = "Ĝ";
const Gcy = "Г";
const Gdot = "Ġ";
const Gfr = "𝔊";
const Gg = "⋙";
const Gopf = "𝔾";
const GreaterEqual = "≥";
const GreaterEqualLess = "⋛";
const GreaterFullEqual = "≧";
const GreaterGreater = "⪢";
const GreaterLess = "≷";
const GreaterSlantEqual = "⩾";
const GreaterTilde = "≳";
const Gscr = "𝒢";
const Gt = "≫";
const HARDcy = "Ъ";
const Hacek = "ˇ";
const Hat = "^";
const Hcirc = "Ĥ";
const Hfr = "ℌ";
const HilbertSpace = "ℋ";
const Hopf = "ℍ";
const HorizontalLine = "─";
const Hscr = "ℋ";
const Hstrok = "Ħ";
const HumpDownHump = "≎";
const HumpEqual = "≏";
const IEcy = "Е";
const IJlig = "Ĳ";
const IOcy = "Ё";
const Iacut = "Í";
const Iacute = "Í";
const Icir = "Î";
const Icirc = "Î";
const Icy = "И";
const Idot = "İ";
const Ifr = "ℑ";
const Igrav = "Ì";
const Igrave = "Ì";
const Im = "ℑ";
const Imacr = "Ī";
const ImaginaryI = "ⅈ";
const Implies = "⇒";
const Int = "∬";
const Integral = "∫";
const Intersection = "⋂";
const InvisibleComma = "⁣";
const InvisibleTimes = "⁢";
const Iogon = "Į";
const Iopf = "𝕀";
const Iota = "Ι";
const Iscr = "ℐ";
const Itilde = "Ĩ";
const Iukcy = "І";
const Ium = "Ï";
const Iuml = "Ï";
const Jcirc = "Ĵ";
const Jcy = "Й";
const Jfr = "𝔍";
const Jopf = "𝕁";
const Jscr = "𝒥";
const Jsercy = "Ј";
const Jukcy = "Є";
const KHcy = "Х";
const KJcy = "Ќ";
const Kappa = "Κ";
const Kcedil = "Ķ";
const Kcy = "К";
const Kfr = "𝔎";
const Kopf = "𝕂";
const Kscr = "𝒦";
const LJcy = "Љ";
const L = "<";
const LT = "<";
const Lacute = "Ĺ";
const Lambda = "Λ";
const Lang = "⟪";
const Laplacetrf = "ℒ";
const Larr = "↞";
const Lcaron = "Ľ";
const Lcedil = "Ļ";
const Lcy = "Л";
const LeftAngleBracket = "⟨";
const LeftArrow = "←";
const LeftArrowBar = "⇤";
const LeftArrowRightArrow = "⇆";
const LeftCeiling = "⌈";
const LeftDoubleBracket = "⟦";
const LeftDownTeeVector = "⥡";
const LeftDownVector = "⇃";
const LeftDownVectorBar = "⥙";
const LeftFloor = "⌊";
const LeftRightArrow = "↔";
const LeftRightVector = "⥎";
const LeftTee = "⊣";
const LeftTeeArrow = "↤";
const LeftTeeVector = "⥚";
const LeftTriangle = "⊲";
const LeftTriangleBar = "⧏";
const LeftTriangleEqual = "⊴";
const LeftUpDownVector = "⥑";
const LeftUpTeeVector = "⥠";
const LeftUpVector = "↿";
const LeftUpVectorBar = "⥘";
const LeftVector = "↼";
const LeftVectorBar = "⥒";
const Leftarrow = "⇐";
const Leftrightarrow = "⇔";
const LessEqualGreater = "⋚";
const LessFullEqual = "≦";
const LessGreater = "≶";
const LessLess = "⪡";
const LessSlantEqual = "⩽";
const LessTilde = "≲";
const Lfr = "𝔏";
const Ll = "⋘";
const Lleftarrow = "⇚";
const Lmidot = "Ŀ";
const LongLeftArrow = "⟵";
const LongLeftRightArrow = "⟷";
const LongRightArrow = "⟶";
const Longleftarrow = "⟸";
const Longleftrightarrow = "⟺";
const Longrightarrow = "⟹";
const Lopf = "𝕃";
const LowerLeftArrow = "↙";
const LowerRightArrow = "↘";
const Lscr = "ℒ";
const Lsh = "↰";
const Lstrok = "Ł";
const Lt = "≪";
const Mcy = "М";
const MediumSpace = " ";
const Mellintrf = "ℳ";
const Mfr = "𝔐";
const MinusPlus = "∓";
const Mopf = "𝕄";
const Mscr = "ℳ";
const Mu = "Μ";
const NJcy = "Њ";
const Nacute = "Ń";
const Ncaron = "Ň";
const Ncedil = "Ņ";
const Ncy = "Н";
const NegativeMediumSpace = "​";
const NegativeThickSpace = "​";
const NegativeThinSpace = "​";
const NegativeVeryThinSpace = "​";
const NestedGreaterGreater = "≫";
const NestedLessLess = "≪";
const NewLine = "\n";
const Nfr = "𝔑";
const NoBreak = "⁠";
const NonBreakingSpace = " ";
const Nopf = "ℕ";
const Not = "⫬";
const NotCongruent = "≢";
const NotCupCap = "≭";
const NotDoubleVerticalBar = "∦";
const NotElement = "∉";
const NotEqual = "≠";
const NotEqualTilde = "≂̸";
const NotExists = "∄";
const NotGreater = "≯";
const NotGreaterEqual = "≱";
const NotGreaterFullEqual = "≧̸";
const NotGreaterGreater = "≫̸";
const NotGreaterLess = "≹";
const NotGreaterSlantEqual = "⩾̸";
const NotGreaterTilde = "≵";
const NotHumpDownHump = "≎̸";
const NotHumpEqual = "≏̸";
const NotLeftTriangle = "⋪";
const NotLeftTriangleBar = "⧏̸";
const NotLeftTriangleEqual = "⋬";
const NotLess = "≮";
const NotLessEqual = "≰";
const NotLessGreater = "≸";
const NotLessLess = "≪̸";
const NotLessSlantEqual = "⩽̸";
const NotLessTilde = "≴";
const NotNestedGreaterGreater = "⪢̸";
const NotNestedLessLess = "⪡̸";
const NotPrecedes = "⊀";
const NotPrecedesEqual = "⪯̸";
const NotPrecedesSlantEqual = "⋠";
const NotReverseElement = "∌";
const NotRightTriangle = "⋫";
const NotRightTriangleBar = "⧐̸";
const NotRightTriangleEqual = "⋭";
const NotSquareSubset = "⊏̸";
const NotSquareSubsetEqual = "⋢";
const NotSquareSuperset = "⊐̸";
const NotSquareSupersetEqual = "⋣";
const NotSubset = "⊂⃒";
const NotSubsetEqual = "⊈";
const NotSucceeds = "⊁";
const NotSucceedsEqual = "⪰̸";
const NotSucceedsSlantEqual = "⋡";
const NotSucceedsTilde = "≿̸";
const NotSuperset = "⊃⃒";
const NotSupersetEqual = "⊉";
const NotTilde = "≁";
const NotTildeEqual = "≄";
const NotTildeFullEqual = "≇";
const NotTildeTilde = "≉";
const NotVerticalBar = "∤";
const Nscr = "𝒩";
const Ntild = "Ñ";
const Ntilde = "Ñ";
const Nu = "Ν";
const OElig = "Œ";
const Oacut = "Ó";
const Oacute = "Ó";
const Ocir = "Ô";
const Ocirc = "Ô";
const Ocy = "О";
const Odblac = "Ő";
const Ofr = "𝔒";
const Ograv = "Ò";
const Ograve = "Ò";
const Omacr = "Ō";
const Omega = "Ω";
const Omicron = "Ο";
const Oopf = "𝕆";
const OpenCurlyDoubleQuote = "“";
const OpenCurlyQuote = "‘";
const Or = "⩔";
const Oscr = "𝒪";
const Oslas = "Ø";
const Oslash = "Ø";
const Otild = "Õ";
const Otilde = "Õ";
const Otimes = "⨷";
const Oum = "Ö";
const Ouml = "Ö";
const OverBar = "‾";
const OverBrace = "⏞";
const OverBracket = "⎴";
const OverParenthesis = "⏜";
const PartialD = "∂";
const Pcy = "П";
const Pfr = "𝔓";
const Phi = "Φ";
const Pi = "Π";
const PlusMinus = "±";
const Poincareplane = "ℌ";
const Popf = "ℙ";
const Pr = "⪻";
const Precedes = "≺";
const PrecedesEqual = "⪯";
const PrecedesSlantEqual = "≼";
const PrecedesTilde = "≾";
const Prime = "″";
const Product = "∏";
const Proportion = "∷";
const Proportional = "∝";
const Pscr = "𝒫";
const Psi = "Ψ";
const QUO = "\"";
const QUOT = "\"";
const Qfr = "𝔔";
const Qopf = "ℚ";
const Qscr = "𝒬";
const RBarr = "⤐";
const RE = "®";
const REG = "®";
const Racute = "Ŕ";
const Rang = "⟫";
const Rarr = "↠";
const Rarrtl = "⤖";
const Rcaron = "Ř";
const Rcedil = "Ŗ";
const Rcy = "Р";
const Re = "ℜ";
const ReverseElement = "∋";
const ReverseEquilibrium = "⇋";
const ReverseUpEquilibrium = "⥯";
const Rfr = "ℜ";
const Rho = "Ρ";
const RightAngleBracket = "⟩";
const RightArrow = "→";
const RightArrowBar = "⇥";
const RightArrowLeftArrow = "⇄";
const RightCeiling = "⌉";
const RightDoubleBracket = "⟧";
const RightDownTeeVector = "⥝";
const RightDownVector = "⇂";
const RightDownVectorBar = "⥕";
const RightFloor = "⌋";
const RightTee = "⊢";
const RightTeeArrow = "↦";
const RightTeeVector = "⥛";
const RightTriangle = "⊳";
const RightTriangleBar = "⧐";
const RightTriangleEqual = "⊵";
const RightUpDownVector = "⥏";
const RightUpTeeVector = "⥜";
const RightUpVector = "↾";
const RightUpVectorBar = "⥔";
const RightVector = "⇀";
const RightVectorBar = "⥓";
const Rightarrow = "⇒";
const Ropf = "ℝ";
const RoundImplies = "⥰";
const Rrightarrow = "⇛";
const Rscr = "ℛ";
const Rsh = "↱";
const RuleDelayed = "⧴";
const SHCHcy = "Щ";
const SHcy = "Ш";
const SOFTcy = "Ь";
const Sacute = "Ś";
const Sc = "⪼";
const Scaron = "Š";
const Scedil = "Ş";
const Scirc = "Ŝ";
const Scy = "С";
const Sfr = "𝔖";
const ShortDownArrow = "↓";
const ShortLeftArrow = "←";
const ShortRightArrow = "→";
const ShortUpArrow = "↑";
const Sigma = "Σ";
const SmallCircle = "∘";
const Sopf = "𝕊";
const Sqrt = "√";
const Square = "□";
const SquareIntersection = "⊓";
const SquareSubset = "⊏";
const SquareSubsetEqual = "⊑";
const SquareSuperset = "⊐";
const SquareSupersetEqual = "⊒";
const SquareUnion = "⊔";
const Sscr = "𝒮";
const Star = "⋆";
const Sub = "⋐";
const Subset = "⋐";
const SubsetEqual = "⊆";
const Succeeds = "≻";
const SucceedsEqual = "⪰";
const SucceedsSlantEqual = "≽";
const SucceedsTilde = "≿";
const SuchThat = "∋";
const Sum = "∑";
const Sup = "⋑";
const Superset = "⊃";
const SupersetEqual = "⊇";
const Supset = "⋑";
const THOR = "Þ";
const THORN = "Þ";
const TRADE = "™";
const TSHcy = "Ћ";
const TScy = "Ц";
const Tab = "\t";
const Tau = "Τ";
const Tcaron = "Ť";
const Tcedil = "Ţ";
const Tcy = "Т";
const Tfr = "𝔗";
const Therefore = "∴";
const Theta = "Θ";
const ThickSpace = "  ";
const ThinSpace = " ";
const Tilde = "∼";
const TildeEqual = "≃";
const TildeFullEqual = "≅";
const TildeTilde = "≈";
const Topf = "𝕋";
const TripleDot = "⃛";
const Tscr = "𝒯";
const Tstrok = "Ŧ";
const Uacut = "Ú";
const Uacute = "Ú";
const Uarr = "↟";
const Uarrocir = "⥉";
const Ubrcy = "Ў";
const Ubreve = "Ŭ";
const Ucir = "Û";
const Ucirc = "Û";
const Ucy = "У";
const Udblac = "Ű";
const Ufr = "𝔘";
const Ugrav = "Ù";
const Ugrave = "Ù";
const Umacr = "Ū";
const UnderBar = "_";
const UnderBrace = "⏟";
const UnderBracket = "⎵";
const UnderParenthesis = "⏝";
const Union = "⋃";
const UnionPlus = "⊎";
const Uogon = "Ų";
const Uopf = "𝕌";
const UpArrow = "↑";
const UpArrowBar = "⤒";
const UpArrowDownArrow = "⇅";
const UpDownArrow = "↕";
const UpEquilibrium = "⥮";
const UpTee = "⊥";
const UpTeeArrow = "↥";
const Uparrow = "⇑";
const Updownarrow = "⇕";
const UpperLeftArrow = "↖";
const UpperRightArrow = "↗";
const Upsi = "ϒ";
const Upsilon = "Υ";
const Uring = "Ů";
const Uscr = "𝒰";
const Utilde = "Ũ";
const Uum = "Ü";
const Uuml = "Ü";
const VDash = "⊫";
const Vbar = "⫫";
const Vcy = "В";
const Vdash = "⊩";
const Vdashl = "⫦";
const Vee = "⋁";
const Verbar = "‖";
const Vert = "‖";
const VerticalBar = "∣";
const VerticalLine = "|";
const VerticalSeparator = "❘";
const VerticalTilde = "≀";
const VeryThinSpace = " ";
const Vfr = "𝔙";
const Vopf = "𝕍";
const Vscr = "𝒱";
const Vvdash = "⊪";
const Wcirc = "Ŵ";
const Wedge = "⋀";
const Wfr = "𝔚";
const Wopf = "𝕎";
const Wscr = "𝒲";
const Xfr = "𝔛";
const Xi = "Ξ";
const Xopf = "𝕏";
const Xscr = "𝒳";
const YAcy = "Я";
const YIcy = "Ї";
const YUcy = "Ю";
const Yacut = "Ý";
const Yacute = "Ý";
const Ycirc = "Ŷ";
const Ycy = "Ы";
const Yfr = "𝔜";
const Yopf = "𝕐";
const Yscr = "𝒴";
const Yuml = "Ÿ";
const ZHcy = "Ж";
const Zacute = "Ź";
const Zcaron = "Ž";
const Zcy = "З";
const Zdot = "Ż";
const ZeroWidthSpace = "​";
const Zeta = "Ζ";
const Zfr = "ℨ";
const Zopf = "ℤ";
const Zscr = "𝒵";
const aacut = "á";
const aacute = "á";
const abreve = "ă";
const ac = "∾";
const acE = "∾̳";
const acd = "∿";
const acir = "â";
const acirc = "â";
const acut = "´";
const acute = "´";
const acy = "а";
const aeli = "æ";
const aelig = "æ";
const af = "⁡";
const afr = "𝔞";
const agrav = "à";
const agrave = "à";
const alefsym = "ℵ";
const aleph = "ℵ";
const alpha = "α";
const amacr = "ā";
const amalg = "⨿";
const am = "&";
const amp = "&";
const and = "∧";
const andand = "⩕";
const andd = "⩜";
const andslope = "⩘";
const andv = "⩚";
const ang = "∠";
const ange = "⦤";
const angle = "∠";
const angmsd = "∡";
const angmsdaa = "⦨";
const angmsdab = "⦩";
const angmsdac = "⦪";
const angmsdad = "⦫";
const angmsdae = "⦬";
const angmsdaf = "⦭";
const angmsdag = "⦮";
const angmsdah = "⦯";
const angrt = "∟";
const angrtvb = "⊾";
const angrtvbd = "⦝";
const angsph = "∢";
const angst = "Å";
const angzarr = "⍼";
const aogon = "ą";
const aopf = "𝕒";
const ap = "≈";
const apE = "⩰";
const apacir = "⩯";
const ape = "≊";
const apid = "≋";
const apos = "'";
const approx = "≈";
const approxeq = "≊";
const arin = "å";
const aring = "å";
const ascr = "𝒶";
const ast = "*";
const asymp = "≈";
const asympeq = "≍";
const atild = "ã";
const atilde = "ã";
const aum = "ä";
const auml = "ä";
const awconint = "∳";
const awint = "⨑";
const bNot = "⫭";
const backcong = "≌";
const backepsilon = "϶";
const backprime = "‵";
const backsim = "∽";
const backsimeq = "⋍";
const barvee = "⊽";
const barwed = "⌅";
const barwedge = "⌅";
const bbrk = "⎵";
const bbrktbrk = "⎶";
const bcong = "≌";
const bcy = "б";
const bdquo = "„";
const becaus = "∵";
const because = "∵";
const bemptyv = "⦰";
const bepsi = "϶";
const bernou = "ℬ";
const beta = "β";
const beth = "ℶ";
const between = "≬";
const bfr = "𝔟";
const bigcap = "⋂";
const bigcirc = "◯";
const bigcup = "⋃";
const bigodot = "⨀";
const bigoplus = "⨁";
const bigotimes = "⨂";
const bigsqcup = "⨆";
const bigstar = "★";
const bigtriangledown = "▽";
const bigtriangleup = "△";
const biguplus = "⨄";
const bigvee = "⋁";
const bigwedge = "⋀";
const bkarow = "⤍";
const blacklozenge = "⧫";
const blacksquare = "▪";
const blacktriangle = "▴";
const blacktriangledown = "▾";
const blacktriangleleft = "◂";
const blacktriangleright = "▸";
const blank = "␣";
const blk12 = "▒";
const blk14 = "░";
const blk34 = "▓";
const block = "█";
const bne = "=⃥";
const bnequiv = "≡⃥";
const bnot = "⌐";
const bopf = "𝕓";
const bot = "⊥";
const bottom = "⊥";
const bowtie = "⋈";
const boxDL = "╗";
const boxDR = "╔";
const boxDl = "╖";
const boxDr = "╓";
const boxH = "═";
const boxHD = "╦";
const boxHU = "╩";
const boxHd = "╤";
const boxHu = "╧";
const boxUL = "╝";
const boxUR = "╚";
const boxUl = "╜";
const boxUr = "╙";
const boxV = "║";
const boxVH = "╬";
const boxVL = "╣";
const boxVR = "╠";
const boxVh = "╫";
const boxVl = "╢";
const boxVr = "╟";
const boxbox = "⧉";
const boxdL = "╕";
const boxdR = "╒";
const boxdl = "┐";
const boxdr = "┌";
const boxh = "─";
const boxhD = "╥";
const boxhU = "╨";
const boxhd = "┬";
const boxhu = "┴";
const boxminus = "⊟";
const boxplus = "⊞";
const boxtimes = "⊠";
const boxuL = "╛";
const boxuR = "╘";
const boxul = "┘";
const boxur = "└";
const boxv = "│";
const boxvH = "╪";
const boxvL = "╡";
const boxvR = "╞";
const boxvh = "┼";
const boxvl = "┤";
const boxvr = "├";
const bprime = "‵";
const breve = "˘";
const brvba = "¦";
const brvbar = "¦";
const bscr = "𝒷";
const bsemi = "⁏";
const bsim = "∽";
const bsime = "⋍";
const bsol = "\\";
const bsolb = "⧅";
const bsolhsub = "⟈";
const bull = "•";
const bullet = "•";
const bump = "≎";
const bumpE = "⪮";
const bumpe = "≏";
const bumpeq = "≏";
const cacute = "ć";
const cap = "∩";
const capand = "⩄";
const capbrcup = "⩉";
const capcap = "⩋";
const capcup = "⩇";
const capdot = "⩀";
const caps = "∩︀";
const caret = "⁁";
const caron = "ˇ";
const ccaps = "⩍";
const ccaron = "č";
const ccedi = "ç";
const ccedil = "ç";
const ccirc = "ĉ";
const ccups = "⩌";
const ccupssm = "⩐";
const cdot = "ċ";
const cedi = "¸";
const cedil = "¸";
const cemptyv = "⦲";
const cen = "¢";
const cent = "¢";
const centerdot = "·";
const cfr = "𝔠";
const chcy = "ч";
const check$2 = "✓";
const checkmark = "✓";
const chi = "χ";
const cir = "○";
const cirE = "⧃";
const circ = "ˆ";
const circeq = "≗";
const circlearrowleft = "↺";
const circlearrowright = "↻";
const circledR = "®";
const circledS = "Ⓢ";
const circledast = "⊛";
const circledcirc = "⊚";
const circleddash = "⊝";
const cire = "≗";
const cirfnint = "⨐";
const cirmid = "⫯";
const cirscir = "⧂";
const clubs = "♣";
const clubsuit = "♣";
const colon = ":";
const colone = "≔";
const coloneq = "≔";
const comma = ",";
const commat = "@";
const comp = "∁";
const compfn = "∘";
const complement = "∁";
const complexes = "ℂ";
const cong = "≅";
const congdot = "⩭";
const conint = "∮";
const copf = "𝕔";
const coprod = "∐";
const cop = "©";
const copy$2 = "©";
const copysr = "℗";
const crarr = "↵";
const cross = "✗";
const cscr = "𝒸";
const csub = "⫏";
const csube = "⫑";
const csup = "⫐";
const csupe = "⫒";
const ctdot = "⋯";
const cudarrl = "⤸";
const cudarrr = "⤵";
const cuepr = "⋞";
const cuesc = "⋟";
const cularr = "↶";
const cularrp = "⤽";
const cup = "∪";
const cupbrcap = "⩈";
const cupcap = "⩆";
const cupcup = "⩊";
const cupdot = "⊍";
const cupor = "⩅";
const cups = "∪︀";
const curarr = "↷";
const curarrm = "⤼";
const curlyeqprec = "⋞";
const curlyeqsucc = "⋟";
const curlyvee = "⋎";
const curlywedge = "⋏";
const curre = "¤";
const curren = "¤";
const curvearrowleft = "↶";
const curvearrowright = "↷";
const cuvee = "⋎";
const cuwed = "⋏";
const cwconint = "∲";
const cwint = "∱";
const cylcty = "⌭";
const dArr = "⇓";
const dHar = "⥥";
const dagger = "†";
const daleth = "ℸ";
const darr = "↓";
const dash = "‐";
const dashv = "⊣";
const dbkarow = "⤏";
const dblac = "˝";
const dcaron = "ď";
const dcy = "д";
const dd = "ⅆ";
const ddagger = "‡";
const ddarr = "⇊";
const ddotseq = "⩷";
const de = "°";
const deg = "°";
const delta = "δ";
const demptyv = "⦱";
const dfisht = "⥿";
const dfr = "𝔡";
const dharl = "⇃";
const dharr = "⇂";
const diam = "⋄";
const diamond = "⋄";
const diamondsuit = "♦";
const diams = "♦";
const die = "¨";
const digamma = "ϝ";
const disin = "⋲";
const div = "÷";
const divid = "÷";
const divide = "÷";
const divideontimes = "⋇";
const divonx = "⋇";
const djcy = "ђ";
const dlcorn = "⌞";
const dlcrop = "⌍";
const dollar = "$";
const dopf = "𝕕";
const dot = "˙";
const doteq = "≐";
const doteqdot = "≑";
const dotminus = "∸";
const dotplus = "∔";
const dotsquare = "⊡";
const doublebarwedge = "⌆";
const downarrow = "↓";
const downdownarrows = "⇊";
const downharpoonleft = "⇃";
const downharpoonright = "⇂";
const drbkarow = "⤐";
const drcorn = "⌟";
const drcrop = "⌌";
const dscr = "𝒹";
const dscy = "ѕ";
const dsol = "⧶";
const dstrok = "đ";
const dtdot = "⋱";
const dtri = "▿";
const dtrif = "▾";
const duarr = "⇵";
const duhar = "⥯";
const dwangle = "⦦";
const dzcy = "џ";
const dzigrarr = "⟿";
const eDDot = "⩷";
const eDot = "≑";
const eacut = "é";
const eacute = "é";
const easter = "⩮";
const ecaron = "ě";
const ecir = "ê";
const ecirc = "ê";
const ecolon = "≕";
const ecy = "э";
const edot = "ė";
const ee = "ⅇ";
const efDot = "≒";
const efr = "𝔢";
const eg = "⪚";
const egrav = "è";
const egrave = "è";
const egs = "⪖";
const egsdot = "⪘";
const el = "⪙";
const elinters = "⏧";
const ell = "ℓ";
const els = "⪕";
const elsdot = "⪗";
const emacr = "ē";
const empty = "∅";
const emptyset = "∅";
const emptyv = "∅";
const emsp13 = " ";
const emsp14 = " ";
const emsp = " ";
const eng = "ŋ";
const ensp = " ";
const eogon = "ę";
const eopf = "𝕖";
const epar = "⋕";
const eparsl = "⧣";
const eplus = "⩱";
const epsi = "ε";
const epsilon = "ε";
const epsiv = "ϵ";
const eqcirc = "≖";
const eqcolon = "≕";
const eqsim = "≂";
const eqslantgtr = "⪖";
const eqslantless = "⪕";
const equals = "=";
const equest = "≟";
const equiv = "≡";
const equivDD = "⩸";
const eqvparsl = "⧥";
const erDot = "≓";
const erarr = "⥱";
const escr = "ℯ";
const esdot = "≐";
const esim = "≂";
const eta = "η";
const et = "ð";
const eth = "ð";
const eum = "ë";
const euml = "ë";
const euro = "€";
const excl = "!";
const exist = "∃";
const expectation = "ℰ";
const exponentiale = "ⅇ";
const fallingdotseq = "≒";
const fcy = "ф";
const female = "♀";
const ffilig = "ﬃ";
const fflig = "ﬀ";
const ffllig = "ﬄ";
const ffr = "𝔣";
const filig = "ﬁ";
const fjlig = "fj";
const flat = "♭";
const fllig = "ﬂ";
const fltns = "▱";
const fnof = "ƒ";
const fopf = "𝕗";
const forall = "∀";
const fork = "⋔";
const forkv = "⫙";
const fpartint = "⨍";
const frac1 = "¼";
const frac12 = "½";
const frac13 = "⅓";
const frac14 = "¼";
const frac15 = "⅕";
const frac16 = "⅙";
const frac18 = "⅛";
const frac23 = "⅔";
const frac25 = "⅖";
const frac3 = "¾";
const frac34 = "¾";
const frac35 = "⅗";
const frac38 = "⅜";
const frac45 = "⅘";
const frac56 = "⅚";
const frac58 = "⅝";
const frac78 = "⅞";
const frasl = "⁄";
const frown = "⌢";
const fscr = "𝒻";
const gE = "≧";
const gEl = "⪌";
const gacute = "ǵ";
const gamma = "γ";
const gammad = "ϝ";
const gap = "⪆";
const gbreve = "ğ";
const gcirc = "ĝ";
const gcy = "г";
const gdot = "ġ";
const ge = "≥";
const gel = "⋛";
const geq = "≥";
const geqq = "≧";
const geqslant = "⩾";
const ges = "⩾";
const gescc = "⪩";
const gesdot = "⪀";
const gesdoto = "⪂";
const gesdotol = "⪄";
const gesl = "⋛︀";
const gesles = "⪔";
const gfr = "𝔤";
const gg = "≫";
const ggg = "⋙";
const gimel = "ℷ";
const gjcy = "ѓ";
const gl = "≷";
const glE = "⪒";
const gla = "⪥";
const glj = "⪤";
const gnE = "≩";
const gnap = "⪊";
const gnapprox = "⪊";
const gne = "⪈";
const gneq = "⪈";
const gneqq = "≩";
const gnsim = "⋧";
const gopf = "𝕘";
const grave = "`";
const gscr = "ℊ";
const gsim = "≳";
const gsime = "⪎";
const gsiml = "⪐";
const g = ">";
const gt = ">";
const gtcc = "⪧";
const gtcir = "⩺";
const gtdot = "⋗";
const gtlPar = "⦕";
const gtquest = "⩼";
const gtrapprox = "⪆";
const gtrarr = "⥸";
const gtrdot = "⋗";
const gtreqless = "⋛";
const gtreqqless = "⪌";
const gtrless = "≷";
const gtrsim = "≳";
const gvertneqq = "≩︀";
const gvnE = "≩︀";
const hArr = "⇔";
const hairsp = " ";
const half = "½";
const hamilt = "ℋ";
const hardcy = "ъ";
const harr = "↔";
const harrcir = "⥈";
const harrw = "↭";
const hbar = "ℏ";
const hcirc = "ĥ";
const hearts = "♥";
const heartsuit = "♥";
const hellip = "…";
const hercon = "⊹";
const hfr = "𝔥";
const hksearow = "⤥";
const hkswarow = "⤦";
const hoarr = "⇿";
const homtht = "∻";
const hookleftarrow = "↩";
const hookrightarrow = "↪";
const hopf = "𝕙";
const horbar = "―";
const hscr = "𝒽";
const hslash = "ℏ";
const hstrok = "ħ";
const hybull = "⁃";
const hyphen = "‐";
const iacut = "í";
const iacute = "í";
const ic = "⁣";
const icir = "î";
const icirc = "î";
const icy = "и";
const iecy = "е";
const iexc = "¡";
const iexcl = "¡";
const iff = "⇔";
const ifr = "𝔦";
const igrav = "ì";
const igrave = "ì";
const ii = "ⅈ";
const iiiint = "⨌";
const iiint = "∭";
const iinfin = "⧜";
const iiota = "℩";
const ijlig = "ĳ";
const imacr = "ī";
const image = "ℑ";
const imagline = "ℐ";
const imagpart = "ℑ";
const imath = "ı";
const imof = "⊷";
const imped = "Ƶ";
const incare = "℅";
const infin = "∞";
const infintie = "⧝";
const inodot = "ı";
const int = "∫";
const intcal = "⊺";
const integers = "ℤ";
const intercal = "⊺";
const intlarhk = "⨗";
const intprod = "⨼";
const iocy = "ё";
const iogon = "į";
const iopf = "𝕚";
const iota = "ι";
const iprod = "⨼";
const iques = "¿";
const iquest = "¿";
const iscr = "𝒾";
const isin = "∈";
const isinE = "⋹";
const isindot = "⋵";
const isins = "⋴";
const isinsv = "⋳";
const isinv = "∈";
const it = "⁢";
const itilde = "ĩ";
const iukcy = "і";
const ium = "ï";
const iuml = "ï";
const jcirc = "ĵ";
const jcy = "й";
const jfr = "𝔧";
const jmath = "ȷ";
const jopf = "𝕛";
const jscr = "𝒿";
const jsercy = "ј";
const jukcy = "є";
const kappa = "κ";
const kappav = "ϰ";
const kcedil = "ķ";
const kcy = "к";
const kfr = "𝔨";
const kgreen = "ĸ";
const khcy = "х";
const kjcy = "ќ";
const kopf = "𝕜";
const kscr = "𝓀";
const lAarr = "⇚";
const lArr = "⇐";
const lAtail = "⤛";
const lBarr = "⤎";
const lE = "≦";
const lEg = "⪋";
const lHar = "⥢";
const lacute = "ĺ";
const laemptyv = "⦴";
const lagran = "ℒ";
const lambda = "λ";
const lang = "⟨";
const langd = "⦑";
const langle = "⟨";
const lap = "⪅";
const laqu = "«";
const laquo = "«";
const larr = "←";
const larrb = "⇤";
const larrbfs = "⤟";
const larrfs = "⤝";
const larrhk = "↩";
const larrlp = "↫";
const larrpl = "⤹";
const larrsim = "⥳";
const larrtl = "↢";
const lat = "⪫";
const latail = "⤙";
const late = "⪭";
const lates = "⪭︀";
const lbarr = "⤌";
const lbbrk = "❲";
const lbrace = "{";
const lbrack = "[";
const lbrke = "⦋";
const lbrksld = "⦏";
const lbrkslu = "⦍";
const lcaron = "ľ";
const lcedil = "ļ";
const lceil = "⌈";
const lcub = "{";
const lcy = "л";
const ldca = "⤶";
const ldquo = "“";
const ldquor = "„";
const ldrdhar = "⥧";
const ldrushar = "⥋";
const ldsh = "↲";
const le = "≤";
const leftarrow = "←";
const leftarrowtail = "↢";
const leftharpoondown = "↽";
const leftharpoonup = "↼";
const leftleftarrows = "⇇";
const leftrightarrow = "↔";
const leftrightarrows = "⇆";
const leftrightharpoons = "⇋";
const leftrightsquigarrow = "↭";
const leftthreetimes = "⋋";
const leg = "⋚";
const leq = "≤";
const leqq = "≦";
const leqslant = "⩽";
const les = "⩽";
const lescc = "⪨";
const lesdot = "⩿";
const lesdoto = "⪁";
const lesdotor = "⪃";
const lesg = "⋚︀";
const lesges = "⪓";
const lessapprox = "⪅";
const lessdot = "⋖";
const lesseqgtr = "⋚";
const lesseqqgtr = "⪋";
const lessgtr = "≶";
const lesssim = "≲";
const lfisht = "⥼";
const lfloor = "⌊";
const lfr = "𝔩";
const lg = "≶";
const lgE = "⪑";
const lhard = "↽";
const lharu = "↼";
const lharul = "⥪";
const lhblk = "▄";
const ljcy = "љ";
const ll = "≪";
const llarr = "⇇";
const llcorner = "⌞";
const llhard = "⥫";
const lltri = "◺";
const lmidot = "ŀ";
const lmoust = "⎰";
const lmoustache = "⎰";
const lnE = "≨";
const lnap = "⪉";
const lnapprox = "⪉";
const lne = "⪇";
const lneq = "⪇";
const lneqq = "≨";
const lnsim = "⋦";
const loang = "⟬";
const loarr = "⇽";
const lobrk = "⟦";
const longleftarrow = "⟵";
const longleftrightarrow = "⟷";
const longmapsto = "⟼";
const longrightarrow = "⟶";
const looparrowleft = "↫";
const looparrowright = "↬";
const lopar = "⦅";
const lopf = "𝕝";
const loplus = "⨭";
const lotimes = "⨴";
const lowast = "∗";
const lowbar = "_";
const loz = "◊";
const lozenge = "◊";
const lozf = "⧫";
const lpar = "(";
const lparlt = "⦓";
const lrarr = "⇆";
const lrcorner = "⌟";
const lrhar = "⇋";
const lrhard = "⥭";
const lrm = "‎";
const lrtri = "⊿";
const lsaquo = "‹";
const lscr = "𝓁";
const lsh = "↰";
const lsim = "≲";
const lsime = "⪍";
const lsimg = "⪏";
const lsqb = "[";
const lsquo = "‘";
const lsquor = "‚";
const lstrok = "ł";
const l = "<";
const lt = "<";
const ltcc = "⪦";
const ltcir = "⩹";
const ltdot = "⋖";
const lthree = "⋋";
const ltimes = "⋉";
const ltlarr = "⥶";
const ltquest = "⩻";
const ltrPar = "⦖";
const ltri = "◃";
const ltrie = "⊴";
const ltrif = "◂";
const lurdshar = "⥊";
const luruhar = "⥦";
const lvertneqq = "≨︀";
const lvnE = "≨︀";
const mDDot = "∺";
const mac = "¯";
const macr = "¯";
const male = "♂";
const malt = "✠";
const maltese = "✠";
const map$3 = "↦";
const mapsto = "↦";
const mapstodown = "↧";
const mapstoleft = "↤";
const mapstoup = "↥";
const marker = "▮";
const mcomma = "⨩";
const mcy = "м";
const mdash = "—";
const measuredangle = "∡";
const mfr = "𝔪";
const mho = "℧";
const micr = "µ";
const micro = "µ";
const mid = "∣";
const midast = "*";
const midcir = "⫰";
const middo = "·";
const middot = "·";
const minus = "−";
const minusb = "⊟";
const minusd = "∸";
const minusdu = "⨪";
const mlcp = "⫛";
const mldr = "…";
const mnplus = "∓";
const models = "⊧";
const mopf = "𝕞";
const mp = "∓";
const mscr = "𝓂";
const mstpos = "∾";
const mu = "μ";
const multimap = "⊸";
const mumap = "⊸";
const nGg = "⋙̸";
const nGt = "≫⃒";
const nGtv = "≫̸";
const nLeftarrow = "⇍";
const nLeftrightarrow = "⇎";
const nLl = "⋘̸";
const nLt = "≪⃒";
const nLtv = "≪̸";
const nRightarrow = "⇏";
const nVDash = "⊯";
const nVdash = "⊮";
const nabla = "∇";
const nacute = "ń";
const nang = "∠⃒";
const nap = "≉";
const napE = "⩰̸";
const napid = "≋̸";
const napos = "ŉ";
const napprox = "≉";
const natur = "♮";
const natural = "♮";
const naturals = "ℕ";
const nbs = " ";
const nbsp = " ";
const nbump = "≎̸";
const nbumpe = "≏̸";
const ncap = "⩃";
const ncaron = "ň";
const ncedil = "ņ";
const ncong = "≇";
const ncongdot = "⩭̸";
const ncup = "⩂";
const ncy = "н";
const ndash = "–";
const ne = "≠";
const neArr = "⇗";
const nearhk = "⤤";
const nearr = "↗";
const nearrow = "↗";
const nedot = "≐̸";
const nequiv = "≢";
const nesear = "⤨";
const nesim = "≂̸";
const nexist = "∄";
const nexists = "∄";
const nfr = "𝔫";
const ngE = "≧̸";
const nge = "≱";
const ngeq = "≱";
const ngeqq = "≧̸";
const ngeqslant = "⩾̸";
const nges = "⩾̸";
const ngsim = "≵";
const ngt = "≯";
const ngtr = "≯";
const nhArr = "⇎";
const nharr = "↮";
const nhpar = "⫲";
const ni = "∋";
const nis = "⋼";
const nisd = "⋺";
const niv = "∋";
const njcy = "њ";
const nlArr = "⇍";
const nlE = "≦̸";
const nlarr = "↚";
const nldr = "‥";
const nle = "≰";
const nleftarrow = "↚";
const nleftrightarrow = "↮";
const nleq = "≰";
const nleqq = "≦̸";
const nleqslant = "⩽̸";
const nles = "⩽̸";
const nless = "≮";
const nlsim = "≴";
const nlt = "≮";
const nltri = "⋪";
const nltrie = "⋬";
const nmid = "∤";
const nopf = "𝕟";
const no = "¬";
const not = "¬";
const notin = "∉";
const notinE = "⋹̸";
const notindot = "⋵̸";
const notinva = "∉";
const notinvb = "⋷";
const notinvc = "⋶";
const notni = "∌";
const notniva = "∌";
const notnivb = "⋾";
const notnivc = "⋽";
const npar = "∦";
const nparallel = "∦";
const nparsl = "⫽⃥";
const npart = "∂̸";
const npolint = "⨔";
const npr = "⊀";
const nprcue = "⋠";
const npre = "⪯̸";
const nprec = "⊀";
const npreceq = "⪯̸";
const nrArr = "⇏";
const nrarr = "↛";
const nrarrc = "⤳̸";
const nrarrw = "↝̸";
const nrightarrow = "↛";
const nrtri = "⋫";
const nrtrie = "⋭";
const nsc = "⊁";
const nsccue = "⋡";
const nsce = "⪰̸";
const nscr = "𝓃";
const nshortmid = "∤";
const nshortparallel = "∦";
const nsim = "≁";
const nsime = "≄";
const nsimeq = "≄";
const nsmid = "∤";
const nspar = "∦";
const nsqsube = "⋢";
const nsqsupe = "⋣";
const nsub = "⊄";
const nsubE = "⫅̸";
const nsube = "⊈";
const nsubset = "⊂⃒";
const nsubseteq = "⊈";
const nsubseteqq = "⫅̸";
const nsucc = "⊁";
const nsucceq = "⪰̸";
const nsup = "⊅";
const nsupE = "⫆̸";
const nsupe = "⊉";
const nsupset = "⊃⃒";
const nsupseteq = "⊉";
const nsupseteqq = "⫆̸";
const ntgl = "≹";
const ntild = "ñ";
const ntilde = "ñ";
const ntlg = "≸";
const ntriangleleft = "⋪";
const ntrianglelefteq = "⋬";
const ntriangleright = "⋫";
const ntrianglerighteq = "⋭";
const nu = "ν";
const num = "#";
const numero = "№";
const numsp = " ";
const nvDash = "⊭";
const nvHarr = "⤄";
const nvap = "≍⃒";
const nvdash = "⊬";
const nvge = "≥⃒";
const nvgt = ">⃒";
const nvinfin = "⧞";
const nvlArr = "⤂";
const nvle = "≤⃒";
const nvlt = "<⃒";
const nvltrie = "⊴⃒";
const nvrArr = "⤃";
const nvrtrie = "⊵⃒";
const nvsim = "∼⃒";
const nwArr = "⇖";
const nwarhk = "⤣";
const nwarr = "↖";
const nwarrow = "↖";
const nwnear = "⤧";
const oS = "Ⓢ";
const oacut = "ó";
const oacute = "ó";
const oast = "⊛";
const ocir = "ô";
const ocirc = "ô";
const ocy = "о";
const odash = "⊝";
const odblac = "ő";
const odiv = "⨸";
const odot = "⊙";
const odsold = "⦼";
const oelig = "œ";
const ofcir = "⦿";
const ofr = "𝔬";
const ogon = "˛";
const ograv = "ò";
const ograve = "ò";
const ogt = "⧁";
const ohbar = "⦵";
const ohm = "Ω";
const oint = "∮";
const olarr = "↺";
const olcir = "⦾";
const olcross = "⦻";
const oline = "‾";
const olt = "⧀";
const omacr = "ō";
const omega = "ω";
const omicron = "ο";
const omid = "⦶";
const ominus = "⊖";
const oopf = "𝕠";
const opar = "⦷";
const operp = "⦹";
const oplus = "⊕";
const or = "∨";
const orarr = "↻";
const ord = "º";
const order$1 = "ℴ";
const orderof = "ℴ";
const ordf = "ª";
const ordm = "º";
const origof = "⊶";
const oror = "⩖";
const orslope = "⩗";
const orv = "⩛";
const oscr = "ℴ";
const oslas = "ø";
const oslash = "ø";
const osol = "⊘";
const otild = "õ";
const otilde = "õ";
const otimes = "⊗";
const otimesas = "⨶";
const oum = "ö";
const ouml = "ö";
const ovbar = "⌽";
const par = "¶";
const para = "¶";
const parallel = "∥";
const parsim = "⫳";
const parsl = "⫽";
const part = "∂";
const pcy = "п";
const percnt = "%";
const period = ".";
const permil = "‰";
const perp = "⊥";
const pertenk = "‱";
const pfr = "𝔭";
const phi = "φ";
const phiv = "ϕ";
const phmmat = "ℳ";
const phone = "☎";
const pi = "π";
const pitchfork = "⋔";
const piv = "ϖ";
const planck = "ℏ";
const planckh = "ℎ";
const plankv = "ℏ";
const plus = "+";
const plusacir = "⨣";
const plusb = "⊞";
const pluscir = "⨢";
const plusdo = "∔";
const plusdu = "⨥";
const pluse = "⩲";
const plusm = "±";
const plusmn = "±";
const plussim = "⨦";
const plustwo = "⨧";
const pm = "±";
const pointint = "⨕";
const popf = "𝕡";
const poun = "£";
const pound = "£";
const pr = "≺";
const prE = "⪳";
const prap = "⪷";
const prcue = "≼";
const pre = "⪯";
const prec = "≺";
const precapprox = "⪷";
const preccurlyeq = "≼";
const preceq = "⪯";
const precnapprox = "⪹";
const precneqq = "⪵";
const precnsim = "⋨";
const precsim = "≾";
const prime = "′";
const primes = "ℙ";
const prnE = "⪵";
const prnap = "⪹";
const prnsim = "⋨";
const prod = "∏";
const profalar = "⌮";
const profline = "⌒";
const profsurf = "⌓";
const prop = "∝";
const propto = "∝";
const prsim = "≾";
const prurel = "⊰";
const pscr = "𝓅";
const psi = "ψ";
const puncsp = " ";
const qfr = "𝔮";
const qint = "⨌";
const qopf = "𝕢";
const qprime = "⁗";
const qscr = "𝓆";
const quaternions = "ℍ";
const quatint = "⨖";
const quest = "?";
const questeq = "≟";
const quo = "\"";
const quot = "\"";
const rAarr = "⇛";
const rArr = "⇒";
const rAtail = "⤜";
const rBarr = "⤏";
const rHar = "⥤";
const race = "∽̱";
const racute = "ŕ";
const radic = "√";
const raemptyv = "⦳";
const rang = "⟩";
const rangd = "⦒";
const range$1 = "⦥";
const rangle = "⟩";
const raqu = "»";
const raquo = "»";
const rarr = "→";
const rarrap = "⥵";
const rarrb = "⇥";
const rarrbfs = "⤠";
const rarrc = "⤳";
const rarrfs = "⤞";
const rarrhk = "↪";
const rarrlp = "↬";
const rarrpl = "⥅";
const rarrsim = "⥴";
const rarrtl = "↣";
const rarrw = "↝";
const ratail = "⤚";
const ratio = "∶";
const rationals = "ℚ";
const rbarr = "⤍";
const rbbrk = "❳";
const rbrace = "}";
const rbrack = "]";
const rbrke = "⦌";
const rbrksld = "⦎";
const rbrkslu = "⦐";
const rcaron = "ř";
const rcedil = "ŗ";
const rceil = "⌉";
const rcub = "}";
const rcy = "р";
const rdca = "⤷";
const rdldhar = "⥩";
const rdquo = "”";
const rdquor = "”";
const rdsh = "↳";
const real = "ℜ";
const realine = "ℛ";
const realpart = "ℜ";
const reals = "ℝ";
const rect = "▭";
const re = "®";
const reg = "®";
const rfisht = "⥽";
const rfloor = "⌋";
const rfr = "𝔯";
const rhard = "⇁";
const rharu = "⇀";
const rharul = "⥬";
const rho = "ρ";
const rhov = "ϱ";
const rightarrow = "→";
const rightarrowtail = "↣";
const rightharpoondown = "⇁";
const rightharpoonup = "⇀";
const rightleftarrows = "⇄";
const rightleftharpoons = "⇌";
const rightrightarrows = "⇉";
const rightsquigarrow = "↝";
const rightthreetimes = "⋌";
const ring = "˚";
const risingdotseq = "≓";
const rlarr = "⇄";
const rlhar = "⇌";
const rlm = "‏";
const rmoust = "⎱";
const rmoustache = "⎱";
const rnmid = "⫮";
const roang = "⟭";
const roarr = "⇾";
const robrk = "⟧";
const ropar = "⦆";
const ropf = "𝕣";
const roplus = "⨮";
const rotimes = "⨵";
const rpar = ")";
const rpargt = "⦔";
const rppolint = "⨒";
const rrarr = "⇉";
const rsaquo = "›";
const rscr = "𝓇";
const rsh = "↱";
const rsqb = "]";
const rsquo = "’";
const rsquor = "’";
const rthree = "⋌";
const rtimes = "⋊";
const rtri = "▹";
const rtrie = "⊵";
const rtrif = "▸";
const rtriltri = "⧎";
const ruluhar = "⥨";
const rx = "℞";
const sacute = "ś";
const sbquo = "‚";
const sc = "≻";
const scE = "⪴";
const scap = "⪸";
const scaron = "š";
const sccue = "≽";
const sce = "⪰";
const scedil = "ş";
const scirc = "ŝ";
const scnE = "⪶";
const scnap = "⪺";
const scnsim = "⋩";
const scpolint = "⨓";
const scsim = "≿";
const scy = "с";
const sdot = "⋅";
const sdotb = "⊡";
const sdote = "⩦";
const seArr = "⇘";
const searhk = "⤥";
const searr = "↘";
const searrow = "↘";
const sec = "§";
const sect = "§";
const semi = ";";
const seswar = "⤩";
const setminus = "∖";
const setmn = "∖";
const sext = "✶";
const sfr = "𝔰";
const sfrown = "⌢";
const sharp = "♯";
const shchcy = "щ";
const shcy = "ш";
const shortmid = "∣";
const shortparallel = "∥";
const sh = "­";
const shy = "­";
const sigma = "σ";
const sigmaf = "ς";
const sigmav = "ς";
const sim = "∼";
const simdot = "⩪";
const sime = "≃";
const simeq = "≃";
const simg = "⪞";
const simgE = "⪠";
const siml = "⪝";
const simlE = "⪟";
const simne = "≆";
const simplus = "⨤";
const simrarr = "⥲";
const slarr = "←";
const smallsetminus = "∖";
const smashp = "⨳";
const smeparsl = "⧤";
const smid = "∣";
const smile = "⌣";
const smt = "⪪";
const smte = "⪬";
const smtes = "⪬︀";
const softcy = "ь";
const sol = "/";
const solb = "⧄";
const solbar = "⌿";
const sopf = "𝕤";
const spades = "♠";
const spadesuit = "♠";
const spar = "∥";
const sqcap = "⊓";
const sqcaps = "⊓︀";
const sqcup = "⊔";
const sqcups = "⊔︀";
const sqsub = "⊏";
const sqsube = "⊑";
const sqsubset = "⊏";
const sqsubseteq = "⊑";
const sqsup = "⊐";
const sqsupe = "⊒";
const sqsupset = "⊐";
const sqsupseteq = "⊒";
const squ = "□";
const square = "□";
const squarf = "▪";
const squf = "▪";
const srarr = "→";
const sscr = "𝓈";
const ssetmn = "∖";
const ssmile = "⌣";
const sstarf = "⋆";
const star$1 = "☆";
const starf = "★";
const straightepsilon = "ϵ";
const straightphi = "ϕ";
const strns = "¯";
const sub = "⊂";
const subE = "⫅";
const subdot = "⪽";
const sube = "⊆";
const subedot = "⫃";
const submult = "⫁";
const subnE = "⫋";
const subne = "⊊";
const subplus = "⪿";
const subrarr = "⥹";
const subset = "⊂";
const subseteq = "⊆";
const subseteqq = "⫅";
const subsetneq = "⊊";
const subsetneqq = "⫋";
const subsim = "⫇";
const subsub = "⫕";
const subsup = "⫓";
const succ = "≻";
const succapprox = "⪸";
const succcurlyeq = "≽";
const succeq = "⪰";
const succnapprox = "⪺";
const succneqq = "⪶";
const succnsim = "⋩";
const succsim = "≿";
const sum = "∑";
const sung = "♪";
const sup = "⊃";
const sup1 = "¹";
const sup2 = "²";
const sup3 = "³";
const supE = "⫆";
const supdot = "⪾";
const supdsub = "⫘";
const supe = "⊇";
const supedot = "⫄";
const suphsol = "⟉";
const suphsub = "⫗";
const suplarr = "⥻";
const supmult = "⫂";
const supnE = "⫌";
const supne = "⊋";
const supplus = "⫀";
const supset = "⊃";
const supseteq = "⊇";
const supseteqq = "⫆";
const supsetneq = "⊋";
const supsetneqq = "⫌";
const supsim = "⫈";
const supsub = "⫔";
const supsup = "⫖";
const swArr = "⇙";
const swarhk = "⤦";
const swarr = "↙";
const swarrow = "↙";
const swnwar = "⤪";
const szli = "ß";
const szlig = "ß";
const target = "⌖";
const tau = "τ";
const tbrk = "⎴";
const tcaron = "ť";
const tcedil = "ţ";
const tcy = "т";
const tdot = "⃛";
const telrec = "⌕";
const tfr = "𝔱";
const there4 = "∴";
const therefore = "∴";
const theta = "θ";
const thetasym = "ϑ";
const thetav = "ϑ";
const thickapprox = "≈";
const thicksim = "∼";
const thinsp = " ";
const thkap = "≈";
const thksim = "∼";
const thor = "þ";
const thorn = "þ";
const tilde = "˜";
const time = "×";
const times = "×";
const timesb = "⊠";
const timesbar = "⨱";
const timesd = "⨰";
const tint = "∭";
const toea = "⤨";
const top = "⊤";
const topbot = "⌶";
const topcir = "⫱";
const topf = "𝕥";
const topfork = "⫚";
const tosa = "⤩";
const tprime = "‴";
const trade = "™";
const triangle = "▵";
const triangledown = "▿";
const triangleleft = "◃";
const trianglelefteq = "⊴";
const triangleq = "≜";
const triangleright = "▹";
const trianglerighteq = "⊵";
const tridot = "◬";
const trie = "≜";
const triminus = "⨺";
const triplus = "⨹";
const trisb = "⧍";
const tritime = "⨻";
const trpezium = "⏢";
const tscr = "𝓉";
const tscy = "ц";
const tshcy = "ћ";
const tstrok = "ŧ";
const twixt = "≬";
const twoheadleftarrow = "↞";
const twoheadrightarrow = "↠";
const uArr = "⇑";
const uHar = "⥣";
const uacut = "ú";
const uacute = "ú";
const uarr = "↑";
const ubrcy = "ў";
const ubreve = "ŭ";
const ucir = "û";
const ucirc = "û";
const ucy = "у";
const udarr = "⇅";
const udblac = "ű";
const udhar = "⥮";
const ufisht = "⥾";
const ufr = "𝔲";
const ugrav = "ù";
const ugrave = "ù";
const uharl = "↿";
const uharr = "↾";
const uhblk = "▀";
const ulcorn = "⌜";
const ulcorner = "⌜";
const ulcrop = "⌏";
const ultri = "◸";
const umacr = "ū";
const um = "¨";
const uml = "¨";
const uogon = "ų";
const uopf = "𝕦";
const uparrow = "↑";
const updownarrow = "↕";
const upharpoonleft = "↿";
const upharpoonright = "↾";
const uplus = "⊎";
const upsi = "υ";
const upsih = "ϒ";
const upsilon = "υ";
const upuparrows = "⇈";
const urcorn = "⌝";
const urcorner = "⌝";
const urcrop = "⌎";
const uring = "ů";
const urtri = "◹";
const uscr = "𝓊";
const utdot = "⋰";
const utilde = "ũ";
const utri = "▵";
const utrif = "▴";
const uuarr = "⇈";
const uum = "ü";
const uuml = "ü";
const uwangle = "⦧";
const vArr = "⇕";
const vBar = "⫨";
const vBarv = "⫩";
const vDash = "⊨";
const vangrt = "⦜";
const varepsilon = "ϵ";
const varkappa = "ϰ";
const varnothing = "∅";
const varphi = "ϕ";
const varpi = "ϖ";
const varpropto = "∝";
const varr = "↕";
const varrho = "ϱ";
const varsigma = "ς";
const varsubsetneq = "⊊︀";
const varsubsetneqq = "⫋︀";
const varsupsetneq = "⊋︀";
const varsupsetneqq = "⫌︀";
const vartheta = "ϑ";
const vartriangleleft = "⊲";
const vartriangleright = "⊳";
const vcy = "в";
const vdash = "⊢";
const vee = "∨";
const veebar = "⊻";
const veeeq = "≚";
const vellip = "⋮";
const verbar = "|";
const vert = "|";
const vfr = "𝔳";
const vltri = "⊲";
const vnsub = "⊂⃒";
const vnsup = "⊃⃒";
const vopf = "𝕧";
const vprop = "∝";
const vrtri = "⊳";
const vscr = "𝓋";
const vsubnE = "⫋︀";
const vsubne = "⊊︀";
const vsupnE = "⫌︀";
const vsupne = "⊋︀";
const vzigzag = "⦚";
const wcirc = "ŵ";
const wedbar = "⩟";
const wedge = "∧";
const wedgeq = "≙";
const weierp = "℘";
const wfr = "𝔴";
const wopf = "𝕨";
const wp = "℘";
const wr = "≀";
const wreath = "≀";
const wscr = "𝓌";
const xcap = "⋂";
const xcirc = "◯";
const xcup = "⋃";
const xdtri = "▽";
const xfr = "𝔵";
const xhArr = "⟺";
const xharr = "⟷";
const xi = "ξ";
const xlArr = "⟸";
const xlarr = "⟵";
const xmap = "⟼";
const xnis = "⋻";
const xodot = "⨀";
const xopf = "𝕩";
const xoplus = "⨁";
const xotime = "⨂";
const xrArr = "⟹";
const xrarr = "⟶";
const xscr = "𝓍";
const xsqcup = "⨆";
const xuplus = "⨄";
const xutri = "△";
const xvee = "⋁";
const xwedge = "⋀";
const yacut = "ý";
const yacute = "ý";
const yacy = "я";
const ycirc = "ŷ";
const ycy = "ы";
const ye = "¥";
const yen = "¥";
const yfr = "𝔶";
const yicy = "ї";
const yopf = "𝕪";
const yscr = "𝓎";
const yucy = "ю";
const yum = "ÿ";
const yuml = "ÿ";
const zacute = "ź";
const zcaron = "ž";
const zcy = "з";
const zdot = "ż";
const zeetrf = "ℨ";
const zeta = "ζ";
const zfr = "𝔷";
const zhcy = "ж";
const zigrarr = "⇝";
const zopf = "𝕫";
const zscr = "𝓏";
const zwj = "‍";
const zwnj = "‌";
var index$2 = {
	AEli: AEli,
	AElig: AElig,
	AM: AM,
	AMP: AMP,
	Aacut: Aacut,
	Aacute: Aacute,
	Abreve: Abreve,
	Acir: Acir,
	Acirc: Acirc,
	Acy: Acy,
	Afr: Afr,
	Agrav: Agrav,
	Agrave: Agrave,
	Alpha: Alpha,
	Amacr: Amacr,
	And: And,
	Aogon: Aogon,
	Aopf: Aopf,
	ApplyFunction: ApplyFunction,
	Arin: Arin,
	Aring: Aring,
	Ascr: Ascr,
	Assign: Assign,
	Atild: Atild,
	Atilde: Atilde,
	Aum: Aum,
	Auml: Auml,
	Backslash: Backslash,
	Barv: Barv,
	Barwed: Barwed,
	Bcy: Bcy,
	Because: Because,
	Bernoullis: Bernoullis,
	Beta: Beta,
	Bfr: Bfr,
	Bopf: Bopf,
	Breve: Breve,
	Bscr: Bscr,
	Bumpeq: Bumpeq,
	CHcy: CHcy,
	COP: COP,
	COPY: COPY,
	Cacute: Cacute,
	Cap: Cap,
	CapitalDifferentialD: CapitalDifferentialD,
	Cayleys: Cayleys,
	Ccaron: Ccaron,
	Ccedi: Ccedi,
	Ccedil: Ccedil,
	Ccirc: Ccirc,
	Cconint: Cconint,
	Cdot: Cdot,
	Cedilla: Cedilla,
	CenterDot: CenterDot,
	Cfr: Cfr,
	Chi: Chi,
	CircleDot: CircleDot,
	CircleMinus: CircleMinus,
	CirclePlus: CirclePlus,
	CircleTimes: CircleTimes,
	ClockwiseContourIntegral: ClockwiseContourIntegral,
	CloseCurlyDoubleQuote: CloseCurlyDoubleQuote,
	CloseCurlyQuote: CloseCurlyQuote,
	Colon: Colon,
	Colone: Colone,
	Congruent: Congruent,
	Conint: Conint,
	ContourIntegral: ContourIntegral,
	Copf: Copf,
	Coproduct: Coproduct,
	CounterClockwiseContourIntegral: CounterClockwiseContourIntegral,
	Cross: Cross,
	Cscr: Cscr,
	Cup: Cup,
	CupCap: CupCap,
	DD: DD,
	DDotrahd: DDotrahd,
	DJcy: DJcy,
	DScy: DScy,
	DZcy: DZcy,
	Dagger: Dagger,
	Darr: Darr,
	Dashv: Dashv,
	Dcaron: Dcaron,
	Dcy: Dcy,
	Del: Del,
	Delta: Delta,
	Dfr: Dfr,
	DiacriticalAcute: DiacriticalAcute,
	DiacriticalDot: DiacriticalDot,
	DiacriticalDoubleAcute: DiacriticalDoubleAcute,
	DiacriticalGrave: DiacriticalGrave,
	DiacriticalTilde: DiacriticalTilde,
	Diamond: Diamond,
	DifferentialD: DifferentialD,
	Dopf: Dopf,
	Dot: Dot,
	DotDot: DotDot,
	DotEqual: DotEqual,
	DoubleContourIntegral: DoubleContourIntegral,
	DoubleDot: DoubleDot,
	DoubleDownArrow: DoubleDownArrow,
	DoubleLeftArrow: DoubleLeftArrow,
	DoubleLeftRightArrow: DoubleLeftRightArrow,
	DoubleLeftTee: DoubleLeftTee,
	DoubleLongLeftArrow: DoubleLongLeftArrow,
	DoubleLongLeftRightArrow: DoubleLongLeftRightArrow,
	DoubleLongRightArrow: DoubleLongRightArrow,
	DoubleRightArrow: DoubleRightArrow,
	DoubleRightTee: DoubleRightTee,
	DoubleUpArrow: DoubleUpArrow,
	DoubleUpDownArrow: DoubleUpDownArrow,
	DoubleVerticalBar: DoubleVerticalBar,
	DownArrow: DownArrow,
	DownArrowBar: DownArrowBar,
	DownArrowUpArrow: DownArrowUpArrow,
	DownBreve: DownBreve,
	DownLeftRightVector: DownLeftRightVector,
	DownLeftTeeVector: DownLeftTeeVector,
	DownLeftVector: DownLeftVector,
	DownLeftVectorBar: DownLeftVectorBar,
	DownRightTeeVector: DownRightTeeVector,
	DownRightVector: DownRightVector,
	DownRightVectorBar: DownRightVectorBar,
	DownTee: DownTee,
	DownTeeArrow: DownTeeArrow,
	Downarrow: Downarrow,
	Dscr: Dscr,
	Dstrok: Dstrok,
	ENG: ENG,
	ET: ET,
	ETH: ETH,
	Eacut: Eacut,
	Eacute: Eacute,
	Ecaron: Ecaron,
	Ecir: Ecir,
	Ecirc: Ecirc,
	Ecy: Ecy,
	Edot: Edot,
	Efr: Efr,
	Egrav: Egrav,
	Egrave: Egrave,
	Element: Element,
	Emacr: Emacr,
	EmptySmallSquare: EmptySmallSquare,
	EmptyVerySmallSquare: EmptyVerySmallSquare,
	Eogon: Eogon,
	Eopf: Eopf,
	Epsilon: Epsilon,
	Equal: Equal,
	EqualTilde: EqualTilde,
	Equilibrium: Equilibrium,
	Escr: Escr,
	Esim: Esim,
	Eta: Eta,
	Eum: Eum,
	Euml: Euml,
	Exists: Exists,
	ExponentialE: ExponentialE,
	Fcy: Fcy,
	Ffr: Ffr,
	FilledSmallSquare: FilledSmallSquare,
	FilledVerySmallSquare: FilledVerySmallSquare,
	Fopf: Fopf,
	ForAll: ForAll,
	Fouriertrf: Fouriertrf,
	Fscr: Fscr,
	GJcy: GJcy,
	G: G,
	GT: GT,
	Gamma: Gamma,
	Gammad: Gammad,
	Gbreve: Gbreve,
	Gcedil: Gcedil,
	Gcirc: Gcirc,
	Gcy: Gcy,
	Gdot: Gdot,
	Gfr: Gfr,
	Gg: Gg,
	Gopf: Gopf,
	GreaterEqual: GreaterEqual,
	GreaterEqualLess: GreaterEqualLess,
	GreaterFullEqual: GreaterFullEqual,
	GreaterGreater: GreaterGreater,
	GreaterLess: GreaterLess,
	GreaterSlantEqual: GreaterSlantEqual,
	GreaterTilde: GreaterTilde,
	Gscr: Gscr,
	Gt: Gt,
	HARDcy: HARDcy,
	Hacek: Hacek,
	Hat: Hat,
	Hcirc: Hcirc,
	Hfr: Hfr,
	HilbertSpace: HilbertSpace,
	Hopf: Hopf,
	HorizontalLine: HorizontalLine,
	Hscr: Hscr,
	Hstrok: Hstrok,
	HumpDownHump: HumpDownHump,
	HumpEqual: HumpEqual,
	IEcy: IEcy,
	IJlig: IJlig,
	IOcy: IOcy,
	Iacut: Iacut,
	Iacute: Iacute,
	Icir: Icir,
	Icirc: Icirc,
	Icy: Icy,
	Idot: Idot,
	Ifr: Ifr,
	Igrav: Igrav,
	Igrave: Igrave,
	Im: Im,
	Imacr: Imacr,
	ImaginaryI: ImaginaryI,
	Implies: Implies,
	Int: Int,
	Integral: Integral,
	Intersection: Intersection,
	InvisibleComma: InvisibleComma,
	InvisibleTimes: InvisibleTimes,
	Iogon: Iogon,
	Iopf: Iopf,
	Iota: Iota,
	Iscr: Iscr,
	Itilde: Itilde,
	Iukcy: Iukcy,
	Ium: Ium,
	Iuml: Iuml,
	Jcirc: Jcirc,
	Jcy: Jcy,
	Jfr: Jfr,
	Jopf: Jopf,
	Jscr: Jscr,
	Jsercy: Jsercy,
	Jukcy: Jukcy,
	KHcy: KHcy,
	KJcy: KJcy,
	Kappa: Kappa,
	Kcedil: Kcedil,
	Kcy: Kcy,
	Kfr: Kfr,
	Kopf: Kopf,
	Kscr: Kscr,
	LJcy: LJcy,
	L: L,
	LT: LT,
	Lacute: Lacute,
	Lambda: Lambda,
	Lang: Lang,
	Laplacetrf: Laplacetrf,
	Larr: Larr,
	Lcaron: Lcaron,
	Lcedil: Lcedil,
	Lcy: Lcy,
	LeftAngleBracket: LeftAngleBracket,
	LeftArrow: LeftArrow,
	LeftArrowBar: LeftArrowBar,
	LeftArrowRightArrow: LeftArrowRightArrow,
	LeftCeiling: LeftCeiling,
	LeftDoubleBracket: LeftDoubleBracket,
	LeftDownTeeVector: LeftDownTeeVector,
	LeftDownVector: LeftDownVector,
	LeftDownVectorBar: LeftDownVectorBar,
	LeftFloor: LeftFloor,
	LeftRightArrow: LeftRightArrow,
	LeftRightVector: LeftRightVector,
	LeftTee: LeftTee,
	LeftTeeArrow: LeftTeeArrow,
	LeftTeeVector: LeftTeeVector,
	LeftTriangle: LeftTriangle,
	LeftTriangleBar: LeftTriangleBar,
	LeftTriangleEqual: LeftTriangleEqual,
	LeftUpDownVector: LeftUpDownVector,
	LeftUpTeeVector: LeftUpTeeVector,
	LeftUpVector: LeftUpVector,
	LeftUpVectorBar: LeftUpVectorBar,
	LeftVector: LeftVector,
	LeftVectorBar: LeftVectorBar,
	Leftarrow: Leftarrow,
	Leftrightarrow: Leftrightarrow,
	LessEqualGreater: LessEqualGreater,
	LessFullEqual: LessFullEqual,
	LessGreater: LessGreater,
	LessLess: LessLess,
	LessSlantEqual: LessSlantEqual,
	LessTilde: LessTilde,
	Lfr: Lfr,
	Ll: Ll,
	Lleftarrow: Lleftarrow,
	Lmidot: Lmidot,
	LongLeftArrow: LongLeftArrow,
	LongLeftRightArrow: LongLeftRightArrow,
	LongRightArrow: LongRightArrow,
	Longleftarrow: Longleftarrow,
	Longleftrightarrow: Longleftrightarrow,
	Longrightarrow: Longrightarrow,
	Lopf: Lopf,
	LowerLeftArrow: LowerLeftArrow,
	LowerRightArrow: LowerRightArrow,
	Lscr: Lscr,
	Lsh: Lsh,
	Lstrok: Lstrok,
	Lt: Lt,
	Mcy: Mcy,
	MediumSpace: MediumSpace,
	Mellintrf: Mellintrf,
	Mfr: Mfr,
	MinusPlus: MinusPlus,
	Mopf: Mopf,
	Mscr: Mscr,
	Mu: Mu,
	NJcy: NJcy,
	Nacute: Nacute,
	Ncaron: Ncaron,
	Ncedil: Ncedil,
	Ncy: Ncy,
	NegativeMediumSpace: NegativeMediumSpace,
	NegativeThickSpace: NegativeThickSpace,
	NegativeThinSpace: NegativeThinSpace,
	NegativeVeryThinSpace: NegativeVeryThinSpace,
	NestedGreaterGreater: NestedGreaterGreater,
	NestedLessLess: NestedLessLess,
	NewLine: NewLine,
	Nfr: Nfr,
	NoBreak: NoBreak,
	NonBreakingSpace: NonBreakingSpace,
	Nopf: Nopf,
	Not: Not,
	NotCongruent: NotCongruent,
	NotCupCap: NotCupCap,
	NotDoubleVerticalBar: NotDoubleVerticalBar,
	NotElement: NotElement,
	NotEqual: NotEqual,
	NotEqualTilde: NotEqualTilde,
	NotExists: NotExists,
	NotGreater: NotGreater,
	NotGreaterEqual: NotGreaterEqual,
	NotGreaterFullEqual: NotGreaterFullEqual,
	NotGreaterGreater: NotGreaterGreater,
	NotGreaterLess: NotGreaterLess,
	NotGreaterSlantEqual: NotGreaterSlantEqual,
	NotGreaterTilde: NotGreaterTilde,
	NotHumpDownHump: NotHumpDownHump,
	NotHumpEqual: NotHumpEqual,
	NotLeftTriangle: NotLeftTriangle,
	NotLeftTriangleBar: NotLeftTriangleBar,
	NotLeftTriangleEqual: NotLeftTriangleEqual,
	NotLess: NotLess,
	NotLessEqual: NotLessEqual,
	NotLessGreater: NotLessGreater,
	NotLessLess: NotLessLess,
	NotLessSlantEqual: NotLessSlantEqual,
	NotLessTilde: NotLessTilde,
	NotNestedGreaterGreater: NotNestedGreaterGreater,
	NotNestedLessLess: NotNestedLessLess,
	NotPrecedes: NotPrecedes,
	NotPrecedesEqual: NotPrecedesEqual,
	NotPrecedesSlantEqual: NotPrecedesSlantEqual,
	NotReverseElement: NotReverseElement,
	NotRightTriangle: NotRightTriangle,
	NotRightTriangleBar: NotRightTriangleBar,
	NotRightTriangleEqual: NotRightTriangleEqual,
	NotSquareSubset: NotSquareSubset,
	NotSquareSubsetEqual: NotSquareSubsetEqual,
	NotSquareSuperset: NotSquareSuperset,
	NotSquareSupersetEqual: NotSquareSupersetEqual,
	NotSubset: NotSubset,
	NotSubsetEqual: NotSubsetEqual,
	NotSucceeds: NotSucceeds,
	NotSucceedsEqual: NotSucceedsEqual,
	NotSucceedsSlantEqual: NotSucceedsSlantEqual,
	NotSucceedsTilde: NotSucceedsTilde,
	NotSuperset: NotSuperset,
	NotSupersetEqual: NotSupersetEqual,
	NotTilde: NotTilde,
	NotTildeEqual: NotTildeEqual,
	NotTildeFullEqual: NotTildeFullEqual,
	NotTildeTilde: NotTildeTilde,
	NotVerticalBar: NotVerticalBar,
	Nscr: Nscr,
	Ntild: Ntild,
	Ntilde: Ntilde,
	Nu: Nu,
	OElig: OElig,
	Oacut: Oacut,
	Oacute: Oacute,
	Ocir: Ocir,
	Ocirc: Ocirc,
	Ocy: Ocy,
	Odblac: Odblac,
	Ofr: Ofr,
	Ograv: Ograv,
	Ograve: Ograve,
	Omacr: Omacr,
	Omega: Omega,
	Omicron: Omicron,
	Oopf: Oopf,
	OpenCurlyDoubleQuote: OpenCurlyDoubleQuote,
	OpenCurlyQuote: OpenCurlyQuote,
	Or: Or,
	Oscr: Oscr,
	Oslas: Oslas,
	Oslash: Oslash,
	Otild: Otild,
	Otilde: Otilde,
	Otimes: Otimes,
	Oum: Oum,
	Ouml: Ouml,
	OverBar: OverBar,
	OverBrace: OverBrace,
	OverBracket: OverBracket,
	OverParenthesis: OverParenthesis,
	PartialD: PartialD,
	Pcy: Pcy,
	Pfr: Pfr,
	Phi: Phi,
	Pi: Pi,
	PlusMinus: PlusMinus,
	Poincareplane: Poincareplane,
	Popf: Popf,
	Pr: Pr,
	Precedes: Precedes,
	PrecedesEqual: PrecedesEqual,
	PrecedesSlantEqual: PrecedesSlantEqual,
	PrecedesTilde: PrecedesTilde,
	Prime: Prime,
	Product: Product,
	Proportion: Proportion,
	Proportional: Proportional,
	Pscr: Pscr,
	Psi: Psi,
	QUO: QUO,
	QUOT: QUOT,
	Qfr: Qfr,
	Qopf: Qopf,
	Qscr: Qscr,
	RBarr: RBarr,
	RE: RE,
	REG: REG,
	Racute: Racute,
	Rang: Rang,
	Rarr: Rarr,
	Rarrtl: Rarrtl,
	Rcaron: Rcaron,
	Rcedil: Rcedil,
	Rcy: Rcy,
	Re: Re,
	ReverseElement: ReverseElement,
	ReverseEquilibrium: ReverseEquilibrium,
	ReverseUpEquilibrium: ReverseUpEquilibrium,
	Rfr: Rfr,
	Rho: Rho,
	RightAngleBracket: RightAngleBracket,
	RightArrow: RightArrow,
	RightArrowBar: RightArrowBar,
	RightArrowLeftArrow: RightArrowLeftArrow,
	RightCeiling: RightCeiling,
	RightDoubleBracket: RightDoubleBracket,
	RightDownTeeVector: RightDownTeeVector,
	RightDownVector: RightDownVector,
	RightDownVectorBar: RightDownVectorBar,
	RightFloor: RightFloor,
	RightTee: RightTee,
	RightTeeArrow: RightTeeArrow,
	RightTeeVector: RightTeeVector,
	RightTriangle: RightTriangle,
	RightTriangleBar: RightTriangleBar,
	RightTriangleEqual: RightTriangleEqual,
	RightUpDownVector: RightUpDownVector,
	RightUpTeeVector: RightUpTeeVector,
	RightUpVector: RightUpVector,
	RightUpVectorBar: RightUpVectorBar,
	RightVector: RightVector,
	RightVectorBar: RightVectorBar,
	Rightarrow: Rightarrow,
	Ropf: Ropf,
	RoundImplies: RoundImplies,
	Rrightarrow: Rrightarrow,
	Rscr: Rscr,
	Rsh: Rsh,
	RuleDelayed: RuleDelayed,
	SHCHcy: SHCHcy,
	SHcy: SHcy,
	SOFTcy: SOFTcy,
	Sacute: Sacute,
	Sc: Sc,
	Scaron: Scaron,
	Scedil: Scedil,
	Scirc: Scirc,
	Scy: Scy,
	Sfr: Sfr,
	ShortDownArrow: ShortDownArrow,
	ShortLeftArrow: ShortLeftArrow,
	ShortRightArrow: ShortRightArrow,
	ShortUpArrow: ShortUpArrow,
	Sigma: Sigma,
	SmallCircle: SmallCircle,
	Sopf: Sopf,
	Sqrt: Sqrt,
	Square: Square,
	SquareIntersection: SquareIntersection,
	SquareSubset: SquareSubset,
	SquareSubsetEqual: SquareSubsetEqual,
	SquareSuperset: SquareSuperset,
	SquareSupersetEqual: SquareSupersetEqual,
	SquareUnion: SquareUnion,
	Sscr: Sscr,
	Star: Star,
	Sub: Sub,
	Subset: Subset,
	SubsetEqual: SubsetEqual,
	Succeeds: Succeeds,
	SucceedsEqual: SucceedsEqual,
	SucceedsSlantEqual: SucceedsSlantEqual,
	SucceedsTilde: SucceedsTilde,
	SuchThat: SuchThat,
	Sum: Sum,
	Sup: Sup,
	Superset: Superset,
	SupersetEqual: SupersetEqual,
	Supset: Supset,
	THOR: THOR,
	THORN: THORN,
	TRADE: TRADE,
	TSHcy: TSHcy,
	TScy: TScy,
	Tab: Tab,
	Tau: Tau,
	Tcaron: Tcaron,
	Tcedil: Tcedil,
	Tcy: Tcy,
	Tfr: Tfr,
	Therefore: Therefore,
	Theta: Theta,
	ThickSpace: ThickSpace,
	ThinSpace: ThinSpace,
	Tilde: Tilde,
	TildeEqual: TildeEqual,
	TildeFullEqual: TildeFullEqual,
	TildeTilde: TildeTilde,
	Topf: Topf,
	TripleDot: TripleDot,
	Tscr: Tscr,
	Tstrok: Tstrok,
	Uacut: Uacut,
	Uacute: Uacute,
	Uarr: Uarr,
	Uarrocir: Uarrocir,
	Ubrcy: Ubrcy,
	Ubreve: Ubreve,
	Ucir: Ucir,
	Ucirc: Ucirc,
	Ucy: Ucy,
	Udblac: Udblac,
	Ufr: Ufr,
	Ugrav: Ugrav,
	Ugrave: Ugrave,
	Umacr: Umacr,
	UnderBar: UnderBar,
	UnderBrace: UnderBrace,
	UnderBracket: UnderBracket,
	UnderParenthesis: UnderParenthesis,
	Union: Union,
	UnionPlus: UnionPlus,
	Uogon: Uogon,
	Uopf: Uopf,
	UpArrow: UpArrow,
	UpArrowBar: UpArrowBar,
	UpArrowDownArrow: UpArrowDownArrow,
	UpDownArrow: UpDownArrow,
	UpEquilibrium: UpEquilibrium,
	UpTee: UpTee,
	UpTeeArrow: UpTeeArrow,
	Uparrow: Uparrow,
	Updownarrow: Updownarrow,
	UpperLeftArrow: UpperLeftArrow,
	UpperRightArrow: UpperRightArrow,
	Upsi: Upsi,
	Upsilon: Upsilon,
	Uring: Uring,
	Uscr: Uscr,
	Utilde: Utilde,
	Uum: Uum,
	Uuml: Uuml,
	VDash: VDash,
	Vbar: Vbar,
	Vcy: Vcy,
	Vdash: Vdash,
	Vdashl: Vdashl,
	Vee: Vee,
	Verbar: Verbar,
	Vert: Vert,
	VerticalBar: VerticalBar,
	VerticalLine: VerticalLine,
	VerticalSeparator: VerticalSeparator,
	VerticalTilde: VerticalTilde,
	VeryThinSpace: VeryThinSpace,
	Vfr: Vfr,
	Vopf: Vopf,
	Vscr: Vscr,
	Vvdash: Vvdash,
	Wcirc: Wcirc,
	Wedge: Wedge,
	Wfr: Wfr,
	Wopf: Wopf,
	Wscr: Wscr,
	Xfr: Xfr,
	Xi: Xi,
	Xopf: Xopf,
	Xscr: Xscr,
	YAcy: YAcy,
	YIcy: YIcy,
	YUcy: YUcy,
	Yacut: Yacut,
	Yacute: Yacute,
	Ycirc: Ycirc,
	Ycy: Ycy,
	Yfr: Yfr,
	Yopf: Yopf,
	Yscr: Yscr,
	Yuml: Yuml,
	ZHcy: ZHcy,
	Zacute: Zacute,
	Zcaron: Zcaron,
	Zcy: Zcy,
	Zdot: Zdot,
	ZeroWidthSpace: ZeroWidthSpace,
	Zeta: Zeta,
	Zfr: Zfr,
	Zopf: Zopf,
	Zscr: Zscr,
	aacut: aacut,
	aacute: aacute,
	abreve: abreve,
	ac: ac,
	acE: acE,
	acd: acd,
	acir: acir,
	acirc: acirc,
	acut: acut,
	acute: acute,
	acy: acy,
	aeli: aeli,
	aelig: aelig,
	af: af,
	afr: afr,
	agrav: agrav,
	agrave: agrave,
	alefsym: alefsym,
	aleph: aleph,
	alpha: alpha,
	amacr: amacr,
	amalg: amalg,
	am: am,
	amp: amp,
	and: and,
	andand: andand,
	andd: andd,
	andslope: andslope,
	andv: andv,
	ang: ang,
	ange: ange,
	angle: angle,
	angmsd: angmsd,
	angmsdaa: angmsdaa,
	angmsdab: angmsdab,
	angmsdac: angmsdac,
	angmsdad: angmsdad,
	angmsdae: angmsdae,
	angmsdaf: angmsdaf,
	angmsdag: angmsdag,
	angmsdah: angmsdah,
	angrt: angrt,
	angrtvb: angrtvb,
	angrtvbd: angrtvbd,
	angsph: angsph,
	angst: angst,
	angzarr: angzarr,
	aogon: aogon,
	aopf: aopf,
	ap: ap,
	apE: apE,
	apacir: apacir,
	ape: ape,
	apid: apid,
	apos: apos,
	approx: approx,
	approxeq: approxeq,
	arin: arin,
	aring: aring,
	ascr: ascr,
	ast: ast,
	asymp: asymp,
	asympeq: asympeq,
	atild: atild,
	atilde: atilde,
	aum: aum,
	auml: auml,
	awconint: awconint,
	awint: awint,
	bNot: bNot,
	backcong: backcong,
	backepsilon: backepsilon,
	backprime: backprime,
	backsim: backsim,
	backsimeq: backsimeq,
	barvee: barvee,
	barwed: barwed,
	barwedge: barwedge,
	bbrk: bbrk,
	bbrktbrk: bbrktbrk,
	bcong: bcong,
	bcy: bcy,
	bdquo: bdquo,
	becaus: becaus,
	because: because,
	bemptyv: bemptyv,
	bepsi: bepsi,
	bernou: bernou,
	beta: beta,
	beth: beth,
	between: between,
	bfr: bfr,
	bigcap: bigcap,
	bigcirc: bigcirc,
	bigcup: bigcup,
	bigodot: bigodot,
	bigoplus: bigoplus,
	bigotimes: bigotimes,
	bigsqcup: bigsqcup,
	bigstar: bigstar,
	bigtriangledown: bigtriangledown,
	bigtriangleup: bigtriangleup,
	biguplus: biguplus,
	bigvee: bigvee,
	bigwedge: bigwedge,
	bkarow: bkarow,
	blacklozenge: blacklozenge,
	blacksquare: blacksquare,
	blacktriangle: blacktriangle,
	blacktriangledown: blacktriangledown,
	blacktriangleleft: blacktriangleleft,
	blacktriangleright: blacktriangleright,
	blank: blank,
	blk12: blk12,
	blk14: blk14,
	blk34: blk34,
	block: block,
	bne: bne,
	bnequiv: bnequiv,
	bnot: bnot,
	bopf: bopf,
	bot: bot,
	bottom: bottom,
	bowtie: bowtie,
	boxDL: boxDL,
	boxDR: boxDR,
	boxDl: boxDl,
	boxDr: boxDr,
	boxH: boxH,
	boxHD: boxHD,
	boxHU: boxHU,
	boxHd: boxHd,
	boxHu: boxHu,
	boxUL: boxUL,
	boxUR: boxUR,
	boxUl: boxUl,
	boxUr: boxUr,
	boxV: boxV,
	boxVH: boxVH,
	boxVL: boxVL,
	boxVR: boxVR,
	boxVh: boxVh,
	boxVl: boxVl,
	boxVr: boxVr,
	boxbox: boxbox,
	boxdL: boxdL,
	boxdR: boxdR,
	boxdl: boxdl,
	boxdr: boxdr,
	boxh: boxh,
	boxhD: boxhD,
	boxhU: boxhU,
	boxhd: boxhd,
	boxhu: boxhu,
	boxminus: boxminus,
	boxplus: boxplus,
	boxtimes: boxtimes,
	boxuL: boxuL,
	boxuR: boxuR,
	boxul: boxul,
	boxur: boxur,
	boxv: boxv,
	boxvH: boxvH,
	boxvL: boxvL,
	boxvR: boxvR,
	boxvh: boxvh,
	boxvl: boxvl,
	boxvr: boxvr,
	bprime: bprime,
	breve: breve,
	brvba: brvba,
	brvbar: brvbar,
	bscr: bscr,
	bsemi: bsemi,
	bsim: bsim,
	bsime: bsime,
	bsol: bsol,
	bsolb: bsolb,
	bsolhsub: bsolhsub,
	bull: bull,
	bullet: bullet,
	bump: bump,
	bumpE: bumpE,
	bumpe: bumpe,
	bumpeq: bumpeq,
	cacute: cacute,
	cap: cap,
	capand: capand,
	capbrcup: capbrcup,
	capcap: capcap,
	capcup: capcup,
	capdot: capdot,
	caps: caps,
	caret: caret,
	caron: caron,
	ccaps: ccaps,
	ccaron: ccaron,
	ccedi: ccedi,
	ccedil: ccedil,
	ccirc: ccirc,
	ccups: ccups,
	ccupssm: ccupssm,
	cdot: cdot,
	cedi: cedi,
	cedil: cedil,
	cemptyv: cemptyv,
	cen: cen,
	cent: cent,
	centerdot: centerdot,
	cfr: cfr,
	chcy: chcy,
	check: check$2,
	checkmark: checkmark,
	chi: chi,
	cir: cir,
	cirE: cirE,
	circ: circ,
	circeq: circeq,
	circlearrowleft: circlearrowleft,
	circlearrowright: circlearrowright,
	circledR: circledR,
	circledS: circledS,
	circledast: circledast,
	circledcirc: circledcirc,
	circleddash: circleddash,
	cire: cire,
	cirfnint: cirfnint,
	cirmid: cirmid,
	cirscir: cirscir,
	clubs: clubs,
	clubsuit: clubsuit,
	colon: colon,
	colone: colone,
	coloneq: coloneq,
	comma: comma,
	commat: commat,
	comp: comp,
	compfn: compfn,
	complement: complement,
	complexes: complexes,
	cong: cong,
	congdot: congdot,
	conint: conint,
	copf: copf,
	coprod: coprod,
	cop: cop,
	copy: copy$2,
	copysr: copysr,
	crarr: crarr,
	cross: cross,
	cscr: cscr,
	csub: csub,
	csube: csube,
	csup: csup,
	csupe: csupe,
	ctdot: ctdot,
	cudarrl: cudarrl,
	cudarrr: cudarrr,
	cuepr: cuepr,
	cuesc: cuesc,
	cularr: cularr,
	cularrp: cularrp,
	cup: cup,
	cupbrcap: cupbrcap,
	cupcap: cupcap,
	cupcup: cupcup,
	cupdot: cupdot,
	cupor: cupor,
	cups: cups,
	curarr: curarr,
	curarrm: curarrm,
	curlyeqprec: curlyeqprec,
	curlyeqsucc: curlyeqsucc,
	curlyvee: curlyvee,
	curlywedge: curlywedge,
	curre: curre,
	curren: curren,
	curvearrowleft: curvearrowleft,
	curvearrowright: curvearrowright,
	cuvee: cuvee,
	cuwed: cuwed,
	cwconint: cwconint,
	cwint: cwint,
	cylcty: cylcty,
	dArr: dArr,
	dHar: dHar,
	dagger: dagger,
	daleth: daleth,
	darr: darr,
	dash: dash,
	dashv: dashv,
	dbkarow: dbkarow,
	dblac: dblac,
	dcaron: dcaron,
	dcy: dcy,
	dd: dd,
	ddagger: ddagger,
	ddarr: ddarr,
	ddotseq: ddotseq,
	de: de,
	deg: deg,
	delta: delta,
	demptyv: demptyv,
	dfisht: dfisht,
	dfr: dfr,
	dharl: dharl,
	dharr: dharr,
	diam: diam,
	diamond: diamond,
	diamondsuit: diamondsuit,
	diams: diams,
	die: die,
	digamma: digamma,
	disin: disin,
	div: div,
	divid: divid,
	divide: divide,
	divideontimes: divideontimes,
	divonx: divonx,
	djcy: djcy,
	dlcorn: dlcorn,
	dlcrop: dlcrop,
	dollar: dollar,
	dopf: dopf,
	dot: dot,
	doteq: doteq,
	doteqdot: doteqdot,
	dotminus: dotminus,
	dotplus: dotplus,
	dotsquare: dotsquare,
	doublebarwedge: doublebarwedge,
	downarrow: downarrow,
	downdownarrows: downdownarrows,
	downharpoonleft: downharpoonleft,
	downharpoonright: downharpoonright,
	drbkarow: drbkarow,
	drcorn: drcorn,
	drcrop: drcrop,
	dscr: dscr,
	dscy: dscy,
	dsol: dsol,
	dstrok: dstrok,
	dtdot: dtdot,
	dtri: dtri,
	dtrif: dtrif,
	duarr: duarr,
	duhar: duhar,
	dwangle: dwangle,
	dzcy: dzcy,
	dzigrarr: dzigrarr,
	eDDot: eDDot,
	eDot: eDot,
	eacut: eacut,
	eacute: eacute,
	easter: easter,
	ecaron: ecaron,
	ecir: ecir,
	ecirc: ecirc,
	ecolon: ecolon,
	ecy: ecy,
	edot: edot,
	ee: ee,
	efDot: efDot,
	efr: efr,
	eg: eg,
	egrav: egrav,
	egrave: egrave,
	egs: egs,
	egsdot: egsdot,
	el: el,
	elinters: elinters,
	ell: ell,
	els: els,
	elsdot: elsdot,
	emacr: emacr,
	empty: empty,
	emptyset: emptyset,
	emptyv: emptyv,
	emsp13: emsp13,
	emsp14: emsp14,
	emsp: emsp,
	eng: eng,
	ensp: ensp,
	eogon: eogon,
	eopf: eopf,
	epar: epar,
	eparsl: eparsl,
	eplus: eplus,
	epsi: epsi,
	epsilon: epsilon,
	epsiv: epsiv,
	eqcirc: eqcirc,
	eqcolon: eqcolon,
	eqsim: eqsim,
	eqslantgtr: eqslantgtr,
	eqslantless: eqslantless,
	equals: equals,
	equest: equest,
	equiv: equiv,
	equivDD: equivDD,
	eqvparsl: eqvparsl,
	erDot: erDot,
	erarr: erarr,
	escr: escr,
	esdot: esdot,
	esim: esim,
	eta: eta,
	et: et,
	eth: eth,
	eum: eum,
	euml: euml,
	euro: euro,
	excl: excl,
	exist: exist,
	expectation: expectation,
	exponentiale: exponentiale,
	fallingdotseq: fallingdotseq,
	fcy: fcy,
	female: female,
	ffilig: ffilig,
	fflig: fflig,
	ffllig: ffllig,
	ffr: ffr,
	filig: filig,
	fjlig: fjlig,
	flat: flat,
	fllig: fllig,
	fltns: fltns,
	fnof: fnof,
	fopf: fopf,
	forall: forall,
	fork: fork,
	forkv: forkv,
	fpartint: fpartint,
	frac1: frac1,
	frac12: frac12,
	frac13: frac13,
	frac14: frac14,
	frac15: frac15,
	frac16: frac16,
	frac18: frac18,
	frac23: frac23,
	frac25: frac25,
	frac3: frac3,
	frac34: frac34,
	frac35: frac35,
	frac38: frac38,
	frac45: frac45,
	frac56: frac56,
	frac58: frac58,
	frac78: frac78,
	frasl: frasl,
	frown: frown,
	fscr: fscr,
	gE: gE,
	gEl: gEl,
	gacute: gacute,
	gamma: gamma,
	gammad: gammad,
	gap: gap,
	gbreve: gbreve,
	gcirc: gcirc,
	gcy: gcy,
	gdot: gdot,
	ge: ge,
	gel: gel,
	geq: geq,
	geqq: geqq,
	geqslant: geqslant,
	ges: ges,
	gescc: gescc,
	gesdot: gesdot,
	gesdoto: gesdoto,
	gesdotol: gesdotol,
	gesl: gesl,
	gesles: gesles,
	gfr: gfr,
	gg: gg,
	ggg: ggg,
	gimel: gimel,
	gjcy: gjcy,
	gl: gl,
	glE: glE,
	gla: gla,
	glj: glj,
	gnE: gnE,
	gnap: gnap,
	gnapprox: gnapprox,
	gne: gne,
	gneq: gneq,
	gneqq: gneqq,
	gnsim: gnsim,
	gopf: gopf,
	grave: grave,
	gscr: gscr,
	gsim: gsim,
	gsime: gsime,
	gsiml: gsiml,
	g: g,
	gt: gt,
	gtcc: gtcc,
	gtcir: gtcir,
	gtdot: gtdot,
	gtlPar: gtlPar,
	gtquest: gtquest,
	gtrapprox: gtrapprox,
	gtrarr: gtrarr,
	gtrdot: gtrdot,
	gtreqless: gtreqless,
	gtreqqless: gtreqqless,
	gtrless: gtrless,
	gtrsim: gtrsim,
	gvertneqq: gvertneqq,
	gvnE: gvnE,
	hArr: hArr,
	hairsp: hairsp,
	half: half,
	hamilt: hamilt,
	hardcy: hardcy,
	harr: harr,
	harrcir: harrcir,
	harrw: harrw,
	hbar: hbar,
	hcirc: hcirc,
	hearts: hearts,
	heartsuit: heartsuit,
	hellip: hellip,
	hercon: hercon,
	hfr: hfr,
	hksearow: hksearow,
	hkswarow: hkswarow,
	hoarr: hoarr,
	homtht: homtht,
	hookleftarrow: hookleftarrow,
	hookrightarrow: hookrightarrow,
	hopf: hopf,
	horbar: horbar,
	hscr: hscr,
	hslash: hslash,
	hstrok: hstrok,
	hybull: hybull,
	hyphen: hyphen,
	iacut: iacut,
	iacute: iacute,
	ic: ic,
	icir: icir,
	icirc: icirc,
	icy: icy,
	iecy: iecy,
	iexc: iexc,
	iexcl: iexcl,
	iff: iff,
	ifr: ifr,
	igrav: igrav,
	igrave: igrave,
	ii: ii,
	iiiint: iiiint,
	iiint: iiint,
	iinfin: iinfin,
	iiota: iiota,
	ijlig: ijlig,
	imacr: imacr,
	image: image,
	imagline: imagline,
	imagpart: imagpart,
	imath: imath,
	imof: imof,
	imped: imped,
	incare: incare,
	infin: infin,
	infintie: infintie,
	inodot: inodot,
	int: int,
	intcal: intcal,
	integers: integers,
	intercal: intercal,
	intlarhk: intlarhk,
	intprod: intprod,
	iocy: iocy,
	iogon: iogon,
	iopf: iopf,
	iota: iota,
	iprod: iprod,
	iques: iques,
	iquest: iquest,
	iscr: iscr,
	isin: isin,
	isinE: isinE,
	isindot: isindot,
	isins: isins,
	isinsv: isinsv,
	isinv: isinv,
	it: it,
	itilde: itilde,
	iukcy: iukcy,
	ium: ium,
	iuml: iuml,
	jcirc: jcirc,
	jcy: jcy,
	jfr: jfr,
	jmath: jmath,
	jopf: jopf,
	jscr: jscr,
	jsercy: jsercy,
	jukcy: jukcy,
	kappa: kappa,
	kappav: kappav,
	kcedil: kcedil,
	kcy: kcy,
	kfr: kfr,
	kgreen: kgreen,
	khcy: khcy,
	kjcy: kjcy,
	kopf: kopf,
	kscr: kscr,
	lAarr: lAarr,
	lArr: lArr,
	lAtail: lAtail,
	lBarr: lBarr,
	lE: lE,
	lEg: lEg,
	lHar: lHar,
	lacute: lacute,
	laemptyv: laemptyv,
	lagran: lagran,
	lambda: lambda,
	lang: lang,
	langd: langd,
	langle: langle,
	lap: lap,
	laqu: laqu,
	laquo: laquo,
	larr: larr,
	larrb: larrb,
	larrbfs: larrbfs,
	larrfs: larrfs,
	larrhk: larrhk,
	larrlp: larrlp,
	larrpl: larrpl,
	larrsim: larrsim,
	larrtl: larrtl,
	lat: lat,
	latail: latail,
	late: late,
	lates: lates,
	lbarr: lbarr,
	lbbrk: lbbrk,
	lbrace: lbrace,
	lbrack: lbrack,
	lbrke: lbrke,
	lbrksld: lbrksld,
	lbrkslu: lbrkslu,
	lcaron: lcaron,
	lcedil: lcedil,
	lceil: lceil,
	lcub: lcub,
	lcy: lcy,
	ldca: ldca,
	ldquo: ldquo,
	ldquor: ldquor,
	ldrdhar: ldrdhar,
	ldrushar: ldrushar,
	ldsh: ldsh,
	le: le,
	leftarrow: leftarrow,
	leftarrowtail: leftarrowtail,
	leftharpoondown: leftharpoondown,
	leftharpoonup: leftharpoonup,
	leftleftarrows: leftleftarrows,
	leftrightarrow: leftrightarrow,
	leftrightarrows: leftrightarrows,
	leftrightharpoons: leftrightharpoons,
	leftrightsquigarrow: leftrightsquigarrow,
	leftthreetimes: leftthreetimes,
	leg: leg,
	leq: leq,
	leqq: leqq,
	leqslant: leqslant,
	les: les,
	lescc: lescc,
	lesdot: lesdot,
	lesdoto: lesdoto,
	lesdotor: lesdotor,
	lesg: lesg,
	lesges: lesges,
	lessapprox: lessapprox,
	lessdot: lessdot,
	lesseqgtr: lesseqgtr,
	lesseqqgtr: lesseqqgtr,
	lessgtr: lessgtr,
	lesssim: lesssim,
	lfisht: lfisht,
	lfloor: lfloor,
	lfr: lfr,
	lg: lg,
	lgE: lgE,
	lhard: lhard,
	lharu: lharu,
	lharul: lharul,
	lhblk: lhblk,
	ljcy: ljcy,
	ll: ll,
	llarr: llarr,
	llcorner: llcorner,
	llhard: llhard,
	lltri: lltri,
	lmidot: lmidot,
	lmoust: lmoust,
	lmoustache: lmoustache,
	lnE: lnE,
	lnap: lnap,
	lnapprox: lnapprox,
	lne: lne,
	lneq: lneq,
	lneqq: lneqq,
	lnsim: lnsim,
	loang: loang,
	loarr: loarr,
	lobrk: lobrk,
	longleftarrow: longleftarrow,
	longleftrightarrow: longleftrightarrow,
	longmapsto: longmapsto,
	longrightarrow: longrightarrow,
	looparrowleft: looparrowleft,
	looparrowright: looparrowright,
	lopar: lopar,
	lopf: lopf,
	loplus: loplus,
	lotimes: lotimes,
	lowast: lowast,
	lowbar: lowbar,
	loz: loz,
	lozenge: lozenge,
	lozf: lozf,
	lpar: lpar,
	lparlt: lparlt,
	lrarr: lrarr,
	lrcorner: lrcorner,
	lrhar: lrhar,
	lrhard: lrhard,
	lrm: lrm,
	lrtri: lrtri,
	lsaquo: lsaquo,
	lscr: lscr,
	lsh: lsh,
	lsim: lsim,
	lsime: lsime,
	lsimg: lsimg,
	lsqb: lsqb,
	lsquo: lsquo,
	lsquor: lsquor,
	lstrok: lstrok,
	l: l,
	lt: lt,
	ltcc: ltcc,
	ltcir: ltcir,
	ltdot: ltdot,
	lthree: lthree,
	ltimes: ltimes,
	ltlarr: ltlarr,
	ltquest: ltquest,
	ltrPar: ltrPar,
	ltri: ltri,
	ltrie: ltrie,
	ltrif: ltrif,
	lurdshar: lurdshar,
	luruhar: luruhar,
	lvertneqq: lvertneqq,
	lvnE: lvnE,
	mDDot: mDDot,
	mac: mac,
	macr: macr,
	male: male,
	malt: malt,
	maltese: maltese,
	map: map$3,
	mapsto: mapsto,
	mapstodown: mapstodown,
	mapstoleft: mapstoleft,
	mapstoup: mapstoup,
	marker: marker,
	mcomma: mcomma,
	mcy: mcy,
	mdash: mdash,
	measuredangle: measuredangle,
	mfr: mfr,
	mho: mho,
	micr: micr,
	micro: micro,
	mid: mid,
	midast: midast,
	midcir: midcir,
	middo: middo,
	middot: middot,
	minus: minus,
	minusb: minusb,
	minusd: minusd,
	minusdu: minusdu,
	mlcp: mlcp,
	mldr: mldr,
	mnplus: mnplus,
	models: models,
	mopf: mopf,
	mp: mp,
	mscr: mscr,
	mstpos: mstpos,
	mu: mu,
	multimap: multimap,
	mumap: mumap,
	nGg: nGg,
	nGt: nGt,
	nGtv: nGtv,
	nLeftarrow: nLeftarrow,
	nLeftrightarrow: nLeftrightarrow,
	nLl: nLl,
	nLt: nLt,
	nLtv: nLtv,
	nRightarrow: nRightarrow,
	nVDash: nVDash,
	nVdash: nVdash,
	nabla: nabla,
	nacute: nacute,
	nang: nang,
	nap: nap,
	napE: napE,
	napid: napid,
	napos: napos,
	napprox: napprox,
	natur: natur,
	natural: natural,
	naturals: naturals,
	nbs: nbs,
	nbsp: nbsp,
	nbump: nbump,
	nbumpe: nbumpe,
	ncap: ncap,
	ncaron: ncaron,
	ncedil: ncedil,
	ncong: ncong,
	ncongdot: ncongdot,
	ncup: ncup,
	ncy: ncy,
	ndash: ndash,
	ne: ne,
	neArr: neArr,
	nearhk: nearhk,
	nearr: nearr,
	nearrow: nearrow,
	nedot: nedot,
	nequiv: nequiv,
	nesear: nesear,
	nesim: nesim,
	nexist: nexist,
	nexists: nexists,
	nfr: nfr,
	ngE: ngE,
	nge: nge,
	ngeq: ngeq,
	ngeqq: ngeqq,
	ngeqslant: ngeqslant,
	nges: nges,
	ngsim: ngsim,
	ngt: ngt,
	ngtr: ngtr,
	nhArr: nhArr,
	nharr: nharr,
	nhpar: nhpar,
	ni: ni,
	nis: nis,
	nisd: nisd,
	niv: niv,
	njcy: njcy,
	nlArr: nlArr,
	nlE: nlE,
	nlarr: nlarr,
	nldr: nldr,
	nle: nle,
	nleftarrow: nleftarrow,
	nleftrightarrow: nleftrightarrow,
	nleq: nleq,
	nleqq: nleqq,
	nleqslant: nleqslant,
	nles: nles,
	nless: nless,
	nlsim: nlsim,
	nlt: nlt,
	nltri: nltri,
	nltrie: nltrie,
	nmid: nmid,
	nopf: nopf,
	no: no,
	not: not,
	notin: notin,
	notinE: notinE,
	notindot: notindot,
	notinva: notinva,
	notinvb: notinvb,
	notinvc: notinvc,
	notni: notni,
	notniva: notniva,
	notnivb: notnivb,
	notnivc: notnivc,
	npar: npar,
	nparallel: nparallel,
	nparsl: nparsl,
	npart: npart,
	npolint: npolint,
	npr: npr,
	nprcue: nprcue,
	npre: npre,
	nprec: nprec,
	npreceq: npreceq,
	nrArr: nrArr,
	nrarr: nrarr,
	nrarrc: nrarrc,
	nrarrw: nrarrw,
	nrightarrow: nrightarrow,
	nrtri: nrtri,
	nrtrie: nrtrie,
	nsc: nsc,
	nsccue: nsccue,
	nsce: nsce,
	nscr: nscr,
	nshortmid: nshortmid,
	nshortparallel: nshortparallel,
	nsim: nsim,
	nsime: nsime,
	nsimeq: nsimeq,
	nsmid: nsmid,
	nspar: nspar,
	nsqsube: nsqsube,
	nsqsupe: nsqsupe,
	nsub: nsub,
	nsubE: nsubE,
	nsube: nsube,
	nsubset: nsubset,
	nsubseteq: nsubseteq,
	nsubseteqq: nsubseteqq,
	nsucc: nsucc,
	nsucceq: nsucceq,
	nsup: nsup,
	nsupE: nsupE,
	nsupe: nsupe,
	nsupset: nsupset,
	nsupseteq: nsupseteq,
	nsupseteqq: nsupseteqq,
	ntgl: ntgl,
	ntild: ntild,
	ntilde: ntilde,
	ntlg: ntlg,
	ntriangleleft: ntriangleleft,
	ntrianglelefteq: ntrianglelefteq,
	ntriangleright: ntriangleright,
	ntrianglerighteq: ntrianglerighteq,
	nu: nu,
	num: num,
	numero: numero,
	numsp: numsp,
	nvDash: nvDash,
	nvHarr: nvHarr,
	nvap: nvap,
	nvdash: nvdash,
	nvge: nvge,
	nvgt: nvgt,
	nvinfin: nvinfin,
	nvlArr: nvlArr,
	nvle: nvle,
	nvlt: nvlt,
	nvltrie: nvltrie,
	nvrArr: nvrArr,
	nvrtrie: nvrtrie,
	nvsim: nvsim,
	nwArr: nwArr,
	nwarhk: nwarhk,
	nwarr: nwarr,
	nwarrow: nwarrow,
	nwnear: nwnear,
	oS: oS,
	oacut: oacut,
	oacute: oacute,
	oast: oast,
	ocir: ocir,
	ocirc: ocirc,
	ocy: ocy,
	odash: odash,
	odblac: odblac,
	odiv: odiv,
	odot: odot,
	odsold: odsold,
	oelig: oelig,
	ofcir: ofcir,
	ofr: ofr,
	ogon: ogon,
	ograv: ograv,
	ograve: ograve,
	ogt: ogt,
	ohbar: ohbar,
	ohm: ohm,
	oint: oint,
	olarr: olarr,
	olcir: olcir,
	olcross: olcross,
	oline: oline,
	olt: olt,
	omacr: omacr,
	omega: omega,
	omicron: omicron,
	omid: omid,
	ominus: ominus,
	oopf: oopf,
	opar: opar,
	operp: operp,
	oplus: oplus,
	or: or,
	orarr: orarr,
	ord: ord,
	order: order$1,
	orderof: orderof,
	ordf: ordf,
	ordm: ordm,
	origof: origof,
	oror: oror,
	orslope: orslope,
	orv: orv,
	oscr: oscr,
	oslas: oslas,
	oslash: oslash,
	osol: osol,
	otild: otild,
	otilde: otilde,
	otimes: otimes,
	otimesas: otimesas,
	oum: oum,
	ouml: ouml,
	ovbar: ovbar,
	par: par,
	para: para,
	parallel: parallel,
	parsim: parsim,
	parsl: parsl,
	part: part,
	pcy: pcy,
	percnt: percnt,
	period: period,
	permil: permil,
	perp: perp,
	pertenk: pertenk,
	pfr: pfr,
	phi: phi,
	phiv: phiv,
	phmmat: phmmat,
	phone: phone,
	pi: pi,
	pitchfork: pitchfork,
	piv: piv,
	planck: planck,
	planckh: planckh,
	plankv: plankv,
	plus: plus,
	plusacir: plusacir,
	plusb: plusb,
	pluscir: pluscir,
	plusdo: plusdo,
	plusdu: plusdu,
	pluse: pluse,
	plusm: plusm,
	plusmn: plusmn,
	plussim: plussim,
	plustwo: plustwo,
	pm: pm,
	pointint: pointint,
	popf: popf,
	poun: poun,
	pound: pound,
	pr: pr,
	prE: prE,
	prap: prap,
	prcue: prcue,
	pre: pre,
	prec: prec,
	precapprox: precapprox,
	preccurlyeq: preccurlyeq,
	preceq: preceq,
	precnapprox: precnapprox,
	precneqq: precneqq,
	precnsim: precnsim,
	precsim: precsim,
	prime: prime,
	primes: primes,
	prnE: prnE,
	prnap: prnap,
	prnsim: prnsim,
	prod: prod,
	profalar: profalar,
	profline: profline,
	profsurf: profsurf,
	prop: prop,
	propto: propto,
	prsim: prsim,
	prurel: prurel,
	pscr: pscr,
	psi: psi,
	puncsp: puncsp,
	qfr: qfr,
	qint: qint,
	qopf: qopf,
	qprime: qprime,
	qscr: qscr,
	quaternions: quaternions,
	quatint: quatint,
	quest: quest,
	questeq: questeq,
	quo: quo,
	quot: quot,
	rAarr: rAarr,
	rArr: rArr,
	rAtail: rAtail,
	rBarr: rBarr,
	rHar: rHar,
	race: race,
	racute: racute,
	radic: radic,
	raemptyv: raemptyv,
	rang: rang,
	rangd: rangd,
	range: range$1,
	rangle: rangle,
	raqu: raqu,
	raquo: raquo,
	rarr: rarr,
	rarrap: rarrap,
	rarrb: rarrb,
	rarrbfs: rarrbfs,
	rarrc: rarrc,
	rarrfs: rarrfs,
	rarrhk: rarrhk,
	rarrlp: rarrlp,
	rarrpl: rarrpl,
	rarrsim: rarrsim,
	rarrtl: rarrtl,
	rarrw: rarrw,
	ratail: ratail,
	ratio: ratio,
	rationals: rationals,
	rbarr: rbarr,
	rbbrk: rbbrk,
	rbrace: rbrace,
	rbrack: rbrack,
	rbrke: rbrke,
	rbrksld: rbrksld,
	rbrkslu: rbrkslu,
	rcaron: rcaron,
	rcedil: rcedil,
	rceil: rceil,
	rcub: rcub,
	rcy: rcy,
	rdca: rdca,
	rdldhar: rdldhar,
	rdquo: rdquo,
	rdquor: rdquor,
	rdsh: rdsh,
	real: real,
	realine: realine,
	realpart: realpart,
	reals: reals,
	rect: rect,
	re: re,
	reg: reg,
	rfisht: rfisht,
	rfloor: rfloor,
	rfr: rfr,
	rhard: rhard,
	rharu: rharu,
	rharul: rharul,
	rho: rho,
	rhov: rhov,
	rightarrow: rightarrow,
	rightarrowtail: rightarrowtail,
	rightharpoondown: rightharpoondown,
	rightharpoonup: rightharpoonup,
	rightleftarrows: rightleftarrows,
	rightleftharpoons: rightleftharpoons,
	rightrightarrows: rightrightarrows,
	rightsquigarrow: rightsquigarrow,
	rightthreetimes: rightthreetimes,
	ring: ring,
	risingdotseq: risingdotseq,
	rlarr: rlarr,
	rlhar: rlhar,
	rlm: rlm,
	rmoust: rmoust,
	rmoustache: rmoustache,
	rnmid: rnmid,
	roang: roang,
	roarr: roarr,
	robrk: robrk,
	ropar: ropar,
	ropf: ropf,
	roplus: roplus,
	rotimes: rotimes,
	rpar: rpar,
	rpargt: rpargt,
	rppolint: rppolint,
	rrarr: rrarr,
	rsaquo: rsaquo,
	rscr: rscr,
	rsh: rsh,
	rsqb: rsqb,
	rsquo: rsquo,
	rsquor: rsquor,
	rthree: rthree,
	rtimes: rtimes,
	rtri: rtri,
	rtrie: rtrie,
	rtrif: rtrif,
	rtriltri: rtriltri,
	ruluhar: ruluhar,
	rx: rx,
	sacute: sacute,
	sbquo: sbquo,
	sc: sc,
	scE: scE,
	scap: scap,
	scaron: scaron,
	sccue: sccue,
	sce: sce,
	scedil: scedil,
	scirc: scirc,
	scnE: scnE,
	scnap: scnap,
	scnsim: scnsim,
	scpolint: scpolint,
	scsim: scsim,
	scy: scy,
	sdot: sdot,
	sdotb: sdotb,
	sdote: sdote,
	seArr: seArr,
	searhk: searhk,
	searr: searr,
	searrow: searrow,
	sec: sec,
	sect: sect,
	semi: semi,
	seswar: seswar,
	setminus: setminus,
	setmn: setmn,
	sext: sext,
	sfr: sfr,
	sfrown: sfrown,
	sharp: sharp,
	shchcy: shchcy,
	shcy: shcy,
	shortmid: shortmid,
	shortparallel: shortparallel,
	sh: sh,
	shy: shy,
	sigma: sigma,
	sigmaf: sigmaf,
	sigmav: sigmav,
	sim: sim,
	simdot: simdot,
	sime: sime,
	simeq: simeq,
	simg: simg,
	simgE: simgE,
	siml: siml,
	simlE: simlE,
	simne: simne,
	simplus: simplus,
	simrarr: simrarr,
	slarr: slarr,
	smallsetminus: smallsetminus,
	smashp: smashp,
	smeparsl: smeparsl,
	smid: smid,
	smile: smile,
	smt: smt,
	smte: smte,
	smtes: smtes,
	softcy: softcy,
	sol: sol,
	solb: solb,
	solbar: solbar,
	sopf: sopf,
	spades: spades,
	spadesuit: spadesuit,
	spar: spar,
	sqcap: sqcap,
	sqcaps: sqcaps,
	sqcup: sqcup,
	sqcups: sqcups,
	sqsub: sqsub,
	sqsube: sqsube,
	sqsubset: sqsubset,
	sqsubseteq: sqsubseteq,
	sqsup: sqsup,
	sqsupe: sqsupe,
	sqsupset: sqsupset,
	sqsupseteq: sqsupseteq,
	squ: squ,
	square: square,
	squarf: squarf,
	squf: squf,
	srarr: srarr,
	sscr: sscr,
	ssetmn: ssetmn,
	ssmile: ssmile,
	sstarf: sstarf,
	star: star$1,
	starf: starf,
	straightepsilon: straightepsilon,
	straightphi: straightphi,
	strns: strns,
	sub: sub,
	subE: subE,
	subdot: subdot,
	sube: sube,
	subedot: subedot,
	submult: submult,
	subnE: subnE,
	subne: subne,
	subplus: subplus,
	subrarr: subrarr,
	subset: subset,
	subseteq: subseteq,
	subseteqq: subseteqq,
	subsetneq: subsetneq,
	subsetneqq: subsetneqq,
	subsim: subsim,
	subsub: subsub,
	subsup: subsup,
	succ: succ,
	succapprox: succapprox,
	succcurlyeq: succcurlyeq,
	succeq: succeq,
	succnapprox: succnapprox,
	succneqq: succneqq,
	succnsim: succnsim,
	succsim: succsim,
	sum: sum,
	sung: sung,
	sup: sup,
	sup1: sup1,
	sup2: sup2,
	sup3: sup3,
	supE: supE,
	supdot: supdot,
	supdsub: supdsub,
	supe: supe,
	supedot: supedot,
	suphsol: suphsol,
	suphsub: suphsub,
	suplarr: suplarr,
	supmult: supmult,
	supnE: supnE,
	supne: supne,
	supplus: supplus,
	supset: supset,
	supseteq: supseteq,
	supseteqq: supseteqq,
	supsetneq: supsetneq,
	supsetneqq: supsetneqq,
	supsim: supsim,
	supsub: supsub,
	supsup: supsup,
	swArr: swArr,
	swarhk: swarhk,
	swarr: swarr,
	swarrow: swarrow,
	swnwar: swnwar,
	szli: szli,
	szlig: szlig,
	target: target,
	tau: tau,
	tbrk: tbrk,
	tcaron: tcaron,
	tcedil: tcedil,
	tcy: tcy,
	tdot: tdot,
	telrec: telrec,
	tfr: tfr,
	there4: there4,
	therefore: therefore,
	theta: theta,
	thetasym: thetasym,
	thetav: thetav,
	thickapprox: thickapprox,
	thicksim: thicksim,
	thinsp: thinsp,
	thkap: thkap,
	thksim: thksim,
	thor: thor,
	thorn: thorn,
	tilde: tilde,
	time: time,
	times: times,
	timesb: timesb,
	timesbar: timesbar,
	timesd: timesd,
	tint: tint,
	toea: toea,
	top: top,
	topbot: topbot,
	topcir: topcir,
	topf: topf,
	topfork: topfork,
	tosa: tosa,
	tprime: tprime,
	trade: trade,
	triangle: triangle,
	triangledown: triangledown,
	triangleleft: triangleleft,
	trianglelefteq: trianglelefteq,
	triangleq: triangleq,
	triangleright: triangleright,
	trianglerighteq: trianglerighteq,
	tridot: tridot,
	trie: trie,
	triminus: triminus,
	triplus: triplus,
	trisb: trisb,
	tritime: tritime,
	trpezium: trpezium,
	tscr: tscr,
	tscy: tscy,
	tshcy: tshcy,
	tstrok: tstrok,
	twixt: twixt,
	twoheadleftarrow: twoheadleftarrow,
	twoheadrightarrow: twoheadrightarrow,
	uArr: uArr,
	uHar: uHar,
	uacut: uacut,
	uacute: uacute,
	uarr: uarr,
	ubrcy: ubrcy,
	ubreve: ubreve,
	ucir: ucir,
	ucirc: ucirc,
	ucy: ucy,
	udarr: udarr,
	udblac: udblac,
	udhar: udhar,
	ufisht: ufisht,
	ufr: ufr,
	ugrav: ugrav,
	ugrave: ugrave,
	uharl: uharl,
	uharr: uharr,
	uhblk: uhblk,
	ulcorn: ulcorn,
	ulcorner: ulcorner,
	ulcrop: ulcrop,
	ultri: ultri,
	umacr: umacr,
	um: um,
	uml: uml,
	uogon: uogon,
	uopf: uopf,
	uparrow: uparrow,
	updownarrow: updownarrow,
	upharpoonleft: upharpoonleft,
	upharpoonright: upharpoonright,
	uplus: uplus,
	upsi: upsi,
	upsih: upsih,
	upsilon: upsilon,
	upuparrows: upuparrows,
	urcorn: urcorn,
	urcorner: urcorner,
	urcrop: urcrop,
	uring: uring,
	urtri: urtri,
	uscr: uscr,
	utdot: utdot,
	utilde: utilde,
	utri: utri,
	utrif: utrif,
	uuarr: uuarr,
	uum: uum,
	uuml: uuml,
	uwangle: uwangle,
	vArr: vArr,
	vBar: vBar,
	vBarv: vBarv,
	vDash: vDash,
	vangrt: vangrt,
	varepsilon: varepsilon,
	varkappa: varkappa,
	varnothing: varnothing,
	varphi: varphi,
	varpi: varpi,
	varpropto: varpropto,
	varr: varr,
	varrho: varrho,
	varsigma: varsigma,
	varsubsetneq: varsubsetneq,
	varsubsetneqq: varsubsetneqq,
	varsupsetneq: varsupsetneq,
	varsupsetneqq: varsupsetneqq,
	vartheta: vartheta,
	vartriangleleft: vartriangleleft,
	vartriangleright: vartriangleright,
	vcy: vcy,
	vdash: vdash,
	vee: vee,
	veebar: veebar,
	veeeq: veeeq,
	vellip: vellip,
	verbar: verbar,
	vert: vert,
	vfr: vfr,
	vltri: vltri,
	vnsub: vnsub,
	vnsup: vnsup,
	vopf: vopf,
	vprop: vprop,
	vrtri: vrtri,
	vscr: vscr,
	vsubnE: vsubnE,
	vsubne: vsubne,
	vsupnE: vsupnE,
	vsupne: vsupne,
	vzigzag: vzigzag,
	wcirc: wcirc,
	wedbar: wedbar,
	wedge: wedge,
	wedgeq: wedgeq,
	weierp: weierp,
	wfr: wfr,
	wopf: wopf,
	wp: wp,
	wr: wr,
	wreath: wreath,
	wscr: wscr,
	xcap: xcap,
	xcirc: xcirc,
	xcup: xcup,
	xdtri: xdtri,
	xfr: xfr,
	xhArr: xhArr,
	xharr: xharr,
	xi: xi,
	xlArr: xlArr,
	xlarr: xlarr,
	xmap: xmap,
	xnis: xnis,
	xodot: xodot,
	xopf: xopf,
	xoplus: xoplus,
	xotime: xotime,
	xrArr: xrArr,
	xrarr: xrarr,
	xscr: xscr,
	xsqcup: xsqcup,
	xuplus: xuplus,
	xutri: xutri,
	xvee: xvee,
	xwedge: xwedge,
	yacut: yacut,
	yacute: yacute,
	yacy: yacy,
	ycirc: ycirc,
	ycy: ycy,
	ye: ye,
	yen: yen,
	yfr: yfr,
	yicy: yicy,
	yopf: yopf,
	yscr: yscr,
	yucy: yucy,
	yum: yum,
	yuml: yuml,
	zacute: zacute,
	zcaron: zcaron,
	zcy: zcy,
	zdot: zdot,
	zeetrf: zeetrf,
	zeta: zeta,
	zfr: zfr,
	zhcy: zhcy,
	zigrarr: zigrarr,
	zopf: zopf,
	zscr: zscr,
	zwj: zwj,
	zwnj: zwnj,
	"Map": "⤅",
	"in": "∈"
};

var characterEntities = Object.freeze({
	AEli: AEli,
	AElig: AElig,
	AM: AM,
	AMP: AMP,
	Aacut: Aacut,
	Aacute: Aacute,
	Abreve: Abreve,
	Acir: Acir,
	Acirc: Acirc,
	Acy: Acy,
	Afr: Afr,
	Agrav: Agrav,
	Agrave: Agrave,
	Alpha: Alpha,
	Amacr: Amacr,
	And: And,
	Aogon: Aogon,
	Aopf: Aopf,
	ApplyFunction: ApplyFunction,
	Arin: Arin,
	Aring: Aring,
	Ascr: Ascr,
	Assign: Assign,
	Atild: Atild,
	Atilde: Atilde,
	Aum: Aum,
	Auml: Auml,
	Backslash: Backslash,
	Barv: Barv,
	Barwed: Barwed,
	Bcy: Bcy,
	Because: Because,
	Bernoullis: Bernoullis,
	Beta: Beta,
	Bfr: Bfr,
	Bopf: Bopf,
	Breve: Breve,
	Bscr: Bscr,
	Bumpeq: Bumpeq,
	CHcy: CHcy,
	COP: COP,
	COPY: COPY,
	Cacute: Cacute,
	Cap: Cap,
	CapitalDifferentialD: CapitalDifferentialD,
	Cayleys: Cayleys,
	Ccaron: Ccaron,
	Ccedi: Ccedi,
	Ccedil: Ccedil,
	Ccirc: Ccirc,
	Cconint: Cconint,
	Cdot: Cdot,
	Cedilla: Cedilla,
	CenterDot: CenterDot,
	Cfr: Cfr,
	Chi: Chi,
	CircleDot: CircleDot,
	CircleMinus: CircleMinus,
	CirclePlus: CirclePlus,
	CircleTimes: CircleTimes,
	ClockwiseContourIntegral: ClockwiseContourIntegral,
	CloseCurlyDoubleQuote: CloseCurlyDoubleQuote,
	CloseCurlyQuote: CloseCurlyQuote,
	Colon: Colon,
	Colone: Colone,
	Congruent: Congruent,
	Conint: Conint,
	ContourIntegral: ContourIntegral,
	Copf: Copf,
	Coproduct: Coproduct,
	CounterClockwiseContourIntegral: CounterClockwiseContourIntegral,
	Cross: Cross,
	Cscr: Cscr,
	Cup: Cup,
	CupCap: CupCap,
	DD: DD,
	DDotrahd: DDotrahd,
	DJcy: DJcy,
	DScy: DScy,
	DZcy: DZcy,
	Dagger: Dagger,
	Darr: Darr,
	Dashv: Dashv,
	Dcaron: Dcaron,
	Dcy: Dcy,
	Del: Del,
	Delta: Delta,
	Dfr: Dfr,
	DiacriticalAcute: DiacriticalAcute,
	DiacriticalDot: DiacriticalDot,
	DiacriticalDoubleAcute: DiacriticalDoubleAcute,
	DiacriticalGrave: DiacriticalGrave,
	DiacriticalTilde: DiacriticalTilde,
	Diamond: Diamond,
	DifferentialD: DifferentialD,
	Dopf: Dopf,
	Dot: Dot,
	DotDot: DotDot,
	DotEqual: DotEqual,
	DoubleContourIntegral: DoubleContourIntegral,
	DoubleDot: DoubleDot,
	DoubleDownArrow: DoubleDownArrow,
	DoubleLeftArrow: DoubleLeftArrow,
	DoubleLeftRightArrow: DoubleLeftRightArrow,
	DoubleLeftTee: DoubleLeftTee,
	DoubleLongLeftArrow: DoubleLongLeftArrow,
	DoubleLongLeftRightArrow: DoubleLongLeftRightArrow,
	DoubleLongRightArrow: DoubleLongRightArrow,
	DoubleRightArrow: DoubleRightArrow,
	DoubleRightTee: DoubleRightTee,
	DoubleUpArrow: DoubleUpArrow,
	DoubleUpDownArrow: DoubleUpDownArrow,
	DoubleVerticalBar: DoubleVerticalBar,
	DownArrow: DownArrow,
	DownArrowBar: DownArrowBar,
	DownArrowUpArrow: DownArrowUpArrow,
	DownBreve: DownBreve,
	DownLeftRightVector: DownLeftRightVector,
	DownLeftTeeVector: DownLeftTeeVector,
	DownLeftVector: DownLeftVector,
	DownLeftVectorBar: DownLeftVectorBar,
	DownRightTeeVector: DownRightTeeVector,
	DownRightVector: DownRightVector,
	DownRightVectorBar: DownRightVectorBar,
	DownTee: DownTee,
	DownTeeArrow: DownTeeArrow,
	Downarrow: Downarrow,
	Dscr: Dscr,
	Dstrok: Dstrok,
	ENG: ENG,
	ET: ET,
	ETH: ETH,
	Eacut: Eacut,
	Eacute: Eacute,
	Ecaron: Ecaron,
	Ecir: Ecir,
	Ecirc: Ecirc,
	Ecy: Ecy,
	Edot: Edot,
	Efr: Efr,
	Egrav: Egrav,
	Egrave: Egrave,
	Element: Element,
	Emacr: Emacr,
	EmptySmallSquare: EmptySmallSquare,
	EmptyVerySmallSquare: EmptyVerySmallSquare,
	Eogon: Eogon,
	Eopf: Eopf,
	Epsilon: Epsilon,
	Equal: Equal,
	EqualTilde: EqualTilde,
	Equilibrium: Equilibrium,
	Escr: Escr,
	Esim: Esim,
	Eta: Eta,
	Eum: Eum,
	Euml: Euml,
	Exists: Exists,
	ExponentialE: ExponentialE,
	Fcy: Fcy,
	Ffr: Ffr,
	FilledSmallSquare: FilledSmallSquare,
	FilledVerySmallSquare: FilledVerySmallSquare,
	Fopf: Fopf,
	ForAll: ForAll,
	Fouriertrf: Fouriertrf,
	Fscr: Fscr,
	GJcy: GJcy,
	G: G,
	GT: GT,
	Gamma: Gamma,
	Gammad: Gammad,
	Gbreve: Gbreve,
	Gcedil: Gcedil,
	Gcirc: Gcirc,
	Gcy: Gcy,
	Gdot: Gdot,
	Gfr: Gfr,
	Gg: Gg,
	Gopf: Gopf,
	GreaterEqual: GreaterEqual,
	GreaterEqualLess: GreaterEqualLess,
	GreaterFullEqual: GreaterFullEqual,
	GreaterGreater: GreaterGreater,
	GreaterLess: GreaterLess,
	GreaterSlantEqual: GreaterSlantEqual,
	GreaterTilde: GreaterTilde,
	Gscr: Gscr,
	Gt: Gt,
	HARDcy: HARDcy,
	Hacek: Hacek,
	Hat: Hat,
	Hcirc: Hcirc,
	Hfr: Hfr,
	HilbertSpace: HilbertSpace,
	Hopf: Hopf,
	HorizontalLine: HorizontalLine,
	Hscr: Hscr,
	Hstrok: Hstrok,
	HumpDownHump: HumpDownHump,
	HumpEqual: HumpEqual,
	IEcy: IEcy,
	IJlig: IJlig,
	IOcy: IOcy,
	Iacut: Iacut,
	Iacute: Iacute,
	Icir: Icir,
	Icirc: Icirc,
	Icy: Icy,
	Idot: Idot,
	Ifr: Ifr,
	Igrav: Igrav,
	Igrave: Igrave,
	Im: Im,
	Imacr: Imacr,
	ImaginaryI: ImaginaryI,
	Implies: Implies,
	Int: Int,
	Integral: Integral,
	Intersection: Intersection,
	InvisibleComma: InvisibleComma,
	InvisibleTimes: InvisibleTimes,
	Iogon: Iogon,
	Iopf: Iopf,
	Iota: Iota,
	Iscr: Iscr,
	Itilde: Itilde,
	Iukcy: Iukcy,
	Ium: Ium,
	Iuml: Iuml,
	Jcirc: Jcirc,
	Jcy: Jcy,
	Jfr: Jfr,
	Jopf: Jopf,
	Jscr: Jscr,
	Jsercy: Jsercy,
	Jukcy: Jukcy,
	KHcy: KHcy,
	KJcy: KJcy,
	Kappa: Kappa,
	Kcedil: Kcedil,
	Kcy: Kcy,
	Kfr: Kfr,
	Kopf: Kopf,
	Kscr: Kscr,
	LJcy: LJcy,
	L: L,
	LT: LT,
	Lacute: Lacute,
	Lambda: Lambda,
	Lang: Lang,
	Laplacetrf: Laplacetrf,
	Larr: Larr,
	Lcaron: Lcaron,
	Lcedil: Lcedil,
	Lcy: Lcy,
	LeftAngleBracket: LeftAngleBracket,
	LeftArrow: LeftArrow,
	LeftArrowBar: LeftArrowBar,
	LeftArrowRightArrow: LeftArrowRightArrow,
	LeftCeiling: LeftCeiling,
	LeftDoubleBracket: LeftDoubleBracket,
	LeftDownTeeVector: LeftDownTeeVector,
	LeftDownVector: LeftDownVector,
	LeftDownVectorBar: LeftDownVectorBar,
	LeftFloor: LeftFloor,
	LeftRightArrow: LeftRightArrow,
	LeftRightVector: LeftRightVector,
	LeftTee: LeftTee,
	LeftTeeArrow: LeftTeeArrow,
	LeftTeeVector: LeftTeeVector,
	LeftTriangle: LeftTriangle,
	LeftTriangleBar: LeftTriangleBar,
	LeftTriangleEqual: LeftTriangleEqual,
	LeftUpDownVector: LeftUpDownVector,
	LeftUpTeeVector: LeftUpTeeVector,
	LeftUpVector: LeftUpVector,
	LeftUpVectorBar: LeftUpVectorBar,
	LeftVector: LeftVector,
	LeftVectorBar: LeftVectorBar,
	Leftarrow: Leftarrow,
	Leftrightarrow: Leftrightarrow,
	LessEqualGreater: LessEqualGreater,
	LessFullEqual: LessFullEqual,
	LessGreater: LessGreater,
	LessLess: LessLess,
	LessSlantEqual: LessSlantEqual,
	LessTilde: LessTilde,
	Lfr: Lfr,
	Ll: Ll,
	Lleftarrow: Lleftarrow,
	Lmidot: Lmidot,
	LongLeftArrow: LongLeftArrow,
	LongLeftRightArrow: LongLeftRightArrow,
	LongRightArrow: LongRightArrow,
	Longleftarrow: Longleftarrow,
	Longleftrightarrow: Longleftrightarrow,
	Longrightarrow: Longrightarrow,
	Lopf: Lopf,
	LowerLeftArrow: LowerLeftArrow,
	LowerRightArrow: LowerRightArrow,
	Lscr: Lscr,
	Lsh: Lsh,
	Lstrok: Lstrok,
	Lt: Lt,
	Mcy: Mcy,
	MediumSpace: MediumSpace,
	Mellintrf: Mellintrf,
	Mfr: Mfr,
	MinusPlus: MinusPlus,
	Mopf: Mopf,
	Mscr: Mscr,
	Mu: Mu,
	NJcy: NJcy,
	Nacute: Nacute,
	Ncaron: Ncaron,
	Ncedil: Ncedil,
	Ncy: Ncy,
	NegativeMediumSpace: NegativeMediumSpace,
	NegativeThickSpace: NegativeThickSpace,
	NegativeThinSpace: NegativeThinSpace,
	NegativeVeryThinSpace: NegativeVeryThinSpace,
	NestedGreaterGreater: NestedGreaterGreater,
	NestedLessLess: NestedLessLess,
	NewLine: NewLine,
	Nfr: Nfr,
	NoBreak: NoBreak,
	NonBreakingSpace: NonBreakingSpace,
	Nopf: Nopf,
	Not: Not,
	NotCongruent: NotCongruent,
	NotCupCap: NotCupCap,
	NotDoubleVerticalBar: NotDoubleVerticalBar,
	NotElement: NotElement,
	NotEqual: NotEqual,
	NotEqualTilde: NotEqualTilde,
	NotExists: NotExists,
	NotGreater: NotGreater,
	NotGreaterEqual: NotGreaterEqual,
	NotGreaterFullEqual: NotGreaterFullEqual,
	NotGreaterGreater: NotGreaterGreater,
	NotGreaterLess: NotGreaterLess,
	NotGreaterSlantEqual: NotGreaterSlantEqual,
	NotGreaterTilde: NotGreaterTilde,
	NotHumpDownHump: NotHumpDownHump,
	NotHumpEqual: NotHumpEqual,
	NotLeftTriangle: NotLeftTriangle,
	NotLeftTriangleBar: NotLeftTriangleBar,
	NotLeftTriangleEqual: NotLeftTriangleEqual,
	NotLess: NotLess,
	NotLessEqual: NotLessEqual,
	NotLessGreater: NotLessGreater,
	NotLessLess: NotLessLess,
	NotLessSlantEqual: NotLessSlantEqual,
	NotLessTilde: NotLessTilde,
	NotNestedGreaterGreater: NotNestedGreaterGreater,
	NotNestedLessLess: NotNestedLessLess,
	NotPrecedes: NotPrecedes,
	NotPrecedesEqual: NotPrecedesEqual,
	NotPrecedesSlantEqual: NotPrecedesSlantEqual,
	NotReverseElement: NotReverseElement,
	NotRightTriangle: NotRightTriangle,
	NotRightTriangleBar: NotRightTriangleBar,
	NotRightTriangleEqual: NotRightTriangleEqual,
	NotSquareSubset: NotSquareSubset,
	NotSquareSubsetEqual: NotSquareSubsetEqual,
	NotSquareSuperset: NotSquareSuperset,
	NotSquareSupersetEqual: NotSquareSupersetEqual,
	NotSubset: NotSubset,
	NotSubsetEqual: NotSubsetEqual,
	NotSucceeds: NotSucceeds,
	NotSucceedsEqual: NotSucceedsEqual,
	NotSucceedsSlantEqual: NotSucceedsSlantEqual,
	NotSucceedsTilde: NotSucceedsTilde,
	NotSuperset: NotSuperset,
	NotSupersetEqual: NotSupersetEqual,
	NotTilde: NotTilde,
	NotTildeEqual: NotTildeEqual,
	NotTildeFullEqual: NotTildeFullEqual,
	NotTildeTilde: NotTildeTilde,
	NotVerticalBar: NotVerticalBar,
	Nscr: Nscr,
	Ntild: Ntild,
	Ntilde: Ntilde,
	Nu: Nu,
	OElig: OElig,
	Oacut: Oacut,
	Oacute: Oacute,
	Ocir: Ocir,
	Ocirc: Ocirc,
	Ocy: Ocy,
	Odblac: Odblac,
	Ofr: Ofr,
	Ograv: Ograv,
	Ograve: Ograve,
	Omacr: Omacr,
	Omega: Omega,
	Omicron: Omicron,
	Oopf: Oopf,
	OpenCurlyDoubleQuote: OpenCurlyDoubleQuote,
	OpenCurlyQuote: OpenCurlyQuote,
	Or: Or,
	Oscr: Oscr,
	Oslas: Oslas,
	Oslash: Oslash,
	Otild: Otild,
	Otilde: Otilde,
	Otimes: Otimes,
	Oum: Oum,
	Ouml: Ouml,
	OverBar: OverBar,
	OverBrace: OverBrace,
	OverBracket: OverBracket,
	OverParenthesis: OverParenthesis,
	PartialD: PartialD,
	Pcy: Pcy,
	Pfr: Pfr,
	Phi: Phi,
	Pi: Pi,
	PlusMinus: PlusMinus,
	Poincareplane: Poincareplane,
	Popf: Popf,
	Pr: Pr,
	Precedes: Precedes,
	PrecedesEqual: PrecedesEqual,
	PrecedesSlantEqual: PrecedesSlantEqual,
	PrecedesTilde: PrecedesTilde,
	Prime: Prime,
	Product: Product,
	Proportion: Proportion,
	Proportional: Proportional,
	Pscr: Pscr,
	Psi: Psi,
	QUO: QUO,
	QUOT: QUOT,
	Qfr: Qfr,
	Qopf: Qopf,
	Qscr: Qscr,
	RBarr: RBarr,
	RE: RE,
	REG: REG,
	Racute: Racute,
	Rang: Rang,
	Rarr: Rarr,
	Rarrtl: Rarrtl,
	Rcaron: Rcaron,
	Rcedil: Rcedil,
	Rcy: Rcy,
	Re: Re,
	ReverseElement: ReverseElement,
	ReverseEquilibrium: ReverseEquilibrium,
	ReverseUpEquilibrium: ReverseUpEquilibrium,
	Rfr: Rfr,
	Rho: Rho,
	RightAngleBracket: RightAngleBracket,
	RightArrow: RightArrow,
	RightArrowBar: RightArrowBar,
	RightArrowLeftArrow: RightArrowLeftArrow,
	RightCeiling: RightCeiling,
	RightDoubleBracket: RightDoubleBracket,
	RightDownTeeVector: RightDownTeeVector,
	RightDownVector: RightDownVector,
	RightDownVectorBar: RightDownVectorBar,
	RightFloor: RightFloor,
	RightTee: RightTee,
	RightTeeArrow: RightTeeArrow,
	RightTeeVector: RightTeeVector,
	RightTriangle: RightTriangle,
	RightTriangleBar: RightTriangleBar,
	RightTriangleEqual: RightTriangleEqual,
	RightUpDownVector: RightUpDownVector,
	RightUpTeeVector: RightUpTeeVector,
	RightUpVector: RightUpVector,
	RightUpVectorBar: RightUpVectorBar,
	RightVector: RightVector,
	RightVectorBar: RightVectorBar,
	Rightarrow: Rightarrow,
	Ropf: Ropf,
	RoundImplies: RoundImplies,
	Rrightarrow: Rrightarrow,
	Rscr: Rscr,
	Rsh: Rsh,
	RuleDelayed: RuleDelayed,
	SHCHcy: SHCHcy,
	SHcy: SHcy,
	SOFTcy: SOFTcy,
	Sacute: Sacute,
	Sc: Sc,
	Scaron: Scaron,
	Scedil: Scedil,
	Scirc: Scirc,
	Scy: Scy,
	Sfr: Sfr,
	ShortDownArrow: ShortDownArrow,
	ShortLeftArrow: ShortLeftArrow,
	ShortRightArrow: ShortRightArrow,
	ShortUpArrow: ShortUpArrow,
	Sigma: Sigma,
	SmallCircle: SmallCircle,
	Sopf: Sopf,
	Sqrt: Sqrt,
	Square: Square,
	SquareIntersection: SquareIntersection,
	SquareSubset: SquareSubset,
	SquareSubsetEqual: SquareSubsetEqual,
	SquareSuperset: SquareSuperset,
	SquareSupersetEqual: SquareSupersetEqual,
	SquareUnion: SquareUnion,
	Sscr: Sscr,
	Star: Star,
	Sub: Sub,
	Subset: Subset,
	SubsetEqual: SubsetEqual,
	Succeeds: Succeeds,
	SucceedsEqual: SucceedsEqual,
	SucceedsSlantEqual: SucceedsSlantEqual,
	SucceedsTilde: SucceedsTilde,
	SuchThat: SuchThat,
	Sum: Sum,
	Sup: Sup,
	Superset: Superset,
	SupersetEqual: SupersetEqual,
	Supset: Supset,
	THOR: THOR,
	THORN: THORN,
	TRADE: TRADE,
	TSHcy: TSHcy,
	TScy: TScy,
	Tab: Tab,
	Tau: Tau,
	Tcaron: Tcaron,
	Tcedil: Tcedil,
	Tcy: Tcy,
	Tfr: Tfr,
	Therefore: Therefore,
	Theta: Theta,
	ThickSpace: ThickSpace,
	ThinSpace: ThinSpace,
	Tilde: Tilde,
	TildeEqual: TildeEqual,
	TildeFullEqual: TildeFullEqual,
	TildeTilde: TildeTilde,
	Topf: Topf,
	TripleDot: TripleDot,
	Tscr: Tscr,
	Tstrok: Tstrok,
	Uacut: Uacut,
	Uacute: Uacute,
	Uarr: Uarr,
	Uarrocir: Uarrocir,
	Ubrcy: Ubrcy,
	Ubreve: Ubreve,
	Ucir: Ucir,
	Ucirc: Ucirc,
	Ucy: Ucy,
	Udblac: Udblac,
	Ufr: Ufr,
	Ugrav: Ugrav,
	Ugrave: Ugrave,
	Umacr: Umacr,
	UnderBar: UnderBar,
	UnderBrace: UnderBrace,
	UnderBracket: UnderBracket,
	UnderParenthesis: UnderParenthesis,
	Union: Union,
	UnionPlus: UnionPlus,
	Uogon: Uogon,
	Uopf: Uopf,
	UpArrow: UpArrow,
	UpArrowBar: UpArrowBar,
	UpArrowDownArrow: UpArrowDownArrow,
	UpDownArrow: UpDownArrow,
	UpEquilibrium: UpEquilibrium,
	UpTee: UpTee,
	UpTeeArrow: UpTeeArrow,
	Uparrow: Uparrow,
	Updownarrow: Updownarrow,
	UpperLeftArrow: UpperLeftArrow,
	UpperRightArrow: UpperRightArrow,
	Upsi: Upsi,
	Upsilon: Upsilon,
	Uring: Uring,
	Uscr: Uscr,
	Utilde: Utilde,
	Uum: Uum,
	Uuml: Uuml,
	VDash: VDash,
	Vbar: Vbar,
	Vcy: Vcy,
	Vdash: Vdash,
	Vdashl: Vdashl,
	Vee: Vee,
	Verbar: Verbar,
	Vert: Vert,
	VerticalBar: VerticalBar,
	VerticalLine: VerticalLine,
	VerticalSeparator: VerticalSeparator,
	VerticalTilde: VerticalTilde,
	VeryThinSpace: VeryThinSpace,
	Vfr: Vfr,
	Vopf: Vopf,
	Vscr: Vscr,
	Vvdash: Vvdash,
	Wcirc: Wcirc,
	Wedge: Wedge,
	Wfr: Wfr,
	Wopf: Wopf,
	Wscr: Wscr,
	Xfr: Xfr,
	Xi: Xi,
	Xopf: Xopf,
	Xscr: Xscr,
	YAcy: YAcy,
	YIcy: YIcy,
	YUcy: YUcy,
	Yacut: Yacut,
	Yacute: Yacute,
	Ycirc: Ycirc,
	Ycy: Ycy,
	Yfr: Yfr,
	Yopf: Yopf,
	Yscr: Yscr,
	Yuml: Yuml,
	ZHcy: ZHcy,
	Zacute: Zacute,
	Zcaron: Zcaron,
	Zcy: Zcy,
	Zdot: Zdot,
	ZeroWidthSpace: ZeroWidthSpace,
	Zeta: Zeta,
	Zfr: Zfr,
	Zopf: Zopf,
	Zscr: Zscr,
	aacut: aacut,
	aacute: aacute,
	abreve: abreve,
	ac: ac,
	acE: acE,
	acd: acd,
	acir: acir,
	acirc: acirc,
	acut: acut,
	acute: acute,
	acy: acy,
	aeli: aeli,
	aelig: aelig,
	af: af,
	afr: afr,
	agrav: agrav,
	agrave: agrave,
	alefsym: alefsym,
	aleph: aleph,
	alpha: alpha,
	amacr: amacr,
	amalg: amalg,
	am: am,
	amp: amp,
	and: and,
	andand: andand,
	andd: andd,
	andslope: andslope,
	andv: andv,
	ang: ang,
	ange: ange,
	angle: angle,
	angmsd: angmsd,
	angmsdaa: angmsdaa,
	angmsdab: angmsdab,
	angmsdac: angmsdac,
	angmsdad: angmsdad,
	angmsdae: angmsdae,
	angmsdaf: angmsdaf,
	angmsdag: angmsdag,
	angmsdah: angmsdah,
	angrt: angrt,
	angrtvb: angrtvb,
	angrtvbd: angrtvbd,
	angsph: angsph,
	angst: angst,
	angzarr: angzarr,
	aogon: aogon,
	aopf: aopf,
	ap: ap,
	apE: apE,
	apacir: apacir,
	ape: ape,
	apid: apid,
	apos: apos,
	approx: approx,
	approxeq: approxeq,
	arin: arin,
	aring: aring,
	ascr: ascr,
	ast: ast,
	asymp: asymp,
	asympeq: asympeq,
	atild: atild,
	atilde: atilde,
	aum: aum,
	auml: auml,
	awconint: awconint,
	awint: awint,
	bNot: bNot,
	backcong: backcong,
	backepsilon: backepsilon,
	backprime: backprime,
	backsim: backsim,
	backsimeq: backsimeq,
	barvee: barvee,
	barwed: barwed,
	barwedge: barwedge,
	bbrk: bbrk,
	bbrktbrk: bbrktbrk,
	bcong: bcong,
	bcy: bcy,
	bdquo: bdquo,
	becaus: becaus,
	because: because,
	bemptyv: bemptyv,
	bepsi: bepsi,
	bernou: bernou,
	beta: beta,
	beth: beth,
	between: between,
	bfr: bfr,
	bigcap: bigcap,
	bigcirc: bigcirc,
	bigcup: bigcup,
	bigodot: bigodot,
	bigoplus: bigoplus,
	bigotimes: bigotimes,
	bigsqcup: bigsqcup,
	bigstar: bigstar,
	bigtriangledown: bigtriangledown,
	bigtriangleup: bigtriangleup,
	biguplus: biguplus,
	bigvee: bigvee,
	bigwedge: bigwedge,
	bkarow: bkarow,
	blacklozenge: blacklozenge,
	blacksquare: blacksquare,
	blacktriangle: blacktriangle,
	blacktriangledown: blacktriangledown,
	blacktriangleleft: blacktriangleleft,
	blacktriangleright: blacktriangleright,
	blank: blank,
	blk12: blk12,
	blk14: blk14,
	blk34: blk34,
	block: block,
	bne: bne,
	bnequiv: bnequiv,
	bnot: bnot,
	bopf: bopf,
	bot: bot,
	bottom: bottom,
	bowtie: bowtie,
	boxDL: boxDL,
	boxDR: boxDR,
	boxDl: boxDl,
	boxDr: boxDr,
	boxH: boxH,
	boxHD: boxHD,
	boxHU: boxHU,
	boxHd: boxHd,
	boxHu: boxHu,
	boxUL: boxUL,
	boxUR: boxUR,
	boxUl: boxUl,
	boxUr: boxUr,
	boxV: boxV,
	boxVH: boxVH,
	boxVL: boxVL,
	boxVR: boxVR,
	boxVh: boxVh,
	boxVl: boxVl,
	boxVr: boxVr,
	boxbox: boxbox,
	boxdL: boxdL,
	boxdR: boxdR,
	boxdl: boxdl,
	boxdr: boxdr,
	boxh: boxh,
	boxhD: boxhD,
	boxhU: boxhU,
	boxhd: boxhd,
	boxhu: boxhu,
	boxminus: boxminus,
	boxplus: boxplus,
	boxtimes: boxtimes,
	boxuL: boxuL,
	boxuR: boxuR,
	boxul: boxul,
	boxur: boxur,
	boxv: boxv,
	boxvH: boxvH,
	boxvL: boxvL,
	boxvR: boxvR,
	boxvh: boxvh,
	boxvl: boxvl,
	boxvr: boxvr,
	bprime: bprime,
	breve: breve,
	brvba: brvba,
	brvbar: brvbar,
	bscr: bscr,
	bsemi: bsemi,
	bsim: bsim,
	bsime: bsime,
	bsol: bsol,
	bsolb: bsolb,
	bsolhsub: bsolhsub,
	bull: bull,
	bullet: bullet,
	bump: bump,
	bumpE: bumpE,
	bumpe: bumpe,
	bumpeq: bumpeq,
	cacute: cacute,
	cap: cap,
	capand: capand,
	capbrcup: capbrcup,
	capcap: capcap,
	capcup: capcup,
	capdot: capdot,
	caps: caps,
	caret: caret,
	caron: caron,
	ccaps: ccaps,
	ccaron: ccaron,
	ccedi: ccedi,
	ccedil: ccedil,
	ccirc: ccirc,
	ccups: ccups,
	ccupssm: ccupssm,
	cdot: cdot,
	cedi: cedi,
	cedil: cedil,
	cemptyv: cemptyv,
	cen: cen,
	cent: cent,
	centerdot: centerdot,
	cfr: cfr,
	chcy: chcy,
	check: check$2,
	checkmark: checkmark,
	chi: chi,
	cir: cir,
	cirE: cirE,
	circ: circ,
	circeq: circeq,
	circlearrowleft: circlearrowleft,
	circlearrowright: circlearrowright,
	circledR: circledR,
	circledS: circledS,
	circledast: circledast,
	circledcirc: circledcirc,
	circleddash: circleddash,
	cire: cire,
	cirfnint: cirfnint,
	cirmid: cirmid,
	cirscir: cirscir,
	clubs: clubs,
	clubsuit: clubsuit,
	colon: colon,
	colone: colone,
	coloneq: coloneq,
	comma: comma,
	commat: commat,
	comp: comp,
	compfn: compfn,
	complement: complement,
	complexes: complexes,
	cong: cong,
	congdot: congdot,
	conint: conint,
	copf: copf,
	coprod: coprod,
	cop: cop,
	copy: copy$2,
	copysr: copysr,
	crarr: crarr,
	cross: cross,
	cscr: cscr,
	csub: csub,
	csube: csube,
	csup: csup,
	csupe: csupe,
	ctdot: ctdot,
	cudarrl: cudarrl,
	cudarrr: cudarrr,
	cuepr: cuepr,
	cuesc: cuesc,
	cularr: cularr,
	cularrp: cularrp,
	cup: cup,
	cupbrcap: cupbrcap,
	cupcap: cupcap,
	cupcup: cupcup,
	cupdot: cupdot,
	cupor: cupor,
	cups: cups,
	curarr: curarr,
	curarrm: curarrm,
	curlyeqprec: curlyeqprec,
	curlyeqsucc: curlyeqsucc,
	curlyvee: curlyvee,
	curlywedge: curlywedge,
	curre: curre,
	curren: curren,
	curvearrowleft: curvearrowleft,
	curvearrowright: curvearrowright,
	cuvee: cuvee,
	cuwed: cuwed,
	cwconint: cwconint,
	cwint: cwint,
	cylcty: cylcty,
	dArr: dArr,
	dHar: dHar,
	dagger: dagger,
	daleth: daleth,
	darr: darr,
	dash: dash,
	dashv: dashv,
	dbkarow: dbkarow,
	dblac: dblac,
	dcaron: dcaron,
	dcy: dcy,
	dd: dd,
	ddagger: ddagger,
	ddarr: ddarr,
	ddotseq: ddotseq,
	de: de,
	deg: deg,
	delta: delta,
	demptyv: demptyv,
	dfisht: dfisht,
	dfr: dfr,
	dharl: dharl,
	dharr: dharr,
	diam: diam,
	diamond: diamond,
	diamondsuit: diamondsuit,
	diams: diams,
	die: die,
	digamma: digamma,
	disin: disin,
	div: div,
	divid: divid,
	divide: divide,
	divideontimes: divideontimes,
	divonx: divonx,
	djcy: djcy,
	dlcorn: dlcorn,
	dlcrop: dlcrop,
	dollar: dollar,
	dopf: dopf,
	dot: dot,
	doteq: doteq,
	doteqdot: doteqdot,
	dotminus: dotminus,
	dotplus: dotplus,
	dotsquare: dotsquare,
	doublebarwedge: doublebarwedge,
	downarrow: downarrow,
	downdownarrows: downdownarrows,
	downharpoonleft: downharpoonleft,
	downharpoonright: downharpoonright,
	drbkarow: drbkarow,
	drcorn: drcorn,
	drcrop: drcrop,
	dscr: dscr,
	dscy: dscy,
	dsol: dsol,
	dstrok: dstrok,
	dtdot: dtdot,
	dtri: dtri,
	dtrif: dtrif,
	duarr: duarr,
	duhar: duhar,
	dwangle: dwangle,
	dzcy: dzcy,
	dzigrarr: dzigrarr,
	eDDot: eDDot,
	eDot: eDot,
	eacut: eacut,
	eacute: eacute,
	easter: easter,
	ecaron: ecaron,
	ecir: ecir,
	ecirc: ecirc,
	ecolon: ecolon,
	ecy: ecy,
	edot: edot,
	ee: ee,
	efDot: efDot,
	efr: efr,
	eg: eg,
	egrav: egrav,
	egrave: egrave,
	egs: egs,
	egsdot: egsdot,
	el: el,
	elinters: elinters,
	ell: ell,
	els: els,
	elsdot: elsdot,
	emacr: emacr,
	empty: empty,
	emptyset: emptyset,
	emptyv: emptyv,
	emsp13: emsp13,
	emsp14: emsp14,
	emsp: emsp,
	eng: eng,
	ensp: ensp,
	eogon: eogon,
	eopf: eopf,
	epar: epar,
	eparsl: eparsl,
	eplus: eplus,
	epsi: epsi,
	epsilon: epsilon,
	epsiv: epsiv,
	eqcirc: eqcirc,
	eqcolon: eqcolon,
	eqsim: eqsim,
	eqslantgtr: eqslantgtr,
	eqslantless: eqslantless,
	equals: equals,
	equest: equest,
	equiv: equiv,
	equivDD: equivDD,
	eqvparsl: eqvparsl,
	erDot: erDot,
	erarr: erarr,
	escr: escr,
	esdot: esdot,
	esim: esim,
	eta: eta,
	et: et,
	eth: eth,
	eum: eum,
	euml: euml,
	euro: euro,
	excl: excl,
	exist: exist,
	expectation: expectation,
	exponentiale: exponentiale,
	fallingdotseq: fallingdotseq,
	fcy: fcy,
	female: female,
	ffilig: ffilig,
	fflig: fflig,
	ffllig: ffllig,
	ffr: ffr,
	filig: filig,
	fjlig: fjlig,
	flat: flat,
	fllig: fllig,
	fltns: fltns,
	fnof: fnof,
	fopf: fopf,
	forall: forall,
	fork: fork,
	forkv: forkv,
	fpartint: fpartint,
	frac1: frac1,
	frac12: frac12,
	frac13: frac13,
	frac14: frac14,
	frac15: frac15,
	frac16: frac16,
	frac18: frac18,
	frac23: frac23,
	frac25: frac25,
	frac3: frac3,
	frac34: frac34,
	frac35: frac35,
	frac38: frac38,
	frac45: frac45,
	frac56: frac56,
	frac58: frac58,
	frac78: frac78,
	frasl: frasl,
	frown: frown,
	fscr: fscr,
	gE: gE,
	gEl: gEl,
	gacute: gacute,
	gamma: gamma,
	gammad: gammad,
	gap: gap,
	gbreve: gbreve,
	gcirc: gcirc,
	gcy: gcy,
	gdot: gdot,
	ge: ge,
	gel: gel,
	geq: geq,
	geqq: geqq,
	geqslant: geqslant,
	ges: ges,
	gescc: gescc,
	gesdot: gesdot,
	gesdoto: gesdoto,
	gesdotol: gesdotol,
	gesl: gesl,
	gesles: gesles,
	gfr: gfr,
	gg: gg,
	ggg: ggg,
	gimel: gimel,
	gjcy: gjcy,
	gl: gl,
	glE: glE,
	gla: gla,
	glj: glj,
	gnE: gnE,
	gnap: gnap,
	gnapprox: gnapprox,
	gne: gne,
	gneq: gneq,
	gneqq: gneqq,
	gnsim: gnsim,
	gopf: gopf,
	grave: grave,
	gscr: gscr,
	gsim: gsim,
	gsime: gsime,
	gsiml: gsiml,
	g: g,
	gt: gt,
	gtcc: gtcc,
	gtcir: gtcir,
	gtdot: gtdot,
	gtlPar: gtlPar,
	gtquest: gtquest,
	gtrapprox: gtrapprox,
	gtrarr: gtrarr,
	gtrdot: gtrdot,
	gtreqless: gtreqless,
	gtreqqless: gtreqqless,
	gtrless: gtrless,
	gtrsim: gtrsim,
	gvertneqq: gvertneqq,
	gvnE: gvnE,
	hArr: hArr,
	hairsp: hairsp,
	half: half,
	hamilt: hamilt,
	hardcy: hardcy,
	harr: harr,
	harrcir: harrcir,
	harrw: harrw,
	hbar: hbar,
	hcirc: hcirc,
	hearts: hearts,
	heartsuit: heartsuit,
	hellip: hellip,
	hercon: hercon,
	hfr: hfr,
	hksearow: hksearow,
	hkswarow: hkswarow,
	hoarr: hoarr,
	homtht: homtht,
	hookleftarrow: hookleftarrow,
	hookrightarrow: hookrightarrow,
	hopf: hopf,
	horbar: horbar,
	hscr: hscr,
	hslash: hslash,
	hstrok: hstrok,
	hybull: hybull,
	hyphen: hyphen,
	iacut: iacut,
	iacute: iacute,
	ic: ic,
	icir: icir,
	icirc: icirc,
	icy: icy,
	iecy: iecy,
	iexc: iexc,
	iexcl: iexcl,
	iff: iff,
	ifr: ifr,
	igrav: igrav,
	igrave: igrave,
	ii: ii,
	iiiint: iiiint,
	iiint: iiint,
	iinfin: iinfin,
	iiota: iiota,
	ijlig: ijlig,
	imacr: imacr,
	image: image,
	imagline: imagline,
	imagpart: imagpart,
	imath: imath,
	imof: imof,
	imped: imped,
	incare: incare,
	infin: infin,
	infintie: infintie,
	inodot: inodot,
	int: int,
	intcal: intcal,
	integers: integers,
	intercal: intercal,
	intlarhk: intlarhk,
	intprod: intprod,
	iocy: iocy,
	iogon: iogon,
	iopf: iopf,
	iota: iota,
	iprod: iprod,
	iques: iques,
	iquest: iquest,
	iscr: iscr,
	isin: isin,
	isinE: isinE,
	isindot: isindot,
	isins: isins,
	isinsv: isinsv,
	isinv: isinv,
	it: it,
	itilde: itilde,
	iukcy: iukcy,
	ium: ium,
	iuml: iuml,
	jcirc: jcirc,
	jcy: jcy,
	jfr: jfr,
	jmath: jmath,
	jopf: jopf,
	jscr: jscr,
	jsercy: jsercy,
	jukcy: jukcy,
	kappa: kappa,
	kappav: kappav,
	kcedil: kcedil,
	kcy: kcy,
	kfr: kfr,
	kgreen: kgreen,
	khcy: khcy,
	kjcy: kjcy,
	kopf: kopf,
	kscr: kscr,
	lAarr: lAarr,
	lArr: lArr,
	lAtail: lAtail,
	lBarr: lBarr,
	lE: lE,
	lEg: lEg,
	lHar: lHar,
	lacute: lacute,
	laemptyv: laemptyv,
	lagran: lagran,
	lambda: lambda,
	lang: lang,
	langd: langd,
	langle: langle,
	lap: lap,
	laqu: laqu,
	laquo: laquo,
	larr: larr,
	larrb: larrb,
	larrbfs: larrbfs,
	larrfs: larrfs,
	larrhk: larrhk,
	larrlp: larrlp,
	larrpl: larrpl,
	larrsim: larrsim,
	larrtl: larrtl,
	lat: lat,
	latail: latail,
	late: late,
	lates: lates,
	lbarr: lbarr,
	lbbrk: lbbrk,
	lbrace: lbrace,
	lbrack: lbrack,
	lbrke: lbrke,
	lbrksld: lbrksld,
	lbrkslu: lbrkslu,
	lcaron: lcaron,
	lcedil: lcedil,
	lceil: lceil,
	lcub: lcub,
	lcy: lcy,
	ldca: ldca,
	ldquo: ldquo,
	ldquor: ldquor,
	ldrdhar: ldrdhar,
	ldrushar: ldrushar,
	ldsh: ldsh,
	le: le,
	leftarrow: leftarrow,
	leftarrowtail: leftarrowtail,
	leftharpoondown: leftharpoondown,
	leftharpoonup: leftharpoonup,
	leftleftarrows: leftleftarrows,
	leftrightarrow: leftrightarrow,
	leftrightarrows: leftrightarrows,
	leftrightharpoons: leftrightharpoons,
	leftrightsquigarrow: leftrightsquigarrow,
	leftthreetimes: leftthreetimes,
	leg: leg,
	leq: leq,
	leqq: leqq,
	leqslant: leqslant,
	les: les,
	lescc: lescc,
	lesdot: lesdot,
	lesdoto: lesdoto,
	lesdotor: lesdotor,
	lesg: lesg,
	lesges: lesges,
	lessapprox: lessapprox,
	lessdot: lessdot,
	lesseqgtr: lesseqgtr,
	lesseqqgtr: lesseqqgtr,
	lessgtr: lessgtr,
	lesssim: lesssim,
	lfisht: lfisht,
	lfloor: lfloor,
	lfr: lfr,
	lg: lg,
	lgE: lgE,
	lhard: lhard,
	lharu: lharu,
	lharul: lharul,
	lhblk: lhblk,
	ljcy: ljcy,
	ll: ll,
	llarr: llarr,
	llcorner: llcorner,
	llhard: llhard,
	lltri: lltri,
	lmidot: lmidot,
	lmoust: lmoust,
	lmoustache: lmoustache,
	lnE: lnE,
	lnap: lnap,
	lnapprox: lnapprox,
	lne: lne,
	lneq: lneq,
	lneqq: lneqq,
	lnsim: lnsim,
	loang: loang,
	loarr: loarr,
	lobrk: lobrk,
	longleftarrow: longleftarrow,
	longleftrightarrow: longleftrightarrow,
	longmapsto: longmapsto,
	longrightarrow: longrightarrow,
	looparrowleft: looparrowleft,
	looparrowright: looparrowright,
	lopar: lopar,
	lopf: lopf,
	loplus: loplus,
	lotimes: lotimes,
	lowast: lowast,
	lowbar: lowbar,
	loz: loz,
	lozenge: lozenge,
	lozf: lozf,
	lpar: lpar,
	lparlt: lparlt,
	lrarr: lrarr,
	lrcorner: lrcorner,
	lrhar: lrhar,
	lrhard: lrhard,
	lrm: lrm,
	lrtri: lrtri,
	lsaquo: lsaquo,
	lscr: lscr,
	lsh: lsh,
	lsim: lsim,
	lsime: lsime,
	lsimg: lsimg,
	lsqb: lsqb,
	lsquo: lsquo,
	lsquor: lsquor,
	lstrok: lstrok,
	l: l,
	lt: lt,
	ltcc: ltcc,
	ltcir: ltcir,
	ltdot: ltdot,
	lthree: lthree,
	ltimes: ltimes,
	ltlarr: ltlarr,
	ltquest: ltquest,
	ltrPar: ltrPar,
	ltri: ltri,
	ltrie: ltrie,
	ltrif: ltrif,
	lurdshar: lurdshar,
	luruhar: luruhar,
	lvertneqq: lvertneqq,
	lvnE: lvnE,
	mDDot: mDDot,
	mac: mac,
	macr: macr,
	male: male,
	malt: malt,
	maltese: maltese,
	map: map$3,
	mapsto: mapsto,
	mapstodown: mapstodown,
	mapstoleft: mapstoleft,
	mapstoup: mapstoup,
	marker: marker,
	mcomma: mcomma,
	mcy: mcy,
	mdash: mdash,
	measuredangle: measuredangle,
	mfr: mfr,
	mho: mho,
	micr: micr,
	micro: micro,
	mid: mid,
	midast: midast,
	midcir: midcir,
	middo: middo,
	middot: middot,
	minus: minus,
	minusb: minusb,
	minusd: minusd,
	minusdu: minusdu,
	mlcp: mlcp,
	mldr: mldr,
	mnplus: mnplus,
	models: models,
	mopf: mopf,
	mp: mp,
	mscr: mscr,
	mstpos: mstpos,
	mu: mu,
	multimap: multimap,
	mumap: mumap,
	nGg: nGg,
	nGt: nGt,
	nGtv: nGtv,
	nLeftarrow: nLeftarrow,
	nLeftrightarrow: nLeftrightarrow,
	nLl: nLl,
	nLt: nLt,
	nLtv: nLtv,
	nRightarrow: nRightarrow,
	nVDash: nVDash,
	nVdash: nVdash,
	nabla: nabla,
	nacute: nacute,
	nang: nang,
	nap: nap,
	napE: napE,
	napid: napid,
	napos: napos,
	napprox: napprox,
	natur: natur,
	natural: natural,
	naturals: naturals,
	nbs: nbs,
	nbsp: nbsp,
	nbump: nbump,
	nbumpe: nbumpe,
	ncap: ncap,
	ncaron: ncaron,
	ncedil: ncedil,
	ncong: ncong,
	ncongdot: ncongdot,
	ncup: ncup,
	ncy: ncy,
	ndash: ndash,
	ne: ne,
	neArr: neArr,
	nearhk: nearhk,
	nearr: nearr,
	nearrow: nearrow,
	nedot: nedot,
	nequiv: nequiv,
	nesear: nesear,
	nesim: nesim,
	nexist: nexist,
	nexists: nexists,
	nfr: nfr,
	ngE: ngE,
	nge: nge,
	ngeq: ngeq,
	ngeqq: ngeqq,
	ngeqslant: ngeqslant,
	nges: nges,
	ngsim: ngsim,
	ngt: ngt,
	ngtr: ngtr,
	nhArr: nhArr,
	nharr: nharr,
	nhpar: nhpar,
	ni: ni,
	nis: nis,
	nisd: nisd,
	niv: niv,
	njcy: njcy,
	nlArr: nlArr,
	nlE: nlE,
	nlarr: nlarr,
	nldr: nldr,
	nle: nle,
	nleftarrow: nleftarrow,
	nleftrightarrow: nleftrightarrow,
	nleq: nleq,
	nleqq: nleqq,
	nleqslant: nleqslant,
	nles: nles,
	nless: nless,
	nlsim: nlsim,
	nlt: nlt,
	nltri: nltri,
	nltrie: nltrie,
	nmid: nmid,
	nopf: nopf,
	no: no,
	not: not,
	notin: notin,
	notinE: notinE,
	notindot: notindot,
	notinva: notinva,
	notinvb: notinvb,
	notinvc: notinvc,
	notni: notni,
	notniva: notniva,
	notnivb: notnivb,
	notnivc: notnivc,
	npar: npar,
	nparallel: nparallel,
	nparsl: nparsl,
	npart: npart,
	npolint: npolint,
	npr: npr,
	nprcue: nprcue,
	npre: npre,
	nprec: nprec,
	npreceq: npreceq,
	nrArr: nrArr,
	nrarr: nrarr,
	nrarrc: nrarrc,
	nrarrw: nrarrw,
	nrightarrow: nrightarrow,
	nrtri: nrtri,
	nrtrie: nrtrie,
	nsc: nsc,
	nsccue: nsccue,
	nsce: nsce,
	nscr: nscr,
	nshortmid: nshortmid,
	nshortparallel: nshortparallel,
	nsim: nsim,
	nsime: nsime,
	nsimeq: nsimeq,
	nsmid: nsmid,
	nspar: nspar,
	nsqsube: nsqsube,
	nsqsupe: nsqsupe,
	nsub: nsub,
	nsubE: nsubE,
	nsube: nsube,
	nsubset: nsubset,
	nsubseteq: nsubseteq,
	nsubseteqq: nsubseteqq,
	nsucc: nsucc,
	nsucceq: nsucceq,
	nsup: nsup,
	nsupE: nsupE,
	nsupe: nsupe,
	nsupset: nsupset,
	nsupseteq: nsupseteq,
	nsupseteqq: nsupseteqq,
	ntgl: ntgl,
	ntild: ntild,
	ntilde: ntilde,
	ntlg: ntlg,
	ntriangleleft: ntriangleleft,
	ntrianglelefteq: ntrianglelefteq,
	ntriangleright: ntriangleright,
	ntrianglerighteq: ntrianglerighteq,
	nu: nu,
	num: num,
	numero: numero,
	numsp: numsp,
	nvDash: nvDash,
	nvHarr: nvHarr,
	nvap: nvap,
	nvdash: nvdash,
	nvge: nvge,
	nvgt: nvgt,
	nvinfin: nvinfin,
	nvlArr: nvlArr,
	nvle: nvle,
	nvlt: nvlt,
	nvltrie: nvltrie,
	nvrArr: nvrArr,
	nvrtrie: nvrtrie,
	nvsim: nvsim,
	nwArr: nwArr,
	nwarhk: nwarhk,
	nwarr: nwarr,
	nwarrow: nwarrow,
	nwnear: nwnear,
	oS: oS,
	oacut: oacut,
	oacute: oacute,
	oast: oast,
	ocir: ocir,
	ocirc: ocirc,
	ocy: ocy,
	odash: odash,
	odblac: odblac,
	odiv: odiv,
	odot: odot,
	odsold: odsold,
	oelig: oelig,
	ofcir: ofcir,
	ofr: ofr,
	ogon: ogon,
	ograv: ograv,
	ograve: ograve,
	ogt: ogt,
	ohbar: ohbar,
	ohm: ohm,
	oint: oint,
	olarr: olarr,
	olcir: olcir,
	olcross: olcross,
	oline: oline,
	olt: olt,
	omacr: omacr,
	omega: omega,
	omicron: omicron,
	omid: omid,
	ominus: ominus,
	oopf: oopf,
	opar: opar,
	operp: operp,
	oplus: oplus,
	or: or,
	orarr: orarr,
	ord: ord,
	order: order$1,
	orderof: orderof,
	ordf: ordf,
	ordm: ordm,
	origof: origof,
	oror: oror,
	orslope: orslope,
	orv: orv,
	oscr: oscr,
	oslas: oslas,
	oslash: oslash,
	osol: osol,
	otild: otild,
	otilde: otilde,
	otimes: otimes,
	otimesas: otimesas,
	oum: oum,
	ouml: ouml,
	ovbar: ovbar,
	par: par,
	para: para,
	parallel: parallel,
	parsim: parsim,
	parsl: parsl,
	part: part,
	pcy: pcy,
	percnt: percnt,
	period: period,
	permil: permil,
	perp: perp,
	pertenk: pertenk,
	pfr: pfr,
	phi: phi,
	phiv: phiv,
	phmmat: phmmat,
	phone: phone,
	pi: pi,
	pitchfork: pitchfork,
	piv: piv,
	planck: planck,
	planckh: planckh,
	plankv: plankv,
	plus: plus,
	plusacir: plusacir,
	plusb: plusb,
	pluscir: pluscir,
	plusdo: plusdo,
	plusdu: plusdu,
	pluse: pluse,
	plusm: plusm,
	plusmn: plusmn,
	plussim: plussim,
	plustwo: plustwo,
	pm: pm,
	pointint: pointint,
	popf: popf,
	poun: poun,
	pound: pound,
	pr: pr,
	prE: prE,
	prap: prap,
	prcue: prcue,
	pre: pre,
	prec: prec,
	precapprox: precapprox,
	preccurlyeq: preccurlyeq,
	preceq: preceq,
	precnapprox: precnapprox,
	precneqq: precneqq,
	precnsim: precnsim,
	precsim: precsim,
	prime: prime,
	primes: primes,
	prnE: prnE,
	prnap: prnap,
	prnsim: prnsim,
	prod: prod,
	profalar: profalar,
	profline: profline,
	profsurf: profsurf,
	prop: prop,
	propto: propto,
	prsim: prsim,
	prurel: prurel,
	pscr: pscr,
	psi: psi,
	puncsp: puncsp,
	qfr: qfr,
	qint: qint,
	qopf: qopf,
	qprime: qprime,
	qscr: qscr,
	quaternions: quaternions,
	quatint: quatint,
	quest: quest,
	questeq: questeq,
	quo: quo,
	quot: quot,
	rAarr: rAarr,
	rArr: rArr,
	rAtail: rAtail,
	rBarr: rBarr,
	rHar: rHar,
	race: race,
	racute: racute,
	radic: radic,
	raemptyv: raemptyv,
	rang: rang,
	rangd: rangd,
	range: range$1,
	rangle: rangle,
	raqu: raqu,
	raquo: raquo,
	rarr: rarr,
	rarrap: rarrap,
	rarrb: rarrb,
	rarrbfs: rarrbfs,
	rarrc: rarrc,
	rarrfs: rarrfs,
	rarrhk: rarrhk,
	rarrlp: rarrlp,
	rarrpl: rarrpl,
	rarrsim: rarrsim,
	rarrtl: rarrtl,
	rarrw: rarrw,
	ratail: ratail,
	ratio: ratio,
	rationals: rationals,
	rbarr: rbarr,
	rbbrk: rbbrk,
	rbrace: rbrace,
	rbrack: rbrack,
	rbrke: rbrke,
	rbrksld: rbrksld,
	rbrkslu: rbrkslu,
	rcaron: rcaron,
	rcedil: rcedil,
	rceil: rceil,
	rcub: rcub,
	rcy: rcy,
	rdca: rdca,
	rdldhar: rdldhar,
	rdquo: rdquo,
	rdquor: rdquor,
	rdsh: rdsh,
	real: real,
	realine: realine,
	realpart: realpart,
	reals: reals,
	rect: rect,
	re: re,
	reg: reg,
	rfisht: rfisht,
	rfloor: rfloor,
	rfr: rfr,
	rhard: rhard,
	rharu: rharu,
	rharul: rharul,
	rho: rho,
	rhov: rhov,
	rightarrow: rightarrow,
	rightarrowtail: rightarrowtail,
	rightharpoondown: rightharpoondown,
	rightharpoonup: rightharpoonup,
	rightleftarrows: rightleftarrows,
	rightleftharpoons: rightleftharpoons,
	rightrightarrows: rightrightarrows,
	rightsquigarrow: rightsquigarrow,
	rightthreetimes: rightthreetimes,
	ring: ring,
	risingdotseq: risingdotseq,
	rlarr: rlarr,
	rlhar: rlhar,
	rlm: rlm,
	rmoust: rmoust,
	rmoustache: rmoustache,
	rnmid: rnmid,
	roang: roang,
	roarr: roarr,
	robrk: robrk,
	ropar: ropar,
	ropf: ropf,
	roplus: roplus,
	rotimes: rotimes,
	rpar: rpar,
	rpargt: rpargt,
	rppolint: rppolint,
	rrarr: rrarr,
	rsaquo: rsaquo,
	rscr: rscr,
	rsh: rsh,
	rsqb: rsqb,
	rsquo: rsquo,
	rsquor: rsquor,
	rthree: rthree,
	rtimes: rtimes,
	rtri: rtri,
	rtrie: rtrie,
	rtrif: rtrif,
	rtriltri: rtriltri,
	ruluhar: ruluhar,
	rx: rx,
	sacute: sacute,
	sbquo: sbquo,
	sc: sc,
	scE: scE,
	scap: scap,
	scaron: scaron,
	sccue: sccue,
	sce: sce,
	scedil: scedil,
	scirc: scirc,
	scnE: scnE,
	scnap: scnap,
	scnsim: scnsim,
	scpolint: scpolint,
	scsim: scsim,
	scy: scy,
	sdot: sdot,
	sdotb: sdotb,
	sdote: sdote,
	seArr: seArr,
	searhk: searhk,
	searr: searr,
	searrow: searrow,
	sec: sec,
	sect: sect,
	semi: semi,
	seswar: seswar,
	setminus: setminus,
	setmn: setmn,
	sext: sext,
	sfr: sfr,
	sfrown: sfrown,
	sharp: sharp,
	shchcy: shchcy,
	shcy: shcy,
	shortmid: shortmid,
	shortparallel: shortparallel,
	sh: sh,
	shy: shy,
	sigma: sigma,
	sigmaf: sigmaf,
	sigmav: sigmav,
	sim: sim,
	simdot: simdot,
	sime: sime,
	simeq: simeq,
	simg: simg,
	simgE: simgE,
	siml: siml,
	simlE: simlE,
	simne: simne,
	simplus: simplus,
	simrarr: simrarr,
	slarr: slarr,
	smallsetminus: smallsetminus,
	smashp: smashp,
	smeparsl: smeparsl,
	smid: smid,
	smile: smile,
	smt: smt,
	smte: smte,
	smtes: smtes,
	softcy: softcy,
	sol: sol,
	solb: solb,
	solbar: solbar,
	sopf: sopf,
	spades: spades,
	spadesuit: spadesuit,
	spar: spar,
	sqcap: sqcap,
	sqcaps: sqcaps,
	sqcup: sqcup,
	sqcups: sqcups,
	sqsub: sqsub,
	sqsube: sqsube,
	sqsubset: sqsubset,
	sqsubseteq: sqsubseteq,
	sqsup: sqsup,
	sqsupe: sqsupe,
	sqsupset: sqsupset,
	sqsupseteq: sqsupseteq,
	squ: squ,
	square: square,
	squarf: squarf,
	squf: squf,
	srarr: srarr,
	sscr: sscr,
	ssetmn: ssetmn,
	ssmile: ssmile,
	sstarf: sstarf,
	star: star$1,
	starf: starf,
	straightepsilon: straightepsilon,
	straightphi: straightphi,
	strns: strns,
	sub: sub,
	subE: subE,
	subdot: subdot,
	sube: sube,
	subedot: subedot,
	submult: submult,
	subnE: subnE,
	subne: subne,
	subplus: subplus,
	subrarr: subrarr,
	subset: subset,
	subseteq: subseteq,
	subseteqq: subseteqq,
	subsetneq: subsetneq,
	subsetneqq: subsetneqq,
	subsim: subsim,
	subsub: subsub,
	subsup: subsup,
	succ: succ,
	succapprox: succapprox,
	succcurlyeq: succcurlyeq,
	succeq: succeq,
	succnapprox: succnapprox,
	succneqq: succneqq,
	succnsim: succnsim,
	succsim: succsim,
	sum: sum,
	sung: sung,
	sup: sup,
	sup1: sup1,
	sup2: sup2,
	sup3: sup3,
	supE: supE,
	supdot: supdot,
	supdsub: supdsub,
	supe: supe,
	supedot: supedot,
	suphsol: suphsol,
	suphsub: suphsub,
	suplarr: suplarr,
	supmult: supmult,
	supnE: supnE,
	supne: supne,
	supplus: supplus,
	supset: supset,
	supseteq: supseteq,
	supseteqq: supseteqq,
	supsetneq: supsetneq,
	supsetneqq: supsetneqq,
	supsim: supsim,
	supsub: supsub,
	supsup: supsup,
	swArr: swArr,
	swarhk: swarhk,
	swarr: swarr,
	swarrow: swarrow,
	swnwar: swnwar,
	szli: szli,
	szlig: szlig,
	target: target,
	tau: tau,
	tbrk: tbrk,
	tcaron: tcaron,
	tcedil: tcedil,
	tcy: tcy,
	tdot: tdot,
	telrec: telrec,
	tfr: tfr,
	there4: there4,
	therefore: therefore,
	theta: theta,
	thetasym: thetasym,
	thetav: thetav,
	thickapprox: thickapprox,
	thicksim: thicksim,
	thinsp: thinsp,
	thkap: thkap,
	thksim: thksim,
	thor: thor,
	thorn: thorn,
	tilde: tilde,
	time: time,
	times: times,
	timesb: timesb,
	timesbar: timesbar,
	timesd: timesd,
	tint: tint,
	toea: toea,
	top: top,
	topbot: topbot,
	topcir: topcir,
	topf: topf,
	topfork: topfork,
	tosa: tosa,
	tprime: tprime,
	trade: trade,
	triangle: triangle,
	triangledown: triangledown,
	triangleleft: triangleleft,
	trianglelefteq: trianglelefteq,
	triangleq: triangleq,
	triangleright: triangleright,
	trianglerighteq: trianglerighteq,
	tridot: tridot,
	trie: trie,
	triminus: triminus,
	triplus: triplus,
	trisb: trisb,
	tritime: tritime,
	trpezium: trpezium,
	tscr: tscr,
	tscy: tscy,
	tshcy: tshcy,
	tstrok: tstrok,
	twixt: twixt,
	twoheadleftarrow: twoheadleftarrow,
	twoheadrightarrow: twoheadrightarrow,
	uArr: uArr,
	uHar: uHar,
	uacut: uacut,
	uacute: uacute,
	uarr: uarr,
	ubrcy: ubrcy,
	ubreve: ubreve,
	ucir: ucir,
	ucirc: ucirc,
	ucy: ucy,
	udarr: udarr,
	udblac: udblac,
	udhar: udhar,
	ufisht: ufisht,
	ufr: ufr,
	ugrav: ugrav,
	ugrave: ugrave,
	uharl: uharl,
	uharr: uharr,
	uhblk: uhblk,
	ulcorn: ulcorn,
	ulcorner: ulcorner,
	ulcrop: ulcrop,
	ultri: ultri,
	umacr: umacr,
	um: um,
	uml: uml,
	uogon: uogon,
	uopf: uopf,
	uparrow: uparrow,
	updownarrow: updownarrow,
	upharpoonleft: upharpoonleft,
	upharpoonright: upharpoonright,
	uplus: uplus,
	upsi: upsi,
	upsih: upsih,
	upsilon: upsilon,
	upuparrows: upuparrows,
	urcorn: urcorn,
	urcorner: urcorner,
	urcrop: urcrop,
	uring: uring,
	urtri: urtri,
	uscr: uscr,
	utdot: utdot,
	utilde: utilde,
	utri: utri,
	utrif: utrif,
	uuarr: uuarr,
	uum: uum,
	uuml: uuml,
	uwangle: uwangle,
	vArr: vArr,
	vBar: vBar,
	vBarv: vBarv,
	vDash: vDash,
	vangrt: vangrt,
	varepsilon: varepsilon,
	varkappa: varkappa,
	varnothing: varnothing,
	varphi: varphi,
	varpi: varpi,
	varpropto: varpropto,
	varr: varr,
	varrho: varrho,
	varsigma: varsigma,
	varsubsetneq: varsubsetneq,
	varsubsetneqq: varsubsetneqq,
	varsupsetneq: varsupsetneq,
	varsupsetneqq: varsupsetneqq,
	vartheta: vartheta,
	vartriangleleft: vartriangleleft,
	vartriangleright: vartriangleright,
	vcy: vcy,
	vdash: vdash,
	vee: vee,
	veebar: veebar,
	veeeq: veeeq,
	vellip: vellip,
	verbar: verbar,
	vert: vert,
	vfr: vfr,
	vltri: vltri,
	vnsub: vnsub,
	vnsup: vnsup,
	vopf: vopf,
	vprop: vprop,
	vrtri: vrtri,
	vscr: vscr,
	vsubnE: vsubnE,
	vsubne: vsubne,
	vsupnE: vsupnE,
	vsupne: vsupne,
	vzigzag: vzigzag,
	wcirc: wcirc,
	wedbar: wedbar,
	wedge: wedge,
	wedgeq: wedgeq,
	weierp: weierp,
	wfr: wfr,
	wopf: wopf,
	wp: wp,
	wr: wr,
	wreath: wreath,
	wscr: wscr,
	xcap: xcap,
	xcirc: xcirc,
	xcup: xcup,
	xdtri: xdtri,
	xfr: xfr,
	xhArr: xhArr,
	xharr: xharr,
	xi: xi,
	xlArr: xlArr,
	xlarr: xlarr,
	xmap: xmap,
	xnis: xnis,
	xodot: xodot,
	xopf: xopf,
	xoplus: xoplus,
	xotime: xotime,
	xrArr: xrArr,
	xrarr: xrarr,
	xscr: xscr,
	xsqcup: xsqcup,
	xuplus: xuplus,
	xutri: xutri,
	xvee: xvee,
	xwedge: xwedge,
	yacut: yacut,
	yacute: yacute,
	yacy: yacy,
	ycirc: ycirc,
	ycy: ycy,
	ye: ye,
	yen: yen,
	yfr: yfr,
	yicy: yicy,
	yopf: yopf,
	yscr: yscr,
	yucy: yucy,
	yum: yum,
	yuml: yuml,
	zacute: zacute,
	zcaron: zcaron,
	zcy: zcy,
	zdot: zdot,
	zeetrf: zeetrf,
	zeta: zeta,
	zfr: zfr,
	zhcy: zhcy,
	zigrarr: zigrarr,
	zopf: zopf,
	zscr: zscr,
	zwj: zwj,
	zwnj: zwnj,
	default: index$2
});

const AElig$1 = "Æ";
const AMP$1 = "&";
const Aacute$1 = "Á";
const Acirc$1 = "Â";
const Agrave$1 = "À";
const Aring$1 = "Å";
const Atilde$1 = "Ã";
const Auml$1 = "Ä";
const COPY$1 = "©";
const Ccedil$1 = "Ç";
const ETH$1 = "Ð";
const Eacute$1 = "É";
const Ecirc$1 = "Ê";
const Egrave$1 = "È";
const Euml$1 = "Ë";
const GT$1 = ">";
const Iacute$1 = "Í";
const Icirc$1 = "Î";
const Igrave$1 = "Ì";
const Iuml$1 = "Ï";
const LT$1 = "<";
const Ntilde$1 = "Ñ";
const Oacute$1 = "Ó";
const Ocirc$1 = "Ô";
const Ograve$1 = "Ò";
const Oslash$1 = "Ø";
const Otilde$1 = "Õ";
const Ouml$1 = "Ö";
const QUOT$1 = "\"";
const REG$1 = "®";
const THORN$1 = "Þ";
const Uacute$1 = "Ú";
const Ucirc$1 = "Û";
const Ugrave$1 = "Ù";
const Uuml$1 = "Ü";
const Yacute$1 = "Ý";
const aacute$1 = "á";
const acirc$1 = "â";
const acute$1 = "´";
const aelig$1 = "æ";
const agrave$1 = "à";
const amp$1 = "&";
const aring$1 = "å";
const atilde$1 = "ã";
const auml$1 = "ä";
const brvbar$1 = "¦";
const ccedil$1 = "ç";
const cedil$1 = "¸";
const cent$1 = "¢";
const copy$3 = "©";
const curren$1 = "¤";
const deg$1 = "°";
const divide$1 = "÷";
const eacute$1 = "é";
const ecirc$1 = "ê";
const egrave$1 = "è";
const eth$1 = "ð";
const euml$1 = "ë";
const frac12$1 = "½";
const frac14$1 = "¼";
const frac34$1 = "¾";
const gt$1 = ">";
const iacute$1 = "í";
const icirc$1 = "î";
const iexcl$1 = "¡";
const igrave$1 = "ì";
const iquest$1 = "¿";
const iuml$1 = "ï";
const laquo$1 = "«";
const lt$1 = "<";
const macr$1 = "¯";
const micro$1 = "µ";
const middot$1 = "·";
const nbsp$1 = " ";
const not$1 = "¬";
const ntilde$1 = "ñ";
const oacute$1 = "ó";
const ocirc$1 = "ô";
const ograve$1 = "ò";
const ordf$1 = "ª";
const ordm$1 = "º";
const oslash$1 = "ø";
const otilde$1 = "õ";
const ouml$1 = "ö";
const para$1 = "¶";
const plusmn$1 = "±";
const pound$1 = "£";
const quot$1 = "\"";
const raquo$1 = "»";
const reg$1 = "®";
const sect$1 = "§";
const shy$1 = "­";
const sup1$1 = "¹";
const sup2$1 = "²";
const sup3$1 = "³";
const szlig$1 = "ß";
const thorn$1 = "þ";
const times$1 = "×";
const uacute$1 = "ú";
const ucirc$1 = "û";
const ugrave$1 = "ù";
const uml$1 = "¨";
const uuml$1 = "ü";
const yacute$1 = "ý";
const yen$1 = "¥";
const yuml$1 = "ÿ";
var index$3 = {
	AElig: AElig$1,
	AMP: AMP$1,
	Aacute: Aacute$1,
	Acirc: Acirc$1,
	Agrave: Agrave$1,
	Aring: Aring$1,
	Atilde: Atilde$1,
	Auml: Auml$1,
	COPY: COPY$1,
	Ccedil: Ccedil$1,
	ETH: ETH$1,
	Eacute: Eacute$1,
	Ecirc: Ecirc$1,
	Egrave: Egrave$1,
	Euml: Euml$1,
	GT: GT$1,
	Iacute: Iacute$1,
	Icirc: Icirc$1,
	Igrave: Igrave$1,
	Iuml: Iuml$1,
	LT: LT$1,
	Ntilde: Ntilde$1,
	Oacute: Oacute$1,
	Ocirc: Ocirc$1,
	Ograve: Ograve$1,
	Oslash: Oslash$1,
	Otilde: Otilde$1,
	Ouml: Ouml$1,
	QUOT: QUOT$1,
	REG: REG$1,
	THORN: THORN$1,
	Uacute: Uacute$1,
	Ucirc: Ucirc$1,
	Ugrave: Ugrave$1,
	Uuml: Uuml$1,
	Yacute: Yacute$1,
	aacute: aacute$1,
	acirc: acirc$1,
	acute: acute$1,
	aelig: aelig$1,
	agrave: agrave$1,
	amp: amp$1,
	aring: aring$1,
	atilde: atilde$1,
	auml: auml$1,
	brvbar: brvbar$1,
	ccedil: ccedil$1,
	cedil: cedil$1,
	cent: cent$1,
	copy: copy$3,
	curren: curren$1,
	deg: deg$1,
	divide: divide$1,
	eacute: eacute$1,
	ecirc: ecirc$1,
	egrave: egrave$1,
	eth: eth$1,
	euml: euml$1,
	frac12: frac12$1,
	frac14: frac14$1,
	frac34: frac34$1,
	gt: gt$1,
	iacute: iacute$1,
	icirc: icirc$1,
	iexcl: iexcl$1,
	igrave: igrave$1,
	iquest: iquest$1,
	iuml: iuml$1,
	laquo: laquo$1,
	lt: lt$1,
	macr: macr$1,
	micro: micro$1,
	middot: middot$1,
	nbsp: nbsp$1,
	not: not$1,
	ntilde: ntilde$1,
	oacute: oacute$1,
	ocirc: ocirc$1,
	ograve: ograve$1,
	ordf: ordf$1,
	ordm: ordm$1,
	oslash: oslash$1,
	otilde: otilde$1,
	ouml: ouml$1,
	para: para$1,
	plusmn: plusmn$1,
	pound: pound$1,
	quot: quot$1,
	raquo: raquo$1,
	reg: reg$1,
	sect: sect$1,
	shy: shy$1,
	sup1: sup1$1,
	sup2: sup2$1,
	sup3: sup3$1,
	szlig: szlig$1,
	thorn: thorn$1,
	times: times$1,
	uacute: uacute$1,
	ucirc: ucirc$1,
	ugrave: ugrave$1,
	uml: uml$1,
	uuml: uuml$1,
	yacute: yacute$1,
	yen: yen$1,
	yuml: yuml$1
};

var characterEntitiesLegacy = Object.freeze({
	AElig: AElig$1,
	AMP: AMP$1,
	Aacute: Aacute$1,
	Acirc: Acirc$1,
	Agrave: Agrave$1,
	Aring: Aring$1,
	Atilde: Atilde$1,
	Auml: Auml$1,
	COPY: COPY$1,
	Ccedil: Ccedil$1,
	ETH: ETH$1,
	Eacute: Eacute$1,
	Ecirc: Ecirc$1,
	Egrave: Egrave$1,
	Euml: Euml$1,
	GT: GT$1,
	Iacute: Iacute$1,
	Icirc: Icirc$1,
	Igrave: Igrave$1,
	Iuml: Iuml$1,
	LT: LT$1,
	Ntilde: Ntilde$1,
	Oacute: Oacute$1,
	Ocirc: Ocirc$1,
	Ograve: Ograve$1,
	Oslash: Oslash$1,
	Otilde: Otilde$1,
	Ouml: Ouml$1,
	QUOT: QUOT$1,
	REG: REG$1,
	THORN: THORN$1,
	Uacute: Uacute$1,
	Ucirc: Ucirc$1,
	Ugrave: Ugrave$1,
	Uuml: Uuml$1,
	Yacute: Yacute$1,
	aacute: aacute$1,
	acirc: acirc$1,
	acute: acute$1,
	aelig: aelig$1,
	agrave: agrave$1,
	amp: amp$1,
	aring: aring$1,
	atilde: atilde$1,
	auml: auml$1,
	brvbar: brvbar$1,
	ccedil: ccedil$1,
	cedil: cedil$1,
	cent: cent$1,
	copy: copy$3,
	curren: curren$1,
	deg: deg$1,
	divide: divide$1,
	eacute: eacute$1,
	ecirc: ecirc$1,
	egrave: egrave$1,
	eth: eth$1,
	euml: euml$1,
	frac12: frac12$1,
	frac14: frac14$1,
	frac34: frac34$1,
	gt: gt$1,
	iacute: iacute$1,
	icirc: icirc$1,
	iexcl: iexcl$1,
	igrave: igrave$1,
	iquest: iquest$1,
	iuml: iuml$1,
	laquo: laquo$1,
	lt: lt$1,
	macr: macr$1,
	micro: micro$1,
	middot: middot$1,
	nbsp: nbsp$1,
	not: not$1,
	ntilde: ntilde$1,
	oacute: oacute$1,
	ocirc: ocirc$1,
	ograve: ograve$1,
	ordf: ordf$1,
	ordm: ordm$1,
	oslash: oslash$1,
	otilde: otilde$1,
	ouml: ouml$1,
	para: para$1,
	plusmn: plusmn$1,
	pound: pound$1,
	quot: quot$1,
	raquo: raquo$1,
	reg: reg$1,
	sect: sect$1,
	shy: shy$1,
	sup1: sup1$1,
	sup2: sup2$1,
	sup3: sup3$1,
	szlig: szlig$1,
	thorn: thorn$1,
	times: times$1,
	uacute: uacute$1,
	ucirc: ucirc$1,
	ugrave: ugrave$1,
	uml: uml$1,
	uuml: uuml$1,
	yacute: yacute$1,
	yen: yen$1,
	yuml: yuml$1,
	default: index$3
});

var index$4 = {
	"0": "�",
	"128": "€",
	"130": "‚",
	"131": "ƒ",
	"132": "„",
	"133": "…",
	"134": "†",
	"135": "‡",
	"136": "ˆ",
	"137": "‰",
	"138": "Š",
	"139": "‹",
	"140": "Œ",
	"142": "Ž",
	"145": "‘",
	"146": "’",
	"147": "“",
	"148": "”",
	"149": "•",
	"150": "–",
	"151": "—",
	"152": "˜",
	"153": "™",
	"154": "š",
	"155": "›",
	"156": "œ",
	"158": "ž",
	"159": "Ÿ"
};

var characterReferenceInvalid = Object.freeze({
	default: index$4
});

var isDecimal = decimal;

/* Check if the given character code, or the character
 * code at the first character, is decimal. */
function decimal(character) {
  var code = typeof character === 'string' ? character.charCodeAt(0) : character;

  return code >= 48 && code <= 57 /* 0-9 */
}

var isHexadecimal = hexadecimal;

/* Check if the given character code, or the character
 * code at the first character, is hexadecimal. */
function hexadecimal(character) {
  var code = typeof character === 'string' ? character.charCodeAt(0) : character;

  return (
    (code >= 97 /* a */ && code <= 102) /* z */ ||
    (code >= 65 /* A */ && code <= 70) /* Z */ ||
    (code >= 48 /* A */ && code <= 57) /* Z */
  )
}

var isAlphabetical = alphabetical;

/* Check if the given character code, or the character
 * code at the first character, is alphabetical. */
function alphabetical(character) {
  var code = typeof character === 'string' ? character.charCodeAt(0) : character;

  return (
    (code >= 97 && code <= 122) /* a-z */ ||
    (code >= 65 && code <= 90) /* A-Z */
  )
}

var isAlphanumerical = alphanumerical;

/* Check if the given character code, or the character
 * code at the first character, is alphanumerical. */
function alphanumerical(character) {
  return isAlphabetical(character) || isDecimal(character)
}

var characterEntities$1 = ( characterEntities && index$2 ) || characterEntities;

var legacy = ( characterEntitiesLegacy && index$3 ) || characterEntitiesLegacy;

var invalid = ( characterReferenceInvalid && index$4 ) || characterReferenceInvalid;

var parseEntities_1 = parseEntities;

var own$3 = {}.hasOwnProperty;
var fromCharCode = String.fromCharCode;
var noop$1 = Function.prototype;

/* Default settings. */
var defaults = {
  warning: null,
  reference: null,
  text: null,
  warningContext: null,
  referenceContext: null,
  textContext: null,
  position: {},
  additional: null,
  attribute: false,
  nonTerminated: true
};

/* Reference types. */
var NAMED = 'named';
var HEXADECIMAL = 'hexadecimal';
var DECIMAL = 'decimal';

/* Map of bases. */
var BASE = {};

BASE[HEXADECIMAL] = 16;
BASE[DECIMAL] = 10;

/* Map of types to tests. Each type of character reference
 * accepts different characters. This test is used to
 * detect whether a reference has ended (as the semicolon
 * is not strictly needed). */
var TESTS = {};

TESTS[NAMED] = isAlphanumerical;
TESTS[DECIMAL] = isDecimal;
TESTS[HEXADECIMAL] = isHexadecimal;

/* Warning messages. */
var NAMED_NOT_TERMINATED = 1;
var NUMERIC_NOT_TERMINATED = 2;
var NAMED_EMPTY = 3;
var NUMERIC_EMPTY = 4;
var NAMED_UNKNOWN = 5;
var NUMERIC_DISALLOWED = 6;
var NUMERIC_PROHIBITED = 7;

var MESSAGES = {};

MESSAGES[NAMED_NOT_TERMINATED] =
  'Named character references must be terminated by a semicolon';
MESSAGES[NUMERIC_NOT_TERMINATED] =
  'Numeric character references must be terminated by a semicolon';
MESSAGES[NAMED_EMPTY] = 'Named character references cannot be empty';
MESSAGES[NUMERIC_EMPTY] = 'Numeric character references cannot be empty';
MESSAGES[NAMED_UNKNOWN] = 'Named character references must be known';
MESSAGES[NUMERIC_DISALLOWED] =
  'Numeric character references cannot be disallowed';
MESSAGES[NUMERIC_PROHIBITED] =
  'Numeric character references cannot be outside the permissible Unicode range';

/* Wrap to ensure clean parameters are given to `parse`. */
function parseEntities(value, options) {
  var settings = {};
  var option;
  var key;

  if (!options) {
    options = {};
  }

  for (key in defaults) {
    option = options[key];
    settings[key] =
      option === null || option === undefined ? defaults[key] : option;
  }

  if (settings.position.indent || settings.position.start) {
    settings.indent = settings.position.indent || [];
    settings.position = settings.position.start;
  }

  return parse$7(value, settings)
}

/* Parse entities. */
function parse$7(value, settings) {
  var additional = settings.additional;
  var nonTerminated = settings.nonTerminated;
  var handleText = settings.text;
  var handleReference = settings.reference;
  var handleWarning = settings.warning;
  var textContext = settings.textContext;
  var referenceContext = settings.referenceContext;
  var warningContext = settings.warningContext;
  var pos = settings.position;
  var indent = settings.indent || [];
  var length = value.length;
  var index = 0;
  var lines = -1;
  var column = pos.column || 1;
  var line = pos.line || 1;
  var queue = '';
  var result = [];
  var entityCharacters;
  var terminated;
  var characters;
  var character;
  var reference;
  var following;
  var warning;
  var reason;
  var output;
  var entity;
  var begin;
  var start;
  var type;
  var test;
  var prev;
  var next;
  var diff;
  var end;

  /* Cache the current point. */
  prev = now();

  /* Wrap `handleWarning`. */
  warning = handleWarning ? parseError : noop$1;

  /* Ensure the algorithm walks over the first character
   * and the end (inclusive). */
  index--;
  length++;

  while (++index < length) {
    /* If the previous character was a newline. */
    if (character === '\n') {
      column = indent[lines] || 1;
    }

    character = at(index);

    /* Handle anything other than an ampersand,
     * including newlines and EOF. */
    if (character !== '&') {
      if (character === '\n') {
        line++;
        lines++;
        column = 0;
      }

      if (character) {
        queue += character;
        column++;
      } else {
        flush();
      }
    } else {
      following = at(index + 1);

      /* The behaviour depends on the identity of the next
       * character. */
      if (
        following === '\t' /* Tab */ ||
        following === '\n' /* Newline */ ||
        following === '\f' /* Form feed */ ||
        following === ' ' /* Space */ ||
        following === '<' /* Less-than */ ||
        following === '&' /* Ampersand */ ||
        following === '' ||
        (additional && following === additional)
      ) {
        /* Not a character reference. No characters
         * are consumed, and nothing is returned.
         * This is not an error, either. */
        queue += character;
        column++;

        continue
      }

      start = index + 1;
      begin = start;
      end = start;

      /* Numerical entity. */
      if (following !== '#') {
        type = NAMED;
      } else {
        end = ++begin;

        /* The behaviour further depends on the
         * character after the U+0023 NUMBER SIGN. */
        following = at(end);

        if (following === 'x' || following === 'X') {
          /* ASCII hex digits. */
          type = HEXADECIMAL;
          end = ++begin;
        } else {
          /* ASCII digits. */
          type = DECIMAL;
        }
      }

      entityCharacters = '';
      entity = '';
      characters = '';
      test = TESTS[type];
      end--;

      while (++end < length) {
        following = at(end);

        if (!test(following)) {
          break
        }

        characters += following;

        /* Check if we can match a legacy named
         * reference.  If so, we cache that as the
         * last viable named reference.  This
         * ensures we do not need to walk backwards
         * later. */
        if (type === NAMED && own$3.call(legacy, characters)) {
          entityCharacters = characters;
          entity = legacy[characters];
        }
      }

      terminated = at(end) === ';';

      if (terminated) {
        end++;

        if (type === NAMED && own$3.call(characterEntities$1, characters)) {
          entityCharacters = characters;
          entity = characterEntities$1[characters];
        }
      }

      diff = 1 + end - start;

      if (!terminated && !nonTerminated) {
        /* Empty. */
      } else if (!characters) {
        /* An empty (possible) entity is valid, unless
         * its numeric (thus an ampersand followed by
         * an octothorp). */
        if (type !== NAMED) {
          warning(NUMERIC_EMPTY, diff);
        }
      } else if (type === NAMED) {
        /* An ampersand followed by anything
         * unknown, and not terminated, is invalid. */
        if (terminated && !entity) {
          warning(NAMED_UNKNOWN, 1);
        } else {
          /* If theres something after an entity
           * name which is not known, cap the
           * reference. */
          if (entityCharacters !== characters) {
            end = begin + entityCharacters.length;
            diff = 1 + end - begin;
            terminated = false;
          }

          /* If the reference is not terminated,
           * warn. */
          if (!terminated) {
            reason = entityCharacters ? NAMED_NOT_TERMINATED : NAMED_EMPTY;

            if (!settings.attribute) {
              warning(reason, diff);
            } else {
              following = at(end);

              if (following === '=') {
                warning(reason, diff);
                entity = null;
              } else if (isAlphanumerical(following)) {
                entity = null;
              } else {
                warning(reason, diff);
              }
            }
          }
        }

        reference = entity;
      } else {
        if (!terminated) {
          /* All non-terminated numeric entities are
           * not rendered, and trigger a warning. */
          warning(NUMERIC_NOT_TERMINATED, diff);
        }

        /* When terminated and number, parse as
         * either hexadecimal or decimal. */
        reference = parseInt(characters, BASE[type]);

        /* Trigger a warning when the parsed number
         * is prohibited, and replace with
         * replacement character. */
        if (prohibited(reference)) {
          warning(NUMERIC_PROHIBITED, diff);
          reference = '\uFFFD';
        } else if (reference in invalid) {
          /* Trigger a warning when the parsed number
           * is disallowed, and replace by an
           * alternative. */
          warning(NUMERIC_DISALLOWED, diff);
          reference = invalid[reference];
        } else {
          /* Parse the number. */
          output = '';

          /* Trigger a warning when the parsed
           * number should not be used. */
          if (disallowed(reference)) {
            warning(NUMERIC_DISALLOWED, diff);
          }

          /* Stringify the number. */
          if (reference > 0xffff) {
            reference -= 0x10000;
            output += fromCharCode((reference >>> (10 & 0x3ff)) | 0xd800);
            reference = 0xdc00 | (reference & 0x3ff);
          }

          reference = output + fromCharCode(reference);
        }
      }

      /* If we could not find a reference, queue the
       * checked characters (as normal characters),
       * and move the pointer to their end. This is
       * possible because we can be certain neither
       * newlines nor ampersands are included. */
      if (!reference) {
        characters = value.slice(start - 1, end);
        queue += characters;
        column += characters.length;
        index = end - 1;
      } else {
        /* Found it! First eat the queued
         * characters as normal text, then eat
         * an entity. */
        flush();

        prev = now();
        index = end - 1;
        column += end - start + 1;
        result.push(reference);
        next = now();
        next.offset++;

        if (handleReference) {
          handleReference.call(
            referenceContext,
            reference,
            {start: prev, end: next},
            value.slice(start - 1, end)
          );
        }

        prev = next;
      }
    }
  }

  /* Return the reduced nodes, and any possible warnings. */
  return result.join('')

  /* Get current position. */
  function now() {
    return {
      line: line,
      column: column,
      offset: index + (pos.offset || 0)
    }
  }

  /* “Throw” a parse-error: a warning. */
  function parseError(code, offset) {
    var position = now();

    position.column += offset;
    position.offset += offset;

    handleWarning.call(warningContext, MESSAGES[code], position, code);
  }

  /* Get character at position. */
  function at(position) {
    return value.charAt(position)
  }

  /* Flush `queue` (normal text). Macro invoked before
   * each entity and at the end of `value`.
   * Does nothing when `queue` is empty. */
  function flush() {
    if (queue) {
      result.push(queue);

      if (handleText) {
        handleText.call(textContext, queue, {start: prev, end: now()});
      }

      queue = '';
    }
  }
}

/* Check if `character` is outside the permissible unicode range. */
function prohibited(code) {
  return (code >= 0xd800 && code <= 0xdfff) || code > 0x10ffff
}

/* Check if `character` is disallowed. */
function disallowed(code) {
  return (
    (code >= 0x0001 && code <= 0x0008) ||
    code === 0x000b ||
    (code >= 0x000d && code <= 0x001f) ||
    (code >= 0x007f && code <= 0x009f) ||
    (code >= 0xfdd0 && code <= 0xfdef) ||
    (code & 0xffff) === 0xffff ||
    (code & 0xffff) === 0xfffe
  )
}

var decode$1 = factory$3;

/* Factory to create an entity decoder. */
function factory$3(ctx) {
  decoder.raw = decodeRaw;

  return decoder;

  /* Normalize `position` to add an `indent`. */
  function normalize(position) {
    var offsets = ctx.offset;
    var line = position.line;
    var result = [];

    while (++line) {
      if (!(line in offsets)) {
        break;
      }

      result.push((offsets[line] || 0) + 1);
    }

    return {
      start: position,
      indent: result
    };
  }

  /* Handle a warning.
   * See https://github.com/wooorm/parse-entities
   * for the warnings. */
  function handleWarning(reason, position, code) {
    if (code === 3) {
      return;
    }

    ctx.file.message(reason, position);
  }

  /* Decode `value` (at `position`) into text-nodes. */
  function decoder(value, position, handler) {
    parseEntities_1(value, {
      position: normalize(position),
      warning: handleWarning,
      text: handler,
      reference: handler,
      textContext: ctx,
      referenceContext: ctx
    });
  }

  /* Decode `value` (at `position`) into a string. */
  function decodeRaw(value, position) {
    return parseEntities_1(value, {
      position: normalize(position),
      warning: handleWarning
    });
  }
}

var tokenizer = factory$4;

var MERGEABLE_NODES = {
  text: mergeText,
  blockquote: mergeBlockquote
};

/* Check whether a node is mergeable with adjacent nodes. */
function mergeable(node) {
  var start;
  var end;

  if (node.type !== 'text' || !node.position) {
    return true;
  }

  start = node.position.start;
  end = node.position.end;

  /* Only merge nodes which occupy the same size as their
   * `value`. */
  return start.line !== end.line ||
      end.column - start.column === node.value.length;
}

/* Merge two text nodes: `node` into `prev`. */
function mergeText(prev, node) {
  prev.value += node.value;

  return prev;
}

/* Merge two blockquotes: `node` into `prev`, unless in
 * CommonMark mode. */
function mergeBlockquote(prev, node) {
  if (this.options.commonmark) {
    return node;
  }

  prev.children = prev.children.concat(node.children);

  return prev;
}

/* Construct a tokenizer.  This creates both
 * `tokenizeInline` and `tokenizeBlock`. */
function factory$4(type) {
  return tokenize;

  /* Tokenizer for a bound `type`. */
  function tokenize(value, location) {
    var self = this;
    var offset = self.offset;
    var tokens = [];
    var methods = self[type + 'Methods'];
    var tokenizers = self[type + 'Tokenizers'];
    var line = location.line;
    var column = location.column;
    var index;
    var length;
    var method;
    var name;
    var matched;
    var valueLength;

    /* Trim white space only lines. */
    if (!value) {
      return tokens;
    }

    /* Expose on `eat`. */
    eat.now = now;
    eat.file = self.file;

    /* Sync initial offset. */
    updatePosition('');

    /* Iterate over `value`, and iterate over all
     * tokenizers.  When one eats something, re-iterate
     * with the remaining value.  If no tokenizer eats,
     * something failed (should not happen) and an
     * exception is thrown. */
    while (value) {
      index = -1;
      length = methods.length;
      matched = false;

      while (++index < length) {
        name = methods[index];
        method = tokenizers[name];

        if (
          method &&
          /* istanbul ignore next */ (!method.onlyAtStart || self.atStart) &&
          (!method.notInList || !self.inList) &&
          (!method.notInBlock || !self.inBlock) &&
          (!method.notInLink || !self.inLink)
        ) {
          valueLength = value.length;

          method.apply(self, [eat, value]);

          matched = valueLength !== value.length;

          if (matched) {
            break;
          }
        }
      }

      /* istanbul ignore if */
      if (!matched) {
        self.file.fail(new Error('Infinite loop'), eat.now());
      }
    }

    self.eof = now();

    return tokens;

    /* Update line, column, and offset based on
     * `value`. */
    function updatePosition(subvalue) {
      var lastIndex = -1;
      var index = subvalue.indexOf('\n');

      while (index !== -1) {
        line++;
        lastIndex = index;
        index = subvalue.indexOf('\n', index + 1);
      }

      if (lastIndex === -1) {
        column += subvalue.length;
      } else {
        column = subvalue.length - lastIndex;
      }

      if (line in offset) {
        if (lastIndex !== -1) {
          column += offset[line];
        } else if (column <= offset[line]) {
          column = offset[line] + 1;
        }
      }
    }

    /* Get offset.  Called before the first character is
     * eaten to retrieve the range's offsets. */
    function getOffset() {
      var indentation = [];
      var pos = line + 1;

      /* Done.  Called when the last character is
       * eaten to retrieve the range’s offsets. */
      return function () {
        var last = line + 1;

        while (pos < last) {
          indentation.push((offset[pos] || 0) + 1);

          pos++;
        }

        return indentation;
      };
    }

    /* Get the current position. */
    function now() {
      var pos = {line: line, column: column};

      pos.offset = self.toOffset(pos);

      return pos;
    }

    /* Store position information for a node. */
    function Position(start) {
      this.start = start;
      this.end = now();
    }

    /* Throw when a value is incorrectly eaten.
     * This shouldn’t happen but will throw on new,
     * incorrect rules. */
    function validateEat(subvalue) {
      /* istanbul ignore if */
      if (value.substring(0, subvalue.length) !== subvalue) {
        /* Capture stack-trace. */
        self.file.fail(
          new Error(
            'Incorrectly eaten value: please report this ' +
            'warning on http://git.io/vg5Ft'
          ),
          now()
        );
      }
    }

    /* Mark position and patch `node.position`. */
    function position() {
      var before = now();

      return update;

      /* Add the position to a node. */
      function update(node, indent) {
        var prev = node.position;
        var start = prev ? prev.start : before;
        var combined = [];
        var n = prev && prev.end.line;
        var l = before.line;

        node.position = new Position(start);

        /* If there was already a `position`, this
         * node was merged.  Fixing `start` wasn’t
         * hard, but the indent is different.
         * Especially because some information, the
         * indent between `n` and `l` wasn’t
         * tracked.  Luckily, that space is
         * (should be?) empty, so we can safely
         * check for it now. */
        if (prev && indent && prev.indent) {
          combined = prev.indent;

          if (n < l) {
            while (++n < l) {
              combined.push((offset[n] || 0) + 1);
            }

            combined.push(before.column);
          }

          indent = combined.concat(indent);
        }

        node.position.indent = indent || [];

        return node;
      }
    }

    /* Add `node` to `parent`s children or to `tokens`.
     * Performs merges where possible. */
    function add(node, parent) {
      var children = parent ? parent.children : tokens;
      var prev = children[children.length - 1];

      if (
        prev &&
        node.type === prev.type &&
        node.type in MERGEABLE_NODES &&
        mergeable(prev) &&
        mergeable(node)
      ) {
        node = MERGEABLE_NODES[node.type].call(self, prev, node);
      }

      if (node !== prev) {
        children.push(node);
      }

      if (self.atStart && tokens.length !== 0) {
        self.exitStart();
      }

      return node;
    }

    /* Remove `subvalue` from `value`.
     * `subvalue` must be at the start of `value`. */
    function eat(subvalue) {
      var indent = getOffset();
      var pos = position();
      var current = now();

      validateEat(subvalue);

      apply.reset = reset;
      reset.test = test;
      apply.test = test;

      value = value.substring(subvalue.length);

      updatePosition(subvalue);

      indent = indent();

      return apply;

      /* Add the given arguments, add `position` to
       * the returned node, and return the node. */
      function apply(node, parent) {
        return pos(add(pos(node), parent), indent);
      }

      /* Functions just like apply, but resets the
       * content:  the line and column are reversed,
       * and the eaten value is re-added.
       * This is useful for nodes with a single
       * type of content, such as lists and tables.
       * See `apply` above for what parameters are
       * expected. */
      function reset() {
        var node = apply.apply(null, arguments);

        line = current.line;
        column = current.column;
        value = subvalue + value;

        return node;
      }

      /* Test the position, after eating, and reverse
       * to a not-eaten state. */
      function test() {
        var result = pos({});

        line = current.line;
        column = current.column;
        value = subvalue + value;

        return result.position;
      }
    }
  }
}

var markdownEscapes = escapes;

var defaults$1 = [
  '\\',
  '`',
  '*',
  '{',
  '}',
  '[',
  ']',
  '(',
  ')',
  '#',
  '+',
  '-',
  '.',
  '!',
  '_',
  '>'
];

var gfm = defaults$1.concat(['~', '|']);

var commonmark = gfm.concat([
  '\n',
  '"',
  '$',
  '%',
  '&',
  "'",
  ',',
  '/',
  ':',
  ';',
  '<',
  '=',
  '?',
  '@',
  '^'
]);

escapes.default = defaults$1;
escapes.gfm = gfm;
escapes.commonmark = commonmark;

/* Get markdown escapes. */
function escapes(options) {
  var settings = options || {};

  if (settings.commonmark) {
    return commonmark
  }

  return settings.gfm ? gfm : defaults$1
}

var blockElements = [
  "address",
  "article",
  "aside",
  "base",
  "basefont",
  "blockquote",
  "body",
  "caption",
  "center",
  "col",
  "colgroup",
  "dd",
  "details",
  "dialog",
  "dir",
  "div",
  "dl",
  "dt",
  "fieldset",
  "figcaption",
  "figure",
  "footer",
  "form",
  "frame",
  "frameset",
  "h1",
  "h2",
  "h3",
  "h4",
  "h5",
  "h6",
  "head",
  "header",
  "hgroup",
  "hr",
  "html",
  "iframe",
  "legend",
  "li",
  "link",
  "main",
  "menu",
  "menuitem",
  "meta",
  "nav",
  "noframes",
  "ol",
  "optgroup",
  "option",
  "p",
  "param",
  "pre",
  "section",
  "source",
  "title",
  "summary",
  "table",
  "tbody",
  "td",
  "tfoot",
  "th",
  "thead",
  "title",
  "tr",
  "track",
  "ul"
]
;

var blockElements$1 = Object.freeze({
	default: blockElements
});

var require$$0$16 = ( blockElements$1 && blockElements ) || blockElements$1;

var defaults$2 = {
  position: true,
  gfm: true,
  commonmark: false,
  footnotes: false,
  pedantic: false,
  blocks: require$$0$16
};

var setOptions_1 = setOptions;

function setOptions(options) {
  var self = this;
  var current = self.options;
  var key;
  var value;

  if (options == null) {
    options = {};
  } else if (typeof options === 'object') {
    options = immutable(options);
  } else {
    throw new Error(
      'Invalid value `' + options + '` ' +
      'for setting `options`'
    );
  }

  for (key in defaults$2) {
    value = options[key];

    if (value == null) {
      value = current[key];
    }

    if (
      (key !== 'blocks' && typeof value !== 'boolean') ||
      (key === 'blocks' && typeof value !== 'object')
    ) {
      throw new Error('Invalid value `' + value + '` for setting `options.' + key + '`');
    }

    options[key] = value;
  }

  self.options = options;
  self.escape = markdownEscapes(options);

  return self;
}

/* eslint-disable max-params */

/* Expose. */
var unistUtilIs = is;

/* Assert if `test` passes for `node`.
 * When a `parent` node is known the `index` of node */
function is(test, node, index, parent, context) {
  var hasParent = parent !== null && parent !== undefined;
  var hasIndex = index !== null && index !== undefined;
  var check = convert(test);

  if (
    hasIndex &&
    (typeof index !== 'number' || index < 0 || index === Infinity)
  ) {
    throw new Error('Expected positive finite index or child node')
  }

  if (hasParent && (!is(null, parent) || !parent.children)) {
    throw new Error('Expected parent node')
  }

  if (!node || !node.type || typeof node.type !== 'string') {
    return false
  }

  if (hasParent !== hasIndex) {
    throw new Error('Expected both parent and index')
  }

  return Boolean(check.call(context, node, index, parent))
}

function convert(test) {
  if (typeof test === 'string') {
    return typeFactory(test)
  }

  if (test === null || test === undefined) {
    return ok$1
  }

  if (typeof test === 'object') {
    return ('length' in test ? anyFactory : matchesFactory)(test)
  }

  if (typeof test === 'function') {
    return test
  }

  throw new Error('Expected function, string, or object as test')
}

function convertAll(tests) {
  var results = [];
  var length = tests.length;
  var index = -1;

  while (++index < length) {
    results[index] = convert(tests[index]);
  }

  return results
}

/* Utility assert each property in `test` is represented
 * in `node`, and each values are strictly equal. */
function matchesFactory(test) {
  return matches

  function matches(node) {
    var key;

    for (key in test) {
      if (node[key] !== test[key]) {
        return false
      }
    }

    return true
  }
}

function anyFactory(tests) {
  var checks = convertAll(tests);
  var length = checks.length;

  return matches

  function matches() {
    var index = -1;

    while (++index < length) {
      if (checks[index].apply(this, arguments)) {
        return true
      }
    }

    return false
  }
}

/* Utility to convert a string into a function which checks
 * a given node’s type for said string. */
function typeFactory(test) {
  return type

  function type(node) {
    return Boolean(node && node.type === test)
  }
}

/* Utility to return true. */
function ok$1() {
  return true
}

var unistUtilVisitParents = visitParents;



var CONTINUE = true;
var SKIP = 'skip';
var EXIT = false;

visitParents.CONTINUE = CONTINUE;
visitParents.SKIP = SKIP;
visitParents.EXIT = EXIT;

function visitParents(tree, test, visitor, reverse) {
  if (typeof test === 'function' && typeof visitor !== 'function') {
    reverse = visitor;
    visitor = test;
    test = null;
  }

  one(tree, null, []);

  // Visit a single node.
  function one(node, index, parents) {
    var result;

    if (!test || unistUtilIs(test, node, index, parents[parents.length - 1] || null)) {
      result = visitor(node, parents);

      if (result === EXIT) {
        return result
      }
    }

    if (node.children && result !== SKIP) {
      return all(node.children, parents.concat(node)) === EXIT ? EXIT : result
    }

    return result
  }

  // Visit children in `parent`.
  function all(children, parents) {
    var min = -1;
    var step = reverse ? -1 : 1;
    var index = (reverse ? children.length : min) + step;
    var child;
    var result;

    while (index > min && index < children.length) {
      child = children[index];
      result = child && one(child, index, parents);

      if (result === EXIT) {
        return result
      }

      index = typeof result === 'number' ? result : index + step;
    }
  }
}

var unistUtilVisit = visit;



var CONTINUE$1 = unistUtilVisitParents.CONTINUE;
var SKIP$1 = unistUtilVisitParents.SKIP;
var EXIT$1 = unistUtilVisitParents.EXIT;

visit.CONTINUE = CONTINUE$1;
visit.SKIP = SKIP$1;
visit.EXIT = EXIT$1;

function visit(tree, test, visitor, reverse) {
  if (typeof test === 'function' && typeof visitor !== 'function') {
    reverse = visitor;
    visitor = test;
    test = null;
  }

  unistUtilVisitParents(tree, test, overload, reverse);

  function overload(node, parents) {
    var parent = parents[parents.length - 1];
    var index = parent ? parent.children.indexOf(node) : null;
    return visitor(node, index, parent)
  }
}

var unistUtilRemovePosition = removePosition;

/* Remove `position`s from `tree`. */
function removePosition(node, force) {
  unistUtilVisit(node, force ? hard : soft);
  return node
}

function hard(node) {
  delete node.position;
}

function soft(node) {
  node.position = undefined;
}

var parse_1$3 = parse$8;

var C_NEWLINE = '\n';
var EXPRESSION_LINE_BREAKS = /\r\n|\r/g;

/* Parse the bound file. */
function parse$8() {
  var self = this;
  var value = String(self.file);
  var start = {line: 1, column: 1, offset: 0};
  var content = immutable(start);
  var node;

  /* Clean non-unix newlines: `\r\n` and `\r` are all
   * changed to `\n`.  This should not affect positional
   * information. */
  value = value.replace(EXPRESSION_LINE_BREAKS, C_NEWLINE);

  if (value.charCodeAt(0) === 0xFEFF) {
    value = value.slice(1);

    content.column++;
    content.offset++;
  }

  node = {
    type: 'root',
    children: self.tokenizeBlock(value, content),
    position: {
      start: start,
      end: self.eof || immutable(start)
    }
  };

  if (!self.options.position) {
    unistUtilRemovePosition(node, true);
  }

  return node;
}

var isWhitespaceCharacter = whitespace;

var fromCode = String.fromCharCode;
var re$1 = /\s/;

/* Check if the given character code, or the character
 * code at the first character, is a whitespace character. */
function whitespace(character) {
  return re$1.test(
    typeof character === 'number' ? fromCode(character) : character.charAt(0)
  )
}

var newline_1 = newline;

/* Tokenise newline. */
function newline(eat, value, silent) {
  var character = value.charAt(0);
  var length;
  var subvalue;
  var queue;
  var index;

  if (character !== '\n') {
    return;
  }

  /* istanbul ignore if - never used (yet) */
  if (silent) {
    return true;
  }

  index = 1;
  length = value.length;
  subvalue = character;
  queue = '';

  while (index < length) {
    character = value.charAt(index);

    if (!isWhitespaceCharacter(character)) {
      break;
    }

    queue += character;

    if (character === '\n') {
      subvalue += queue;
      queue = '';
    }

    index++;
  }

  eat(subvalue);
}

var trimTrailingLines_1 = trimTrailingLines;

var line = '\n';

/* Remove final newline characters from `value`. */
function trimTrailingLines(value) {
  var val = String(value);
  var index = val.length;

  while (val.charAt(--index) === line) {
    /* Empty */
  }

  return val.slice(0, index + 1)
}

var codeIndented = indentedCode;

var C_NEWLINE$1 = '\n';
var C_TAB = '\t';
var C_SPACE = ' ';

var CODE_INDENT_COUNT = 4;
var CODE_INDENT = repeatString(C_SPACE, CODE_INDENT_COUNT);

/* Tokenise indented code. */
function indentedCode(eat, value, silent) {
  var index = -1;
  var length = value.length;
  var subvalue = '';
  var content = '';
  var subvalueQueue = '';
  var contentQueue = '';
  var character;
  var blankQueue;
  var indent;

  while (++index < length) {
    character = value.charAt(index);

    if (indent) {
      indent = false;

      subvalue += subvalueQueue;
      content += contentQueue;
      subvalueQueue = '';
      contentQueue = '';

      if (character === C_NEWLINE$1) {
        subvalueQueue = character;
        contentQueue = character;
      } else {
        subvalue += character;
        content += character;

        while (++index < length) {
          character = value.charAt(index);

          if (!character || character === C_NEWLINE$1) {
            contentQueue = character;
            subvalueQueue = character;
            break;
          }

          subvalue += character;
          content += character;
        }
      }
    } else if (
      character === C_SPACE &&
      value.charAt(index + 1) === character &&
      value.charAt(index + 2) === character &&
      value.charAt(index + 3) === character
    ) {
      subvalueQueue += CODE_INDENT;
      index += 3;
      indent = true;
    } else if (character === C_TAB) {
      subvalueQueue += character;
      indent = true;
    } else {
      blankQueue = '';

      while (character === C_TAB || character === C_SPACE) {
        blankQueue += character;
        character = value.charAt(++index);
      }

      if (character !== C_NEWLINE$1) {
        break;
      }

      subvalueQueue += blankQueue + character;
      contentQueue += character;
    }
  }

  if (content) {
    if (silent) {
      return true;
    }

    return eat(subvalue)({
      type: 'code',
      lang: null,
      value: trimTrailingLines_1(content)
    });
  }
}

var codeFenced = fencedCode;

var C_NEWLINE$2 = '\n';
var C_TAB$1 = '\t';
var C_SPACE$1 = ' ';
var C_TILDE = '~';
var C_TICK = '`';

var MIN_FENCE_COUNT = 3;
var CODE_INDENT_COUNT$1 = 4;

function fencedCode(eat, value, silent) {
  var self = this;
  var settings = self.options;
  var length = value.length + 1;
  var index = 0;
  var subvalue = '';
  var fenceCount;
  var marker;
  var character;
  var flag;
  var queue;
  var content;
  var exdentedContent;
  var closing;
  var exdentedClosing;
  var indent;
  var now;

  if (!settings.gfm) {
    return;
  }

  /* Eat initial spacing. */
  while (index < length) {
    character = value.charAt(index);

    if (character !== C_SPACE$1 && character !== C_TAB$1) {
      break;
    }

    subvalue += character;
    index++;
  }

  indent = index;

  /* Eat the fence. */
  character = value.charAt(index);

  if (character !== C_TILDE && character !== C_TICK) {
    return;
  }

  index++;
  marker = character;
  fenceCount = 1;
  subvalue += character;

  while (index < length) {
    character = value.charAt(index);

    if (character !== marker) {
      break;
    }

    subvalue += character;
    fenceCount++;
    index++;
  }

  if (fenceCount < MIN_FENCE_COUNT) {
    return;
  }

  /* Eat spacing before flag. */
  while (index < length) {
    character = value.charAt(index);

    if (character !== C_SPACE$1 && character !== C_TAB$1) {
      break;
    }

    subvalue += character;
    index++;
  }

  /* Eat flag. */
  flag = '';
  queue = '';

  while (index < length) {
    character = value.charAt(index);

    if (
      character === C_NEWLINE$2 ||
      character === C_TILDE ||
      character === C_TICK
    ) {
      break;
    }

    if (character === C_SPACE$1 || character === C_TAB$1) {
      queue += character;
    } else {
      flag += queue + character;
      queue = '';
    }

    index++;
  }

  character = value.charAt(index);

  if (character && character !== C_NEWLINE$2) {
    return;
  }

  if (silent) {
    return true;
  }

  now = eat.now();
  now.column += subvalue.length;
  now.offset += subvalue.length;

  subvalue += flag;
  flag = self.decode.raw(self.unescape(flag), now);

  if (queue) {
    subvalue += queue;
  }

  queue = '';
  closing = '';
  exdentedClosing = '';
  content = '';
  exdentedContent = '';

  /* Eat content. */
  while (index < length) {
    character = value.charAt(index);
    content += closing;
    exdentedContent += exdentedClosing;
    closing = '';
    exdentedClosing = '';

    if (character !== C_NEWLINE$2) {
      content += character;
      exdentedClosing += character;
      index++;
      continue;
    }

    /* Add the newline to `subvalue` if its the first
     * character.  Otherwise, add it to the `closing`
     * queue. */
    if (content) {
      closing += character;
      exdentedClosing += character;
    } else {
      subvalue += character;
    }

    queue = '';
    index++;

    while (index < length) {
      character = value.charAt(index);

      if (character !== C_SPACE$1) {
        break;
      }

      queue += character;
      index++;
    }

    closing += queue;
    exdentedClosing += queue.slice(indent);

    if (queue.length >= CODE_INDENT_COUNT$1) {
      continue;
    }

    queue = '';

    while (index < length) {
      character = value.charAt(index);

      if (character !== marker) {
        break;
      }

      queue += character;
      index++;
    }

    closing += queue;
    exdentedClosing += queue;

    if (queue.length < fenceCount) {
      continue;
    }

    queue = '';

    while (index < length) {
      character = value.charAt(index);

      if (character !== C_SPACE$1 && character !== C_TAB$1) {
        break;
      }

      closing += character;
      exdentedClosing += character;
      index++;
    }

    if (!character || character === C_NEWLINE$2) {
      break;
    }
  }

  subvalue += content + closing;

  return eat(subvalue)({
    type: 'code',
    lang: flag || null,
    value: trimTrailingLines_1(exdentedContent)
  });
}

var trim_1 = createCommonjsModule(function (module, exports) {
exports = module.exports = trim;

function trim(str){
  return str.replace(/^\s*|\s*$/g, '');
}

exports.left = function(str){
  return str.replace(/^\s*/, '');
};

exports.right = function(str){
  return str.replace(/\s*$/, '');
};
});

var trim_2 = trim_1.left;
var trim_3 = trim_1.right;

var interrupt_1 = interrupt;

function interrupt(interruptors, tokenizers, ctx, params) {
  var bools = ['pedantic', 'commonmark'];
  var count = bools.length;
  var length = interruptors.length;
  var index = -1;
  var interruptor;
  var config;
  var fn;
  var offset;
  var bool;
  var ignore;

  while (++index < length) {
    interruptor = interruptors[index];
    config = interruptor[1] || {};
    fn = interruptor[0];
    offset = -1;
    ignore = false;

    while (++offset < count) {
      bool = bools[offset];

      if (config[bool] !== undefined && config[bool] !== ctx.options[bool]) {
        ignore = true;
        break;
      }
    }

    if (ignore) {
      continue;
    }

    if (tokenizers[fn].apply(ctx, params)) {
      return true;
    }
  }

  return false;
}

var blockquote_1 = blockquote;

var C_NEWLINE$3 = '\n';
var C_TAB$2 = '\t';
var C_SPACE$2 = ' ';
var C_GT = '>';

/* Tokenise a blockquote. */
function blockquote(eat, value, silent) {
  var self = this;
  var offsets = self.offset;
  var tokenizers = self.blockTokenizers;
  var interruptors = self.interruptBlockquote;
  var now = eat.now();
  var currentLine = now.line;
  var length = value.length;
  var values = [];
  var contents = [];
  var indents = [];
  var add;
  var index = 0;
  var character;
  var rest;
  var nextIndex;
  var content;
  var line;
  var startIndex;
  var prefixed;
  var exit;

  while (index < length) {
    character = value.charAt(index);

    if (character !== C_SPACE$2 && character !== C_TAB$2) {
      break;
    }

    index++;
  }

  if (value.charAt(index) !== C_GT) {
    return;
  }

  if (silent) {
    return true;
  }

  index = 0;

  while (index < length) {
    nextIndex = value.indexOf(C_NEWLINE$3, index);
    startIndex = index;
    prefixed = false;

    if (nextIndex === -1) {
      nextIndex = length;
    }

    while (index < length) {
      character = value.charAt(index);

      if (character !== C_SPACE$2 && character !== C_TAB$2) {
        break;
      }

      index++;
    }

    if (value.charAt(index) === C_GT) {
      index++;
      prefixed = true;

      if (value.charAt(index) === C_SPACE$2) {
        index++;
      }
    } else {
      index = startIndex;
    }

    content = value.slice(index, nextIndex);

    if (!prefixed && !trim_1(content)) {
      index = startIndex;
      break;
    }

    if (!prefixed) {
      rest = value.slice(index);

      /* Check if the following code contains a possible
       * block. */
      if (interrupt_1(interruptors, tokenizers, self, [eat, rest, true])) {
        break;
      }
    }

    line = startIndex === index ? content : value.slice(startIndex, nextIndex);

    indents.push(index - startIndex);
    values.push(line);
    contents.push(content);

    index = nextIndex + 1;
  }

  index = -1;
  length = indents.length;
  add = eat(values.join(C_NEWLINE$3));

  while (++index < length) {
    offsets[currentLine] = (offsets[currentLine] || 0) + indents[index];
    currentLine++;
  }

  exit = self.enterBlock();
  contents = self.tokenizeBlock(contents.join(C_NEWLINE$3), now);
  exit();

  return add({
    type: 'blockquote',
    children: contents
  });
}

var headingAtx = atxHeading;

var C_NEWLINE$4 = '\n';
var C_TAB$3 = '\t';
var C_SPACE$3 = ' ';
var C_HASH = '#';

var MAX_ATX_COUNT = 6;

function atxHeading(eat, value, silent) {
  var self = this;
  var settings = self.options;
  var length = value.length + 1;
  var index = -1;
  var now = eat.now();
  var subvalue = '';
  var content = '';
  var character;
  var queue;
  var depth;

  /* Eat initial spacing. */
  while (++index < length) {
    character = value.charAt(index);

    if (character !== C_SPACE$3 && character !== C_TAB$3) {
      index--;
      break;
    }

    subvalue += character;
  }

  /* Eat hashes. */
  depth = 0;

  while (++index <= length) {
    character = value.charAt(index);

    if (character !== C_HASH) {
      index--;
      break;
    }

    subvalue += character;
    depth++;
  }

  if (depth > MAX_ATX_COUNT) {
    return;
  }

  if (
    !depth ||
    (!settings.pedantic && value.charAt(index + 1) === C_HASH)
  ) {
    return;
  }

  length = value.length + 1;

  /* Eat intermediate white-space. */
  queue = '';

  while (++index < length) {
    character = value.charAt(index);

    if (character !== C_SPACE$3 && character !== C_TAB$3) {
      index--;
      break;
    }

    queue += character;
  }

  /* Exit when not in pedantic mode without spacing. */
  if (
    !settings.pedantic &&
    queue.length === 0 &&
    character &&
    character !== C_NEWLINE$4
  ) {
    return;
  }

  if (silent) {
    return true;
  }

  /* Eat content. */
  subvalue += queue;
  queue = '';
  content = '';

  while (++index < length) {
    character = value.charAt(index);

    if (!character || character === C_NEWLINE$4) {
      break;
    }

    if (
      character !== C_SPACE$3 &&
      character !== C_TAB$3 &&
      character !== C_HASH
    ) {
      content += queue + character;
      queue = '';
      continue;
    }

    while (character === C_SPACE$3 || character === C_TAB$3) {
      queue += character;
      character = value.charAt(++index);
    }

    while (character === C_HASH) {
      queue += character;
      character = value.charAt(++index);
    }

    while (character === C_SPACE$3 || character === C_TAB$3) {
      queue += character;
      character = value.charAt(++index);
    }

    index--;
  }

  now.column += subvalue.length;
  now.offset += subvalue.length;
  subvalue += content + queue;

  return eat(subvalue)({
    type: 'heading',
    depth: depth,
    children: self.tokenizeInline(content, now)
  });
}

var thematicBreak_1 = thematicBreak;

var C_NEWLINE$5 = '\n';
var C_TAB$4 = '\t';
var C_SPACE$4 = ' ';
var C_ASTERISK = '*';
var C_UNDERSCORE = '_';
var C_DASH = '-';

var THEMATIC_BREAK_MARKER_COUNT = 3;

function thematicBreak(eat, value, silent) {
  var index = -1;
  var length = value.length + 1;
  var subvalue = '';
  var character;
  var marker;
  var markerCount;
  var queue;

  while (++index < length) {
    character = value.charAt(index);

    if (character !== C_TAB$4 && character !== C_SPACE$4) {
      break;
    }

    subvalue += character;
  }

  if (
    character !== C_ASTERISK &&
    character !== C_DASH &&
    character !== C_UNDERSCORE
  ) {
    return;
  }

  marker = character;
  subvalue += character;
  markerCount = 1;
  queue = '';

  while (++index < length) {
    character = value.charAt(index);

    if (character === marker) {
      markerCount++;
      subvalue += queue + marker;
      queue = '';
    } else if (character === C_SPACE$4) {
      queue += character;
    } else if (
      markerCount >= THEMATIC_BREAK_MARKER_COUNT &&
      (!character || character === C_NEWLINE$5)
    ) {
      subvalue += queue;

      if (silent) {
        return true;
      }

      return eat(subvalue)({type: 'thematicBreak'});
    } else {
      return;
    }
  }
}

var getIndentation = indentation;

/* Map of characters, and their column length,
 * which can be used as indentation. */
var characters = {' ': 1, '\t': 4};

/* Gets indentation information for a line. */
function indentation(value) {
  var index = 0;
  var indent = 0;
  var character = value.charAt(index);
  var stops = {};
  var size;

  while (character in characters) {
    size = characters[character];

    indent += size;

    if (size > 1) {
      indent = Math.floor(indent / size) * size;
    }

    stops[indent] = index;

    character = value.charAt(++index);
  }

  return {indent: indent, stops: stops};
}

var removeIndentation = indentation$1;

var C_SPACE$5 = ' ';
var C_NEWLINE$6 = '\n';
var C_TAB$5 = '\t';

/* Remove the minimum indent from every line in `value`.
 * Supports both tab, spaced, and mixed indentation (as
 * well as possible). */
function indentation$1(value, maximum) {
  var values = value.split(C_NEWLINE$6);
  var position = values.length + 1;
  var minIndent = Infinity;
  var matrix = [];
  var index;
  var indentation;
  var stops;
  var padding;

  values.unshift(repeatString(C_SPACE$5, maximum) + '!');

  while (position--) {
    indentation = getIndentation(values[position]);

    matrix[position] = indentation.stops;

    if (trim_1(values[position]).length === 0) {
      continue;
    }

    if (indentation.indent) {
      if (indentation.indent > 0 && indentation.indent < minIndent) {
        minIndent = indentation.indent;
      }
    } else {
      minIndent = Infinity;

      break;
    }
  }

  if (minIndent !== Infinity) {
    position = values.length;

    while (position--) {
      stops = matrix[position];
      index = minIndent;

      while (index && !(index in stops)) {
        index--;
      }

      if (
        trim_1(values[position]).length !== 0 &&
        minIndent &&
        index !== minIndent
      ) {
        padding = C_TAB$5;
      } else {
        padding = '';
      }

      values[position] = padding + values[position].slice(
        index in stops ? stops[index] + 1 : 0
      );
    }
  }

  values.shift();

  return values.join(C_NEWLINE$6);
}

/* eslint-disable max-params */








var list_1 = list;

var C_ASTERISK$1 = '*';
var C_UNDERSCORE$1 = '_';
var C_PLUS = '+';
var C_DASH$1 = '-';
var C_DOT = '.';
var C_SPACE$6 = ' ';
var C_NEWLINE$7 = '\n';
var C_TAB$6 = '\t';
var C_PAREN_CLOSE = ')';
var C_X_LOWER = 'x';

var TAB_SIZE = 4;
var EXPRESSION_LOOSE_LIST_ITEM = /\n\n(?!\s*$)/;
var EXPRESSION_TASK_ITEM = /^\[([ \t]|x|X)][ \t]/;
var EXPRESSION_BULLET = /^([ \t]*)([*+-]|\d+[.)])( {1,4}(?! )| |\t|$|(?=\n))([^\n]*)/;
var EXPRESSION_PEDANTIC_BULLET = /^([ \t]*)([*+-]|\d+[.)])([ \t]+)/;
var EXPRESSION_INITIAL_INDENT = /^( {1,4}|\t)?/gm;

/* Map of characters which can be used to mark
 * list-items. */
var LIST_UNORDERED_MARKERS = {};

LIST_UNORDERED_MARKERS[C_ASTERISK$1] = true;
LIST_UNORDERED_MARKERS[C_PLUS] = true;
LIST_UNORDERED_MARKERS[C_DASH$1] = true;

/* Map of characters which can be used to mark
 * list-items after a digit. */
var LIST_ORDERED_MARKERS = {};

LIST_ORDERED_MARKERS[C_DOT] = true;

/* Map of characters which can be used to mark
 * list-items after a digit. */
var LIST_ORDERED_COMMONMARK_MARKERS = {};

LIST_ORDERED_COMMONMARK_MARKERS[C_DOT] = true;
LIST_ORDERED_COMMONMARK_MARKERS[C_PAREN_CLOSE] = true;

function list(eat, value, silent) {
  var self = this;
  var commonmark = self.options.commonmark;
  var pedantic = self.options.pedantic;
  var tokenizers = self.blockTokenizers;
  var interuptors = self.interruptList;
  var markers;
  var index = 0;
  var length = value.length;
  var start = null;
  var size = 0;
  var queue;
  var ordered;
  var character;
  var marker;
  var nextIndex;
  var startIndex;
  var prefixed;
  var currentMarker;
  var content;
  var line;
  var prevEmpty;
  var empty;
  var items;
  var allLines;
  var emptyLines;
  var item;
  var enterTop;
  var exitBlockquote;
  var isLoose;
  var node;
  var now;
  var end;
  var indented;

  while (index < length) {
    character = value.charAt(index);

    if (character === C_TAB$6) {
      size += TAB_SIZE - (size % TAB_SIZE);
    } else if (character === C_SPACE$6) {
      size++;
    } else {
      break;
    }

    index++;
  }

  if (size >= TAB_SIZE) {
    return;
  }

  character = value.charAt(index);

  markers = commonmark ?
    LIST_ORDERED_COMMONMARK_MARKERS :
    LIST_ORDERED_MARKERS;

  if (LIST_UNORDERED_MARKERS[character] === true) {
    marker = character;
    ordered = false;
  } else {
    ordered = true;
    queue = '';

    while (index < length) {
      character = value.charAt(index);

      if (!isDecimal(character)) {
        break;
      }

      queue += character;
      index++;
    }

    character = value.charAt(index);

    if (!queue || markers[character] !== true) {
      return;
    }

    start = parseInt(queue, 10);
    marker = character;
  }

  character = value.charAt(++index);

  if (character !== C_SPACE$6 && character !== C_TAB$6) {
    return;
  }

  if (silent) {
    return true;
  }

  index = 0;
  items = [];
  allLines = [];
  emptyLines = [];

  while (index < length) {
    nextIndex = value.indexOf(C_NEWLINE$7, index);
    startIndex = index;
    prefixed = false;
    indented = false;

    if (nextIndex === -1) {
      nextIndex = length;
    }

    end = index + TAB_SIZE;
    size = 0;

    while (index < length) {
      character = value.charAt(index);

      if (character === C_TAB$6) {
        size += TAB_SIZE - (size % TAB_SIZE);
      } else if (character === C_SPACE$6) {
        size++;
      } else {
        break;
      }

      index++;
    }

    if (size >= TAB_SIZE) {
      indented = true;
    }

    if (item && size >= item.indent) {
      indented = true;
    }

    character = value.charAt(index);
    currentMarker = null;

    if (!indented) {
      if (LIST_UNORDERED_MARKERS[character] === true) {
        currentMarker = character;
        index++;
        size++;
      } else {
        queue = '';

        while (index < length) {
          character = value.charAt(index);

          if (!isDecimal(character)) {
            break;
          }

          queue += character;
          index++;
        }

        character = value.charAt(index);
        index++;

        if (queue && markers[character] === true) {
          currentMarker = character;
          size += queue.length + 1;
        }
      }

      if (currentMarker) {
        character = value.charAt(index);

        if (character === C_TAB$6) {
          size += TAB_SIZE - (size % TAB_SIZE);
          index++;
        } else if (character === C_SPACE$6) {
          end = index + TAB_SIZE;

          while (index < end) {
            if (value.charAt(index) !== C_SPACE$6) {
              break;
            }

            index++;
            size++;
          }

          if (index === end && value.charAt(index) === C_SPACE$6) {
            index -= TAB_SIZE - 1;
            size -= TAB_SIZE - 1;
          }
        } else if (character !== C_NEWLINE$7 && character !== '') {
          currentMarker = null;
        }
      }
    }

    if (currentMarker) {
      if (!pedantic && marker !== currentMarker) {
        break;
      }

      prefixed = true;
    } else {
      if (!commonmark && !indented && value.charAt(startIndex) === C_SPACE$6) {
        indented = true;
      } else if (commonmark && item) {
        indented = size >= item.indent || size > TAB_SIZE;
      }

      prefixed = false;
      index = startIndex;
    }

    line = value.slice(startIndex, nextIndex);
    content = startIndex === index ? line : value.slice(index, nextIndex);

    if (
      currentMarker === C_ASTERISK$1 ||
      currentMarker === C_UNDERSCORE$1 ||
      currentMarker === C_DASH$1
    ) {
      if (tokenizers.thematicBreak.call(self, eat, line, true)) {
        break;
      }
    }

    prevEmpty = empty;
    empty = !trim_1(content).length;

    if (indented && item) {
      item.value = item.value.concat(emptyLines, line);
      allLines = allLines.concat(emptyLines, line);
      emptyLines = [];
    } else if (prefixed) {
      if (emptyLines.length !== 0) {
        item.value.push('');
        item.trail = emptyLines.concat();
      }

      item = {
        value: [line],
        indent: size,
        trail: []
      };

      items.push(item);
      allLines = allLines.concat(emptyLines, line);
      emptyLines = [];
    } else if (empty) {
      if (prevEmpty) {
        break;
      }

      emptyLines.push(line);
    } else {
      if (prevEmpty) {
        break;
      }

      if (interrupt_1(interuptors, tokenizers, self, [eat, line, true])) {
        break;
      }

      item.value = item.value.concat(emptyLines, line);
      allLines = allLines.concat(emptyLines, line);
      emptyLines = [];
    }

    index = nextIndex + 1;
  }

  node = eat(allLines.join(C_NEWLINE$7)).reset({
    type: 'list',
    ordered: ordered,
    start: start,
    loose: null,
    children: []
  });

  enterTop = self.enterList();
  exitBlockquote = self.enterBlock();
  isLoose = false;
  index = -1;
  length = items.length;

  while (++index < length) {
    item = items[index].value.join(C_NEWLINE$7);
    now = eat.now();

    item = eat(item)(listItem(self, item, now), node);

    if (item.loose) {
      isLoose = true;
    }

    item = items[index].trail.join(C_NEWLINE$7);

    if (index !== length - 1) {
      item += C_NEWLINE$7;
    }

    eat(item);
  }

  enterTop();
  exitBlockquote();

  node.loose = isLoose;

  return node;
}

function listItem(ctx, value, position) {
  var offsets = ctx.offset;
  var fn = ctx.options.pedantic ? pedanticListItem : normalListItem;
  var checked = null;
  var task;
  var indent;

  value = fn.apply(null, arguments);

  if (ctx.options.gfm) {
    task = value.match(EXPRESSION_TASK_ITEM);

    if (task) {
      indent = task[0].length;
      checked = task[1].toLowerCase() === C_X_LOWER;
      offsets[position.line] += indent;
      value = value.slice(indent);
    }
  }

  return {
    type: 'listItem',
    loose: EXPRESSION_LOOSE_LIST_ITEM.test(value) ||
      value.charAt(value.length - 1) === C_NEWLINE$7,
    checked: checked,
    children: ctx.tokenizeBlock(value, position)
  };
}

/* Create a list-item using overly simple mechanics. */
function pedanticListItem(ctx, value, position) {
  var offsets = ctx.offset;
  var line = position.line;

  /* Remove the list-item’s bullet. */
  value = value.replace(EXPRESSION_PEDANTIC_BULLET, replacer);

  /* The initial line was also matched by the below, so
   * we reset the `line`. */
  line = position.line;

  return value.replace(EXPRESSION_INITIAL_INDENT, replacer);

  /* A simple replacer which removed all matches,
   * and adds their length to `offset`. */
  function replacer($0) {
    offsets[line] = (offsets[line] || 0) + $0.length;
    line++;

    return '';
  }
}

/* Create a list-item using sane mechanics. */
function normalListItem(ctx, value, position) {
  var offsets = ctx.offset;
  var line = position.line;
  var max;
  var bullet;
  var rest;
  var lines;
  var trimmedLines;
  var index;
  var length;

  /* Remove the list-item’s bullet. */
  value = value.replace(EXPRESSION_BULLET, replacer);

  lines = value.split(C_NEWLINE$7);

  trimmedLines = removeIndentation(value, getIndentation(max).indent).split(C_NEWLINE$7);

  /* We replaced the initial bullet with something
   * else above, which was used to trick
   * `removeIndentation` into removing some more
   * characters when possible.  However, that could
   * result in the initial line to be stripped more
   * than it should be. */
  trimmedLines[0] = rest;

  offsets[line] = (offsets[line] || 0) + bullet.length;
  line++;

  index = 0;
  length = lines.length;

  while (++index < length) {
    offsets[line] = (offsets[line] || 0) +
      lines[index].length - trimmedLines[index].length;
    line++;
  }

  return trimmedLines.join(C_NEWLINE$7);

  function replacer($0, $1, $2, $3, $4) {
    bullet = $1 + $2 + $3;
    rest = $4;

    /* Make sure that the first nine numbered list items
     * can indent with an extra space.  That is, when
     * the bullet did not receive an extra final space. */
    if (Number($2) < 10 && bullet.length % 2 === 1) {
      $2 = C_SPACE$6 + $2;
    }

    max = $1 + repeatString(C_SPACE$6, $2.length) + $3;

    return max + rest;
  }
}

var headingSetext = setextHeading;

var C_NEWLINE$8 = '\n';
var C_TAB$7 = '\t';
var C_SPACE$7 = ' ';
var C_EQUALS = '=';
var C_DASH$2 = '-';

var MAX_HEADING_INDENT = 3;

/* Map of characters which can be used to mark setext
 * headers, mapping to their corresponding depth. */
var SETEXT_MARKERS = {};

SETEXT_MARKERS[C_EQUALS] = 1;
SETEXT_MARKERS[C_DASH$2] = 2;

function setextHeading(eat, value, silent) {
  var self = this;
  var now = eat.now();
  var length = value.length;
  var index = -1;
  var subvalue = '';
  var content;
  var queue;
  var character;
  var marker;
  var depth;

  /* Eat initial indentation. */
  while (++index < length) {
    character = value.charAt(index);

    if (character !== C_SPACE$7 || index >= MAX_HEADING_INDENT) {
      index--;
      break;
    }

    subvalue += character;
  }

  /* Eat content. */
  content = '';
  queue = '';

  while (++index < length) {
    character = value.charAt(index);

    if (character === C_NEWLINE$8) {
      index--;
      break;
    }

    if (character === C_SPACE$7 || character === C_TAB$7) {
      queue += character;
    } else {
      content += queue + character;
      queue = '';
    }
  }

  now.column += subvalue.length;
  now.offset += subvalue.length;
  subvalue += content + queue;

  /* Ensure the content is followed by a newline and a
   * valid marker. */
  character = value.charAt(++index);
  marker = value.charAt(++index);

  if (character !== C_NEWLINE$8 || !SETEXT_MARKERS[marker]) {
    return;
  }

  subvalue += character;

  /* Eat Setext-line. */
  queue = marker;
  depth = SETEXT_MARKERS[marker];

  while (++index < length) {
    character = value.charAt(index);

    if (character !== marker) {
      if (character !== C_NEWLINE$8) {
        return;
      }

      index--;
      break;
    }

    queue += character;
  }

  if (silent) {
    return true;
  }

  return eat(subvalue + queue)({
    type: 'heading',
    depth: depth,
    children: self.tokenizeInline(content, now)
  });
}

var attributeName = '[a-zA-Z_:][a-zA-Z0-9:._-]*';
var unquoted = '[^"\'=<>`\\u0000-\\u0020]+';
var singleQuoted = '\'[^\']*\'';
var doubleQuoted = '"[^"]*"';
var attributeValue = '(?:' + unquoted + '|' + singleQuoted + '|' + doubleQuoted + ')';
var attribute = '(?:\\s+' + attributeName + '(?:\\s*=\\s*' + attributeValue + ')?)';
var openTag = '<[A-Za-z][A-Za-z0-9\\-]*' + attribute + '*\\s*\\/?>';
var closeTag = '<\\/[A-Za-z][A-Za-z0-9\\-]*\\s*>';
var comment = '<!---->|<!--(?:-?[^>-])(?:-?[^-])*-->';
var processing = '<[?].*?[?]>';
var declaration = '<![A-Za-z]+\\s+[^>]*>';
var cdata = '<!\\[CDATA\\[[\\s\\S]*?\\]\\]>';

var openCloseTag = new RegExp('^(?:' + openTag + '|' + closeTag + ')');

var tag = new RegExp('^(?:' +
  openTag + '|' +
  closeTag + '|' +
  comment + '|' +
  processing + '|' +
  declaration + '|' +
  cdata +
')');

var html = {
	openCloseTag: openCloseTag,
	tag: tag
};

var openCloseTag$1 = html.openCloseTag;

var htmlBlock = blockHTML;

var C_TAB$8 = '\t';
var C_SPACE$8 = ' ';
var C_NEWLINE$9 = '\n';
var C_LT = '<';

function blockHTML(eat, value, silent) {
  var self = this;
  var blocks = self.options.blocks;
  var length = value.length;
  var index = 0;
  var next;
  var line;
  var offset;
  var character;
  var count;
  var sequence;
  var subvalue;

  var sequences = [
    [/^<(script|pre|style)(?=(\s|>|$))/i, /<\/(script|pre|style)>/i, true],
    [/^<!--/, /-->/, true],
    [/^<\?/, /\?>/, true],
    [/^<![A-Za-z]/, />/, true],
    [/^<!\[CDATA\[/, /\]\]>/, true],
    [new RegExp('^</?(' + blocks.join('|') + ')(?=(\\s|/?>|$))', 'i'), /^$/, true],
    [new RegExp(openCloseTag$1.source + '\\s*$'), /^$/, false]
  ];

  /* Eat initial spacing. */
  while (index < length) {
    character = value.charAt(index);

    if (character !== C_TAB$8 && character !== C_SPACE$8) {
      break;
    }

    index++;
  }

  if (value.charAt(index) !== C_LT) {
    return;
  }

  next = value.indexOf(C_NEWLINE$9, index + 1);
  next = next === -1 ? length : next;
  line = value.slice(index, next);
  offset = -1;
  count = sequences.length;

  while (++offset < count) {
    if (sequences[offset][0].test(line)) {
      sequence = sequences[offset];
      break;
    }
  }

  if (!sequence) {
    return;
  }

  if (silent) {
    return sequence[2];
  }

  index = next;

  if (!sequence[1].test(line)) {
    while (index < length) {
      next = value.indexOf(C_NEWLINE$9, index + 1);
      next = next === -1 ? length : next;
      line = value.slice(index + 1, next);

      if (sequence[1].test(line)) {
        if (line) {
          index = next;
        }

        break;
      }

      index = next;
    }
  }

  subvalue = value.slice(0, index);

  return eat(subvalue)({type: 'html', value: subvalue});
}

var collapseWhiteSpace = collapse;

/* collapse(' \t\nbar \nbaz\t'); // ' bar baz ' */
function collapse(value) {
  return String(value).replace(/\s+/g, ' ')
}

var normalize_1 = normalize$2;

/* Normalize an identifier.  Collapses multiple white space
 * characters into a single space, and removes casing. */
function normalize$2(value) {
  return collapseWhiteSpace(value).toLowerCase();
}

var footnoteDefinition_1 = footnoteDefinition;
footnoteDefinition.notInList = true;
footnoteDefinition.notInBlock = true;

var C_BACKSLASH = '\\';
var C_NEWLINE$10 = '\n';
var C_TAB$9 = '\t';
var C_SPACE$9 = ' ';
var C_BRACKET_OPEN = '[';
var C_BRACKET_CLOSE = ']';
var C_CARET = '^';
var C_COLON = ':';

var EXPRESSION_INITIAL_TAB = /^( {4}|\t)?/gm;

function footnoteDefinition(eat, value, silent) {
  var self = this;
  var offsets = self.offset;
  var index;
  var length;
  var subvalue;
  var now;
  var currentLine;
  var content;
  var queue;
  var subqueue;
  var character;
  var identifier;
  var add;
  var exit;

  if (!self.options.footnotes) {
    return;
  }

  index = 0;
  length = value.length;
  subvalue = '';
  now = eat.now();
  currentLine = now.line;

  while (index < length) {
    character = value.charAt(index);

    if (!isWhitespaceCharacter(character)) {
      break;
    }

    subvalue += character;
    index++;
  }

  if (
    value.charAt(index) !== C_BRACKET_OPEN ||
    value.charAt(index + 1) !== C_CARET
  ) {
    return;
  }

  subvalue += C_BRACKET_OPEN + C_CARET;
  index = subvalue.length;
  queue = '';

  while (index < length) {
    character = value.charAt(index);

    if (character === C_BRACKET_CLOSE) {
      break;
    } else if (character === C_BACKSLASH) {
      queue += character;
      index++;
      character = value.charAt(index);
    }

    queue += character;
    index++;
  }

  if (
    !queue ||
    value.charAt(index) !== C_BRACKET_CLOSE ||
    value.charAt(index + 1) !== C_COLON
  ) {
    return;
  }

  if (silent) {
    return true;
  }

  identifier = normalize_1(queue);
  subvalue += queue + C_BRACKET_CLOSE + C_COLON;
  index = subvalue.length;

  while (index < length) {
    character = value.charAt(index);

    if (character !== C_TAB$9 && character !== C_SPACE$9) {
      break;
    }

    subvalue += character;
    index++;
  }

  now.column += subvalue.length;
  now.offset += subvalue.length;
  queue = '';
  content = '';
  subqueue = '';

  while (index < length) {
    character = value.charAt(index);

    if (character === C_NEWLINE$10) {
      subqueue = character;
      index++;

      while (index < length) {
        character = value.charAt(index);

        if (character !== C_NEWLINE$10) {
          break;
        }

        subqueue += character;
        index++;
      }

      queue += subqueue;
      subqueue = '';

      while (index < length) {
        character = value.charAt(index);

        if (character !== C_SPACE$9) {
          break;
        }

        subqueue += character;
        index++;
      }

      if (subqueue.length === 0) {
        break;
      }

      queue += subqueue;
    }

    if (queue) {
      content += queue;
      queue = '';
    }

    content += character;
    index++;
  }

  subvalue += content;

  content = content.replace(EXPRESSION_INITIAL_TAB, function (line) {
    offsets[currentLine] = (offsets[currentLine] || 0) + line.length;
    currentLine++;

    return '';
  });

  add = eat(subvalue);

  exit = self.enterBlock();
  content = self.tokenizeBlock(content, now);
  exit();

  return add({
    type: 'footnoteDefinition',
    identifier: identifier,
    children: content
  });
}

var definition_1 = definition;
definition.notInList = true;
definition.notInBlock = true;

var C_DOUBLE_QUOTE = '"';
var C_SINGLE_QUOTE = '\'';
var C_BACKSLASH$1 = '\\';
var C_NEWLINE$11 = '\n';
var C_TAB$10 = '\t';
var C_SPACE$10 = ' ';
var C_BRACKET_OPEN$1 = '[';
var C_BRACKET_CLOSE$1 = ']';
var C_PAREN_OPEN = '(';
var C_PAREN_CLOSE$1 = ')';
var C_COLON$1 = ':';
var C_LT$1 = '<';
var C_GT$1 = '>';

function definition(eat, value, silent) {
  var self = this;
  var commonmark = self.options.commonmark;
  var index = 0;
  var length = value.length;
  var subvalue = '';
  var beforeURL;
  var beforeTitle;
  var queue;
  var character;
  var test;
  var identifier;
  var url;
  var title;

  while (index < length) {
    character = value.charAt(index);

    if (character !== C_SPACE$10 && character !== C_TAB$10) {
      break;
    }

    subvalue += character;
    index++;
  }

  character = value.charAt(index);

  if (character !== C_BRACKET_OPEN$1) {
    return;
  }

  index++;
  subvalue += character;
  queue = '';

  while (index < length) {
    character = value.charAt(index);

    if (character === C_BRACKET_CLOSE$1) {
      break;
    } else if (character === C_BACKSLASH$1) {
      queue += character;
      index++;
      character = value.charAt(index);
    }

    queue += character;
    index++;
  }

  if (
    !queue ||
    value.charAt(index) !== C_BRACKET_CLOSE$1 ||
    value.charAt(index + 1) !== C_COLON$1
  ) {
    return;
  }

  identifier = queue;
  subvalue += queue + C_BRACKET_CLOSE$1 + C_COLON$1;
  index = subvalue.length;
  queue = '';

  while (index < length) {
    character = value.charAt(index);

    if (
      character !== C_TAB$10 &&
      character !== C_SPACE$10 &&
      character !== C_NEWLINE$11
    ) {
      break;
    }

    subvalue += character;
    index++;
  }

  character = value.charAt(index);
  queue = '';
  beforeURL = subvalue;

  if (character === C_LT$1) {
    index++;

    while (index < length) {
      character = value.charAt(index);

      if (!isEnclosedURLCharacter(character)) {
        break;
      }

      queue += character;
      index++;
    }

    character = value.charAt(index);

    if (character === isEnclosedURLCharacter.delimiter) {
      subvalue += C_LT$1 + queue + character;
      index++;
    } else {
      if (commonmark) {
        return;
      }

      index -= queue.length + 1;
      queue = '';
    }
  }

  if (!queue) {
    while (index < length) {
      character = value.charAt(index);

      if (!isUnclosedURLCharacter(character)) {
        break;
      }

      queue += character;
      index++;
    }

    subvalue += queue;
  }

  if (!queue) {
    return;
  }

  url = queue;
  queue = '';

  while (index < length) {
    character = value.charAt(index);

    if (
      character !== C_TAB$10 &&
      character !== C_SPACE$10 &&
      character !== C_NEWLINE$11
    ) {
      break;
    }

    queue += character;
    index++;
  }

  character = value.charAt(index);
  test = null;

  if (character === C_DOUBLE_QUOTE) {
    test = C_DOUBLE_QUOTE;
  } else if (character === C_SINGLE_QUOTE) {
    test = C_SINGLE_QUOTE;
  } else if (character === C_PAREN_OPEN) {
    test = C_PAREN_CLOSE$1;
  }

  if (!test) {
    queue = '';
    index = subvalue.length;
  } else if (queue) {
    subvalue += queue + character;
    index = subvalue.length;
    queue = '';

    while (index < length) {
      character = value.charAt(index);

      if (character === test) {
        break;
      }

      if (character === C_NEWLINE$11) {
        index++;
        character = value.charAt(index);

        if (character === C_NEWLINE$11 || character === test) {
          return;
        }

        queue += C_NEWLINE$11;
      }

      queue += character;
      index++;
    }

    character = value.charAt(index);

    if (character !== test) {
      return;
    }

    beforeTitle = subvalue;
    subvalue += queue + character;
    index++;
    title = queue;
    queue = '';
  } else {
    return;
  }

  while (index < length) {
    character = value.charAt(index);

    if (character !== C_TAB$10 && character !== C_SPACE$10) {
      break;
    }

    subvalue += character;
    index++;
  }

  character = value.charAt(index);

  if (!character || character === C_NEWLINE$11) {
    if (silent) {
      return true;
    }

    beforeURL = eat(beforeURL).test().end;
    url = self.decode.raw(self.unescape(url), beforeURL);

    if (title) {
      beforeTitle = eat(beforeTitle).test().end;
      title = self.decode.raw(self.unescape(title), beforeTitle);
    }

    return eat(subvalue)({
      type: 'definition',
      identifier: normalize_1(identifier),
      title: title || null,
      url: url
    });
  }
}

/* Check if `character` can be inside an enclosed URI. */
function isEnclosedURLCharacter(character) {
  return character !== C_GT$1 &&
    character !== C_BRACKET_OPEN$1 &&
    character !== C_BRACKET_CLOSE$1;
}

isEnclosedURLCharacter.delimiter = C_GT$1;

/* Check if `character` can be inside an unclosed URI. */
function isUnclosedURLCharacter(character) {
  return character !== C_BRACKET_OPEN$1 &&
    character !== C_BRACKET_CLOSE$1 &&
    !isWhitespaceCharacter(character);
}

var table_1 = table$1;

var C_BACKSLASH$2 = '\\';
var C_TICK$1 = '`';
var C_DASH$3 = '-';
var C_PIPE = '|';
var C_COLON$2 = ':';
var C_SPACE$11 = ' ';
var C_NEWLINE$12 = '\n';
var C_TAB$11 = '\t';

var MIN_TABLE_COLUMNS = 1;
var MIN_TABLE_ROWS = 2;

var TABLE_ALIGN_LEFT = 'left';
var TABLE_ALIGN_CENTER = 'center';
var TABLE_ALIGN_RIGHT = 'right';
var TABLE_ALIGN_NONE = null;

function table$1(eat, value, silent) {
  var self = this;
  var index;
  var alignments;
  var alignment;
  var subvalue;
  var row;
  var length;
  var lines;
  var queue;
  var character;
  var hasDash;
  var align;
  var cell;
  var preamble;
  var count;
  var opening;
  var now;
  var position;
  var lineCount;
  var line;
  var rows;
  var table;
  var lineIndex;
  var pipeIndex;
  var first;

  /* Exit when not in gfm-mode. */
  if (!self.options.gfm) {
    return;
  }

  /* Get the rows.
   * Detecting tables soon is hard, so there are some
   * checks for performance here, such as the minimum
   * number of rows, and allowed characters in the
   * alignment row. */
  index = 0;
  lineCount = 0;
  length = value.length + 1;
  lines = [];

  while (index < length) {
    lineIndex = value.indexOf(C_NEWLINE$12, index);
    pipeIndex = value.indexOf(C_PIPE, index + 1);

    if (lineIndex === -1) {
      lineIndex = value.length;
    }

    if (pipeIndex === -1 || pipeIndex > lineIndex) {
      if (lineCount < MIN_TABLE_ROWS) {
        return;
      }

      break;
    }

    lines.push(value.slice(index, lineIndex));
    lineCount++;
    index = lineIndex + 1;
  }

  /* Parse the alignment row. */
  subvalue = lines.join(C_NEWLINE$12);
  alignments = lines.splice(1, 1)[0] || [];
  index = 0;
  length = alignments.length;
  lineCount--;
  alignment = false;
  align = [];

  while (index < length) {
    character = alignments.charAt(index);

    if (character === C_PIPE) {
      hasDash = null;

      if (alignment === false) {
        if (first === false) {
          return;
        }
      } else {
        align.push(alignment);
        alignment = false;
      }

      first = false;
    } else if (character === C_DASH$3) {
      hasDash = true;
      alignment = alignment || TABLE_ALIGN_NONE;
    } else if (character === C_COLON$2) {
      if (alignment === TABLE_ALIGN_LEFT) {
        alignment = TABLE_ALIGN_CENTER;
      } else if (hasDash && alignment === TABLE_ALIGN_NONE) {
        alignment = TABLE_ALIGN_RIGHT;
      } else {
        alignment = TABLE_ALIGN_LEFT;
      }
    } else if (!isWhitespaceCharacter(character)) {
      return;
    }

    index++;
  }

  if (alignment !== false) {
    align.push(alignment);
  }

  /* Exit when without enough columns. */
  if (align.length < MIN_TABLE_COLUMNS) {
    return;
  }

  /* istanbul ignore if - never used (yet) */
  if (silent) {
    return true;
  }

  /* Parse the rows. */
  position = -1;
  rows = [];

  table = eat(subvalue).reset({
    type: 'table',
    align: align,
    children: rows
  });

  while (++position < lineCount) {
    line = lines[position];
    row = {type: 'tableRow', children: []};

    /* Eat a newline character when this is not the
     * first row. */
    if (position) {
      eat(C_NEWLINE$12);
    }

    /* Eat the row. */
    eat(line).reset(row, table);

    length = line.length + 1;
    index = 0;
    queue = '';
    cell = '';
    preamble = true;
    count = null;
    opening = null;

    while (index < length) {
      character = line.charAt(index);

      if (character === C_TAB$11 || character === C_SPACE$11) {
        if (cell) {
          queue += character;
        } else {
          eat(character);
        }

        index++;
        continue;
      }

      if (character === '' || character === C_PIPE) {
        if (preamble) {
          eat(character);
        } else {
          if (character && opening) {
            queue += character;
            index++;
            continue;
          }

          if ((cell || character) && !preamble) {
            subvalue = cell;

            if (queue.length > 1) {
              if (character) {
                subvalue += queue.slice(0, queue.length - 1);
                queue = queue.charAt(queue.length - 1);
              } else {
                subvalue += queue;
                queue = '';
              }
            }

            now = eat.now();

            eat(subvalue)({
              type: 'tableCell',
              children: self.tokenizeInline(cell, now)
            }, row);
          }

          eat(queue + character);

          queue = '';
          cell = '';
        }
      } else {
        if (queue) {
          cell += queue;
          queue = '';
        }

        cell += character;

        if (character === C_BACKSLASH$2 && index !== length - 2) {
          cell += line.charAt(index + 1);
          index++;
        }

        if (character === C_TICK$1) {
          count = 1;

          while (line.charAt(index + 1) === character) {
            cell += character;
            index++;
            count++;
          }

          if (!opening) {
            opening = count;
          } else if (count >= opening) {
            opening = 0;
          }
        }
      }

      preamble = false;
      index++;
    }

    /* Eat the alignment row. */
    if (!position) {
      eat(C_NEWLINE$12 + alignments);
    }
  }

  return table;
}

var paragraph_1 = paragraph;

var C_NEWLINE$13 = '\n';
var C_TAB$12 = '\t';
var C_SPACE$12 = ' ';

var TAB_SIZE$1 = 4;

/* Tokenise paragraph. */
function paragraph(eat, value, silent) {
  var self = this;
  var settings = self.options;
  var commonmark = settings.commonmark;
  var gfm = settings.gfm;
  var tokenizers = self.blockTokenizers;
  var interruptors = self.interruptParagraph;
  var index = value.indexOf(C_NEWLINE$13);
  var length = value.length;
  var position;
  var subvalue;
  var character;
  var size;
  var now;

  while (index < length) {
    /* Eat everything if there’s no following newline. */
    if (index === -1) {
      index = length;
      break;
    }

    /* Stop if the next character is NEWLINE. */
    if (value.charAt(index + 1) === C_NEWLINE$13) {
      break;
    }

    /* In commonmark-mode, following indented lines
     * are part of the paragraph. */
    if (commonmark) {
      size = 0;
      position = index + 1;

      while (position < length) {
        character = value.charAt(position);

        if (character === C_TAB$12) {
          size = TAB_SIZE$1;
          break;
        } else if (character === C_SPACE$12) {
          size++;
        } else {
          break;
        }

        position++;
      }

      if (size >= TAB_SIZE$1) {
        index = value.indexOf(C_NEWLINE$13, index + 1);
        continue;
      }
    }

    subvalue = value.slice(index + 1);

    /* Check if the following code contains a possible
     * block. */
    if (interrupt_1(interruptors, tokenizers, self, [eat, subvalue, true])) {
      break;
    }

    /* Break if the following line starts a list, when
     * already in a list, or when in commonmark, or when
     * in gfm mode and the bullet is *not* numeric. */
    if (
      tokenizers.list.call(self, eat, subvalue, true) &&
      (
        self.inList ||
        commonmark ||
        (gfm && !isDecimal(trim_1.left(subvalue).charAt(0)))
      )
    ) {
      break;
    }

    position = index;
    index = value.indexOf(C_NEWLINE$13, index + 1);

    if (index !== -1 && trim_1(value.slice(position, index)) === '') {
      index = position;
      break;
    }
  }

  subvalue = value.slice(0, index);

  if (trim_1(subvalue) === '') {
    eat(subvalue);

    return null;
  }

  /* istanbul ignore if - never used (yet) */
  if (silent) {
    return true;
  }

  now = eat.now();
  subvalue = trimTrailingLines_1(subvalue);

  return eat(subvalue)({
    type: 'paragraph',
    children: self.tokenizeInline(subvalue, now)
  });
}

var _escape = locate;

function locate(value, fromIndex) {
  return value.indexOf('\\', fromIndex);
}

var _escape$2 = escape;
escape.locator = _escape;

function escape(eat, value, silent) {
  var self = this;
  var character;
  var node;

  if (value.charAt(0) === '\\') {
    character = value.charAt(1);

    if (self.escape.indexOf(character) !== -1) {
      /* istanbul ignore if - never used (yet) */
      if (silent) {
        return true;
      }

      if (character === '\n') {
        node = {type: 'break'};
      } else {
        node = {
          type: 'text',
          value: character
        };
      }

      return eat('\\' + character)(node);
    }
  }
}

var tag$1 = locate$2;

function locate$2(value, fromIndex) {
  return value.indexOf('<', fromIndex);
}

var autoLink_1 = autoLink;
autoLink.locator = tag$1;
autoLink.notInLink = true;

var C_LT$2 = '<';
var C_GT$2 = '>';
var C_AT_SIGN = '@';
var C_SLASH = '/';
var MAILTO = 'mailto:';
var MAILTO_LENGTH = MAILTO.length;

/* Tokenise a link. */
function autoLink(eat, value, silent) {
  var self;
  var subvalue;
  var length;
  var index;
  var queue;
  var character;
  var hasAtCharacter;
  var link;
  var now;
  var content;
  var tokenize;
  var exit;

  if (value.charAt(0) !== C_LT$2) {
    return;
  }

  self = this;
  subvalue = '';
  length = value.length;
  index = 0;
  queue = '';
  hasAtCharacter = false;
  link = '';

  index++;
  subvalue = C_LT$2;

  while (index < length) {
    character = value.charAt(index);

    if (
      isWhitespaceCharacter(character) ||
      character === C_GT$2 ||
      character === C_AT_SIGN ||
      (character === ':' && value.charAt(index + 1) === C_SLASH)
    ) {
      break;
    }

    queue += character;
    index++;
  }

  if (!queue) {
    return;
  }

  link += queue;
  queue = '';

  character = value.charAt(index);
  link += character;
  index++;

  if (character === C_AT_SIGN) {
    hasAtCharacter = true;
  } else {
    if (
      character !== ':' ||
      value.charAt(index + 1) !== C_SLASH
    ) {
      return;
    }

    link += C_SLASH;
    index++;
  }

  while (index < length) {
    character = value.charAt(index);

    if (isWhitespaceCharacter(character) || character === C_GT$2) {
      break;
    }

    queue += character;
    index++;
  }

  character = value.charAt(index);

  if (!queue || character !== C_GT$2) {
    return;
  }

  /* istanbul ignore if - never used (yet) */
  if (silent) {
    return true;
  }

  link += queue;
  content = link;
  subvalue += link + character;
  now = eat.now();
  now.column++;
  now.offset++;

  if (hasAtCharacter) {
    if (link.slice(0, MAILTO_LENGTH).toLowerCase() === MAILTO) {
      content = content.substr(MAILTO_LENGTH);
      now.column += MAILTO_LENGTH;
      now.offset += MAILTO_LENGTH;
    } else {
      link = MAILTO + link;
    }
  }

  /* Temporarily remove support for escapes in autolinks. */
  tokenize = self.inlineTokenizers.escape;
  self.inlineTokenizers.escape = null;
  exit = self.enterLink();

  content = self.tokenizeInline(content, now);

  self.inlineTokenizers.escape = tokenize;
  exit();

  return eat(subvalue)({
    type: 'link',
    title: null,
    url: parseEntities_1(link),
    children: content
  });
}

var url = locate$4;

var PROTOCOLS = ['https://', 'http://', 'mailto:'];

function locate$4(value, fromIndex) {
  var length = PROTOCOLS.length;
  var index = -1;
  var min = -1;
  var position;

  if (!this.options.gfm) {
    return -1;
  }

  while (++index < length) {
    position = value.indexOf(PROTOCOLS[index], fromIndex);

    if (position !== -1 && (position < min || min === -1)) {
      min = position;
    }
  }

  return min;
}

var url_1 = url$2;
url$2.locator = url;
url$2.notInLink = true;

var C_BRACKET_OPEN$2 = '[';
var C_BRACKET_CLOSE$2 = ']';
var C_PAREN_OPEN$1 = '(';
var C_PAREN_CLOSE$2 = ')';
var C_LT$3 = '<';
var C_AT_SIGN$1 = '@';

var HTTP_PROTOCOL = 'http://';
var HTTPS_PROTOCOL = 'https://';
var MAILTO_PROTOCOL = 'mailto:';

var PROTOCOLS$1 = [
  HTTP_PROTOCOL,
  HTTPS_PROTOCOL,
  MAILTO_PROTOCOL
];

var PROTOCOLS_LENGTH = PROTOCOLS$1.length;

function url$2(eat, value, silent) {
  var self = this;
  var subvalue;
  var content;
  var character;
  var index;
  var position;
  var protocol;
  var match;
  var length;
  var queue;
  var parenCount;
  var nextCharacter;
  var exit;

  if (!self.options.gfm) {
    return;
  }

  subvalue = '';
  index = -1;
  length = PROTOCOLS_LENGTH;

  while (++index < length) {
    protocol = PROTOCOLS$1[index];
    match = value.slice(0, protocol.length);

    if (match.toLowerCase() === protocol) {
      subvalue = match;
      break;
    }
  }

  if (!subvalue) {
    return;
  }

  index = subvalue.length;
  length = value.length;
  queue = '';
  parenCount = 0;

  while (index < length) {
    character = value.charAt(index);

    if (isWhitespaceCharacter(character) || character === C_LT$3) {
      break;
    }

    if (
      character === '.' ||
      character === ',' ||
      character === ':' ||
      character === ';' ||
      character === '"' ||
      character === '\'' ||
      character === ')' ||
      character === ']'
    ) {
      nextCharacter = value.charAt(index + 1);

      if (!nextCharacter || isWhitespaceCharacter(nextCharacter)) {
        break;
      }
    }

    if (character === C_PAREN_OPEN$1 || character === C_BRACKET_OPEN$2) {
      parenCount++;
    }

    if (character === C_PAREN_CLOSE$2 || character === C_BRACKET_CLOSE$2) {
      parenCount--;

      if (parenCount < 0) {
        break;
      }
    }

    queue += character;
    index++;
  }

  if (!queue) {
    return;
  }

  subvalue += queue;
  content = subvalue;

  if (protocol === MAILTO_PROTOCOL) {
    position = queue.indexOf(C_AT_SIGN$1);

    if (position === -1 || position === length - 1) {
      return;
    }

    content = content.substr(MAILTO_PROTOCOL.length);
  }

  /* istanbul ignore if - never used (yet) */
  if (silent) {
    return true;
  }

  exit = self.enterLink();
  content = self.tokenizeInline(content, eat.now());
  exit();

  return eat(subvalue)({
    type: 'link',
    title: null,
    url: parseEntities_1(subvalue),
    children: content
  });
}

var tag$3 = html.tag;

var htmlInline = inlineHTML;
inlineHTML.locator = tag$1;

var EXPRESSION_HTML_LINK_OPEN = /^<a /i;
var EXPRESSION_HTML_LINK_CLOSE = /^<\/a>/i;

function inlineHTML(eat, value, silent) {
  var self = this;
  var length = value.length;
  var character;
  var subvalue;

  if (value.charAt(0) !== '<' || length < 3) {
    return;
  }

  character = value.charAt(1);

  if (
    !isAlphabetical(character) &&
    character !== '?' &&
    character !== '!' &&
    character !== '/'
  ) {
    return;
  }

  subvalue = value.match(tag$3);

  if (!subvalue) {
    return;
  }

  /* istanbul ignore if - not used yet. */
  if (silent) {
    return true;
  }

  subvalue = subvalue[0];

  if (!self.inLink && EXPRESSION_HTML_LINK_OPEN.test(subvalue)) {
    self.inLink = true;
  } else if (self.inLink && EXPRESSION_HTML_LINK_CLOSE.test(subvalue)) {
    self.inLink = false;
  }

  return eat(subvalue)({type: 'html', value: subvalue});
}

var link = locate$6;

function locate$6(value, fromIndex) {
  var link = value.indexOf('[', fromIndex);
  var image = value.indexOf('![', fromIndex);

  if (image === -1) {
    return link;
  }

  /* Link can never be `-1` if an image is found, so we don’t need
   * to check for that :) */
  return link < image ? link : image;
}

var link_1 = link$2;
link$2.locator = link;

var own$4 = {}.hasOwnProperty;

var C_BACKSLASH$3 = '\\';
var C_BRACKET_OPEN$3 = '[';
var C_BRACKET_CLOSE$3 = ']';
var C_PAREN_OPEN$2 = '(';
var C_PAREN_CLOSE$3 = ')';
var C_LT$4 = '<';
var C_GT$3 = '>';
var C_TICK$2 = '`';
var C_DOUBLE_QUOTE$1 = '"';
var C_SINGLE_QUOTE$1 = '\'';

/* Map of characters, which can be used to mark link
 * and image titles. */
var LINK_MARKERS = {};

LINK_MARKERS[C_DOUBLE_QUOTE$1] = C_DOUBLE_QUOTE$1;
LINK_MARKERS[C_SINGLE_QUOTE$1] = C_SINGLE_QUOTE$1;

/* Map of characters, which can be used to mark link
 * and image titles in commonmark-mode. */
var COMMONMARK_LINK_MARKERS = {};

COMMONMARK_LINK_MARKERS[C_DOUBLE_QUOTE$1] = C_DOUBLE_QUOTE$1;
COMMONMARK_LINK_MARKERS[C_SINGLE_QUOTE$1] = C_SINGLE_QUOTE$1;
COMMONMARK_LINK_MARKERS[C_PAREN_OPEN$2] = C_PAREN_CLOSE$3;

function link$2(eat, value, silent) {
  var self = this;
  var subvalue = '';
  var index = 0;
  var character = value.charAt(0);
  var pedantic = self.options.pedantic;
  var commonmark = self.options.commonmark;
  var gfm = self.options.gfm;
  var closed;
  var count;
  var opening;
  var beforeURL;
  var beforeTitle;
  var subqueue;
  var hasMarker;
  var markers;
  var isImage;
  var content;
  var marker;
  var length;
  var title;
  var depth;
  var queue;
  var url;
  var now;
  var exit;
  var node;

  /* Detect whether this is an image. */
  if (character === '!') {
    isImage = true;
    subvalue = character;
    character = value.charAt(++index);
  }

  /* Eat the opening. */
  if (character !== C_BRACKET_OPEN$3) {
    return;
  }

  /* Exit when this is a link and we’re already inside
   * a link. */
  if (!isImage && self.inLink) {
    return;
  }

  subvalue += character;
  queue = '';
  index++;

  /* Eat the content. */
  length = value.length;
  now = eat.now();
  depth = 0;

  now.column += index;
  now.offset += index;

  while (index < length) {
    character = value.charAt(index);
    subqueue = character;

    if (character === C_TICK$2) {
      /* Inline-code in link content. */
      count = 1;

      while (value.charAt(index + 1) === C_TICK$2) {
        subqueue += character;
        index++;
        count++;
      }

      if (!opening) {
        opening = count;
      } else if (count >= opening) {
        opening = 0;
      }
    } else if (character === C_BACKSLASH$3) {
      /* Allow brackets to be escaped. */
      index++;
      subqueue += value.charAt(index);
    /* In GFM mode, brackets in code still count.
     * In all other modes, they don’t.  This empty
     * block prevents the next statements are
     * entered. */
    } else if ((!opening || gfm) && character === C_BRACKET_OPEN$3) {
      depth++;
    } else if ((!opening || gfm) && character === C_BRACKET_CLOSE$3) {
      if (depth) {
        depth--;
      } else {
        /* Allow white-space between content and
         * url in GFM mode. */
        if (!pedantic) {
          while (index < length) {
            character = value.charAt(index + 1);

            if (!isWhitespaceCharacter(character)) {
              break;
            }

            subqueue += character;
            index++;
          }
        }

        if (value.charAt(index + 1) !== C_PAREN_OPEN$2) {
          return;
        }

        subqueue += C_PAREN_OPEN$2;
        closed = true;
        index++;

        break;
      }
    }

    queue += subqueue;
    subqueue = '';
    index++;
  }

  /* Eat the content closing. */
  if (!closed) {
    return;
  }

  content = queue;
  subvalue += queue + subqueue;
  index++;

  /* Eat white-space. */
  while (index < length) {
    character = value.charAt(index);

    if (!isWhitespaceCharacter(character)) {
      break;
    }

    subvalue += character;
    index++;
  }

  /* Eat the URL. */
  character = value.charAt(index);
  markers = commonmark ? COMMONMARK_LINK_MARKERS : LINK_MARKERS;
  queue = '';
  beforeURL = subvalue;

  if (character === C_LT$4) {
    index++;
    beforeURL += C_LT$4;

    while (index < length) {
      character = value.charAt(index);

      if (character === C_GT$3) {
        break;
      }

      if (commonmark && character === '\n') {
        return;
      }

      queue += character;
      index++;
    }

    if (value.charAt(index) !== C_GT$3) {
      return;
    }

    subvalue += C_LT$4 + queue + C_GT$3;
    url = queue;
    index++;
  } else {
    character = null;
    subqueue = '';

    while (index < length) {
      character = value.charAt(index);

      if (subqueue && own$4.call(markers, character)) {
        break;
      }

      if (isWhitespaceCharacter(character)) {
        if (!pedantic) {
          break;
        }

        subqueue += character;
      } else {
        if (character === C_PAREN_OPEN$2) {
          depth++;
        } else if (character === C_PAREN_CLOSE$3) {
          if (depth === 0) {
            break;
          }

          depth--;
        }

        queue += subqueue;
        subqueue = '';

        if (character === C_BACKSLASH$3) {
          queue += C_BACKSLASH$3;
          character = value.charAt(++index);
        }

        queue += character;
      }

      index++;
    }

    subvalue += queue;
    url = queue;
    index = subvalue.length;
  }

  /* Eat white-space. */
  queue = '';

  while (index < length) {
    character = value.charAt(index);

    if (!isWhitespaceCharacter(character)) {
      break;
    }

    queue += character;
    index++;
  }

  character = value.charAt(index);
  subvalue += queue;

  /* Eat the title. */
  if (queue && own$4.call(markers, character)) {
    index++;
    subvalue += character;
    queue = '';
    marker = markers[character];
    beforeTitle = subvalue;

    /* In commonmark-mode, things are pretty easy: the
     * marker cannot occur inside the title.
     *
     * Non-commonmark does, however, support nested
     * delimiters. */
    if (commonmark) {
      while (index < length) {
        character = value.charAt(index);

        if (character === marker) {
          break;
        }

        if (character === C_BACKSLASH$3) {
          queue += C_BACKSLASH$3;
          character = value.charAt(++index);
        }

        index++;
        queue += character;
      }

      character = value.charAt(index);

      if (character !== marker) {
        return;
      }

      title = queue;
      subvalue += queue + character;
      index++;

      while (index < length) {
        character = value.charAt(index);

        if (!isWhitespaceCharacter(character)) {
          break;
        }

        subvalue += character;
        index++;
      }
    } else {
      subqueue = '';

      while (index < length) {
        character = value.charAt(index);

        if (character === marker) {
          if (hasMarker) {
            queue += marker + subqueue;
            subqueue = '';
          }

          hasMarker = true;
        } else if (!hasMarker) {
          queue += character;
        } else if (character === C_PAREN_CLOSE$3) {
          subvalue += queue + marker + subqueue;
          title = queue;
          break;
        } else if (isWhitespaceCharacter(character)) {
          subqueue += character;
        } else {
          queue += marker + subqueue + character;
          subqueue = '';
          hasMarker = false;
        }

        index++;
      }
    }
  }

  if (value.charAt(index) !== C_PAREN_CLOSE$3) {
    return;
  }

  /* istanbul ignore if - never used (yet) */
  if (silent) {
    return true;
  }

  subvalue += C_PAREN_CLOSE$3;

  url = self.decode.raw(self.unescape(url), eat(beforeURL).test().end);

  if (title) {
    beforeTitle = eat(beforeTitle).test().end;
    title = self.decode.raw(self.unescape(title), beforeTitle);
  }

  node = {
    type: isImage ? 'image' : 'link',
    title: title || null,
    url: url
  };

  if (isImage) {
    node.alt = self.decode.raw(self.unescape(content), now) || null;
  } else {
    exit = self.enterLink();
    node.children = self.tokenizeInline(content, now);
    exit();
  }

  return eat(subvalue)(node);
}

var reference_1 = reference;
reference.locator = link;

var T_LINK = 'link';
var T_IMAGE = 'image';
var T_FOOTNOTE = 'footnote';
var REFERENCE_TYPE_SHORTCUT = 'shortcut';
var REFERENCE_TYPE_COLLAPSED = 'collapsed';
var REFERENCE_TYPE_FULL = 'full';
var C_CARET$1 = '^';
var C_BACKSLASH$4 = '\\';
var C_BRACKET_OPEN$4 = '[';
var C_BRACKET_CLOSE$4 = ']';

function reference(eat, value, silent) {
  var self = this;
  var character = value.charAt(0);
  var index = 0;
  var length = value.length;
  var subvalue = '';
  var intro = '';
  var type = T_LINK;
  var referenceType = REFERENCE_TYPE_SHORTCUT;
  var content;
  var identifier;
  var now;
  var node;
  var exit;
  var queue;
  var bracketed;
  var depth;

  /* Check whether we’re eating an image. */
  if (character === '!') {
    type = T_IMAGE;
    intro = character;
    character = value.charAt(++index);
  }

  if (character !== C_BRACKET_OPEN$4) {
    return;
  }

  index++;
  intro += character;
  queue = '';

  /* Check whether we’re eating a footnote. */
  if (
    self.options.footnotes &&
    type === T_LINK &&
    value.charAt(index) === C_CARET$1
  ) {
    intro += C_CARET$1;
    index++;
    type = T_FOOTNOTE;
  }

  /* Eat the text. */
  depth = 0;

  while (index < length) {
    character = value.charAt(index);

    if (character === C_BRACKET_OPEN$4) {
      bracketed = true;
      depth++;
    } else if (character === C_BRACKET_CLOSE$4) {
      if (!depth) {
        break;
      }

      depth--;
    }

    if (character === C_BACKSLASH$4) {
      queue += C_BACKSLASH$4;
      character = value.charAt(++index);
    }

    queue += character;
    index++;
  }

  subvalue = queue;
  content = queue;
  character = value.charAt(index);

  if (character !== C_BRACKET_CLOSE$4) {
    return;
  }

  index++;
  subvalue += character;
  queue = '';

  while (index < length) {
    character = value.charAt(index);

    if (!isWhitespaceCharacter(character)) {
      break;
    }

    queue += character;
    index++;
  }

  character = value.charAt(index);

  /* Inline footnotes cannot have an identifier. */
  if (type !== T_FOOTNOTE && character === C_BRACKET_OPEN$4) {
    identifier = '';
    queue += character;
    index++;

    while (index < length) {
      character = value.charAt(index);

      if (character === C_BRACKET_OPEN$4 || character === C_BRACKET_CLOSE$4) {
        break;
      }

      if (character === C_BACKSLASH$4) {
        identifier += C_BACKSLASH$4;
        character = value.charAt(++index);
      }

      identifier += character;
      index++;
    }

    character = value.charAt(index);

    if (character === C_BRACKET_CLOSE$4) {
      referenceType = identifier ? REFERENCE_TYPE_FULL : REFERENCE_TYPE_COLLAPSED;
      queue += identifier + character;
      index++;
    } else {
      identifier = '';
    }

    subvalue += queue;
    queue = '';
  } else {
    if (!content) {
      return;
    }

    identifier = content;
  }

  /* Brackets cannot be inside the identifier. */
  if (referenceType !== REFERENCE_TYPE_FULL && bracketed) {
    return;
  }

  subvalue = intro + subvalue;

  if (type === T_LINK && self.inLink) {
    return null;
  }

  /* istanbul ignore if - never used (yet) */
  if (silent) {
    return true;
  }

  if (type === T_FOOTNOTE && content.indexOf(' ') !== -1) {
    return eat(subvalue)({
      type: 'footnote',
      children: this.tokenizeInline(content, eat.now())
    });
  }

  now = eat.now();
  now.column += intro.length;
  now.offset += intro.length;
  identifier = referenceType === REFERENCE_TYPE_FULL ? identifier : content;

  node = {
    type: type + 'Reference',
    identifier: normalize_1(identifier)
  };

  if (type === T_LINK || type === T_IMAGE) {
    node.referenceType = referenceType;
  }

  if (type === T_LINK) {
    exit = self.enterLink();
    node.children = self.tokenizeInline(content, now);
    exit();
  } else if (type === T_IMAGE) {
    node.alt = self.decode.raw(self.unescape(content), now) || null;
  }

  return eat(subvalue)(node);
}

var strong = locate$8;

function locate$8(value, fromIndex) {
  var asterisk = value.indexOf('**', fromIndex);
  var underscore = value.indexOf('__', fromIndex);

  if (underscore === -1) {
    return asterisk;
  }

  if (asterisk === -1) {
    return underscore;
  }

  return underscore < asterisk ? underscore : asterisk;
}

var strong_1 = strong$2;
strong$2.locator = strong;

var C_ASTERISK$2 = '*';
var C_UNDERSCORE$2 = '_';

function strong$2(eat, value, silent) {
  var self = this;
  var index = 0;
  var character = value.charAt(index);
  var now;
  var pedantic;
  var marker;
  var queue;
  var subvalue;
  var length;
  var prev;

  if (
    (character !== C_ASTERISK$2 && character !== C_UNDERSCORE$2) ||
    value.charAt(++index) !== character
  ) {
    return;
  }

  pedantic = self.options.pedantic;
  marker = character;
  subvalue = marker + marker;
  length = value.length;
  index++;
  queue = '';
  character = '';

  if (pedantic && isWhitespaceCharacter(value.charAt(index))) {
    return;
  }

  while (index < length) {
    prev = character;
    character = value.charAt(index);

    if (
      character === marker &&
      value.charAt(index + 1) === marker &&
      (!pedantic || !isWhitespaceCharacter(prev))
    ) {
      character = value.charAt(index + 2);

      if (character !== marker) {
        if (!trim_1(queue)) {
          return;
        }

        /* istanbul ignore if - never used (yet) */
        if (silent) {
          return true;
        }

        now = eat.now();
        now.column += 2;
        now.offset += 2;

        return eat(subvalue + queue + subvalue)({
          type: 'strong',
          children: self.tokenizeInline(queue, now)
        });
      }
    }

    if (!pedantic && character === '\\') {
      queue += character;
      character = value.charAt(++index);
    }

    queue += character;
    index++;
  }
}

var isWordCharacter = wordCharacter;

var fromCode$1 = String.fromCharCode;
var re$2 = /\w/;

/* Check if the given character code, or the character
 * code at the first character, is a word character. */
function wordCharacter(character) {
  return re$2.test(
    typeof character === 'number' ? fromCode$1(character) : character.charAt(0)
  )
}

var emphasis = locate$10;

function locate$10(value, fromIndex) {
  var asterisk = value.indexOf('*', fromIndex);
  var underscore = value.indexOf('_', fromIndex);

  if (underscore === -1) {
    return asterisk;
  }

  if (asterisk === -1) {
    return underscore;
  }

  return underscore < asterisk ? underscore : asterisk;
}

var emphasis_1 = emphasis$2;
emphasis$2.locator = emphasis;

var C_ASTERISK$3 = '*';
var C_UNDERSCORE$3 = '_';

function emphasis$2(eat, value, silent) {
  var self = this;
  var index = 0;
  var character = value.charAt(index);
  var now;
  var pedantic;
  var marker;
  var queue;
  var subvalue;
  var length;
  var prev;

  if (character !== C_ASTERISK$3 && character !== C_UNDERSCORE$3) {
    return;
  }

  pedantic = self.options.pedantic;
  subvalue = character;
  marker = character;
  length = value.length;
  index++;
  queue = '';
  character = '';

  if (pedantic && isWhitespaceCharacter(value.charAt(index))) {
    return;
  }

  while (index < length) {
    prev = character;
    character = value.charAt(index);

    if (character === marker && (!pedantic || !isWhitespaceCharacter(prev))) {
      character = value.charAt(++index);

      if (character !== marker) {
        if (!trim_1(queue) || prev === marker) {
          return;
        }

        if (!pedantic && marker === C_UNDERSCORE$3 && isWordCharacter(character)) {
          queue += marker;
          continue;
        }

        /* istanbul ignore if - never used (yet) */
        if (silent) {
          return true;
        }

        now = eat.now();
        now.column++;
        now.offset++;

        return eat(subvalue + queue + marker)({
          type: 'emphasis',
          children: self.tokenizeInline(queue, now)
        });
      }

      queue += marker;
    }

    if (!pedantic && character === '\\') {
      queue += character;
      character = value.charAt(++index);
    }

    queue += character;
    index++;
  }
}

var _delete = locate$12;

function locate$12(value, fromIndex) {
  return value.indexOf('~~', fromIndex);
}

var _delete$2 = strikethrough;
strikethrough.locator = _delete;

var C_TILDE$1 = '~';
var DOUBLE = '~~';

function strikethrough(eat, value, silent) {
  var self = this;
  var character = '';
  var previous = '';
  var preceding = '';
  var subvalue = '';
  var index;
  var length;
  var now;

  if (
    !self.options.gfm ||
    value.charAt(0) !== C_TILDE$1 ||
    value.charAt(1) !== C_TILDE$1 ||
    isWhitespaceCharacter(value.charAt(2))
  ) {
    return;
  }

  index = 1;
  length = value.length;
  now = eat.now();
  now.column += 2;
  now.offset += 2;

  while (++index < length) {
    character = value.charAt(index);

    if (
      character === C_TILDE$1 &&
      previous === C_TILDE$1 &&
      (!preceding || !isWhitespaceCharacter(preceding))
    ) {
      /* istanbul ignore if - never used (yet) */
      if (silent) {
        return true;
      }

      return eat(DOUBLE + subvalue + DOUBLE)({
        type: 'delete',
        children: self.tokenizeInline(subvalue, now)
      });
    }

    subvalue += previous;
    preceding = previous;
    previous = character;
  }
}

var codeInline = locate$14;

function locate$14(value, fromIndex) {
  return value.indexOf('`', fromIndex);
}

var codeInline$2 = inlineCode;
inlineCode.locator = codeInline;

var C_TICK$3 = '`';

/* Tokenise inline code. */
function inlineCode(eat, value, silent) {
  var length = value.length;
  var index = 0;
  var queue = '';
  var tickQueue = '';
  var contentQueue;
  var subqueue;
  var count;
  var openingCount;
  var subvalue;
  var character;
  var found;
  var next;

  while (index < length) {
    if (value.charAt(index) !== C_TICK$3) {
      break;
    }

    queue += C_TICK$3;
    index++;
  }

  if (!queue) {
    return;
  }

  subvalue = queue;
  openingCount = index;
  queue = '';
  next = value.charAt(index);
  count = 0;

  while (index < length) {
    character = next;
    next = value.charAt(index + 1);

    if (character === C_TICK$3) {
      count++;
      tickQueue += character;
    } else {
      count = 0;
      queue += character;
    }

    if (count && next !== C_TICK$3) {
      if (count === openingCount) {
        subvalue += queue + tickQueue;
        found = true;
        break;
      }

      queue += tickQueue;
      tickQueue = '';
    }

    index++;
  }

  if (!found) {
    if (openingCount % 2 !== 0) {
      return;
    }

    queue = '';
  }

  /* istanbul ignore if - never used (yet) */
  if (silent) {
    return true;
  }

  contentQueue = '';
  subqueue = '';
  length = queue.length;
  index = -1;

  while (++index < length) {
    character = queue.charAt(index);

    if (isWhitespaceCharacter(character)) {
      subqueue += character;
      continue;
    }

    if (subqueue) {
      if (contentQueue) {
        contentQueue += subqueue;
      }

      subqueue = '';
    }

    contentQueue += character;
  }

  return eat(subvalue)({
    type: 'inlineCode',
    value: contentQueue
  });
}

var _break = locate$16;

function locate$16(value, fromIndex) {
  var index = value.indexOf('\n', fromIndex);

  while (index > fromIndex) {
    if (value.charAt(index - 1) !== ' ') {
      break;
    }

    index--;
  }

  return index;
}

var _break$2 = hardBreak;
hardBreak.locator = _break;

var MIN_BREAK_LENGTH = 2;

function hardBreak(eat, value, silent) {
  var length = value.length;
  var index = -1;
  var queue = '';
  var character;

  while (++index < length) {
    character = value.charAt(index);

    if (character === '\n') {
      if (index < MIN_BREAK_LENGTH) {
        return;
      }

      /* istanbul ignore if - never used (yet) */
      if (silent) {
        return true;
      }

      queue += character;

      return eat(queue)({type: 'break'});
    }

    if (character !== ' ') {
      return;
    }

    queue += character;
  }
}

var text_1 = text;

function text(eat, value, silent) {
  var self = this;
  var methods;
  var tokenizers;
  var index;
  var length;
  var subvalue;
  var position;
  var tokenizer;
  var name;
  var min;
  var now;

  /* istanbul ignore if - never used (yet) */
  if (silent) {
    return true;
  }

  methods = self.inlineMethods;
  length = methods.length;
  tokenizers = self.inlineTokenizers;
  index = -1;
  min = value.length;

  while (++index < length) {
    name = methods[index];

    if (name === 'text' || !tokenizers[name]) {
      continue;
    }

    tokenizer = tokenizers[name].locator;

    if (!tokenizer) {
      eat.file.fail('Missing locator: `' + name + '`');
    }

    position = tokenizer.call(self, value, 1);

    if (position !== -1 && position < min) {
      min = position;
    }
  }

  subvalue = value.slice(0, min);
  now = eat.now();

  self.decode(subvalue, now, function (content, position, source) {
    eat(source || content)({
      type: 'text',
      value: content
    });
  });
}

var parser = Parser;

function Parser(doc, file) {
  this.file = file;
  this.offset = {};
  this.options = immutable(this.options);
  this.setOptions({});

  this.inList = false;
  this.inBlock = false;
  this.inLink = false;
  this.atStart = true;

  this.toOffset = vfileLocation(file).toOffset;
  this.unescape = _unescape(this, 'escape');
  this.decode = decode$1(this);
}

var proto$3 = Parser.prototype;

/* Expose core. */
proto$3.setOptions = setOptions_1;
proto$3.parse = parse_1$3;

/* Expose `defaults`. */
proto$3.options = defaults$2;

/* Enter and exit helpers. */
proto$3.exitStart = stateToggle('atStart', true);
proto$3.enterList = stateToggle('inList', false);
proto$3.enterLink = stateToggle('inLink', false);
proto$3.enterBlock = stateToggle('inBlock', false);

/* Nodes that can interupt a paragraph:
 *
 * ```markdown
 * A paragraph, followed by a thematic break.
 * ___
 * ```
 *
 * In the above example, the thematic break “interupts”
 * the paragraph. */
proto$3.interruptParagraph = [
  ['thematicBreak'],
  ['atxHeading'],
  ['fencedCode'],
  ['blockquote'],
  ['html'],
  ['setextHeading', {commonmark: false}],
  ['definition', {commonmark: false}],
  ['footnote', {commonmark: false}]
];

/* Nodes that can interupt a list:
 *
 * ```markdown
 * - One
 * ___
 * ```
 *
 * In the above example, the thematic break “interupts”
 * the list. */
proto$3.interruptList = [
  ['fencedCode', {pedantic: false}],
  ['thematicBreak', {pedantic: false}],
  ['definition', {commonmark: false}],
  ['footnote', {commonmark: false}]
];

/* Nodes that can interupt a blockquote:
 *
 * ```markdown
 * > A paragraph.
 * ___
 * ```
 *
 * In the above example, the thematic break “interupts”
 * the blockquote. */
proto$3.interruptBlockquote = [
  ['indentedCode', {commonmark: true}],
  ['fencedCode', {commonmark: true}],
  ['atxHeading', {commonmark: true}],
  ['setextHeading', {commonmark: true}],
  ['thematicBreak', {commonmark: true}],
  ['html', {commonmark: true}],
  ['list', {commonmark: true}],
  ['definition', {commonmark: false}],
  ['footnote', {commonmark: false}]
];

/* Handlers. */
proto$3.blockTokenizers = {
  newline: newline_1,
  indentedCode: codeIndented,
  fencedCode: codeFenced,
  blockquote: blockquote_1,
  atxHeading: headingAtx,
  thematicBreak: thematicBreak_1,
  list: list_1,
  setextHeading: headingSetext,
  html: htmlBlock,
  footnote: footnoteDefinition_1,
  definition: definition_1,
  table: table_1,
  paragraph: paragraph_1
};

proto$3.inlineTokenizers = {
  escape: _escape$2,
  autoLink: autoLink_1,
  url: url_1,
  html: htmlInline,
  link: link_1,
  reference: reference_1,
  strong: strong_1,
  emphasis: emphasis_1,
  deletion: _delete$2,
  code: codeInline$2,
  break: _break$2,
  text: text_1
};

/* Expose precedence. */
proto$3.blockMethods = keys$1(proto$3.blockTokenizers);
proto$3.inlineMethods = keys$1(proto$3.inlineTokenizers);

/* Tokenizers. */
proto$3.tokenizeBlock = tokenizer('block');
proto$3.tokenizeInline = tokenizer('inline');
proto$3.tokenizeFactory = tokenizer;

/* Get all keys in `value`. */
function keys$1(value) {
  var result = [];
  var key;

  for (key in value) {
    result.push(key);
  }

  return result;
}

var remarkParse = parse$9;
parse$9.Parser = parser;

function parse$9(options) {
  var Local = unherit_1(parser);
  Local.prototype.options = immutable(Local.prototype.options, this.data('settings'), options);
  this.Parser = Local;
}

var returner_1 = returner;

function returner(value) {
  return value;
}

var enterLinkReference = enter;

/* Shortcut and collapsed link references need no escaping
 * and encoding during the processing of child nodes (it
 * must be implied from identifier).
 *
 * This toggler turns encoding and escaping off for shortcut
 * and collapsed references.
 *
 * Implies `enterLink`.
 */
function enter(compiler, node) {
  var encode = compiler.encode;
  var escape = compiler.escape;
  var exit = compiler.enterLink();

  if (
    node.referenceType !== 'shortcut' &&
    node.referenceType !== 'collapsed'
  ) {
    return exit;
  }

  compiler.escape = returner_1;
  compiler.encode = returner_1;

  return function () {
    compiler.encode = encode;
    compiler.escape = escape;
    exit();
  };
}

var defaults$5 = {
  gfm: true,
  commonmark: false,
  pedantic: false,
  entities: 'false',
  setext: false,
  closeAtx: false,
  looseTable: false,
  spacedTable: true,
  paddedTable: true,
  stringLength: stringLength,
  incrementListMarker: true,
  fences: false,
  fence: '`',
  bullet: '-',
  listItemIndent: 'tab',
  rule: '*',
  ruleSpaces: true,
  ruleRepetition: 3,
  strong: '*',
  emphasis: '_'
};

function stringLength(value) {
  return value.length;
}

const nbsp$2 = " ";
const iexcl$2 = "¡";
const cent$2 = "¢";
const pound$2 = "£";
const curren$2 = "¤";
const yen$2 = "¥";
const brvbar$2 = "¦";
const sect$2 = "§";
const uml$2 = "¨";
const copy$4 = "©";
const ordf$2 = "ª";
const laquo$2 = "«";
const not$2 = "¬";
const shy$2 = "­";
const reg$2 = "®";
const macr$2 = "¯";
const deg$2 = "°";
const plusmn$2 = "±";
const sup2$2 = "²";
const sup3$2 = "³";
const acute$2 = "´";
const micro$2 = "µ";
const para$2 = "¶";
const middot$2 = "·";
const cedil$2 = "¸";
const sup1$2 = "¹";
const ordm$2 = "º";
const raquo$2 = "»";
const frac14$2 = "¼";
const frac12$2 = "½";
const frac34$2 = "¾";
const iquest$2 = "¿";
const Agrave$2 = "À";
const Aacute$2 = "Á";
const Acirc$2 = "Â";
const Atilde$2 = "Ã";
const Auml$2 = "Ä";
const Aring$2 = "Å";
const AElig$2 = "Æ";
const Ccedil$2 = "Ç";
const Egrave$2 = "È";
const Eacute$2 = "É";
const Ecirc$2 = "Ê";
const Euml$2 = "Ë";
const Igrave$2 = "Ì";
const Iacute$2 = "Í";
const Icirc$2 = "Î";
const Iuml$2 = "Ï";
const ETH$2 = "Ð";
const Ntilde$2 = "Ñ";
const Ograve$2 = "Ò";
const Oacute$2 = "Ó";
const Ocirc$2 = "Ô";
const Otilde$2 = "Õ";
const Ouml$2 = "Ö";
const times$2 = "×";
const Oslash$2 = "Ø";
const Ugrave$2 = "Ù";
const Uacute$2 = "Ú";
const Ucirc$2 = "Û";
const Uuml$2 = "Ü";
const Yacute$2 = "Ý";
const THORN$2 = "Þ";
const szlig$2 = "ß";
const agrave$2 = "à";
const aacute$2 = "á";
const acirc$2 = "â";
const atilde$2 = "ã";
const auml$2 = "ä";
const aring$2 = "å";
const aelig$2 = "æ";
const ccedil$2 = "ç";
const egrave$2 = "è";
const eacute$2 = "é";
const ecirc$2 = "ê";
const euml$2 = "ë";
const igrave$2 = "ì";
const iacute$2 = "í";
const icirc$2 = "î";
const iuml$2 = "ï";
const eth$2 = "ð";
const ntilde$2 = "ñ";
const ograve$2 = "ò";
const oacute$2 = "ó";
const ocirc$2 = "ô";
const otilde$2 = "õ";
const ouml$2 = "ö";
const divide$2 = "÷";
const oslash$2 = "ø";
const ugrave$2 = "ù";
const uacute$2 = "ú";
const ucirc$2 = "û";
const uuml$2 = "ü";
const yacute$2 = "ý";
const thorn$2 = "þ";
const yuml$2 = "ÿ";
const fnof$1 = "ƒ";
const Alpha$1 = "Α";
const Beta$1 = "Β";
const Gamma$1 = "Γ";
const Delta$1 = "Δ";
const Epsilon$1 = "Ε";
const Zeta$1 = "Ζ";
const Eta$1 = "Η";
const Theta$1 = "Θ";
const Iota$1 = "Ι";
const Kappa$1 = "Κ";
const Lambda$1 = "Λ";
const Mu$1 = "Μ";
const Nu$1 = "Ν";
const Xi$1 = "Ξ";
const Omicron$1 = "Ο";
const Pi$1 = "Π";
const Rho$1 = "Ρ";
const Sigma$1 = "Σ";
const Tau$1 = "Τ";
const Upsilon$1 = "Υ";
const Phi$1 = "Φ";
const Chi$1 = "Χ";
const Psi$1 = "Ψ";
const Omega$1 = "Ω";
const alpha$1 = "α";
const beta$1 = "β";
const gamma$1 = "γ";
const delta$1 = "δ";
const epsilon$1 = "ε";
const zeta$1 = "ζ";
const eta$1 = "η";
const theta$1 = "θ";
const iota$1 = "ι";
const kappa$1 = "κ";
const lambda$1 = "λ";
const mu$1 = "μ";
const nu$1 = "ν";
const xi$1 = "ξ";
const omicron$1 = "ο";
const pi$1 = "π";
const rho$1 = "ρ";
const sigmaf$1 = "ς";
const sigma$1 = "σ";
const tau$1 = "τ";
const upsilon$1 = "υ";
const phi$1 = "φ";
const chi$1 = "χ";
const psi$1 = "ψ";
const omega$1 = "ω";
const thetasym$1 = "ϑ";
const upsih$1 = "ϒ";
const piv$1 = "ϖ";
const bull$1 = "•";
const hellip$1 = "…";
const prime$1 = "′";
const Prime$1 = "″";
const oline$1 = "‾";
const frasl$1 = "⁄";
const weierp$1 = "℘";
const image$1 = "ℑ";
const real$1 = "ℜ";
const trade$1 = "™";
const alefsym$1 = "ℵ";
const larr$1 = "←";
const uarr$1 = "↑";
const rarr$1 = "→";
const darr$1 = "↓";
const harr$1 = "↔";
const crarr$1 = "↵";
const lArr$1 = "⇐";
const uArr$1 = "⇑";
const rArr$1 = "⇒";
const dArr$1 = "⇓";
const hArr$1 = "⇔";
const forall$1 = "∀";
const part$1 = "∂";
const exist$1 = "∃";
const empty$1 = "∅";
const nabla$1 = "∇";
const isin$1 = "∈";
const notin$1 = "∉";
const ni$1 = "∋";
const prod$1 = "∏";
const sum$1 = "∑";
const minus$1 = "−";
const lowast$1 = "∗";
const radic$1 = "√";
const prop$1 = "∝";
const infin$1 = "∞";
const ang$1 = "∠";
const and$1 = "∧";
const or$1 = "∨";
const cap$1 = "∩";
const cup$1 = "∪";
const int$1 = "∫";
const there4$1 = "∴";
const sim$1 = "∼";
const cong$1 = "≅";
const asymp$1 = "≈";
const ne$1 = "≠";
const equiv$1 = "≡";
const le$1 = "≤";
const ge$1 = "≥";
const sub$1 = "⊂";
const sup$1 = "⊃";
const nsub$1 = "⊄";
const sube$1 = "⊆";
const supe$1 = "⊇";
const oplus$1 = "⊕";
const otimes$1 = "⊗";
const perp$1 = "⊥";
const sdot$1 = "⋅";
const lceil$1 = "⌈";
const rceil$1 = "⌉";
const lfloor$1 = "⌊";
const rfloor$1 = "⌋";
const lang$1 = "〈";
const rang$1 = "〉";
const loz$1 = "◊";
const spades$1 = "♠";
const clubs$1 = "♣";
const hearts$1 = "♥";
const diams$1 = "♦";
const quot$2 = "\"";
const amp$2 = "&";
const lt$2 = "<";
const gt$2 = ">";
const OElig$1 = "Œ";
const oelig$1 = "œ";
const Scaron$1 = "Š";
const scaron$1 = "š";
const Yuml$1 = "Ÿ";
const circ$1 = "ˆ";
const tilde$1 = "˜";
const ensp$1 = " ";
const emsp$1 = " ";
const thinsp$1 = " ";
const zwnj$1 = "‌";
const zwj$1 = "‍";
const lrm$1 = "‎";
const rlm$1 = "‏";
const ndash$1 = "–";
const mdash$1 = "—";
const lsquo$1 = "‘";
const rsquo$1 = "’";
const sbquo$1 = "‚";
const ldquo$1 = "“";
const rdquo$1 = "”";
const bdquo$1 = "„";
const dagger$1 = "†";
const Dagger$1 = "‡";
const permil$1 = "‰";
const lsaquo$1 = "‹";
const rsaquo$1 = "›";
const euro$1 = "€";
var index$5 = {
	nbsp: nbsp$2,
	iexcl: iexcl$2,
	cent: cent$2,
	pound: pound$2,
	curren: curren$2,
	yen: yen$2,
	brvbar: brvbar$2,
	sect: sect$2,
	uml: uml$2,
	copy: copy$4,
	ordf: ordf$2,
	laquo: laquo$2,
	not: not$2,
	shy: shy$2,
	reg: reg$2,
	macr: macr$2,
	deg: deg$2,
	plusmn: plusmn$2,
	sup2: sup2$2,
	sup3: sup3$2,
	acute: acute$2,
	micro: micro$2,
	para: para$2,
	middot: middot$2,
	cedil: cedil$2,
	sup1: sup1$2,
	ordm: ordm$2,
	raquo: raquo$2,
	frac14: frac14$2,
	frac12: frac12$2,
	frac34: frac34$2,
	iquest: iquest$2,
	Agrave: Agrave$2,
	Aacute: Aacute$2,
	Acirc: Acirc$2,
	Atilde: Atilde$2,
	Auml: Auml$2,
	Aring: Aring$2,
	AElig: AElig$2,
	Ccedil: Ccedil$2,
	Egrave: Egrave$2,
	Eacute: Eacute$2,
	Ecirc: Ecirc$2,
	Euml: Euml$2,
	Igrave: Igrave$2,
	Iacute: Iacute$2,
	Icirc: Icirc$2,
	Iuml: Iuml$2,
	ETH: ETH$2,
	Ntilde: Ntilde$2,
	Ograve: Ograve$2,
	Oacute: Oacute$2,
	Ocirc: Ocirc$2,
	Otilde: Otilde$2,
	Ouml: Ouml$2,
	times: times$2,
	Oslash: Oslash$2,
	Ugrave: Ugrave$2,
	Uacute: Uacute$2,
	Ucirc: Ucirc$2,
	Uuml: Uuml$2,
	Yacute: Yacute$2,
	THORN: THORN$2,
	szlig: szlig$2,
	agrave: agrave$2,
	aacute: aacute$2,
	acirc: acirc$2,
	atilde: atilde$2,
	auml: auml$2,
	aring: aring$2,
	aelig: aelig$2,
	ccedil: ccedil$2,
	egrave: egrave$2,
	eacute: eacute$2,
	ecirc: ecirc$2,
	euml: euml$2,
	igrave: igrave$2,
	iacute: iacute$2,
	icirc: icirc$2,
	iuml: iuml$2,
	eth: eth$2,
	ntilde: ntilde$2,
	ograve: ograve$2,
	oacute: oacute$2,
	ocirc: ocirc$2,
	otilde: otilde$2,
	ouml: ouml$2,
	divide: divide$2,
	oslash: oslash$2,
	ugrave: ugrave$2,
	uacute: uacute$2,
	ucirc: ucirc$2,
	uuml: uuml$2,
	yacute: yacute$2,
	thorn: thorn$2,
	yuml: yuml$2,
	fnof: fnof$1,
	Alpha: Alpha$1,
	Beta: Beta$1,
	Gamma: Gamma$1,
	Delta: Delta$1,
	Epsilon: Epsilon$1,
	Zeta: Zeta$1,
	Eta: Eta$1,
	Theta: Theta$1,
	Iota: Iota$1,
	Kappa: Kappa$1,
	Lambda: Lambda$1,
	Mu: Mu$1,
	Nu: Nu$1,
	Xi: Xi$1,
	Omicron: Omicron$1,
	Pi: Pi$1,
	Rho: Rho$1,
	Sigma: Sigma$1,
	Tau: Tau$1,
	Upsilon: Upsilon$1,
	Phi: Phi$1,
	Chi: Chi$1,
	Psi: Psi$1,
	Omega: Omega$1,
	alpha: alpha$1,
	beta: beta$1,
	gamma: gamma$1,
	delta: delta$1,
	epsilon: epsilon$1,
	zeta: zeta$1,
	eta: eta$1,
	theta: theta$1,
	iota: iota$1,
	kappa: kappa$1,
	lambda: lambda$1,
	mu: mu$1,
	nu: nu$1,
	xi: xi$1,
	omicron: omicron$1,
	pi: pi$1,
	rho: rho$1,
	sigmaf: sigmaf$1,
	sigma: sigma$1,
	tau: tau$1,
	upsilon: upsilon$1,
	phi: phi$1,
	chi: chi$1,
	psi: psi$1,
	omega: omega$1,
	thetasym: thetasym$1,
	upsih: upsih$1,
	piv: piv$1,
	bull: bull$1,
	hellip: hellip$1,
	prime: prime$1,
	Prime: Prime$1,
	oline: oline$1,
	frasl: frasl$1,
	weierp: weierp$1,
	image: image$1,
	real: real$1,
	trade: trade$1,
	alefsym: alefsym$1,
	larr: larr$1,
	uarr: uarr$1,
	rarr: rarr$1,
	darr: darr$1,
	harr: harr$1,
	crarr: crarr$1,
	lArr: lArr$1,
	uArr: uArr$1,
	rArr: rArr$1,
	dArr: dArr$1,
	hArr: hArr$1,
	forall: forall$1,
	part: part$1,
	exist: exist$1,
	empty: empty$1,
	nabla: nabla$1,
	isin: isin$1,
	notin: notin$1,
	ni: ni$1,
	prod: prod$1,
	sum: sum$1,
	minus: minus$1,
	lowast: lowast$1,
	radic: radic$1,
	prop: prop$1,
	infin: infin$1,
	ang: ang$1,
	and: and$1,
	or: or$1,
	cap: cap$1,
	cup: cup$1,
	int: int$1,
	there4: there4$1,
	sim: sim$1,
	cong: cong$1,
	asymp: asymp$1,
	ne: ne$1,
	equiv: equiv$1,
	le: le$1,
	ge: ge$1,
	sub: sub$1,
	sup: sup$1,
	nsub: nsub$1,
	sube: sube$1,
	supe: supe$1,
	oplus: oplus$1,
	otimes: otimes$1,
	perp: perp$1,
	sdot: sdot$1,
	lceil: lceil$1,
	rceil: rceil$1,
	lfloor: lfloor$1,
	rfloor: rfloor$1,
	lang: lang$1,
	rang: rang$1,
	loz: loz$1,
	spades: spades$1,
	clubs: clubs$1,
	hearts: hearts$1,
	diams: diams$1,
	quot: quot$2,
	amp: amp$2,
	lt: lt$2,
	gt: gt$2,
	OElig: OElig$1,
	oelig: oelig$1,
	Scaron: Scaron$1,
	scaron: scaron$1,
	Yuml: Yuml$1,
	circ: circ$1,
	tilde: tilde$1,
	ensp: ensp$1,
	emsp: emsp$1,
	thinsp: thinsp$1,
	zwnj: zwnj$1,
	zwj: zwj$1,
	lrm: lrm$1,
	rlm: rlm$1,
	ndash: ndash$1,
	mdash: mdash$1,
	lsquo: lsquo$1,
	rsquo: rsquo$1,
	sbquo: sbquo$1,
	ldquo: ldquo$1,
	rdquo: rdquo$1,
	bdquo: bdquo$1,
	dagger: dagger$1,
	Dagger: Dagger$1,
	permil: permil$1,
	lsaquo: lsaquo$1,
	rsaquo: rsaquo$1,
	euro: euro$1
};

var characterEntitiesHtml4 = Object.freeze({
	nbsp: nbsp$2,
	iexcl: iexcl$2,
	cent: cent$2,
	pound: pound$2,
	curren: curren$2,
	yen: yen$2,
	brvbar: brvbar$2,
	sect: sect$2,
	uml: uml$2,
	copy: copy$4,
	ordf: ordf$2,
	laquo: laquo$2,
	not: not$2,
	shy: shy$2,
	reg: reg$2,
	macr: macr$2,
	deg: deg$2,
	plusmn: plusmn$2,
	sup2: sup2$2,
	sup3: sup3$2,
	acute: acute$2,
	micro: micro$2,
	para: para$2,
	middot: middot$2,
	cedil: cedil$2,
	sup1: sup1$2,
	ordm: ordm$2,
	raquo: raquo$2,
	frac14: frac14$2,
	frac12: frac12$2,
	frac34: frac34$2,
	iquest: iquest$2,
	Agrave: Agrave$2,
	Aacute: Aacute$2,
	Acirc: Acirc$2,
	Atilde: Atilde$2,
	Auml: Auml$2,
	Aring: Aring$2,
	AElig: AElig$2,
	Ccedil: Ccedil$2,
	Egrave: Egrave$2,
	Eacute: Eacute$2,
	Ecirc: Ecirc$2,
	Euml: Euml$2,
	Igrave: Igrave$2,
	Iacute: Iacute$2,
	Icirc: Icirc$2,
	Iuml: Iuml$2,
	ETH: ETH$2,
	Ntilde: Ntilde$2,
	Ograve: Ograve$2,
	Oacute: Oacute$2,
	Ocirc: Ocirc$2,
	Otilde: Otilde$2,
	Ouml: Ouml$2,
	times: times$2,
	Oslash: Oslash$2,
	Ugrave: Ugrave$2,
	Uacute: Uacute$2,
	Ucirc: Ucirc$2,
	Uuml: Uuml$2,
	Yacute: Yacute$2,
	THORN: THORN$2,
	szlig: szlig$2,
	agrave: agrave$2,
	aacute: aacute$2,
	acirc: acirc$2,
	atilde: atilde$2,
	auml: auml$2,
	aring: aring$2,
	aelig: aelig$2,
	ccedil: ccedil$2,
	egrave: egrave$2,
	eacute: eacute$2,
	ecirc: ecirc$2,
	euml: euml$2,
	igrave: igrave$2,
	iacute: iacute$2,
	icirc: icirc$2,
	iuml: iuml$2,
	eth: eth$2,
	ntilde: ntilde$2,
	ograve: ograve$2,
	oacute: oacute$2,
	ocirc: ocirc$2,
	otilde: otilde$2,
	ouml: ouml$2,
	divide: divide$2,
	oslash: oslash$2,
	ugrave: ugrave$2,
	uacute: uacute$2,
	ucirc: ucirc$2,
	uuml: uuml$2,
	yacute: yacute$2,
	thorn: thorn$2,
	yuml: yuml$2,
	fnof: fnof$1,
	Alpha: Alpha$1,
	Beta: Beta$1,
	Gamma: Gamma$1,
	Delta: Delta$1,
	Epsilon: Epsilon$1,
	Zeta: Zeta$1,
	Eta: Eta$1,
	Theta: Theta$1,
	Iota: Iota$1,
	Kappa: Kappa$1,
	Lambda: Lambda$1,
	Mu: Mu$1,
	Nu: Nu$1,
	Xi: Xi$1,
	Omicron: Omicron$1,
	Pi: Pi$1,
	Rho: Rho$1,
	Sigma: Sigma$1,
	Tau: Tau$1,
	Upsilon: Upsilon$1,
	Phi: Phi$1,
	Chi: Chi$1,
	Psi: Psi$1,
	Omega: Omega$1,
	alpha: alpha$1,
	beta: beta$1,
	gamma: gamma$1,
	delta: delta$1,
	epsilon: epsilon$1,
	zeta: zeta$1,
	eta: eta$1,
	theta: theta$1,
	iota: iota$1,
	kappa: kappa$1,
	lambda: lambda$1,
	mu: mu$1,
	nu: nu$1,
	xi: xi$1,
	omicron: omicron$1,
	pi: pi$1,
	rho: rho$1,
	sigmaf: sigmaf$1,
	sigma: sigma$1,
	tau: tau$1,
	upsilon: upsilon$1,
	phi: phi$1,
	chi: chi$1,
	psi: psi$1,
	omega: omega$1,
	thetasym: thetasym$1,
	upsih: upsih$1,
	piv: piv$1,
	bull: bull$1,
	hellip: hellip$1,
	prime: prime$1,
	Prime: Prime$1,
	oline: oline$1,
	frasl: frasl$1,
	weierp: weierp$1,
	image: image$1,
	real: real$1,
	trade: trade$1,
	alefsym: alefsym$1,
	larr: larr$1,
	uarr: uarr$1,
	rarr: rarr$1,
	darr: darr$1,
	harr: harr$1,
	crarr: crarr$1,
	lArr: lArr$1,
	uArr: uArr$1,
	rArr: rArr$1,
	dArr: dArr$1,
	hArr: hArr$1,
	forall: forall$1,
	part: part$1,
	exist: exist$1,
	empty: empty$1,
	nabla: nabla$1,
	isin: isin$1,
	notin: notin$1,
	ni: ni$1,
	prod: prod$1,
	sum: sum$1,
	minus: minus$1,
	lowast: lowast$1,
	radic: radic$1,
	prop: prop$1,
	infin: infin$1,
	ang: ang$1,
	and: and$1,
	or: or$1,
	cap: cap$1,
	cup: cup$1,
	int: int$1,
	there4: there4$1,
	sim: sim$1,
	cong: cong$1,
	asymp: asymp$1,
	ne: ne$1,
	equiv: equiv$1,
	le: le$1,
	ge: ge$1,
	sub: sub$1,
	sup: sup$1,
	nsub: nsub$1,
	sube: sube$1,
	supe: supe$1,
	oplus: oplus$1,
	otimes: otimes$1,
	perp: perp$1,
	sdot: sdot$1,
	lceil: lceil$1,
	rceil: rceil$1,
	lfloor: lfloor$1,
	rfloor: rfloor$1,
	lang: lang$1,
	rang: rang$1,
	loz: loz$1,
	spades: spades$1,
	clubs: clubs$1,
	hearts: hearts$1,
	diams: diams$1,
	quot: quot$2,
	amp: amp$2,
	lt: lt$2,
	gt: gt$2,
	OElig: OElig$1,
	oelig: oelig$1,
	Scaron: Scaron$1,
	scaron: scaron$1,
	Yuml: Yuml$1,
	circ: circ$1,
	tilde: tilde$1,
	ensp: ensp$1,
	emsp: emsp$1,
	thinsp: thinsp$1,
	zwnj: zwnj$1,
	zwj: zwj$1,
	lrm: lrm$1,
	rlm: rlm$1,
	ndash: ndash$1,
	mdash: mdash$1,
	lsquo: lsquo$1,
	rsquo: rsquo$1,
	sbquo: sbquo$1,
	ldquo: ldquo$1,
	rdquo: rdquo$1,
	bdquo: bdquo$1,
	dagger: dagger$1,
	Dagger: Dagger$1,
	permil: permil$1,
	lsaquo: lsaquo$1,
	rsaquo: rsaquo$1,
	euro: euro$1,
	default: index$5
});

var dangerous = [
  "cent",
  "copy",
  "divide",
  "gt",
  "lt",
  "not",
  "para",
  "times"
]
;

var dangerous$1 = Object.freeze({
	default: dangerous
});

var entities = ( characterEntitiesHtml4 && index$5 ) || characterEntitiesHtml4;

var dangerous$2 = ( dangerous$1 && dangerous ) || dangerous$1;

/* Expose. */
var stringifyEntities = encode;
encode.escape = escape$1;

var own$5 = {}.hasOwnProperty;

/* List of enforced escapes. */
var escapes$2 = ['"', "'", '<', '>', '&', '`'];

/* Map of characters to names. */
var characters$1 = construct();

/* Default escapes. */
var defaultEscapes = toExpression(escapes$2);

/* Surrogate pairs. */
var surrogatePair = /[\uD800-\uDBFF][\uDC00-\uDFFF]/g;

/* Non-ASCII characters. */
// eslint-disable-next-line no-control-regex, unicorn/no-hex-escape
var bmp = /[\x01-\t\x0B\f\x0E-\x1F\x7F\x81\x8D\x8F\x90\x9D\xA0-\uFFFF]/g;

/* Encode special characters in `value`. */
function encode(value, options) {
  var settings = options || {};
  var subset = settings.subset;
  var set = subset ? toExpression(subset) : defaultEscapes;
  var escapeOnly = settings.escapeOnly;
  var omit = settings.omitOptionalSemicolons;

  value = value.replace(set, function(char, pos, val) {
    return one$1(char, val.charAt(pos + 1), settings)
  });

  if (subset || escapeOnly) {
    return value
  }

  return value
    .replace(surrogatePair, replaceSurrogatePair)
    .replace(bmp, replaceBmp)

  function replaceSurrogatePair(pair, pos, val) {
    return toHexReference(
      (pair.charCodeAt(0) - 0xd800) * 0x400 +
        pair.charCodeAt(1) -
        0xdc00 +
        0x10000,
      val.charAt(pos + 2),
      omit
    )
  }

  function replaceBmp(char, pos, val) {
    return one$1(char, val.charAt(pos + 1), settings)
  }
}

/* Shortcut to escape special characters in HTML. */
function escape$1(value) {
  return encode(value, {
    escapeOnly: true,
    useNamedReferences: true
  })
}

/* Encode `char` according to `options`. */
function one$1(char, next, options) {
  var shortest = options.useShortestReferences;
  var omit = options.omitOptionalSemicolons;
  var named;
  var numeric;

  if ((shortest || options.useNamedReferences) && own$5.call(characters$1, char)) {
    named = toNamed(characters$1[char], next, omit, options.attribute);
  }

  if (shortest || !named) {
    numeric = toHexReference(char.charCodeAt(0), next, omit);
  }

  if (named && (!shortest || named.length < numeric.length)) {
    return named
  }

  return numeric
}

/* Transform `code` into an entity. */
function toNamed(name, next, omit, attribute) {
  var value = '&' + name;

  if (
    omit &&
    own$5.call(legacy, name) &&
    dangerous$2.indexOf(name) === -1 &&
    (!attribute || (next && next !== '=' && !isAlphanumerical(next)))
  ) {
    return value
  }

  return value + ';'
}

/* Transform `code` into a hexadecimal character reference. */
function toHexReference(code, next, omit) {
  var value = '&#x' + code.toString(16).toUpperCase();
  return omit && next && !isHexadecimal(next) ? value : value + ';'
}

/* Create an expression for `characters`. */
function toExpression(characters) {
  return new RegExp('[' + characters.join('') + ']', 'g')
}

/* Construct the map. */
function construct() {
  var chars = {};
  var name;

  for (name in entities) {
    chars[entities[name]] = name;
  }

  return chars
}

var isAlphanumeric = function (str) {
	if (typeof str !== 'string') {
		throw new TypeError('Expected a string');
	}

	return !/[^0-9a-z\xDF-\xFF]/.test(str.toLowerCase());
};

var entityPrefixLength = length;

/* Returns the length of HTML entity that is a prefix of
 * the given string (excluding the ampersand), 0 if it
 * does not start with an entity. */
function length(value) {
  var prefix;

  /* istanbul ignore if - Currently also tested for at
   * implemention, but we keep it here because that’s
   * proper. */
  if (value.charAt(0) !== '&') {
    return 0;
  }

  prefix = value.split('&', 2).join('&');

  return prefix.length - parseEntities_1(prefix).length;
}

var _escape$4 = factory$5;

var BACKSLASH = '\\';
var BULLETS = ['*', '-', '+'];
var ALLIGNMENT = [':', '-', ' ', '|'];
var entities$1 = {'<': '&lt;', ':': '&#x3A;', '&': '&amp;', '|': '&#x7C;', '~': '&#x7E;'};

/* Factory to escape characters. */
function factory$5(options) {
  return escape;

  /* Escape punctuation characters in a node's value. */
  function escape(value, node, parent) {
    var self = this;
    var gfm = options.gfm;
    var commonmark = options.commonmark;
    var pedantic = options.pedantic;
    var markers = commonmark ? ['.', ')'] : ['.'];
    var siblings = parent && parent.children;
    var index = siblings && siblings.indexOf(node);
    var prev = siblings && siblings[index - 1];
    var next = siblings && siblings[index + 1];
    var length = value.length;
    var escapable = markdownEscapes(options);
    var position = -1;
    var queue = [];
    var escaped = queue;
    var afterNewLine;
    var character;
    var wordCharBefore;
    var wordCharAfter;
    var offset;
    var replace;

    if (prev) {
      afterNewLine = text$1(prev) && /\n\s*$/.test(prev.value);
    } else {
      afterNewLine = !parent || parent.type === 'root' || parent.type === 'paragraph';
    }

    function one(character) {
      return escapable.indexOf(character) === -1 ?
        entities$1[character] : BACKSLASH + character;
    }

    while (++position < length) {
      character = value.charAt(position);
      replace = false;

      if (character === '\n') {
        afterNewLine = true;
      } else if (
        character === BACKSLASH ||
        character === '`' ||
        character === '*' ||
        character === '[' ||
        character === '<' ||
        (character === '&' && entityPrefixLength(value.slice(position)) > 0) ||
        (character === ']' && self.inLink) ||
        (gfm && character === '~' && value.charAt(position + 1) === '~') ||
        (gfm && character === '|' && (self.inTable || alignment(value, position))) ||
        (
          character === '_' &&
          /* Delegate leading/trailing underscores
           * to the multinode version below. */
          position > 0 &&
          position < length - 1 &&
          (
              pedantic ||
              !isAlphanumeric(value.charAt(position - 1)) ||
              !isAlphanumeric(value.charAt(position + 1))
          )
        ) ||
        (gfm && !self.inLink && character === ':' && protocol(queue.join('')))
      ) {
        replace = true;
      } else if (afterNewLine) {
        if (
          character === '>' ||
          character === '#' ||
          BULLETS.indexOf(character) !== -1
        ) {
          replace = true;
        } else if (isDecimal(character)) {
          offset = position + 1;

          while (offset < length) {
            if (!isDecimal(value.charAt(offset))) {
              break;
            }

            offset++;
          }

          if (markers.indexOf(value.charAt(offset)) !== -1) {
            next = value.charAt(offset + 1);

            if (!next || next === ' ' || next === '\t' || next === '\n') {
              queue.push(value.slice(position, offset));
              position = offset;
              character = value.charAt(position);
              replace = true;
            }
          }
        }
      }

      if (afterNewLine && !isWhitespaceCharacter(character)) {
        afterNewLine = false;
      }

      queue.push(replace ? one(character) : character);
    }

    /* Multi-node versions. */
    if (siblings && text$1(node)) {
      /* Check for an opening parentheses after a
       * link-reference (which can be joined by
       * white-space). */
      if (prev && prev.referenceType === 'shortcut') {
        position = -1;
        length = escaped.length;

        while (++position < length) {
          character = escaped[position];

          if (character === ' ' || character === '\t') {
            continue;
          }

          if (character === '(' || character === ':') {
            escaped[position] = one(character);
          }

          break;
        }

        /* If the current node is all spaces / tabs,
         * preceded by a shortcut, and followed by
         * a text starting with `(`, escape it. */
        if (
          text$1(next) &&
          position === length &&
          next.value.charAt(0) === '('
        ) {
          escaped.push(BACKSLASH);
        }
      }

      /* Ensure non-auto-links are not seen as links.
       * This pattern needs to check the preceding
       * nodes too. */
      if (
        gfm &&
        !self.inLink &&
        text$1(prev) &&
        value.charAt(0) === ':' &&
        protocol(prev.value.slice(-6))
      ) {
        escaped[0] = one(':');
      }

      /* Escape ampersand if it would otherwise
       * start an entity. */
      if (
        text$1(next) &&
        value.charAt(length - 1) === '&' &&
        entityPrefixLength('&' + next.value) !== 0
      ) {
        escaped[escaped.length - 1] = one('&');
      }

      /* Escape double tildes in GFM. */
      if (
        gfm &&
        text$1(next) &&
        value.charAt(length - 1) === '~' &&
        next.value.charAt(0) === '~'
      ) {
        escaped.splice(escaped.length - 1, 0, BACKSLASH);
      }

      /* Escape underscores, but not mid-word (unless
       * in pedantic mode). */
      wordCharBefore = text$1(prev) && isAlphanumeric(prev.value.slice(-1));
      wordCharAfter = text$1(next) && isAlphanumeric(next.value.charAt(0));

      if (length === 1) {
        if (value === '_' && (pedantic || !wordCharBefore || !wordCharAfter)) {
          escaped.unshift(BACKSLASH);
        }
      } else {
        if (
          value.charAt(0) === '_' &&
          (pedantic || !wordCharBefore || !isAlphanumeric(value.charAt(1)))
        ) {
          escaped.unshift(BACKSLASH);
        }

        if (
          value.charAt(length - 1) === '_' &&
          (pedantic || !wordCharAfter || !isAlphanumeric(value.charAt(length - 2)))
        ) {
          escaped.splice(escaped.length - 1, 0, BACKSLASH);
        }
      }
    }

    return escaped.join('');
  }
}

/* Check if `index` in `value` is inside an alignment row. */
function alignment(value, index) {
  var start = value.lastIndexOf('\n', index);
  var end = value.indexOf('\n', index);

  start = start === -1 ? -1 : start;
  end = end === -1 ? value.length : end;

  while (++start < end) {
    if (ALLIGNMENT.indexOf(value.charAt(start)) === -1) {
      return false;
    }
  }

  return true;
}

/* Check if `node` is a text node. */
function text$1(node) {
  return node && node.type === 'text';
}

/* Check if `value` ends in a protocol. */
function protocol(value) {
  var val = value.slice(-6).toLowerCase();
  return val === 'mailto' || val.slice(-5) === 'https' || val.slice(-4) === 'http';
}

var setOptions_1$2 = setOptions$1;

/* Map of applicable enum's. */
var maps = {
  entities: {true: true, false: true, numbers: true, escape: true},
  bullet: {'*': true, '-': true, '+': true},
  rule: {'-': true, _: true, '*': true},
  listItemIndent: {tab: true, mixed: true, 1: true},
  emphasis: {_: true, '*': true},
  strong: {_: true, '*': true},
  fence: {'`': true, '~': true}
};

/* Expose `validate`. */
var validate = {
  boolean: validateBoolean,
  string: validateString,
  number: validateNumber,
  function: validateFunction
};

/* Set options.  Does not overwrite previously set
 * options. */
function setOptions$1(options) {
  var self = this;
  var current = self.options;
  var ruleRepetition;
  var key;

  if (options == null) {
    options = {};
  } else if (typeof options === 'object') {
    options = immutable(options);
  } else {
    throw new Error('Invalid value `' + options + '` for setting `options`');
  }

  for (key in defaults$5) {
    validate[typeof defaults$5[key]](options, key, current[key], maps[key]);
  }

  ruleRepetition = options.ruleRepetition;

  if (ruleRepetition && ruleRepetition < 3) {
    raise(ruleRepetition, 'options.ruleRepetition');
  }

  self.encode = encodeFactory(String(options.entities));
  self.escape = _escape$4(options);

  self.options = options;

  return self;
}

/* Throw an exception with in its `message` `value`
 * and `name`. */
function raise(value, name) {
  throw new Error('Invalid value `' + value + '` for setting `' + name + '`');
}

/* Validate a value to be boolean. Defaults to `def`.
 * Raises an exception with `context[name]` when not
 * a boolean. */
function validateBoolean(context, name, def) {
  var value = context[name];

  if (value == null) {
    value = def;
  }

  if (typeof value !== 'boolean') {
    raise(value, 'options.' + name);
  }

  context[name] = value;
}

/* Validate a value to be boolean. Defaults to `def`.
 * Raises an exception with `context[name]` when not
 * a boolean. */
function validateNumber(context, name, def) {
  var value = context[name];

  if (value == null) {
    value = def;
  }

  if (isNaN(value)) {
    raise(value, 'options.' + name);
  }

  context[name] = value;
}

/* Validate a value to be in `map`. Defaults to `def`.
 * Raises an exception with `context[name]` when not
 * in `map`. */
function validateString(context, name, def, map) {
  var value = context[name];

  if (value == null) {
    value = def;
  }

  value = String(value);

  if (!(value in map)) {
    raise(value, 'options.' + name);
  }

  context[name] = value;
}

/* Validate a value to be function. Defaults to `def`.
 * Raises an exception with `context[name]` when not
 * a function. */
function validateFunction(context, name, def) {
  var value = context[name];

  if (value == null) {
    value = def;
  }

  if (typeof value !== 'function') {
    raise(value, 'options.' + name);
  }

  context[name] = value;
}

/* Factory to encode HTML entities.
 * Creates a no-operation function when `type` is
 * `'false'`, a function which encodes using named
 * references when `type` is `'true'`, and a function
 * which encodes using numbered references when `type` is
 * `'numbers'`. */
function encodeFactory(type) {
  var options = {};

  if (type === 'false') {
    return returner_1;
  }

  if (type === 'true') {
    options.useNamedReferences = true;
  }

  if (type === 'escape') {
    options.escapeOnly = true;
    options.useNamedReferences = true;
  }

  return wrapped;

  /* Encode HTML entities using the bound options. */
  function wrapped(value) {
    return stringifyEntities(value, options);
  }
}

var mdastUtilCompact = compact;

/* Make an MDAST tree compact by merging adjacent text nodes. */
function compact(tree, commonmark) {
  unistUtilVisit(tree, visitor);

  return tree

  function visitor(child, index, parent) {
    var siblings = parent ? parent.children : [];
    var prev = index && siblings[index - 1];

    if (
      prev &&
      child.type === prev.type &&
      mergeable$1(prev, commonmark) &&
      mergeable$1(child, commonmark)
    ) {
      if (child.value) {
        prev.value += child.value;
      }

      if (child.children) {
        prev.children = prev.children.concat(child.children);
      }

      siblings.splice(index, 1);

      if (prev.position && child.position) {
        prev.position.end = child.position.end;
      }

      return index
    }
  }
}

function mergeable$1(node, commonmark) {
  var start;
  var end;

  if (node.type === 'text') {
    if (!node.position) {
      return true
    }

    start = node.position.start;
    end = node.position.end;

    /* Only merge nodes which occupy the same size as their `value`. */
    return (
      start.line !== end.line || end.column - start.column === node.value.length
    )
  }

  return commonmark && node.type === 'blockquote'
}

var compile_1 = compile$2;

/* Stringify the given tree. */
function compile$2() {
  return this.visit(mdastUtilCompact(this.tree, this.options.commonmark));
}

var one_1 = one$2;

function one$2(node, parent) {
  var self = this;
  var visitors = self.visitors;

  /* Fail on unknown nodes. */
  if (typeof visitors[node.type] !== 'function') {
    self.file.fail(
      new Error(
        'Missing compiler for node of type `' +
        node.type + '`: `' + node + '`'
      ),
      node
    );
  }

  return visitors[node.type].call(self, node, parent);
}

var all_1 = all;

/* Visit all children of `parent`. */
function all(parent) {
  var self = this;
  var children = parent.children;
  var length = children.length;
  var results = [];
  var index = -1;

  while (++index < length) {
    results[index] = self.visit(children[index], parent);
  }

  return results;
}

var block_1 = block$1;

/* Stringify a block node with block children (e.g., `root`
 * or `blockquote`).
 * Knows about code following a list, or adjacent lists
 * with similar bullets, and places an extra newline
 * between them. */
function block$1(node) {
  var self = this;
  var values = [];
  var children = node.children;
  var length = children.length;
  var index = -1;
  var child;
  var prev;

  while (++index < length) {
    child = children[index];

    if (prev) {
      /* Duplicate nodes, such as a list
       * directly following another list,
       * often need multiple new lines.
       *
       * Additionally, code blocks following a list
       * might easily be mistaken for a paragraph
       * in the list itself. */
      if (child.type === prev.type && prev.type === 'list') {
        values.push(prev.ordered === child.ordered ? '\n\n\n' : '\n\n');
      } else if (prev.type === 'list' && child.type === 'code' && !child.lang) {
        values.push('\n\n\n');
      } else {
        values.push('\n\n');
      }
    }

    values.push(self.visit(child, node));

    prev = child;
  }

  return values.join('');
}

var orderedItems_1 = orderedItems;

/* Visit ordered list items.
 *
 * Starts the list with
 * `node.start` and increments each following list item
 * bullet by one:
 *
 *     2. foo
 *     3. bar
 *
 * In `incrementListMarker: false` mode, does not increment
 * each marker and stays on `node.start`:
 *
 *     1. foo
 *     1. bar
 */
function orderedItems(node) {
  var self = this;
  var fn = self.visitors.listItem;
  var increment = self.options.incrementListMarker;
  var values = [];
  var start = node.start;
  var children = node.children;
  var length = children.length;
  var index = -1;
  var bullet;

  while (++index < length) {
    bullet = (increment ? start + index : start) + '.';
    values[index] = fn.call(self, children[index], node, index, bullet);
  }

  return values.join('\n');
}

var unorderedItems_1 = unorderedItems;

/* Visit unordered list items.
 * Uses `options.bullet` as each item's bullet.
 */
function unorderedItems(node) {
  var self = this;
  var bullet = self.options.bullet;
  var fn = self.visitors.listItem;
  var children = node.children;
  var length = children.length;
  var index = -1;
  var values = [];

  while (++index < length) {
    values[index] = fn.call(self, children[index], node, index, bullet);
  }

  return values.join('\n');
}

var root_1 = root;

/* Stringify a root.
 * Adds a final newline to ensure valid POSIX files. */
function root(node) {
  return this.block(node) + '\n';
}

var text_1$2 = text$2;

/* Stringify text.
 * Supports named entities in `settings.encode: true` mode:
 *
 *     AT&amp;T
 *
 * Supports numbered entities in `settings.encode: numbers`
 * mode:
 *
 *     AT&#x26;T
 */
function text$2(node, parent) {
  return this.encode(this.escape(node.value, node, parent), node);
}

var heading_1 = heading;

/* Stringify a heading.
 *
 * In `setext: true` mode and when `depth` is smaller than
 * three, creates a setext header:
 *
 *     Foo
 *     ===
 *
 * Otherwise, an ATX header is generated:
 *
 *     ### Foo
 *
 * In `closeAtx: true` mode, the header is closed with
 * hashes:
 *
 *     ### Foo ###
 */
function heading(node) {
  var self = this;
  var depth = node.depth;
  var setext = self.options.setext;
  var closeAtx = self.options.closeAtx;
  var content = self.all(node).join('');
  var prefix;

  if (setext && depth < 3) {
    return content + '\n' + repeatString(depth === 1 ? '=' : '-', content.length);
  }

  prefix = repeatString('#', node.depth);

  return prefix + ' ' + content + (closeAtx ? ' ' + prefix : '');
}

var paragraph_1$2 = paragraph$1;

function paragraph$1(node) {
  return this.all(node).join('');
}

var blockquote_1$2 = blockquote$1;

function blockquote$1(node) {
  var values = this.block(node).split('\n');
  var result = [];
  var length = values.length;
  var index = -1;
  var value;

  while (++index < length) {
    value = values[index];
    result[index] = (value ? ' ' : '') + value;
  }

  return '>' + result.join('\n>');
}

var list_1$2 = list$1;

/* Which method to use based on `list.ordered`. */
var ORDERED_MAP = {
  true: 'visitOrderedItems',
  false: 'visitUnorderedItems'
};

function list$1(node) {
  return this[ORDERED_MAP[node.ordered]](node);
}

var pad_1 = pad;

var INDENT = 4;

/* Pad `value` with `level * INDENT` spaces.  Respects
 * lines. Ignores empty lines. */
function pad(value, level) {
  var index;
  var padding;

  value = value.split('\n');

  index = value.length;
  padding = repeatString(' ', level * INDENT);

  while (index--) {
    if (value[index].length !== 0) {
      value[index] = padding + value[index];
    }
  }

  return value.join('\n');
}

var listItem_1 = listItem$1;

/* Which checkbox to use. */
var CHECKBOX_MAP = {
  undefined: '',
  null: '',
  true: '[x] ',
  false: '[ ] '
};

/* Stringify a list item.
 *
 * Prefixes the content with a checked checkbox when
 * `checked: true`:
 *
 *     [x] foo
 *
 * Prefixes the content with an unchecked checkbox when
 * `checked: false`:
 *
 *     [ ] foo
 */
function listItem$1(node, parent, position, bullet) {
  var self = this;
  var style = self.options.listItemIndent;
  var loose = node.loose;
  var children = node.children;
  var length = children.length;
  var values = [];
  var index = -1;
  var value;
  var indent;
  var spacing;

  while (++index < length) {
    values[index] = self.visit(children[index], node);
  }

  value = CHECKBOX_MAP[node.checked] + values.join(loose ? '\n\n' : '\n');

  if (style === '1' || (style === 'mixed' && value.indexOf('\n') === -1)) {
    indent = bullet.length + 1;
    spacing = ' ';
  } else {
    indent = Math.ceil((bullet.length + 1) / 4) * 4;
    spacing = repeatString(' ', indent - bullet.length);
  }

  value = bullet + spacing + pad_1(value, indent / 4).slice(indent);

  if (loose && parent.children.length - 1 !== position) {
    value += '\n';
  }

  return value;
}

/* Expose. */
var longestStreak_1 = longestStreak;

/* Get the count of the longest repeating streak of
 * `character` in `value`. */
function longestStreak(value, character) {
  var count = 0;
  var maximum = 0;
  var expected;
  var index;

  if (typeof character !== 'string' || character.length !== 1) {
    throw new Error('Expected character');
  }

  value = String(value);
  index = value.indexOf(character);
  expected = index;

  while (index !== -1) {
    count++;

    if (index === expected) {
      if (count > maximum) {
        maximum = count;
      }
    } else {
      count = 1;
    }

    expected = index + 1;
    index = value.indexOf(character, expected);
  }

  return maximum;
}

var inlineCode_1 = inlineCode$1;

/* Stringify inline code.
 *
 * Knows about internal ticks (`\``), and ensures one more
 * tick is used to enclose the inline code:
 *
 *     ```foo ``bar`` baz```
 *
 * Even knows about inital and final ticks:
 *
 *     `` `foo ``
 *     `` foo` ``
 */
function inlineCode$1(node) {
  var value = node.value;
  var ticks = repeatString('`', longestStreak_1(value, '`') + 1);
  var start = ticks;
  var end = ticks;

  if (value.charAt(0) === '`') {
    start += ' ';
  }

  if (value.charAt(value.length - 1) === '`') {
    end = ' ' + end;
  }

  return start + value + end;
}

var code_1 = code;

var FENCE = /([`~])\1{2}/;

/* Stringify code.
 * Creates indented code when:
 *
 * - No language tag exists;
 * - Not in `fences: true` mode;
 * - A non-empty value exists.
 *
 * Otherwise, GFM fenced code is created:
 *
 *     ```js
 *     foo();
 *     ```
 *
 * When in ``fence: `~` `` mode, uses tildes as fences:
 *
 *     ~~~js
 *     foo();
 *     ~~~
 *
 * Knows about internal fences (Note: GitHub/Kramdown does
 * not support this):
 *
 *     ````javascript
 *     ```markdown
 *     foo
 *     ```
 *     ````
 */
function code(node, parent) {
  var self = this;
  var value = node.value;
  var options = self.options;
  var marker = options.fence;
  var language = self.encode(node.lang || '', node);
  var fence;

  /* Without (needed) fences. */
  if (!language && !options.fences && value) {
    /* Throw when pedantic, in a list item which
     * isn’t compiled using a tab. */
    if (
      parent &&
      parent.type === 'listItem' &&
      options.listItemIndent !== 'tab' &&
      options.pedantic
    ) {
      self.file.fail('Cannot indent code properly. See http://git.io/vgFvT', node.position);
    }

    return pad_1(value, 1);
  }

  fence = longestStreak_1(value, marker) + 1;

  /* Fix GFM / RedCarpet bug, where fence-like characters
   * inside fenced code can exit a code-block.
   * Yes, even when the outer fence uses different
   * characters, or is longer.
   * Thus, we can only pad the code to make it work. */
  if (FENCE.test(value)) {
    value = pad_1(value, 1);
  }

  fence = repeatString(marker, Math.max(fence, 3));

  return fence + language + '\n' + value + '\n' + fence;
}

var html_1 = html$2;

function html$2(node) {
  return node.value;
}

var thematicBreak$1 = thematic;

/* Stringify a `thematic-break`.
 * The character used is configurable through `rule`: (`'_'`)
 *
 *     ___
 *
 * The number of repititions is defined through
 * `ruleRepetition`: (`6`)
 *
 *     ******
 *
 * Whether spaces delimit each character, is configured
 * through `ruleSpaces`: (`true`)
 *
 *     * * *
 */
function thematic() {
  var options = this.options;
  var rule = repeatString(options.rule, options.ruleRepetition);
  return options.ruleSpaces ? rule.split('').join(' ') : rule;
}

var strong_1$2 = strong$3;

/* Stringify a `strong`.
 *
 * The marker used is configurable by `strong`, which
 * defaults to an asterisk (`'*'`) but also accepts an
 * underscore (`'_'`):
 *
 *     __foo__
 */
function strong$3(node) {
  var marker = repeatString(this.options.strong, 2);
  return marker + this.all(node).join('') + marker;
}

var emphasis_1$2 = emphasis$3;

/* Stringify an `emphasis`.
 *
 * The marker used is configurable through `emphasis`, which
 * defaults to an underscore (`'_'`) but also accepts an
 * asterisk (`'*'`):
 *
 *     *foo*
 */
function emphasis$3(node) {
  var marker = this.options.emphasis;
  return marker + this.all(node).join('') + marker;
}

var _break$4 = lineBreak;

var map$4 = {true: '\\\n', false: '  \n'};

function lineBreak() {
  return map$4[this.options.commonmark];
}

var _delete$4 = strikethrough$1;

function strikethrough$1(node) {
  return '~~' + this.all(node).join('') + '~~';
}

var ccount_1 = ccount;

function ccount(value, character) {
  var count = 0;
  var index;

  value = String(value);

  if (typeof character !== 'string' || character.length !== 1) {
    throw new Error('Expected character')
  }

  index = value.indexOf(character);

  while (index !== -1) {
    count++;
    index = value.indexOf(character, index + 1);
  }

  return count
}

var encloseUri = enclose;

var re$3 = /\s/;

/* Wrap `url` in angle brackets when needed, or when
 * forced.
 * In links, images, and definitions, the URL part needs
 * to be enclosed when it:
 *
 * - has a length of `0`;
 * - contains white-space;
 * - has more or less opening than closing parentheses.
 */
function enclose(uri, always) {
  if (always || uri.length === 0 || re$3.test(uri) || ccount_1(uri, '(') !== ccount_1(uri, ')')) {
    return '<' + uri + '>';
  }

  return uri;
}

var encloseTitle = enclose$1;

/* There is currently no way to support nested delimiters
 * across Markdown.pl, CommonMark, and GitHub (RedCarpet).
 * The following code supports Markdown.pl and GitHub.
 * CommonMark is not supported when mixing double- and
 * single quotes inside a title. */
function enclose$1(title) {
  var delimiter = title.indexOf('"') === -1 ? '"' : '\'';
  return delimiter + title + delimiter;
}

var link_1$2 = link$3;

/* Expression for a protocol:
 * http://en.wikipedia.org/wiki/URI_scheme#Generic_syntax */
var PROTOCOL = /^[a-z][a-z+.-]+:\/?/i;

/* Stringify a link.
 *
 * When no title exists, the compiled `children` equal
 * `url`, and `url` starts with a protocol, an auto
 * link is created:
 *
 *     <http://example.com>
 *
 * Otherwise, is smart about enclosing `url` (see
 * `encloseURI()`) and `title` (see `encloseTitle()`).
 *
 *    [foo](<foo at bar dot com> 'An "example" e-mail')
 *
 * Supports named entities in the `url` and `title` when
 * in `settings.encode` mode. */
function link$3(node) {
  var self = this;
  var content = self.encode(node.url || '', node);
  var exit = self.enterLink();
  var escaped = self.encode(self.escape(node.url || '', node));
  var value = self.all(node).join('');

  exit();

  if (
    node.title == null &&
    PROTOCOL.test(content) &&
    (escaped === value || escaped === 'mailto:' + value)
  ) {
    /* Backslash escapes do not work in autolinks,
     * so we do not escape. */
    return encloseUri(self.encode(node.url), true);
  }

  content = encloseUri(content);

  if (node.title) {
    content += ' ' + encloseTitle(self.encode(self.escape(node.title, node), node));
  }

  return '[' + value + '](' + content + ')';
}

var copyIdentifierEncoding = copy$5;

var PUNCTUATION = /[-!"#$%&'()*+,./:;<=>?@[\\\]^`{|}~_]/;

/* For shortcut and collapsed reference links, the contents
 * is also an identifier, so we need to restore the original
 * encoding and escaping that were present in the source
 * string.
 *
 * This function takes the unescaped & unencoded value from
 * shortcut's child nodes and the identifier and encodes
 * the former according to the latter. */
function copy$5(value, identifier) {
  var length = value.length;
  var count = identifier.length;
  var result = [];
  var position = 0;
  var index = 0;
  var start;

  while (index < length) {
    /* Take next non-punctuation characters from `value`. */
    start = index;

    while (index < length && !PUNCTUATION.test(value.charAt(index))) {
      index += 1;
    }

    result.push(value.slice(start, index));

    /* Advance `position` to the next punctuation character. */
    while (position < count && !PUNCTUATION.test(identifier.charAt(position))) {
      position += 1;
    }

    /* Take next punctuation characters from `identifier`. */
    start = position;

    while (position < count && PUNCTUATION.test(identifier.charAt(position))) {
      if (identifier.charAt(position) === '&') {
        position += entityPrefixLength(identifier.slice(position));
      }

      position += 1;
    }

    result.push(identifier.slice(start, position));

    /* Advance `index` to the next non-punctuation character. */
    while (index < length && PUNCTUATION.test(value.charAt(index))) {
      index += 1;
    }
  }

  return result.join('');
}

var label_1 = label;

/* Stringify a reference label.
 * Because link references are easily, mistakingly,
 * created (for example, `[foo]`), reference nodes have
 * an extra property depicting how it looked in the
 * original document, so stringification can cause minimal
 * changes. */
function label(node) {
  var type = node.referenceType;
  var value = type === 'full' ? node.identifier : '';

  return type === 'shortcut' ? value : '[' + value + ']';
}

var linkReference_1 = linkReference;

function linkReference(node) {
  var self = this;
  var type = node.referenceType;
  var exit = self.enterLinkReference(self, node);
  var value = self.all(node).join('');

  exit();

  if (type === 'shortcut' || type === 'collapsed') {
    value = copyIdentifierEncoding(value, node.identifier);
  }

  return '[' + value + ']' + label_1(node);
}

var imageReference_1 = imageReference;

function imageReference(node) {
  return '![' + (this.encode(node.alt, node) || '') + ']' + label_1(node);
}

var definition_1$2 = definition$1;

/* Stringify an URL definition.
 *
 * Is smart about enclosing `url` (see `encloseURI()`) and
 * `title` (see `encloseTitle()`).
 *
 *    [foo]: <foo at bar dot com> 'An "example" e-mail'
 */
function definition$1(node) {
  var content = encloseUri(node.url);

  if (node.title) {
    content += ' ' + encloseTitle(node.title);
  }

  return '[' + node.identifier + ']: ' + content;
}

var image_1 = image$2;

/* Stringify an image.
 *
 * Is smart about enclosing `url` (see `encloseURI()`) and
 * `title` (see `encloseTitle()`).
 *
 *    ![foo](</fav icon.png> 'My "favourite" icon')
 *
 * Supports named entities in `url`, `alt`, and `title`
 * when in `settings.encode` mode.
 */
function image$2(node) {
  var self = this;
  var content = encloseUri(self.encode(node.url || '', node));
  var exit = self.enterLink();
  var alt = self.encode(self.escape(node.alt || '', node));

  exit();

  if (node.title) {
    content += ' ' + encloseTitle(self.encode(node.title, node));
  }

  return '![' + alt + '](' + content + ')';
}

var footnote_1 = footnote;

function footnote(node) {
  return '[^' + this.all(node).join('') + ']';
}

var footnoteReference_1 = footnoteReference;

function footnoteReference(node) {
  return '[^' + node.identifier + ']';
}

var footnoteDefinition_1$2 = footnoteDefinition$1;

function footnoteDefinition$1(node) {
  var id = node.identifier.toLowerCase();
  var content = this.all(node).join('\n\n' + repeatString(' ', 4));

  return '[^' + id + ']: ' + content;
}

/* Expose. */
var markdownTable_1 = markdownTable;

/* Expressions. */
var EXPRESSION_DOT = /\./;
var EXPRESSION_LAST_DOT = /\.[^.]*$/;

/* Allowed alignment values. */
var LEFT = 'l';
var RIGHT = 'r';
var CENTER = 'c';
var DOT = '.';
var NULL = '';

var ALLIGNMENT$1 = [LEFT, RIGHT, CENTER, DOT, NULL];
var MIN_CELL_SIZE = 3;

/* Characters. */
var COLON = ':';
var DASH = '-';
var PIPE = '|';
var SPACE = ' ';
var NEW_LINE = '\n';

/* Create a table from a matrix of strings. */
function markdownTable(table, options) {
  var settings = options || {};
  var delimiter = settings.delimiter;
  var start = settings.start;
  var end = settings.end;
  var alignment = settings.align;
  var calculateStringLength = settings.stringLength || lengthNoop;
  var cellCount = 0;
  var rowIndex = -1;
  var rowLength = table.length;
  var sizes = [];
  var align;
  var rule;
  var rows;
  var row;
  var cells;
  var index;
  var position;
  var size;
  var value;
  var spacing;
  var before;
  var after;

  alignment = alignment ? alignment.concat() : [];

  if (delimiter === null || delimiter === undefined) {
    delimiter = SPACE + PIPE + SPACE;
  }

  if (start === null || start === undefined) {
    start = PIPE + SPACE;
  }

  if (end === null || end === undefined) {
    end = SPACE + PIPE;
  }

  while (++rowIndex < rowLength) {
    row = table[rowIndex];

    index = -1;

    if (row.length > cellCount) {
      cellCount = row.length;
    }

    while (++index < cellCount) {
      position = row[index] ? dotindex$1(row[index]) : null;

      if (!sizes[index]) {
        sizes[index] = MIN_CELL_SIZE;
      }

      if (position > sizes[index]) {
        sizes[index] = position;
      }
    }
  }

  if (typeof alignment === 'string') {
    alignment = pad$2(cellCount, alignment).split('');
  }

  /* Make sure only valid alignments are used. */
  index = -1;

  while (++index < cellCount) {
    align = alignment[index];

    if (typeof align === 'string') {
      align = align.charAt(0).toLowerCase();
    }

    if (ALLIGNMENT$1.indexOf(align) === -1) {
      align = NULL;
    }

    alignment[index] = align;
  }

  rowIndex = -1;
  rows = [];

  while (++rowIndex < rowLength) {
    row = table[rowIndex];

    index = -1;
    cells = [];

    while (++index < cellCount) {
      value = row[index];

      value = stringify$6(value);

      if (alignment[index] === DOT) {
        position = dotindex$1(value);

        size =
          sizes[index] +
          (EXPRESSION_DOT.test(value) ? 0 : 1) -
          (calculateStringLength(value) - position);

        cells[index] = value + pad$2(size - 1);
      } else {
        cells[index] = value;
      }
    }

    rows[rowIndex] = cells;
  }

  sizes = [];
  rowIndex = -1;

  while (++rowIndex < rowLength) {
    cells = rows[rowIndex];

    index = -1;

    while (++index < cellCount) {
      value = cells[index];

      if (!sizes[index]) {
        sizes[index] = MIN_CELL_SIZE;
      }

      size = calculateStringLength(value);

      if (size > sizes[index]) {
        sizes[index] = size;
      }
    }
  }

  rowIndex = -1;

  while (++rowIndex < rowLength) {
    cells = rows[rowIndex];

    index = -1;

    if (settings.pad !== false) {
      while (++index < cellCount) {
        value = cells[index];

        position = sizes[index] - (calculateStringLength(value) || 0);
        spacing = pad$2(position);

        if (alignment[index] === RIGHT || alignment[index] === DOT) {
          value = spacing + value;
        } else if (alignment[index] === CENTER) {
          position /= 2;

          if (position % 1 === 0) {
            before = position;
            after = position;
          } else {
            before = position + 0.5;
            after = position - 0.5;
          }

          value = pad$2(before) + value + pad$2(after);
        } else {
          value += spacing;
        }

        cells[index] = value;
      }
    }

    rows[rowIndex] = cells.join(delimiter);
  }

  if (settings.rule !== false) {
    index = -1;
    rule = [];

    while (++index < cellCount) {
      /* When `pad` is false, make the rule the same size as the first row. */
      if (settings.pad === false) {
        value = table[0][index];
        spacing = calculateStringLength(stringify$6(value));
        spacing = spacing > MIN_CELL_SIZE ? spacing : MIN_CELL_SIZE;
      } else {
        spacing = sizes[index];
      }

      align = alignment[index];

      /* When `align` is left, don't add colons. */
      value = align === RIGHT || align === NULL ? DASH : COLON;
      value += pad$2(spacing - 2, DASH);
      value += align !== LEFT && align !== NULL ? COLON : DASH;

      rule[index] = value;
    }

    rows.splice(1, 0, rule.join(delimiter));
  }

  return start + rows.join(end + NEW_LINE + start) + end
}

function stringify$6(value) {
  return value === null || value === undefined ? '' : String(value)
}

/* Get the length of `value`. */
function lengthNoop(value) {
  return String(value).length
}

/* Get a string consisting of `length` `character`s. */
function pad$2(length, character) {
  return new Array(length + 1).join(character || SPACE)
}

/* Get the position of the last dot in `value`. */
function dotindex$1(value) {
  var match = EXPRESSION_LAST_DOT.exec(value);

  return match ? match.index + 1 : value.length
}

var table_1$2 = table$2;

/* Stringify table.
 *
 * Creates a fenced table by default, but not in
 * `looseTable: true` mode:
 *
 *     Foo | Bar
 *     :-: | ---
 *     Baz | Qux
 *
 * NOTE: Be careful with `looseTable: true` mode, as a
 * loose table inside an indented code block on GitHub
 * renders as an actual table!
 *
 * Creates a spaced table by default, but not in
 * `spacedTable: false`:
 *
 *     |Foo|Bar|
 *     |:-:|---|
 *     |Baz|Qux|
 */
function table$2(node) {
  var self = this;
  var options = self.options;
  var loose = options.looseTable;
  var spaced = options.spacedTable;
  var pad = options.paddedTable;
  var stringLength = options.stringLength;
  var rows = node.children;
  var index = rows.length;
  var exit = self.enterTable();
  var result = [];
  var start;
  var end;

  while (index--) {
    result[index] = self.all(rows[index]);
  }

  exit();

  if (loose) {
    start = '';
    end = '';
  } else if (spaced) {
    start = '| ';
    end = ' |';
  } else {
    start = '|';
    end = '|';
  }

  return markdownTable_1(result, {
    align: node.align,
    pad: pad,
    start: start,
    end: end,
    stringLength: stringLength,
    delimiter: spaced ? ' | ' : '|'
  });
}

var tableCell_1 = tableCell;

function tableCell(node) {
  return this.all(node).join('');
}

var compiler = Compiler;

/* Construct a new compiler. */
function Compiler(tree, file) {
  this.inLink = false;
  this.inTable = false;
  this.tree = tree;
  this.file = file;
  this.options = immutable(this.options);
  this.setOptions({});
}

var proto$4 = Compiler.prototype;

/* Enter and exit helpers. */
proto$4.enterLink = stateToggle('inLink', false);
proto$4.enterTable = stateToggle('inTable', false);
proto$4.enterLinkReference = enterLinkReference;

/* Configuration. */
proto$4.options = defaults$5;
proto$4.setOptions = setOptions_1$2;

proto$4.compile = compile_1;
proto$4.visit = one_1;
proto$4.all = all_1;
proto$4.block = block_1;
proto$4.visitOrderedItems = orderedItems_1;
proto$4.visitUnorderedItems = unorderedItems_1;

/* Expose visitors. */
proto$4.visitors = {
  root: root_1,
  text: text_1$2,
  heading: heading_1,
  paragraph: paragraph_1$2,
  blockquote: blockquote_1$2,
  list: list_1$2,
  listItem: listItem_1,
  inlineCode: inlineCode_1,
  code: code_1,
  html: html_1,
  thematicBreak: thematicBreak$1,
  strong: strong_1$2,
  emphasis: emphasis_1$2,
  break: _break$4,
  delete: _delete$4,
  link: link_1$2,
  linkReference: linkReference_1,
  imageReference: imageReference_1,
  definition: definition_1$2,
  image: image_1,
  footnote: footnote_1,
  footnoteReference: footnoteReference_1,
  footnoteDefinition: footnoteDefinition_1$2,
  table: table_1$2,
  tableCell: tableCell_1
};

var remarkStringify = stringify$7;
stringify$7.Compiler = compiler;

function stringify$7(options) {
  var Local = unherit_1(compiler);
  Local.prototype.options = immutable(Local.prototype.options, this.data('settings'), options);
  this.Compiler = Local;
}

var remark = unified_1().use(remarkParse).use(remarkStringify).freeze();

const _from = "remark@8.0.0";
const _id = "remark@8.0.0";
const _inBundle = false;
const _integrity = "sha512-K0PTsaZvJlXTl9DN6qYlvjTkqSZBFELhROZMrblm2rB+085flN84nz4g/BscKRMqDvhzlK1oQ/xnWQumdeNZYw==";
const _location = "/remark";
const _phantomChildren = {};
const _requested = {"type":"version","registry":true,"raw":"remark@8.0.0","name":"remark","escapedName":"remark","rawSpec":"8.0.0","saveSpec":null,"fetchSpec":"8.0.0"};
const _requiredBy = ["#USER","/"];
const _resolved = "https://registry.npmjs.org/remark/-/remark-8.0.0.tgz";
const _shasum = "287b6df2fe1190e263c1d15e486d3fa835594d6d";
const _spec = "remark@8.0.0";
const _where = "/mnt/d/code/node/tools/node-lint-md-cli-rollup";
const author = {"name":"Titus Wormer","email":"tituswormer@gmail.com","url":"http://wooorm.com"};
const bugs = {"url":"https://github.com/wooorm/remark/issues"};
const bundleDependencies = false;
const contributors = [{"name":"Titus Wormer","email":"tituswormer@gmail.com","url":"http://wooorm.com"}];
const dependencies = {"remark-parse":"^4.0.0","remark-stringify":"^4.0.0","unified":"^6.0.0"};
const deprecated$1 = false;
const description = "Markdown processor powered by plugins";
const files = ["index.js"];
const homepage = "http://remark.js.org";
const keywords = ["markdown","abstract","syntax","tree","ast","parse","stringify","process"];
const license = "MIT";
const name = "remark";
const repository = {"type":"git","url":"https://github.com/wooorm/remark/tree/master/packages/remark"};
const scripts = {};
const version$1 = "8.0.0";
const xo = false;
var _package = {
	_from: _from,
	_id: _id,
	_inBundle: _inBundle,
	_integrity: _integrity,
	_location: _location,
	_phantomChildren: _phantomChildren,
	_requested: _requested,
	_requiredBy: _requiredBy,
	_resolved: _resolved,
	_shasum: _shasum,
	_spec: _spec,
	_where: _where,
	author: author,
	bugs: bugs,
	bundleDependencies: bundleDependencies,
	contributors: contributors,
	dependencies: dependencies,
	deprecated: deprecated$1,
	description: description,
	files: files,
	homepage: homepage,
	keywords: keywords,
	license: license,
	name: name,
	repository: repository,
	scripts: scripts,
	version: version$1,
	xo: xo
};

var _package$1 = Object.freeze({
	_from: _from,
	_id: _id,
	_inBundle: _inBundle,
	_integrity: _integrity,
	_location: _location,
	_phantomChildren: _phantomChildren,
	_requested: _requested,
	_requiredBy: _requiredBy,
	_resolved: _resolved,
	_shasum: _shasum,
	_spec: _spec,
	_where: _where,
	author: author,
	bugs: bugs,
	bundleDependencies: bundleDependencies,
	contributors: contributors,
	dependencies: dependencies,
	deprecated: deprecated$1,
	description: description,
	files: files,
	homepage: homepage,
	keywords: keywords,
	license: license,
	name: name,
	repository: repository,
	scripts: scripts,
	version: version$1,
	xo: xo,
	default: _package
});

const name$1 = "node-lint-md-cli-rollup";
const description$1 = "remark packaged for node markdown linting";
const version$2 = "1.0.0";
const devDependencies = {"rollup":"^0.55.5","rollup-plugin-commonjs":"^8.0.2","rollup-plugin-json":"^2.3.1","rollup-plugin-node-resolve":"^3.4.0"};
const dependencies$1 = {"markdown-extensions":"^1.1.0","remark":"^8.0.0","remark-lint":"^6.0.2","remark-preset-lint-node":"./remark-preset-lint-node","unified-args":"^6.0.0","unified-engine":"^5.1.0"};
const scripts$1 = {"build":"rollup -c","build-node":"npm run build && cp dist/* .."};
var _package$2 = {
	name: name$1,
	description: description$1,
	version: version$2,
	devDependencies: devDependencies,
	dependencies: dependencies$1,
	scripts: scripts$1
};

var _package$3 = Object.freeze({
	name: name$1,
	description: description$1,
	version: version$2,
	devDependencies: devDependencies,
	dependencies: dependencies$1,
	scripts: scripts$1,
	default: _package$2
});

var trim_1$2 = createCommonjsModule(function (module, exports) {
exports = module.exports = trim;

function trim(str){
  return str.replace(/^\s*|\s*$/g, '');
}

exports.left = function(str){
  return str.replace(/^\s*/, '');
};

exports.right = function(str){
  return str.replace(/\s*$/, '');
};
});

var trim_2$1 = trim_1$2.left;
var trim_3$1 = trim_1$2.right;

/* Expose. */
var vfileLocation$3 = factory$6;

/* Factory. */
function factory$6(file) {
  var contents = indices$1(String(file));

  return {
    toPosition: offsetToPositionFactory$1(contents),
    toOffset: positionToOffsetFactory$1(contents)
  }
}

/* Factory to get the line and column-based `position` for
 * `offset` in the bound indices. */
function offsetToPositionFactory$1(indices) {
  return offsetToPosition

  /* Get the line and column-based `position` for
   * `offset` in the bound indices. */
  function offsetToPosition(offset) {
    var index = -1;
    var length = indices.length;

    if (offset < 0) {
      return {}
    }

    while (++index < length) {
      if (indices[index] > offset) {
        return {
          line: index + 1,
          column: offset - (indices[index - 1] || 0) + 1,
          offset: offset
        }
      }
    }

    return {}
  }
}

/* Factory to get the `offset` for a line and column-based
 * `position` in the bound indices. */
function positionToOffsetFactory$1(indices) {
  return positionToOffset

  /* Get the `offset` for a line and column-based
   * `position` in the bound indices. */
  function positionToOffset(position) {
    var line = position && position.line;
    var column = position && position.column;

    if (!isNaN(line) && !isNaN(column) && line - 1 in indices) {
      return (indices[line - 2] || 0) + column - 1 || 0
    }

    return -1
  }
}

/* Get indices of line-breaks in `value`. */
function indices$1(value) {
  var result = [];
  var index = value.indexOf('\n');

  while (index !== -1) {
    result.push(index + 1);
    index = value.indexOf('\n', index + 1);
  }

  result.push(value.length + 1);

  return result
}

/* eslint-disable max-params */

/* Expose. */
var unistUtilIs$2 = is$2;

/* Assert if `test` passes for `node`.
 * When a `parent` node is known the `index` of node */
function is$2(test, node, index, parent, context) {
  var hasParent = parent !== null && parent !== undefined;
  var hasIndex = index !== null && index !== undefined;
  var check = convert$1(test);

  if (
    hasIndex &&
    (typeof index !== 'number' || index < 0 || index === Infinity)
  ) {
    throw new Error('Expected positive finite index or child node')
  }

  if (hasParent && (!is$2(null, parent) || !parent.children)) {
    throw new Error('Expected parent node')
  }

  if (!node || !node.type || typeof node.type !== 'string') {
    return false
  }

  if (hasParent !== hasIndex) {
    throw new Error('Expected both parent and index')
  }

  return Boolean(check.call(context, node, index, parent))
}

function convert$1(test) {
  if (typeof test === 'string') {
    return typeFactory$1(test)
  }

  if (test === null || test === undefined) {
    return ok$2
  }

  if (typeof test === 'object') {
    return ('length' in test ? anyFactory$1 : matchesFactory$1)(test)
  }

  if (typeof test === 'function') {
    return test
  }

  throw new Error('Expected function, string, or object as test')
}

function convertAll$1(tests) {
  var results = [];
  var length = tests.length;
  var index = -1;

  while (++index < length) {
    results[index] = convert$1(tests[index]);
  }

  return results
}

/* Utility assert each property in `test` is represented
 * in `node`, and each values are strictly equal. */
function matchesFactory$1(test) {
  return matches

  function matches(node) {
    var key;

    for (key in test) {
      if (node[key] !== test[key]) {
        return false
      }
    }

    return true
  }
}

function anyFactory$1(tests) {
  var checks = convertAll$1(tests);
  var length = checks.length;

  return matches

  function matches() {
    var index = -1;

    while (++index < length) {
      if (checks[index].apply(this, arguments)) {
        return true
      }
    }

    return false
  }
}

/* Utility to convert a string into a function which checks
 * a given node’s type for said string. */
function typeFactory$1(test) {
  return type

  function type(node) {
    return Boolean(node && node.type === test)
  }
}

/* Utility to return true. */
function ok$2() {
  return true
}

var unistUtilVisitParents$2 = visitParents$2;



var CONTINUE$2 = true;
var SKIP$2 = 'skip';
var EXIT$2 = false;

visitParents$2.CONTINUE = CONTINUE$2;
visitParents$2.SKIP = SKIP$2;
visitParents$2.EXIT = EXIT$2;

function visitParents$2(tree, test, visitor, reverse) {
  if (typeof test === 'function' && typeof visitor !== 'function') {
    reverse = visitor;
    visitor = test;
    test = null;
  }

  one(tree, null, []);

  // Visit a single node.
  function one(node, index, parents) {
    var result;

    if (!test || unistUtilIs$2(test, node, index, parents[parents.length - 1] || null)) {
      result = visitor(node, parents);

      if (result === EXIT$2) {
        return result
      }
    }

    if (node.children && result !== SKIP$2) {
      return all(node.children, parents.concat(node)) === EXIT$2 ? EXIT$2 : result
    }

    return result
  }

  // Visit children in `parent`.
  function all(children, parents) {
    var min = -1;
    var step = reverse ? -1 : 1;
    var index = (reverse ? children.length : min) + step;
    var child;
    var result;

    while (index > min && index < children.length) {
      child = children[index];
      result = child && one(child, index, parents);

      if (result === EXIT$2) {
        return result
      }

      index = typeof result === 'number' ? result : index + step;
    }
  }
}

var unistUtilVisit$2 = visit$2;



var CONTINUE$3 = unistUtilVisitParents$2.CONTINUE;
var SKIP$3 = unistUtilVisitParents$2.SKIP;
var EXIT$3 = unistUtilVisitParents$2.EXIT;

visit$2.CONTINUE = CONTINUE$3;
visit$2.SKIP = SKIP$3;
visit$2.EXIT = EXIT$3;

function visit$2(tree, test, visitor, reverse) {
  if (typeof test === 'function' && typeof visitor !== 'function') {
    reverse = visitor;
    visitor = test;
    test = null;
  }

  unistUtilVisitParents$2(tree, test, overload, reverse);

  function overload(node, parents) {
    var parent = parents[parents.length - 1];
    var index = parent ? parent.children.indexOf(node) : null;
    return visitor(node, index, parent)
  }
}

/* Map of allowed verbs. */
var ALLOWED_VERBS = {
  enable: true,
  disable: true,
  ignore: true
};

var unifiedMessageControl = messageControl;

function messageControl(options) {
  var name = options && options.name;
  var marker = options && options.marker;
  var test = options && options.test;
  var sources;
  var known;
  var reset;
  var enable;
  var disable;

  if (!name) {
    throw new Error('Expected `name` in `options`, got `' + name + '`')
  }

  if (!marker) {
    throw new Error('Expected `name` in `options`, got `' + name + '`')
  }

  if (!test) {
    throw new Error('Expected `test` in `options`, got `' + test + '`')
  }

  known = options.known;
  reset = options.reset;
  enable = options.enable || [];
  disable = options.disable || [];
  sources = options.source;

  if (!sources) {
    sources = [name];
  } else if (typeof sources === 'string') {
    sources = [sources];
  }

  return transformer

  function transformer(tree, file) {
    var toOffset = vfileLocation$3(file).toOffset;
    var initial = !reset;
    var gaps = detectGaps(tree, file);
    var scope = {};
    var globals = [];

    unistUtilVisit$2(tree, test, visitor);

    file.messages = file.messages.filter(filter);

    function visitor(node, position, parent) {
      var mark = marker(node);
      var ruleIds;
      var ruleId;
      var verb;
      var index;
      var length;
      var next;
      var pos;
      var tail;

      if (!mark || mark.name !== options.name) {
        return
      }

      ruleIds = mark.attributes.split(/\s/g);
      verb = ruleIds.shift();
      next = parent.children[position + 1];
      pos = mark.node.position && mark.node.position.start;
      tail = next && next.position && next.position.end;

      if (!verb || !ALLOWED_VERBS[verb] === true) {
        file.fail(
          'Unknown keyword `' +
            verb +
            '`: expected ' +
            "`'enable'`, `'disable'`, or `'ignore'`",
          mark.node
        );
      }

      length = ruleIds.length;
      index = -1;

      while (++index < length) {
        ruleId = ruleIds[index];

        if (isKnown(ruleId, verb, mark.node)) {
          toggle(pos, verb === 'enable', ruleId);

          if (verb === 'ignore') {
            toggle(tail, true, ruleId);
          }
        }
      }

      /* Apply to all rules. */
      if (!length) {
        if (verb === 'ignore') {
          toggle(pos, false);
          toggle(tail, true);
        } else {
          toggle(pos, verb === 'enable');
          reset = verb !== 'enable';
        }
      }
    }

    function filter(message) {
      var gapIndex = gaps.length;
      var ruleId = message.ruleId;
      var ranges = scope[ruleId];
      var pos;

      /* Keep messages from a different source. */
      if (!message.source || sources.indexOf(message.source) === -1) {
        return true
      }

      /* We only ignore messages if they‘re disabled,
       * *not* when they’re not in the document. */
      if (!message.line) {
        message.line = 1;
      }

      if (!message.column) {
        message.column = 1;
      }

      /* Check whether the warning is inside a gap. */
      pos = toOffset(message);

      while (gapIndex--) {
        if (gaps[gapIndex].start <= pos && gaps[gapIndex].end > pos) {
          return false
        }
      }

      /* Check whether allowed by specific and global states. */
      return check(message, ranges, ruleId) && check(message, globals)
    }

    /* Helper to check (and possibly warn) if a ruleId is unknown. */
    function isKnown(ruleId, verb, pos) {
      var result = known ? known.indexOf(ruleId) !== -1 : true;

      if (!result) {
        file.warn('Unknown rule: cannot ' + verb + " `'" + ruleId + "'`", pos);
      }

      return result
    }

    /* Get the latest state of a rule. When without `ruleId`, gets global state. */
    function getState(ruleId) {
      var ranges = ruleId ? scope[ruleId] : globals;

      if (ranges && ranges.length !== 0) {
        return ranges[ranges.length - 1].state
      }

      if (!ruleId) {
        return !reset
      }

      if (reset) {
        return enable.indexOf(ruleId) !== -1
      }

      return disable.indexOf(ruleId) === -1
    }

    /* Handle a rule. */
    function toggle(pos, state, ruleId) {
      var markers = ruleId ? scope[ruleId] : globals;
      var currentState;
      var previousState;

      if (!markers) {
        markers = [];
        scope[ruleId] = markers;
      }

      previousState = getState(ruleId);
      currentState = state;

      if (currentState !== previousState) {
        markers.push({state: currentState, position: pos});
      }

      /* Toggle all known rules. */
      if (!ruleId) {
        for (ruleId in scope) {
          toggle(pos, state, ruleId);
        }
      }
    }

    /* Check all `ranges` for `message`. */
    function check(message, ranges, id) {
      /* Check the state at the message's position. */
      var index = ranges && ranges.length;
      var length = -1;
      var range;

      while (--index > length) {
        range = ranges[index];

        /* istanbul ignore if - generated marker. */
        if (!range.position || !range.position.line || !range.position.column) {
          continue
        }

        if (
          range.position.line < message.line ||
          (range.position.line === message.line &&
            range.position.column < message.column)
        ) {
          return range.state === true
        }
      }

      /* The first marker ocurred after the first
       * message, so we check the initial state. */
      if (!id) {
        return initial || reset
      }

      return reset ? enable.indexOf(id) !== -1 : disable.indexOf(id) === -1
    }
  }
}

/* Detect gaps in `ast`. */
function detectGaps(tree, file) {
  var lastNode = tree.children[tree.children.length - 1];
  var offset = 0;
  var isGap = false;
  var gaps = [];

  /* Find all gaps. */
  unistUtilVisit$2(tree, one);

  /* Get the end of the document.
   * This detects if the last node was the last node.
   * If not, there’s an extra gap between the last node
   * and the end of the document. */
  if (
    lastNode &&
    lastNode.position &&
    lastNode.position.end &&
    offset === lastNode.position.end.offset &&
    trim_1$2(file.toString().slice(offset)) !== ''
  ) {
    update();

    update(
      tree && tree.position && tree.position.end && tree.position.end.offset - 1
    );
  }

  return gaps

  function one(node) {
    var pos = node.position;

    update(pos && pos.start && pos.start.offset);

    if (!node.children) {
      update(pos && pos.end && pos.end.offset);
    }
  }

  /* Detect a new position. */
  function update(latest) {
    if (latest === null || latest === undefined) {
      isGap = true;
      return
    }

    if (offset >= latest) {
      return
    }

    if (isGap) {
      gaps.push({start: offset, end: latest});
      isGap = false;
    }

    offset = latest;
  }
}

/* Expose. */
var mdastCommentMarker = marker$1;

/* HTML type. */
var T_HTML = 'html';

/* Expression for eliminating extra spaces */
var SPACES = /\s+/g;

/* Expression for parsing parameters. */
var PARAMETERS = new RegExp(
  '\\s+' +
    '(' +
    '[-a-z0-9_]+' +
    ')' +
    '(?:' +
    '=' +
    '(?:' +
    '"' +
    '(' +
    '(?:' +
    '\\\\[\\s\\S]' +
    '|' +
    '[^"]' +
    ')+' +
    ')' +
    '"' +
    '|' +
    "'" +
    '(' +
    '(?:' +
    '\\\\[\\s\\S]' +
    '|' +
    "[^']" +
    ')+' +
    ')' +
    "'" +
    '|' +
    '(' +
    '(?:' +
    '\\\\[\\s\\S]' +
    '|' +
    '[^"\'\\s]' +
    ')+' +
    ')' +
    ')' +
    ')?',
  'gi'
);

var MARKER = new RegExp(
  '(' +
    '\\s*' +
    '<!--' +
    '\\s*' +
    '([a-zA-Z0-9-]+)' +
    '(\\s+([\\s\\S]*?))?' +
    '\\s*' +
    '-->' +
    '\\s*' +
    ')'
);

/* Parse a comment marker */
function marker$1(node) {
  var value;
  var match;
  var params;

  if (!node || node.type !== T_HTML) {
    return null
  }

  value = node.value;
  match = value.match(MARKER);

  if (!match || match[1].length !== value.length) {
    return null
  }

  params = parameters(match[3] || '');

  if (!params) {
    return null
  }

  return {
    name: match[2],
    attributes: match[4] || '',
    parameters: params,
    node: node
  }
}

/* Parse `value` into an object. */
function parameters(value) {
  var attributes = {};
  var rest = value.replace(PARAMETERS, replacer);

  return rest.replace(SPACES, '') ? null : attributes

  /* eslint-disable max-params */
  function replacer($0, $1, $2, $3, $4) {
    var result = $2 || $3 || $4 || '';

    if (result === 'true' || result === '') {
      result = true;
    } else if (result === 'false') {
      result = false;
    } else if (!isNaN(result)) {
      result = Number(result);
    }

    attributes[$1] = result;

    return ''
  }
}

var immutable$2 = extend$6;

var hasOwnProperty$1 = Object.prototype.hasOwnProperty;

function extend$6() {
    var target = {};

    for (var i = 0; i < arguments.length; i++) {
        var source = arguments[i];

        for (var key in source) {
            if (hasOwnProperty$1.call(source, key)) {
                target[key] = source[key];
            }
        }
    }

    return target
}

var remarkMessageControl = messageControl$1;

function messageControl$1(options) {
  var settings = options || {};

  return unifiedMessageControl(immutable$2(options, {
    marker: settings.marker || mdastCommentMarker,
    test: settings.test || 'html'
  }));
}

var remarkLint = lint;

/* `remark-lint`.  This adds support for ignoring stuff from
 * messages (`<!--lint ignore-->`).
 * All rules are in their own packages and presets. */
function lint() {
  this.use(lintMessageControl);
}

function lintMessageControl() {
  return remarkMessageControl({name: 'lint', source: 'remark-lint'})
}

/**
 * An Array.prototype.slice.call(arguments) alternative
 *
 * @param {Object} args something with a length
 * @param {Number} slice
 * @param {Number} sliceEnd
 * @api public
 */

var sliced = function (args, slice, sliceEnd) {
  var ret = [];
  var len = args.length;

  if (0 === len) return ret;

  var start = slice < 0
    ? Math.max(0, slice + len)
    : slice || 0;

  if (sliceEnd !== undefined) {
    len = sliceEnd < 0
      ? sliceEnd + len
      : sliceEnd;
  }

  while (len-- > start) {
    ret[len - start] = args[len];
  }

  return ret;
};

/**
 * slice() reference.
 */

var slice$4 = Array.prototype.slice;

/**
 * Expose `co`.
 */

var co_1 = co;

/**
 * Wrap the given generator `fn` and
 * return a thunk.
 *
 * @param {Function} fn
 * @return {Function}
 * @api public
 */

function co(fn) {
  var isGenFun = isGeneratorFunction(fn);

  return function (done) {
    var ctx = this;

    // in toThunk() below we invoke co()
    // with a generator, so optimize for
    // this case
    var gen = fn;

    // we only need to parse the arguments
    // if gen is a generator function.
    if (isGenFun) {
      var args = slice$4.call(arguments), len = args.length;
      var hasCallback = len && 'function' == typeof args[len - 1];
      done = hasCallback ? args.pop() : error;
      gen = fn.apply(this, args);
    } else {
      done = done || error;
    }

    next();

    // #92
    // wrap the callback in a setImmediate
    // so that any of its errors aren't caught by `co`
    function exit(err, res) {
      setImmediate(function(){
        done.call(ctx, err, res);
      });
    }

    function next(err, res) {
      var ret;

      // multiple args
      if (arguments.length > 2) res = slice$4.call(arguments, 1);

      // error
      if (err) {
        try {
          ret = gen.throw(err);
        } catch (e) {
          return exit(e);
        }
      }

      // ok
      if (!err) {
        try {
          ret = gen.next(res);
        } catch (e) {
          return exit(e);
        }
      }

      // done
      if (ret.done) return exit(null, ret.value);

      // normalize
      ret.value = toThunk(ret.value, ctx);

      // run
      if ('function' == typeof ret.value) {
        var called = false;
        try {
          ret.value.call(ctx, function(){
            if (called) return;
            called = true;
            next.apply(ctx, arguments);
          });
        } catch (e) {
          setImmediate(function(){
            if (called) return;
            called = true;
            next(e);
          });
        }
        return;
      }

      // invalid
      next(new TypeError('You may only yield a function, promise, generator, array, or object, '
        + 'but the following was passed: "' + String(ret.value) + '"'));
    }
  }
}

/**
 * Convert `obj` into a normalized thunk.
 *
 * @param {Mixed} obj
 * @param {Mixed} ctx
 * @return {Function}
 * @api private
 */

function toThunk(obj, ctx) {

  if (isGeneratorFunction(obj)) {
    return co(obj.call(ctx));
  }

  if (isGenerator(obj)) {
    return co(obj);
  }

  if (isPromise(obj)) {
    return promiseToThunk(obj);
  }

  if ('function' == typeof obj) {
    return obj;
  }

  if (isObject$3(obj) || Array.isArray(obj)) {
    return objectToThunk.call(ctx, obj);
  }

  return obj;
}

/**
 * Convert an object of yieldables to a thunk.
 *
 * @param {Object} obj
 * @return {Function}
 * @api private
 */

function objectToThunk(obj){
  var ctx = this;
  var isArray = Array.isArray(obj);

  return function(done){
    var keys = Object.keys(obj);
    var pending = keys.length;
    var results = isArray
      ? new Array(pending) // predefine the array length
      : new obj.constructor();
    var finished;

    if (!pending) {
      setImmediate(function(){
        done(null, results);
      });
      return;
    }

    // prepopulate object keys to preserve key ordering
    if (!isArray) {
      for (var i = 0; i < pending; i++) {
        results[keys[i]] = undefined;
      }
    }

    for (var i = 0; i < keys.length; i++) {
      run(obj[keys[i]], keys[i]);
    }

    function run(fn, key) {
      if (finished) return;
      try {
        fn = toThunk(fn, ctx);

        if ('function' != typeof fn) {
          results[key] = fn;
          return --pending || done(null, results);
        }

        fn.call(ctx, function(err, res){
          if (finished) return;

          if (err) {
            finished = true;
            return done(err);
          }

          results[key] = res;
          --pending || done(null, results);
        });
      } catch (err) {
        finished = true;
        done(err);
      }
    }
  }
}

/**
 * Convert `promise` to a thunk.
 *
 * @param {Object} promise
 * @return {Function}
 * @api private
 */

function promiseToThunk(promise) {
  return function(fn){
    promise.then(function(res) {
      fn(null, res);
    }, fn);
  }
}

/**
 * Check if `obj` is a promise.
 *
 * @param {Object} obj
 * @return {Boolean}
 * @api private
 */

function isPromise(obj) {
  return obj && 'function' == typeof obj.then;
}

/**
 * Check if `obj` is a generator.
 *
 * @param {Mixed} obj
 * @return {Boolean}
 * @api private
 */

function isGenerator(obj) {
  return obj && 'function' == typeof obj.next && 'function' == typeof obj.throw;
}

/**
 * Check if `obj` is a generator function.
 *
 * @param {Mixed} obj
 * @return {Boolean}
 * @api private
 */

function isGeneratorFunction(obj) {
  return obj && obj.constructor && 'GeneratorFunction' == obj.constructor.name;
}

/**
 * Check for plain object.
 *
 * @param {Mixed} val
 * @return {Boolean}
 * @api private
 */

function isObject$3(val) {
  return val && Object == val.constructor;
}

/**
 * Throw `err` in a new stack.
 *
 * This is used when co() is invoked
 * without supplying a callback, which
 * should only be for demonstrational
 * purposes.
 *
 * @param {Error} err
 * @api private
 */

function error(err) {
  if (!err) return;
  setImmediate(function(){
    throw err;
  });
}

/**
 * Module Dependencies
 */


var noop$2 = function(){};


/**
 * Export `wrapped`
 */

var wrapped_1 = wrapped;

/**
 * Wrap a function to support
 * sync, async, and gen functions.
 *
 * @param {Function} fn
 * @return {Function}
 * @api public
 */

function wrapped(fn) {
  function wrap() {
    var args = sliced(arguments);
    var last = args[args.length - 1];
    var ctx = this;

    // done
    var done = typeof last == 'function' ? args.pop() : noop$2;

    // nothing
    if (!fn) {
      return done.apply(ctx, [null].concat(args));
    }

    // generator
    if (generator(fn)) {
      return co_1(fn).apply(ctx, args.concat(done));
    }

    // async
    if (fn.length > args.length) {
      // NOTE: this only handles uncaught synchronous errors
      try {
        return fn.apply(ctx, args.concat(done));
      } catch (e) {
        return done(e);
      }
    }

    // sync
    return sync$2(fn, done).apply(ctx, args);
  }

  return wrap;
}

/**
 * Wrap a synchronous function execution.
 *
 * @param {Function} fn
 * @param {Function} done
 * @return {Function}
 * @api private
 */

function sync$2(fn, done) {
  return function () {
    var ret;

    try {
      ret = fn.apply(this, arguments);
    } catch (err) {
      return done(err);
    }

    if (promise(ret)) {
      ret.then(function (value) { done(null, value); }, done);
    } else {
      ret instanceof Error ? done(ret) : done(null, ret);
    }
  }
}

/**
 * Is `value` a generator?
 *
 * @param {Mixed} value
 * @return {Boolean}
 * @api private
 */

function generator(value) {
  return value
    && value.constructor
    && 'GeneratorFunction' == value.constructor.name;
}


/**
 * Is `value` a promise?
 *
 * @param {Mixed} value
 * @return {Boolean}
 * @api private
 */

function promise(value) {
  return value && 'function' == typeof value.then;
}

var unifiedLintRule = factory$7;

function factory$7(id, rule) {
  var parts = id.split(':');
  var source = parts[0];
  var ruleId = parts[1];
  var fn = wrapped_1(rule);

  /* istanbul ignore if - possibly useful if externalised later. */
  if (!ruleId) {
    ruleId = source;
    source = null;
  }

  attacher.displayName = id;

  return attacher

  function attacher(raw) {
    var config = coerce(ruleId, raw);
    var severity = config[0];
    var options = config[1];
    var fatal = severity === 2;

    return severity ? transformer : undefined

    function transformer(tree, file, next) {
      var index = file.messages.length;

      fn(tree, file, options, done);

      function done(err) {
        var messages = file.messages;
        var message;

        /* Add the error, if not already properly added. */
        /* istanbul ignore if - only happens for incorrect plugins */
        if (err && messages.indexOf(err) === -1) {
          try {
            file.fail(err);
          } catch (err) {}
        }

        while (index < messages.length) {
          message = messages[index];
          message.ruleId = ruleId;
          message.source = source;
          message.fatal = fatal;

          index++;
        }

        next();
      }
    }
  }
}

/* Coerce a value to a severity--options tuple. */
function coerce(name, value) {
  var def = 1;
  var result;
  var level;

  /* istanbul ignore if - Handled by unified in v6.0.0 */
  if (typeof value === 'boolean') {
    result = [value];
  } else if (value == null) {
    result = [def];
  } else if (
    typeof value === 'object' &&
    (typeof value[0] === 'number' ||
      typeof value[0] === 'boolean' ||
      typeof value[0] === 'string')
  ) {
    result = value.concat();
  } else {
    result = [1, value];
  }

  level = result[0];

  if (typeof level === 'boolean') {
    level = level ? 1 : 0;
  } else if (typeof level === 'string') {
    if (level === 'off') {
      level = 0;
    } else if (level === 'on' || level === 'warn') {
      level = 1;
    } else if (level === 'error') {
      level = 2;
    } else {
      level = 1;
      result = [level, result];
    }
  }

  if (level < 0 || level > 2) {
    throw new Error(
      'Invalid severity `' +
        level +
        '` for `' +
        name +
        '`, ' +
        'expected 0, 1, or 2'
    )
  }

  result[0] = level;

  return result
}

var unistUtilPosition = createCommonjsModule(function (module, exports) {
var position = exports;

position.start = factory('start');
position.end = factory('end');

/* Factory to get a `type` point in the positional info of a node. */
function factory(type) {
  point.displayName = type;

  return point

  /* Get a point in `node.position` at a bound `type`. */
  function point(node) {
    var point = (node && node.position && node.position[type]) || {};

    return {
      line: point.line || null,
      column: point.column || null,
      offset: isNaN(point.offset) ? null : point.offset
    }
  }
}
});

/* Expose. */
var unistUtilGenerated = generated;

/* Detect if a node was available in the original document. */
function generated(node) {
  var position = optional(optional(node).position);
  var start = optional(position.start);
  var end = optional(position.end);

  return !start.line || !start.column || !end.line || !end.column
}

/* Return `value` if it’s an object, an empty object
 * otherwise. */
function optional(value) {
  return value && typeof value === 'object' ? value : {}
}

var remarkLintCheckboxContentIndent = unifiedLintRule(
  'remark-lint:checkbox-content-indent',
  checkboxContentIndent
);

var start = unistUtilPosition.start;
var end = unistUtilPosition.end;

var reason = 'Checkboxes should be followed by a single character';

function checkboxContentIndent(tree, file) {
  var contents = String(file);
  var location = vfileLocation$3(file);

  unistUtilVisit$2(tree, 'listItem', visitor);

  function visitor(node) {
    var initial;
    var final;
    var value;

    /* Exit early for items without checkbox. */
    if (typeof node.checked !== 'boolean' || unistUtilGenerated(node)) {
      return
    }

    initial = start(node).offset;
    /* istanbul ignore next - hard to test, couldn’t find a case. */
    final = (node.children.length ? start(node.children[0]) : end(node)).offset;

    while (/[^\S\n]/.test(contents.charAt(final))) {
      final++;
    }

    /* For a checkbox to be parsed, it must be followed
     * by a white space. */
    value = contents.slice(initial, final);
    value = value.slice(value.indexOf(']') + 1);

    if (value.length !== 1) {
      file.message(reason, {
        start: location.toPosition(final - value.length + 1),
        end: location.toPosition(final)
      });
    }
  }
}

var remarkLintDefinitionSpacing = unifiedLintRule('remark-lint:definition-spacing', definitionSpacing);

var label$2 = /^\s*\[((?:\\[\s\S]|[^[\]])+)]/;
var reason$1 = 'Do not use consecutive white-space in definition labels';

function definitionSpacing(tree, file) {
  var contents = String(file);

  unistUtilVisit$2(tree, ['definition', 'footnoteDefinition'], validate);

  function validate(node) {
    var start = unistUtilPosition.start(node).offset;
    var end = unistUtilPosition.end(node).offset;

    if (
      !unistUtilGenerated(node) &&
      /[ \t\n]{2,}/.test(contents.slice(start, end).match(label$2)[1])
    ) {
      file.message(reason$1, node);
    }
  }
}

var remarkLintFencedCodeFlag = unifiedLintRule('remark-lint:fenced-code-flag', fencedCodeFlag);

var start$1 = unistUtilPosition.start;
var end$1 = unistUtilPosition.end;

var fence = /^ {0,3}([~`])\1{2,}/;
var reasonInvalid = 'Invalid code-language flag';
var reasonMissing = 'Missing code-language flag';

function fencedCodeFlag(tree, file, pref) {
  var contents = String(file);
  var allowEmpty = false;
  var flags = [];

  if (typeof pref === 'object' && !('length' in pref)) {
    allowEmpty = Boolean(pref.allowEmpty);
    pref = pref.flags;
  }

  if (typeof pref === 'object' && 'length' in pref) {
    flags = String(pref).split(',');
  }

  unistUtilVisit$2(tree, 'code', visitor);

  function visitor(node) {
    var value;

    if (!unistUtilGenerated(node)) {
      if (node.lang) {
        if (flags.length !== 0 && flags.indexOf(node.lang) === -1) {
          file.message(reasonInvalid, node);
        }
      } else {
        value = contents.slice(start$1(node).offset, end$1(node).offset);

        if (!allowEmpty && fence.test(value)) {
          file.message(reasonMissing, node);
        }
      }
    }
  }
}

var remarkLintFinalDefinition = unifiedLintRule('remark-lint:final-definition', finalDefinition);

var start$2 = unistUtilPosition.start;

function finalDefinition(tree, file) {
  var last = null;

  unistUtilVisit$2(tree, visitor, true);

  function visitor(node) {
    var line = start$2(node).line;

    /* Ignore generated nodes. */
    if (node.type === 'root' || unistUtilGenerated(node)) {
      return
    }

    if (node.type === 'definition') {
      if (last !== null && last > line) {
        file.message(
          'Move definitions to the end of the file (after the node at line `' +
            last +
            '`)',
          node
        );
      }
    } else if (last === null) {
      last = line;
    }
  }
}

var remarkLintFinalNewline = unifiedLintRule('remark-lint:final-newline', finalNewline);

function finalNewline(tree, file) {
  var contents = String(file);
  var last = contents.length - 1;

  if (last > -1 && contents.charAt(last) !== '\n') {
    file.message('Missing newline character at end of file');
  }
}

var remarkLintHardBreakSpaces = unifiedLintRule('remark-lint:hard-break-spaces', hardBreakSpaces);

var reason$2 = 'Use two spaces for hard line breaks';

function hardBreakSpaces(tree, file) {
  var contents = String(file);

  unistUtilVisit$2(tree, 'break', visitor);

  function visitor(node) {
    var value;

    if (!unistUtilGenerated(node)) {
      value = contents
        .slice(unistUtilPosition.start(node).offset, unistUtilPosition.end(node).offset)
        .split('\n', 1)[0]
        .replace(/\r$/, '');

      if (value.length > 2) {
        file.message(reason$2, node);
      }
    }
  }
}

var mdastUtilToString = toString$5;

/* Get the text content of a node.  If the node itself
 * does not expose plain-text fields, `toString` will
 * recursivly try its children. */
function toString$5(node) {
  return (
    valueOf$1(node) ||
    (node.children && node.children.map(toString$5).join('')) ||
    ''
  )
}

/* Get the value of `node`.  Checks, `value`,
 * `alt`, and `title`, in that order. */
function valueOf$1(node) {
  return (
    (node && node.value ? node.value : node.alt ? node.alt : node.title) || ''
  )
}

var remarkLintNoAutoLinkWithoutProtocol = unifiedLintRule(
  'remark-lint:no-auto-link-without-protocol',
  noAutoLinkWithoutProtocol
);

var start$3 = unistUtilPosition.start;
var end$2 = unistUtilPosition.end;

/* Protocol expression. See:
 * http://en.wikipedia.org/wiki/URI_scheme#Generic_syntax */
var protocol$1 = /^[a-z][a-z+.-]+:\/?/i;

var reason$3 = 'All automatic links must start with a protocol';

function noAutoLinkWithoutProtocol(tree, file) {
  unistUtilVisit$2(tree, 'link', visitor);

  function visitor(node) {
    var children;

    if (!unistUtilGenerated(node)) {
      children = node.children;

      if (
        start$3(node).column === start$3(children[0]).column - 1 &&
        end$2(node).column === end$2(children[children.length - 1]).column + 1 &&
        !protocol$1.test(mdastUtilToString(node))
      ) {
        file.message(reason$3, node);
      }
    }
  }
}

var remarkLintNoBlockquoteWithoutCaret = unifiedLintRule('remark-lint:no-blockquote-without-caret', noBlockquoteWithoutCaret);

function noBlockquoteWithoutCaret(ast, file) {
  var contents = file.toString();
  var location = vfileLocation$3(file);
  var last = contents.length;

  unistUtilVisit$2(ast, 'blockquote', visitor);

  function visitor(node) {
    var start = unistUtilPosition.start(node).line;
    var indent = node.position && node.position.indent;

    if (unistUtilGenerated(node) || !indent || indent.length === 0) {
      return;
    }

    indent.forEach(eachLine);

    function eachLine(column, n) {
      var character;
      var line = start + n + 1;
      var offset = location.toOffset({
        line: line,
        column: column
      }) - 1;

      while (++offset < last) {
        character = contents.charAt(offset);

        if (character === '>') {
          return;
        }

        /* istanbul ignore else - just for safety */
        if (character !== ' ' && character !== '\t') {
          break;
        }
      }

      file.message('Missing caret in blockquote', {
        line: line,
        column: column
      });
    }
  }
}

var own$6 = {}.hasOwnProperty;

var unistUtilStringifyPosition$2 = stringify$9;

function stringify$9(value) {
  /* Nothing. */
  if (!value || typeof value !== 'object') {
    return null
  }

  /* Node. */
  if (own$6.call(value, 'position') || own$6.call(value, 'type')) {
    return position$2(value.position)
  }

  /* Position. */
  if (own$6.call(value, 'start') || own$6.call(value, 'end')) {
    return position$2(value)
  }

  /* Point. */
  if (own$6.call(value, 'line') || own$6.call(value, 'column')) {
    return point$1(value)
  }

  /* ? */
  return null
}

function point$1(point) {
  if (!point || typeof point !== 'object') {
    point = {};
  }

  return index$6(point.line) + ':' + index$6(point.column)
}

function position$2(pos) {
  if (!pos || typeof pos !== 'object') {
    pos = {};
  }

  return point$1(pos.start) + '-' + point$1(pos.end)
}

function index$6(value) {
  return value && typeof value === 'number' ? value : 1
}

var remarkLintNoDuplicateDefinitions = unifiedLintRule(
  'remark-lint:no-duplicate-definitions',
  noDuplicateDefinitions
);

var reason$4 = 'Do not use definitions with the same identifier';

function noDuplicateDefinitions(tree, file) {
  var map = {};

  unistUtilVisit$2(tree, ['definition', 'footnoteDefinition'], validate);

  function validate(node) {
    var identifier;
    var duplicate;

    if (!unistUtilGenerated(node)) {
      identifier = node.identifier;
      duplicate = map[identifier];

      if (duplicate && duplicate.type) {
        file.message(
          reason$4 + ' (' + unistUtilStringifyPosition$2(unistUtilPosition.start(duplicate)) + ')',
          node
        );
      }

      map[identifier] = node;
    }
  }
}

var remarkLintNoFileNameArticles = unifiedLintRule('remark-lint:no-file-name-articles', noFileNameArticles);

function noFileNameArticles(tree, file) {
  var match = file.stem && file.stem.match(/^(the|teh|an?)\b/i);

  if (match) {
    file.message('Do not start file names with `' + match[0] + '`');
  }
}

var remarkLintNoFileNameConsecutiveDashes = unifiedLintRule(
  'remark-lint:no-file-name-consecutive-dashes',
  noFileNameConsecutiveDashes
);

var reason$5 = 'Do not use consecutive dashes in a file name';

function noFileNameConsecutiveDashes(tree, file) {
  if (file.stem && /-{2,}/.test(file.stem)) {
    file.message(reason$5);
  }
}

var remarkLintNoFileNameOuterDashes = unifiedLintRule(
  'remark-lint:no-file-name-outer-dashes',
  noFileNameOuterDashes
);

var reason$6 = 'Do not use initial or final dashes in a file name';

function noFileNameOuterDashes(tree, file) {
  if (file.stem && /^-|-$/.test(file.stem)) {
    file.message(reason$6);
  }
}

var mdastUtilHeadingStyle = style;

function style(node, relative) {
  var last = node.children[node.children.length - 1];
  var depth = node.depth;
  var pos = node && node.position && node.position.end;
  var final = last && last.position && last.position.end;

  if (!pos) {
    return null
  }

  /* This can only occur for `'atx'` and `'atx-closed'`
   * headings.  This might incorrectly match `'atx'`
   * headings with lots of trailing white space as an
   * `'atx-closed'` heading. */
  if (!last) {
    if (pos.column - 1 <= depth * 2) {
      return consolidate(depth, relative)
    }

    return 'atx-closed'
  }

  if (final.line + 1 === pos.line) {
    return 'setext'
  }

  if (final.column + depth < pos.column) {
    return 'atx-closed'
  }

  return consolidate(depth, relative)
}

/* Get the probable style of an atx-heading, depending on
 * preferred style. */
function consolidate(depth, relative) {
  return depth < 3
    ? 'atx'
    : relative === 'atx' || relative === 'setext'
      ? relative
      : null
}

const addendum = "addenda";
const aircraft = "aircraft";
const alga = "algae";
const alumna = "alumnae";
const alumnus = "alumni";
const amoeba = "amoebae";
const analysis = "analyses";
const antenna = "antennae";
const antithesis = "antitheses";
const apex = "apices";
const appendix = "appendices";
const automaton = "automata";
const axis = "axes";
const bacillus = "bacilli";
const bacterium = "bacteria";
const barracks = "barracks";
const basis = "bases";
const beau = "beaux";
const bison = "bison";
const buffalo = "buffalo";
const bureau = "bureaus";
const cactus = "cacti";
const calf = "calves";
const carp = "carp";
const census = "censuses";
const chassis = "chassis";
const cherub = "cherubim";
const child = "children";
const cod = "cod";
const codex = "codices";
const concerto = "concerti";
const corpus = "corpora";
const crisis = "crises";
const criterion = "criteria";
const curriculum = "curricula";
const datum = "data";
const deer = "deer";
const diagnosis = "diagnoses";
const die$1 = "dice";
const dwarf = "dwarfs";
const echo = "echoes";
const elf = "elves";
const elk = "elk";
const ellipsis = "ellipses";
const embargo = "embargoes";
const emphasis$4 = "emphases";
const erratum = "errata";
const fez = "fezes";
const firmware = "firmware";
const fish = "fish";
const focus = "foci";
const foot = "feet";
const formula = "formulae";
const fungus = "fungi";
const gallows = "gallows";
const genus = "genera";
const goose = "geese";
const graffito = "graffiti";
const grouse = "grouse";
const half$1 = "halves";
const hero = "heroes";
const hoof = "hooves";
const hovercraft = "hovercraft";
const hypothesis = "hypotheses";
const index$7 = "indices";
const kakapo = "kakapo";
const knife = "knives";
const larva = "larvae";
const leaf = "leaves";
const libretto = "libretti";
const life = "lives";
const loaf = "loaves";
const locus = "loci";
const louse = "lice";
const man = "men";
const matrix = "matrices";
const means = "means";
const medium = "media";
const memorandum = "memoranda";
const millennium = "millennia";
const minutia = "minutiae";
const moose = "moose";
const mouse = "mice";
const nebula = "nebulae";
const nemesis = "nemeses";
const neurosis = "neuroses";
const news = "news";
const nucleus = "nuclei";
const oasis = "oases";
const offspring = "offspring";
const opus = "opera";
const ovum = "ova";
const ox = "oxen";
const paralysis = "paralyses";
const parenthesis = "parentheses";
const person = "people";
const phenomenon = "phenomena";
const phylum = "phyla";
const pike = "pike";
const polyhedron = "polyhedra";
const potato = "potatoes";
const prognosis = "prognoses";
const quiz = "quizzes";
const radius = "radii";
const referendum = "referenda";
const salmon = "salmon";
const scarf = "scarves";
const self$1 = "selves";
const series = "series";
const sheep = "sheep";
const shelf = "shelves";
const shrimp = "shrimp";
const spacecraft = "spacecraft";
const species = "species";
const spectrum = "spectra";
const squid = "squid";
const stimulus = "stimuli";
const stratum = "strata";
const swine = "swine";
const syllabus = "syllabi";
const symposium = "symposia";
const synopsis = "synopses";
const synthesis = "syntheses";
const tableau = "tableaus";
const that = "those";
const thesis = "theses";
const thief = "thieves";
const tomato = "tomatoes";
const tooth = "teeth";
const trout = "trout";
const tuna = "tuna";
const vertebra = "vertebrae";
const vertex = "vertices";
const veto = "vetoes";
const vita = "vitae";
const vortex = "vortices";
const watercraft = "watercraft";
const wharf = "wharves";
const wife = "wives";
const wolf = "wolves";
const woman = "women";
var irregularPlurals = {
	addendum: addendum,
	aircraft: aircraft,
	alga: alga,
	alumna: alumna,
	alumnus: alumnus,
	amoeba: amoeba,
	analysis: analysis,
	antenna: antenna,
	antithesis: antithesis,
	apex: apex,
	appendix: appendix,
	automaton: automaton,
	axis: axis,
	bacillus: bacillus,
	bacterium: bacterium,
	barracks: barracks,
	basis: basis,
	beau: beau,
	bison: bison,
	buffalo: buffalo,
	bureau: bureau,
	cactus: cactus,
	calf: calf,
	carp: carp,
	census: census,
	chassis: chassis,
	cherub: cherub,
	child: child,
	cod: cod,
	codex: codex,
	concerto: concerto,
	corpus: corpus,
	crisis: crisis,
	criterion: criterion,
	curriculum: curriculum,
	datum: datum,
	deer: deer,
	diagnosis: diagnosis,
	die: die$1,
	dwarf: dwarf,
	echo: echo,
	elf: elf,
	elk: elk,
	ellipsis: ellipsis,
	embargo: embargo,
	emphasis: emphasis$4,
	erratum: erratum,
	fez: fez,
	firmware: firmware,
	fish: fish,
	focus: focus,
	foot: foot,
	formula: formula,
	fungus: fungus,
	gallows: gallows,
	genus: genus,
	goose: goose,
	graffito: graffito,
	grouse: grouse,
	half: half$1,
	hero: hero,
	hoof: hoof,
	hovercraft: hovercraft,
	hypothesis: hypothesis,
	index: index$7,
	kakapo: kakapo,
	knife: knife,
	larva: larva,
	leaf: leaf,
	libretto: libretto,
	life: life,
	loaf: loaf,
	locus: locus,
	louse: louse,
	man: man,
	matrix: matrix,
	means: means,
	medium: medium,
	memorandum: memorandum,
	millennium: millennium,
	minutia: minutia,
	moose: moose,
	mouse: mouse,
	nebula: nebula,
	nemesis: nemesis,
	neurosis: neurosis,
	news: news,
	nucleus: nucleus,
	oasis: oasis,
	offspring: offspring,
	opus: opus,
	ovum: ovum,
	ox: ox,
	paralysis: paralysis,
	parenthesis: parenthesis,
	person: person,
	phenomenon: phenomenon,
	phylum: phylum,
	pike: pike,
	polyhedron: polyhedron,
	potato: potato,
	prognosis: prognosis,
	quiz: quiz,
	radius: radius,
	referendum: referendum,
	salmon: salmon,
	scarf: scarf,
	self: self$1,
	series: series,
	sheep: sheep,
	shelf: shelf,
	shrimp: shrimp,
	spacecraft: spacecraft,
	species: species,
	spectrum: spectrum,
	squid: squid,
	stimulus: stimulus,
	stratum: stratum,
	swine: swine,
	syllabus: syllabus,
	symposium: symposium,
	synopsis: synopsis,
	synthesis: synthesis,
	tableau: tableau,
	that: that,
	thesis: thesis,
	thief: thief,
	tomato: tomato,
	tooth: tooth,
	trout: trout,
	tuna: tuna,
	vertebra: vertebra,
	vertex: vertex,
	veto: veto,
	vita: vita,
	vortex: vortex,
	watercraft: watercraft,
	wharf: wharf,
	wife: wife,
	wolf: wolf,
	woman: woman,
	"château": "châteaus",
	"faux pas": "faux pas",
	"this": "these"
};

var irregularPlurals$1 = Object.freeze({
	addendum: addendum,
	aircraft: aircraft,
	alga: alga,
	alumna: alumna,
	alumnus: alumnus,
	amoeba: amoeba,
	analysis: analysis,
	antenna: antenna,
	antithesis: antithesis,
	apex: apex,
	appendix: appendix,
	automaton: automaton,
	axis: axis,
	bacillus: bacillus,
	bacterium: bacterium,
	barracks: barracks,
	basis: basis,
	beau: beau,
	bison: bison,
	buffalo: buffalo,
	bureau: bureau,
	cactus: cactus,
	calf: calf,
	carp: carp,
	census: census,
	chassis: chassis,
	cherub: cherub,
	child: child,
	cod: cod,
	codex: codex,
	concerto: concerto,
	corpus: corpus,
	crisis: crisis,
	criterion: criterion,
	curriculum: curriculum,
	datum: datum,
	deer: deer,
	diagnosis: diagnosis,
	die: die$1,
	dwarf: dwarf,
	echo: echo,
	elf: elf,
	elk: elk,
	ellipsis: ellipsis,
	embargo: embargo,
	emphasis: emphasis$4,
	erratum: erratum,
	fez: fez,
	firmware: firmware,
	fish: fish,
	focus: focus,
	foot: foot,
	formula: formula,
	fungus: fungus,
	gallows: gallows,
	genus: genus,
	goose: goose,
	graffito: graffito,
	grouse: grouse,
	half: half$1,
	hero: hero,
	hoof: hoof,
	hovercraft: hovercraft,
	hypothesis: hypothesis,
	index: index$7,
	kakapo: kakapo,
	knife: knife,
	larva: larva,
	leaf: leaf,
	libretto: libretto,
	life: life,
	loaf: loaf,
	locus: locus,
	louse: louse,
	man: man,
	matrix: matrix,
	means: means,
	medium: medium,
	memorandum: memorandum,
	millennium: millennium,
	minutia: minutia,
	moose: moose,
	mouse: mouse,
	nebula: nebula,
	nemesis: nemesis,
	neurosis: neurosis,
	news: news,
	nucleus: nucleus,
	oasis: oasis,
	offspring: offspring,
	opus: opus,
	ovum: ovum,
	ox: ox,
	paralysis: paralysis,
	parenthesis: parenthesis,
	person: person,
	phenomenon: phenomenon,
	phylum: phylum,
	pike: pike,
	polyhedron: polyhedron,
	potato: potato,
	prognosis: prognosis,
	quiz: quiz,
	radius: radius,
	referendum: referendum,
	salmon: salmon,
	scarf: scarf,
	self: self$1,
	series: series,
	sheep: sheep,
	shelf: shelf,
	shrimp: shrimp,
	spacecraft: spacecraft,
	species: species,
	spectrum: spectrum,
	squid: squid,
	stimulus: stimulus,
	stratum: stratum,
	swine: swine,
	syllabus: syllabus,
	symposium: symposium,
	synopsis: synopsis,
	synthesis: synthesis,
	tableau: tableau,
	that: that,
	thesis: thesis,
	thief: thief,
	tomato: tomato,
	tooth: tooth,
	trout: trout,
	tuna: tuna,
	vertebra: vertebra,
	vertex: vertex,
	veto: veto,
	vita: vita,
	vortex: vortex,
	watercraft: watercraft,
	wharf: wharf,
	wife: wife,
	wolf: wolf,
	woman: woman,
	default: irregularPlurals
});

var irregularPlurals$2 = ( irregularPlurals$1 && irregularPlurals ) || irregularPlurals$1;

var irregularPlurals_1 = createCommonjsModule(function (module) {
const map = new Map();
// TODO: Use Object.entries when targeting Node.js 8
for (const key of Object.keys(irregularPlurals$2)) {
	map.set(key, irregularPlurals$2[key]);
}

// Ensure nobody can modify each others Map
Object.defineProperty(module, 'exports', {
	get() {
		return map;
	}
});
});

var plur = createCommonjsModule(function (module) {
module.exports = (word, plural, count) => {
	if (typeof plural === 'number') {
		count = plural;
	}

	if (irregularPlurals_1.has(word.toLowerCase())) {
		plural = irregularPlurals_1.get(word.toLowerCase());

		const firstLetter = word.charAt(0);
		const isFirstLetterUpperCase = firstLetter === firstLetter.toUpperCase();
		if (isFirstLetterUpperCase) {
			plural = firstLetter.toUpperCase() + plural.slice(1);
		}

		const isWholeWordUpperCase = word === word.toUpperCase();
		if (isWholeWordUpperCase) {
			plural = plural.toUpperCase();
		}
	} else if (typeof plural !== 'string') {
		plural = (word.replace(/(?:s|x|z|ch|sh)$/i, '$&e').replace(/([^aeiou])y$/i, '$1ie') + 's')
			.replace(/i?e?s$/i, m => {
				const isTailLowerCase = word.slice(-1) === word.slice(-1).toLowerCase();
				return isTailLowerCase ? m.toLowerCase() : m.toUpperCase();
			});
	}

	return Math.abs(count) === 1 ? word : plural;
};
});

var remarkLintNoHeadingContentIndent = unifiedLintRule(
  'remark-lint:no-heading-content-indent',
  noHeadingContentIndent
);

var start$4 = unistUtilPosition.start;
var end$3 = unistUtilPosition.end;

function noHeadingContentIndent(tree, file) {
  var contents = String(file);

  unistUtilVisit$2(tree, 'heading', visitor);

  function visitor(node) {
    var depth;
    var children;
    var type;
    var head;
    var initial;
    var final;
    var diff;
    var index;
    var char;
    var reason;

    if (unistUtilGenerated(node)) {
      return
    }

    depth = node.depth;
    children = node.children;
    type = mdastUtilHeadingStyle(node, 'atx');

    if (type === 'atx' || type === 'atx-closed') {
      initial = start$4(node);
      index = initial.offset;
      char = contents.charAt(index);

      while (char && char !== '#') {
        char = contents.charAt(++index);
      }

      /* istanbul ignore if - CR/LF bug: remarkjs/remark#195. */
      if (!char) {
        return
      }

      index = depth + (index - initial.offset);
      head = start$4(children[0]).column;

      /* Ignore empty headings. */
      if (!head) {
        return
      }

      diff = head - initial.column - 1 - index;

      if (diff) {
        reason =
          (diff > 0 ? 'Remove' : 'Add') +
          ' ' +
          Math.abs(diff) +
          ' ' +
          plur('space', diff) +
          ' before this heading’s content';

        file.message(reason, start$4(children[0]));
      }
    }

    /* Closed ATX-heading always must have a space
     * between their content and the final hashes,
     * thus, there is no `add x spaces`. */
    if (type === 'atx-closed') {
      final = end$3(children[children.length - 1]);
      diff = end$3(node).column - final.column - 1 - depth;

      if (diff) {
        reason =
          'Remove ' +
          diff +
          ' ' +
          plur('space', diff) +
          ' after this heading’s content';

        file.message(reason, final);
      }
    }
  }
}

var remarkLintNoHeadingIndent = unifiedLintRule('remark-lint:no-heading-indent', noHeadingIndent);

var start$5 = unistUtilPosition.start;

function noHeadingIndent(tree, file) {
  var contents = String(file);
  var length = contents.length;

  unistUtilVisit$2(tree, 'heading', visitor);

  function visitor(node) {
    var initial;
    var begin;
    var index;
    var character;
    var diff;

    if (unistUtilGenerated(node)) {
      return
    }

    initial = start$5(node);
    begin = initial.offset;
    index = begin - 1;

    while (++index < length) {
      character = contents.charAt(index);

      if (character !== ' ' && character !== '\t') {
        break
      }
    }

    diff = index - begin;

    if (diff) {
      file.message(
        'Remove ' + diff + ' ' + plur('space', diff) + ' before this heading',
        {
          line: initial.line,
          column: initial.column + diff
        }
      );
    }
  }
}

var remarkLintNoInlinePadding = unifiedLintRule('remark-lint:no-inline-padding', noInlinePadding);

function noInlinePadding(tree, file) {
  unistUtilVisit$2(tree, ['emphasis', 'strong', 'delete', 'image', 'link'], visitor);

  function visitor(node) {
    var contents;

    if (!unistUtilGenerated(node)) {
      contents = mdastUtilToString(node);

      if (
        contents.charAt(0) === ' ' ||
        contents.charAt(contents.length - 1) === ' '
      ) {
        file.message('Don’t pad `' + node.type + '` with inner spaces', node);
      }
    }
  }
}

var start$6 = unistUtilPosition.start;



var remarkLintNoMultipleToplevelHeadings = unifiedLintRule(
  'remark-lint:no-multiple-toplevel-headings',
  noMultipleToplevelHeadings
);

function noMultipleToplevelHeadings(tree, file, pref) {
  var style = pref ? pref : 1;
  var duplicate;

  unistUtilVisit$2(tree, 'heading', visitor);

  function visitor(node) {
    if (!unistUtilGenerated(node) && node.depth === style) {
      if (duplicate) {
        file.message(
          'Don’t use multiple top level headings (' + duplicate + ')',
          node
        );
      } else {
        duplicate = unistUtilStringifyPosition$2(start$6(node));
      }
    }
  }
}

var remarkLintNoShellDollars = unifiedLintRule('remark-lint:no-shell-dollars', noShellDollars);

var reason$7 = 'Do not use dollar signs before shell-commands';

/* List of shell script file extensions (also used as code
 * flags for syntax highlighting on GitHub):
 * https://github.com/github/linguist/blob/5bf8cf5/lib/
 * linguist/languages.yml#L3002. */
var flags = [
  'sh',
  'bash',
  'bats',
  'cgi',
  'command',
  'fcgi',
  'ksh',
  'tmux',
  'tool',
  'zsh'
];

function noShellDollars(tree, file) {
  unistUtilVisit$2(tree, 'code', visitor);

  function visitor(node) {
    var lines;
    var line;
    var length;
    var index;

    /* Check both known shell-code and unknown code. */
    if (!unistUtilGenerated(node) && node.lang && flags.indexOf(node.lang) !== -1) {
      lines = node.value.split('\n');
      length = lines.length;
      index = -1;

      if (length <= 1) {
        return
      }

      while (++index < length) {
        line = lines[index];

        if (line.trim() && !line.match(/^\s*\$\s*/)) {
          return
        }
      }

      file.message(reason$7, node);
    }
  }
}

var remarkLintNoShortcutReferenceImage = unifiedLintRule(
  'remark-lint:no-shortcut-reference-image',
  noShortcutReferenceImage
);

var reason$8 = 'Use the trailing [] on reference images';

function noShortcutReferenceImage(tree, file) {
  unistUtilVisit$2(tree, 'imageReference', visitor);

  function visitor(node) {
    if (!unistUtilGenerated(node) && node.referenceType === 'shortcut') {
      file.message(reason$8, node);
    }
  }
}

var remarkLintNoTableIndentation = unifiedLintRule('remark-lint:no-table-indentation', noTableIndentation);

var reason$9 = 'Do not indent table rows';

function noTableIndentation(tree, file) {
  var contents = String(file);

  unistUtilVisit$2(tree, 'table', visitor);

  function visitor(node) {
    if (!unistUtilGenerated(node)) {
      node.children.forEach(each);
    }

    return unistUtilVisit$2.SKIP
  }

  function each(row) {
    var fence = contents.slice(
      unistUtilPosition.start(row).offset,
      unistUtilPosition.start(row.children[0]).offset
    );

    if (fence.indexOf('|') > 1) {
      file.message(reason$9, row);
    }
  }
}

var remarkLintNoTabs = unifiedLintRule('remark-lint:no-tabs', noTabs);

var reason$10 = 'Use spaces instead of hard-tabs';

function noTabs(tree, file) {
  var content = String(file);
  var position = vfileLocation$3(file).toPosition;
  var index = content.indexOf('\t');

  while (index !== -1) {
    file.message(reason$10, position(index));
    index = content.indexOf('\t', index + 1);
  }
}

var remarkLintNoUnusedDefinitions = unifiedLintRule('remark-lint:no-unused-definitions', noUnusedDefinitions);

var reason$11 = 'Found unused definition';

function noUnusedDefinitions(tree, file) {
  var map = {};
  var identifier;
  var entry;

  unistUtilVisit$2(tree, ['definition', 'footnoteDefinition'], find);
  unistUtilVisit$2(tree, ['imageReference', 'linkReference', 'footnoteReference'], mark);

  for (identifier in map) {
    entry = map[identifier];

    if (!entry.used) {
      file.message(reason$11, entry.node);
    }
  }

  function find(node) {
    if (!unistUtilGenerated(node)) {
      map[node.identifier.toUpperCase()] = {node: node, used: false};
    }
  }

  function mark(node) {
    var info = map[node.identifier.toUpperCase()];

    if (!unistUtilGenerated(node) && info) {
      info.used = true;
    }
  }
}

var rule$1 = unifiedLintRule;




var remarkLintRuleStyle = rule$1('remark-lint:rule-style', ruleStyle);

var start$7 = unistUtilPosition.start;
var end$4 = unistUtilPosition.end;

function ruleStyle(tree, file, pref) {
  var contents = String(file);

  pref = typeof pref === 'string' && pref !== 'consistent' ? pref : null;

  if (pref !== null && /[^-_* ]/.test(pref)) {
    file.fail(
      "Invalid preferred rule-style: provide a valid markdown rule, or `'consistent'`"
    );
  }

  unistUtilVisit$2(tree, 'thematicBreak', visitor);

  function visitor(node) {
    var initial = start$7(node).offset;
    var final = end$4(node).offset;
    var rule;

    if (!unistUtilGenerated(node)) {
      rule = contents.slice(initial, final);

      if (pref) {
        if (rule !== pref) {
          file.message('Rules should use `' + pref + '`', node);
        }
      } else {
        pref = rule;
      }
    }
  }
}

var remarkLintTablePipes = unifiedLintRule('remark-lint:table-pipes', tablePipes);

var start$8 = unistUtilPosition.start;
var end$5 = unistUtilPosition.end;

var reasonStart = 'Missing initial pipe in table fence';
var reasonEnd = 'Missing final pipe in table fence';

function tablePipes(tree, file) {
  var contents = String(file);

  unistUtilVisit$2(tree, 'table', visitor);

  function visitor(node) {
    var rows = node.children;
    var length = rows.length;
    var index = -1;
    var row;
    var cells;
    var head;
    var tail;
    var initial;
    var final;

    while (++index < length) {
      row = rows[index];

      if (!unistUtilGenerated(row)) {
        cells = row.children;
        head = cells[0];
        tail = cells[cells.length - 1];
        initial = contents.slice(start$8(row).offset, start$8(head).offset);
        final = contents.slice(end$5(tail).offset, end$5(row).offset);

        if (initial.indexOf('|') === -1) {
          file.message(reasonStart, start$8(row));
        }

        if (final.indexOf('|') === -1) {
          file.message(reasonEnd, end$5(row));
        }
      }
    }
  }
}

var remarkLintBlockquoteIndentation = unifiedLintRule(
  'remark-lint:blockquote-indentation',
  blockquoteIndentation
);

function blockquoteIndentation(tree, file, pref) {
  pref = typeof pref === 'number' && !isNaN(pref) ? pref : null;

  unistUtilVisit$2(tree, 'blockquote', visitor);

  function visitor(node) {
    var diff;
    var reason;

    if (unistUtilGenerated(node) || node.children.length === 0) {
      return
    }

    if (pref) {
      diff = pref - check$3(node);

      if (diff !== 0) {
        reason =
          (diff > 0 ? 'Add' : 'Remove') +
          ' ' +
          Math.abs(diff) +
          ' ' +
          plur('space', diff) +
          ' between blockquote and content';

        file.message(reason, unistUtilPosition.start(node.children[0]));
      }
    } else {
      pref = check$3(node);
    }
  }
}

function check$3(node) {
  var head = node.children[0];
  var indentation = unistUtilPosition.start(head).column - unistUtilPosition.start(node).column;
  var padding = mdastUtilToString(head).match(/^ +/);

  if (padding) {
    indentation += padding[0].length;
  }

  return indentation
}

var remarkLintCheckboxCharacterStyle = unifiedLintRule(
  'remark-lint:checkbox-character-style',
  checkboxCharacterStyle
);

var start$9 = unistUtilPosition.start;
var end$6 = unistUtilPosition.end;

var checked = {x: true, X: true};
var unchecked = {' ': true, '\t': true};
var types = {true: 'checked', false: 'unchecked'};

function checkboxCharacterStyle(tree, file, pref) {
  var contents = String(file);
  var location = vfileLocation$3(file);

  pref = typeof pref === 'object' ? pref : {};

  if (pref.unchecked && unchecked[pref.unchecked] !== true) {
    file.fail(
      'Invalid unchecked checkbox marker `' +
        pref.unchecked +
        "`: use either `'\\t'`, or `' '`"
    );
  }

  if (pref.checked && checked[pref.checked] !== true) {
    file.fail(
      'Invalid checked checkbox marker `' +
        pref.checked +
        "`: use either `'x'`, or `'X'`"
    );
  }

  unistUtilVisit$2(tree, 'listItem', visitor);

  function visitor(node) {
    var type;
    var initial;
    var final;
    var value;
    var style;
    var character;
    var reason;

    /* Exit early for items without checkbox. */
    if (typeof node.checked !== 'boolean' || unistUtilGenerated(node)) {
      return
    }

    type = types[node.checked];
    initial = start$9(node).offset;
    final = (node.children.length ? start$9(node.children[0]) : end$6(node)).offset;

    /* For a checkbox to be parsed, it must be followed by a white space. */
    value = contents
      .slice(initial, final)
      .trimRight()
      .slice(0, -1);

    /* The checkbox character is behind a square bracket. */
    character = value.charAt(value.length - 1);
    style = pref[type];

    if (style) {
      if (character !== style) {
        reason =
          type.charAt(0).toUpperCase() +
          type.slice(1) +
          ' checkboxes should use `' +
          style +
          '` as a marker';

        file.message(reason, {
          start: location.toPosition(initial + value.length - 1),
          end: location.toPosition(initial + value.length)
        });
      }
    } else {
      pref[type] = character;
    }
  }
}

var remarkLintCodeBlockStyle = unifiedLintRule('remark-lint:code-block-style', codeBlockStyle);

var start$10 = unistUtilPosition.start;
var end$7 = unistUtilPosition.end;

var styles = {null: true, fenced: true, indented: true};

function codeBlockStyle(tree, file, pref) {
  var contents = String(file);

  pref = typeof pref === 'string' && pref !== 'consistent' ? pref : null;

  if (styles[pref] !== true) {
    file.fail(
      'Invalid code block style `' +
        pref +
        "`: use either `'consistent'`, `'fenced'`, or `'indented'`"
    );
  }

  unistUtilVisit$2(tree, 'code', visitor);

  function visitor(node) {
    var current = check(node);

    if (current) {
      if (!pref) {
        pref = current;
      } else if (pref !== current) {
        file.message('Code blocks should be ' + pref, node);
      }
    }
  }

  /* Get the style of `node`. */
  function check(node) {
    var initial = start$10(node).offset;
    var final = end$7(node).offset;

    if (unistUtilGenerated(node)) {
      return null
    }

    return node.lang || /^\s*([~`])\1{2,}/.test(contents.slice(initial, final))
      ? 'fenced'
      : 'indented'
  }
}

var remarkLintFencedCodeMarker = unifiedLintRule('remark-lint:fenced-code-marker', fencedCodeMarker);

var markers = {
  '`': true,
  '~': true,
  null: true
};

function fencedCodeMarker(tree, file, pref) {
  var contents = String(file);

  pref = typeof pref === 'string' && pref !== 'consistent' ? pref : null;

  if (markers[pref] !== true) {
    file.fail(
      'Invalid fenced code marker `' +
        pref +
        "`: use either `'consistent'`, `` '`' ``, or `'~'`"
    );
  }

  unistUtilVisit$2(tree, 'code', visitor);

  function visitor(node) {
    var marker;

    if (!unistUtilGenerated(node)) {
      marker = contents
        .substr(unistUtilPosition.start(node).offset, 4)
        .trimLeft()
        .charAt(0);

      /* Ignore unfenced code blocks. */
      if (markers[marker] === true) {
        if (pref) {
          if (marker !== pref) {
            file.message(
              'Fenced code should use ' + pref + ' as a marker',
              node
            );
          }
        } else {
          pref = marker;
        }
      }
    }
  }
}

var remarkLintFileExtension = unifiedLintRule('remark-lint:file-extension', fileExtension);

function fileExtension(tree, file, pref) {
  var ext = file.extname;

  pref = typeof pref === 'string' ? pref : 'md';

  if (ext && ext.slice(1) !== pref) {
    file.message('Invalid extension: use `' + pref + '`');
  }
}

var remarkLintFirstHeadingLevel = unifiedLintRule('remark-lint:first-heading-level', firstHeadingLevel);

var re$4 = /<h([1-6])/;

function firstHeadingLevel(tree, file, pref) {
  var style = pref && pref !== true ? pref : 1;

  unistUtilVisit$2(tree, visitor);

  function visitor(node) {
    var depth;

    if (!unistUtilGenerated(node)) {
      if (node.type === 'heading') {
        depth = node.depth;
      } else if (node.type === 'html') {
        depth = infer(node);
      }

      if (depth !== undefined) {
        if (depth !== style) {
          file.message('First heading level should be `' + style + '`', node);
        }

        return unistUtilVisit$2.EXIT
      }
    }
  }
}

function infer(node) {
  var results = node.value.match(re$4);
  return results ? Number(results[1]) : undefined
}

var remarkLintHeadingStyle = unifiedLintRule('remark-lint:heading-style', headingStyle);

var types$1 = ['atx', 'atx-closed', 'setext'];

function headingStyle(tree, file, pref) {
  pref = types$1.indexOf(pref) === -1 ? null : pref;

  unistUtilVisit$2(tree, 'heading', visitor);

  function visitor(node) {
    if (!unistUtilGenerated(node)) {
      if (pref) {
        if (mdastUtilHeadingStyle(node, pref) !== pref) {
          file.message('Headings should use ' + pref, node);
        }
      } else {
        pref = mdastUtilHeadingStyle(node, pref);
      }
    }
  }
}

var remarkLintProhibitedStrings = unifiedLintRule('remark-lint:prohibited-strings', prohibitedStrings);

function testProhibited(val, content) {
  const re = new RegExp(`(\\.|@[a-z0-9/-]*)?${val.no}(\\.\\w)?`, 'g');

  let result = null;
  while (result = re.exec(content)) {
    if (!result[1] && !result[2]) {
      return true;
    }
  }

  return false;
}

function prohibitedStrings(ast, file, strings) {
  unistUtilVisit$2(ast, 'text', checkText);

  function checkText(node) {
    const content = node.value;

    strings.forEach((val) => {
      if (testProhibited(val, content)) {
        file.message(`Use "${val.yes}" instead of "${val.no}"`, node);
      }
    });
  }
}

var remarkLintStrongMarker = unifiedLintRule('remark-lint:strong-marker', strongMarker);

var markers$1 = {'*': true, _: true, null: true};

function strongMarker(tree, file, pref) {
  var contents = String(file);

  pref = typeof pref === 'string' && pref !== 'consistent' ? pref : null;

  if (markers$1[pref] !== true) {
    file.fail(
      'Invalid strong marker `' +
        pref +
        "`: use either `'consistent'`, `'*'`, or `'_'`"
    );
  }

  unistUtilVisit$2(tree, 'strong', visitor);

  function visitor(node) {
    var marker = contents.charAt(unistUtilPosition.start(node).offset);

    if (!unistUtilGenerated(node)) {
      if (pref) {
        if (marker !== pref) {
          file.message('Strong should use `' + pref + '` as a marker', node);
        }
      } else {
        pref = marker;
      }
    }
  }
}

var remarkLintTableCellPadding = unifiedLintRule('remark-lint:table-cell-padding', tableCellPadding);

var start$11 = unistUtilPosition.start;
var end$8 = unistUtilPosition.end;

var styles$1 = {null: true, padded: true, compact: true};

function tableCellPadding(tree, file, pref) {
  var contents = String(file);

  pref = typeof pref === 'string' && pref !== 'consistent' ? pref : null;

  if (styles$1[pref] !== true) {
    file.fail('Invalid table-cell-padding style `' + pref + '`');
  }

  unistUtilVisit$2(tree, 'table', visitor);

  function visitor(node) {
    var rows = node.children;
    var sizes = new Array(node.align.length);
    var length = unistUtilGenerated(node) ? -1 : rows.length;
    var index = -1;
    var entries = [];
    var style;
    var row;
    var cells;
    var column;
    var cellCount;
    var cell;
    var next;
    var fence;
    var pos;
    var entry;
    var final;

    /* Check rows. */
    while (++index < length) {
      row = rows[index];
      cells = row.children;
      cellCount = cells.length;
      column = -2; /* Start without a first cell */
      next = null;
      final = undefined;

      /* Check fences (before, between, and after cells) */
      while (++column < cellCount) {
        cell = next;
        next = cells[column + 1];

        fence = contents.slice(
          cell ? end$8(cell).offset : start$11(row).offset,
          next ? start$11(next).offset : end$8(row).offset
        );

        pos = fence.indexOf('|');

        if (cell && cell.children.length !== 0 && final !== undefined) {
          entries.push({node: cell, start: final, end: pos, index: column});

          /* Detect max space per column. */
          sizes[column] = Math.max(sizes[column] || 0, size(cell));
        } else {
          final = undefined;
        }

        if (next && next.children.length !== 0) {
          final = fence.length - pos - 1;
        } else {
          final = undefined;
        }
      }
    }

    if (pref) {
      style = pref === 'padded' ? 1 : 0;
    } else {
      style = entries[0] && (!entries[0].start || !entries[0].end) ? 0 : 1;
    }

    index = -1;
    length = entries.length;

    while (++index < length) {
      entry = entries[index];
      checkSide('start', entry, style, sizes);
      checkSide('end', entry, style, sizes);
    }

    return unistUtilVisit$2.SKIP
  }

  function checkSide(side, entry, style, sizes) {
    var cell = entry.node;
    var spacing = entry[side];
    var index = entry.index;
    var reason;

    if (spacing === undefined || spacing === style) {
      return
    }

    reason = 'Cell should be ';

    if (style === 0) {
      reason += 'compact';

      /* Ignore every cell except the biggest in the column. */
      if (size(cell) < sizes[index]) {
        return
      }
    } else {
      reason += 'padded';

      if (spacing > style) {
        reason += ' with 1 space, not ' + spacing;

        /* May be right or center aligned. */
        if (size(cell) < sizes[index]) {
          return
        }
      }
    }

    file.message(reason, cell.position[side]);
  }
}

function size(node) {
  return end$8(node).offset - start$11(node).offset
}

var remarkLintMaximumLineLength = unifiedLintRule('remark-lint:maximum-line-length', maximumLineLength);

var start$12 = unistUtilPosition.start;
var end$9 = unistUtilPosition.end;

function maximumLineLength(tree, file, pref) {
  var style = typeof pref === 'number' && !isNaN(pref) ? pref : 80;
  var content = String(file);
  var lines = content.split(/\r?\n/);
  var length = lines.length;
  var index = -1;
  var lineLength;

  unistUtilVisit$2(tree, ['heading', 'table', 'code', 'definition'], ignore);
  unistUtilVisit$2(tree, ['link', 'image', 'inlineCode'], inline);

  /* Iterate over every line, and warn for violating lines. */
  while (++index < length) {
    lineLength = lines[index].length;

    if (lineLength > style) {
      file.message('Line must be at most ' + style + ' characters', {
        line: index + 1,
        column: lineLength + 1
      });
    }
  }

  /* Finally, whitelist some inline spans, but only if they occur at or after
   * the wrap.  However, when they do, and there’s white-space after it, they
   * are not whitelisted. */
  function inline(node, pos, parent) {
    var next = parent.children[pos + 1];
    var initial;
    var final;

    /* istanbul ignore if - Nothing to whitelist when generated. */
    if (unistUtilGenerated(node)) {
      return
    }

    initial = start$12(node);
    final = end$9(node);

    /* No whitelisting when starting after the border, or ending before it. */
    if (initial.column > style || final.column < style) {
      return
    }

    /* No whitelisting when there’s white-space after
     * the link. */
    if (
      next &&
      start$12(next).line === initial.line &&
      (!next.value || /^(.+?[ \t].+?)/.test(next.value))
    ) {
      return
    }

    whitelist(initial.line - 1, final.line);
  }

  function ignore(node) {
    /* istanbul ignore else - Hard to test, as we only run this case on `position: true` */
    if (!unistUtilGenerated(node)) {
      whitelist(start$12(node).line - 1, end$9(node).line);
    }
  }

  /* Whitelist from `initial` to `final`, zero-based. */
  function whitelist(initial, final) {
    while (initial < final) {
      lines[initial++] = '';
    }
  }
}

var plugins$1 = [
  remarkLint,
  remarkLintCheckboxContentIndent,
  remarkLintDefinitionSpacing,
  remarkLintFencedCodeFlag,
  remarkLintFinalDefinition,
  remarkLintFinalNewline,
  remarkLintHardBreakSpaces,
  remarkLintNoAutoLinkWithoutProtocol,
  remarkLintNoBlockquoteWithoutCaret,
  remarkLintNoDuplicateDefinitions,
  remarkLintNoFileNameArticles,
  remarkLintNoFileNameConsecutiveDashes,
  remarkLintNoFileNameOuterDashes,
  remarkLintNoHeadingContentIndent,
  remarkLintNoHeadingIndent,
  remarkLintNoInlinePadding,
  remarkLintNoMultipleToplevelHeadings,
  remarkLintNoShellDollars,
  remarkLintNoShortcutReferenceImage,
  remarkLintNoTableIndentation,
  remarkLintNoTabs,
  remarkLintNoUnusedDefinitions,
  remarkLintRuleStyle,
  remarkLintTablePipes,
  [remarkLintBlockquoteIndentation, 2],
  [
    remarkLintCheckboxCharacterStyle,
    {
      'checked': 'x', 'unchecked': ' '
    }
  ],
  [remarkLintCodeBlockStyle, 'fenced'],
  [remarkLintFencedCodeMarker, '`'],
  [remarkLintFileExtension, 'md'],
  [remarkLintFirstHeadingLevel, 1],
  [remarkLintHeadingStyle, 'atx'],
  [
    remarkLintProhibitedStrings,
    [
      { no: 'Github', yes: 'GitHub' },
      { no: 'Javascript', yes: 'JavaScript' },
      { no: 'Node.JS', yes: 'Node.js' },
      { no: 'v8', yes: 'V8' }
    ]
  ],
  [remarkLintStrongMarker, '*'],
  [remarkLintTableCellPadding, 'padded'],
  [remarkLintMaximumLineLength, 80]
];

var remarkPresetLintNode = {
	plugins: plugins$1
};

var proc = ( _package$1 && _package ) || _package$1;

var cli = ( _package$3 && _package$2 ) || _package$3;

const { plugins: plugins$2 } = remarkPresetLintNode;

const args = {
  processor: remark,
  name: proc.name,
  description: cli.description,
  version: [
    proc.name + ': ' + proc.version,
    cli.name + ': ' + cli.version
  ].join(', '),
  ignoreName: '.' + proc.name + 'ignore',
  extensions: markdownExtensions$2
};
const config = options_1(process.argv.slice(2), args);
config.detectConfig = false;
config.plugins = plugins$2;

lib$2(config, function done(err, code) {
  if (err) console.error(err);
  process.exit(code);
});

var cliEntry = {

};

module.exports = cliEntry;

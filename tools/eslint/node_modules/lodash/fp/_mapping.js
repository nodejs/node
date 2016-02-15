/** Used to map aliases to their real names. */
exports.aliasToReal = {
  '__': 'placeholder',
  'all': 'some',
  'allPass': 'overEvery',
  'apply': 'spread',
  'assoc': 'set',
  'assocPath': 'set',
  'compose': 'flowRight',
  'contains': 'includes',
  'dissoc': 'unset',
  'dissocPath': 'unset',
  'each': 'forEach',
  'eachRight': 'forEachRight',
  'equals': 'isEqual',
  'extend': 'assignIn',
  'extendWith': 'assignInWith',
  'first': 'head',
  'init': 'initial',
  'mapObj': 'mapValues',
  'omitAll': 'omit',
  'nAry': 'ary',
  'path': 'get',
  'pathEq': 'matchesProperty',
  'pathOr': 'getOr',
  'pickAll': 'pick',
  'pipe': 'flow',
  'prop': 'get',
  'propOf': 'propertyOf',
  'propOr': 'getOr',
  'somePass': 'overSome',
  'unapply': 'rest',
  'unnest': 'flatten',
  'useWith': 'overArgs',
  'whereEq': 'filter',
  'zipObj': 'zipObject'
};

/** Used to map ary to method names. */
exports.aryMethod = {
  1: [
      'attempt', 'ceil', 'create', 'curry', 'curryRight', 'floor', 'fromPairs',
      'invert', 'iteratee', 'memoize', 'method', 'methodOf', 'mixin', 'over',
      'overEvery', 'overSome', 'rest', 'reverse', 'round', 'runInContext',
      'spread', 'template', 'trim', 'trimEnd', 'trimStart', 'uniqueId', 'words'
    ],
  2: [
      'add', 'after', 'ary', 'assign', 'assignIn', 'at', 'before', 'bind', 'bindKey',
      'chunk', 'cloneDeepWith', 'cloneWith', 'concat', 'countBy', 'curryN',
      'curryRightN', 'debounce', 'defaults', 'defaultsDeep', 'delay', 'difference',
      'drop', 'dropRight', 'dropRightWhile', 'dropWhile', 'endsWith', 'eq', 'every',
      'filter', 'find', 'find', 'findIndex', 'findKey', 'findLast', 'findLastIndex',
      'findLastKey', 'flatMap', 'forEach', 'forEachRight', 'forIn', 'forInRight',
      'forOwn', 'forOwnRight', 'get', 'groupBy', 'gt', 'gte', 'has', 'hasIn',
      'includes', 'indexOf', 'intersection', 'invertBy', 'invoke', 'invokeMap',
      'isEqual', 'isMatch', 'join', 'keyBy', 'lastIndexOf', 'lt', 'lte', 'map',
      'mapKeys', 'mapValues', 'matchesProperty', 'maxBy', 'merge', 'minBy', 'omit',
      'omitBy', 'orderBy', 'overArgs', 'pad', 'padEnd', 'padStart', 'parseInt',
      'partial', 'partialRight', 'partition', 'pick', 'pickBy', 'pull', 'pullAll',
      'pullAt', 'random', 'range', 'rangeRight', 'rearg', 'reject', 'remove',
      'repeat', 'result', 'sampleSize', 'some', 'sortBy', 'sortedIndex',
      'sortedIndexOf', 'sortedLastIndex', 'sortedLastIndexOf', 'sortedUniqBy',
      'split', 'startsWith', 'subtract', 'sumBy', 'take', 'takeRight', 'takeRightWhile',
      'takeWhile', 'tap', 'throttle', 'thru', 'times', 'trimChars', 'trimCharsEnd',
      'trimCharsStart', 'truncate', 'union', 'uniqBy', 'uniqWith', 'unset',
      'unzipWith', 'without', 'wrap', 'xor', 'zip', 'zipObject', 'zipObjectDeep'
    ],
  3: [
      'assignInWith', 'assignWith', 'clamp', 'differenceBy', 'differenceWith',
      'getOr', 'inRange', 'intersectionBy', 'intersectionWith', 'isEqualWith',
      'isMatchWith', 'mergeWith', 'pullAllBy', 'reduce', 'reduceRight', 'replace',
      'set', 'slice', 'sortedIndexBy', 'sortedLastIndexBy', 'transform', 'unionBy',
      'unionWith', 'xorBy', 'xorWith', 'zipWith'
    ],
  4: [
      'fill', 'setWith'
    ]
};

/** Used to map ary to rearg configs. */
exports.aryRearg = {
  2: [1, 0],
  3: [2, 1, 0],
  4: [3, 2, 0, 1]
};

/** Used to iterate `mapping.aryMethod` keys. */
exports.caps = [1, 2, 3, 4];

/** Used to map method names to their iteratee ary. */
exports.iterateeAry = {
  'assignWith': 2,
  'assignInWith': 2,
  'cloneDeepWith': 1,
  'cloneWith': 1,
  'dropRightWhile': 1,
  'dropWhile': 1,
  'every': 1,
  'filter': 1,
  'find': 1,
  'findIndex': 1,
  'findKey': 1,
  'findLast': 1,
  'findLastIndex': 1,
  'findLastKey': 1,
  'flatMap': 1,
  'forEach': 1,
  'forEachRight': 1,
  'forIn': 1,
  'forInRight': 1,
  'forOwn': 1,
  'forOwnRight': 1,
  'isEqualWith': 2,
  'isMatchWith': 2,
  'map': 1,
  'mapKeys': 1,
  'mapValues': 1,
  'partition': 1,
  'reduce': 2,
  'reduceRight': 2,
  'reject': 1,
  'remove': 1,
  'some': 1,
  'takeRightWhile': 1,
  'takeWhile': 1,
  'times': 1,
  'transform': 2
};

/** Used to map method names to iteratee rearg configs. */
exports.iterateeRearg = {
  'findKey': [1],
  'findLastKey': [1],
  'mapKeys': [1]
};

/** Used to map method names to rearg configs. */
exports.methodRearg = {
  'assignInWith': [1, 2, 0],
  'assignWith': [1, 2, 0],
  'clamp': [2, 0, 1],
  'mergeWith': [1, 2, 0],
  'reduce': [2, 0, 1],
  'reduceRight': [2, 0, 1],
  'set': [2, 0, 1],
  'setWith': [3, 1, 2, 0],
  'slice': [2, 0, 1],
  'transform': [2, 0, 1]
};

/** Used to map method names to spread configs. */
exports.methodSpread = {
  'partial': 1,
  'partialRight': 1
};

/** Used to identify methods which mutate arrays or objects. */
exports.mutate = {
  'array': {
    'fill': true,
    'pull': true,
    'pullAll': true,
    'pullAllBy': true,
    'pullAt': true,
    'remove': true,
    'reverse': true
  },
  'object': {
    'assign': true,
    'assignIn': true,
    'assignInWith': true,
    'assignWith': true,
    'defaults': true,
    'defaultsDeep': true,
    'merge': true,
    'mergeWith': true
  },
  'set': {
    'set': true,
    'setWith': true,
    'unset': true
  }
};

/** Used to track methods with placeholder support */
exports.placeholder = {
  'bind': true,
  'bindKey': true,
  'curry': true,
  'curryRight': true,
  'partial': true,
  'partialRight': true
};

/** Used to map real names to their aliases. */
exports.realToAlias = (function() {
  var hasOwnProperty = Object.prototype.hasOwnProperty,
      object = exports.aliasToReal,
      result = {};

  for (var key in object) {
    var value = object[key];
    if (hasOwnProperty.call(result, value)) {
      result[value].push(key);
    } else {
      result[value] = [key];
    }
  }
  return result;
}());

/** Used to map method names to other names. */
exports.rename = {
  'curryN': 'curry',
  'curryRightN': 'curryRight',
  'getOr': 'get',
  'trimChars': 'trim',
  'trimCharsEnd': 'trimEnd',
  'trimCharsStart': 'trimStart'
};

/** Used to track methods that skip `_.rearg`. */
exports.skipRearg = {
  'add': true,
  'assign': true,
  'assignIn': true,
  'concat': true,
  'difference': true,
  'gt': true,
  'gte': true,
  'lt': true,
  'lte': true,
  'matchesProperty': true,
  'merge': true,
  'partial': true,
  'partialRight': true,
  'random': true,
  'range': true,
  'rangeRight': true,
  'subtract': true,
  'zip': true,
  'zipObject': true
};

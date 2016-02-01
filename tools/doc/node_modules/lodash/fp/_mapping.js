module.exports = {

  /** Used to map method names to their aliases. */
  'alias': {
    'ary': ['nAry'],
    'assignIn': ['extend'],
    'assignInWith': ['extendWith'],
    'filter': ['whereEq'],
    'flatten': ['unnest'],
    'flow': ['pipe'],
    'flowRight': ['compose'],
    'forEach': ['each'],
    'forEachRight': ['eachRight'],
    'get': ['path', 'prop'],
    'getOr': ['pathOr', 'propOr'],
    'head': ['first'],
    'includes': ['contains'],
    'initial': ['init'],
    'isEqual': ['equals'],
    'mapValues': ['mapObj'],
    'matchesProperty': ['pathEq'],
    'omit': ['dissoc', 'omitAll'],
    'overArgs': ['useWith'],
    'overEvery': ['allPass'],
    'overSome': ['somePass'],
    'pick': ['pickAll'],
    'propertyOf': ['propOf'],
    'rest': ['unapply'],
    'some': ['all'],
    'spread': ['apply'],
    'zipObject': ['zipObj']
  },

  /** Used to map method names to their iteratee ary. */
  'aryIteratee': {
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
  },

  /** Used to map ary to method names. */
  'aryMethod': {
    1:[
        'attempt', 'ceil', 'create', 'curry', 'curryRight', 'floor', 'fromPairs',
        'invert', 'iteratee', 'memoize', 'method', 'methodOf', 'mixin', 'over',
        'overEvery', 'overSome', 'rest', 'reverse', 'round', 'runInContext',
        'template', 'trim', 'trimEnd', 'trimStart', 'uniqueId', 'words'
      ],
    2:[
        'add', 'after', 'ary', 'assign', 'at', 'before', 'bind', 'bindKey',
        'chunk', 'cloneDeepWith', 'cloneWith', 'concat', 'countBy', 'curryN',
        'curryRightN', 'debounce', 'defaults', 'defaultsDeep', 'delay', 'difference',
        'drop', 'dropRight', 'dropRightWhile', 'dropWhile', 'endsWith', 'eq',
        'every', 'extend', 'filter', 'find', 'find', 'findIndex', 'findKey',
        'findLast', 'findLastIndex', 'findLastKey', 'flatMap', 'forEach',
        'forEachRight', 'forIn', 'forInRight', 'forOwn', 'forOwnRight', 'get',
        'groupBy', 'gt', 'gte', 'has', 'hasIn', 'includes', 'indexOf', 'intersection',
        'invoke', 'invokeMap', 'isEqual', 'isMatch', 'join', 'keyBy', 'lastIndexOf',
        'lt', 'lte', 'map', 'mapKeys', 'mapValues', 'matchesProperty', 'maxBy',
        'merge', 'minBy', 'omit', 'omitBy', 'orderBy', 'overArgs', 'pad', 'padEnd',
        'padStart', 'parseInt', 'partition', 'pick', 'pickBy', 'pull', 'pullAll',
        'pullAt', 'random', 'range', 'rangeRight', 'rearg', 'reject', 'remove',
        'repeat', 'result', 'sampleSize', 'some', 'sortBy', 'sortedIndex',
        'sortedIndexOf', 'sortedLastIndex', 'sortedLastIndexOf', 'sortedUniqBy',
        'split', 'startsWith', 'subtract', 'sumBy', 'take', 'takeRight', 'takeRightWhile',
        'takeWhile', 'tap', 'throttle', 'thru', 'times', 'truncate', 'union', 'uniqBy',
        'uniqWith', 'unset', 'unzipWith', 'without', 'wrap', 'xor', 'zip', 'zipObject'
      ],
    3:[
        'assignInWith', 'assignWith', 'clamp', 'differenceBy', 'differenceWith',
        'getOr', 'inRange', 'intersectionBy', 'intersectionWith', 'isEqualWith',
        'isMatchWith', 'mergeWith', 'pullAllBy', 'reduce', 'reduceRight', 'replace',
        'set', 'slice', 'sortedIndexBy', 'sortedLastIndexBy', 'transform', 'unionBy',
        'unionWith', 'xorBy', 'xorWith', 'zipWith'
      ],
    4:[
        'fill', 'setWith'
      ]
  },

  /** Used to map ary to rearg configs. */
  'aryRearg': {
    2: [1, 0],
    3: [2, 1, 0],
    4: [3, 2, 0, 1]
  },

  /** Used to map method names to iteratee rearg configs. */
  'iterateeRearg': {
    'findKey': [1],
    'findLastKey': [1],
    'mapKeys': [1]
  },

  /** Used to map method names to rearg configs. */
  'methodRearg': {
    'clamp': [2, 0, 1],
    'reduce': [2, 0, 1],
    'reduceRight': [2, 0, 1],
    'set': [2, 0, 1],
    'setWith': [3, 1, 2, 0],
    'slice': [2, 0, 1],
    'transform': [2, 0, 1]
  },

  /** Used to iterate `mapping.aryMethod` keys. */
  'caps': [1, 2, 3, 4],

  /** Used to map keys to other keys. */
  'key': {
    'curryN': 'curry',
    'curryRightN': 'curryRight',
    'getOr': 'get'
  },

  /** Used to identify methods which mutate arrays or objects. */
  'mutate': {
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
      'setWith': true
    }
  },

  /** Used to track methods with placeholder support */
  'placeholder': {
    'bind': true,
    'bindKey': true,
    'curry': true,
    'curryRight': true,
    'partial': true,
    'partialRight': true
  },

  /** Used to track methods that skip `_.rearg`. */
  'skipRearg': {
    'assign': true,
    'assignIn': true,
    'concat': true,
    'defaults': true,
    'defaultsDeep': true,
    'difference': true,
    'matchesProperty': true,
    'merge': true,
    'random': true,
    'range': true,
    'rangeRight': true,
    'zip': true,
    'zipObject': true
  }
};

// A list of all the synonyms of assert methods.
// In addition to these, multi-word camelCase are also synonymized to
// all lowercase and snake_case
module.exports = multiword({
  ok: ['true', 'assert'],
  notOk: ['false', 'assertNot'],

  error: ['ifError', 'ifErr'],
  throws: ['throw'],
  doesNotThrow: ['notThrow'],

  // exactly the same.  ===
  equal: [
    'equals', 'isEqual', 'is', 'strictEqual', 'strictEquals', 'strictIs',
    'isStrict', 'isStrictly'
  ],

  // not equal.  !==
  not: [
    'inequal', 'notEqual', 'notEquals', 'notStrictEqual', 'notStrictEquals',
    'isNotEqual', 'isNot', 'doesNotEqual', 'isInequal'
  ],

  // deep equivalence.  == for scalars
  same: [
    'equivalent', 'looseEqual', 'looseEquals', 'deepEqual',
    'deepEquals', 'isLoose', 'looseIs', 'isEquivalent'
  ],

  // deep inequivalence. != for scalars
  notSame: [
    'inequivalent', 'looseInequal', 'notDeep', 'deepInequal',
    'notLoose', 'looseNot', 'notEquivalent', 'isNotDeepEqual',
    'isNotDeeply', 'notDeepEqual', 'isInequivalent',
    'isNotEquivalent'
  ],

  // deep equivalence, === for scalars
  strictSame: [
    'strictEquivalent', 'strictDeepEqual', 'sameStrict', 'deepIs',
    'isDeeply', 'isDeep', 'strictDeepEquals'
  ],

  // deep inequivalence, !== for scalars
  strictNotSame: [
    'strictInequivalent', 'strictDeepInequal', 'notSameStrict', 'deepNot',
    'notDeeply', 'strictDeepInequals', 'notStrictSame'
  ],

  // found has the fields in wanted, string matches regexp
  match: [
    'has', 'hasFields', 'matches', 'similar', 'like', 'isLike',
    'includes', 'include', 'isSimilar', 'contains'
  ],

  notMatch: [
    'dissimilar', 'unsimilar', 'notSimilar', 'unlike', 'isUnlike',
    'notLike', 'isNotLike', 'doesNotHave', 'isNotSimilar', 'isDissimilar'
  ],

  type: [
    'isa', 'isA'
  ]
})

function multiword (obj) {
  Object.keys(obj).forEach(function (i) {
    var list = obj[i]
    var res = [ multiword_(i) ].concat(list.map(multiword_))
    res = res.reduce(function (set, i) {
      set.push.apply(set, i)
      return set
    }, [])
    obj[i] = res
  })
  return obj
}

function multiword_ (str) {
  var res = [ str ]
  if (str.match(/[A-Z]/)) {
    res.push(str.toLowerCase())
    res.push(str.replace(/[A-Z]/g, function ($0) {
      return '_' + $0.toLowerCase()
    }))
  }
  return res
}

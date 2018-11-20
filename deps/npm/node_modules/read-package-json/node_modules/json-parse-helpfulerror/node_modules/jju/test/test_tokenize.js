var assert = require('assert')
var parse = require('../').parse

function tokenize(arg) {
  var result = []
  parse(arg, {_tokenize: function(smth) {
    result.push(smth)
  }})
  assert.deepEqual(result.map(function(x){return x.raw}).join(''), arg)
  return result
}

function addTest(x, exp) {
  function fn(){assert.deepEqual(tokenize(x), exp)}

  if (typeof(describe) === 'function') {
    it('test_tokenize: ' + JSON.stringify(x), fn)
  } else {
    fn()
  }
}

addTest('123', [ { raw: '123', value: 123, type: 'literal', stack: [] }])

addTest(' /* zz */\r\n true /* zz */\n',
[ { raw: ' ', type: 'whitespace', stack: [] },
  { raw: '/* zz */', type: 'comment', stack: [] },
  { raw: '\r\n', type: 'newline', stack: [] },
  { raw: ' ', type: 'whitespace', stack: [] },
  { raw: 'true', type: 'literal', value: true, stack: [] },
  { raw: ' ', type: 'whitespace', stack: [] },
  { raw: '/* zz */', type: 'comment', stack: [] },
  { raw: '\n', type: 'newline', stack: [] } ])

addTest('{q:123,  w : /*zz*/\n\r 345 } ',
[ { raw: '{', type: 'separator', stack: [] },
  { raw: 'q', type: 'key', value: 'q', stack: [] },
  { raw: ':', type: 'separator', stack: [] },
  { raw: '123', type: 'literal', value: 123, stack: ['q'] },
  { raw: ',', type: 'separator', stack: [] },
  { raw: '  ', type: 'whitespace', stack: [] },
  { raw: 'w', type: 'key', value: 'w', stack: [] },
  { raw: ' ', type: 'whitespace', stack: [] },
  { raw: ':', type: 'separator', stack: [] },
  { raw: ' ', type: 'whitespace', stack: [] },
  { raw: '/*zz*/', type: 'comment', stack: [] },
  { raw: '\n', type: 'newline', stack: [] },
  { raw: '\r', type: 'newline', stack: [] },
  { raw: ' ', type: 'whitespace', stack: [] },
  { raw: '345', type: 'literal', value: 345, stack: ['w'] },
  { raw: ' ', type: 'whitespace', stack: [] },
  { raw: '}', type: 'separator', stack: [] },
  { raw: ' ', type: 'whitespace', stack: [] } ])

addTest('null /* */// xxx\n//xxx',
[ { raw: 'null', type: 'literal', value: null, stack: [] },
  { raw: ' ', type: 'whitespace', stack: [] },
  { raw: '/* */', type: 'comment', stack: [] },
  { raw: '// xxx', type: 'comment', stack: [] },
  { raw: '\n', type: 'newline', stack: [] },
  { raw: '//xxx', type: 'comment', stack: [] } ])

addTest('[1,2,[[],[1]],{},{1:2},{q:{q:{}}},]',
[ { raw: '[', type: 'separator', stack: [] },
  { raw: '1', type: 'literal', value: 1, stack: [0] },
  { raw: ',', type: 'separator', stack: [] },
  { raw: '2', type: 'literal', value: 2, stack: [1] },
  { raw: ',', type: 'separator', stack: [] },
  { raw: '[', type: 'separator', stack: [2] },
  { raw: '[', type: 'separator', stack: [2,0] },
  { raw: ']', type: 'separator', stack: [2,0] },
  { raw: ',', type: 'separator', stack: [2] },
  { raw: '[', type: 'separator', stack: [2,1] },
  { raw: '1', type: 'literal', value: 1, stack: [2,1,0] },
  { raw: ']', type: 'separator', stack: [2,1] },
  { raw: ']', type: 'separator', stack: [2] },
  { raw: ',', type: 'separator', stack: [] },
  { raw: '{', type: 'separator', stack: [3] },
  { raw: '}', type: 'separator', stack: [3] },
  { raw: ',', type: 'separator', stack: [] },
  { raw: '{', type: 'separator', stack: [4] },
  { raw: '1', type: 'key', value: 1, stack: [4] },
  { raw: ':', type: 'separator', stack: [4] },
  { raw: '2', type: 'literal', value: 2, stack: [4,'1'] },
  { raw: '}', type: 'separator', stack: [4] },
  { raw: ',', type: 'separator', stack: [] },
  { raw: '{', type: 'separator', stack: [5] },
  { raw: 'q', type: 'key', value: 'q', stack: [5] },
  { raw: ':', type: 'separator', stack: [5] },
  { raw: '{', type: 'separator', stack: [5,'q'] },
  { raw: 'q', type: 'key', value: 'q', stack: [5,'q'] },
  { raw: ':', type: 'separator', stack: [5,'q'] },
  { raw: '{', type: 'separator', stack: [5,'q','q'] },
  { raw: '}', type: 'separator', stack: [5,'q','q'] },
  { raw: '}', type: 'separator', stack: [5,'q'] },
  { raw: '}', type: 'separator', stack: [5] },
  { raw: ',', type: 'separator', stack: [] },
  { raw: ']', type: 'separator', stack: [] } ])


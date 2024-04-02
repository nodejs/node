var test = require('tape');
var Parser = require('../');

var input = '{\n  "string": "value",\n  "number": 3,\n  "object"';
var input2 = ': {\n  "key": "vд"\n  },\n  "array": [\n  -1,\n  12\n  ]\n  ';
var input3 = '"null": null, "true": true, "false": false, "frac": 3.14 }';

var offsets = [
  [ 0, Parser.C.LEFT_BRACE ],
  [ 4, Parser.C.STRING ],
  [ 12, Parser.C.COLON ],
  [ 14, Parser.C.STRING ],
  [ 21, Parser.C.COMMA ],
  [ 25, Parser.C.STRING ],
  [ 33, Parser.C.COLON ],
  [ 35, Parser.C.NUMBER ],
  [ 36, Parser.C.COMMA ],
  [ 40, Parser.C.STRING ],
  [ 48, Parser.C.COLON ],
  [ 50, Parser.C.LEFT_BRACE ],
  [ 54, Parser.C.STRING ],
  [ 59, Parser.C.COLON ],
  [ 61, Parser.C.STRING ],
  [ 69, Parser.C.RIGHT_BRACE ],
  [ 70, Parser.C.COMMA ],
  [ 74, Parser.C.STRING ],
  [ 81, Parser.C.COLON ],
  [ 83, Parser.C.LEFT_BRACKET ],
  [ 87, Parser.C.NUMBER ],
  [ 89, Parser.C.COMMA ],
  [ 93, Parser.C.NUMBER ],
  [ 98, Parser.C.RIGHT_BRACKET ],
  [ 102, Parser.C.STRING ],
  [ 108, Parser.C.COLON ],
  [ 110, Parser.C.NULL ],
  [ 114, Parser.C.COMMA ],
  [ 116, Parser.C.STRING ],
  [ 122, Parser.C.COLON ],
  [ 124, Parser.C.TRUE ],
  [ 128, Parser.C.COMMA ],
  [ 130, Parser.C.STRING ],
  [ 137, Parser.C.COLON ],
  [ 139, Parser.C.FALSE ],
  [ 144, Parser.C.COMMA ],
  [ 146, Parser.C.STRING ],
  [ 152, Parser.C.COLON ],
  [ 154, Parser.C.NUMBER ],
  [ 159, Parser.C.RIGHT_BRACE ]
];

test('offset', function(t) {
  t.plan(offsets.length * 2 + 1);

  var p = new Parser();
  var i = 0;
  p.onToken = function (token) {
    t.equal(p.offset, offsets[i][0]);
    t.equal(token, offsets[i][1]);
    i++;
  };

  p.write(input);
  p.write(input2);
  p.write(input3);

  t.equal(i, offsets.length);
});

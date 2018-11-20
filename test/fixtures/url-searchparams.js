module.exports = [
  ['', '', []],
  [
    'foo=918854443121279438895193',
    'foo=918854443121279438895193',
    [['foo', '918854443121279438895193']]
  ],
  ['foo=bar', 'foo=bar', [['foo', 'bar']]],
  ['foo=bar&foo=quux', 'foo=bar&foo=quux', [['foo', 'bar'], ['foo', 'quux']]],
  ['foo=1&bar=2', 'foo=1&bar=2', [['foo', '1'], ['bar', '2']]],
  [
    "my%20weird%20field=q1!2%22'w%245%267%2Fz8)%3F",
    'my+weird+field=q1%212%22%27w%245%267%2Fz8%29%3F',
    [['my weird field', 'q1!2"\'w$5&7/z8)?']]
  ],
  ['foo%3Dbaz=bar', 'foo%3Dbaz=bar', [['foo=baz', 'bar']]],
  ['foo=baz=bar', 'foo=baz%3Dbar', [['foo', 'baz=bar']]],
  [
    'str=foo&arr=1&somenull&arr=2&undef=&arr=3',
    'str=foo&arr=1&somenull=&arr=2&undef=&arr=3',
    [
      ['str', 'foo'],
      ['arr', '1'],
      ['somenull', ''],
      ['arr', '2'],
      ['undef', ''],
      ['arr', '3']
    ]
  ],
  [' foo = bar ', '+foo+=+bar+', [[' foo ', ' bar ']]],
  ['foo=%zx', 'foo=%25zx', [['foo', '%zx']]],
  ['foo=%EF%BF%BD', 'foo=%EF%BF%BD', [['foo', '\ufffd']]],
  // See: https://github.com/joyent/node/issues/3058
  ['foo&bar=baz', 'foo=&bar=baz', [['foo', ''], ['bar', 'baz']]],
  ['a=b&c&d=e', 'a=b&c=&d=e', [['a', 'b'], ['c', ''], ['d', 'e']]],
  ['a=b&c=&d=e', 'a=b&c=&d=e', [['a', 'b'], ['c', ''], ['d', 'e']]],
  ['a=b&=c&d=e', 'a=b&=c&d=e', [['a', 'b'], ['', 'c'], ['d', 'e']]],
  ['a=b&=&d=e', 'a=b&=&d=e', [['a', 'b'], ['', ''], ['d', 'e']]],
  ['&&foo=bar&&', 'foo=bar', [['foo', 'bar']]],
  ['&', '', []],
  ['&&&&', '', []],
  ['&=&', '=', [['', '']]],
  ['&=&=', '=&=', [['', ''], ['', '']]],
  ['=', '=', [['', '']]],
  ['+', '+=', [[' ', '']]],
  ['+=', '+=', [[' ', '']]],
  ['+&', '+=', [[' ', '']]],
  ['=+', '=+', [['', ' ']]],
  ['+=&', '+=', [[' ', '']]],
  ['a&&b', 'a=&b=', [['a', ''], ['b', '']]],
  ['a=a&&b=b', 'a=a&b=b', [['a', 'a'], ['b', 'b']]],
  ['&a', 'a=', [['a', '']]],
  ['&=', '=', [['', '']]],
  ['a&a&', 'a=&a=', [['a', ''], ['a', '']]],
  ['a&a&a&', 'a=&a=&a=', [['a', ''], ['a', ''], ['a', '']]],
  ['a&a&a&a&', 'a=&a=&a=&a=', [['a', ''], ['a', ''], ['a', ''], ['a', '']]],
  ['a=&a=value&a=', 'a=&a=value&a=', [['a', ''], ['a', 'value'], ['a', '']]],
  ['foo%20bar=baz%20quux', 'foo+bar=baz+quux', [['foo bar', 'baz quux']]],
  ['+foo=+bar', '+foo=+bar', [[' foo', ' bar']]],
  ['a+', 'a+=', [['a ', '']]],
  ['=a+', '=a+', [['', 'a ']]],
  ['a+&', 'a+=', [['a ', '']]],
  ['=a+&', '=a+', [['', 'a ']]],
  ['%20+', '++=', [['  ', '']]],
  ['=%20+', '=++', [['', '  ']]],
  ['%20+&', '++=', [['  ', '']]],
  ['=%20+&', '=++', [['', '  ']]],
  [
    // fake percent encoding
    'foo=%©ar&baz=%A©uux&xyzzy=%©ud',
    'foo=%25%C2%A9ar&baz=%25A%C2%A9uux&xyzzy=%25%C2%A9ud',
    [['foo', '%©ar'], ['baz', '%A©uux'], ['xyzzy', '%©ud']]
  ],
  // always preserve order of key-value pairs
  ['a=1&b=2&a=3', 'a=1&b=2&a=3', [['a', '1'], ['b', '2'], ['a', '3']]],
  ['?a', '%3Fa=', [['?a', '']]]
];

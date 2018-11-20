var test = require('tape');
var Parser = require('../');

test('2 byte utf8 \'De\' character: д', function (t) {
  t.plan(1);

  var p = new Parser();
  p.onValue = function (value) {
    t.equal(value, 'д');
  };

  var de_buffer = new Buffer([0xd0, 0xb4]);

  p.write('"');
  p.write(de_buffer);
  p.write('"');

});

test('3 byte utf8 \'Han\' character: 我', function (t) {
  t.plan(1);

  var p = new Parser();
  p.onValue = function (value) {
    t.equal(value, '我');
  };

  var han_buffer = new Buffer([0xe6, 0x88, 0x91]);
  p.write('"');
  p.write(han_buffer);
  p.write('"');
});

test('4 byte utf8 character (unicode scalar U+2070E): 𠜎', function (t) {
  t.plan(1);

  var p = new Parser();
  p.onValue = function (value) {
    t.equal(value, '𠜎');
  };

  var Ux2070E_buffer = new Buffer([0xf0, 0xa0, 0x9c, 0x8e]);
  p.write('"');
  p.write(Ux2070E_buffer);
  p.write('"');
});

test('3 byte utf8 \'Han\' character chunked inbetween 2nd and 3rd byte: 我', function (t) {
  t.plan(1);

  var p = new Parser();
  p.onValue = function (value) {
    t.equal(value, '我');
  };

  var han_buffer_first = new Buffer([0xe6, 0x88]);
  var han_buffer_second = new Buffer([0x91]);
  p.write('"');
  p.write(han_buffer_first);
  p.write(han_buffer_second);
  p.write('"');
});

test('4 byte utf8 character (unicode scalar U+2070E) chunked inbetween 2nd and 3rd byte: 𠜎', function (t) {
  t.plan(1);

  var p = new Parser();
  p.onValue = function (value) {
    t.equal(value, '𠜎');
  };

  var Ux2070E_buffer_first = new Buffer([0xf0, 0xa0]);
  var Ux2070E_buffer_second = new Buffer([0x9c, 0x8e]);
  p.write('"');
  p.write(Ux2070E_buffer_first);
  p.write(Ux2070E_buffer_second);
  p.write('"');
});

test('1-4 byte utf8 character string chunked inbetween random bytes: Aж文𠜱B', function (t) {
  t.plan(1);

var p = new Parser();
  p.onValue = function (value) {
    t.equal(value, 'Aж文𠜱B');
  };

  var eclectic_buffer = new Buffer([0x41, // A
                                    0xd0, 0xb6, // ж
                                    0xe6, 0x96, 0x87, // 文
                                    0xf0, 0xa0, 0x9c, 0xb1, // 𠜱
                                    0x42]); // B

  var rand_chunk = Math.floor(Math.random() * (eclectic_buffer.length));
  var first_buffer = eclectic_buffer.slice(0, rand_chunk);
  var second_buffer = eclectic_buffer.slice(rand_chunk);

  //console.log('eclectic_buffer: ' + eclectic_buffer)
  //console.log('sliced from 0 to ' + rand_chunk);
  //console.log(first_buffer);
  //console.log('then sliced from ' + rand_chunk + ' to the end');
  //console.log(second_buffer);

  console.log('chunked after offset ' + rand_chunk);
  p.write('"');
  p.write(first_buffer);
  p.write(second_buffer);
  p.write('"');

});
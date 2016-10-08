'use strict';

var test = require('tape');
var qs = require('../');
var iconv = require('iconv-lite');

test('stringify()', function (t) {
    t.test('stringifies a querystring object', function (st) {
        st.equal(qs.stringify({ a: 'b' }), 'a=b');
        st.equal(qs.stringify({ a: 1 }), 'a=1');
        st.equal(qs.stringify({ a: 1, b: 2 }), 'a=1&b=2');
        st.equal(qs.stringify({ a: 'A_Z' }), 'a=A_Z');
        st.equal(qs.stringify({ a: '€' }), 'a=%E2%82%AC');
        st.equal(qs.stringify({ a: '' }), 'a=%EE%80%80');
        st.equal(qs.stringify({ a: 'א' }), 'a=%D7%90');
        st.equal(qs.stringify({ a: '𐐷' }), 'a=%F0%90%90%B7');
        st.end();
    });

    t.test('stringifies a nested object', function (st) {
        st.equal(qs.stringify({ a: { b: 'c' } }), 'a%5Bb%5D=c');
        st.equal(qs.stringify({ a: { b: { c: { d: 'e' } } } }), 'a%5Bb%5D%5Bc%5D%5Bd%5D=e');
        st.end();
    });

    t.test('stringifies a nested object with dots notation', function (st) {
        st.equal(qs.stringify({ a: { b: 'c' } }, { allowDots: true }), 'a.b=c');
        st.equal(qs.stringify({ a: { b: { c: { d: 'e' } } } }, { allowDots: true }), 'a.b.c.d=e');
        st.end();
    });

    t.test('stringifies an array value', function (st) {
        st.equal(qs.stringify({ a: ['b', 'c', 'd'] }), 'a%5B0%5D=b&a%5B1%5D=c&a%5B2%5D=d');
        st.end();
    });

    t.test('omits nulls when asked', function (st) {
        st.equal(qs.stringify({ a: 'b', c: null }, { skipNulls: true }), 'a=b');
        st.end();
    });


    t.test('omits nested nulls when asked', function (st) {
        st.equal(qs.stringify({ a: { b: 'c', d: null } }, { skipNulls: true }), 'a%5Bb%5D=c');
        st.end();
    });

    t.test('omits array indices when asked', function (st) {
        st.equal(qs.stringify({ a: ['b', 'c', 'd'] }, { indices: false }), 'a=b&a=c&a=d');
        st.end();
    });

    t.test('stringifies a nested array value', function (st) {
        st.equal(qs.stringify({ a: { b: ['c', 'd'] } }), 'a%5Bb%5D%5B0%5D=c&a%5Bb%5D%5B1%5D=d');
        st.end();
    });

    t.test('stringifies a nested array value with dots notation', function (st) {
        st.equal(qs.stringify({ a: { b: ['c', 'd'] } }, { allowDots: true, encode: false }), 'a.b[0]=c&a.b[1]=d');
        st.end();
    });

    t.test('stringifies an object inside an array', function (st) {
        st.equal(qs.stringify({ a: [{ b: 'c' }] }), 'a%5B0%5D%5Bb%5D=c');
        st.equal(qs.stringify({ a: [{ b: { c: [1] } }] }), 'a%5B0%5D%5Bb%5D%5Bc%5D%5B0%5D=1');
        st.end();
    });

    t.test('stringifies an array with mixed objects and primitives', function (st) {
        st.equal(qs.stringify({ a: [{ b: 1 }, 2, 3] }, { encode: false }), 'a[0][b]=1&a[1]=2&a[2]=3');
        st.end();
    });

    t.test('stringifies an object inside an array with dots notation', function (st) {
        st.equal(qs.stringify({ a: [{ b: 'c' }] }, { allowDots: true, encode: false }), 'a[0].b=c');
        st.equal(qs.stringify({ a: [{ b: { c: [1] } }] }, { allowDots: true, encode: false }), 'a[0].b.c[0]=1');
        st.end();
    });

    t.test('does not omit object keys when indices = false', function (st) {
        st.equal(qs.stringify({ a: [{ b: 'c' }] }, { indices: false }), 'a%5Bb%5D=c');
        st.end();
    });

    t.test('uses indices notation for arrays when indices=true', function (st) {
        st.equal(qs.stringify({ a: ['b', 'c'] }, { indices: true }), 'a%5B0%5D=b&a%5B1%5D=c');
        st.end();
    });

    t.test('uses indices notation for arrays when no arrayFormat is specified', function (st) {
        st.equal(qs.stringify({ a: ['b', 'c'] }), 'a%5B0%5D=b&a%5B1%5D=c');
        st.end();
    });

    t.test('uses indices notation for arrays when no arrayFormat=indices', function (st) {
        st.equal(qs.stringify({ a: ['b', 'c'] }, { arrayFormat: 'indices' }), 'a%5B0%5D=b&a%5B1%5D=c');
        st.end();
    });

    t.test('uses repeat notation for arrays when no arrayFormat=repeat', function (st) {
        st.equal(qs.stringify({ a: ['b', 'c'] }, { arrayFormat: 'repeat' }), 'a=b&a=c');
        st.end();
    });

    t.test('uses brackets notation for arrays when no arrayFormat=brackets', function (st) {
        st.equal(qs.stringify({ a: ['b', 'c'] }, { arrayFormat: 'brackets' }), 'a%5B%5D=b&a%5B%5D=c');
        st.end();
    });

    t.test('stringifies a complicated object', function (st) {
        st.equal(qs.stringify({ a: { b: 'c', d: 'e' } }), 'a%5Bb%5D=c&a%5Bd%5D=e');
        st.end();
    });

    t.test('stringifies an empty value', function (st) {
        st.equal(qs.stringify({ a: '' }), 'a=');
        st.equal(qs.stringify({ a: null }, { strictNullHandling: true }), 'a');

        st.equal(qs.stringify({ a: '', b: '' }), 'a=&b=');
        st.equal(qs.stringify({ a: null, b: '' }, { strictNullHandling: true }), 'a&b=');

        st.equal(qs.stringify({ a: { b: '' } }), 'a%5Bb%5D=');
        st.equal(qs.stringify({ a: { b: null } }, { strictNullHandling: true }), 'a%5Bb%5D');
        st.equal(qs.stringify({ a: { b: null } }, { strictNullHandling: false }), 'a%5Bb%5D=');

        st.end();
    });

    t.test('stringifies an empty object', function (st) {
        var obj = Object.create(null);
        obj.a = 'b';
        st.equal(qs.stringify(obj), 'a=b');
        st.end();
    });

    t.test('returns an empty string for invalid input', function (st) {
        st.equal(qs.stringify(undefined), '');
        st.equal(qs.stringify(false), '');
        st.equal(qs.stringify(null), '');
        st.equal(qs.stringify(''), '');
        st.end();
    });

    t.test('stringifies an object with an empty object as a child', function (st) {
        var obj = {
            a: Object.create(null)
        };

        obj.a.b = 'c';
        st.equal(qs.stringify(obj), 'a%5Bb%5D=c');
        st.end();
    });

    t.test('drops keys with a value of undefined', function (st) {
        st.equal(qs.stringify({ a: undefined }), '');

        st.equal(qs.stringify({ a: { b: undefined, c: null } }, { strictNullHandling: true }), 'a%5Bc%5D');
        st.equal(qs.stringify({ a: { b: undefined, c: null } }, { strictNullHandling: false }), 'a%5Bc%5D=');
        st.equal(qs.stringify({ a: { b: undefined, c: '' } }), 'a%5Bc%5D=');
        st.end();
    });

    t.test('url encodes values', function (st) {
        st.equal(qs.stringify({ a: 'b c' }), 'a=b%20c');
        st.end();
    });

    t.test('stringifies a date', function (st) {
        var now = new Date();
        var str = 'a=' + encodeURIComponent(now.toISOString());
        st.equal(qs.stringify({ a: now }), str);
        st.end();
    });

    t.test('stringifies the weird object from qs', function (st) {
        st.equal(qs.stringify({ 'my weird field': '~q1!2"\'w$5&7/z8)?' }), 'my%20weird%20field=~q1%212%22%27w%245%267%2Fz8%29%3F');
        st.end();
    });

    t.test('skips properties that are part of the object prototype', function (st) {
        Object.prototype.crash = 'test';
        st.equal(qs.stringify({ a: 'b' }), 'a=b');
        st.equal(qs.stringify({ a: { b: 'c' } }), 'a%5Bb%5D=c');
        delete Object.prototype.crash;
        st.end();
    });

    t.test('stringifies boolean values', function (st) {
        st.equal(qs.stringify({ a: true }), 'a=true');
        st.equal(qs.stringify({ a: { b: true } }), 'a%5Bb%5D=true');
        st.equal(qs.stringify({ b: false }), 'b=false');
        st.equal(qs.stringify({ b: { c: false } }), 'b%5Bc%5D=false');
        st.end();
    });

    t.test('stringifies buffer values', function (st) {
        st.equal(qs.stringify({ a: new Buffer('test') }), 'a=test');
        st.equal(qs.stringify({ a: { b: new Buffer('test') } }), 'a%5Bb%5D=test');
        st.end();
    });

    t.test('stringifies an object using an alternative delimiter', function (st) {
        st.equal(qs.stringify({ a: 'b', c: 'd' }, { delimiter: ';' }), 'a=b;c=d');
        st.end();
    });

    t.test('doesn\'t blow up when Buffer global is missing', function (st) {
        var tempBuffer = global.Buffer;
        delete global.Buffer;
        var result = qs.stringify({ a: 'b', c: 'd' });
        global.Buffer = tempBuffer;
        st.equal(result, 'a=b&c=d');
        st.end();
    });

    t.test('selects properties when filter=array', function (st) {
        st.equal(qs.stringify({ a: 'b' }, { filter: ['a'] }), 'a=b');
        st.equal(qs.stringify({ a: 1 }, { filter: [] }), '');
        st.equal(qs.stringify({ a: { b: [1, 2, 3, 4], c: 'd' }, c: 'f' }, { filter: ['a', 'b', 0, 2] }), 'a%5Bb%5D%5B0%5D=1&a%5Bb%5D%5B2%5D=3');
        st.end();
    });

    t.test('supports custom representations when filter=function', function (st) {
        var calls = 0;
        var obj = { a: 'b', c: 'd', e: { f: new Date(1257894000000) } };
        var filterFunc = function (prefix, value) {
            calls++;
            if (calls === 1) {
                st.equal(prefix, '', 'prefix is empty');
                st.equal(value, obj);
            } else if (prefix === 'c') {
                return;
            } else if (value instanceof Date) {
                st.equal(prefix, 'e[f]');
                return value.getTime();
            }
            return value;
        };

        st.equal(qs.stringify(obj, { filter: filterFunc }), 'a=b&e%5Bf%5D=1257894000000');
        st.equal(calls, 5);
        st.end();
    });

    t.test('can disable uri encoding', function (st) {
        st.equal(qs.stringify({ a: 'b' }, { encode: false }), 'a=b');
        st.equal(qs.stringify({ a: { b: 'c' } }, { encode: false }), 'a[b]=c');
        st.equal(qs.stringify({ a: 'b', c: null }, { strictNullHandling: true, encode: false }), 'a=b&c');
        st.end();
    });

    t.test('can sort the keys', function (st) {
        var sort = function (a, b) { return a.localeCompare(b); };
        st.equal(qs.stringify({ a: 'c', z: 'y', b: 'f' }, { sort: sort }), 'a=c&b=f&z=y');
        st.equal(qs.stringify({ a: 'c', z: { j: 'a', i: 'b' }, b: 'f' }, { sort: sort }), 'a=c&b=f&z%5Bi%5D=b&z%5Bj%5D=a');
        st.end();
    });

    t.test('can sort the keys at depth 3 or more too', function (st) {
        var sort = function (a, b) { return a.localeCompare(b); };
        st.equal(qs.stringify({ a: 'a', z: { zj: {zjb: 'zjb', zja: 'zja'}, zi: {zib: 'zib', zia: 'zia'} }, b: 'b' }, { sort: sort, encode: false }), 'a=a&b=b&z[zi][zia]=zia&z[zi][zib]=zib&z[zj][zja]=zja&z[zj][zjb]=zjb');
        st.equal(qs.stringify({ a: 'a', z: { zj: {zjb: 'zjb', zja: 'zja'}, zi: {zib: 'zib', zia: 'zia'} }, b: 'b' }, { sort: null, encode: false }), 'a=a&z[zj][zjb]=zjb&z[zj][zja]=zja&z[zi][zib]=zib&z[zi][zia]=zia&b=b');
        st.end();
    });

    t.test('can stringify with custom encoding', function (st) {
        st.equal(qs.stringify({ 県: '大阪府', '': ''}, {
            encoder: function (str) {
                if (str.length === 0) {
                    return '';
                }
                var buf = iconv.encode(str, 'shiftjis');
                var result = [];
                for (var i=0; i < buf.length; ++i) {
                    result.push(buf.readUInt8(i).toString(16));
                }
                return '%' + result.join('%');
            }
        }), '%8c%a7=%91%e5%8d%e3%95%7b&=');
        st.end();
    });

    t.test('throws error with wrong encoder', function (st) {
        st.throws(function () {
            qs.stringify({}, {
                encoder: 'string'
            });
        }, new TypeError('Encoder has to be a function.'));
        st.end();
    });

    t.test('can use custom encoder for a buffer object', {
        skip: typeof Buffer === 'undefined'
    }, function (st) {
        st.equal(qs.stringify({ a: new Buffer([1]) }, {
            encoder: function (buffer) {
                if (typeof buffer === 'string') {
                    return buffer;
                }
                return String.fromCharCode(buffer.readUInt8(0) + 97);
            }
        }), 'a=b');
        st.end();
    });
});

var test = require('tape');
var fs = require ('fs');
var join = require('path').join;
var couch_sample_file = join(__dirname, 'fixtures','couch_sample.json');
var JSONStream = require('../');

var fixture = {
  obj: {
    one: 1,
    two: 2,
    three: 3
  }
};

function assertFixtureKeys(stream, t) {
    var keys = [];
    var values = [];
    stream.on('data', function(data) {
        keys.push(data.key);
        values.push(data.value);
    });

    stream.on('end', function() {
        t.deepEqual(keys, ['one', 'two', 'three']);
        t.deepEqual(values, [1,2,3]);
        t.end();
    });
    stream.write(JSON.stringify(fixture));
    stream.end();
}

test('keys via string', function(t) {
    var stream = JSONStream.parse('obj.$*');
    assertFixtureKeys(stream, t);
});

test('keys via array', function(t) {
    var stream = JSONStream.parse(['obj',{emitKey: true}]);
    assertFixtureKeys(stream, t);
});

test('advanced keys', function(t) {
    var advanced = fs.readFileSync(couch_sample_file);
    var stream = JSONStream.parse(['rows', true, 'doc', {emitKey: true}]);

    var keys = [];
    var values = [];
    stream.on('data', function(data) {
        keys.push(data.key);
        values.push(data.value);
    });

    stream.on('end', function() {
        t.deepEqual(keys, [
            '_id', '_rev', 'hello',
            '_id', '_rev', 'hello'
        ]);
        t.deepEqual(values, [
            "change1_0.6995461115147918", "1-e240bae28c7bb3667f02760f6398d508", 1,
            "change2_0.6995461115147918", "1-13677d36b98c0c075145bb8975105153", 2
        ]);
        t.end();
    });
    stream.write(advanced);
    stream.end();
});

test('parent keys', function(t) {
    var stream = JSONStream.parse('$*');
    var d = null;
    stream.on('data', function(data) {
        if(d) t.fail('should only be called once');
        d = data;
    });

    stream.on('end', function() {
        t.deepEqual(d,{
            key: 'obj',
            value: fixture.obj
        });
        t.end();
    });
    stream.write(JSON.stringify(fixture));
    stream.end();
})

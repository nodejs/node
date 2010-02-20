process.mixin(require("./common"));

var got_error = false;
var success_count = 0;

fs.stat(".", function (err, stats) {
  if (err) {
    got_error = true;
  } else {
    p(stats);
    assert.ok(stats.mtime instanceof Date);
    success_count++;
  }
});

puts("stating: " + __filename);
fs.stat(__filename, function (err, s) {
  if (err) {
    got_error = true;
  } else {
    p(s);
    success_count++;

    puts("isDirectory: " + JSON.stringify( s.isDirectory() ) );
    assert.equal(false, s.isDirectory());

    puts("isFile: " + JSON.stringify( s.isFile() ) );
    assert.equal(true, s.isFile());

    puts("isSocket: " + JSON.stringify( s.isSocket() ) );
    assert.equal(false, s.isSocket());

    puts("isBlockDevice: " + JSON.stringify( s.isBlockDevice() ) );
    assert.equal(false, s.isBlockDevice());

    puts("isCharacterDevice: " + JSON.stringify( s.isCharacterDevice() ) );
    assert.equal(false, s.isCharacterDevice());

    puts("isFIFO: " + JSON.stringify( s.isFIFO() ) );
    assert.equal(false, s.isFIFO());

    puts("isSymbolicLink: " + JSON.stringify( s.isSymbolicLink() ) );
    assert.equal(false, s.isSymbolicLink());

    assert.ok(s.mtime instanceof Date);
  }
});

process.addListener("exit", function () {
  assert.equal(2, success_count);
  assert.equal(false, got_error);
});


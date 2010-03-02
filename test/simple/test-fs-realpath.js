process.mixin(require("../common"));

var async_completed = 0, async_expected = 0;

// a. deep relative file symlink
var dstPath = path.join(fixturesDir, 'cycles', 'root.js');
var linkData1 = "../../cycles/root.js";
var linkPath1 = path.join(fixturesDir, "nested-index", 'one', 'symlink1.js');
try {fs.unlinkSync(linkPath1);}catch(e){}
fs.symlinkSync(linkData1, linkPath1);

var linkData2 = "../one/symlink1.js";
var linkPath2 = path.join(fixturesDir, "nested-index", 'two', 'symlink1-b.js');
try {fs.unlinkSync(linkPath2);}catch(e){}
fs.symlinkSync(linkData2, linkPath2);

// b. deep relative directory symlink
var dstPath_b = path.join(fixturesDir, 'cycles', 'folder');
var linkData1b = "../../cycles/folder";
var linkPath1b = path.join(fixturesDir, "nested-index", 'one', 'symlink1-dir');
try {fs.unlinkSync(linkPath1b);}catch(e){}
fs.symlinkSync(linkData1b, linkPath1b);

var linkData2b = "../one/symlink1-dir";
var linkPath2b = path.join(fixturesDir, "nested-index", 'two', 'symlink12-dir');
try {fs.unlinkSync(linkPath2b);}catch(e){}
fs.symlinkSync(linkData2b, linkPath2b);

assert.equal(fs.realpathSync(linkPath2), dstPath);
assert.equal(fs.realpathSync(linkPath2b), dstPath_b);

async_expected++;
fs.realpath(linkPath2, function(err, rpath) {
  if (err) throw err;
  assert.equal(rpath, dstPath);
  async_completed++;
});

async_expected++;
fs.realpath(linkPath2b, function(err, rpath) {
  if (err) throw err;
  assert.equal(rpath, dstPath_b);
  async_completed++;
});

// todo: test shallow symlinks (file & dir)
// todo: test non-symlinks (file & dir)
// todo: test error on cyclic symlinks

process.addListener("exit", function () {
  try {fs.unlinkSync(linkPath1);}catch(e){}
  try {fs.unlinkSync(linkPath2);}catch(e){}
  try {fs.unlinkSync(linkPath1b);}catch(e){}
  try {fs.unlinkSync(linkPath2b);}catch(e){}
  assert.equal(async_completed, async_expected);
});

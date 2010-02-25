process.mixin(require("../common"));

var dirname = path.dirname(__filename);

assert.equal(true, process.cwd() !== dirname);

process.chdir(dirname);

assert.equal(true, process.cwd() === dirname);

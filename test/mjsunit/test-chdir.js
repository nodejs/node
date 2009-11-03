process.mixin(require("./common"));

var dirname = path.dirname(__filename);

assertTrue(process.cwd() !== dirname);

process.chdir(dirname);

assertTrue(process.cwd() === dirname);

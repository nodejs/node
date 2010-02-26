process.mixin(require('../common'));

var fixture = path.join(__dirname, "../fixtures/x.txt");

assert.equal("xyz\n", fs.readFileSync(fixture));

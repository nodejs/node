var path = require('path');

for (var i = 2; i < process.argv.length; i++) {
    require(path.resolve(process.cwd(), process.argv[i]));
}

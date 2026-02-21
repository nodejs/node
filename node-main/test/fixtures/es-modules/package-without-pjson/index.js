const identifier = 'package-without-pjson';

const common = require('../common');
common.requireNoPackageJSONAbove();

console.log(identifier);
module.exports = identifier;

const { getOptionsAsFlagsFromBinding } = require('internal/options');

const flags = getOptionsAsFlagsFromBinding();
console.log(JSON.stringify(flags.sort()));

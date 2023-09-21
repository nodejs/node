const {
  setDeserializeMainFunction,
} = require('v8').startupSnapshot;

// To make sure the cwd is present in the cache
process.cwd();

setDeserializeMainFunction(() => {
  console.log(process.cwd());
});

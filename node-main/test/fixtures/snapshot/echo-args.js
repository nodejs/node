const {
  setDeserializeMainFunction,
} = require('v8').startupSnapshot;

const originalArgv = [...process.argv];

setDeserializeMainFunction(() => {
  console.log(JSON.stringify({
    currentArgv: process.argv,
    originalArgv
  }));
});

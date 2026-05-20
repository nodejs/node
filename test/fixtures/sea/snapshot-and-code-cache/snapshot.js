const {
  setDeserializeMainFunction,
} = require('v8').startupSnapshot;

setDeserializeMainFunction(() => {
  console.log('Hello from snapshot');
});

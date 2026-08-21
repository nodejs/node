const {
  setDeserializeMainFunction,
} = require('v8').startupSnapshot;

console.log(JSON.stringify(Object.keys(console), null, 2));

setDeserializeMainFunction(() => {
  console.log(JSON.stringify(Object.keys(console), null, 2));
});

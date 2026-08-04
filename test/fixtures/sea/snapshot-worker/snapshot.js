const {
  setDeserializeMainFunction,
} = require('v8').startupSnapshot;

setDeserializeMainFunction(() => {
  const { Worker } = require('worker_threads');
  new Worker("console.log('Hello from Worker')", { eval: true });
});

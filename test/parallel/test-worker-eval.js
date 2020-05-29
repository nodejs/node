'use strict';
require('../common');
const assert = require('assert');
const { Worker } = require('worker_threads');

const moduleString = 'export default import.meta.url;';
const cjsString = 'module.exports=__filename;';

const cjsLoadingESMError = /Unexpected token 'export'/;
const esmLoadingCJSError = /module is not defined/;
const invalidValueError =
  /The argument 'options\.type' must be either "module" or "classic"/;
const invalidValueError2 =
  /The argument 'options\.type' must be set to "module" when filename is a data: URL/;

function runInWorker(evalString, options) {
  return new Promise((resolve, reject) => {
    const worker = new Worker(evalString, options);
    worker.on('error', reject);
    worker.on('exit', resolve);
  });
}

{
  const options = { eval: true };
  assert.rejects(runInWorker(moduleString, options), cjsLoadingESMError);
  runInWorker(cjsString, options); // does not reject
}

{
  const options = { eval: true, type: 'classic' };
  assert.rejects(runInWorker(moduleString, options), cjsLoadingESMError);
  runInWorker(cjsString, options); // does not reject
}

{
  const options = { eval: true, type: 'module' };
  runInWorker(moduleString, options); // does not reject
  assert.rejects(runInWorker(cjsString, options), esmLoadingCJSError);
}

{
  const options = { eval: false, type: 'module' };
  runInWorker(new URL(`data:text/javascript,${moduleString}`),
              options); // does not reject
  assert.rejects(runInWorker(new URL(`data:text/javascript,${cjsString}`),
                             options), esmLoadingCJSError);
}

{
  const options = { eval: true, type: 'wasm' };
  assert.rejects(runInWorker(moduleString, options), invalidValueError);
  assert.rejects(runInWorker(cjsString, options), invalidValueError);
}

{
  const options = { type: 'classic' };
  assert.rejects(runInWorker(new URL(`data:text/javascript,${moduleString}`),
                             options), invalidValueError2);
  assert.rejects(runInWorker(new URL(`data:text/javascript,${cjsString}`),
                             options), invalidValueError2);
}

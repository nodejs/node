const ERROR_TEXT = 'emptyStackError';

function Hello() {
  Error.stackTraceLimit = 0;
  throw Error(ERROR_TEXT)
}

setImmediate(function () {
  Hello();
});

module.exports = {
  ERROR_TEXT
};
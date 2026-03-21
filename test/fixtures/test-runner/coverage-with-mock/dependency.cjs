throw new Error('This should never be called');

exports.dependency = function dependency() {
  return 'foo';
}

exports.unused = function unused() {
  return 'bar';
}

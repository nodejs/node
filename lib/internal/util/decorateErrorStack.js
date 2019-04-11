'use strict';

function decorateErrorStack(err) {
    if (!(isError(err) && err.stack) ||
        getHiddenValue(err, kDecoratedPrivateSymbolIndex) === true)
      return;
  
    const arrow = getHiddenValue(err, kArrowMessagePrivateSymbolIndex);
  
    if (arrow) {
      err.stack = arrow + err.stack;
      setHiddenValue(err, kDecoratedPrivateSymbolIndex, true);
    }
  }

  module.exports = {
    decorateErrorStack
  }
/**
 * @fileoverview Utility functions for React pragma configuration
 * @author Yannick Croissant
 */
'use strict';

var JSX_ANNOTATION_REGEX = /^\*\s*@jsx\s+([^\s]+)/;

function getFromContext(context) {
  var pragma = 'React';
  // .eslintrc shared settings (http://eslint.org/docs/user-guide/configuring#adding-shared-settings)
  if (context.settings.react && context.settings.react.pragma) {
    pragma = context.settings.react.pragma;
  // Deprecated pragma option, here for backward compatibility
  } else if (context.options[0] && context.options[0].pragma) {
    pragma = context.options[0].pragma;
  }
  return pragma.split('.')[0];
}

function getFromNode(node) {
  var matches = JSX_ANNOTATION_REGEX.exec(node.value);
  if (!matches) {
    return false;
  }
  return matches[1].split('.')[0];
}

module.exports = {
  getFromContext: getFromContext,
  getFromNode: getFromNode
};

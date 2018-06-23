'use strict';

var assign = require('object-assign');


module.exports = function u (type, props, value) {
  if (value == null && (typeof props != 'object' || Array.isArray(props))) {
    value = props;
    props = {};
  }

  return assign({}, props, { type: String(type) },
                value != null && (Array.isArray(value)
                                  ? { children: value }
                                  : { value: String(value) }));
};

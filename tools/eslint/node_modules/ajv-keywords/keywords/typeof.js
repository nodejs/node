'use strict';

module.exports = defFunc;

var KNOWN_TYPES = ['undefined', 'string', 'number', 'object', 'function', 'boolean', 'symbol'];

var definition = defFunc.definition = {
  inline: function (it, keyword, schema) {
    var data = 'data' + (it.dataLevel || '');
    if (typeof schema == 'string') return 'typeof ' + data + ' == "' + schema + '"';
    schema = 'validate.schema' + it.schemaPath + '.' + keyword;
    return schema + '.indexOf(typeof ' + data + ') >= 0';
  },
  metaSchema: {
    anyOf: [
      {
        type: 'string',
        enum: KNOWN_TYPES
      },
      {
        type: 'array',
        items: {
          type: 'string',
          enum: KNOWN_TYPES
        }
      }
    ]
  }
};

function defFunc(ajv) {
  ajv.addKeyword('typeof', definition);
}

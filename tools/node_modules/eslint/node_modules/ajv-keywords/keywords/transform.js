'use strict';

module.exports = function defFunc (ajv) {
  defFunc.definition = {
    type: 'string',
    errors: false,
    modifying: true,
    valid: true,
    compile: function (schema, parentSchema) {

      // build hash table to enum values
      var hashtable = {};

      if (schema.indexOf('toEnumCase') !== -1) {
        // requires `enum` in schema
        if (!parentSchema.enum)
          throw new Error('Missing enum. To use `transform:["toEnumCase"]`, `enum:[...]` is required.');
        for (var i = parentSchema.enum.length; i--; i) {
          var v = parentSchema.enum[i];
          if (typeof v !== 'string') continue;
          var k = makeHashTableKey(v);
          // requires all `enum` values have unique keys
          if (hashtable[k])
            throw new Error('Invalid enum uniqueness. To use `transform:["toEnumCase"]`, all values must be unique when case insensitive.');
          hashtable[k] = v;
        }
      }

      var transform = {
        trimLeft: function (value) {
          return value.replace(/^[\s]+/, '');
        },
        trimRight: function (value) {
          return value.replace(/[\s]+$/, '');
        },
        trim: function (value) {
          return value.trim();
        },
        toLowerCase: function (value) {
          return value.toLowerCase();
        },
        toUpperCase: function (value) {
          return value.toUpperCase();
        },
        toEnumCase: function (value) {
          return hashtable[makeHashTableKey(value)] || value;
        }
      };

      return function (value, objectKey, object, key) {
        // skip if value only
        if (!object) return;

        // apply transform in order provided
        for (var j = 0, l = schema.length; j < l; j++) {
          if (typeof object[key] !== 'string') continue;
          object[key] = transform[schema[j]](object[key]);
        }
      };
    },
    metaSchema: {
      type: 'array',
      items: {
        type: 'string',
        enum: [
          'trimLeft', 'trimRight', 'trim',
          'toLowerCase', 'toUpperCase', 'toEnumCase'
        ]
      }
    }
  };

  ajv.addKeyword('transform', defFunc.definition);
  return ajv;

  function makeHashTableKey (value) {
    return value.toLowerCase();
  }
};

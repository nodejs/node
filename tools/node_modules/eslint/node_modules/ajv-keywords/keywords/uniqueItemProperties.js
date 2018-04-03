'use strict';

module.exports = function defFunc(ajv) {
  defFunc.definition = {
    type: 'array',
    compile: function(keys, parentSchema, it) {
      var equal = it.util.equal;
      return function(data) {
        if (data.length > 1) {
          for (var k=0; k < keys.length; k++) {
            var key = keys[k];
            for (var i = data.length; i--;) {
              if (typeof data[i] != 'object') continue;
              for (var j = i; j--;) {
                if (typeof data[j] == 'object' && equal(data[i][key], data[j][key]))
                  return false;
              }
            }
          }
        }
        return true;
      };
    },
    metaSchema: {
      type: 'array',
      items: {type: 'string'}
    }
  };

  ajv.addKeyword('uniqueItemProperties', defFunc.definition);
  return ajv;
};

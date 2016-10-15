'use strict';

var IDENTIFIER = /^[a-z_$][a-z0-9_$]*$/i;
var customRuleCode = require('./dotjs/custom');

/**
 * Define custom keyword
 * @this  Ajv
 * @param {String} keyword custom keyword, should be a valid identifier, should be different from all standard, custom and macro keywords.
 * @param {Object} definition keyword definition object with properties `type` (type(s) which the keyword applies to), `validate` or `compile`.
 */
module.exports = function addKeyword(keyword, definition) {
  /* eslint no-shadow: 0 */
  var self = this;
  if (this.RULES.keywords[keyword])
    throw new Error('Keyword ' + keyword + ' is already defined');

  if (!IDENTIFIER.test(keyword))
    throw new Error('Keyword ' + keyword + ' is not a valid identifier');

  if (definition) {
    var dataType = definition.type;
    if (Array.isArray(dataType)) {
      var i, len = dataType.length;
      for (i=0; i<len; i++) checkDataType(dataType[i]);
      for (i=0; i<len; i++) _addRule(keyword, dataType[i], definition);
    } else {
      if (dataType) checkDataType(dataType);
      _addRule(keyword, dataType, definition);
    }

    var $data = definition.$data === true && this._opts.v5;
    if ($data && !definition.validate)
      throw new Error('$data support: neither "validate" nor "compile" functions are defined');

    var metaSchema = definition.metaSchema;
    if (metaSchema) {
      if ($data) {
        metaSchema = {
          anyOf: [
            metaSchema,
            { '$ref': 'https://raw.githubusercontent.com/epoberezkin/ajv/master/lib/refs/json-schema-v5.json#/definitions/$data' }
          ]
        };
      }
      definition.validateSchema = self.compile(metaSchema, true);
    }
  }

  this.RULES.keywords[keyword] = this.RULES.all[keyword] = true;


  function _addRule(keyword, dataType, definition) {
    var ruleGroup;
    for (var i=0; i<self.RULES.length; i++) {
      var rg = self.RULES[i];
      if (rg.type == dataType) {
        ruleGroup = rg;
        break;
      }
    }

    if (!ruleGroup) {
      ruleGroup = { type: dataType, rules: [] };
      self.RULES.push(ruleGroup);
    }

    var rule = {
      keyword: keyword,
      definition: definition,
      custom: true,
      code: customRuleCode
    };
    ruleGroup.rules.push(rule);
    self.RULES.custom[keyword] = rule;
  }


  function checkDataType(dataType) {
    if (!self.RULES.types[dataType]) throw new Error('Unknown type ' + dataType);
  }
};

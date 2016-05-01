/**
 * @fileoverview Create configurations for a rule
 * @author Ian VanSchooten
 */

"use strict";

//------------------------------------------------------------------------------
// Requirements
//------------------------------------------------------------------------------

var rules = require("../rules"),
    loadRules = require("../load-rules");


//------------------------------------------------------------------------------
// Helpers
//------------------------------------------------------------------------------

/**
 * Wrap all of the elements of an array into arrays.
 * @param   {*[]}     xs Any array.
 * @returns {Array[]}    An array of arrays.
 */
function explodeArray(xs) {
    return xs.reduce(function(accumulator, x) {
        accumulator.push([x]);
        return accumulator;
    }, []);
}

/**
 * Mix two arrays such that each element of the second array is concatenated
 * onto each element of the first array.
 *
 * For example:
 * combineArrays([a, [b, c]], [x, y]); // -> [[a, x], [a, y], [b, c, x], [b, c, y]]
 *
 * @param   {array} arr1 The first array to combine.
 * @param   {array} arr2 The second array to combine.
 * @returns {array}      A mixture of the elements of the first and second arrays.
 */
function combineArrays(arr1, arr2) {
    var res = [];

    if (arr1.length === 0) {
        return explodeArray(arr2);
    }
    if (arr2.length === 0) {
        return explodeArray(arr1);
    }
    arr1.forEach(function(x1) {
        arr2.forEach(function(x2) {
            res.push([].concat(x1, x2));
        });
    });
    return res;
}

/**
 * Group together valid rule configurations based on object properties
 *
 * e.g.:
 * groupByProperty([
 *     {before: true},
 *     {before: false},
 *     {after: true},
 *     {after: false}
 * ]);
 *
 * will return:
 * [
 *     [{before: true}, {before: false}],
 *     [{after: true}, {after: false}]
 * ]
 *
 * @param   {Object[]} objects Array of objects, each with one property/value pair
 * @returns {Array[]}          Array of arrays of objects grouped by property
 */
function groupByProperty(objects) {
    var groupedObj = objects.reduce(function(accumulator, obj) {
        var prop = Object.keys(obj)[0];

        accumulator[prop] = accumulator[prop] ? accumulator[prop].concat(obj) : [obj];
        return accumulator;
    }, {});

    return Object.keys(groupedObj).map(function(prop) {
        return groupedObj[prop];
    });
}


//------------------------------------------------------------------------------
// Private
//------------------------------------------------------------------------------

/**
 * Configuration settings for a rule.
 *
 * A configuration can be a single number (severity), or an array where the first
 * element in the array is the severity, and is the only required element.
 * Configs may also have one or more additional elements to specify rule
 * configuration or options.
 *
 * @typedef {array|number} ruleConfig
 * @param {number}  0  The rule's severity (0, 1, 2).
 */

/**
 * Object whose keys are rule names and values are arrays of valid ruleConfig items
 * which should be linted against the target source code to determine error counts.
 * (a ruleConfigSet.ruleConfigs).
 *
 * e.g. rulesConfig = {
 *     "comma-dangle": [2, [2, "always"], [2, "always-multiline"], [2, "never"]],
 *     "no-console": [2]
 * }
 * @typedef rulesConfig
 */


/**
 * Create valid rule configurations by combining two arrays,
 * with each array containing multiple objects each with a
 * single property/value pair and matching properties.
 *
 * e.g.:
 * combinePropertyObjects(
 *     [{before: true}, {before: false}],
 *     [{after: true}, {after: false}]
 * );
 *
 * will return:
 * [
 *     {before: true, after: true},
 *     {before: true, after: false},
 *     {before: false, after: true},
 *     {before: false, after: false}
 * ]
 *
 * @param   {Object[]} objArr1 Single key/value objects, all with the same key
 * @param   {Object[]} objArr2 Single key/value objects, all with another key
 * @returns {Object[]}         Combined objects for each combination of input properties and values
 */
function combinePropertyObjects(objArr1, objArr2) {
    var res = [];

    if (objArr1.length === 0) {
        return objArr2;
    }
    if (objArr2.length === 0) {
        return objArr1;
    }
    objArr1.forEach(function(obj1) {
        objArr2.forEach(function(obj2) {
            var combinedObj = {};
            var obj1Props = Object.keys(obj1);
            var obj2Props = Object.keys(obj2);

            obj1Props.forEach(function(prop1) {
                combinedObj[prop1] = obj1[prop1];
            });
            obj2Props.forEach(function(prop2) {
                combinedObj[prop2] = obj2[prop2];
            });
            res.push(combinedObj);
        });
    });
    return res;
}

 /**
  * Creates a new instance of a rule configuration set
  *
  * A rule configuration set is an array of configurations that are valid for a
  * given rule.  For example, the configuration set for the "semi" rule could be:
  *
  * ruleConfigSet.ruleConfigs // -> [[2], [2, "always"], [2, "never"]]
  *
  * @param {ruleConfig[]} configs Valid rule configurations
  * @constructor
  */
function RuleConfigSet(configs) {

    /**
    * Stored valid rule configurations for this instance
    * @type {array}
    */
    this.ruleConfigs = configs || [];

}

RuleConfigSet.prototype = {

    constructor: RuleConfigSet,

    /**
    * Add a severity level to the front of all configs in the instance.
    * This should only be called after all configs have been added to the instance.
    *
    * @param {number} [severity=2] The level of severity for the rule (0, 1, 2)
    * @returns {void}
    */
    addErrorSeverity: function(severity) {
        severity = severity || 2;

        this.ruleConfigs = this.ruleConfigs.map(function(config) {
            config.unshift(severity);
            return config;
        });

        // Add a single config at the beginning consisting of only the severity
        this.ruleConfigs.unshift(severity);
    },

    /**
    * Add rule configs from an array of strings (schema enums)
    * @param  {string[]} enums Array of valid rule options (e.g. ["always", "never"])
    * @returns {void}
    */
    addEnums: function(enums) {
        this.ruleConfigs = this.ruleConfigs.concat(combineArrays(this.ruleConfigs, enums));
    },

    /**
    * Add rule configurations from a schema object
    * @param  {Object} obj Schema item with type === "object"
    * @returns {void}
    */
    addObject: function(obj) {
        var objectConfigSet = {
            objectConfigs: [],
            add: function(property, values) {
                var optionObj;

                for (var idx = 0; idx < values.length; idx++) {
                    optionObj = {};
                    optionObj[property] = values[idx];
                    this.objectConfigs.push(optionObj);
                }
            },

            combine: function() {
                this.objectConfigs = groupByProperty(this.objectConfigs).reduce(function(accumulator, objArr) {
                    return combinePropertyObjects(accumulator, objArr);
                }, []);
            }
        };

        /*
         * The object schema could have multiple independent properties.
         * If any contain enums or booleans, they can be added and then combined
         */
        Object.keys(obj.properties).forEach(function(prop) {
            if (obj.properties[prop].enum) {
                objectConfigSet.add(prop, obj.properties[prop].enum);
            }
            if (obj.properties[prop].type && obj.properties[prop].type === "boolean") {
                objectConfigSet.add(prop, [true, false]);
            }
        });
        objectConfigSet.combine();

        if (objectConfigSet.objectConfigs.length > 0) {
            this.ruleConfigs = this.ruleConfigs.concat(combineArrays(this.ruleConfigs, objectConfigSet.objectConfigs));
        }
    }
};

/**
* Generate valid rule configurations based on a schema object
* @param   {Object} schema  A rule's schema object
* @returns {array[]}        Valid rule configurations
*/
function generateConfigsFromSchema(schema) {
    var configSet = new RuleConfigSet();

    if (Array.isArray(schema)) {
        schema.forEach(function(opt) {
            if (opt.enum) {
                configSet.addEnums(opt.enum);
            }

            if (opt.type && opt.type === "object") {
                configSet.addObject(opt);
            }

            if (opt.oneOf) {

                // TODO (IanVS): not yet implemented
            }
        });
    }
    configSet.addErrorSeverity();
    return configSet.ruleConfigs;
}

/**
* Generate possible rule configurations for all of the core rules
* @returns {rulesConfig} Hash of rule names and arrays of possible configurations
*/
function createCoreRuleConfigs() {
    var ruleList = loadRules();

    return Object.keys(ruleList).reduce(function(accumulator, id) {
        var rule = rules.get(id);
        var schema = (typeof rule === "function") ? rule.schema : rule.meta.schema;

        accumulator[id] = generateConfigsFromSchema(schema);
        return accumulator;
    }, {});
}


//------------------------------------------------------------------------------
// Public Interface
//------------------------------------------------------------------------------

module.exports = {
    generateConfigsFromSchema: generateConfigsFromSchema,
    createCoreRuleConfigs: createCoreRuleConfigs
};

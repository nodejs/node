'use strict';

const {
  ArrayFrom,
  ArrayIsArray,
  ArrayPrototypePush,
  ArrayPrototypeUnshift,
  ObjectKeys,
  ObjectPrototypeHasOwnProperty,
  ObjectValues,
  SetPrototypeForEach,
} = primordials;

const { previewEntries } = internalBinding('util');
const { Buffer: { isBuffer } } = require('buffer');
const { inspect } = require('internal/util/inspect');
const {
  isTypedArray, isSet, isMap, isSetIterator, isMapIterator,
} = require('internal/util/types');

const keyKey = 'Key';
const valuesKey = 'Values';
const indexKey = '(index)';
const iterKey = '(iteration index)';

const isArray = (v) => ArrayIsArray(v) || isTypedArray(v) || isBuffer(v);

/**
 * Builds the rows of a table from tabular data and renders them into a string.
 * Shared by `console.table` and `util.table`.
 * @param {any} tabularData The data to tabulate.
 * @param {string[]} [properties] The subset of properties to include as columns.
 * @param {object} [inspectOptions] Base options merged into each cell's inspect call.
 * @returns {string} The rendered table.
 */
function table(tabularData, properties, inspectOptions) {
  // Lazy require to avoid a cycle with the console bootstrap path.
  const cliTable = require('internal/cli_table');

  const _inspect = (v) => {
    const depth = v !== null &&
                  typeof v === 'object' &&
                  !isArray(v) &&
                  ObjectKeys(v).length > 2 ? -1 : 0;
    const opt = {
      depth,
      maxArrayLength: 3,
      breakLength: Infinity,
      ...inspectOptions,
    };
    return inspect(v, opt);
  };
  const getIndexArray = (length) => ArrayFrom(
    { length }, (_, i) => _inspect(i));

  const mapIter = isMapIterator(tabularData);
  let isMapLike;
  if (mapIter) {
    const res = previewEntries(tabularData, true);
    tabularData = res[0];
    isMapLike = res[1];
  } else {
    isMapLike = isMap(tabularData);
  }

  if (isMapLike) {
    const keys = [];
    const values = [];
    let length = 0;
    if (mapIter) {
      for (let i = 0; i < tabularData.length / 2; ++i) {
        ArrayPrototypePush(keys, _inspect(tabularData[i * 2]));
        ArrayPrototypePush(values, _inspect(tabularData[i * 2 + 1]));
        length++;
      }
    } else {
      for (const { 0: k, 1: v } of tabularData) {
        ArrayPrototypePush(keys, _inspect(k));
        ArrayPrototypePush(values, _inspect(v));
        length++;
      }
    }
    return cliTable([
      iterKey, keyKey, valuesKey,
    ], [
      getIndexArray(length),
      keys,
      values,
    ]);
  }

  const setIter = isSetIterator(tabularData);
  if (setIter)
    tabularData = previewEntries(tabularData);

  const setlike = setIter || mapIter || isSet(tabularData);
  if (setlike) {
    const values = [];
    if (setIter || mapIter) {
      // `previewEntries` already yielded an array, so read it by index.
      for (let j = 0; j < tabularData.length; j++)
        ArrayPrototypePush(values, _inspect(tabularData[j]));
    } else {
      // A real Set: iterate with `SetPrototypeForEach` to avoid the iterator
      // protocol and a second pass over the data.
      SetPrototypeForEach(tabularData,
                          (v) => ArrayPrototypePush(values, _inspect(v)));
    }
    return cliTable([iterKey, valuesKey], [getIndexArray(values.length), values]);
  }

  const map = { __proto__: null };
  let hasPrimitives = false;
  const valuesKeyArray = [];
  const indexKeyArray = ObjectKeys(tabularData);

  for (let i = 0; i < indexKeyArray.length; i++) {
    const item = tabularData[indexKeyArray[i]];
    const primitive = item === null ||
        (typeof item !== 'function' && typeof item !== 'object');
    if (properties === undefined && primitive) {
      hasPrimitives = true;
      valuesKeyArray[i] = _inspect(item);
    } else {
      const keys = properties || ObjectKeys(item);
      for (let k = 0; k < keys.length; k++) {
        const key = keys[k];
        map[key] ??= [];
        if ((primitive && properties) ||
             !ObjectPrototypeHasOwnProperty(item, key))
          map[key][i] = '';
        else
          map[key][i] = _inspect(item[key]);
      }
    }
  }

  const keys = ObjectKeys(map);
  const values = ObjectValues(map);
  if (hasPrimitives) {
    ArrayPrototypePush(keys, valuesKey);
    ArrayPrototypePush(values, valuesKeyArray);
  }
  ArrayPrototypeUnshift(keys, indexKey);
  ArrayPrototypeUnshift(values, indexKeyArray);

  return cliTable(keys, values);
}

module.exports = table;

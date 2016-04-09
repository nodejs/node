/**
 * Creates a function that performs a mathematical operation on two values.
 *
 * @private
 * @param {Function} operator The function to perform the operation.
 * @returns {Function} Returns the new mathematical operation function.
 */
function createMathOperation(operator) {
  return function(value, other) {
    var result;
    if (value === undefined && other === undefined) {
      return 0;
    }
    if (value !== undefined) {
      result = value;
    }
    if (other !== undefined) {
      result = result === undefined ? other : operator(result, other);
    }
    return result;
  };
}

module.exports = createMathOperation;

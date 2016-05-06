var Evaluator = require('esmangle-evaluator');

var recast = require('recast');
var types = recast.types;
var b = types.builders;

var VISITOR_METHODS = {
  visitLogicalExpression: visitLogicalExp,
  visitIfStatement: visitCondition,
  visitConditionalExpression: visitCondition
};

module.exports = function(branch) {
  recast.visit(branch, VISITOR_METHODS);
  return branch;
};


/**
 *  "||" and "&&"
 */
function visitLogicalExp(path) {
  var leftEval = Evaluator.booleanCondition(path.node.left);

  if (typeof leftEval !== 'boolean') {
    // console.log('___ %s ___', path.node.operator);
    this.traverse(path);
    return;
  }

  var leftSideEffect = Evaluator.hasSideEffect(path.node.left);
  if (leftSideEffect) {
    this.traverse(path);
    return;
  }

  if (leftEval === true && path.node.operator === '||') {
    // console.log('true || ___');
    path.replace(path.node.left);
    recast.visit(path, VISITOR_METHODS);
    return false;
  }

  if (leftEval === true && path.node.operator === '&&') {
    // console.log('true && ___');
    path.replace(path.node.right);
    recast.visit(path, VISITOR_METHODS);
    return false;
  }

  if (leftEval === false && path.node.operator === '&&') {
    // console.log('false && ___');
    path.replace(path.node.left);
    recast.visit(path, VISITOR_METHODS);
    return false;
  }

  if (leftEval === false && path.node.operator === '||') {
    // console.log('false || ___');
    path.replace(path.node.right);
    recast.visit(path, VISITOR_METHODS);
    return false;
  }
}

/**
 *  "if" and ternary "?"
 */
function visitCondition(path) {
  var testEval = Evaluator.booleanCondition(path.node.test);

  if (typeof testEval !== 'boolean') {
    // console.log('if/? ___');
    this.traverse(path);
    return;
  }

  var testSideEffect = Evaluator.hasSideEffect(path.node.test);
  if (testSideEffect) {
    this.traverse(path);
    return;
  }

  if (testEval === true) {
    // console.log('if/? (true)');
    path.replace(path.value.consequent);
    recast.visit(path, VISITOR_METHODS);
    return false;
  }

  if (testEval === false) {
    // console.log('if/? (false)');
    path.replace(path.value.alternate);
    recast.visit(path, VISITOR_METHODS);
    return false;
  }
}

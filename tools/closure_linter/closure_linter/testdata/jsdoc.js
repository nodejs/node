// Copyright 2007 The Closure Linter Authors. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS-IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

/**
 * @fileoverview Errors related to JsDoc.
 *
 * @author robbyw@google.com (Robby Walker)
 *
 * @author  robbyw@google.com  (Robby Walker) // EXTRA_SPACE, EXTRA_SPACE
 * @author robbyw@google.com(Robby Walker) // MISSING_SPACE
 *
 * @author robbyw@google.com () // INVALID_AUTHOR_TAG_DESCRIPTION
 * @author robbyw@google.com  // INVALID_AUTHOR_TAG_DESCRIPTION
 *
 * @owner  ajp@google.com (Andy Perelson)
 * @badtag   // INVALID_JSDOC_TAG
 * @customtag This tag is passed as a flag in full_test.py
 * @requires anotherCustomTagPassedInFromFullTestThatShouldAllowASingleWordLongerThan80Lines
 * @requires firstWord, secondWordWhichShouldMakeThisLineTooLongSinceThereIsAFirstWord
 * @wizmodule
 * @wizModule // INVALID_JSDOC_TAG
 */
// -4: LINE_TOO_LONG

goog.provide('MyClass');
goog.provide('goog.NumberLike');
goog.provide('goog.math.Vec2.sum');

goog.require('goog.array');
goog.require('goog.color');
goog.require('goog.dom.Range');
goog.require('goog.math.Matrix');
goog.require('goog.math.Vec2');


/**
 * Test the "no compilation should be done after annotation processing" tag.
 * @nocompile
 */


/**
 * @returns // INVALID_JSDOC_TAG
 * @params  // INVALID_JSDOC_TAG
 * @defines // INVALID_JSDOC_TAG
 * @nginject // INVALID_JSDOC_TAG
 * @wizAction // INVALID_JSDOC_TAG
 */
function badTags() {
}


// +4: MISSING_JSDOC_TAG_DESCRIPTION
/**
 * @license Description.
 * @preserve Good tag, missing punctuation
 * @preserve
 */
function goodTags() {
  /** @preserveTry */
  try {
    hexColor = goog.color.parse(value).hex;
  } catch (ext) {
    // Regression test. The preserveTry tag was incorrectly causing a warning
    // for a missing period at the end of tag description. Parsed as
    // flag: preserve, description: Try.
  }
}


/**
 * Some documentation goes here.
 *
 * @param {Object} object Good docs.
 * @ngInject
 * @wizaction
 */
function good(object) {
}


/**
 * Some documentation goes here.
 * @param {function(string, string) : string} f A function.
 */
function setConcatFunc(f) {
}


/**
 * Some docs.
 */
function missingParam(object) { // MISSING_PARAMETER_DOCUMENTATION
}


/**
 * @return {number} Hiya.
 * @override
 */
function missingParamButInherit(object) {
  return 3;
}


/**
 * @inheritDoc
 */
function missingParamButInherit(object) {
}


/**
 * @override
 */
function missingParamButOverride(object) {
}


// +2: UNNECESSARY_BRACES_AROUND_INHERIT_DOC
/**
 * {@inheritDoc}
 */
function missingParamButInherit(object) {
}


/**
 * Some docs.
 *
 * @param {Object} object Docs.
 */
function mismatchedParam(elem) { // WRONG_PARAMETER_DOCUMENTATION
  /** @param {number} otherElem */
  function nestedFunction(elem) { // WRONG_PARAMETER_DOCUMENTATION
  };
}


/**
 * @return {boolean} A boolean primitive.
 */
function goodReturn() {
  return something;
}


/**
 * @return {some.long.type.that.will.make.the.description.start.on.next.line}
 *     An object.
 */
function anotherGoodReturn() {
  return something;
}


// +2: MISSING_JSDOC_TAG_TYPE
/**
 * @return false.
 */
function missingReturnType() {
  return something;
}


// +2: MISSING_SPACE
/**
 * @return{type}
 */
function missingSpaceOnReturnType() {
  return something;
}


// +2: MISSING_JSDOC_TAG_TYPE
/**
 * @return
 */
function missingReturnType() {
  return something;
}

class.missingDocs = function() { // MISSING_MEMBER_DOCUMENTATION
};


/**
 * No return doc needed.
 */
function okMissingReturnDoc() {
  return;
}


// +2: UNNECESSARY_RETURN_DOCUMENTATION
/**
 * @return {number} Unnecessary return doc.
 */
function unnecessaryMissingReturnDoc() {
}


/**
 * The "suppress" causes the compiler to ignore the 'debugger' statement.
 * @suppress {checkDebuggerStatement}
 */
function checkDebuggerStatementWithSuppress() {
  debugger;
}


/**
 * Return doc is present, but the function doesn't have a 'return' statement.
 * The "suppress" causes the compiler to ignore the error.
 * @suppress {missingReturn}
 * @return {string}
 */
function unnecessaryMissingReturnDocWithSuppress() {
  if (false) {
    return '';
  } else {
    // Missing return statement in this branch.
  }
}


// +3: MISSING_JSDOC_TAG_TYPE
// +2: UNNECESSARY_RETURN_DOCUMENTATION
/**
 * @return
 */
function unnecessaryMissingReturnNoType() {
}


/**
 * @return {undefined} Ok unnecessary return doc.
 */
function okUnnecessaryMissingReturnDoc() {
}


/**
 * @return {*} Ok unnecessary return doc.
 */
function okUnnecessaryMissingReturnDoc2() {
}


/**
 * @return {void} Ok unnecessary return doc.
 */
function okUnnecessaryMissingReturnDoc3() {
}


/**
 * This function doesn't return anything, but it does contain the string return.
 */
function makeSureReturnTokenizesRight() {
  fn(returnIsNotSomethingHappeningHere);
}


/**
 * @return {number|undefined} Ok unnecessary return doc.
 */
function okUnnecessaryMissingReturnDoc3() {
}


/**
 * @return {number} Ok unnecessary return doc.
 */
function okUnnecessaryReturnWithThrow() {
  throw 'foo';
}


/** @inheritDoc */
function okNoReturnWithInheritDoc() {
  return 10;
}


/** @override */
function okNoReturnWithOverride() {
  return 10;
}


/**
 * No return doc.
 */ // MISSING_RETURN_DOCUMENTATION
function badMissingReturnDoc() {
  return 10;
}



/**
 * Constructor so we should not have a return jsdoc tag.
 * @constructor
 */
function OkNoReturnWithConstructor() {
  return this;
}


/**
 * Type of array is known, so the cast is unnecessary.
 * @suppress {unnecessaryCasts}
 */
function unnecessaryCastWithSuppress() {
  var numberArray = /** @type {!Array.<number>} */ ([]);
  /** @type {number} */ (goog.array.peek(numberArray));
}



/**
 * Make sure the 'unrestricted' annotation is accepted.
 * @constructor @unrestricted
 */
function UnrestrictedClass() {}



/**
 * Check definition of fields in constructors.
 * @constructor
 */
function AConstructor() {
  /**
   * A field.
   * @type {string}
   * @private
   */
  this.isOk_ = 'ok';

  // +5: MISSING_PRIVATE
  /**
   * Another field.
   * @type {string}
   */
  this.isBad_ = 'missing private';

  /**
   * This is ok, but a little weird.
   * @type {number}
   * @private
   */
  var x = this.x_ = 10;

  // At first, this block mis-attributed the first typecast as a member doc,
  // and therefore expected it to contain @private.
  if (goog.math.Matrix.isValidArray(/** @type {Array} */ (m))) {
    this.array_ = goog.array.clone(/** @type {Array.<Array.<number>>} */ (m));
  }

  // Use the private and local variables we've defined so they don't generate a
  // warning.
  var y = [
    this.isOk_,
    this.isBad_,
    this.array_,
    this.x_,
    y,
    x
  ];
}


/**
 * @desc This message description is allowed.
 */
var MSG_YADDA_YADDA_YADDA = 'A great message!';


/**
 * @desc So is this one.
 * @hidden
 * @meaning Some unusual meaning.
 */
x.y.z.MSG_YADDA_YADDA_YADDA = 'A great message!';


/**
 * @desc But desc can only apply to messages.
 */
var x = 10; // INVALID_USE_OF_DESC_TAG


/**
 * Same with hidden.
 * @hidden
 */
var x = 10; // INVALID_USE_OF_DESC_TAG


/**
 * Same with meaning.
 * @meaning Some unusual meaning.
 */
var x = 10; // INVALID_USE_OF_DESC_TAG


// +9: MISSING_SPACE
// +9: MISSING_JSDOC_TAG_TYPE
// +10: OUT_OF_ORDER_JSDOC_TAG_TYPE
// +10: MISSING_JSDOC_TAG_TYPE, MISSING_SPACE
/**
 * Lots of problems in this documentation.
 *
 * @param {Object} q params b & d are missing descriptions.
 * @param {Object} a param d is missing a type (oh my).
 * @param {Object}b
 * @param d
 * @param {Object} x param desc.
 * @param z {type} Out of order type.
 * @param{} y Empty type and missing space.
 * @param {Object} omega mis-matched param.
 */
function manyProblems(a, b, c, d, x, z, y, alpha) {
  // -1: MISSING_PARAMETER_DOCUMENTATION, EXTRA_PARAMETER_DOCUMENTATION
  // -2: WRONG_PARAMETER_DOCUMENTATION
}


/**
 * Good docs
 *
 * @param {really.really.really.really.really.really.really.long.type} good
 *     My param description.
 * @param {really.really.really.really.really.really.really.really.long.type}
 *     okay My param description.
 * @param
 *     {really.really.really.really.really.really.really.really.really.really.long.type}
 *     fine Wow that's a lot of wrapping.
 */
function wrappedParams(good, okay, fine) {
}


// +4: MISSING_JSDOC_TAG_TYPE
// +3: MISSING_JSDOC_PARAM_NAME
/**
 * Really bad
 * @param
 */
function reallyBadParam(a) { // MISSING_PARAMETER_DOCUMENTATION
}


/**
 * Some docs.
 *
 * @private
 */
class.goodPrivate_ = function() {
};


/**
 * Some docs.
 */
class.missingPrivate_ = function() { // MISSING_PRIVATE
};


/**
 * Some docs.
 *
 * @private
 */
class.extraPrivate = function() { // EXTRA_PRIVATE
};


/**
 * Anything ending with two underscores is not treated as private.
 */
class.__iterator__ = function() {
};


/**
 * Some docs.
 * @package
 */
class.goodPackage = function() {
};


/**
 * Some docs.
 * @package
 */
class.badPackage_ = function() { // MISSING_PRIVATE
};


/**
 * Some docs.
 * @protected
 */
class.goodProtected = function() {
};


/**
 * Some docs.
 * @protected
 */
class.badProtected_ = function() { // MISSING_PRIVATE
};


/**
 * Example of a legacy name.
 * @protected
 * @suppress {underscore}
 */
class.dom_ = function() {
  /** @suppress {with} */
  with ({}) {}
};


/**
 * Legacy names must be protected.
 * @suppress {underscore}
 */
class.dom_ = function() {
};


/**
 * Allow compound suppression.
 * @private
 */
class.dom_ = function() {
  /** @suppress {visibility|with} */
  with ({}) {}
};


/**
 * Allow compound suppression.
 * @private
 */
class.dom_ = function() {
  /** @suppress {visibility,with} */
  with ({}) {}
};


// +4: UNNECESSARY_SUPPRESS
/**
 * Some docs.
 * @private
 * @suppress {underscore}
 */
class.unnecessarySuppress_ = function() {
};


/**
 * Some docs.
 * @public
 */
class.goodProtected = function() {
};


/**
 * Some docs.
 * @public
 */
class.badProtected_ = function() { // MISSING_PRIVATE
};


/**
 * Example of a legacy name.
 * @public
 * @suppress {underscore}
 */
class.dom_ = function() {
};


// +5: JSDOC_PREFER_QUESTION_TO_PIPE_NULL
// +7: JSDOC_PREFER_QUESTION_TO_PIPE_NULL
/**
 * Check JsDoc type annotations.
 * @param {Object?} good A good one.
 * @param {Object|null} bad A bad one.
 * @param {Object|Element?} ok1 This is acceptable.
 * @param {Object|Element|null} right The right way to do the above.
 * @param {null|Object} bad2 Another bad one.
 * @param {Object?|Element} ok2 Not good but acceptable.
 * @param {Array.<string|number>?} complicated A good one that was reported as
 *     bad.  See bug 1154506.
 */
class.sampleFunction = function(good, bad, ok1, right, bad2, ok2,
    complicated) {
};


/**
 * @return {Object?} A good return.
 */
class.goodReturn = function() {
  return something;
};


/** @type {Array.<Object|null>} // JSDOC_PREFER_QUESTION_TO_PIPE_NULL */
class.badType;


/**
 * For template types, the ?TYPE notation is not parsed correctly by the
 * compiler, so don't warn here.
 * @type {Array.<TYPE|null>}
 * @template TYPE
 */
class.goodTemplateType;


// As the syntax may look ambivalent: The function returns just null.
/** @type {function():null|Object} */
class.goodType;


/** @type {function():(null|Object)} // JSDOC_PREFER_QUESTION_TO_PIPE_NULL */
class.badType;


// As the syntax may look ambivalent: The function returns just Object.
/** @type {function():Object|null} // JSDOC_PREFER_QUESTION_TO_PIPE_NULL */
class.badType;


/** @type {(function():Object)|null} // JSDOC_PREFER_QUESTION_TO_PIPE_NULL */
class.badType;


/** @type {function(null,Object)} */
class.goodType;


/** @type {{a:null,b:Object}} */
class.goodType;


// +2: JSDOC_PREFER_QUESTION_TO_PIPE_NULL
/**
 * @return {Object|null} A bad return.
 */
class.badReturn = function() {
  return something;
};


/**
 * @return {Object|Element?} An not so pretty return, but acceptable.
 */
class.uglyReturn = function() {
  return something;
};


/**
 * @return {Object|Element|null} The right way to do the above.
 */
class.okReturn = function() {
  return something;
};


// +2: MISSING_SPACE, MISSING_SPACE
/**
 * @return{mytype}Something.
 */
class.missingSpacesReturn = function() {
  return something;
};


/**
 * A good type in the new notation.
 * @type {Object?}
 */
class.otherGoodType = null;


/**
 * A complex type that should allow both ? and |.
 * @bug 1570763
 * @type {function(number?, Object|undefined):void}
 */
class.complexGoodType = goog.nullFunction;


/**
 * A complex bad type that we can catch, though there are many we can't.
 * Its acceptable.
 * @type {Array.<string>|string?}
 */
class.complexBadType = x || 'foo';


/**
 * A strange good type that caught a bad version of type checking from
 * other.js, so I added it here too just because.
 * @type {number|string|Object|Element|Array.<Object>|null}
 */
class.aStrangeGoodType = null;


/**
 * A type that includes spaces.
 * @type {function() : void}
 */
class.assignedFunc = goog.nullFunction;


// +4: JSDOC_PREFER_QUESTION_TO_PIPE_NULL
// +3: MISSING_BRACES_AROUND_TYPE
/**
 * A bad type.
 * @type Object|null
 */
class.badType = null;


// +3: JSDOC_PREFER_QUESTION_TO_PIPE_NULL
/**
 * A bad type, in the new notation.
 * @type {Object|null}
 */
class.badType = null;


/**
 * An not pretty type, but acceptable.
 * @type {Object|Element?}
 */
class.uglyType = null;


/**
 * The right way to do the above.
 * @type {Object|Element|null}
 */
class.okType = null;


/**
 * @type {boolean} Is it okay to have a description here?
 */
class.maybeOkType = null;


/**
 * A property whose type will be infered from the right hand side since it is
 * constant.
 * @const
 */
class.okWithoutType = 'stout';


/**
 * Const property without type and text in next line. b/10407058.
 * @const
 * TODO(user): Nothing to do, just for scenario.
 */
class.okWithoutType = 'string';


/**
 * Another constant property, but we should use the type tag if the type can't
 * be inferred.
 * @type {string}
 * @const
 */
class.useTypeWithConst = functionWithUntypedReturnValue();


/**
 * Another constant property, but using type with const if the type can't
 * be inferred.
 * @const {string}
 */
class.useTypeWithConst = functionWithUntypedReturnValue();


// +3: MISSING_BRACES_AROUND_TYPE
/**
 * Constant property without proper type.
 * @const string
 */
class.useImproperTypeWithConst = functionWithUntypedReturnValue();


/**
 * @define {boolean} A define.
 */
var COMPILED = false;


// +2: MISSING_JSDOC_TAG_TYPE
/**
 * @define A define without type info.
 */
var UNTYPED_DEFINE = false;


// +4: MISSING_JSDOC_TAG_DESCRIPTION, MISSING_SPACE
/**
 * A define without a description and missing a space.
 *
 * @define{boolean}
 */
var UNDESCRIBED_DEFINE = false;


// Test where to check for docs.
/**
 * Docs for member object.
 * @type {Object}
 */
x.objectContainingFunctionNeedsNoDocs = {
  x: function(params, params) {}
};

if (test) {
  x.functionInIfBlockNeedsDocs = function() { // MISSING_MEMBER_DOCUMENTATION
    x.functionInFunctionNeedsNoDocs = function() {
    };
  };
} else {
  x.functionInElseBlockNeedsDocs = function() { // MISSING_MEMBER_DOCUMENTATION
    x.functionInFunctionNeedsNoDocs = function() {
    };
  };
}


/**
 * Regression test.
 * @param {goog.math.Vec2} a
 * @param {goog.math.Vec2} b
 * @return {goog.math.Vec2} The sum vector.
 */
goog.math.Vec2.sum = function(a, b) {
  return new goog.math.Vec2(a.x + b.x, a.y + b.y);
};


// +6: JSDOC_MISSING_OPTIONAL_PREFIX
// +8: JSDOC_MISSING_OPTIONAL_PREFIX
// +8: JSDOC_MISSING_OPTIONAL_TYPE
// +8: JSDOC_MISSING_OPTIONAL_TYPE
/**
 * Optional parameters test.
 * @param {number=} numberOptional The name should be prefixed by opt_.
 * @param {function(number=)} funcOk Ok.
 * @param {number} numberOk The type is ok.
 * @param {function(string=):number=} funcOpt Param name need opt_ prefix.
 * @param {string} opt_stringMissing The type miss an ending =.
 * @param {function(number=)} opt_func The type miss an ending =.
 * @param {string=} opt_ok The type is ok.
 * @param {function(string=):number=} opt_funcOk Type is ok.
 */
goog.math.Vec2.aFunction = function(
    numberOptional, funcOk, numberOk, funcOpt, opt_stringMissing, opt_func,
    opt_ok, opt_funcOk) {
};


/**
 * Good documentation!
 *
 * @override
 */
class.goodOverrideDocs = function() {
};


/**
 * Test that flags embedded in docs don't trigger ends with invalid character
 * error.
 * @bug 2983692
 * @deprecated Please use the {@code @hidden} annotation.
 */
function goodEndChar() {
}


/**
 * Test that previous case handles unballanced doc tags.
 * @param {boolean} a Whether we should honor '{' characters in the string.
 */
function goodEndChar2(a) {
}


/**
 * Regression test for braces in description invalidly being matched as types.
 * This caused a false error for missing punctuation because the bad token
 * caused us to incorrectly calculate the full description.
 * @bug 1406513
 * @return {Object|undefined} A hash containing the attributes for the found url
 *     as in: {url: "page1.html", title: "First page"}
 *     or undefined if no match was found.
 */
x.z.a = function() {
  return a;
};


/**
 * @bug 1492606 HTML parse error for JSDoc descriptions grashed gjslint.
 * @param {string} description a long email or common name, e.g.,
 *     "John Doe <john.doe@gmail.com>" or "Birthdays Calendar"
 */
function calendar(description) {
}


/**
 * @bug 1492606 HTML parse error for JSDoc descriptions grashed gjslint.
 * @param {string} description a long email or common name, e.g.,
 *     "John Doe <john.doe@gmail.com>" or <b>"Birthdays Calendar".</b>
 */
function calendar(description) {
}


/**
 * Regression test for invoked functions, this code used to report missing
 * param and missing return errors.
 * @type {number}
 */
x.y.z = (function(x) {
  return x + 1;
})();


/**
 * Test for invoked function as part of an expression.  It should not return
 * an error for missing docs for x.
 */
goog.currentTime = something.Else || (function(x) {
  //...
})(10);


/**
 * @type boolean //MISSING_BRACES_AROUND_TYPE
 */
foo.bar = true;


/**
 * @enum {null //MISSING_BRACES_AROUND_TYPE
 */
bar.foo = null;


/**
 * @extends Object} //MISSING_BRACES_AROUND_TYPE
 */ // JSDOC_DOES_NOT_PARSE
bar.baz = x;


/** @inheritDoc */ // INVALID_INHERIT_DOC_PRIVATE
x.privateFoo_ = function() { // MISSING_PRIVATE
};


/**
 * Does bar.
 * @override // INVALID_OVERRIDE_PRIVATE
 */
x.privateBar_ = function() { // MISSING_PRIVATE
};


/**
 * Inherits private baz_ method (evil, wrong behavior, but we have no choice).
 * @override
 * @suppress {accessControls}
 */
x.prototype.privateBaz_ = function() {
};


/**
 * This looks like a function but it's a function call.
 * @type {number}
 */
test.x = function() {
  return 3;
}();


/**
 * Invalid reference to this.
 */ // MISSING_JSDOC_TAG_THIS
test.x.y = function() {
  var x = this.x; // UNUSED_LOCAL_VARIABLE
};


/**
 * Invalid write to this.
 */ // MISSING_JSDOC_TAG_THIS
test.x.y = function() {
  this.x = 10;
};


/**
 * Invalid standalone this.
 */ // MISSING_JSDOC_TAG_THIS
test.x.y = function() {
  some.func.call(this);
};


/**
 * Invalid reference to this.
 */ // MISSING_JSDOC_TAG_THIS
function a() {
  var x = this.x; // UNUSED_LOCAL_VARIABLE
}


/**
 * Invalid write to this.
 */ // MISSING_JSDOC_TAG_THIS
function b() {
  this.x = 10;
}


/**
 * Invalid standalone this.
 */ // MISSING_JSDOC_TAG_THIS
function c() {
  some.func.call(this);
}


/**
 * Ok to do any in a prototype.
 */
test.prototype.x = function() {
  var x = this.x;
  this.y = x;
  some.func.call(this);
};


/**
 * Ok to do any in a prototype that ends in a hex-like number.
 */
test.prototype.getColorX2 = function() {
  var x = this.x;
  this.y = x;
  some.func.call(this);
};


/**
 * Ok to do any in a function with documented this usage.
 * @this {test.x.y} Object bound to this via goog.bind.
 */
function a() {
  var x = this.x;
  this.y = x;
  some.func.call(this);
}


/**
 * Ok to do any in a function with documented this usage.
 * @this {test.x.y} Object bound to this via goog.bind.
 */
test.x.y = function() {
  var x = this.x;
  this.y = x;
  some.func.call(this);
};


/**
 * Regression test for bug 1220601. Wrapped function declarations shouldn't
 * cause need for an (at)this flag, which I can't write out or it would get
 * parsed as being here.
 * @param {Event} e The event.
 */
detroit.commands.ChangeOwnerCommand
    .prototype.handleDocumentStoreCompleteEvent = function(e) {
  this.x = e.target;
};



/**
 * Ok to do any in a constructor.
 * @constructor
 */
test.x.y = function() {
  this.y = x;
  var x = this.y; // UNUSED_LOCAL_VARIABLE
  some.func.call(this);
};

// Test that anonymous function doesn't throw an error.
window.setTimeout(function() {
  var x = 10; // UNUSED_LOCAL_VARIABLE
}, 0);


/**
 * @bug 1234567
 */
function testGoodBug() {
}


/**
 * @bug 1234567 Descriptions are allowed.
 */
function testGoodBugWithDescription() {
}


// +2: NO_BUG_NUMBER_AFTER_BUG_TAG
/**
 * @bug Wrong
 */
function testBadBugNumber() {
}


// +2: NO_BUG_NUMBER_AFTER_BUG_TAG
/**
 * @bug Wrong
 */
function testMissingBugNumber() {
}



/**
 * @interface
 */
function testInterface() {
}



/**
 * @implements {testInterface}
 * @constructor
 */
function testImplements() {
}


/**
 * Function that has an export jsdoc tag.
 * @export
 */
function testExport() {
}


/**
 * Declare and doc this member here, without assigning to it.
 * @bug 1473402
 * @type {number}
 */
x.declareOnly;

if (!someCondition) {
  x.declareOnly = 10;
}


/**
 * JsDoc describing array x.y as an array of function(arg).  The missing
 * semicolon caused the original bug.
 * @type {Array.<Function>}
 */
x.y = [] // MISSING_SEMICOLON
x.y[0] = function(arg) {};
x.y[1] = function(arg) {};


/**
 * Regression test for unfiled bug where descriptions didn't properly exclude
 * the star-slash that must end doc comments.
 * @return {Function} A factory method.
 */
x.y.foo = function() {
  /** @return {goog.dom.Range} A range. */
  return function() {
    return goog.dom.Range.createRangeFromNothing();
  };
};


// +4: INCORRECT_SUPPRESS_SYNTAX
// +4: INVALID_SUPPRESS_TYPE
/**
 * Docs...
 * @suppress
 * @suppress {fake}
 */
class.x = 10;


/**
 * These docs are OK.  They used to not parse the identifier due to the use of
 * array indices.
 * @bug 1640846
 * @private
 */
window['goog']['forms']['Validation'].prototype.form_ = null;


/**
 * Check JsDoc multiline type annotations.
 * @param {string|
 *     number} multiline description.
 */
function testMultiline(multiline) {
}


/**
 * Check JsDoc nosideeffects annotations.
 * @nosideeffects
 */
function testNoSideEffects() {
}


/**
 * @enum {google.visualization.DateFormat|google.visualization.NumberFormat|
 *     google.visualization.PatternFormat}
 */
MultiLineEnumTypeTest = {
  FOO: 1,
  BAR: 2,
  BAZ: 3
};


/**
 * @enum {google.visualization.DateFormat|google.visualization.NumberFormat|google.visualization.PatternFormat}
 */
AllowedLongLineEnum = {
  CAT: 1,
  DOG: 2,
  RAT: 3
};


/**
 * Typeless enum test
 * @enum
 */
TypelessEnumTest = {
  OK: 0,
  CHECKING: 1,
  DOWNLOADING: 2,
  FAILURE: 3
};

// Regression test for bug 1880803, shouldn't need to document assignments to
// prototype.
x.prototype = {};

y
    .prototype = {};

x.y
    .z.prototype = {};

x.myprototype = {}; // MISSING_MEMBER_DOCUMENTATION

x.prototype.y = 5; // MISSING_MEMBER_DOCUMENTATION

x.prototype
    .y.z = {}; // MISSING_MEMBER_DOCUMENTATION


/** @typedef {(string|number)} */
goog.NumberLike;


/**
 * Something from the html5 externs file.
 * @type {string}
 * @implicitCast
 */
CanvasRenderingContext2D.prototype.fillStyle;



/**
 * Regression test.
 * @bug 2994247
 * @inheritDoc
 * @extends {Bar}
 * @constructor
 * @private
 */
Foo_ = function() {
};


/**
 * @param {function(this:T,...)} fn The function.
 * @param {T} obj The object.
 * @template T
 */
function bind(fn, obj) {
}



/**
 * @constructor
 * @classTemplate T
 */
function MyClass() {
}


foo(/** @lends {T} */ ({foo: 'bar'}));



/**
 * @param {*} x  .
 * @constructor
 * @struct
 */
function StructMaker(x) { this.x = x; }

var structObjLit = /** @struct */ { x: 123 };



/**
 * @param {*} x .
 * @constructor
 * @dict
 */
function DictMaker(x) { this['x'] = x; }

var dictObjLit = /** @dict */ { x: 123 };


/**
 * @idGenerator
 * @param {string} x .
 * @return {string} .
 */
function makeId(x) {
  return '';
}


/**
 * @consistentIdGenerator
 * @param {string} x .
 * @return {string} .
 */
function makeConsistentId(x) {
  return '';
}


/**
 * @stableIdGenerator
 * @param {string} x .
 * @return {string} .
 */
function makeStableId(x) {
  return '';
}


/**
 * Test to make sure defining object with object literal doest not produce
 * doc warning for @this.
 * Regression test for b/4073735.
 */
var Foo = function();
Foo.prototype = {
  /**
   * @return {number} Never.
   */
  method: function() {
    return this.method();
  }
};

/** Regression tests for annotation types with spaces. */


/** @enum {goog.events.Event<string, number>} */
var Bar;



/**
 * @constructor
 * @implements {goog.dom.Range<string, number>}
 */
var Foo = function() {
  /** @final {goog.events.Event<string, number>} */
  this.bar = null;
};

/* Regression tests for not ending block comments. Keep at end of file! **/
/**
 * When there are multiple asteriks. In the failure case we would get an
 * error that the file ended mid comment, with no end comment token***/
/**
 * Was a separate bug 2950646 when the closing bit was on it's own line
 * because the ending star was being put into a different token type: DOC_PREFIX
 * rather than DOC_COMMENT.
 **/

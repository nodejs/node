/**
 * @preserve Copyright 2012 Robert Gust-Bardon <http://robert.gust-bardon.org/>.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 *     * Redistributions of source code must retain the above
 *       copyright notice, this list of conditions and the following
 *       disclaimer.
 *
 *     * Redistributions in binary form must reproduce the above
 *       copyright notice, this list of conditions and the following
 *       disclaimer in the documentation and/or other materials
 *       provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDER "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY,
 * OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR
 * TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF
 * THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/**
 * @fileoverview Enhances <a href="https://github.com/mishoo/UglifyJS/"
 * >UglifyJS</a> with consolidation of null, Boolean, and String values.
 * <p>Also known as aliasing, this feature has been deprecated in <a href=
 * "http://closure-compiler.googlecode.com/">the Closure Compiler</a> since its
 * initial release, where it is unavailable from the <abbr title=
 * "command line interface">CLI</a>. The Closure Compiler allows one to log and
 * influence this process. In contrast, this implementation does not introduce
 * any variable declarations in global code and derives String values from
 * identifier names used as property accessors.</p>
 * <p>Consolidating literals may worsen the data compression ratio when an <a
 * href="http://tools.ietf.org/html/rfc2616#section-3.5">encoding
 * transformation</a> is applied. For instance, <a href=
 * "http://code.jquery.com/jquery-1.7.1.js">jQuery 1.7.1</a> takes 248235 bytes.
 * Building it with <a href="https://github.com/mishoo/UglifyJS/tarball/v1.2.5">
 * UglifyJS v1.2.5</a> results in 93647 bytes (37.73% of the original) which are
 * then compressed to 33154 bytes (13.36% of the original) using <a href=
 * "http://linux.die.net/man/1/gzip">gzip(1)</a>. Building it with the same
 * version of UglifyJS 1.2.5 patched with the implementation of consolidation
 * results in 80784 bytes (a decrease of 12863 bytes, i.e. 13.74%, in comparison
 * to the aforementioned 93647 bytes) which are then compressed to 34013 bytes
 * (an increase of 859 bytes, i.e. 2.59%, in comparison to the aforementioned
 * 33154 bytes).</p>
 * <p>Written in <a href="http://es5.github.com/#x4.2.2">the strict variant</a>
 * of <a href="http://es5.github.com/">ECMA-262 5.1 Edition</a>. Encoded in <a
 * href="http://tools.ietf.org/html/rfc3629">UTF-8</a>. Follows <a href=
 * "http://google-styleguide.googlecode.com/svn-history/r76/trunk/javascriptguide.xml"
 * >Revision 2.28 of the Google JavaScript Style Guide</a> (except for the
 * discouraged use of the {@code function} tag and the {@code namespace} tag).
 * 100% typed for the <a href=
 * "http://closure-compiler.googlecode.com/files/compiler-20120123.tar.gz"
 * >Closure Compiler Version 1741</a>.</p>
 * <p>Should you find this software useful, please consider <a href=
 * "https://paypal.com/cgi-bin/webscr?cmd=_s-xclick&hosted_button_id=JZLW72X8FD4WG"
 * >a donation</a>.</p>
 * @author follow.me@RGustBardon (Robert Gust-Bardon)
 * @supported Tested with:
 *     <ul>
 *     <li><a href="http://nodejs.org/dist/v0.6.10/">Node v0.6.10</a>,</li>
 *     <li><a href="https://github.com/mishoo/UglifyJS/tarball/v1.2.5">UglifyJS
 *       v1.2.5</a>.</li>
 *     </ul>
 */

/*global console:false, exports:true, module:false, require:false */
/*jshint sub:true */
/**
 * Consolidates null, Boolean, and String values found inside an <abbr title=
 * "abstract syntax tree">AST</abbr>.
 * @param {!TSyntacticCodeUnit} oAbstractSyntaxTree An array-like object
 *     representing an <abbr title="abstract syntax tree">AST</abbr>.
 * @return {!TSyntacticCodeUnit} An array-like object representing an <abbr
 *     title="abstract syntax tree">AST</abbr> with its null, Boolean, and
 *     String values consolidated.
 */
// TODO(user) Consolidation of mathematical values found in numeric literals.
// TODO(user) Unconsolidation.
// TODO(user) Consolidation of ECMA-262 6th Edition programs.
// TODO(user) Rewrite in ECMA-262 6th Edition.
exports['ast_consolidate'] = function(oAbstractSyntaxTree) {
  'use strict';
  /*jshint bitwise:true, curly:true, eqeqeq:true, forin:true, immed:true,
        latedef:true, newcap:true, noarge:true, noempty:true, nonew:true,
        onevar:true, plusplus:true, regexp:true, undef:true, strict:true,
        sub:false, trailing:true */

  var _,
      /**
       * A record consisting of data about one or more source elements.
       * @constructor
       * @nosideeffects
       */
      TSourceElementsData = function() {
        /**
         * The category of the elements.
         * @type {number}
         * @see ESourceElementCategories
         */
        this.nCategory = ESourceElementCategories.N_OTHER;
        /**
         * The number of occurrences (within the elements) of each primitive
         * value that could be consolidated.
         * @type {!Array.<!Object.<string, number>>}
         */
        this.aCount = [];
        this.aCount[EPrimaryExpressionCategories.N_IDENTIFIER_NAMES] = {};
        this.aCount[EPrimaryExpressionCategories.N_STRING_LITERALS] = {};
        this.aCount[EPrimaryExpressionCategories.N_NULL_AND_BOOLEAN_LITERALS] =
            {};
        /**
         * Identifier names found within the elements.
         * @type {!Array.<string>}
         */
        this.aIdentifiers = [];
        /**
         * Prefixed representation Strings of each primitive value that could be
         * consolidated within the elements.
         * @type {!Array.<string>}
         */
        this.aPrimitiveValues = [];
      },
      /**
       * A record consisting of data about a primitive value that could be
       * consolidated.
       * @constructor
       * @nosideeffects
       */
      TPrimitiveValue = function() {
        /**
         * The difference in the number of terminal symbols between the original
         * source text and the one with the primitive value consolidated. If the
         * difference is positive, the primitive value is considered worthwhile.
         * @type {number}
         */
        this.nSaving = 0;
        /**
         * An identifier name of the variable that will be declared and assigned
         * the primitive value if the primitive value is consolidated.
         * @type {string}
         */
        this.sName = '';
      },
      /**
       * A record consisting of data on what to consolidate within the range of
       * source elements that is currently being considered.
       * @constructor
       * @nosideeffects
       */
      TSolution = function() {
        /**
         * An object whose keys are prefixed representation Strings of each
         * primitive value that could be consolidated within the elements and
         * whose values are corresponding data about those primitive values.
         * @type {!Object.<string, {nSaving: number, sName: string}>}
         * @see TPrimitiveValue
         */
        this.oPrimitiveValues = {};
        /**
         * The difference in the number of terminal symbols between the original
         * source text and the one with all the worthwhile primitive values
         * consolidated.
         * @type {number}
         * @see TPrimitiveValue#nSaving
         */
        this.nSavings = 0;
      },
      /**
       * The processor of <abbr title="abstract syntax tree">AST</abbr>s found
       * in UglifyJS.
       * @namespace
       * @type {!TProcessor}
       */
      oProcessor = (/** @type {!TProcessor} */ require('./process')),
      /**
       * A record consisting of a number of constants that represent the
       * difference in the number of terminal symbols between a source text with
       * a modified syntactic code unit and the original one.
       * @namespace
       * @type {!Object.<string, number>}
       */
      oWeights = {
        /**
         * The difference in the number of punctuators required by the bracket
         * notation and the dot notation.
         * <p><code>'[]'.length - '.'.length</code></p>
         * @const
         * @type {number}
         */
        N_PROPERTY_ACCESSOR: 1,
        /**
         * The number of punctuators required by a variable declaration with an
         * initialiser.
         * <p><code>':'.length + ';'.length</code></p>
         * @const
         * @type {number}
         */
        N_VARIABLE_DECLARATION: 2,
        /**
         * The number of terminal symbols required to introduce a variable
         * statement (excluding its variable declaration list).
         * <p><code>'var '.length</code></p>
         * @const
         * @type {number}
         */
        N_VARIABLE_STATEMENT_AFFIXATION: 4,
        /**
         * The number of terminal symbols needed to enclose source elements
         * within a function call with no argument values to a function with an
         * empty parameter list.
         * <p><code>'(function(){}());'.length</code></p>
         * @const
         * @type {number}
         */
        N_CLOSURE: 17
      },
      /**
       * Categories of primary expressions from which primitive values that
       * could be consolidated are derivable.
       * @namespace
       * @enum {number}
       */
      EPrimaryExpressionCategories = {
        /**
         * Identifier names used as property accessors.
         * @type {number}
         */
        N_IDENTIFIER_NAMES: 0,
        /**
         * String literals.
         * @type {number}
         */
        N_STRING_LITERALS: 1,
        /**
         * Null and Boolean literals.
         * @type {number}
         */
        N_NULL_AND_BOOLEAN_LITERALS: 2
      },
      /**
       * Prefixes of primitive values that could be consolidated.
       * The String values of the prefixes must have same number of characters.
       * The prefixes must not be used in any properties defined in any version
       * of <a href=
       * "http://www.ecma-international.org/publications/standards/Ecma-262.htm"
       * >ECMA-262</a>.
       * @namespace
       * @enum {string}
       */
      EValuePrefixes = {
        /**
         * Identifies String values.
         * @type {string}
         */
        S_STRING: '#S',
        /**
         * Identifies null and Boolean values.
         * @type {string}
         */
        S_SYMBOLIC: '#O'
      },
      /**
       * Categories of source elements in terms of their appropriateness of
       * having their primitive values consolidated.
       * @namespace
       * @enum {number}
       */
      ESourceElementCategories = {
        /**
         * Identifies a source element that includes the <a href=
         * "http://es5.github.com/#x12.10">{@code with}</a> statement.
         * @type {number}
         */
        N_WITH: 0,
        /**
         * Identifies a source element that includes the <a href=
         * "http://es5.github.com/#x15.1.2.1">{@code eval}</a> identifier name.
         * @type {number}
         */
        N_EVAL: 1,
        /**
         * Identifies a source element that must be excluded from the process
         * unless its whole scope is examined.
         * @type {number}
         */
        N_EXCLUDABLE: 2,
        /**
         * Identifies source elements not posing any problems.
         * @type {number}
         */
        N_OTHER: 3
      },
      /**
       * The list of literals (other than the String ones) whose primitive
       * values can be consolidated.
       * @const
       * @type {!Array.<string>}
       */
      A_OTHER_SUBSTITUTABLE_LITERALS = [
        'null',   // The null literal.
        'false',  // The Boolean literal {@code false}.
        'true'    // The Boolean literal {@code true}.
      ];

  (/**
    * Consolidates all worthwhile primitive values in a syntactic code unit.
    * @param {!TSyntacticCodeUnit} oSyntacticCodeUnit An array-like object
    *     representing the branch of the abstract syntax tree representing the
    *     syntactic code unit along with its scope.
    * @see TPrimitiveValue#nSaving
    */
   function fExamineSyntacticCodeUnit(oSyntacticCodeUnit) {
     var _,
         /**
          * Indicates whether the syntactic code unit represents global code.
          * @type {boolean}
          */
         bIsGlobal = 'toplevel' === oSyntacticCodeUnit[0],
         /**
          * Indicates whether the whole scope is being examined.
          * @type {boolean}
          */
         bIsWhollyExaminable = !bIsGlobal,
         /**
          * An array-like object representing source elements that constitute a
          * syntactic code unit.
          * @type {!TSyntacticCodeUnit}
          */
         oSourceElements,
         /**
          * A record consisting of data about the source element that is
          * currently being examined.
          * @type {!TSourceElementsData}
          */
         oSourceElementData,
         /**
          * The scope of the syntactic code unit.
          * @type {!TScope}
          */
         oScope,
         /**
          * An instance of an object that allows the traversal of an <abbr
          * title="abstract syntax tree">AST</abbr>.
          * @type {!TWalker}
          */
         oWalker,
         /**
          * An object encompassing collections of functions used during the
          * traversal of an <abbr title="abstract syntax tree">AST</abbr>.
          * @namespace
          * @type {!Object.<string, !Object.<string, function(...[*])>>}
          */
         oWalkers = {
           /**
            * A collection of functions used during the surveyance of source
            * elements.
            * @namespace
            * @type {!Object.<string, function(...[*])>}
            */
           oSurveySourceElement: {
             /**#nocode+*/  // JsDoc Toolkit 2.4.0 hides some of the keys.
             /**
              * Classifies the source element as excludable if it does not
              * contain a {@code with} statement or the {@code eval} identifier
              * name. Adds the identifier of the function and its formal
              * parameters to the list of identifier names found.
              * @param {string} sIdentifier The identifier of the function.
              * @param {!Array.<string>} aFormalParameterList Formal parameters.
              * @param {!TSyntacticCodeUnit} oFunctionBody Function code.
              */
             'defun': function(
                 sIdentifier,
                 aFormalParameterList,
                 oFunctionBody) {
               fClassifyAsExcludable();
               fAddIdentifier(sIdentifier);
               aFormalParameterList.forEach(fAddIdentifier);
             },
             /**
              * Increments the count of the number of occurrences of the String
              * value that is equivalent to the sequence of terminal symbols
              * that constitute the encountered identifier name.
              * @param {!TSyntacticCodeUnit} oExpression The nonterminal
              *     MemberExpression.
              * @param {string} sIdentifierName The identifier name used as the
              *     property accessor.
              * @return {!Array} The encountered branch of an <abbr title=
              *     "abstract syntax tree">AST</abbr> with its nonterminal
              *     MemberExpression traversed.
              */
             'dot': function(oExpression, sIdentifierName) {
               fCountPrimaryExpression(
                   EPrimaryExpressionCategories.N_IDENTIFIER_NAMES,
                   EValuePrefixes.S_STRING + sIdentifierName);
               return ['dot', oWalker.walk(oExpression), sIdentifierName];
             },
             /**
              * Adds the optional identifier of the function and its formal
              * parameters to the list of identifier names found.
              * @param {?string} sIdentifier The optional identifier of the
              *     function.
              * @param {!Array.<string>} aFormalParameterList Formal parameters.
              * @param {!TSyntacticCodeUnit} oFunctionBody Function code.
              */
             'function': function(
                 sIdentifier,
                 aFormalParameterList,
                 oFunctionBody) {
               if ('string' === typeof sIdentifier) {
                 fAddIdentifier(sIdentifier);
               }
               aFormalParameterList.forEach(fAddIdentifier);
             },
             /**
              * Either increments the count of the number of occurrences of the
              * encountered null or Boolean value or classifies a source element
              * as containing the {@code eval} identifier name.
              * @param {string} sIdentifier The identifier encountered.
              */
             'name': function(sIdentifier) {
               if (-1 !== A_OTHER_SUBSTITUTABLE_LITERALS.indexOf(sIdentifier)) {
                 fCountPrimaryExpression(
                     EPrimaryExpressionCategories.N_NULL_AND_BOOLEAN_LITERALS,
                     EValuePrefixes.S_SYMBOLIC + sIdentifier);
               } else {
                 if ('eval' === sIdentifier) {
                   oSourceElementData.nCategory =
                       ESourceElementCategories.N_EVAL;
                 }
                 fAddIdentifier(sIdentifier);
               }
             },
             /**
              * Classifies the source element as excludable if it does not
              * contain a {@code with} statement or the {@code eval} identifier
              * name.
              * @param {TSyntacticCodeUnit} oExpression The expression whose
              *     value is to be returned.
              */
             'return': function(oExpression) {
               fClassifyAsExcludable();
             },
             /**
              * Increments the count of the number of occurrences of the
              * encountered String value.
              * @param {string} sStringValue The String value of the string
              *     literal encountered.
              */
             'string': function(sStringValue) {
               if (sStringValue.length > 0) {
                 fCountPrimaryExpression(
                     EPrimaryExpressionCategories.N_STRING_LITERALS,
                     EValuePrefixes.S_STRING + sStringValue);
               }
             },
             /**
              * Adds the identifier reserved for an exception to the list of
              * identifier names found.
              * @param {!TSyntacticCodeUnit} oTry A block of code in which an
              *     exception can occur.
              * @param {Array} aCatch The identifier reserved for an exception
              *     and a block of code to handle the exception.
              * @param {TSyntacticCodeUnit} oFinally An optional block of code
              *     to be evaluated regardless of whether an exception occurs.
              */
             'try': function(oTry, aCatch, oFinally) {
               if (Array.isArray(aCatch)) {
                 fAddIdentifier(aCatch[0]);
               }
             },
             /**
              * Classifies the source element as excludable if it does not
              * contain a {@code with} statement or the {@code eval} identifier
              * name. Adds the identifier of each declared variable to the list
              * of identifier names found.
              * @param {!Array.<!Array>} aVariableDeclarationList Variable
              *     declarations.
              */
             'var': function(aVariableDeclarationList) {
               fClassifyAsExcludable();
               aVariableDeclarationList.forEach(fAddVariable);
             },
             /**
              * Classifies a source element as containing the {@code with}
              * statement.
              * @param {!TSyntacticCodeUnit} oExpression An expression whose
              *     value is to be converted to a value of type Object and
              *     become the binding object of a new object environment
              *     record of a new lexical environment in which the statement
              *     is to be executed.
              * @param {!TSyntacticCodeUnit} oStatement The statement to be
              *     executed in the augmented lexical environment.
              * @return {!Array} An empty array to stop the traversal.
              */
             'with': function(oExpression, oStatement) {
               oSourceElementData.nCategory = ESourceElementCategories.N_WITH;
               return [];
             }
             /**#nocode-*/  // JsDoc Toolkit 2.4.0 hides some of the keys.
           },
           /**
            * A collection of functions used while looking for nested functions.
            * @namespace
            * @type {!Object.<string, function(...[*])>}
            */
           oExamineFunctions: {
             /**#nocode+*/  // JsDoc Toolkit 2.4.0 hides some of the keys.
             /**
              * Orders an examination of a nested function declaration.
              * @this {!TSyntacticCodeUnit} An array-like object representing
              *     the branch of an <abbr title="abstract syntax tree"
              *     >AST</abbr> representing the syntactic code unit along with
              *     its scope.
              * @return {!Array} An empty array to stop the traversal.
              */
             'defun': function() {
               fExamineSyntacticCodeUnit(this);
               return [];
             },
             /**
              * Orders an examination of a nested function expression.
              * @this {!TSyntacticCodeUnit} An array-like object representing
              *     the branch of an <abbr title="abstract syntax tree"
              *     >AST</abbr> representing the syntactic code unit along with
              *     its scope.
              * @return {!Array} An empty array to stop the traversal.
              */
             'function': function() {
               fExamineSyntacticCodeUnit(this);
               return [];
             }
             /**#nocode-*/  // JsDoc Toolkit 2.4.0 hides some of the keys.
           }
         },
         /**
          * Records containing data about source elements.
          * @type {Array.<TSourceElementsData>}
          */
         aSourceElementsData = [],
         /**
          * The index (in the source text order) of the source element
          * immediately following a <a href="http://es5.github.com/#x14.1"
          * >Directive Prologue</a>.
          * @type {number}
          */
         nAfterDirectivePrologue = 0,
         /**
          * The index (in the source text order) of the source element that is
          * currently being considered.
          * @type {number}
          */
         nPosition,
         /**
          * The index (in the source text order) of the source element that is
          * the last element of the range of source elements that is currently
          * being considered.
          * @type {(undefined|number)}
          */
         nTo,
         /**
          * Initiates the traversal of a source element.
          * @param {!TWalker} oWalker An instance of an object that allows the
          *     traversal of an abstract syntax tree.
          * @param {!TSyntacticCodeUnit} oSourceElement A source element from
          *     which the traversal should commence.
          * @return {function(): !TSyntacticCodeUnit} A function that is able to
          *     initiate the traversal from a given source element.
          */
         cContext = function(oWalker, oSourceElement) {
           /**
            * @return {!TSyntacticCodeUnit} A function that is able to
            *     initiate the traversal from a given source element.
            */
           var fLambda = function() {
             return oWalker.walk(oSourceElement);
           };

           return fLambda;
         },
         /**
          * Classifies the source element as excludable if it does not
          * contain a {@code with} statement or the {@code eval} identifier
          * name.
          */
         fClassifyAsExcludable = function() {
           if (oSourceElementData.nCategory ===
               ESourceElementCategories.N_OTHER) {
             oSourceElementData.nCategory =
                 ESourceElementCategories.N_EXCLUDABLE;
           }
         },
         /**
          * Adds an identifier to the list of identifier names found.
          * @param {string} sIdentifier The identifier to be added.
          */
         fAddIdentifier = function(sIdentifier) {
           if (-1 === oSourceElementData.aIdentifiers.indexOf(sIdentifier)) {
             oSourceElementData.aIdentifiers.push(sIdentifier);
           }
         },
         /**
          * Adds the identifier of a variable to the list of identifier names
          * found.
          * @param {!Array} aVariableDeclaration A variable declaration.
          */
         fAddVariable = function(aVariableDeclaration) {
           fAddIdentifier(/** @type {string} */ aVariableDeclaration[0]);
         },
         /**
          * Increments the count of the number of occurrences of the prefixed
          * String representation attributed to the primary expression.
          * @param {number} nCategory The category of the primary expression.
          * @param {string} sName The prefixed String representation attributed
          *     to the primary expression.
          */
         fCountPrimaryExpression = function(nCategory, sName) {
           if (!oSourceElementData.aCount[nCategory].hasOwnProperty(sName)) {
             oSourceElementData.aCount[nCategory][sName] = 0;
             if (-1 === oSourceElementData.aPrimitiveValues.indexOf(sName)) {
               oSourceElementData.aPrimitiveValues.push(sName);
             }
           }
           oSourceElementData.aCount[nCategory][sName] += 1;
         },
         /**
          * Consolidates all worthwhile primitive values in a range of source
          *     elements.
          * @param {number} nFrom The index (in the source text order) of the
          *     source element that is the first element of the range.
          * @param {number} nTo The index (in the source text order) of the
          *     source element that is the last element of the range.
          * @param {boolean} bEnclose Indicates whether the range should be
          *     enclosed within a function call with no argument values to a
          *     function with an empty parameter list if any primitive values
          *     are consolidated.
          * @see TPrimitiveValue#nSaving
          */
         fExamineSourceElements = function(nFrom, nTo, bEnclose) {
           var _,
               /**
                * The index of the last mangled name.
                * @type {number}
                */
               nIndex = oScope.cname,
               /**
                * The index of the source element that is currently being
                * considered.
                * @type {number}
                */
               nPosition,
               /**
                * A collection of functions used during the consolidation of
                * primitive values and identifier names used as property
                * accessors.
                * @namespace
                * @type {!Object.<string, function(...[*])>}
                */
               oWalkersTransformers = {
                 /**
                  * If the String value that is equivalent to the sequence of
                  * terminal symbols that constitute the encountered identifier
                  * name is worthwhile, a syntactic conversion from the dot
                  * notation to the bracket notation ensues with that sequence
                  * being substituted by an identifier name to which the value
                  * is assigned.
                  * Applies to property accessors that use the dot notation.
                  * @param {!TSyntacticCodeUnit} oExpression The nonterminal
                  *     MemberExpression.
                  * @param {string} sIdentifierName The identifier name used as
                  *     the property accessor.
                  * @return {!Array} A syntactic code unit that is equivalent to
                  *     the one encountered.
                  * @see TPrimitiveValue#nSaving
                  */
                 'dot': function(oExpression, sIdentifierName) {
                   /**
                    * The prefixed String value that is equivalent to the
                    * sequence of terminal symbols that constitute the
                    * encountered identifier name.
                    * @type {string}
                    */
                   var sPrefixed = EValuePrefixes.S_STRING + sIdentifierName;

                   return oSolutionBest.oPrimitiveValues.hasOwnProperty(
                       sPrefixed) &&
                       oSolutionBest.oPrimitiveValues[sPrefixed].nSaving > 0 ?
                       ['sub',
                        oWalker.walk(oExpression),
                        ['name',
                         oSolutionBest.oPrimitiveValues[sPrefixed].sName]] :
                       ['dot', oWalker.walk(oExpression), sIdentifierName];
                 },
                 /**
                  * If the encountered identifier is a null or Boolean literal
                  * and its value is worthwhile, the identifier is substituted
                  * by an identifier name to which that value is assigned.
                  * Applies to identifier names.
                  * @param {string} sIdentifier The identifier encountered.
                  * @return {!Array} A syntactic code unit that is equivalent to
                  *     the one encountered.
                  * @see TPrimitiveValue#nSaving
                  */
                 'name': function(sIdentifier) {
                   /**
                    * The prefixed representation String of the identifier.
                    * @type {string}
                    */
                   var sPrefixed = EValuePrefixes.S_SYMBOLIC + sIdentifier;

                   return [
                     'name',
                     oSolutionBest.oPrimitiveValues.hasOwnProperty(sPrefixed) &&
                     oSolutionBest.oPrimitiveValues[sPrefixed].nSaving > 0 ?
                     oSolutionBest.oPrimitiveValues[sPrefixed].sName :
                     sIdentifier
                   ];
                 },
                 /**
                  * If the encountered String value is worthwhile, it is
                  * substituted by an identifier name to which that value is
                  * assigned.
                  * Applies to String values.
                  * @param {string} sStringValue The String value of the string
                  *     literal encountered.
                  * @return {!Array} A syntactic code unit that is equivalent to
                  *     the one encountered.
                  * @see TPrimitiveValue#nSaving
                  */
                 'string': function(sStringValue) {
                   /**
                    * The prefixed representation String of the primitive value
                    * of the literal.
                    * @type {string}
                    */
                   var sPrefixed =
                       EValuePrefixes.S_STRING + sStringValue;

                   return oSolutionBest.oPrimitiveValues.hasOwnProperty(
                       sPrefixed) &&
                       oSolutionBest.oPrimitiveValues[sPrefixed].nSaving > 0 ?
                       ['name',
                        oSolutionBest.oPrimitiveValues[sPrefixed].sName] :
                       ['string', sStringValue];
                 }
               },
               /**
                * Such data on what to consolidate within the range of source
                * elements that is currently being considered that lead to the
                * greatest known reduction of the number of the terminal symbols
                * in comparison to the original source text.
                * @type {!TSolution}
                */
               oSolutionBest = new TSolution(),
               /**
                * Data representing an ongoing attempt to find a better
                * reduction of the number of the terminal symbols in comparison
                * to the original source text than the best one that is
                * currently known.
                * @type {!TSolution}
                * @see oSolutionBest
                */
               oSolutionCandidate = new TSolution(),
               /**
                * A record consisting of data about the range of source elements
                * that is currently being examined.
                * @type {!TSourceElementsData}
                */
               oSourceElementsData = new TSourceElementsData(),
               /**
                * Variable declarations for each primitive value that is to be
                * consolidated within the elements.
                * @type {!Array.<!Array>}
                */
               aVariableDeclarations = [],
               /**
                * Augments a list with a prefixed representation String.
                * @param {!Array.<string>} aList A list that is to be augmented.
                * @return {function(string)} A function that augments a list
                *     with a prefixed representation String.
                */
               cAugmentList = function(aList) {
                 /**
                  * @param {string} sPrefixed Prefixed representation String of
                  *     a primitive value that could be consolidated within the
                  *     elements.
                  */
                 var fLambda = function(sPrefixed) {
                   if (-1 === aList.indexOf(sPrefixed)) {
                     aList.push(sPrefixed);
                   }
                 };

                 return fLambda;
               },
               /**
                * Adds the number of occurrences of a primitive value of a given
                * category that could be consolidated in the source element with
                * a given index to the count of occurrences of that primitive
                * value within the range of source elements that is currently
                * being considered.
                * @param {number} nPosition The index (in the source text order)
                *     of a source element.
                * @param {number} nCategory The category of the primary
                *     expression from which the primitive value is derived.
                * @return {function(string)} A function that performs the
                *     addition.
                * @see cAddOccurrencesInCategory
                */
               cAddOccurrences = function(nPosition, nCategory) {
                 /**
                  * @param {string} sPrefixed The prefixed representation String
                  *     of a primitive value.
                  */
                 var fLambda = function(sPrefixed) {
                   if (!oSourceElementsData.aCount[nCategory].hasOwnProperty(
                           sPrefixed)) {
                     oSourceElementsData.aCount[nCategory][sPrefixed] = 0;
                   }
                   oSourceElementsData.aCount[nCategory][sPrefixed] +=
                       aSourceElementsData[nPosition].aCount[nCategory][
                           sPrefixed];
                 };

                 return fLambda;
               },
               /**
                * Adds the number of occurrences of each primitive value of a
                * given category that could be consolidated in the source
                * element with a given index to the count of occurrences of that
                * primitive values within the range of source elements that is
                * currently being considered.
                * @param {number} nPosition The index (in the source text order)
                *     of a source element.
                * @return {function(number)} A function that performs the
                *     addition.
                * @see fAddOccurrences
                */
               cAddOccurrencesInCategory = function(nPosition) {
                 /**
                  * @param {number} nCategory The category of the primary
                  *     expression from which the primitive value is derived.
                  */
                 var fLambda = function(nCategory) {
                   Object.keys(
                       aSourceElementsData[nPosition].aCount[nCategory]
                   ).forEach(cAddOccurrences(nPosition, nCategory));
                 };

                 return fLambda;
               },
               /**
                * Adds the number of occurrences of each primitive value that
                * could be consolidated in the source element with a given index
                * to the count of occurrences of that primitive values within
                * the range of source elements that is currently being
                * considered.
                * @param {number} nPosition The index (in the source text order)
                *     of a source element.
                */
               fAddOccurrences = function(nPosition) {
                 Object.keys(aSourceElementsData[nPosition].aCount).forEach(
                     cAddOccurrencesInCategory(nPosition));
               },
               /**
                * Creates a variable declaration for a primitive value if that
                * primitive value is to be consolidated within the elements.
                * @param {string} sPrefixed Prefixed representation String of a
                *     primitive value that could be consolidated within the
                *     elements.
                * @see aVariableDeclarations
                */
               cAugmentVariableDeclarations = function(sPrefixed) {
                 if (oSolutionBest.oPrimitiveValues[sPrefixed].nSaving > 0) {
                   aVariableDeclarations.push([
                     oSolutionBest.oPrimitiveValues[sPrefixed].sName,
                     [0 === sPrefixed.indexOf(EValuePrefixes.S_SYMBOLIC) ?
                      'name' : 'string',
                      sPrefixed.substring(EValuePrefixes.S_SYMBOLIC.length)]
                   ]);
                 }
               },
               /**
                * Sorts primitive values with regard to the difference in the
                * number of terminal symbols between the original source text
                * and the one with those primitive values consolidated.
                * @param {string} sPrefixed0 The prefixed representation String
                *     of the first of the two primitive values that are being
                *     compared.
                * @param {string} sPrefixed1 The prefixed representation String
                *     of the second of the two primitive values that are being
                *     compared.
                * @return {number}
                *     <dl>
                *         <dt>-1</dt>
                *         <dd>if the first primitive value must be placed before
                *              the other one,</dd>
                *         <dt>0</dt>
                *         <dd>if the first primitive value may be placed before
                *              the other one,</dd>
                *         <dt>1</dt>
                *         <dd>if the first primitive value must not be placed
                *              before the other one.</dd>
                *     </dl>
                * @see TSolution.oPrimitiveValues
                */
               cSortPrimitiveValues = function(sPrefixed0, sPrefixed1) {
                 /**
                  * The difference between:
                  * <ol>
                  * <li>the difference in the number of terminal symbols
                  *     between the original source text and the one with the
                  *     first primitive value consolidated, and</li>
                  * <li>the difference in the number of terminal symbols
                  *     between the original source text and the one with the
                  *     second primitive value consolidated.</li>
                  * </ol>
                  * @type {number}
                  */
                 var nDifference =
                     oSolutionCandidate.oPrimitiveValues[sPrefixed0].nSaving -
                     oSolutionCandidate.oPrimitiveValues[sPrefixed1].nSaving;

                 return nDifference > 0 ? -1 : nDifference < 0 ? 1 : 0;
               },
               /**
                * Assigns an identifier name to a primitive value and calculates
                * whether instances of that primitive value are worth
                * consolidating.
                * @param {string} sPrefixed The prefixed representation String
                *     of a primitive value that is being evaluated.
                */
               fEvaluatePrimitiveValue = function(sPrefixed) {
                 var _,
                     /**
                      * The index of the last mangled name.
                      * @type {number}
                      */
                     nIndex,
                     /**
                      * The representation String of the primitive value that is
                      * being evaluated.
                      * @type {string}
                      */
                     sName =
                         sPrefixed.substring(EValuePrefixes.S_SYMBOLIC.length),
                     /**
                      * The number of source characters taken up by the
                      * representation String of the primitive value that is
                      * being evaluated.
                      * @type {number}
                      */
                     nLengthOriginal = sName.length,
                     /**
                      * The number of source characters taken up by the
                      * identifier name that could substitute the primitive
                      * value that is being evaluated.
                      * substituted.
                      * @type {number}
                      */
                     nLengthSubstitution,
                     /**
                      * The number of source characters taken up by by the
                      * representation String of the primitive value that is
                      * being evaluated when it is represented by a string
                      * literal.
                      * @type {number}
                      */
                     nLengthString = oProcessor.make_string(sName).length;

                 oSolutionCandidate.oPrimitiveValues[sPrefixed] =
                     new TPrimitiveValue();
                 do {  // Find an identifier unused in this or any nested scope.
                   nIndex = oScope.cname;
                   oSolutionCandidate.oPrimitiveValues[sPrefixed].sName =
                       oScope.next_mangled();
                 } while (-1 !== oSourceElementsData.aIdentifiers.indexOf(
                     oSolutionCandidate.oPrimitiveValues[sPrefixed].sName));
                 nLengthSubstitution = oSolutionCandidate.oPrimitiveValues[
                     sPrefixed].sName.length;
                 if (0 === sPrefixed.indexOf(EValuePrefixes.S_SYMBOLIC)) {
                   // foo:null, or foo:null;
                   oSolutionCandidate.oPrimitiveValues[sPrefixed].nSaving -=
                       nLengthSubstitution + nLengthOriginal +
                       oWeights.N_VARIABLE_DECLARATION;
                   // null vs foo
                   oSolutionCandidate.oPrimitiveValues[sPrefixed].nSaving +=
                       oSourceElementsData.aCount[
                           EPrimaryExpressionCategories.
                               N_NULL_AND_BOOLEAN_LITERALS][sPrefixed] *
                       (nLengthOriginal - nLengthSubstitution);
                 } else {
                   // foo:'fromCharCode';
                   oSolutionCandidate.oPrimitiveValues[sPrefixed].nSaving -=
                       nLengthSubstitution + nLengthString +
                       oWeights.N_VARIABLE_DECLARATION;
                   // .fromCharCode vs [foo]
                   if (oSourceElementsData.aCount[
                           EPrimaryExpressionCategories.N_IDENTIFIER_NAMES
                       ].hasOwnProperty(sPrefixed)) {
                     oSolutionCandidate.oPrimitiveValues[sPrefixed].nSaving +=
                         oSourceElementsData.aCount[
                             EPrimaryExpressionCategories.N_IDENTIFIER_NAMES
                         ][sPrefixed] *
                         (nLengthOriginal - nLengthSubstitution -
                          oWeights.N_PROPERTY_ACCESSOR);
                   }
                   // 'fromCharCode' vs foo
                   if (oSourceElementsData.aCount[
                           EPrimaryExpressionCategories.N_STRING_LITERALS
                       ].hasOwnProperty(sPrefixed)) {
                     oSolutionCandidate.oPrimitiveValues[sPrefixed].nSaving +=
                         oSourceElementsData.aCount[
                             EPrimaryExpressionCategories.N_STRING_LITERALS
                         ][sPrefixed] *
                         (nLengthString - nLengthSubstitution);
                   }
                 }
                 if (oSolutionCandidate.oPrimitiveValues[sPrefixed].nSaving >
                     0) {
                   oSolutionCandidate.nSavings +=
                       oSolutionCandidate.oPrimitiveValues[sPrefixed].nSaving;
                 } else {
                   oScope.cname = nIndex; // Free the identifier name.
                 }
               },
               /**
                * Adds a variable declaration to an existing variable statement.
                * @param {!Array} aVariableDeclaration A variable declaration
                *     with an initialiser.
                */
               cAddVariableDeclaration = function(aVariableDeclaration) {
                 (/** @type {!Array} */ oSourceElements[nFrom][1]).unshift(
                     aVariableDeclaration);
               };

           if (nFrom > nTo) {
             return;
           }
           // If the range is a closure, reuse the closure.
           if (nFrom === nTo &&
               'stat' === oSourceElements[nFrom][0] &&
               'call' === oSourceElements[nFrom][1][0] &&
               'function' === oSourceElements[nFrom][1][1][0]) {
             fExamineSyntacticCodeUnit(oSourceElements[nFrom][1][1]);
             return;
           }
           // Create a list of all derived primitive values within the range.
           for (nPosition = nFrom; nPosition <= nTo; nPosition += 1) {
             aSourceElementsData[nPosition].aPrimitiveValues.forEach(
                 cAugmentList(oSourceElementsData.aPrimitiveValues));
           }
           if (0 === oSourceElementsData.aPrimitiveValues.length) {
             return;
           }
           for (nPosition = nFrom; nPosition <= nTo; nPosition += 1) {
             // Add the number of occurrences to the total count.
             fAddOccurrences(nPosition);
             // Add identifiers of this or any nested scope to the list.
             aSourceElementsData[nPosition].aIdentifiers.forEach(
                 cAugmentList(oSourceElementsData.aIdentifiers));
           }
           // Distribute identifier names among derived primitive values.
           do {  // If there was any progress, find a better distribution.
             oSolutionBest = oSolutionCandidate;
             if (Object.keys(oSolutionCandidate.oPrimitiveValues).length > 0) {
               // Sort primitive values descending by their worthwhileness.
               oSourceElementsData.aPrimitiveValues.sort(cSortPrimitiveValues);
             }
             oSolutionCandidate = new TSolution();
             oSourceElementsData.aPrimitiveValues.forEach(
                 fEvaluatePrimitiveValue);
             oScope.cname = nIndex;
           } while (oSolutionCandidate.nSavings > oSolutionBest.nSavings);
           // Take the necessity of adding a variable statement into account.
           if ('var' !== oSourceElements[nFrom][0]) {
             oSolutionBest.nSavings -= oWeights.N_VARIABLE_STATEMENT_AFFIXATION;
           }
           if (bEnclose) {
             // Take the necessity of forming a closure into account.
             oSolutionBest.nSavings -= oWeights.N_CLOSURE;
           }
           if (oSolutionBest.nSavings > 0) {
             // Create variable declarations suitable for UglifyJS.
             Object.keys(oSolutionBest.oPrimitiveValues).forEach(
                 cAugmentVariableDeclarations);
             // Rewrite expressions that contain worthwhile primitive values.
             for (nPosition = nFrom; nPosition <= nTo; nPosition += 1) {
               oWalker = oProcessor.ast_walker();
               oSourceElements[nPosition] =
                   oWalker.with_walkers(
                       oWalkersTransformers,
                       cContext(oWalker, oSourceElements[nPosition]));
             }
             if ('var' === oSourceElements[nFrom][0]) {  // Reuse the statement.
               (/** @type {!Array.<!Array>} */ aVariableDeclarations.reverse(
                   )).forEach(cAddVariableDeclaration);
             } else {  // Add a variable statement.
               Array.prototype.splice.call(
                   oSourceElements,
                   nFrom,
                   0,
                   ['var', aVariableDeclarations]);
               nTo += 1;
             }
             if (bEnclose) {
               // Add a closure.
               Array.prototype.splice.call(
                   oSourceElements,
                   nFrom,
                   0,
                   ['stat', ['call', ['function', null, [], []], []]]);
               // Copy source elements into the closure.
               for (nPosition = nTo + 1; nPosition > nFrom; nPosition -= 1) {
                 Array.prototype.unshift.call(
                     oSourceElements[nFrom][1][1][3],
                     oSourceElements[nPosition]);
               }
               // Remove source elements outside the closure.
               Array.prototype.splice.call(
                   oSourceElements,
                   nFrom + 1,
                   nTo - nFrom + 1);
             }
           }
           if (bEnclose) {
             // Restore the availability of identifier names.
             oScope.cname = nIndex;
           }
         };

     oSourceElements = (/** @type {!TSyntacticCodeUnit} */
         oSyntacticCodeUnit[bIsGlobal ? 1 : 3]);
     if (0 === oSourceElements.length) {
       return;
     }
     oScope = bIsGlobal ? oSyntacticCodeUnit.scope : oSourceElements.scope;
     // Skip a Directive Prologue.
     while (nAfterDirectivePrologue < oSourceElements.length &&
            'stat' === oSourceElements[nAfterDirectivePrologue][0] &&
            'string' === oSourceElements[nAfterDirectivePrologue][1][0]) {
       nAfterDirectivePrologue += 1;
       aSourceElementsData.push(null);
     }
     if (oSourceElements.length === nAfterDirectivePrologue) {
       return;
     }
     for (nPosition = nAfterDirectivePrologue;
          nPosition < oSourceElements.length;
          nPosition += 1) {
       oSourceElementData = new TSourceElementsData();
       oWalker = oProcessor.ast_walker();
       // Classify a source element.
       // Find its derived primitive values and count their occurrences.
       // Find all identifiers used (including nested scopes).
       oWalker.with_walkers(
           oWalkers.oSurveySourceElement,
           cContext(oWalker, oSourceElements[nPosition]));
       // Establish whether the scope is still wholly examinable.
       bIsWhollyExaminable = bIsWhollyExaminable &&
           ESourceElementCategories.N_WITH !== oSourceElementData.nCategory &&
           ESourceElementCategories.N_EVAL !== oSourceElementData.nCategory;
       aSourceElementsData.push(oSourceElementData);
     }
     if (bIsWhollyExaminable) {  // Examine the whole scope.
       fExamineSourceElements(
           nAfterDirectivePrologue,
           oSourceElements.length - 1,
           false);
     } else {  // Examine unexcluded ranges of source elements.
       for (nPosition = oSourceElements.length - 1;
            nPosition >= nAfterDirectivePrologue;
            nPosition -= 1) {
         oSourceElementData = (/** @type {!TSourceElementsData} */
             aSourceElementsData[nPosition]);
         if (ESourceElementCategories.N_OTHER ===
             oSourceElementData.nCategory) {
           if ('undefined' === typeof nTo) {
             nTo = nPosition;  // Indicate the end of a range.
           }
           // Examine the range if it immediately follows a Directive Prologue.
           if (nPosition === nAfterDirectivePrologue) {
             fExamineSourceElements(nPosition, nTo, true);
           }
         } else {
           if ('undefined' !== typeof nTo) {
             // Examine the range that immediately follows this source element.
             fExamineSourceElements(nPosition + 1, nTo, true);
             nTo = void 0;  // Obliterate the range.
           }
           // Examine nested functions.
           oWalker = oProcessor.ast_walker();
           oWalker.with_walkers(
               oWalkers.oExamineFunctions,
               cContext(oWalker, oSourceElements[nPosition]));
         }
       }
     }
   }(oAbstractSyntaxTree = oProcessor.ast_add_scope(oAbstractSyntaxTree)));
  return oAbstractSyntaxTree;
};
/*jshint sub:false */


if (require.main === module) {
  (function() {
    'use strict';
    /*jshint bitwise:true, curly:true, eqeqeq:true, forin:true, immed:true,
         latedef:true, newcap:true, noarge:true, noempty:true, nonew:true,
         onevar:true, plusplus:true, regexp:true, undef:true, strict:true,
         sub:false, trailing:true */

    var _,
        /**
         * NodeJS module for unit testing.
         * @namespace
         * @type {!TAssert}
         * @see http://nodejs.org/docs/v0.6.10/api/all.html#assert
         */
        oAssert = (/** @type {!TAssert} */ require('assert')),
        /**
         * The parser of ECMA-262 found in UglifyJS.
         * @namespace
         * @type {!TParser}
         */
        oParser = (/** @type {!TParser} */ require('./parse-js')),
        /**
         * The processor of <abbr title="abstract syntax tree">AST</abbr>s
         * found in UglifyJS.
         * @namespace
         * @type {!TProcessor}
         */
        oProcessor = (/** @type {!TProcessor} */ require('./process')),
        /**
         * An instance of an object that allows the traversal of an <abbr
         * title="abstract syntax tree">AST</abbr>.
         * @type {!TWalker}
         */
        oWalker,
        /**
         * A collection of functions for the removal of the scope information
         * during the traversal of an <abbr title="abstract syntax tree"
         * >AST</abbr>.
         * @namespace
         * @type {!Object.<string, function(...[*])>}
         */
        oWalkersPurifiers = {
          /**#nocode+*/  // JsDoc Toolkit 2.4.0 hides some of the keys.
          /**
           * Deletes the scope information from the branch of the abstract
           * syntax tree representing the encountered function declaration.
           * @param {string} sIdentifier The identifier of the function.
           * @param {!Array.<string>} aFormalParameterList Formal parameters.
           * @param {!TSyntacticCodeUnit} oFunctionBody Function code.
           */
          'defun': function(
              sIdentifier,
              aFormalParameterList,
              oFunctionBody) {
            delete oFunctionBody.scope;
          },
          /**
           * Deletes the scope information from the branch of the abstract
           * syntax tree representing the encountered function expression.
           * @param {?string} sIdentifier The optional identifier of the
           *     function.
           * @param {!Array.<string>} aFormalParameterList Formal parameters.
           * @param {!TSyntacticCodeUnit} oFunctionBody Function code.
           */
          'function': function(
              sIdentifier,
              aFormalParameterList,
              oFunctionBody) {
            delete oFunctionBody.scope;
          }
          /**#nocode-*/  // JsDoc Toolkit 2.4.0 hides some of the keys.
        },
        /**
         * Initiates the traversal of a source element.
         * @param {!TWalker} oWalker An instance of an object that allows the
         *     traversal of an abstract syntax tree.
         * @param {!TSyntacticCodeUnit} oSourceElement A source element from
         *     which the traversal should commence.
         * @return {function(): !TSyntacticCodeUnit} A function that is able to
         *     initiate the traversal from a given source element.
         */
        cContext = function(oWalker, oSourceElement) {
          /**
           * @return {!TSyntacticCodeUnit} A function that is able to
           *     initiate the traversal from a given source element.
           */
          var fLambda = function() {
            return oWalker.walk(oSourceElement);
          };

          return fLambda;
        },
        /**
         * A record consisting of configuration for the code generation phase.
         * @type {!Object}
         */
        oCodeGenerationOptions = {
          beautify: true
        },
        /**
         * Tests whether consolidation of an ECMAScript program yields expected
         * results.
         * @param {{
         *       sTitle: string,
         *       sInput: string,
         *       sOutput: string
         *     }} oUnitTest A record consisting of data about a unit test: its
         *     name, an ECMAScript program, and, if consolidation is to take
         *     place, the resulting ECMAScript program.
         */
        cAssert = function(oUnitTest) {
          var _,
              /**
               * An array-like object representing the <abbr title=
               * "abstract syntax tree">AST</abbr> obtained after consolidation.
               * @type {!TSyntacticCodeUnit}
               */
              oSyntacticCodeUnitActual =
                  exports.ast_consolidate(oParser.parse(oUnitTest.sInput)),
              /**
               * An array-like object representing the expected <abbr title=
               * "abstract syntax tree">AST</abbr>.
               * @type {!TSyntacticCodeUnit}
               */
              oSyntacticCodeUnitExpected = oParser.parse(
                  oUnitTest.hasOwnProperty('sOutput') ?
                  oUnitTest.sOutput : oUnitTest.sInput);

          delete oSyntacticCodeUnitActual.scope;
          oWalker = oProcessor.ast_walker();
          oWalker.with_walkers(
              oWalkersPurifiers,
              cContext(oWalker, oSyntacticCodeUnitActual));
          try {
            oAssert.deepEqual(
                oSyntacticCodeUnitActual,
                oSyntacticCodeUnitExpected);
          } catch (oException) {
            console.error(
                '########## A unit test has failed.\n' +
                oUnitTest.sTitle + '\n' +
                '#####  actual code  (' +
                oProcessor.gen_code(oSyntacticCodeUnitActual).length +
                ' bytes)\n' +
                oProcessor.gen_code(
                    oSyntacticCodeUnitActual,
                    oCodeGenerationOptions) + '\n' +
                '##### expected code (' +
                oProcessor.gen_code(oSyntacticCodeUnitExpected).length +
                ' bytes)\n' +
                oProcessor.gen_code(
                    oSyntacticCodeUnitExpected,
                    oCodeGenerationOptions));
          }
        };

    [
      // 7.6.1 Reserved Words.
      {
        sTitle:
            'Omission of keywords while choosing an identifier name.',
        sInput:
            '(function() {' +
            '  var a, b, c, d, e, f, g, h, i, j, k, l, m,' +
            '      n, o, p, q, r, s, t, u, v, w, x, y, z,' +
            '      A, B, C, D, E, F, G, H, I, J, K, L, M,' +
            '      N, O, P, Q, R, S, T, U, V, W, X, Y, Z,' +
            '      $, _,' +
            '      aa, ab, ac, ad, ae, af, ag, ah, ai, aj, ak, al, am,' +
            '      an, ao, ap, aq, ar, as, at, au, av, aw, ax, ay, az,' +
            '      aA, aB, aC, aD, aE, aF, aG, aH, aI, aJ, aK, aL, aM,' +
            '      aN, aO, aP, aQ, aR, aS, aT, aU, aV, aW, aX, aY, aZ,' +
            '      a$, a_,' +
            '      ba, bb, bc, bd, be, bf, bg, bh, bi, bj, bk, bl, bm,' +
            '      bn, bo, bp, bq, br, bs, bt, bu, bv, bw, bx, by, bz,' +
            '      bA, bB, bC, bD, bE, bF, bG, bH, bI, bJ, bK, bL, bM,' +
            '      bN, bO, bP, bQ, bR, bS, bT, bU, bV, bW, bX, bY, bZ,' +
            '      b$, b_,' +
            '      ca, cb, cc, cd, ce, cf, cg, ch, ci, cj, ck, cl, cm,' +
            '      cn, co, cp, cq, cr, cs, ct, cu, cv, cw, cx, cy, cz,' +
            '      cA, cB, cC, cD, cE, cF, cG, cH, cI, cJ, cK, cL, cM,' +
            '      cN, cO, cP, cQ, cR, cS, cT, cU, cV, cW, cX, cY, cZ,' +
            '      c$, c_,' +
            '      da, db, dc, dd, de, df, dg, dh, di, dj, dk, dl, dm,' +
            '      dn, dq, dr, ds, dt, du, dv, dw, dx, dy, dz,' +
            '      dA, dB, dC, dD, dE, dF, dG, dH, dI, dJ, dK, dL, dM,' +
            '      dN, dO, dP, dQ, dR, dS, dT, dU, dV, dW, dX, dY, dZ,' +
            '      d$, d_;' +
            '  void ["abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ",' +
            '        "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ"];' +
            '}());',
        sOutput:
            '(function() {' +
            '  var dp =' +
            '      "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ",' +
            '      a, b, c, d, e, f, g, h, i, j, k, l, m,' +
            '      n, o, p, q, r, s, t, u, v, w, x, y, z,' +
            '      A, B, C, D, E, F, G, H, I, J, K, L, M,' +
            '      N, O, P, Q, R, S, T, U, V, W, X, Y, Z,' +
            '      $, _,' +
            '      aa, ab, ac, ad, ae, af, ag, ah, ai, aj, ak, al, am,' +
            '      an, ao, ap, aq, ar, as, at, au, av, aw, ax, ay, az,' +
            '      aA, aB, aC, aD, aE, aF, aG, aH, aI, aJ, aK, aL, aM,' +
            '      aN, aO, aP, aQ, aR, aS, aT, aU, aV, aW, aX, aY, aZ,' +
            '      a$, a_,' +
            '      ba, bb, bc, bd, be, bf, bg, bh, bi, bj, bk, bl, bm,' +
            '      bn, bo, bp, bq, br, bs, bt, bu, bv, bw, bx, by, bz,' +
            '      bA, bB, bC, bD, bE, bF, bG, bH, bI, bJ, bK, bL, bM,' +
            '      bN, bO, bP, bQ, bR, bS, bT, bU, bV, bW, bX, bY, bZ,' +
            '      b$, b_,' +
            '      ca, cb, cc, cd, ce, cf, cg, ch, ci, cj, ck, cl, cm,' +
            '      cn, co, cp, cq, cr, cs, ct, cu, cv, cw, cx, cy, cz,' +
            '      cA, cB, cC, cD, cE, cF, cG, cH, cI, cJ, cK, cL, cM,' +
            '      cN, cO, cP, cQ, cR, cS, cT, cU, cV, cW, cX, cY, cZ,' +
            '      c$, c_,' +
            '      da, db, dc, dd, de, df, dg, dh, di, dj, dk, dl, dm,' +
            '      dn, dq, dr, ds, dt, du, dv, dw, dx, dy, dz,' +
            '      dA, dB, dC, dD, dE, dF, dG, dH, dI, dJ, dK, dL, dM,' +
            '      dN, dO, dP, dQ, dR, dS, dT, dU, dV, dW, dX, dY, dZ,' +
            '      d$, d_;' +
            '  void [dp, dp];' +
            '}());'
      },
      // 7.8.1 Null Literals.
      {
        sTitle:
            'Evaluation with regard to the null value.',
        sInput:
            '/*jshint evil:true */' +
            '(function() {' +
            '  var foo;' +
            '  void [null, null, null];' +
            '}());' +
            'eval("");' +
            '(function() {' +
            '  var foo;' +
            '  void [null, null];' +
            '}());',
        sOutput:
            '/*jshint evil:true */' +
            '(function() {' +
            '  var a = null, foo;' +
            '  void [a, a, a];' +
            '}());' +
            'eval("");' +
            '(function() {' +
            '  var foo;' +
            '  void [null, null];' +
            '}());'
      },
      // 7.8.2 Boolean Literals.
      {
        sTitle:
            'Evaluation with regard to the false value.',
        sInput:
            '/*jshint evil:true */' +
            '(function() {' +
            '  var foo;' +
            '  void [false, false, false];' +
            '}());' +
            'eval("");' +
            '(function() {' +
            '  var foo;' +
            '  void [false, false];' +
            '}());',
        sOutput:
            '/*jshint evil:true */' +
            '(function() {' +
            '  var a = false, foo;' +
            '  void [a, a, a];' +
            '}());' +
            'eval("");' +
            '(function() {' +
            '  var foo;' +
            '  void [false, false];' +
            '}());'
      },
      {
        sTitle:
            'Evaluation with regard to the true value.',
        sInput:
            '/*jshint evil:true */' +
            '(function() {' +
            '  var foo;' +
            '  void [true, true, true];' +
            '}());' +
            'eval("");' +
            '(function() {' +
            '  var foo;' +
            '  void [true, true];' +
            '}());',
        sOutput:
            '/*jshint evil:true */' +
            '(function() {' +
            '  var a = true, foo;' +
            '  void [a, a, a];' +
            '}());' +
            'eval("");' +
            '(function() {' +
            '  var foo;' +
            '  void [true, true];' +
            '}());'
      },
      // 7.8.4 String Literals.
      {
        sTitle:
            'Evaluation with regard to the String value of a string literal.',
        sInput:
            '(function() {' +
            '  var foo;' +
            '  void ["abcd", "abcd", "abc", "abc"];' +
            '}());',
        sOutput:
            '(function() {' +
            '  var a = "abcd", foo;' +
            '  void [a, a, "abc", "abc"];' +
            '}());'
      },
      // 7.8.5 Regular Expression Literals.
      {
        sTitle:
            'Preservation of the pattern of a regular expression literal.',
        sInput:
            'void [/abcdefghijklmnopqrstuvwxyz/, /abcdefghijklmnopqrstuvwxyz/];'
      },
      {
        sTitle:
            'Preservation of the flags of a regular expression literal.',
        sInput:
            'void [/(?:)/gim, /(?:)/gim, /(?:)/gim, /(?:)/gim, /(?:)/gim,' +
            '      /(?:)/gim, /(?:)/gim, /(?:)/gim, /(?:)/gim, /(?:)/gim,' +
            '      /(?:)/gim, /(?:)/gim, /(?:)/gim, /(?:)/gim, /(?:)/gim];'
      },
      // 10.2 Lexical Environments.
      {
        sTitle:
            'Preservation of identifier names in the same scope.',
        sInput:
            '/*jshint shadow:true */' +
            'var a;' +
            'function b(i) {' +
            '}' +
            'for (var c; 0 === Math.random(););' +
            'for (var d in {});' +
            'void ["abcdefghijklmnopqrstuvwxyz"];' +
            'void [b(a), b(c), b(d)];' +
            'void [typeof e];' +
            'i: for (; 0 === Math.random();) {' +
            '  if (42 === (new Date()).getMinutes()) {' +
            '    continue i;' +
            '  } else {' +
            '    break i;' +
            '  }' +
            '}' +
            'try {' +
            '} catch (f) {' +
            '} finally {' +
            '}' +
            '(function g(h) {' +
            '}());' +
            'void [{' +
            '  i: 42,' +
            '  "j": 42,' +
            '  \'k\': 42' +
            '}];' +
            'void ["abcdefghijklmnopqrstuvwxyz"];',
        sOutput:
            '/*jshint shadow:true */' +
            'var a;' +
            'function b(i) {' +
            '}' +
            'for (var c; 0 === Math.random(););' +
            'for (var d in {});' +
            '(function() {' +
            '  var i = "abcdefghijklmnopqrstuvwxyz";' +
            '  void [i];' +
            '  void [b(a), b(c), b(d)];' +
            '  void [typeof e];' +
            '  i: for (; 0 === Math.random();) {' +
            '    if (42 === (new Date()).getMinutes()) {' +
            '      continue i;' +
            '    } else {' +
            '      break i;' +
            '    }' +
            '  }' +
            '  try {' +
            '  } catch (f) {' +
            '  } finally {' +
            '  }' +
            '  (function g(h) {' +
            '  }());' +
            '  void [{' +
            '    i: 42,' +
            '    "j": 42,' +
            '    \'k\': 42' +
            '  }];' +
            '  void [i];' +
            '}());'
      },
      {
        sTitle:
            'Preservation of identifier names in nested function code.',
        sInput:
            '(function() {' +
            '  void ["abcdefghijklmnopqrstuvwxyz"];' +
            '  (function() {' +
            '    var a;' +
            '    for (var b; 0 === Math.random(););' +
            '    for (var c in {});' +
            '    void [typeof d];' +
            '    h: for (; 0 === Math.random();) {' +
            '      if (42 === (new Date()).getMinutes()) {' +
            '        continue h;' +
            '      } else {' +
            '        break h;' +
            '      }' +
            '    }' +
            '    try {' +
            '    } catch (e) {' +
            '    } finally {' +
            '    }' +
            '    (function f(g) {' +
            '    }());' +
            '    void [{' +
            '      h: 42,' +
            '      "i": 42,' +
            '      \'j\': 42' +
            '    }];' +
            '  }());' +
            '  void ["abcdefghijklmnopqrstuvwxyz"];' +
            '}());',
        sOutput:
            '(function() {' +
            '  var h = "abcdefghijklmnopqrstuvwxyz";' +
            '  void [h];' +
            '  (function() {' +
            '    var a;' +
            '    for (var b; 0 === Math.random(););' +
            '    for (var c in {});' +
            '    void [typeof d];' +
            '    h: for (; 0 === Math.random();) {' +
            '      if (42 === (new Date()).getMinutes()) {' +
            '        continue h;' +
            '      } else {' +
            '        break h;' +
            '      }' +
            '    }' +
            '    try {' +
            '    } catch (e) {' +
            '    } finally {' +
            '    }' +
            '    (function f(g) {' +
            '    }());' +
            '    void [{' +
            '      h: 42,' +
            '      "i": 42,' +
            '      \'j\': 42' +
            '    }];' +
            '  }());' +
            '  void [h];' +
            '}());'
      },
      {
        sTitle:
            'Consolidation of a closure with other source elements.',
        sInput:
            '(function(foo) {' +
            '}("abcdefghijklmnopqrstuvwxyz"));' +
            'void ["abcdefghijklmnopqrstuvwxyz"];',
        sOutput:
            '(function() {' +
            '  var a = "abcdefghijklmnopqrstuvwxyz";' +
            '  (function(foo) {' +
            '  })(a);' +
            '  void [a];' +
            '}());'
      },
      {
        sTitle:
            'Consolidation of function code instead of a sole closure.',
        sInput:
            '(function(foo, bar) {' +
            '  void ["abcdefghijklmnopqrstuvwxyz",' +
            '        "abcdefghijklmnopqrstuvwxyz"];' +
            '}("abcdefghijklmnopqrstuvwxyz", "abcdefghijklmnopqrstuvwxyz"));',
        sOutput:
            '(function(foo, bar) {' +
            '  var a = "abcdefghijklmnopqrstuvwxyz";' +
            '  void [a, a];' +
            '}("abcdefghijklmnopqrstuvwxyz", "abcdefghijklmnopqrstuvwxyz"));'
      },
      // 11.1.5 Object Initialiser.
      {
        sTitle:
            'Preservation of property names of an object initialiser.',
        sInput:
            'var foo = {' +
            '  abcdefghijklmnopqrstuvwxyz: 42,' +
            '  "zyxwvutsrqponmlkjihgfedcba": 42,' +
            '  \'mlkjihgfedcbanopqrstuvwxyz\': 42' +
            '};' +
            'void [' +
            '  foo.abcdefghijklmnopqrstuvwxyz,' +
            '  "zyxwvutsrqponmlkjihgfedcba",' +
            '  \'mlkjihgfedcbanopqrstuvwxyz\'' +
            '];'
      },
      {
        sTitle:
            'Evaluation with regard to String values derived from identifier ' +
            'names used as property accessors.',
        sInput:
            '(function() {' +
            '  var foo;' +
            '  void [' +
            '    Math.abcdefghij,' +
            '    Math.abcdefghij,' +
            '    Math.abcdefghi,' +
            '    Math.abcdefghi' +
            '  ];' +
            '}());',
        sOutput:
            '(function() {' +
            '  var a = "abcdefghij", foo;' +
            '  void [' +
            '    Math[a],' +
            '    Math[a],' +
            '    Math.abcdefghi,' +
            '    Math.abcdefghi' +
            '  ];' +
            '}());'
      },
      // 11.2.1 Property Accessors.
      {
        sTitle:
            'Preservation of identifiers in the nonterminal MemberExpression.',
        sInput:
            'void [' +
            '  Math.E,' +
            '  Math.LN10,' +
            '  Math.LN2,' +
            '  Math.LOG2E,' +
            '  Math.LOG10E,' +
            '  Math.PI,' +
            '  Math.SQRT1_2,' +
            '  Math.SQRT2,' +
            '  Math.abs,' +
            '  Math.acos' +
            '];'
      },
      // 12.2 Variable Statement.
      {
        sTitle:
            'Preservation of the identifier of a variable that is being ' +
            'declared in a variable statement.',
        sInput:
            '(function() {' +
            '  var abcdefghijklmnopqrstuvwxyz;' +
            '  void [abcdefghijklmnopqrstuvwxyz];' +
            '}());'
      },
      {
        sTitle:
            'Exclusion of a variable statement in global code.',
        sInput:
            'void ["abcdefghijklmnopqrstuvwxyz"];' +
            'var foo = "abcdefghijklmnopqrstuvwxyz",' +
            '    bar = "abcdefghijklmnopqrstuvwxyz";' +
            'void ["abcdefghijklmnopqrstuvwxyz"];'
      },
      {
        sTitle:
            'Exclusion of a variable statement in function code that ' +
            'contains a with statement.',
        sInput:
            '(function() {' +
            '  with ({});' +
            '  void ["abcdefghijklmnopqrstuvwxyz"];' +
            '  var foo;' +
            '  void ["abcdefghijklmnopqrstuvwxyz"];' +
            '}());'
      },
      {
        sTitle:
            'Exclusion of a variable statement in function code that ' +
            'contains a direct call to the eval function.',
        sInput:
            '/*jshint evil:true */' +
            'void [' +
            '  function() {' +
            '    eval("");' +
            '    void ["abcdefghijklmnopqrstuvwxyz"];' +
            '    var foo;' +
            '    void ["abcdefghijklmnopqrstuvwxyz"];' +
            '  }' +
            '];'
      },
      {
        sTitle:
            'Consolidation within a variable statement in global code.',
        sInput:
            'var foo = function() {' +
            '  void ["abcdefghijklmnopqrstuvwxyz",' +
            '        "abcdefghijklmnopqrstuvwxyz"];' +
            '};',
        sOutput:
            'var foo = function() {' +
            '  var a = "abcdefghijklmnopqrstuvwxyz";' +
            '  void [a, a];' +
            '};'
      },
      {
        sTitle:
            'Consolidation within a variable statement excluded in function ' +
            'code due to the presence of a with statement.',
        sInput:
            '(function() {' +
            '  with ({});' +
            '  var foo = function() {' +
            '    void ["abcdefghijklmnopqrstuvwxyz",' +
            '          "abcdefghijklmnopqrstuvwxyz"];' +
            '  };' +
            '}());',
        sOutput:
            '(function() {' +
            '  with ({});' +
            '  var foo = function() {' +
            '    var a = "abcdefghijklmnopqrstuvwxyz";' +
            '    void [a, a];' +
            '  };' +
            '}());'
      },
      {
        sTitle:
            'Consolidation within a variable statement excluded in function ' +
            'code due to the presence of a direct call to the eval function.',
        sInput:
            '/*jshint evil:true */' +
            '(function() {' +
            '  eval("");' +
            '  var foo = function() {' +
            '    void ["abcdefghijklmnopqrstuvwxyz",' +
            '          "abcdefghijklmnopqrstuvwxyz"];' +
            '  };' +
            '}());',
        sOutput:
            '/*jshint evil:true */' +
            '(function() {' +
            '  eval("");' +
            '  var foo = function() {' +
            '    var a = "abcdefghijklmnopqrstuvwxyz";' +
            '    void [a, a];' +
            '  };' +
            '}());'
      },
      {
        sTitle:
            'Inclusion of a variable statement in function code that ' +
            'contains no with statement and no direct call to the eval ' +
            'function.',
        sInput:
            '(function() {' +
            '  void ["abcdefghijklmnopqrstuvwxyz"];' +
            '  var foo;' +
            '  void ["abcdefghijklmnopqrstuvwxyz"];' +
            '}());',
        sOutput:
            '(function() {' +
            '  var a = "abcdefghijklmnopqrstuvwxyz";' +
            '  void [a];' +
            '  var foo;' +
            '  void [a];' +
            '}());'
      },
      {
        sTitle:
            'Ignorance with regard to a variable statement in global code.',
        sInput:
            'var foo = "abcdefghijklmnopqrstuvwxyz";' +
            'void ["abcdefghijklmnopqrstuvwxyz",' +
            '      "abcdefghijklmnopqrstuvwxyz"];',
        sOutput:
            'var foo = "abcdefghijklmnopqrstuvwxyz";' +
            '(function() {' +
            '  var a = "abcdefghijklmnopqrstuvwxyz";' +
            '  void [a, a];' +
            '}());'
      },
      // 12.4 Expression Statement.
      {
        sTitle:
            'Preservation of identifiers in an expression statement.',
        sInput:
            'void [typeof abcdefghijklmnopqrstuvwxyz,' +
            '      typeof abcdefghijklmnopqrstuvwxyz];'
      },
      // 12.6.3 The {@code for} Statement.
      {
        sTitle:
            'Preservation of identifiers in the variable declaration list of ' +
            'a for statement.',
        sInput:
            'for (var abcdefghijklmnopqrstuvwxyz; 0 === Math.random(););' +
            'for (var abcdefghijklmnopqrstuvwxyz; 0 === Math.random(););'
      },
      // 12.6.4 The {@code for-in} Statement.
      {
        sTitle:
            'Preservation of identifiers in the variable declaration list of ' +
            'a for-in statement.',
        sInput:
            'for (var abcdefghijklmnopqrstuvwxyz in {});' +
            'for (var abcdefghijklmnopqrstuvwxyz in {});'
      },
      // 12.7 The {@code continue} Statement.
      {
        sTitle:
            'Preservation of the identifier in a continue statement.',
        sInput:
            'abcdefghijklmnopqrstuvwxyz: for (; 0 === Math.random();) {' +
            '  continue abcdefghijklmnopqrstuvwxyz;' +
            '}' +
            'abcdefghijklmnopqrstuvwxyz: for (; 0 === Math.random();) {' +
            '  continue abcdefghijklmnopqrstuvwxyz;' +
            '}'
      },
      // 12.8 The {@code break} Statement.
      {
        sTitle:
            'Preservation of the identifier in a break statement.',
        sInput:
            'abcdefghijklmnopqrstuvwxyz: for (; 0 === Math.random();) {' +
            '  break abcdefghijklmnopqrstuvwxyz;' +
            '}' +
            'abcdefghijklmnopqrstuvwxyz: for (; 0 === Math.random();) {' +
            '  break abcdefghijklmnopqrstuvwxyz;' +
            '}'
      },
      // 12.9 The {@code return} Statement.
      {
        sTitle:
            'Exclusion of a return statement in function code that contains ' +
            'a with statement.',
        sInput:
            '(function() {' +
            '  with ({});' +
            '  void ["abcdefghijklmnopqrstuvwxyz"];' +
            '  if (0 === Math.random()) {' +
            '    return;' +
            '  } else {' +
            '  }' +
            '  void ["abcdefghijklmnopqrstuvwxyz"];' +
            '}());'
      },
      {
        sTitle:
            'Exclusion of a return statement in function code that contains ' +
            'a direct call to the eval function.',
        sInput:
            '/*jshint evil:true */' +
            '(function() {' +
            '  eval("");' +
            '  void ["abcdefghijklmnopqrstuvwxyz"];' +
            '  if (0 === Math.random()) {' +
            '    return;' +
            '  } else {' +
            '  }' +
            '  void ["abcdefghijklmnopqrstuvwxyz"];' +
            '}());'
      },
      {
        sTitle:
            'Consolidation within a return statement excluded in function ' +
            'code due to the presence of a with statement.',
        sInput:
            '(function() {' +
            '  with ({});' +
            '  return function() {' +
            '    void ["abcdefghijklmnopqrstuvwxyz",' +
            '          "abcdefghijklmnopqrstuvwxyz"];' +
            '  };' +
            '}());',
        sOutput:
            '(function() {' +
            '  with ({});' +
            '  return function() {' +
            '    var a = "abcdefghijklmnopqrstuvwxyz";' +
            '    void [a, a];' +
            '  };' +
            '}());'
      },
      {
        sTitle:
            'Consolidation within a return statement excluded in function ' +
            'code due to the presence of a direct call to the eval function.',
        sInput:
            '/*jshint evil:true */' +
            '(function() {' +
            '  eval("");' +
            '  return function() {' +
            '    void ["abcdefghijklmnopqrstuvwxyz",' +
            '          "abcdefghijklmnopqrstuvwxyz"];' +
            '  };' +
            '}());',
        sOutput:
            '/*jshint evil:true */' +
            '(function() {' +
            '  eval("");' +
            '  return function() {' +
            '    var a = "abcdefghijklmnopqrstuvwxyz";' +
            '    void [a, a];' +
            '  };' +
            '}());'
      },
      {
        sTitle:
            'Inclusion of a return statement in function code that contains ' +
            'no with statement and no direct call to the eval function.',
        sInput:
            '(function() {' +
            '  void ["abcdefghijklmnopqrstuvwxyz"];' +
            '  if (0 === Math.random()) {' +
            '    return;' +
            '  } else {' +
            '  }' +
            '  void ["abcdefghijklmnopqrstuvwxyz"];' +
            '}());',
        sOutput:
            '(function() {' +
            '  var a = "abcdefghijklmnopqrstuvwxyz";' +
            '  void [a];' +
            '  if (0 === Math.random()) {' +
            '    return;' +
            '  } else {' +
            '  }' +
            '  void [a];' +
            '}());'
      },
      // 12.10 The {@code with} Statement.
      {
        sTitle:
            'Preservation of the statement in a with statement.',
        sInput:
            'with ({}) {' +
            '  void ["abcdefghijklmnopqrstuvwxyz",' +
            '        "abcdefghijklmnopqrstuvwxyz"];' +
            '}'
      },
      {
        sTitle:
            'Exclusion of a with statement in the same syntactic code unit.',
        sInput:
            'void ["abcdefghijklmnopqrstuvwxyz"];' +
            'with ({' +
            '  foo: "abcdefghijklmnopqrstuvwxyz",' +
            '  bar: "abcdefghijklmnopqrstuvwxyz"' +
            '}) {' +
            '  void ["abcdefghijklmnopqrstuvwxyz",' +
            '        "abcdefghijklmnopqrstuvwxyz"];' +
            '}' +
            'void ["abcdefghijklmnopqrstuvwxyz"];'
      },
      {
        sTitle:
            'Exclusion of a with statement in nested function code.',
        sInput:
            'void ["abcdefghijklmnopqrstuvwxyz"];' +
            '(function() {' +
            '  with ({' +
            '    foo: "abcdefghijklmnopqrstuvwxyz",' +
            '    bar: "abcdefghijklmnopqrstuvwxyz"' +
            '  }) {' +
            '    void ["abcdefghijklmnopqrstuvwxyz",' +
            '          "abcdefghijklmnopqrstuvwxyz"];' +
            '  }' +
            '}());' +
            'void ["abcdefghijklmnopqrstuvwxyz"];'
      },
      // 12.12 Labelled Statements.
      {
        sTitle:
            'Preservation of the label of a labelled statement.',
        sInput:
            'abcdefghijklmnopqrstuvwxyz: for (; 0 === Math.random(););' +
            'abcdefghijklmnopqrstuvwxyz: for (; 0 === Math.random(););'
      },
      // 12.14 The {@code try} Statement.
      {
        sTitle:
            'Preservation of the identifier in the catch clause of a try' +
            'statement.',
        sInput:
            'try {' +
            '} catch (abcdefghijklmnopqrstuvwxyz) {' +
            '} finally {' +
            '}' +
            'try {' +
            '} catch (abcdefghijklmnopqrstuvwxyz) {' +
            '} finally {' +
            '}'
      },
      // 13 Function Definition.
      {
        sTitle:
            'Preservation of the identifier of a function declaration.',
        sInput:
            'function abcdefghijklmnopqrstuvwxyz() {' +
            '}' +
            'void [abcdefghijklmnopqrstuvwxyz];'
      },
      {
        sTitle:
            'Preservation of the identifier of a function expression.',
        sInput:
            'void [' +
            '  function abcdefghijklmnopqrstuvwxyz() {' +
            '  },' +
            '  function abcdefghijklmnopqrstuvwxyz() {' +
            '  }' +
            '];'
      },
      {
        sTitle:
            'Preservation of a formal parameter of a function declaration.',
        sInput:
            'function foo(abcdefghijklmnopqrstuvwxyz) {' +
            '}' +
            'function bar(abcdefghijklmnopqrstuvwxyz) {' +
            '}'
      },
      {
        sTitle:
            'Preservation of a formal parameter in a function expression.',
        sInput:
            'void [' +
            '  function(abcdefghijklmnopqrstuvwxyz) {' +
            '  },' +
            '  function(abcdefghijklmnopqrstuvwxyz) {' +
            '  }' +
            '];'
      },
      {
        sTitle:
            'Exclusion of a function declaration.',
        sInput:
            'void ["abcdefghijklmnopqrstuvwxyz"];' +
            'function foo() {' +
            '}' +
            'void ["abcdefghijklmnopqrstuvwxyz"];'
      },
      {
        sTitle:
            'Consolidation within a function declaration.',
        sInput:
            'function foo() {' +
            '  void ["abcdefghijklmnopqrstuvwxyz",' +
            '        "abcdefghijklmnopqrstuvwxyz"];' +
            '}',
        sOutput:
            'function foo() {' +
            '  var a = "abcdefghijklmnopqrstuvwxyz";' +
            '  void [a, a];' +
            '}'
      },
      // 14 Program.
      {
        sTitle:
            'Preservation of a program without source elements.',
        sInput:
            ''
      },
      // 14.1 Directive Prologues and the Use Strict Directive.
      {
        sTitle:
            'Preservation of a Directive Prologue in global code.',
        sInput:
            '"abcdefghijklmnopqrstuvwxyz";' +
            '\'zyxwvutsrqponmlkjihgfedcba\';'
      },
      {
        sTitle:
            'Preservation of a Directive Prologue in a function declaration.',
        sInput:
            'function foo() {' +
            '  "abcdefghijklmnopqrstuvwxyz";' +
            '  \'zyxwvutsrqponmlkjihgfedcba\';' +
            '}'
      },
      {
        sTitle:
            'Preservation of a Directive Prologue in a function expression.',
        sInput:
            'void [' +
            '  function() {' +
            '    "abcdefghijklmnopqrstuvwxyz";' +
            '    \'zyxwvutsrqponmlkjihgfedcba\';' +
            '  }' +
            '];'
      },
      {
        sTitle:
            'Ignorance with regard to a Directive Prologue in global code.',
        sInput:
            '"abcdefghijklmnopqrstuvwxyz";' +
            'void ["abcdefghijklmnopqrstuvwxyz",' +
            '      "abcdefghijklmnopqrstuvwxyz"];',
        sOutput:
            '"abcdefghijklmnopqrstuvwxyz";' +
            '(function() {' +
            '  var a = "abcdefghijklmnopqrstuvwxyz";' +
            '  void [a, a];' +
            '}());'
      },
      {
        sTitle:
            'Ignorance with regard to a Directive Prologue in a function' +
            'declaration.',
        sInput:
            'function foo() {' +
            '  "abcdefghijklmnopqrstuvwxyz";' +
            '  void ["abcdefghijklmnopqrstuvwxyz",' +
            '        "abcdefghijklmnopqrstuvwxyz"];' +
            '}',
        sOutput:
            'function foo() {' +
            '  "abcdefghijklmnopqrstuvwxyz";' +
            '  var a = "abcdefghijklmnopqrstuvwxyz";' +
            '  void [a, a];' +
            '}'
      },
      {
        sTitle:
            'Ignorance with regard to a Directive Prologue in a function' +
            'expression.',
        sInput:
            '(function() {' +
            '  "abcdefghijklmnopqrstuvwxyz";' +
            '  void ["abcdefghijklmnopqrstuvwxyz",' +
            '        "abcdefghijklmnopqrstuvwxyz"];' +
            '}());',
        sOutput:
            '(function() {' +
            '  "abcdefghijklmnopqrstuvwxyz";' +
            '  var a = "abcdefghijklmnopqrstuvwxyz";' +
            '  void [a, a];' +
            '}());'
      },
      // 15.1 The Global Object.
      {
        sTitle:
            'Preservation of a property of the global object.',
        sInput:
            'void [undefined, undefined, undefined, undefined, undefined];'
      },
      // 15.1.2.1.1 Direct Call to Eval.
      {
        sTitle:
            'Exclusion of a direct call to the eval function in the same ' +
            'syntactic code unit.',
        sInput:
            '/*jshint evil:true */' +
            'void ["abcdefghijklmnopqrstuvwxyz"];' +
            'eval("");' +
            'void ["abcdefghijklmnopqrstuvwxyz"];'
      },
      {
        sTitle:
            'Exclusion of a direct call to the eval function in nested ' +
            'function code.',
        sInput:
            '/*jshint evil:true */' +
            'void ["abcdefghijklmnopqrstuvwxyz"];' +
            '(function() {' +
            '  eval("");' +
            '}());' +
            'void ["abcdefghijklmnopqrstuvwxyz"];'
      },
      {
        sTitle:
            'Consolidation within a direct call to the eval function.',
        sInput:
            '/*jshint evil:true */' +
            'eval(function() {' +
            '  void ["abcdefghijklmnopqrstuvwxyz",' +
            '        "abcdefghijklmnopqrstuvwxyz"];' +
            '}());',
        sOutput:
            '/*jshint evil:true */' +
            'eval(function() {' +
            '  var a = "abcdefghijklmnopqrstuvwxyz";' +
            '  void [a, a];' +
            '}());'
      },
      // Consolidation proper.
      {
        sTitle:
            'No consolidation if it does not result in a reduction of the ' +
            'number of source characters.',
        sInput:
            '(function() {' +
            '  var foo;' +
            '  void ["ab", "ab", "abc", "abc"];' +
            '}());'
      },
      {
        sTitle:
            'Identification of a range of source elements at the beginning ' +
            'of global code.',
        sInput:
            '/*jshint evil:true */' +
            '"abcdefghijklmnopqrstuvwxyz";' +
            'void ["abcdefghijklmnopqrstuvwxyz",' +
            '      "abcdefghijklmnopqrstuvwxyz"];' +
            'eval("");',
        sOutput:
            '/*jshint evil:true */' +
            '"abcdefghijklmnopqrstuvwxyz";' +
            '(function() {' +
            '  var a = "abcdefghijklmnopqrstuvwxyz";' +
            '  void [a, a];' +
            '}());' +
            'eval("");'
      },
      {
        sTitle:
            'Identification of a range of source elements in the middle of ' +
            'global code.',
        sInput:
            '/*jshint evil:true */' +
            '"abcdefghijklmnopqrstuvwxyz";' +
            'eval("");' +
            'void ["abcdefghijklmnopqrstuvwxyz",' +
            '      "abcdefghijklmnopqrstuvwxyz"];' +
            'eval("");',
        sOutput:
            '/*jshint evil:true */' +
            '"abcdefghijklmnopqrstuvwxyz";' +
            'eval("");' +
            '(function() {' +
            '  var a = "abcdefghijklmnopqrstuvwxyz";' +
            '  void [a, a];' +
            '}());' +
            'eval("");'
      },
      {
        sTitle:
            'Identification of a range of source elements at the end of ' +
            'global code.',
        sInput:
            '/*jshint evil:true */' +
            '"abcdefghijklmnopqrstuvwxyz";' +
            'eval("");' +
            'void ["abcdefghijklmnopqrstuvwxyz",' +
            '      "abcdefghijklmnopqrstuvwxyz"];',
        sOutput:
            '/*jshint evil:true */' +
            '"abcdefghijklmnopqrstuvwxyz";' +
            'eval("");' +
            '(function() {' +
            '  var a = "abcdefghijklmnopqrstuvwxyz";' +
            '  void [a, a];' +
            '}());'
      },
      {
        sTitle:
            'Identification of a range of source elements at the beginning ' +
            'of function code.',
        sInput:
            '/*jshint evil:true */' +
            '(function() {' +
            '  "abcdefghijklmnopqrstuvwxyz";' +
            '  void ["abcdefghijklmnopqrstuvwxyz",' +
            '        "abcdefghijklmnopqrstuvwxyz"];' +
            '  eval("");' +
            '}());',
        sOutput:
            '/*jshint evil:true */' +
            '(function() {' +
            '  "abcdefghijklmnopqrstuvwxyz";' +
            '  (function() {' +
            '    var a = "abcdefghijklmnopqrstuvwxyz";' +
            '    void [a, a];' +
            '  }());' +
            '  eval("");' +
            '}());'
      },
      {
        sTitle:
            'Identification of a range of source elements in the middle of ' +
            'function code.',
        sInput:
            '/*jshint evil:true */' +
            '(function() {' +
            '  "abcdefghijklmnopqrstuvwxyz";' +
            '  eval("");' +
            '  void ["abcdefghijklmnopqrstuvwxyz",' +
            '        "abcdefghijklmnopqrstuvwxyz"];' +
            '  eval("");' +
            '}());',
        sOutput:
            '/*jshint evil:true */' +
            '(function() {' +
            '  "abcdefghijklmnopqrstuvwxyz";' +
            '  eval("");' +
            '  (function() {' +
            '    var a = "abcdefghijklmnopqrstuvwxyz";' +
            '    void [a, a];' +
            '  }());' +
            '  eval("");' +
            '}());'
      },
      {
        sTitle:
            'Identification of a range of source elements at the end of ' +
            'function code.',
        sInput:
            '/*jshint evil:true */' +
            '(function() {' +
            '  "abcdefghijklmnopqrstuvwxyz";' +
            '  eval("");' +
            '  void ["abcdefghijklmnopqrstuvwxyz",' +
            '        "abcdefghijklmnopqrstuvwxyz"];' +
            '}());',
        sOutput:
            '/*jshint evil:true */' +
            '(function() {' +
            '  "abcdefghijklmnopqrstuvwxyz";' +
            '  eval("");' +
            '  (function() {' +
            '    var a = "abcdefghijklmnopqrstuvwxyz";' +
            '    void [a, a];' +
            '  }());' +
            '}());'
      },
      {
        sTitle:
            'Evaluation with regard to String values of String literals and ' +
            'String values derived from identifier names used as property' +
            'accessors.',
        sInput:
            '(function() {' +
            '  var foo;' +
            '  void ["abcdefg", Math.abcdefg, "abcdef", Math.abcdef];' +
            '}());',
        sOutput:
            '(function() {' +
            '  var a = "abcdefg", foo;' +
            '  void [a, Math[a], "abcdef", Math.abcdef];' +
            '}());'
      },
      {
        sTitle:
            'Evaluation with regard to the necessity of adding a variable ' +
            'statement.',
        sInput:
            '/*jshint evil:true */' +
            '(function() {' +
            '  void ["abcdefgh", "abcdefgh"];' +
            '}());' +
            'eval("");' +
            '(function() {' +
            '  void ["abcdefg", "abcdefg"];' +
            '}());' +
            'eval("");' +
            '(function() {' +
            '  var foo;' +
            '  void ["abcd", "abcd"];' +
            '}());',
        sOutput:
            '/*jshint evil:true */' +
            '(function() {' +
            '  var a = "abcdefgh";' +
            '  void [a, a];' +
            '}());' +
            'eval("");' +
            '(function() {' +
            '  void ["abcdefg", "abcdefg"];' +
            '}());' +
            'eval("");' +
            '(function() {' +
            '  var a = "abcd", foo;' +
            '  void [a, a];' +
            '}());'
      },
      {
        sTitle:
            'Evaluation with regard to the necessity of enclosing source ' +
            'elements.',
        sInput:
            '/*jshint evil:true */' +
            'void ["abcdefghijklmnopqrstuvwxy", "abcdefghijklmnopqrstuvwxy"];' +
            'eval("");' +
            'void ["abcdefghijklmnopqrstuvwx", "abcdefghijklmnopqrstuvwx"];' +
            'eval("");' +
            '(function() {' +
            '  void ["abcdefgh", "abcdefgh"];' +
            '}());' +
            '(function() {' +
            '  void ["abcdefghijklmnopqrstuvwxy",' +
            '        "abcdefghijklmnopqrstuvwxy"];' +
            '  eval("");' +
            '  void ["abcdefghijklmnopqrstuvwx",' +
            '        "abcdefghijklmnopqrstuvwx"];' +
            '  eval("");' +
            '  (function() {' +
            '    void ["abcdefgh", "abcdefgh"];' +
            '  }());' +
            '}());',
        sOutput:
            '/*jshint evil:true */' +
            '(function() {' +
            '  var a = "abcdefghijklmnopqrstuvwxy";' +
            '  void [a, a];' +
            '}());' +
            'eval("");' +
            'void ["abcdefghijklmnopqrstuvwx", "abcdefghijklmnopqrstuvwx"];' +
            'eval("");' +
            '(function() {' +
            '  var a = "abcdefgh";' +
            '  void [a, a];' +
            '}());' +
            '(function() {' +
            '  (function() {' +
            '    var a = "abcdefghijklmnopqrstuvwxy";' +
            '    void [a, a];' +
            '  }());' +
            '  eval("");' +
            '  void ["abcdefghijklmnopqrstuvwx", "abcdefghijklmnopqrstuvwx"];' +
            '  eval("");' +
            '  (function() {' +
            '    var a = "abcdefgh";' +
            '    void [a, a];' +
            '  }());' +
            '}());'
      },
      {
        sTitle:
            'Employment of a closure while consolidating in global code.',
        sInput:
            'void ["abcdefghijklmnopqrstuvwxyz",' +
            '      "abcdefghijklmnopqrstuvwxyz"];',
        sOutput:
            '(function() {' +
            '  var a = "abcdefghijklmnopqrstuvwxyz";' +
            '  void [a, a];' +
            '}());'
      },
      {
        sTitle:
            'Assignment of a shorter identifier to a value whose ' +
            'consolidation results in a greater reduction of the number of ' +
            'source characters.',
        sInput:
            '(function() {' +
            '  var b, c, d, e, f, g, h, i, j, k, l, m,' +
            '      n, o, p, q, r, s, t, u, v, w, x, y, z,' +
            '      A, B, C, D, E, F, G, H, I, J, K, L, M,' +
            '      N, O, P, Q, R, S, T, U, V, W, X, Y, Z,' +
            '      $, _;' +
            '  void ["abcde", "abcde", "edcba", "edcba", "edcba"];' +
            '}());',
        sOutput:
            '(function() {' +
            '  var a = "edcba",' +
            '      b, c, d, e, f, g, h, i, j, k, l, m,' +
            '      n, o, p, q, r, s, t, u, v, w, x, y, z,' +
            '      A, B, C, D, E, F, G, H, I, J, K, L, M,' +
            '      N, O, P, Q, R, S, T, U, V, W, X, Y, Z,' +
            '      $, _;' +
            '  void ["abcde", "abcde", a, a, a];' +
            '}());'
      }
    ].forEach(cAssert);
  }());
}

/* Local Variables:      */
/* mode: js              */
/* coding: utf-8         */
/* indent-tabs-mode: nil */
/* tab-width: 2          */
/* End:                  */
/* vim: set ft=javascript fenc=utf-8 et ts=2 sts=2 sw=2: */
/* :mode=javascript:noTabs=true:tabSize=2:indentSize=2:deepIndent=true: */


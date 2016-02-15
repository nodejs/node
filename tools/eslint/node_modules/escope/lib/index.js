'use strict';

var _typeof = typeof Symbol === "function" && typeof Symbol.iterator === "symbol" ? function (obj) { return typeof obj; } : function (obj) { return obj && typeof Symbol === "function" && obj.constructor === Symbol ? "symbol" : typeof obj; }; /*
                                                                                                                                                                                                                                                    Copyright (C) 2012-2014 Yusuke Suzuki <utatane.tea@gmail.com>
                                                                                                                                                                                                                                                    Copyright (C) 2013 Alex Seville <hi@alexanderseville.com>
                                                                                                                                                                                                                                                    Copyright (C) 2014 Thiago de Arruda <tpadilha84@gmail.com>

                                                                                                                                                                                                                                                    Redistribution and use in source and binary forms, with or without
                                                                                                                                                                                                                                                    modification, are permitted provided that the following conditions are met:

                                                                                                                                                                                                                                                      * Redistributions of source code must retain the above copyright
                                                                                                                                                                                                                                                        notice, this list of conditions and the following disclaimer.
                                                                                                                                                                                                                                                      * Redistributions in binary form must reproduce the above copyright
                                                                                                                                                                                                                                                        notice, this list of conditions and the following disclaimer in the
                                                                                                                                                                                                                                                        documentation and/or other materials provided with the distribution.

                                                                                                                                                                                                                                                    THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
                                                                                                                                                                                                                                                    AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
                                                                                                                                                                                                                                                    IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
                                                                                                                                                                                                                                                    ARE DISCLAIMED. IN NO EVENT SHALL <COPYRIGHT HOLDER> BE LIABLE FOR ANY
                                                                                                                                                                                                                                                    DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
                                                                                                                                                                                                                                                    (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
                                                                                                                                                                                                                                                    LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
                                                                                                                                                                                                                                                    ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
                                                                                                                                                                                                                                                    (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
                                                                                                                                                                                                                                                    THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
                                                                                                                                                                                                                                                  */

/**
 * Escope (<a href="http://github.com/estools/escope">escope</a>) is an <a
 * href="http://www.ecma-international.org/publications/standards/Ecma-262.htm">ECMAScript</a>
 * scope analyzer extracted from the <a
 * href="http://github.com/estools/esmangle">esmangle project</a/>.
 * <p>
 * <em>escope</em> finds lexical scopes in a source program, i.e. areas of that
 * program where different occurrences of the same identifier refer to the same
 * variable. With each scope the contained variables are collected, and each
 * identifier reference in code is linked to its corresponding variable (if
 * possible).
 * <p>
 * <em>escope</em> works on a syntax tree of the parsed source code which has
 * to adhere to the <a
 * href="https://developer.mozilla.org/en-US/docs/SpiderMonkey/Parser_API">
 * Mozilla Parser API</a>. E.g. <a href="http://esprima.org">esprima</a> is a parser
 * that produces such syntax trees.
 * <p>
 * The main interface is the {@link analyze} function.
 * @module escope
 */

/*jslint bitwise:true */

Object.defineProperty(exports, "__esModule", {
    value: true
});
exports.ScopeManager = exports.Scope = exports.Variable = exports.Reference = exports.version = undefined;
exports.analyze = analyze;

var _assert = require('assert');

var _assert2 = _interopRequireDefault(_assert);

var _scopeManager = require('./scope-manager');

var _scopeManager2 = _interopRequireDefault(_scopeManager);

var _referencer = require('./referencer');

var _referencer2 = _interopRequireDefault(_referencer);

var _reference = require('./reference');

var _reference2 = _interopRequireDefault(_reference);

var _variable = require('./variable');

var _variable2 = _interopRequireDefault(_variable);

var _scope = require('./scope');

var _scope2 = _interopRequireDefault(_scope);

var _package = require('../package.json');

function _interopRequireDefault(obj) { return obj && obj.__esModule ? obj : { default: obj }; }

function defaultOptions() {
    return {
        optimistic: false,
        directive: false,
        nodejsScope: false,
        impliedStrict: false,
        sourceType: 'script', // one of ['script', 'module']
        ecmaVersion: 5
    };
}

function updateDeeply(target, override) {
    var key, val;

    function isHashObject(target) {
        return (typeof target === 'undefined' ? 'undefined' : _typeof(target)) === 'object' && target instanceof Object && !(target instanceof RegExp);
    }

    for (key in override) {
        if (override.hasOwnProperty(key)) {
            val = override[key];
            if (isHashObject(val)) {
                if (isHashObject(target[key])) {
                    updateDeeply(target[key], val);
                } else {
                    target[key] = updateDeeply({}, val);
                }
            } else {
                target[key] = val;
            }
        }
    }
    return target;
}

/**
 * Main interface function. Takes an Esprima syntax tree and returns the
 * analyzed scopes.
 * @function analyze
 * @param {esprima.Tree} tree
 * @param {Object} providedOptions - Options that tailor the scope analysis
 * @param {boolean} [providedOptions.optimistic=false] - the optimistic flag
 * @param {boolean} [providedOptions.directive=false]- the directive flag
 * @param {boolean} [providedOptions.ignoreEval=false]- whether to check 'eval()' calls
 * @param {boolean} [providedOptions.nodejsScope=false]- whether the whole
 * script is executed under node.js environment. When enabled, escope adds
 * a function scope immediately following the global scope.
 * @param {boolean} [providedOptions.impliedStrict=false]- implied strict mode
 * (if ecmaVersion >= 5).
 * @param {string} [providedOptions.sourceType='script']- the source type of the script. one of 'script' and 'module'
 * @param {number} [providedOptions.ecmaVersion=5]- which ECMAScript version is considered
 * @return {ScopeManager}
 */
function analyze(tree, providedOptions) {
    var scopeManager, referencer, options;

    options = updateDeeply(defaultOptions(), providedOptions);

    scopeManager = new _scopeManager2.default(options);

    referencer = new _referencer2.default(scopeManager);
    referencer.visit(tree);

    (0, _assert2.default)(scopeManager.__currentScope === null, 'currentScope should be null.');

    return scopeManager;
}

exports.
/** @name module:escope.version */
version = _package.version;
exports.
/** @name module:escope.Reference */
Reference = _reference2.default;
exports.
/** @name module:escope.Variable */
Variable = _variable2.default;
exports.
/** @name module:escope.Scope */
Scope = _scope2.default;
exports.
/** @name module:escope.ScopeManager */
ScopeManager = _scopeManager2.default;

/* vim: set sw=4 ts=4 et tw=80 : */
//# sourceMappingURL=data:application/json;base64,eyJ2ZXJzaW9uIjozLCJzb3VyY2VzIjpbImluZGV4LmpzIl0sIm5hbWVzIjpbXSwibWFwcGluZ3MiOiI7Ozs7Ozs7Ozs7Ozs7Ozs7Ozs7Ozs7Ozs7Ozs7Ozs7Ozs7Ozs7Ozs7Ozs7Ozs7Ozs7Ozs7Ozs7O1FBZ0hnQjs7Ozs7Ozs7Ozs7Ozs7Ozs7Ozs7Ozs7Ozs7Ozs7O0FBckRoQixTQUFTLGNBQVQsR0FBMEI7QUFDdEIsV0FBTztBQUNILG9CQUFZLEtBQVo7QUFDQSxtQkFBVyxLQUFYO0FBQ0EscUJBQWEsS0FBYjtBQUNBLHVCQUFlLEtBQWY7QUFDQSxvQkFBWSxRQUFaO0FBQ0EscUJBQWEsQ0FBYjtLQU5KLENBRHNCO0NBQTFCOztBQVdBLFNBQVMsWUFBVCxDQUFzQixNQUF0QixFQUE4QixRQUE5QixFQUF3QztBQUNwQyxRQUFJLEdBQUosRUFBUyxHQUFULENBRG9DOztBQUdwQyxhQUFTLFlBQVQsQ0FBc0IsTUFBdEIsRUFBOEI7QUFDMUIsZUFBTyxRQUFPLHVEQUFQLEtBQWtCLFFBQWxCLElBQThCLGtCQUFrQixNQUFsQixJQUE0QixFQUFFLGtCQUFrQixNQUFsQixDQUFGLENBRHZDO0tBQTlCOztBQUlBLFNBQUssR0FBTCxJQUFZLFFBQVosRUFBc0I7QUFDbEIsWUFBSSxTQUFTLGNBQVQsQ0FBd0IsR0FBeEIsQ0FBSixFQUFrQztBQUM5QixrQkFBTSxTQUFTLEdBQVQsQ0FBTixDQUQ4QjtBQUU5QixnQkFBSSxhQUFhLEdBQWIsQ0FBSixFQUF1QjtBQUNuQixvQkFBSSxhQUFhLE9BQU8sR0FBUCxDQUFiLENBQUosRUFBK0I7QUFDM0IsaUNBQWEsT0FBTyxHQUFQLENBQWIsRUFBMEIsR0FBMUIsRUFEMkI7aUJBQS9CLE1BRU87QUFDSCwyQkFBTyxHQUFQLElBQWMsYUFBYSxFQUFiLEVBQWlCLEdBQWpCLENBQWQsQ0FERztpQkFGUDthQURKLE1BTU87QUFDSCx1QkFBTyxHQUFQLElBQWMsR0FBZCxDQURHO2FBTlA7U0FGSjtLQURKO0FBY0EsV0FBTyxNQUFQLENBckJvQztDQUF4Qzs7Ozs7Ozs7Ozs7Ozs7Ozs7Ozs7QUEwQ08sU0FBUyxPQUFULENBQWlCLElBQWpCLEVBQXVCLGVBQXZCLEVBQXdDO0FBQzNDLFFBQUksWUFBSixFQUFrQixVQUFsQixFQUE4QixPQUE5QixDQUQyQzs7QUFHM0MsY0FBVSxhQUFhLGdCQUFiLEVBQStCLGVBQS9CLENBQVYsQ0FIMkM7O0FBSzNDLG1CQUFlLDJCQUFpQixPQUFqQixDQUFmLENBTDJDOztBQU8zQyxpQkFBYSx5QkFBZSxZQUFmLENBQWIsQ0FQMkM7QUFRM0MsZUFBVyxLQUFYLENBQWlCLElBQWpCLEVBUjJDOztBQVUzQywwQkFBTyxhQUFhLGNBQWIsS0FBZ0MsSUFBaEMsRUFBc0MsOEJBQTdDLEVBVjJDOztBQVkzQyxXQUFPLFlBQVAsQ0FaMkM7Q0FBeEM7Ozs7QUFpQkg7OztBQUVBOzs7QUFFQTs7O0FBRUE7OztBQUVBIiwiZmlsZSI6ImluZGV4LmpzIiwic291cmNlc0NvbnRlbnQiOlsiLypcbiAgQ29weXJpZ2h0IChDKSAyMDEyLTIwMTQgWXVzdWtlIFN1enVraSA8dXRhdGFuZS50ZWFAZ21haWwuY29tPlxuICBDb3B5cmlnaHQgKEMpIDIwMTMgQWxleCBTZXZpbGxlIDxoaUBhbGV4YW5kZXJzZXZpbGxlLmNvbT5cbiAgQ29weXJpZ2h0IChDKSAyMDE0IFRoaWFnbyBkZSBBcnJ1ZGEgPHRwYWRpbGhhODRAZ21haWwuY29tPlxuXG4gIFJlZGlzdHJpYnV0aW9uIGFuZCB1c2UgaW4gc291cmNlIGFuZCBiaW5hcnkgZm9ybXMsIHdpdGggb3Igd2l0aG91dFxuICBtb2RpZmljYXRpb24sIGFyZSBwZXJtaXR0ZWQgcHJvdmlkZWQgdGhhdCB0aGUgZm9sbG93aW5nIGNvbmRpdGlvbnMgYXJlIG1ldDpcblxuICAgICogUmVkaXN0cmlidXRpb25zIG9mIHNvdXJjZSBjb2RlIG11c3QgcmV0YWluIHRoZSBhYm92ZSBjb3B5cmlnaHRcbiAgICAgIG5vdGljZSwgdGhpcyBsaXN0IG9mIGNvbmRpdGlvbnMgYW5kIHRoZSBmb2xsb3dpbmcgZGlzY2xhaW1lci5cbiAgICAqIFJlZGlzdHJpYnV0aW9ucyBpbiBiaW5hcnkgZm9ybSBtdXN0IHJlcHJvZHVjZSB0aGUgYWJvdmUgY29weXJpZ2h0XG4gICAgICBub3RpY2UsIHRoaXMgbGlzdCBvZiBjb25kaXRpb25zIGFuZCB0aGUgZm9sbG93aW5nIGRpc2NsYWltZXIgaW4gdGhlXG4gICAgICBkb2N1bWVudGF0aW9uIGFuZC9vciBvdGhlciBtYXRlcmlhbHMgcHJvdmlkZWQgd2l0aCB0aGUgZGlzdHJpYnV0aW9uLlxuXG4gIFRISVMgU09GVFdBUkUgSVMgUFJPVklERUQgQlkgVEhFIENPUFlSSUdIVCBIT0xERVJTIEFORCBDT05UUklCVVRPUlMgXCJBUyBJU1wiXG4gIEFORCBBTlkgRVhQUkVTUyBPUiBJTVBMSUVEIFdBUlJBTlRJRVMsIElOQ0xVRElORywgQlVUIE5PVCBMSU1JVEVEIFRPLCBUSEVcbiAgSU1QTElFRCBXQVJSQU5USUVTIE9GIE1FUkNIQU5UQUJJTElUWSBBTkQgRklUTkVTUyBGT1IgQSBQQVJUSUNVTEFSIFBVUlBPU0VcbiAgQVJFIERJU0NMQUlNRUQuIElOIE5PIEVWRU5UIFNIQUxMIDxDT1BZUklHSFQgSE9MREVSPiBCRSBMSUFCTEUgRk9SIEFOWVxuICBESVJFQ1QsIElORElSRUNULCBJTkNJREVOVEFMLCBTUEVDSUFMLCBFWEVNUExBUlksIE9SIENPTlNFUVVFTlRJQUwgREFNQUdFU1xuICAoSU5DTFVESU5HLCBCVVQgTk9UIExJTUlURUQgVE8sIFBST0NVUkVNRU5UIE9GIFNVQlNUSVRVVEUgR09PRFMgT1IgU0VSVklDRVM7XG4gIExPU1MgT0YgVVNFLCBEQVRBLCBPUiBQUk9GSVRTOyBPUiBCVVNJTkVTUyBJTlRFUlJVUFRJT04pIEhPV0VWRVIgQ0FVU0VEIEFORFxuICBPTiBBTlkgVEhFT1JZIE9GIExJQUJJTElUWSwgV0hFVEhFUiBJTiBDT05UUkFDVCwgU1RSSUNUIExJQUJJTElUWSwgT1IgVE9SVFxuICAoSU5DTFVESU5HIE5FR0xJR0VOQ0UgT1IgT1RIRVJXSVNFKSBBUklTSU5HIElOIEFOWSBXQVkgT1VUIE9GIFRIRSBVU0UgT0ZcbiAgVEhJUyBTT0ZUV0FSRSwgRVZFTiBJRiBBRFZJU0VEIE9GIFRIRSBQT1NTSUJJTElUWSBPRiBTVUNIIERBTUFHRS5cbiovXG5cbi8qKlxuICogRXNjb3BlICg8YSBocmVmPVwiaHR0cDovL2dpdGh1Yi5jb20vZXN0b29scy9lc2NvcGVcIj5lc2NvcGU8L2E+KSBpcyBhbiA8YVxuICogaHJlZj1cImh0dHA6Ly93d3cuZWNtYS1pbnRlcm5hdGlvbmFsLm9yZy9wdWJsaWNhdGlvbnMvc3RhbmRhcmRzL0VjbWEtMjYyLmh0bVwiPkVDTUFTY3JpcHQ8L2E+XG4gKiBzY29wZSBhbmFseXplciBleHRyYWN0ZWQgZnJvbSB0aGUgPGFcbiAqIGhyZWY9XCJodHRwOi8vZ2l0aHViLmNvbS9lc3Rvb2xzL2VzbWFuZ2xlXCI+ZXNtYW5nbGUgcHJvamVjdDwvYS8+LlxuICogPHA+XG4gKiA8ZW0+ZXNjb3BlPC9lbT4gZmluZHMgbGV4aWNhbCBzY29wZXMgaW4gYSBzb3VyY2UgcHJvZ3JhbSwgaS5lLiBhcmVhcyBvZiB0aGF0XG4gKiBwcm9ncmFtIHdoZXJlIGRpZmZlcmVudCBvY2N1cnJlbmNlcyBvZiB0aGUgc2FtZSBpZGVudGlmaWVyIHJlZmVyIHRvIHRoZSBzYW1lXG4gKiB2YXJpYWJsZS4gV2l0aCBlYWNoIHNjb3BlIHRoZSBjb250YWluZWQgdmFyaWFibGVzIGFyZSBjb2xsZWN0ZWQsIGFuZCBlYWNoXG4gKiBpZGVudGlmaWVyIHJlZmVyZW5jZSBpbiBjb2RlIGlzIGxpbmtlZCB0byBpdHMgY29ycmVzcG9uZGluZyB2YXJpYWJsZSAoaWZcbiAqIHBvc3NpYmxlKS5cbiAqIDxwPlxuICogPGVtPmVzY29wZTwvZW0+IHdvcmtzIG9uIGEgc3ludGF4IHRyZWUgb2YgdGhlIHBhcnNlZCBzb3VyY2UgY29kZSB3aGljaCBoYXNcbiAqIHRvIGFkaGVyZSB0byB0aGUgPGFcbiAqIGhyZWY9XCJodHRwczovL2RldmVsb3Blci5tb3ppbGxhLm9yZy9lbi1VUy9kb2NzL1NwaWRlck1vbmtleS9QYXJzZXJfQVBJXCI+XG4gKiBNb3ppbGxhIFBhcnNlciBBUEk8L2E+LiBFLmcuIDxhIGhyZWY9XCJodHRwOi8vZXNwcmltYS5vcmdcIj5lc3ByaW1hPC9hPiBpcyBhIHBhcnNlclxuICogdGhhdCBwcm9kdWNlcyBzdWNoIHN5bnRheCB0cmVlcy5cbiAqIDxwPlxuICogVGhlIG1haW4gaW50ZXJmYWNlIGlzIHRoZSB7QGxpbmsgYW5hbHl6ZX0gZnVuY3Rpb24uXG4gKiBAbW9kdWxlIGVzY29wZVxuICovXG5cbi8qanNsaW50IGJpdHdpc2U6dHJ1ZSAqL1xuXG5pbXBvcnQgYXNzZXJ0IGZyb20gJ2Fzc2VydCc7XG5cbmltcG9ydCBTY29wZU1hbmFnZXIgZnJvbSAnLi9zY29wZS1tYW5hZ2VyJztcbmltcG9ydCBSZWZlcmVuY2VyIGZyb20gJy4vcmVmZXJlbmNlcic7XG5pbXBvcnQgUmVmZXJlbmNlIGZyb20gJy4vcmVmZXJlbmNlJztcbmltcG9ydCBWYXJpYWJsZSBmcm9tICcuL3ZhcmlhYmxlJztcbmltcG9ydCBTY29wZSBmcm9tICcuL3Njb3BlJztcbmltcG9ydCB7IHZlcnNpb24gfSBmcm9tICcuLi9wYWNrYWdlLmpzb24nO1xuXG5mdW5jdGlvbiBkZWZhdWx0T3B0aW9ucygpIHtcbiAgICByZXR1cm4ge1xuICAgICAgICBvcHRpbWlzdGljOiBmYWxzZSxcbiAgICAgICAgZGlyZWN0aXZlOiBmYWxzZSxcbiAgICAgICAgbm9kZWpzU2NvcGU6IGZhbHNlLFxuICAgICAgICBpbXBsaWVkU3RyaWN0OiBmYWxzZSxcbiAgICAgICAgc291cmNlVHlwZTogJ3NjcmlwdCcsICAvLyBvbmUgb2YgWydzY3JpcHQnLCAnbW9kdWxlJ11cbiAgICAgICAgZWNtYVZlcnNpb246IDVcbiAgICB9O1xufVxuXG5mdW5jdGlvbiB1cGRhdGVEZWVwbHkodGFyZ2V0LCBvdmVycmlkZSkge1xuICAgIHZhciBrZXksIHZhbDtcblxuICAgIGZ1bmN0aW9uIGlzSGFzaE9iamVjdCh0YXJnZXQpIHtcbiAgICAgICAgcmV0dXJuIHR5cGVvZiB0YXJnZXQgPT09ICdvYmplY3QnICYmIHRhcmdldCBpbnN0YW5jZW9mIE9iamVjdCAmJiAhKHRhcmdldCBpbnN0YW5jZW9mIFJlZ0V4cCk7XG4gICAgfVxuXG4gICAgZm9yIChrZXkgaW4gb3ZlcnJpZGUpIHtcbiAgICAgICAgaWYgKG92ZXJyaWRlLmhhc093blByb3BlcnR5KGtleSkpIHtcbiAgICAgICAgICAgIHZhbCA9IG92ZXJyaWRlW2tleV07XG4gICAgICAgICAgICBpZiAoaXNIYXNoT2JqZWN0KHZhbCkpIHtcbiAgICAgICAgICAgICAgICBpZiAoaXNIYXNoT2JqZWN0KHRhcmdldFtrZXldKSkge1xuICAgICAgICAgICAgICAgICAgICB1cGRhdGVEZWVwbHkodGFyZ2V0W2tleV0sIHZhbCk7XG4gICAgICAgICAgICAgICAgfSBlbHNlIHtcbiAgICAgICAgICAgICAgICAgICAgdGFyZ2V0W2tleV0gPSB1cGRhdGVEZWVwbHkoe30sIHZhbCk7XG4gICAgICAgICAgICAgICAgfVxuICAgICAgICAgICAgfSBlbHNlIHtcbiAgICAgICAgICAgICAgICB0YXJnZXRba2V5XSA9IHZhbDtcbiAgICAgICAgICAgIH1cbiAgICAgICAgfVxuICAgIH1cbiAgICByZXR1cm4gdGFyZ2V0O1xufVxuXG4vKipcbiAqIE1haW4gaW50ZXJmYWNlIGZ1bmN0aW9uLiBUYWtlcyBhbiBFc3ByaW1hIHN5bnRheCB0cmVlIGFuZCByZXR1cm5zIHRoZVxuICogYW5hbHl6ZWQgc2NvcGVzLlxuICogQGZ1bmN0aW9uIGFuYWx5emVcbiAqIEBwYXJhbSB7ZXNwcmltYS5UcmVlfSB0cmVlXG4gKiBAcGFyYW0ge09iamVjdH0gcHJvdmlkZWRPcHRpb25zIC0gT3B0aW9ucyB0aGF0IHRhaWxvciB0aGUgc2NvcGUgYW5hbHlzaXNcbiAqIEBwYXJhbSB7Ym9vbGVhbn0gW3Byb3ZpZGVkT3B0aW9ucy5vcHRpbWlzdGljPWZhbHNlXSAtIHRoZSBvcHRpbWlzdGljIGZsYWdcbiAqIEBwYXJhbSB7Ym9vbGVhbn0gW3Byb3ZpZGVkT3B0aW9ucy5kaXJlY3RpdmU9ZmFsc2VdLSB0aGUgZGlyZWN0aXZlIGZsYWdcbiAqIEBwYXJhbSB7Ym9vbGVhbn0gW3Byb3ZpZGVkT3B0aW9ucy5pZ25vcmVFdmFsPWZhbHNlXS0gd2hldGhlciB0byBjaGVjayAnZXZhbCgpJyBjYWxsc1xuICogQHBhcmFtIHtib29sZWFufSBbcHJvdmlkZWRPcHRpb25zLm5vZGVqc1Njb3BlPWZhbHNlXS0gd2hldGhlciB0aGUgd2hvbGVcbiAqIHNjcmlwdCBpcyBleGVjdXRlZCB1bmRlciBub2RlLmpzIGVudmlyb25tZW50LiBXaGVuIGVuYWJsZWQsIGVzY29wZSBhZGRzXG4gKiBhIGZ1bmN0aW9uIHNjb3BlIGltbWVkaWF0ZWx5IGZvbGxvd2luZyB0aGUgZ2xvYmFsIHNjb3BlLlxuICogQHBhcmFtIHtib29sZWFufSBbcHJvdmlkZWRPcHRpb25zLmltcGxpZWRTdHJpY3Q9ZmFsc2VdLSBpbXBsaWVkIHN0cmljdCBtb2RlXG4gKiAoaWYgZWNtYVZlcnNpb24gPj0gNSkuXG4gKiBAcGFyYW0ge3N0cmluZ30gW3Byb3ZpZGVkT3B0aW9ucy5zb3VyY2VUeXBlPSdzY3JpcHQnXS0gdGhlIHNvdXJjZSB0eXBlIG9mIHRoZSBzY3JpcHQuIG9uZSBvZiAnc2NyaXB0JyBhbmQgJ21vZHVsZSdcbiAqIEBwYXJhbSB7bnVtYmVyfSBbcHJvdmlkZWRPcHRpb25zLmVjbWFWZXJzaW9uPTVdLSB3aGljaCBFQ01BU2NyaXB0IHZlcnNpb24gaXMgY29uc2lkZXJlZFxuICogQHJldHVybiB7U2NvcGVNYW5hZ2VyfVxuICovXG5leHBvcnQgZnVuY3Rpb24gYW5hbHl6ZSh0cmVlLCBwcm92aWRlZE9wdGlvbnMpIHtcbiAgICB2YXIgc2NvcGVNYW5hZ2VyLCByZWZlcmVuY2VyLCBvcHRpb25zO1xuXG4gICAgb3B0aW9ucyA9IHVwZGF0ZURlZXBseShkZWZhdWx0T3B0aW9ucygpLCBwcm92aWRlZE9wdGlvbnMpO1xuXG4gICAgc2NvcGVNYW5hZ2VyID0gbmV3IFNjb3BlTWFuYWdlcihvcHRpb25zKTtcblxuICAgIHJlZmVyZW5jZXIgPSBuZXcgUmVmZXJlbmNlcihzY29wZU1hbmFnZXIpO1xuICAgIHJlZmVyZW5jZXIudmlzaXQodHJlZSk7XG5cbiAgICBhc3NlcnQoc2NvcGVNYW5hZ2VyLl9fY3VycmVudFNjb3BlID09PSBudWxsLCAnY3VycmVudFNjb3BlIHNob3VsZCBiZSBudWxsLicpO1xuXG4gICAgcmV0dXJuIHNjb3BlTWFuYWdlcjtcbn1cblxuZXhwb3J0IHtcbiAgICAvKiogQG5hbWUgbW9kdWxlOmVzY29wZS52ZXJzaW9uICovXG4gICAgdmVyc2lvbixcbiAgICAvKiogQG5hbWUgbW9kdWxlOmVzY29wZS5SZWZlcmVuY2UgKi9cbiAgICBSZWZlcmVuY2UsXG4gICAgLyoqIEBuYW1lIG1vZHVsZTplc2NvcGUuVmFyaWFibGUgKi9cbiAgICBWYXJpYWJsZSxcbiAgICAvKiogQG5hbWUgbW9kdWxlOmVzY29wZS5TY29wZSAqL1xuICAgIFNjb3BlLFxuICAgIC8qKiBAbmFtZSBtb2R1bGU6ZXNjb3BlLlNjb3BlTWFuYWdlciAqL1xuICAgIFNjb3BlTWFuYWdlclxufTtcblxuXG4vKiB2aW06IHNldCBzdz00IHRzPTQgZXQgdHc9ODAgOiAqL1xuIl0sInNvdXJjZVJvb3QiOiIvc291cmNlLyJ9

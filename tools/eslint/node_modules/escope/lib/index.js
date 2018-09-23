"use strict";

var _interopRequire = function (obj) { return obj && obj.__esModule ? obj["default"] : obj; };

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
 * @param {string} [providedOptions.sourceType='script']- the source type of the script. one of 'script' and 'module'
 * @param {number} [providedOptions.ecmaVersion=5]- which ECMAScript version is considered
 * @return {ScopeManager}
 */
exports.analyze = analyze;
Object.defineProperty(exports, "__esModule", {
    value: true
});
/*
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

var assert = _interopRequire(require("assert"));

var ScopeManager = _interopRequire(require("./scope-manager"));

var Referencer = _interopRequire(require("./referencer"));

var Reference = _interopRequire(require("./reference"));

var Variable = _interopRequire(require("./variable"));

var Scope = _interopRequire(require("./scope"));

var version = require("../package.json").version;

function defaultOptions() {
    return {
        optimistic: false,
        directive: false,
        nodejsScope: false,
        sourceType: "script", // one of ['script', 'module']
        ecmaVersion: 5
    };
}

function updateDeeply(target, override) {
    var key, val;

    function isHashObject(target) {
        return typeof target === "object" && target instanceof Object && !(target instanceof RegExp);
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
function analyze(tree, providedOptions) {
    var scopeManager, referencer, options;

    options = updateDeeply(defaultOptions(), providedOptions);

    scopeManager = new ScopeManager(options);

    referencer = new Referencer(scopeManager);
    referencer.visit(tree);

    assert(scopeManager.__currentScope === null, "currentScope should be null.");

    return scopeManager;
}

exports.version = version;

/* vim: set sw=4 ts=4 et tw=80 : */
exports.Reference = Reference;
exports.Variable = Variable;
exports.Scope = Scope;
exports.ScopeManager = ScopeManager;

/** @name module:escope.version */

/** @name module:escope.Reference */

/** @name module:escope.Variable */

/** @name module:escope.Scope */

/** @name module:escope.ScopeManager */
//# sourceMappingURL=data:application/json;base64,eyJ2ZXJzaW9uIjozLCJzb3VyY2VzIjpbImluZGV4LmpzIl0sIm5hbWVzIjpbXSwibWFwcGluZ3MiOiI7Ozs7Ozs7Ozs7Ozs7Ozs7Ozs7O1FBNkdnQixPQUFPLEdBQVAsT0FBTzs7Ozs7Ozs7Ozs7Ozs7Ozs7Ozs7Ozs7Ozs7Ozs7Ozs7Ozs7Ozs7Ozs7Ozs7Ozs7Ozs7Ozs7O0lBM0RoQixNQUFNLDJCQUFNLFFBQVE7O0lBRXBCLFlBQVksMkJBQU0saUJBQWlCOztJQUNuQyxVQUFVLDJCQUFNLGNBQWM7O0lBQzlCLFNBQVMsMkJBQU0sYUFBYTs7SUFDNUIsUUFBUSwyQkFBTSxZQUFZOztJQUMxQixLQUFLLDJCQUFNLFNBQVM7O0lBQ2xCLE9BQU8sV0FBUSxpQkFBaUIsRUFBaEMsT0FBTzs7QUFFaEIsU0FBUyxjQUFjLEdBQUc7QUFDdEIsV0FBTztBQUNILGtCQUFVLEVBQUUsS0FBSztBQUNqQixpQkFBUyxFQUFFLEtBQUs7QUFDaEIsbUJBQVcsRUFBRSxLQUFLO0FBQ2xCLGtCQUFVLEVBQUUsUUFBUTtBQUNwQixtQkFBVyxFQUFFLENBQUM7S0FDakIsQ0FBQztDQUNMOztBQUVELFNBQVMsWUFBWSxDQUFDLE1BQU0sRUFBRSxRQUFRLEVBQUU7QUFDcEMsUUFBSSxHQUFHLEVBQUUsR0FBRyxDQUFDOztBQUViLGFBQVMsWUFBWSxDQUFDLE1BQU0sRUFBRTtBQUMxQixlQUFPLE9BQU8sTUFBTSxLQUFLLFFBQVEsSUFBSSxNQUFNLFlBQVksTUFBTSxJQUFJLEVBQUUsTUFBTSxZQUFZLE1BQU0sQ0FBQSxBQUFDLENBQUM7S0FDaEc7O0FBRUQsU0FBSyxHQUFHLElBQUksUUFBUSxFQUFFO0FBQ2xCLFlBQUksUUFBUSxDQUFDLGNBQWMsQ0FBQyxHQUFHLENBQUMsRUFBRTtBQUM5QixlQUFHLEdBQUcsUUFBUSxDQUFDLEdBQUcsQ0FBQyxDQUFDO0FBQ3BCLGdCQUFJLFlBQVksQ0FBQyxHQUFHLENBQUMsRUFBRTtBQUNuQixvQkFBSSxZQUFZLENBQUMsTUFBTSxDQUFDLEdBQUcsQ0FBQyxDQUFDLEVBQUU7QUFDM0IsZ0NBQVksQ0FBQyxNQUFNLENBQUMsR0FBRyxDQUFDLEVBQUUsR0FBRyxDQUFDLENBQUM7aUJBQ2xDLE1BQU07QUFDSCwwQkFBTSxDQUFDLEdBQUcsQ0FBQyxHQUFHLFlBQVksQ0FBQyxFQUFFLEVBQUUsR0FBRyxDQUFDLENBQUM7aUJBQ3ZDO2FBQ0osTUFBTTtBQUNILHNCQUFNLENBQUMsR0FBRyxDQUFDLEdBQUcsR0FBRyxDQUFDO2FBQ3JCO1NBQ0o7S0FDSjtBQUNELFdBQU8sTUFBTSxDQUFDO0NBQ2pCO0FBa0JNLFNBQVMsT0FBTyxDQUFDLElBQUksRUFBRSxlQUFlLEVBQUU7QUFDM0MsUUFBSSxZQUFZLEVBQUUsVUFBVSxFQUFFLE9BQU8sQ0FBQzs7QUFFdEMsV0FBTyxHQUFHLFlBQVksQ0FBQyxjQUFjLEVBQUUsRUFBRSxlQUFlLENBQUMsQ0FBQzs7QUFFMUQsZ0JBQVksR0FBRyxJQUFJLFlBQVksQ0FBQyxPQUFPLENBQUMsQ0FBQzs7QUFFekMsY0FBVSxHQUFHLElBQUksVUFBVSxDQUFDLFlBQVksQ0FBQyxDQUFDO0FBQzFDLGNBQVUsQ0FBQyxLQUFLLENBQUMsSUFBSSxDQUFDLENBQUM7O0FBRXZCLFVBQU0sQ0FBQyxZQUFZLENBQUMsY0FBYyxLQUFLLElBQUksRUFBRSw4QkFBOEIsQ0FBQyxDQUFDOztBQUU3RSxXQUFPLFlBQVksQ0FBQztDQUN2Qjs7UUFJRyxPQUFPLEdBQVAsT0FBTzs7O1FBRVAsU0FBUyxHQUFULFNBQVM7UUFFVCxRQUFRLEdBQVIsUUFBUTtRQUVSLEtBQUssR0FBTCxLQUFLO1FBRUwsWUFBWSxHQUFaLFlBQVkiLCJmaWxlIjoiaW5kZXguanMiLCJzb3VyY2VzQ29udGVudCI6WyIvKlxuICBDb3B5cmlnaHQgKEMpIDIwMTItMjAxNCBZdXN1a2UgU3V6dWtpIDx1dGF0YW5lLnRlYUBnbWFpbC5jb20+XG4gIENvcHlyaWdodCAoQykgMjAxMyBBbGV4IFNldmlsbGUgPGhpQGFsZXhhbmRlcnNldmlsbGUuY29tPlxuICBDb3B5cmlnaHQgKEMpIDIwMTQgVGhpYWdvIGRlIEFycnVkYSA8dHBhZGlsaGE4NEBnbWFpbC5jb20+XG5cbiAgUmVkaXN0cmlidXRpb24gYW5kIHVzZSBpbiBzb3VyY2UgYW5kIGJpbmFyeSBmb3Jtcywgd2l0aCBvciB3aXRob3V0XG4gIG1vZGlmaWNhdGlvbiwgYXJlIHBlcm1pdHRlZCBwcm92aWRlZCB0aGF0IHRoZSBmb2xsb3dpbmcgY29uZGl0aW9ucyBhcmUgbWV0OlxuXG4gICAgKiBSZWRpc3RyaWJ1dGlvbnMgb2Ygc291cmNlIGNvZGUgbXVzdCByZXRhaW4gdGhlIGFib3ZlIGNvcHlyaWdodFxuICAgICAgbm90aWNlLCB0aGlzIGxpc3Qgb2YgY29uZGl0aW9ucyBhbmQgdGhlIGZvbGxvd2luZyBkaXNjbGFpbWVyLlxuICAgICogUmVkaXN0cmlidXRpb25zIGluIGJpbmFyeSBmb3JtIG11c3QgcmVwcm9kdWNlIHRoZSBhYm92ZSBjb3B5cmlnaHRcbiAgICAgIG5vdGljZSwgdGhpcyBsaXN0IG9mIGNvbmRpdGlvbnMgYW5kIHRoZSBmb2xsb3dpbmcgZGlzY2xhaW1lciBpbiB0aGVcbiAgICAgIGRvY3VtZW50YXRpb24gYW5kL29yIG90aGVyIG1hdGVyaWFscyBwcm92aWRlZCB3aXRoIHRoZSBkaXN0cmlidXRpb24uXG5cbiAgVEhJUyBTT0ZUV0FSRSBJUyBQUk9WSURFRCBCWSBUSEUgQ09QWVJJR0hUIEhPTERFUlMgQU5EIENPTlRSSUJVVE9SUyBcIkFTIElTXCJcbiAgQU5EIEFOWSBFWFBSRVNTIE9SIElNUExJRUQgV0FSUkFOVElFUywgSU5DTFVESU5HLCBCVVQgTk9UIExJTUlURUQgVE8sIFRIRVxuICBJTVBMSUVEIFdBUlJBTlRJRVMgT0YgTUVSQ0hBTlRBQklMSVRZIEFORCBGSVRORVNTIEZPUiBBIFBBUlRJQ1VMQVIgUFVSUE9TRVxuICBBUkUgRElTQ0xBSU1FRC4gSU4gTk8gRVZFTlQgU0hBTEwgPENPUFlSSUdIVCBIT0xERVI+IEJFIExJQUJMRSBGT1IgQU5ZXG4gIERJUkVDVCwgSU5ESVJFQ1QsIElOQ0lERU5UQUwsIFNQRUNJQUwsIEVYRU1QTEFSWSwgT1IgQ09OU0VRVUVOVElBTCBEQU1BR0VTXG4gIChJTkNMVURJTkcsIEJVVCBOT1QgTElNSVRFRCBUTywgUFJPQ1VSRU1FTlQgT0YgU1VCU1RJVFVURSBHT09EUyBPUiBTRVJWSUNFUztcbiAgTE9TUyBPRiBVU0UsIERBVEEsIE9SIFBST0ZJVFM7IE9SIEJVU0lORVNTIElOVEVSUlVQVElPTikgSE9XRVZFUiBDQVVTRUQgQU5EXG4gIE9OIEFOWSBUSEVPUlkgT0YgTElBQklMSVRZLCBXSEVUSEVSIElOIENPTlRSQUNULCBTVFJJQ1QgTElBQklMSVRZLCBPUiBUT1JUXG4gIChJTkNMVURJTkcgTkVHTElHRU5DRSBPUiBPVEhFUldJU0UpIEFSSVNJTkcgSU4gQU5ZIFdBWSBPVVQgT0YgVEhFIFVTRSBPRlxuICBUSElTIFNPRlRXQVJFLCBFVkVOIElGIEFEVklTRUQgT0YgVEhFIFBPU1NJQklMSVRZIE9GIFNVQ0ggREFNQUdFLlxuKi9cblxuLyoqXG4gKiBFc2NvcGUgKDxhIGhyZWY9XCJodHRwOi8vZ2l0aHViLmNvbS9lc3Rvb2xzL2VzY29wZVwiPmVzY29wZTwvYT4pIGlzIGFuIDxhXG4gKiBocmVmPVwiaHR0cDovL3d3dy5lY21hLWludGVybmF0aW9uYWwub3JnL3B1YmxpY2F0aW9ucy9zdGFuZGFyZHMvRWNtYS0yNjIuaHRtXCI+RUNNQVNjcmlwdDwvYT5cbiAqIHNjb3BlIGFuYWx5emVyIGV4dHJhY3RlZCBmcm9tIHRoZSA8YVxuICogaHJlZj1cImh0dHA6Ly9naXRodWIuY29tL2VzdG9vbHMvZXNtYW5nbGVcIj5lc21hbmdsZSBwcm9qZWN0PC9hLz4uXG4gKiA8cD5cbiAqIDxlbT5lc2NvcGU8L2VtPiBmaW5kcyBsZXhpY2FsIHNjb3BlcyBpbiBhIHNvdXJjZSBwcm9ncmFtLCBpLmUuIGFyZWFzIG9mIHRoYXRcbiAqIHByb2dyYW0gd2hlcmUgZGlmZmVyZW50IG9jY3VycmVuY2VzIG9mIHRoZSBzYW1lIGlkZW50aWZpZXIgcmVmZXIgdG8gdGhlIHNhbWVcbiAqIHZhcmlhYmxlLiBXaXRoIGVhY2ggc2NvcGUgdGhlIGNvbnRhaW5lZCB2YXJpYWJsZXMgYXJlIGNvbGxlY3RlZCwgYW5kIGVhY2hcbiAqIGlkZW50aWZpZXIgcmVmZXJlbmNlIGluIGNvZGUgaXMgbGlua2VkIHRvIGl0cyBjb3JyZXNwb25kaW5nIHZhcmlhYmxlIChpZlxuICogcG9zc2libGUpLlxuICogPHA+XG4gKiA8ZW0+ZXNjb3BlPC9lbT4gd29ya3Mgb24gYSBzeW50YXggdHJlZSBvZiB0aGUgcGFyc2VkIHNvdXJjZSBjb2RlIHdoaWNoIGhhc1xuICogdG8gYWRoZXJlIHRvIHRoZSA8YVxuICogaHJlZj1cImh0dHBzOi8vZGV2ZWxvcGVyLm1vemlsbGEub3JnL2VuLVVTL2RvY3MvU3BpZGVyTW9ua2V5L1BhcnNlcl9BUElcIj5cbiAqIE1vemlsbGEgUGFyc2VyIEFQSTwvYT4uIEUuZy4gPGEgaHJlZj1cImh0dHA6Ly9lc3ByaW1hLm9yZ1wiPmVzcHJpbWE8L2E+IGlzIGEgcGFyc2VyXG4gKiB0aGF0IHByb2R1Y2VzIHN1Y2ggc3ludGF4IHRyZWVzLlxuICogPHA+XG4gKiBUaGUgbWFpbiBpbnRlcmZhY2UgaXMgdGhlIHtAbGluayBhbmFseXplfSBmdW5jdGlvbi5cbiAqIEBtb2R1bGUgZXNjb3BlXG4gKi9cblxuLypqc2xpbnQgYml0d2lzZTp0cnVlICovXG5cbmltcG9ydCBhc3NlcnQgZnJvbSAnYXNzZXJ0JztcblxuaW1wb3J0IFNjb3BlTWFuYWdlciBmcm9tICcuL3Njb3BlLW1hbmFnZXInO1xuaW1wb3J0IFJlZmVyZW5jZXIgZnJvbSAnLi9yZWZlcmVuY2VyJztcbmltcG9ydCBSZWZlcmVuY2UgZnJvbSAnLi9yZWZlcmVuY2UnO1xuaW1wb3J0IFZhcmlhYmxlIGZyb20gJy4vdmFyaWFibGUnO1xuaW1wb3J0IFNjb3BlIGZyb20gJy4vc2NvcGUnO1xuaW1wb3J0IHsgdmVyc2lvbiB9IGZyb20gJy4uL3BhY2thZ2UuanNvbic7XG5cbmZ1bmN0aW9uIGRlZmF1bHRPcHRpb25zKCkge1xuICAgIHJldHVybiB7XG4gICAgICAgIG9wdGltaXN0aWM6IGZhbHNlLFxuICAgICAgICBkaXJlY3RpdmU6IGZhbHNlLFxuICAgICAgICBub2RlanNTY29wZTogZmFsc2UsXG4gICAgICAgIHNvdXJjZVR5cGU6ICdzY3JpcHQnLCAgLy8gb25lIG9mIFsnc2NyaXB0JywgJ21vZHVsZSddXG4gICAgICAgIGVjbWFWZXJzaW9uOiA1XG4gICAgfTtcbn1cblxuZnVuY3Rpb24gdXBkYXRlRGVlcGx5KHRhcmdldCwgb3ZlcnJpZGUpIHtcbiAgICB2YXIga2V5LCB2YWw7XG5cbiAgICBmdW5jdGlvbiBpc0hhc2hPYmplY3QodGFyZ2V0KSB7XG4gICAgICAgIHJldHVybiB0eXBlb2YgdGFyZ2V0ID09PSAnb2JqZWN0JyAmJiB0YXJnZXQgaW5zdGFuY2VvZiBPYmplY3QgJiYgISh0YXJnZXQgaW5zdGFuY2VvZiBSZWdFeHApO1xuICAgIH1cblxuICAgIGZvciAoa2V5IGluIG92ZXJyaWRlKSB7XG4gICAgICAgIGlmIChvdmVycmlkZS5oYXNPd25Qcm9wZXJ0eShrZXkpKSB7XG4gICAgICAgICAgICB2YWwgPSBvdmVycmlkZVtrZXldO1xuICAgICAgICAgICAgaWYgKGlzSGFzaE9iamVjdCh2YWwpKSB7XG4gICAgICAgICAgICAgICAgaWYgKGlzSGFzaE9iamVjdCh0YXJnZXRba2V5XSkpIHtcbiAgICAgICAgICAgICAgICAgICAgdXBkYXRlRGVlcGx5KHRhcmdldFtrZXldLCB2YWwpO1xuICAgICAgICAgICAgICAgIH0gZWxzZSB7XG4gICAgICAgICAgICAgICAgICAgIHRhcmdldFtrZXldID0gdXBkYXRlRGVlcGx5KHt9LCB2YWwpO1xuICAgICAgICAgICAgICAgIH1cbiAgICAgICAgICAgIH0gZWxzZSB7XG4gICAgICAgICAgICAgICAgdGFyZ2V0W2tleV0gPSB2YWw7XG4gICAgICAgICAgICB9XG4gICAgICAgIH1cbiAgICB9XG4gICAgcmV0dXJuIHRhcmdldDtcbn1cblxuLyoqXG4gKiBNYWluIGludGVyZmFjZSBmdW5jdGlvbi4gVGFrZXMgYW4gRXNwcmltYSBzeW50YXggdHJlZSBhbmQgcmV0dXJucyB0aGVcbiAqIGFuYWx5emVkIHNjb3Blcy5cbiAqIEBmdW5jdGlvbiBhbmFseXplXG4gKiBAcGFyYW0ge2VzcHJpbWEuVHJlZX0gdHJlZVxuICogQHBhcmFtIHtPYmplY3R9IHByb3ZpZGVkT3B0aW9ucyAtIE9wdGlvbnMgdGhhdCB0YWlsb3IgdGhlIHNjb3BlIGFuYWx5c2lzXG4gKiBAcGFyYW0ge2Jvb2xlYW59IFtwcm92aWRlZE9wdGlvbnMub3B0aW1pc3RpYz1mYWxzZV0gLSB0aGUgb3B0aW1pc3RpYyBmbGFnXG4gKiBAcGFyYW0ge2Jvb2xlYW59IFtwcm92aWRlZE9wdGlvbnMuZGlyZWN0aXZlPWZhbHNlXS0gdGhlIGRpcmVjdGl2ZSBmbGFnXG4gKiBAcGFyYW0ge2Jvb2xlYW59IFtwcm92aWRlZE9wdGlvbnMuaWdub3JlRXZhbD1mYWxzZV0tIHdoZXRoZXIgdG8gY2hlY2sgJ2V2YWwoKScgY2FsbHNcbiAqIEBwYXJhbSB7Ym9vbGVhbn0gW3Byb3ZpZGVkT3B0aW9ucy5ub2RlanNTY29wZT1mYWxzZV0tIHdoZXRoZXIgdGhlIHdob2xlXG4gKiBzY3JpcHQgaXMgZXhlY3V0ZWQgdW5kZXIgbm9kZS5qcyBlbnZpcm9ubWVudC4gV2hlbiBlbmFibGVkLCBlc2NvcGUgYWRkc1xuICogYSBmdW5jdGlvbiBzY29wZSBpbW1lZGlhdGVseSBmb2xsb3dpbmcgdGhlIGdsb2JhbCBzY29wZS5cbiAqIEBwYXJhbSB7c3RyaW5nfSBbcHJvdmlkZWRPcHRpb25zLnNvdXJjZVR5cGU9J3NjcmlwdCddLSB0aGUgc291cmNlIHR5cGUgb2YgdGhlIHNjcmlwdC4gb25lIG9mICdzY3JpcHQnIGFuZCAnbW9kdWxlJ1xuICogQHBhcmFtIHtudW1iZXJ9IFtwcm92aWRlZE9wdGlvbnMuZWNtYVZlcnNpb249NV0tIHdoaWNoIEVDTUFTY3JpcHQgdmVyc2lvbiBpcyBjb25zaWRlcmVkXG4gKiBAcmV0dXJuIHtTY29wZU1hbmFnZXJ9XG4gKi9cbmV4cG9ydCBmdW5jdGlvbiBhbmFseXplKHRyZWUsIHByb3ZpZGVkT3B0aW9ucykge1xuICAgIHZhciBzY29wZU1hbmFnZXIsIHJlZmVyZW5jZXIsIG9wdGlvbnM7XG5cbiAgICBvcHRpb25zID0gdXBkYXRlRGVlcGx5KGRlZmF1bHRPcHRpb25zKCksIHByb3ZpZGVkT3B0aW9ucyk7XG5cbiAgICBzY29wZU1hbmFnZXIgPSBuZXcgU2NvcGVNYW5hZ2VyKG9wdGlvbnMpO1xuXG4gICAgcmVmZXJlbmNlciA9IG5ldyBSZWZlcmVuY2VyKHNjb3BlTWFuYWdlcik7XG4gICAgcmVmZXJlbmNlci52aXNpdCh0cmVlKTtcblxuICAgIGFzc2VydChzY29wZU1hbmFnZXIuX19jdXJyZW50U2NvcGUgPT09IG51bGwsICdjdXJyZW50U2NvcGUgc2hvdWxkIGJlIG51bGwuJyk7XG5cbiAgICByZXR1cm4gc2NvcGVNYW5hZ2VyO1xufVxuXG5leHBvcnQge1xuICAgIC8qKiBAbmFtZSBtb2R1bGU6ZXNjb3BlLnZlcnNpb24gKi9cbiAgICB2ZXJzaW9uLFxuICAgIC8qKiBAbmFtZSBtb2R1bGU6ZXNjb3BlLlJlZmVyZW5jZSAqL1xuICAgIFJlZmVyZW5jZSxcbiAgICAvKiogQG5hbWUgbW9kdWxlOmVzY29wZS5WYXJpYWJsZSAqL1xuICAgIFZhcmlhYmxlLFxuICAgIC8qKiBAbmFtZSBtb2R1bGU6ZXNjb3BlLlNjb3BlICovXG4gICAgU2NvcGUsXG4gICAgLyoqIEBuYW1lIG1vZHVsZTplc2NvcGUuU2NvcGVNYW5hZ2VyICovXG4gICAgU2NvcGVNYW5hZ2VyXG59O1xuXG5cbi8qIHZpbTogc2V0IHN3PTQgdHM9NCBldCB0dz04MCA6ICovXG4iXSwic291cmNlUm9vdCI6Ii9zb3VyY2UvIn0=
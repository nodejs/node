"use strict";

var _interopRequire = function (obj) { return obj && obj.__esModule ? obj["default"] : obj; };

var _get = function get(object, property, receiver) { var desc = Object.getOwnPropertyDescriptor(object, property); if (desc === undefined) { var parent = Object.getPrototypeOf(object); if (parent === null) { return undefined; } else { return get(parent, property, receiver); } } else if ("value" in desc && desc.writable) { return desc.value; } else { var getter = desc.get; if (getter === undefined) { return undefined; } return getter.call(receiver); } };

var _inherits = function (subClass, superClass) { if (typeof superClass !== "function" && superClass !== null) { throw new TypeError("Super expression must either be null or a function, not " + typeof superClass); } subClass.prototype = Object.create(superClass && superClass.prototype, { constructor: { value: subClass, enumerable: false, writable: true, configurable: true } }); if (superClass) subClass.__proto__ = superClass; };

var _classCallCheck = function (instance, Constructor) { if (!(instance instanceof Constructor)) { throw new TypeError("Cannot call a class as a function"); } };

Object.defineProperty(exports, "__esModule", {
  value: true
});
/*
  Copyright (C) 2015 Yusuke Suzuki <utatane.tea@gmail.com>

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

var Variable = _interopRequire(require("./variable"));

/**
 * @class Definition
 */

var Definition = function Definition(type, name, node, parent, index, kind) {
  _classCallCheck(this, Definition);

  /**
   * @member {String} Definition#type - type of the occurrence (e.g. "Parameter", "Variable", ...).
   */
  this.type = type;
  /**
   * @member {esprima.Identifier} Definition#name - the identifier AST node of the occurrence.
   */
  this.name = name;
  /**
   * @member {esprima.Node} Definition#node - the enclosing node of the identifier.
   */
  this.node = node;
  /**
   * @member {esprima.Node?} Definition#parent - the enclosing statement node of the identifier.
   */
  this.parent = parent;
  /**
   * @member {Number?} Definition#index - the index in the declaration statement.
   */
  this.index = index;
  /**
   * @member {String?} Definition#kind - the kind of the declaration statement.
   */
  this.kind = kind;
};

exports["default"] = Definition;

/**
 * @class ParameterDefinition
 */

var ParameterDefinition = (function (_Definition) {
  function ParameterDefinition(name, node, index, rest) {
    _classCallCheck(this, ParameterDefinition);

    _get(Object.getPrototypeOf(ParameterDefinition.prototype), "constructor", this).call(this, Variable.Parameter, name, node, null, index, null);
    /**
     * Whether the parameter definition is a part of a rest parameter.
     * @member {boolean} ParameterDefinition#rest
     */
    this.rest = rest;
  }

  _inherits(ParameterDefinition, _Definition);

  return ParameterDefinition;
})(Definition);

exports.ParameterDefinition = ParameterDefinition;

/* vim: set sw=4 ts=4 et tw=80 : */
exports.Definition = Definition;
//# sourceMappingURL=data:application/json;base64,eyJ2ZXJzaW9uIjozLCJzb3VyY2VzIjpbImRlZmluaXRpb24uanMiXSwibmFtZXMiOltdLCJtYXBwaW5ncyI6Ijs7Ozs7Ozs7Ozs7Ozs7Ozs7Ozs7Ozs7Ozs7Ozs7Ozs7Ozs7OztJQXdCTyxRQUFRLDJCQUFNLFlBQVk7Ozs7OztJQUtaLFVBQVUsR0FDaEIsU0FETSxVQUFVLENBQ2YsSUFBSSxFQUFFLElBQUksRUFBRSxJQUFJLEVBQUUsTUFBTSxFQUFFLEtBQUssRUFBRSxJQUFJLEVBQUU7d0JBRGxDLFVBQVU7Ozs7O0FBS3ZCLE1BQUksQ0FBQyxJQUFJLEdBQUcsSUFBSSxDQUFDOzs7O0FBSWpCLE1BQUksQ0FBQyxJQUFJLEdBQUcsSUFBSSxDQUFDOzs7O0FBSWpCLE1BQUksQ0FBQyxJQUFJLEdBQUcsSUFBSSxDQUFDOzs7O0FBSWpCLE1BQUksQ0FBQyxNQUFNLEdBQUcsTUFBTSxDQUFDOzs7O0FBSXJCLE1BQUksQ0FBQyxLQUFLLEdBQUcsS0FBSyxDQUFDOzs7O0FBSW5CLE1BQUksQ0FBQyxJQUFJLEdBQUcsSUFBSSxDQUFDO0NBQ3BCOztxQkExQmdCLFVBQVU7Ozs7OztJQWdDekIsbUJBQW1CO0FBQ1YsV0FEVCxtQkFBbUIsQ0FDVCxJQUFJLEVBQUUsSUFBSSxFQUFFLEtBQUssRUFBRSxJQUFJLEVBQUU7MEJBRG5DLG1CQUFtQjs7QUFFakIsK0JBRkYsbUJBQW1CLDZDQUVYLFFBQVEsQ0FBQyxTQUFTLEVBQUUsSUFBSSxFQUFFLElBQUksRUFBRSxJQUFJLEVBQUUsS0FBSyxFQUFFLElBQUksRUFBRTs7Ozs7QUFLekQsUUFBSSxDQUFDLElBQUksR0FBRyxJQUFJLENBQUM7R0FDcEI7O1lBUkMsbUJBQW1COztTQUFuQixtQkFBbUI7R0FBUyxVQUFVOztRQVl4QyxtQkFBbUIsR0FBbkIsbUJBQW1COzs7UUFDbkIsVUFBVSxHQUFWLFVBQVUiLCJmaWxlIjoiZGVmaW5pdGlvbi5qcyIsInNvdXJjZXNDb250ZW50IjpbIi8qXG4gIENvcHlyaWdodCAoQykgMjAxNSBZdXN1a2UgU3V6dWtpIDx1dGF0YW5lLnRlYUBnbWFpbC5jb20+XG5cbiAgUmVkaXN0cmlidXRpb24gYW5kIHVzZSBpbiBzb3VyY2UgYW5kIGJpbmFyeSBmb3Jtcywgd2l0aCBvciB3aXRob3V0XG4gIG1vZGlmaWNhdGlvbiwgYXJlIHBlcm1pdHRlZCBwcm92aWRlZCB0aGF0IHRoZSBmb2xsb3dpbmcgY29uZGl0aW9ucyBhcmUgbWV0OlxuXG4gICAgKiBSZWRpc3RyaWJ1dGlvbnMgb2Ygc291cmNlIGNvZGUgbXVzdCByZXRhaW4gdGhlIGFib3ZlIGNvcHlyaWdodFxuICAgICAgbm90aWNlLCB0aGlzIGxpc3Qgb2YgY29uZGl0aW9ucyBhbmQgdGhlIGZvbGxvd2luZyBkaXNjbGFpbWVyLlxuICAgICogUmVkaXN0cmlidXRpb25zIGluIGJpbmFyeSBmb3JtIG11c3QgcmVwcm9kdWNlIHRoZSBhYm92ZSBjb3B5cmlnaHRcbiAgICAgIG5vdGljZSwgdGhpcyBsaXN0IG9mIGNvbmRpdGlvbnMgYW5kIHRoZSBmb2xsb3dpbmcgZGlzY2xhaW1lciBpbiB0aGVcbiAgICAgIGRvY3VtZW50YXRpb24gYW5kL29yIG90aGVyIG1hdGVyaWFscyBwcm92aWRlZCB3aXRoIHRoZSBkaXN0cmlidXRpb24uXG5cbiAgVEhJUyBTT0ZUV0FSRSBJUyBQUk9WSURFRCBCWSBUSEUgQ09QWVJJR0hUIEhPTERFUlMgQU5EIENPTlRSSUJVVE9SUyBcIkFTIElTXCJcbiAgQU5EIEFOWSBFWFBSRVNTIE9SIElNUExJRUQgV0FSUkFOVElFUywgSU5DTFVESU5HLCBCVVQgTk9UIExJTUlURUQgVE8sIFRIRVxuICBJTVBMSUVEIFdBUlJBTlRJRVMgT0YgTUVSQ0hBTlRBQklMSVRZIEFORCBGSVRORVNTIEZPUiBBIFBBUlRJQ1VMQVIgUFVSUE9TRVxuICBBUkUgRElTQ0xBSU1FRC4gSU4gTk8gRVZFTlQgU0hBTEwgPENPUFlSSUdIVCBIT0xERVI+IEJFIExJQUJMRSBGT1IgQU5ZXG4gIERJUkVDVCwgSU5ESVJFQ1QsIElOQ0lERU5UQUwsIFNQRUNJQUwsIEVYRU1QTEFSWSwgT1IgQ09OU0VRVUVOVElBTCBEQU1BR0VTXG4gIChJTkNMVURJTkcsIEJVVCBOT1QgTElNSVRFRCBUTywgUFJPQ1VSRU1FTlQgT0YgU1VCU1RJVFVURSBHT09EUyBPUiBTRVJWSUNFUztcbiAgTE9TUyBPRiBVU0UsIERBVEEsIE9SIFBST0ZJVFM7IE9SIEJVU0lORVNTIElOVEVSUlVQVElPTikgSE9XRVZFUiBDQVVTRUQgQU5EXG4gIE9OIEFOWSBUSEVPUlkgT0YgTElBQklMSVRZLCBXSEVUSEVSIElOIENPTlRSQUNULCBTVFJJQ1QgTElBQklMSVRZLCBPUiBUT1JUXG4gIChJTkNMVURJTkcgTkVHTElHRU5DRSBPUiBPVEhFUldJU0UpIEFSSVNJTkcgSU4gQU5ZIFdBWSBPVVQgT0YgVEhFIFVTRSBPRlxuICBUSElTIFNPRlRXQVJFLCBFVkVOIElGIEFEVklTRUQgT0YgVEhFIFBPU1NJQklMSVRZIE9GIFNVQ0ggREFNQUdFLlxuKi9cblxuaW1wb3J0IFZhcmlhYmxlIGZyb20gJy4vdmFyaWFibGUnO1xuXG4vKipcbiAqIEBjbGFzcyBEZWZpbml0aW9uXG4gKi9cbmV4cG9ydCBkZWZhdWx0IGNsYXNzIERlZmluaXRpb24ge1xuICAgIGNvbnN0cnVjdG9yKHR5cGUsIG5hbWUsIG5vZGUsIHBhcmVudCwgaW5kZXgsIGtpbmQpIHtcbiAgICAgICAgLyoqXG4gICAgICAgICAqIEBtZW1iZXIge1N0cmluZ30gRGVmaW5pdGlvbiN0eXBlIC0gdHlwZSBvZiB0aGUgb2NjdXJyZW5jZSAoZS5nLiBcIlBhcmFtZXRlclwiLCBcIlZhcmlhYmxlXCIsIC4uLikuXG4gICAgICAgICAqL1xuICAgICAgICB0aGlzLnR5cGUgPSB0eXBlO1xuICAgICAgICAvKipcbiAgICAgICAgICogQG1lbWJlciB7ZXNwcmltYS5JZGVudGlmaWVyfSBEZWZpbml0aW9uI25hbWUgLSB0aGUgaWRlbnRpZmllciBBU1Qgbm9kZSBvZiB0aGUgb2NjdXJyZW5jZS5cbiAgICAgICAgICovXG4gICAgICAgIHRoaXMubmFtZSA9IG5hbWU7XG4gICAgICAgIC8qKlxuICAgICAgICAgKiBAbWVtYmVyIHtlc3ByaW1hLk5vZGV9IERlZmluaXRpb24jbm9kZSAtIHRoZSBlbmNsb3Npbmcgbm9kZSBvZiB0aGUgaWRlbnRpZmllci5cbiAgICAgICAgICovXG4gICAgICAgIHRoaXMubm9kZSA9IG5vZGU7XG4gICAgICAgIC8qKlxuICAgICAgICAgKiBAbWVtYmVyIHtlc3ByaW1hLk5vZGU/fSBEZWZpbml0aW9uI3BhcmVudCAtIHRoZSBlbmNsb3Npbmcgc3RhdGVtZW50IG5vZGUgb2YgdGhlIGlkZW50aWZpZXIuXG4gICAgICAgICAqL1xuICAgICAgICB0aGlzLnBhcmVudCA9IHBhcmVudDtcbiAgICAgICAgLyoqXG4gICAgICAgICAqIEBtZW1iZXIge051bWJlcj99IERlZmluaXRpb24jaW5kZXggLSB0aGUgaW5kZXggaW4gdGhlIGRlY2xhcmF0aW9uIHN0YXRlbWVudC5cbiAgICAgICAgICovXG4gICAgICAgIHRoaXMuaW5kZXggPSBpbmRleDtcbiAgICAgICAgLyoqXG4gICAgICAgICAqIEBtZW1iZXIge1N0cmluZz99IERlZmluaXRpb24ja2luZCAtIHRoZSBraW5kIG9mIHRoZSBkZWNsYXJhdGlvbiBzdGF0ZW1lbnQuXG4gICAgICAgICAqL1xuICAgICAgICB0aGlzLmtpbmQgPSBraW5kO1xuICAgIH1cbn1cblxuLyoqXG4gKiBAY2xhc3MgUGFyYW1ldGVyRGVmaW5pdGlvblxuICovXG5jbGFzcyBQYXJhbWV0ZXJEZWZpbml0aW9uIGV4dGVuZHMgRGVmaW5pdGlvbiB7XG4gICAgY29uc3RydWN0b3IobmFtZSwgbm9kZSwgaW5kZXgsIHJlc3QpIHtcbiAgICAgICAgc3VwZXIoVmFyaWFibGUuUGFyYW1ldGVyLCBuYW1lLCBub2RlLCBudWxsLCBpbmRleCwgbnVsbCk7XG4gICAgICAgIC8qKlxuICAgICAgICAgKiBXaGV0aGVyIHRoZSBwYXJhbWV0ZXIgZGVmaW5pdGlvbiBpcyBhIHBhcnQgb2YgYSByZXN0IHBhcmFtZXRlci5cbiAgICAgICAgICogQG1lbWJlciB7Ym9vbGVhbn0gUGFyYW1ldGVyRGVmaW5pdGlvbiNyZXN0XG4gICAgICAgICAqL1xuICAgICAgICB0aGlzLnJlc3QgPSByZXN0O1xuICAgIH1cbn1cblxuZXhwb3J0IHtcbiAgICBQYXJhbWV0ZXJEZWZpbml0aW9uLFxuICAgIERlZmluaXRpb25cbn1cblxuLyogdmltOiBzZXQgc3c9NCB0cz00IGV0IHR3PTgwIDogKi9cbiJdLCJzb3VyY2VSb290IjoiL3NvdXJjZS8ifQ==
"use strict";

var _classCallCheck = function (instance, Constructor) { if (!(instance instanceof Constructor)) { throw new TypeError("Cannot call a class as a function"); } };

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

/**
 * A Variable represents a locally scoped identifier. These include arguments to
 * functions.
 * @class Variable
 */

var Variable = function Variable(name, scope) {
  _classCallCheck(this, Variable);

  /**
   * The variable name, as given in the source code.
   * @member {String} Variable#name
   */
  this.name = name;
  /**
   * List of defining occurrences of this variable (like in 'var ...'
   * statements or as parameter), as AST nodes.
   * @member {esprima.Identifier[]} Variable#identifiers
   */
  this.identifiers = [];
  /**
   * List of {@link Reference|references} of this variable (excluding parameter entries)
   * in its defining scope and all nested scopes. For defining
   * occurrences only see {@link Variable#defs}.
   * @member {Reference[]} Variable#references
   */
  this.references = [];

  /**
   * List of defining occurrences of this variable (like in 'var ...'
   * statements or as parameter), as custom objects.
   * @member {Definition[]} Variable#defs
   */
  this.defs = [];

  this.tainted = false;
  /**
   * Whether this is a stack variable.
   * @member {boolean} Variable#stack
   */
  this.stack = true;
  /**
   * Reference to the enclosing Scope.
   * @member {Scope} Variable#scope
   */
  this.scope = scope;
};

module.exports = Variable;

Variable.CatchClause = "CatchClause";
Variable.Parameter = "Parameter";
Variable.FunctionName = "FunctionName";
Variable.ClassName = "ClassName";
Variable.Variable = "Variable";
Variable.ImportBinding = "ImportBinding";
Variable.TDZ = "TDZ";
Variable.ImplicitGlobalVariable = "ImplicitGlobalVariable";

/* vim: set sw=4 ts=4 et tw=80 : */
//# sourceMappingURL=data:application/json;base64,eyJ2ZXJzaW9uIjozLCJzb3VyY2VzIjpbInZhcmlhYmxlLmpzIl0sIm5hbWVzIjpbXSwibWFwcGluZ3MiOiI7Ozs7Ozs7Ozs7Ozs7Ozs7Ozs7Ozs7Ozs7Ozs7Ozs7Ozs7SUE2QnFCLFFBQVEsR0FDZCxTQURNLFFBQVEsQ0FDYixJQUFJLEVBQUUsS0FBSyxFQUFFO3dCQURSLFFBQVE7Ozs7OztBQU1yQixNQUFJLENBQUMsSUFBSSxHQUFHLElBQUksQ0FBQzs7Ozs7O0FBTWpCLE1BQUksQ0FBQyxXQUFXLEdBQUcsRUFBRSxDQUFDOzs7Ozs7O0FBT3RCLE1BQUksQ0FBQyxVQUFVLEdBQUcsRUFBRSxDQUFDOzs7Ozs7O0FBT3JCLE1BQUksQ0FBQyxJQUFJLEdBQUcsRUFBRSxDQUFDOztBQUVmLE1BQUksQ0FBQyxPQUFPLEdBQUcsS0FBSyxDQUFDOzs7OztBQUtyQixNQUFJLENBQUMsS0FBSyxHQUFHLElBQUksQ0FBQzs7Ozs7QUFLbEIsTUFBSSxDQUFDLEtBQUssR0FBRyxLQUFLLENBQUM7Q0FDdEI7O2lCQXZDZ0IsUUFBUTs7QUEwQzdCLFFBQVEsQ0FBQyxXQUFXLEdBQUcsYUFBYSxDQUFDO0FBQ3JDLFFBQVEsQ0FBQyxTQUFTLEdBQUcsV0FBVyxDQUFDO0FBQ2pDLFFBQVEsQ0FBQyxZQUFZLEdBQUcsY0FBYyxDQUFDO0FBQ3ZDLFFBQVEsQ0FBQyxTQUFTLEdBQUcsV0FBVyxDQUFDO0FBQ2pDLFFBQVEsQ0FBQyxRQUFRLEdBQUcsVUFBVSxDQUFDO0FBQy9CLFFBQVEsQ0FBQyxhQUFhLEdBQUcsZUFBZSxDQUFDO0FBQ3pDLFFBQVEsQ0FBQyxHQUFHLEdBQUcsS0FBSyxDQUFDO0FBQ3JCLFFBQVEsQ0FBQyxzQkFBc0IsR0FBRyx3QkFBd0IsQ0FBQyIsImZpbGUiOiJ2YXJpYWJsZS5qcyIsInNvdXJjZXNDb250ZW50IjpbIi8qXG4gIENvcHlyaWdodCAoQykgMjAxNSBZdXN1a2UgU3V6dWtpIDx1dGF0YW5lLnRlYUBnbWFpbC5jb20+XG5cbiAgUmVkaXN0cmlidXRpb24gYW5kIHVzZSBpbiBzb3VyY2UgYW5kIGJpbmFyeSBmb3Jtcywgd2l0aCBvciB3aXRob3V0XG4gIG1vZGlmaWNhdGlvbiwgYXJlIHBlcm1pdHRlZCBwcm92aWRlZCB0aGF0IHRoZSBmb2xsb3dpbmcgY29uZGl0aW9ucyBhcmUgbWV0OlxuXG4gICAgKiBSZWRpc3RyaWJ1dGlvbnMgb2Ygc291cmNlIGNvZGUgbXVzdCByZXRhaW4gdGhlIGFib3ZlIGNvcHlyaWdodFxuICAgICAgbm90aWNlLCB0aGlzIGxpc3Qgb2YgY29uZGl0aW9ucyBhbmQgdGhlIGZvbGxvd2luZyBkaXNjbGFpbWVyLlxuICAgICogUmVkaXN0cmlidXRpb25zIGluIGJpbmFyeSBmb3JtIG11c3QgcmVwcm9kdWNlIHRoZSBhYm92ZSBjb3B5cmlnaHRcbiAgICAgIG5vdGljZSwgdGhpcyBsaXN0IG9mIGNvbmRpdGlvbnMgYW5kIHRoZSBmb2xsb3dpbmcgZGlzY2xhaW1lciBpbiB0aGVcbiAgICAgIGRvY3VtZW50YXRpb24gYW5kL29yIG90aGVyIG1hdGVyaWFscyBwcm92aWRlZCB3aXRoIHRoZSBkaXN0cmlidXRpb24uXG5cbiAgVEhJUyBTT0ZUV0FSRSBJUyBQUk9WSURFRCBCWSBUSEUgQ09QWVJJR0hUIEhPTERFUlMgQU5EIENPTlRSSUJVVE9SUyBcIkFTIElTXCJcbiAgQU5EIEFOWSBFWFBSRVNTIE9SIElNUExJRUQgV0FSUkFOVElFUywgSU5DTFVESU5HLCBCVVQgTk9UIExJTUlURUQgVE8sIFRIRVxuICBJTVBMSUVEIFdBUlJBTlRJRVMgT0YgTUVSQ0hBTlRBQklMSVRZIEFORCBGSVRORVNTIEZPUiBBIFBBUlRJQ1VMQVIgUFVSUE9TRVxuICBBUkUgRElTQ0xBSU1FRC4gSU4gTk8gRVZFTlQgU0hBTEwgPENPUFlSSUdIVCBIT0xERVI+IEJFIExJQUJMRSBGT1IgQU5ZXG4gIERJUkVDVCwgSU5ESVJFQ1QsIElOQ0lERU5UQUwsIFNQRUNJQUwsIEVYRU1QTEFSWSwgT1IgQ09OU0VRVUVOVElBTCBEQU1BR0VTXG4gIChJTkNMVURJTkcsIEJVVCBOT1QgTElNSVRFRCBUTywgUFJPQ1VSRU1FTlQgT0YgU1VCU1RJVFVURSBHT09EUyBPUiBTRVJWSUNFUztcbiAgTE9TUyBPRiBVU0UsIERBVEEsIE9SIFBST0ZJVFM7IE9SIEJVU0lORVNTIElOVEVSUlVQVElPTikgSE9XRVZFUiBDQVVTRUQgQU5EXG4gIE9OIEFOWSBUSEVPUlkgT0YgTElBQklMSVRZLCBXSEVUSEVSIElOIENPTlRSQUNULCBTVFJJQ1QgTElBQklMSVRZLCBPUiBUT1JUXG4gIChJTkNMVURJTkcgTkVHTElHRU5DRSBPUiBPVEhFUldJU0UpIEFSSVNJTkcgSU4gQU5ZIFdBWSBPVVQgT0YgVEhFIFVTRSBPRlxuICBUSElTIFNPRlRXQVJFLCBFVkVOIElGIEFEVklTRUQgT0YgVEhFIFBPU1NJQklMSVRZIE9GIFNVQ0ggREFNQUdFLlxuKi9cblxuLyoqXG4gKiBBIFZhcmlhYmxlIHJlcHJlc2VudHMgYSBsb2NhbGx5IHNjb3BlZCBpZGVudGlmaWVyLiBUaGVzZSBpbmNsdWRlIGFyZ3VtZW50cyB0b1xuICogZnVuY3Rpb25zLlxuICogQGNsYXNzIFZhcmlhYmxlXG4gKi9cbmV4cG9ydCBkZWZhdWx0IGNsYXNzIFZhcmlhYmxlIHtcbiAgICBjb25zdHJ1Y3RvcihuYW1lLCBzY29wZSkge1xuICAgICAgICAvKipcbiAgICAgICAgICogVGhlIHZhcmlhYmxlIG5hbWUsIGFzIGdpdmVuIGluIHRoZSBzb3VyY2UgY29kZS5cbiAgICAgICAgICogQG1lbWJlciB7U3RyaW5nfSBWYXJpYWJsZSNuYW1lXG4gICAgICAgICAqL1xuICAgICAgICB0aGlzLm5hbWUgPSBuYW1lO1xuICAgICAgICAvKipcbiAgICAgICAgICogTGlzdCBvZiBkZWZpbmluZyBvY2N1cnJlbmNlcyBvZiB0aGlzIHZhcmlhYmxlIChsaWtlIGluICd2YXIgLi4uJ1xuICAgICAgICAgKiBzdGF0ZW1lbnRzIG9yIGFzIHBhcmFtZXRlciksIGFzIEFTVCBub2Rlcy5cbiAgICAgICAgICogQG1lbWJlciB7ZXNwcmltYS5JZGVudGlmaWVyW119IFZhcmlhYmxlI2lkZW50aWZpZXJzXG4gICAgICAgICAqL1xuICAgICAgICB0aGlzLmlkZW50aWZpZXJzID0gW107XG4gICAgICAgIC8qKlxuICAgICAgICAgKiBMaXN0IG9mIHtAbGluayBSZWZlcmVuY2V8cmVmZXJlbmNlc30gb2YgdGhpcyB2YXJpYWJsZSAoZXhjbHVkaW5nIHBhcmFtZXRlciBlbnRyaWVzKVxuICAgICAgICAgKiBpbiBpdHMgZGVmaW5pbmcgc2NvcGUgYW5kIGFsbCBuZXN0ZWQgc2NvcGVzLiBGb3IgZGVmaW5pbmdcbiAgICAgICAgICogb2NjdXJyZW5jZXMgb25seSBzZWUge0BsaW5rIFZhcmlhYmxlI2RlZnN9LlxuICAgICAgICAgKiBAbWVtYmVyIHtSZWZlcmVuY2VbXX0gVmFyaWFibGUjcmVmZXJlbmNlc1xuICAgICAgICAgKi9cbiAgICAgICAgdGhpcy5yZWZlcmVuY2VzID0gW107XG5cbiAgICAgICAgLyoqXG4gICAgICAgICAqIExpc3Qgb2YgZGVmaW5pbmcgb2NjdXJyZW5jZXMgb2YgdGhpcyB2YXJpYWJsZSAobGlrZSBpbiAndmFyIC4uLidcbiAgICAgICAgICogc3RhdGVtZW50cyBvciBhcyBwYXJhbWV0ZXIpLCBhcyBjdXN0b20gb2JqZWN0cy5cbiAgICAgICAgICogQG1lbWJlciB7RGVmaW5pdGlvbltdfSBWYXJpYWJsZSNkZWZzXG4gICAgICAgICAqL1xuICAgICAgICB0aGlzLmRlZnMgPSBbXTtcblxuICAgICAgICB0aGlzLnRhaW50ZWQgPSBmYWxzZTtcbiAgICAgICAgLyoqXG4gICAgICAgICAqIFdoZXRoZXIgdGhpcyBpcyBhIHN0YWNrIHZhcmlhYmxlLlxuICAgICAgICAgKiBAbWVtYmVyIHtib29sZWFufSBWYXJpYWJsZSNzdGFja1xuICAgICAgICAgKi9cbiAgICAgICAgdGhpcy5zdGFjayA9IHRydWU7XG4gICAgICAgIC8qKlxuICAgICAgICAgKiBSZWZlcmVuY2UgdG8gdGhlIGVuY2xvc2luZyBTY29wZS5cbiAgICAgICAgICogQG1lbWJlciB7U2NvcGV9IFZhcmlhYmxlI3Njb3BlXG4gICAgICAgICAqL1xuICAgICAgICB0aGlzLnNjb3BlID0gc2NvcGU7XG4gICAgfVxufVxuXG5WYXJpYWJsZS5DYXRjaENsYXVzZSA9ICdDYXRjaENsYXVzZSc7XG5WYXJpYWJsZS5QYXJhbWV0ZXIgPSAnUGFyYW1ldGVyJztcblZhcmlhYmxlLkZ1bmN0aW9uTmFtZSA9ICdGdW5jdGlvbk5hbWUnO1xuVmFyaWFibGUuQ2xhc3NOYW1lID0gJ0NsYXNzTmFtZSc7XG5WYXJpYWJsZS5WYXJpYWJsZSA9ICdWYXJpYWJsZSc7XG5WYXJpYWJsZS5JbXBvcnRCaW5kaW5nID0gJ0ltcG9ydEJpbmRpbmcnO1xuVmFyaWFibGUuVERaID0gJ1REWic7XG5WYXJpYWJsZS5JbXBsaWNpdEdsb2JhbFZhcmlhYmxlID0gJ0ltcGxpY2l0R2xvYmFsVmFyaWFibGUnO1xuXG4vKiB2aW06IHNldCBzdz00IHRzPTQgZXQgdHc9ODAgOiAqL1xuIl0sInNvdXJjZVJvb3QiOiIvc291cmNlLyJ9

"use strict";

var _createClass = (function () { function defineProperties(target, props) { for (var key in props) { var prop = props[key]; prop.configurable = true; if (prop.value) prop.writable = true; } Object.defineProperties(target, props); } return function (Constructor, protoProps, staticProps) { if (protoProps) defineProperties(Constructor.prototype, protoProps); if (staticProps) defineProperties(Constructor, staticProps); return Constructor; }; })();

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

var READ = 1;
var WRITE = 2;
var RW = READ | WRITE;

/**
 * A Reference represents a single occurrence of an identifier in code.
 * @class Reference
 */

var Reference = (function () {
  function Reference(ident, scope, flag, writeExpr, maybeImplicitGlobal, partial) {
    _classCallCheck(this, Reference);

    /**
     * Identifier syntax node.
     * @member {esprima#Identifier} Reference#identifier
     */
    this.identifier = ident;
    /**
     * Reference to the enclosing Scope.
     * @member {Scope} Reference#from
     */
    this.from = scope;
    /**
     * Whether the reference comes from a dynamic scope (such as 'eval',
     * 'with', etc.), and may be trapped by dynamic scopes.
     * @member {boolean} Reference#tainted
     */
    this.tainted = false;
    /**
     * The variable this reference is resolved with.
     * @member {Variable} Reference#resolved
     */
    this.resolved = null;
    /**
     * The read-write mode of the reference. (Value is one of {@link
     * Reference.READ}, {@link Reference.RW}, {@link Reference.WRITE}).
     * @member {number} Reference#flag
     * @private
     */
    this.flag = flag;
    if (this.isWrite()) {
      /**
       * If reference is writeable, this is the tree being written to it.
       * @member {esprima#Node} Reference#writeExpr
       */
      this.writeExpr = writeExpr;
      /**
       * Whether the Reference might refer to a partial value of writeExpr.
       * @member {boolean} Reference#partial
       */
      this.partial = partial;
    }
    this.__maybeImplicitGlobal = maybeImplicitGlobal;
  }

  _createClass(Reference, {
    isStatic: {

      /**
       * Whether the reference is static.
       * @method Reference#isStatic
       * @return {boolean}
       */

      value: function isStatic() {
        return !this.tainted && this.resolved && this.resolved.scope.isStatic();
      }
    },
    isWrite: {

      /**
       * Whether the reference is writeable.
       * @method Reference#isWrite
       * @return {boolean}
       */

      value: function isWrite() {
        return !!(this.flag & Reference.WRITE);
      }
    },
    isRead: {

      /**
       * Whether the reference is readable.
       * @method Reference#isRead
       * @return {boolean}
       */

      value: function isRead() {
        return !!(this.flag & Reference.READ);
      }
    },
    isReadOnly: {

      /**
       * Whether the reference is read-only.
       * @method Reference#isReadOnly
       * @return {boolean}
       */

      value: function isReadOnly() {
        return this.flag === Reference.READ;
      }
    },
    isWriteOnly: {

      /**
       * Whether the reference is write-only.
       * @method Reference#isWriteOnly
       * @return {boolean}
       */

      value: function isWriteOnly() {
        return this.flag === Reference.WRITE;
      }
    },
    isReadWrite: {

      /**
       * Whether the reference is read-write.
       * @method Reference#isReadWrite
       * @return {boolean}
       */

      value: function isReadWrite() {
        return this.flag === Reference.RW;
      }
    }
  });

  return Reference;
})();

module.exports = Reference;

/**
 * @constant Reference.READ
 * @private
 */
Reference.READ = READ;
/**
 * @constant Reference.WRITE
 * @private
 */
Reference.WRITE = WRITE;
/**
 * @constant Reference.RW
 * @private
 */
Reference.RW = RW;

/* vim: set sw=4 ts=4 et tw=80 : */
//# sourceMappingURL=data:application/json;base64,eyJ2ZXJzaW9uIjozLCJzb3VyY2VzIjpbInJlZmVyZW5jZS5qcyJdLCJuYW1lcyI6W10sIm1hcHBpbmdzIjoiOzs7Ozs7Ozs7Ozs7Ozs7Ozs7Ozs7Ozs7Ozs7Ozs7QUF3QkEsSUFBTSxJQUFJLEdBQUcsQ0FBRyxDQUFDO0FBQ2pCLElBQU0sS0FBSyxHQUFHLENBQUcsQ0FBQztBQUNsQixJQUFNLEVBQUUsR0FBRyxJQUFJLEdBQUcsS0FBSyxDQUFDOzs7Ozs7O0lBTUgsU0FBUztBQUNmLFdBRE0sU0FBUyxDQUNkLEtBQUssRUFBRSxLQUFLLEVBQUUsSUFBSSxFQUFHLFNBQVMsRUFBRSxtQkFBbUIsRUFBRSxPQUFPLEVBQUU7MEJBRHpELFNBQVM7Ozs7OztBQU10QixRQUFJLENBQUMsVUFBVSxHQUFHLEtBQUssQ0FBQzs7Ozs7QUFLeEIsUUFBSSxDQUFDLElBQUksR0FBRyxLQUFLLENBQUM7Ozs7OztBQU1sQixRQUFJLENBQUMsT0FBTyxHQUFHLEtBQUssQ0FBQzs7Ozs7QUFLckIsUUFBSSxDQUFDLFFBQVEsR0FBRyxJQUFJLENBQUM7Ozs7Ozs7QUFPckIsUUFBSSxDQUFDLElBQUksR0FBRyxJQUFJLENBQUM7QUFDakIsUUFBSSxJQUFJLENBQUMsT0FBTyxFQUFFLEVBQUU7Ozs7O0FBS2hCLFVBQUksQ0FBQyxTQUFTLEdBQUcsU0FBUyxDQUFDOzs7OztBQUszQixVQUFJLENBQUMsT0FBTyxHQUFHLE9BQU8sQ0FBQztLQUMxQjtBQUNELFFBQUksQ0FBQyxxQkFBcUIsR0FBRyxtQkFBbUIsQ0FBQztHQUNwRDs7ZUEzQ2dCLFNBQVM7QUFrRDFCLFlBQVE7Ozs7Ozs7O2FBQUEsb0JBQUc7QUFDUCxlQUFPLENBQUMsSUFBSSxDQUFDLE9BQU8sSUFBSSxJQUFJLENBQUMsUUFBUSxJQUFJLElBQUksQ0FBQyxRQUFRLENBQUMsS0FBSyxDQUFDLFFBQVEsRUFBRSxDQUFDO09BQzNFOztBQU9ELFdBQU87Ozs7Ozs7O2FBQUEsbUJBQUc7QUFDTixlQUFPLENBQUMsRUFBRSxJQUFJLENBQUMsSUFBSSxHQUFHLFNBQVMsQ0FBQyxLQUFLLENBQUEsQUFBQyxDQUFDO09BQzFDOztBQU9ELFVBQU07Ozs7Ozs7O2FBQUEsa0JBQUc7QUFDTCxlQUFPLENBQUMsRUFBRSxJQUFJLENBQUMsSUFBSSxHQUFHLFNBQVMsQ0FBQyxJQUFJLENBQUEsQUFBQyxDQUFDO09BQ3pDOztBQU9ELGNBQVU7Ozs7Ozs7O2FBQUEsc0JBQUc7QUFDVCxlQUFPLElBQUksQ0FBQyxJQUFJLEtBQUssU0FBUyxDQUFDLElBQUksQ0FBQztPQUN2Qzs7QUFPRCxlQUFXOzs7Ozs7OzthQUFBLHVCQUFHO0FBQ1YsZUFBTyxJQUFJLENBQUMsSUFBSSxLQUFLLFNBQVMsQ0FBQyxLQUFLLENBQUM7T0FDeEM7O0FBT0QsZUFBVzs7Ozs7Ozs7YUFBQSx1QkFBRztBQUNWLGVBQU8sSUFBSSxDQUFDLElBQUksS0FBSyxTQUFTLENBQUMsRUFBRSxDQUFDO09BQ3JDOzs7O1NBakdnQixTQUFTOzs7aUJBQVQsU0FBUzs7Ozs7O0FBd0c5QixTQUFTLENBQUMsSUFBSSxHQUFHLElBQUksQ0FBQzs7Ozs7QUFLdEIsU0FBUyxDQUFDLEtBQUssR0FBRyxLQUFLLENBQUM7Ozs7O0FBS3hCLFNBQVMsQ0FBQyxFQUFFLEdBQUcsRUFBRSxDQUFDIiwiZmlsZSI6InJlZmVyZW5jZS5qcyIsInNvdXJjZXNDb250ZW50IjpbIi8qXG4gIENvcHlyaWdodCAoQykgMjAxNSBZdXN1a2UgU3V6dWtpIDx1dGF0YW5lLnRlYUBnbWFpbC5jb20+XG5cbiAgUmVkaXN0cmlidXRpb24gYW5kIHVzZSBpbiBzb3VyY2UgYW5kIGJpbmFyeSBmb3Jtcywgd2l0aCBvciB3aXRob3V0XG4gIG1vZGlmaWNhdGlvbiwgYXJlIHBlcm1pdHRlZCBwcm92aWRlZCB0aGF0IHRoZSBmb2xsb3dpbmcgY29uZGl0aW9ucyBhcmUgbWV0OlxuXG4gICAgKiBSZWRpc3RyaWJ1dGlvbnMgb2Ygc291cmNlIGNvZGUgbXVzdCByZXRhaW4gdGhlIGFib3ZlIGNvcHlyaWdodFxuICAgICAgbm90aWNlLCB0aGlzIGxpc3Qgb2YgY29uZGl0aW9ucyBhbmQgdGhlIGZvbGxvd2luZyBkaXNjbGFpbWVyLlxuICAgICogUmVkaXN0cmlidXRpb25zIGluIGJpbmFyeSBmb3JtIG11c3QgcmVwcm9kdWNlIHRoZSBhYm92ZSBjb3B5cmlnaHRcbiAgICAgIG5vdGljZSwgdGhpcyBsaXN0IG9mIGNvbmRpdGlvbnMgYW5kIHRoZSBmb2xsb3dpbmcgZGlzY2xhaW1lciBpbiB0aGVcbiAgICAgIGRvY3VtZW50YXRpb24gYW5kL29yIG90aGVyIG1hdGVyaWFscyBwcm92aWRlZCB3aXRoIHRoZSBkaXN0cmlidXRpb24uXG5cbiAgVEhJUyBTT0ZUV0FSRSBJUyBQUk9WSURFRCBCWSBUSEUgQ09QWVJJR0hUIEhPTERFUlMgQU5EIENPTlRSSUJVVE9SUyBcIkFTIElTXCJcbiAgQU5EIEFOWSBFWFBSRVNTIE9SIElNUExJRUQgV0FSUkFOVElFUywgSU5DTFVESU5HLCBCVVQgTk9UIExJTUlURUQgVE8sIFRIRVxuICBJTVBMSUVEIFdBUlJBTlRJRVMgT0YgTUVSQ0hBTlRBQklMSVRZIEFORCBGSVRORVNTIEZPUiBBIFBBUlRJQ1VMQVIgUFVSUE9TRVxuICBBUkUgRElTQ0xBSU1FRC4gSU4gTk8gRVZFTlQgU0hBTEwgPENPUFlSSUdIVCBIT0xERVI+IEJFIExJQUJMRSBGT1IgQU5ZXG4gIERJUkVDVCwgSU5ESVJFQ1QsIElOQ0lERU5UQUwsIFNQRUNJQUwsIEVYRU1QTEFSWSwgT1IgQ09OU0VRVUVOVElBTCBEQU1BR0VTXG4gIChJTkNMVURJTkcsIEJVVCBOT1QgTElNSVRFRCBUTywgUFJPQ1VSRU1FTlQgT0YgU1VCU1RJVFVURSBHT09EUyBPUiBTRVJWSUNFUztcbiAgTE9TUyBPRiBVU0UsIERBVEEsIE9SIFBST0ZJVFM7IE9SIEJVU0lORVNTIElOVEVSUlVQVElPTikgSE9XRVZFUiBDQVVTRUQgQU5EXG4gIE9OIEFOWSBUSEVPUlkgT0YgTElBQklMSVRZLCBXSEVUSEVSIElOIENPTlRSQUNULCBTVFJJQ1QgTElBQklMSVRZLCBPUiBUT1JUXG4gIChJTkNMVURJTkcgTkVHTElHRU5DRSBPUiBPVEhFUldJU0UpIEFSSVNJTkcgSU4gQU5ZIFdBWSBPVVQgT0YgVEhFIFVTRSBPRlxuICBUSElTIFNPRlRXQVJFLCBFVkVOIElGIEFEVklTRUQgT0YgVEhFIFBPU1NJQklMSVRZIE9GIFNVQ0ggREFNQUdFLlxuKi9cblxuY29uc3QgUkVBRCA9IDB4MTtcbmNvbnN0IFdSSVRFID0gMHgyO1xuY29uc3QgUlcgPSBSRUFEIHwgV1JJVEU7XG5cbi8qKlxuICogQSBSZWZlcmVuY2UgcmVwcmVzZW50cyBhIHNpbmdsZSBvY2N1cnJlbmNlIG9mIGFuIGlkZW50aWZpZXIgaW4gY29kZS5cbiAqIEBjbGFzcyBSZWZlcmVuY2VcbiAqL1xuZXhwb3J0IGRlZmF1bHQgY2xhc3MgUmVmZXJlbmNlIHtcbiAgICBjb25zdHJ1Y3RvcihpZGVudCwgc2NvcGUsIGZsYWcsICB3cml0ZUV4cHIsIG1heWJlSW1wbGljaXRHbG9iYWwsIHBhcnRpYWwpIHtcbiAgICAgICAgLyoqXG4gICAgICAgICAqIElkZW50aWZpZXIgc3ludGF4IG5vZGUuXG4gICAgICAgICAqIEBtZW1iZXIge2VzcHJpbWEjSWRlbnRpZmllcn0gUmVmZXJlbmNlI2lkZW50aWZpZXJcbiAgICAgICAgICovXG4gICAgICAgIHRoaXMuaWRlbnRpZmllciA9IGlkZW50O1xuICAgICAgICAvKipcbiAgICAgICAgICogUmVmZXJlbmNlIHRvIHRoZSBlbmNsb3NpbmcgU2NvcGUuXG4gICAgICAgICAqIEBtZW1iZXIge1Njb3BlfSBSZWZlcmVuY2UjZnJvbVxuICAgICAgICAgKi9cbiAgICAgICAgdGhpcy5mcm9tID0gc2NvcGU7XG4gICAgICAgIC8qKlxuICAgICAgICAgKiBXaGV0aGVyIHRoZSByZWZlcmVuY2UgY29tZXMgZnJvbSBhIGR5bmFtaWMgc2NvcGUgKHN1Y2ggYXMgJ2V2YWwnLFxuICAgICAgICAgKiAnd2l0aCcsIGV0Yy4pLCBhbmQgbWF5IGJlIHRyYXBwZWQgYnkgZHluYW1pYyBzY29wZXMuXG4gICAgICAgICAqIEBtZW1iZXIge2Jvb2xlYW59IFJlZmVyZW5jZSN0YWludGVkXG4gICAgICAgICAqL1xuICAgICAgICB0aGlzLnRhaW50ZWQgPSBmYWxzZTtcbiAgICAgICAgLyoqXG4gICAgICAgICAqIFRoZSB2YXJpYWJsZSB0aGlzIHJlZmVyZW5jZSBpcyByZXNvbHZlZCB3aXRoLlxuICAgICAgICAgKiBAbWVtYmVyIHtWYXJpYWJsZX0gUmVmZXJlbmNlI3Jlc29sdmVkXG4gICAgICAgICAqL1xuICAgICAgICB0aGlzLnJlc29sdmVkID0gbnVsbDtcbiAgICAgICAgLyoqXG4gICAgICAgICAqIFRoZSByZWFkLXdyaXRlIG1vZGUgb2YgdGhlIHJlZmVyZW5jZS4gKFZhbHVlIGlzIG9uZSBvZiB7QGxpbmtcbiAgICAgICAgICogUmVmZXJlbmNlLlJFQUR9LCB7QGxpbmsgUmVmZXJlbmNlLlJXfSwge0BsaW5rIFJlZmVyZW5jZS5XUklURX0pLlxuICAgICAgICAgKiBAbWVtYmVyIHtudW1iZXJ9IFJlZmVyZW5jZSNmbGFnXG4gICAgICAgICAqIEBwcml2YXRlXG4gICAgICAgICAqL1xuICAgICAgICB0aGlzLmZsYWcgPSBmbGFnO1xuICAgICAgICBpZiAodGhpcy5pc1dyaXRlKCkpIHtcbiAgICAgICAgICAgIC8qKlxuICAgICAgICAgICAgICogSWYgcmVmZXJlbmNlIGlzIHdyaXRlYWJsZSwgdGhpcyBpcyB0aGUgdHJlZSBiZWluZyB3cml0dGVuIHRvIGl0LlxuICAgICAgICAgICAgICogQG1lbWJlciB7ZXNwcmltYSNOb2RlfSBSZWZlcmVuY2Ujd3JpdGVFeHByXG4gICAgICAgICAgICAgKi9cbiAgICAgICAgICAgIHRoaXMud3JpdGVFeHByID0gd3JpdGVFeHByO1xuICAgICAgICAgICAgLyoqXG4gICAgICAgICAgICAgKiBXaGV0aGVyIHRoZSBSZWZlcmVuY2UgbWlnaHQgcmVmZXIgdG8gYSBwYXJ0aWFsIHZhbHVlIG9mIHdyaXRlRXhwci5cbiAgICAgICAgICAgICAqIEBtZW1iZXIge2Jvb2xlYW59IFJlZmVyZW5jZSNwYXJ0aWFsXG4gICAgICAgICAgICAgKi9cbiAgICAgICAgICAgIHRoaXMucGFydGlhbCA9IHBhcnRpYWw7XG4gICAgICAgIH1cbiAgICAgICAgdGhpcy5fX21heWJlSW1wbGljaXRHbG9iYWwgPSBtYXliZUltcGxpY2l0R2xvYmFsO1xuICAgIH1cblxuICAgIC8qKlxuICAgICAqIFdoZXRoZXIgdGhlIHJlZmVyZW5jZSBpcyBzdGF0aWMuXG4gICAgICogQG1ldGhvZCBSZWZlcmVuY2UjaXNTdGF0aWNcbiAgICAgKiBAcmV0dXJuIHtib29sZWFufVxuICAgICAqL1xuICAgIGlzU3RhdGljKCkge1xuICAgICAgICByZXR1cm4gIXRoaXMudGFpbnRlZCAmJiB0aGlzLnJlc29sdmVkICYmIHRoaXMucmVzb2x2ZWQuc2NvcGUuaXNTdGF0aWMoKTtcbiAgICB9XG5cbiAgICAvKipcbiAgICAgKiBXaGV0aGVyIHRoZSByZWZlcmVuY2UgaXMgd3JpdGVhYmxlLlxuICAgICAqIEBtZXRob2QgUmVmZXJlbmNlI2lzV3JpdGVcbiAgICAgKiBAcmV0dXJuIHtib29sZWFufVxuICAgICAqL1xuICAgIGlzV3JpdGUoKSB7XG4gICAgICAgIHJldHVybiAhISh0aGlzLmZsYWcgJiBSZWZlcmVuY2UuV1JJVEUpO1xuICAgIH1cblxuICAgIC8qKlxuICAgICAqIFdoZXRoZXIgdGhlIHJlZmVyZW5jZSBpcyByZWFkYWJsZS5cbiAgICAgKiBAbWV0aG9kIFJlZmVyZW5jZSNpc1JlYWRcbiAgICAgKiBAcmV0dXJuIHtib29sZWFufVxuICAgICAqL1xuICAgIGlzUmVhZCgpIHtcbiAgICAgICAgcmV0dXJuICEhKHRoaXMuZmxhZyAmIFJlZmVyZW5jZS5SRUFEKTtcbiAgICB9XG5cbiAgICAvKipcbiAgICAgKiBXaGV0aGVyIHRoZSByZWZlcmVuY2UgaXMgcmVhZC1vbmx5LlxuICAgICAqIEBtZXRob2QgUmVmZXJlbmNlI2lzUmVhZE9ubHlcbiAgICAgKiBAcmV0dXJuIHtib29sZWFufVxuICAgICAqL1xuICAgIGlzUmVhZE9ubHkoKSB7XG4gICAgICAgIHJldHVybiB0aGlzLmZsYWcgPT09IFJlZmVyZW5jZS5SRUFEO1xuICAgIH1cblxuICAgIC8qKlxuICAgICAqIFdoZXRoZXIgdGhlIHJlZmVyZW5jZSBpcyB3cml0ZS1vbmx5LlxuICAgICAqIEBtZXRob2QgUmVmZXJlbmNlI2lzV3JpdGVPbmx5XG4gICAgICogQHJldHVybiB7Ym9vbGVhbn1cbiAgICAgKi9cbiAgICBpc1dyaXRlT25seSgpIHtcbiAgICAgICAgcmV0dXJuIHRoaXMuZmxhZyA9PT0gUmVmZXJlbmNlLldSSVRFO1xuICAgIH1cblxuICAgIC8qKlxuICAgICAqIFdoZXRoZXIgdGhlIHJlZmVyZW5jZSBpcyByZWFkLXdyaXRlLlxuICAgICAqIEBtZXRob2QgUmVmZXJlbmNlI2lzUmVhZFdyaXRlXG4gICAgICogQHJldHVybiB7Ym9vbGVhbn1cbiAgICAgKi9cbiAgICBpc1JlYWRXcml0ZSgpIHtcbiAgICAgICAgcmV0dXJuIHRoaXMuZmxhZyA9PT0gUmVmZXJlbmNlLlJXO1xuICAgIH1cbn1cblxuLyoqXG4gKiBAY29uc3RhbnQgUmVmZXJlbmNlLlJFQURcbiAqIEBwcml2YXRlXG4gKi9cblJlZmVyZW5jZS5SRUFEID0gUkVBRDtcbi8qKlxuICogQGNvbnN0YW50IFJlZmVyZW5jZS5XUklURVxuICogQHByaXZhdGVcbiAqL1xuUmVmZXJlbmNlLldSSVRFID0gV1JJVEU7XG4vKipcbiAqIEBjb25zdGFudCBSZWZlcmVuY2UuUldcbiAqIEBwcml2YXRlXG4gKi9cblJlZmVyZW5jZS5SVyA9IFJXO1xuXG4vKiB2aW06IHNldCBzdz00IHRzPTQgZXQgdHc9ODAgOiAqL1xuIl0sInNvdXJjZVJvb3QiOiIvc291cmNlLyJ9
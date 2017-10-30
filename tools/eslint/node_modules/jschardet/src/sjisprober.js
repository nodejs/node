/*
 * The Original Code is Mozilla Universal charset detector code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2001
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   António Afonso (antonio.afonso gmail.com) - port to JavaScript
 *   Mark Pilgrim - port to Python
 *   Shy Shalom - original C code
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301  USA
 */

!function(jschardet) {

jschardet.SJISProber = function() {
    jschardet.MultiByteCharSetProber.apply(this);

    var self = this;

    function init() {
        self._mCodingSM = new jschardet.CodingStateMachine(jschardet.SJISSMModel);
        self._mDistributionAnalyzer = new jschardet.SJISDistributionAnalysis();
        self._mContextAnalyzer = new jschardet.SJISContextAnalysis();
        self.reset();
    }

    this.reset = function() {
        jschardet.SJISProber.prototype.reset.apply(this);
        this._mContextAnalyzer.reset();
    }

    this.getCharsetName = function() {
        return "SHIFT_JIS";
    }

    this.feed = function(aBuf) {
        var aLen = aBuf.length;
        for( var i = 0; i < aLen; i++ ) {
            var codingState = this._mCodingSM.nextState(aBuf[i]);
            if( codingState == jschardet.Constants.error ) {
                if( jschardet.Constants._debug ) {
                    jschardet.log(this.getCharsetName() + " prober hit error at byte " + i + "\n");
                }
                this._mState = jschardet.Constants.notMe;
                break;
            } else if( codingState == jschardet.Constants.itsMe ) {
                this._mState = jschardet.Constants.foundIt;
                break;
            } else if( codingState == jschardet.Constants.start ) {
                var charLen = this._mCodingSM.getCurrentCharLen();
                if( i == 0 ) {
                    this._mLastChar[1] = aBuf[0];
                    this._mContextAnalyzer.feed(this._mLastChar.slice(2 - charLen), charLen);
                    this._mDistributionAnalyzer.feed(this._mLastChar, charLen);
                } else {
                    this._mContextAnalyzer.feed(aBuf.slice(i + 1 - charLen, i + 3 - charLen), charLen);
                    this._mDistributionAnalyzer.feed(aBuf.slice(i - 1, i + 1), charLen);
                }
            }
        }

        this._mLastChar[0] = aBuf[aLen - 1];

        if( this.getState() == jschardet.Constants.detecting ) {
            if( this._mContextAnalyzer.gotEnoughData() &&
                this.getConfidence() > jschardet.Constants.SHORTCUT_THRESHOLD ) {
                this._mState = jschardet.Constants.foundIt;
            }
        }

        return this.getState();
    }

    this.getConfidence = function() {
        var contxtCf = this._mContextAnalyzer.getConfidence();
        var distribCf = this._mDistributionAnalyzer.getConfidence();
        return Math.max(contxtCf, distribCf);
    }

    init();
}
jschardet.SJISProber.prototype = new jschardet.MultiByteCharSetProber();

}(require('./init'));

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

(function() {
    var UDF = 0; // undefined
    var OTH = 1; // other
    jschardet.OTH = 1;
    var ASC = 2; // ascii capital letter
    var ASS = 3; // ascii small letter
    var ACV = 4; // accent capital vowel
    var ACO = 5; // accent capital other
    var ASV = 6; // accent small vowel
    var ASO = 7; // accent small other

    jschardet.Latin1_CharToClass = [
      OTH, OTH, OTH, OTH, OTH, OTH, OTH, OTH,   // 00 - 07
      OTH, OTH, OTH, OTH, OTH, OTH, OTH, OTH,   // 08 - 0F
      OTH, OTH, OTH, OTH, OTH, OTH, OTH, OTH,   // 10 - 17
      OTH, OTH, OTH, OTH, OTH, OTH, OTH, OTH,   // 18 - 1F
      OTH, OTH, OTH, OTH, OTH, OTH, OTH, OTH,   // 20 - 27
      OTH, OTH, OTH, OTH, OTH, OTH, OTH, OTH,   // 28 - 2F
      OTH, OTH, OTH, OTH, OTH, OTH, OTH, OTH,   // 30 - 37
      OTH, OTH, OTH, OTH, OTH, OTH, OTH, OTH,   // 38 - 3F
      OTH, ASC, ASC, ASC, ASC, ASC, ASC, ASC,   // 40 - 47
      ASC, ASC, ASC, ASC, ASC, ASC, ASC, ASC,   // 48 - 4F
      ASC, ASC, ASC, ASC, ASC, ASC, ASC, ASC,   // 50 - 57
      ASC, ASC, ASC, OTH, OTH, OTH, OTH, OTH,   // 58 - 5F
      OTH, ASS, ASS, ASS, ASS, ASS, ASS, ASS,   // 60 - 67
      ASS, ASS, ASS, ASS, ASS, ASS, ASS, ASS,   // 68 - 6F
      ASS, ASS, ASS, ASS, ASS, ASS, ASS, ASS,   // 70 - 77
      ASS, ASS, ASS, OTH, OTH, OTH, OTH, OTH,   // 78 - 7F
      OTH, UDF, OTH, ASO, OTH, OTH, OTH, OTH,   // 80 - 87
      OTH, OTH, ACO, OTH, ACO, UDF, ACO, UDF,   // 88 - 8F
      UDF, OTH, OTH, OTH, OTH, OTH, OTH, OTH,   // 90 - 97
      OTH, OTH, ASO, OTH, ASO, UDF, ASO, ACO,   // 98 - 9F
      OTH, OTH, OTH, OTH, OTH, OTH, OTH, OTH,   // A0 - A7
      OTH, OTH, OTH, OTH, OTH, OTH, OTH, OTH,   // A8 - AF
      OTH, OTH, OTH, OTH, OTH, OTH, OTH, OTH,   // B0 - B7
      OTH, OTH, OTH, OTH, OTH, OTH, OTH, OTH,   // B8 - BF
      ACV, ACV, ACV, ACV, ACV, ACV, ACO, ACO,   // C0 - C7
      ACV, ACV, ACV, ACV, ACV, ACV, ACV, ACV,   // C8 - CF
      ACO, ACO, ACV, ACV, ACV, ACV, ACV, OTH,   // D0 - D7
      ACV, ACV, ACV, ACV, ACV, ACO, ACO, ACO,   // D8 - DF
      ASV, ASV, ASV, ASV, ASV, ASV, ASO, ASO,   // E0 - E7
      ASV, ASV, ASV, ASV, ASV, ASV, ASV, ASV,   // E8 - EF
      ASO, ASO, ASV, ASV, ASV, ASV, ASV, OTH,   // F0 - F7
      ASV, ASV, ASV, ASV, ASV, ASO, ASO, ASO    // F8 - FF
    ];

    // 0 : illegal
    // 1 : very unlikely
    // 2 : normal
    // 3 : very likely
    jschardet.Latin1ClassModel = [
    // UDF OTH ASC ASS ACV ACO ASV ASO
       0,  0,  0,  0,  0,  0,  0,  0,  // UDF
       0,  3,  3,  3,  3,  3,  3,  3,  // OTH
       0,  3,  3,  3,  3,  3,  3,  3,  // ASC
       0,  3,  3,  3,  1,  1,  3,  3,  // ASS
       0,  3,  3,  3,  1,  2,  1,  2,  // ACV
       0,  3,  3,  3,  3,  3,  3,  3,  // ACO
       0,  3,  1,  3,  1,  1,  1,  3,  // ASV
       0,  3,  1,  3,  1,  1,  3,  3   // ASO
    ];
})();

jschardet.Latin1Prober = function() {
    jschardet.CharSetProber.apply(this);

    var FREQ_CAT_NUM = 4;
    var CLASS_NUM = 8; // total classes
    var self = this;

    function init() {
        self.reset();
    }

    this.reset = function() {
        this._mLastCharClass = jschardet.OTH;
        this._mFreqCounter = [];
        for( var i = 0; i < FREQ_CAT_NUM; this._mFreqCounter[i++] = 0 );
        jschardet.Latin1Prober.prototype.reset.apply(this);
    }

    this.getCharsetName = function() {
        return "windows-1252";
    }

    this.feed = function(aBuf) {
        aBuf = this.filterWithEnglishLetters(aBuf);
        for( var i = 0; i < aBuf.length; i++ ) {
            var c = aBuf.charCodeAt(i);
            var charClass = jschardet.Latin1_CharToClass[c];
            var freq = jschardet.Latin1ClassModel[(this._mLastCharClass * CLASS_NUM) + charClass];
            if( freq == 0 ) {
                this._mState = jschardet.Constants.notMe;
                break;
            }
            this._mFreqCounter[freq]++;
            this._mLastCharClass = charClass;
        }

        return this.getState();
    }

    this.getConfidence = function() {
        var confidence;
        var constants;
        
        if( this.getState() == jschardet.Constants.notMe ) {
            return 0.01;
        }

        var total = 0;
        for( var i = 0; i < this._mFreqCounter.length; i++ ) {
            total += this._mFreqCounter[i];
        }
        if( total < 0.01 ) {
            constants = 0.0;
        } else {
            confidence = (this._mFreqCounter[3] / total) - (this._mFreqCounter[1] * 20 / total);
        }
        if( confidence < 0 ) {
            confidence = 0.0;
        }
        // lower the confidence of latin1 so that other more accurate detector
        // can take priority.
        //
        // antonio.afonso: need to change this otherwise languages like pt, es, fr using latin1 will never be detected.
        confidence = confidence * 0.95;
        return confidence;
    }

    init();
}
jschardet.Latin1Prober.prototype = new jschardet.CharSetProber();

}(require('./init'));

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

jschardet.SBCSGroupProber = function() {
    jschardet.CharSetGroupProber.apply(this);

    var self = this;

    function init() {
        self._mProbers = [
            new jschardet.SingleByteCharSetProber(jschardet.Win1251CyrillicModel),
            new jschardet.SingleByteCharSetProber(jschardet.Koi8rModel),
            new jschardet.SingleByteCharSetProber(jschardet.Latin5CyrillicModel),
            new jschardet.SingleByteCharSetProber(jschardet.MacCyrillicModel),
            new jschardet.SingleByteCharSetProber(jschardet.Ibm866Model),
            new jschardet.SingleByteCharSetProber(jschardet.Ibm855Model),
            new jschardet.SingleByteCharSetProber(jschardet.Latin7GreekModel),
            new jschardet.SingleByteCharSetProber(jschardet.Win1253GreekModel),
            new jschardet.SingleByteCharSetProber(jschardet.Latin5BulgarianModel),
            new jschardet.SingleByteCharSetProber(jschardet.Win1251BulgarianModel),
            new jschardet.SingleByteCharSetProber(jschardet.Latin2HungarianModel),
            new jschardet.SingleByteCharSetProber(jschardet.Win1250HungarianModel),
            new jschardet.SingleByteCharSetProber(jschardet.TIS620ThaiModel)
        ];
        var hebrewProber = new jschardet.HebrewProber();
        var logicalHebrewProber = new jschardet.SingleByteCharSetProber(jschardet.Win1255HebrewModel, false, hebrewProber);
        var visualHebrewProber = new jschardet.SingleByteCharSetProber(jschardet.Win1255HebrewModel, true, hebrewProber);
        hebrewProber.setModelProbers(logicalHebrewProber, visualHebrewProber);
        self._mProbers.push(hebrewProber, logicalHebrewProber, visualHebrewProber);

        self.reset();
    }

    init();
}
jschardet.SBCSGroupProber.prototype = new jschardet.CharSetGroupProber();

}(require('./init'));

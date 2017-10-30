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

var jschardet = exports;

require('./constants');
require('./codingstatemachine');
require('./escsm');
require('./mbcssm');
require('./charsetprober');
require('./mbcharsetprober');
require('./jisfreq');
require('./gb2312freq');
require('./euckrfreq');
require('./big5freq');
require('./euctwfreq');
require('./chardistribution');
require('./jpcntx');
require('./sjisprober');
require('./utf8prober');
require('./charsetgroupprober');
require('./eucjpprober');
require('./gb2312prober');
require('./euckrprober');
require('./big5prober');
require('./euctwprober');
require('./mbcsgroupprober');
require('./sbcharsetprober');
require('./langgreekmodel');
require('./langthaimodel');
require('./langbulgarianmodel');
require('./langcyrillicmodel');
require('./hebrewprober');
require('./langhebrewmodel');
require('./langhungarianmodel');
require('./sbcsgroupprober');
require('./latin1prober');
require('./escprober');
require('./universaldetector');

jschardet.VERSION = "1.4.1";
jschardet.detect = function(buffer) {
    var u = new jschardet.UniversalDetector();
    u.reset();
    if( typeof Buffer == 'function' && buffer instanceof Buffer ) {
        u.feed(buffer.toString('binary'));
    } else {
        u.feed(buffer);
    }
    u.close();
    return u.result;
}
jschardet.log = function() {
  console.log.apply(console, arguments);
}

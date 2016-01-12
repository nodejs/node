/*
 Copyright (C) 2007 Apple Inc.  All rights reserved.

 Redistribution and use in source and binary forms, with or without
 modification, are permitted provided that the following conditions
 are met:
 1. Redistributions of source code must retain the above copyright
    notice, this list of conditions and the following disclaimer.
 2. Redistributions in binary form must reproduce the above copyright
    notice, this list of conditions and the following disclaimer in the
    documentation and/or other materials provided with the distribution.

 THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
*/

if(typeof(WScript) === "undefined")
{
    var WScript = {
        Echo: print
    }
}

function record(time) {
    document.getElementById("console").innerHTML = time + "ms";
    if (window.parent) {
        parent.recordResult(time);
    }
}

var data =
"{\"summary\":{\"turnover\":0.3736,\"correlation2\":0." +
"7147,\"concentration\":0.3652,\"beta\":0.8814,\"totalValue\":1.3" +
"091078259E8,\"correlation\":0.7217},\"watchlist\":[],\"shortCash" +
"\":-1611000,\"holdings\":[{\"type\":\"LONG\",\"commission\":1040" +
",\"cost\":9001920,\"quantity\":26000,\"lots\":[{\"marketCap\":\"" +
"L\",\"industry\":\"TECHNOLOGY\",\"style\":\"G\",\"buyDate\":\"20" +
"08-10-08 13:44:20.000\",\"quantity\":8000},{\"marketCap\":\"L\"," +
"\"industry\":\"TECHNOLOGY\",\"style\":\"G\",\"buyDate\":\"2008-1" +
"0-15 13:28:02.000\",\"quantity\":18000}],\"stock\":\"GOOG\"},{\"" +
"type\":\"LONG\",\"commission\":8000,\"cost\":4672000,\"quantity" +
"\":200000,\"lots\":[{\"marketCap\":\"L\",\"industry\":\"TECHNOLO" +
"GY\",\"style\":\"G\",\"buyDate\":\"2008-10-15 13:28:54.000\",\"q" +
"uantity\":200000}],\"stock\":\"MSFT\"},{\"type\":\"LONG\",\"comm" +
"ission\":21877,\"cost\":1.001592313E7,\"quantity\":546919,\"lots" +
"\":[{\"marketCap\":\"L\",\"industry\":\"FINANCIAL\",\"style\":\"" +
"G\",\"buyDate\":\"2008-08-01 09:50:17.000\",\"quantity\":103092}" +
",{\"marketCap\":\"L\",\"industry\":\"FINANCIAL\",\"style\":\"G\"" +
",\"buyDate\":\"2008-08-18 10:31:34.000\",\"quantity\":49950},{\"" +
"marketCap\":\"L\",\"industry\":\"FINANCIAL\",\"style\":\"G\",\"b" +
"uyDate\":\"2008-08-29 09:35:22.000\",\"quantity\":45045},{\"mark" +
"etCap\":\"L\",\"industry\":\"FINANCIAL\",\"style\":\"G\",\"buyDa" +
"te\":\"2008-09-15 09:40:32.000\",\"quantity\":48400},{\"marketCa" +
"p\":\"L\",\"industry\":\"FINANCIAL\",\"style\":\"G\",\"buyDate\"" +
":\"2008-10-06 11:21:50.000\",\"quantity\":432},{\"marketCap\":\"" +
"L\",\"industry\":\"FINANCIAL\",\"style\":\"G\",\"buyDate\":\"200" +
"8-10-15 13:30:05.000\",\"quantity\":300000}],\"stock\":\"UBS\"}," +
"{\"type\":\"LONG\",\"commission\":4000,\"cost\":6604849.1,\"quan" +
"tity\":122741,\"lots\":[{\"marketCap\":\"L\",\"industry\":\"SERV" +
"ICES\",\"style\":\"V\",\"buyDate\":\"2008-04-26 04:44:34.000\"," +
"\"quantity\":22741},{\"marketCap\":\"L\",\"industry\":\"SERVICES" +
"\",\"style\":\"V\",\"buyDate\":\"2008-10-15 13:31:02.000\",\"qua" +
"ntity\":100000}],\"stock\":\"V\"},{\"type\":\"LONG\",\"commissio" +
"n\":2805,\"cost\":5005558.25,\"quantity\":70121,\"lots\":[{\"mar" +
"ketCap\":\"M\",\"industry\":\"RETAIL\",\"style\":\"G\",\"buyDate" +
"\":\"2008-10-10 10:48:36.000\",\"quantity\":121},{\"marketCap\":" +
"\"M\",\"industry\":\"RETAIL\",\"style\":\"G\",\"buyDate\":\"2008" +
"-10-15 13:33:44.000\",\"quantity\":70000}],\"stock\":\"LDG\"},{" +
"\"type\":\"LONG\",\"commission\":10000,\"cost\":5382500,\"quanti" +
"ty\":250000,\"lots\":[{\"marketCap\":\"L\",\"industry\":\"RETAIL" +
"\",\"style\":\"V\",\"buyDate\":\"2008-10-15 13:34:30.000\",\"qua" +
"ntity\":250000}],\"stock\":\"SWY\"},{\"type\":\"LONG\",\"commiss" +
"ion\":1120,\"cost\":1240960,\"quantity\":28000,\"lots\":[{\"mark" +
"etCap\":\"u\",\"industry\":\"ETF\",\"style\":\"B\",\"buyDate\":" +
"\"2008-10-15 15:57:39.000\",\"quantity\":28000}],\"stock\":\"OIL" +
"\"},{\"type\":\"LONG\",\"commission\":400,\"cost\":236800,\"quan" +
"tity\":10000,\"lots\":[{\"marketCap\":\"M\",\"industry\":\"UTILI" +
"TIES_AND_ENERGY\",\"style\":\"G\",\"buyDate\":\"2008-10-15 15:58" +
":03.000\",\"quantity\":10000}],\"stock\":\"COG\"},{\"type\":\"LO" +
"NG\",\"commission\":3200,\"cost\":1369600,\"quantity\":80000,\"l" +
"ots\":[{\"marketCap\":\"S\",\"industry\":\"UTILITIES_AND_ENERGY" +
"\",\"style\":\"G\",\"buyDate\":\"2008-10-15 15:58:32.000\",\"qua" +
"ntity\":80000}],\"stock\":\"CRZO\"},{\"type\":\"LONG\",\"commiss" +
"ion\":429,\"cost\":108164.8,\"quantity\":10720,\"lots\":[{\"mark" +
"etCap\":\"u\",\"industry\":\"FINANCIAL\",\"style\":\"V\",\"buyDa" +
"te\":\"2008-10-16 09:37:06.000\",\"quantity\":10720}],\"stock\":" +
"\"FGI\"},{\"type\":\"LONG\",\"commission\":1080,\"cost\":494910," +
"\"quantity\":27000,\"lots\":[{\"marketCap\":\"L\",\"industry\":" +
"\"RETAIL\",\"style\":\"V\",\"buyDate\":\"2008-10-16 09:37:06.000" +
"\",\"quantity\":27000}],\"stock\":\"LOW\"},{\"type\":\"LONG\",\"" +
"commission\":4080,\"cost\":4867440,\"quantity\":102000,\"lots\":" +
"[{\"marketCap\":\"L\",\"industry\":\"HEALTHCARE\",\"style\":\"V" +
"\",\"buyDate\":\"2008-10-16 09:37:06.000\",\"quantity\":102000}]" +
",\"stock\":\"AMGN\"},{\"type\":\"SHORT\",\"commission\":4000,\"" +
"cost\":-1159000,\"quantity\":-100000,\"lots\":[{\"marketCap\":" +
"\"L\",\"industry\":\"TECHNOLOGY\",\"style\":\"V\",\"buyDate\":" +
"\"2008-10-16 09:37:06.000\",\"quantity\":-100000}],\"stock\":\"" +
"AMAT\"},{\"type\":\"LONG\",\"commission\":2,\"cost\":5640002,\"" +
"quantity\":50,\"lots\":[{\"marketCap\":\"L\",\"industry\":\"FIN" +
"ANCIAL\",\"style\":\"B\",\"buyDate\":\"2008-10-16 09:37:06.000" +
"\",\"quantity\":50}],\"stock\":\"BRKA\"},{\"type\":\"SHORT\",\"" +
"commission\":4000,\"cost\":-436000,\"quantity\":-100000,\"lots" +
"\":[{\"marketCap\":\"M\",\"industry\":\"TRANSPORTATION\",\"styl" +
"e\":\"G\",\"buyDate\":\"2008-10-16 09:37:06.000\",\"quantity\":-" +
"100000}],\"stock\":\"JBLU\"},{\"type\":\"LONG\",\"commission\":8" +
"000,\"cost\":1.1534E7,\"quantity\":200000,\"lots\":[{\"marketCap" +
"\":\"S\",\"industry\":\"FINANCIAL\",\"style\":\"G\",\"buyDate\":" +
"\"2008-10-16 14:35:24.000\",\"quantity\":200000}],\"stock\":\"US" +
"O\"},{\"type\":\"LONG\",\"commission\":4000,\"cost\":1.0129E7,\"" +
"quantity\":100000,\"lots\":[{\"marketCap\":\"L\",\"industry\":\"" +
"TECHNOLOGY\",\"style\":\"G\",\"buyDate\":\"2008-10-15 13:28:26.0" +
"00\",\"quantity\":50000},{\"marketCap\":\"L\",\"industry\":\"TEC" +
"HNOLOGY\",\"style\":\"G\",\"buyDate\":\"2008-10-17 09:33:09.000" +
"\",\"quantity\":50000}],\"stock\":\"AAPL\"},{\"type\":\"LONG\"," +
"\"commission\":1868,\"cost\":9971367.2,\"quantity\":54280,\"lots" +
"\":[{\"marketCap\":\"L\",\"industry\":\"SERVICES\",\"style\":\"G" +
"\",\"buyDate\":\"2008-04-26 04:44:34.000\",\"quantity\":7580},{" +
"\"marketCap\":\"L\",\"industry\":\"SERVICES\",\"style\":\"G\",\"" +
"buyDate\":\"2008-05-29 09:50:28.000\",\"quantity\":7500},{\"mark" +
"etCap\":\"L\",\"industry\":\"SERVICES\",\"style\":\"G\",\"buyDat" +
"e\":\"2008-10-15 13:30:38.000\",\"quantity\":33000},{\"marketCap" +
"\":\"L\",\"industry\":\"SERVICES\",\"style\":\"G\",\"buyDate\":" +
"\"2008-10-17 09:33:09.000\",\"quantity\":6200}],\"stock\":\"MA\"" +
"}],\"longCash\":4.600368106E7,\"ownerId\":8,\"pendingOrders\":[{" +
"\"total\":487000,\"type\":\"cover\",\"subtotal\":483000,\"price" +
"\":4.83,\"commission\":4000,\"date\":\"2008-10-17 23:56:06.000\"" +
",\"quantity\":100000,\"expires\":\"2008-10-20 16:00:00.000\",\"s" +
"tock\":\"JBLU\",\"id\":182375},{\"total\":6271600,\"type\":\"buy" +
"\",\"subtotal\":6270000,\"price\":156.75,\"commission\":1600,\"d" +
"ate\":\"2008-10-17 23:56:40.000\",\"quantity\":40000,\"expires\"" +
":\"2008-10-20 16:00:00.000\",\"stock\":\"MA\",\"id\":182376}],\"" +
"inceptionDate\":\"2008-04-26 04:44:29.000\",\"withdrawals\":0,\"" +
"id\":219948,\"deposits\":0}";


var _sunSpiderStartDate = new Date();


for (var i = 0; i < 1000; i++) {
  var x = JSON.parse(data);
}

var _sunSpiderInterval = new Date() - _sunSpiderStartDate;

WScript.Echo("### TIME:", _sunSpiderInterval, "ms");

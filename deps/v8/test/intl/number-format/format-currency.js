// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Make sure currency formatting is correct (for USD only displays two decimal
// places, for JPY 0, and for EUR 2).

var nf_USD = new Intl.NumberFormat(['en'], {style: 'currency', currency: 'USD'});

assertEquals("$54,306.40", nf_USD.format(parseFloat(54306.4047970)));

var nf_JPY = new Intl.NumberFormat(['ja'],
    {style: 'currency', currency: 'JPY', currencyDisplay: "code"});

assertEquals("JPY54,306", nf_JPY.format(parseFloat(54306.4047970)));

var nf_EUR = new Intl.NumberFormat(['pt'], {style: 'currency', currency: 'EUR'});

assertEquals("€\u00a01.000,00", nf_EUR.format(1000.00));

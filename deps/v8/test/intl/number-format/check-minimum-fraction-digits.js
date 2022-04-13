// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Make sure minimumFractionDigits and maximumFractionDigits are honored

var nf = new Intl.NumberFormat("en-us", { useGrouping: false, minimumFractionDigits: 4, maximumFractionDigits: 8});

assertEquals("12345.6789", nf.format(12345.6789));
assertEquals("12345.678912", nf.format(12345.678912));
assertEquals("12345.6700", nf.format(12345.67));
assertEquals("12345.67891234", nf.format(12345.6789123421));

nf = new Intl.NumberFormat("en-us", { useGrouping: false, minimumFractionDigits: 4, maximumFractionDigits: 8, style: 'percent'});

assertEquals("12345.6789%", nf.format(123.456789));
assertEquals("12345.678912%", nf.format(123.45678912));
assertEquals("12345.6700%", nf.format(123.4567));
assertEquals("12345.67891234%", nf.format(123.456789123421));

nf = new Intl.NumberFormat('en', {minimumFractionDigits: 4, maximumFractionDigits: 8, style: 'currency', currency: 'USD'});

assertEquals("$54,306.404797", nf.format(54306.4047970));
assertEquals("$54,306.4000", nf.format(54306.4));
assertEquals("$54,306.40000001", nf.format(54306.400000011));

// Ensure that appropriate defaults exist when minimum and maximum are not specified

nf = new Intl.NumberFormat("en-us", { useGrouping: false });

assertEquals("12345.679", nf.format(12345.6789));
assertEquals("12345.679", nf.format(12345.678912));
assertEquals("12345.67", nf.format(12345.6700));
assertEquals("12345", nf.format(12345));
assertEquals("12345.679", nf.format(12345.6789123421));

nf = new Intl.NumberFormat("en-us", { useGrouping: false, style: 'percent'});

assertEquals("12346%", nf.format(123.456789));
assertEquals("12346%", nf.format(123.45678912));
assertEquals("12346%", nf.format(123.456700));
assertEquals("12346%", nf.format(123.456789123421));
assertEquals("12345%", nf.format(123.45));

// For currency, the minimum or the maximum can be overwritten individually

nf = new Intl.NumberFormat('en', {minimumFractionDigits: 0, style: 'currency', currency: 'USD'});

assertEquals("$54,306.4", nf.format(54306.4047970));
assertEquals("$54,306.4", nf.format(54306.4));
assertEquals("$54,306", nf.format(54306));

nf = new Intl.NumberFormat('en', {maximumFractionDigits: 3, style: 'currency', currency: 'USD'});

assertEquals("$54,306.405", nf.format(54306.4047970));
assertEquals("$54,306.40", nf.format(54306.4));
assertEquals("$54,306.00", nf.format(54306));

nf = new Intl.NumberFormat('en', {maximumFractionDigits: 0, style: 'currency', currency: 'USD'});

assertEquals("$54,306", nf.format(54306.4047970));
assertEquals("$54,306", nf.format(54306.4));
assertEquals("$54,306", nf.format(54306));

assertThrows(() => new Intl.NumberFormat('en',
      {minimumFractionDigits: 1, maximumFractionDigits: 0, style: 'currency', currency: 'USD'}));

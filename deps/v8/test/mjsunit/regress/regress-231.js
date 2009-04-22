// Copyright 2009 the V8 project authors. All rights reserved.
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * Redistributions of source code must retain the above copyright
//       notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
//       copyright notice, this list of conditions and the following
//       disclaimer in the documentation and/or other materials provided
//       with the distribution.
//     * Neither the name of Google Inc. nor the names of its
//       contributors may be used to endorse or promote products derived
//       from this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

// See issue 231 <URL: http://code.google.com/p/v8/issues/detail?id=231 >
// A stack growth during a look-ahead could restore a pointer to the old stack.
// (Test derived from crash at ibs.blumex.com).

var re = /Ggcy\b[^D]*D((?:(?=([^G]+))\2|G(?!gcy\b[^D]*D))*?)GIgcyD/;

var str = 'GgcyDGgcy.saaaa.aDGaaa.aynaaaaaaaaacaaaaagcaaaaaaaancaDGgnayr' +
    '.aryycnaaaataaaa.aryyacnaaataaaa.aaaaraaaaa.aaagaaaaaaaaDGgaaaaDGga' +
    '.aaagaaaaaaaaDGga.nyataaaaragraa.anyataaagaca.agayraaarataga.aaacaa' +
    '.aaagaa.aaacaaaDGaaa.aynaaaaaaaaacaaaaagcaaaaaacaagaa.agayraaaGgaaa' +
    '.trgaaaaaagaatGanyara.caagaaGaD.araaaa_aat_aayDDaaDGaaa.aynaaaaaaaa' +
    'acaaaaagcaaaaaacaaaaa.agayraaaGgaaa.trgaaaaaaatGanyaraDDaaDGacna.ay' +
    'naaaaaaaaacaaaaagcaaaaaacaaaraGgaaa.naaaaagaaaaaaraynaaGanyaraDDaaD' +
    'aGgaaa.saaangaaaaraaaGgaaa.trgaaaragaaaarGanyaraDDDaGIacnaDGIaaaDGI' +
    'aaaDGIgaDGga.anyataaagaca.agayraaaaagaa.aaaaa.cnaaaata.aca.aca.aca.' +
    'acaaaDGgnayr.aaaaraaaaa.aaagaaaaaaaaDGgaaaaDGgaDGga.aayacnaaaaa.ayn' +
    'aaaaaaaaacaaaaagcaaaaaanaraDGaDacaaaaag_anaraGIaDGIgaDGIgaDGgaDGga.' +
    'aayacnaaaaa.aaagaaaaaaaaDGaa.aaagaaaaaaaa.aaaraaaa.aaaanaraaaa.IDGI' +
    'gaDGIgaDGgaDGga.aynaaaaaaaaacaaaaagcaaaaaaaraaaa.anyataaagacaDaGgaa' +
    'a.trgGragaaaacgGaaaaaaaG_aaaaa_Gaaaaaaaaa,.aGanar.anaraDDaaGIgaDGga' +
    '.aynaaaaaaaaacaaaaagcaaaaaaanyara.anyataaagacaDGaDaaaaag_caaaaag_an' +
    'araGIaDGIgaDGIgaDGgaDGga.aynaaaaaaaaacaaaaagcaaaaaaaraaaa.anyataaag' +
    'acaDaGgaaa.trgGragaaaacgGaaaaaaaG_aaaaa_aaaaaa,.aaaaacaDDaaGIgaDGga' +
    '.aynaaaaaaaaacaaaaagcaaaaaaanyara.anyataaagacaDGaDataaac_araaaaGIaD' +
    'GIgaDGIgaDaagcyaaGgaDGga.aayacnaaaaa.aaagaaaaaaaaDGaa.aaagaaaaaaaa.' +
    'aaaraaaa.aaaanaraaaa.IDGIgaDGIgaDGgcy.asaadanyaga.aa.aaaDGgaDGga.ay' +
    'naaaaaaaaacaaaaagcaaaaaaaraaaa.anyataaagacaDaGgaaa.trgGragaaaacgGaa' +
    'aaaaaG_aaaaa_DaaaaGaa,.aDanyagaaDDaaGIgaDGga.aynaaaaaaaaacaaaaagcaa' +
    'aaaaanyara.anyataaagacaDGaDadanyagaaGIaDGIgaDGIgaDGIgcyDGgcy.asaaga' +
    'cras.agra_yratga.aa.aaaarsaaraa.aa.agra_yratga.aa.aaaDGgaDGga.aynaa' +
    'aaaaaaacaaaaagcaaaaaaaraaaa.anyataaagacaDaGgaaa.trgGragaaaacgGaaaaa' +
    'aaG_aaaaa_aGaaaaaaGaa,.aaratgaaDDaaGIgaDGga.aynaaaaaaaaacaaaaagcaaa' +
    'aaaanyara.anyataaagacaDGaDaagra_yratgaaGIaDGIgaDGIgaDGIgcyDGgcy.asa' +
    'agacras.aratag.aa.aaaarsaaraa.aa.aratag.aa.aaaDGgaDGga.aynaaaaaaaaa' +
    'caaaaagcaaaaaaaraaaa.anyataaagacaDaGgaaa.trgGragaaaacgGaaaaaaaG_aaa' +
    'aa_aaaaaGa,.aaratagaDDaaGIgaDGga.aynaaaaaaaaacaaaaagcaaaaaaanyara.a' +
    'nyataaagacaDGaDaaratagaGIaDGIgaDGIgaDGIgcyDGgcy.asaagacras.gaaax_ar' +
    'atag.aa.aaaarsaaraa.aa.gaaax_aratag.aa.aaaDGgaDGga.aynaaaaaaaaacaaa' +
    'aagcaaaaaaaraaaa.anyataaagacaDaGgaaa.trgGragaaaacgGaaaaaaaG_aaaaa_G' +
    'aaaaaaaaaGa,.aaratagaDDaaGIgaDGga.aynaaaaaaaaacaaaaagcaaaaaaanyara.' +
    'anyataaagacaDGaDagaaax_aratagaGIaDGIgaDGIgaDGIgcyDGgcy.asaagacras.c' +
    'ag_aaar.aa.aaaarsaaraa.aa.cag_aaar.aa.aaaDGgaDGga.aynaaaaaaaaacaaaa' +
    'agcaaaaaaaraaaa.anyataaagacaDaGgaaa.trgGragaaaacgGaaaaaaaG_aaaaa_aa' +
    'Gaaaaa,.aaagaaaraDDaaGIgaDGga.aynaaaaaaaaacaaaaagcaaaaaaanyara.anya' +
    'taaagacaDGaDacag_aaaraGIaDGIgaDGIgaDGIgcyDGgcy.asaagacras.aaggaata_' +
    'aa_cynaga_cc.aa.aaaarsaaraa.aa.aaggaata_aa_cynaga_cc.aa.aaaDGgaDGga' +
    '.aynaaaaaaaaacaaaaagcaaaaaaaraaaa.anyataaagacaDaGgaaa.trgGragaaaacg' +
    'GaaaaaaaG_aaaaa_aaGGaaaa_aa_aaaaGa_aaa,.aaynagaIcagaDDaaGIgaDGga.ay' +
    'naaaaaaaaacaaaaagcaaaaaaanyara.anyataaagacaDGaDaaaggaata_aa_cynaga_' +
    'ccaGIaDGIgaDGIgaDGIgcyDGgcy.asaagacras.syaara_aanargra.aa.aaaarsaar' +
    'aa.aa.syaara_aanargra.aa.aaaDGgaDGga.aynaaaaaaaaacaaaaagcaaaaaaaraa' +
    'aa.anyataaagacaDaGgaaa.trgGragaaaacgGaaaaaaaG_aaaaa_aaaaaaaaaaaGaaa' +
    ',.aaanargraaDDaaGIgaDGga.aynaaaaaaaaacaaaaagcaaaaaaanyara.anyataaag' +
    'acaDGaDasyaara_aanargraaGIaDGIgaDGIgaDGIgcyDGgcy.asaagacras.cynag_a' +
    'anargra.aa.aaaarsaaraa.aa.cynag_aanargra.aa.aaaDGgaDGga.aynaaaaaaaa' +
    'acaaaaagcaaaaaaaraaaa.anyataaagacaDaGgaaa.trgGragaaaacgGaaaaaaaG_aa' +
    'aaa_aaaaGaaaaaGaaa,.aaanargraaDDaaGIgaDGga.aynaaaaaaaaacaaaaagcaaaa' +
    'aaanyara.anyataaagacaDGaDacynag_aanargraaGIaDGIgaDGIgaDGIgcyDGgaDGg' +
    'a.aynaaaaaaaaacaaaaagcaaaaaaaraaaa.anyataaagacaDaGgaaa.trgGragaaaac' +
    'gGaaaaaaaG';

//Shouldn't crash.

var res = re.test(str);
assertTrue(res);
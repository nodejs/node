// Protocol Buffers - Google's data interchange format
// Copyright 2008 Google Inc.  All rights reserved.
// https://developers.google.com/protocol-buffers/
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//     * Neither the name of Google Inc. nor the names of its
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
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

// Author: kenton@google.com (Kenton Varda)

#include <google/protobuf/stubs/strutil.h>

#include <locale.h>

#include <google/protobuf/stubs/stl_util.h>
#include <google/protobuf/testing/googletest.h>
#include <gtest/gtest.h>

#ifdef _WIN32
#define snprintf _snprintf
#endif

namespace google {
namespace protobuf {
namespace {

// TODO(kenton):  Copy strutil tests from google3?

TEST(StringUtilityTest, ImmuneToLocales) {
  // Remember the old locale.
  char* old_locale_cstr = setlocale(LC_NUMERIC, nullptr);
  ASSERT_TRUE(old_locale_cstr != nullptr);
  string old_locale = old_locale_cstr;

  // Set the locale to "C".
  ASSERT_TRUE(setlocale(LC_NUMERIC, "C") != nullptr);

  EXPECT_EQ("1.5", SimpleDtoa(1.5));
  EXPECT_EQ("1.5", SimpleFtoa(1.5));

  if (setlocale(LC_NUMERIC, "es_ES") == nullptr &&
      setlocale(LC_NUMERIC, "es_ES.utf8") == nullptr) {
    // Some systems may not have the desired locale available.
    GOOGLE_LOG(WARNING)
      << "Couldn't set locale to es_ES.  Skipping this test.";
  } else {
    EXPECT_EQ("1.5", SimpleDtoa(1.5));
    EXPECT_EQ("1.5", SimpleFtoa(1.5));
  }

  // Return to original locale.
  setlocale(LC_NUMERIC, old_locale.c_str());
}

#define EXPECT_EQ_ARRAY(len, x, y, msg)                     \
  for (int j = 0; j < len; ++j) {                           \
    EXPECT_EQ(x[j], y[j]) << "" # x << " != " # y           \
                          << " byte " << j << ": " << msg;  \
  }

static struct {
  int plain_length;
  const char* plaintext;
  const char* cyphertext;
} base64_tests[] = {
  // Empty string.
  { 0, "", ""},

  // Basic bit patterns;
  // values obtained with "echo -n '...' | uuencode -m test"

  { 1, "\000", "AA==" },
  { 1, "\001", "AQ==" },
  { 1, "\002", "Ag==" },
  { 1, "\004", "BA==" },
  { 1, "\010", "CA==" },
  { 1, "\020", "EA==" },
  { 1, "\040", "IA==" },
  { 1, "\100", "QA==" },
  { 1, "\200", "gA==" },

  { 1, "\377", "/w==" },
  { 1, "\376", "/g==" },
  { 1, "\375", "/Q==" },
  { 1, "\373", "+w==" },
  { 1, "\367", "9w==" },
  { 1, "\357", "7w==" },
  { 1, "\337", "3w==" },
  { 1, "\277", "vw==" },
  { 1, "\177", "fw==" },
  { 2, "\000\000", "AAA=" },
  { 2, "\000\001", "AAE=" },
  { 2, "\000\002", "AAI=" },
  { 2, "\000\004", "AAQ=" },
  { 2, "\000\010", "AAg=" },
  { 2, "\000\020", "ABA=" },
  { 2, "\000\040", "ACA=" },
  { 2, "\000\100", "AEA=" },
  { 2, "\000\200", "AIA=" },
  { 2, "\001\000", "AQA=" },
  { 2, "\002\000", "AgA=" },
  { 2, "\004\000", "BAA=" },
  { 2, "\010\000", "CAA=" },
  { 2, "\020\000", "EAA=" },
  { 2, "\040\000", "IAA=" },
  { 2, "\100\000", "QAA=" },
  { 2, "\200\000", "gAA=" },

  { 2, "\377\377", "//8=" },
  { 2, "\377\376", "//4=" },
  { 2, "\377\375", "//0=" },
  { 2, "\377\373", "//s=" },
  { 2, "\377\367", "//c=" },
  { 2, "\377\357", "/+8=" },
  { 2, "\377\337", "/98=" },
  { 2, "\377\277", "/78=" },
  { 2, "\377\177", "/38=" },
  { 2, "\376\377", "/v8=" },
  { 2, "\375\377", "/f8=" },
  { 2, "\373\377", "+/8=" },
  { 2, "\367\377", "9/8=" },
  { 2, "\357\377", "7/8=" },
  { 2, "\337\377", "3/8=" },
  { 2, "\277\377", "v/8=" },
  { 2, "\177\377", "f/8=" },

  { 3, "\000\000\000", "AAAA" },
  { 3, "\000\000\001", "AAAB" },
  { 3, "\000\000\002", "AAAC" },
  { 3, "\000\000\004", "AAAE" },
  { 3, "\000\000\010", "AAAI" },
  { 3, "\000\000\020", "AAAQ" },
  { 3, "\000\000\040", "AAAg" },
  { 3, "\000\000\100", "AABA" },
  { 3, "\000\000\200", "AACA" },
  { 3, "\000\001\000", "AAEA" },
  { 3, "\000\002\000", "AAIA" },
  { 3, "\000\004\000", "AAQA" },
  { 3, "\000\010\000", "AAgA" },
  { 3, "\000\020\000", "ABAA" },
  { 3, "\000\040\000", "ACAA" },
  { 3, "\000\100\000", "AEAA" },
  { 3, "\000\200\000", "AIAA" },
  { 3, "\001\000\000", "AQAA" },
  { 3, "\002\000\000", "AgAA" },
  { 3, "\004\000\000", "BAAA" },
  { 3, "\010\000\000", "CAAA" },
  { 3, "\020\000\000", "EAAA" },
  { 3, "\040\000\000", "IAAA" },
  { 3, "\100\000\000", "QAAA" },
  { 3, "\200\000\000", "gAAA" },

  { 3, "\377\377\377", "////" },
  { 3, "\377\377\376", "///+" },
  { 3, "\377\377\375", "///9" },
  { 3, "\377\377\373", "///7" },
  { 3, "\377\377\367", "///3" },
  { 3, "\377\377\357", "///v" },
  { 3, "\377\377\337", "///f" },
  { 3, "\377\377\277", "//+/" },
  { 3, "\377\377\177", "//9/" },
  { 3, "\377\376\377", "//7/" },
  { 3, "\377\375\377", "//3/" },
  { 3, "\377\373\377", "//v/" },
  { 3, "\377\367\377", "//f/" },
  { 3, "\377\357\377", "/+//" },
  { 3, "\377\337\377", "/9//" },
  { 3, "\377\277\377", "/7//" },
  { 3, "\377\177\377", "/3//" },
  { 3, "\376\377\377", "/v//" },
  { 3, "\375\377\377", "/f//" },
  { 3, "\373\377\377", "+///" },
  { 3, "\367\377\377", "9///" },
  { 3, "\357\377\377", "7///" },
  { 3, "\337\377\377", "3///" },
  { 3, "\277\377\377", "v///" },
  { 3, "\177\377\377", "f///" },

  // Random numbers: values obtained with
  //
  //  #! /bin/bash
  //  dd bs=$1 count=1 if=/dev/random of=/tmp/bar.random
  //  od -N $1 -t o1 /tmp/bar.random
  //  uuencode -m test < /tmp/bar.random
  //
  // where $1 is the number of bytes (2, 3)

  { 2, "\243\361", "o/E=" },
  { 2, "\024\167", "FHc=" },
  { 2, "\313\252", "y6o=" },
  { 2, "\046\041", "JiE=" },
  { 2, "\145\236", "ZZ4=" },
  { 2, "\254\325", "rNU=" },
  { 2, "\061\330", "Mdg=" },
  { 2, "\245\032", "pRo=" },
  { 2, "\006\000", "BgA=" },
  { 2, "\375\131", "/Vk=" },
  { 2, "\303\210", "w4g=" },
  { 2, "\040\037", "IB8=" },
  { 2, "\261\372", "sfo=" },
  { 2, "\335\014", "3Qw=" },
  { 2, "\233\217", "m48=" },
  { 2, "\373\056", "+y4=" },
  { 2, "\247\232", "p5o=" },
  { 2, "\107\053", "Rys=" },
  { 2, "\204\077", "hD8=" },
  { 2, "\276\211", "vok=" },
  { 2, "\313\110", "y0g=" },
  { 2, "\363\376", "8/4=" },
  { 2, "\251\234", "qZw=" },
  { 2, "\103\262", "Q7I=" },
  { 2, "\142\312", "Yso=" },
  { 2, "\067\211", "N4k=" },
  { 2, "\220\001", "kAE=" },
  { 2, "\152\240", "aqA=" },
  { 2, "\367\061", "9zE=" },
  { 2, "\133\255", "W60=" },
  { 2, "\176\035", "fh0=" },
  { 2, "\032\231", "Gpk=" },

  { 3, "\013\007\144", "Cwdk" },
  { 3, "\030\112\106", "GEpG" },
  { 3, "\047\325\046", "J9Um" },
  { 3, "\310\160\022", "yHAS" },
  { 3, "\131\100\237", "WUCf" },
  { 3, "\064\342\134", "NOJc" },
  { 3, "\010\177\004", "CH8E" },
  { 3, "\345\147\205", "5WeF" },
  { 3, "\300\343\360", "wOPw" },
  { 3, "\061\240\201", "MaCB" },
  { 3, "\225\333\044", "ldsk" },
  { 3, "\215\137\352", "jV/q" },
  { 3, "\371\147\160", "+Wdw" },
  { 3, "\030\320\051", "GNAp" },
  { 3, "\044\174\241", "JHyh" },
  { 3, "\260\127\037", "sFcf" },
  { 3, "\111\045\033", "SSUb" },
  { 3, "\202\114\107", "gkxH" },
  { 3, "\057\371\042", "L/ki" },
  { 3, "\223\247\244", "k6ek" },
  { 3, "\047\216\144", "J45k" },
  { 3, "\203\070\327", "gzjX" },
  { 3, "\247\140\072", "p2A6" },
  { 3, "\124\115\116", "VE1O" },
  { 3, "\157\162\050", "b3Io" },
  { 3, "\357\223\004", "75ME" },
  { 3, "\052\117\156", "Kk9u" },
  { 3, "\347\154\000", "52wA" },
  { 3, "\303\012\142", "wwpi" },
  { 3, "\060\035\362", "MB3y" },
  { 3, "\130\226\361", "WJbx" },
  { 3, "\173\013\071", "ews5" },
  { 3, "\336\004\027", "3gQX" },
  { 3, "\357\366\234", "7/ac" },
  { 3, "\353\304\111", "68RJ" },
  { 3, "\024\264\131", "FLRZ" },
  { 3, "\075\114\251", "PUyp" },
  { 3, "\315\031\225", "zRmV" },
  { 3, "\154\201\276", "bIG+" },
  { 3, "\200\066\072", "gDY6" },
  { 3, "\142\350\267", "Yui3" },
  { 3, "\033\000\166", "GwB2" },
  { 3, "\210\055\077", "iC0/" },
  { 3, "\341\037\124", "4R9U" },
  { 3, "\161\103\152", "cUNq" },
  { 3, "\270\142\131", "uGJZ" },
  { 3, "\337\076\074", "3z48" },
  { 3, "\375\106\362", "/Uby" },
  { 3, "\227\301\127", "l8FX" },
  { 3, "\340\002\234", "4AKc" },
  { 3, "\121\064\033", "UTQb" },
  { 3, "\157\134\143", "b1xj" },
  { 3, "\247\055\327", "py3X" },
  { 3, "\340\142\005", "4GIF" },
  { 3, "\060\260\143", "MLBj" },
  { 3, "\075\203\170", "PYN4" },
  { 3, "\143\160\016", "Y3AO" },
  { 3, "\313\013\063", "ywsz" },
  { 3, "\174\236\135", "fJ5d" },
  { 3, "\103\047\026", "QycW" },
  { 3, "\365\005\343", "9QXj" },
  { 3, "\271\160\223", "uXCT" },
  { 3, "\362\255\172", "8q16" },
  { 3, "\113\012\015", "SwoN" },

  // various lengths, generated by this python script:
  //
  // from string import lowercase as lc
  // for i in range(27):
  //   print '{ %2d, "%s",%s "%s" },' % (i, lc[:i], ' ' * (26-i),
  //                                     lc[:i].encode('base64').strip())

  {  0, "",                           "" },
  {  1, "a",                          "YQ==" },
  {  2, "ab",                         "YWI=" },
  {  3, "abc",                        "YWJj" },
  {  4, "abcd",                       "YWJjZA==" },
  {  5, "abcde",                      "YWJjZGU=" },
  {  6, "abcdef",                     "YWJjZGVm" },
  {  7, "abcdefg",                    "YWJjZGVmZw==" },
  {  8, "abcdefgh",                   "YWJjZGVmZ2g=" },
  {  9, "abcdefghi",                  "YWJjZGVmZ2hp" },
  { 10, "abcdefghij",                 "YWJjZGVmZ2hpag==" },
  { 11, "abcdefghijk",                "YWJjZGVmZ2hpams=" },
  { 12, "abcdefghijkl",               "YWJjZGVmZ2hpamts" },
  { 13, "abcdefghijklm",              "YWJjZGVmZ2hpamtsbQ==" },
  { 14, "abcdefghijklmn",             "YWJjZGVmZ2hpamtsbW4=" },
  { 15, "abcdefghijklmno",            "YWJjZGVmZ2hpamtsbW5v" },
  { 16, "abcdefghijklmnop",           "YWJjZGVmZ2hpamtsbW5vcA==" },
  { 17, "abcdefghijklmnopq",          "YWJjZGVmZ2hpamtsbW5vcHE=" },
  { 18, "abcdefghijklmnopqr",         "YWJjZGVmZ2hpamtsbW5vcHFy" },
  { 19, "abcdefghijklmnopqrs",        "YWJjZGVmZ2hpamtsbW5vcHFycw==" },
  { 20, "abcdefghijklmnopqrst",       "YWJjZGVmZ2hpamtsbW5vcHFyc3Q=" },
  { 21, "abcdefghijklmnopqrstu",      "YWJjZGVmZ2hpamtsbW5vcHFyc3R1" },
  { 22, "abcdefghijklmnopqrstuv",     "YWJjZGVmZ2hpamtsbW5vcHFyc3R1dg==" },
  { 23, "abcdefghijklmnopqrstuvw",    "YWJjZGVmZ2hpamtsbW5vcHFyc3R1dnc=" },
  { 24, "abcdefghijklmnopqrstuvwx",   "YWJjZGVmZ2hpamtsbW5vcHFyc3R1dnd4" },
  { 25, "abcdefghijklmnopqrstuvwxy",  "YWJjZGVmZ2hpamtsbW5vcHFyc3R1dnd4eQ==" },
  { 26, "abcdefghijklmnopqrstuvwxyz", "YWJjZGVmZ2hpamtsbW5vcHFyc3R1dnd4eXo=" },
};

static struct {
  const char* plaintext;
  const char* cyphertext;
} base64_strings[] = {
  // Some google quotes
  // Cyphertext created with "uuencode (GNU sharutils) 4.6.3"
  // (Note that we're testing the websafe encoding, though, so if
  // you add messages, be sure to run "tr -- '+/' '-_'" on the output)
  { "I was always good at math and science, and I never realized "
    "that was unusual or somehow undesirable. So one of the things "
    "I care a lot about is helping to remove that stigma, "
    "to show girls that you can be feminine, you can like the things "
    "that girls like, but you can also be really good at technology. "
    "You can be really good at building things."
    " - Marissa Meyer, Newsweek, 2010-12-22" "\n",

    "SSB3YXMgYWx3YXlzIGdvb2QgYXQgbWF0aCBhbmQgc2NpZW5jZSwgYW5kIEkg"
    "bmV2ZXIgcmVhbGl6ZWQgdGhhdCB3YXMgdW51c3VhbCBvciBzb21laG93IHVu"
    "ZGVzaXJhYmxlLiBTbyBvbmUgb2YgdGhlIHRoaW5ncyBJIGNhcmUgYSBsb3Qg"
    "YWJvdXQgaXMgaGVscGluZyB0byByZW1vdmUgdGhhdCBzdGlnbWEsIHRvIHNo"
    "b3cgZ2lybHMgdGhhdCB5b3UgY2FuIGJlIGZlbWluaW5lLCB5b3UgY2FuIGxp"
    "a2UgdGhlIHRoaW5ncyB0aGF0IGdpcmxzIGxpa2UsIGJ1dCB5b3UgY2FuIGFs"
    "c28gYmUgcmVhbGx5IGdvb2QgYXQgdGVjaG5vbG9neS4gWW91IGNhbiBiZSBy"
    "ZWFsbHkgZ29vZCBhdCBidWlsZGluZyB0aGluZ3MuIC0gTWFyaXNzYSBNZXll"
    "ciwgTmV3c3dlZWssIDIwMTAtMTItMjIK" },

  { "Typical first year for a new cluster: "
    "~0.5 overheating "
    "~1 PDU failure "
    "~1 rack-move "
    "~1 network rewiring "
    "~20 rack failures "
    "~5 racks go wonky "
    "~8 network maintenances "
    "~12 router reloads "
    "~3 router failures "
    "~dozens of minor 30-second blips for dns "
    "~1000 individual machine failures "
    "~thousands of hard drive failures "
    "slow disks, bad memory, misconfigured machines, flaky machines, etc."
    " - Jeff Dean, The Joys of Real Hardware" "\n",

    "VHlwaWNhbCBmaXJzdCB5ZWFyIGZvciBhIG5ldyBjbHVzdGVyOiB-MC41IG92"
    "ZXJoZWF0aW5nIH4xIFBEVSBmYWlsdXJlIH4xIHJhY2stbW92ZSB-MSBuZXR3"
    "b3JrIHJld2lyaW5nIH4yMCByYWNrIGZhaWx1cmVzIH41IHJhY2tzIGdvIHdv"
    "bmt5IH44IG5ldHdvcmsgbWFpbnRlbmFuY2VzIH4xMiByb3V0ZXIgcmVsb2Fk"
    "cyB-MyByb3V0ZXIgZmFpbHVyZXMgfmRvemVucyBvZiBtaW5vciAzMC1zZWNv"
    "bmQgYmxpcHMgZm9yIGRucyB-MTAwMCBpbmRpdmlkdWFsIG1hY2hpbmUgZmFp"
    "bHVyZXMgfnRob3VzYW5kcyBvZiBoYXJkIGRyaXZlIGZhaWx1cmVzIHNsb3cg"
    "ZGlza3MsIGJhZCBtZW1vcnksIG1pc2NvbmZpZ3VyZWQgbWFjaGluZXMsIGZs"
    "YWt5IG1hY2hpbmVzLCBldGMuIC0gSmVmZiBEZWFuLCBUaGUgSm95cyBvZiBS"
    "ZWFsIEhhcmR3YXJlCg" },

  { "I'm the head of the webspam team at Google.  "
    "That means that if you type your name into Google and get porn back, "
    "it's my fault. Unless you're a porn star, in which case porn is a "
    "completely reasonable response."
    " - Matt Cutts, Google Plus" "\n",

    "SSdtIHRoZSBoZWFkIG9mIHRoZSB3ZWJzcGFtIHRlYW0gYXQgR29vZ2xlLiAg"
    "VGhhdCBtZWFucyB0aGF0IGlmIHlvdSB0eXBlIHlvdXIgbmFtZSBpbnRvIEdv"
    "b2dsZSBhbmQgZ2V0IHBvcm4gYmFjaywgaXQncyBteSBmYXVsdC4gVW5sZXNz"
    "IHlvdSdyZSBhIHBvcm4gc3RhciwgaW4gd2hpY2ggY2FzZSBwb3JuIGlzIGEg"
    "Y29tcGxldGVseSByZWFzb25hYmxlIHJlc3BvbnNlLiAtIE1hdHQgQ3V0dHMs"
    "IEdvb2dsZSBQbHVzCg" },

  { "It will still be a long time before machines approach human intelligence. "
    "But luckily, machines don't actually have to be intelligent; "
    "they just have to fake it. Access to a wealth of information, "
    "combined with a rudimentary decision-making capacity, "
    "can often be almost as useful. Of course, the results are better yet "
    "when coupled with intelligence. A reference librarian with access to "
    "a good search engine is a formidable tool."
    " - Craig Silverstein, Siemens Pictures of the Future, Spring 2004" "\n",

    "SXQgd2lsbCBzdGlsbCBiZSBhIGxvbmcgdGltZSBiZWZvcmUgbWFjaGluZXMg"
    "YXBwcm9hY2ggaHVtYW4gaW50ZWxsaWdlbmNlLiBCdXQgbHVja2lseSwgbWFj"
    "aGluZXMgZG9uJ3QgYWN0dWFsbHkgaGF2ZSB0byBiZSBpbnRlbGxpZ2VudDsg"
    "dGhleSBqdXN0IGhhdmUgdG8gZmFrZSBpdC4gQWNjZXNzIHRvIGEgd2VhbHRo"
    "IG9mIGluZm9ybWF0aW9uLCBjb21iaW5lZCB3aXRoIGEgcnVkaW1lbnRhcnkg"
    "ZGVjaXNpb24tbWFraW5nIGNhcGFjaXR5LCBjYW4gb2Z0ZW4gYmUgYWxtb3N0"
    "IGFzIHVzZWZ1bC4gT2YgY291cnNlLCB0aGUgcmVzdWx0cyBhcmUgYmV0dGVy"
    "IHlldCB3aGVuIGNvdXBsZWQgd2l0aCBpbnRlbGxpZ2VuY2UuIEEgcmVmZXJl"
    "bmNlIGxpYnJhcmlhbiB3aXRoIGFjY2VzcyB0byBhIGdvb2Qgc2VhcmNoIGVu"
    "Z2luZSBpcyBhIGZvcm1pZGFibGUgdG9vbC4gLSBDcmFpZyBTaWx2ZXJzdGVp"
    "biwgU2llbWVucyBQaWN0dXJlcyBvZiB0aGUgRnV0dXJlLCBTcHJpbmcgMjAw"
    "NAo" },

  // Degenerate edge case
  { "",
    "" },
};

TEST(Base64, EscapeAndUnescape) {
  // Check the short strings; this tests the math (and boundaries)
  for (int i = 0; i < sizeof(base64_tests) / sizeof(base64_tests[0]); ++i) {
    char encode_buffer[100];
    int encode_length;
    char decode_buffer[100];
    int decode_length;
    int cypher_length;
    string decode_str;

    const unsigned char* unsigned_plaintext =
      reinterpret_cast<const unsigned char*>(base64_tests[i].plaintext);

    StringPiece plaintext(base64_tests[i].plaintext,
                          base64_tests[i].plain_length);

    cypher_length = strlen(base64_tests[i].cyphertext);

    // The basic escape function:
    memset(encode_buffer, 0, sizeof(encode_buffer));
    encode_length = Base64Escape(unsigned_plaintext,
                                 base64_tests[i].plain_length,
                                 encode_buffer,
                                 sizeof(encode_buffer));
    //    Is it of the expected length?
    EXPECT_EQ(encode_length, cypher_length);
    // Would it have been okay to allocate only CalculateBase64EscapeLen()?
    EXPECT_EQ(CalculateBase64EscapedLen(base64_tests[i].plain_length),
              encode_length);

    //    Is it the expected encoded value?
    ASSERT_STREQ(encode_buffer, base64_tests[i].cyphertext);

    // If we encode it into a buffer of exactly the right length...
    memset(encode_buffer, 0, sizeof(encode_buffer));
    encode_length = Base64Escape(unsigned_plaintext,
                                          base64_tests[i].plain_length,
                                          encode_buffer,
                                          cypher_length);
    //    Is it still of the expected length?
    EXPECT_EQ(encode_length, cypher_length);

    //    And is the value still correct?  (i.e., not losing the last byte)
    EXPECT_STREQ(encode_buffer, base64_tests[i].cyphertext);

    // If we decode it back:
    decode_str.clear();
    EXPECT_TRUE(Base64Unescape(
        StringPiece(encode_buffer, cypher_length), &decode_str));

    //    Is it of the expected length?
    EXPECT_EQ(base64_tests[i].plain_length, decode_str.length());

    //    Is it the expected decoded value?
    EXPECT_EQ(plaintext, decode_str);

    // Let's try with a pre-populated string.
    string encoded("this junk should be ignored");
    Base64Escape(string(base64_tests[i].plaintext,
                        base64_tests[i].plain_length),
                 &encoded);
    EXPECT_EQ(encoded, string(encode_buffer, cypher_length));

    string decoded("this junk should be ignored");
    EXPECT_TRUE(Base64Unescape(
        StringPiece(encode_buffer, cypher_length), &decoded));
    EXPECT_EQ(decoded.size(), base64_tests[i].plain_length);
    EXPECT_EQ_ARRAY(decoded.size(), decoded, base64_tests[i].plaintext, i);

    // Our decoder treats the padding '=' characters at the end as
    // optional (but if there are any, there must be the correct
    // number of them.)  If encode_buffer has any, run some additional
    // tests that fiddle with them.
    char* first_equals = strchr(encode_buffer, '=');
    if (first_equals) {
      // How many equals signs does the string start with?
      int equals = (*(first_equals+1) == '=') ? 2 : 1;

      // Try chopping off the equals sign(s) entirely.  The decoder
      // should still be okay with this.
      string decoded2("this junk should also be ignored");
      *first_equals = '\0';
      EXPECT_TRUE(Base64Unescape(
          StringPiece(encode_buffer, first_equals - encode_buffer), &decoded2));
      EXPECT_EQ(decoded.size(), base64_tests[i].plain_length);
      EXPECT_EQ_ARRAY(decoded.size(), decoded, base64_tests[i].plaintext, i);

      // Now test chopping off the equals sign(s) and adding
      // whitespace.  Our decoder should still accept this.
      decoded2.assign("this junk should be ignored");
      *first_equals = ' ';
      *(first_equals+1) = '\0';
      EXPECT_TRUE(Base64Unescape(
          StringPiece(encode_buffer, first_equals - encode_buffer + 1),
          &decoded2));
      EXPECT_EQ(decoded.size(), base64_tests[i].plain_length);
      EXPECT_EQ_ARRAY(decoded.size(), decoded, base64_tests[i].plaintext, i);

      // Now stick a bad character at the end of the string.  The decoder
      // should refuse this string.
      decoded2.assign("this junk should be ignored");
      *first_equals = '?';
      *(first_equals+1) = '\0';
      EXPECT_TRUE(
          !Base64Unescape(
              StringPiece(encode_buffer, first_equals - encode_buffer + 1),
              &decoded2));

      int len;

      // Test whitespace mixed with the padding.  (eg "AA = = ")  The
      // decoder should accept this.
      if (equals == 2) {
        snprintf(first_equals, 6, " = = ");
        len = first_equals - encode_buffer + 5;
      } else {
        snprintf(first_equals, 6, " = ");
        len = first_equals - encode_buffer + 3;
      }
      decoded2.assign("this junk should be ignored");
      EXPECT_TRUE(
          Base64Unescape(StringPiece(encode_buffer, len), &decoded2));
      EXPECT_EQ(decoded.size(), base64_tests[i].plain_length);
      EXPECT_EQ_ARRAY(decoded.size(), decoded, base64_tests[i].plaintext, i);

      // Test whitespace mixed with the padding, but with the wrong
      // number of equals signs (eg "AA = ").  The decoder should
      // refuse these strings.
      if (equals == 1) {
        snprintf(first_equals, 6, " = = ");
        len = first_equals - encode_buffer + 5;
      } else {
        snprintf(first_equals, 6, " = ");
        len = first_equals - encode_buffer + 3;
      }
      EXPECT_TRUE(
          !Base64Unescape(StringPiece(encode_buffer, len), &decoded2));
    }

    // Cool! the basic Base64 encoder/decoder works.
    // Let's try the alternate alphabet: tr -- '+/' '-_'

    char websafe[100];
    memset(websafe, 0, sizeof(websafe));
    strncpy(websafe, base64_tests[i].cyphertext, cypher_length);
    for (int c = 0; c < sizeof(websafe); ++c) {
      if ('+' == websafe[c]) { websafe[c] = '-'; }
      if ('/' == websafe[c]) { websafe[c] = '_'; }
    }

    // The websafe escape function:
    memset(encode_buffer, 0, sizeof(encode_buffer));
    encode_length = WebSafeBase64Escape(unsigned_plaintext,
                                                 base64_tests[i].plain_length,
                                                 encode_buffer,
                                                 sizeof(encode_buffer),
                                                 true);
    //    Is it of the expected length?
    EXPECT_EQ(encode_length, cypher_length);
    EXPECT_EQ(
        CalculateBase64EscapedLen(base64_tests[i].plain_length, true),
        encode_length);

    //    Is it the expected encoded value?
    EXPECT_STREQ(encode_buffer, websafe);

    //    If we encode it into a buffer of exactly the right length...
    memset(encode_buffer, 0, sizeof(encode_buffer));
    encode_length = WebSafeBase64Escape(unsigned_plaintext,
                                                 base64_tests[i].plain_length,
                                                 encode_buffer,
                                                 cypher_length,
                                                 true);
    //    Is it still of the expected length?
    EXPECT_EQ(encode_length, cypher_length);

    //    And is the value still correct?  (i.e., not losing the last byte)
    EXPECT_STREQ(encode_buffer, websafe);

    //    Let's try the string version of the encoder
    encoded = "this junk should be ignored";
    WebSafeBase64Escape(
        unsigned_plaintext, base64_tests[i].plain_length,
        &encoded, true);
    EXPECT_EQ(encoded.size(), cypher_length);
    EXPECT_STREQ(encoded.c_str(), websafe);

    //    If we decode it back:
    memset(decode_buffer, 0, sizeof(decode_buffer));
    decode_length = WebSafeBase64Unescape(encode_buffer,
                                                   cypher_length,
                                                   decode_buffer,
                                                   sizeof(decode_buffer));

    //    Is it of the expected length?
    EXPECT_EQ(decode_length, base64_tests[i].plain_length);

    //    Is it the expected decoded value?
    EXPECT_EQ(0,
              memcmp(decode_buffer, base64_tests[i].plaintext, decode_length));

    //    If we decode it into a buffer of exactly the right length...
    memset(decode_buffer, 0, sizeof(decode_buffer));
    decode_length = WebSafeBase64Unescape(encode_buffer,
                                                   cypher_length,
                                                   decode_buffer,
                                                   decode_length);

    //    Is it still of the expected length?
    EXPECT_EQ(decode_length, base64_tests[i].plain_length);

    //    And is it the expected decoded value?
    EXPECT_EQ(0,
              memcmp(decode_buffer, base64_tests[i].plaintext, decode_length));

    // Try using '.' for the pad character.
    for (int c = cypher_length - 1; c >= 0 && '=' == encode_buffer[c]; --c) {
      encode_buffer[c] = '.';
    }

    // If we decode it back:
    memset(decode_buffer, 0, sizeof(decode_buffer));
    decode_length = WebSafeBase64Unescape(encode_buffer,
                                                   cypher_length,
                                                   decode_buffer,
                                                   sizeof(decode_buffer));

    // Is it of the expected length?
    EXPECT_EQ(decode_length, base64_tests[i].plain_length);

    // Is it the expected decoded value?
    EXPECT_EQ(0,
              memcmp(decode_buffer, base64_tests[i].plaintext, decode_length));

    // If we decode it into a buffer of exactly the right length...
    memset(decode_buffer, 0, sizeof(decode_buffer));
    decode_length = WebSafeBase64Unescape(encode_buffer,
                                                   cypher_length,
                                                   decode_buffer,
                                                   decode_length);

    // Is it still of the expected length?
    EXPECT_EQ(decode_length, base64_tests[i].plain_length);

    // And is it the expected decoded value?
    EXPECT_EQ(0,
              memcmp(decode_buffer, base64_tests[i].plaintext, decode_length));

    // Let's try the string version of the decoder
    decoded = "this junk should be ignored";
    EXPECT_TRUE(WebSafeBase64Unescape(
        StringPiece(encode_buffer, cypher_length), &decoded));
    EXPECT_EQ(decoded.size(), base64_tests[i].plain_length);
    EXPECT_EQ_ARRAY(decoded.size(), decoded, base64_tests[i].plaintext, i);

    // Okay! the websafe Base64 encoder/decoder works.
    // Let's try the unpadded version

    for (int c = 0; c < sizeof(websafe); ++c) {
      if ('=' == websafe[c]) {
        websafe[c] = '\0';
        cypher_length = c;
        break;
      }
    }

    // The websafe escape function:
    memset(encode_buffer, 0, sizeof(encode_buffer));
    encode_length = WebSafeBase64Escape(unsigned_plaintext,
                                                 base64_tests[i].plain_length,
                                                 encode_buffer,
                                                 sizeof(encode_buffer),
                                                 false);
    //    Is it of the expected length?
    EXPECT_EQ(encode_length, cypher_length);
    EXPECT_EQ(
        CalculateBase64EscapedLen(base64_tests[i].plain_length, false),
        encode_length);

    //    Is it the expected encoded value?
    EXPECT_STREQ(encode_buffer, websafe);

    //    If we encode it into a buffer of exactly the right length...
    memset(encode_buffer, 0, sizeof(encode_buffer));
    encode_length = WebSafeBase64Escape(unsigned_plaintext,
                                                 base64_tests[i].plain_length,
                                                 encode_buffer,
                                                 cypher_length,
                                                 false);
    //    Is it still of the expected length?
    EXPECT_EQ(encode_length, cypher_length);

    //    And is the value still correct?  (i.e., not losing the last byte)
    EXPECT_STREQ(encode_buffer, websafe);

    // Let's try the (other) string version of the encoder
    string plain(base64_tests[i].plaintext, base64_tests[i].plain_length);
    encoded = "this junk should be ignored";
    WebSafeBase64Escape(plain, &encoded);
    EXPECT_EQ(encoded.size(), cypher_length);
    EXPECT_STREQ(encoded.c_str(), websafe);

    //    If we decode it back:
    memset(decode_buffer, 0, sizeof(decode_buffer));
    decode_length = WebSafeBase64Unescape(encode_buffer,
                                                   cypher_length,
                                                   decode_buffer,
                                                   sizeof(decode_buffer));

    //    Is it of the expected length?
    EXPECT_EQ(decode_length, base64_tests[i].plain_length);

    //    Is it the expected decoded value?
    EXPECT_EQ(0,
              memcmp(decode_buffer, base64_tests[i].plaintext, decode_length));

    //    If we decode it into a buffer of exactly the right length...
    memset(decode_buffer, 0, sizeof(decode_buffer));
    decode_length = WebSafeBase64Unescape(encode_buffer,
                                                   cypher_length,
                                                   decode_buffer,
                                                   decode_length);

    //    Is it still of the expected length?
    EXPECT_EQ(decode_length, base64_tests[i].plain_length);

    //    And is it the expected decoded value?
    EXPECT_EQ(0,
              memcmp(decode_buffer, base64_tests[i].plaintext, decode_length));


    // Let's try the string version of the decoder
    decoded = "this junk should be ignored";
    EXPECT_TRUE(WebSafeBase64Unescape(
        StringPiece(encode_buffer, cypher_length), &decoded));
    EXPECT_EQ(decoded.size(), base64_tests[i].plain_length);
    EXPECT_EQ_ARRAY(decoded.size(), decoded, base64_tests[i].plaintext, i);

    // This value works.  Try the next.
  }

  // Now try the long strings, this tests the streaming
  for (int i = 0; i < sizeof(base64_strings) / sizeof(base64_strings[0]);
       ++i) {
    const unsigned char* unsigned_plaintext =
      reinterpret_cast<const unsigned char*>(base64_strings[i].plaintext);
    int plain_length = strlen(base64_strings[i].plaintext);
    int cypher_length = strlen(base64_strings[i].cyphertext);
    std::vector<char> buffer(cypher_length+1);
    int encode_length = WebSafeBase64Escape(unsigned_plaintext,
                                                     plain_length,
                                                     &buffer[0],
                                                     buffer.size(),
                                                     false);
    EXPECT_EQ(cypher_length, encode_length);
    EXPECT_EQ(
        CalculateBase64EscapedLen(plain_length, false), encode_length);
    buffer[ encode_length ] = '\0';
    EXPECT_STREQ(base64_strings[i].cyphertext, &buffer[0]);
  }

  // Verify the behavior when decoding bad data
  {
    const char* bad_data = "ab-/";
    string buf;
    EXPECT_FALSE(Base64Unescape(StringPiece(bad_data), &buf));
    EXPECT_TRUE(!WebSafeBase64Unescape(bad_data, &buf));
    EXPECT_TRUE(buf.empty());
  }
}

// Test StrCat of ints and longs of various sizes and signdedness.
TEST(StrCat, Ints) {
  const short s = -1;  // NOLINT(runtime/int)
  const uint16_t us = 2;
  const int i = -3;
  const unsigned int ui = 4;
  const long l = -5;                 // NOLINT(runtime/int)
  const unsigned long ul = 6;        // NOLINT(runtime/int)
  const long long ll = -7;           // NOLINT(runtime/int)
  const unsigned long long ull = 8;  // NOLINT(runtime/int)
  const ptrdiff_t ptrdiff = -9;
  const size_t size = 10;
  const intptr_t intptr = -12;
  const uintptr_t uintptr = 13;
  string answer;
  answer = StrCat(s, us);
  EXPECT_EQ(answer, "-12");
  answer = StrCat(i, ui);
  EXPECT_EQ(answer, "-34");
  answer = StrCat(l, ul);
  EXPECT_EQ(answer, "-56");
  answer = StrCat(ll, ull);
  EXPECT_EQ(answer, "-78");
  answer = StrCat(ptrdiff, size);
  EXPECT_EQ(answer, "-910");
  answer = StrCat(ptrdiff, intptr);
  EXPECT_EQ(answer, "-9-12");
  answer = StrCat(uintptr, 0);
  EXPECT_EQ(answer, "130");
}

}  // anonymous namespace
}  // namespace protobuf
}  // namespace google

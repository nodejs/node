// Copyright 2013 the V8 project authors. All rights reserved.
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

var processor = new ArgumentsProcessor(arguments);
var distortion_per_entry = 0;
var range_start_override = undefined;
var range_end_override = undefined;

if (!processor.parse()) processor.printUsageAndExit();;
var result = processor.result();
var distortion = parseInt(result.distortion);
if (isNaN(distortion)) processor.printUsageAndExit();;
// Convert picoseconds to milliseconds.
distortion_per_entry = distortion / 1000000;
var rangelimits = result.range.split(",");
var range_start = parseInt(rangelimits[0]);
var range_end = parseInt(rangelimits[1]);
if (!isNaN(range_start)) range_start_override = range_start;
if (!isNaN(range_end)) range_end_override = range_end;

var kResX = 1600;
var kResY = 600;
function log_error(text) {
  print(text);
  quit(1);
}
var psc = new PlotScriptComposer(kResX, kResY, log_error);
psc.collectData(readline, distortion_per_entry);
psc.findPlotRange(range_start_override, range_end_override);
print("set terminal pngcairo size " + kResX + "," + kResY +
      " enhanced font 'Helvetica,10'");
psc.assembleOutput(print);

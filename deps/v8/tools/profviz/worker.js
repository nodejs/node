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

var delegateList = {
  "load scripts" : load_scripts,
  "run" : run,
}

self.addEventListener("message", function(event) {
  var call = delegateList[event.data["call"]];
  var result = call(event.data["args"]);
}, false);


function log(text) {
  self.postMessage({ "call" : "log", "args" : text });
}


function displayplot(content) {
  self.postMessage({ "call" : "displayplot", "args" : content});
}


function displayprof(content) {
  self.postMessage({ "call" : "displayprof", "args" : content});
}


function setRange(start, end) {
  self.postMessage({ "call" : "range",
                     "args" : { "start" : start, "end" : end } });
}


function time(name, fun) {
  log(name + "...");
  var start = Date.now();
  fun();
  log(" took " + (Date.now() - start) / 1000 + "s.\n");
}


function load_scripts(scripts) {
  time("Loading scripts",
       function() { for (var i in scripts) importScripts(scripts[i]); });
  self.postMessage({ "call" : "script" });
}


function log_error(text) {
  self.postMessage({"call": "error", "args": text});
  self.postMessage({"call": "reset"});
}


function run(args) {
  var file = args["file"];
  var resx = args["resx"];
  var resy = args["resy"];
  var distortion = args["distortion"];
  var range_start_override = args["range_start"];
  var range_end_override = args["range_end"];

  var reader = new FileReaderSync();
  var content_lines;

  time("Reading log file (" + (file.size / 1024).toFixed(1) + " kB)",
       function() {
         var content = reader.readAsText(file);
         content_lines = content.split("\n");
       });

  time("Producing statistical profile",
       function() {
         var profile = "";
         print = function(text) { profile += text + "\n"; };
         // Dummy entries provider, as we cannot call nm.
         var entriesProvider = new UnixCppEntriesProvider("", "", "");
         var targetRootFS = "";
         var separateIc = false;
         var callGraphSize = 5;
         var ignoreUnknown = true;
         var stateFilter = null;
         var range = range_start_override + "," + range_end_override;

         var tickProcessor = new TickProcessor(entriesProvider,
                                               separateIc,
                                               callGraphSize,
                                               ignoreUnknown,
                                               stateFilter,
                                               distortion,
                                               range);
         for (var i = 0; i < content_lines.length; i++) {
           tickProcessor.processLogLine(content_lines[i]);
         }
         tickProcessor.printStatistics();
         displayprof(profile);
       });

  var input_file_name = "input_temp";
  var output_file_name = "output.svg";

  var psc = new PlotScriptComposer(resx, resy, log_error);
  var objects = 0;

  time("Collecting events (" + content_lines.length + " entries)",
       function() {
         var line_cursor = 0;
         var input = function() { return content_lines[line_cursor++]; };
         psc.collectData(input, distortion);
         psc.findPlotRange(range_start_override,
                           range_end_override,
                           setRange);
       });

  time("Assembling plot script",
       function() {
         var plot_script = "";
         var output = function(text) { plot_script += text + "\n"; };
         output("set terminal svg size " + resx + "," + resy +
                " enhanced font \"Helvetica,10\"");
         output("set output \""+ output_file_name + "\"");
         objects = psc.assembleOutput(output);
         if (FS.findObject(input_file_name)) {
           FS.deleteFile(input_file_name);
         }
         var arrc = Module["intArrayFromString"](plot_script, true);
         FS.createDataFile("/", input_file_name, arrc);
       });

  time("Running gnuplot (" + objects + " objects)",
       function() { Module.run([input_file_name]); });

  displayplot(FS.findObject(output_file_name));
}


var Module = {
    "noInitialRun": true,
    print: function(text) {
        self.postMessage({"call": "error", "args": text});
    },
    printErr: function(text) {
        self.postMessage({"call": "error", "args": text});
    },
};

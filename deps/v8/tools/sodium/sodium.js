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

var Sodium = (function() {
  "use strict";

  var kinds = ["FUNCTION", "OPTIMIZED_FUNCTION", "STUB", "BUILTIN",
               "LOAD_IC", "KEYED_LOAD_IC", "CALL_IC", "KEYED_CALL_IC",
               "STORE_IC", "KEYED_STORE_IC", "BINARY_OP_IC", "COMPARE_IC",
               "COMPARE_NIL_IC", "TO_BOOLEAN_IC"];
  var kindsWithSource = {
    'FUNCTION': true,
    'OPTIMIZED_FUNCTION': true
  };

  var addressRegEx = "0x[0-9a-f]{8,16}";
  var nameFinder = new RegExp("^name = (.+)$");
  var kindFinder = new RegExp("^kind = (.+)$");
  var firstPositionFinder = new RegExp("^source_position = (\\d+)$");
  var separatorFilter = new RegExp("^--- (.)+ ---$");
  var rawSourceFilter = new RegExp("^--- Raw source ---$");
  var codeEndFinder = new RegExp("^--- End code ---$");
  var whiteSpaceLineFinder = new RegExp("^\\W*$");
  var instructionBeginFinder =
    new RegExp("^Instructions\\W+\\(size = \\d+\\)");
  var instructionFinder =
    new RegExp("^\(" + addressRegEx + "\)\(\\W+\\d+\\W+.+\)");
  var positionFinder =
    new RegExp("^(" + addressRegEx + ")\\W+position\\W+\\((\\d+)\\)");
  var addressFinder = new RegExp("\(" + addressRegEx + "\)");
  var addressReplacer = new RegExp("\(" + addressRegEx + "\)", "gi");

  var fileContent = "";
  var selectedFunctionKind = "";
  var currentFunctionKind = "";

  var currentFunctionName = "";
  var firstSourcePosition = 0;
  var startAddress = "";
  var readingSource = false;
  var readingAsm = false;
  var sourceBegin = -1;
  var sourceEnd = -1;
  var asmBegin = -1;
  var asmEnd = -1;
  var codeObjects = [];
  var selectedAsm = null;
  var selectedSource = null;
  var selectedSourceClass = "";

  function Code(name, kind, sourceBegin, sourceEnd, asmBegin, asmEnd,
                firstSourcePosition, startAddress) {
    this.name = name;
    this.kind = kind;
    this.sourceBegin = sourceBegin;
    this.sourceEnd = sourceEnd;
    this.asmBegin = asmBegin;
    this.asmEnd = asmEnd;
    this.firstSourcePosition = firstSourcePosition;
    this.startAddress = startAddress;
  }

  function getCurrentCodeObject() {
    var functionSelect = document.getElementById('function-selector-id');
    return functionSelect.options[functionSelect.selectedIndex].codeObject;
  }

  function getCurrentSourceText() {
    var code = getCurrentCodeObject();
    if (code.sourceBegin == -1 || code.sourceEnd == -1) return "";
    return fileContent.substring(code.sourceBegin, code.sourceEnd);
  }

  function getCurrentAsmText() {
    var code = getCurrentCodeObject();
    if (code.asmBegin == -1 || code.asmEnd == -1) return "";
    return fileContent.substring(code.asmBegin, code.asmEnd);
  }

  function setKindByIndex(index) {
    selectedFunctionKind = kinds[index];
  }

  function processLine(text, begin, end) {
    var line = text.substring(begin, end);
    if (readingSource) {
      if (separatorFilter.exec(line) != null) {
        readingSource = false;
      } else {
        if (sourceBegin == -1) {
          sourceBegin = begin;
        }
        sourceEnd = end;
      }
    } else {
      if (readingAsm) {
        if (codeEndFinder.exec(line) != null) {
          readingAsm = false;
          asmEnd = begin;
          var newCode =
            new Code(currentFunctionName, currentFunctionKind,
                     sourceBegin, sourceEnd, asmBegin, asmEnd,
                     firstSourcePosition, startAddress);
          codeObjects.push(newCode);
          currentFunctionKind = null;
        } else {
          if (asmBegin == -1) {
            matches = instructionBeginFinder.exec(line);
            if (matches != null) {
              asmBegin = begin;
            }
          }
          if (startAddress == "") {
            matches = instructionFinder.exec(line);
            if (matches != null) {
              startAddress = matches[1];
            }
          }
        }
      } else {
        var matches = kindFinder.exec(line);
        if (matches != null) {
          currentFunctionKind = matches[1];
          if (!kindsWithSource[currentFunctionKind]) {
            sourceBegin = -1;
            sourceEnd = -1;
          }
        } else if (currentFunctionKind != null) {
          matches = nameFinder.exec(line);
          if (matches != null) {
            readingAsm = true;
            asmBegin = -1;
            currentFunctionName = matches[1];
          }
        } else if (rawSourceFilter.exec(line) != null) {
          readingSource = true;
          sourceBegin = -1;
        } else {
          var matches = firstPositionFinder.exec(line);
          if (matches != null) {
            firstSourcePosition = parseInt(matches[1]);
          }
        }
      }
    }
  }

  function processLines(source, size, processLine) {
    var firstChar = 0;
    for (var x = 0; x < size; x++) {
      var curChar = source[x];
      if (curChar == '\n' || curChar == '\r') {
        processLine(source, firstChar, x);
        firstChar = x + 1;
      }
    }
    if (firstChar != size - 1) {
      processLine(source, firstChar, size - 1);
    }
  }

  function processFileContent() {
    document.getElementById('source-text-pre').innerHTML = '';
    sourceBegin = -1;
    codeObjects = [];
    processLines(fileContent, fileContent.length, processLine);
    var functionSelectElement = document.getElementById('function-selector-id');
    functionSelectElement.innerHTML = '';
    var length = codeObjects.length;
    for (var i = 0; i < codeObjects.length; ++i) {
      var code = codeObjects[i];
      if (code.kind == selectedFunctionKind) {
        var optionElement = document.createElement("option");
        optionElement.codeObject = code;
        optionElement.text = code.name;
        functionSelectElement.add(optionElement, null);
      }
    }
  }

  function asmClick(element) {
    if (element == selectedAsm) return;
    if (selectedAsm != null) {
      selectedAsm.classList.remove('highlight-yellow');
    }
    selectedAsm = element;
    selectedAsm.classList.add('highlight-yellow');

    var pc = element.firstChild.innerText;
    var sourceLine = null;
    if (addressFinder.exec(pc) != null) {
      var position = findSourcePosition(pc);
      var line = findSourceLine(position);
      sourceLine = document.getElementById('source-line-' + line);
      var sourceLineTop = sourceLine.offsetTop;
      makeSourcePosVisible(sourceLineTop);
    }
    if (selectedSource == sourceLine) return;
    if (selectedSource != null) {
      selectedSource.classList.remove('highlight-yellow');
      selectedSource.classList.add(selectedSourceClass);
    }
    if (sourceLine != null) {
      selectedSourceClass = sourceLine.classList[0];
      sourceLine.classList.remove(selectedSourceClass);
      sourceLine.classList.add('highlight-yellow');
    }
    selectedSource = sourceLine;
  }

  function makeContainerPosVisible(container, newTop) {
    var height = container.offsetHeight;
    var margin = Math.floor(height / 4);
    if (newTop < container.scrollTop + margin) {
      newTop -= margin;
      if (newTop < 0) newTop = 0;
      container.scrollTop = newTop;
      return;
    }
    if (newTop > (container.scrollTop + 3 * margin)) {
      newTop = newTop - 3 * margin;
      container.scrollTop = newTop;
    }
  }

  function makeAsmPosVisible(newTop) {
    var asmContainer = document.getElementById('asm-container');
    makeContainerPosVisible(asmContainer, newTop);
  }

  function makeSourcePosVisible(newTop) {
    var sourceContainer = document.getElementById('source-container');
    makeContainerPosVisible(sourceContainer, newTop);
  }

  function addressClick(element, event) {
    event.stopPropagation();
    var asmLineId = 'address-' + element.innerText;
    var asmLineElement = document.getElementById(asmLineId);
    if (asmLineElement != null) {
      var asmLineTop = asmLineElement.parentNode.offsetTop;
      makeAsmPosVisible(asmLineTop);
      asmLineElement.classList.add('highlight-flash-blue');
      window.setTimeout(function() {
        asmLineElement.classList.remove('highlight-flash-blue');
      }, 1500);
    }
  }

  function prepareAsm(originalSource) {
    var newSource = "";
    var lineNumber = 1;
    var functionProcessLine = function(text, begin, end) {
      var currentLine = text.substring(begin, end);
      var matches = instructionFinder.exec(currentLine);
      var clickHandler = "";
      if (matches != null) {
        var restOfLine = matches[2];
        restOfLine = restOfLine.replace(
          addressReplacer,
          '<span class="hover-underline" ' +
            'onclick="Sodium.addressClick(this, event);">\$1</span>');
        currentLine = '<span id="address-' + matches[1] + '" >' +
          matches[1] + '</span>' + restOfLine;
        clickHandler = 'onclick=\'Sodium.asmClick(this)\' ';
      } else if (whiteSpaceLineFinder.exec(currentLine)) {
        currentLine = "<br>";
      }
      newSource += '<pre style=\'margin-bottom: -12px;\' ' + clickHandler + '>' +
        currentLine + '</pre>';
      lineNumber++;
    }
    processLines(originalSource, originalSource.length, functionProcessLine);
    return newSource;
  }

  function findSourcePosition(pcToSearch) {
    var position = 0;
    var distance = 0x7FFFFFFF;
    var pcToSearchOffset = parseInt(pcToSearch);
    var processOneLine = function(text, begin, end) {
      var currentLine = text.substring(begin, end);
      var matches = positionFinder.exec(currentLine);
      if (matches != null) {
        var pcOffset = parseInt(matches[1]);
        if (pcOffset <= pcToSearchOffset) {
          var dist =  pcToSearchOffset - pcOffset;
          var pos = parseInt(matches[2]);
          if ((dist < distance) || (dist == distance && pos > position)) {
            position = pos;
            distance = dist;
          }
        }
      }
    }
    var asmText = getCurrentAsmText();
    processLines(asmText, asmText.length, processOneLine);
    var code = getCurrentCodeObject();
    if (position == 0) return 0;
    return position - code.firstSourcePosition;
  }

  function findSourceLine(position) {
    if (position == 0) return 1;
    var line = 0;
    var processOneLine = function(text, begin, end) {
      if (begin < position) {
        line++;
      }
    }
    var sourceText = getCurrentSourceText();
    processLines(sourceText, sourceText.length, processOneLine);
    return line;
  }

  function functionChangedHandler() {
    var functionSelect = document.getElementById('function-selector-id');
    var source = getCurrentSourceText();
    var sourceDivElement = document.getElementById('source-text');
    var code = getCurrentCodeObject();
    var newHtml = "<pre class=\"prettyprint linenums\" id=\"source-text\">"
      + 'function ' + code.name + source + "</pre>";
    sourceDivElement.innerHTML = newHtml;
    try {
      // Wrap in try to work when offline.
      PR.prettyPrint();
    } catch (e) {
    }
    var sourceLineContainer = sourceDivElement.firstChild.firstChild;
    var lineCount = sourceLineContainer.childElementCount;
    var current = sourceLineContainer.firstChild;
    for (var i = 1; i < lineCount; ++i) {
      current.id = "source-line-" + i;
      current = current.nextElementSibling;
    }

    var asm = getCurrentAsmText();
    document.getElementById('asm-text').innerHTML = prepareAsm(asm);
  }

  function kindChangedHandler(element) {
    setKindByIndex(element.selectedIndex);
    processFileContent();
    functionChangedHandler();
  }

  function readLog(evt) {
    //Retrieve the first (and only!) File from the FileList object
    var f = evt.target.files[0];
    if (f) {
      var r = new FileReader();
      r.onload = function(e) {
        var file = evt.target.files[0];
        currentFunctionKind = "";
        fileContent = e.target.result;
        processFileContent();
        functionChangedHandler();
      }
      r.readAsText(f);
    } else {
      alert("Failed to load file");
    }
  }

  function buildFunctionKindSelector(kindSelectElement) {
    for (var x = 0; x < kinds.length; ++x) {
      var optionElement = document.createElement("option");
      optionElement.value = x;
      optionElement.text = kinds[x];
      kindSelectElement.add(optionElement, null);
    }
    kindSelectElement.selectedIndex = 1;
    setKindByIndex(1);
  }

  return {
    buildFunctionKindSelector: buildFunctionKindSelector,
    kindChangedHandler: kindChangedHandler,
    functionChangedHandler: functionChangedHandler,
    asmClick: asmClick,
    addressClick: addressClick,
    readLog: readLog
  };

})();

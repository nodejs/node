(function webpackUniversalModuleDefinition(root, factory) {
	if(typeof exports === 'object' && typeof module === 'object')
		module.exports = factory();
	else if(typeof define === 'function' && define.amd)
		define([], factory);
	else if(typeof exports === 'object')
		exports["sourceMap"] = factory();
	else
		root["sourceMap"] = factory();
})(this, function() {
return /******/ (function(modules) { // webpackBootstrap
/******/ 	// The module cache
/******/ 	var installedModules = {};
/******/
/******/ 	// The require function
/******/ 	function __webpack_require__(moduleId) {
/******/
/******/ 		// Check if module is in cache
/******/ 		if(installedModules[moduleId])
/******/ 			return installedModules[moduleId].exports;
/******/
/******/ 		// Create a new module (and put it into the cache)
/******/ 		var module = installedModules[moduleId] = {
/******/ 			exports: {},
/******/ 			id: moduleId,
/******/ 			loaded: false
/******/ 		};
/******/
/******/ 		// Execute the module function
/******/ 		modules[moduleId].call(module.exports, module, module.exports, __webpack_require__);
/******/
/******/ 		// Flag the module as loaded
/******/ 		module.loaded = true;
/******/
/******/ 		// Return the exports of the module
/******/ 		return module.exports;
/******/ 	}
/******/
/******/
/******/ 	// expose the modules object (__webpack_modules__)
/******/ 	__webpack_require__.m = modules;
/******/
/******/ 	// expose the module cache
/******/ 	__webpack_require__.c = installedModules;
/******/
/******/ 	// __webpack_public_path__
/******/ 	__webpack_require__.p = "";
/******/
/******/ 	// Load entry module and return exports
/******/ 	return __webpack_require__(0);
/******/ })
/************************************************************************/
/******/ ([
/* 0 */
/***/ function(module, exports, __webpack_require__) {

	/*
	 * Copyright 2009-2011 Mozilla Foundation and contributors
	 * Licensed under the New BSD license. See LICENSE.txt or:
	 * http://opensource.org/licenses/BSD-3-Clause
	 */
	exports.SourceMapGenerator = __webpack_require__(1).SourceMapGenerator;
	exports.SourceMapConsumer = __webpack_require__(7).SourceMapConsumer;
	exports.SourceNode = __webpack_require__(10).SourceNode;


/***/ },
/* 1 */
/***/ function(module, exports, __webpack_require__) {

	/* -*- Mode: js; js-indent-level: 2; -*- */
	/*
	 * Copyright 2011 Mozilla Foundation and contributors
	 * Licensed under the New BSD license. See LICENSE or:
	 * http://opensource.org/licenses/BSD-3-Clause
	 */
	
	var base64VLQ = __webpack_require__(2);
	var util = __webpack_require__(4);
	var ArraySet = __webpack_require__(5).ArraySet;
	var MappingList = __webpack_require__(6).MappingList;
	
	/**
	 * An instance of the SourceMapGenerator represents a source map which is
	 * being built incrementally. You may pass an object with the following
	 * properties:
	 *
	 *   - file: The filename of the generated source.
	 *   - sourceRoot: A root for all relative URLs in this source map.
	 */
	function SourceMapGenerator(aArgs) {
	  if (!aArgs) {
	    aArgs = {};
	  }
	  this._file = util.getArg(aArgs, 'file', null);
	  this._sourceRoot = util.getArg(aArgs, 'sourceRoot', null);
	  this._skipValidation = util.getArg(aArgs, 'skipValidation', false);
	  this._sources = new ArraySet();
	  this._names = new ArraySet();
	  this._mappings = new MappingList();
	  this._sourcesContents = null;
	}
	
	SourceMapGenerator.prototype._version = 3;
	
	/**
	 * Creates a new SourceMapGenerator based on a SourceMapConsumer
	 *
	 * @param aSourceMapConsumer The SourceMap.
	 */
	SourceMapGenerator.fromSourceMap =
	  function SourceMapGenerator_fromSourceMap(aSourceMapConsumer) {
	    var sourceRoot = aSourceMapConsumer.sourceRoot;
	    var generator = new SourceMapGenerator({
	      file: aSourceMapConsumer.file,
	      sourceRoot: sourceRoot
	    });
	    aSourceMapConsumer.eachMapping(function (mapping) {
	      var newMapping = {
	        generated: {
	          line: mapping.generatedLine,
	          column: mapping.generatedColumn
	        }
	      };
	
	      if (mapping.source != null) {
	        newMapping.source = mapping.source;
	        if (sourceRoot != null) {
	          newMapping.source = util.relative(sourceRoot, newMapping.source);
	        }
	
	        newMapping.original = {
	          line: mapping.originalLine,
	          column: mapping.originalColumn
	        };
	
	        if (mapping.name != null) {
	          newMapping.name = mapping.name;
	        }
	      }
	
	      generator.addMapping(newMapping);
	    });
	    aSourceMapConsumer.sources.forEach(function (sourceFile) {
	      var content = aSourceMapConsumer.sourceContentFor(sourceFile);
	      if (content != null) {
	        generator.setSourceContent(sourceFile, content);
	      }
	    });
	    return generator;
	  };
	
	/**
	 * Add a single mapping from original source line and column to the generated
	 * source's line and column for this source map being created. The mapping
	 * object should have the following properties:
	 *
	 *   - generated: An object with the generated line and column positions.
	 *   - original: An object with the original line and column positions.
	 *   - source: The original source file (relative to the sourceRoot).
	 *   - name: An optional original token name for this mapping.
	 */
	SourceMapGenerator.prototype.addMapping =
	  function SourceMapGenerator_addMapping(aArgs) {
	    var generated = util.getArg(aArgs, 'generated');
	    var original = util.getArg(aArgs, 'original', null);
	    var source = util.getArg(aArgs, 'source', null);
	    var name = util.getArg(aArgs, 'name', null);
	
	    if (!this._skipValidation) {
	      this._validateMapping(generated, original, source, name);
	    }
	
	    if (source != null) {
	      source = String(source);
	      if (!this._sources.has(source)) {
	        this._sources.add(source);
	      }
	    }
	
	    if (name != null) {
	      name = String(name);
	      if (!this._names.has(name)) {
	        this._names.add(name);
	      }
	    }
	
	    this._mappings.add({
	      generatedLine: generated.line,
	      generatedColumn: generated.column,
	      originalLine: original != null && original.line,
	      originalColumn: original != null && original.column,
	      source: source,
	      name: name
	    });
	  };
	
	/**
	 * Set the source content for a source file.
	 */
	SourceMapGenerator.prototype.setSourceContent =
	  function SourceMapGenerator_setSourceContent(aSourceFile, aSourceContent) {
	    var source = aSourceFile;
	    if (this._sourceRoot != null) {
	      source = util.relative(this._sourceRoot, source);
	    }
	
	    if (aSourceContent != null) {
	      // Add the source content to the _sourcesContents map.
	      // Create a new _sourcesContents map if the property is null.
	      if (!this._sourcesContents) {
	        this._sourcesContents = Object.create(null);
	      }
	      this._sourcesContents[util.toSetString(source)] = aSourceContent;
	    } else if (this._sourcesContents) {
	      // Remove the source file from the _sourcesContents map.
	      // If the _sourcesContents map is empty, set the property to null.
	      delete this._sourcesContents[util.toSetString(source)];
	      if (Object.keys(this._sourcesContents).length === 0) {
	        this._sourcesContents = null;
	      }
	    }
	  };
	
	/**
	 * Applies the mappings of a sub-source-map for a specific source file to the
	 * source map being generated. Each mapping to the supplied source file is
	 * rewritten using the supplied source map. Note: The resolution for the
	 * resulting mappings is the minimium of this map and the supplied map.
	 *
	 * @param aSourceMapConsumer The source map to be applied.
	 * @param aSourceFile Optional. The filename of the source file.
	 *        If omitted, SourceMapConsumer's file property will be used.
	 * @param aSourceMapPath Optional. The dirname of the path to the source map
	 *        to be applied. If relative, it is relative to the SourceMapConsumer.
	 *        This parameter is needed when the two source maps aren't in the same
	 *        directory, and the source map to be applied contains relative source
	 *        paths. If so, those relative source paths need to be rewritten
	 *        relative to the SourceMapGenerator.
	 */
	SourceMapGenerator.prototype.applySourceMap =
	  function SourceMapGenerator_applySourceMap(aSourceMapConsumer, aSourceFile, aSourceMapPath) {
	    var sourceFile = aSourceFile;
	    // If aSourceFile is omitted, we will use the file property of the SourceMap
	    if (aSourceFile == null) {
	      if (aSourceMapConsumer.file == null) {
	        throw new Error(
	          'SourceMapGenerator.prototype.applySourceMap requires either an explicit source file, ' +
	          'or the source map\'s "file" property. Both were omitted.'
	        );
	      }
	      sourceFile = aSourceMapConsumer.file;
	    }
	    var sourceRoot = this._sourceRoot;
	    // Make "sourceFile" relative if an absolute Url is passed.
	    if (sourceRoot != null) {
	      sourceFile = util.relative(sourceRoot, sourceFile);
	    }
	    // Applying the SourceMap can add and remove items from the sources and
	    // the names array.
	    var newSources = new ArraySet();
	    var newNames = new ArraySet();
	
	    // Find mappings for the "sourceFile"
	    this._mappings.unsortedForEach(function (mapping) {
	      if (mapping.source === sourceFile && mapping.originalLine != null) {
	        // Check if it can be mapped by the source map, then update the mapping.
	        var original = aSourceMapConsumer.originalPositionFor({
	          line: mapping.originalLine,
	          column: mapping.originalColumn
	        });
	        if (original.source != null) {
	          // Copy mapping
	          mapping.source = original.source;
	          if (aSourceMapPath != null) {
	            mapping.source = util.join(aSourceMapPath, mapping.source)
	          }
	          if (sourceRoot != null) {
	            mapping.source = util.relative(sourceRoot, mapping.source);
	          }
	          mapping.originalLine = original.line;
	          mapping.originalColumn = original.column;
	          if (original.name != null) {
	            mapping.name = original.name;
	          }
	        }
	      }
	
	      var source = mapping.source;
	      if (source != null && !newSources.has(source)) {
	        newSources.add(source);
	      }
	
	      var name = mapping.name;
	      if (name != null && !newNames.has(name)) {
	        newNames.add(name);
	      }
	
	    }, this);
	    this._sources = newSources;
	    this._names = newNames;
	
	    // Copy sourcesContents of applied map.
	    aSourceMapConsumer.sources.forEach(function (sourceFile) {
	      var content = aSourceMapConsumer.sourceContentFor(sourceFile);
	      if (content != null) {
	        if (aSourceMapPath != null) {
	          sourceFile = util.join(aSourceMapPath, sourceFile);
	        }
	        if (sourceRoot != null) {
	          sourceFile = util.relative(sourceRoot, sourceFile);
	        }
	        this.setSourceContent(sourceFile, content);
	      }
	    }, this);
	  };
	
	/**
	 * A mapping can have one of the three levels of data:
	 *
	 *   1. Just the generated position.
	 *   2. The Generated position, original position, and original source.
	 *   3. Generated and original position, original source, as well as a name
	 *      token.
	 *
	 * To maintain consistency, we validate that any new mapping being added falls
	 * in to one of these categories.
	 */
	SourceMapGenerator.prototype._validateMapping =
	  function SourceMapGenerator_validateMapping(aGenerated, aOriginal, aSource,
	                                              aName) {
	    if (aGenerated && 'line' in aGenerated && 'column' in aGenerated
	        && aGenerated.line > 0 && aGenerated.column >= 0
	        && !aOriginal && !aSource && !aName) {
	      // Case 1.
	      return;
	    }
	    else if (aGenerated && 'line' in aGenerated && 'column' in aGenerated
	             && aOriginal && 'line' in aOriginal && 'column' in aOriginal
	             && aGenerated.line > 0 && aGenerated.column >= 0
	             && aOriginal.line > 0 && aOriginal.column >= 0
	             && aSource) {
	      // Cases 2 and 3.
	      return;
	    }
	    else {
	      throw new Error('Invalid mapping: ' + JSON.stringify({
	        generated: aGenerated,
	        source: aSource,
	        original: aOriginal,
	        name: aName
	      }));
	    }
	  };
	
	/**
	 * Serialize the accumulated mappings in to the stream of base 64 VLQs
	 * specified by the source map format.
	 */
	SourceMapGenerator.prototype._serializeMappings =
	  function SourceMapGenerator_serializeMappings() {
	    var previousGeneratedColumn = 0;
	    var previousGeneratedLine = 1;
	    var previousOriginalColumn = 0;
	    var previousOriginalLine = 0;
	    var previousName = 0;
	    var previousSource = 0;
	    var result = '';
	    var next;
	    var mapping;
	    var nameIdx;
	    var sourceIdx;
	
	    var mappings = this._mappings.toArray();
	    for (var i = 0, len = mappings.length; i < len; i++) {
	      mapping = mappings[i];
	      next = ''
	
	      if (mapping.generatedLine !== previousGeneratedLine) {
	        previousGeneratedColumn = 0;
	        while (mapping.generatedLine !== previousGeneratedLine) {
	          next += ';';
	          previousGeneratedLine++;
	        }
	      }
	      else {
	        if (i > 0) {
	          if (!util.compareByGeneratedPositionsInflated(mapping, mappings[i - 1])) {
	            continue;
	          }
	          next += ',';
	        }
	      }
	
	      next += base64VLQ.encode(mapping.generatedColumn
	                                 - previousGeneratedColumn);
	      previousGeneratedColumn = mapping.generatedColumn;
	
	      if (mapping.source != null) {
	        sourceIdx = this._sources.indexOf(mapping.source);
	        next += base64VLQ.encode(sourceIdx - previousSource);
	        previousSource = sourceIdx;
	
	        // lines are stored 0-based in SourceMap spec version 3
	        next += base64VLQ.encode(mapping.originalLine - 1
	                                   - previousOriginalLine);
	        previousOriginalLine = mapping.originalLine - 1;
	
	        next += base64VLQ.encode(mapping.originalColumn
	                                   - previousOriginalColumn);
	        previousOriginalColumn = mapping.originalColumn;
	
	        if (mapping.name != null) {
	          nameIdx = this._names.indexOf(mapping.name);
	          next += base64VLQ.encode(nameIdx - previousName);
	          previousName = nameIdx;
	        }
	      }
	
	      result += next;
	    }
	
	    return result;
	  };
	
	SourceMapGenerator.prototype._generateSourcesContent =
	  function SourceMapGenerator_generateSourcesContent(aSources, aSourceRoot) {
	    return aSources.map(function (source) {
	      if (!this._sourcesContents) {
	        return null;
	      }
	      if (aSourceRoot != null) {
	        source = util.relative(aSourceRoot, source);
	      }
	      var key = util.toSetString(source);
	      return Object.prototype.hasOwnProperty.call(this._sourcesContents, key)
	        ? this._sourcesContents[key]
	        : null;
	    }, this);
	  };
	
	/**
	 * Externalize the source map.
	 */
	SourceMapGenerator.prototype.toJSON =
	  function SourceMapGenerator_toJSON() {
	    var map = {
	      version: this._version,
	      sources: this._sources.toArray(),
	      names: this._names.toArray(),
	      mappings: this._serializeMappings()
	    };
	    if (this._file != null) {
	      map.file = this._file;
	    }
	    if (this._sourceRoot != null) {
	      map.sourceRoot = this._sourceRoot;
	    }
	    if (this._sourcesContents) {
	      map.sourcesContent = this._generateSourcesContent(map.sources, map.sourceRoot);
	    }
	
	    return map;
	  };
	
	/**
	 * Render the source map being generated to a string.
	 */
	SourceMapGenerator.prototype.toString =
	  function SourceMapGenerator_toString() {
	    return JSON.stringify(this.toJSON());
	  };
	
	exports.SourceMapGenerator = SourceMapGenerator;


/***/ },
/* 2 */
/***/ function(module, exports, __webpack_require__) {

	/* -*- Mode: js; js-indent-level: 2; -*- */
	/*
	 * Copyright 2011 Mozilla Foundation and contributors
	 * Licensed under the New BSD license. See LICENSE or:
	 * http://opensource.org/licenses/BSD-3-Clause
	 *
	 * Based on the Base 64 VLQ implementation in Closure Compiler:
	 * https://code.google.com/p/closure-compiler/source/browse/trunk/src/com/google/debugging/sourcemap/Base64VLQ.java
	 *
	 * Copyright 2011 The Closure Compiler Authors. All rights reserved.
	 * Redistribution and use in source and binary forms, with or without
	 * modification, are permitted provided that the following conditions are
	 * met:
	 *
	 *  * Redistributions of source code must retain the above copyright
	 *    notice, this list of conditions and the following disclaimer.
	 *  * Redistributions in binary form must reproduce the above
	 *    copyright notice, this list of conditions and the following
	 *    disclaimer in the documentation and/or other materials provided
	 *    with the distribution.
	 *  * Neither the name of Google Inc. nor the names of its
	 *    contributors may be used to endorse or promote products derived
	 *    from this software without specific prior written permission.
	 *
	 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
	 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
	 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
	 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
	 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
	 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
	 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
	 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
	 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
	 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
	 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
	 */
	
	var base64 = __webpack_require__(3);
	
	// A single base 64 digit can contain 6 bits of data. For the base 64 variable
	// length quantities we use in the source map spec, the first bit is the sign,
	// the next four bits are the actual value, and the 6th bit is the
	// continuation bit. The continuation bit tells us whether there are more
	// digits in this value following this digit.
	//
	//   Continuation
	//   |    Sign
	//   |    |
	//   V    V
	//   101011
	
	var VLQ_BASE_SHIFT = 5;
	
	// binary: 100000
	var VLQ_BASE = 1 << VLQ_BASE_SHIFT;
	
	// binary: 011111
	var VLQ_BASE_MASK = VLQ_BASE - 1;
	
	// binary: 100000
	var VLQ_CONTINUATION_BIT = VLQ_BASE;
	
	/**
	 * Converts from a two-complement value to a value where the sign bit is
	 * placed in the least significant bit.  For example, as decimals:
	 *   1 becomes 2 (10 binary), -1 becomes 3 (11 binary)
	 *   2 becomes 4 (100 binary), -2 becomes 5 (101 binary)
	 */
	function toVLQSigned(aValue) {
	  return aValue < 0
	    ? ((-aValue) << 1) + 1
	    : (aValue << 1) + 0;
	}
	
	/**
	 * Converts to a two-complement value from a value where the sign bit is
	 * placed in the least significant bit.  For example, as decimals:
	 *   2 (10 binary) becomes 1, 3 (11 binary) becomes -1
	 *   4 (100 binary) becomes 2, 5 (101 binary) becomes -2
	 */
	function fromVLQSigned(aValue) {
	  var isNegative = (aValue & 1) === 1;
	  var shifted = aValue >> 1;
	  return isNegative
	    ? -shifted
	    : shifted;
	}
	
	/**
	 * Returns the base 64 VLQ encoded value.
	 */
	exports.encode = function base64VLQ_encode(aValue) {
	  var encoded = "";
	  var digit;
	
	  var vlq = toVLQSigned(aValue);
	
	  do {
	    digit = vlq & VLQ_BASE_MASK;
	    vlq >>>= VLQ_BASE_SHIFT;
	    if (vlq > 0) {
	      // There are still more digits in this value, so we must make sure the
	      // continuation bit is marked.
	      digit |= VLQ_CONTINUATION_BIT;
	    }
	    encoded += base64.encode(digit);
	  } while (vlq > 0);
	
	  return encoded;
	};
	
	/**
	 * Decodes the next base 64 VLQ value from the given string and returns the
	 * value and the rest of the string via the out parameter.
	 */
	exports.decode = function base64VLQ_decode(aStr, aIndex, aOutParam) {
	  var strLen = aStr.length;
	  var result = 0;
	  var shift = 0;
	  var continuation, digit;
	
	  do {
	    if (aIndex >= strLen) {
	      throw new Error("Expected more digits in base 64 VLQ value.");
	    }
	
	    digit = base64.decode(aStr.charCodeAt(aIndex++));
	    if (digit === -1) {
	      throw new Error("Invalid base64 digit: " + aStr.charAt(aIndex - 1));
	    }
	
	    continuation = !!(digit & VLQ_CONTINUATION_BIT);
	    digit &= VLQ_BASE_MASK;
	    result = result + (digit << shift);
	    shift += VLQ_BASE_SHIFT;
	  } while (continuation);
	
	  aOutParam.value = fromVLQSigned(result);
	  aOutParam.rest = aIndex;
	};


/***/ },
/* 3 */
/***/ function(module, exports) {

	/* -*- Mode: js; js-indent-level: 2; -*- */
	/*
	 * Copyright 2011 Mozilla Foundation and contributors
	 * Licensed under the New BSD license. See LICENSE or:
	 * http://opensource.org/licenses/BSD-3-Clause
	 */
	
	var intToCharMap = 'ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/'.split('');
	
	/**
	 * Encode an integer in the range of 0 to 63 to a single base 64 digit.
	 */
	exports.encode = function (number) {
	  if (0 <= number && number < intToCharMap.length) {
	    return intToCharMap[number];
	  }
	  throw new TypeError("Must be between 0 and 63: " + number);
	};
	
	/**
	 * Decode a single base 64 character code digit to an integer. Returns -1 on
	 * failure.
	 */
	exports.decode = function (charCode) {
	  var bigA = 65;     // 'A'
	  var bigZ = 90;     // 'Z'
	
	  var littleA = 97;  // 'a'
	  var littleZ = 122; // 'z'
	
	  var zero = 48;     // '0'
	  var nine = 57;     // '9'
	
	  var plus = 43;     // '+'
	  var slash = 47;    // '/'
	
	  var littleOffset = 26;
	  var numberOffset = 52;
	
	  // 0 - 25: ABCDEFGHIJKLMNOPQRSTUVWXYZ
	  if (bigA <= charCode && charCode <= bigZ) {
	    return (charCode - bigA);
	  }
	
	  // 26 - 51: abcdefghijklmnopqrstuvwxyz
	  if (littleA <= charCode && charCode <= littleZ) {
	    return (charCode - littleA + littleOffset);
	  }
	
	  // 52 - 61: 0123456789
	  if (zero <= charCode && charCode <= nine) {
	    return (charCode - zero + numberOffset);
	  }
	
	  // 62: +
	  if (charCode == plus) {
	    return 62;
	  }
	
	  // 63: /
	  if (charCode == slash) {
	    return 63;
	  }
	
	  // Invalid base64 digit.
	  return -1;
	};


/***/ },
/* 4 */
/***/ function(module, exports) {

	/* -*- Mode: js; js-indent-level: 2; -*- */
	/*
	 * Copyright 2011 Mozilla Foundation and contributors
	 * Licensed under the New BSD license. See LICENSE or:
	 * http://opensource.org/licenses/BSD-3-Clause
	 */
	
	/**
	 * This is a helper function for getting values from parameter/options
	 * objects.
	 *
	 * @param args The object we are extracting values from
	 * @param name The name of the property we are getting.
	 * @param defaultValue An optional value to return if the property is missing
	 * from the object. If this is not specified and the property is missing, an
	 * error will be thrown.
	 */
	function getArg(aArgs, aName, aDefaultValue) {
	  if (aName in aArgs) {
	    return aArgs[aName];
	  } else if (arguments.length === 3) {
	    return aDefaultValue;
	  } else {
	    throw new Error('"' + aName + '" is a required argument.');
	  }
	}
	exports.getArg = getArg;
	
	var urlRegexp = /^(?:([\w+\-.]+):)?\/\/(?:(\w+:\w+)@)?([\w.]*)(?::(\d+))?(\S*)$/;
	var dataUrlRegexp = /^data:.+\,.+$/;
	
	function urlParse(aUrl) {
	  var match = aUrl.match(urlRegexp);
	  if (!match) {
	    return null;
	  }
	  return {
	    scheme: match[1],
	    auth: match[2],
	    host: match[3],
	    port: match[4],
	    path: match[5]
	  };
	}
	exports.urlParse = urlParse;
	
	function urlGenerate(aParsedUrl) {
	  var url = '';
	  if (aParsedUrl.scheme) {
	    url += aParsedUrl.scheme + ':';
	  }
	  url += '//';
	  if (aParsedUrl.auth) {
	    url += aParsedUrl.auth + '@';
	  }
	  if (aParsedUrl.host) {
	    url += aParsedUrl.host;
	  }
	  if (aParsedUrl.port) {
	    url += ":" + aParsedUrl.port
	  }
	  if (aParsedUrl.path) {
	    url += aParsedUrl.path;
	  }
	  return url;
	}
	exports.urlGenerate = urlGenerate;
	
	/**
	 * Normalizes a path, or the path portion of a URL:
	 *
	 * - Replaces consequtive slashes with one slash.
	 * - Removes unnecessary '.' parts.
	 * - Removes unnecessary '<dir>/..' parts.
	 *
	 * Based on code in the Node.js 'path' core module.
	 *
	 * @param aPath The path or url to normalize.
	 */
	function normalize(aPath) {
	  var path = aPath;
	  var url = urlParse(aPath);
	  if (url) {
	    if (!url.path) {
	      return aPath;
	    }
	    path = url.path;
	  }
	  var isAbsolute = exports.isAbsolute(path);
	
	  var parts = path.split(/\/+/);
	  for (var part, up = 0, i = parts.length - 1; i >= 0; i--) {
	    part = parts[i];
	    if (part === '.') {
	      parts.splice(i, 1);
	    } else if (part === '..') {
	      up++;
	    } else if (up > 0) {
	      if (part === '') {
	        // The first part is blank if the path is absolute. Trying to go
	        // above the root is a no-op. Therefore we can remove all '..' parts
	        // directly after the root.
	        parts.splice(i + 1, up);
	        up = 0;
	      } else {
	        parts.splice(i, 2);
	        up--;
	      }
	    }
	  }
	  path = parts.join('/');
	
	  if (path === '') {
	    path = isAbsolute ? '/' : '.';
	  }
	
	  if (url) {
	    url.path = path;
	    return urlGenerate(url);
	  }
	  return path;
	}
	exports.normalize = normalize;
	
	/**
	 * Joins two paths/URLs.
	 *
	 * @param aRoot The root path or URL.
	 * @param aPath The path or URL to be joined with the root.
	 *
	 * - If aPath is a URL or a data URI, aPath is returned, unless aPath is a
	 *   scheme-relative URL: Then the scheme of aRoot, if any, is prepended
	 *   first.
	 * - Otherwise aPath is a path. If aRoot is a URL, then its path portion
	 *   is updated with the result and aRoot is returned. Otherwise the result
	 *   is returned.
	 *   - If aPath is absolute, the result is aPath.
	 *   - Otherwise the two paths are joined with a slash.
	 * - Joining for example 'http://' and 'www.example.com' is also supported.
	 */
	function join(aRoot, aPath) {
	  if (aRoot === "") {
	    aRoot = ".";
	  }
	  if (aPath === "") {
	    aPath = ".";
	  }
	  var aPathUrl = urlParse(aPath);
	  var aRootUrl = urlParse(aRoot);
	  if (aRootUrl) {
	    aRoot = aRootUrl.path || '/';
	  }
	
	  // `join(foo, '//www.example.org')`
	  if (aPathUrl && !aPathUrl.scheme) {
	    if (aRootUrl) {
	      aPathUrl.scheme = aRootUrl.scheme;
	    }
	    return urlGenerate(aPathUrl);
	  }
	
	  if (aPathUrl || aPath.match(dataUrlRegexp)) {
	    return aPath;
	  }
	
	  // `join('http://', 'www.example.com')`
	  if (aRootUrl && !aRootUrl.host && !aRootUrl.path) {
	    aRootUrl.host = aPath;
	    return urlGenerate(aRootUrl);
	  }
	
	  var joined = aPath.charAt(0) === '/'
	    ? aPath
	    : normalize(aRoot.replace(/\/+$/, '') + '/' + aPath);
	
	  if (aRootUrl) {
	    aRootUrl.path = joined;
	    return urlGenerate(aRootUrl);
	  }
	  return joined;
	}
	exports.join = join;
	
	exports.isAbsolute = function (aPath) {
	  return aPath.charAt(0) === '/' || !!aPath.match(urlRegexp);
	};
	
	/**
	 * Make a path relative to a URL or another path.
	 *
	 * @param aRoot The root path or URL.
	 * @param aPath The path or URL to be made relative to aRoot.
	 */
	function relative(aRoot, aPath) {
	  if (aRoot === "") {
	    aRoot = ".";
	  }
	
	  aRoot = aRoot.replace(/\/$/, '');
	
	  // It is possible for the path to be above the root. In this case, simply
	  // checking whether the root is a prefix of the path won't work. Instead, we
	  // need to remove components from the root one by one, until either we find
	  // a prefix that fits, or we run out of components to remove.
	  var level = 0;
	  while (aPath.indexOf(aRoot + '/') !== 0) {
	    var index = aRoot.lastIndexOf("/");
	    if (index < 0) {
	      return aPath;
	    }
	
	    // If the only part of the root that is left is the scheme (i.e. http://,
	    // file:///, etc.), one or more slashes (/), or simply nothing at all, we
	    // have exhausted all components, so the path is not relative to the root.
	    aRoot = aRoot.slice(0, index);
	    if (aRoot.match(/^([^\/]+:\/)?\/*$/)) {
	      return aPath;
	    }
	
	    ++level;
	  }
	
	  // Make sure we add a "../" for each component we removed from the root.
	  return Array(level + 1).join("../") + aPath.substr(aRoot.length + 1);
	}
	exports.relative = relative;
	
	var supportsNullProto = (function () {
	  var obj = Object.create(null);
	  return !('__proto__' in obj);
	}());
	
	function identity (s) {
	  return s;
	}
	
	/**
	 * Because behavior goes wacky when you set `__proto__` on objects, we
	 * have to prefix all the strings in our set with an arbitrary character.
	 *
	 * See https://github.com/mozilla/source-map/pull/31 and
	 * https://github.com/mozilla/source-map/issues/30
	 *
	 * @param String aStr
	 */
	function toSetString(aStr) {
	  if (isProtoString(aStr)) {
	    return '$' + aStr;
	  }
	
	  return aStr;
	}
	exports.toSetString = supportsNullProto ? identity : toSetString;
	
	function fromSetString(aStr) {
	  if (isProtoString(aStr)) {
	    return aStr.slice(1);
	  }
	
	  return aStr;
	}
	exports.fromSetString = supportsNullProto ? identity : fromSetString;
	
	function isProtoString(s) {
	  if (!s) {
	    return false;
	  }
	
	  var length = s.length;
	
	  if (length < 9 /* "__proto__".length */) {
	    return false;
	  }
	
	  if (s.charCodeAt(length - 1) !== 95  /* '_' */ ||
	      s.charCodeAt(length - 2) !== 95  /* '_' */ ||
	      s.charCodeAt(length - 3) !== 111 /* 'o' */ ||
	      s.charCodeAt(length - 4) !== 116 /* 't' */ ||
	      s.charCodeAt(length - 5) !== 111 /* 'o' */ ||
	      s.charCodeAt(length - 6) !== 114 /* 'r' */ ||
	      s.charCodeAt(length - 7) !== 112 /* 'p' */ ||
	      s.charCodeAt(length - 8) !== 95  /* '_' */ ||
	      s.charCodeAt(length - 9) !== 95  /* '_' */) {
	    return false;
	  }
	
	  for (var i = length - 10; i >= 0; i--) {
	    if (s.charCodeAt(i) !== 36 /* '$' */) {
	      return false;
	    }
	  }
	
	  return true;
	}
	
	/**
	 * Comparator between two mappings where the original positions are compared.
	 *
	 * Optionally pass in `true` as `onlyCompareGenerated` to consider two
	 * mappings with the same original source/line/column, but different generated
	 * line and column the same. Useful when searching for a mapping with a
	 * stubbed out mapping.
	 */
	function compareByOriginalPositions(mappingA, mappingB, onlyCompareOriginal) {
	  var cmp = mappingA.source - mappingB.source;
	  if (cmp !== 0) {
	    return cmp;
	  }
	
	  cmp = mappingA.originalLine - mappingB.originalLine;
	  if (cmp !== 0) {
	    return cmp;
	  }
	
	  cmp = mappingA.originalColumn - mappingB.originalColumn;
	  if (cmp !== 0 || onlyCompareOriginal) {
	    return cmp;
	  }
	
	  cmp = mappingA.generatedColumn - mappingB.generatedColumn;
	  if (cmp !== 0) {
	    return cmp;
	  }
	
	  cmp = mappingA.generatedLine - mappingB.generatedLine;
	  if (cmp !== 0) {
	    return cmp;
	  }
	
	  return mappingA.name - mappingB.name;
	}
	exports.compareByOriginalPositions = compareByOriginalPositions;
	
	/**
	 * Comparator between two mappings with deflated source and name indices where
	 * the generated positions are compared.
	 *
	 * Optionally pass in `true` as `onlyCompareGenerated` to consider two
	 * mappings with the same generated line and column, but different
	 * source/name/original line and column the same. Useful when searching for a
	 * mapping with a stubbed out mapping.
	 */
	function compareByGeneratedPositionsDeflated(mappingA, mappingB, onlyCompareGenerated) {
	  var cmp = mappingA.generatedLine - mappingB.generatedLine;
	  if (cmp !== 0) {
	    return cmp;
	  }
	
	  cmp = mappingA.generatedColumn - mappingB.generatedColumn;
	  if (cmp !== 0 || onlyCompareGenerated) {
	    return cmp;
	  }
	
	  cmp = mappingA.source - mappingB.source;
	  if (cmp !== 0) {
	    return cmp;
	  }
	
	  cmp = mappingA.originalLine - mappingB.originalLine;
	  if (cmp !== 0) {
	    return cmp;
	  }
	
	  cmp = mappingA.originalColumn - mappingB.originalColumn;
	  if (cmp !== 0) {
	    return cmp;
	  }
	
	  return mappingA.name - mappingB.name;
	}
	exports.compareByGeneratedPositionsDeflated = compareByGeneratedPositionsDeflated;
	
	function strcmp(aStr1, aStr2) {
	  if (aStr1 === aStr2) {
	    return 0;
	  }
	
	  if (aStr1 > aStr2) {
	    return 1;
	  }
	
	  return -1;
	}
	
	/**
	 * Comparator between two mappings with inflated source and name strings where
	 * the generated positions are compared.
	 */
	function compareByGeneratedPositionsInflated(mappingA, mappingB) {
	  var cmp = mappingA.generatedLine - mappingB.generatedLine;
	  if (cmp !== 0) {
	    return cmp;
	  }
	
	  cmp = mappingA.generatedColumn - mappingB.generatedColumn;
	  if (cmp !== 0) {
	    return cmp;
	  }
	
	  cmp = strcmp(mappingA.source, mappingB.source);
	  if (cmp !== 0) {
	    return cmp;
	  }
	
	  cmp = mappingA.originalLine - mappingB.originalLine;
	  if (cmp !== 0) {
	    return cmp;
	  }
	
	  cmp = mappingA.originalColumn - mappingB.originalColumn;
	  if (cmp !== 0) {
	    return cmp;
	  }
	
	  return strcmp(mappingA.name, mappingB.name);
	}
	exports.compareByGeneratedPositionsInflated = compareByGeneratedPositionsInflated;


/***/ },
/* 5 */
/***/ function(module, exports, __webpack_require__) {

	/* -*- Mode: js; js-indent-level: 2; -*- */
	/*
	 * Copyright 2011 Mozilla Foundation and contributors
	 * Licensed under the New BSD license. See LICENSE or:
	 * http://opensource.org/licenses/BSD-3-Clause
	 */
	
	var util = __webpack_require__(4);
	var has = Object.prototype.hasOwnProperty;
	
	/**
	 * A data structure which is a combination of an array and a set. Adding a new
	 * member is O(1), testing for membership is O(1), and finding the index of an
	 * element is O(1). Removing elements from the set is not supported. Only
	 * strings are supported for membership.
	 */
	function ArraySet() {
	  this._array = [];
	  this._set = Object.create(null);
	}
	
	/**
	 * Static method for creating ArraySet instances from an existing array.
	 */
	ArraySet.fromArray = function ArraySet_fromArray(aArray, aAllowDuplicates) {
	  var set = new ArraySet();
	  for (var i = 0, len = aArray.length; i < len; i++) {
	    set.add(aArray[i], aAllowDuplicates);
	  }
	  return set;
	};
	
	/**
	 * Return how many unique items are in this ArraySet. If duplicates have been
	 * added, than those do not count towards the size.
	 *
	 * @returns Number
	 */
	ArraySet.prototype.size = function ArraySet_size() {
	  return Object.getOwnPropertyNames(this._set).length;
	};
	
	/**
	 * Add the given string to this set.
	 *
	 * @param String aStr
	 */
	ArraySet.prototype.add = function ArraySet_add(aStr, aAllowDuplicates) {
	  var sStr = util.toSetString(aStr);
	  var isDuplicate = has.call(this._set, sStr);
	  var idx = this._array.length;
	  if (!isDuplicate || aAllowDuplicates) {
	    this._array.push(aStr);
	  }
	  if (!isDuplicate) {
	    this._set[sStr] = idx;
	  }
	};
	
	/**
	 * Is the given string a member of this set?
	 *
	 * @param String aStr
	 */
	ArraySet.prototype.has = function ArraySet_has(aStr) {
	  var sStr = util.toSetString(aStr);
	  return has.call(this._set, sStr);
	};
	
	/**
	 * What is the index of the given string in the array?
	 *
	 * @param String aStr
	 */
	ArraySet.prototype.indexOf = function ArraySet_indexOf(aStr) {
	  var sStr = util.toSetString(aStr);
	  if (has.call(this._set, sStr)) {
	    return this._set[sStr];
	  }
	  throw new Error('"' + aStr + '" is not in the set.');
	};
	
	/**
	 * What is the element at the given index?
	 *
	 * @param Number aIdx
	 */
	ArraySet.prototype.at = function ArraySet_at(aIdx) {
	  if (aIdx >= 0 && aIdx < this._array.length) {
	    return this._array[aIdx];
	  }
	  throw new Error('No element indexed by ' + aIdx);
	};
	
	/**
	 * Returns the array representation of this set (which has the proper indices
	 * indicated by indexOf). Note that this is a copy of the internal array used
	 * for storing the members so that no one can mess with internal state.
	 */
	ArraySet.prototype.toArray = function ArraySet_toArray() {
	  return this._array.slice();
	};
	
	exports.ArraySet = ArraySet;


/***/ },
/* 6 */
/***/ function(module, exports, __webpack_require__) {

	/* -*- Mode: js; js-indent-level: 2; -*- */
	/*
	 * Copyright 2014 Mozilla Foundation and contributors
	 * Licensed under the New BSD license. See LICENSE or:
	 * http://opensource.org/licenses/BSD-3-Clause
	 */
	
	var util = __webpack_require__(4);
	
	/**
	 * Determine whether mappingB is after mappingA with respect to generated
	 * position.
	 */
	function generatedPositionAfter(mappingA, mappingB) {
	  // Optimized for most common case
	  var lineA = mappingA.generatedLine;
	  var lineB = mappingB.generatedLine;
	  var columnA = mappingA.generatedColumn;
	  var columnB = mappingB.generatedColumn;
	  return lineB > lineA || lineB == lineA && columnB >= columnA ||
	         util.compareByGeneratedPositionsInflated(mappingA, mappingB) <= 0;
	}
	
	/**
	 * A data structure to provide a sorted view of accumulated mappings in a
	 * performance conscious manner. It trades a neglibable overhead in general
	 * case for a large speedup in case of mappings being added in order.
	 */
	function MappingList() {
	  this._array = [];
	  this._sorted = true;
	  // Serves as infimum
	  this._last = {generatedLine: -1, generatedColumn: 0};
	}
	
	/**
	 * Iterate through internal items. This method takes the same arguments that
	 * `Array.prototype.forEach` takes.
	 *
	 * NOTE: The order of the mappings is NOT guaranteed.
	 */
	MappingList.prototype.unsortedForEach =
	  function MappingList_forEach(aCallback, aThisArg) {
	    this._array.forEach(aCallback, aThisArg);
	  };
	
	/**
	 * Add the given source mapping.
	 *
	 * @param Object aMapping
	 */
	MappingList.prototype.add = function MappingList_add(aMapping) {
	  if (generatedPositionAfter(this._last, aMapping)) {
	    this._last = aMapping;
	    this._array.push(aMapping);
	  } else {
	    this._sorted = false;
	    this._array.push(aMapping);
	  }
	};
	
	/**
	 * Returns the flat, sorted array of mappings. The mappings are sorted by
	 * generated position.
	 *
	 * WARNING: This method returns internal data without copying, for
	 * performance. The return value must NOT be mutated, and should be treated as
	 * an immutable borrow. If you want to take ownership, you must make your own
	 * copy.
	 */
	MappingList.prototype.toArray = function MappingList_toArray() {
	  if (!this._sorted) {
	    this._array.sort(util.compareByGeneratedPositionsInflated);
	    this._sorted = true;
	  }
	  return this._array;
	};
	
	exports.MappingList = MappingList;


/***/ },
/* 7 */
/***/ function(module, exports, __webpack_require__) {

	/* -*- Mode: js; js-indent-level: 2; -*- */
	/*
	 * Copyright 2011 Mozilla Foundation and contributors
	 * Licensed under the New BSD license. See LICENSE or:
	 * http://opensource.org/licenses/BSD-3-Clause
	 */
	
	var util = __webpack_require__(4);
	var binarySearch = __webpack_require__(8);
	var ArraySet = __webpack_require__(5).ArraySet;
	var base64VLQ = __webpack_require__(2);
	var quickSort = __webpack_require__(9).quickSort;
	
	function SourceMapConsumer(aSourceMap) {
	  var sourceMap = aSourceMap;
	  if (typeof aSourceMap === 'string') {
	    sourceMap = JSON.parse(aSourceMap.replace(/^\)\]\}'/, ''));
	  }
	
	  return sourceMap.sections != null
	    ? new IndexedSourceMapConsumer(sourceMap)
	    : new BasicSourceMapConsumer(sourceMap);
	}
	
	SourceMapConsumer.fromSourceMap = function(aSourceMap) {
	  return BasicSourceMapConsumer.fromSourceMap(aSourceMap);
	}
	
	/**
	 * The version of the source mapping spec that we are consuming.
	 */
	SourceMapConsumer.prototype._version = 3;
	
	// `__generatedMappings` and `__originalMappings` are arrays that hold the
	// parsed mapping coordinates from the source map's "mappings" attribute. They
	// are lazily instantiated, accessed via the `_generatedMappings` and
	// `_originalMappings` getters respectively, and we only parse the mappings
	// and create these arrays once queried for a source location. We jump through
	// these hoops because there can be many thousands of mappings, and parsing
	// them is expensive, so we only want to do it if we must.
	//
	// Each object in the arrays is of the form:
	//
	//     {
	//       generatedLine: The line number in the generated code,
	//       generatedColumn: The column number in the generated code,
	//       source: The path to the original source file that generated this
	//               chunk of code,
	//       originalLine: The line number in the original source that
	//                     corresponds to this chunk of generated code,
	//       originalColumn: The column number in the original source that
	//                       corresponds to this chunk of generated code,
	//       name: The name of the original symbol which generated this chunk of
	//             code.
	//     }
	//
	// All properties except for `generatedLine` and `generatedColumn` can be
	// `null`.
	//
	// `_generatedMappings` is ordered by the generated positions.
	//
	// `_originalMappings` is ordered by the original positions.
	
	SourceMapConsumer.prototype.__generatedMappings = null;
	Object.defineProperty(SourceMapConsumer.prototype, '_generatedMappings', {
	  get: function () {
	    if (!this.__generatedMappings) {
	      this._parseMappings(this._mappings, this.sourceRoot);
	    }
	
	    return this.__generatedMappings;
	  }
	});
	
	SourceMapConsumer.prototype.__originalMappings = null;
	Object.defineProperty(SourceMapConsumer.prototype, '_originalMappings', {
	  get: function () {
	    if (!this.__originalMappings) {
	      this._parseMappings(this._mappings, this.sourceRoot);
	    }
	
	    return this.__originalMappings;
	  }
	});
	
	SourceMapConsumer.prototype._charIsMappingSeparator =
	  function SourceMapConsumer_charIsMappingSeparator(aStr, index) {
	    var c = aStr.charAt(index);
	    return c === ";" || c === ",";
	  };
	
	/**
	 * Parse the mappings in a string in to a data structure which we can easily
	 * query (the ordered arrays in the `this.__generatedMappings` and
	 * `this.__originalMappings` properties).
	 */
	SourceMapConsumer.prototype._parseMappings =
	  function SourceMapConsumer_parseMappings(aStr, aSourceRoot) {
	    throw new Error("Subclasses must implement _parseMappings");
	  };
	
	SourceMapConsumer.GENERATED_ORDER = 1;
	SourceMapConsumer.ORIGINAL_ORDER = 2;
	
	SourceMapConsumer.GREATEST_LOWER_BOUND = 1;
	SourceMapConsumer.LEAST_UPPER_BOUND = 2;
	
	/**
	 * Iterate over each mapping between an original source/line/column and a
	 * generated line/column in this source map.
	 *
	 * @param Function aCallback
	 *        The function that is called with each mapping.
	 * @param Object aContext
	 *        Optional. If specified, this object will be the value of `this` every
	 *        time that `aCallback` is called.
	 * @param aOrder
	 *        Either `SourceMapConsumer.GENERATED_ORDER` or
	 *        `SourceMapConsumer.ORIGINAL_ORDER`. Specifies whether you want to
	 *        iterate over the mappings sorted by the generated file's line/column
	 *        order or the original's source/line/column order, respectively. Defaults to
	 *        `SourceMapConsumer.GENERATED_ORDER`.
	 */
	SourceMapConsumer.prototype.eachMapping =
	  function SourceMapConsumer_eachMapping(aCallback, aContext, aOrder) {
	    var context = aContext || null;
	    var order = aOrder || SourceMapConsumer.GENERATED_ORDER;
	
	    var mappings;
	    switch (order) {
	    case SourceMapConsumer.GENERATED_ORDER:
	      mappings = this._generatedMappings;
	      break;
	    case SourceMapConsumer.ORIGINAL_ORDER:
	      mappings = this._originalMappings;
	      break;
	    default:
	      throw new Error("Unknown order of iteration.");
	    }
	
	    var sourceRoot = this.sourceRoot;
	    mappings.map(function (mapping) {
	      var source = mapping.source === null ? null : this._sources.at(mapping.source);
	      if (source != null && sourceRoot != null) {
	        source = util.join(sourceRoot, source);
	      }
	      return {
	        source: source,
	        generatedLine: mapping.generatedLine,
	        generatedColumn: mapping.generatedColumn,
	        originalLine: mapping.originalLine,
	        originalColumn: mapping.originalColumn,
	        name: mapping.name === null ? null : this._names.at(mapping.name)
	      };
	    }, this).forEach(aCallback, context);
	  };
	
	/**
	 * Returns all generated line and column information for the original source,
	 * line, and column provided. If no column is provided, returns all mappings
	 * corresponding to a either the line we are searching for or the next
	 * closest line that has any mappings. Otherwise, returns all mappings
	 * corresponding to the given line and either the column we are searching for
	 * or the next closest column that has any offsets.
	 *
	 * The only argument is an object with the following properties:
	 *
	 *   - source: The filename of the original source.
	 *   - line: The line number in the original source.
	 *   - column: Optional. the column number in the original source.
	 *
	 * and an array of objects is returned, each with the following properties:
	 *
	 *   - line: The line number in the generated source, or null.
	 *   - column: The column number in the generated source, or null.
	 */
	SourceMapConsumer.prototype.allGeneratedPositionsFor =
	  function SourceMapConsumer_allGeneratedPositionsFor(aArgs) {
	    var line = util.getArg(aArgs, 'line');
	
	    // When there is no exact match, BasicSourceMapConsumer.prototype._findMapping
	    // returns the index of the closest mapping less than the needle. By
	    // setting needle.originalColumn to 0, we thus find the last mapping for
	    // the given line, provided such a mapping exists.
	    var needle = {
	      source: util.getArg(aArgs, 'source'),
	      originalLine: line,
	      originalColumn: util.getArg(aArgs, 'column', 0)
	    };
	
	    if (this.sourceRoot != null) {
	      needle.source = util.relative(this.sourceRoot, needle.source);
	    }
	    if (!this._sources.has(needle.source)) {
	      return [];
	    }
	    needle.source = this._sources.indexOf(needle.source);
	
	    var mappings = [];
	
	    var index = this._findMapping(needle,
	                                  this._originalMappings,
	                                  "originalLine",
	                                  "originalColumn",
	                                  util.compareByOriginalPositions,
	                                  binarySearch.LEAST_UPPER_BOUND);
	    if (index >= 0) {
	      var mapping = this._originalMappings[index];
	
	      if (aArgs.column === undefined) {
	        var originalLine = mapping.originalLine;
	
	        // Iterate until either we run out of mappings, or we run into
	        // a mapping for a different line than the one we found. Since
	        // mappings are sorted, this is guaranteed to find all mappings for
	        // the line we found.
	        while (mapping && mapping.originalLine === originalLine) {
	          mappings.push({
	            line: util.getArg(mapping, 'generatedLine', null),
	            column: util.getArg(mapping, 'generatedColumn', null),
	            lastColumn: util.getArg(mapping, 'lastGeneratedColumn', null)
	          });
	
	          mapping = this._originalMappings[++index];
	        }
	      } else {
	        var originalColumn = mapping.originalColumn;
	
	        // Iterate until either we run out of mappings, or we run into
	        // a mapping for a different line than the one we were searching for.
	        // Since mappings are sorted, this is guaranteed to find all mappings for
	        // the line we are searching for.
	        while (mapping &&
	               mapping.originalLine === line &&
	               mapping.originalColumn == originalColumn) {
	          mappings.push({
	            line: util.getArg(mapping, 'generatedLine', null),
	            column: util.getArg(mapping, 'generatedColumn', null),
	            lastColumn: util.getArg(mapping, 'lastGeneratedColumn', null)
	          });
	
	          mapping = this._originalMappings[++index];
	        }
	      }
	    }
	
	    return mappings;
	  };
	
	exports.SourceMapConsumer = SourceMapConsumer;
	
	/**
	 * A BasicSourceMapConsumer instance represents a parsed source map which we can
	 * query for information about the original file positions by giving it a file
	 * position in the generated source.
	 *
	 * The only parameter is the raw source map (either as a JSON string, or
	 * already parsed to an object). According to the spec, source maps have the
	 * following attributes:
	 *
	 *   - version: Which version of the source map spec this map is following.
	 *   - sources: An array of URLs to the original source files.
	 *   - names: An array of identifiers which can be referrenced by individual mappings.
	 *   - sourceRoot: Optional. The URL root from which all sources are relative.
	 *   - sourcesContent: Optional. An array of contents of the original source files.
	 *   - mappings: A string of base64 VLQs which contain the actual mappings.
	 *   - file: Optional. The generated file this source map is associated with.
	 *
	 * Here is an example source map, taken from the source map spec[0]:
	 *
	 *     {
	 *       version : 3,
	 *       file: "out.js",
	 *       sourceRoot : "",
	 *       sources: ["foo.js", "bar.js"],
	 *       names: ["src", "maps", "are", "fun"],
	 *       mappings: "AA,AB;;ABCDE;"
	 *     }
	 *
	 * [0]: https://docs.google.com/document/d/1U1RGAehQwRypUTovF1KRlpiOFze0b-_2gc6fAH0KY0k/edit?pli=1#
	 */
	function BasicSourceMapConsumer(aSourceMap) {
	  var sourceMap = aSourceMap;
	  if (typeof aSourceMap === 'string') {
	    sourceMap = JSON.parse(aSourceMap.replace(/^\)\]\}'/, ''));
	  }
	
	  var version = util.getArg(sourceMap, 'version');
	  var sources = util.getArg(sourceMap, 'sources');
	  // Sass 3.3 leaves out the 'names' array, so we deviate from the spec (which
	  // requires the array) to play nice here.
	  var names = util.getArg(sourceMap, 'names', []);
	  var sourceRoot = util.getArg(sourceMap, 'sourceRoot', null);
	  var sourcesContent = util.getArg(sourceMap, 'sourcesContent', null);
	  var mappings = util.getArg(sourceMap, 'mappings');
	  var file = util.getArg(sourceMap, 'file', null);
	
	  // Once again, Sass deviates from the spec and supplies the version as a
	  // string rather than a number, so we use loose equality checking here.
	  if (version != this._version) {
	    throw new Error('Unsupported version: ' + version);
	  }
	
	  sources = sources
	    .map(String)
	    // Some source maps produce relative source paths like "./foo.js" instead of
	    // "foo.js".  Normalize these first so that future comparisons will succeed.
	    // See bugzil.la/1090768.
	    .map(util.normalize)
	    // Always ensure that absolute sources are internally stored relative to
	    // the source root, if the source root is absolute. Not doing this would
	    // be particularly problematic when the source root is a prefix of the
	    // source (valid, but why??). See github issue #199 and bugzil.la/1188982.
	    .map(function (source) {
	      return sourceRoot && util.isAbsolute(sourceRoot) && util.isAbsolute(source)
	        ? util.relative(sourceRoot, source)
	        : source;
	    });
	
	  // Pass `true` below to allow duplicate names and sources. While source maps
	  // are intended to be compressed and deduplicated, the TypeScript compiler
	  // sometimes generates source maps with duplicates in them. See Github issue
	  // #72 and bugzil.la/889492.
	  this._names = ArraySet.fromArray(names.map(String), true);
	  this._sources = ArraySet.fromArray(sources, true);
	
	  this.sourceRoot = sourceRoot;
	  this.sourcesContent = sourcesContent;
	  this._mappings = mappings;
	  this.file = file;
	}
	
	BasicSourceMapConsumer.prototype = Object.create(SourceMapConsumer.prototype);
	BasicSourceMapConsumer.prototype.consumer = SourceMapConsumer;
	
	/**
	 * Create a BasicSourceMapConsumer from a SourceMapGenerator.
	 *
	 * @param SourceMapGenerator aSourceMap
	 *        The source map that will be consumed.
	 * @returns BasicSourceMapConsumer
	 */
	BasicSourceMapConsumer.fromSourceMap =
	  function SourceMapConsumer_fromSourceMap(aSourceMap) {
	    var smc = Object.create(BasicSourceMapConsumer.prototype);
	
	    var names = smc._names = ArraySet.fromArray(aSourceMap._names.toArray(), true);
	    var sources = smc._sources = ArraySet.fromArray(aSourceMap._sources.toArray(), true);
	    smc.sourceRoot = aSourceMap._sourceRoot;
	    smc.sourcesContent = aSourceMap._generateSourcesContent(smc._sources.toArray(),
	                                                            smc.sourceRoot);
	    smc.file = aSourceMap._file;
	
	    // Because we are modifying the entries (by converting string sources and
	    // names to indices into the sources and names ArraySets), we have to make
	    // a copy of the entry or else bad things happen. Shared mutable state
	    // strikes again! See github issue #191.
	
	    var generatedMappings = aSourceMap._mappings.toArray().slice();
	    var destGeneratedMappings = smc.__generatedMappings = [];
	    var destOriginalMappings = smc.__originalMappings = [];
	
	    for (var i = 0, length = generatedMappings.length; i < length; i++) {
	      var srcMapping = generatedMappings[i];
	      var destMapping = new Mapping;
	      destMapping.generatedLine = srcMapping.generatedLine;
	      destMapping.generatedColumn = srcMapping.generatedColumn;
	
	      if (srcMapping.source) {
	        destMapping.source = sources.indexOf(srcMapping.source);
	        destMapping.originalLine = srcMapping.originalLine;
	        destMapping.originalColumn = srcMapping.originalColumn;
	
	        if (srcMapping.name) {
	          destMapping.name = names.indexOf(srcMapping.name);
	        }
	
	        destOriginalMappings.push(destMapping);
	      }
	
	      destGeneratedMappings.push(destMapping);
	    }
	
	    quickSort(smc.__originalMappings, util.compareByOriginalPositions);
	
	    return smc;
	  };
	
	/**
	 * The version of the source mapping spec that we are consuming.
	 */
	BasicSourceMapConsumer.prototype._version = 3;
	
	/**
	 * The list of original sources.
	 */
	Object.defineProperty(BasicSourceMapConsumer.prototype, 'sources', {
	  get: function () {
	    return this._sources.toArray().map(function (s) {
	      return this.sourceRoot != null ? util.join(this.sourceRoot, s) : s;
	    }, this);
	  }
	});
	
	/**
	 * Provide the JIT with a nice shape / hidden class.
	 */
	function Mapping() {
	  this.generatedLine = 0;
	  this.generatedColumn = 0;
	  this.source = null;
	  this.originalLine = null;
	  this.originalColumn = null;
	  this.name = null;
	}
	
	/**
	 * Parse the mappings in a string in to a data structure which we can easily
	 * query (the ordered arrays in the `this.__generatedMappings` and
	 * `this.__originalMappings` properties).
	 */
	BasicSourceMapConsumer.prototype._parseMappings =
	  function SourceMapConsumer_parseMappings(aStr, aSourceRoot) {
	    var generatedLine = 1;
	    var previousGeneratedColumn = 0;
	    var previousOriginalLine = 0;
	    var previousOriginalColumn = 0;
	    var previousSource = 0;
	    var previousName = 0;
	    var length = aStr.length;
	    var index = 0;
	    var cachedSegments = {};
	    var temp = {};
	    var originalMappings = [];
	    var generatedMappings = [];
	    var mapping, str, segment, end, value;
	
	    while (index < length) {
	      if (aStr.charAt(index) === ';') {
	        generatedLine++;
	        index++;
	        previousGeneratedColumn = 0;
	      }
	      else if (aStr.charAt(index) === ',') {
	        index++;
	      }
	      else {
	        mapping = new Mapping();
	        mapping.generatedLine = generatedLine;
	
	        // Because each offset is encoded relative to the previous one,
	        // many segments often have the same encoding. We can exploit this
	        // fact by caching the parsed variable length fields of each segment,
	        // allowing us to avoid a second parse if we encounter the same
	        // segment again.
	        for (end = index; end < length; end++) {
	          if (this._charIsMappingSeparator(aStr, end)) {
	            break;
	          }
	        }
	        str = aStr.slice(index, end);
	
	        segment = cachedSegments[str];
	        if (segment) {
	          index += str.length;
	        } else {
	          segment = [];
	          while (index < end) {
	            base64VLQ.decode(aStr, index, temp);
	            value = temp.value;
	            index = temp.rest;
	            segment.push(value);
	          }
	
	          if (segment.length === 2) {
	            throw new Error('Found a source, but no line and column');
	          }
	
	          if (segment.length === 3) {
	            throw new Error('Found a source and line, but no column');
	          }
	
	          cachedSegments[str] = segment;
	        }
	
	        // Generated column.
	        mapping.generatedColumn = previousGeneratedColumn + segment[0];
	        previousGeneratedColumn = mapping.generatedColumn;
	
	        if (segment.length > 1) {
	          // Original source.
	          mapping.source = previousSource + segment[1];
	          previousSource += segment[1];
	
	          // Original line.
	          mapping.originalLine = previousOriginalLine + segment[2];
	          previousOriginalLine = mapping.originalLine;
	          // Lines are stored 0-based
	          mapping.originalLine += 1;
	
	          // Original column.
	          mapping.originalColumn = previousOriginalColumn + segment[3];
	          previousOriginalColumn = mapping.originalColumn;
	
	          if (segment.length > 4) {
	            // Original name.
	            mapping.name = previousName + segment[4];
	            previousName += segment[4];
	          }
	        }
	
	        generatedMappings.push(mapping);
	        if (typeof mapping.originalLine === 'number') {
	          originalMappings.push(mapping);
	        }
	      }
	    }
	
	    quickSort(generatedMappings, util.compareByGeneratedPositionsDeflated);
	    this.__generatedMappings = generatedMappings;
	
	    quickSort(originalMappings, util.compareByOriginalPositions);
	    this.__originalMappings = originalMappings;
	  };
	
	/**
	 * Find the mapping that best matches the hypothetical "needle" mapping that
	 * we are searching for in the given "haystack" of mappings.
	 */
	BasicSourceMapConsumer.prototype._findMapping =
	  function SourceMapConsumer_findMapping(aNeedle, aMappings, aLineName,
	                                         aColumnName, aComparator, aBias) {
	    // To return the position we are searching for, we must first find the
	    // mapping for the given position and then return the opposite position it
	    // points to. Because the mappings are sorted, we can use binary search to
	    // find the best mapping.
	
	    if (aNeedle[aLineName] <= 0) {
	      throw new TypeError('Line must be greater than or equal to 1, got '
	                          + aNeedle[aLineName]);
	    }
	    if (aNeedle[aColumnName] < 0) {
	      throw new TypeError('Column must be greater than or equal to 0, got '
	                          + aNeedle[aColumnName]);
	    }
	
	    return binarySearch.search(aNeedle, aMappings, aComparator, aBias);
	  };
	
	/**
	 * Compute the last column for each generated mapping. The last column is
	 * inclusive.
	 */
	BasicSourceMapConsumer.prototype.computeColumnSpans =
	  function SourceMapConsumer_computeColumnSpans() {
	    for (var index = 0; index < this._generatedMappings.length; ++index) {
	      var mapping = this._generatedMappings[index];
	
	      // Mappings do not contain a field for the last generated columnt. We
	      // can come up with an optimistic estimate, however, by assuming that
	      // mappings are contiguous (i.e. given two consecutive mappings, the
	      // first mapping ends where the second one starts).
	      if (index + 1 < this._generatedMappings.length) {
	        var nextMapping = this._generatedMappings[index + 1];
	
	        if (mapping.generatedLine === nextMapping.generatedLine) {
	          mapping.lastGeneratedColumn = nextMapping.generatedColumn - 1;
	          continue;
	        }
	      }
	
	      // The last mapping for each line spans the entire line.
	      mapping.lastGeneratedColumn = Infinity;
	    }
	  };
	
	/**
	 * Returns the original source, line, and column information for the generated
	 * source's line and column positions provided. The only argument is an object
	 * with the following properties:
	 *
	 *   - line: The line number in the generated source.
	 *   - column: The column number in the generated source.
	 *   - bias: Either 'SourceMapConsumer.GREATEST_LOWER_BOUND' or
	 *     'SourceMapConsumer.LEAST_UPPER_BOUND'. Specifies whether to return the
	 *     closest element that is smaller than or greater than the one we are
	 *     searching for, respectively, if the exact element cannot be found.
	 *     Defaults to 'SourceMapConsumer.GREATEST_LOWER_BOUND'.
	 *
	 * and an object is returned with the following properties:
	 *
	 *   - source: The original source file, or null.
	 *   - line: The line number in the original source, or null.
	 *   - column: The column number in the original source, or null.
	 *   - name: The original identifier, or null.
	 */
	BasicSourceMapConsumer.prototype.originalPositionFor =
	  function SourceMapConsumer_originalPositionFor(aArgs) {
	    var needle = {
	      generatedLine: util.getArg(aArgs, 'line'),
	      generatedColumn: util.getArg(aArgs, 'column')
	    };
	
	    var index = this._findMapping(
	      needle,
	      this._generatedMappings,
	      "generatedLine",
	      "generatedColumn",
	      util.compareByGeneratedPositionsDeflated,
	      util.getArg(aArgs, 'bias', SourceMapConsumer.GREATEST_LOWER_BOUND)
	    );
	
	    if (index >= 0) {
	      var mapping = this._generatedMappings[index];
	
	      if (mapping.generatedLine === needle.generatedLine) {
	        var source = util.getArg(mapping, 'source', null);
	        if (source !== null) {
	          source = this._sources.at(source);
	          if (this.sourceRoot != null) {
	            source = util.join(this.sourceRoot, source);
	          }
	        }
	        var name = util.getArg(mapping, 'name', null);
	        if (name !== null) {
	          name = this._names.at(name);
	        }
	        return {
	          source: source,
	          line: util.getArg(mapping, 'originalLine', null),
	          column: util.getArg(mapping, 'originalColumn', null),
	          name: name
	        };
	      }
	    }
	
	    return {
	      source: null,
	      line: null,
	      column: null,
	      name: null
	    };
	  };
	
	/**
	 * Return true if we have the source content for every source in the source
	 * map, false otherwise.
	 */
	BasicSourceMapConsumer.prototype.hasContentsOfAllSources =
	  function BasicSourceMapConsumer_hasContentsOfAllSources() {
	    if (!this.sourcesContent) {
	      return false;
	    }
	    return this.sourcesContent.length >= this._sources.size() &&
	      !this.sourcesContent.some(function (sc) { return sc == null; });
	  };
	
	/**
	 * Returns the original source content. The only argument is the url of the
	 * original source file. Returns null if no original source content is
	 * available.
	 */
	BasicSourceMapConsumer.prototype.sourceContentFor =
	  function SourceMapConsumer_sourceContentFor(aSource, nullOnMissing) {
	    if (!this.sourcesContent) {
	      return null;
	    }
	
	    if (this.sourceRoot != null) {
	      aSource = util.relative(this.sourceRoot, aSource);
	    }
	
	    if (this._sources.has(aSource)) {
	      return this.sourcesContent[this._sources.indexOf(aSource)];
	    }
	
	    var url;
	    if (this.sourceRoot != null
	        && (url = util.urlParse(this.sourceRoot))) {
	      // XXX: file:// URIs and absolute paths lead to unexpected behavior for
	      // many users. We can help them out when they expect file:// URIs to
	      // behave like it would if they were running a local HTTP server. See
	      // https://bugzilla.mozilla.org/show_bug.cgi?id=885597.
	      var fileUriAbsPath = aSource.replace(/^file:\/\//, "");
	      if (url.scheme == "file"
	          && this._sources.has(fileUriAbsPath)) {
	        return this.sourcesContent[this._sources.indexOf(fileUriAbsPath)]
	      }
	
	      if ((!url.path || url.path == "/")
	          && this._sources.has("/" + aSource)) {
	        return this.sourcesContent[this._sources.indexOf("/" + aSource)];
	      }
	    }
	
	    // This function is used recursively from
	    // IndexedSourceMapConsumer.prototype.sourceContentFor. In that case, we
	    // don't want to throw if we can't find the source - we just want to
	    // return null, so we provide a flag to exit gracefully.
	    if (nullOnMissing) {
	      return null;
	    }
	    else {
	      throw new Error('"' + aSource + '" is not in the SourceMap.');
	    }
	  };
	
	/**
	 * Returns the generated line and column information for the original source,
	 * line, and column positions provided. The only argument is an object with
	 * the following properties:
	 *
	 *   - source: The filename of the original source.
	 *   - line: The line number in the original source.
	 *   - column: The column number in the original source.
	 *   - bias: Either 'SourceMapConsumer.GREATEST_LOWER_BOUND' or
	 *     'SourceMapConsumer.LEAST_UPPER_BOUND'. Specifies whether to return the
	 *     closest element that is smaller than or greater than the one we are
	 *     searching for, respectively, if the exact element cannot be found.
	 *     Defaults to 'SourceMapConsumer.GREATEST_LOWER_BOUND'.
	 *
	 * and an object is returned with the following properties:
	 *
	 *   - line: The line number in the generated source, or null.
	 *   - column: The column number in the generated source, or null.
	 */
	BasicSourceMapConsumer.prototype.generatedPositionFor =
	  function SourceMapConsumer_generatedPositionFor(aArgs) {
	    var source = util.getArg(aArgs, 'source');
	    if (this.sourceRoot != null) {
	      source = util.relative(this.sourceRoot, source);
	    }
	    if (!this._sources.has(source)) {
	      return {
	        line: null,
	        column: null,
	        lastColumn: null
	      };
	    }
	    source = this._sources.indexOf(source);
	
	    var needle = {
	      source: source,
	      originalLine: util.getArg(aArgs, 'line'),
	      originalColumn: util.getArg(aArgs, 'column')
	    };
	
	    var index = this._findMapping(
	      needle,
	      this._originalMappings,
	      "originalLine",
	      "originalColumn",
	      util.compareByOriginalPositions,
	      util.getArg(aArgs, 'bias', SourceMapConsumer.GREATEST_LOWER_BOUND)
	    );
	
	    if (index >= 0) {
	      var mapping = this._originalMappings[index];
	
	      if (mapping.source === needle.source) {
	        return {
	          line: util.getArg(mapping, 'generatedLine', null),
	          column: util.getArg(mapping, 'generatedColumn', null),
	          lastColumn: util.getArg(mapping, 'lastGeneratedColumn', null)
	        };
	      }
	    }
	
	    return {
	      line: null,
	      column: null,
	      lastColumn: null
	    };
	  };
	
	exports.BasicSourceMapConsumer = BasicSourceMapConsumer;
	
	/**
	 * An IndexedSourceMapConsumer instance represents a parsed source map which
	 * we can query for information. It differs from BasicSourceMapConsumer in
	 * that it takes "indexed" source maps (i.e. ones with a "sections" field) as
	 * input.
	 *
	 * The only parameter is a raw source map (either as a JSON string, or already
	 * parsed to an object). According to the spec for indexed source maps, they
	 * have the following attributes:
	 *
	 *   - version: Which version of the source map spec this map is following.
	 *   - file: Optional. The generated file this source map is associated with.
	 *   - sections: A list of section definitions.
	 *
	 * Each value under the "sections" field has two fields:
	 *   - offset: The offset into the original specified at which this section
	 *       begins to apply, defined as an object with a "line" and "column"
	 *       field.
	 *   - map: A source map definition. This source map could also be indexed,
	 *       but doesn't have to be.
	 *
	 * Instead of the "map" field, it's also possible to have a "url" field
	 * specifying a URL to retrieve a source map from, but that's currently
	 * unsupported.
	 *
	 * Here's an example source map, taken from the source map spec[0], but
	 * modified to omit a section which uses the "url" field.
	 *
	 *  {
	 *    version : 3,
	 *    file: "app.js",
	 *    sections: [{
	 *      offset: {line:100, column:10},
	 *      map: {
	 *        version : 3,
	 *        file: "section.js",
	 *        sources: ["foo.js", "bar.js"],
	 *        names: ["src", "maps", "are", "fun"],
	 *        mappings: "AAAA,E;;ABCDE;"
	 *      }
	 *    }],
	 *  }
	 *
	 * [0]: https://docs.google.com/document/d/1U1RGAehQwRypUTovF1KRlpiOFze0b-_2gc6fAH0KY0k/edit#heading=h.535es3xeprgt
	 */
	function IndexedSourceMapConsumer(aSourceMap) {
	  var sourceMap = aSourceMap;
	  if (typeof aSourceMap === 'string') {
	    sourceMap = JSON.parse(aSourceMap.replace(/^\)\]\}'/, ''));
	  }
	
	  var version = util.getArg(sourceMap, 'version');
	  var sections = util.getArg(sourceMap, 'sections');
	
	  if (version != this._version) {
	    throw new Error('Unsupported version: ' + version);
	  }
	
	  this._sources = new ArraySet();
	  this._names = new ArraySet();
	
	  var lastOffset = {
	    line: -1,
	    column: 0
	  };
	  this._sections = sections.map(function (s) {
	    if (s.url) {
	      // The url field will require support for asynchronicity.
	      // See https://github.com/mozilla/source-map/issues/16
	      throw new Error('Support for url field in sections not implemented.');
	    }
	    var offset = util.getArg(s, 'offset');
	    var offsetLine = util.getArg(offset, 'line');
	    var offsetColumn = util.getArg(offset, 'column');
	
	    if (offsetLine < lastOffset.line ||
	        (offsetLine === lastOffset.line && offsetColumn < lastOffset.column)) {
	      throw new Error('Section offsets must be ordered and non-overlapping.');
	    }
	    lastOffset = offset;
	
	    return {
	      generatedOffset: {
	        // The offset fields are 0-based, but we use 1-based indices when
	        // encoding/decoding from VLQ.
	        generatedLine: offsetLine + 1,
	        generatedColumn: offsetColumn + 1
	      },
	      consumer: new SourceMapConsumer(util.getArg(s, 'map'))
	    }
	  });
	}
	
	IndexedSourceMapConsumer.prototype = Object.create(SourceMapConsumer.prototype);
	IndexedSourceMapConsumer.prototype.constructor = SourceMapConsumer;
	
	/**
	 * The version of the source mapping spec that we are consuming.
	 */
	IndexedSourceMapConsumer.prototype._version = 3;
	
	/**
	 * The list of original sources.
	 */
	Object.defineProperty(IndexedSourceMapConsumer.prototype, 'sources', {
	  get: function () {
	    var sources = [];
	    for (var i = 0; i < this._sections.length; i++) {
	      for (var j = 0; j < this._sections[i].consumer.sources.length; j++) {
	        sources.push(this._sections[i].consumer.sources[j]);
	      }
	    }
	    return sources;
	  }
	});
	
	/**
	 * Returns the original source, line, and column information for the generated
	 * source's line and column positions provided. The only argument is an object
	 * with the following properties:
	 *
	 *   - line: The line number in the generated source.
	 *   - column: The column number in the generated source.
	 *
	 * and an object is returned with the following properties:
	 *
	 *   - source: The original source file, or null.
	 *   - line: The line number in the original source, or null.
	 *   - column: The column number in the original source, or null.
	 *   - name: The original identifier, or null.
	 */
	IndexedSourceMapConsumer.prototype.originalPositionFor =
	  function IndexedSourceMapConsumer_originalPositionFor(aArgs) {
	    var needle = {
	      generatedLine: util.getArg(aArgs, 'line'),
	      generatedColumn: util.getArg(aArgs, 'column')
	    };
	
	    // Find the section containing the generated position we're trying to map
	    // to an original position.
	    var sectionIndex = binarySearch.search(needle, this._sections,
	      function(needle, section) {
	        var cmp = needle.generatedLine - section.generatedOffset.generatedLine;
	        if (cmp) {
	          return cmp;
	        }
	
	        return (needle.generatedColumn -
	                section.generatedOffset.generatedColumn);
	      });
	    var section = this._sections[sectionIndex];
	
	    if (!section) {
	      return {
	        source: null,
	        line: null,
	        column: null,
	        name: null
	      };
	    }
	
	    return section.consumer.originalPositionFor({
	      line: needle.generatedLine -
	        (section.generatedOffset.generatedLine - 1),
	      column: needle.generatedColumn -
	        (section.generatedOffset.generatedLine === needle.generatedLine
	         ? section.generatedOffset.generatedColumn - 1
	         : 0),
	      bias: aArgs.bias
	    });
	  };
	
	/**
	 * Return true if we have the source content for every source in the source
	 * map, false otherwise.
	 */
	IndexedSourceMapConsumer.prototype.hasContentsOfAllSources =
	  function IndexedSourceMapConsumer_hasContentsOfAllSources() {
	    return this._sections.every(function (s) {
	      return s.consumer.hasContentsOfAllSources();
	    });
	  };
	
	/**
	 * Returns the original source content. The only argument is the url of the
	 * original source file. Returns null if no original source content is
	 * available.
	 */
	IndexedSourceMapConsumer.prototype.sourceContentFor =
	  function IndexedSourceMapConsumer_sourceContentFor(aSource, nullOnMissing) {
	    for (var i = 0; i < this._sections.length; i++) {
	      var section = this._sections[i];
	
	      var content = section.consumer.sourceContentFor(aSource, true);
	      if (content) {
	        return content;
	      }
	    }
	    if (nullOnMissing) {
	      return null;
	    }
	    else {
	      throw new Error('"' + aSource + '" is not in the SourceMap.');
	    }
	  };
	
	/**
	 * Returns the generated line and column information for the original source,
	 * line, and column positions provided. The only argument is an object with
	 * the following properties:
	 *
	 *   - source: The filename of the original source.
	 *   - line: The line number in the original source.
	 *   - column: The column number in the original source.
	 *
	 * and an object is returned with the following properties:
	 *
	 *   - line: The line number in the generated source, or null.
	 *   - column: The column number in the generated source, or null.
	 */
	IndexedSourceMapConsumer.prototype.generatedPositionFor =
	  function IndexedSourceMapConsumer_generatedPositionFor(aArgs) {
	    for (var i = 0; i < this._sections.length; i++) {
	      var section = this._sections[i];
	
	      // Only consider this section if the requested source is in the list of
	      // sources of the consumer.
	      if (section.consumer.sources.indexOf(util.getArg(aArgs, 'source')) === -1) {
	        continue;
	      }
	      var generatedPosition = section.consumer.generatedPositionFor(aArgs);
	      if (generatedPosition) {
	        var ret = {
	          line: generatedPosition.line +
	            (section.generatedOffset.generatedLine - 1),
	          column: generatedPosition.column +
	            (section.generatedOffset.generatedLine === generatedPosition.line
	             ? section.generatedOffset.generatedColumn - 1
	             : 0)
	        };
	        return ret;
	      }
	    }
	
	    return {
	      line: null,
	      column: null
	    };
	  };
	
	/**
	 * Parse the mappings in a string in to a data structure which we can easily
	 * query (the ordered arrays in the `this.__generatedMappings` and
	 * `this.__originalMappings` properties).
	 */
	IndexedSourceMapConsumer.prototype._parseMappings =
	  function IndexedSourceMapConsumer_parseMappings(aStr, aSourceRoot) {
	    this.__generatedMappings = [];
	    this.__originalMappings = [];
	    for (var i = 0; i < this._sections.length; i++) {
	      var section = this._sections[i];
	      var sectionMappings = section.consumer._generatedMappings;
	      for (var j = 0; j < sectionMappings.length; j++) {
	        var mapping = sectionMappings[j];
	
	        var source = section.consumer._sources.at(mapping.source);
	        if (section.consumer.sourceRoot !== null) {
	          source = util.join(section.consumer.sourceRoot, source);
	        }
	        this._sources.add(source);
	        source = this._sources.indexOf(source);
	
	        var name = section.consumer._names.at(mapping.name);
	        this._names.add(name);
	        name = this._names.indexOf(name);
	
	        // The mappings coming from the consumer for the section have
	        // generated positions relative to the start of the section, so we
	        // need to offset them to be relative to the start of the concatenated
	        // generated file.
	        var adjustedMapping = {
	          source: source,
	          generatedLine: mapping.generatedLine +
	            (section.generatedOffset.generatedLine - 1),
	          generatedColumn: mapping.generatedColumn +
	            (section.generatedOffset.generatedLine === mapping.generatedLine
	            ? section.generatedOffset.generatedColumn - 1
	            : 0),
	          originalLine: mapping.originalLine,
	          originalColumn: mapping.originalColumn,
	          name: name
	        };
	
	        this.__generatedMappings.push(adjustedMapping);
	        if (typeof adjustedMapping.originalLine === 'number') {
	          this.__originalMappings.push(adjustedMapping);
	        }
	      }
	    }
	
	    quickSort(this.__generatedMappings, util.compareByGeneratedPositionsDeflated);
	    quickSort(this.__originalMappings, util.compareByOriginalPositions);
	  };
	
	exports.IndexedSourceMapConsumer = IndexedSourceMapConsumer;


/***/ },
/* 8 */
/***/ function(module, exports) {

	/* -*- Mode: js; js-indent-level: 2; -*- */
	/*
	 * Copyright 2011 Mozilla Foundation and contributors
	 * Licensed under the New BSD license. See LICENSE or:
	 * http://opensource.org/licenses/BSD-3-Clause
	 */
	
	exports.GREATEST_LOWER_BOUND = 1;
	exports.LEAST_UPPER_BOUND = 2;
	
	/**
	 * Recursive implementation of binary search.
	 *
	 * @param aLow Indices here and lower do not contain the needle.
	 * @param aHigh Indices here and higher do not contain the needle.
	 * @param aNeedle The element being searched for.
	 * @param aHaystack The non-empty array being searched.
	 * @param aCompare Function which takes two elements and returns -1, 0, or 1.
	 * @param aBias Either 'binarySearch.GREATEST_LOWER_BOUND' or
	 *     'binarySearch.LEAST_UPPER_BOUND'. Specifies whether to return the
	 *     closest element that is smaller than or greater than the one we are
	 *     searching for, respectively, if the exact element cannot be found.
	 */
	function recursiveSearch(aLow, aHigh, aNeedle, aHaystack, aCompare, aBias) {
	  // This function terminates when one of the following is true:
	  //
	  //   1. We find the exact element we are looking for.
	  //
	  //   2. We did not find the exact element, but we can return the index of
	  //      the next-closest element.
	  //
	  //   3. We did not find the exact element, and there is no next-closest
	  //      element than the one we are searching for, so we return -1.
	  var mid = Math.floor((aHigh - aLow) / 2) + aLow;
	  var cmp = aCompare(aNeedle, aHaystack[mid], true);
	  if (cmp === 0) {
	    // Found the element we are looking for.
	    return mid;
	  }
	  else if (cmp > 0) {
	    // Our needle is greater than aHaystack[mid].
	    if (aHigh - mid > 1) {
	      // The element is in the upper half.
	      return recursiveSearch(mid, aHigh, aNeedle, aHaystack, aCompare, aBias);
	    }
	
	    // The exact needle element was not found in this haystack. Determine if
	    // we are in termination case (3) or (2) and return the appropriate thing.
	    if (aBias == exports.LEAST_UPPER_BOUND) {
	      return aHigh < aHaystack.length ? aHigh : -1;
	    } else {
	      return mid;
	    }
	  }
	  else {
	    // Our needle is less than aHaystack[mid].
	    if (mid - aLow > 1) {
	      // The element is in the lower half.
	      return recursiveSearch(aLow, mid, aNeedle, aHaystack, aCompare, aBias);
	    }
	
	    // we are in termination case (3) or (2) and return the appropriate thing.
	    if (aBias == exports.LEAST_UPPER_BOUND) {
	      return mid;
	    } else {
	      return aLow < 0 ? -1 : aLow;
	    }
	  }
	}
	
	/**
	 * This is an implementation of binary search which will always try and return
	 * the index of the closest element if there is no exact hit. This is because
	 * mappings between original and generated line/col pairs are single points,
	 * and there is an implicit region between each of them, so a miss just means
	 * that you aren't on the very start of a region.
	 *
	 * @param aNeedle The element you are looking for.
	 * @param aHaystack The array that is being searched.
	 * @param aCompare A function which takes the needle and an element in the
	 *     array and returns -1, 0, or 1 depending on whether the needle is less
	 *     than, equal to, or greater than the element, respectively.
	 * @param aBias Either 'binarySearch.GREATEST_LOWER_BOUND' or
	 *     'binarySearch.LEAST_UPPER_BOUND'. Specifies whether to return the
	 *     closest element that is smaller than or greater than the one we are
	 *     searching for, respectively, if the exact element cannot be found.
	 *     Defaults to 'binarySearch.GREATEST_LOWER_BOUND'.
	 */
	exports.search = function search(aNeedle, aHaystack, aCompare, aBias) {
	  if (aHaystack.length === 0) {
	    return -1;
	  }
	
	  var index = recursiveSearch(-1, aHaystack.length, aNeedle, aHaystack,
	                              aCompare, aBias || exports.GREATEST_LOWER_BOUND);
	  if (index < 0) {
	    return -1;
	  }
	
	  // We have found either the exact element, or the next-closest element than
	  // the one we are searching for. However, there may be more than one such
	  // element. Make sure we always return the smallest of these.
	  while (index - 1 >= 0) {
	    if (aCompare(aHaystack[index], aHaystack[index - 1], true) !== 0) {
	      break;
	    }
	    --index;
	  }
	
	  return index;
	};


/***/ },
/* 9 */
/***/ function(module, exports) {

	/* -*- Mode: js; js-indent-level: 2; -*- */
	/*
	 * Copyright 2011 Mozilla Foundation and contributors
	 * Licensed under the New BSD license. See LICENSE or:
	 * http://opensource.org/licenses/BSD-3-Clause
	 */
	
	// It turns out that some (most?) JavaScript engines don't self-host
	// `Array.prototype.sort`. This makes sense because C++ will likely remain
	// faster than JS when doing raw CPU-intensive sorting. However, when using a
	// custom comparator function, calling back and forth between the VM's C++ and
	// JIT'd JS is rather slow *and* loses JIT type information, resulting in
	// worse generated code for the comparator function than would be optimal. In
	// fact, when sorting with a comparator, these costs outweigh the benefits of
	// sorting in C++. By using our own JS-implemented Quick Sort (below), we get
	// a ~3500ms mean speed-up in `bench/bench.html`.
	
	/**
	 * Swap the elements indexed by `x` and `y` in the array `ary`.
	 *
	 * @param {Array} ary
	 *        The array.
	 * @param {Number} x
	 *        The index of the first item.
	 * @param {Number} y
	 *        The index of the second item.
	 */
	function swap(ary, x, y) {
	  var temp = ary[x];
	  ary[x] = ary[y];
	  ary[y] = temp;
	}
	
	/**
	 * Returns a random integer within the range `low .. high` inclusive.
	 *
	 * @param {Number} low
	 *        The lower bound on the range.
	 * @param {Number} high
	 *        The upper bound on the range.
	 */
	function randomIntInRange(low, high) {
	  return Math.round(low + (Math.random() * (high - low)));
	}
	
	/**
	 * The Quick Sort algorithm.
	 *
	 * @param {Array} ary
	 *        An array to sort.
	 * @param {function} comparator
	 *        Function to use to compare two items.
	 * @param {Number} p
	 *        Start index of the array
	 * @param {Number} r
	 *        End index of the array
	 */
	function doQuickSort(ary, comparator, p, r) {
	  // If our lower bound is less than our upper bound, we (1) partition the
	  // array into two pieces and (2) recurse on each half. If it is not, this is
	  // the empty array and our base case.
	
	  if (p < r) {
	    // (1) Partitioning.
	    //
	    // The partitioning chooses a pivot between `p` and `r` and moves all
	    // elements that are less than or equal to the pivot to the before it, and
	    // all the elements that are greater than it after it. The effect is that
	    // once partition is done, the pivot is in the exact place it will be when
	    // the array is put in sorted order, and it will not need to be moved
	    // again. This runs in O(n) time.
	
	    // Always choose a random pivot so that an input array which is reverse
	    // sorted does not cause O(n^2) running time.
	    var pivotIndex = randomIntInRange(p, r);
	    var i = p - 1;
	
	    swap(ary, pivotIndex, r);
	    var pivot = ary[r];
	
	    // Immediately after `j` is incremented in this loop, the following hold
	    // true:
	    //
	    //   * Every element in `ary[p .. i]` is less than or equal to the pivot.
	    //
	    //   * Every element in `ary[i+1 .. j-1]` is greater than the pivot.
	    for (var j = p; j < r; j++) {
	      if (comparator(ary[j], pivot) <= 0) {
	        i += 1;
	        swap(ary, i, j);
	      }
	    }
	
	    swap(ary, i + 1, j);
	    var q = i + 1;
	
	    // (2) Recurse on each half.
	
	    doQuickSort(ary, comparator, p, q - 1);
	    doQuickSort(ary, comparator, q + 1, r);
	  }
	}
	
	/**
	 * Sort the given array in-place with the given comparator function.
	 *
	 * @param {Array} ary
	 *        An array to sort.
	 * @param {function} comparator
	 *        Function to use to compare two items.
	 */
	exports.quickSort = function (ary, comparator) {
	  doQuickSort(ary, comparator, 0, ary.length - 1);
	};


/***/ },
/* 10 */
/***/ function(module, exports, __webpack_require__) {

	/* -*- Mode: js; js-indent-level: 2; -*- */
	/*
	 * Copyright 2011 Mozilla Foundation and contributors
	 * Licensed under the New BSD license. See LICENSE or:
	 * http://opensource.org/licenses/BSD-3-Clause
	 */
	
	var SourceMapGenerator = __webpack_require__(1).SourceMapGenerator;
	var util = __webpack_require__(4);
	
	// Matches a Windows-style `\r\n` newline or a `\n` newline used by all other
	// operating systems these days (capturing the result).
	var REGEX_NEWLINE = /(\r?\n)/;
	
	// Newline character code for charCodeAt() comparisons
	var NEWLINE_CODE = 10;
	
	// Private symbol for identifying `SourceNode`s when multiple versions of
	// the source-map library are loaded. This MUST NOT CHANGE across
	// versions!
	var isSourceNode = "$$$isSourceNode$$$";
	
	/**
	 * SourceNodes provide a way to abstract over interpolating/concatenating
	 * snippets of generated JavaScript source code while maintaining the line and
	 * column information associated with the original source code.
	 *
	 * @param aLine The original line number.
	 * @param aColumn The original column number.
	 * @param aSource The original source's filename.
	 * @param aChunks Optional. An array of strings which are snippets of
	 *        generated JS, or other SourceNodes.
	 * @param aName The original identifier.
	 */
	function SourceNode(aLine, aColumn, aSource, aChunks, aName) {
	  this.children = [];
	  this.sourceContents = {};
	  this.line = aLine == null ? null : aLine;
	  this.column = aColumn == null ? null : aColumn;
	  this.source = aSource == null ? null : aSource;
	  this.name = aName == null ? null : aName;
	  this[isSourceNode] = true;
	  if (aChunks != null) this.add(aChunks);
	}
	
	/**
	 * Creates a SourceNode from generated code and a SourceMapConsumer.
	 *
	 * @param aGeneratedCode The generated code
	 * @param aSourceMapConsumer The SourceMap for the generated code
	 * @param aRelativePath Optional. The path that relative sources in the
	 *        SourceMapConsumer should be relative to.
	 */
	SourceNode.fromStringWithSourceMap =
	  function SourceNode_fromStringWithSourceMap(aGeneratedCode, aSourceMapConsumer, aRelativePath) {
	    // The SourceNode we want to fill with the generated code
	    // and the SourceMap
	    var node = new SourceNode();
	
	    // All even indices of this array are one line of the generated code,
	    // while all odd indices are the newlines between two adjacent lines
	    // (since `REGEX_NEWLINE` captures its match).
	    // Processed fragments are removed from this array, by calling `shiftNextLine`.
	    var remainingLines = aGeneratedCode.split(REGEX_NEWLINE);
	    var shiftNextLine = function() {
	      var lineContents = remainingLines.shift();
	      // The last line of a file might not have a newline.
	      var newLine = remainingLines.shift() || "";
	      return lineContents + newLine;
	    };
	
	    // We need to remember the position of "remainingLines"
	    var lastGeneratedLine = 1, lastGeneratedColumn = 0;
	
	    // The generate SourceNodes we need a code range.
	    // To extract it current and last mapping is used.
	    // Here we store the last mapping.
	    var lastMapping = null;
	
	    aSourceMapConsumer.eachMapping(function (mapping) {
	      if (lastMapping !== null) {
	        // We add the code from "lastMapping" to "mapping":
	        // First check if there is a new line in between.
	        if (lastGeneratedLine < mapping.generatedLine) {
	          // Associate first line with "lastMapping"
	          addMappingWithCode(lastMapping, shiftNextLine());
	          lastGeneratedLine++;
	          lastGeneratedColumn = 0;
	          // The remaining code is added without mapping
	        } else {
	          // There is no new line in between.
	          // Associate the code between "lastGeneratedColumn" and
	          // "mapping.generatedColumn" with "lastMapping"
	          var nextLine = remainingLines[0];
	          var code = nextLine.substr(0, mapping.generatedColumn -
	                                        lastGeneratedColumn);
	          remainingLines[0] = nextLine.substr(mapping.generatedColumn -
	                                              lastGeneratedColumn);
	          lastGeneratedColumn = mapping.generatedColumn;
	          addMappingWithCode(lastMapping, code);
	          // No more remaining code, continue
	          lastMapping = mapping;
	          return;
	        }
	      }
	      // We add the generated code until the first mapping
	      // to the SourceNode without any mapping.
	      // Each line is added as separate string.
	      while (lastGeneratedLine < mapping.generatedLine) {
	        node.add(shiftNextLine());
	        lastGeneratedLine++;
	      }
	      if (lastGeneratedColumn < mapping.generatedColumn) {
	        var nextLine = remainingLines[0];
	        node.add(nextLine.substr(0, mapping.generatedColumn));
	        remainingLines[0] = nextLine.substr(mapping.generatedColumn);
	        lastGeneratedColumn = mapping.generatedColumn;
	      }
	      lastMapping = mapping;
	    }, this);
	    // We have processed all mappings.
	    if (remainingLines.length > 0) {
	      if (lastMapping) {
	        // Associate the remaining code in the current line with "lastMapping"
	        addMappingWithCode(lastMapping, shiftNextLine());
	      }
	      // and add the remaining lines without any mapping
	      node.add(remainingLines.join(""));
	    }
	
	    // Copy sourcesContent into SourceNode
	    aSourceMapConsumer.sources.forEach(function (sourceFile) {
	      var content = aSourceMapConsumer.sourceContentFor(sourceFile);
	      if (content != null) {
	        if (aRelativePath != null) {
	          sourceFile = util.join(aRelativePath, sourceFile);
	        }
	        node.setSourceContent(sourceFile, content);
	      }
	    });
	
	    return node;
	
	    function addMappingWithCode(mapping, code) {
	      if (mapping === null || mapping.source === undefined) {
	        node.add(code);
	      } else {
	        var source = aRelativePath
	          ? util.join(aRelativePath, mapping.source)
	          : mapping.source;
	        node.add(new SourceNode(mapping.originalLine,
	                                mapping.originalColumn,
	                                source,
	                                code,
	                                mapping.name));
	      }
	    }
	  };
	
	/**
	 * Add a chunk of generated JS to this source node.
	 *
	 * @param aChunk A string snippet of generated JS code, another instance of
	 *        SourceNode, or an array where each member is one of those things.
	 */
	SourceNode.prototype.add = function SourceNode_add(aChunk) {
	  if (Array.isArray(aChunk)) {
	    aChunk.forEach(function (chunk) {
	      this.add(chunk);
	    }, this);
	  }
	  else if (aChunk[isSourceNode] || typeof aChunk === "string") {
	    if (aChunk) {
	      this.children.push(aChunk);
	    }
	  }
	  else {
	    throw new TypeError(
	      "Expected a SourceNode, string, or an array of SourceNodes and strings. Got " + aChunk
	    );
	  }
	  return this;
	};
	
	/**
	 * Add a chunk of generated JS to the beginning of this source node.
	 *
	 * @param aChunk A string snippet of generated JS code, another instance of
	 *        SourceNode, or an array where each member is one of those things.
	 */
	SourceNode.prototype.prepend = function SourceNode_prepend(aChunk) {
	  if (Array.isArray(aChunk)) {
	    for (var i = aChunk.length-1; i >= 0; i--) {
	      this.prepend(aChunk[i]);
	    }
	  }
	  else if (aChunk[isSourceNode] || typeof aChunk === "string") {
	    this.children.unshift(aChunk);
	  }
	  else {
	    throw new TypeError(
	      "Expected a SourceNode, string, or an array of SourceNodes and strings. Got " + aChunk
	    );
	  }
	  return this;
	};
	
	/**
	 * Walk over the tree of JS snippets in this node and its children. The
	 * walking function is called once for each snippet of JS and is passed that
	 * snippet and the its original associated source's line/column location.
	 *
	 * @param aFn The traversal function.
	 */
	SourceNode.prototype.walk = function SourceNode_walk(aFn) {
	  var chunk;
	  for (var i = 0, len = this.children.length; i < len; i++) {
	    chunk = this.children[i];
	    if (chunk[isSourceNode]) {
	      chunk.walk(aFn);
	    }
	    else {
	      if (chunk !== '') {
	        aFn(chunk, { source: this.source,
	                     line: this.line,
	                     column: this.column,
	                     name: this.name });
	      }
	    }
	  }
	};
	
	/**
	 * Like `String.prototype.join` except for SourceNodes. Inserts `aStr` between
	 * each of `this.children`.
	 *
	 * @param aSep The separator.
	 */
	SourceNode.prototype.join = function SourceNode_join(aSep) {
	  var newChildren;
	  var i;
	  var len = this.children.length;
	  if (len > 0) {
	    newChildren = [];
	    for (i = 0; i < len-1; i++) {
	      newChildren.push(this.children[i]);
	      newChildren.push(aSep);
	    }
	    newChildren.push(this.children[i]);
	    this.children = newChildren;
	  }
	  return this;
	};
	
	/**
	 * Call String.prototype.replace on the very right-most source snippet. Useful
	 * for trimming whitespace from the end of a source node, etc.
	 *
	 * @param aPattern The pattern to replace.
	 * @param aReplacement The thing to replace the pattern with.
	 */
	SourceNode.prototype.replaceRight = function SourceNode_replaceRight(aPattern, aReplacement) {
	  var lastChild = this.children[this.children.length - 1];
	  if (lastChild[isSourceNode]) {
	    lastChild.replaceRight(aPattern, aReplacement);
	  }
	  else if (typeof lastChild === 'string') {
	    this.children[this.children.length - 1] = lastChild.replace(aPattern, aReplacement);
	  }
	  else {
	    this.children.push(''.replace(aPattern, aReplacement));
	  }
	  return this;
	};
	
	/**
	 * Set the source content for a source file. This will be added to the SourceMapGenerator
	 * in the sourcesContent field.
	 *
	 * @param aSourceFile The filename of the source file
	 * @param aSourceContent The content of the source file
	 */
	SourceNode.prototype.setSourceContent =
	  function SourceNode_setSourceContent(aSourceFile, aSourceContent) {
	    this.sourceContents[util.toSetString(aSourceFile)] = aSourceContent;
	  };
	
	/**
	 * Walk over the tree of SourceNodes. The walking function is called for each
	 * source file content and is passed the filename and source content.
	 *
	 * @param aFn The traversal function.
	 */
	SourceNode.prototype.walkSourceContents =
	  function SourceNode_walkSourceContents(aFn) {
	    for (var i = 0, len = this.children.length; i < len; i++) {
	      if (this.children[i][isSourceNode]) {
	        this.children[i].walkSourceContents(aFn);
	      }
	    }
	
	    var sources = Object.keys(this.sourceContents);
	    for (var i = 0, len = sources.length; i < len; i++) {
	      aFn(util.fromSetString(sources[i]), this.sourceContents[sources[i]]);
	    }
	  };
	
	/**
	 * Return the string representation of this source node. Walks over the tree
	 * and concatenates all the various snippets together to one string.
	 */
	SourceNode.prototype.toString = function SourceNode_toString() {
	  var str = "";
	  this.walk(function (chunk) {
	    str += chunk;
	  });
	  return str;
	};
	
	/**
	 * Returns the string representation of this source node along with a source
	 * map.
	 */
	SourceNode.prototype.toStringWithSourceMap = function SourceNode_toStringWithSourceMap(aArgs) {
	  var generated = {
	    code: "",
	    line: 1,
	    column: 0
	  };
	  var map = new SourceMapGenerator(aArgs);
	  var sourceMappingActive = false;
	  var lastOriginalSource = null;
	  var lastOriginalLine = null;
	  var lastOriginalColumn = null;
	  var lastOriginalName = null;
	  this.walk(function (chunk, original) {
	    generated.code += chunk;
	    if (original.source !== null
	        && original.line !== null
	        && original.column !== null) {
	      if(lastOriginalSource !== original.source
	         || lastOriginalLine !== original.line
	         || lastOriginalColumn !== original.column
	         || lastOriginalName !== original.name) {
	        map.addMapping({
	          source: original.source,
	          original: {
	            line: original.line,
	            column: original.column
	          },
	          generated: {
	            line: generated.line,
	            column: generated.column
	          },
	          name: original.name
	        });
	      }
	      lastOriginalSource = original.source;
	      lastOriginalLine = original.line;
	      lastOriginalColumn = original.column;
	      lastOriginalName = original.name;
	      sourceMappingActive = true;
	    } else if (sourceMappingActive) {
	      map.addMapping({
	        generated: {
	          line: generated.line,
	          column: generated.column
	        }
	      });
	      lastOriginalSource = null;
	      sourceMappingActive = false;
	    }
	    for (var idx = 0, length = chunk.length; idx < length; idx++) {
	      if (chunk.charCodeAt(idx) === NEWLINE_CODE) {
	        generated.line++;
	        generated.column = 0;
	        // Mappings end at eol
	        if (idx + 1 === length) {
	          lastOriginalSource = null;
	          sourceMappingActive = false;
	        } else if (sourceMappingActive) {
	          map.addMapping({
	            source: original.source,
	            original: {
	              line: original.line,
	              column: original.column
	            },
	            generated: {
	              line: generated.line,
	              column: generated.column
	            },
	            name: original.name
	          });
	        }
	      } else {
	        generated.column++;
	      }
	    }
	  });
	  this.walkSourceContents(function (sourceFile, sourceContent) {
	    map.setSourceContent(sourceFile, sourceContent);
	  });
	
	  return { code: generated.code, map: map };
	};
	
	exports.SourceNode = SourceNode;


/***/ }
/******/ ])
});
;
//# sourceMappingURL=data:application/json;base64,eyJ2ZXJzaW9uIjozLCJzb3VyY2VzIjpbIndlYnBhY2s6Ly8vd2VicGFjay91bml2ZXJzYWxNb2R1bGVEZWZpbml0aW9uIiwid2VicGFjazovLy93ZWJwYWNrL2Jvb3RzdHJhcCBlMTM0NGZmMjJhYzA2Zjk0NWVlNCIsIndlYnBhY2s6Ly8vLi9zb3VyY2UtbWFwLmpzIiwid2VicGFjazovLy8uL2xpYi9zb3VyY2UtbWFwLWdlbmVyYXRvci5qcyIsIndlYnBhY2s6Ly8vLi9saWIvYmFzZTY0LXZscS5qcyIsIndlYnBhY2s6Ly8vLi9saWIvYmFzZTY0LmpzIiwid2VicGFjazovLy8uL2xpYi91dGlsLmpzIiwid2VicGFjazovLy8uL2xpYi9hcnJheS1zZXQuanMiLCJ3ZWJwYWNrOi8vLy4vbGliL21hcHBpbmctbGlzdC5qcyIsIndlYnBhY2s6Ly8vLi9saWIvc291cmNlLW1hcC1jb25zdW1lci5qcyIsIndlYnBhY2s6Ly8vLi9saWIvYmluYXJ5LXNlYXJjaC5qcyIsIndlYnBhY2s6Ly8vLi9saWIvcXVpY2stc29ydC5qcyIsIndlYnBhY2s6Ly8vLi9saWIvc291cmNlLW5vZGUuanMiXSwibmFtZXMiOltdLCJtYXBwaW5ncyI6IkFBQUE7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0EsQ0FBQztBQUNELE87QUNWQTtBQUNBOztBQUVBO0FBQ0E7O0FBRUE7QUFDQTtBQUNBOztBQUVBO0FBQ0E7QUFDQSx1QkFBZTtBQUNmO0FBQ0E7QUFDQTs7QUFFQTtBQUNBOztBQUVBO0FBQ0E7O0FBRUE7QUFDQTtBQUNBOzs7QUFHQTtBQUNBOztBQUVBO0FBQ0E7O0FBRUE7QUFDQTs7QUFFQTtBQUNBOzs7Ozs7O0FDdENBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7Ozs7Ozs7QUNQQSxpQkFBZ0Isb0JBQW9CO0FBQ3BDO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7O0FBRUE7QUFDQTtBQUNBO0FBQ0E7O0FBRUE7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTs7QUFFQTs7QUFFQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0EsTUFBSztBQUNMO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBOztBQUVBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7O0FBRUE7QUFDQTtBQUNBO0FBQ0E7O0FBRUE7QUFDQTtBQUNBO0FBQ0E7O0FBRUE7QUFDQSxNQUFLO0FBQ0w7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBLE1BQUs7QUFDTDtBQUNBOztBQUVBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBOztBQUVBO0FBQ0E7QUFDQTs7QUFFQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7O0FBRUE7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBOztBQUVBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0EsTUFBSztBQUNMOztBQUVBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTs7QUFFQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBLE1BQUs7QUFDTDtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBOztBQUVBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7O0FBRUE7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQSxVQUFTO0FBQ1Q7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7O0FBRUE7QUFDQTtBQUNBO0FBQ0E7O0FBRUE7QUFDQTtBQUNBO0FBQ0E7O0FBRUEsTUFBSztBQUNMO0FBQ0E7O0FBRUE7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0EsTUFBSztBQUNMOztBQUVBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0EsUUFBTztBQUNQO0FBQ0E7O0FBRUE7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTs7QUFFQTtBQUNBLDJDQUEwQyxTQUFTO0FBQ25EO0FBQ0E7O0FBRUE7QUFDQTtBQUNBO0FBQ0EscUJBQW9CO0FBQ3BCO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7O0FBRUE7QUFDQTtBQUNBOztBQUVBO0FBQ0E7QUFDQTtBQUNBOztBQUVBO0FBQ0E7QUFDQTtBQUNBOztBQUVBO0FBQ0E7QUFDQTs7QUFFQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7O0FBRUE7QUFDQTs7QUFFQTtBQUNBOztBQUVBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0EsTUFBSztBQUNMOztBQUVBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7O0FBRUE7QUFDQTs7QUFFQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTs7QUFFQTs7Ozs7OztBQ25aQSxpQkFBZ0Isb0JBQW9CO0FBQ3BDO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQSw0REFBMkQ7QUFDM0QscUJBQW9CO0FBQ3BCO0FBQ0E7QUFDQTtBQUNBOztBQUVBOztBQUVBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7O0FBRUE7O0FBRUE7QUFDQTs7QUFFQTtBQUNBOztBQUVBO0FBQ0E7O0FBRUE7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTs7QUFFQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTs7QUFFQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7O0FBRUE7O0FBRUE7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0EsSUFBRzs7QUFFSDtBQUNBOztBQUVBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTs7QUFFQTtBQUNBO0FBQ0E7QUFDQTs7QUFFQTtBQUNBO0FBQ0E7QUFDQTs7QUFFQTtBQUNBO0FBQ0E7QUFDQTtBQUNBLElBQUc7O0FBRUg7QUFDQTtBQUNBOzs7Ozs7O0FDM0lBLGlCQUFnQixvQkFBb0I7QUFDcEM7QUFDQTtBQUNBO0FBQ0E7QUFDQTs7QUFFQTs7QUFFQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7O0FBRUE7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBLGlCQUFnQjtBQUNoQixpQkFBZ0I7O0FBRWhCLG9CQUFtQjtBQUNuQixxQkFBb0I7O0FBRXBCLGlCQUFnQjtBQUNoQixpQkFBZ0I7O0FBRWhCLGlCQUFnQjtBQUNoQixrQkFBaUI7O0FBRWpCO0FBQ0E7O0FBRUE7QUFDQTtBQUNBO0FBQ0E7O0FBRUE7QUFDQTtBQUNBO0FBQ0E7O0FBRUE7QUFDQTtBQUNBO0FBQ0E7O0FBRUE7QUFDQTtBQUNBO0FBQ0E7O0FBRUE7QUFDQTtBQUNBO0FBQ0E7O0FBRUE7QUFDQTtBQUNBOzs7Ozs7O0FDbEVBLGlCQUFnQixvQkFBb0I7QUFDcEM7QUFDQTtBQUNBO0FBQ0E7QUFDQTs7QUFFQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBLElBQUc7QUFDSDtBQUNBLElBQUc7QUFDSDtBQUNBO0FBQ0E7QUFDQTs7QUFFQTtBQUNBOztBQUVBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7O0FBRUE7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBOztBQUVBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTs7QUFFQTtBQUNBLCtDQUE4QyxRQUFRO0FBQ3REO0FBQ0E7QUFDQTtBQUNBLE1BQUs7QUFDTDtBQUNBLE1BQUs7QUFDTDtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQSxRQUFPO0FBQ1A7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBOztBQUVBO0FBQ0E7QUFDQTs7QUFFQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTs7QUFFQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTs7QUFFQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTs7QUFFQTtBQUNBO0FBQ0E7O0FBRUE7QUFDQTtBQUNBO0FBQ0E7QUFDQTs7QUFFQTtBQUNBO0FBQ0E7O0FBRUE7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7O0FBRUE7QUFDQTtBQUNBOztBQUVBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBOztBQUVBOztBQUVBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBOztBQUVBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBOztBQUVBO0FBQ0E7O0FBRUE7QUFDQTtBQUNBO0FBQ0E7O0FBRUE7QUFDQTtBQUNBO0FBQ0EsRUFBQzs7QUFFRDtBQUNBO0FBQ0E7O0FBRUE7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7O0FBRUE7QUFDQTtBQUNBOztBQUVBO0FBQ0E7QUFDQTtBQUNBOztBQUVBO0FBQ0E7QUFDQTs7QUFFQTtBQUNBO0FBQ0E7QUFDQTs7QUFFQTs7QUFFQTtBQUNBO0FBQ0E7O0FBRUE7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTs7QUFFQSw0QkFBMkIsUUFBUTtBQUNuQztBQUNBO0FBQ0E7QUFDQTs7QUFFQTtBQUNBOztBQUVBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBOztBQUVBO0FBQ0E7QUFDQTtBQUNBOztBQUVBO0FBQ0E7QUFDQTtBQUNBOztBQUVBO0FBQ0E7QUFDQTtBQUNBOztBQUVBO0FBQ0E7QUFDQTtBQUNBOztBQUVBO0FBQ0E7QUFDQTs7QUFFQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBOztBQUVBO0FBQ0E7QUFDQTtBQUNBOztBQUVBO0FBQ0E7QUFDQTtBQUNBOztBQUVBO0FBQ0E7QUFDQTtBQUNBOztBQUVBO0FBQ0E7QUFDQTtBQUNBOztBQUVBO0FBQ0E7QUFDQTs7QUFFQTtBQUNBO0FBQ0E7QUFDQTs7QUFFQTtBQUNBO0FBQ0E7O0FBRUE7QUFDQTs7QUFFQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7O0FBRUE7QUFDQTtBQUNBO0FBQ0E7O0FBRUE7QUFDQTtBQUNBO0FBQ0E7O0FBRUE7QUFDQTtBQUNBO0FBQ0E7O0FBRUE7QUFDQTtBQUNBO0FBQ0E7O0FBRUE7QUFDQTtBQUNBOzs7Ozs7O0FDaGFBLGlCQUFnQixvQkFBb0I7QUFDcEM7QUFDQTtBQUNBO0FBQ0E7QUFDQTs7QUFFQTtBQUNBOztBQUVBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBOztBQUVBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQSx1Q0FBc0MsU0FBUztBQUMvQztBQUNBO0FBQ0E7QUFDQTs7QUFFQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7O0FBRUE7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7O0FBRUE7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBOztBQUVBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTs7QUFFQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBOztBQUVBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7O0FBRUE7Ozs7Ozs7QUN2R0EsaUJBQWdCLG9CQUFvQjtBQUNwQztBQUNBO0FBQ0E7QUFDQTtBQUNBOztBQUVBOztBQUVBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBOztBQUVBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBLGlCQUFnQjtBQUNoQjs7QUFFQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTs7QUFFQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQSxJQUFHO0FBQ0g7QUFDQTtBQUNBO0FBQ0E7O0FBRUE7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7O0FBRUE7Ozs7Ozs7QUM5RUEsaUJBQWdCLG9CQUFvQjtBQUNwQztBQUNBO0FBQ0E7QUFDQTtBQUNBOztBQUVBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7O0FBRUE7QUFDQTtBQUNBO0FBQ0EsdURBQXNEO0FBQ3REOztBQUVBO0FBQ0E7QUFDQTtBQUNBOztBQUVBO0FBQ0E7QUFDQTs7QUFFQTtBQUNBO0FBQ0E7QUFDQTs7QUFFQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBOztBQUVBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTs7QUFFQTtBQUNBO0FBQ0EsRUFBQzs7QUFFRDtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7O0FBRUE7QUFDQTtBQUNBLEVBQUM7O0FBRUQ7QUFDQTtBQUNBO0FBQ0Esb0JBQW1CO0FBQ25COztBQUVBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTs7QUFFQTtBQUNBOztBQUVBO0FBQ0E7O0FBRUE7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTs7QUFFQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBOztBQUVBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQSxNQUFLO0FBQ0w7O0FBRUE7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7O0FBRUE7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBOztBQUVBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBOztBQUVBOztBQUVBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7O0FBRUE7QUFDQTs7QUFFQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQSxZQUFXOztBQUVYO0FBQ0E7QUFDQSxRQUFPO0FBQ1A7O0FBRUE7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBLFlBQVc7O0FBRVg7QUFDQTtBQUNBO0FBQ0E7O0FBRUE7QUFDQTs7QUFFQTs7QUFFQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBLDRCQUEyQixNQUFNO0FBQ2pDO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0EsdURBQXNEO0FBQ3REOztBQUVBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTs7QUFFQTtBQUNBO0FBQ0E7QUFDQTtBQUNBOztBQUVBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQSxNQUFLOztBQUVMO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTs7QUFFQTtBQUNBO0FBQ0E7QUFDQTtBQUNBOztBQUVBO0FBQ0E7O0FBRUE7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7O0FBRUE7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBOztBQUVBO0FBQ0E7QUFDQTtBQUNBOztBQUVBO0FBQ0E7QUFDQTs7QUFFQSx1REFBc0QsWUFBWTtBQUNsRTtBQUNBO0FBQ0E7QUFDQTs7QUFFQTtBQUNBO0FBQ0E7QUFDQTs7QUFFQTtBQUNBO0FBQ0E7O0FBRUE7QUFDQTs7QUFFQTtBQUNBOztBQUVBOztBQUVBO0FBQ0E7O0FBRUE7QUFDQTtBQUNBO0FBQ0E7O0FBRUE7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQSxNQUFLO0FBQ0w7QUFDQSxFQUFDOztBQUVEO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7O0FBRUE7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTs7QUFFQTtBQUNBLG9DQUFtQztBQUNuQztBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTs7QUFFQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0EsMEJBQXlCLGNBQWM7QUFDdkM7QUFDQTtBQUNBO0FBQ0E7QUFDQTs7QUFFQTtBQUNBO0FBQ0E7QUFDQSxVQUFTO0FBQ1Q7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7O0FBRUE7QUFDQTtBQUNBOztBQUVBO0FBQ0E7QUFDQTs7QUFFQTtBQUNBOztBQUVBO0FBQ0E7QUFDQTs7QUFFQTtBQUNBO0FBQ0E7QUFDQTs7QUFFQTtBQUNBO0FBQ0E7QUFDQTtBQUNBOztBQUVBO0FBQ0E7QUFDQTs7QUFFQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7O0FBRUE7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBOztBQUVBO0FBQ0E7O0FBRUE7QUFDQTtBQUNBOztBQUVBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7O0FBRUE7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTs7QUFFQTtBQUNBOztBQUVBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBLHdCQUF1Qix3Q0FBd0M7QUFDL0Q7O0FBRUE7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBOztBQUVBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7O0FBRUE7QUFDQTtBQUNBO0FBQ0E7O0FBRUE7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTs7QUFFQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBOztBQUVBO0FBQ0E7O0FBRUE7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTs7QUFFQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTs7QUFFQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBLGdEQUErQyxtQkFBbUIsRUFBRTtBQUNwRTs7QUFFQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTs7QUFFQTtBQUNBO0FBQ0E7O0FBRUE7QUFDQTtBQUNBOztBQUVBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTs7QUFFQTtBQUNBO0FBQ0E7QUFDQTtBQUNBOztBQUVBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7O0FBRUE7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBOztBQUVBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7O0FBRUE7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTs7QUFFQTtBQUNBOztBQUVBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7O0FBRUE7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBOztBQUVBOztBQUVBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQSxrQkFBaUIsb0JBQW9CO0FBQ3JDO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQSw4QkFBNkIsTUFBTTtBQUNuQztBQUNBLFFBQU87QUFDUDtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBLHVEQUFzRDtBQUN0RDs7QUFFQTtBQUNBOztBQUVBO0FBQ0E7QUFDQTs7QUFFQTtBQUNBOztBQUVBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBOztBQUVBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7O0FBRUE7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0EsUUFBTztBQUNQO0FBQ0E7QUFDQSxJQUFHO0FBQ0g7O0FBRUE7QUFDQTs7QUFFQTtBQUNBO0FBQ0E7QUFDQTs7QUFFQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQSxvQkFBbUIsMkJBQTJCO0FBQzlDLHNCQUFxQiwrQ0FBK0M7QUFDcEU7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBLEVBQUM7O0FBRUQ7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBOztBQUVBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7O0FBRUE7QUFDQTtBQUNBLFFBQU87QUFDUDs7QUFFQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBOztBQUVBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQSxNQUFLO0FBQ0w7O0FBRUE7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBLE1BQUs7QUFDTDs7QUFFQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBLG9CQUFtQiwyQkFBMkI7QUFDOUM7O0FBRUE7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBOztBQUVBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0Esb0JBQW1CLDJCQUEyQjtBQUM5Qzs7QUFFQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7O0FBRUE7QUFDQTtBQUNBO0FBQ0E7QUFDQTs7QUFFQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQSxvQkFBbUIsMkJBQTJCO0FBQzlDO0FBQ0E7QUFDQSxzQkFBcUIsNEJBQTRCO0FBQ2pEOztBQUVBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTs7QUFFQTtBQUNBO0FBQ0E7O0FBRUE7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7O0FBRUE7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBOztBQUVBO0FBQ0E7QUFDQTs7QUFFQTs7Ozs7OztBQ3pqQ0EsaUJBQWdCLG9CQUFvQjtBQUNwQztBQUNBO0FBQ0E7QUFDQTtBQUNBOztBQUVBO0FBQ0E7O0FBRUE7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTs7QUFFQTtBQUNBO0FBQ0E7QUFDQTtBQUNBLE1BQUs7QUFDTDtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7O0FBRUE7QUFDQTtBQUNBO0FBQ0EsTUFBSztBQUNMO0FBQ0E7QUFDQTtBQUNBOztBQUVBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBOztBQUVBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7O0FBRUE7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBOztBQUVBO0FBQ0E7Ozs7Ozs7QUM5R0EsaUJBQWdCLG9CQUFvQjtBQUNwQztBQUNBO0FBQ0E7QUFDQTtBQUNBOztBQUVBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTs7QUFFQTtBQUNBO0FBQ0E7QUFDQSxZQUFXLE1BQU07QUFDakI7QUFDQSxZQUFXLE9BQU87QUFDbEI7QUFDQSxZQUFXLE9BQU87QUFDbEI7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7O0FBRUE7QUFDQTtBQUNBO0FBQ0EsWUFBVyxPQUFPO0FBQ2xCO0FBQ0EsWUFBVyxPQUFPO0FBQ2xCO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7O0FBRUE7QUFDQTtBQUNBO0FBQ0EsWUFBVyxNQUFNO0FBQ2pCO0FBQ0EsWUFBVyxTQUFTO0FBQ3BCO0FBQ0EsWUFBVyxPQUFPO0FBQ2xCO0FBQ0EsWUFBVyxPQUFPO0FBQ2xCO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTs7QUFFQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7O0FBRUE7QUFDQTtBQUNBO0FBQ0E7O0FBRUE7QUFDQTs7QUFFQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQSxvQkFBbUIsT0FBTztBQUMxQjtBQUNBO0FBQ0E7QUFDQTtBQUNBOztBQUVBO0FBQ0E7O0FBRUE7O0FBRUE7QUFDQTtBQUNBO0FBQ0E7O0FBRUE7QUFDQTtBQUNBO0FBQ0EsWUFBVyxNQUFNO0FBQ2pCO0FBQ0EsWUFBVyxTQUFTO0FBQ3BCO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7Ozs7Ozs7QUNqSEEsaUJBQWdCLG9CQUFvQjtBQUNwQztBQUNBO0FBQ0E7QUFDQTtBQUNBOztBQUVBO0FBQ0E7O0FBRUE7QUFDQTtBQUNBOztBQUVBO0FBQ0E7O0FBRUE7QUFDQTtBQUNBO0FBQ0E7O0FBRUE7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7O0FBRUE7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7O0FBRUE7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTs7QUFFQTtBQUNBOztBQUVBO0FBQ0E7QUFDQTtBQUNBOztBQUVBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0EsVUFBUztBQUNUO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQSxNQUFLO0FBQ0w7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBOztBQUVBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBLE1BQUs7O0FBRUw7O0FBRUE7QUFDQTtBQUNBO0FBQ0EsUUFBTztBQUNQO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7O0FBRUE7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQSxNQUFLO0FBQ0w7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7O0FBRUE7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBLGtDQUFpQyxRQUFRO0FBQ3pDO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBOztBQUVBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBLDhDQUE2QyxTQUFTO0FBQ3REO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBLHFCQUFvQjtBQUNwQjtBQUNBO0FBQ0EsdUNBQXNDO0FBQ3RDO0FBQ0E7QUFDQTtBQUNBOztBQUVBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBLGdCQUFlLFdBQVc7QUFDMUI7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTs7QUFFQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBOztBQUVBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7O0FBRUE7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBLGdEQUErQyxTQUFTO0FBQ3hEO0FBQ0E7QUFDQTtBQUNBOztBQUVBO0FBQ0EsMENBQXlDLFNBQVM7QUFDbEQ7QUFDQTtBQUNBOztBQUVBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQSxJQUFHO0FBQ0g7QUFDQTs7QUFFQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQSxZQUFXO0FBQ1g7QUFDQTtBQUNBO0FBQ0EsWUFBVztBQUNYO0FBQ0EsVUFBUztBQUNUO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBLE1BQUs7QUFDTDtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0EsUUFBTztBQUNQO0FBQ0E7QUFDQTtBQUNBLDZDQUE0QyxjQUFjO0FBQzFEO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0EsVUFBUztBQUNUO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQSxjQUFhO0FBQ2I7QUFDQTtBQUNBO0FBQ0EsY0FBYTtBQUNiO0FBQ0EsWUFBVztBQUNYO0FBQ0EsUUFBTztBQUNQO0FBQ0E7QUFDQTtBQUNBLElBQUc7QUFDSDtBQUNBO0FBQ0EsSUFBRzs7QUFFSCxXQUFVO0FBQ1Y7O0FBRUEiLCJmaWxlIjoic291cmNlLW1hcC5kZWJ1Zy5qcyIsInNvdXJjZXNDb250ZW50IjpbIihmdW5jdGlvbiB3ZWJwYWNrVW5pdmVyc2FsTW9kdWxlRGVmaW5pdGlvbihyb290LCBmYWN0b3J5KSB7XG5cdGlmKHR5cGVvZiBleHBvcnRzID09PSAnb2JqZWN0JyAmJiB0eXBlb2YgbW9kdWxlID09PSAnb2JqZWN0Jylcblx0XHRtb2R1bGUuZXhwb3J0cyA9IGZhY3RvcnkoKTtcblx0ZWxzZSBpZih0eXBlb2YgZGVmaW5lID09PSAnZnVuY3Rpb24nICYmIGRlZmluZS5hbWQpXG5cdFx0ZGVmaW5lKFtdLCBmYWN0b3J5KTtcblx0ZWxzZSBpZih0eXBlb2YgZXhwb3J0cyA9PT0gJ29iamVjdCcpXG5cdFx0ZXhwb3J0c1tcInNvdXJjZU1hcFwiXSA9IGZhY3RvcnkoKTtcblx0ZWxzZVxuXHRcdHJvb3RbXCJzb3VyY2VNYXBcIl0gPSBmYWN0b3J5KCk7XG59KSh0aGlzLCBmdW5jdGlvbigpIHtcbnJldHVybiBcblxuXG4vKiogV0VCUEFDSyBGT09URVIgKipcbiAqKiB3ZWJwYWNrL3VuaXZlcnNhbE1vZHVsZURlZmluaXRpb25cbiAqKi8iLCIgXHQvLyBUaGUgbW9kdWxlIGNhY2hlXG4gXHR2YXIgaW5zdGFsbGVkTW9kdWxlcyA9IHt9O1xuXG4gXHQvLyBUaGUgcmVxdWlyZSBmdW5jdGlvblxuIFx0ZnVuY3Rpb24gX193ZWJwYWNrX3JlcXVpcmVfXyhtb2R1bGVJZCkge1xuXG4gXHRcdC8vIENoZWNrIGlmIG1vZHVsZSBpcyBpbiBjYWNoZVxuIFx0XHRpZihpbnN0YWxsZWRNb2R1bGVzW21vZHVsZUlkXSlcbiBcdFx0XHRyZXR1cm4gaW5zdGFsbGVkTW9kdWxlc1ttb2R1bGVJZF0uZXhwb3J0cztcblxuIFx0XHQvLyBDcmVhdGUgYSBuZXcgbW9kdWxlIChhbmQgcHV0IGl0IGludG8gdGhlIGNhY2hlKVxuIFx0XHR2YXIgbW9kdWxlID0gaW5zdGFsbGVkTW9kdWxlc1ttb2R1bGVJZF0gPSB7XG4gXHRcdFx0ZXhwb3J0czoge30sXG4gXHRcdFx0aWQ6IG1vZHVsZUlkLFxuIFx0XHRcdGxvYWRlZDogZmFsc2VcbiBcdFx0fTtcblxuIFx0XHQvLyBFeGVjdXRlIHRoZSBtb2R1bGUgZnVuY3Rpb25cbiBcdFx0bW9kdWxlc1ttb2R1bGVJZF0uY2FsbChtb2R1bGUuZXhwb3J0cywgbW9kdWxlLCBtb2R1bGUuZXhwb3J0cywgX193ZWJwYWNrX3JlcXVpcmVfXyk7XG5cbiBcdFx0Ly8gRmxhZyB0aGUgbW9kdWxlIGFzIGxvYWRlZFxuIFx0XHRtb2R1bGUubG9hZGVkID0gdHJ1ZTtcblxuIFx0XHQvLyBSZXR1cm4gdGhlIGV4cG9ydHMgb2YgdGhlIG1vZHVsZVxuIFx0XHRyZXR1cm4gbW9kdWxlLmV4cG9ydHM7XG4gXHR9XG5cblxuIFx0Ly8gZXhwb3NlIHRoZSBtb2R1bGVzIG9iamVjdCAoX193ZWJwYWNrX21vZHVsZXNfXylcbiBcdF9fd2VicGFja19yZXF1aXJlX18ubSA9IG1vZHVsZXM7XG5cbiBcdC8vIGV4cG9zZSB0aGUgbW9kdWxlIGNhY2hlXG4gXHRfX3dlYnBhY2tfcmVxdWlyZV9fLmMgPSBpbnN0YWxsZWRNb2R1bGVzO1xuXG4gXHQvLyBfX3dlYnBhY2tfcHVibGljX3BhdGhfX1xuIFx0X193ZWJwYWNrX3JlcXVpcmVfXy5wID0gXCJcIjtcblxuIFx0Ly8gTG9hZCBlbnRyeSBtb2R1bGUgYW5kIHJldHVybiBleHBvcnRzXG4gXHRyZXR1cm4gX193ZWJwYWNrX3JlcXVpcmVfXygwKTtcblxuXG5cbi8qKiBXRUJQQUNLIEZPT1RFUiAqKlxuICoqIHdlYnBhY2svYm9vdHN0cmFwIGUxMzQ0ZmYyMmFjMDZmOTQ1ZWU0XG4gKiovIiwiLypcbiAqIENvcHlyaWdodCAyMDA5LTIwMTEgTW96aWxsYSBGb3VuZGF0aW9uIGFuZCBjb250cmlidXRvcnNcbiAqIExpY2Vuc2VkIHVuZGVyIHRoZSBOZXcgQlNEIGxpY2Vuc2UuIFNlZSBMSUNFTlNFLnR4dCBvcjpcbiAqIGh0dHA6Ly9vcGVuc291cmNlLm9yZy9saWNlbnNlcy9CU0QtMy1DbGF1c2VcbiAqL1xuZXhwb3J0cy5Tb3VyY2VNYXBHZW5lcmF0b3IgPSByZXF1aXJlKCcuL2xpYi9zb3VyY2UtbWFwLWdlbmVyYXRvcicpLlNvdXJjZU1hcEdlbmVyYXRvcjtcbmV4cG9ydHMuU291cmNlTWFwQ29uc3VtZXIgPSByZXF1aXJlKCcuL2xpYi9zb3VyY2UtbWFwLWNvbnN1bWVyJykuU291cmNlTWFwQ29uc3VtZXI7XG5leHBvcnRzLlNvdXJjZU5vZGUgPSByZXF1aXJlKCcuL2xpYi9zb3VyY2Utbm9kZScpLlNvdXJjZU5vZGU7XG5cblxuXG4vKioqKioqKioqKioqKioqKipcbiAqKiBXRUJQQUNLIEZPT1RFUlxuICoqIC4vc291cmNlLW1hcC5qc1xuICoqIG1vZHVsZSBpZCA9IDBcbiAqKiBtb2R1bGUgY2h1bmtzID0gMFxuICoqLyIsIi8qIC0qLSBNb2RlOiBqczsganMtaW5kZW50LWxldmVsOiAyOyAtKi0gKi9cbi8qXG4gKiBDb3B5cmlnaHQgMjAxMSBNb3ppbGxhIEZvdW5kYXRpb24gYW5kIGNvbnRyaWJ1dG9yc1xuICogTGljZW5zZWQgdW5kZXIgdGhlIE5ldyBCU0QgbGljZW5zZS4gU2VlIExJQ0VOU0Ugb3I6XG4gKiBodHRwOi8vb3BlbnNvdXJjZS5vcmcvbGljZW5zZXMvQlNELTMtQ2xhdXNlXG4gKi9cblxudmFyIGJhc2U2NFZMUSA9IHJlcXVpcmUoJy4vYmFzZTY0LXZscScpO1xudmFyIHV0aWwgPSByZXF1aXJlKCcuL3V0aWwnKTtcbnZhciBBcnJheVNldCA9IHJlcXVpcmUoJy4vYXJyYXktc2V0JykuQXJyYXlTZXQ7XG52YXIgTWFwcGluZ0xpc3QgPSByZXF1aXJlKCcuL21hcHBpbmctbGlzdCcpLk1hcHBpbmdMaXN0O1xuXG4vKipcbiAqIEFuIGluc3RhbmNlIG9mIHRoZSBTb3VyY2VNYXBHZW5lcmF0b3IgcmVwcmVzZW50cyBhIHNvdXJjZSBtYXAgd2hpY2ggaXNcbiAqIGJlaW5nIGJ1aWx0IGluY3JlbWVudGFsbHkuIFlvdSBtYXkgcGFzcyBhbiBvYmplY3Qgd2l0aCB0aGUgZm9sbG93aW5nXG4gKiBwcm9wZXJ0aWVzOlxuICpcbiAqICAgLSBmaWxlOiBUaGUgZmlsZW5hbWUgb2YgdGhlIGdlbmVyYXRlZCBzb3VyY2UuXG4gKiAgIC0gc291cmNlUm9vdDogQSByb290IGZvciBhbGwgcmVsYXRpdmUgVVJMcyBpbiB0aGlzIHNvdXJjZSBtYXAuXG4gKi9cbmZ1bmN0aW9uIFNvdXJjZU1hcEdlbmVyYXRvcihhQXJncykge1xuICBpZiAoIWFBcmdzKSB7XG4gICAgYUFyZ3MgPSB7fTtcbiAgfVxuICB0aGlzLl9maWxlID0gdXRpbC5nZXRBcmcoYUFyZ3MsICdmaWxlJywgbnVsbCk7XG4gIHRoaXMuX3NvdXJjZVJvb3QgPSB1dGlsLmdldEFyZyhhQXJncywgJ3NvdXJjZVJvb3QnLCBudWxsKTtcbiAgdGhpcy5fc2tpcFZhbGlkYXRpb24gPSB1dGlsLmdldEFyZyhhQXJncywgJ3NraXBWYWxpZGF0aW9uJywgZmFsc2UpO1xuICB0aGlzLl9zb3VyY2VzID0gbmV3IEFycmF5U2V0KCk7XG4gIHRoaXMuX25hbWVzID0gbmV3IEFycmF5U2V0KCk7XG4gIHRoaXMuX21hcHBpbmdzID0gbmV3IE1hcHBpbmdMaXN0KCk7XG4gIHRoaXMuX3NvdXJjZXNDb250ZW50cyA9IG51bGw7XG59XG5cblNvdXJjZU1hcEdlbmVyYXRvci5wcm90b3R5cGUuX3ZlcnNpb24gPSAzO1xuXG4vKipcbiAqIENyZWF0ZXMgYSBuZXcgU291cmNlTWFwR2VuZXJhdG9yIGJhc2VkIG9uIGEgU291cmNlTWFwQ29uc3VtZXJcbiAqXG4gKiBAcGFyYW0gYVNvdXJjZU1hcENvbnN1bWVyIFRoZSBTb3VyY2VNYXAuXG4gKi9cblNvdXJjZU1hcEdlbmVyYXRvci5mcm9tU291cmNlTWFwID1cbiAgZnVuY3Rpb24gU291cmNlTWFwR2VuZXJhdG9yX2Zyb21Tb3VyY2VNYXAoYVNvdXJjZU1hcENvbnN1bWVyKSB7XG4gICAgdmFyIHNvdXJjZVJvb3QgPSBhU291cmNlTWFwQ29uc3VtZXIuc291cmNlUm9vdDtcbiAgICB2YXIgZ2VuZXJhdG9yID0gbmV3IFNvdXJjZU1hcEdlbmVyYXRvcih7XG4gICAgICBmaWxlOiBhU291cmNlTWFwQ29uc3VtZXIuZmlsZSxcbiAgICAgIHNvdXJjZVJvb3Q6IHNvdXJjZVJvb3RcbiAgICB9KTtcbiAgICBhU291cmNlTWFwQ29uc3VtZXIuZWFjaE1hcHBpbmcoZnVuY3Rpb24gKG1hcHBpbmcpIHtcbiAgICAgIHZhciBuZXdNYXBwaW5nID0ge1xuICAgICAgICBnZW5lcmF0ZWQ6IHtcbiAgICAgICAgICBsaW5lOiBtYXBwaW5nLmdlbmVyYXRlZExpbmUsXG4gICAgICAgICAgY29sdW1uOiBtYXBwaW5nLmdlbmVyYXRlZENvbHVtblxuICAgICAgICB9XG4gICAgICB9O1xuXG4gICAgICBpZiAobWFwcGluZy5zb3VyY2UgIT0gbnVsbCkge1xuICAgICAgICBuZXdNYXBwaW5nLnNvdXJjZSA9IG1hcHBpbmcuc291cmNlO1xuICAgICAgICBpZiAoc291cmNlUm9vdCAhPSBudWxsKSB7XG4gICAgICAgICAgbmV3TWFwcGluZy5zb3VyY2UgPSB1dGlsLnJlbGF0aXZlKHNvdXJjZVJvb3QsIG5ld01hcHBpbmcuc291cmNlKTtcbiAgICAgICAgfVxuXG4gICAgICAgIG5ld01hcHBpbmcub3JpZ2luYWwgPSB7XG4gICAgICAgICAgbGluZTogbWFwcGluZy5vcmlnaW5hbExpbmUsXG4gICAgICAgICAgY29sdW1uOiBtYXBwaW5nLm9yaWdpbmFsQ29sdW1uXG4gICAgICAgIH07XG5cbiAgICAgICAgaWYgKG1hcHBpbmcubmFtZSAhPSBudWxsKSB7XG4gICAgICAgICAgbmV3TWFwcGluZy5uYW1lID0gbWFwcGluZy5uYW1lO1xuICAgICAgICB9XG4gICAgICB9XG5cbiAgICAgIGdlbmVyYXRvci5hZGRNYXBwaW5nKG5ld01hcHBpbmcpO1xuICAgIH0pO1xuICAgIGFTb3VyY2VNYXBDb25zdW1lci5zb3VyY2VzLmZvckVhY2goZnVuY3Rpb24gKHNvdXJjZUZpbGUpIHtcbiAgICAgIHZhciBjb250ZW50ID0gYVNvdXJjZU1hcENvbnN1bWVyLnNvdXJjZUNvbnRlbnRGb3Ioc291cmNlRmlsZSk7XG4gICAgICBpZiAoY29udGVudCAhPSBudWxsKSB7XG4gICAgICAgIGdlbmVyYXRvci5zZXRTb3VyY2VDb250ZW50KHNvdXJjZUZpbGUsIGNvbnRlbnQpO1xuICAgICAgfVxuICAgIH0pO1xuICAgIHJldHVybiBnZW5lcmF0b3I7XG4gIH07XG5cbi8qKlxuICogQWRkIGEgc2luZ2xlIG1hcHBpbmcgZnJvbSBvcmlnaW5hbCBzb3VyY2UgbGluZSBhbmQgY29sdW1uIHRvIHRoZSBnZW5lcmF0ZWRcbiAqIHNvdXJjZSdzIGxpbmUgYW5kIGNvbHVtbiBmb3IgdGhpcyBzb3VyY2UgbWFwIGJlaW5nIGNyZWF0ZWQuIFRoZSBtYXBwaW5nXG4gKiBvYmplY3Qgc2hvdWxkIGhhdmUgdGhlIGZvbGxvd2luZyBwcm9wZXJ0aWVzOlxuICpcbiAqICAgLSBnZW5lcmF0ZWQ6IEFuIG9iamVjdCB3aXRoIHRoZSBnZW5lcmF0ZWQgbGluZSBhbmQgY29sdW1uIHBvc2l0aW9ucy5cbiAqICAgLSBvcmlnaW5hbDogQW4gb2JqZWN0IHdpdGggdGhlIG9yaWdpbmFsIGxpbmUgYW5kIGNvbHVtbiBwb3NpdGlvbnMuXG4gKiAgIC0gc291cmNlOiBUaGUgb3JpZ2luYWwgc291cmNlIGZpbGUgKHJlbGF0aXZlIHRvIHRoZSBzb3VyY2VSb290KS5cbiAqICAgLSBuYW1lOiBBbiBvcHRpb25hbCBvcmlnaW5hbCB0b2tlbiBuYW1lIGZvciB0aGlzIG1hcHBpbmcuXG4gKi9cblNvdXJjZU1hcEdlbmVyYXRvci5wcm90b3R5cGUuYWRkTWFwcGluZyA9XG4gIGZ1bmN0aW9uIFNvdXJjZU1hcEdlbmVyYXRvcl9hZGRNYXBwaW5nKGFBcmdzKSB7XG4gICAgdmFyIGdlbmVyYXRlZCA9IHV0aWwuZ2V0QXJnKGFBcmdzLCAnZ2VuZXJhdGVkJyk7XG4gICAgdmFyIG9yaWdpbmFsID0gdXRpbC5nZXRBcmcoYUFyZ3MsICdvcmlnaW5hbCcsIG51bGwpO1xuICAgIHZhciBzb3VyY2UgPSB1dGlsLmdldEFyZyhhQXJncywgJ3NvdXJjZScsIG51bGwpO1xuICAgIHZhciBuYW1lID0gdXRpbC5nZXRBcmcoYUFyZ3MsICduYW1lJywgbnVsbCk7XG5cbiAgICBpZiAoIXRoaXMuX3NraXBWYWxpZGF0aW9uKSB7XG4gICAgICB0aGlzLl92YWxpZGF0ZU1hcHBpbmcoZ2VuZXJhdGVkLCBvcmlnaW5hbCwgc291cmNlLCBuYW1lKTtcbiAgICB9XG5cbiAgICBpZiAoc291cmNlICE9IG51bGwpIHtcbiAgICAgIHNvdXJjZSA9IFN0cmluZyhzb3VyY2UpO1xuICAgICAgaWYgKCF0aGlzLl9zb3VyY2VzLmhhcyhzb3VyY2UpKSB7XG4gICAgICAgIHRoaXMuX3NvdXJjZXMuYWRkKHNvdXJjZSk7XG4gICAgICB9XG4gICAgfVxuXG4gICAgaWYgKG5hbWUgIT0gbnVsbCkge1xuICAgICAgbmFtZSA9IFN0cmluZyhuYW1lKTtcbiAgICAgIGlmICghdGhpcy5fbmFtZXMuaGFzKG5hbWUpKSB7XG4gICAgICAgIHRoaXMuX25hbWVzLmFkZChuYW1lKTtcbiAgICAgIH1cbiAgICB9XG5cbiAgICB0aGlzLl9tYXBwaW5ncy5hZGQoe1xuICAgICAgZ2VuZXJhdGVkTGluZTogZ2VuZXJhdGVkLmxpbmUsXG4gICAgICBnZW5lcmF0ZWRDb2x1bW46IGdlbmVyYXRlZC5jb2x1bW4sXG4gICAgICBvcmlnaW5hbExpbmU6IG9yaWdpbmFsICE9IG51bGwgJiYgb3JpZ2luYWwubGluZSxcbiAgICAgIG9yaWdpbmFsQ29sdW1uOiBvcmlnaW5hbCAhPSBudWxsICYmIG9yaWdpbmFsLmNvbHVtbixcbiAgICAgIHNvdXJjZTogc291cmNlLFxuICAgICAgbmFtZTogbmFtZVxuICAgIH0pO1xuICB9O1xuXG4vKipcbiAqIFNldCB0aGUgc291cmNlIGNvbnRlbnQgZm9yIGEgc291cmNlIGZpbGUuXG4gKi9cblNvdXJjZU1hcEdlbmVyYXRvci5wcm90b3R5cGUuc2V0U291cmNlQ29udGVudCA9XG4gIGZ1bmN0aW9uIFNvdXJjZU1hcEdlbmVyYXRvcl9zZXRTb3VyY2VDb250ZW50KGFTb3VyY2VGaWxlLCBhU291cmNlQ29udGVudCkge1xuICAgIHZhciBzb3VyY2UgPSBhU291cmNlRmlsZTtcbiAgICBpZiAodGhpcy5fc291cmNlUm9vdCAhPSBudWxsKSB7XG4gICAgICBzb3VyY2UgPSB1dGlsLnJlbGF0aXZlKHRoaXMuX3NvdXJjZVJvb3QsIHNvdXJjZSk7XG4gICAgfVxuXG4gICAgaWYgKGFTb3VyY2VDb250ZW50ICE9IG51bGwpIHtcbiAgICAgIC8vIEFkZCB0aGUgc291cmNlIGNvbnRlbnQgdG8gdGhlIF9zb3VyY2VzQ29udGVudHMgbWFwLlxuICAgICAgLy8gQ3JlYXRlIGEgbmV3IF9zb3VyY2VzQ29udGVudHMgbWFwIGlmIHRoZSBwcm9wZXJ0eSBpcyBudWxsLlxuICAgICAgaWYgKCF0aGlzLl9zb3VyY2VzQ29udGVudHMpIHtcbiAgICAgICAgdGhpcy5fc291cmNlc0NvbnRlbnRzID0gT2JqZWN0LmNyZWF0ZShudWxsKTtcbiAgICAgIH1cbiAgICAgIHRoaXMuX3NvdXJjZXNDb250ZW50c1t1dGlsLnRvU2V0U3RyaW5nKHNvdXJjZSldID0gYVNvdXJjZUNvbnRlbnQ7XG4gICAgfSBlbHNlIGlmICh0aGlzLl9zb3VyY2VzQ29udGVudHMpIHtcbiAgICAgIC8vIFJlbW92ZSB0aGUgc291cmNlIGZpbGUgZnJvbSB0aGUgX3NvdXJjZXNDb250ZW50cyBtYXAuXG4gICAgICAvLyBJZiB0aGUgX3NvdXJjZXNDb250ZW50cyBtYXAgaXMgZW1wdHksIHNldCB0aGUgcHJvcGVydHkgdG8gbnVsbC5cbiAgICAgIGRlbGV0ZSB0aGlzLl9zb3VyY2VzQ29udGVudHNbdXRpbC50b1NldFN0cmluZyhzb3VyY2UpXTtcbiAgICAgIGlmIChPYmplY3Qua2V5cyh0aGlzLl9zb3VyY2VzQ29udGVudHMpLmxlbmd0aCA9PT0gMCkge1xuICAgICAgICB0aGlzLl9zb3VyY2VzQ29udGVudHMgPSBudWxsO1xuICAgICAgfVxuICAgIH1cbiAgfTtcblxuLyoqXG4gKiBBcHBsaWVzIHRoZSBtYXBwaW5ncyBvZiBhIHN1Yi1zb3VyY2UtbWFwIGZvciBhIHNwZWNpZmljIHNvdXJjZSBmaWxlIHRvIHRoZVxuICogc291cmNlIG1hcCBiZWluZyBnZW5lcmF0ZWQuIEVhY2ggbWFwcGluZyB0byB0aGUgc3VwcGxpZWQgc291cmNlIGZpbGUgaXNcbiAqIHJld3JpdHRlbiB1c2luZyB0aGUgc3VwcGxpZWQgc291cmNlIG1hcC4gTm90ZTogVGhlIHJlc29sdXRpb24gZm9yIHRoZVxuICogcmVzdWx0aW5nIG1hcHBpbmdzIGlzIHRoZSBtaW5pbWl1bSBvZiB0aGlzIG1hcCBhbmQgdGhlIHN1cHBsaWVkIG1hcC5cbiAqXG4gKiBAcGFyYW0gYVNvdXJjZU1hcENvbnN1bWVyIFRoZSBzb3VyY2UgbWFwIHRvIGJlIGFwcGxpZWQuXG4gKiBAcGFyYW0gYVNvdXJjZUZpbGUgT3B0aW9uYWwuIFRoZSBmaWxlbmFtZSBvZiB0aGUgc291cmNlIGZpbGUuXG4gKiAgICAgICAgSWYgb21pdHRlZCwgU291cmNlTWFwQ29uc3VtZXIncyBmaWxlIHByb3BlcnR5IHdpbGwgYmUgdXNlZC5cbiAqIEBwYXJhbSBhU291cmNlTWFwUGF0aCBPcHRpb25hbC4gVGhlIGRpcm5hbWUgb2YgdGhlIHBhdGggdG8gdGhlIHNvdXJjZSBtYXBcbiAqICAgICAgICB0byBiZSBhcHBsaWVkLiBJZiByZWxhdGl2ZSwgaXQgaXMgcmVsYXRpdmUgdG8gdGhlIFNvdXJjZU1hcENvbnN1bWVyLlxuICogICAgICAgIFRoaXMgcGFyYW1ldGVyIGlzIG5lZWRlZCB3aGVuIHRoZSB0d28gc291cmNlIG1hcHMgYXJlbid0IGluIHRoZSBzYW1lXG4gKiAgICAgICAgZGlyZWN0b3J5LCBhbmQgdGhlIHNvdXJjZSBtYXAgdG8gYmUgYXBwbGllZCBjb250YWlucyByZWxhdGl2ZSBzb3VyY2VcbiAqICAgICAgICBwYXRocy4gSWYgc28sIHRob3NlIHJlbGF0aXZlIHNvdXJjZSBwYXRocyBuZWVkIHRvIGJlIHJld3JpdHRlblxuICogICAgICAgIHJlbGF0aXZlIHRvIHRoZSBTb3VyY2VNYXBHZW5lcmF0b3IuXG4gKi9cblNvdXJjZU1hcEdlbmVyYXRvci5wcm90b3R5cGUuYXBwbHlTb3VyY2VNYXAgPVxuICBmdW5jdGlvbiBTb3VyY2VNYXBHZW5lcmF0b3JfYXBwbHlTb3VyY2VNYXAoYVNvdXJjZU1hcENvbnN1bWVyLCBhU291cmNlRmlsZSwgYVNvdXJjZU1hcFBhdGgpIHtcbiAgICB2YXIgc291cmNlRmlsZSA9IGFTb3VyY2VGaWxlO1xuICAgIC8vIElmIGFTb3VyY2VGaWxlIGlzIG9taXR0ZWQsIHdlIHdpbGwgdXNlIHRoZSBmaWxlIHByb3BlcnR5IG9mIHRoZSBTb3VyY2VNYXBcbiAgICBpZiAoYVNvdXJjZUZpbGUgPT0gbnVsbCkge1xuICAgICAgaWYgKGFTb3VyY2VNYXBDb25zdW1lci5maWxlID09IG51bGwpIHtcbiAgICAgICAgdGhyb3cgbmV3IEVycm9yKFxuICAgICAgICAgICdTb3VyY2VNYXBHZW5lcmF0b3IucHJvdG90eXBlLmFwcGx5U291cmNlTWFwIHJlcXVpcmVzIGVpdGhlciBhbiBleHBsaWNpdCBzb3VyY2UgZmlsZSwgJyArXG4gICAgICAgICAgJ29yIHRoZSBzb3VyY2UgbWFwXFwncyBcImZpbGVcIiBwcm9wZXJ0eS4gQm90aCB3ZXJlIG9taXR0ZWQuJ1xuICAgICAgICApO1xuICAgICAgfVxuICAgICAgc291cmNlRmlsZSA9IGFTb3VyY2VNYXBDb25zdW1lci5maWxlO1xuICAgIH1cbiAgICB2YXIgc291cmNlUm9vdCA9IHRoaXMuX3NvdXJjZVJvb3Q7XG4gICAgLy8gTWFrZSBcInNvdXJjZUZpbGVcIiByZWxhdGl2ZSBpZiBhbiBhYnNvbHV0ZSBVcmwgaXMgcGFzc2VkLlxuICAgIGlmIChzb3VyY2VSb290ICE9IG51bGwpIHtcbiAgICAgIHNvdXJjZUZpbGUgPSB1dGlsLnJlbGF0aXZlKHNvdXJjZVJvb3QsIHNvdXJjZUZpbGUpO1xuICAgIH1cbiAgICAvLyBBcHBseWluZyB0aGUgU291cmNlTWFwIGNhbiBhZGQgYW5kIHJlbW92ZSBpdGVtcyBmcm9tIHRoZSBzb3VyY2VzIGFuZFxuICAgIC8vIHRoZSBuYW1lcyBhcnJheS5cbiAgICB2YXIgbmV3U291cmNlcyA9IG5ldyBBcnJheVNldCgpO1xuICAgIHZhciBuZXdOYW1lcyA9IG5ldyBBcnJheVNldCgpO1xuXG4gICAgLy8gRmluZCBtYXBwaW5ncyBmb3IgdGhlIFwic291cmNlRmlsZVwiXG4gICAgdGhpcy5fbWFwcGluZ3MudW5zb3J0ZWRGb3JFYWNoKGZ1bmN0aW9uIChtYXBwaW5nKSB7XG4gICAgICBpZiAobWFwcGluZy5zb3VyY2UgPT09IHNvdXJjZUZpbGUgJiYgbWFwcGluZy5vcmlnaW5hbExpbmUgIT0gbnVsbCkge1xuICAgICAgICAvLyBDaGVjayBpZiBpdCBjYW4gYmUgbWFwcGVkIGJ5IHRoZSBzb3VyY2UgbWFwLCB0aGVuIHVwZGF0ZSB0aGUgbWFwcGluZy5cbiAgICAgICAgdmFyIG9yaWdpbmFsID0gYVNvdXJjZU1hcENvbnN1bWVyLm9yaWdpbmFsUG9zaXRpb25Gb3Ioe1xuICAgICAgICAgIGxpbmU6IG1hcHBpbmcub3JpZ2luYWxMaW5lLFxuICAgICAgICAgIGNvbHVtbjogbWFwcGluZy5vcmlnaW5hbENvbHVtblxuICAgICAgICB9KTtcbiAgICAgICAgaWYgKG9yaWdpbmFsLnNvdXJjZSAhPSBudWxsKSB7XG4gICAgICAgICAgLy8gQ29weSBtYXBwaW5nXG4gICAgICAgICAgbWFwcGluZy5zb3VyY2UgPSBvcmlnaW5hbC5zb3VyY2U7XG4gICAgICAgICAgaWYgKGFTb3VyY2VNYXBQYXRoICE9IG51bGwpIHtcbiAgICAgICAgICAgIG1hcHBpbmcuc291cmNlID0gdXRpbC5qb2luKGFTb3VyY2VNYXBQYXRoLCBtYXBwaW5nLnNvdXJjZSlcbiAgICAgICAgICB9XG4gICAgICAgICAgaWYgKHNvdXJjZVJvb3QgIT0gbnVsbCkge1xuICAgICAgICAgICAgbWFwcGluZy5zb3VyY2UgPSB1dGlsLnJlbGF0aXZlKHNvdXJjZVJvb3QsIG1hcHBpbmcuc291cmNlKTtcbiAgICAgICAgICB9XG4gICAgICAgICAgbWFwcGluZy5vcmlnaW5hbExpbmUgPSBvcmlnaW5hbC5saW5lO1xuICAgICAgICAgIG1hcHBpbmcub3JpZ2luYWxDb2x1bW4gPSBvcmlnaW5hbC5jb2x1bW47XG4gICAgICAgICAgaWYgKG9yaWdpbmFsLm5hbWUgIT0gbnVsbCkge1xuICAgICAgICAgICAgbWFwcGluZy5uYW1lID0gb3JpZ2luYWwubmFtZTtcbiAgICAgICAgICB9XG4gICAgICAgIH1cbiAgICAgIH1cblxuICAgICAgdmFyIHNvdXJjZSA9IG1hcHBpbmcuc291cmNlO1xuICAgICAgaWYgKHNvdXJjZSAhPSBudWxsICYmICFuZXdTb3VyY2VzLmhhcyhzb3VyY2UpKSB7XG4gICAgICAgIG5ld1NvdXJjZXMuYWRkKHNvdXJjZSk7XG4gICAgICB9XG5cbiAgICAgIHZhciBuYW1lID0gbWFwcGluZy5uYW1lO1xuICAgICAgaWYgKG5hbWUgIT0gbnVsbCAmJiAhbmV3TmFtZXMuaGFzKG5hbWUpKSB7XG4gICAgICAgIG5ld05hbWVzLmFkZChuYW1lKTtcbiAgICAgIH1cblxuICAgIH0sIHRoaXMpO1xuICAgIHRoaXMuX3NvdXJjZXMgPSBuZXdTb3VyY2VzO1xuICAgIHRoaXMuX25hbWVzID0gbmV3TmFtZXM7XG5cbiAgICAvLyBDb3B5IHNvdXJjZXNDb250ZW50cyBvZiBhcHBsaWVkIG1hcC5cbiAgICBhU291cmNlTWFwQ29uc3VtZXIuc291cmNlcy5mb3JFYWNoKGZ1bmN0aW9uIChzb3VyY2VGaWxlKSB7XG4gICAgICB2YXIgY29udGVudCA9IGFTb3VyY2VNYXBDb25zdW1lci5zb3VyY2VDb250ZW50Rm9yKHNvdXJjZUZpbGUpO1xuICAgICAgaWYgKGNvbnRlbnQgIT0gbnVsbCkge1xuICAgICAgICBpZiAoYVNvdXJjZU1hcFBhdGggIT0gbnVsbCkge1xuICAgICAgICAgIHNvdXJjZUZpbGUgPSB1dGlsLmpvaW4oYVNvdXJjZU1hcFBhdGgsIHNvdXJjZUZpbGUpO1xuICAgICAgICB9XG4gICAgICAgIGlmIChzb3VyY2VSb290ICE9IG51bGwpIHtcbiAgICAgICAgICBzb3VyY2VGaWxlID0gdXRpbC5yZWxhdGl2ZShzb3VyY2VSb290LCBzb3VyY2VGaWxlKTtcbiAgICAgICAgfVxuICAgICAgICB0aGlzLnNldFNvdXJjZUNvbnRlbnQoc291cmNlRmlsZSwgY29udGVudCk7XG4gICAgICB9XG4gICAgfSwgdGhpcyk7XG4gIH07XG5cbi8qKlxuICogQSBtYXBwaW5nIGNhbiBoYXZlIG9uZSBvZiB0aGUgdGhyZWUgbGV2ZWxzIG9mIGRhdGE6XG4gKlxuICogICAxLiBKdXN0IHRoZSBnZW5lcmF0ZWQgcG9zaXRpb24uXG4gKiAgIDIuIFRoZSBHZW5lcmF0ZWQgcG9zaXRpb24sIG9yaWdpbmFsIHBvc2l0aW9uLCBhbmQgb3JpZ2luYWwgc291cmNlLlxuICogICAzLiBHZW5lcmF0ZWQgYW5kIG9yaWdpbmFsIHBvc2l0aW9uLCBvcmlnaW5hbCBzb3VyY2UsIGFzIHdlbGwgYXMgYSBuYW1lXG4gKiAgICAgIHRva2VuLlxuICpcbiAqIFRvIG1haW50YWluIGNvbnNpc3RlbmN5LCB3ZSB2YWxpZGF0ZSB0aGF0IGFueSBuZXcgbWFwcGluZyBiZWluZyBhZGRlZCBmYWxsc1xuICogaW4gdG8gb25lIG9mIHRoZXNlIGNhdGVnb3JpZXMuXG4gKi9cblNvdXJjZU1hcEdlbmVyYXRvci5wcm90b3R5cGUuX3ZhbGlkYXRlTWFwcGluZyA9XG4gIGZ1bmN0aW9uIFNvdXJjZU1hcEdlbmVyYXRvcl92YWxpZGF0ZU1hcHBpbmcoYUdlbmVyYXRlZCwgYU9yaWdpbmFsLCBhU291cmNlLFxuICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgIGFOYW1lKSB7XG4gICAgaWYgKGFHZW5lcmF0ZWQgJiYgJ2xpbmUnIGluIGFHZW5lcmF0ZWQgJiYgJ2NvbHVtbicgaW4gYUdlbmVyYXRlZFxuICAgICAgICAmJiBhR2VuZXJhdGVkLmxpbmUgPiAwICYmIGFHZW5lcmF0ZWQuY29sdW1uID49IDBcbiAgICAgICAgJiYgIWFPcmlnaW5hbCAmJiAhYVNvdXJjZSAmJiAhYU5hbWUpIHtcbiAgICAgIC8vIENhc2UgMS5cbiAgICAgIHJldHVybjtcbiAgICB9XG4gICAgZWxzZSBpZiAoYUdlbmVyYXRlZCAmJiAnbGluZScgaW4gYUdlbmVyYXRlZCAmJiAnY29sdW1uJyBpbiBhR2VuZXJhdGVkXG4gICAgICAgICAgICAgJiYgYU9yaWdpbmFsICYmICdsaW5lJyBpbiBhT3JpZ2luYWwgJiYgJ2NvbHVtbicgaW4gYU9yaWdpbmFsXG4gICAgICAgICAgICAgJiYgYUdlbmVyYXRlZC5saW5lID4gMCAmJiBhR2VuZXJhdGVkLmNvbHVtbiA+PSAwXG4gICAgICAgICAgICAgJiYgYU9yaWdpbmFsLmxpbmUgPiAwICYmIGFPcmlnaW5hbC5jb2x1bW4gPj0gMFxuICAgICAgICAgICAgICYmIGFTb3VyY2UpIHtcbiAgICAgIC8vIENhc2VzIDIgYW5kIDMuXG4gICAgICByZXR1cm47XG4gICAgfVxuICAgIGVsc2Uge1xuICAgICAgdGhyb3cgbmV3IEVycm9yKCdJbnZhbGlkIG1hcHBpbmc6ICcgKyBKU09OLnN0cmluZ2lmeSh7XG4gICAgICAgIGdlbmVyYXRlZDogYUdlbmVyYXRlZCxcbiAgICAgICAgc291cmNlOiBhU291cmNlLFxuICAgICAgICBvcmlnaW5hbDogYU9yaWdpbmFsLFxuICAgICAgICBuYW1lOiBhTmFtZVxuICAgICAgfSkpO1xuICAgIH1cbiAgfTtcblxuLyoqXG4gKiBTZXJpYWxpemUgdGhlIGFjY3VtdWxhdGVkIG1hcHBpbmdzIGluIHRvIHRoZSBzdHJlYW0gb2YgYmFzZSA2NCBWTFFzXG4gKiBzcGVjaWZpZWQgYnkgdGhlIHNvdXJjZSBtYXAgZm9ybWF0LlxuICovXG5Tb3VyY2VNYXBHZW5lcmF0b3IucHJvdG90eXBlLl9zZXJpYWxpemVNYXBwaW5ncyA9XG4gIGZ1bmN0aW9uIFNvdXJjZU1hcEdlbmVyYXRvcl9zZXJpYWxpemVNYXBwaW5ncygpIHtcbiAgICB2YXIgcHJldmlvdXNHZW5lcmF0ZWRDb2x1bW4gPSAwO1xuICAgIHZhciBwcmV2aW91c0dlbmVyYXRlZExpbmUgPSAxO1xuICAgIHZhciBwcmV2aW91c09yaWdpbmFsQ29sdW1uID0gMDtcbiAgICB2YXIgcHJldmlvdXNPcmlnaW5hbExpbmUgPSAwO1xuICAgIHZhciBwcmV2aW91c05hbWUgPSAwO1xuICAgIHZhciBwcmV2aW91c1NvdXJjZSA9IDA7XG4gICAgdmFyIHJlc3VsdCA9ICcnO1xuICAgIHZhciBuZXh0O1xuICAgIHZhciBtYXBwaW5nO1xuICAgIHZhciBuYW1lSWR4O1xuICAgIHZhciBzb3VyY2VJZHg7XG5cbiAgICB2YXIgbWFwcGluZ3MgPSB0aGlzLl9tYXBwaW5ncy50b0FycmF5KCk7XG4gICAgZm9yICh2YXIgaSA9IDAsIGxlbiA9IG1hcHBpbmdzLmxlbmd0aDsgaSA8IGxlbjsgaSsrKSB7XG4gICAgICBtYXBwaW5nID0gbWFwcGluZ3NbaV07XG4gICAgICBuZXh0ID0gJydcblxuICAgICAgaWYgKG1hcHBpbmcuZ2VuZXJhdGVkTGluZSAhPT0gcHJldmlvdXNHZW5lcmF0ZWRMaW5lKSB7XG4gICAgICAgIHByZXZpb3VzR2VuZXJhdGVkQ29sdW1uID0gMDtcbiAgICAgICAgd2hpbGUgKG1hcHBpbmcuZ2VuZXJhdGVkTGluZSAhPT0gcHJldmlvdXNHZW5lcmF0ZWRMaW5lKSB7XG4gICAgICAgICAgbmV4dCArPSAnOyc7XG4gICAgICAgICAgcHJldmlvdXNHZW5lcmF0ZWRMaW5lKys7XG4gICAgICAgIH1cbiAgICAgIH1cbiAgICAgIGVsc2Uge1xuICAgICAgICBpZiAoaSA+IDApIHtcbiAgICAgICAgICBpZiAoIXV0aWwuY29tcGFyZUJ5R2VuZXJhdGVkUG9zaXRpb25zSW5mbGF0ZWQobWFwcGluZywgbWFwcGluZ3NbaSAtIDFdKSkge1xuICAgICAgICAgICAgY29udGludWU7XG4gICAgICAgICAgfVxuICAgICAgICAgIG5leHQgKz0gJywnO1xuICAgICAgICB9XG4gICAgICB9XG5cbiAgICAgIG5leHQgKz0gYmFzZTY0VkxRLmVuY29kZShtYXBwaW5nLmdlbmVyYXRlZENvbHVtblxuICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgLSBwcmV2aW91c0dlbmVyYXRlZENvbHVtbik7XG4gICAgICBwcmV2aW91c0dlbmVyYXRlZENvbHVtbiA9IG1hcHBpbmcuZ2VuZXJhdGVkQ29sdW1uO1xuXG4gICAgICBpZiAobWFwcGluZy5zb3VyY2UgIT0gbnVsbCkge1xuICAgICAgICBzb3VyY2VJZHggPSB0aGlzLl9zb3VyY2VzLmluZGV4T2YobWFwcGluZy5zb3VyY2UpO1xuICAgICAgICBuZXh0ICs9IGJhc2U2NFZMUS5lbmNvZGUoc291cmNlSWR4IC0gcHJldmlvdXNTb3VyY2UpO1xuICAgICAgICBwcmV2aW91c1NvdXJjZSA9IHNvdXJjZUlkeDtcblxuICAgICAgICAvLyBsaW5lcyBhcmUgc3RvcmVkIDAtYmFzZWQgaW4gU291cmNlTWFwIHNwZWMgdmVyc2lvbiAzXG4gICAgICAgIG5leHQgKz0gYmFzZTY0VkxRLmVuY29kZShtYXBwaW5nLm9yaWdpbmFsTGluZSAtIDFcbiAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgLSBwcmV2aW91c09yaWdpbmFsTGluZSk7XG4gICAgICAgIHByZXZpb3VzT3JpZ2luYWxMaW5lID0gbWFwcGluZy5vcmlnaW5hbExpbmUgLSAxO1xuXG4gICAgICAgIG5leHQgKz0gYmFzZTY0VkxRLmVuY29kZShtYXBwaW5nLm9yaWdpbmFsQ29sdW1uXG4gICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgIC0gcHJldmlvdXNPcmlnaW5hbENvbHVtbik7XG4gICAgICAgIHByZXZpb3VzT3JpZ2luYWxDb2x1bW4gPSBtYXBwaW5nLm9yaWdpbmFsQ29sdW1uO1xuXG4gICAgICAgIGlmIChtYXBwaW5nLm5hbWUgIT0gbnVsbCkge1xuICAgICAgICAgIG5hbWVJZHggPSB0aGlzLl9uYW1lcy5pbmRleE9mKG1hcHBpbmcubmFtZSk7XG4gICAgICAgICAgbmV4dCArPSBiYXNlNjRWTFEuZW5jb2RlKG5hbWVJZHggLSBwcmV2aW91c05hbWUpO1xuICAgICAgICAgIHByZXZpb3VzTmFtZSA9IG5hbWVJZHg7XG4gICAgICAgIH1cbiAgICAgIH1cblxuICAgICAgcmVzdWx0ICs9IG5leHQ7XG4gICAgfVxuXG4gICAgcmV0dXJuIHJlc3VsdDtcbiAgfTtcblxuU291cmNlTWFwR2VuZXJhdG9yLnByb3RvdHlwZS5fZ2VuZXJhdGVTb3VyY2VzQ29udGVudCA9XG4gIGZ1bmN0aW9uIFNvdXJjZU1hcEdlbmVyYXRvcl9nZW5lcmF0ZVNvdXJjZXNDb250ZW50KGFTb3VyY2VzLCBhU291cmNlUm9vdCkge1xuICAgIHJldHVybiBhU291cmNlcy5tYXAoZnVuY3Rpb24gKHNvdXJjZSkge1xuICAgICAgaWYgKCF0aGlzLl9zb3VyY2VzQ29udGVudHMpIHtcbiAgICAgICAgcmV0dXJuIG51bGw7XG4gICAgICB9XG4gICAgICBpZiAoYVNvdXJjZVJvb3QgIT0gbnVsbCkge1xuICAgICAgICBzb3VyY2UgPSB1dGlsLnJlbGF0aXZlKGFTb3VyY2VSb290LCBzb3VyY2UpO1xuICAgICAgfVxuICAgICAgdmFyIGtleSA9IHV0aWwudG9TZXRTdHJpbmcoc291cmNlKTtcbiAgICAgIHJldHVybiBPYmplY3QucHJvdG90eXBlLmhhc093blByb3BlcnR5LmNhbGwodGhpcy5fc291cmNlc0NvbnRlbnRzLCBrZXkpXG4gICAgICAgID8gdGhpcy5fc291cmNlc0NvbnRlbnRzW2tleV1cbiAgICAgICAgOiBudWxsO1xuICAgIH0sIHRoaXMpO1xuICB9O1xuXG4vKipcbiAqIEV4dGVybmFsaXplIHRoZSBzb3VyY2UgbWFwLlxuICovXG5Tb3VyY2VNYXBHZW5lcmF0b3IucHJvdG90eXBlLnRvSlNPTiA9XG4gIGZ1bmN0aW9uIFNvdXJjZU1hcEdlbmVyYXRvcl90b0pTT04oKSB7XG4gICAgdmFyIG1hcCA9IHtcbiAgICAgIHZlcnNpb246IHRoaXMuX3ZlcnNpb24sXG4gICAgICBzb3VyY2VzOiB0aGlzLl9zb3VyY2VzLnRvQXJyYXkoKSxcbiAgICAgIG5hbWVzOiB0aGlzLl9uYW1lcy50b0FycmF5KCksXG4gICAgICBtYXBwaW5nczogdGhpcy5fc2VyaWFsaXplTWFwcGluZ3MoKVxuICAgIH07XG4gICAgaWYgKHRoaXMuX2ZpbGUgIT0gbnVsbCkge1xuICAgICAgbWFwLmZpbGUgPSB0aGlzLl9maWxlO1xuICAgIH1cbiAgICBpZiAodGhpcy5fc291cmNlUm9vdCAhPSBudWxsKSB7XG4gICAgICBtYXAuc291cmNlUm9vdCA9IHRoaXMuX3NvdXJjZVJvb3Q7XG4gICAgfVxuICAgIGlmICh0aGlzLl9zb3VyY2VzQ29udGVudHMpIHtcbiAgICAgIG1hcC5zb3VyY2VzQ29udGVudCA9IHRoaXMuX2dlbmVyYXRlU291cmNlc0NvbnRlbnQobWFwLnNvdXJjZXMsIG1hcC5zb3VyY2VSb290KTtcbiAgICB9XG5cbiAgICByZXR1cm4gbWFwO1xuICB9O1xuXG4vKipcbiAqIFJlbmRlciB0aGUgc291cmNlIG1hcCBiZWluZyBnZW5lcmF0ZWQgdG8gYSBzdHJpbmcuXG4gKi9cblNvdXJjZU1hcEdlbmVyYXRvci5wcm90b3R5cGUudG9TdHJpbmcgPVxuICBmdW5jdGlvbiBTb3VyY2VNYXBHZW5lcmF0b3JfdG9TdHJpbmcoKSB7XG4gICAgcmV0dXJuIEpTT04uc3RyaW5naWZ5KHRoaXMudG9KU09OKCkpO1xuICB9O1xuXG5leHBvcnRzLlNvdXJjZU1hcEdlbmVyYXRvciA9IFNvdXJjZU1hcEdlbmVyYXRvcjtcblxuXG5cbi8qKioqKioqKioqKioqKioqKlxuICoqIFdFQlBBQ0sgRk9PVEVSXG4gKiogLi9saWIvc291cmNlLW1hcC1nZW5lcmF0b3IuanNcbiAqKiBtb2R1bGUgaWQgPSAxXG4gKiogbW9kdWxlIGNodW5rcyA9IDBcbiAqKi8iLCIvKiAtKi0gTW9kZToganM7IGpzLWluZGVudC1sZXZlbDogMjsgLSotICovXG4vKlxuICogQ29weXJpZ2h0IDIwMTEgTW96aWxsYSBGb3VuZGF0aW9uIGFuZCBjb250cmlidXRvcnNcbiAqIExpY2Vuc2VkIHVuZGVyIHRoZSBOZXcgQlNEIGxpY2Vuc2UuIFNlZSBMSUNFTlNFIG9yOlxuICogaHR0cDovL29wZW5zb3VyY2Uub3JnL2xpY2Vuc2VzL0JTRC0zLUNsYXVzZVxuICpcbiAqIEJhc2VkIG9uIHRoZSBCYXNlIDY0IFZMUSBpbXBsZW1lbnRhdGlvbiBpbiBDbG9zdXJlIENvbXBpbGVyOlxuICogaHR0cHM6Ly9jb2RlLmdvb2dsZS5jb20vcC9jbG9zdXJlLWNvbXBpbGVyL3NvdXJjZS9icm93c2UvdHJ1bmsvc3JjL2NvbS9nb29nbGUvZGVidWdnaW5nL3NvdXJjZW1hcC9CYXNlNjRWTFEuamF2YVxuICpcbiAqIENvcHlyaWdodCAyMDExIFRoZSBDbG9zdXJlIENvbXBpbGVyIEF1dGhvcnMuIEFsbCByaWdodHMgcmVzZXJ2ZWQuXG4gKiBSZWRpc3RyaWJ1dGlvbiBhbmQgdXNlIGluIHNvdXJjZSBhbmQgYmluYXJ5IGZvcm1zLCB3aXRoIG9yIHdpdGhvdXRcbiAqIG1vZGlmaWNhdGlvbiwgYXJlIHBlcm1pdHRlZCBwcm92aWRlZCB0aGF0IHRoZSBmb2xsb3dpbmcgY29uZGl0aW9ucyBhcmVcbiAqIG1ldDpcbiAqXG4gKiAgKiBSZWRpc3RyaWJ1dGlvbnMgb2Ygc291cmNlIGNvZGUgbXVzdCByZXRhaW4gdGhlIGFib3ZlIGNvcHlyaWdodFxuICogICAgbm90aWNlLCB0aGlzIGxpc3Qgb2YgY29uZGl0aW9ucyBhbmQgdGhlIGZvbGxvd2luZyBkaXNjbGFpbWVyLlxuICogICogUmVkaXN0cmlidXRpb25zIGluIGJpbmFyeSBmb3JtIG11c3QgcmVwcm9kdWNlIHRoZSBhYm92ZVxuICogICAgY29weXJpZ2h0IG5vdGljZSwgdGhpcyBsaXN0IG9mIGNvbmRpdGlvbnMgYW5kIHRoZSBmb2xsb3dpbmdcbiAqICAgIGRpc2NsYWltZXIgaW4gdGhlIGRvY3VtZW50YXRpb24gYW5kL29yIG90aGVyIG1hdGVyaWFscyBwcm92aWRlZFxuICogICAgd2l0aCB0aGUgZGlzdHJpYnV0aW9uLlxuICogICogTmVpdGhlciB0aGUgbmFtZSBvZiBHb29nbGUgSW5jLiBub3IgdGhlIG5hbWVzIG9mIGl0c1xuICogICAgY29udHJpYnV0b3JzIG1heSBiZSB1c2VkIHRvIGVuZG9yc2Ugb3IgcHJvbW90ZSBwcm9kdWN0cyBkZXJpdmVkXG4gKiAgICBmcm9tIHRoaXMgc29mdHdhcmUgd2l0aG91dCBzcGVjaWZpYyBwcmlvciB3cml0dGVuIHBlcm1pc3Npb24uXG4gKlxuICogVEhJUyBTT0ZUV0FSRSBJUyBQUk9WSURFRCBCWSBUSEUgQ09QWVJJR0hUIEhPTERFUlMgQU5EIENPTlRSSUJVVE9SU1xuICogXCJBUyBJU1wiIEFORCBBTlkgRVhQUkVTUyBPUiBJTVBMSUVEIFdBUlJBTlRJRVMsIElOQ0xVRElORywgQlVUIE5PVFxuICogTElNSVRFRCBUTywgVEhFIElNUExJRUQgV0FSUkFOVElFUyBPRiBNRVJDSEFOVEFCSUxJVFkgQU5EIEZJVE5FU1MgRk9SXG4gKiBBIFBBUlRJQ1VMQVIgUFVSUE9TRSBBUkUgRElTQ0xBSU1FRC4gSU4gTk8gRVZFTlQgU0hBTEwgVEhFIENPUFlSSUdIVFxuICogT1dORVIgT1IgQ09OVFJJQlVUT1JTIEJFIExJQUJMRSBGT1IgQU5ZIERJUkVDVCwgSU5ESVJFQ1QsIElOQ0lERU5UQUwsXG4gKiBTUEVDSUFMLCBFWEVNUExBUlksIE9SIENPTlNFUVVFTlRJQUwgREFNQUdFUyAoSU5DTFVESU5HLCBCVVQgTk9UXG4gKiBMSU1JVEVEIFRPLCBQUk9DVVJFTUVOVCBPRiBTVUJTVElUVVRFIEdPT0RTIE9SIFNFUlZJQ0VTOyBMT1NTIE9GIFVTRSxcbiAqIERBVEEsIE9SIFBST0ZJVFM7IE9SIEJVU0lORVNTIElOVEVSUlVQVElPTikgSE9XRVZFUiBDQVVTRUQgQU5EIE9OIEFOWVxuICogVEhFT1JZIE9GIExJQUJJTElUWSwgV0hFVEhFUiBJTiBDT05UUkFDVCwgU1RSSUNUIExJQUJJTElUWSwgT1IgVE9SVFxuICogKElOQ0xVRElORyBORUdMSUdFTkNFIE9SIE9USEVSV0lTRSkgQVJJU0lORyBJTiBBTlkgV0FZIE9VVCBPRiBUSEUgVVNFXG4gKiBPRiBUSElTIFNPRlRXQVJFLCBFVkVOIElGIEFEVklTRUQgT0YgVEhFIFBPU1NJQklMSVRZIE9GIFNVQ0ggREFNQUdFLlxuICovXG5cbnZhciBiYXNlNjQgPSByZXF1aXJlKCcuL2Jhc2U2NCcpO1xuXG4vLyBBIHNpbmdsZSBiYXNlIDY0IGRpZ2l0IGNhbiBjb250YWluIDYgYml0cyBvZiBkYXRhLiBGb3IgdGhlIGJhc2UgNjQgdmFyaWFibGVcbi8vIGxlbmd0aCBxdWFudGl0aWVzIHdlIHVzZSBpbiB0aGUgc291cmNlIG1hcCBzcGVjLCB0aGUgZmlyc3QgYml0IGlzIHRoZSBzaWduLFxuLy8gdGhlIG5leHQgZm91ciBiaXRzIGFyZSB0aGUgYWN0dWFsIHZhbHVlLCBhbmQgdGhlIDZ0aCBiaXQgaXMgdGhlXG4vLyBjb250aW51YXRpb24gYml0LiBUaGUgY29udGludWF0aW9uIGJpdCB0ZWxscyB1cyB3aGV0aGVyIHRoZXJlIGFyZSBtb3JlXG4vLyBkaWdpdHMgaW4gdGhpcyB2YWx1ZSBmb2xsb3dpbmcgdGhpcyBkaWdpdC5cbi8vXG4vLyAgIENvbnRpbnVhdGlvblxuLy8gICB8ICAgIFNpZ25cbi8vICAgfCAgICB8XG4vLyAgIFYgICAgVlxuLy8gICAxMDEwMTFcblxudmFyIFZMUV9CQVNFX1NISUZUID0gNTtcblxuLy8gYmluYXJ5OiAxMDAwMDBcbnZhciBWTFFfQkFTRSA9IDEgPDwgVkxRX0JBU0VfU0hJRlQ7XG5cbi8vIGJpbmFyeTogMDExMTExXG52YXIgVkxRX0JBU0VfTUFTSyA9IFZMUV9CQVNFIC0gMTtcblxuLy8gYmluYXJ5OiAxMDAwMDBcbnZhciBWTFFfQ09OVElOVUFUSU9OX0JJVCA9IFZMUV9CQVNFO1xuXG4vKipcbiAqIENvbnZlcnRzIGZyb20gYSB0d28tY29tcGxlbWVudCB2YWx1ZSB0byBhIHZhbHVlIHdoZXJlIHRoZSBzaWduIGJpdCBpc1xuICogcGxhY2VkIGluIHRoZSBsZWFzdCBzaWduaWZpY2FudCBiaXQuICBGb3IgZXhhbXBsZSwgYXMgZGVjaW1hbHM6XG4gKiAgIDEgYmVjb21lcyAyICgxMCBiaW5hcnkpLCAtMSBiZWNvbWVzIDMgKDExIGJpbmFyeSlcbiAqICAgMiBiZWNvbWVzIDQgKDEwMCBiaW5hcnkpLCAtMiBiZWNvbWVzIDUgKDEwMSBiaW5hcnkpXG4gKi9cbmZ1bmN0aW9uIHRvVkxRU2lnbmVkKGFWYWx1ZSkge1xuICByZXR1cm4gYVZhbHVlIDwgMFxuICAgID8gKCgtYVZhbHVlKSA8PCAxKSArIDFcbiAgICA6IChhVmFsdWUgPDwgMSkgKyAwO1xufVxuXG4vKipcbiAqIENvbnZlcnRzIHRvIGEgdHdvLWNvbXBsZW1lbnQgdmFsdWUgZnJvbSBhIHZhbHVlIHdoZXJlIHRoZSBzaWduIGJpdCBpc1xuICogcGxhY2VkIGluIHRoZSBsZWFzdCBzaWduaWZpY2FudCBiaXQuICBGb3IgZXhhbXBsZSwgYXMgZGVjaW1hbHM6XG4gKiAgIDIgKDEwIGJpbmFyeSkgYmVjb21lcyAxLCAzICgxMSBiaW5hcnkpIGJlY29tZXMgLTFcbiAqICAgNCAoMTAwIGJpbmFyeSkgYmVjb21lcyAyLCA1ICgxMDEgYmluYXJ5KSBiZWNvbWVzIC0yXG4gKi9cbmZ1bmN0aW9uIGZyb21WTFFTaWduZWQoYVZhbHVlKSB7XG4gIHZhciBpc05lZ2F0aXZlID0gKGFWYWx1ZSAmIDEpID09PSAxO1xuICB2YXIgc2hpZnRlZCA9IGFWYWx1ZSA+PiAxO1xuICByZXR1cm4gaXNOZWdhdGl2ZVxuICAgID8gLXNoaWZ0ZWRcbiAgICA6IHNoaWZ0ZWQ7XG59XG5cbi8qKlxuICogUmV0dXJucyB0aGUgYmFzZSA2NCBWTFEgZW5jb2RlZCB2YWx1ZS5cbiAqL1xuZXhwb3J0cy5lbmNvZGUgPSBmdW5jdGlvbiBiYXNlNjRWTFFfZW5jb2RlKGFWYWx1ZSkge1xuICB2YXIgZW5jb2RlZCA9IFwiXCI7XG4gIHZhciBkaWdpdDtcblxuICB2YXIgdmxxID0gdG9WTFFTaWduZWQoYVZhbHVlKTtcblxuICBkbyB7XG4gICAgZGlnaXQgPSB2bHEgJiBWTFFfQkFTRV9NQVNLO1xuICAgIHZscSA+Pj49IFZMUV9CQVNFX1NISUZUO1xuICAgIGlmICh2bHEgPiAwKSB7XG4gICAgICAvLyBUaGVyZSBhcmUgc3RpbGwgbW9yZSBkaWdpdHMgaW4gdGhpcyB2YWx1ZSwgc28gd2UgbXVzdCBtYWtlIHN1cmUgdGhlXG4gICAgICAvLyBjb250aW51YXRpb24gYml0IGlzIG1hcmtlZC5cbiAgICAgIGRpZ2l0IHw9IFZMUV9DT05USU5VQVRJT05fQklUO1xuICAgIH1cbiAgICBlbmNvZGVkICs9IGJhc2U2NC5lbmNvZGUoZGlnaXQpO1xuICB9IHdoaWxlICh2bHEgPiAwKTtcblxuICByZXR1cm4gZW5jb2RlZDtcbn07XG5cbi8qKlxuICogRGVjb2RlcyB0aGUgbmV4dCBiYXNlIDY0IFZMUSB2YWx1ZSBmcm9tIHRoZSBnaXZlbiBzdHJpbmcgYW5kIHJldHVybnMgdGhlXG4gKiB2YWx1ZSBhbmQgdGhlIHJlc3Qgb2YgdGhlIHN0cmluZyB2aWEgdGhlIG91dCBwYXJhbWV0ZXIuXG4gKi9cbmV4cG9ydHMuZGVjb2RlID0gZnVuY3Rpb24gYmFzZTY0VkxRX2RlY29kZShhU3RyLCBhSW5kZXgsIGFPdXRQYXJhbSkge1xuICB2YXIgc3RyTGVuID0gYVN0ci5sZW5ndGg7XG4gIHZhciByZXN1bHQgPSAwO1xuICB2YXIgc2hpZnQgPSAwO1xuICB2YXIgY29udGludWF0aW9uLCBkaWdpdDtcblxuICBkbyB7XG4gICAgaWYgKGFJbmRleCA+PSBzdHJMZW4pIHtcbiAgICAgIHRocm93IG5ldyBFcnJvcihcIkV4cGVjdGVkIG1vcmUgZGlnaXRzIGluIGJhc2UgNjQgVkxRIHZhbHVlLlwiKTtcbiAgICB9XG5cbiAgICBkaWdpdCA9IGJhc2U2NC5kZWNvZGUoYVN0ci5jaGFyQ29kZUF0KGFJbmRleCsrKSk7XG4gICAgaWYgKGRpZ2l0ID09PSAtMSkge1xuICAgICAgdGhyb3cgbmV3IEVycm9yKFwiSW52YWxpZCBiYXNlNjQgZGlnaXQ6IFwiICsgYVN0ci5jaGFyQXQoYUluZGV4IC0gMSkpO1xuICAgIH1cblxuICAgIGNvbnRpbnVhdGlvbiA9ICEhKGRpZ2l0ICYgVkxRX0NPTlRJTlVBVElPTl9CSVQpO1xuICAgIGRpZ2l0ICY9IFZMUV9CQVNFX01BU0s7XG4gICAgcmVzdWx0ID0gcmVzdWx0ICsgKGRpZ2l0IDw8IHNoaWZ0KTtcbiAgICBzaGlmdCArPSBWTFFfQkFTRV9TSElGVDtcbiAgfSB3aGlsZSAoY29udGludWF0aW9uKTtcblxuICBhT3V0UGFyYW0udmFsdWUgPSBmcm9tVkxRU2lnbmVkKHJlc3VsdCk7XG4gIGFPdXRQYXJhbS5yZXN0ID0gYUluZGV4O1xufTtcblxuXG5cbi8qKioqKioqKioqKioqKioqKlxuICoqIFdFQlBBQ0sgRk9PVEVSXG4gKiogLi9saWIvYmFzZTY0LXZscS5qc1xuICoqIG1vZHVsZSBpZCA9IDJcbiAqKiBtb2R1bGUgY2h1bmtzID0gMFxuICoqLyIsIi8qIC0qLSBNb2RlOiBqczsganMtaW5kZW50LWxldmVsOiAyOyAtKi0gKi9cbi8qXG4gKiBDb3B5cmlnaHQgMjAxMSBNb3ppbGxhIEZvdW5kYXRpb24gYW5kIGNvbnRyaWJ1dG9yc1xuICogTGljZW5zZWQgdW5kZXIgdGhlIE5ldyBCU0QgbGljZW5zZS4gU2VlIExJQ0VOU0Ugb3I6XG4gKiBodHRwOi8vb3BlbnNvdXJjZS5vcmcvbGljZW5zZXMvQlNELTMtQ2xhdXNlXG4gKi9cblxudmFyIGludFRvQ2hhck1hcCA9ICdBQkNERUZHSElKS0xNTk9QUVJTVFVWV1hZWmFiY2RlZmdoaWprbG1ub3BxcnN0dXZ3eHl6MDEyMzQ1Njc4OSsvJy5zcGxpdCgnJyk7XG5cbi8qKlxuICogRW5jb2RlIGFuIGludGVnZXIgaW4gdGhlIHJhbmdlIG9mIDAgdG8gNjMgdG8gYSBzaW5nbGUgYmFzZSA2NCBkaWdpdC5cbiAqL1xuZXhwb3J0cy5lbmNvZGUgPSBmdW5jdGlvbiAobnVtYmVyKSB7XG4gIGlmICgwIDw9IG51bWJlciAmJiBudW1iZXIgPCBpbnRUb0NoYXJNYXAubGVuZ3RoKSB7XG4gICAgcmV0dXJuIGludFRvQ2hhck1hcFtudW1iZXJdO1xuICB9XG4gIHRocm93IG5ldyBUeXBlRXJyb3IoXCJNdXN0IGJlIGJldHdlZW4gMCBhbmQgNjM6IFwiICsgbnVtYmVyKTtcbn07XG5cbi8qKlxuICogRGVjb2RlIGEgc2luZ2xlIGJhc2UgNjQgY2hhcmFjdGVyIGNvZGUgZGlnaXQgdG8gYW4gaW50ZWdlci4gUmV0dXJucyAtMSBvblxuICogZmFpbHVyZS5cbiAqL1xuZXhwb3J0cy5kZWNvZGUgPSBmdW5jdGlvbiAoY2hhckNvZGUpIHtcbiAgdmFyIGJpZ0EgPSA2NTsgICAgIC8vICdBJ1xuICB2YXIgYmlnWiA9IDkwOyAgICAgLy8gJ1onXG5cbiAgdmFyIGxpdHRsZUEgPSA5NzsgIC8vICdhJ1xuICB2YXIgbGl0dGxlWiA9IDEyMjsgLy8gJ3onXG5cbiAgdmFyIHplcm8gPSA0ODsgICAgIC8vICcwJ1xuICB2YXIgbmluZSA9IDU3OyAgICAgLy8gJzknXG5cbiAgdmFyIHBsdXMgPSA0MzsgICAgIC8vICcrJ1xuICB2YXIgc2xhc2ggPSA0NzsgICAgLy8gJy8nXG5cbiAgdmFyIGxpdHRsZU9mZnNldCA9IDI2O1xuICB2YXIgbnVtYmVyT2Zmc2V0ID0gNTI7XG5cbiAgLy8gMCAtIDI1OiBBQkNERUZHSElKS0xNTk9QUVJTVFVWV1hZWlxuICBpZiAoYmlnQSA8PSBjaGFyQ29kZSAmJiBjaGFyQ29kZSA8PSBiaWdaKSB7XG4gICAgcmV0dXJuIChjaGFyQ29kZSAtIGJpZ0EpO1xuICB9XG5cbiAgLy8gMjYgLSA1MTogYWJjZGVmZ2hpamtsbW5vcHFyc3R1dnd4eXpcbiAgaWYgKGxpdHRsZUEgPD0gY2hhckNvZGUgJiYgY2hhckNvZGUgPD0gbGl0dGxlWikge1xuICAgIHJldHVybiAoY2hhckNvZGUgLSBsaXR0bGVBICsgbGl0dGxlT2Zmc2V0KTtcbiAgfVxuXG4gIC8vIDUyIC0gNjE6IDAxMjM0NTY3ODlcbiAgaWYgKHplcm8gPD0gY2hhckNvZGUgJiYgY2hhckNvZGUgPD0gbmluZSkge1xuICAgIHJldHVybiAoY2hhckNvZGUgLSB6ZXJvICsgbnVtYmVyT2Zmc2V0KTtcbiAgfVxuXG4gIC8vIDYyOiArXG4gIGlmIChjaGFyQ29kZSA9PSBwbHVzKSB7XG4gICAgcmV0dXJuIDYyO1xuICB9XG5cbiAgLy8gNjM6IC9cbiAgaWYgKGNoYXJDb2RlID09IHNsYXNoKSB7XG4gICAgcmV0dXJuIDYzO1xuICB9XG5cbiAgLy8gSW52YWxpZCBiYXNlNjQgZGlnaXQuXG4gIHJldHVybiAtMTtcbn07XG5cblxuXG4vKioqKioqKioqKioqKioqKipcbiAqKiBXRUJQQUNLIEZPT1RFUlxuICoqIC4vbGliL2Jhc2U2NC5qc1xuICoqIG1vZHVsZSBpZCA9IDNcbiAqKiBtb2R1bGUgY2h1bmtzID0gMFxuICoqLyIsIi8qIC0qLSBNb2RlOiBqczsganMtaW5kZW50LWxldmVsOiAyOyAtKi0gKi9cbi8qXG4gKiBDb3B5cmlnaHQgMjAxMSBNb3ppbGxhIEZvdW5kYXRpb24gYW5kIGNvbnRyaWJ1dG9yc1xuICogTGljZW5zZWQgdW5kZXIgdGhlIE5ldyBCU0QgbGljZW5zZS4gU2VlIExJQ0VOU0Ugb3I6XG4gKiBodHRwOi8vb3BlbnNvdXJjZS5vcmcvbGljZW5zZXMvQlNELTMtQ2xhdXNlXG4gKi9cblxuLyoqXG4gKiBUaGlzIGlzIGEgaGVscGVyIGZ1bmN0aW9uIGZvciBnZXR0aW5nIHZhbHVlcyBmcm9tIHBhcmFtZXRlci9vcHRpb25zXG4gKiBvYmplY3RzLlxuICpcbiAqIEBwYXJhbSBhcmdzIFRoZSBvYmplY3Qgd2UgYXJlIGV4dHJhY3RpbmcgdmFsdWVzIGZyb21cbiAqIEBwYXJhbSBuYW1lIFRoZSBuYW1lIG9mIHRoZSBwcm9wZXJ0eSB3ZSBhcmUgZ2V0dGluZy5cbiAqIEBwYXJhbSBkZWZhdWx0VmFsdWUgQW4gb3B0aW9uYWwgdmFsdWUgdG8gcmV0dXJuIGlmIHRoZSBwcm9wZXJ0eSBpcyBtaXNzaW5nXG4gKiBmcm9tIHRoZSBvYmplY3QuIElmIHRoaXMgaXMgbm90IHNwZWNpZmllZCBhbmQgdGhlIHByb3BlcnR5IGlzIG1pc3NpbmcsIGFuXG4gKiBlcnJvciB3aWxsIGJlIHRocm93bi5cbiAqL1xuZnVuY3Rpb24gZ2V0QXJnKGFBcmdzLCBhTmFtZSwgYURlZmF1bHRWYWx1ZSkge1xuICBpZiAoYU5hbWUgaW4gYUFyZ3MpIHtcbiAgICByZXR1cm4gYUFyZ3NbYU5hbWVdO1xuICB9IGVsc2UgaWYgKGFyZ3VtZW50cy5sZW5ndGggPT09IDMpIHtcbiAgICByZXR1cm4gYURlZmF1bHRWYWx1ZTtcbiAgfSBlbHNlIHtcbiAgICB0aHJvdyBuZXcgRXJyb3IoJ1wiJyArIGFOYW1lICsgJ1wiIGlzIGEgcmVxdWlyZWQgYXJndW1lbnQuJyk7XG4gIH1cbn1cbmV4cG9ydHMuZ2V0QXJnID0gZ2V0QXJnO1xuXG52YXIgdXJsUmVnZXhwID0gL14oPzooW1xcdytcXC0uXSspOik/XFwvXFwvKD86KFxcdys6XFx3KylAKT8oW1xcdy5dKikoPzo6KFxcZCspKT8oXFxTKikkLztcbnZhciBkYXRhVXJsUmVnZXhwID0gL15kYXRhOi4rXFwsLiskLztcblxuZnVuY3Rpb24gdXJsUGFyc2UoYVVybCkge1xuICB2YXIgbWF0Y2ggPSBhVXJsLm1hdGNoKHVybFJlZ2V4cCk7XG4gIGlmICghbWF0Y2gpIHtcbiAgICByZXR1cm4gbnVsbDtcbiAgfVxuICByZXR1cm4ge1xuICAgIHNjaGVtZTogbWF0Y2hbMV0sXG4gICAgYXV0aDogbWF0Y2hbMl0sXG4gICAgaG9zdDogbWF0Y2hbM10sXG4gICAgcG9ydDogbWF0Y2hbNF0sXG4gICAgcGF0aDogbWF0Y2hbNV1cbiAgfTtcbn1cbmV4cG9ydHMudXJsUGFyc2UgPSB1cmxQYXJzZTtcblxuZnVuY3Rpb24gdXJsR2VuZXJhdGUoYVBhcnNlZFVybCkge1xuICB2YXIgdXJsID0gJyc7XG4gIGlmIChhUGFyc2VkVXJsLnNjaGVtZSkge1xuICAgIHVybCArPSBhUGFyc2VkVXJsLnNjaGVtZSArICc6JztcbiAgfVxuICB1cmwgKz0gJy8vJztcbiAgaWYgKGFQYXJzZWRVcmwuYXV0aCkge1xuICAgIHVybCArPSBhUGFyc2VkVXJsLmF1dGggKyAnQCc7XG4gIH1cbiAgaWYgKGFQYXJzZWRVcmwuaG9zdCkge1xuICAgIHVybCArPSBhUGFyc2VkVXJsLmhvc3Q7XG4gIH1cbiAgaWYgKGFQYXJzZWRVcmwucG9ydCkge1xuICAgIHVybCArPSBcIjpcIiArIGFQYXJzZWRVcmwucG9ydFxuICB9XG4gIGlmIChhUGFyc2VkVXJsLnBhdGgpIHtcbiAgICB1cmwgKz0gYVBhcnNlZFVybC5wYXRoO1xuICB9XG4gIHJldHVybiB1cmw7XG59XG5leHBvcnRzLnVybEdlbmVyYXRlID0gdXJsR2VuZXJhdGU7XG5cbi8qKlxuICogTm9ybWFsaXplcyBhIHBhdGgsIG9yIHRoZSBwYXRoIHBvcnRpb24gb2YgYSBVUkw6XG4gKlxuICogLSBSZXBsYWNlcyBjb25zZXF1dGl2ZSBzbGFzaGVzIHdpdGggb25lIHNsYXNoLlxuICogLSBSZW1vdmVzIHVubmVjZXNzYXJ5ICcuJyBwYXJ0cy5cbiAqIC0gUmVtb3ZlcyB1bm5lY2Vzc2FyeSAnPGRpcj4vLi4nIHBhcnRzLlxuICpcbiAqIEJhc2VkIG9uIGNvZGUgaW4gdGhlIE5vZGUuanMgJ3BhdGgnIGNvcmUgbW9kdWxlLlxuICpcbiAqIEBwYXJhbSBhUGF0aCBUaGUgcGF0aCBvciB1cmwgdG8gbm9ybWFsaXplLlxuICovXG5mdW5jdGlvbiBub3JtYWxpemUoYVBhdGgpIHtcbiAgdmFyIHBhdGggPSBhUGF0aDtcbiAgdmFyIHVybCA9IHVybFBhcnNlKGFQYXRoKTtcbiAgaWYgKHVybCkge1xuICAgIGlmICghdXJsLnBhdGgpIHtcbiAgICAgIHJldHVybiBhUGF0aDtcbiAgICB9XG4gICAgcGF0aCA9IHVybC5wYXRoO1xuICB9XG4gIHZhciBpc0Fic29sdXRlID0gZXhwb3J0cy5pc0Fic29sdXRlKHBhdGgpO1xuXG4gIHZhciBwYXJ0cyA9IHBhdGguc3BsaXQoL1xcLysvKTtcbiAgZm9yICh2YXIgcGFydCwgdXAgPSAwLCBpID0gcGFydHMubGVuZ3RoIC0gMTsgaSA+PSAwOyBpLS0pIHtcbiAgICBwYXJ0ID0gcGFydHNbaV07XG4gICAgaWYgKHBhcnQgPT09ICcuJykge1xuICAgICAgcGFydHMuc3BsaWNlKGksIDEpO1xuICAgIH0gZWxzZSBpZiAocGFydCA9PT0gJy4uJykge1xuICAgICAgdXArKztcbiAgICB9IGVsc2UgaWYgKHVwID4gMCkge1xuICAgICAgaWYgKHBhcnQgPT09ICcnKSB7XG4gICAgICAgIC8vIFRoZSBmaXJzdCBwYXJ0IGlzIGJsYW5rIGlmIHRoZSBwYXRoIGlzIGFic29sdXRlLiBUcnlpbmcgdG8gZ29cbiAgICAgICAgLy8gYWJvdmUgdGhlIHJvb3QgaXMgYSBuby1vcC4gVGhlcmVmb3JlIHdlIGNhbiByZW1vdmUgYWxsICcuLicgcGFydHNcbiAgICAgICAgLy8gZGlyZWN0bHkgYWZ0ZXIgdGhlIHJvb3QuXG4gICAgICAgIHBhcnRzLnNwbGljZShpICsgMSwgdXApO1xuICAgICAgICB1cCA9IDA7XG4gICAgICB9IGVsc2Uge1xuICAgICAgICBwYXJ0cy5zcGxpY2UoaSwgMik7XG4gICAgICAgIHVwLS07XG4gICAgICB9XG4gICAgfVxuICB9XG4gIHBhdGggPSBwYXJ0cy5qb2luKCcvJyk7XG5cbiAgaWYgKHBhdGggPT09ICcnKSB7XG4gICAgcGF0aCA9IGlzQWJzb2x1dGUgPyAnLycgOiAnLic7XG4gIH1cblxuICBpZiAodXJsKSB7XG4gICAgdXJsLnBhdGggPSBwYXRoO1xuICAgIHJldHVybiB1cmxHZW5lcmF0ZSh1cmwpO1xuICB9XG4gIHJldHVybiBwYXRoO1xufVxuZXhwb3J0cy5ub3JtYWxpemUgPSBub3JtYWxpemU7XG5cbi8qKlxuICogSm9pbnMgdHdvIHBhdGhzL1VSTHMuXG4gKlxuICogQHBhcmFtIGFSb290IFRoZSByb290IHBhdGggb3IgVVJMLlxuICogQHBhcmFtIGFQYXRoIFRoZSBwYXRoIG9yIFVSTCB0byBiZSBqb2luZWQgd2l0aCB0aGUgcm9vdC5cbiAqXG4gKiAtIElmIGFQYXRoIGlzIGEgVVJMIG9yIGEgZGF0YSBVUkksIGFQYXRoIGlzIHJldHVybmVkLCB1bmxlc3MgYVBhdGggaXMgYVxuICogICBzY2hlbWUtcmVsYXRpdmUgVVJMOiBUaGVuIHRoZSBzY2hlbWUgb2YgYVJvb3QsIGlmIGFueSwgaXMgcHJlcGVuZGVkXG4gKiAgIGZpcnN0LlxuICogLSBPdGhlcndpc2UgYVBhdGggaXMgYSBwYXRoLiBJZiBhUm9vdCBpcyBhIFVSTCwgdGhlbiBpdHMgcGF0aCBwb3J0aW9uXG4gKiAgIGlzIHVwZGF0ZWQgd2l0aCB0aGUgcmVzdWx0IGFuZCBhUm9vdCBpcyByZXR1cm5lZC4gT3RoZXJ3aXNlIHRoZSByZXN1bHRcbiAqICAgaXMgcmV0dXJuZWQuXG4gKiAgIC0gSWYgYVBhdGggaXMgYWJzb2x1dGUsIHRoZSByZXN1bHQgaXMgYVBhdGguXG4gKiAgIC0gT3RoZXJ3aXNlIHRoZSB0d28gcGF0aHMgYXJlIGpvaW5lZCB3aXRoIGEgc2xhc2guXG4gKiAtIEpvaW5pbmcgZm9yIGV4YW1wbGUgJ2h0dHA6Ly8nIGFuZCAnd3d3LmV4YW1wbGUuY29tJyBpcyBhbHNvIHN1cHBvcnRlZC5cbiAqL1xuZnVuY3Rpb24gam9pbihhUm9vdCwgYVBhdGgpIHtcbiAgaWYgKGFSb290ID09PSBcIlwiKSB7XG4gICAgYVJvb3QgPSBcIi5cIjtcbiAgfVxuICBpZiAoYVBhdGggPT09IFwiXCIpIHtcbiAgICBhUGF0aCA9IFwiLlwiO1xuICB9XG4gIHZhciBhUGF0aFVybCA9IHVybFBhcnNlKGFQYXRoKTtcbiAgdmFyIGFSb290VXJsID0gdXJsUGFyc2UoYVJvb3QpO1xuICBpZiAoYVJvb3RVcmwpIHtcbiAgICBhUm9vdCA9IGFSb290VXJsLnBhdGggfHwgJy8nO1xuICB9XG5cbiAgLy8gYGpvaW4oZm9vLCAnLy93d3cuZXhhbXBsZS5vcmcnKWBcbiAgaWYgKGFQYXRoVXJsICYmICFhUGF0aFVybC5zY2hlbWUpIHtcbiAgICBpZiAoYVJvb3RVcmwpIHtcbiAgICAgIGFQYXRoVXJsLnNjaGVtZSA9IGFSb290VXJsLnNjaGVtZTtcbiAgICB9XG4gICAgcmV0dXJuIHVybEdlbmVyYXRlKGFQYXRoVXJsKTtcbiAgfVxuXG4gIGlmIChhUGF0aFVybCB8fCBhUGF0aC5tYXRjaChkYXRhVXJsUmVnZXhwKSkge1xuICAgIHJldHVybiBhUGF0aDtcbiAgfVxuXG4gIC8vIGBqb2luKCdodHRwOi8vJywgJ3d3dy5leGFtcGxlLmNvbScpYFxuICBpZiAoYVJvb3RVcmwgJiYgIWFSb290VXJsLmhvc3QgJiYgIWFSb290VXJsLnBhdGgpIHtcbiAgICBhUm9vdFVybC5ob3N0ID0gYVBhdGg7XG4gICAgcmV0dXJuIHVybEdlbmVyYXRlKGFSb290VXJsKTtcbiAgfVxuXG4gIHZhciBqb2luZWQgPSBhUGF0aC5jaGFyQXQoMCkgPT09ICcvJ1xuICAgID8gYVBhdGhcbiAgICA6IG5vcm1hbGl6ZShhUm9vdC5yZXBsYWNlKC9cXC8rJC8sICcnKSArICcvJyArIGFQYXRoKTtcblxuICBpZiAoYVJvb3RVcmwpIHtcbiAgICBhUm9vdFVybC5wYXRoID0gam9pbmVkO1xuICAgIHJldHVybiB1cmxHZW5lcmF0ZShhUm9vdFVybCk7XG4gIH1cbiAgcmV0dXJuIGpvaW5lZDtcbn1cbmV4cG9ydHMuam9pbiA9IGpvaW47XG5cbmV4cG9ydHMuaXNBYnNvbHV0ZSA9IGZ1bmN0aW9uIChhUGF0aCkge1xuICByZXR1cm4gYVBhdGguY2hhckF0KDApID09PSAnLycgfHwgISFhUGF0aC5tYXRjaCh1cmxSZWdleHApO1xufTtcblxuLyoqXG4gKiBNYWtlIGEgcGF0aCByZWxhdGl2ZSB0byBhIFVSTCBvciBhbm90aGVyIHBhdGguXG4gKlxuICogQHBhcmFtIGFSb290IFRoZSByb290IHBhdGggb3IgVVJMLlxuICogQHBhcmFtIGFQYXRoIFRoZSBwYXRoIG9yIFVSTCB0byBiZSBtYWRlIHJlbGF0aXZlIHRvIGFSb290LlxuICovXG5mdW5jdGlvbiByZWxhdGl2ZShhUm9vdCwgYVBhdGgpIHtcbiAgaWYgKGFSb290ID09PSBcIlwiKSB7XG4gICAgYVJvb3QgPSBcIi5cIjtcbiAgfVxuXG4gIGFSb290ID0gYVJvb3QucmVwbGFjZSgvXFwvJC8sICcnKTtcblxuICAvLyBJdCBpcyBwb3NzaWJsZSBmb3IgdGhlIHBhdGggdG8gYmUgYWJvdmUgdGhlIHJvb3QuIEluIHRoaXMgY2FzZSwgc2ltcGx5XG4gIC8vIGNoZWNraW5nIHdoZXRoZXIgdGhlIHJvb3QgaXMgYSBwcmVmaXggb2YgdGhlIHBhdGggd29uJ3Qgd29yay4gSW5zdGVhZCwgd2VcbiAgLy8gbmVlZCB0byByZW1vdmUgY29tcG9uZW50cyBmcm9tIHRoZSByb290IG9uZSBieSBvbmUsIHVudGlsIGVpdGhlciB3ZSBmaW5kXG4gIC8vIGEgcHJlZml4IHRoYXQgZml0cywgb3Igd2UgcnVuIG91dCBvZiBjb21wb25lbnRzIHRvIHJlbW92ZS5cbiAgdmFyIGxldmVsID0gMDtcbiAgd2hpbGUgKGFQYXRoLmluZGV4T2YoYVJvb3QgKyAnLycpICE9PSAwKSB7XG4gICAgdmFyIGluZGV4ID0gYVJvb3QubGFzdEluZGV4T2YoXCIvXCIpO1xuICAgIGlmIChpbmRleCA8IDApIHtcbiAgICAgIHJldHVybiBhUGF0aDtcbiAgICB9XG5cbiAgICAvLyBJZiB0aGUgb25seSBwYXJ0IG9mIHRoZSByb290IHRoYXQgaXMgbGVmdCBpcyB0aGUgc2NoZW1lIChpLmUuIGh0dHA6Ly8sXG4gICAgLy8gZmlsZTovLy8sIGV0Yy4pLCBvbmUgb3IgbW9yZSBzbGFzaGVzICgvKSwgb3Igc2ltcGx5IG5vdGhpbmcgYXQgYWxsLCB3ZVxuICAgIC8vIGhhdmUgZXhoYXVzdGVkIGFsbCBjb21wb25lbnRzLCBzbyB0aGUgcGF0aCBpcyBub3QgcmVsYXRpdmUgdG8gdGhlIHJvb3QuXG4gICAgYVJvb3QgPSBhUm9vdC5zbGljZSgwLCBpbmRleCk7XG4gICAgaWYgKGFSb290Lm1hdGNoKC9eKFteXFwvXSs6XFwvKT9cXC8qJC8pKSB7XG4gICAgICByZXR1cm4gYVBhdGg7XG4gICAgfVxuXG4gICAgKytsZXZlbDtcbiAgfVxuXG4gIC8vIE1ha2Ugc3VyZSB3ZSBhZGQgYSBcIi4uL1wiIGZvciBlYWNoIGNvbXBvbmVudCB3ZSByZW1vdmVkIGZyb20gdGhlIHJvb3QuXG4gIHJldHVybiBBcnJheShsZXZlbCArIDEpLmpvaW4oXCIuLi9cIikgKyBhUGF0aC5zdWJzdHIoYVJvb3QubGVuZ3RoICsgMSk7XG59XG5leHBvcnRzLnJlbGF0aXZlID0gcmVsYXRpdmU7XG5cbnZhciBzdXBwb3J0c051bGxQcm90byA9IChmdW5jdGlvbiAoKSB7XG4gIHZhciBvYmogPSBPYmplY3QuY3JlYXRlKG51bGwpO1xuICByZXR1cm4gISgnX19wcm90b19fJyBpbiBvYmopO1xufSgpKTtcblxuZnVuY3Rpb24gaWRlbnRpdHkgKHMpIHtcbiAgcmV0dXJuIHM7XG59XG5cbi8qKlxuICogQmVjYXVzZSBiZWhhdmlvciBnb2VzIHdhY2t5IHdoZW4geW91IHNldCBgX19wcm90b19fYCBvbiBvYmplY3RzLCB3ZVxuICogaGF2ZSB0byBwcmVmaXggYWxsIHRoZSBzdHJpbmdzIGluIG91ciBzZXQgd2l0aCBhbiBhcmJpdHJhcnkgY2hhcmFjdGVyLlxuICpcbiAqIFNlZSBodHRwczovL2dpdGh1Yi5jb20vbW96aWxsYS9zb3VyY2UtbWFwL3B1bGwvMzEgYW5kXG4gKiBodHRwczovL2dpdGh1Yi5jb20vbW96aWxsYS9zb3VyY2UtbWFwL2lzc3Vlcy8zMFxuICpcbiAqIEBwYXJhbSBTdHJpbmcgYVN0clxuICovXG5mdW5jdGlvbiB0b1NldFN0cmluZyhhU3RyKSB7XG4gIGlmIChpc1Byb3RvU3RyaW5nKGFTdHIpKSB7XG4gICAgcmV0dXJuICckJyArIGFTdHI7XG4gIH1cblxuICByZXR1cm4gYVN0cjtcbn1cbmV4cG9ydHMudG9TZXRTdHJpbmcgPSBzdXBwb3J0c051bGxQcm90byA/IGlkZW50aXR5IDogdG9TZXRTdHJpbmc7XG5cbmZ1bmN0aW9uIGZyb21TZXRTdHJpbmcoYVN0cikge1xuICBpZiAoaXNQcm90b1N0cmluZyhhU3RyKSkge1xuICAgIHJldHVybiBhU3RyLnNsaWNlKDEpO1xuICB9XG5cbiAgcmV0dXJuIGFTdHI7XG59XG5leHBvcnRzLmZyb21TZXRTdHJpbmcgPSBzdXBwb3J0c051bGxQcm90byA/IGlkZW50aXR5IDogZnJvbVNldFN0cmluZztcblxuZnVuY3Rpb24gaXNQcm90b1N0cmluZyhzKSB7XG4gIGlmICghcykge1xuICAgIHJldHVybiBmYWxzZTtcbiAgfVxuXG4gIHZhciBsZW5ndGggPSBzLmxlbmd0aDtcblxuICBpZiAobGVuZ3RoIDwgOSAvKiBcIl9fcHJvdG9fX1wiLmxlbmd0aCAqLykge1xuICAgIHJldHVybiBmYWxzZTtcbiAgfVxuXG4gIGlmIChzLmNoYXJDb2RlQXQobGVuZ3RoIC0gMSkgIT09IDk1ICAvKiAnXycgKi8gfHxcbiAgICAgIHMuY2hhckNvZGVBdChsZW5ndGggLSAyKSAhPT0gOTUgIC8qICdfJyAqLyB8fFxuICAgICAgcy5jaGFyQ29kZUF0KGxlbmd0aCAtIDMpICE9PSAxMTEgLyogJ28nICovIHx8XG4gICAgICBzLmNoYXJDb2RlQXQobGVuZ3RoIC0gNCkgIT09IDExNiAvKiAndCcgKi8gfHxcbiAgICAgIHMuY2hhckNvZGVBdChsZW5ndGggLSA1KSAhPT0gMTExIC8qICdvJyAqLyB8fFxuICAgICAgcy5jaGFyQ29kZUF0KGxlbmd0aCAtIDYpICE9PSAxMTQgLyogJ3InICovIHx8XG4gICAgICBzLmNoYXJDb2RlQXQobGVuZ3RoIC0gNykgIT09IDExMiAvKiAncCcgKi8gfHxcbiAgICAgIHMuY2hhckNvZGVBdChsZW5ndGggLSA4KSAhPT0gOTUgIC8qICdfJyAqLyB8fFxuICAgICAgcy5jaGFyQ29kZUF0KGxlbmd0aCAtIDkpICE9PSA5NSAgLyogJ18nICovKSB7XG4gICAgcmV0dXJuIGZhbHNlO1xuICB9XG5cbiAgZm9yICh2YXIgaSA9IGxlbmd0aCAtIDEwOyBpID49IDA7IGktLSkge1xuICAgIGlmIChzLmNoYXJDb2RlQXQoaSkgIT09IDM2IC8qICckJyAqLykge1xuICAgICAgcmV0dXJuIGZhbHNlO1xuICAgIH1cbiAgfVxuXG4gIHJldHVybiB0cnVlO1xufVxuXG4vKipcbiAqIENvbXBhcmF0b3IgYmV0d2VlbiB0d28gbWFwcGluZ3Mgd2hlcmUgdGhlIG9yaWdpbmFsIHBvc2l0aW9ucyBhcmUgY29tcGFyZWQuXG4gKlxuICogT3B0aW9uYWxseSBwYXNzIGluIGB0cnVlYCBhcyBgb25seUNvbXBhcmVHZW5lcmF0ZWRgIHRvIGNvbnNpZGVyIHR3b1xuICogbWFwcGluZ3Mgd2l0aCB0aGUgc2FtZSBvcmlnaW5hbCBzb3VyY2UvbGluZS9jb2x1bW4sIGJ1dCBkaWZmZXJlbnQgZ2VuZXJhdGVkXG4gKiBsaW5lIGFuZCBjb2x1bW4gdGhlIHNhbWUuIFVzZWZ1bCB3aGVuIHNlYXJjaGluZyBmb3IgYSBtYXBwaW5nIHdpdGggYVxuICogc3R1YmJlZCBvdXQgbWFwcGluZy5cbiAqL1xuZnVuY3Rpb24gY29tcGFyZUJ5T3JpZ2luYWxQb3NpdGlvbnMobWFwcGluZ0EsIG1hcHBpbmdCLCBvbmx5Q29tcGFyZU9yaWdpbmFsKSB7XG4gIHZhciBjbXAgPSBtYXBwaW5nQS5zb3VyY2UgLSBtYXBwaW5nQi5zb3VyY2U7XG4gIGlmIChjbXAgIT09IDApIHtcbiAgICByZXR1cm4gY21wO1xuICB9XG5cbiAgY21wID0gbWFwcGluZ0Eub3JpZ2luYWxMaW5lIC0gbWFwcGluZ0Iub3JpZ2luYWxMaW5lO1xuICBpZiAoY21wICE9PSAwKSB7XG4gICAgcmV0dXJuIGNtcDtcbiAgfVxuXG4gIGNtcCA9IG1hcHBpbmdBLm9yaWdpbmFsQ29sdW1uIC0gbWFwcGluZ0Iub3JpZ2luYWxDb2x1bW47XG4gIGlmIChjbXAgIT09IDAgfHwgb25seUNvbXBhcmVPcmlnaW5hbCkge1xuICAgIHJldHVybiBjbXA7XG4gIH1cblxuICBjbXAgPSBtYXBwaW5nQS5nZW5lcmF0ZWRDb2x1bW4gLSBtYXBwaW5nQi5nZW5lcmF0ZWRDb2x1bW47XG4gIGlmIChjbXAgIT09IDApIHtcbiAgICByZXR1cm4gY21wO1xuICB9XG5cbiAgY21wID0gbWFwcGluZ0EuZ2VuZXJhdGVkTGluZSAtIG1hcHBpbmdCLmdlbmVyYXRlZExpbmU7XG4gIGlmIChjbXAgIT09IDApIHtcbiAgICByZXR1cm4gY21wO1xuICB9XG5cbiAgcmV0dXJuIG1hcHBpbmdBLm5hbWUgLSBtYXBwaW5nQi5uYW1lO1xufVxuZXhwb3J0cy5jb21wYXJlQnlPcmlnaW5hbFBvc2l0aW9ucyA9IGNvbXBhcmVCeU9yaWdpbmFsUG9zaXRpb25zO1xuXG4vKipcbiAqIENvbXBhcmF0b3IgYmV0d2VlbiB0d28gbWFwcGluZ3Mgd2l0aCBkZWZsYXRlZCBzb3VyY2UgYW5kIG5hbWUgaW5kaWNlcyB3aGVyZVxuICogdGhlIGdlbmVyYXRlZCBwb3NpdGlvbnMgYXJlIGNvbXBhcmVkLlxuICpcbiAqIE9wdGlvbmFsbHkgcGFzcyBpbiBgdHJ1ZWAgYXMgYG9ubHlDb21wYXJlR2VuZXJhdGVkYCB0byBjb25zaWRlciB0d29cbiAqIG1hcHBpbmdzIHdpdGggdGhlIHNhbWUgZ2VuZXJhdGVkIGxpbmUgYW5kIGNvbHVtbiwgYnV0IGRpZmZlcmVudFxuICogc291cmNlL25hbWUvb3JpZ2luYWwgbGluZSBhbmQgY29sdW1uIHRoZSBzYW1lLiBVc2VmdWwgd2hlbiBzZWFyY2hpbmcgZm9yIGFcbiAqIG1hcHBpbmcgd2l0aCBhIHN0dWJiZWQgb3V0IG1hcHBpbmcuXG4gKi9cbmZ1bmN0aW9uIGNvbXBhcmVCeUdlbmVyYXRlZFBvc2l0aW9uc0RlZmxhdGVkKG1hcHBpbmdBLCBtYXBwaW5nQiwgb25seUNvbXBhcmVHZW5lcmF0ZWQpIHtcbiAgdmFyIGNtcCA9IG1hcHBpbmdBLmdlbmVyYXRlZExpbmUgLSBtYXBwaW5nQi5nZW5lcmF0ZWRMaW5lO1xuICBpZiAoY21wICE9PSAwKSB7XG4gICAgcmV0dXJuIGNtcDtcbiAgfVxuXG4gIGNtcCA9IG1hcHBpbmdBLmdlbmVyYXRlZENvbHVtbiAtIG1hcHBpbmdCLmdlbmVyYXRlZENvbHVtbjtcbiAgaWYgKGNtcCAhPT0gMCB8fCBvbmx5Q29tcGFyZUdlbmVyYXRlZCkge1xuICAgIHJldHVybiBjbXA7XG4gIH1cblxuICBjbXAgPSBtYXBwaW5nQS5zb3VyY2UgLSBtYXBwaW5nQi5zb3VyY2U7XG4gIGlmIChjbXAgIT09IDApIHtcbiAgICByZXR1cm4gY21wO1xuICB9XG5cbiAgY21wID0gbWFwcGluZ0Eub3JpZ2luYWxMaW5lIC0gbWFwcGluZ0Iub3JpZ2luYWxMaW5lO1xuICBpZiAoY21wICE9PSAwKSB7XG4gICAgcmV0dXJuIGNtcDtcbiAgfVxuXG4gIGNtcCA9IG1hcHBpbmdBLm9yaWdpbmFsQ29sdW1uIC0gbWFwcGluZ0Iub3JpZ2luYWxDb2x1bW47XG4gIGlmIChjbXAgIT09IDApIHtcbiAgICByZXR1cm4gY21wO1xuICB9XG5cbiAgcmV0dXJuIG1hcHBpbmdBLm5hbWUgLSBtYXBwaW5nQi5uYW1lO1xufVxuZXhwb3J0cy5jb21wYXJlQnlHZW5lcmF0ZWRQb3NpdGlvbnNEZWZsYXRlZCA9IGNvbXBhcmVCeUdlbmVyYXRlZFBvc2l0aW9uc0RlZmxhdGVkO1xuXG5mdW5jdGlvbiBzdHJjbXAoYVN0cjEsIGFTdHIyKSB7XG4gIGlmIChhU3RyMSA9PT0gYVN0cjIpIHtcbiAgICByZXR1cm4gMDtcbiAgfVxuXG4gIGlmIChhU3RyMSA+IGFTdHIyKSB7XG4gICAgcmV0dXJuIDE7XG4gIH1cblxuICByZXR1cm4gLTE7XG59XG5cbi8qKlxuICogQ29tcGFyYXRvciBiZXR3ZWVuIHR3byBtYXBwaW5ncyB3aXRoIGluZmxhdGVkIHNvdXJjZSBhbmQgbmFtZSBzdHJpbmdzIHdoZXJlXG4gKiB0aGUgZ2VuZXJhdGVkIHBvc2l0aW9ucyBhcmUgY29tcGFyZWQuXG4gKi9cbmZ1bmN0aW9uIGNvbXBhcmVCeUdlbmVyYXRlZFBvc2l0aW9uc0luZmxhdGVkKG1hcHBpbmdBLCBtYXBwaW5nQikge1xuICB2YXIgY21wID0gbWFwcGluZ0EuZ2VuZXJhdGVkTGluZSAtIG1hcHBpbmdCLmdlbmVyYXRlZExpbmU7XG4gIGlmIChjbXAgIT09IDApIHtcbiAgICByZXR1cm4gY21wO1xuICB9XG5cbiAgY21wID0gbWFwcGluZ0EuZ2VuZXJhdGVkQ29sdW1uIC0gbWFwcGluZ0IuZ2VuZXJhdGVkQ29sdW1uO1xuICBpZiAoY21wICE9PSAwKSB7XG4gICAgcmV0dXJuIGNtcDtcbiAgfVxuXG4gIGNtcCA9IHN0cmNtcChtYXBwaW5nQS5zb3VyY2UsIG1hcHBpbmdCLnNvdXJjZSk7XG4gIGlmIChjbXAgIT09IDApIHtcbiAgICByZXR1cm4gY21wO1xuICB9XG5cbiAgY21wID0gbWFwcGluZ0Eub3JpZ2luYWxMaW5lIC0gbWFwcGluZ0Iub3JpZ2luYWxMaW5lO1xuICBpZiAoY21wICE9PSAwKSB7XG4gICAgcmV0dXJuIGNtcDtcbiAgfVxuXG4gIGNtcCA9IG1hcHBpbmdBLm9yaWdpbmFsQ29sdW1uIC0gbWFwcGluZ0Iub3JpZ2luYWxDb2x1bW47XG4gIGlmIChjbXAgIT09IDApIHtcbiAgICByZXR1cm4gY21wO1xuICB9XG5cbiAgcmV0dXJuIHN0cmNtcChtYXBwaW5nQS5uYW1lLCBtYXBwaW5nQi5uYW1lKTtcbn1cbmV4cG9ydHMuY29tcGFyZUJ5R2VuZXJhdGVkUG9zaXRpb25zSW5mbGF0ZWQgPSBjb21wYXJlQnlHZW5lcmF0ZWRQb3NpdGlvbnNJbmZsYXRlZDtcblxuXG5cbi8qKioqKioqKioqKioqKioqKlxuICoqIFdFQlBBQ0sgRk9PVEVSXG4gKiogLi9saWIvdXRpbC5qc1xuICoqIG1vZHVsZSBpZCA9IDRcbiAqKiBtb2R1bGUgY2h1bmtzID0gMFxuICoqLyIsIi8qIC0qLSBNb2RlOiBqczsganMtaW5kZW50LWxldmVsOiAyOyAtKi0gKi9cbi8qXG4gKiBDb3B5cmlnaHQgMjAxMSBNb3ppbGxhIEZvdW5kYXRpb24gYW5kIGNvbnRyaWJ1dG9yc1xuICogTGljZW5zZWQgdW5kZXIgdGhlIE5ldyBCU0QgbGljZW5zZS4gU2VlIExJQ0VOU0Ugb3I6XG4gKiBodHRwOi8vb3BlbnNvdXJjZS5vcmcvbGljZW5zZXMvQlNELTMtQ2xhdXNlXG4gKi9cblxudmFyIHV0aWwgPSByZXF1aXJlKCcuL3V0aWwnKTtcbnZhciBoYXMgPSBPYmplY3QucHJvdG90eXBlLmhhc093blByb3BlcnR5O1xuXG4vKipcbiAqIEEgZGF0YSBzdHJ1Y3R1cmUgd2hpY2ggaXMgYSBjb21iaW5hdGlvbiBvZiBhbiBhcnJheSBhbmQgYSBzZXQuIEFkZGluZyBhIG5ld1xuICogbWVtYmVyIGlzIE8oMSksIHRlc3RpbmcgZm9yIG1lbWJlcnNoaXAgaXMgTygxKSwgYW5kIGZpbmRpbmcgdGhlIGluZGV4IG9mIGFuXG4gKiBlbGVtZW50IGlzIE8oMSkuIFJlbW92aW5nIGVsZW1lbnRzIGZyb20gdGhlIHNldCBpcyBub3Qgc3VwcG9ydGVkLiBPbmx5XG4gKiBzdHJpbmdzIGFyZSBzdXBwb3J0ZWQgZm9yIG1lbWJlcnNoaXAuXG4gKi9cbmZ1bmN0aW9uIEFycmF5U2V0KCkge1xuICB0aGlzLl9hcnJheSA9IFtdO1xuICB0aGlzLl9zZXQgPSBPYmplY3QuY3JlYXRlKG51bGwpO1xufVxuXG4vKipcbiAqIFN0YXRpYyBtZXRob2QgZm9yIGNyZWF0aW5nIEFycmF5U2V0IGluc3RhbmNlcyBmcm9tIGFuIGV4aXN0aW5nIGFycmF5LlxuICovXG5BcnJheVNldC5mcm9tQXJyYXkgPSBmdW5jdGlvbiBBcnJheVNldF9mcm9tQXJyYXkoYUFycmF5LCBhQWxsb3dEdXBsaWNhdGVzKSB7XG4gIHZhciBzZXQgPSBuZXcgQXJyYXlTZXQoKTtcbiAgZm9yICh2YXIgaSA9IDAsIGxlbiA9IGFBcnJheS5sZW5ndGg7IGkgPCBsZW47IGkrKykge1xuICAgIHNldC5hZGQoYUFycmF5W2ldLCBhQWxsb3dEdXBsaWNhdGVzKTtcbiAgfVxuICByZXR1cm4gc2V0O1xufTtcblxuLyoqXG4gKiBSZXR1cm4gaG93IG1hbnkgdW5pcXVlIGl0ZW1zIGFyZSBpbiB0aGlzIEFycmF5U2V0LiBJZiBkdXBsaWNhdGVzIGhhdmUgYmVlblxuICogYWRkZWQsIHRoYW4gdGhvc2UgZG8gbm90IGNvdW50IHRvd2FyZHMgdGhlIHNpemUuXG4gKlxuICogQHJldHVybnMgTnVtYmVyXG4gKi9cbkFycmF5U2V0LnByb3RvdHlwZS5zaXplID0gZnVuY3Rpb24gQXJyYXlTZXRfc2l6ZSgpIHtcbiAgcmV0dXJuIE9iamVjdC5nZXRPd25Qcm9wZXJ0eU5hbWVzKHRoaXMuX3NldCkubGVuZ3RoO1xufTtcblxuLyoqXG4gKiBBZGQgdGhlIGdpdmVuIHN0cmluZyB0byB0aGlzIHNldC5cbiAqXG4gKiBAcGFyYW0gU3RyaW5nIGFTdHJcbiAqL1xuQXJyYXlTZXQucHJvdG90eXBlLmFkZCA9IGZ1bmN0aW9uIEFycmF5U2V0X2FkZChhU3RyLCBhQWxsb3dEdXBsaWNhdGVzKSB7XG4gIHZhciBzU3RyID0gdXRpbC50b1NldFN0cmluZyhhU3RyKTtcbiAgdmFyIGlzRHVwbGljYXRlID0gaGFzLmNhbGwodGhpcy5fc2V0LCBzU3RyKTtcbiAgdmFyIGlkeCA9IHRoaXMuX2FycmF5Lmxlbmd0aDtcbiAgaWYgKCFpc0R1cGxpY2F0ZSB8fCBhQWxsb3dEdXBsaWNhdGVzKSB7XG4gICAgdGhpcy5fYXJyYXkucHVzaChhU3RyKTtcbiAgfVxuICBpZiAoIWlzRHVwbGljYXRlKSB7XG4gICAgdGhpcy5fc2V0W3NTdHJdID0gaWR4O1xuICB9XG59O1xuXG4vKipcbiAqIElzIHRoZSBnaXZlbiBzdHJpbmcgYSBtZW1iZXIgb2YgdGhpcyBzZXQ/XG4gKlxuICogQHBhcmFtIFN0cmluZyBhU3RyXG4gKi9cbkFycmF5U2V0LnByb3RvdHlwZS5oYXMgPSBmdW5jdGlvbiBBcnJheVNldF9oYXMoYVN0cikge1xuICB2YXIgc1N0ciA9IHV0aWwudG9TZXRTdHJpbmcoYVN0cik7XG4gIHJldHVybiBoYXMuY2FsbCh0aGlzLl9zZXQsIHNTdHIpO1xufTtcblxuLyoqXG4gKiBXaGF0IGlzIHRoZSBpbmRleCBvZiB0aGUgZ2l2ZW4gc3RyaW5nIGluIHRoZSBhcnJheT9cbiAqXG4gKiBAcGFyYW0gU3RyaW5nIGFTdHJcbiAqL1xuQXJyYXlTZXQucHJvdG90eXBlLmluZGV4T2YgPSBmdW5jdGlvbiBBcnJheVNldF9pbmRleE9mKGFTdHIpIHtcbiAgdmFyIHNTdHIgPSB1dGlsLnRvU2V0U3RyaW5nKGFTdHIpO1xuICBpZiAoaGFzLmNhbGwodGhpcy5fc2V0LCBzU3RyKSkge1xuICAgIHJldHVybiB0aGlzLl9zZXRbc1N0cl07XG4gIH1cbiAgdGhyb3cgbmV3IEVycm9yKCdcIicgKyBhU3RyICsgJ1wiIGlzIG5vdCBpbiB0aGUgc2V0LicpO1xufTtcblxuLyoqXG4gKiBXaGF0IGlzIHRoZSBlbGVtZW50IGF0IHRoZSBnaXZlbiBpbmRleD9cbiAqXG4gKiBAcGFyYW0gTnVtYmVyIGFJZHhcbiAqL1xuQXJyYXlTZXQucHJvdG90eXBlLmF0ID0gZnVuY3Rpb24gQXJyYXlTZXRfYXQoYUlkeCkge1xuICBpZiAoYUlkeCA+PSAwICYmIGFJZHggPCB0aGlzLl9hcnJheS5sZW5ndGgpIHtcbiAgICByZXR1cm4gdGhpcy5fYXJyYXlbYUlkeF07XG4gIH1cbiAgdGhyb3cgbmV3IEVycm9yKCdObyBlbGVtZW50IGluZGV4ZWQgYnkgJyArIGFJZHgpO1xufTtcblxuLyoqXG4gKiBSZXR1cm5zIHRoZSBhcnJheSByZXByZXNlbnRhdGlvbiBvZiB0aGlzIHNldCAod2hpY2ggaGFzIHRoZSBwcm9wZXIgaW5kaWNlc1xuICogaW5kaWNhdGVkIGJ5IGluZGV4T2YpLiBOb3RlIHRoYXQgdGhpcyBpcyBhIGNvcHkgb2YgdGhlIGludGVybmFsIGFycmF5IHVzZWRcbiAqIGZvciBzdG9yaW5nIHRoZSBtZW1iZXJzIHNvIHRoYXQgbm8gb25lIGNhbiBtZXNzIHdpdGggaW50ZXJuYWwgc3RhdGUuXG4gKi9cbkFycmF5U2V0LnByb3RvdHlwZS50b0FycmF5ID0gZnVuY3Rpb24gQXJyYXlTZXRfdG9BcnJheSgpIHtcbiAgcmV0dXJuIHRoaXMuX2FycmF5LnNsaWNlKCk7XG59O1xuXG5leHBvcnRzLkFycmF5U2V0ID0gQXJyYXlTZXQ7XG5cblxuXG4vKioqKioqKioqKioqKioqKipcbiAqKiBXRUJQQUNLIEZPT1RFUlxuICoqIC4vbGliL2FycmF5LXNldC5qc1xuICoqIG1vZHVsZSBpZCA9IDVcbiAqKiBtb2R1bGUgY2h1bmtzID0gMFxuICoqLyIsIi8qIC0qLSBNb2RlOiBqczsganMtaW5kZW50LWxldmVsOiAyOyAtKi0gKi9cbi8qXG4gKiBDb3B5cmlnaHQgMjAxNCBNb3ppbGxhIEZvdW5kYXRpb24gYW5kIGNvbnRyaWJ1dG9yc1xuICogTGljZW5zZWQgdW5kZXIgdGhlIE5ldyBCU0QgbGljZW5zZS4gU2VlIExJQ0VOU0Ugb3I6XG4gKiBodHRwOi8vb3BlbnNvdXJjZS5vcmcvbGljZW5zZXMvQlNELTMtQ2xhdXNlXG4gKi9cblxudmFyIHV0aWwgPSByZXF1aXJlKCcuL3V0aWwnKTtcblxuLyoqXG4gKiBEZXRlcm1pbmUgd2hldGhlciBtYXBwaW5nQiBpcyBhZnRlciBtYXBwaW5nQSB3aXRoIHJlc3BlY3QgdG8gZ2VuZXJhdGVkXG4gKiBwb3NpdGlvbi5cbiAqL1xuZnVuY3Rpb24gZ2VuZXJhdGVkUG9zaXRpb25BZnRlcihtYXBwaW5nQSwgbWFwcGluZ0IpIHtcbiAgLy8gT3B0aW1pemVkIGZvciBtb3N0IGNvbW1vbiBjYXNlXG4gIHZhciBsaW5lQSA9IG1hcHBpbmdBLmdlbmVyYXRlZExpbmU7XG4gIHZhciBsaW5lQiA9IG1hcHBpbmdCLmdlbmVyYXRlZExpbmU7XG4gIHZhciBjb2x1bW5BID0gbWFwcGluZ0EuZ2VuZXJhdGVkQ29sdW1uO1xuICB2YXIgY29sdW1uQiA9IG1hcHBpbmdCLmdlbmVyYXRlZENvbHVtbjtcbiAgcmV0dXJuIGxpbmVCID4gbGluZUEgfHwgbGluZUIgPT0gbGluZUEgJiYgY29sdW1uQiA+PSBjb2x1bW5BIHx8XG4gICAgICAgICB1dGlsLmNvbXBhcmVCeUdlbmVyYXRlZFBvc2l0aW9uc0luZmxhdGVkKG1hcHBpbmdBLCBtYXBwaW5nQikgPD0gMDtcbn1cblxuLyoqXG4gKiBBIGRhdGEgc3RydWN0dXJlIHRvIHByb3ZpZGUgYSBzb3J0ZWQgdmlldyBvZiBhY2N1bXVsYXRlZCBtYXBwaW5ncyBpbiBhXG4gKiBwZXJmb3JtYW5jZSBjb25zY2lvdXMgbWFubmVyLiBJdCB0cmFkZXMgYSBuZWdsaWJhYmxlIG92ZXJoZWFkIGluIGdlbmVyYWxcbiAqIGNhc2UgZm9yIGEgbGFyZ2Ugc3BlZWR1cCBpbiBjYXNlIG9mIG1hcHBpbmdzIGJlaW5nIGFkZGVkIGluIG9yZGVyLlxuICovXG5mdW5jdGlvbiBNYXBwaW5nTGlzdCgpIHtcbiAgdGhpcy5fYXJyYXkgPSBbXTtcbiAgdGhpcy5fc29ydGVkID0gdHJ1ZTtcbiAgLy8gU2VydmVzIGFzIGluZmltdW1cbiAgdGhpcy5fbGFzdCA9IHtnZW5lcmF0ZWRMaW5lOiAtMSwgZ2VuZXJhdGVkQ29sdW1uOiAwfTtcbn1cblxuLyoqXG4gKiBJdGVyYXRlIHRocm91Z2ggaW50ZXJuYWwgaXRlbXMuIFRoaXMgbWV0aG9kIHRha2VzIHRoZSBzYW1lIGFyZ3VtZW50cyB0aGF0XG4gKiBgQXJyYXkucHJvdG90eXBlLmZvckVhY2hgIHRha2VzLlxuICpcbiAqIE5PVEU6IFRoZSBvcmRlciBvZiB0aGUgbWFwcGluZ3MgaXMgTk9UIGd1YXJhbnRlZWQuXG4gKi9cbk1hcHBpbmdMaXN0LnByb3RvdHlwZS51bnNvcnRlZEZvckVhY2ggPVxuICBmdW5jdGlvbiBNYXBwaW5nTGlzdF9mb3JFYWNoKGFDYWxsYmFjaywgYVRoaXNBcmcpIHtcbiAgICB0aGlzLl9hcnJheS5mb3JFYWNoKGFDYWxsYmFjaywgYVRoaXNBcmcpO1xuICB9O1xuXG4vKipcbiAqIEFkZCB0aGUgZ2l2ZW4gc291cmNlIG1hcHBpbmcuXG4gKlxuICogQHBhcmFtIE9iamVjdCBhTWFwcGluZ1xuICovXG5NYXBwaW5nTGlzdC5wcm90b3R5cGUuYWRkID0gZnVuY3Rpb24gTWFwcGluZ0xpc3RfYWRkKGFNYXBwaW5nKSB7XG4gIGlmIChnZW5lcmF0ZWRQb3NpdGlvbkFmdGVyKHRoaXMuX2xhc3QsIGFNYXBwaW5nKSkge1xuICAgIHRoaXMuX2xhc3QgPSBhTWFwcGluZztcbiAgICB0aGlzLl9hcnJheS5wdXNoKGFNYXBwaW5nKTtcbiAgfSBlbHNlIHtcbiAgICB0aGlzLl9zb3J0ZWQgPSBmYWxzZTtcbiAgICB0aGlzLl9hcnJheS5wdXNoKGFNYXBwaW5nKTtcbiAgfVxufTtcblxuLyoqXG4gKiBSZXR1cm5zIHRoZSBmbGF0LCBzb3J0ZWQgYXJyYXkgb2YgbWFwcGluZ3MuIFRoZSBtYXBwaW5ncyBhcmUgc29ydGVkIGJ5XG4gKiBnZW5lcmF0ZWQgcG9zaXRpb24uXG4gKlxuICogV0FSTklORzogVGhpcyBtZXRob2QgcmV0dXJucyBpbnRlcm5hbCBkYXRhIHdpdGhvdXQgY29weWluZywgZm9yXG4gKiBwZXJmb3JtYW5jZS4gVGhlIHJldHVybiB2YWx1ZSBtdXN0IE5PVCBiZSBtdXRhdGVkLCBhbmQgc2hvdWxkIGJlIHRyZWF0ZWQgYXNcbiAqIGFuIGltbXV0YWJsZSBib3Jyb3cuIElmIHlvdSB3YW50IHRvIHRha2Ugb3duZXJzaGlwLCB5b3UgbXVzdCBtYWtlIHlvdXIgb3duXG4gKiBjb3B5LlxuICovXG5NYXBwaW5nTGlzdC5wcm90b3R5cGUudG9BcnJheSA9IGZ1bmN0aW9uIE1hcHBpbmdMaXN0X3RvQXJyYXkoKSB7XG4gIGlmICghdGhpcy5fc29ydGVkKSB7XG4gICAgdGhpcy5fYXJyYXkuc29ydCh1dGlsLmNvbXBhcmVCeUdlbmVyYXRlZFBvc2l0aW9uc0luZmxhdGVkKTtcbiAgICB0aGlzLl9zb3J0ZWQgPSB0cnVlO1xuICB9XG4gIHJldHVybiB0aGlzLl9hcnJheTtcbn07XG5cbmV4cG9ydHMuTWFwcGluZ0xpc3QgPSBNYXBwaW5nTGlzdDtcblxuXG5cbi8qKioqKioqKioqKioqKioqKlxuICoqIFdFQlBBQ0sgRk9PVEVSXG4gKiogLi9saWIvbWFwcGluZy1saXN0LmpzXG4gKiogbW9kdWxlIGlkID0gNlxuICoqIG1vZHVsZSBjaHVua3MgPSAwXG4gKiovIiwiLyogLSotIE1vZGU6IGpzOyBqcy1pbmRlbnQtbGV2ZWw6IDI7IC0qLSAqL1xuLypcbiAqIENvcHlyaWdodCAyMDExIE1vemlsbGEgRm91bmRhdGlvbiBhbmQgY29udHJpYnV0b3JzXG4gKiBMaWNlbnNlZCB1bmRlciB0aGUgTmV3IEJTRCBsaWNlbnNlLiBTZWUgTElDRU5TRSBvcjpcbiAqIGh0dHA6Ly9vcGVuc291cmNlLm9yZy9saWNlbnNlcy9CU0QtMy1DbGF1c2VcbiAqL1xuXG52YXIgdXRpbCA9IHJlcXVpcmUoJy4vdXRpbCcpO1xudmFyIGJpbmFyeVNlYXJjaCA9IHJlcXVpcmUoJy4vYmluYXJ5LXNlYXJjaCcpO1xudmFyIEFycmF5U2V0ID0gcmVxdWlyZSgnLi9hcnJheS1zZXQnKS5BcnJheVNldDtcbnZhciBiYXNlNjRWTFEgPSByZXF1aXJlKCcuL2Jhc2U2NC12bHEnKTtcbnZhciBxdWlja1NvcnQgPSByZXF1aXJlKCcuL3F1aWNrLXNvcnQnKS5xdWlja1NvcnQ7XG5cbmZ1bmN0aW9uIFNvdXJjZU1hcENvbnN1bWVyKGFTb3VyY2VNYXApIHtcbiAgdmFyIHNvdXJjZU1hcCA9IGFTb3VyY2VNYXA7XG4gIGlmICh0eXBlb2YgYVNvdXJjZU1hcCA9PT0gJ3N0cmluZycpIHtcbiAgICBzb3VyY2VNYXAgPSBKU09OLnBhcnNlKGFTb3VyY2VNYXAucmVwbGFjZSgvXlxcKVxcXVxcfScvLCAnJykpO1xuICB9XG5cbiAgcmV0dXJuIHNvdXJjZU1hcC5zZWN0aW9ucyAhPSBudWxsXG4gICAgPyBuZXcgSW5kZXhlZFNvdXJjZU1hcENvbnN1bWVyKHNvdXJjZU1hcClcbiAgICA6IG5ldyBCYXNpY1NvdXJjZU1hcENvbnN1bWVyKHNvdXJjZU1hcCk7XG59XG5cblNvdXJjZU1hcENvbnN1bWVyLmZyb21Tb3VyY2VNYXAgPSBmdW5jdGlvbihhU291cmNlTWFwKSB7XG4gIHJldHVybiBCYXNpY1NvdXJjZU1hcENvbnN1bWVyLmZyb21Tb3VyY2VNYXAoYVNvdXJjZU1hcCk7XG59XG5cbi8qKlxuICogVGhlIHZlcnNpb24gb2YgdGhlIHNvdXJjZSBtYXBwaW5nIHNwZWMgdGhhdCB3ZSBhcmUgY29uc3VtaW5nLlxuICovXG5Tb3VyY2VNYXBDb25zdW1lci5wcm90b3R5cGUuX3ZlcnNpb24gPSAzO1xuXG4vLyBgX19nZW5lcmF0ZWRNYXBwaW5nc2AgYW5kIGBfX29yaWdpbmFsTWFwcGluZ3NgIGFyZSBhcnJheXMgdGhhdCBob2xkIHRoZVxuLy8gcGFyc2VkIG1hcHBpbmcgY29vcmRpbmF0ZXMgZnJvbSB0aGUgc291cmNlIG1hcCdzIFwibWFwcGluZ3NcIiBhdHRyaWJ1dGUuIFRoZXlcbi8vIGFyZSBsYXppbHkgaW5zdGFudGlhdGVkLCBhY2Nlc3NlZCB2aWEgdGhlIGBfZ2VuZXJhdGVkTWFwcGluZ3NgIGFuZFxuLy8gYF9vcmlnaW5hbE1hcHBpbmdzYCBnZXR0ZXJzIHJlc3BlY3RpdmVseSwgYW5kIHdlIG9ubHkgcGFyc2UgdGhlIG1hcHBpbmdzXG4vLyBhbmQgY3JlYXRlIHRoZXNlIGFycmF5cyBvbmNlIHF1ZXJpZWQgZm9yIGEgc291cmNlIGxvY2F0aW9uLiBXZSBqdW1wIHRocm91Z2hcbi8vIHRoZXNlIGhvb3BzIGJlY2F1c2UgdGhlcmUgY2FuIGJlIG1hbnkgdGhvdXNhbmRzIG9mIG1hcHBpbmdzLCBhbmQgcGFyc2luZ1xuLy8gdGhlbSBpcyBleHBlbnNpdmUsIHNvIHdlIG9ubHkgd2FudCB0byBkbyBpdCBpZiB3ZSBtdXN0LlxuLy9cbi8vIEVhY2ggb2JqZWN0IGluIHRoZSBhcnJheXMgaXMgb2YgdGhlIGZvcm06XG4vL1xuLy8gICAgIHtcbi8vICAgICAgIGdlbmVyYXRlZExpbmU6IFRoZSBsaW5lIG51bWJlciBpbiB0aGUgZ2VuZXJhdGVkIGNvZGUsXG4vLyAgICAgICBnZW5lcmF0ZWRDb2x1bW46IFRoZSBjb2x1bW4gbnVtYmVyIGluIHRoZSBnZW5lcmF0ZWQgY29kZSxcbi8vICAgICAgIHNvdXJjZTogVGhlIHBhdGggdG8gdGhlIG9yaWdpbmFsIHNvdXJjZSBmaWxlIHRoYXQgZ2VuZXJhdGVkIHRoaXNcbi8vICAgICAgICAgICAgICAgY2h1bmsgb2YgY29kZSxcbi8vICAgICAgIG9yaWdpbmFsTGluZTogVGhlIGxpbmUgbnVtYmVyIGluIHRoZSBvcmlnaW5hbCBzb3VyY2UgdGhhdFxuLy8gICAgICAgICAgICAgICAgICAgICBjb3JyZXNwb25kcyB0byB0aGlzIGNodW5rIG9mIGdlbmVyYXRlZCBjb2RlLFxuLy8gICAgICAgb3JpZ2luYWxDb2x1bW46IFRoZSBjb2x1bW4gbnVtYmVyIGluIHRoZSBvcmlnaW5hbCBzb3VyY2UgdGhhdFxuLy8gICAgICAgICAgICAgICAgICAgICAgIGNvcnJlc3BvbmRzIHRvIHRoaXMgY2h1bmsgb2YgZ2VuZXJhdGVkIGNvZGUsXG4vLyAgICAgICBuYW1lOiBUaGUgbmFtZSBvZiB0aGUgb3JpZ2luYWwgc3ltYm9sIHdoaWNoIGdlbmVyYXRlZCB0aGlzIGNodW5rIG9mXG4vLyAgICAgICAgICAgICBjb2RlLlxuLy8gICAgIH1cbi8vXG4vLyBBbGwgcHJvcGVydGllcyBleGNlcHQgZm9yIGBnZW5lcmF0ZWRMaW5lYCBhbmQgYGdlbmVyYXRlZENvbHVtbmAgY2FuIGJlXG4vLyBgbnVsbGAuXG4vL1xuLy8gYF9nZW5lcmF0ZWRNYXBwaW5nc2AgaXMgb3JkZXJlZCBieSB0aGUgZ2VuZXJhdGVkIHBvc2l0aW9ucy5cbi8vXG4vLyBgX29yaWdpbmFsTWFwcGluZ3NgIGlzIG9yZGVyZWQgYnkgdGhlIG9yaWdpbmFsIHBvc2l0aW9ucy5cblxuU291cmNlTWFwQ29uc3VtZXIucHJvdG90eXBlLl9fZ2VuZXJhdGVkTWFwcGluZ3MgPSBudWxsO1xuT2JqZWN0LmRlZmluZVByb3BlcnR5KFNvdXJjZU1hcENvbnN1bWVyLnByb3RvdHlwZSwgJ19nZW5lcmF0ZWRNYXBwaW5ncycsIHtcbiAgZ2V0OiBmdW5jdGlvbiAoKSB7XG4gICAgaWYgKCF0aGlzLl9fZ2VuZXJhdGVkTWFwcGluZ3MpIHtcbiAgICAgIHRoaXMuX3BhcnNlTWFwcGluZ3ModGhpcy5fbWFwcGluZ3MsIHRoaXMuc291cmNlUm9vdCk7XG4gICAgfVxuXG4gICAgcmV0dXJuIHRoaXMuX19nZW5lcmF0ZWRNYXBwaW5ncztcbiAgfVxufSk7XG5cblNvdXJjZU1hcENvbnN1bWVyLnByb3RvdHlwZS5fX29yaWdpbmFsTWFwcGluZ3MgPSBudWxsO1xuT2JqZWN0LmRlZmluZVByb3BlcnR5KFNvdXJjZU1hcENvbnN1bWVyLnByb3RvdHlwZSwgJ19vcmlnaW5hbE1hcHBpbmdzJywge1xuICBnZXQ6IGZ1bmN0aW9uICgpIHtcbiAgICBpZiAoIXRoaXMuX19vcmlnaW5hbE1hcHBpbmdzKSB7XG4gICAgICB0aGlzLl9wYXJzZU1hcHBpbmdzKHRoaXMuX21hcHBpbmdzLCB0aGlzLnNvdXJjZVJvb3QpO1xuICAgIH1cblxuICAgIHJldHVybiB0aGlzLl9fb3JpZ2luYWxNYXBwaW5ncztcbiAgfVxufSk7XG5cblNvdXJjZU1hcENvbnN1bWVyLnByb3RvdHlwZS5fY2hhcklzTWFwcGluZ1NlcGFyYXRvciA9XG4gIGZ1bmN0aW9uIFNvdXJjZU1hcENvbnN1bWVyX2NoYXJJc01hcHBpbmdTZXBhcmF0b3IoYVN0ciwgaW5kZXgpIHtcbiAgICB2YXIgYyA9IGFTdHIuY2hhckF0KGluZGV4KTtcbiAgICByZXR1cm4gYyA9PT0gXCI7XCIgfHwgYyA9PT0gXCIsXCI7XG4gIH07XG5cbi8qKlxuICogUGFyc2UgdGhlIG1hcHBpbmdzIGluIGEgc3RyaW5nIGluIHRvIGEgZGF0YSBzdHJ1Y3R1cmUgd2hpY2ggd2UgY2FuIGVhc2lseVxuICogcXVlcnkgKHRoZSBvcmRlcmVkIGFycmF5cyBpbiB0aGUgYHRoaXMuX19nZW5lcmF0ZWRNYXBwaW5nc2AgYW5kXG4gKiBgdGhpcy5fX29yaWdpbmFsTWFwcGluZ3NgIHByb3BlcnRpZXMpLlxuICovXG5Tb3VyY2VNYXBDb25zdW1lci5wcm90b3R5cGUuX3BhcnNlTWFwcGluZ3MgPVxuICBmdW5jdGlvbiBTb3VyY2VNYXBDb25zdW1lcl9wYXJzZU1hcHBpbmdzKGFTdHIsIGFTb3VyY2VSb290KSB7XG4gICAgdGhyb3cgbmV3IEVycm9yKFwiU3ViY2xhc3NlcyBtdXN0IGltcGxlbWVudCBfcGFyc2VNYXBwaW5nc1wiKTtcbiAgfTtcblxuU291cmNlTWFwQ29uc3VtZXIuR0VORVJBVEVEX09SREVSID0gMTtcblNvdXJjZU1hcENvbnN1bWVyLk9SSUdJTkFMX09SREVSID0gMjtcblxuU291cmNlTWFwQ29uc3VtZXIuR1JFQVRFU1RfTE9XRVJfQk9VTkQgPSAxO1xuU291cmNlTWFwQ29uc3VtZXIuTEVBU1RfVVBQRVJfQk9VTkQgPSAyO1xuXG4vKipcbiAqIEl0ZXJhdGUgb3ZlciBlYWNoIG1hcHBpbmcgYmV0d2VlbiBhbiBvcmlnaW5hbCBzb3VyY2UvbGluZS9jb2x1bW4gYW5kIGFcbiAqIGdlbmVyYXRlZCBsaW5lL2NvbHVtbiBpbiB0aGlzIHNvdXJjZSBtYXAuXG4gKlxuICogQHBhcmFtIEZ1bmN0aW9uIGFDYWxsYmFja1xuICogICAgICAgIFRoZSBmdW5jdGlvbiB0aGF0IGlzIGNhbGxlZCB3aXRoIGVhY2ggbWFwcGluZy5cbiAqIEBwYXJhbSBPYmplY3QgYUNvbnRleHRcbiAqICAgICAgICBPcHRpb25hbC4gSWYgc3BlY2lmaWVkLCB0aGlzIG9iamVjdCB3aWxsIGJlIHRoZSB2YWx1ZSBvZiBgdGhpc2AgZXZlcnlcbiAqICAgICAgICB0aW1lIHRoYXQgYGFDYWxsYmFja2AgaXMgY2FsbGVkLlxuICogQHBhcmFtIGFPcmRlclxuICogICAgICAgIEVpdGhlciBgU291cmNlTWFwQ29uc3VtZXIuR0VORVJBVEVEX09SREVSYCBvclxuICogICAgICAgIGBTb3VyY2VNYXBDb25zdW1lci5PUklHSU5BTF9PUkRFUmAuIFNwZWNpZmllcyB3aGV0aGVyIHlvdSB3YW50IHRvXG4gKiAgICAgICAgaXRlcmF0ZSBvdmVyIHRoZSBtYXBwaW5ncyBzb3J0ZWQgYnkgdGhlIGdlbmVyYXRlZCBmaWxlJ3MgbGluZS9jb2x1bW5cbiAqICAgICAgICBvcmRlciBvciB0aGUgb3JpZ2luYWwncyBzb3VyY2UvbGluZS9jb2x1bW4gb3JkZXIsIHJlc3BlY3RpdmVseS4gRGVmYXVsdHMgdG9cbiAqICAgICAgICBgU291cmNlTWFwQ29uc3VtZXIuR0VORVJBVEVEX09SREVSYC5cbiAqL1xuU291cmNlTWFwQ29uc3VtZXIucHJvdG90eXBlLmVhY2hNYXBwaW5nID1cbiAgZnVuY3Rpb24gU291cmNlTWFwQ29uc3VtZXJfZWFjaE1hcHBpbmcoYUNhbGxiYWNrLCBhQ29udGV4dCwgYU9yZGVyKSB7XG4gICAgdmFyIGNvbnRleHQgPSBhQ29udGV4dCB8fCBudWxsO1xuICAgIHZhciBvcmRlciA9IGFPcmRlciB8fCBTb3VyY2VNYXBDb25zdW1lci5HRU5FUkFURURfT1JERVI7XG5cbiAgICB2YXIgbWFwcGluZ3M7XG4gICAgc3dpdGNoIChvcmRlcikge1xuICAgIGNhc2UgU291cmNlTWFwQ29uc3VtZXIuR0VORVJBVEVEX09SREVSOlxuICAgICAgbWFwcGluZ3MgPSB0aGlzLl9nZW5lcmF0ZWRNYXBwaW5ncztcbiAgICAgIGJyZWFrO1xuICAgIGNhc2UgU291cmNlTWFwQ29uc3VtZXIuT1JJR0lOQUxfT1JERVI6XG4gICAgICBtYXBwaW5ncyA9IHRoaXMuX29yaWdpbmFsTWFwcGluZ3M7XG4gICAgICBicmVhaztcbiAgICBkZWZhdWx0OlxuICAgICAgdGhyb3cgbmV3IEVycm9yKFwiVW5rbm93biBvcmRlciBvZiBpdGVyYXRpb24uXCIpO1xuICAgIH1cblxuICAgIHZhciBzb3VyY2VSb290ID0gdGhpcy5zb3VyY2VSb290O1xuICAgIG1hcHBpbmdzLm1hcChmdW5jdGlvbiAobWFwcGluZykge1xuICAgICAgdmFyIHNvdXJjZSA9IG1hcHBpbmcuc291cmNlID09PSBudWxsID8gbnVsbCA6IHRoaXMuX3NvdXJjZXMuYXQobWFwcGluZy5zb3VyY2UpO1xuICAgICAgaWYgKHNvdXJjZSAhPSBudWxsICYmIHNvdXJjZVJvb3QgIT0gbnVsbCkge1xuICAgICAgICBzb3VyY2UgPSB1dGlsLmpvaW4oc291cmNlUm9vdCwgc291cmNlKTtcbiAgICAgIH1cbiAgICAgIHJldHVybiB7XG4gICAgICAgIHNvdXJjZTogc291cmNlLFxuICAgICAgICBnZW5lcmF0ZWRMaW5lOiBtYXBwaW5nLmdlbmVyYXRlZExpbmUsXG4gICAgICAgIGdlbmVyYXRlZENvbHVtbjogbWFwcGluZy5nZW5lcmF0ZWRDb2x1bW4sXG4gICAgICAgIG9yaWdpbmFsTGluZTogbWFwcGluZy5vcmlnaW5hbExpbmUsXG4gICAgICAgIG9yaWdpbmFsQ29sdW1uOiBtYXBwaW5nLm9yaWdpbmFsQ29sdW1uLFxuICAgICAgICBuYW1lOiBtYXBwaW5nLm5hbWUgPT09IG51bGwgPyBudWxsIDogdGhpcy5fbmFtZXMuYXQobWFwcGluZy5uYW1lKVxuICAgICAgfTtcbiAgICB9LCB0aGlzKS5mb3JFYWNoKGFDYWxsYmFjaywgY29udGV4dCk7XG4gIH07XG5cbi8qKlxuICogUmV0dXJucyBhbGwgZ2VuZXJhdGVkIGxpbmUgYW5kIGNvbHVtbiBpbmZvcm1hdGlvbiBmb3IgdGhlIG9yaWdpbmFsIHNvdXJjZSxcbiAqIGxpbmUsIGFuZCBjb2x1bW4gcHJvdmlkZWQuIElmIG5vIGNvbHVtbiBpcyBwcm92aWRlZCwgcmV0dXJucyBhbGwgbWFwcGluZ3NcbiAqIGNvcnJlc3BvbmRpbmcgdG8gYSBlaXRoZXIgdGhlIGxpbmUgd2UgYXJlIHNlYXJjaGluZyBmb3Igb3IgdGhlIG5leHRcbiAqIGNsb3Nlc3QgbGluZSB0aGF0IGhhcyBhbnkgbWFwcGluZ3MuIE90aGVyd2lzZSwgcmV0dXJucyBhbGwgbWFwcGluZ3NcbiAqIGNvcnJlc3BvbmRpbmcgdG8gdGhlIGdpdmVuIGxpbmUgYW5kIGVpdGhlciB0aGUgY29sdW1uIHdlIGFyZSBzZWFyY2hpbmcgZm9yXG4gKiBvciB0aGUgbmV4dCBjbG9zZXN0IGNvbHVtbiB0aGF0IGhhcyBhbnkgb2Zmc2V0cy5cbiAqXG4gKiBUaGUgb25seSBhcmd1bWVudCBpcyBhbiBvYmplY3Qgd2l0aCB0aGUgZm9sbG93aW5nIHByb3BlcnRpZXM6XG4gKlxuICogICAtIHNvdXJjZTogVGhlIGZpbGVuYW1lIG9mIHRoZSBvcmlnaW5hbCBzb3VyY2UuXG4gKiAgIC0gbGluZTogVGhlIGxpbmUgbnVtYmVyIGluIHRoZSBvcmlnaW5hbCBzb3VyY2UuXG4gKiAgIC0gY29sdW1uOiBPcHRpb25hbC4gdGhlIGNvbHVtbiBudW1iZXIgaW4gdGhlIG9yaWdpbmFsIHNvdXJjZS5cbiAqXG4gKiBhbmQgYW4gYXJyYXkgb2Ygb2JqZWN0cyBpcyByZXR1cm5lZCwgZWFjaCB3aXRoIHRoZSBmb2xsb3dpbmcgcHJvcGVydGllczpcbiAqXG4gKiAgIC0gbGluZTogVGhlIGxpbmUgbnVtYmVyIGluIHRoZSBnZW5lcmF0ZWQgc291cmNlLCBvciBudWxsLlxuICogICAtIGNvbHVtbjogVGhlIGNvbHVtbiBudW1iZXIgaW4gdGhlIGdlbmVyYXRlZCBzb3VyY2UsIG9yIG51bGwuXG4gKi9cblNvdXJjZU1hcENvbnN1bWVyLnByb3RvdHlwZS5hbGxHZW5lcmF0ZWRQb3NpdGlvbnNGb3IgPVxuICBmdW5jdGlvbiBTb3VyY2VNYXBDb25zdW1lcl9hbGxHZW5lcmF0ZWRQb3NpdGlvbnNGb3IoYUFyZ3MpIHtcbiAgICB2YXIgbGluZSA9IHV0aWwuZ2V0QXJnKGFBcmdzLCAnbGluZScpO1xuXG4gICAgLy8gV2hlbiB0aGVyZSBpcyBubyBleGFjdCBtYXRjaCwgQmFzaWNTb3VyY2VNYXBDb25zdW1lci5wcm90b3R5cGUuX2ZpbmRNYXBwaW5nXG4gICAgLy8gcmV0dXJucyB0aGUgaW5kZXggb2YgdGhlIGNsb3Nlc3QgbWFwcGluZyBsZXNzIHRoYW4gdGhlIG5lZWRsZS4gQnlcbiAgICAvLyBzZXR0aW5nIG5lZWRsZS5vcmlnaW5hbENvbHVtbiB0byAwLCB3ZSB0aHVzIGZpbmQgdGhlIGxhc3QgbWFwcGluZyBmb3JcbiAgICAvLyB0aGUgZ2l2ZW4gbGluZSwgcHJvdmlkZWQgc3VjaCBhIG1hcHBpbmcgZXhpc3RzLlxuICAgIHZhciBuZWVkbGUgPSB7XG4gICAgICBzb3VyY2U6IHV0aWwuZ2V0QXJnKGFBcmdzLCAnc291cmNlJyksXG4gICAgICBvcmlnaW5hbExpbmU6IGxpbmUsXG4gICAgICBvcmlnaW5hbENvbHVtbjogdXRpbC5nZXRBcmcoYUFyZ3MsICdjb2x1bW4nLCAwKVxuICAgIH07XG5cbiAgICBpZiAodGhpcy5zb3VyY2VSb290ICE9IG51bGwpIHtcbiAgICAgIG5lZWRsZS5zb3VyY2UgPSB1dGlsLnJlbGF0aXZlKHRoaXMuc291cmNlUm9vdCwgbmVlZGxlLnNvdXJjZSk7XG4gICAgfVxuICAgIGlmICghdGhpcy5fc291cmNlcy5oYXMobmVlZGxlLnNvdXJjZSkpIHtcbiAgICAgIHJldHVybiBbXTtcbiAgICB9XG4gICAgbmVlZGxlLnNvdXJjZSA9IHRoaXMuX3NvdXJjZXMuaW5kZXhPZihuZWVkbGUuc291cmNlKTtcblxuICAgIHZhciBtYXBwaW5ncyA9IFtdO1xuXG4gICAgdmFyIGluZGV4ID0gdGhpcy5fZmluZE1hcHBpbmcobmVlZGxlLFxuICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgIHRoaXMuX29yaWdpbmFsTWFwcGluZ3MsXG4gICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgXCJvcmlnaW5hbExpbmVcIixcbiAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICBcIm9yaWdpbmFsQ29sdW1uXCIsXG4gICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgdXRpbC5jb21wYXJlQnlPcmlnaW5hbFBvc2l0aW9ucyxcbiAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICBiaW5hcnlTZWFyY2guTEVBU1RfVVBQRVJfQk9VTkQpO1xuICAgIGlmIChpbmRleCA+PSAwKSB7XG4gICAgICB2YXIgbWFwcGluZyA9IHRoaXMuX29yaWdpbmFsTWFwcGluZ3NbaW5kZXhdO1xuXG4gICAgICBpZiAoYUFyZ3MuY29sdW1uID09PSB1bmRlZmluZWQpIHtcbiAgICAgICAgdmFyIG9yaWdpbmFsTGluZSA9IG1hcHBpbmcub3JpZ2luYWxMaW5lO1xuXG4gICAgICAgIC8vIEl0ZXJhdGUgdW50aWwgZWl0aGVyIHdlIHJ1biBvdXQgb2YgbWFwcGluZ3MsIG9yIHdlIHJ1biBpbnRvXG4gICAgICAgIC8vIGEgbWFwcGluZyBmb3IgYSBkaWZmZXJlbnQgbGluZSB0aGFuIHRoZSBvbmUgd2UgZm91bmQuIFNpbmNlXG4gICAgICAgIC8vIG1hcHBpbmdzIGFyZSBzb3J0ZWQsIHRoaXMgaXMgZ3VhcmFudGVlZCB0byBmaW5kIGFsbCBtYXBwaW5ncyBmb3JcbiAgICAgICAgLy8gdGhlIGxpbmUgd2UgZm91bmQuXG4gICAgICAgIHdoaWxlIChtYXBwaW5nICYmIG1hcHBpbmcub3JpZ2luYWxMaW5lID09PSBvcmlnaW5hbExpbmUpIHtcbiAgICAgICAgICBtYXBwaW5ncy5wdXNoKHtcbiAgICAgICAgICAgIGxpbmU6IHV0aWwuZ2V0QXJnKG1hcHBpbmcsICdnZW5lcmF0ZWRMaW5lJywgbnVsbCksXG4gICAgICAgICAgICBjb2x1bW46IHV0aWwuZ2V0QXJnKG1hcHBpbmcsICdnZW5lcmF0ZWRDb2x1bW4nLCBudWxsKSxcbiAgICAgICAgICAgIGxhc3RDb2x1bW46IHV0aWwuZ2V0QXJnKG1hcHBpbmcsICdsYXN0R2VuZXJhdGVkQ29sdW1uJywgbnVsbClcbiAgICAgICAgICB9KTtcblxuICAgICAgICAgIG1hcHBpbmcgPSB0aGlzLl9vcmlnaW5hbE1hcHBpbmdzWysraW5kZXhdO1xuICAgICAgICB9XG4gICAgICB9IGVsc2Uge1xuICAgICAgICB2YXIgb3JpZ2luYWxDb2x1bW4gPSBtYXBwaW5nLm9yaWdpbmFsQ29sdW1uO1xuXG4gICAgICAgIC8vIEl0ZXJhdGUgdW50aWwgZWl0aGVyIHdlIHJ1biBvdXQgb2YgbWFwcGluZ3MsIG9yIHdlIHJ1biBpbnRvXG4gICAgICAgIC8vIGEgbWFwcGluZyBmb3IgYSBkaWZmZXJlbnQgbGluZSB0aGFuIHRoZSBvbmUgd2Ugd2VyZSBzZWFyY2hpbmcgZm9yLlxuICAgICAgICAvLyBTaW5jZSBtYXBwaW5ncyBhcmUgc29ydGVkLCB0aGlzIGlzIGd1YXJhbnRlZWQgdG8gZmluZCBhbGwgbWFwcGluZ3MgZm9yXG4gICAgICAgIC8vIHRoZSBsaW5lIHdlIGFyZSBzZWFyY2hpbmcgZm9yLlxuICAgICAgICB3aGlsZSAobWFwcGluZyAmJlxuICAgICAgICAgICAgICAgbWFwcGluZy5vcmlnaW5hbExpbmUgPT09IGxpbmUgJiZcbiAgICAgICAgICAgICAgIG1hcHBpbmcub3JpZ2luYWxDb2x1bW4gPT0gb3JpZ2luYWxDb2x1bW4pIHtcbiAgICAgICAgICBtYXBwaW5ncy5wdXNoKHtcbiAgICAgICAgICAgIGxpbmU6IHV0aWwuZ2V0QXJnKG1hcHBpbmcsICdnZW5lcmF0ZWRMaW5lJywgbnVsbCksXG4gICAgICAgICAgICBjb2x1bW46IHV0aWwuZ2V0QXJnKG1hcHBpbmcsICdnZW5lcmF0ZWRDb2x1bW4nLCBudWxsKSxcbiAgICAgICAgICAgIGxhc3RDb2x1bW46IHV0aWwuZ2V0QXJnKG1hcHBpbmcsICdsYXN0R2VuZXJhdGVkQ29sdW1uJywgbnVsbClcbiAgICAgICAgICB9KTtcblxuICAgICAgICAgIG1hcHBpbmcgPSB0aGlzLl9vcmlnaW5hbE1hcHBpbmdzWysraW5kZXhdO1xuICAgICAgICB9XG4gICAgICB9XG4gICAgfVxuXG4gICAgcmV0dXJuIG1hcHBpbmdzO1xuICB9O1xuXG5leHBvcnRzLlNvdXJjZU1hcENvbnN1bWVyID0gU291cmNlTWFwQ29uc3VtZXI7XG5cbi8qKlxuICogQSBCYXNpY1NvdXJjZU1hcENvbnN1bWVyIGluc3RhbmNlIHJlcHJlc2VudHMgYSBwYXJzZWQgc291cmNlIG1hcCB3aGljaCB3ZSBjYW5cbiAqIHF1ZXJ5IGZvciBpbmZvcm1hdGlvbiBhYm91dCB0aGUgb3JpZ2luYWwgZmlsZSBwb3NpdGlvbnMgYnkgZ2l2aW5nIGl0IGEgZmlsZVxuICogcG9zaXRpb24gaW4gdGhlIGdlbmVyYXRlZCBzb3VyY2UuXG4gKlxuICogVGhlIG9ubHkgcGFyYW1ldGVyIGlzIHRoZSByYXcgc291cmNlIG1hcCAoZWl0aGVyIGFzIGEgSlNPTiBzdHJpbmcsIG9yXG4gKiBhbHJlYWR5IHBhcnNlZCB0byBhbiBvYmplY3QpLiBBY2NvcmRpbmcgdG8gdGhlIHNwZWMsIHNvdXJjZSBtYXBzIGhhdmUgdGhlXG4gKiBmb2xsb3dpbmcgYXR0cmlidXRlczpcbiAqXG4gKiAgIC0gdmVyc2lvbjogV2hpY2ggdmVyc2lvbiBvZiB0aGUgc291cmNlIG1hcCBzcGVjIHRoaXMgbWFwIGlzIGZvbGxvd2luZy5cbiAqICAgLSBzb3VyY2VzOiBBbiBhcnJheSBvZiBVUkxzIHRvIHRoZSBvcmlnaW5hbCBzb3VyY2UgZmlsZXMuXG4gKiAgIC0gbmFtZXM6IEFuIGFycmF5IG9mIGlkZW50aWZpZXJzIHdoaWNoIGNhbiBiZSByZWZlcnJlbmNlZCBieSBpbmRpdmlkdWFsIG1hcHBpbmdzLlxuICogICAtIHNvdXJjZVJvb3Q6IE9wdGlvbmFsLiBUaGUgVVJMIHJvb3QgZnJvbSB3aGljaCBhbGwgc291cmNlcyBhcmUgcmVsYXRpdmUuXG4gKiAgIC0gc291cmNlc0NvbnRlbnQ6IE9wdGlvbmFsLiBBbiBhcnJheSBvZiBjb250ZW50cyBvZiB0aGUgb3JpZ2luYWwgc291cmNlIGZpbGVzLlxuICogICAtIG1hcHBpbmdzOiBBIHN0cmluZyBvZiBiYXNlNjQgVkxRcyB3aGljaCBjb250YWluIHRoZSBhY3R1YWwgbWFwcGluZ3MuXG4gKiAgIC0gZmlsZTogT3B0aW9uYWwuIFRoZSBnZW5lcmF0ZWQgZmlsZSB0aGlzIHNvdXJjZSBtYXAgaXMgYXNzb2NpYXRlZCB3aXRoLlxuICpcbiAqIEhlcmUgaXMgYW4gZXhhbXBsZSBzb3VyY2UgbWFwLCB0YWtlbiBmcm9tIHRoZSBzb3VyY2UgbWFwIHNwZWNbMF06XG4gKlxuICogICAgIHtcbiAqICAgICAgIHZlcnNpb24gOiAzLFxuICogICAgICAgZmlsZTogXCJvdXQuanNcIixcbiAqICAgICAgIHNvdXJjZVJvb3QgOiBcIlwiLFxuICogICAgICAgc291cmNlczogW1wiZm9vLmpzXCIsIFwiYmFyLmpzXCJdLFxuICogICAgICAgbmFtZXM6IFtcInNyY1wiLCBcIm1hcHNcIiwgXCJhcmVcIiwgXCJmdW5cIl0sXG4gKiAgICAgICBtYXBwaW5nczogXCJBQSxBQjs7QUJDREU7XCJcbiAqICAgICB9XG4gKlxuICogWzBdOiBodHRwczovL2RvY3MuZ29vZ2xlLmNvbS9kb2N1bWVudC9kLzFVMVJHQWVoUXdSeXBVVG92RjFLUmxwaU9GemUwYi1fMmdjNmZBSDBLWTBrL2VkaXQ/cGxpPTEjXG4gKi9cbmZ1bmN0aW9uIEJhc2ljU291cmNlTWFwQ29uc3VtZXIoYVNvdXJjZU1hcCkge1xuICB2YXIgc291cmNlTWFwID0gYVNvdXJjZU1hcDtcbiAgaWYgKHR5cGVvZiBhU291cmNlTWFwID09PSAnc3RyaW5nJykge1xuICAgIHNvdXJjZU1hcCA9IEpTT04ucGFyc2UoYVNvdXJjZU1hcC5yZXBsYWNlKC9eXFwpXFxdXFx9Jy8sICcnKSk7XG4gIH1cblxuICB2YXIgdmVyc2lvbiA9IHV0aWwuZ2V0QXJnKHNvdXJjZU1hcCwgJ3ZlcnNpb24nKTtcbiAgdmFyIHNvdXJjZXMgPSB1dGlsLmdldEFyZyhzb3VyY2VNYXAsICdzb3VyY2VzJyk7XG4gIC8vIFNhc3MgMy4zIGxlYXZlcyBvdXQgdGhlICduYW1lcycgYXJyYXksIHNvIHdlIGRldmlhdGUgZnJvbSB0aGUgc3BlYyAod2hpY2hcbiAgLy8gcmVxdWlyZXMgdGhlIGFycmF5KSB0byBwbGF5IG5pY2UgaGVyZS5cbiAgdmFyIG5hbWVzID0gdXRpbC5nZXRBcmcoc291cmNlTWFwLCAnbmFtZXMnLCBbXSk7XG4gIHZhciBzb3VyY2VSb290ID0gdXRpbC5nZXRBcmcoc291cmNlTWFwLCAnc291cmNlUm9vdCcsIG51bGwpO1xuICB2YXIgc291cmNlc0NvbnRlbnQgPSB1dGlsLmdldEFyZyhzb3VyY2VNYXAsICdzb3VyY2VzQ29udGVudCcsIG51bGwpO1xuICB2YXIgbWFwcGluZ3MgPSB1dGlsLmdldEFyZyhzb3VyY2VNYXAsICdtYXBwaW5ncycpO1xuICB2YXIgZmlsZSA9IHV0aWwuZ2V0QXJnKHNvdXJjZU1hcCwgJ2ZpbGUnLCBudWxsKTtcblxuICAvLyBPbmNlIGFnYWluLCBTYXNzIGRldmlhdGVzIGZyb20gdGhlIHNwZWMgYW5kIHN1cHBsaWVzIHRoZSB2ZXJzaW9uIGFzIGFcbiAgLy8gc3RyaW5nIHJhdGhlciB0aGFuIGEgbnVtYmVyLCBzbyB3ZSB1c2UgbG9vc2UgZXF1YWxpdHkgY2hlY2tpbmcgaGVyZS5cbiAgaWYgKHZlcnNpb24gIT0gdGhpcy5fdmVyc2lvbikge1xuICAgIHRocm93IG5ldyBFcnJvcignVW5zdXBwb3J0ZWQgdmVyc2lvbjogJyArIHZlcnNpb24pO1xuICB9XG5cbiAgc291cmNlcyA9IHNvdXJjZXNcbiAgICAubWFwKFN0cmluZylcbiAgICAvLyBTb21lIHNvdXJjZSBtYXBzIHByb2R1Y2UgcmVsYXRpdmUgc291cmNlIHBhdGhzIGxpa2UgXCIuL2Zvby5qc1wiIGluc3RlYWQgb2ZcbiAgICAvLyBcImZvby5qc1wiLiAgTm9ybWFsaXplIHRoZXNlIGZpcnN0IHNvIHRoYXQgZnV0dXJlIGNvbXBhcmlzb25zIHdpbGwgc3VjY2VlZC5cbiAgICAvLyBTZWUgYnVnemlsLmxhLzEwOTA3NjguXG4gICAgLm1hcCh1dGlsLm5vcm1hbGl6ZSlcbiAgICAvLyBBbHdheXMgZW5zdXJlIHRoYXQgYWJzb2x1dGUgc291cmNlcyBhcmUgaW50ZXJuYWxseSBzdG9yZWQgcmVsYXRpdmUgdG9cbiAgICAvLyB0aGUgc291cmNlIHJvb3QsIGlmIHRoZSBzb3VyY2Ugcm9vdCBpcyBhYnNvbHV0ZS4gTm90IGRvaW5nIHRoaXMgd291bGRcbiAgICAvLyBiZSBwYXJ0aWN1bGFybHkgcHJvYmxlbWF0aWMgd2hlbiB0aGUgc291cmNlIHJvb3QgaXMgYSBwcmVmaXggb2YgdGhlXG4gICAgLy8gc291cmNlICh2YWxpZCwgYnV0IHdoeT8/KS4gU2VlIGdpdGh1YiBpc3N1ZSAjMTk5IGFuZCBidWd6aWwubGEvMTE4ODk4Mi5cbiAgICAubWFwKGZ1bmN0aW9uIChzb3VyY2UpIHtcbiAgICAgIHJldHVybiBzb3VyY2VSb290ICYmIHV0aWwuaXNBYnNvbHV0ZShzb3VyY2VSb290KSAmJiB1dGlsLmlzQWJzb2x1dGUoc291cmNlKVxuICAgICAgICA/IHV0aWwucmVsYXRpdmUoc291cmNlUm9vdCwgc291cmNlKVxuICAgICAgICA6IHNvdXJjZTtcbiAgICB9KTtcblxuICAvLyBQYXNzIGB0cnVlYCBiZWxvdyB0byBhbGxvdyBkdXBsaWNhdGUgbmFtZXMgYW5kIHNvdXJjZXMuIFdoaWxlIHNvdXJjZSBtYXBzXG4gIC8vIGFyZSBpbnRlbmRlZCB0byBiZSBjb21wcmVzc2VkIGFuZCBkZWR1cGxpY2F0ZWQsIHRoZSBUeXBlU2NyaXB0IGNvbXBpbGVyXG4gIC8vIHNvbWV0aW1lcyBnZW5lcmF0ZXMgc291cmNlIG1hcHMgd2l0aCBkdXBsaWNhdGVzIGluIHRoZW0uIFNlZSBHaXRodWIgaXNzdWVcbiAgLy8gIzcyIGFuZCBidWd6aWwubGEvODg5NDkyLlxuICB0aGlzLl9uYW1lcyA9IEFycmF5U2V0LmZyb21BcnJheShuYW1lcy5tYXAoU3RyaW5nKSwgdHJ1ZSk7XG4gIHRoaXMuX3NvdXJjZXMgPSBBcnJheVNldC5mcm9tQXJyYXkoc291cmNlcywgdHJ1ZSk7XG5cbiAgdGhpcy5zb3VyY2VSb290ID0gc291cmNlUm9vdDtcbiAgdGhpcy5zb3VyY2VzQ29udGVudCA9IHNvdXJjZXNDb250ZW50O1xuICB0aGlzLl9tYXBwaW5ncyA9IG1hcHBpbmdzO1xuICB0aGlzLmZpbGUgPSBmaWxlO1xufVxuXG5CYXNpY1NvdXJjZU1hcENvbnN1bWVyLnByb3RvdHlwZSA9IE9iamVjdC5jcmVhdGUoU291cmNlTWFwQ29uc3VtZXIucHJvdG90eXBlKTtcbkJhc2ljU291cmNlTWFwQ29uc3VtZXIucHJvdG90eXBlLmNvbnN1bWVyID0gU291cmNlTWFwQ29uc3VtZXI7XG5cbi8qKlxuICogQ3JlYXRlIGEgQmFzaWNTb3VyY2VNYXBDb25zdW1lciBmcm9tIGEgU291cmNlTWFwR2VuZXJhdG9yLlxuICpcbiAqIEBwYXJhbSBTb3VyY2VNYXBHZW5lcmF0b3IgYVNvdXJjZU1hcFxuICogICAgICAgIFRoZSBzb3VyY2UgbWFwIHRoYXQgd2lsbCBiZSBjb25zdW1lZC5cbiAqIEByZXR1cm5zIEJhc2ljU291cmNlTWFwQ29uc3VtZXJcbiAqL1xuQmFzaWNTb3VyY2VNYXBDb25zdW1lci5mcm9tU291cmNlTWFwID1cbiAgZnVuY3Rpb24gU291cmNlTWFwQ29uc3VtZXJfZnJvbVNvdXJjZU1hcChhU291cmNlTWFwKSB7XG4gICAgdmFyIHNtYyA9IE9iamVjdC5jcmVhdGUoQmFzaWNTb3VyY2VNYXBDb25zdW1lci5wcm90b3R5cGUpO1xuXG4gICAgdmFyIG5hbWVzID0gc21jLl9uYW1lcyA9IEFycmF5U2V0LmZyb21BcnJheShhU291cmNlTWFwLl9uYW1lcy50b0FycmF5KCksIHRydWUpO1xuICAgIHZhciBzb3VyY2VzID0gc21jLl9zb3VyY2VzID0gQXJyYXlTZXQuZnJvbUFycmF5KGFTb3VyY2VNYXAuX3NvdXJjZXMudG9BcnJheSgpLCB0cnVlKTtcbiAgICBzbWMuc291cmNlUm9vdCA9IGFTb3VyY2VNYXAuX3NvdXJjZVJvb3Q7XG4gICAgc21jLnNvdXJjZXNDb250ZW50ID0gYVNvdXJjZU1hcC5fZ2VuZXJhdGVTb3VyY2VzQ29udGVudChzbWMuX3NvdXJjZXMudG9BcnJheSgpLFxuICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgc21jLnNvdXJjZVJvb3QpO1xuICAgIHNtYy5maWxlID0gYVNvdXJjZU1hcC5fZmlsZTtcblxuICAgIC8vIEJlY2F1c2Ugd2UgYXJlIG1vZGlmeWluZyB0aGUgZW50cmllcyAoYnkgY29udmVydGluZyBzdHJpbmcgc291cmNlcyBhbmRcbiAgICAvLyBuYW1lcyB0byBpbmRpY2VzIGludG8gdGhlIHNvdXJjZXMgYW5kIG5hbWVzIEFycmF5U2V0cyksIHdlIGhhdmUgdG8gbWFrZVxuICAgIC8vIGEgY29weSBvZiB0aGUgZW50cnkgb3IgZWxzZSBiYWQgdGhpbmdzIGhhcHBlbi4gU2hhcmVkIG11dGFibGUgc3RhdGVcbiAgICAvLyBzdHJpa2VzIGFnYWluISBTZWUgZ2l0aHViIGlzc3VlICMxOTEuXG5cbiAgICB2YXIgZ2VuZXJhdGVkTWFwcGluZ3MgPSBhU291cmNlTWFwLl9tYXBwaW5ncy50b0FycmF5KCkuc2xpY2UoKTtcbiAgICB2YXIgZGVzdEdlbmVyYXRlZE1hcHBpbmdzID0gc21jLl9fZ2VuZXJhdGVkTWFwcGluZ3MgPSBbXTtcbiAgICB2YXIgZGVzdE9yaWdpbmFsTWFwcGluZ3MgPSBzbWMuX19vcmlnaW5hbE1hcHBpbmdzID0gW107XG5cbiAgICBmb3IgKHZhciBpID0gMCwgbGVuZ3RoID0gZ2VuZXJhdGVkTWFwcGluZ3MubGVuZ3RoOyBpIDwgbGVuZ3RoOyBpKyspIHtcbiAgICAgIHZhciBzcmNNYXBwaW5nID0gZ2VuZXJhdGVkTWFwcGluZ3NbaV07XG4gICAgICB2YXIgZGVzdE1hcHBpbmcgPSBuZXcgTWFwcGluZztcbiAgICAgIGRlc3RNYXBwaW5nLmdlbmVyYXRlZExpbmUgPSBzcmNNYXBwaW5nLmdlbmVyYXRlZExpbmU7XG4gICAgICBkZXN0TWFwcGluZy5nZW5lcmF0ZWRDb2x1bW4gPSBzcmNNYXBwaW5nLmdlbmVyYXRlZENvbHVtbjtcblxuICAgICAgaWYgKHNyY01hcHBpbmcuc291cmNlKSB7XG4gICAgICAgIGRlc3RNYXBwaW5nLnNvdXJjZSA9IHNvdXJjZXMuaW5kZXhPZihzcmNNYXBwaW5nLnNvdXJjZSk7XG4gICAgICAgIGRlc3RNYXBwaW5nLm9yaWdpbmFsTGluZSA9IHNyY01hcHBpbmcub3JpZ2luYWxMaW5lO1xuICAgICAgICBkZXN0TWFwcGluZy5vcmlnaW5hbENvbHVtbiA9IHNyY01hcHBpbmcub3JpZ2luYWxDb2x1bW47XG5cbiAgICAgICAgaWYgKHNyY01hcHBpbmcubmFtZSkge1xuICAgICAgICAgIGRlc3RNYXBwaW5nLm5hbWUgPSBuYW1lcy5pbmRleE9mKHNyY01hcHBpbmcubmFtZSk7XG4gICAgICAgIH1cblxuICAgICAgICBkZXN0T3JpZ2luYWxNYXBwaW5ncy5wdXNoKGRlc3RNYXBwaW5nKTtcbiAgICAgIH1cblxuICAgICAgZGVzdEdlbmVyYXRlZE1hcHBpbmdzLnB1c2goZGVzdE1hcHBpbmcpO1xuICAgIH1cblxuICAgIHF1aWNrU29ydChzbWMuX19vcmlnaW5hbE1hcHBpbmdzLCB1dGlsLmNvbXBhcmVCeU9yaWdpbmFsUG9zaXRpb25zKTtcblxuICAgIHJldHVybiBzbWM7XG4gIH07XG5cbi8qKlxuICogVGhlIHZlcnNpb24gb2YgdGhlIHNvdXJjZSBtYXBwaW5nIHNwZWMgdGhhdCB3ZSBhcmUgY29uc3VtaW5nLlxuICovXG5CYXNpY1NvdXJjZU1hcENvbnN1bWVyLnByb3RvdHlwZS5fdmVyc2lvbiA9IDM7XG5cbi8qKlxuICogVGhlIGxpc3Qgb2Ygb3JpZ2luYWwgc291cmNlcy5cbiAqL1xuT2JqZWN0LmRlZmluZVByb3BlcnR5KEJhc2ljU291cmNlTWFwQ29uc3VtZXIucHJvdG90eXBlLCAnc291cmNlcycsIHtcbiAgZ2V0OiBmdW5jdGlvbiAoKSB7XG4gICAgcmV0dXJuIHRoaXMuX3NvdXJjZXMudG9BcnJheSgpLm1hcChmdW5jdGlvbiAocykge1xuICAgICAgcmV0dXJuIHRoaXMuc291cmNlUm9vdCAhPSBudWxsID8gdXRpbC5qb2luKHRoaXMuc291cmNlUm9vdCwgcykgOiBzO1xuICAgIH0sIHRoaXMpO1xuICB9XG59KTtcblxuLyoqXG4gKiBQcm92aWRlIHRoZSBKSVQgd2l0aCBhIG5pY2Ugc2hhcGUgLyBoaWRkZW4gY2xhc3MuXG4gKi9cbmZ1bmN0aW9uIE1hcHBpbmcoKSB7XG4gIHRoaXMuZ2VuZXJhdGVkTGluZSA9IDA7XG4gIHRoaXMuZ2VuZXJhdGVkQ29sdW1uID0gMDtcbiAgdGhpcy5zb3VyY2UgPSBudWxsO1xuICB0aGlzLm9yaWdpbmFsTGluZSA9IG51bGw7XG4gIHRoaXMub3JpZ2luYWxDb2x1bW4gPSBudWxsO1xuICB0aGlzLm5hbWUgPSBudWxsO1xufVxuXG4vKipcbiAqIFBhcnNlIHRoZSBtYXBwaW5ncyBpbiBhIHN0cmluZyBpbiB0byBhIGRhdGEgc3RydWN0dXJlIHdoaWNoIHdlIGNhbiBlYXNpbHlcbiAqIHF1ZXJ5ICh0aGUgb3JkZXJlZCBhcnJheXMgaW4gdGhlIGB0aGlzLl9fZ2VuZXJhdGVkTWFwcGluZ3NgIGFuZFxuICogYHRoaXMuX19vcmlnaW5hbE1hcHBpbmdzYCBwcm9wZXJ0aWVzKS5cbiAqL1xuQmFzaWNTb3VyY2VNYXBDb25zdW1lci5wcm90b3R5cGUuX3BhcnNlTWFwcGluZ3MgPVxuICBmdW5jdGlvbiBTb3VyY2VNYXBDb25zdW1lcl9wYXJzZU1hcHBpbmdzKGFTdHIsIGFTb3VyY2VSb290KSB7XG4gICAgdmFyIGdlbmVyYXRlZExpbmUgPSAxO1xuICAgIHZhciBwcmV2aW91c0dlbmVyYXRlZENvbHVtbiA9IDA7XG4gICAgdmFyIHByZXZpb3VzT3JpZ2luYWxMaW5lID0gMDtcbiAgICB2YXIgcHJldmlvdXNPcmlnaW5hbENvbHVtbiA9IDA7XG4gICAgdmFyIHByZXZpb3VzU291cmNlID0gMDtcbiAgICB2YXIgcHJldmlvdXNOYW1lID0gMDtcbiAgICB2YXIgbGVuZ3RoID0gYVN0ci5sZW5ndGg7XG4gICAgdmFyIGluZGV4ID0gMDtcbiAgICB2YXIgY2FjaGVkU2VnbWVudHMgPSB7fTtcbiAgICB2YXIgdGVtcCA9IHt9O1xuICAgIHZhciBvcmlnaW5hbE1hcHBpbmdzID0gW107XG4gICAgdmFyIGdlbmVyYXRlZE1hcHBpbmdzID0gW107XG4gICAgdmFyIG1hcHBpbmcsIHN0ciwgc2VnbWVudCwgZW5kLCB2YWx1ZTtcblxuICAgIHdoaWxlIChpbmRleCA8IGxlbmd0aCkge1xuICAgICAgaWYgKGFTdHIuY2hhckF0KGluZGV4KSA9PT0gJzsnKSB7XG4gICAgICAgIGdlbmVyYXRlZExpbmUrKztcbiAgICAgICAgaW5kZXgrKztcbiAgICAgICAgcHJldmlvdXNHZW5lcmF0ZWRDb2x1bW4gPSAwO1xuICAgICAgfVxuICAgICAgZWxzZSBpZiAoYVN0ci5jaGFyQXQoaW5kZXgpID09PSAnLCcpIHtcbiAgICAgICAgaW5kZXgrKztcbiAgICAgIH1cbiAgICAgIGVsc2Uge1xuICAgICAgICBtYXBwaW5nID0gbmV3IE1hcHBpbmcoKTtcbiAgICAgICAgbWFwcGluZy5nZW5lcmF0ZWRMaW5lID0gZ2VuZXJhdGVkTGluZTtcblxuICAgICAgICAvLyBCZWNhdXNlIGVhY2ggb2Zmc2V0IGlzIGVuY29kZWQgcmVsYXRpdmUgdG8gdGhlIHByZXZpb3VzIG9uZSxcbiAgICAgICAgLy8gbWFueSBzZWdtZW50cyBvZnRlbiBoYXZlIHRoZSBzYW1lIGVuY29kaW5nLiBXZSBjYW4gZXhwbG9pdCB0aGlzXG4gICAgICAgIC8vIGZhY3QgYnkgY2FjaGluZyB0aGUgcGFyc2VkIHZhcmlhYmxlIGxlbmd0aCBmaWVsZHMgb2YgZWFjaCBzZWdtZW50LFxuICAgICAgICAvLyBhbGxvd2luZyB1cyB0byBhdm9pZCBhIHNlY29uZCBwYXJzZSBpZiB3ZSBlbmNvdW50ZXIgdGhlIHNhbWVcbiAgICAgICAgLy8gc2VnbWVudCBhZ2Fpbi5cbiAgICAgICAgZm9yIChlbmQgPSBpbmRleDsgZW5kIDwgbGVuZ3RoOyBlbmQrKykge1xuICAgICAgICAgIGlmICh0aGlzLl9jaGFySXNNYXBwaW5nU2VwYXJhdG9yKGFTdHIsIGVuZCkpIHtcbiAgICAgICAgICAgIGJyZWFrO1xuICAgICAgICAgIH1cbiAgICAgICAgfVxuICAgICAgICBzdHIgPSBhU3RyLnNsaWNlKGluZGV4LCBlbmQpO1xuXG4gICAgICAgIHNlZ21lbnQgPSBjYWNoZWRTZWdtZW50c1tzdHJdO1xuICAgICAgICBpZiAoc2VnbWVudCkge1xuICAgICAgICAgIGluZGV4ICs9IHN0ci5sZW5ndGg7XG4gICAgICAgIH0gZWxzZSB7XG4gICAgICAgICAgc2VnbWVudCA9IFtdO1xuICAgICAgICAgIHdoaWxlIChpbmRleCA8IGVuZCkge1xuICAgICAgICAgICAgYmFzZTY0VkxRLmRlY29kZShhU3RyLCBpbmRleCwgdGVtcCk7XG4gICAgICAgICAgICB2YWx1ZSA9IHRlbXAudmFsdWU7XG4gICAgICAgICAgICBpbmRleCA9IHRlbXAucmVzdDtcbiAgICAgICAgICAgIHNlZ21lbnQucHVzaCh2YWx1ZSk7XG4gICAgICAgICAgfVxuXG4gICAgICAgICAgaWYgKHNlZ21lbnQubGVuZ3RoID09PSAyKSB7XG4gICAgICAgICAgICB0aHJvdyBuZXcgRXJyb3IoJ0ZvdW5kIGEgc291cmNlLCBidXQgbm8gbGluZSBhbmQgY29sdW1uJyk7XG4gICAgICAgICAgfVxuXG4gICAgICAgICAgaWYgKHNlZ21lbnQubGVuZ3RoID09PSAzKSB7XG4gICAgICAgICAgICB0aHJvdyBuZXcgRXJyb3IoJ0ZvdW5kIGEgc291cmNlIGFuZCBsaW5lLCBidXQgbm8gY29sdW1uJyk7XG4gICAgICAgICAgfVxuXG4gICAgICAgICAgY2FjaGVkU2VnbWVudHNbc3RyXSA9IHNlZ21lbnQ7XG4gICAgICAgIH1cblxuICAgICAgICAvLyBHZW5lcmF0ZWQgY29sdW1uLlxuICAgICAgICBtYXBwaW5nLmdlbmVyYXRlZENvbHVtbiA9IHByZXZpb3VzR2VuZXJhdGVkQ29sdW1uICsgc2VnbWVudFswXTtcbiAgICAgICAgcHJldmlvdXNHZW5lcmF0ZWRDb2x1bW4gPSBtYXBwaW5nLmdlbmVyYXRlZENvbHVtbjtcblxuICAgICAgICBpZiAoc2VnbWVudC5sZW5ndGggPiAxKSB7XG4gICAgICAgICAgLy8gT3JpZ2luYWwgc291cmNlLlxuICAgICAgICAgIG1hcHBpbmcuc291cmNlID0gcHJldmlvdXNTb3VyY2UgKyBzZWdtZW50WzFdO1xuICAgICAgICAgIHByZXZpb3VzU291cmNlICs9IHNlZ21lbnRbMV07XG5cbiAgICAgICAgICAvLyBPcmlnaW5hbCBsaW5lLlxuICAgICAgICAgIG1hcHBpbmcub3JpZ2luYWxMaW5lID0gcHJldmlvdXNPcmlnaW5hbExpbmUgKyBzZWdtZW50WzJdO1xuICAgICAgICAgIHByZXZpb3VzT3JpZ2luYWxMaW5lID0gbWFwcGluZy5vcmlnaW5hbExpbmU7XG4gICAgICAgICAgLy8gTGluZXMgYXJlIHN0b3JlZCAwLWJhc2VkXG4gICAgICAgICAgbWFwcGluZy5vcmlnaW5hbExpbmUgKz0gMTtcblxuICAgICAgICAgIC8vIE9yaWdpbmFsIGNvbHVtbi5cbiAgICAgICAgICBtYXBwaW5nLm9yaWdpbmFsQ29sdW1uID0gcHJldmlvdXNPcmlnaW5hbENvbHVtbiArIHNlZ21lbnRbM107XG4gICAgICAgICAgcHJldmlvdXNPcmlnaW5hbENvbHVtbiA9IG1hcHBpbmcub3JpZ2luYWxDb2x1bW47XG5cbiAgICAgICAgICBpZiAoc2VnbWVudC5sZW5ndGggPiA0KSB7XG4gICAgICAgICAgICAvLyBPcmlnaW5hbCBuYW1lLlxuICAgICAgICAgICAgbWFwcGluZy5uYW1lID0gcHJldmlvdXNOYW1lICsgc2VnbWVudFs0XTtcbiAgICAgICAgICAgIHByZXZpb3VzTmFtZSArPSBzZWdtZW50WzRdO1xuICAgICAgICAgIH1cbiAgICAgICAgfVxuXG4gICAgICAgIGdlbmVyYXRlZE1hcHBpbmdzLnB1c2gobWFwcGluZyk7XG4gICAgICAgIGlmICh0eXBlb2YgbWFwcGluZy5vcmlnaW5hbExpbmUgPT09ICdudW1iZXInKSB7XG4gICAgICAgICAgb3JpZ2luYWxNYXBwaW5ncy5wdXNoKG1hcHBpbmcpO1xuICAgICAgICB9XG4gICAgICB9XG4gICAgfVxuXG4gICAgcXVpY2tTb3J0KGdlbmVyYXRlZE1hcHBpbmdzLCB1dGlsLmNvbXBhcmVCeUdlbmVyYXRlZFBvc2l0aW9uc0RlZmxhdGVkKTtcbiAgICB0aGlzLl9fZ2VuZXJhdGVkTWFwcGluZ3MgPSBnZW5lcmF0ZWRNYXBwaW5ncztcblxuICAgIHF1aWNrU29ydChvcmlnaW5hbE1hcHBpbmdzLCB1dGlsLmNvbXBhcmVCeU9yaWdpbmFsUG9zaXRpb25zKTtcbiAgICB0aGlzLl9fb3JpZ2luYWxNYXBwaW5ncyA9IG9yaWdpbmFsTWFwcGluZ3M7XG4gIH07XG5cbi8qKlxuICogRmluZCB0aGUgbWFwcGluZyB0aGF0IGJlc3QgbWF0Y2hlcyB0aGUgaHlwb3RoZXRpY2FsIFwibmVlZGxlXCIgbWFwcGluZyB0aGF0XG4gKiB3ZSBhcmUgc2VhcmNoaW5nIGZvciBpbiB0aGUgZ2l2ZW4gXCJoYXlzdGFja1wiIG9mIG1hcHBpbmdzLlxuICovXG5CYXNpY1NvdXJjZU1hcENvbnN1bWVyLnByb3RvdHlwZS5fZmluZE1hcHBpbmcgPVxuICBmdW5jdGlvbiBTb3VyY2VNYXBDb25zdW1lcl9maW5kTWFwcGluZyhhTmVlZGxlLCBhTWFwcGluZ3MsIGFMaW5lTmFtZSxcbiAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgYUNvbHVtbk5hbWUsIGFDb21wYXJhdG9yLCBhQmlhcykge1xuICAgIC8vIFRvIHJldHVybiB0aGUgcG9zaXRpb24gd2UgYXJlIHNlYXJjaGluZyBmb3IsIHdlIG11c3QgZmlyc3QgZmluZCB0aGVcbiAgICAvLyBtYXBwaW5nIGZvciB0aGUgZ2l2ZW4gcG9zaXRpb24gYW5kIHRoZW4gcmV0dXJuIHRoZSBvcHBvc2l0ZSBwb3NpdGlvbiBpdFxuICAgIC8vIHBvaW50cyB0by4gQmVjYXVzZSB0aGUgbWFwcGluZ3MgYXJlIHNvcnRlZCwgd2UgY2FuIHVzZSBiaW5hcnkgc2VhcmNoIHRvXG4gICAgLy8gZmluZCB0aGUgYmVzdCBtYXBwaW5nLlxuXG4gICAgaWYgKGFOZWVkbGVbYUxpbmVOYW1lXSA8PSAwKSB7XG4gICAgICB0aHJvdyBuZXcgVHlwZUVycm9yKCdMaW5lIG11c3QgYmUgZ3JlYXRlciB0aGFuIG9yIGVxdWFsIHRvIDEsIGdvdCAnXG4gICAgICAgICAgICAgICAgICAgICAgICAgICsgYU5lZWRsZVthTGluZU5hbWVdKTtcbiAgICB9XG4gICAgaWYgKGFOZWVkbGVbYUNvbHVtbk5hbWVdIDwgMCkge1xuICAgICAgdGhyb3cgbmV3IFR5cGVFcnJvcignQ29sdW1uIG11c3QgYmUgZ3JlYXRlciB0aGFuIG9yIGVxdWFsIHRvIDAsIGdvdCAnXG4gICAgICAgICAgICAgICAgICAgICAgICAgICsgYU5lZWRsZVthQ29sdW1uTmFtZV0pO1xuICAgIH1cblxuICAgIHJldHVybiBiaW5hcnlTZWFyY2guc2VhcmNoKGFOZWVkbGUsIGFNYXBwaW5ncywgYUNvbXBhcmF0b3IsIGFCaWFzKTtcbiAgfTtcblxuLyoqXG4gKiBDb21wdXRlIHRoZSBsYXN0IGNvbHVtbiBmb3IgZWFjaCBnZW5lcmF0ZWQgbWFwcGluZy4gVGhlIGxhc3QgY29sdW1uIGlzXG4gKiBpbmNsdXNpdmUuXG4gKi9cbkJhc2ljU291cmNlTWFwQ29uc3VtZXIucHJvdG90eXBlLmNvbXB1dGVDb2x1bW5TcGFucyA9XG4gIGZ1bmN0aW9uIFNvdXJjZU1hcENvbnN1bWVyX2NvbXB1dGVDb2x1bW5TcGFucygpIHtcbiAgICBmb3IgKHZhciBpbmRleCA9IDA7IGluZGV4IDwgdGhpcy5fZ2VuZXJhdGVkTWFwcGluZ3MubGVuZ3RoOyArK2luZGV4KSB7XG4gICAgICB2YXIgbWFwcGluZyA9IHRoaXMuX2dlbmVyYXRlZE1hcHBpbmdzW2luZGV4XTtcblxuICAgICAgLy8gTWFwcGluZ3MgZG8gbm90IGNvbnRhaW4gYSBmaWVsZCBmb3IgdGhlIGxhc3QgZ2VuZXJhdGVkIGNvbHVtbnQuIFdlXG4gICAgICAvLyBjYW4gY29tZSB1cCB3aXRoIGFuIG9wdGltaXN0aWMgZXN0aW1hdGUsIGhvd2V2ZXIsIGJ5IGFzc3VtaW5nIHRoYXRcbiAgICAgIC8vIG1hcHBpbmdzIGFyZSBjb250aWd1b3VzIChpLmUuIGdpdmVuIHR3byBjb25zZWN1dGl2ZSBtYXBwaW5ncywgdGhlXG4gICAgICAvLyBmaXJzdCBtYXBwaW5nIGVuZHMgd2hlcmUgdGhlIHNlY29uZCBvbmUgc3RhcnRzKS5cbiAgICAgIGlmIChpbmRleCArIDEgPCB0aGlzLl9nZW5lcmF0ZWRNYXBwaW5ncy5sZW5ndGgpIHtcbiAgICAgICAgdmFyIG5leHRNYXBwaW5nID0gdGhpcy5fZ2VuZXJhdGVkTWFwcGluZ3NbaW5kZXggKyAxXTtcblxuICAgICAgICBpZiAobWFwcGluZy5nZW5lcmF0ZWRMaW5lID09PSBuZXh0TWFwcGluZy5nZW5lcmF0ZWRMaW5lKSB7XG4gICAgICAgICAgbWFwcGluZy5sYXN0R2VuZXJhdGVkQ29sdW1uID0gbmV4dE1hcHBpbmcuZ2VuZXJhdGVkQ29sdW1uIC0gMTtcbiAgICAgICAgICBjb250aW51ZTtcbiAgICAgICAgfVxuICAgICAgfVxuXG4gICAgICAvLyBUaGUgbGFzdCBtYXBwaW5nIGZvciBlYWNoIGxpbmUgc3BhbnMgdGhlIGVudGlyZSBsaW5lLlxuICAgICAgbWFwcGluZy5sYXN0R2VuZXJhdGVkQ29sdW1uID0gSW5maW5pdHk7XG4gICAgfVxuICB9O1xuXG4vKipcbiAqIFJldHVybnMgdGhlIG9yaWdpbmFsIHNvdXJjZSwgbGluZSwgYW5kIGNvbHVtbiBpbmZvcm1hdGlvbiBmb3IgdGhlIGdlbmVyYXRlZFxuICogc291cmNlJ3MgbGluZSBhbmQgY29sdW1uIHBvc2l0aW9ucyBwcm92aWRlZC4gVGhlIG9ubHkgYXJndW1lbnQgaXMgYW4gb2JqZWN0XG4gKiB3aXRoIHRoZSBmb2xsb3dpbmcgcHJvcGVydGllczpcbiAqXG4gKiAgIC0gbGluZTogVGhlIGxpbmUgbnVtYmVyIGluIHRoZSBnZW5lcmF0ZWQgc291cmNlLlxuICogICAtIGNvbHVtbjogVGhlIGNvbHVtbiBudW1iZXIgaW4gdGhlIGdlbmVyYXRlZCBzb3VyY2UuXG4gKiAgIC0gYmlhczogRWl0aGVyICdTb3VyY2VNYXBDb25zdW1lci5HUkVBVEVTVF9MT1dFUl9CT1VORCcgb3JcbiAqICAgICAnU291cmNlTWFwQ29uc3VtZXIuTEVBU1RfVVBQRVJfQk9VTkQnLiBTcGVjaWZpZXMgd2hldGhlciB0byByZXR1cm4gdGhlXG4gKiAgICAgY2xvc2VzdCBlbGVtZW50IHRoYXQgaXMgc21hbGxlciB0aGFuIG9yIGdyZWF0ZXIgdGhhbiB0aGUgb25lIHdlIGFyZVxuICogICAgIHNlYXJjaGluZyBmb3IsIHJlc3BlY3RpdmVseSwgaWYgdGhlIGV4YWN0IGVsZW1lbnQgY2Fubm90IGJlIGZvdW5kLlxuICogICAgIERlZmF1bHRzIHRvICdTb3VyY2VNYXBDb25zdW1lci5HUkVBVEVTVF9MT1dFUl9CT1VORCcuXG4gKlxuICogYW5kIGFuIG9iamVjdCBpcyByZXR1cm5lZCB3aXRoIHRoZSBmb2xsb3dpbmcgcHJvcGVydGllczpcbiAqXG4gKiAgIC0gc291cmNlOiBUaGUgb3JpZ2luYWwgc291cmNlIGZpbGUsIG9yIG51bGwuXG4gKiAgIC0gbGluZTogVGhlIGxpbmUgbnVtYmVyIGluIHRoZSBvcmlnaW5hbCBzb3VyY2UsIG9yIG51bGwuXG4gKiAgIC0gY29sdW1uOiBUaGUgY29sdW1uIG51bWJlciBpbiB0aGUgb3JpZ2luYWwgc291cmNlLCBvciBudWxsLlxuICogICAtIG5hbWU6IFRoZSBvcmlnaW5hbCBpZGVudGlmaWVyLCBvciBudWxsLlxuICovXG5CYXNpY1NvdXJjZU1hcENvbnN1bWVyLnByb3RvdHlwZS5vcmlnaW5hbFBvc2l0aW9uRm9yID1cbiAgZnVuY3Rpb24gU291cmNlTWFwQ29uc3VtZXJfb3JpZ2luYWxQb3NpdGlvbkZvcihhQXJncykge1xuICAgIHZhciBuZWVkbGUgPSB7XG4gICAgICBnZW5lcmF0ZWRMaW5lOiB1dGlsLmdldEFyZyhhQXJncywgJ2xpbmUnKSxcbiAgICAgIGdlbmVyYXRlZENvbHVtbjogdXRpbC5nZXRBcmcoYUFyZ3MsICdjb2x1bW4nKVxuICAgIH07XG5cbiAgICB2YXIgaW5kZXggPSB0aGlzLl9maW5kTWFwcGluZyhcbiAgICAgIG5lZWRsZSxcbiAgICAgIHRoaXMuX2dlbmVyYXRlZE1hcHBpbmdzLFxuICAgICAgXCJnZW5lcmF0ZWRMaW5lXCIsXG4gICAgICBcImdlbmVyYXRlZENvbHVtblwiLFxuICAgICAgdXRpbC5jb21wYXJlQnlHZW5lcmF0ZWRQb3NpdGlvbnNEZWZsYXRlZCxcbiAgICAgIHV0aWwuZ2V0QXJnKGFBcmdzLCAnYmlhcycsIFNvdXJjZU1hcENvbnN1bWVyLkdSRUFURVNUX0xPV0VSX0JPVU5EKVxuICAgICk7XG5cbiAgICBpZiAoaW5kZXggPj0gMCkge1xuICAgICAgdmFyIG1hcHBpbmcgPSB0aGlzLl9nZW5lcmF0ZWRNYXBwaW5nc1tpbmRleF07XG5cbiAgICAgIGlmIChtYXBwaW5nLmdlbmVyYXRlZExpbmUgPT09IG5lZWRsZS5nZW5lcmF0ZWRMaW5lKSB7XG4gICAgICAgIHZhciBzb3VyY2UgPSB1dGlsLmdldEFyZyhtYXBwaW5nLCAnc291cmNlJywgbnVsbCk7XG4gICAgICAgIGlmIChzb3VyY2UgIT09IG51bGwpIHtcbiAgICAgICAgICBzb3VyY2UgPSB0aGlzLl9zb3VyY2VzLmF0KHNvdXJjZSk7XG4gICAgICAgICAgaWYgKHRoaXMuc291cmNlUm9vdCAhPSBudWxsKSB7XG4gICAgICAgICAgICBzb3VyY2UgPSB1dGlsLmpvaW4odGhpcy5zb3VyY2VSb290LCBzb3VyY2UpO1xuICAgICAgICAgIH1cbiAgICAgICAgfVxuICAgICAgICB2YXIgbmFtZSA9IHV0aWwuZ2V0QXJnKG1hcHBpbmcsICduYW1lJywgbnVsbCk7XG4gICAgICAgIGlmIChuYW1lICE9PSBudWxsKSB7XG4gICAgICAgICAgbmFtZSA9IHRoaXMuX25hbWVzLmF0KG5hbWUpO1xuICAgICAgICB9XG4gICAgICAgIHJldHVybiB7XG4gICAgICAgICAgc291cmNlOiBzb3VyY2UsXG4gICAgICAgICAgbGluZTogdXRpbC5nZXRBcmcobWFwcGluZywgJ29yaWdpbmFsTGluZScsIG51bGwpLFxuICAgICAgICAgIGNvbHVtbjogdXRpbC5nZXRBcmcobWFwcGluZywgJ29yaWdpbmFsQ29sdW1uJywgbnVsbCksXG4gICAgICAgICAgbmFtZTogbmFtZVxuICAgICAgICB9O1xuICAgICAgfVxuICAgIH1cblxuICAgIHJldHVybiB7XG4gICAgICBzb3VyY2U6IG51bGwsXG4gICAgICBsaW5lOiBudWxsLFxuICAgICAgY29sdW1uOiBudWxsLFxuICAgICAgbmFtZTogbnVsbFxuICAgIH07XG4gIH07XG5cbi8qKlxuICogUmV0dXJuIHRydWUgaWYgd2UgaGF2ZSB0aGUgc291cmNlIGNvbnRlbnQgZm9yIGV2ZXJ5IHNvdXJjZSBpbiB0aGUgc291cmNlXG4gKiBtYXAsIGZhbHNlIG90aGVyd2lzZS5cbiAqL1xuQmFzaWNTb3VyY2VNYXBDb25zdW1lci5wcm90b3R5cGUuaGFzQ29udGVudHNPZkFsbFNvdXJjZXMgPVxuICBmdW5jdGlvbiBCYXNpY1NvdXJjZU1hcENvbnN1bWVyX2hhc0NvbnRlbnRzT2ZBbGxTb3VyY2VzKCkge1xuICAgIGlmICghdGhpcy5zb3VyY2VzQ29udGVudCkge1xuICAgICAgcmV0dXJuIGZhbHNlO1xuICAgIH1cbiAgICByZXR1cm4gdGhpcy5zb3VyY2VzQ29udGVudC5sZW5ndGggPj0gdGhpcy5fc291cmNlcy5zaXplKCkgJiZcbiAgICAgICF0aGlzLnNvdXJjZXNDb250ZW50LnNvbWUoZnVuY3Rpb24gKHNjKSB7IHJldHVybiBzYyA9PSBudWxsOyB9KTtcbiAgfTtcblxuLyoqXG4gKiBSZXR1cm5zIHRoZSBvcmlnaW5hbCBzb3VyY2UgY29udGVudC4gVGhlIG9ubHkgYXJndW1lbnQgaXMgdGhlIHVybCBvZiB0aGVcbiAqIG9yaWdpbmFsIHNvdXJjZSBmaWxlLiBSZXR1cm5zIG51bGwgaWYgbm8gb3JpZ2luYWwgc291cmNlIGNvbnRlbnQgaXNcbiAqIGF2YWlsYWJsZS5cbiAqL1xuQmFzaWNTb3VyY2VNYXBDb25zdW1lci5wcm90b3R5cGUuc291cmNlQ29udGVudEZvciA9XG4gIGZ1bmN0aW9uIFNvdXJjZU1hcENvbnN1bWVyX3NvdXJjZUNvbnRlbnRGb3IoYVNvdXJjZSwgbnVsbE9uTWlzc2luZykge1xuICAgIGlmICghdGhpcy5zb3VyY2VzQ29udGVudCkge1xuICAgICAgcmV0dXJuIG51bGw7XG4gICAgfVxuXG4gICAgaWYgKHRoaXMuc291cmNlUm9vdCAhPSBudWxsKSB7XG4gICAgICBhU291cmNlID0gdXRpbC5yZWxhdGl2ZSh0aGlzLnNvdXJjZVJvb3QsIGFTb3VyY2UpO1xuICAgIH1cblxuICAgIGlmICh0aGlzLl9zb3VyY2VzLmhhcyhhU291cmNlKSkge1xuICAgICAgcmV0dXJuIHRoaXMuc291cmNlc0NvbnRlbnRbdGhpcy5fc291cmNlcy5pbmRleE9mKGFTb3VyY2UpXTtcbiAgICB9XG5cbiAgICB2YXIgdXJsO1xuICAgIGlmICh0aGlzLnNvdXJjZVJvb3QgIT0gbnVsbFxuICAgICAgICAmJiAodXJsID0gdXRpbC51cmxQYXJzZSh0aGlzLnNvdXJjZVJvb3QpKSkge1xuICAgICAgLy8gWFhYOiBmaWxlOi8vIFVSSXMgYW5kIGFic29sdXRlIHBhdGhzIGxlYWQgdG8gdW5leHBlY3RlZCBiZWhhdmlvciBmb3JcbiAgICAgIC8vIG1hbnkgdXNlcnMuIFdlIGNhbiBoZWxwIHRoZW0gb3V0IHdoZW4gdGhleSBleHBlY3QgZmlsZTovLyBVUklzIHRvXG4gICAgICAvLyBiZWhhdmUgbGlrZSBpdCB3b3VsZCBpZiB0aGV5IHdlcmUgcnVubmluZyBhIGxvY2FsIEhUVFAgc2VydmVyLiBTZWVcbiAgICAgIC8vIGh0dHBzOi8vYnVnemlsbGEubW96aWxsYS5vcmcvc2hvd19idWcuY2dpP2lkPTg4NTU5Ny5cbiAgICAgIHZhciBmaWxlVXJpQWJzUGF0aCA9IGFTb3VyY2UucmVwbGFjZSgvXmZpbGU6XFwvXFwvLywgXCJcIik7XG4gICAgICBpZiAodXJsLnNjaGVtZSA9PSBcImZpbGVcIlxuICAgICAgICAgICYmIHRoaXMuX3NvdXJjZXMuaGFzKGZpbGVVcmlBYnNQYXRoKSkge1xuICAgICAgICByZXR1cm4gdGhpcy5zb3VyY2VzQ29udGVudFt0aGlzLl9zb3VyY2VzLmluZGV4T2YoZmlsZVVyaUFic1BhdGgpXVxuICAgICAgfVxuXG4gICAgICBpZiAoKCF1cmwucGF0aCB8fCB1cmwucGF0aCA9PSBcIi9cIilcbiAgICAgICAgICAmJiB0aGlzLl9zb3VyY2VzLmhhcyhcIi9cIiArIGFTb3VyY2UpKSB7XG4gICAgICAgIHJldHVybiB0aGlzLnNvdXJjZXNDb250ZW50W3RoaXMuX3NvdXJjZXMuaW5kZXhPZihcIi9cIiArIGFTb3VyY2UpXTtcbiAgICAgIH1cbiAgICB9XG5cbiAgICAvLyBUaGlzIGZ1bmN0aW9uIGlzIHVzZWQgcmVjdXJzaXZlbHkgZnJvbVxuICAgIC8vIEluZGV4ZWRTb3VyY2VNYXBDb25zdW1lci5wcm90b3R5cGUuc291cmNlQ29udGVudEZvci4gSW4gdGhhdCBjYXNlLCB3ZVxuICAgIC8vIGRvbid0IHdhbnQgdG8gdGhyb3cgaWYgd2UgY2FuJ3QgZmluZCB0aGUgc291cmNlIC0gd2UganVzdCB3YW50IHRvXG4gICAgLy8gcmV0dXJuIG51bGwsIHNvIHdlIHByb3ZpZGUgYSBmbGFnIHRvIGV4aXQgZ3JhY2VmdWxseS5cbiAgICBpZiAobnVsbE9uTWlzc2luZykge1xuICAgICAgcmV0dXJuIG51bGw7XG4gICAgfVxuICAgIGVsc2Uge1xuICAgICAgdGhyb3cgbmV3IEVycm9yKCdcIicgKyBhU291cmNlICsgJ1wiIGlzIG5vdCBpbiB0aGUgU291cmNlTWFwLicpO1xuICAgIH1cbiAgfTtcblxuLyoqXG4gKiBSZXR1cm5zIHRoZSBnZW5lcmF0ZWQgbGluZSBhbmQgY29sdW1uIGluZm9ybWF0aW9uIGZvciB0aGUgb3JpZ2luYWwgc291cmNlLFxuICogbGluZSwgYW5kIGNvbHVtbiBwb3NpdGlvbnMgcHJvdmlkZWQuIFRoZSBvbmx5IGFyZ3VtZW50IGlzIGFuIG9iamVjdCB3aXRoXG4gKiB0aGUgZm9sbG93aW5nIHByb3BlcnRpZXM6XG4gKlxuICogICAtIHNvdXJjZTogVGhlIGZpbGVuYW1lIG9mIHRoZSBvcmlnaW5hbCBzb3VyY2UuXG4gKiAgIC0gbGluZTogVGhlIGxpbmUgbnVtYmVyIGluIHRoZSBvcmlnaW5hbCBzb3VyY2UuXG4gKiAgIC0gY29sdW1uOiBUaGUgY29sdW1uIG51bWJlciBpbiB0aGUgb3JpZ2luYWwgc291cmNlLlxuICogICAtIGJpYXM6IEVpdGhlciAnU291cmNlTWFwQ29uc3VtZXIuR1JFQVRFU1RfTE9XRVJfQk9VTkQnIG9yXG4gKiAgICAgJ1NvdXJjZU1hcENvbnN1bWVyLkxFQVNUX1VQUEVSX0JPVU5EJy4gU3BlY2lmaWVzIHdoZXRoZXIgdG8gcmV0dXJuIHRoZVxuICogICAgIGNsb3Nlc3QgZWxlbWVudCB0aGF0IGlzIHNtYWxsZXIgdGhhbiBvciBncmVhdGVyIHRoYW4gdGhlIG9uZSB3ZSBhcmVcbiAqICAgICBzZWFyY2hpbmcgZm9yLCByZXNwZWN0aXZlbHksIGlmIHRoZSBleGFjdCBlbGVtZW50IGNhbm5vdCBiZSBmb3VuZC5cbiAqICAgICBEZWZhdWx0cyB0byAnU291cmNlTWFwQ29uc3VtZXIuR1JFQVRFU1RfTE9XRVJfQk9VTkQnLlxuICpcbiAqIGFuZCBhbiBvYmplY3QgaXMgcmV0dXJuZWQgd2l0aCB0aGUgZm9sbG93aW5nIHByb3BlcnRpZXM6XG4gKlxuICogICAtIGxpbmU6IFRoZSBsaW5lIG51bWJlciBpbiB0aGUgZ2VuZXJhdGVkIHNvdXJjZSwgb3IgbnVsbC5cbiAqICAgLSBjb2x1bW46IFRoZSBjb2x1bW4gbnVtYmVyIGluIHRoZSBnZW5lcmF0ZWQgc291cmNlLCBvciBudWxsLlxuICovXG5CYXNpY1NvdXJjZU1hcENvbnN1bWVyLnByb3RvdHlwZS5nZW5lcmF0ZWRQb3NpdGlvbkZvciA9XG4gIGZ1bmN0aW9uIFNvdXJjZU1hcENvbnN1bWVyX2dlbmVyYXRlZFBvc2l0aW9uRm9yKGFBcmdzKSB7XG4gICAgdmFyIHNvdXJjZSA9IHV0aWwuZ2V0QXJnKGFBcmdzLCAnc291cmNlJyk7XG4gICAgaWYgKHRoaXMuc291cmNlUm9vdCAhPSBudWxsKSB7XG4gICAgICBzb3VyY2UgPSB1dGlsLnJlbGF0aXZlKHRoaXMuc291cmNlUm9vdCwgc291cmNlKTtcbiAgICB9XG4gICAgaWYgKCF0aGlzLl9zb3VyY2VzLmhhcyhzb3VyY2UpKSB7XG4gICAgICByZXR1cm4ge1xuICAgICAgICBsaW5lOiBudWxsLFxuICAgICAgICBjb2x1bW46IG51bGwsXG4gICAgICAgIGxhc3RDb2x1bW46IG51bGxcbiAgICAgIH07XG4gICAgfVxuICAgIHNvdXJjZSA9IHRoaXMuX3NvdXJjZXMuaW5kZXhPZihzb3VyY2UpO1xuXG4gICAgdmFyIG5lZWRsZSA9IHtcbiAgICAgIHNvdXJjZTogc291cmNlLFxuICAgICAgb3JpZ2luYWxMaW5lOiB1dGlsLmdldEFyZyhhQXJncywgJ2xpbmUnKSxcbiAgICAgIG9yaWdpbmFsQ29sdW1uOiB1dGlsLmdldEFyZyhhQXJncywgJ2NvbHVtbicpXG4gICAgfTtcblxuICAgIHZhciBpbmRleCA9IHRoaXMuX2ZpbmRNYXBwaW5nKFxuICAgICAgbmVlZGxlLFxuICAgICAgdGhpcy5fb3JpZ2luYWxNYXBwaW5ncyxcbiAgICAgIFwib3JpZ2luYWxMaW5lXCIsXG4gICAgICBcIm9yaWdpbmFsQ29sdW1uXCIsXG4gICAgICB1dGlsLmNvbXBhcmVCeU9yaWdpbmFsUG9zaXRpb25zLFxuICAgICAgdXRpbC5nZXRBcmcoYUFyZ3MsICdiaWFzJywgU291cmNlTWFwQ29uc3VtZXIuR1JFQVRFU1RfTE9XRVJfQk9VTkQpXG4gICAgKTtcblxuICAgIGlmIChpbmRleCA+PSAwKSB7XG4gICAgICB2YXIgbWFwcGluZyA9IHRoaXMuX29yaWdpbmFsTWFwcGluZ3NbaW5kZXhdO1xuXG4gICAgICBpZiAobWFwcGluZy5zb3VyY2UgPT09IG5lZWRsZS5zb3VyY2UpIHtcbiAgICAgICAgcmV0dXJuIHtcbiAgICAgICAgICBsaW5lOiB1dGlsLmdldEFyZyhtYXBwaW5nLCAnZ2VuZXJhdGVkTGluZScsIG51bGwpLFxuICAgICAgICAgIGNvbHVtbjogdXRpbC5nZXRBcmcobWFwcGluZywgJ2dlbmVyYXRlZENvbHVtbicsIG51bGwpLFxuICAgICAgICAgIGxhc3RDb2x1bW46IHV0aWwuZ2V0QXJnKG1hcHBpbmcsICdsYXN0R2VuZXJhdGVkQ29sdW1uJywgbnVsbClcbiAgICAgICAgfTtcbiAgICAgIH1cbiAgICB9XG5cbiAgICByZXR1cm4ge1xuICAgICAgbGluZTogbnVsbCxcbiAgICAgIGNvbHVtbjogbnVsbCxcbiAgICAgIGxhc3RDb2x1bW46IG51bGxcbiAgICB9O1xuICB9O1xuXG5leHBvcnRzLkJhc2ljU291cmNlTWFwQ29uc3VtZXIgPSBCYXNpY1NvdXJjZU1hcENvbnN1bWVyO1xuXG4vKipcbiAqIEFuIEluZGV4ZWRTb3VyY2VNYXBDb25zdW1lciBpbnN0YW5jZSByZXByZXNlbnRzIGEgcGFyc2VkIHNvdXJjZSBtYXAgd2hpY2hcbiAqIHdlIGNhbiBxdWVyeSBmb3IgaW5mb3JtYXRpb24uIEl0IGRpZmZlcnMgZnJvbSBCYXNpY1NvdXJjZU1hcENvbnN1bWVyIGluXG4gKiB0aGF0IGl0IHRha2VzIFwiaW5kZXhlZFwiIHNvdXJjZSBtYXBzIChpLmUuIG9uZXMgd2l0aCBhIFwic2VjdGlvbnNcIiBmaWVsZCkgYXNcbiAqIGlucHV0LlxuICpcbiAqIFRoZSBvbmx5IHBhcmFtZXRlciBpcyBhIHJhdyBzb3VyY2UgbWFwIChlaXRoZXIgYXMgYSBKU09OIHN0cmluZywgb3IgYWxyZWFkeVxuICogcGFyc2VkIHRvIGFuIG9iamVjdCkuIEFjY29yZGluZyB0byB0aGUgc3BlYyBmb3IgaW5kZXhlZCBzb3VyY2UgbWFwcywgdGhleVxuICogaGF2ZSB0aGUgZm9sbG93aW5nIGF0dHJpYnV0ZXM6XG4gKlxuICogICAtIHZlcnNpb246IFdoaWNoIHZlcnNpb24gb2YgdGhlIHNvdXJjZSBtYXAgc3BlYyB0aGlzIG1hcCBpcyBmb2xsb3dpbmcuXG4gKiAgIC0gZmlsZTogT3B0aW9uYWwuIFRoZSBnZW5lcmF0ZWQgZmlsZSB0aGlzIHNvdXJjZSBtYXAgaXMgYXNzb2NpYXRlZCB3aXRoLlxuICogICAtIHNlY3Rpb25zOiBBIGxpc3Qgb2Ygc2VjdGlvbiBkZWZpbml0aW9ucy5cbiAqXG4gKiBFYWNoIHZhbHVlIHVuZGVyIHRoZSBcInNlY3Rpb25zXCIgZmllbGQgaGFzIHR3byBmaWVsZHM6XG4gKiAgIC0gb2Zmc2V0OiBUaGUgb2Zmc2V0IGludG8gdGhlIG9yaWdpbmFsIHNwZWNpZmllZCBhdCB3aGljaCB0aGlzIHNlY3Rpb25cbiAqICAgICAgIGJlZ2lucyB0byBhcHBseSwgZGVmaW5lZCBhcyBhbiBvYmplY3Qgd2l0aCBhIFwibGluZVwiIGFuZCBcImNvbHVtblwiXG4gKiAgICAgICBmaWVsZC5cbiAqICAgLSBtYXA6IEEgc291cmNlIG1hcCBkZWZpbml0aW9uLiBUaGlzIHNvdXJjZSBtYXAgY291bGQgYWxzbyBiZSBpbmRleGVkLFxuICogICAgICAgYnV0IGRvZXNuJ3QgaGF2ZSB0byBiZS5cbiAqXG4gKiBJbnN0ZWFkIG9mIHRoZSBcIm1hcFwiIGZpZWxkLCBpdCdzIGFsc28gcG9zc2libGUgdG8gaGF2ZSBhIFwidXJsXCIgZmllbGRcbiAqIHNwZWNpZnlpbmcgYSBVUkwgdG8gcmV0cmlldmUgYSBzb3VyY2UgbWFwIGZyb20sIGJ1dCB0aGF0J3MgY3VycmVudGx5XG4gKiB1bnN1cHBvcnRlZC5cbiAqXG4gKiBIZXJlJ3MgYW4gZXhhbXBsZSBzb3VyY2UgbWFwLCB0YWtlbiBmcm9tIHRoZSBzb3VyY2UgbWFwIHNwZWNbMF0sIGJ1dFxuICogbW9kaWZpZWQgdG8gb21pdCBhIHNlY3Rpb24gd2hpY2ggdXNlcyB0aGUgXCJ1cmxcIiBmaWVsZC5cbiAqXG4gKiAge1xuICogICAgdmVyc2lvbiA6IDMsXG4gKiAgICBmaWxlOiBcImFwcC5qc1wiLFxuICogICAgc2VjdGlvbnM6IFt7XG4gKiAgICAgIG9mZnNldDoge2xpbmU6MTAwLCBjb2x1bW46MTB9LFxuICogICAgICBtYXA6IHtcbiAqICAgICAgICB2ZXJzaW9uIDogMyxcbiAqICAgICAgICBmaWxlOiBcInNlY3Rpb24uanNcIixcbiAqICAgICAgICBzb3VyY2VzOiBbXCJmb28uanNcIiwgXCJiYXIuanNcIl0sXG4gKiAgICAgICAgbmFtZXM6IFtcInNyY1wiLCBcIm1hcHNcIiwgXCJhcmVcIiwgXCJmdW5cIl0sXG4gKiAgICAgICAgbWFwcGluZ3M6IFwiQUFBQSxFOztBQkNERTtcIlxuICogICAgICB9XG4gKiAgICB9XSxcbiAqICB9XG4gKlxuICogWzBdOiBodHRwczovL2RvY3MuZ29vZ2xlLmNvbS9kb2N1bWVudC9kLzFVMVJHQWVoUXdSeXBVVG92RjFLUmxwaU9GemUwYi1fMmdjNmZBSDBLWTBrL2VkaXQjaGVhZGluZz1oLjUzNWVzM3hlcHJndFxuICovXG5mdW5jdGlvbiBJbmRleGVkU291cmNlTWFwQ29uc3VtZXIoYVNvdXJjZU1hcCkge1xuICB2YXIgc291cmNlTWFwID0gYVNvdXJjZU1hcDtcbiAgaWYgKHR5cGVvZiBhU291cmNlTWFwID09PSAnc3RyaW5nJykge1xuICAgIHNvdXJjZU1hcCA9IEpTT04ucGFyc2UoYVNvdXJjZU1hcC5yZXBsYWNlKC9eXFwpXFxdXFx9Jy8sICcnKSk7XG4gIH1cblxuICB2YXIgdmVyc2lvbiA9IHV0aWwuZ2V0QXJnKHNvdXJjZU1hcCwgJ3ZlcnNpb24nKTtcbiAgdmFyIHNlY3Rpb25zID0gdXRpbC5nZXRBcmcoc291cmNlTWFwLCAnc2VjdGlvbnMnKTtcblxuICBpZiAodmVyc2lvbiAhPSB0aGlzLl92ZXJzaW9uKSB7XG4gICAgdGhyb3cgbmV3IEVycm9yKCdVbnN1cHBvcnRlZCB2ZXJzaW9uOiAnICsgdmVyc2lvbik7XG4gIH1cblxuICB0aGlzLl9zb3VyY2VzID0gbmV3IEFycmF5U2V0KCk7XG4gIHRoaXMuX25hbWVzID0gbmV3IEFycmF5U2V0KCk7XG5cbiAgdmFyIGxhc3RPZmZzZXQgPSB7XG4gICAgbGluZTogLTEsXG4gICAgY29sdW1uOiAwXG4gIH07XG4gIHRoaXMuX3NlY3Rpb25zID0gc2VjdGlvbnMubWFwKGZ1bmN0aW9uIChzKSB7XG4gICAgaWYgKHMudXJsKSB7XG4gICAgICAvLyBUaGUgdXJsIGZpZWxkIHdpbGwgcmVxdWlyZSBzdXBwb3J0IGZvciBhc3luY2hyb25pY2l0eS5cbiAgICAgIC8vIFNlZSBodHRwczovL2dpdGh1Yi5jb20vbW96aWxsYS9zb3VyY2UtbWFwL2lzc3Vlcy8xNlxuICAgICAgdGhyb3cgbmV3IEVycm9yKCdTdXBwb3J0IGZvciB1cmwgZmllbGQgaW4gc2VjdGlvbnMgbm90IGltcGxlbWVudGVkLicpO1xuICAgIH1cbiAgICB2YXIgb2Zmc2V0ID0gdXRpbC5nZXRBcmcocywgJ29mZnNldCcpO1xuICAgIHZhciBvZmZzZXRMaW5lID0gdXRpbC5nZXRBcmcob2Zmc2V0LCAnbGluZScpO1xuICAgIHZhciBvZmZzZXRDb2x1bW4gPSB1dGlsLmdldEFyZyhvZmZzZXQsICdjb2x1bW4nKTtcblxuICAgIGlmIChvZmZzZXRMaW5lIDwgbGFzdE9mZnNldC5saW5lIHx8XG4gICAgICAgIChvZmZzZXRMaW5lID09PSBsYXN0T2Zmc2V0LmxpbmUgJiYgb2Zmc2V0Q29sdW1uIDwgbGFzdE9mZnNldC5jb2x1bW4pKSB7XG4gICAgICB0aHJvdyBuZXcgRXJyb3IoJ1NlY3Rpb24gb2Zmc2V0cyBtdXN0IGJlIG9yZGVyZWQgYW5kIG5vbi1vdmVybGFwcGluZy4nKTtcbiAgICB9XG4gICAgbGFzdE9mZnNldCA9IG9mZnNldDtcblxuICAgIHJldHVybiB7XG4gICAgICBnZW5lcmF0ZWRPZmZzZXQ6IHtcbiAgICAgICAgLy8gVGhlIG9mZnNldCBmaWVsZHMgYXJlIDAtYmFzZWQsIGJ1dCB3ZSB1c2UgMS1iYXNlZCBpbmRpY2VzIHdoZW5cbiAgICAgICAgLy8gZW5jb2RpbmcvZGVjb2RpbmcgZnJvbSBWTFEuXG4gICAgICAgIGdlbmVyYXRlZExpbmU6IG9mZnNldExpbmUgKyAxLFxuICAgICAgICBnZW5lcmF0ZWRDb2x1bW46IG9mZnNldENvbHVtbiArIDFcbiAgICAgIH0sXG4gICAgICBjb25zdW1lcjogbmV3IFNvdXJjZU1hcENvbnN1bWVyKHV0aWwuZ2V0QXJnKHMsICdtYXAnKSlcbiAgICB9XG4gIH0pO1xufVxuXG5JbmRleGVkU291cmNlTWFwQ29uc3VtZXIucHJvdG90eXBlID0gT2JqZWN0LmNyZWF0ZShTb3VyY2VNYXBDb25zdW1lci5wcm90b3R5cGUpO1xuSW5kZXhlZFNvdXJjZU1hcENvbnN1bWVyLnByb3RvdHlwZS5jb25zdHJ1Y3RvciA9IFNvdXJjZU1hcENvbnN1bWVyO1xuXG4vKipcbiAqIFRoZSB2ZXJzaW9uIG9mIHRoZSBzb3VyY2UgbWFwcGluZyBzcGVjIHRoYXQgd2UgYXJlIGNvbnN1bWluZy5cbiAqL1xuSW5kZXhlZFNvdXJjZU1hcENvbnN1bWVyLnByb3RvdHlwZS5fdmVyc2lvbiA9IDM7XG5cbi8qKlxuICogVGhlIGxpc3Qgb2Ygb3JpZ2luYWwgc291cmNlcy5cbiAqL1xuT2JqZWN0LmRlZmluZVByb3BlcnR5KEluZGV4ZWRTb3VyY2VNYXBDb25zdW1lci5wcm90b3R5cGUsICdzb3VyY2VzJywge1xuICBnZXQ6IGZ1bmN0aW9uICgpIHtcbiAgICB2YXIgc291cmNlcyA9IFtdO1xuICAgIGZvciAodmFyIGkgPSAwOyBpIDwgdGhpcy5fc2VjdGlvbnMubGVuZ3RoOyBpKyspIHtcbiAgICAgIGZvciAodmFyIGogPSAwOyBqIDwgdGhpcy5fc2VjdGlvbnNbaV0uY29uc3VtZXIuc291cmNlcy5sZW5ndGg7IGorKykge1xuICAgICAgICBzb3VyY2VzLnB1c2godGhpcy5fc2VjdGlvbnNbaV0uY29uc3VtZXIuc291cmNlc1tqXSk7XG4gICAgICB9XG4gICAgfVxuICAgIHJldHVybiBzb3VyY2VzO1xuICB9XG59KTtcblxuLyoqXG4gKiBSZXR1cm5zIHRoZSBvcmlnaW5hbCBzb3VyY2UsIGxpbmUsIGFuZCBjb2x1bW4gaW5mb3JtYXRpb24gZm9yIHRoZSBnZW5lcmF0ZWRcbiAqIHNvdXJjZSdzIGxpbmUgYW5kIGNvbHVtbiBwb3NpdGlvbnMgcHJvdmlkZWQuIFRoZSBvbmx5IGFyZ3VtZW50IGlzIGFuIG9iamVjdFxuICogd2l0aCB0aGUgZm9sbG93aW5nIHByb3BlcnRpZXM6XG4gKlxuICogICAtIGxpbmU6IFRoZSBsaW5lIG51bWJlciBpbiB0aGUgZ2VuZXJhdGVkIHNvdXJjZS5cbiAqICAgLSBjb2x1bW46IFRoZSBjb2x1bW4gbnVtYmVyIGluIHRoZSBnZW5lcmF0ZWQgc291cmNlLlxuICpcbiAqIGFuZCBhbiBvYmplY3QgaXMgcmV0dXJuZWQgd2l0aCB0aGUgZm9sbG93aW5nIHByb3BlcnRpZXM6XG4gKlxuICogICAtIHNvdXJjZTogVGhlIG9yaWdpbmFsIHNvdXJjZSBmaWxlLCBvciBudWxsLlxuICogICAtIGxpbmU6IFRoZSBsaW5lIG51bWJlciBpbiB0aGUgb3JpZ2luYWwgc291cmNlLCBvciBudWxsLlxuICogICAtIGNvbHVtbjogVGhlIGNvbHVtbiBudW1iZXIgaW4gdGhlIG9yaWdpbmFsIHNvdXJjZSwgb3IgbnVsbC5cbiAqICAgLSBuYW1lOiBUaGUgb3JpZ2luYWwgaWRlbnRpZmllciwgb3IgbnVsbC5cbiAqL1xuSW5kZXhlZFNvdXJjZU1hcENvbnN1bWVyLnByb3RvdHlwZS5vcmlnaW5hbFBvc2l0aW9uRm9yID1cbiAgZnVuY3Rpb24gSW5kZXhlZFNvdXJjZU1hcENvbnN1bWVyX29yaWdpbmFsUG9zaXRpb25Gb3IoYUFyZ3MpIHtcbiAgICB2YXIgbmVlZGxlID0ge1xuICAgICAgZ2VuZXJhdGVkTGluZTogdXRpbC5nZXRBcmcoYUFyZ3MsICdsaW5lJyksXG4gICAgICBnZW5lcmF0ZWRDb2x1bW46IHV0aWwuZ2V0QXJnKGFBcmdzLCAnY29sdW1uJylcbiAgICB9O1xuXG4gICAgLy8gRmluZCB0aGUgc2VjdGlvbiBjb250YWluaW5nIHRoZSBnZW5lcmF0ZWQgcG9zaXRpb24gd2UncmUgdHJ5aW5nIHRvIG1hcFxuICAgIC8vIHRvIGFuIG9yaWdpbmFsIHBvc2l0aW9uLlxuICAgIHZhciBzZWN0aW9uSW5kZXggPSBiaW5hcnlTZWFyY2guc2VhcmNoKG5lZWRsZSwgdGhpcy5fc2VjdGlvbnMsXG4gICAgICBmdW5jdGlvbihuZWVkbGUsIHNlY3Rpb24pIHtcbiAgICAgICAgdmFyIGNtcCA9IG5lZWRsZS5nZW5lcmF0ZWRMaW5lIC0gc2VjdGlvbi5nZW5lcmF0ZWRPZmZzZXQuZ2VuZXJhdGVkTGluZTtcbiAgICAgICAgaWYgKGNtcCkge1xuICAgICAgICAgIHJldHVybiBjbXA7XG4gICAgICAgIH1cblxuICAgICAgICByZXR1cm4gKG5lZWRsZS5nZW5lcmF0ZWRDb2x1bW4gLVxuICAgICAgICAgICAgICAgIHNlY3Rpb24uZ2VuZXJhdGVkT2Zmc2V0LmdlbmVyYXRlZENvbHVtbik7XG4gICAgICB9KTtcbiAgICB2YXIgc2VjdGlvbiA9IHRoaXMuX3NlY3Rpb25zW3NlY3Rpb25JbmRleF07XG5cbiAgICBpZiAoIXNlY3Rpb24pIHtcbiAgICAgIHJldHVybiB7XG4gICAgICAgIHNvdXJjZTogbnVsbCxcbiAgICAgICAgbGluZTogbnVsbCxcbiAgICAgICAgY29sdW1uOiBudWxsLFxuICAgICAgICBuYW1lOiBudWxsXG4gICAgICB9O1xuICAgIH1cblxuICAgIHJldHVybiBzZWN0aW9uLmNvbnN1bWVyLm9yaWdpbmFsUG9zaXRpb25Gb3Ioe1xuICAgICAgbGluZTogbmVlZGxlLmdlbmVyYXRlZExpbmUgLVxuICAgICAgICAoc2VjdGlvbi5nZW5lcmF0ZWRPZmZzZXQuZ2VuZXJhdGVkTGluZSAtIDEpLFxuICAgICAgY29sdW1uOiBuZWVkbGUuZ2VuZXJhdGVkQ29sdW1uIC1cbiAgICAgICAgKHNlY3Rpb24uZ2VuZXJhdGVkT2Zmc2V0LmdlbmVyYXRlZExpbmUgPT09IG5lZWRsZS5nZW5lcmF0ZWRMaW5lXG4gICAgICAgICA/IHNlY3Rpb24uZ2VuZXJhdGVkT2Zmc2V0LmdlbmVyYXRlZENvbHVtbiAtIDFcbiAgICAgICAgIDogMCksXG4gICAgICBiaWFzOiBhQXJncy5iaWFzXG4gICAgfSk7XG4gIH07XG5cbi8qKlxuICogUmV0dXJuIHRydWUgaWYgd2UgaGF2ZSB0aGUgc291cmNlIGNvbnRlbnQgZm9yIGV2ZXJ5IHNvdXJjZSBpbiB0aGUgc291cmNlXG4gKiBtYXAsIGZhbHNlIG90aGVyd2lzZS5cbiAqL1xuSW5kZXhlZFNvdXJjZU1hcENvbnN1bWVyLnByb3RvdHlwZS5oYXNDb250ZW50c09mQWxsU291cmNlcyA9XG4gIGZ1bmN0aW9uIEluZGV4ZWRTb3VyY2VNYXBDb25zdW1lcl9oYXNDb250ZW50c09mQWxsU291cmNlcygpIHtcbiAgICByZXR1cm4gdGhpcy5fc2VjdGlvbnMuZXZlcnkoZnVuY3Rpb24gKHMpIHtcbiAgICAgIHJldHVybiBzLmNvbnN1bWVyLmhhc0NvbnRlbnRzT2ZBbGxTb3VyY2VzKCk7XG4gICAgfSk7XG4gIH07XG5cbi8qKlxuICogUmV0dXJucyB0aGUgb3JpZ2luYWwgc291cmNlIGNvbnRlbnQuIFRoZSBvbmx5IGFyZ3VtZW50IGlzIHRoZSB1cmwgb2YgdGhlXG4gKiBvcmlnaW5hbCBzb3VyY2UgZmlsZS4gUmV0dXJucyBudWxsIGlmIG5vIG9yaWdpbmFsIHNvdXJjZSBjb250ZW50IGlzXG4gKiBhdmFpbGFibGUuXG4gKi9cbkluZGV4ZWRTb3VyY2VNYXBDb25zdW1lci5wcm90b3R5cGUuc291cmNlQ29udGVudEZvciA9XG4gIGZ1bmN0aW9uIEluZGV4ZWRTb3VyY2VNYXBDb25zdW1lcl9zb3VyY2VDb250ZW50Rm9yKGFTb3VyY2UsIG51bGxPbk1pc3NpbmcpIHtcbiAgICBmb3IgKHZhciBpID0gMDsgaSA8IHRoaXMuX3NlY3Rpb25zLmxlbmd0aDsgaSsrKSB7XG4gICAgICB2YXIgc2VjdGlvbiA9IHRoaXMuX3NlY3Rpb25zW2ldO1xuXG4gICAgICB2YXIgY29udGVudCA9IHNlY3Rpb24uY29uc3VtZXIuc291cmNlQ29udGVudEZvcihhU291cmNlLCB0cnVlKTtcbiAgICAgIGlmIChjb250ZW50KSB7XG4gICAgICAgIHJldHVybiBjb250ZW50O1xuICAgICAgfVxuICAgIH1cbiAgICBpZiAobnVsbE9uTWlzc2luZykge1xuICAgICAgcmV0dXJuIG51bGw7XG4gICAgfVxuICAgIGVsc2Uge1xuICAgICAgdGhyb3cgbmV3IEVycm9yKCdcIicgKyBhU291cmNlICsgJ1wiIGlzIG5vdCBpbiB0aGUgU291cmNlTWFwLicpO1xuICAgIH1cbiAgfTtcblxuLyoqXG4gKiBSZXR1cm5zIHRoZSBnZW5lcmF0ZWQgbGluZSBhbmQgY29sdW1uIGluZm9ybWF0aW9uIGZvciB0aGUgb3JpZ2luYWwgc291cmNlLFxuICogbGluZSwgYW5kIGNvbHVtbiBwb3NpdGlvbnMgcHJvdmlkZWQuIFRoZSBvbmx5IGFyZ3VtZW50IGlzIGFuIG9iamVjdCB3aXRoXG4gKiB0aGUgZm9sbG93aW5nIHByb3BlcnRpZXM6XG4gKlxuICogICAtIHNvdXJjZTogVGhlIGZpbGVuYW1lIG9mIHRoZSBvcmlnaW5hbCBzb3VyY2UuXG4gKiAgIC0gbGluZTogVGhlIGxpbmUgbnVtYmVyIGluIHRoZSBvcmlnaW5hbCBzb3VyY2UuXG4gKiAgIC0gY29sdW1uOiBUaGUgY29sdW1uIG51bWJlciBpbiB0aGUgb3JpZ2luYWwgc291cmNlLlxuICpcbiAqIGFuZCBhbiBvYmplY3QgaXMgcmV0dXJuZWQgd2l0aCB0aGUgZm9sbG93aW5nIHByb3BlcnRpZXM6XG4gKlxuICogICAtIGxpbmU6IFRoZSBsaW5lIG51bWJlciBpbiB0aGUgZ2VuZXJhdGVkIHNvdXJjZSwgb3IgbnVsbC5cbiAqICAgLSBjb2x1bW46IFRoZSBjb2x1bW4gbnVtYmVyIGluIHRoZSBnZW5lcmF0ZWQgc291cmNlLCBvciBudWxsLlxuICovXG5JbmRleGVkU291cmNlTWFwQ29uc3VtZXIucHJvdG90eXBlLmdlbmVyYXRlZFBvc2l0aW9uRm9yID1cbiAgZnVuY3Rpb24gSW5kZXhlZFNvdXJjZU1hcENvbnN1bWVyX2dlbmVyYXRlZFBvc2l0aW9uRm9yKGFBcmdzKSB7XG4gICAgZm9yICh2YXIgaSA9IDA7IGkgPCB0aGlzLl9zZWN0aW9ucy5sZW5ndGg7IGkrKykge1xuICAgICAgdmFyIHNlY3Rpb24gPSB0aGlzLl9zZWN0aW9uc1tpXTtcblxuICAgICAgLy8gT25seSBjb25zaWRlciB0aGlzIHNlY3Rpb24gaWYgdGhlIHJlcXVlc3RlZCBzb3VyY2UgaXMgaW4gdGhlIGxpc3Qgb2ZcbiAgICAgIC8vIHNvdXJjZXMgb2YgdGhlIGNvbnN1bWVyLlxuICAgICAgaWYgKHNlY3Rpb24uY29uc3VtZXIuc291cmNlcy5pbmRleE9mKHV0aWwuZ2V0QXJnKGFBcmdzLCAnc291cmNlJykpID09PSAtMSkge1xuICAgICAgICBjb250aW51ZTtcbiAgICAgIH1cbiAgICAgIHZhciBnZW5lcmF0ZWRQb3NpdGlvbiA9IHNlY3Rpb24uY29uc3VtZXIuZ2VuZXJhdGVkUG9zaXRpb25Gb3IoYUFyZ3MpO1xuICAgICAgaWYgKGdlbmVyYXRlZFBvc2l0aW9uKSB7XG4gICAgICAgIHZhciByZXQgPSB7XG4gICAgICAgICAgbGluZTogZ2VuZXJhdGVkUG9zaXRpb24ubGluZSArXG4gICAgICAgICAgICAoc2VjdGlvbi5nZW5lcmF0ZWRPZmZzZXQuZ2VuZXJhdGVkTGluZSAtIDEpLFxuICAgICAgICAgIGNvbHVtbjogZ2VuZXJhdGVkUG9zaXRpb24uY29sdW1uICtcbiAgICAgICAgICAgIChzZWN0aW9uLmdlbmVyYXRlZE9mZnNldC5nZW5lcmF0ZWRMaW5lID09PSBnZW5lcmF0ZWRQb3NpdGlvbi5saW5lXG4gICAgICAgICAgICAgPyBzZWN0aW9uLmdlbmVyYXRlZE9mZnNldC5nZW5lcmF0ZWRDb2x1bW4gLSAxXG4gICAgICAgICAgICAgOiAwKVxuICAgICAgICB9O1xuICAgICAgICByZXR1cm4gcmV0O1xuICAgICAgfVxuICAgIH1cblxuICAgIHJldHVybiB7XG4gICAgICBsaW5lOiBudWxsLFxuICAgICAgY29sdW1uOiBudWxsXG4gICAgfTtcbiAgfTtcblxuLyoqXG4gKiBQYXJzZSB0aGUgbWFwcGluZ3MgaW4gYSBzdHJpbmcgaW4gdG8gYSBkYXRhIHN0cnVjdHVyZSB3aGljaCB3ZSBjYW4gZWFzaWx5XG4gKiBxdWVyeSAodGhlIG9yZGVyZWQgYXJyYXlzIGluIHRoZSBgdGhpcy5fX2dlbmVyYXRlZE1hcHBpbmdzYCBhbmRcbiAqIGB0aGlzLl9fb3JpZ2luYWxNYXBwaW5nc2AgcHJvcGVydGllcykuXG4gKi9cbkluZGV4ZWRTb3VyY2VNYXBDb25zdW1lci5wcm90b3R5cGUuX3BhcnNlTWFwcGluZ3MgPVxuICBmdW5jdGlvbiBJbmRleGVkU291cmNlTWFwQ29uc3VtZXJfcGFyc2VNYXBwaW5ncyhhU3RyLCBhU291cmNlUm9vdCkge1xuICAgIHRoaXMuX19nZW5lcmF0ZWRNYXBwaW5ncyA9IFtdO1xuICAgIHRoaXMuX19vcmlnaW5hbE1hcHBpbmdzID0gW107XG4gICAgZm9yICh2YXIgaSA9IDA7IGkgPCB0aGlzLl9zZWN0aW9ucy5sZW5ndGg7IGkrKykge1xuICAgICAgdmFyIHNlY3Rpb24gPSB0aGlzLl9zZWN0aW9uc1tpXTtcbiAgICAgIHZhciBzZWN0aW9uTWFwcGluZ3MgPSBzZWN0aW9uLmNvbnN1bWVyLl9nZW5lcmF0ZWRNYXBwaW5ncztcbiAgICAgIGZvciAodmFyIGogPSAwOyBqIDwgc2VjdGlvbk1hcHBpbmdzLmxlbmd0aDsgaisrKSB7XG4gICAgICAgIHZhciBtYXBwaW5nID0gc2VjdGlvbk1hcHBpbmdzW2pdO1xuXG4gICAgICAgIHZhciBzb3VyY2UgPSBzZWN0aW9uLmNvbnN1bWVyLl9zb3VyY2VzLmF0KG1hcHBpbmcuc291cmNlKTtcbiAgICAgICAgaWYgKHNlY3Rpb24uY29uc3VtZXIuc291cmNlUm9vdCAhPT0gbnVsbCkge1xuICAgICAgICAgIHNvdXJjZSA9IHV0aWwuam9pbihzZWN0aW9uLmNvbnN1bWVyLnNvdXJjZVJvb3QsIHNvdXJjZSk7XG4gICAgICAgIH1cbiAgICAgICAgdGhpcy5fc291cmNlcy5hZGQoc291cmNlKTtcbiAgICAgICAgc291cmNlID0gdGhpcy5fc291cmNlcy5pbmRleE9mKHNvdXJjZSk7XG5cbiAgICAgICAgdmFyIG5hbWUgPSBzZWN0aW9uLmNvbnN1bWVyLl9uYW1lcy5hdChtYXBwaW5nLm5hbWUpO1xuICAgICAgICB0aGlzLl9uYW1lcy5hZGQobmFtZSk7XG4gICAgICAgIG5hbWUgPSB0aGlzLl9uYW1lcy5pbmRleE9mKG5hbWUpO1xuXG4gICAgICAgIC8vIFRoZSBtYXBwaW5ncyBjb21pbmcgZnJvbSB0aGUgY29uc3VtZXIgZm9yIHRoZSBzZWN0aW9uIGhhdmVcbiAgICAgICAgLy8gZ2VuZXJhdGVkIHBvc2l0aW9ucyByZWxhdGl2ZSB0byB0aGUgc3RhcnQgb2YgdGhlIHNlY3Rpb24sIHNvIHdlXG4gICAgICAgIC8vIG5lZWQgdG8gb2Zmc2V0IHRoZW0gdG8gYmUgcmVsYXRpdmUgdG8gdGhlIHN0YXJ0IG9mIHRoZSBjb25jYXRlbmF0ZWRcbiAgICAgICAgLy8gZ2VuZXJhdGVkIGZpbGUuXG4gICAgICAgIHZhciBhZGp1c3RlZE1hcHBpbmcgPSB7XG4gICAgICAgICAgc291cmNlOiBzb3VyY2UsXG4gICAgICAgICAgZ2VuZXJhdGVkTGluZTogbWFwcGluZy5nZW5lcmF0ZWRMaW5lICtcbiAgICAgICAgICAgIChzZWN0aW9uLmdlbmVyYXRlZE9mZnNldC5nZW5lcmF0ZWRMaW5lIC0gMSksXG4gICAgICAgICAgZ2VuZXJhdGVkQ29sdW1uOiBtYXBwaW5nLmdlbmVyYXRlZENvbHVtbiArXG4gICAgICAgICAgICAoc2VjdGlvbi5nZW5lcmF0ZWRPZmZzZXQuZ2VuZXJhdGVkTGluZSA9PT0gbWFwcGluZy5nZW5lcmF0ZWRMaW5lXG4gICAgICAgICAgICA/IHNlY3Rpb24uZ2VuZXJhdGVkT2Zmc2V0LmdlbmVyYXRlZENvbHVtbiAtIDFcbiAgICAgICAgICAgIDogMCksXG4gICAgICAgICAgb3JpZ2luYWxMaW5lOiBtYXBwaW5nLm9yaWdpbmFsTGluZSxcbiAgICAgICAgICBvcmlnaW5hbENvbHVtbjogbWFwcGluZy5vcmlnaW5hbENvbHVtbixcbiAgICAgICAgICBuYW1lOiBuYW1lXG4gICAgICAgIH07XG5cbiAgICAgICAgdGhpcy5fX2dlbmVyYXRlZE1hcHBpbmdzLnB1c2goYWRqdXN0ZWRNYXBwaW5nKTtcbiAgICAgICAgaWYgKHR5cGVvZiBhZGp1c3RlZE1hcHBpbmcub3JpZ2luYWxMaW5lID09PSAnbnVtYmVyJykge1xuICAgICAgICAgIHRoaXMuX19vcmlnaW5hbE1hcHBpbmdzLnB1c2goYWRqdXN0ZWRNYXBwaW5nKTtcbiAgICAgICAgfVxuICAgICAgfVxuICAgIH1cblxuICAgIHF1aWNrU29ydCh0aGlzLl9fZ2VuZXJhdGVkTWFwcGluZ3MsIHV0aWwuY29tcGFyZUJ5R2VuZXJhdGVkUG9zaXRpb25zRGVmbGF0ZWQpO1xuICAgIHF1aWNrU29ydCh0aGlzLl9fb3JpZ2luYWxNYXBwaW5ncywgdXRpbC5jb21wYXJlQnlPcmlnaW5hbFBvc2l0aW9ucyk7XG4gIH07XG5cbmV4cG9ydHMuSW5kZXhlZFNvdXJjZU1hcENvbnN1bWVyID0gSW5kZXhlZFNvdXJjZU1hcENvbnN1bWVyO1xuXG5cblxuLyoqKioqKioqKioqKioqKioqXG4gKiogV0VCUEFDSyBGT09URVJcbiAqKiAuL2xpYi9zb3VyY2UtbWFwLWNvbnN1bWVyLmpzXG4gKiogbW9kdWxlIGlkID0gN1xuICoqIG1vZHVsZSBjaHVua3MgPSAwXG4gKiovIiwiLyogLSotIE1vZGU6IGpzOyBqcy1pbmRlbnQtbGV2ZWw6IDI7IC0qLSAqL1xuLypcbiAqIENvcHlyaWdodCAyMDExIE1vemlsbGEgRm91bmRhdGlvbiBhbmQgY29udHJpYnV0b3JzXG4gKiBMaWNlbnNlZCB1bmRlciB0aGUgTmV3IEJTRCBsaWNlbnNlLiBTZWUgTElDRU5TRSBvcjpcbiAqIGh0dHA6Ly9vcGVuc291cmNlLm9yZy9saWNlbnNlcy9CU0QtMy1DbGF1c2VcbiAqL1xuXG5leHBvcnRzLkdSRUFURVNUX0xPV0VSX0JPVU5EID0gMTtcbmV4cG9ydHMuTEVBU1RfVVBQRVJfQk9VTkQgPSAyO1xuXG4vKipcbiAqIFJlY3Vyc2l2ZSBpbXBsZW1lbnRhdGlvbiBvZiBiaW5hcnkgc2VhcmNoLlxuICpcbiAqIEBwYXJhbSBhTG93IEluZGljZXMgaGVyZSBhbmQgbG93ZXIgZG8gbm90IGNvbnRhaW4gdGhlIG5lZWRsZS5cbiAqIEBwYXJhbSBhSGlnaCBJbmRpY2VzIGhlcmUgYW5kIGhpZ2hlciBkbyBub3QgY29udGFpbiB0aGUgbmVlZGxlLlxuICogQHBhcmFtIGFOZWVkbGUgVGhlIGVsZW1lbnQgYmVpbmcgc2VhcmNoZWQgZm9yLlxuICogQHBhcmFtIGFIYXlzdGFjayBUaGUgbm9uLWVtcHR5IGFycmF5IGJlaW5nIHNlYXJjaGVkLlxuICogQHBhcmFtIGFDb21wYXJlIEZ1bmN0aW9uIHdoaWNoIHRha2VzIHR3byBlbGVtZW50cyBhbmQgcmV0dXJucyAtMSwgMCwgb3IgMS5cbiAqIEBwYXJhbSBhQmlhcyBFaXRoZXIgJ2JpbmFyeVNlYXJjaC5HUkVBVEVTVF9MT1dFUl9CT1VORCcgb3JcbiAqICAgICAnYmluYXJ5U2VhcmNoLkxFQVNUX1VQUEVSX0JPVU5EJy4gU3BlY2lmaWVzIHdoZXRoZXIgdG8gcmV0dXJuIHRoZVxuICogICAgIGNsb3Nlc3QgZWxlbWVudCB0aGF0IGlzIHNtYWxsZXIgdGhhbiBvciBncmVhdGVyIHRoYW4gdGhlIG9uZSB3ZSBhcmVcbiAqICAgICBzZWFyY2hpbmcgZm9yLCByZXNwZWN0aXZlbHksIGlmIHRoZSBleGFjdCBlbGVtZW50IGNhbm5vdCBiZSBmb3VuZC5cbiAqL1xuZnVuY3Rpb24gcmVjdXJzaXZlU2VhcmNoKGFMb3csIGFIaWdoLCBhTmVlZGxlLCBhSGF5c3RhY2ssIGFDb21wYXJlLCBhQmlhcykge1xuICAvLyBUaGlzIGZ1bmN0aW9uIHRlcm1pbmF0ZXMgd2hlbiBvbmUgb2YgdGhlIGZvbGxvd2luZyBpcyB0cnVlOlxuICAvL1xuICAvLyAgIDEuIFdlIGZpbmQgdGhlIGV4YWN0IGVsZW1lbnQgd2UgYXJlIGxvb2tpbmcgZm9yLlxuICAvL1xuICAvLyAgIDIuIFdlIGRpZCBub3QgZmluZCB0aGUgZXhhY3QgZWxlbWVudCwgYnV0IHdlIGNhbiByZXR1cm4gdGhlIGluZGV4IG9mXG4gIC8vICAgICAgdGhlIG5leHQtY2xvc2VzdCBlbGVtZW50LlxuICAvL1xuICAvLyAgIDMuIFdlIGRpZCBub3QgZmluZCB0aGUgZXhhY3QgZWxlbWVudCwgYW5kIHRoZXJlIGlzIG5vIG5leHQtY2xvc2VzdFxuICAvLyAgICAgIGVsZW1lbnQgdGhhbiB0aGUgb25lIHdlIGFyZSBzZWFyY2hpbmcgZm9yLCBzbyB3ZSByZXR1cm4gLTEuXG4gIHZhciBtaWQgPSBNYXRoLmZsb29yKChhSGlnaCAtIGFMb3cpIC8gMikgKyBhTG93O1xuICB2YXIgY21wID0gYUNvbXBhcmUoYU5lZWRsZSwgYUhheXN0YWNrW21pZF0sIHRydWUpO1xuICBpZiAoY21wID09PSAwKSB7XG4gICAgLy8gRm91bmQgdGhlIGVsZW1lbnQgd2UgYXJlIGxvb2tpbmcgZm9yLlxuICAgIHJldHVybiBtaWQ7XG4gIH1cbiAgZWxzZSBpZiAoY21wID4gMCkge1xuICAgIC8vIE91ciBuZWVkbGUgaXMgZ3JlYXRlciB0aGFuIGFIYXlzdGFja1ttaWRdLlxuICAgIGlmIChhSGlnaCAtIG1pZCA+IDEpIHtcbiAgICAgIC8vIFRoZSBlbGVtZW50IGlzIGluIHRoZSB1cHBlciBoYWxmLlxuICAgICAgcmV0dXJuIHJlY3Vyc2l2ZVNlYXJjaChtaWQsIGFIaWdoLCBhTmVlZGxlLCBhSGF5c3RhY2ssIGFDb21wYXJlLCBhQmlhcyk7XG4gICAgfVxuXG4gICAgLy8gVGhlIGV4YWN0IG5lZWRsZSBlbGVtZW50IHdhcyBub3QgZm91bmQgaW4gdGhpcyBoYXlzdGFjay4gRGV0ZXJtaW5lIGlmXG4gICAgLy8gd2UgYXJlIGluIHRlcm1pbmF0aW9uIGNhc2UgKDMpIG9yICgyKSBhbmQgcmV0dXJuIHRoZSBhcHByb3ByaWF0ZSB0aGluZy5cbiAgICBpZiAoYUJpYXMgPT0gZXhwb3J0cy5MRUFTVF9VUFBFUl9CT1VORCkge1xuICAgICAgcmV0dXJuIGFIaWdoIDwgYUhheXN0YWNrLmxlbmd0aCA/IGFIaWdoIDogLTE7XG4gICAgfSBlbHNlIHtcbiAgICAgIHJldHVybiBtaWQ7XG4gICAgfVxuICB9XG4gIGVsc2Uge1xuICAgIC8vIE91ciBuZWVkbGUgaXMgbGVzcyB0aGFuIGFIYXlzdGFja1ttaWRdLlxuICAgIGlmIChtaWQgLSBhTG93ID4gMSkge1xuICAgICAgLy8gVGhlIGVsZW1lbnQgaXMgaW4gdGhlIGxvd2VyIGhhbGYuXG4gICAgICByZXR1cm4gcmVjdXJzaXZlU2VhcmNoKGFMb3csIG1pZCwgYU5lZWRsZSwgYUhheXN0YWNrLCBhQ29tcGFyZSwgYUJpYXMpO1xuICAgIH1cblxuICAgIC8vIHdlIGFyZSBpbiB0ZXJtaW5hdGlvbiBjYXNlICgzKSBvciAoMikgYW5kIHJldHVybiB0aGUgYXBwcm9wcmlhdGUgdGhpbmcuXG4gICAgaWYgKGFCaWFzID09IGV4cG9ydHMuTEVBU1RfVVBQRVJfQk9VTkQpIHtcbiAgICAgIHJldHVybiBtaWQ7XG4gICAgfSBlbHNlIHtcbiAgICAgIHJldHVybiBhTG93IDwgMCA/IC0xIDogYUxvdztcbiAgICB9XG4gIH1cbn1cblxuLyoqXG4gKiBUaGlzIGlzIGFuIGltcGxlbWVudGF0aW9uIG9mIGJpbmFyeSBzZWFyY2ggd2hpY2ggd2lsbCBhbHdheXMgdHJ5IGFuZCByZXR1cm5cbiAqIHRoZSBpbmRleCBvZiB0aGUgY2xvc2VzdCBlbGVtZW50IGlmIHRoZXJlIGlzIG5vIGV4YWN0IGhpdC4gVGhpcyBpcyBiZWNhdXNlXG4gKiBtYXBwaW5ncyBiZXR3ZWVuIG9yaWdpbmFsIGFuZCBnZW5lcmF0ZWQgbGluZS9jb2wgcGFpcnMgYXJlIHNpbmdsZSBwb2ludHMsXG4gKiBhbmQgdGhlcmUgaXMgYW4gaW1wbGljaXQgcmVnaW9uIGJldHdlZW4gZWFjaCBvZiB0aGVtLCBzbyBhIG1pc3MganVzdCBtZWFuc1xuICogdGhhdCB5b3UgYXJlbid0IG9uIHRoZSB2ZXJ5IHN0YXJ0IG9mIGEgcmVnaW9uLlxuICpcbiAqIEBwYXJhbSBhTmVlZGxlIFRoZSBlbGVtZW50IHlvdSBhcmUgbG9va2luZyBmb3IuXG4gKiBAcGFyYW0gYUhheXN0YWNrIFRoZSBhcnJheSB0aGF0IGlzIGJlaW5nIHNlYXJjaGVkLlxuICogQHBhcmFtIGFDb21wYXJlIEEgZnVuY3Rpb24gd2hpY2ggdGFrZXMgdGhlIG5lZWRsZSBhbmQgYW4gZWxlbWVudCBpbiB0aGVcbiAqICAgICBhcnJheSBhbmQgcmV0dXJucyAtMSwgMCwgb3IgMSBkZXBlbmRpbmcgb24gd2hldGhlciB0aGUgbmVlZGxlIGlzIGxlc3NcbiAqICAgICB0aGFuLCBlcXVhbCB0bywgb3IgZ3JlYXRlciB0aGFuIHRoZSBlbGVtZW50LCByZXNwZWN0aXZlbHkuXG4gKiBAcGFyYW0gYUJpYXMgRWl0aGVyICdiaW5hcnlTZWFyY2guR1JFQVRFU1RfTE9XRVJfQk9VTkQnIG9yXG4gKiAgICAgJ2JpbmFyeVNlYXJjaC5MRUFTVF9VUFBFUl9CT1VORCcuIFNwZWNpZmllcyB3aGV0aGVyIHRvIHJldHVybiB0aGVcbiAqICAgICBjbG9zZXN0IGVsZW1lbnQgdGhhdCBpcyBzbWFsbGVyIHRoYW4gb3IgZ3JlYXRlciB0aGFuIHRoZSBvbmUgd2UgYXJlXG4gKiAgICAgc2VhcmNoaW5nIGZvciwgcmVzcGVjdGl2ZWx5LCBpZiB0aGUgZXhhY3QgZWxlbWVudCBjYW5ub3QgYmUgZm91bmQuXG4gKiAgICAgRGVmYXVsdHMgdG8gJ2JpbmFyeVNlYXJjaC5HUkVBVEVTVF9MT1dFUl9CT1VORCcuXG4gKi9cbmV4cG9ydHMuc2VhcmNoID0gZnVuY3Rpb24gc2VhcmNoKGFOZWVkbGUsIGFIYXlzdGFjaywgYUNvbXBhcmUsIGFCaWFzKSB7XG4gIGlmIChhSGF5c3RhY2subGVuZ3RoID09PSAwKSB7XG4gICAgcmV0dXJuIC0xO1xuICB9XG5cbiAgdmFyIGluZGV4ID0gcmVjdXJzaXZlU2VhcmNoKC0xLCBhSGF5c3RhY2subGVuZ3RoLCBhTmVlZGxlLCBhSGF5c3RhY2ssXG4gICAgICAgICAgICAgICAgICAgICAgICAgICAgICBhQ29tcGFyZSwgYUJpYXMgfHwgZXhwb3J0cy5HUkVBVEVTVF9MT1dFUl9CT1VORCk7XG4gIGlmIChpbmRleCA8IDApIHtcbiAgICByZXR1cm4gLTE7XG4gIH1cblxuICAvLyBXZSBoYXZlIGZvdW5kIGVpdGhlciB0aGUgZXhhY3QgZWxlbWVudCwgb3IgdGhlIG5leHQtY2xvc2VzdCBlbGVtZW50IHRoYW5cbiAgLy8gdGhlIG9uZSB3ZSBhcmUgc2VhcmNoaW5nIGZvci4gSG93ZXZlciwgdGhlcmUgbWF5IGJlIG1vcmUgdGhhbiBvbmUgc3VjaFxuICAvLyBlbGVtZW50LiBNYWtlIHN1cmUgd2UgYWx3YXlzIHJldHVybiB0aGUgc21hbGxlc3Qgb2YgdGhlc2UuXG4gIHdoaWxlIChpbmRleCAtIDEgPj0gMCkge1xuICAgIGlmIChhQ29tcGFyZShhSGF5c3RhY2tbaW5kZXhdLCBhSGF5c3RhY2tbaW5kZXggLSAxXSwgdHJ1ZSkgIT09IDApIHtcbiAgICAgIGJyZWFrO1xuICAgIH1cbiAgICAtLWluZGV4O1xuICB9XG5cbiAgcmV0dXJuIGluZGV4O1xufTtcblxuXG5cbi8qKioqKioqKioqKioqKioqKlxuICoqIFdFQlBBQ0sgRk9PVEVSXG4gKiogLi9saWIvYmluYXJ5LXNlYXJjaC5qc1xuICoqIG1vZHVsZSBpZCA9IDhcbiAqKiBtb2R1bGUgY2h1bmtzID0gMFxuICoqLyIsIi8qIC0qLSBNb2RlOiBqczsganMtaW5kZW50LWxldmVsOiAyOyAtKi0gKi9cbi8qXG4gKiBDb3B5cmlnaHQgMjAxMSBNb3ppbGxhIEZvdW5kYXRpb24gYW5kIGNvbnRyaWJ1dG9yc1xuICogTGljZW5zZWQgdW5kZXIgdGhlIE5ldyBCU0QgbGljZW5zZS4gU2VlIExJQ0VOU0Ugb3I6XG4gKiBodHRwOi8vb3BlbnNvdXJjZS5vcmcvbGljZW5zZXMvQlNELTMtQ2xhdXNlXG4gKi9cblxuLy8gSXQgdHVybnMgb3V0IHRoYXQgc29tZSAobW9zdD8pIEphdmFTY3JpcHQgZW5naW5lcyBkb24ndCBzZWxmLWhvc3Rcbi8vIGBBcnJheS5wcm90b3R5cGUuc29ydGAuIFRoaXMgbWFrZXMgc2Vuc2UgYmVjYXVzZSBDKysgd2lsbCBsaWtlbHkgcmVtYWluXG4vLyBmYXN0ZXIgdGhhbiBKUyB3aGVuIGRvaW5nIHJhdyBDUFUtaW50ZW5zaXZlIHNvcnRpbmcuIEhvd2V2ZXIsIHdoZW4gdXNpbmcgYVxuLy8gY3VzdG9tIGNvbXBhcmF0b3IgZnVuY3Rpb24sIGNhbGxpbmcgYmFjayBhbmQgZm9ydGggYmV0d2VlbiB0aGUgVk0ncyBDKysgYW5kXG4vLyBKSVQnZCBKUyBpcyByYXRoZXIgc2xvdyAqYW5kKiBsb3NlcyBKSVQgdHlwZSBpbmZvcm1hdGlvbiwgcmVzdWx0aW5nIGluXG4vLyB3b3JzZSBnZW5lcmF0ZWQgY29kZSBmb3IgdGhlIGNvbXBhcmF0b3IgZnVuY3Rpb24gdGhhbiB3b3VsZCBiZSBvcHRpbWFsLiBJblxuLy8gZmFjdCwgd2hlbiBzb3J0aW5nIHdpdGggYSBjb21wYXJhdG9yLCB0aGVzZSBjb3N0cyBvdXR3ZWlnaCB0aGUgYmVuZWZpdHMgb2Zcbi8vIHNvcnRpbmcgaW4gQysrLiBCeSB1c2luZyBvdXIgb3duIEpTLWltcGxlbWVudGVkIFF1aWNrIFNvcnQgKGJlbG93KSwgd2UgZ2V0XG4vLyBhIH4zNTAwbXMgbWVhbiBzcGVlZC11cCBpbiBgYmVuY2gvYmVuY2guaHRtbGAuXG5cbi8qKlxuICogU3dhcCB0aGUgZWxlbWVudHMgaW5kZXhlZCBieSBgeGAgYW5kIGB5YCBpbiB0aGUgYXJyYXkgYGFyeWAuXG4gKlxuICogQHBhcmFtIHtBcnJheX0gYXJ5XG4gKiAgICAgICAgVGhlIGFycmF5LlxuICogQHBhcmFtIHtOdW1iZXJ9IHhcbiAqICAgICAgICBUaGUgaW5kZXggb2YgdGhlIGZpcnN0IGl0ZW0uXG4gKiBAcGFyYW0ge051bWJlcn0geVxuICogICAgICAgIFRoZSBpbmRleCBvZiB0aGUgc2Vjb25kIGl0ZW0uXG4gKi9cbmZ1bmN0aW9uIHN3YXAoYXJ5LCB4LCB5KSB7XG4gIHZhciB0ZW1wID0gYXJ5W3hdO1xuICBhcnlbeF0gPSBhcnlbeV07XG4gIGFyeVt5XSA9IHRlbXA7XG59XG5cbi8qKlxuICogUmV0dXJucyBhIHJhbmRvbSBpbnRlZ2VyIHdpdGhpbiB0aGUgcmFuZ2UgYGxvdyAuLiBoaWdoYCBpbmNsdXNpdmUuXG4gKlxuICogQHBhcmFtIHtOdW1iZXJ9IGxvd1xuICogICAgICAgIFRoZSBsb3dlciBib3VuZCBvbiB0aGUgcmFuZ2UuXG4gKiBAcGFyYW0ge051bWJlcn0gaGlnaFxuICogICAgICAgIFRoZSB1cHBlciBib3VuZCBvbiB0aGUgcmFuZ2UuXG4gKi9cbmZ1bmN0aW9uIHJhbmRvbUludEluUmFuZ2UobG93LCBoaWdoKSB7XG4gIHJldHVybiBNYXRoLnJvdW5kKGxvdyArIChNYXRoLnJhbmRvbSgpICogKGhpZ2ggLSBsb3cpKSk7XG59XG5cbi8qKlxuICogVGhlIFF1aWNrIFNvcnQgYWxnb3JpdGhtLlxuICpcbiAqIEBwYXJhbSB7QXJyYXl9IGFyeVxuICogICAgICAgIEFuIGFycmF5IHRvIHNvcnQuXG4gKiBAcGFyYW0ge2Z1bmN0aW9ufSBjb21wYXJhdG9yXG4gKiAgICAgICAgRnVuY3Rpb24gdG8gdXNlIHRvIGNvbXBhcmUgdHdvIGl0ZW1zLlxuICogQHBhcmFtIHtOdW1iZXJ9IHBcbiAqICAgICAgICBTdGFydCBpbmRleCBvZiB0aGUgYXJyYXlcbiAqIEBwYXJhbSB7TnVtYmVyfSByXG4gKiAgICAgICAgRW5kIGluZGV4IG9mIHRoZSBhcnJheVxuICovXG5mdW5jdGlvbiBkb1F1aWNrU29ydChhcnksIGNvbXBhcmF0b3IsIHAsIHIpIHtcbiAgLy8gSWYgb3VyIGxvd2VyIGJvdW5kIGlzIGxlc3MgdGhhbiBvdXIgdXBwZXIgYm91bmQsIHdlICgxKSBwYXJ0aXRpb24gdGhlXG4gIC8vIGFycmF5IGludG8gdHdvIHBpZWNlcyBhbmQgKDIpIHJlY3Vyc2Ugb24gZWFjaCBoYWxmLiBJZiBpdCBpcyBub3QsIHRoaXMgaXNcbiAgLy8gdGhlIGVtcHR5IGFycmF5IGFuZCBvdXIgYmFzZSBjYXNlLlxuXG4gIGlmIChwIDwgcikge1xuICAgIC8vICgxKSBQYXJ0aXRpb25pbmcuXG4gICAgLy9cbiAgICAvLyBUaGUgcGFydGl0aW9uaW5nIGNob29zZXMgYSBwaXZvdCBiZXR3ZWVuIGBwYCBhbmQgYHJgIGFuZCBtb3ZlcyBhbGxcbiAgICAvLyBlbGVtZW50cyB0aGF0IGFyZSBsZXNzIHRoYW4gb3IgZXF1YWwgdG8gdGhlIHBpdm90IHRvIHRoZSBiZWZvcmUgaXQsIGFuZFxuICAgIC8vIGFsbCB0aGUgZWxlbWVudHMgdGhhdCBhcmUgZ3JlYXRlciB0aGFuIGl0IGFmdGVyIGl0LiBUaGUgZWZmZWN0IGlzIHRoYXRcbiAgICAvLyBvbmNlIHBhcnRpdGlvbiBpcyBkb25lLCB0aGUgcGl2b3QgaXMgaW4gdGhlIGV4YWN0IHBsYWNlIGl0IHdpbGwgYmUgd2hlblxuICAgIC8vIHRoZSBhcnJheSBpcyBwdXQgaW4gc29ydGVkIG9yZGVyLCBhbmQgaXQgd2lsbCBub3QgbmVlZCB0byBiZSBtb3ZlZFxuICAgIC8vIGFnYWluLiBUaGlzIHJ1bnMgaW4gTyhuKSB0aW1lLlxuXG4gICAgLy8gQWx3YXlzIGNob29zZSBhIHJhbmRvbSBwaXZvdCBzbyB0aGF0IGFuIGlucHV0IGFycmF5IHdoaWNoIGlzIHJldmVyc2VcbiAgICAvLyBzb3J0ZWQgZG9lcyBub3QgY2F1c2UgTyhuXjIpIHJ1bm5pbmcgdGltZS5cbiAgICB2YXIgcGl2b3RJbmRleCA9IHJhbmRvbUludEluUmFuZ2UocCwgcik7XG4gICAgdmFyIGkgPSBwIC0gMTtcblxuICAgIHN3YXAoYXJ5LCBwaXZvdEluZGV4LCByKTtcbiAgICB2YXIgcGl2b3QgPSBhcnlbcl07XG5cbiAgICAvLyBJbW1lZGlhdGVseSBhZnRlciBgamAgaXMgaW5jcmVtZW50ZWQgaW4gdGhpcyBsb29wLCB0aGUgZm9sbG93aW5nIGhvbGRcbiAgICAvLyB0cnVlOlxuICAgIC8vXG4gICAgLy8gICAqIEV2ZXJ5IGVsZW1lbnQgaW4gYGFyeVtwIC4uIGldYCBpcyBsZXNzIHRoYW4gb3IgZXF1YWwgdG8gdGhlIHBpdm90LlxuICAgIC8vXG4gICAgLy8gICAqIEV2ZXJ5IGVsZW1lbnQgaW4gYGFyeVtpKzEgLi4gai0xXWAgaXMgZ3JlYXRlciB0aGFuIHRoZSBwaXZvdC5cbiAgICBmb3IgKHZhciBqID0gcDsgaiA8IHI7IGorKykge1xuICAgICAgaWYgKGNvbXBhcmF0b3IoYXJ5W2pdLCBwaXZvdCkgPD0gMCkge1xuICAgICAgICBpICs9IDE7XG4gICAgICAgIHN3YXAoYXJ5LCBpLCBqKTtcbiAgICAgIH1cbiAgICB9XG5cbiAgICBzd2FwKGFyeSwgaSArIDEsIGopO1xuICAgIHZhciBxID0gaSArIDE7XG5cbiAgICAvLyAoMikgUmVjdXJzZSBvbiBlYWNoIGhhbGYuXG5cbiAgICBkb1F1aWNrU29ydChhcnksIGNvbXBhcmF0b3IsIHAsIHEgLSAxKTtcbiAgICBkb1F1aWNrU29ydChhcnksIGNvbXBhcmF0b3IsIHEgKyAxLCByKTtcbiAgfVxufVxuXG4vKipcbiAqIFNvcnQgdGhlIGdpdmVuIGFycmF5IGluLXBsYWNlIHdpdGggdGhlIGdpdmVuIGNvbXBhcmF0b3IgZnVuY3Rpb24uXG4gKlxuICogQHBhcmFtIHtBcnJheX0gYXJ5XG4gKiAgICAgICAgQW4gYXJyYXkgdG8gc29ydC5cbiAqIEBwYXJhbSB7ZnVuY3Rpb259IGNvbXBhcmF0b3JcbiAqICAgICAgICBGdW5jdGlvbiB0byB1c2UgdG8gY29tcGFyZSB0d28gaXRlbXMuXG4gKi9cbmV4cG9ydHMucXVpY2tTb3J0ID0gZnVuY3Rpb24gKGFyeSwgY29tcGFyYXRvcikge1xuICBkb1F1aWNrU29ydChhcnksIGNvbXBhcmF0b3IsIDAsIGFyeS5sZW5ndGggLSAxKTtcbn07XG5cblxuXG4vKioqKioqKioqKioqKioqKipcbiAqKiBXRUJQQUNLIEZPT1RFUlxuICoqIC4vbGliL3F1aWNrLXNvcnQuanNcbiAqKiBtb2R1bGUgaWQgPSA5XG4gKiogbW9kdWxlIGNodW5rcyA9IDBcbiAqKi8iLCIvKiAtKi0gTW9kZToganM7IGpzLWluZGVudC1sZXZlbDogMjsgLSotICovXG4vKlxuICogQ29weXJpZ2h0IDIwMTEgTW96aWxsYSBGb3VuZGF0aW9uIGFuZCBjb250cmlidXRvcnNcbiAqIExpY2Vuc2VkIHVuZGVyIHRoZSBOZXcgQlNEIGxpY2Vuc2UuIFNlZSBMSUNFTlNFIG9yOlxuICogaHR0cDovL29wZW5zb3VyY2Uub3JnL2xpY2Vuc2VzL0JTRC0zLUNsYXVzZVxuICovXG5cbnZhciBTb3VyY2VNYXBHZW5lcmF0b3IgPSByZXF1aXJlKCcuL3NvdXJjZS1tYXAtZ2VuZXJhdG9yJykuU291cmNlTWFwR2VuZXJhdG9yO1xudmFyIHV0aWwgPSByZXF1aXJlKCcuL3V0aWwnKTtcblxuLy8gTWF0Y2hlcyBhIFdpbmRvd3Mtc3R5bGUgYFxcclxcbmAgbmV3bGluZSBvciBhIGBcXG5gIG5ld2xpbmUgdXNlZCBieSBhbGwgb3RoZXJcbi8vIG9wZXJhdGluZyBzeXN0ZW1zIHRoZXNlIGRheXMgKGNhcHR1cmluZyB0aGUgcmVzdWx0KS5cbnZhciBSRUdFWF9ORVdMSU5FID0gLyhcXHI/XFxuKS87XG5cbi8vIE5ld2xpbmUgY2hhcmFjdGVyIGNvZGUgZm9yIGNoYXJDb2RlQXQoKSBjb21wYXJpc29uc1xudmFyIE5FV0xJTkVfQ09ERSA9IDEwO1xuXG4vLyBQcml2YXRlIHN5bWJvbCBmb3IgaWRlbnRpZnlpbmcgYFNvdXJjZU5vZGVgcyB3aGVuIG11bHRpcGxlIHZlcnNpb25zIG9mXG4vLyB0aGUgc291cmNlLW1hcCBsaWJyYXJ5IGFyZSBsb2FkZWQuIFRoaXMgTVVTVCBOT1QgQ0hBTkdFIGFjcm9zc1xuLy8gdmVyc2lvbnMhXG52YXIgaXNTb3VyY2VOb2RlID0gXCIkJCRpc1NvdXJjZU5vZGUkJCRcIjtcblxuLyoqXG4gKiBTb3VyY2VOb2RlcyBwcm92aWRlIGEgd2F5IHRvIGFic3RyYWN0IG92ZXIgaW50ZXJwb2xhdGluZy9jb25jYXRlbmF0aW5nXG4gKiBzbmlwcGV0cyBvZiBnZW5lcmF0ZWQgSmF2YVNjcmlwdCBzb3VyY2UgY29kZSB3aGlsZSBtYWludGFpbmluZyB0aGUgbGluZSBhbmRcbiAqIGNvbHVtbiBpbmZvcm1hdGlvbiBhc3NvY2lhdGVkIHdpdGggdGhlIG9yaWdpbmFsIHNvdXJjZSBjb2RlLlxuICpcbiAqIEBwYXJhbSBhTGluZSBUaGUgb3JpZ2luYWwgbGluZSBudW1iZXIuXG4gKiBAcGFyYW0gYUNvbHVtbiBUaGUgb3JpZ2luYWwgY29sdW1uIG51bWJlci5cbiAqIEBwYXJhbSBhU291cmNlIFRoZSBvcmlnaW5hbCBzb3VyY2UncyBmaWxlbmFtZS5cbiAqIEBwYXJhbSBhQ2h1bmtzIE9wdGlvbmFsLiBBbiBhcnJheSBvZiBzdHJpbmdzIHdoaWNoIGFyZSBzbmlwcGV0cyBvZlxuICogICAgICAgIGdlbmVyYXRlZCBKUywgb3Igb3RoZXIgU291cmNlTm9kZXMuXG4gKiBAcGFyYW0gYU5hbWUgVGhlIG9yaWdpbmFsIGlkZW50aWZpZXIuXG4gKi9cbmZ1bmN0aW9uIFNvdXJjZU5vZGUoYUxpbmUsIGFDb2x1bW4sIGFTb3VyY2UsIGFDaHVua3MsIGFOYW1lKSB7XG4gIHRoaXMuY2hpbGRyZW4gPSBbXTtcbiAgdGhpcy5zb3VyY2VDb250ZW50cyA9IHt9O1xuICB0aGlzLmxpbmUgPSBhTGluZSA9PSBudWxsID8gbnVsbCA6IGFMaW5lO1xuICB0aGlzLmNvbHVtbiA9IGFDb2x1bW4gPT0gbnVsbCA/IG51bGwgOiBhQ29sdW1uO1xuICB0aGlzLnNvdXJjZSA9IGFTb3VyY2UgPT0gbnVsbCA/IG51bGwgOiBhU291cmNlO1xuICB0aGlzLm5hbWUgPSBhTmFtZSA9PSBudWxsID8gbnVsbCA6IGFOYW1lO1xuICB0aGlzW2lzU291cmNlTm9kZV0gPSB0cnVlO1xuICBpZiAoYUNodW5rcyAhPSBudWxsKSB0aGlzLmFkZChhQ2h1bmtzKTtcbn1cblxuLyoqXG4gKiBDcmVhdGVzIGEgU291cmNlTm9kZSBmcm9tIGdlbmVyYXRlZCBjb2RlIGFuZCBhIFNvdXJjZU1hcENvbnN1bWVyLlxuICpcbiAqIEBwYXJhbSBhR2VuZXJhdGVkQ29kZSBUaGUgZ2VuZXJhdGVkIGNvZGVcbiAqIEBwYXJhbSBhU291cmNlTWFwQ29uc3VtZXIgVGhlIFNvdXJjZU1hcCBmb3IgdGhlIGdlbmVyYXRlZCBjb2RlXG4gKiBAcGFyYW0gYVJlbGF0aXZlUGF0aCBPcHRpb25hbC4gVGhlIHBhdGggdGhhdCByZWxhdGl2ZSBzb3VyY2VzIGluIHRoZVxuICogICAgICAgIFNvdXJjZU1hcENvbnN1bWVyIHNob3VsZCBiZSByZWxhdGl2ZSB0by5cbiAqL1xuU291cmNlTm9kZS5mcm9tU3RyaW5nV2l0aFNvdXJjZU1hcCA9XG4gIGZ1bmN0aW9uIFNvdXJjZU5vZGVfZnJvbVN0cmluZ1dpdGhTb3VyY2VNYXAoYUdlbmVyYXRlZENvZGUsIGFTb3VyY2VNYXBDb25zdW1lciwgYVJlbGF0aXZlUGF0aCkge1xuICAgIC8vIFRoZSBTb3VyY2VOb2RlIHdlIHdhbnQgdG8gZmlsbCB3aXRoIHRoZSBnZW5lcmF0ZWQgY29kZVxuICAgIC8vIGFuZCB0aGUgU291cmNlTWFwXG4gICAgdmFyIG5vZGUgPSBuZXcgU291cmNlTm9kZSgpO1xuXG4gICAgLy8gQWxsIGV2ZW4gaW5kaWNlcyBvZiB0aGlzIGFycmF5IGFyZSBvbmUgbGluZSBvZiB0aGUgZ2VuZXJhdGVkIGNvZGUsXG4gICAgLy8gd2hpbGUgYWxsIG9kZCBpbmRpY2VzIGFyZSB0aGUgbmV3bGluZXMgYmV0d2VlbiB0d28gYWRqYWNlbnQgbGluZXNcbiAgICAvLyAoc2luY2UgYFJFR0VYX05FV0xJTkVgIGNhcHR1cmVzIGl0cyBtYXRjaCkuXG4gICAgLy8gUHJvY2Vzc2VkIGZyYWdtZW50cyBhcmUgcmVtb3ZlZCBmcm9tIHRoaXMgYXJyYXksIGJ5IGNhbGxpbmcgYHNoaWZ0TmV4dExpbmVgLlxuICAgIHZhciByZW1haW5pbmdMaW5lcyA9IGFHZW5lcmF0ZWRDb2RlLnNwbGl0KFJFR0VYX05FV0xJTkUpO1xuICAgIHZhciBzaGlmdE5leHRMaW5lID0gZnVuY3Rpb24oKSB7XG4gICAgICB2YXIgbGluZUNvbnRlbnRzID0gcmVtYWluaW5nTGluZXMuc2hpZnQoKTtcbiAgICAgIC8vIFRoZSBsYXN0IGxpbmUgb2YgYSBmaWxlIG1pZ2h0IG5vdCBoYXZlIGEgbmV3bGluZS5cbiAgICAgIHZhciBuZXdMaW5lID0gcmVtYWluaW5nTGluZXMuc2hpZnQoKSB8fCBcIlwiO1xuICAgICAgcmV0dXJuIGxpbmVDb250ZW50cyArIG5ld0xpbmU7XG4gICAgfTtcblxuICAgIC8vIFdlIG5lZWQgdG8gcmVtZW1iZXIgdGhlIHBvc2l0aW9uIG9mIFwicmVtYWluaW5nTGluZXNcIlxuICAgIHZhciBsYXN0R2VuZXJhdGVkTGluZSA9IDEsIGxhc3RHZW5lcmF0ZWRDb2x1bW4gPSAwO1xuXG4gICAgLy8gVGhlIGdlbmVyYXRlIFNvdXJjZU5vZGVzIHdlIG5lZWQgYSBjb2RlIHJhbmdlLlxuICAgIC8vIFRvIGV4dHJhY3QgaXQgY3VycmVudCBhbmQgbGFzdCBtYXBwaW5nIGlzIHVzZWQuXG4gICAgLy8gSGVyZSB3ZSBzdG9yZSB0aGUgbGFzdCBtYXBwaW5nLlxuICAgIHZhciBsYXN0TWFwcGluZyA9IG51bGw7XG5cbiAgICBhU291cmNlTWFwQ29uc3VtZXIuZWFjaE1hcHBpbmcoZnVuY3Rpb24gKG1hcHBpbmcpIHtcbiAgICAgIGlmIChsYXN0TWFwcGluZyAhPT0gbnVsbCkge1xuICAgICAgICAvLyBXZSBhZGQgdGhlIGNvZGUgZnJvbSBcImxhc3RNYXBwaW5nXCIgdG8gXCJtYXBwaW5nXCI6XG4gICAgICAgIC8vIEZpcnN0IGNoZWNrIGlmIHRoZXJlIGlzIGEgbmV3IGxpbmUgaW4gYmV0d2Vlbi5cbiAgICAgICAgaWYgKGxhc3RHZW5lcmF0ZWRMaW5lIDwgbWFwcGluZy5nZW5lcmF0ZWRMaW5lKSB7XG4gICAgICAgICAgLy8gQXNzb2NpYXRlIGZpcnN0IGxpbmUgd2l0aCBcImxhc3RNYXBwaW5nXCJcbiAgICAgICAgICBhZGRNYXBwaW5nV2l0aENvZGUobGFzdE1hcHBpbmcsIHNoaWZ0TmV4dExpbmUoKSk7XG4gICAgICAgICAgbGFzdEdlbmVyYXRlZExpbmUrKztcbiAgICAgICAgICBsYXN0R2VuZXJhdGVkQ29sdW1uID0gMDtcbiAgICAgICAgICAvLyBUaGUgcmVtYWluaW5nIGNvZGUgaXMgYWRkZWQgd2l0aG91dCBtYXBwaW5nXG4gICAgICAgIH0gZWxzZSB7XG4gICAgICAgICAgLy8gVGhlcmUgaXMgbm8gbmV3IGxpbmUgaW4gYmV0d2Vlbi5cbiAgICAgICAgICAvLyBBc3NvY2lhdGUgdGhlIGNvZGUgYmV0d2VlbiBcImxhc3RHZW5lcmF0ZWRDb2x1bW5cIiBhbmRcbiAgICAgICAgICAvLyBcIm1hcHBpbmcuZ2VuZXJhdGVkQ29sdW1uXCIgd2l0aCBcImxhc3RNYXBwaW5nXCJcbiAgICAgICAgICB2YXIgbmV4dExpbmUgPSByZW1haW5pbmdMaW5lc1swXTtcbiAgICAgICAgICB2YXIgY29kZSA9IG5leHRMaW5lLnN1YnN0cigwLCBtYXBwaW5nLmdlbmVyYXRlZENvbHVtbiAtXG4gICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgbGFzdEdlbmVyYXRlZENvbHVtbik7XG4gICAgICAgICAgcmVtYWluaW5nTGluZXNbMF0gPSBuZXh0TGluZS5zdWJzdHIobWFwcGluZy5nZW5lcmF0ZWRDb2x1bW4gLVxuICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgIGxhc3RHZW5lcmF0ZWRDb2x1bW4pO1xuICAgICAgICAgIGxhc3RHZW5lcmF0ZWRDb2x1bW4gPSBtYXBwaW5nLmdlbmVyYXRlZENvbHVtbjtcbiAgICAgICAgICBhZGRNYXBwaW5nV2l0aENvZGUobGFzdE1hcHBpbmcsIGNvZGUpO1xuICAgICAgICAgIC8vIE5vIG1vcmUgcmVtYWluaW5nIGNvZGUsIGNvbnRpbnVlXG4gICAgICAgICAgbGFzdE1hcHBpbmcgPSBtYXBwaW5nO1xuICAgICAgICAgIHJldHVybjtcbiAgICAgICAgfVxuICAgICAgfVxuICAgICAgLy8gV2UgYWRkIHRoZSBnZW5lcmF0ZWQgY29kZSB1bnRpbCB0aGUgZmlyc3QgbWFwcGluZ1xuICAgICAgLy8gdG8gdGhlIFNvdXJjZU5vZGUgd2l0aG91dCBhbnkgbWFwcGluZy5cbiAgICAgIC8vIEVhY2ggbGluZSBpcyBhZGRlZCBhcyBzZXBhcmF0ZSBzdHJpbmcuXG4gICAgICB3aGlsZSAobGFzdEdlbmVyYXRlZExpbmUgPCBtYXBwaW5nLmdlbmVyYXRlZExpbmUpIHtcbiAgICAgICAgbm9kZS5hZGQoc2hpZnROZXh0TGluZSgpKTtcbiAgICAgICAgbGFzdEdlbmVyYXRlZExpbmUrKztcbiAgICAgIH1cbiAgICAgIGlmIChsYXN0R2VuZXJhdGVkQ29sdW1uIDwgbWFwcGluZy5nZW5lcmF0ZWRDb2x1bW4pIHtcbiAgICAgICAgdmFyIG5leHRMaW5lID0gcmVtYWluaW5nTGluZXNbMF07XG4gICAgICAgIG5vZGUuYWRkKG5leHRMaW5lLnN1YnN0cigwLCBtYXBwaW5nLmdlbmVyYXRlZENvbHVtbikpO1xuICAgICAgICByZW1haW5pbmdMaW5lc1swXSA9IG5leHRMaW5lLnN1YnN0cihtYXBwaW5nLmdlbmVyYXRlZENvbHVtbik7XG4gICAgICAgIGxhc3RHZW5lcmF0ZWRDb2x1bW4gPSBtYXBwaW5nLmdlbmVyYXRlZENvbHVtbjtcbiAgICAgIH1cbiAgICAgIGxhc3RNYXBwaW5nID0gbWFwcGluZztcbiAgICB9LCB0aGlzKTtcbiAgICAvLyBXZSBoYXZlIHByb2Nlc3NlZCBhbGwgbWFwcGluZ3MuXG4gICAgaWYgKHJlbWFpbmluZ0xpbmVzLmxlbmd0aCA+IDApIHtcbiAgICAgIGlmIChsYXN0TWFwcGluZykge1xuICAgICAgICAvLyBBc3NvY2lhdGUgdGhlIHJlbWFpbmluZyBjb2RlIGluIHRoZSBjdXJyZW50IGxpbmUgd2l0aCBcImxhc3RNYXBwaW5nXCJcbiAgICAgICAgYWRkTWFwcGluZ1dpdGhDb2RlKGxhc3RNYXBwaW5nLCBzaGlmdE5leHRMaW5lKCkpO1xuICAgICAgfVxuICAgICAgLy8gYW5kIGFkZCB0aGUgcmVtYWluaW5nIGxpbmVzIHdpdGhvdXQgYW55IG1hcHBpbmdcbiAgICAgIG5vZGUuYWRkKHJlbWFpbmluZ0xpbmVzLmpvaW4oXCJcIikpO1xuICAgIH1cblxuICAgIC8vIENvcHkgc291cmNlc0NvbnRlbnQgaW50byBTb3VyY2VOb2RlXG4gICAgYVNvdXJjZU1hcENvbnN1bWVyLnNvdXJjZXMuZm9yRWFjaChmdW5jdGlvbiAoc291cmNlRmlsZSkge1xuICAgICAgdmFyIGNvbnRlbnQgPSBhU291cmNlTWFwQ29uc3VtZXIuc291cmNlQ29udGVudEZvcihzb3VyY2VGaWxlKTtcbiAgICAgIGlmIChjb250ZW50ICE9IG51bGwpIHtcbiAgICAgICAgaWYgKGFSZWxhdGl2ZVBhdGggIT0gbnVsbCkge1xuICAgICAgICAgIHNvdXJjZUZpbGUgPSB1dGlsLmpvaW4oYVJlbGF0aXZlUGF0aCwgc291cmNlRmlsZSk7XG4gICAgICAgIH1cbiAgICAgICAgbm9kZS5zZXRTb3VyY2VDb250ZW50KHNvdXJjZUZpbGUsIGNvbnRlbnQpO1xuICAgICAgfVxuICAgIH0pO1xuXG4gICAgcmV0dXJuIG5vZGU7XG5cbiAgICBmdW5jdGlvbiBhZGRNYXBwaW5nV2l0aENvZGUobWFwcGluZywgY29kZSkge1xuICAgICAgaWYgKG1hcHBpbmcgPT09IG51bGwgfHwgbWFwcGluZy5zb3VyY2UgPT09IHVuZGVmaW5lZCkge1xuICAgICAgICBub2RlLmFkZChjb2RlKTtcbiAgICAgIH0gZWxzZSB7XG4gICAgICAgIHZhciBzb3VyY2UgPSBhUmVsYXRpdmVQYXRoXG4gICAgICAgICAgPyB1dGlsLmpvaW4oYVJlbGF0aXZlUGF0aCwgbWFwcGluZy5zb3VyY2UpXG4gICAgICAgICAgOiBtYXBwaW5nLnNvdXJjZTtcbiAgICAgICAgbm9kZS5hZGQobmV3IFNvdXJjZU5vZGUobWFwcGluZy5vcmlnaW5hbExpbmUsXG4gICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgIG1hcHBpbmcub3JpZ2luYWxDb2x1bW4sXG4gICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgIHNvdXJjZSxcbiAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgY29kZSxcbiAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgbWFwcGluZy5uYW1lKSk7XG4gICAgICB9XG4gICAgfVxuICB9O1xuXG4vKipcbiAqIEFkZCBhIGNodW5rIG9mIGdlbmVyYXRlZCBKUyB0byB0aGlzIHNvdXJjZSBub2RlLlxuICpcbiAqIEBwYXJhbSBhQ2h1bmsgQSBzdHJpbmcgc25pcHBldCBvZiBnZW5lcmF0ZWQgSlMgY29kZSwgYW5vdGhlciBpbnN0YW5jZSBvZlxuICogICAgICAgIFNvdXJjZU5vZGUsIG9yIGFuIGFycmF5IHdoZXJlIGVhY2ggbWVtYmVyIGlzIG9uZSBvZiB0aG9zZSB0aGluZ3MuXG4gKi9cblNvdXJjZU5vZGUucHJvdG90eXBlLmFkZCA9IGZ1bmN0aW9uIFNvdXJjZU5vZGVfYWRkKGFDaHVuaykge1xuICBpZiAoQXJyYXkuaXNBcnJheShhQ2h1bmspKSB7XG4gICAgYUNodW5rLmZvckVhY2goZnVuY3Rpb24gKGNodW5rKSB7XG4gICAgICB0aGlzLmFkZChjaHVuayk7XG4gICAgfSwgdGhpcyk7XG4gIH1cbiAgZWxzZSBpZiAoYUNodW5rW2lzU291cmNlTm9kZV0gfHwgdHlwZW9mIGFDaHVuayA9PT0gXCJzdHJpbmdcIikge1xuICAgIGlmIChhQ2h1bmspIHtcbiAgICAgIHRoaXMuY2hpbGRyZW4ucHVzaChhQ2h1bmspO1xuICAgIH1cbiAgfVxuICBlbHNlIHtcbiAgICB0aHJvdyBuZXcgVHlwZUVycm9yKFxuICAgICAgXCJFeHBlY3RlZCBhIFNvdXJjZU5vZGUsIHN0cmluZywgb3IgYW4gYXJyYXkgb2YgU291cmNlTm9kZXMgYW5kIHN0cmluZ3MuIEdvdCBcIiArIGFDaHVua1xuICAgICk7XG4gIH1cbiAgcmV0dXJuIHRoaXM7XG59O1xuXG4vKipcbiAqIEFkZCBhIGNodW5rIG9mIGdlbmVyYXRlZCBKUyB0byB0aGUgYmVnaW5uaW5nIG9mIHRoaXMgc291cmNlIG5vZGUuXG4gKlxuICogQHBhcmFtIGFDaHVuayBBIHN0cmluZyBzbmlwcGV0IG9mIGdlbmVyYXRlZCBKUyBjb2RlLCBhbm90aGVyIGluc3RhbmNlIG9mXG4gKiAgICAgICAgU291cmNlTm9kZSwgb3IgYW4gYXJyYXkgd2hlcmUgZWFjaCBtZW1iZXIgaXMgb25lIG9mIHRob3NlIHRoaW5ncy5cbiAqL1xuU291cmNlTm9kZS5wcm90b3R5cGUucHJlcGVuZCA9IGZ1bmN0aW9uIFNvdXJjZU5vZGVfcHJlcGVuZChhQ2h1bmspIHtcbiAgaWYgKEFycmF5LmlzQXJyYXkoYUNodW5rKSkge1xuICAgIGZvciAodmFyIGkgPSBhQ2h1bmsubGVuZ3RoLTE7IGkgPj0gMDsgaS0tKSB7XG4gICAgICB0aGlzLnByZXBlbmQoYUNodW5rW2ldKTtcbiAgICB9XG4gIH1cbiAgZWxzZSBpZiAoYUNodW5rW2lzU291cmNlTm9kZV0gfHwgdHlwZW9mIGFDaHVuayA9PT0gXCJzdHJpbmdcIikge1xuICAgIHRoaXMuY2hpbGRyZW4udW5zaGlmdChhQ2h1bmspO1xuICB9XG4gIGVsc2Uge1xuICAgIHRocm93IG5ldyBUeXBlRXJyb3IoXG4gICAgICBcIkV4cGVjdGVkIGEgU291cmNlTm9kZSwgc3RyaW5nLCBvciBhbiBhcnJheSBvZiBTb3VyY2VOb2RlcyBhbmQgc3RyaW5ncy4gR290IFwiICsgYUNodW5rXG4gICAgKTtcbiAgfVxuICByZXR1cm4gdGhpcztcbn07XG5cbi8qKlxuICogV2FsayBvdmVyIHRoZSB0cmVlIG9mIEpTIHNuaXBwZXRzIGluIHRoaXMgbm9kZSBhbmQgaXRzIGNoaWxkcmVuLiBUaGVcbiAqIHdhbGtpbmcgZnVuY3Rpb24gaXMgY2FsbGVkIG9uY2UgZm9yIGVhY2ggc25pcHBldCBvZiBKUyBhbmQgaXMgcGFzc2VkIHRoYXRcbiAqIHNuaXBwZXQgYW5kIHRoZSBpdHMgb3JpZ2luYWwgYXNzb2NpYXRlZCBzb3VyY2UncyBsaW5lL2NvbHVtbiBsb2NhdGlvbi5cbiAqXG4gKiBAcGFyYW0gYUZuIFRoZSB0cmF2ZXJzYWwgZnVuY3Rpb24uXG4gKi9cblNvdXJjZU5vZGUucHJvdG90eXBlLndhbGsgPSBmdW5jdGlvbiBTb3VyY2VOb2RlX3dhbGsoYUZuKSB7XG4gIHZhciBjaHVuaztcbiAgZm9yICh2YXIgaSA9IDAsIGxlbiA9IHRoaXMuY2hpbGRyZW4ubGVuZ3RoOyBpIDwgbGVuOyBpKyspIHtcbiAgICBjaHVuayA9IHRoaXMuY2hpbGRyZW5baV07XG4gICAgaWYgKGNodW5rW2lzU291cmNlTm9kZV0pIHtcbiAgICAgIGNodW5rLndhbGsoYUZuKTtcbiAgICB9XG4gICAgZWxzZSB7XG4gICAgICBpZiAoY2h1bmsgIT09ICcnKSB7XG4gICAgICAgIGFGbihjaHVuaywgeyBzb3VyY2U6IHRoaXMuc291cmNlLFxuICAgICAgICAgICAgICAgICAgICAgbGluZTogdGhpcy5saW5lLFxuICAgICAgICAgICAgICAgICAgICAgY29sdW1uOiB0aGlzLmNvbHVtbixcbiAgICAgICAgICAgICAgICAgICAgIG5hbWU6IHRoaXMubmFtZSB9KTtcbiAgICAgIH1cbiAgICB9XG4gIH1cbn07XG5cbi8qKlxuICogTGlrZSBgU3RyaW5nLnByb3RvdHlwZS5qb2luYCBleGNlcHQgZm9yIFNvdXJjZU5vZGVzLiBJbnNlcnRzIGBhU3RyYCBiZXR3ZWVuXG4gKiBlYWNoIG9mIGB0aGlzLmNoaWxkcmVuYC5cbiAqXG4gKiBAcGFyYW0gYVNlcCBUaGUgc2VwYXJhdG9yLlxuICovXG5Tb3VyY2VOb2RlLnByb3RvdHlwZS5qb2luID0gZnVuY3Rpb24gU291cmNlTm9kZV9qb2luKGFTZXApIHtcbiAgdmFyIG5ld0NoaWxkcmVuO1xuICB2YXIgaTtcbiAgdmFyIGxlbiA9IHRoaXMuY2hpbGRyZW4ubGVuZ3RoO1xuICBpZiAobGVuID4gMCkge1xuICAgIG5ld0NoaWxkcmVuID0gW107XG4gICAgZm9yIChpID0gMDsgaSA8IGxlbi0xOyBpKyspIHtcbiAgICAgIG5ld0NoaWxkcmVuLnB1c2godGhpcy5jaGlsZHJlbltpXSk7XG4gICAgICBuZXdDaGlsZHJlbi5wdXNoKGFTZXApO1xuICAgIH1cbiAgICBuZXdDaGlsZHJlbi5wdXNoKHRoaXMuY2hpbGRyZW5baV0pO1xuICAgIHRoaXMuY2hpbGRyZW4gPSBuZXdDaGlsZHJlbjtcbiAgfVxuICByZXR1cm4gdGhpcztcbn07XG5cbi8qKlxuICogQ2FsbCBTdHJpbmcucHJvdG90eXBlLnJlcGxhY2Ugb24gdGhlIHZlcnkgcmlnaHQtbW9zdCBzb3VyY2Ugc25pcHBldC4gVXNlZnVsXG4gKiBmb3IgdHJpbW1pbmcgd2hpdGVzcGFjZSBmcm9tIHRoZSBlbmQgb2YgYSBzb3VyY2Ugbm9kZSwgZXRjLlxuICpcbiAqIEBwYXJhbSBhUGF0dGVybiBUaGUgcGF0dGVybiB0byByZXBsYWNlLlxuICogQHBhcmFtIGFSZXBsYWNlbWVudCBUaGUgdGhpbmcgdG8gcmVwbGFjZSB0aGUgcGF0dGVybiB3aXRoLlxuICovXG5Tb3VyY2VOb2RlLnByb3RvdHlwZS5yZXBsYWNlUmlnaHQgPSBmdW5jdGlvbiBTb3VyY2VOb2RlX3JlcGxhY2VSaWdodChhUGF0dGVybiwgYVJlcGxhY2VtZW50KSB7XG4gIHZhciBsYXN0Q2hpbGQgPSB0aGlzLmNoaWxkcmVuW3RoaXMuY2hpbGRyZW4ubGVuZ3RoIC0gMV07XG4gIGlmIChsYXN0Q2hpbGRbaXNTb3VyY2VOb2RlXSkge1xuICAgIGxhc3RDaGlsZC5yZXBsYWNlUmlnaHQoYVBhdHRlcm4sIGFSZXBsYWNlbWVudCk7XG4gIH1cbiAgZWxzZSBpZiAodHlwZW9mIGxhc3RDaGlsZCA9PT0gJ3N0cmluZycpIHtcbiAgICB0aGlzLmNoaWxkcmVuW3RoaXMuY2hpbGRyZW4ubGVuZ3RoIC0gMV0gPSBsYXN0Q2hpbGQucmVwbGFjZShhUGF0dGVybiwgYVJlcGxhY2VtZW50KTtcbiAgfVxuICBlbHNlIHtcbiAgICB0aGlzLmNoaWxkcmVuLnB1c2goJycucmVwbGFjZShhUGF0dGVybiwgYVJlcGxhY2VtZW50KSk7XG4gIH1cbiAgcmV0dXJuIHRoaXM7XG59O1xuXG4vKipcbiAqIFNldCB0aGUgc291cmNlIGNvbnRlbnQgZm9yIGEgc291cmNlIGZpbGUuIFRoaXMgd2lsbCBiZSBhZGRlZCB0byB0aGUgU291cmNlTWFwR2VuZXJhdG9yXG4gKiBpbiB0aGUgc291cmNlc0NvbnRlbnQgZmllbGQuXG4gKlxuICogQHBhcmFtIGFTb3VyY2VGaWxlIFRoZSBmaWxlbmFtZSBvZiB0aGUgc291cmNlIGZpbGVcbiAqIEBwYXJhbSBhU291cmNlQ29udGVudCBUaGUgY29udGVudCBvZiB0aGUgc291cmNlIGZpbGVcbiAqL1xuU291cmNlTm9kZS5wcm90b3R5cGUuc2V0U291cmNlQ29udGVudCA9XG4gIGZ1bmN0aW9uIFNvdXJjZU5vZGVfc2V0U291cmNlQ29udGVudChhU291cmNlRmlsZSwgYVNvdXJjZUNvbnRlbnQpIHtcbiAgICB0aGlzLnNvdXJjZUNvbnRlbnRzW3V0aWwudG9TZXRTdHJpbmcoYVNvdXJjZUZpbGUpXSA9IGFTb3VyY2VDb250ZW50O1xuICB9O1xuXG4vKipcbiAqIFdhbGsgb3ZlciB0aGUgdHJlZSBvZiBTb3VyY2VOb2Rlcy4gVGhlIHdhbGtpbmcgZnVuY3Rpb24gaXMgY2FsbGVkIGZvciBlYWNoXG4gKiBzb3VyY2UgZmlsZSBjb250ZW50IGFuZCBpcyBwYXNzZWQgdGhlIGZpbGVuYW1lIGFuZCBzb3VyY2UgY29udGVudC5cbiAqXG4gKiBAcGFyYW0gYUZuIFRoZSB0cmF2ZXJzYWwgZnVuY3Rpb24uXG4gKi9cblNvdXJjZU5vZGUucHJvdG90eXBlLndhbGtTb3VyY2VDb250ZW50cyA9XG4gIGZ1bmN0aW9uIFNvdXJjZU5vZGVfd2Fsa1NvdXJjZUNvbnRlbnRzKGFGbikge1xuICAgIGZvciAodmFyIGkgPSAwLCBsZW4gPSB0aGlzLmNoaWxkcmVuLmxlbmd0aDsgaSA8IGxlbjsgaSsrKSB7XG4gICAgICBpZiAodGhpcy5jaGlsZHJlbltpXVtpc1NvdXJjZU5vZGVdKSB7XG4gICAgICAgIHRoaXMuY2hpbGRyZW5baV0ud2Fsa1NvdXJjZUNvbnRlbnRzKGFGbik7XG4gICAgICB9XG4gICAgfVxuXG4gICAgdmFyIHNvdXJjZXMgPSBPYmplY3Qua2V5cyh0aGlzLnNvdXJjZUNvbnRlbnRzKTtcbiAgICBmb3IgKHZhciBpID0gMCwgbGVuID0gc291cmNlcy5sZW5ndGg7IGkgPCBsZW47IGkrKykge1xuICAgICAgYUZuKHV0aWwuZnJvbVNldFN0cmluZyhzb3VyY2VzW2ldKSwgdGhpcy5zb3VyY2VDb250ZW50c1tzb3VyY2VzW2ldXSk7XG4gICAgfVxuICB9O1xuXG4vKipcbiAqIFJldHVybiB0aGUgc3RyaW5nIHJlcHJlc2VudGF0aW9uIG9mIHRoaXMgc291cmNlIG5vZGUuIFdhbGtzIG92ZXIgdGhlIHRyZWVcbiAqIGFuZCBjb25jYXRlbmF0ZXMgYWxsIHRoZSB2YXJpb3VzIHNuaXBwZXRzIHRvZ2V0aGVyIHRvIG9uZSBzdHJpbmcuXG4gKi9cblNvdXJjZU5vZGUucHJvdG90eXBlLnRvU3RyaW5nID0gZnVuY3Rpb24gU291cmNlTm9kZV90b1N0cmluZygpIHtcbiAgdmFyIHN0ciA9IFwiXCI7XG4gIHRoaXMud2FsayhmdW5jdGlvbiAoY2h1bmspIHtcbiAgICBzdHIgKz0gY2h1bms7XG4gIH0pO1xuICByZXR1cm4gc3RyO1xufTtcblxuLyoqXG4gKiBSZXR1cm5zIHRoZSBzdHJpbmcgcmVwcmVzZW50YXRpb24gb2YgdGhpcyBzb3VyY2Ugbm9kZSBhbG9uZyB3aXRoIGEgc291cmNlXG4gKiBtYXAuXG4gKi9cblNvdXJjZU5vZGUucHJvdG90eXBlLnRvU3RyaW5nV2l0aFNvdXJjZU1hcCA9IGZ1bmN0aW9uIFNvdXJjZU5vZGVfdG9TdHJpbmdXaXRoU291cmNlTWFwKGFBcmdzKSB7XG4gIHZhciBnZW5lcmF0ZWQgPSB7XG4gICAgY29kZTogXCJcIixcbiAgICBsaW5lOiAxLFxuICAgIGNvbHVtbjogMFxuICB9O1xuICB2YXIgbWFwID0gbmV3IFNvdXJjZU1hcEdlbmVyYXRvcihhQXJncyk7XG4gIHZhciBzb3VyY2VNYXBwaW5nQWN0aXZlID0gZmFsc2U7XG4gIHZhciBsYXN0T3JpZ2luYWxTb3VyY2UgPSBudWxsO1xuICB2YXIgbGFzdE9yaWdpbmFsTGluZSA9IG51bGw7XG4gIHZhciBsYXN0T3JpZ2luYWxDb2x1bW4gPSBudWxsO1xuICB2YXIgbGFzdE9yaWdpbmFsTmFtZSA9IG51bGw7XG4gIHRoaXMud2FsayhmdW5jdGlvbiAoY2h1bmssIG9yaWdpbmFsKSB7XG4gICAgZ2VuZXJhdGVkLmNvZGUgKz0gY2h1bms7XG4gICAgaWYgKG9yaWdpbmFsLnNvdXJjZSAhPT0gbnVsbFxuICAgICAgICAmJiBvcmlnaW5hbC5saW5lICE9PSBudWxsXG4gICAgICAgICYmIG9yaWdpbmFsLmNvbHVtbiAhPT0gbnVsbCkge1xuICAgICAgaWYobGFzdE9yaWdpbmFsU291cmNlICE9PSBvcmlnaW5hbC5zb3VyY2VcbiAgICAgICAgIHx8IGxhc3RPcmlnaW5hbExpbmUgIT09IG9yaWdpbmFsLmxpbmVcbiAgICAgICAgIHx8IGxhc3RPcmlnaW5hbENvbHVtbiAhPT0gb3JpZ2luYWwuY29sdW1uXG4gICAgICAgICB8fCBsYXN0T3JpZ2luYWxOYW1lICE9PSBvcmlnaW5hbC5uYW1lKSB7XG4gICAgICAgIG1hcC5hZGRNYXBwaW5nKHtcbiAgICAgICAgICBzb3VyY2U6IG9yaWdpbmFsLnNvdXJjZSxcbiAgICAgICAgICBvcmlnaW5hbDoge1xuICAgICAgICAgICAgbGluZTogb3JpZ2luYWwubGluZSxcbiAgICAgICAgICAgIGNvbHVtbjogb3JpZ2luYWwuY29sdW1uXG4gICAgICAgICAgfSxcbiAgICAgICAgICBnZW5lcmF0ZWQ6IHtcbiAgICAgICAgICAgIGxpbmU6IGdlbmVyYXRlZC5saW5lLFxuICAgICAgICAgICAgY29sdW1uOiBnZW5lcmF0ZWQuY29sdW1uXG4gICAgICAgICAgfSxcbiAgICAgICAgICBuYW1lOiBvcmlnaW5hbC5uYW1lXG4gICAgICAgIH0pO1xuICAgICAgfVxuICAgICAgbGFzdE9yaWdpbmFsU291cmNlID0gb3JpZ2luYWwuc291cmNlO1xuICAgICAgbGFzdE9yaWdpbmFsTGluZSA9IG9yaWdpbmFsLmxpbmU7XG4gICAgICBsYXN0T3JpZ2luYWxDb2x1bW4gPSBvcmlnaW5hbC5jb2x1bW47XG4gICAgICBsYXN0T3JpZ2luYWxOYW1lID0gb3JpZ2luYWwubmFtZTtcbiAgICAgIHNvdXJjZU1hcHBpbmdBY3RpdmUgPSB0cnVlO1xuICAgIH0gZWxzZSBpZiAoc291cmNlTWFwcGluZ0FjdGl2ZSkge1xuICAgICAgbWFwLmFkZE1hcHBpbmcoe1xuICAgICAgICBnZW5lcmF0ZWQ6IHtcbiAgICAgICAgICBsaW5lOiBnZW5lcmF0ZWQubGluZSxcbiAgICAgICAgICBjb2x1bW46IGdlbmVyYXRlZC5jb2x1bW5cbiAgICAgICAgfVxuICAgICAgfSk7XG4gICAgICBsYXN0T3JpZ2luYWxTb3VyY2UgPSBudWxsO1xuICAgICAgc291cmNlTWFwcGluZ0FjdGl2ZSA9IGZhbHNlO1xuICAgIH1cbiAgICBmb3IgKHZhciBpZHggPSAwLCBsZW5ndGggPSBjaHVuay5sZW5ndGg7IGlkeCA8IGxlbmd0aDsgaWR4KyspIHtcbiAgICAgIGlmIChjaHVuay5jaGFyQ29kZUF0KGlkeCkgPT09IE5FV0xJTkVfQ09ERSkge1xuICAgICAgICBnZW5lcmF0ZWQubGluZSsrO1xuICAgICAgICBnZW5lcmF0ZWQuY29sdW1uID0gMDtcbiAgICAgICAgLy8gTWFwcGluZ3MgZW5kIGF0IGVvbFxuICAgICAgICBpZiAoaWR4ICsgMSA9PT0gbGVuZ3RoKSB7XG4gICAgICAgICAgbGFzdE9yaWdpbmFsU291cmNlID0gbnVsbDtcbiAgICAgICAgICBzb3VyY2VNYXBwaW5nQWN0aXZlID0gZmFsc2U7XG4gICAgICAgIH0gZWxzZSBpZiAoc291cmNlTWFwcGluZ0FjdGl2ZSkge1xuICAgICAgICAgIG1hcC5hZGRNYXBwaW5nKHtcbiAgICAgICAgICAgIHNvdXJjZTogb3JpZ2luYWwuc291cmNlLFxuICAgICAgICAgICAgb3JpZ2luYWw6IHtcbiAgICAgICAgICAgICAgbGluZTogb3JpZ2luYWwubGluZSxcbiAgICAgICAgICAgICAgY29sdW1uOiBvcmlnaW5hbC5jb2x1bW5cbiAgICAgICAgICAgIH0sXG4gICAgICAgICAgICBnZW5lcmF0ZWQ6IHtcbiAgICAgICAgICAgICAgbGluZTogZ2VuZXJhdGVkLmxpbmUsXG4gICAgICAgICAgICAgIGNvbHVtbjogZ2VuZXJhdGVkLmNvbHVtblxuICAgICAgICAgICAgfSxcbiAgICAgICAgICAgIG5hbWU6IG9yaWdpbmFsLm5hbWVcbiAgICAgICAgICB9KTtcbiAgICAgICAgfVxuICAgICAgfSBlbHNlIHtcbiAgICAgICAgZ2VuZXJhdGVkLmNvbHVtbisrO1xuICAgICAgfVxuICAgIH1cbiAgfSk7XG4gIHRoaXMud2Fsa1NvdXJjZUNvbnRlbnRzKGZ1bmN0aW9uIChzb3VyY2VGaWxlLCBzb3VyY2VDb250ZW50KSB7XG4gICAgbWFwLnNldFNvdXJjZUNvbnRlbnQoc291cmNlRmlsZSwgc291cmNlQ29udGVudCk7XG4gIH0pO1xuXG4gIHJldHVybiB7IGNvZGU6IGdlbmVyYXRlZC5jb2RlLCBtYXA6IG1hcCB9O1xufTtcblxuZXhwb3J0cy5Tb3VyY2VOb2RlID0gU291cmNlTm9kZTtcblxuXG5cbi8qKioqKioqKioqKioqKioqKlxuICoqIFdFQlBBQ0sgRk9PVEVSXG4gKiogLi9saWIvc291cmNlLW5vZGUuanNcbiAqKiBtb2R1bGUgaWQgPSAxMFxuICoqIG1vZHVsZSBjaHVua3MgPSAwXG4gKiovIl0sInNvdXJjZVJvb3QiOiIifQ==
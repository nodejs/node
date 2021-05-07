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

// This is a copy from blink dev tools, see:
// http://src.chromium.org/viewvc/blink/trunk/Source/devtools/front_end/SourceMap.js
// revision: 153407

// Added to make the file work without dev tools
export const WebInspector = {};
WebInspector.ParsedURL = {};
WebInspector.ParsedURL.completeURL = function(){};
// start of original file content

/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
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

/**
 * Implements Source Map V3 model. See http://code.google.com/p/closure-compiler/wiki/SourceMaps
 * for format description.
 * @constructor
 * @param {string} sourceMappingURL
 * @param {SourceMapV3} payload
 */
WebInspector.SourceMap = function(sourceMappingURL, payload)
{
    if (!WebInspector.SourceMap.prototype._base64Map) {
        const base64Digits = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
        WebInspector.SourceMap.prototype._base64Map = {};
        for (let i = 0; i < base64Digits.length; ++i)
            WebInspector.SourceMap.prototype._base64Map[base64Digits.charAt(i)] = i;
    }

    this._sourceMappingURL = sourceMappingURL;
    this._reverseMappingsBySourceURL = {};
    this._mappings = [];
    this._sources = {};
    this._sourceContentByURL = {};
    this._parseMappingPayload(payload);
}

/**
 * @param {string} sourceMapURL
 * @param {string} compiledURL
 * @param {function(WebInspector.SourceMap)} callback
 */
WebInspector.SourceMap.load = function(sourceMapURL, compiledURL, callback)
{
    NetworkAgent.loadResourceForFrontend(WebInspector.resourceTreeModel.mainFrame.id, sourceMapURL, undefined, contentLoaded.bind(this));

    /**
     * @param {?Protocol.Error} error
     * @param {number} statusCode
     * @param {NetworkAgent.Headers} headers
     * @param {string} content
     */
    function contentLoaded(error, statusCode, headers, content)
    {
        if (error || !content || statusCode >= 400) {
            console.error(`Could not load content for ${sourceMapURL} : ${error || (`HTTP status code: ${statusCode}`)}`);
            callback(null);
            return;
        }

        if (content.slice(0, 3) === ")]}")
            content = content.substring(content.indexOf('\n'));
        try {
            const payload = /** @type {SourceMapV3} */ (JSON.parse(content));
            const baseURL = sourceMapURL.startsWith("data:") ? compiledURL : sourceMapURL;
            callback(new WebInspector.SourceMap(baseURL, payload));
        } catch(e) {
            console.error(e.message);
            callback(null);
        }
    }
}

WebInspector.SourceMap.prototype = {
    /**
     * @return {Array.<string>}
     */
    sources()
    {
        return Object.keys(this._sources);
    },

    /**
     * @param {string} sourceURL
     * @return {string|undefined}
     */
    sourceContent(sourceURL)
    {
        return this._sourceContentByURL[sourceURL];
    },

    /**
     * @param {string} sourceURL
     * @param {WebInspector.ResourceType} contentType
     * @return {WebInspector.ContentProvider}
     */
    sourceContentProvider(sourceURL, contentType)
    {
        const lastIndexOfDot = sourceURL.lastIndexOf(".");
        const extension = lastIndexOfDot !== -1 ? sourceURL.substr(lastIndexOfDot + 1) : "";
        const mimeType = WebInspector.ResourceType.mimeTypesForExtensions[extension.toLowerCase()];
        const sourceContent = this.sourceContent(sourceURL);
        if (sourceContent)
            return new WebInspector.StaticContentProvider(contentType, sourceContent, mimeType);
        return new WebInspector.CompilerSourceMappingContentProvider(sourceURL, contentType, mimeType);
    },

    /**
     * @param {SourceMapV3} mappingPayload
     */
    _parseMappingPayload(mappingPayload)
    {
        if (mappingPayload.sections)
            this._parseSections(mappingPayload.sections);
        else
            this._parseMap(mappingPayload, 0, 0);
    },

    /**
     * @param {Array.<SourceMapV3.Section>} sections
     */
    _parseSections(sections)
    {
        for (let i = 0; i < sections.length; ++i) {
            const section = sections[i];
            this._parseMap(section.map, section.offset.line, section.offset.column);
        }
    },

    /**
     * @param {number} lineNumber in compiled resource
     * @param {number} columnNumber in compiled resource
     * @return {?Array}
     */
    findEntry(lineNumber, columnNumber)
    {
        let first = 0;
        let count = this._mappings.length;
        while (count > 1) {
          const step = count >> 1;
          const middle = first + step;
          const mapping = this._mappings[middle];
          if (lineNumber < mapping[0] || (lineNumber === mapping[0] && columnNumber < mapping[1]))
              count = step;
          else {
              first = middle;
              count -= step;
          }
        }
        const entry = this._mappings[first];
        if (!first && entry && (lineNumber < entry[0] || (lineNumber === entry[0] && columnNumber < entry[1])))
            return null;
        return entry;
    },

    /**
     * @param {string} sourceURL of the originating resource
     * @param {number} lineNumber in the originating resource
     * @return {Array}
     */
    findEntryReversed(sourceURL, lineNumber)
    {
        const mappings = this._reverseMappingsBySourceURL[sourceURL];
        for ( ; lineNumber < mappings.length; ++lineNumber) {
            const mapping = mappings[lineNumber];
            if (mapping)
                return mapping;
        }
        return this._mappings[0];
    },

    /**
     * @override
     */
    _parseMap(map, lineNumber, columnNumber)
    {
        let sourceIndex = 0;
        let sourceLineNumber = 0;
        let sourceColumnNumber = 0;
        let nameIndex = 0;

        const sources = [];
        const originalToCanonicalURLMap = {};
        for (let i = 0; i < map.sources.length; ++i) {
            const originalSourceURL = map.sources[i];
            let sourceRoot = map.sourceRoot || "";
            if (sourceRoot && !sourceRoot.endsWith("/")) sourceRoot += "/";
            const href = sourceRoot + originalSourceURL;
            const url = WebInspector.ParsedURL.completeURL(this._sourceMappingURL, href) || href;
            originalToCanonicalURLMap[originalSourceURL] = url;
            sources.push(url);
            this._sources[url] = true;

            if (map.sourcesContent && map.sourcesContent[i]) {
                this._sourceContentByURL[url] = map.sourcesContent[i];
            }
        }

        const stringCharIterator = new WebInspector.SourceMap.StringCharIterator(map.mappings);
        let sourceURL = sources[sourceIndex];

        while (true) {
            if (stringCharIterator.peek() === ",")
                stringCharIterator.next();
            else {
                while (stringCharIterator.peek() === ";") {
                    lineNumber += 1;
                    columnNumber = 0;
                    stringCharIterator.next();
                }
                if (!stringCharIterator.hasNext())
                    break;
            }

            columnNumber += this._decodeVLQ(stringCharIterator);
            if (this._isSeparator(stringCharIterator.peek())) {
                this._mappings.push([lineNumber, columnNumber]);
                continue;
            }

            const sourceIndexDelta = this._decodeVLQ(stringCharIterator);
            if (sourceIndexDelta) {
                sourceIndex += sourceIndexDelta;
                sourceURL = sources[sourceIndex];
            }
            sourceLineNumber += this._decodeVLQ(stringCharIterator);
            sourceColumnNumber += this._decodeVLQ(stringCharIterator);
            if (!this._isSeparator(stringCharIterator.peek()))
                nameIndex += this._decodeVLQ(stringCharIterator);

            this._mappings.push([lineNumber, columnNumber, sourceURL, sourceLineNumber, sourceColumnNumber]);
        }

        for (let i = 0; i < this._mappings.length; ++i) {
            const mapping = this._mappings[i];
            const url = mapping[2];
            if (!url) continue;
            if (!this._reverseMappingsBySourceURL[url]) {
                this._reverseMappingsBySourceURL[url] = [];
            }
            const reverseMappings = this._reverseMappingsBySourceURL[url];
            const sourceLine = mapping[3];
            if (!reverseMappings[sourceLine]) {
                reverseMappings[sourceLine] = [mapping[0], mapping[1]];
            }
        }
    },

    /**
     * @param {string} char
     * @return {boolean}
     */
    _isSeparator(char)
    {
        return char === "," || char === ";";
    },

    /**
     * @param {WebInspector.SourceMap.StringCharIterator} stringCharIterator
     * @return {number}
     */
    _decodeVLQ(stringCharIterator)
    {
        // Read unsigned value.
        let result = 0;
        let shift = 0;
        let digit;
        do {
            digit = this._base64Map[stringCharIterator.next()];
            result += (digit & this._VLQ_BASE_MASK) << shift;
            shift += this._VLQ_BASE_SHIFT;
        } while (digit & this._VLQ_CONTINUATION_MASK);

        // Fix the sign.
        const negative = result & 1;
        // Use unsigned right shift, so that the 32nd bit is properly shifted
        // to the 31st, and the 32nd becomes unset.
        result >>>= 1;
        if (negate) {
          // We need to OR 0x80000000 here to ensure the 32nd bit (the sign bit
          // in a 32bit int) is always set for negative numbers. If `result`
          // were 1, (meaning `negate` is true and all other bits were zeros),
          // `result` would now be 0. But -0 doesn't flip the 32nd bit as
          // intended. All other numbers will successfully set the 32nd bit
          // without issue, so doing this is a noop for them.
          return -result | 0x80000000;
        }
        return result;
    },

    _VLQ_BASE_SHIFT: 5,
    _VLQ_BASE_MASK: (1 << 5) - 1,
    _VLQ_CONTINUATION_MASK: 1 << 5
}

/**
 * @constructor
 * @param {string} string
 */
WebInspector.SourceMap.StringCharIterator = function(string)
{
    this._string = string;
    this._position = 0;
}

WebInspector.SourceMap.StringCharIterator.prototype = {
    /**
     * @return {string}
     */
    next()
    {
        return this._string.charAt(this._position++);
    },

    /**
     * @return {string}
     */
    peek()
    {
        return this._string.charAt(this._position);
    },

    /**
     * @return {boolean}
     */
    hasNext()
    {
        return this._position < this._string.length;
    }
}

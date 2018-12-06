/*
SHJS - Syntax Highlighting in JavaScript
Copyright (C) 2007, 2008 gnombat@users.sourceforge.net
License: http://shjs.sourceforge.net/doc/gplv3.html
*/
'use strict';

if (!this.sh_languages) {
  this.sh_languages = {};
}
const sh_requests = {};

function sh_isEmailAddress(url) {
  if (/^mailto:/.test(url)) {
    return false;
  }
  return url.indexOf('@') !== -1;
}

function sh_setHref(tags, numTags, inputString) {
  let url = inputString.substring(tags[numTags - 2].pos, tags[numTags - 1].pos);
  if (url.length >= 2 &&
      url.charAt(0) === '<' &&
      url.charAt(url.length - 1) === '>') {
    url = url.substr(1, url.length - 2);
  }
  if (sh_isEmailAddress(url)) {
    url = `mailto:${url}`;
  }
  tags[numTags - 2].node.href = url;
}

/*
Konqueror has a bug where the regular expression /$/g will not match at the end
of a line more than once:

  var regex = /$/g;
  var match;

  var line = '1234567890';
  regex.lastIndex = 10;
  match = regex.exec(line);

  var line2 = 'abcde';
  regex.lastIndex = 5;
  match = regex.exec(line2);  // fails
*/
function sh_konquerorExec(s) {
  const result = [''];
  result.index = s.length;
  result.input = s;
  return result;
}

/**
Highlights all elements containing source code in a text string.  The return
value is an array of objects, each representing an HTML start or end tag.  Each
object has a property named pos, which is an integer representing the text
offset of the tag. Every start tag also has a property named node, which is the
DOM element started by the tag. End tags do not have this property.
@param  inputString  a text string
@param  language  a language definition object
@return  an array of tag objects
*/
function sh_highlightString(inputString, language) {
  if (/Konqueror/.test(navigator.userAgent)) {
    if (!language.konquered) {
      for (let s = 0; s < language.length; s++) {
        for (let p = 0; p < language[s].length; p++) {
          const r = language[s][p][0];
          if (r.source === '$') {
            r.exec = sh_konquerorExec;
          }
        }
      }
      language.konquered = true;
    }
  }

  const a = document.createElement('a');
  const span = document.createElement('span');

  // the result
  const tags = [];
  let numTags = 0;

  // each element is a pattern object from language
  const patternStack = [];

  // the current position within inputString
  let pos = 0;

  // the name of the current style, or null if there is no current style
  let currentStyle = null;

  const output = (s, style) => {
    const length = s.length;
    /*
      this is more than just an optimization
      we don't want to output empty <span></span> elements
    */
    if (length === 0) {
      return;
    }
    if (!style) {
      const stackLength = patternStack.length;
      if (stackLength !== 0) {
        const pattern = patternStack[stackLength - 1];
        // check whether this is a state or an environment
        if (!pattern[3]) {
          /*
            it's not a state - it's an environment;
            use the style for this environment
          */
          style = pattern[1];
        }
      }
    }
    if (currentStyle !== style) {
      if (currentStyle) {
        tags[numTags++] = { pos: pos };
        if (currentStyle === 'sh_url') {
          sh_setHref(tags, numTags, inputString);
        }
      }
      if (style) {
        let clone;
        if (style === 'sh_url') {
          clone = a.cloneNode(false);
        } else {
          clone = span.cloneNode(false);
        }
        clone.className = style;
        tags[numTags++] = { node: clone, pos: pos };
      }
    }
    pos += length;
    currentStyle = style;
  };

  const endOfLinePattern = /\r\n|\r|\n/g;
  endOfLinePattern.lastIndex = 0;
  const inputStringLength = inputString.length;
  while (pos < inputStringLength) {
    const start = pos;
    let end;
    let startOfNextLine;
    const endOfLineMatch = endOfLinePattern.exec(inputString);
    if (endOfLineMatch === null) {
      end = inputStringLength;
      startOfNextLine = inputStringLength;
    } else {
      end = endOfLineMatch.index;
      startOfNextLine = endOfLinePattern.lastIndex;
    }

    const line = inputString.substring(start, end);

    const matchCache = [];
    for (;;) {
      const posWithinLine = pos - start;

      let stateIndex;
      const stackLength = patternStack.length;
      if (stackLength === 0) {
        stateIndex = 0;
      } else {
        // get the next state
        stateIndex = patternStack[stackLength - 1][2];
      }

      const state = language[stateIndex];
      const numPatterns = state.length;
      let mc = matchCache[stateIndex];
      if (!mc) {
        mc = matchCache[stateIndex] = [];
      }
      let bestMatch = null;
      let bestPatternIndex = -1;
      for (let i = 0; i < numPatterns; i++) {
        let match;
        if (i < mc.length && (mc[i] === null || posWithinLine <= mc[i].index)) {
          match = mc[i];
        } else {
          const regex = state[i][0];
          regex.lastIndex = posWithinLine;
          match = regex.exec(line);
          mc[i] = match;
        }
        if (match !== null &&
           (bestMatch === null || match.index < bestMatch.index)) {
          bestMatch = match;
          bestPatternIndex = i;
          if (match.index === posWithinLine) {
            break;
          }
        }
      }

      if (bestMatch === null) {
        output(line.substring(posWithinLine), null);
        break;
      } else {
        // got a match
        if (bestMatch.index > posWithinLine) {
          output(line.substring(posWithinLine, bestMatch.index), null);
        }

        const pattern = state[bestPatternIndex];

        const newStyle = pattern[1];
        let matchedString;
        if (newStyle instanceof Array) {
          for (let subexp = 0; subexp < newStyle.length; subexp++) {
            matchedString = bestMatch[subexp + 1];
            output(matchedString, newStyle[subexp]);
          }
        } else {
          matchedString = bestMatch[0];
          output(matchedString, newStyle);
        }

        switch (pattern[2]) {
          case -1:
          // do nothing
            break;
          case -2:
          // exit
            patternStack.pop();
            break;
          case -3:
          // exitall
            patternStack.length = 0;
            break;
          default:
          // this was the start of a delimited pattern or a state/environment
            patternStack.push(pattern);
            break;
        }
      }
    }

    // end of the line
    if (currentStyle) {
      tags[numTags++] = { pos: pos };
      if (currentStyle === 'sh_url') {
        sh_setHref(tags, numTags, inputString);
      }
      currentStyle = null;
    }
    pos = startOfNextLine;
  }

  return tags;
}

// ////////////////////////////////////////////////////////////////////////////
// DOM-dependent functions

function sh_getClasses(element) {
  const result = [];
  const htmlClass = element.className;
  if (htmlClass && htmlClass.length > 0) {
    const htmlClasses = htmlClass.split(' ');
    for (let i = 0; i < htmlClasses.length; i++) {
      if (htmlClasses[i].length > 0) {
        result.push(htmlClasses[i]);
      }
    }
  }
  return result;
}

function sh_addClass(element, name) {
  const htmlClasses = sh_getClasses(element);
  for (let i = 0; i < htmlClasses.length; i++) {
    if (name.toLowerCase() === htmlClasses[i].toLowerCase()) {
      return;
    }
  }
  htmlClasses.push(name);
  element.className = htmlClasses.join(' ');
}

/**
Extracts the tags from an HTML DOM NodeList.
@param  nodeList  a DOM NodeList
@param  result  an object with text, tags and pos properties
*/
function sh_extractTagsFromNodeList(nodeList, result) {
  const length = nodeList.length;
  for (let i = 0; i < length; i++) {
    const node = nodeList.item(i);
    switch (node.nodeType) {
      case 1:
        if (node.nodeName.toLowerCase() === 'br') {
          let terminator;
          if (/MSIE/.test(navigator.userAgent)) {
            terminator = '\r';
          } else {
            terminator = '\n';
          }
          result.text.push(terminator);
          result.pos++;
        } else {
          result.tags.push({ node: node.cloneNode(false), pos: result.pos });
          sh_extractTagsFromNodeList(node.childNodes, result);
          result.tags.push({ pos: result.pos });
        }
        break;
      case 3:
      case 4:
        result.text.push(node.data);
        result.pos += node.length;
        break;
    }
  }
}

/**
Extracts the tags from the text of an HTML element. The extracted tags will be
returned as an array of tag objects. See sh_highlightString for the format of
the tag objects.
@param  element  a DOM element
@param  tags  an empty array; the extracted tag objects will be returned in it
@return  the text of the element
@see  sh_highlightString
*/
function sh_extractTags(element, tags) {
  const result = {};
  result.text = [];
  result.tags = tags;
  result.pos = 0;
  sh_extractTagsFromNodeList(element.childNodes, result);
  return result.text.join('');
}

/**
Merges the original tags from an element with the tags produced by highlighting.
@param  originalTags  an array containing the original tags
@param  highlightTags  an array containing the highlighting tags - these must not overlap
@result  an array containing the merged tags
*/
function sh_mergeTags(originalTags, highlightTags) {
  const numOriginalTags = originalTags.length;
  if (numOriginalTags === 0) {
    return highlightTags;
  }

  const numHighlightTags = highlightTags.length;
  if (numHighlightTags === 0) {
    return originalTags;
  }

  const result = [];
  let originalIndex = 0;
  let highlightIndex = 0;

  while (originalIndex < numOriginalTags && highlightIndex < numHighlightTags) {
    const originalTag = originalTags[originalIndex];
    const highlightTag = highlightTags[highlightIndex];

    if (originalTag.pos <= highlightTag.pos) {
      result.push(originalTag);
      originalIndex++;
    } else {
      result.push(highlightTag);
      if (highlightTags[highlightIndex + 1].pos <= originalTag.pos) {
        highlightIndex++;
        result.push(highlightTags[highlightIndex]);
        highlightIndex++;
      } else {
        // new end tag
        result.push({ pos: originalTag.pos });

        // new start tag
        highlightTags[highlightIndex] = {
          node: highlightTag.node.cloneNode(false),
          pos: originalTag.pos
        };
      }
    }
  }

  while (originalIndex < numOriginalTags) {
    result.push(originalTags[originalIndex]);
    originalIndex++;
  }

  while (highlightIndex < numHighlightTags) {
    result.push(highlightTags[highlightIndex]);
    highlightIndex++;
  }

  return result;
}

/**
Inserts tags into text.
@param  tags  an array of tag objects
@param  text  a string representing the text
@return  a DOM DocumentFragment representing the resulting HTML
*/
function sh_insertTags(tags, text) {
  const doc = document;

  const result = document.createDocumentFragment();
  let tagIndex = 0;
  const numTags = tags.length;
  let textPos = 0;
  const textLength = text.length;
  let currentNode = result;

  // output one tag or text node every iteration
  while (textPos < textLength || tagIndex < numTags) {
    let tag;
    let tagPos;
    if (tagIndex < numTags) {
      tag = tags[tagIndex];
      tagPos = tag.pos;
    } else {
      tagPos = textLength;
    }

    if (tagPos <= textPos) {
      // output the tag
      if (tag.node) {
        // start tag
        const newNode = tag.node;
        currentNode.appendChild(newNode);
        currentNode = newNode;
      } else {
        // end tag
        currentNode = currentNode.parentNode;
      }
      tagIndex++;
    } else {
      // output text
      currentNode.appendChild(doc.createTextNode(text.substring(textPos,
                                                                tagPos)));
      textPos = tagPos;
    }
  }

  return result;
}

/**
Highlights an element containing source code.  Upon completion of this function,
the element will have been placed in the "sh_sourceCode" class.
@param  element  a DOM <pre> element containing the source code to be highlighted
@param  language  a language definition object
*/
function sh_highlightElement(element, language) {
  sh_addClass(element, 'sh_sourceCode');
  const originalTags = [];
  const inputString = sh_extractTags(element, originalTags);
  const highlightTags = sh_highlightString(inputString, language);
  const tags = sh_mergeTags(originalTags, highlightTags);
  const documentFragment = sh_insertTags(tags, inputString);
  while (element.hasChildNodes()) {
    element.removeChild(element.firstChild);
  }
  element.appendChild(documentFragment);
}

function sh_getXMLHttpRequest() {
  if (window.ActiveXObject) {
    return new ActiveXObject('Msxml2.XMLHTTP');
  } else if (window.XMLHttpRequest) {
    return new XMLHttpRequest();
  }
  throw 'No XMLHttpRequest implementation available';
}

function sh_load(language, element, prefix, suffix) {
  if (language in sh_requests) {
    sh_requests[language].push(element);
    return;
  }
  sh_requests[language] = [element];
  let request = sh_getXMLHttpRequest();
  const url = `${prefix}sh_${language}${suffix}`;
  request.open('GET', url, true);
  request.onreadystatechange = function() {
    if (request.readyState === 4) {
      try {
        if (!request.status || request.status === 200) {
          eval(request.responseText);
          const elements = sh_requests[language];
          for (let i = 0; i < elements.length; i++) {
            sh_highlightElement(elements[i], sh_languages[language]);
          }
        } else {
          throw `HTTP error: status ${request.status}`;
        }
      } finally {
        request = null;
      }
    }
  };
  request.send(null);
}

/**
Highlights all elements containing source code on the current page. Elements
containing source code must be "pre" elements with a "class" attribute of
"sh_LANGUAGE", where LANGUAGE is a valid language identifier; e.g., "sh_java"
identifies the element as containing "java" language source code.
*/
function highlight(prefix, suffix, tag) {
  const nodeList = document.getElementsByTagName(tag);
  for (let i = 0; i < nodeList.length; i++) {
    const element = nodeList.item(i);
    const htmlClasses = sh_getClasses(element);
    let highlighted = false;
    let donthighlight = false;
    for (let j = 0; j < htmlClasses.length; j++) {
      const htmlClass = htmlClasses[j].toLowerCase();
      if (htmlClass === 'sh_none') {
        donthighlight = true;
        continue;
      }
      if (htmlClass.substr(0, 3) === 'sh_') {
        const language = htmlClass.substring(3);
        if (language in sh_languages) {
          sh_highlightElement(element, sh_languages[language]);
          highlighted = true;
        } else if (typeof (prefix) === 'string' &&
                   typeof (suffix) === 'string') {
          sh_load(language, element, prefix, suffix);
        } else {
          throw `Found <${tag}> element with class="${htmlClass}", but no such language exists`;
        }
        break;
      }
    }
    if (highlighted === false && donthighlight === false) {
      sh_highlightElement(element, sh_languages.javascript);
    }
  }
}

function sh_highlightDocument(prefix, suffix) {
  highlight(prefix, suffix, 'tt');
  highlight(prefix, suffix, 'code');
  highlight(prefix, suffix, 'pre');
}

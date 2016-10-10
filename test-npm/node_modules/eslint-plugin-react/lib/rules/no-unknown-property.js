/**
 * @fileoverview Prevent usage of unknown DOM property
 * @author Yannick Croissant
 */
'use strict';

// ------------------------------------------------------------------------------
// Constants
// ------------------------------------------------------------------------------

var UNKNOWN_MESSAGE = 'Unknown property \'{{name}}\' found, use \'{{standardName}}\' instead';

var DOM_ATTRIBUTE_NAMES = {
  // Standard
  'accept-charset': 'acceptCharset',
  class: 'className',
  for: 'htmlFor',
  'http-equiv': 'httpEquiv',
  // SVG
  'clip-path': 'clipPath',
  'fill-opacity': 'fillOpacity',
  'font-family': 'fontFamily',
  'font-size': 'fontSize',
  'marker-end': 'markerEnd',
  'marker-mid': 'markerMid',
  'marker-start': 'markerStart',
  'stop-color': 'stopColor',
  'stop-opacity': 'stopOpacity',
  'stroke-dasharray': 'strokeDasharray',
  'stroke-linecap': 'strokeLinecap',
  'stroke-opacity': 'strokeOpacity',
  'stroke-width': 'strokeWidth',
  'text-anchor': 'textAnchor',
  'xlink:actuate': 'xlinkActuate',
  'xlink:arcrole': 'xlinkArcrole',
  'xlink:href': 'xlinkHref',
  'xlink:role': 'xlinkRole',
  'xlink:show': 'xlinkShow',
  'xlink:title': 'xlinkTitle',
  'xlink:type': 'xlinkType',
  'xml:base': 'xmlBase',
  'xml:lang': 'xmlLang',
  'xml:space': 'xmlSpace'
};

var DOM_PROPERTY_NAMES = [
  // Standard
  'acceptCharset', 'accessKey', 'allowFullScreen', 'allowTransparency', 'autoComplete', 'autoFocus', 'autoPlay',
  'cellPadding', 'cellSpacing', 'charSet', 'classID', 'className', 'colSpan', 'contentEditable', 'contextMenu',
  'crossOrigin', 'dateTime', 'encType', 'formAction', 'formEncType', 'formMethod', 'formNoValidate', 'formTarget',
  'frameBorder', 'hrefLang', 'htmlFor', 'httpEquiv', 'marginHeight', 'marginWidth', 'maxLength', 'mediaGroup',
  'noValidate', 'onBlur', 'onChange', 'onClick', 'onContextMenu', 'onCopy', 'onCut', 'onDoubleClick',
  'onDrag', 'onDragEnd', 'onDragEnter', 'onDragExit', 'onDragLeave', 'onDragOver', 'onDragStart', 'onDrop',
  'onFocus', 'onInput', 'onKeyDown', 'onKeyPress', 'onKeyUp', 'onMouseDown', 'onMouseEnter', 'onMouseLeave',
  'onMouseMove', 'onMouseOut', 'onMouseOver', 'onMouseUp', 'onPaste', 'onScroll', 'onSubmit', 'onTouchCancel',
  'onTouchEnd', 'onTouchMove', 'onTouchStart', 'onWheel',
  'radioGroup', 'readOnly', 'rowSpan', 'spellCheck', 'srcDoc', 'srcSet', 'tabIndex', 'useMap',
  // Non standard
  'autoCapitalize', 'autoCorrect',
  'autoSave',
  'itemProp', 'itemScope', 'itemType', 'itemRef', 'itemID'
];

// ------------------------------------------------------------------------------
// Helpers
// ------------------------------------------------------------------------------

/**
 * Checks if a node matches the JSX tag convention.
 * @param {Object} node - JSX element being tested.
 * @returns {boolean} Whether or not the node name match the JSX tag convention.
 */
var tagConvention = /^[a-z][^-]*$/;
function isTagName(node) {
  if (tagConvention.test(node.parent.name.name)) {
    // http://www.w3.org/TR/custom-elements/#type-extension-semantics
    return !node.parent.attributes.some(function(attrNode) {
      return (
        attrNode.type === 'JSXAttribute' &&
        attrNode.name.type === 'JSXIdentifier' &&
        attrNode.name.name === 'is'
      );
    });
  }
  return false;
}

/**
 * Get the standard name of the attribute.
 * @param {String} name - Name of the attribute.
 * @returns {String} The standard name of the attribute.
 */
function getStandardName(name) {
  if (DOM_ATTRIBUTE_NAMES[name]) {
    return DOM_ATTRIBUTE_NAMES[name];
  }
  var i;
  var found = DOM_PROPERTY_NAMES.some(function(element, index) {
    i = index;
    return element.toLowerCase() === name;
  });
  return found ? DOM_PROPERTY_NAMES[i] : null;
}

// ------------------------------------------------------------------------------
// Rule Definition
// ------------------------------------------------------------------------------

module.exports = function(context) {

  var sourceCode = context.getSourceCode();

  return {

    JSXAttribute: function(node) {
      var name = sourceCode.getText(node.name);
      var standardName = getStandardName(name);
      if (!isTagName(node) || !standardName) {
        return;
      }
      context.report({
        node: node,
        message: UNKNOWN_MESSAGE,
        data: {
          name: name,
          standardName: standardName
        },
        fix: function(fixer) {
          return fixer.replaceText(node.name, standardName);
        }
      });
    }
  };

};

module.exports.schema = [];

'use strict';

/* Expose. */
module.exports = getPropertyInformation;

/* Constants. */
var USE_ATTRIBUTE = 0x1;
var USE_PROPERTY = 0x2;
var BOOLEAN_VALUE = 0x8;
var NUMERIC_VALUE = 0x10;
var POSITIVE_NUMERIC_VALUE = 0x20 | 0x10;
var OVERLOADED_BOOLEAN_VALUE = 0x40;
var SPACE_SEPARATED = 0x80;
var COMMA_SEPARATED = 0x100;

/* Map of properties. Names are camel-cased properties. */
var propertyConfig = {
  /* Standard Properties. */
  abbr: null,
  accept: COMMA_SEPARATED,
  acceptCharset: SPACE_SEPARATED,
  accessKey: SPACE_SEPARATED,
  action: null,
  allowFullScreen: USE_ATTRIBUTE | BOOLEAN_VALUE,
  allowTransparency: USE_ATTRIBUTE,
  alt: null,
  /* https://html.spec.whatwg.org/#attr-link-as */
  as: null,
  async: BOOLEAN_VALUE,
  autoComplete: SPACE_SEPARATED,
  autoFocus: BOOLEAN_VALUE,
  autoPlay: BOOLEAN_VALUE,
  capture: USE_ATTRIBUTE | BOOLEAN_VALUE,
  cellPadding: null,
  cellSpacing: null,
  challenge: USE_ATTRIBUTE,
  charSet: USE_ATTRIBUTE,
  checked: USE_PROPERTY | BOOLEAN_VALUE,
  cite: null,
  /* To set className on SVG elements, it's necessary to
   * use .setAttribute; this works on HTML elements too
   * in all browsers except IE8. */
  className: USE_ATTRIBUTE | SPACE_SEPARATED,
  cols: USE_ATTRIBUTE | POSITIVE_NUMERIC_VALUE,
  colSpan: null,
  command: null,
  content: null,
  contentEditable: null,
  contextMenu: USE_ATTRIBUTE,
  controls: USE_PROPERTY | BOOLEAN_VALUE,
  /* https://github.com/WICG/controls-list/blob/gh-pages/explainer.md */
  controlsList: SPACE_SEPARATED,
  coords: NUMERIC_VALUE | COMMA_SEPARATED,
  crossOrigin: null,
  /* For `<object />` acts as `src`. */
  data: null,
  dateTime: USE_ATTRIBUTE,
  default: BOOLEAN_VALUE,
  defer: BOOLEAN_VALUE,
  dir: null,
  dirName: null,
  disabled: USE_ATTRIBUTE | BOOLEAN_VALUE,
  download: OVERLOADED_BOOLEAN_VALUE,
  draggable: null,
  dropzone: SPACE_SEPARATED,
  encType: null,
  form: USE_ATTRIBUTE,
  formAction: USE_ATTRIBUTE,
  formEncType: USE_ATTRIBUTE,
  formMethod: USE_ATTRIBUTE,
  formNoValidate: BOOLEAN_VALUE,
  formTarget: USE_ATTRIBUTE,
  frameBorder: USE_ATTRIBUTE,
  headers: SPACE_SEPARATED,
  height: USE_ATTRIBUTE | POSITIVE_NUMERIC_VALUE,
  hidden: USE_ATTRIBUTE | BOOLEAN_VALUE,
  high: NUMERIC_VALUE,
  href: null,
  hrefLang: null,
  htmlFor: SPACE_SEPARATED,
  httpEquiv: SPACE_SEPARATED,
  id: USE_PROPERTY,
  inputMode: USE_ATTRIBUTE,
  /* Web Components */
  is: USE_ATTRIBUTE,
  isMap: BOOLEAN_VALUE,
  keyParams: USE_ATTRIBUTE,
  keyType: USE_ATTRIBUTE,
  kind: null,
  label: null,
  lang: null,
  list: USE_ATTRIBUTE,
  loop: USE_PROPERTY | BOOLEAN_VALUE,
  low: NUMERIC_VALUE,
  manifest: USE_ATTRIBUTE,
  marginHeight: NUMERIC_VALUE,
  marginWidth: NUMERIC_VALUE,
  max: null,
  maxLength: USE_ATTRIBUTE | POSITIVE_NUMERIC_VALUE,
  media: USE_ATTRIBUTE,
  mediaGroup: null,
  menu: null,
  method: null,
  min: null,
  minLength: USE_ATTRIBUTE | POSITIVE_NUMERIC_VALUE,
  multiple: USE_PROPERTY | BOOLEAN_VALUE,
  muted: USE_PROPERTY | BOOLEAN_VALUE,
  name: null,
  nonce: null,
  noValidate: BOOLEAN_VALUE,
  open: BOOLEAN_VALUE,
  optimum: NUMERIC_VALUE,
  pattern: null,
  ping: SPACE_SEPARATED,
  placeholder: null,
  /* https://html.spec.whatwg.org/#attr-video-playsinline */
  playsInline: BOOLEAN_VALUE,
  poster: null,
  preload: null,
  /* https://html.spec.whatwg.org/#dom-head-profile */
  profile: null,
  radioGroup: null,
  readOnly: USE_PROPERTY | BOOLEAN_VALUE,
  /* https://html.spec.whatwg.org/#attr-link-referrerpolicy */
  referrerPolicy: null,
  /* `rel` is `relList` in DOM */
  rel: SPACE_SEPARATED | USE_ATTRIBUTE,
  required: BOOLEAN_VALUE,
  reversed: BOOLEAN_VALUE,
  role: USE_ATTRIBUTE,
  rows: USE_ATTRIBUTE | POSITIVE_NUMERIC_VALUE,
  rowSpan: POSITIVE_NUMERIC_VALUE,
  sandbox: SPACE_SEPARATED,
  scope: null,
  scoped: BOOLEAN_VALUE,
  scrolling: null,
  seamless: USE_ATTRIBUTE | BOOLEAN_VALUE,
  selected: USE_PROPERTY | BOOLEAN_VALUE,
  shape: null,
  size: USE_ATTRIBUTE | POSITIVE_NUMERIC_VALUE,
  sizes: USE_ATTRIBUTE | SPACE_SEPARATED,
  /* https://html.spec.whatwg.org/#attr-slot */
  slot: null,
  sortable: BOOLEAN_VALUE,
  sorted: SPACE_SEPARATED,
  span: POSITIVE_NUMERIC_VALUE,
  spellCheck: null,
  src: null,
  srcDoc: USE_PROPERTY,
  srcLang: null,
  srcSet: USE_ATTRIBUTE | COMMA_SEPARATED,
  start: NUMERIC_VALUE,
  step: null,
  style: null,
  summary: null,
  tabIndex: NUMERIC_VALUE,
  target: null,
  title: null,
  translate: null,
  type: null,
  typeMustMatch: BOOLEAN_VALUE,
  useMap: null,
  value: USE_PROPERTY,
  volume: POSITIVE_NUMERIC_VALUE,
  width: USE_ATTRIBUTE | NUMERIC_VALUE,
  wmode: USE_ATTRIBUTE,
  wrap: null,

  /* Non-standard Properties. */

  /* `autoCapitalize` and `autoCorrect` are supported in
   * Mobile Safari for keyboard hints. */
  autoCapitalize: null,
  autoCorrect: null,
  /* `autoSave` allows WebKit/Blink to persist values of
   * input fields on page reloads */
  autoSave: null,
  /* `itemProp`, `itemScope`, `itemType` are for Microdata
   * support. See http://schema.org/docs/gs.html */
  itemProp: USE_ATTRIBUTE | SPACE_SEPARATED,
  itemScope: USE_ATTRIBUTE | BOOLEAN_VALUE,
  itemType: USE_ATTRIBUTE | SPACE_SEPARATED,
  /* `itemID` and `itemRef` are for Microdata support as well
   * but only specified in the the WHATWG spec document.
   * See https://html.spec.whatwg.org/multipage/
   * microdata.html#microdata-dom-api */
  itemID: USE_ATTRIBUTE,
  itemRef: USE_ATTRIBUTE | SPACE_SEPARATED,
  /* `property` is supported for OpenGraph in meta tags. */
  property: null,
  /* `results` show looking glass icon and recent searches
   * on input search fields in WebKit/Blink */
  results: null,
  /* IE-only attribute that specifies security
   * restrictions on an iframe as an alternative to the
   * sandbox attribute on IE < 10 */
  security: USE_ATTRIBUTE,
  /* IE-only attribute that controls focus behavior */
  unselectable: USE_ATTRIBUTE,

  /* Ancient. */
  xmlLang: USE_ATTRIBUTE,
  xmlBase: USE_ATTRIBUTE
};

/* Map of properties to attributes.
 * Names are lower-case properties.
 * Values are HTML attributes. */
var propertyToAttributeMapping = {
  xmlbase: 'xml:base',
  xmllang: 'xml:lang',
  classname: 'class',
  htmlfor: 'for',
  httpequiv: 'http-equiv',
  acceptcharset: 'accept-charset'
};

/* Expand config. */
var information = {};
var property;
var name;
var config;

getPropertyInformation.all = information;

for (property in propertyConfig) {
  name = lower(property);
  name = propertyToAttributeMapping[name] || name;
  config = propertyConfig[property];

  information[name] = {
    name: name,
    propertyName: property,
    mustUseAttribute: check(config, USE_ATTRIBUTE),
    mustUseProperty: check(config, USE_PROPERTY),
    boolean: check(config, BOOLEAN_VALUE),
    overloadedBoolean: check(config, OVERLOADED_BOOLEAN_VALUE),
    numeric: check(config, NUMERIC_VALUE),
    positiveNumeric: check(config, POSITIVE_NUMERIC_VALUE),
    commaSeparated: check(config, COMMA_SEPARATED),
    spaceSeparated: check(config, SPACE_SEPARATED)
  };
}

/* Get a config for a property. */
function getPropertyInformation(propertyName) {
  var insensitive = lower(propertyName);

  return information[propertyToAttributeMapping[insensitive] || insensitive];
}

/* Check a mask. */
function check(value, bitmask) {
  return (value & bitmask) === bitmask;
}

/* Lower-case a string. */
function lower(value) {
  return value.toLowerCase();
}

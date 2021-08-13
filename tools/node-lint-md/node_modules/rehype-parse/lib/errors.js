export const errors = {
  abandonedHeadElementChild: {
    reason: 'Unexpected metadata element after head',
    description:
      'Unexpected element after head. Expected the element before `</head>`',
    url: false
  },
  abruptClosingOfEmptyComment: {
    reason: 'Unexpected abruptly closed empty comment',
    description: 'Unexpected `>` or `->`. Expected `-->` to close comments'
  },
  abruptDoctypePublicIdentifier: {
    reason: 'Unexpected abruptly closed public identifier',
    description:
      'Unexpected `>`. Expected a closing `"` or `\'` after the public identifier'
  },
  abruptDoctypeSystemIdentifier: {
    reason: 'Unexpected abruptly closed system identifier',
    description:
      'Unexpected `>`. Expected a closing `"` or `\'` after the identifier identifier'
  },
  absenceOfDigitsInNumericCharacterReference: {
    reason: 'Unexpected non-digit at start of numeric character reference',
    description:
      'Unexpected `%c`. Expected `[0-9]` for decimal references or `[0-9a-fA-F]` for hexadecimal references'
  },
  cdataInHtmlContent: {
    reason: 'Unexpected CDATA section in HTML',
    description:
      'Unexpected `<![CDATA[` in HTML. Remove it, use a comment, or encode special characters instead'
  },
  characterReferenceOutsideUnicodeRange: {
    reason: 'Unexpected too big numeric character reference',
    description:
      'Unexpectedly high character reference. Expected character references to be at most hexadecimal 10ffff (or decimal 1114111)'
  },
  closingOfElementWithOpenChildElements: {
    reason: 'Unexpected closing tag with open child elements',
    description:
      'Unexpectedly closing tag. Expected other tags to be closed first',
    url: false
  },
  controlCharacterInInputStream: {
    reason: 'Unexpected control character',
    description:
      'Unexpected control character `%x`. Expected a non-control code point, 0x00, or ASCII whitespace'
  },
  controlCharacterReference: {
    reason: 'Unexpected control character reference',
    description:
      'Unexpectedly control character in reference. Expected a non-control code point, 0x00, or ASCII whitespace'
  },
  disallowedContentInNoscriptInHead: {
    reason: 'Disallowed content inside `<noscript>` in `<head>`',
    description:
      'Unexpected text character `%c`. Only use text in `<noscript>`s in `<body>`',
    url: false
  },
  duplicateAttribute: {
    reason: 'Unexpected duplicate attribute',
    description:
      'Unexpectedly double attribute. Expected attributes to occur only once'
  },
  endTagWithAttributes: {
    reason: 'Unexpected attribute on closing tag',
    description: 'Unexpected attribute. Expected `>` instead'
  },
  endTagWithTrailingSolidus: {
    reason: 'Unexpected slash at end of closing tag',
    description: 'Unexpected `%c-1`. Expected `>` instead'
  },
  endTagWithoutMatchingOpenElement: {
    reason: 'Unexpected unopened end tag',
    description: 'Unexpected end tag. Expected no end tag or another end tag',
    url: false
  },
  eofBeforeTagName: {
    reason: 'Unexpected end of file',
    description: 'Unexpected end of file. Expected tag name instead'
  },
  eofInCdata: {
    reason: 'Unexpected end of file in CDATA',
    description: 'Unexpected end of file. Expected `]]>` to close the CDATA'
  },
  eofInComment: {
    reason: 'Unexpected end of file in comment',
    description: 'Unexpected end of file. Expected `-->` to close the comment'
  },
  eofInDoctype: {
    reason: 'Unexpected end of file in doctype',
    description:
      'Unexpected end of file. Expected a valid doctype (such as `<!doctype html>`)'
  },
  eofInElementThatCanContainOnlyText: {
    reason: 'Unexpected end of file in element that can only contain text',
    description: 'Unexpected end of file. Expected text or a closing tag',
    url: false
  },
  eofInScriptHtmlCommentLikeText: {
    reason: 'Unexpected end of file in comment inside script',
    description: 'Unexpected end of file. Expected `-->` to close the comment'
  },
  eofInTag: {
    reason: 'Unexpected end of file in tag',
    description: 'Unexpected end of file. Expected `>` to close the tag'
  },
  incorrectlyClosedComment: {
    reason: 'Incorrectly closed comment',
    description: 'Unexpected `%c-1`. Expected `-->` to close the comment'
  },
  incorrectlyOpenedComment: {
    reason: 'Incorrectly opened comment',
    description: 'Unexpected `%c`. Expected `<!--` to open the comment'
  },
  invalidCharacterSequenceAfterDoctypeName: {
    reason: 'Invalid sequence after doctype name',
    description: 'Unexpected sequence at `%c`. Expected `public` or `system`'
  },
  invalidFirstCharacterOfTagName: {
    reason: 'Invalid first character in tag name',
    description: 'Unexpected `%c`. Expected an ASCII letter instead'
  },
  misplacedDoctype: {
    reason: 'Misplaced doctype',
    description: 'Unexpected doctype. Expected doctype before head',
    url: false
  },
  misplacedStartTagForHeadElement: {
    reason: 'Misplaced `<head>` start tag',
    description:
      'Unexpected start tag `<head>`. Expected `<head>` directly after doctype',
    url: false
  },
  missingAttributeValue: {
    reason: 'Missing attribute value',
    description:
      'Unexpected `%c-1`. Expected an attribute value or no `%c-1` instead'
  },
  missingDoctype: {
    reason: 'Missing doctype before other content',
    description: 'Expected a `<!doctype html>` before anything else',
    url: false
  },
  missingDoctypeName: {
    reason: 'Missing doctype name',
    description: 'Unexpected doctype end at `%c`. Expected `html` instead'
  },
  missingDoctypePublicIdentifier: {
    reason: 'Missing public identifier in doctype',
    description: 'Unexpected `%c`. Expected identifier for `public` instead'
  },
  missingDoctypeSystemIdentifier: {
    reason: 'Missing system identifier in doctype',
    description:
      'Unexpected `%c`. Expected identifier for `system` instead (suggested: `"about:legacy-compat"`)'
  },
  missingEndTagName: {
    reason: 'Missing name in end tag',
    description: 'Unexpected `%c`. Expected an ASCII letter instead'
  },
  missingQuoteBeforeDoctypePublicIdentifier: {
    reason: 'Missing quote before public identifier in doctype',
    description: 'Unexpected `%c`. Expected `"` or `\'` instead'
  },
  missingQuoteBeforeDoctypeSystemIdentifier: {
    reason: 'Missing quote before system identifier in doctype',
    description: 'Unexpected `%c`. Expected `"` or `\'` instead'
  },
  missingSemicolonAfterCharacterReference: {
    reason: 'Missing semicolon after character reference',
    description: 'Unexpected `%c`. Expected `;` instead'
  },
  missingWhitespaceAfterDoctypePublicKeyword: {
    reason: 'Missing whitespace after public identifier in doctype',
    description: 'Unexpected `%c`. Expected ASCII whitespace instead'
  },
  missingWhitespaceAfterDoctypeSystemKeyword: {
    reason: 'Missing whitespace after system identifier in doctype',
    description: 'Unexpected `%c`. Expected ASCII whitespace instead'
  },
  missingWhitespaceBeforeDoctypeName: {
    reason: 'Missing whitespace before doctype name',
    description: 'Unexpected `%c`. Expected ASCII whitespace instead'
  },
  missingWhitespaceBetweenAttributes: {
    reason: 'Missing whitespace between attributes',
    description: 'Unexpected `%c`. Expected ASCII whitespace instead'
  },
  missingWhitespaceBetweenDoctypePublicAndSystemIdentifiers: {
    reason:
      'Missing whitespace between public and system identifiers in doctype',
    description: 'Unexpected `%c`. Expected ASCII whitespace instead'
  },
  nestedComment: {
    reason: 'Unexpected nested comment',
    description: 'Unexpected `<!--`. Expected `-->`'
  },
  nestedNoscriptInHead: {
    reason: 'Unexpected nested `<noscript>` in `<head>`',
    description:
      'Unexpected `<noscript>`. Expected a closing tag or a meta element',
    url: false
  },
  nonConformingDoctype: {
    reason: 'Unexpected non-conforming doctype declaration',
    description:
      'Expected `<!doctype html>` or `<!doctype html system "about:legacy-compat">`',
    url: false
  },
  nonVoidHtmlElementStartTagWithTrailingSolidus: {
    reason: 'Unexpected trailing slash on start tag of non-void element',
    description: 'Unexpected `/`. Expected `>` instead'
  },
  noncharacterCharacterReference: {
    reason:
      'Unexpected noncharacter code point referenced by character reference',
    description: 'Unexpected code point. Do not use noncharacters in HTML'
  },
  noncharacterInInputStream: {
    reason: 'Unexpected noncharacter character',
    description: 'Unexpected code point `%x`. Do not use noncharacters in HTML'
  },
  nullCharacterReference: {
    reason: 'Unexpected NULL character referenced by character reference',
    description: 'Unexpected code point. Do not use NULL characters in HTML'
  },
  openElementsLeftAfterEof: {
    reason: 'Unexpected end of file',
    description: 'Unexpected end of file. Expected closing tag instead',
    url: false
  },
  surrogateCharacterReference: {
    reason: 'Unexpected surrogate character referenced by character reference',
    description:
      'Unexpected code point. Do not use lone surrogate characters in HTML'
  },
  surrogateInInputStream: {
    reason: 'Unexpected surrogate character',
    description:
      'Unexpected code point `%x`. Do not use lone surrogate characters in HTML'
  },
  unexpectedCharacterAfterDoctypeSystemIdentifier: {
    reason: 'Invalid character after system identifier in doctype',
    description: 'Unexpected character at `%c`. Expected `>`'
  },
  unexpectedCharacterInAttributeName: {
    reason: 'Unexpected character in attribute name',
    description:
      'Unexpected `%c`. Expected whitespace, `/`, `>`, `=`, or probably an ASCII letter'
  },
  unexpectedCharacterInUnquotedAttributeValue: {
    reason: 'Unexpected character in unquoted attribute value',
    description: 'Unexpected `%c`. Quote the attribute value to include it'
  },
  unexpectedEqualsSignBeforeAttributeName: {
    reason: 'Unexpected equals sign before attribute name',
    description: 'Unexpected `%c`. Add an attribute name before it'
  },
  unexpectedNullCharacter: {
    reason: 'Unexpected NULL character',
    description:
      'Unexpected code point `%x`. Do not use NULL characters in HTML'
  },
  unexpectedQuestionMarkInsteadOfTagName: {
    reason: 'Unexpected question mark instead of tag name',
    description: 'Unexpected `%c`. Expected an ASCII letter instead'
  },
  unexpectedSolidusInTag: {
    reason: 'Unexpected slash in tag',
    description:
      'Unexpected `%c-1`. Expected it followed by `>` or in a quoted attribute value'
  },
  unknownNamedCharacterReference: {
    reason: 'Unexpected unknown named character reference',
    description:
      'Unexpected character reference. Expected known named character references'
  }
}

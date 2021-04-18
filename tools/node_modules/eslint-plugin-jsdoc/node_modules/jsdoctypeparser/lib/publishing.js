'use strict';

const {OPTIONAL, PARENTHESIS} = require('./NodeType');
const {OptionalTypeSyntax, VariadicTypeSyntax} = require('./SyntaxType');
const {format} = require('util');

/** @typedef {import('./parsing').AstNode} AstNode */
/** @typedef {(node) => string} ConcretePublish */
/** @typedef {{ [T in import('./NodeType').Type]: (node: AstNode, publish: ConcretePublish) => string }} Publisher */

/**
 * @param {AstNode} node
 * @param {Publisher} [opt_publisher]
 * @return {string}
 */
function publish(node, opt_publisher) {
  const publisher = opt_publisher || createDefaultPublisher();
  return publisher[node.type](node, function(childNode) {
    return publish(childNode, publisher);
  });
}

/**
 * @private
 * @param {string} str
 * @param {'none'|'single'|'double'|undefined} quoteStyle
 * @returns {string} Formatted string
 */
function addQuotesForName (str, quoteStyle) {
  // For `MemberName`, not strings
  if (!quoteStyle || quoteStyle === 'none') {
    return str;
  }
  const singleQuoteStyle = quoteStyle === 'single';

  return format(
    singleQuoteStyle
      ? "'%s'"
      : '"%s"',
    str
      .replace(/\\/g, '\\\\')
      .replace(
        singleQuoteStyle ? /'/gu : /"/gu,
        '\\' + (singleQuoteStyle ? "'" : '"')
      )
  );
}

/** @return {Publisher} */
function createDefaultPublisher() {
  return {
    NAME (nameNode) {
      return nameNode.name;
    },
    MEMBER (memberNode, concretePublish) {
      return format('%s.%s%s', concretePublish(memberNode.owner),
                    memberNode.hasEventPrefix ? 'event:' : '',
                    addQuotesForName(memberNode.name, memberNode.quoteStyle));
    },
    UNION (unionNode, concretePublish) {
      return format('%s | %s', concretePublish(unionNode.left),
                    concretePublish(unionNode.right));
    },
    INTERSECTION (unionNode, concretePublish) {
      return format('%s & %s', concretePublish(unionNode.left),
                    concretePublish(unionNode.right));
    },
    VARIADIC (variadicNode, concretePublish) {
      if (variadicNode.meta.syntax === VariadicTypeSyntax.ONLY_DOTS) {
        return '...';
      }
      return format('...%s', concretePublish(variadicNode.value));
    },
    RECORD (recordNode, concretePublish) {
      const concretePublishedEntries = recordNode.entries.map(concretePublish);
      return format('{%s}', concretePublishedEntries.join(', '));
    },
    RECORD_ENTRY (entryNode, concretePublish) {
      const {readonly, value, key, quoteStyle} = entryNode;
      const readonlyString = readonly ? 'readonly ' : '';
      if (!value) return readonlyString + addQuotesForName(key, quoteStyle);
      const keySuffix = (
        value.type === OPTIONAL &&
        value.meta.syntax === OptionalTypeSyntax.SUFFIX_KEY_QUESTION_MARK
      )
        ? '?'
        : '';
      return format('%s%s%s: %s', readonlyString, addQuotesForName(key, quoteStyle), keySuffix, concretePublish(value));
    },
    TUPLE (tupleNode, concretePublish) {
      const concretePublishedEntries = tupleNode.entries.map(concretePublish);
      return format('[%s]', concretePublishedEntries.join(', '));
    },
    GENERIC (genericNode, concretePublish) {
      const concretePublishedObjects = genericNode.objects.map(concretePublish);
      switch (genericNode.meta.syntax) {
      case 'SQUARE_BRACKET':
        return format('%s[]', concretePublishedObjects.join(', '));
      case 'ANGLE_BRACKET_WITH_DOT':
        return format('%s.<%s>', concretePublish(genericNode.subject),
                      concretePublishedObjects.join(', '));
      }
      return format('%s<%s>', concretePublish(genericNode.subject),
                    concretePublishedObjects.join(', '));
    },
    MODULE (moduleNode, concretePublish) {
      return format('module:%s', concretePublish(moduleNode.value));
    },
    FILE_PATH (filePathNode) {
      return addQuotesForName(filePathNode.path, filePathNode.quoteStyle);
    },
    OPTIONAL (optionalNode, concretePublish) {
      if (optionalNode.meta.syntax === OptionalTypeSyntax.SUFFIX_KEY_QUESTION_MARK) {
        return concretePublish(optionalNode.value);
      }
      return format('%s=', concretePublish(optionalNode.value));
    },
    NULLABLE (nullableNode, concretePublish) {
      return format('?%s', concretePublish(nullableNode.value));
    },
    NOT_NULLABLE (notNullableNode, concretePublish) {
      return format('!%s', concretePublish(notNullableNode.value));
    },
    FUNCTION (functionNode, concretePublish) {
      const publidshedParams = functionNode.params.map(concretePublish);

      if (functionNode.new) {
        publidshedParams.unshift(format('new: %s',
          concretePublish(functionNode.new)));
      }

      if (functionNode.this) {
        publidshedParams.unshift(format('this: %s',
          concretePublish(functionNode.this)));
      }

      if (functionNode.returns) {
        return format('function(%s): %s', publidshedParams.join(', '),
                           concretePublish(functionNode.returns));
      }

      return format('function(%s)', publidshedParams.join(', '));
    },
    ARROW (functionNode, concretePublish) {
      const publishedParams = functionNode.params.map(concretePublish);
      return (functionNode.new ? 'new ' : '') + format('(%s) => %s', publishedParams.join(', '), concretePublish(functionNode.returns));
    },
    NAMED_PARAMETER (parameterNode, concretePublish) {
      return parameterNode.name + ': ' + concretePublish(parameterNode.typeName);
    },
    ANY () {
      return '*';
    },
    UNKNOWN () {
      return '?';
    },
    INNER_MEMBER (memberNode, concretePublish) {
      return concretePublish(memberNode.owner) + '~' +
        (memberNode.hasEventPrefix ? 'event:' : '') +
        addQuotesForName(memberNode.name, memberNode.quoteStyle);
    },
    INSTANCE_MEMBER (memberNode, concretePublish) {
      return concretePublish(memberNode.owner) + '#' +
        (memberNode.hasEventPrefix ? 'event:' : '') +
        addQuotesForName(memberNode.name, memberNode.quoteStyle);
    },
    STRING_VALUE (stringValueNode) {
      return addQuotesForName(stringValueNode.string, stringValueNode.quoteStyle)
    },
    NUMBER_VALUE (numberValueNode) {
      return numberValueNode.number;
    },
    EXTERNAL (externalNode /* , concretePublish */) {
      const {name, quoteStyle} = externalNode;
      return format('external:%s', addQuotesForName(name, quoteStyle));
    },
    PARENTHESIS (parenthesizedNode, concretePublish) {
      return format('(%s)', concretePublish(parenthesizedNode.value));
    },
    TYPE_QUERY (typeQueryNode, concretePublish) {
      return format('typeof %s', concretePublish(typeQueryNode.name));
    },
    KEY_QUERY (keyQueryNode, concretePublish) {
      if (keyQueryNode.value.type === PARENTHESIS) {
        return format('keyof%s', concretePublish(keyQueryNode.value));
      }
      return format('keyof %s', concretePublish(keyQueryNode.value));
    },
    IMPORT (importNode, concretePublish) {
      return format('import(%s)', concretePublish(importNode.path));
    },
  };
}


module.exports = {
  publish: publish,
  createDefaultPublisher: createDefaultPublisher,
};

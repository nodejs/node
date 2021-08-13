// Type definitions for parse5 6.0
// Project: https://github.com/inikulin/parse5
// Definitions by: Ivan Nikulin <https://github.com/inikulin>
//                 ExE Boss <https://github.com/ExE-Boss>
//                 James Garbutt <https://github.com/43081j>
// Definitions: https://github.com/DefinitelyTyped/DefinitelyTyped

export interface Location {
    /**
     * One-based column index of the last character
     */
    endCol: number;

    /**
     * Zero-based last character index
     */
    endOffset: number;

    /**
     * One-based line index of the last character
     */
    endLine: number;

    /**
     * One-based column index of the first character
     */
    startCol: number;

    /**
     * Zero-based first character index
     */
    startOffset: number;

    /**
     * One-based line index of the first character
     */
    startLine: number;
}

export interface AttributesLocation {
    [attributeName: string]: Location;
}

export interface StartTagLocation extends Location {
    /**
     * Start tag attributes' location info
     */
    attrs: AttributesLocation;
}

export interface ElementLocation extends StartTagLocation {
    /**
     * Element's start tag location info.
     */
    startTag: StartTagLocation;

    /**
     * Element's end tag location info.
     */
    endTag: Location;
}

export interface ParserOptions<T extends TreeAdapter = TreeAdapter> {
    /**
     * The [scripting flag](https://html.spec.whatwg.org/multipage/parsing.html#scripting-flag). If set
     * to `true`, `noscript` element content will be parsed as text.
     *
     *  **Default:** `true`
     */
    scriptingEnabled?: boolean | undefined;

    /**
     * Enables source code location information. When enabled, each node (except the root node)
     * will have a `sourceCodeLocation` property. If the node is not an empty element, `sourceCodeLocation` will
     * be a {@link ElementLocation} object, otherwise it will be {@link Location}.
     * If the element was implicitly created by the parser (as part of
     * [tree correction](https://html.spec.whatwg.org/multipage/syntax.html#an-introduction-to-error-handling-and-strange-cases-in-the-parser)),
     * its `sourceCodeLocation` property will be `undefined`.
     *
     * **Default:** `false`
     */
    sourceCodeLocationInfo?: boolean | undefined;

    /**
     * Specifies the resulting tree format.
     *
     * **Default:** `treeAdapters.default`
     */
    treeAdapter?: T | undefined;
}

export interface SerializerOptions<T extends TreeAdapter = TreeAdapter> {
    /***
     * Specifies input tree format.
     *
     * **Default:** `treeAdapters.default`
     */
    treeAdapter?: T | undefined;
}

/**
 * [Document mode](https://dom.spec.whatwg.org/#concept-document-limited-quirks).
 */
export type DocumentMode = "no-quirks" | "quirks" | "limited-quirks";

// Default tree adapter

/**
 * Element attribute.
 */
export interface Attribute {
    /**
     * The name of the attribute.
     */
    name: string;

    /**
     * The value of the attribute.
     */
    value: string;

    /**
     * The namespace of the attribute.
     */
    namespace?: string | undefined;

    /**
     * The namespace-related prefix of the attribute.
     */
    prefix?: string | undefined;
}

/**
 * Default tree adapter DocumentType interface.
 */
export interface DocumentType {
    /**
     * The name of the node.
     */
    nodeName: "#documentType";

    /**
     * Document type name.
     */
    name: string;

    /**
     * Document type public identifier.
     */
    publicId: string;

    /**
     * Document type system identifier.
     */
    systemId: string;
}

/**
 * Default tree adapter Document interface.
 */
export interface Document {
    /**
     * The name of the node.
     */
    nodeName: "#document";

    /**
     * [Document mode](https://dom.spec.whatwg.org/#concept-document-limited-quirks).
     */
    mode: DocumentMode;

    /**
     * Child nodes.
     */
    childNodes: ChildNode[];
}

/**
 * Default tree adapter DocumentFragment interface.
 */
export interface DocumentFragment {
    /**
     * The name of the node.
     */
    nodeName: "#document-fragment";

    /**
     * Child nodes.
     */
    childNodes: ChildNode[];
}

/**
 * Default tree adapter Element interface.
 */
export interface Element {
    /**
     * The name of the node. Equals to element {@link tagName}.
     */
    nodeName: string;

    /**
     * Element tag name.
     */
    tagName: string;

    /**
     * Element namespace.
     */
    namespaceURI: string;

    /**
     * List of element attributes.
     */
    attrs: Attribute[];

    /**
     * Element source code location info. Available if location info is enabled via {@link ParserOptions}.
     */
    sourceCodeLocation?: ElementLocation | undefined;

    /**
     * Child nodes.
     */
    childNodes: ChildNode[];

    /**
     * Parent node.
     */
    parentNode: ParentNode;
}

/**
 * Default tree adapter CommentNode interface.
 */
export interface CommentNode {
    /**
     * The name of the node.
     */
    nodeName: "#comment";

    /**
     * Comment text.
     */
    data: string;

    /**
     * Comment source code location info. Available if location info is enabled via {@link ParserOptions}.
     */
    sourceCodeLocation?: Location | undefined;

    /**
     * Parent node.
     */
    parentNode: ParentNode;
}

/**
 * Default tree adapter TextNode interface.
 */
export interface TextNode {
    /**
     * The name of the node.
     */
    nodeName: "#text";

    /**
     * Text content.
     */
    value: string;

    /**
     * Text node source code location info. Available if location info is enabled via {@link ParserOptions}.
     */
    sourceCodeLocation?: Location | undefined;

    /**
     * Parent node.
     */
    parentNode: ParentNode;
}

/**
 * Default tree adapter Node interface.
 */
export type Node = CommentNode | Document | DocumentFragment | DocumentType | Element | TextNode;

/**
 * Default tree adapter ChildNode type.
 */
export type ChildNode = TextNode | Element | CommentNode;

/**
 * Default tree adapter ParentNode type.
 */
export type ParentNode = Document | DocumentFragment | Element;

export interface TreeAdapterTypeMap {
    attribute: unknown;
    childNode: unknown;
    commentNode: unknown;
    document: unknown;
    documentFragment: unknown;
    documentType: unknown;
    element: unknown;
    node: unknown;
    parentNode: unknown;
    textNode: unknown;
}

export interface TreeAdapter {
    adoptAttributes(recipient: unknown, attrs: unknown[]): void;
    appendChild(parentNode: unknown, newNode: unknown): void;
    createCommentNode(data: string): unknown;
    createDocument(): unknown;
    createDocumentFragment(): unknown;
    createElement(
        tagName: string,
        namespaceURI: string,
        attrs: unknown[]
    ): unknown;
    detachNode(node: unknown): void;
    getAttrList(element: unknown): unknown[];
    getChildNodes(node: unknown): unknown[];
    getCommentNodeContent(commentNode: unknown): string;
    getDocumentMode(document: unknown): unknown;
    getDocumentTypeNodeName(doctypeNode: unknown): string;
    getDocumentTypeNodePublicId(doctypeNode: unknown): string;
    getDocumentTypeNodeSystemId(doctypeNode: unknown): string;
    getFirstChild(node: unknown): unknown;
    getNamespaceURI(element: unknown): string;
    getNodeSourceCodeLocation(node: unknown): Location | StartTagLocation | ElementLocation;
    getParentNode(node: unknown): unknown;
    getTagName(element: unknown): string;
    getTextNodeContent(textNode: unknown): string;
    getTemplateContent(templateElement: unknown): unknown;
    insertBefore(
        parentNode: unknown,
        newNode: unknown,
        referenceNode: unknown
    ): void;
    insertText(parentNode: unknown, text: string): void;
    insertTextBefore(
        parentNode: unknown,
        text: string,
        referenceNode: unknown
    ): void;
    isCommentNode(node: unknown): boolean;
    isDocumentTypeNode(node: unknown): boolean;
    isElementNode(node: unknown): boolean;
    isTextNode(node: unknown): boolean;
    setDocumentMode(document: unknown, mode: DocumentMode): void;
    setDocumentType(
        document: unknown,
        name: string,
        publicId: string,
        systemId: string
    ): void;
    setNodeSourceCodeLocation(node: unknown, location: Location | StartTagLocation | ElementLocation): void;
    setTemplateContent(
        templateElement: unknown,
        contentElement: unknown
    ): void;
}

/**
 * Tree adapter is a set of utility functions that provides minimal required abstraction layer beetween parser and a specific AST format.
 * Note that `TreeAdapter` is not designed to be a general purpose AST manipulation library. You can build such library
 * on top of existing `TreeAdapter` or use one of the existing libraries from npm.
 *
 * @see [default implementation](https://github.com/inikulin/parse5/blob/master/packages/parse5/lib/tree-adapters/default.js)
 */
export interface TypedTreeAdapter<T extends TreeAdapterTypeMap> extends TreeAdapter {
    /**
     * Copies attributes to the given element. Only attributes that are not yet present in the element are copied.
     *
     * @param recipient - Element to copy attributes into.
     * @param attrs - Attributes to copy.
     */
    adoptAttributes(recipient: T['element'], attrs: Array<T['attribute']>): void;

    /**
     * Appends a child node to the given parent node.
     *
     * @param parentNode - Parent node.
     * @param newNode -  Child node.
     */
    appendChild(parentNode: T['parentNode'], newNode: T['node']): void;

    /**
     * Creates a comment node.
     *
     * @param data - Comment text.
     */
    createCommentNode(data: string): T['commentNode'];

    /**
     * Creates a document node.
     */
    createDocument(): T['document'];

    /**
     * Creates a document fragment node.
     */
    createDocumentFragment(): T['documentFragment'];

    /**
     * Creates an element node.
     *
     * @param tagName - Tag name of the element.
     * @param namespaceURI - Namespace of the element.
     * @param attrs - Attribute name-value pair array. Foreign attributes may contain `namespace` and `prefix` fields as well.
     */
    createElement(
        tagName: string,
        namespaceURI: string,
        attrs: Array<T['attribute']>
    ): T['element'];

    /**
     * Removes a node from its parent.
     *
     * @param node - Node to remove.
     */
    detachNode(node: T['node']): void;

    /**
     * Returns the given element's attributes in an array, in the form of name-value pairs.
     * Foreign attributes may contain `namespace` and `prefix` fields as well.
     *
     * @param element - Element.
     */
    getAttrList(element: T['element']): Array<T['attribute']>;

    /**
     * Returns the given node's children in an array.
     *
     * @param node - Node.
     */
    getChildNodes(node: T['parentNode']): Array<T['childNode']>;

    /**
     * Returns the given comment node's content.
     *
     * @param commentNode - Comment node.
     */
    getCommentNodeContent(commentNode: T['commentNode']): string;

    /**
     * Returns [document mode](https://dom.spec.whatwg.org/#concept-document-limited-quirks).
     *
     * @param document - Document node.
     */
    getDocumentMode(document: T['document']): DocumentMode;

    /**
     * Returns the given document type node's name.
     *
     * @param doctypeNode - Document type node.
     */
    getDocumentTypeNodeName(doctypeNode: T['documentType']): string;

    /**
     * Returns the given document type node's public identifier.
     *
     * @param doctypeNode - Document type node.
     */
    getDocumentTypeNodePublicId(doctypeNode: T['documentType']): string;

    /**
     * Returns the given document type node's system identifier.
     *
     * @param doctypeNode - Document type node.
     */
    getDocumentTypeNodeSystemId(doctypeNode: T['documentType']): string;

    /**
     * Returns the first child of the given node.
     *
     * @param node - Node.
     */
    getFirstChild(node: T['parentNode']): T['childNode']|undefined;

    /**
     * Returns the given element's namespace.
     *
     * @param element - Element.
     */
    getNamespaceURI(element: T['element']): string;

    /**
     * Returns the given node's source code location information.
     *
     * @param node - Node.
     */
    getNodeSourceCodeLocation(node: T['node']): Location | StartTagLocation | ElementLocation;

    /**
     * Returns the given node's parent.
     *
     * @param node - Node.
     */
    getParentNode(node: T['childNode']): T['parentNode'];

    /**
     * Returns the given element's tag name.
     *
     * @param element - Element.
     */
    getTagName(element: T['element']): string;

    /**
     * Returns the given text node's content.
     *
     * @param textNode - Text node.
     */
    getTextNodeContent(textNode: T['textNode']): string;

    /**
     * Returns the `<template>` element content element.
     *
     * @param templateElement - `<template>` element.
     */
    getTemplateContent(templateElement: T['element']): T['documentFragment'];

    /**
     * Inserts a child node to the given parent node before the given reference node.
     *
     * @param parentNode - Parent node.
     * @param newNode -  Child node.
     * @param referenceNode -  Reference node.
     */
    insertBefore(
        parentNode: T['parentNode'],
        newNode: T['node'],
        referenceNode: T['node']
    ): void;

    /**
     * Inserts text into a node. If the last child of the node is a text node, the provided text will be appended to the
     * text node content. Otherwise, inserts a new text node with the given text.
     *
     * @param parentNode - Node to insert text into.
     * @param text - Text to insert.
     */
    insertText(parentNode: T['parentNode'], text: string): void;

    /**
     * Inserts text into a sibling node that goes before the reference node. If this sibling node is the text node,
     * the provided text will be appended to the text node content. Otherwise, inserts a new sibling text node with
     * the given text before the reference node.
     *
     * @param parentNode - Node to insert text into.
     * @param text - Text to insert.
     * @param referenceNode - Node to insert text before.
     */
    insertTextBefore(
        parentNode: T['parentNode'],
        text: string,
        referenceNode: T['node']
    ): void;

    /**
     * Determines if the given node is a comment node.
     *
     * @param node - Node.
     */
    isCommentNode(node: T['node']): node is T['commentNode'];

    /**
     * Determines if the given node is a document type node.
     *
     * @param node - Node.
     */
    isDocumentTypeNode(node: T['node']): node is T['documentType'];

    /**
     * Determines if the given node is an element.
     *
     * @param node - Node.
     */
    isElementNode(node: T['node']): node is T['element'];

    /**
     * Determines if the given node is a text node.
     *
     * @param node - Node.
     */
    isTextNode(node: T['node']): node is T['textNode'];

    /**
     * Sets the [document mode](https://dom.spec.whatwg.org/#concept-document-limited-quirks).
     *
     * @param document - Document node.
     * @param mode - Document mode.
     */
    setDocumentMode(document: T['document'], mode: DocumentMode): void;

    /**
     * Sets the document type. If the `document` already contains a document type node, the `name`, `publicId` and `systemId`
     * properties of this node will be updated with the provided values. Otherwise, creates a new document type node
     * with the given properties and inserts it into the `document`.
     *
     * @param document - Document node.
     * @param name -  Document type name.
     * @param publicId - Document type public identifier.
     * @param systemId - Document type system identifier.
     */
    setDocumentType(
        document: T['document'],
        name: string,
        publicId: string,
        systemId: string
    ): void;

    /**
     * Attaches source code location information to the node.
     *
     * @param node - Node.
     */
    setNodeSourceCodeLocation(node: T['node'], location: Location | StartTagLocation | ElementLocation): void;

    /**
     * Sets the `<template>` element content element.
     *
     * @param templateElement - `<template>` element.
     * @param contentElement -  Content element.
     */
    setTemplateContent(
        templateElement: T['element'],
        contentElement: T['documentFragment']
    ): void;
}

/**
 * Parses an HTML string.
 *
 * @param html - Input HTML string.
 * @param options - Parsing options.
 *
 * @example
 * ```js
 *
 * const parse5 = require('parse5');
 *
 * const document = parse5.parse('<!DOCTYPE html><html><head></head><body>Hi there!</body></html>');
 *
 * console.log(document.childNodes[1].tagName); //> 'html'
 * ```
 */
export function parse<T extends TreeAdapter = typeof import('./lib/tree-adapters/default')>(
    html: string,
    options?: ParserOptions<T>): T extends TypedTreeAdapter<infer TMap> ? TMap['document'] : Document;

/**
 * Parses an HTML fragment.
 *
 * @param fragmentContext - Parsing context element. If specified, given fragment will be parsed as if it was set to the context element's `innerHTML` property.
 * @param html - Input HTML fragment string.
 * @param options - Parsing options.
 *
 * @example
 * ```js
 *
 * const parse5 = require('parse5');
 *
 * const documentFragment = parse5.parseFragment('<table></table>');
 *
 * console.log(documentFragment.childNodes[0].tagName); //> 'table'
 *
 * // Parses the html fragment in the context of the parsed <table> element.
 * const trFragment = parser.parseFragment(documentFragment.childNodes[0], '<tr><td>Shake it, baby</td></tr>');
 *
 * console.log(trFragment.childNodes[0].childNodes[0].tagName); //> 'td'
 * ```
 */
export function parseFragment<T extends TreeAdapter = typeof import('./lib/tree-adapters/default')>(
    html: string,
    options?: ParserOptions<T>
): T extends TypedTreeAdapter<infer TMap> ? TMap['documentFragment'] : DocumentFragment;
export function parseFragment<T extends TreeAdapter = typeof import('./lib/tree-adapters/default')>(
    fragmentContext: Element,
    html: string,
    options?: ParserOptions<T>): T extends TypedTreeAdapter<infer TMap> ? TMap['documentFragment'] : DocumentFragment;

/**
 * Serializes an AST node to an HTML string.
 *
 * @param node - Node to serialize.
 * @param options - Serialization options.
 *
 * @example
 * ```js
 *
 * const parse5 = require('parse5');
 *
 * const document = parse5.parse('<!DOCTYPE html><html><head></head><body>Hi there!</body></html>');
 *
 * // Serializes a document.
 * const html = parse5.serialize(document);
 *
 * // Serializes the <html> element content.
 * const str = parse5.serialize(document.childNodes[1]);
 *
 * console.log(str); //> '<head></head><body>Hi there!</body>'
 * ```
 */
export function serialize<T extends TreeAdapter = typeof import('./lib/tree-adapters/default')>(
    node: T extends TypedTreeAdapter<infer TMap> ? TMap['node'] : Node,
    options?: SerializerOptions<T>): string;

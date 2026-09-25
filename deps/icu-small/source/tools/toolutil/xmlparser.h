// © 2016 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html
/*
*******************************************************************************
*
*   Copyright (C) 2004-2005, International Business Machines
*   Corporation and others.  All Rights Reserved.
*
*******************************************************************************
*   file name:  xmlparser.h
*   encoding:   UTF-8
*   tab size:   8 (not used)
*   indentation:4
*
*   created on: 2004jul21
*   created by: Andy Heninger
*
* Tiny XML parser using ICU and intended for use in ICU tests and in build tools.
* Not suitable for production use. Not supported.
* Not conformant. Not efficient.
* But very small.
*/

#ifndef __XMLPARSER_H__
#define __XMLPARSER_H__

#include "unicode/uobject.h"
#include "unicode/unistr.h"
#include "unicode/regex.h"
#include "uvector.h"
#include "hash.h"

#if !UCONFIG_NO_REGULAR_EXPRESSIONS && !UCONFIG_NO_CONVERSION

enum UXMLNodeType {
    /** Node type string (text contents), stored as a UnicodeString. */
    UXML_NODE_TYPE_STRING,
    /** Node type element, stored as a UXMLElement. */
    UXML_NODE_TYPE_ELEMENT,
    UXML_NODE_TYPE_COUNT
};

U_NAMESPACE_BEGIN

class UXMLParser;

/**
 * This class represents an element node in a parsed XML tree.
 */
class U_TOOLUTIL_API UXMLElement : public UObject {
public:
    /**
     * Destructor.
     */
    virtual ~UXMLElement();

    /**
     * Get the tag name of this element.
     */
    const UnicodeString &getTagName() const;
    /**
     * Get the text contents of the element.
     * Append the contents of all text child nodes.
     * @param recurse If true, also recursively appends the contents of all
     *        text child nodes of element children.
     * @return The text contents.
     */
    UnicodeString getText(UBool recurse) const;
    /**
     * Get the number of attributes.
     */
    int32_t countAttributes() const;
    /**
     * Get the i-th attribute.
     * @param i Index of the attribute.
     * @param name Output parameter, receives the attribute name.
     * @param value Output parameter, receives the attribute value.
     * @return A pointer to the attribute value (may be &value or a pointer to an
     *         internal string object), or nullptr if i is out of bounds.
     */
    const UnicodeString *getAttribute(int32_t i, UnicodeString &name, UnicodeString &value) const;
    /**
     * Get the value of the attribute with the given name.
     * @param name Attribute name to be looked up.
     * @return A pointer to the attribute value, or nullptr if this element
     * does not have this attribute.
     */
    const UnicodeString *getAttribute(const UnicodeString &name) const;
    /**
     * Get the number of child nodes.
     */
    int32_t countChildren() const;
    /**
     * Get the i-th child node.
     * @param i Index of the child node.
     * @param type The child node type.
     * @return A pointer to the child node object, or nullptr if i is out of bounds.
     */
    const UObject *getChild(int32_t i, UXMLNodeType &type) const;
    /**
     * Get the next child element node, skipping non-element child nodes.
     * @param i Enumeration index; initialize to 0 before getting the first child element.
     * @return A pointer to the next child element, or nullptr if there is none.
     */
    const UXMLElement *nextChildElement(int32_t &i) const;
    /**
     * Get the immediate child element with the given name.
     * If there are multiple child elements with this name, then return
     * the first one.
     * @param name Element name to be looked up.
     * @return A pointer to the element node, or nullptr if this element
     * does not have this immediate child element.
     */
    const UXMLElement *getChildElement(const UnicodeString &name) const;

    /**
     * ICU "poor man's RTTI", returns a UClassID for the actual class.
     */
    virtual UClassID getDynamicClassID() const override;

    /**
     * ICU "poor man's RTTI", returns a UClassID for this class.
     */
    static UClassID U_EXPORT2 getStaticClassID();

private:
    // prevent default construction etc.
    UXMLElement();
    UXMLElement(const UXMLElement &other);
    UXMLElement &operator=(const UXMLElement &other);

    void appendText(UnicodeString &text, UBool recurse) const;

    friend class UXMLParser;

    UXMLElement(const UXMLParser *parser, const UnicodeString *name, UErrorCode &errorCode);

    const UXMLParser *fParser;
    const UnicodeString *fName;          // The tag name of this element (owned by the UXMLParser)
    UnicodeString       fContent;        // The text content of this node.  All element content is 
                                         //   concatenated even when there are intervening nested elements
                                         //   (which doesn't happen with most xml files we care about)
                                         //   Sections of content containing only white space are dropped,
                                         //   which gets rid  the bogus white space content from
                                         //   elements which are primarily containers for nested elements.
    UVector             fAttNames;       // A vector containing the names of this element's attributes
                                         //    The names are UnicodeString objects, owned by the UXMLParser.
    UVector             fAttValues;      // A vector containing the attribute values for
                                         //    this element's attributes.  The order is the same
                                         //    as that of the attribute name vector.

    UVector             fChildren;       // The child nodes of this element (a Vector)

    UXMLElement        *fParent;         // A pointer to the parent element of this element.
};

/**
 * A simple XML parser; it is neither efficient nor conformant and only useful for
 * restricted types of XML documents.
 *
 * The parse methods parse whole documents and return the parse trees via their
 * root elements.
 */
class U_TOOLUTIL_API UXMLParser : public UObject {
public:
    /**
     * Create an XML parser.
     */
    static UXMLParser *createParser(UErrorCode &errorCode);
    /**
     * Destructor.
     */
    virtual ~UXMLParser();

    /**
     * Parse an XML document, create the entire document tree, and
     * return a pointer to the root element of the parsed tree.
     * The caller must delete the element.
     */
    UXMLElement *parse(const UnicodeString &src, UErrorCode &errorCode);
    /**
     * Parse an XML file, create the entire document tree, and
     * return a pointer to the root element of the parsed tree.
     * The caller must delete the element.
     */
    UXMLElement *parseFile(const char *filename, UErrorCode &errorCode);

    /**
     * ICU "poor man's RTTI", returns a UClassID for the actual class.
     */
    virtual UClassID getDynamicClassID() const override;

    /**
     * ICU "poor man's RTTI", returns a UClassID for this class.
     */
    static UClassID U_EXPORT2 getStaticClassID();

private:
    // prevent default construction etc.
    UXMLParser();
    UXMLParser(const UXMLParser &other);
    UXMLParser &operator=(const UXMLParser &other);

    // constructor
    UXMLParser(UErrorCode &status);

    void           parseMisc(UErrorCode &status);
    UXMLElement   *createElement(RegexMatcher &mEl, UErrorCode &status);
    void           error(const char *message, UErrorCode &status);
    UnicodeString  scanContent(UErrorCode &status);
    void           replaceCharRefs(UnicodeString &s, UErrorCode &status);

    const UnicodeString *intern(const UnicodeString &s, UErrorCode &errorCode);
public:
    // public for UXMLElement only
    const UnicodeString *findName(const UnicodeString &s) const;
private:

    // There is one ICU regex matcher for each of the major XML syntax items
    //  that are recognized.
    RegexMatcher mXMLDecl;
    RegexMatcher mXMLComment;
    RegexMatcher mXMLSP;
    RegexMatcher mXMLDoctype;
    RegexMatcher mXMLPI;
    RegexMatcher mXMLElemStart;
    RegexMatcher mXMLElemEnd;
    RegexMatcher mXMLElemEmpty;
    RegexMatcher mXMLCharData;
    RegexMatcher mAttrValue;
    RegexMatcher mAttrNormalizer;
    RegexMatcher mNewLineNormalizer;
    RegexMatcher mAmps;

    Hashtable             fNames;           // interned element/attribute name strings
    UStack                fElementStack;    // Stack holds the parent elements when nested
                                            //    elements are being parsed.  All items on this
                                            //    stack are of type UXMLElement.
    int32_t               fPos;             // String index of the current scan position in
                                            //    xml source (in fSrc).
    UnicodeString         fOneLF;
};

U_NAMESPACE_END
#endif /* !UCONFIG_NO_REGULAR_EXPRESSIONS */

#endif

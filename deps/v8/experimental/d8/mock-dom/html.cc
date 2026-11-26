// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "experimental/d8/mock-dom/html.h"

#include <cstdio>
#include <set>

#include "src/base/logging.h"

namespace v8 {
namespace mock_dom {

namespace {

// --- Tables ------------------------------------------------------------------

struct NamedEntity {
  const char* name;
  uint32_t code_point;
};

const NamedEntity kEntities[] = {
    {"amp", 38},      {"lt", 60},       {"gt", 62},       {"quot", 34},
    {"apos", 39},     {"nbsp", 160},    {"copy", 169},    {"reg", 174},
    {"trade", 8482},  {"hellip", 8230}, {"mdash", 8212},  {"ndash", 8211},
    {"lsquo", 8216},  {"rsquo", 8217},  {"ldquo", 8220},  {"rdquo", 8221},
    {"laquo", 171},   {"raquo", 187},   {"bull", 8226},   {"middot", 183},
    {"times", 215},   {"divide", 247},  {"deg", 176},     {"plusmn", 177},
    {"euro", 8364},   {"pound", 163},   {"yen", 165},     {"cent", 162},
    {"sect", 167},    {"para", 182},    {"larr", 8592},   {"rarr", 8594},
    {"uarr", 8593},   {"darr", 8595},   {"harr", 8596},   {"check", 10003},
    {"cross", 10007}, {"hearts", 9829}, {"star", 9734},   {"ensp", 8194},
    {"emsp", 8195},   {"thinsp", 8201}, {"zwnj", 8204},   {"zwj", 8205},
    {"lrm", 8206},    {"rlm", 8207},    {"shy", 173},     {"iexcl", 161},
    {"iquest", 191},  {"frac12", 189},  {"frac14", 188},  {"frac34", 190},
    {"sup2", 178},    {"sup3", 179},    {"micro", 181},   {"ne", 8800},
    {"le", 8804},     {"ge", 8805},     {"infin", 8734},  {"asymp", 8776},
    {"minus", 8722},  {"prime", 8242},  {"Prime", 8243},  {"dagger", 8224},
    {"permil", 8240}, {"lsaquo", 8249}, {"rsaquo", 8250}, {"sbquo", 8218},
    {"bdquo", 8222},  {"oline", 8254},  {"frasl", 8260},  {"eacute", 233},
    {"egrave", 232},  {"agrave", 224},  {"aacute", 225},  {"ccedil", 231},
    {"ntilde", 241},  {"uuml", 252},    {"ouml", 246},    {"auml", 228},
    {"szlig", 223},   {"Auml", 196},    {"Ouml", 214},    {"Uuml", 220},
    {"oslash", 248},  {"aring", 229},   {"Eacute", 201},  {"iacute", 237},
    {"oacute", 243},  {"uacute", 250},  {"acirc", 226},   {"ecirc", 234},
    {"ocirc", 244},   {"lang", 9001},   {"rang", 9002},   {"NewLine", 10},
    {"Tab", 9},       {"colon", 58},    {"comma", 44},    {"period", 46},
    {"lpar", 40},     {"rpar", 41},     {"num", 35},      {"equals", 61},
    {"sol", 47},      {"excl", 33},     {"quest", 63},    {"semi", 59},
    {"lbrack", 91},   {"rbrack", 93},   {"lbrace", 123},  {"rbrace", 125},
    {"dollar", 36},   {"percnt", 37},   {"plus", 43},     {"ast", 42},
    {"commat", 64},   {"lowbar", 95},   {"grave", 96},    {"vert", 124},
    {"bsol", 92},     {"hyphen", 8208}, {"dash", 8208},   {"rarrw", 8605},
};

const char* const kVoidElements[] = {
    "area",  "base", "br",   "col",   "embed",  "hr",    "img",
    "input", "link", "meta", "param", "source", "track", "wbr"};

const char* const kRawTextElements[] = {"script",   "style",    "xmp",
                                        "iframe",   "noembed",  "noframes",
                                        "noscript", "plaintext"};

// A start tag for one of these closes an open <p>.
const char* const kClosesP[] = {
    "address", "article", "aside",     "blockquote", "center",   "details",
    "dialog",  "dir",     "div",       "dl",         "fieldset", "figcaption",
    "figure",  "footer",  "form",      "h1",         "h2",       "h3",
    "h4",      "h5",      "h6",        "header",     "hgroup",   "hr",
    "main",    "menu",    "nav",       "ol",         "p",        "pre",
    "section", "summary", "table",     "ul",         "li",       "dd",
    "dt",      "listing", "plaintext", "search"};

// Formatting elements: the parser remembers these after they are closed
// implicitly and reopens them (see ReconstructFormatting).
const char* const kFormattingElements[] = {
    "a",    "b", "big",   "code",   "em",     "font", "i",
    "nobr", "s", "small", "strike", "strong", "tt",   "u"};

// Start tags that do not reopen formatting elements: block-level and
// structural elements, and elements whose content is not body content.
const char* const kNoReconstruct[] = {
    "base",     "body",   "caption", "col",     "colgroup", "frame",
    "frameset", "head",   "html",    "link",    "meta",     "noframes",
    "noscript", "script", "style",   "tbody",   "td",       "template",
    "tfoot",    "th",     "thead",   "title",   "tr",       "param",
    "source",   "track",  "iframe",  "noembed", "textarea", "xmp"};

// The "special" category of the HTML parser, as far as this parser builds
// such elements. A formatting element that is closed while one of these is
// still open inside it is split around that element.
const char* const kSpecialElements[] = {
    "address", "article",  "aside",    "blockquote", "body",   "button",
    "caption", "center",   "dd",       "details",    "dir",    "div",
    "dl",      "dt",       "fieldset", "figcaption", "figure", "footer",
    "form",    "h1",       "h2",       "h3",         "h4",     "h5",
    "h6",      "header",   "hgroup",   "html",       "li",     "listing",
    "main",    "menu",     "nav",      "ol",         "p",      "pre",
    "search",  "section",  "select",   "summary",    "table",  "tbody",
    "td",      "template", "tfoot",    "th",         "thead",  "tr",
    "ul",      "marquee",  "object",   "applet"};

// These scope the list of formatting elements: nothing opened outside is
// reopened inside.
const char* const kFormattingScopes[] = {
    "applet", "object", "marquee", "template", "td", "th", "caption"};

struct CaseAdjustment {
  const char* lower;
  const char* adjusted;
};

const CaseAdjustment kSVGTagAdjustments[] = {
    {"altglyph", "altGlyph"},
    {"altglyphdef", "altGlyphDef"},
    {"altglyphitem", "altGlyphItem"},
    {"animatecolor", "animateColor"},
    {"animatemotion", "animateMotion"},
    {"animatetransform", "animateTransform"},
    {"clippath", "clipPath"},
    {"feblend", "feBlend"},
    {"fecolormatrix", "feColorMatrix"},
    {"fecomponenttransfer", "feComponentTransfer"},
    {"fecomposite", "feComposite"},
    {"feconvolvematrix", "feConvolveMatrix"},
    {"fediffuselighting", "feDiffuseLighting"},
    {"fedisplacementmap", "feDisplacementMap"},
    {"fedistantlight", "feDistantLight"},
    {"fedropshadow", "feDropShadow"},
    {"feflood", "feFlood"},
    {"fefunca", "feFuncA"},
    {"fefuncb", "feFuncB"},
    {"fefuncg", "feFuncG"},
    {"fefuncr", "feFuncR"},
    {"fegaussianblur", "feGaussianBlur"},
    {"feimage", "feImage"},
    {"femerge", "feMerge"},
    {"femergenode", "feMergeNode"},
    {"femorphology", "feMorphology"},
    {"feoffset", "feOffset"},
    {"fepointlight", "fePointLight"},
    {"fespecularlighting", "feSpecularLighting"},
    {"fespotlight", "feSpotLight"},
    {"fetile", "feTile"},
    {"feturbulence", "feTurbulence"},
    {"foreignobject", "foreignObject"},
    {"glyphref", "glyphRef"},
    {"lineargradient", "linearGradient"},
    {"radialgradient", "radialGradient"},
    {"textpath", "textPath"},
};

const CaseAdjustment kSVGAttributeAdjustments[] = {
    {"attributename", "attributeName"},
    {"attributetype", "attributeType"},
    {"basefrequency", "baseFrequency"},
    {"baseprofile", "baseProfile"},
    {"calcmode", "calcMode"},
    {"clippathunits", "clipPathUnits"},
    {"diffuseconstant", "diffuseConstant"},
    {"edgemode", "edgeMode"},
    {"filterunits", "filterUnits"},
    {"glyphref", "glyphRef"},
    {"gradienttransform", "gradientTransform"},
    {"gradientunits", "gradientUnits"},
    {"kernelmatrix", "kernelMatrix"},
    {"kernelunitlength", "kernelUnitLength"},
    {"keypoints", "keyPoints"},
    {"keysplines", "keySplines"},
    {"keytimes", "keyTimes"},
    {"lengthadjust", "lengthAdjust"},
    {"limitingconeangle", "limitingConeAngle"},
    {"markerheight", "markerHeight"},
    {"markerunits", "markerUnits"},
    {"markerwidth", "markerWidth"},
    {"maskcontentunits", "maskContentUnits"},
    {"maskunits", "maskUnits"},
    {"numoctaves", "numOctaves"},
    {"pathlength", "pathLength"},
    {"patterncontentunits", "patternContentUnits"},
    {"patterntransform", "patternTransform"},
    {"patternunits", "patternUnits"},
    {"pointsatx", "pointsAtX"},
    {"pointsaty", "pointsAtY"},
    {"pointsatz", "pointsAtZ"},
    {"preservealpha", "preserveAlpha"},
    {"preserveaspectratio", "preserveAspectRatio"},
    {"primitiveunits", "primitiveUnits"},
    {"refx", "refX"},
    {"refy", "refY"},
    {"repeatcount", "repeatCount"},
    {"repeatdur", "repeatDur"},
    {"requiredextensions", "requiredExtensions"},
    {"requiredfeatures", "requiredFeatures"},
    {"specularconstant", "specularConstant"},
    {"specularexponent", "specularExponent"},
    {"spreadmethod", "spreadMethod"},
    {"startoffset", "startOffset"},
    {"stddeviation", "stdDeviation"},
    {"stitchtiles", "stitchTiles"},
    {"surfacescale", "surfaceScale"},
    {"systemlanguage", "systemLanguage"},
    {"tablevalues", "tableValues"},
    {"targetx", "targetX"},
    {"targety", "targetY"},
    {"textlength", "textLength"},
    {"viewbox", "viewBox"},
    {"viewtarget", "viewTarget"},
    {"xchannelselector", "xChannelSelector"},
    {"ychannelselector", "yChannelSelector"},
    {"zoomandpan", "zoomAndPan"},
};

template <size_t N>
bool Contains(const char* const (&table)[N], std::string_view name) {
  for (const char* entry : table) {
    if (name == entry) return true;
  }
  return false;
}

template <size_t N>
std::string Adjust(const CaseAdjustment (&table)[N], const std::string& name) {
  for (const CaseAdjustment& entry : table) {
    if (name == entry.lower) return entry.adjusted;
  }
  return name;
}

bool IsAsciiAlpha(char c) {
  return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
}
bool IsAsciiAlnum(char c) { return IsAsciiAlpha(c) || (c >= '0' && c <= '9'); }

// Decodes character references in |text|.
std::string DecodeEntities(std::string_view text) {
  if (text.find('&') == std::string_view::npos) return std::string(text);
  std::string out;
  out.reserve(text.size());
  size_t i = 0;
  while (i < text.size()) {
    char c = text[i];
    if (c != '&') {
      out.push_back(c);
      ++i;
      continue;
    }
    size_t start = i + 1;
    if (start < text.size() && text[start] == '#') {
      size_t p = start + 1;
      uint32_t code_point = 0;
      bool any = false;
      if (p < text.size() && (text[p] == 'x' || text[p] == 'X')) {
        ++p;
        while (p < text.size() &&
               std::isxdigit(static_cast<unsigned char>(text[p]))) {
          char d = text[p];
          code_point = code_point * 16 +
                       (d <= '9' ? d - '0' : (ToAsciiLower(d) - 'a' + 10));
          any = true;
          ++p;
        }
      } else {
        while (p < text.size() && text[p] >= '0' && text[p] <= '9') {
          code_point = code_point * 10 + (text[p] - '0');
          any = true;
          ++p;
        }
      }
      if (any) {
        if (p < text.size() && text[p] == ';') ++p;
        if (code_point == 0 || code_point > 0x10FFFF) code_point = 0xFFFD;
        AppendUtf8(&out, code_point);
        i = p;
        continue;
      }
    } else {
      size_t p = start;
      while (p < text.size() && IsAsciiAlnum(text[p])) ++p;
      std::string_view name = text.substr(start, p - start);
      bool terminated = p < text.size() && text[p] == ';';
      bool matched = false;
      if (!name.empty()) {
        for (const NamedEntity& entity : kEntities) {
          if (name == entity.name) {
            // The legacy entities are recognised without a semicolon too.
            bool legacy = name == "amp" || name == "lt" || name == "gt" ||
                          name == "quot" || name == "nbsp" || name == "copy" ||
                          name == "reg";
            if (!terminated && !legacy) break;
            AppendUtf8(&out, entity.code_point);
            i = terminated ? p + 1 : p;
            matched = true;
            break;
          }
        }
        if (!matched && terminated) {
          // An entity this table does not know would silently turn into the
          // wrong text; say so once per name.
          static std::set<std::string>* warned = new std::set<std::string>();
          if (warned->insert(std::string(name)).second) {
            fprintf(stderr,
                    "[mock-dom] warning: unknown character reference &%s;\n",
                    std::string(name).c_str());
          }
        }
      }
      if (matched) continue;
    }
    out.push_back('&');
    ++i;
  }
  return out;
}

// --- Parser ------------------------------------------------------------------

class HTMLParser {
  CPPGC_STACK_ALLOCATED();

 public:
  HTMLParser(Isolate* isolate, Document* document, ContainerNode* root,
             std::string_view html, bool document_mode)
      : isolate_(isolate),
        document_(document),
        root_(root),
        html_(html),
        document_mode_(document_mode) {}

  void set_context(Element* context) {
    if (!context) return;
    context_ns_ = context->ns();
    if (context->local_name().is_8bit()) {
      context_name_ = std::string(context->local_name().view8());
    }
    if (context_ns_ == Namespace::kSVG &&
        (context->local_name().EqualsAscii("foreignObject") ||
         context->local_name().EqualsAscii("desc") ||
         context->local_name().EqualsAscii("title"))) {
      context_ns_ = Namespace::kHTML;
    }
    if (context_ns_ == Namespace::kNone) context_ns_ = Namespace::kHTML;
  }

  void Run() {
    size_t i = 0;
    const size_t n = html_.size();
    while (i < n) {
      size_t lt = html_.find('<', i);
      if (lt == std::string_view::npos) {
        Text(html_.substr(i), true);
        break;
      }
      if (lt > i) Text(html_.substr(i, lt - i), true);
      i = lt;
      if (html_.compare(i, 4, "<!--") == 0) {
        size_t end = html_.find("-->", i + 4);
        std::string_view data = end == std::string_view::npos
                                    ? html_.substr(i + 4)
                                    : html_.substr(i + 4, end - (i + 4));
        Comment(data);
        i = end == std::string_view::npos ? n : end + 3;
        continue;
      }
      if (i + 1 < n && html_[i + 1] == '!') {
        size_t end = html_.find('>', i);
        if (end == std::string_view::npos) end = n;
        std::string_view declaration = html_.substr(i + 2, end - (i + 2));
        if (html_.compare(i, 9, "<![CDATA[") == 0) {
          size_t close = html_.find("]]>", i + 9);
          Text(html_.substr(i + 9, close == std::string_view::npos
                                       ? std::string_view::npos
                                       : close - (i + 9)),
               false);
          i = close == std::string_view::npos ? n : close + 3;
          continue;
        }
        if (declaration.size() >= 7 &&
            EqualsIgnoringAsciiCase(declaration.substr(0, 7), "doctype")) {
          Doctype(StripHtmlSpace(declaration.substr(7)));
        } else {
          // Any other "<!...>" is a bogus comment.
          Comment(declaration);
        }
        i = end == n ? n : end + 1;
        continue;
      }
      if (i + 1 < n && html_[i + 1] == '/') {
        size_t end = html_.find('>', i);
        if (end == std::string_view::npos) end = n;
        std::string_view name = html_.substr(i + 2, end - (i + 2));
        size_t space = name.find_first_of(" \t\n\r\f/");
        if (space != std::string_view::npos) name = name.substr(0, space);
        EndTag(AsciiLower(name));
        i = end == n ? n : end + 1;
        continue;
      }
      if (i + 1 < n && IsAsciiAlpha(html_[i + 1])) {
        i = StartTag(i);
        continue;
      }
      if (i + 1 < n && html_[i + 1] == '?') {
        // "<?...>" is a bogus comment whose data starts with the '?'.
        size_t end = html_.find('>', i);
        if (end == std::string_view::npos) end = n;
        Comment(html_.substr(i + 1, end - (i + 1)));
        i = end == n ? n : end + 1;
        continue;
      }
      // A lone '<' is text.
      Text("<", false);
      ++i;
    }
    if (document_mode_) {
      EnsureHead();
      EnsureBody();
    }
  }

 private:
  struct ParsedAttribute {
    std::string name;
    std::string value;
  };

  ContainerNode* InsertionTarget() {
    if (stack_.empty()) return root_;
    Element* top = stack_.back();
    if (top->HasTag("template")) return top->TemplateContent(isolate_);
    return top;
  }

  Namespace CurrentNamespace() const {
    if (stack_.empty()) return context_ns_;
    Element* top = stack_.back();
    if (top->ns() == Namespace::kSVG &&
        (top->local_name().EqualsAscii("foreignObject") ||
         top->local_name().EqualsAscii("desc") ||
         top->local_name().EqualsAscii("title"))) {
      return Namespace::kHTML;
    }
    return top->ns();
  }

  // Document-mode scaffolding.
  void EnsureHtml() {
    if (html_element_) return;
    html_element_ = document_->CreateElement(isolate_, Namespace::kHTML,
                                             AtomicString("html"));
    document_->ParserAppendChild(isolate_, html_element_);
  }
  void EnsureHead() {
    EnsureHtml();
    if (head_element_) return;
    head_element_ = document_->CreateElement(isolate_, Namespace::kHTML,
                                             AtomicString("head"));
    html_element_->ParserAppendChild(isolate_, head_element_);
  }
  void EnsureBody() {
    EnsureHead();
    if (body_element_) return;
    body_element_ = document_->CreateElement(isolate_, Namespace::kHTML,
                                             AtomicString("body"));
    html_element_->ParserAppendChild(isolate_, body_element_);
  }
  bool InDocumentPreamble() const {
    // True while the insertion point is still the html or head element
    // itself, i.e. the parser has not committed to the body yet.
    return document_mode_ && !in_body_ && stack_.size() <= 1;
  }
  void SwitchToBody() {
    EnsureBody();
    in_body_ = true;
    stack_.clear();
    stack_.push_back(body_element_);
  }

  void Text(std::string_view raw, bool decode) {
    if (raw.empty()) return;
    if (InDocumentPreamble()) {
      if (StripHtmlSpace(raw).empty()) return;
      SwitchToBody();
    }
    std::string text = decode ? DecodeEntities(raw) : std::string(raw);
    if (drop_leading_newline_) {
      drop_leading_newline_ = false;
      if (!text.empty() && text[0] == '\n') text.erase(0, 1);
      if (text.empty()) return;
    }
    ReconstructFormatting();
    ContainerNode* target = InsertionTarget();
    // Adjacent character tokens form a single Text node.
    Node* last = target->last();
    if (last && last->IsText() && last == last_text_) {
      CharacterData* existing = static_cast<CharacterData*>(last);
      pending_text_ += text;
      existing->SetData(isolate_, String::FromUtf8(pending_text_));
      return;
    }
    pending_text_ = text;
    last_text_ = document_->CreateTextNode(isolate_, String::FromUtf8(text));
    target->ParserAppendChild(isolate_, last_text_);
  }

  void Comment(std::string_view data) {
    last_text_ = nullptr;
    Node* comment = document_->CreateComment(isolate_, String::FromUtf8(data));
    if (document_mode_ && !html_element_) {
      document_->ParserAppendChild(isolate_, comment);
      return;
    }
    if (document_mode_ && stack_.empty()) {
      html_element_->ParserAppendChild(isolate_, comment);
      return;
    }
    InsertionTarget()->ParserAppendChild(isolate_, comment);
  }

  void Doctype(std::string_view rest) {
    if (!document_mode_ || html_element_) return;
    size_t space = rest.find_first_of(" \t\n\r\f");
    std::string name = AsciiLower(rest.substr(0, space));
    document_->ParserAppendChild(
        isolate_, MakeGarbageCollected<DocumentType>(
                      isolate_, document_, AtomicString::FromUtf8(name)));
  }

  void PopUntil(size_t index) {
    if (!formatting_.empty()) {
      for (size_t i = stack_.size(); i-- > index;) {
        Element* element = stack_[i];
        if (!element->IsHTML() || !element->local_name().is_8bit() ||
            !Contains(kFormattingScopes, element->local_name().view8())) {
          continue;
        }
        // Leaving the scope forgets the formatting elements opened in it.
        while (!formatting_.empty()) {
          Element* entry = formatting_.back();
          formatting_.pop_back();
          if (!entry) break;
        }
      }
    }
    stack_.resize(index);
  }

  // Pops the stack down to and including the nearest open element named
  // |name|, stopping at any element in |boundaries|.
  template <size_t N>
  void CloseElement(std::string_view name, const char* const (&boundaries)[N]) {
    for (size_t i = stack_.size(); i-- > 0;) {
      Element* element = stack_[i];
      if (!element->IsHTML()) return;
      if (element->local_name().EqualsAscii(name)) {
        PopUntil(i);
        return;
      }
      if (Contains(boundaries, element->local_name().view8())) return;
    }
  }

  void ApplyImpliedEndTags(const std::string& name) {
    static const char* const kPBoundaries[] = {
        "table",    "td",      "th",      "button", "html",  "body",
        "template", "caption", "marquee", "object", "applet"};
    static const char* const kListBoundaries[] = {"ul", "ol", "table",
                                                  "template", "body"};
    static const char* const kDlBoundaries[] = {"dl", "table", "template",
                                                "body"};
    static const char* const kSelectBoundaries[] = {"select", "template",
                                                    "body"};
    static const char* const kTableBoundaries[] = {"table", "template", "body"};
    static const char* const kRowBoundaries[] = {"tr", "table", "template",
                                                 "body"};
    if (Contains(kClosesP, name)) CloseElement("p", kPBoundaries);
    if (name == "li") {
      CloseElement("li", kListBoundaries);
    } else if (name == "dd" || name == "dt") {
      CloseElement("dd", kDlBoundaries);
      CloseElement("dt", kDlBoundaries);
    } else if (name == "option") {
      CloseElement("option", kSelectBoundaries);
    } else if (name == "optgroup") {
      CloseElement("option", kSelectBoundaries);
      CloseElement("optgroup", kSelectBoundaries);
    } else if (name == "tr") {
      CloseElement("td", kRowBoundaries);
      CloseElement("th", kRowBoundaries);
      CloseElement("tr", kTableBoundaries);
    } else if (name == "td" || name == "th") {
      CloseElement("td", kRowBoundaries);
      CloseElement("th", kRowBoundaries);
    } else if (name == "thead" || name == "tbody" || name == "tfoot") {
      CloseElement("td", kRowBoundaries);
      CloseElement("th", kRowBoundaries);
      CloseElement("tr", kTableBoundaries);
      CloseElement("thead", kTableBoundaries);
      CloseElement("tbody", kTableBoundaries);
      CloseElement("tfoot", kTableBoundaries);
    }
  }

  void MergeAttributes(Element* element,
                       const std::vector<ParsedAttribute>& attributes) {
    for (const ParsedAttribute& attribute : attributes) {
      AtomicString name = AtomicString::FromUtf8(attribute.name);
      if (!element->HasAttribute(name)) {
        element->ParserSetAttribute(name,
                                    AtomicString::FromUtf8(attribute.value));
      }
    }
  }

  // Parses the start tag at |pos| and returns the position after it (and
  // after any raw text content it owns).
  size_t StartTag(size_t pos) {
    const size_t n = html_.size();
    size_t i = pos + 1;
    size_t name_start = i;
    while (i < n && !IsHtmlSpace(html_[i]) && html_[i] != '>' &&
           html_[i] != '/') {
      ++i;
    }
    std::string name = AsciiLower(html_.substr(name_start, i - name_start));

    std::vector<ParsedAttribute> attributes;
    bool self_closing = false;
    while (i < n) {
      while (i < n && (IsHtmlSpace(html_[i]) || html_[i] == '/')) {
        if (html_[i] == '/' && i + 1 < n && html_[i + 1] == '>') {
          self_closing = true;
        }
        ++i;
      }
      if (i >= n || html_[i] == '>') break;
      size_t attr_start = i;
      while (i < n && !IsHtmlSpace(html_[i]) && html_[i] != '=' &&
             html_[i] != '>' && !(html_[i] == '/' && i > attr_start)) {
        ++i;
      }
      ParsedAttribute attribute;
      attribute.name = std::string(html_.substr(attr_start, i - attr_start));
      while (i < n && IsHtmlSpace(html_[i])) ++i;
      if (i < n && html_[i] == '=') {
        ++i;
        while (i < n && IsHtmlSpace(html_[i])) ++i;
        if (i < n && (html_[i] == '"' || html_[i] == '\'')) {
          char quote = html_[i++];
          size_t end = html_.find(quote, i);
          if (end == std::string_view::npos) end = n;
          attribute.value = DecodeEntities(html_.substr(i, end - i));
          i = end == n ? n : end + 1;
        } else {
          size_t value_start = i;
          while (i < n && !IsHtmlSpace(html_[i]) && html_[i] != '>') ++i;
          attribute.value =
              DecodeEntities(html_.substr(value_start, i - value_start));
        }
      }
      if (!attribute.name.empty()) attributes.push_back(std::move(attribute));
    }
    if (i < n) ++i;  // '>'

    last_text_ = nullptr;
    Namespace ns = CurrentNamespace();
    if (ns == Namespace::kHTML) {
      if (name == "svg") {
        ns = Namespace::kSVG;
      } else if (name == "math") {
        ns = Namespace::kMathML;
      }
    }

    if (ns == Namespace::kHTML && document_mode_) {
      if (name == "html") {
        EnsureHtml();
        MergeAttributes(html_element_, attributes);
        return i;
      }
      if (name == "head") {
        if (!in_body_ && !head_element_) {
          EnsureHead();
          MergeAttributes(head_element_, attributes);
          stack_.clear();
          stack_.push_back(head_element_);
        }
        return i;
      }
      if (name == "body") {
        bool created = !body_element_;
        if (!in_body_) SwitchToBody();
        if (created || true) MergeAttributes(body_element_, attributes);
        return i;
      }
      if (InDocumentPreamble()) {
        static const char* const kHeadContent[] = {
            "meta", "link",     "title",    "script",   "style",
            "base", "noscript", "template", "noframes", "bgsound"};
        if (Contains(kHeadContent, name) && !after_head_) {
          EnsureHead();
          if (stack_.empty()) stack_.push_back(head_element_);
        } else {
          SwitchToBody();
        }
      }
    } else if (ns == Namespace::kHTML &&
               (name == "html" || name == "head" || name == "body")) {
      // Fragment parsing ignores these.
      return i;
    }

    const bool formatting =
        ns == Namespace::kHTML && Contains(kFormattingElements, name);
    if (ns == Namespace::kHTML) {
      ApplyImpliedEndTags(name);
      // A second <a> (or <nobr>) closes the one that is still open.
      if ((name == "a" || name == "nobr") && FindFormatting(name) >= 0) {
        AdoptionAgency(name);
        int stale = FindFormatting(name);
        if (stale >= 0) {
          Element* element = formatting_[stale];
          formatting_.erase(formatting_.begin() + stale);
          auto it = std::find(stack_.begin(), stack_.end(), element);
          if (it != stack_.end()) stack_.erase(it);
        }
      }
      if (!formatting_.empty() && !Contains(kClosesP, name) &&
          !Contains(kNoReconstruct, name)) {
        ReconstructFormatting();
      }
      // Rows and cells written without their <tbody> or <tr> get one.
      if (name == "tr" || name == "td" || name == "th") {
        if (CurrentTagIs("table")) InsertImplied("tbody");
        if (name != "tr" && (CurrentTagIs("tbody") || CurrentTagIs("thead") ||
                             CurrentTagIs("tfoot"))) {
          InsertImplied("tr");
        }
      }
    }

    std::string local_name = name;
    if (ns == Namespace::kSVG) local_name = Adjust(kSVGTagAdjustments, name);
    Element* element = document_->CreateElement(
        isolate_, ns, AtomicString::FromUtf8(local_name));
    for (const ParsedAttribute& attribute : attributes) {
      std::string attribute_name = AsciiLower(attribute.name);
      if (ns == Namespace::kSVG) {
        attribute_name = Adjust(kSVGAttributeAdjustments, attribute_name);
      }
      AtomicString atom = AtomicString::FromUtf8(attribute_name);
      if (!element->HasAttribute(atom)) {
        element->ParserSetAttribute(atom,
                                    AtomicString::FromUtf8(attribute.value));
      }
    }
    InsertionTarget()->ParserAppendChild(isolate_, element);

    if (ns != Namespace::kHTML) {
      if (!self_closing) stack_.push_back(element);
      return i;
    }
    if (Contains(kVoidElements, name)) return i;

    bool raw_text = Contains(kRawTextElements, name);
    bool rcdata = name == "textarea" || name == "title";
    if (raw_text || rcdata) {
      // The content runs to the matching end tag and contains no markup.
      size_t end = i;
      while (true) {
        end = html_.find("</", end);
        if (end == std::string_view::npos) {
          end = n;
          break;
        }
        if (end + 2 + name.size() <= n &&
            EqualsIgnoringAsciiCase(html_.substr(end + 2, name.size()), name)) {
          break;
        }
        end += 2;
      }
      std::string_view content = html_.substr(i, end - i);
      if (name == "textarea" && !content.empty() && content[0] == '\n') {
        content.remove_prefix(1);
      }
      if (!content.empty()) {
        std::string text =
            rcdata ? DecodeEntities(content) : std::string(content);
        element->ParserAppendChild(
            isolate_,
            document_->CreateTextNode(isolate_, String::FromUtf8(text)));
      }
      if (end == n) return n;
      size_t close = html_.find('>', end);
      return close == std::string_view::npos ? n : close + 1;
    }

    stack_.push_back(element);
    if (formatting) {
      formatting_.push_back(element);
    } else if (Contains(kFormattingScopes, name)) {
      formatting_.push_back(nullptr);
    }
    if (name == "pre" || name == "listing") drop_leading_newline_ = true;
    return i;
  }

  void EndTag(const std::string& name) {
    last_text_ = nullptr;
    drop_leading_newline_ = false;
    if (document_mode_) {
      if (name == "head") {
        if (!in_body_) {
          stack_.clear();
          after_head_ = true;
        }
        return;
      }
      if (name == "body" || name == "html") return;
    }
    if (!formatting_.empty()) {
      if (Contains(kFormattingElements, name)) {
        if (AdoptionAgency(name)) return;
      }
    }
    for (size_t i = stack_.size(); i-- > 0;) {
      Element* element = stack_[i];
      std::string_view local = element->local_name().is_8bit()
                                   ? element->local_name().view8()
                                   : std::string_view();
      bool matches = element->IsHTML() ? local == name
                                       : EqualsIgnoringAsciiCase(local, name);
      if (matches) {
        // The document scaffolding elements are never popped by a stray tag.
        if (document_mode_ && i == 0 &&
            (element == body_element_ || element == head_element_)) {
          PopUntil(1);
          return;
        }
        PopUntil(i);
        return;
      }
    }
    // No matching open element: the end tag is ignored.
  }

  // Whether new content is inserted into an HTML element named |name|.
  bool CurrentTagIs(std::string_view name) const {
    if (stack_.empty())
      return context_ns_ == Namespace::kHTML && context_name_ == name;
    return stack_.back()->IsHTML() &&
           stack_.back()->local_name().EqualsAscii(name);
  }

  void InsertImplied(const char* name) {
    Element* element = document_->CreateElement(isolate_, Namespace::kHTML,
                                                AtomicString(name));
    InsertionTarget()->ParserAppendChild(isolate_, element);
    stack_.push_back(element);
  }

  // --- Formatting elements --------------------------------------------------
  //
  // <b>, <a> and friends stay "active" when the element they are in is
  // closed around them, e.g. by the </p> in "<p><a>x</p>": the next text or
  // inline element reopens a copy of them. This is the list of active
  // formatting elements of the HTML parsing algorithm, without the limit of
  // three identical entries.

  bool IsOpen(Element* element) const {
    return std::find(stack_.begin(), stack_.end(), element) != stack_.end();
  }

  static bool IsSpecial(Element* element) {
    if (!element->IsHTML()) return true;
    const AtomicString& name = element->local_name();
    return name.is_8bit() && Contains(kSpecialElements, name.view8());
  }

  // Index of the last formatting element named |name| in the current scope,
  // or -1.
  int FindFormatting(std::string_view name) const {
    for (size_t i = formatting_.size(); i-- > 0;) {
      Element* element = formatting_[i];
      if (!element) break;
      if (element->local_name().EqualsAscii(name)) return static_cast<int>(i);
    }
    return -1;
  }

  Element* CloneForParser(Element* element) {
    return static_cast<Element*>(element->Clone(isolate_, document_, false));
  }

  // "Reconstruct the active formatting elements".
  void ReconstructFormatting() {
    if (formatting_.empty()) return;
    Element* last = formatting_.back();
    if (!last || IsOpen(last)) return;
    size_t first = formatting_.size() - 1;
    while (first > 0 && formatting_[first - 1] &&
           !IsOpen(formatting_[first - 1])) {
      --first;
    }
    for (size_t i = first; i < formatting_.size(); ++i) {
      Element* clone = CloneForParser(formatting_[i]);
      InsertionTarget()->ParserAppendChild(isolate_, clone);
      stack_.push_back(clone);
      formatting_[i] = clone;
    }
    last_text_ = nullptr;
  }

  // The "adoption agency algorithm" for an end tag (or implied end) of the
  // formatting element |name|. Returns false if there is no such active
  // formatting element, in which case the tag is handled like any other.
  bool AdoptionAgency(std::string_view name) {
    for (int outer = 0; outer < 8; ++outer) {
      int entry = FindFormatting(name);
      if (entry < 0) return outer > 0;
      Element* formatting_element = formatting_[entry];
      auto open = std::find(stack_.begin(), stack_.end(), formatting_element);
      if (open == stack_.end()) {
        formatting_.erase(formatting_.begin() + entry);
        return true;
      }
      size_t position = open - stack_.begin();
      size_t furthest = position + 1;
      while (furthest < stack_.size() && !IsSpecial(stack_[furthest])) {
        ++furthest;
      }
      if (furthest == stack_.size()) {
        // Nothing block-like is open inside it: just close it.
        PopUntil(position);
        formatting_.erase(formatting_.begin() + entry);
        return true;
      }
      Element* furthest_block = stack_[furthest];
      ContainerNode* common_ancestor =
          position == 0 ? root_
                        : static_cast<ContainerNode*>(stack_[position - 1]);
      if (position > 0 && stack_[position - 1]->HasTag("template")) {
        common_ancestor = stack_[position - 1]->TemplateContent(isolate_);
      }
      size_t bookmark = entry;
      // Walk up from the furthest block to the formatting element, cloning
      // the formatting elements in between and dropping everything else.
      Node* last_node = furthest_block;
      size_t index = furthest;
      for (int inner = 1;; ++inner) {
        --index;
        Element* node = stack_[index];
        if (node == formatting_element) break;
        auto in_list = std::find(formatting_.begin(), formatting_.end(), node);
        if (inner > 3 && in_list != formatting_.end()) {
          size_t removed = in_list - formatting_.begin();
          formatting_.erase(in_list);
          if (removed < bookmark) --bookmark;
          in_list = formatting_.end();
        }
        if (in_list == formatting_.end()) {
          stack_.erase(stack_.begin() + index);
          continue;
        }
        Element* clone = CloneForParser(node);
        *in_list = clone;
        stack_[index] = clone;
        if (last_node == furthest_block) {
          bookmark = (in_list - formatting_.begin()) + 1;
        }
        clone->ParserMoveChild(isolate_, last_node);
        last_node = clone;
      }
      common_ancestor->ParserMoveChild(isolate_, last_node);
      // The furthest block keeps its place; its content moves into a new
      // copy of the formatting element inside it.
      Element* replacement = CloneForParser(formatting_element);
      while (Node* child = furthest_block->first()) {
        replacement->ParserMoveChild(isolate_, child);
      }
      furthest_block->ParserAppendChild(isolate_, replacement);
      auto old_entry =
          std::find(formatting_.begin(), formatting_.end(), formatting_element);
      size_t old_index = old_entry - formatting_.begin();
      formatting_.erase(old_entry);
      if (old_index < bookmark) --bookmark;
      formatting_.insert(formatting_.begin() + bookmark, replacement);
      stack_.erase(std::find(stack_.begin(), stack_.end(), formatting_element));
      auto block = std::find(stack_.begin(), stack_.end(), furthest_block);
      stack_.insert(block + 1, replacement);
      last_text_ = nullptr;
    }
    return true;
  }

  Isolate* isolate_;
  Document* document_;
  ContainerNode* root_;
  std::string_view html_;
  bool document_mode_;
  Namespace context_ns_ = Namespace::kHTML;
  // Local name of the element whose innerHTML is being parsed, if any.
  std::string context_name_;
  // Open elements. They are attached to the tree as they are created, which
  // keeps them alive.
  CPPGC_PLUGIN_IGNORE("open elements are already attached to the tree")
  std::vector<Element*> stack_;
  // Active formatting elements, oldest first; null entries are scope
  // markers. Entries are attached to the tree like the open elements.
  CPPGC_PLUGIN_IGNORE("formatting elements are already attached to the tree")
  std::vector<Element*> formatting_;
  Element* html_element_ = nullptr;
  Element* head_element_ = nullptr;
  Element* body_element_ = nullptr;
  bool in_body_ = false;
  bool after_head_ = false;
  bool drop_leading_newline_ = false;
  Node* last_text_ = nullptr;
  std::string pending_text_;
};

// --- Serialization -----------------------------------------------------------

void EscapeText(const std::string& text, bool attribute, std::string* out) {
  for (size_t i = 0; i < text.size(); ++i) {
    unsigned char c = static_cast<unsigned char>(text[i]);
    if (c == '&') {
      out->append("&amp;");
    } else if (c == 0xC2 && i + 1 < text.size() &&
               static_cast<unsigned char>(text[i + 1]) == 0xA0) {
      out->append("&nbsp;");
      ++i;
    } else if (attribute && c == '"') {
      out->append("&quot;");
    } else if (!attribute && c == '<') {
      out->append("&lt;");
    } else if (!attribute && c == '>') {
      out->append("&gt;");
    } else {
      out->push_back(text[i]);
    }
  }
}

}  // namespace

void SerializeChildren(const Node* node, std::string* out) {
  const Node* container = node;
  if (node->IsElement()) {
    const Element* element = static_cast<const Element*>(node);
    if (element->HasTag("template") && element->rare_data() &&
        element->rare_data()->template_content) {
      container = element->rare_data()->template_content.Get();
    }
  }
  for (const Node* child = container->first_child(); child;
       child = child->next_sibling()) {
    SerializeNode(child, out);
  }
}

void SerializeNode(const Node* node, std::string* out) {
  switch (node->node_type()) {
    case Node::kElementNode: {
      const Element* element = static_cast<const Element*>(node);
      std::string name = element->local_name().ToUtf8();
      out->push_back('<');
      out->append(name);
      element->SynchronizeAttributes();
      for (const Attribute& attribute : element->attributes()) {
        out->push_back(' ');
        out->append(attribute.name.ToUtf8());
        out->append("=\"");
        EscapeText(attribute.value.ToUtf8(), true, out);
        out->push_back('"');
      }
      out->push_back('>');
      if (element->IsHTML() && Contains(kVoidElements, name)) return;
      SerializeChildren(element, out);
      out->append("</");
      out->append(name);
      out->push_back('>');
      return;
    }
    case Node::kTextNode: {
      std::string text =
          static_cast<const CharacterData*>(node)->data().ToUtf8();
      // Text inside script, style and the other raw text elements is
      // serialized verbatim.
      const Element* parent = node->parent_element();
      if (parent && parent->IsHTML() && parent->local_name().is_8bit() &&
          Contains(kRawTextElements, parent->local_name().view8())) {
        out->append(text);
      } else {
        EscapeText(text, false, out);
      }
      return;
    }
    case Node::kCommentNode:
      out->append("<!--");
      out->append(static_cast<const CharacterData*>(node)->data().ToUtf8());
      out->append("-->");
      return;
    case Node::kDocumentTypeNode:
      out->append("<!DOCTYPE ");
      out->append(static_cast<const DocumentType*>(node)->name.ToUtf8());
      out->push_back('>');
      return;
    case Node::kDocumentNode:
    case Node::kDocumentFragmentNode:
      SerializeChildren(node, out);
      return;
  }
}

void ParseHTMLFragment(Isolate* isolate, ContainerNode* container,
                       Element* context_element, std::string_view html) {
  Document* document = container->IsDocument()
                           ? static_cast<Document*>(container)
                           : container->document();
  HTMLParser parser(isolate, document, container, html, false);
  parser.set_context(context_element);
  parser.Run();
}

void ParseHTMLDocument(Isolate* isolate, Document* document,
                       std::string_view html) {
  CEReactionsScope reactions(isolate);
  HTMLParser parser(isolate, document, document, html, true);
  parser.Run();
}

String Element::InnerHTML() const {
  std::string out;
  SerializeChildren(this, &out);
  return String::FromUtf8(out);
}

String Element::OuterHTML() const {
  std::string out;
  SerializeNode(this, &out);
  return String::FromUtf8(out);
}

void Element::SetInnerHTML(Isolate* isolate, const String& html) {
  CEReactionsScope reactions(isolate);
  ContainerNode* target = this;
  if (HasTag("template")) target = TemplateContent(isolate);
  // Parse into a detached fragment first and then swap it in, so the tree is
  // only notified once, as Blink's replaceChildrenWithFragment does.
  DocumentFragment* fragment =
      MakeGarbageCollected<DocumentFragment>(isolate, target->document());
  std::string utf8 = html.ToUtf8();
  ParseHTMLFragment(isolate, fragment, this, utf8);
  target->RemoveChildren(isolate);
  ExceptionState exception_state;
  target->AppendChild(isolate, fragment, &exception_state);
}

}  // namespace mock_dom
}  // namespace v8

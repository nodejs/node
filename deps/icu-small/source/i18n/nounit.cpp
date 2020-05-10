// © 2017 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html

#include "unicode/nounit.h"
#include "uassert.h"

#if !UCONFIG_NO_FORMATTING

U_NAMESPACE_BEGIN

UOBJECT_DEFINE_RTTI_IMPLEMENTATION(NoUnit)

NoUnit U_EXPORT2 NoUnit::base() {
    return NoUnit("");
}

NoUnit U_EXPORT2 NoUnit::percent() {
    return NoUnit("percent");
}

NoUnit U_EXPORT2 NoUnit::permille() {
    return NoUnit("permille");
}

NoUnit::NoUnit(const char* subtype) {
    initNoUnit(subtype);
}

NoUnit::NoUnit(const NoUnit& other) : MeasureUnit(other) {
}

NoUnit* NoUnit::clone() const {
    return new NoUnit(*this);
}

NoUnit::~NoUnit() {
}


U_NAMESPACE_END

#endif

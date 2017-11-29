// © 2017 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html

#include "unicode/utypes.h"

#if !UCONFIG_NO_FORMATTING && !UPRV_INCOMPLETE_CPP11_SUPPORT

#include "uassert.h"
#include "unicode/numberformatter.h"
#include "number_decimalquantity.h"
#include "number_formatimpl.h"
#include "umutex.h"

using namespace icu;
using namespace icu::number;
using namespace icu::number::impl;

template<typename Derived>
Derived NumberFormatterSettings<Derived>::notation(const Notation &notation) const {
    Derived copy(*this);
    // NOTE: Slicing is OK.
    copy.fMacros.notation = notation;
    return copy;
}

template<typename Derived>
Derived NumberFormatterSettings<Derived>::unit(const icu::MeasureUnit &unit) const {
    Derived copy(*this);
    // NOTE: Slicing occurs here. However, CurrencyUnit can be restored from MeasureUnit.
    // TimeUnit may be affected, but TimeUnit is not as relevant to number formatting.
    copy.fMacros.unit = unit;
    return copy;
}

template<typename Derived>
Derived NumberFormatterSettings<Derived>::adoptUnit(const icu::MeasureUnit *unit) const {
    Derived copy(*this);
    // Just copy the unit into the MacroProps by value, and delete it since we have ownership.
    // NOTE: Slicing occurs here. However, CurrencyUnit can be restored from MeasureUnit.
    // TimeUnit may be affected, but TimeUnit is not as relevant to number formatting.
    if (unit != nullptr) {
        copy.fMacros.unit = *unit;
        delete unit;
    }
    return copy;
}

template<typename Derived>
Derived NumberFormatterSettings<Derived>::rounding(const Rounder &rounder) const {
    Derived copy(*this);
    // NOTE: Slicing is OK.
    copy.fMacros.rounder = rounder;
    return copy;
}

template<typename Derived>
Derived NumberFormatterSettings<Derived>::grouping(const Grouper &grouper) const {
    Derived copy(*this);
    copy.fMacros.grouper = grouper;
    return copy;
}

template<typename Derived>
Derived NumberFormatterSettings<Derived>::integerWidth(const IntegerWidth &style) const {
    Derived copy(*this);
    copy.fMacros.integerWidth = style;
    return copy;
}

template<typename Derived>
Derived NumberFormatterSettings<Derived>::symbols(const DecimalFormatSymbols &symbols) const {
    Derived copy(*this);
    copy.fMacros.symbols.setTo(symbols);
    return copy;
}

template<typename Derived>
Derived NumberFormatterSettings<Derived>::adoptSymbols(const NumberingSystem *ns) const {
    Derived copy(*this);
    copy.fMacros.symbols.setTo(ns);
    return copy;
}

template<typename Derived>
Derived NumberFormatterSettings<Derived>::unitWidth(const UNumberUnitWidth &width) const {
    Derived copy(*this);
    copy.fMacros.unitWidth = width;
    return copy;
}

template<typename Derived>
Derived NumberFormatterSettings<Derived>::sign(const UNumberSignDisplay &style) const {
    Derived copy(*this);
    copy.fMacros.sign = style;
    return copy;
}

template<typename Derived>
Derived NumberFormatterSettings<Derived>::decimal(const UNumberDecimalSeparatorDisplay &style) const {
    Derived copy(*this);
    copy.fMacros.decimal = style;
    return copy;
}

template<typename Derived>
Derived NumberFormatterSettings<Derived>::padding(const Padder &padder) const {
    Derived copy(*this);
    copy.fMacros.padder = padder;
    return copy;
}

template<typename Derived>
Derived NumberFormatterSettings<Derived>::threshold(int32_t threshold) const {
    Derived copy(*this);
    copy.fMacros.threshold = threshold;
    return copy;
}

// Declare all classes that implement NumberFormatterSettings
// See https://stackoverflow.com/a/495056/1407170
template
class icu::number::NumberFormatterSettings<icu::number::UnlocalizedNumberFormatter>;
template
class icu::number::NumberFormatterSettings<icu::number::LocalizedNumberFormatter>;


UnlocalizedNumberFormatter NumberFormatter::with() {
    UnlocalizedNumberFormatter result;
    return result;
}

LocalizedNumberFormatter NumberFormatter::withLocale(const Locale &locale) {
    return with().locale(locale);
}

// Make the child class constructor that takes the parent class call the parent class's copy constructor
UnlocalizedNumberFormatter::UnlocalizedNumberFormatter(
        const NumberFormatterSettings <UnlocalizedNumberFormatter> &other)
        : NumberFormatterSettings<UnlocalizedNumberFormatter>(other) {
}

// Make the child class constructor that takes the parent class call the parent class's copy constructor
// For LocalizedNumberFormatter, also copy over the extra fields
LocalizedNumberFormatter::LocalizedNumberFormatter(
        const NumberFormatterSettings <LocalizedNumberFormatter> &other)
        : NumberFormatterSettings<LocalizedNumberFormatter>(other) {
    // No additional copies required
}

LocalizedNumberFormatter::LocalizedNumberFormatter(const MacroProps &macros, const Locale &locale) {
    fMacros = macros;
    fMacros.locale = locale;
}

LocalizedNumberFormatter UnlocalizedNumberFormatter::locale(const Locale &locale) const {
    return LocalizedNumberFormatter(fMacros, locale);
}

SymbolsWrapper::SymbolsWrapper(const SymbolsWrapper &other) {
    doCopyFrom(other);
}

SymbolsWrapper &SymbolsWrapper::operator=(const SymbolsWrapper &other) {
    if (this == &other) {
        return *this;
    }
    doCleanup();
    doCopyFrom(other);
    return *this;
}

SymbolsWrapper::~SymbolsWrapper() {
    doCleanup();
}

void SymbolsWrapper::setTo(const DecimalFormatSymbols &dfs) {
    doCleanup();
    fType = SYMPTR_DFS;
    fPtr.dfs = new DecimalFormatSymbols(dfs);
}

void SymbolsWrapper::setTo(const NumberingSystem *ns) {
    doCleanup();
    fType = SYMPTR_NS;
    fPtr.ns = ns;
}

void SymbolsWrapper::doCopyFrom(const SymbolsWrapper &other) {
    fType = other.fType;
    switch (fType) {
        case SYMPTR_NONE:
            // No action necessary
            break;
        case SYMPTR_DFS:
            // Memory allocation failures are exposed in copyErrorTo()
            if (other.fPtr.dfs != nullptr) {
                fPtr.dfs = new DecimalFormatSymbols(*other.fPtr.dfs);
            } else {
                fPtr.dfs = nullptr;
            }
            break;
        case SYMPTR_NS:
            // Memory allocation failures are exposed in copyErrorTo()
            if (other.fPtr.ns != nullptr) {
                fPtr.ns = new NumberingSystem(*other.fPtr.ns);
            } else {
                fPtr.ns = nullptr;
            }
            break;
    }
}

void SymbolsWrapper::doCleanup() {
    switch (fType) {
        case SYMPTR_NONE:
            // No action necessary
            break;
        case SYMPTR_DFS:
            delete fPtr.dfs;
            break;
        case SYMPTR_NS:
            delete fPtr.ns;
            break;
    }
}

bool SymbolsWrapper::isDecimalFormatSymbols() const {
    return fType == SYMPTR_DFS;
}

bool SymbolsWrapper::isNumberingSystem() const {
    return fType == SYMPTR_NS;
}

const DecimalFormatSymbols* SymbolsWrapper::getDecimalFormatSymbols() const {
    U_ASSERT(fType == SYMPTR_DFS);
    return fPtr.dfs;
}

const NumberingSystem* SymbolsWrapper::getNumberingSystem() const {
    U_ASSERT(fType == SYMPTR_NS);
    return fPtr.ns;
}

LocalizedNumberFormatter::~LocalizedNumberFormatter() {
    delete fCompiled;
}

FormattedNumber LocalizedNumberFormatter::formatInt(int64_t value, UErrorCode &status) const {
    if (U_FAILURE(status)) { return FormattedNumber(U_ILLEGAL_ARGUMENT_ERROR); }
    auto results = new NumberFormatterResults();
    if (results == nullptr) {
        status = U_MEMORY_ALLOCATION_ERROR;
        return FormattedNumber(status);
    }
    results->quantity.setToLong(value);
    return formatImpl(results, status);
}

FormattedNumber LocalizedNumberFormatter::formatDouble(double value, UErrorCode &status) const {
    if (U_FAILURE(status)) { return FormattedNumber(U_ILLEGAL_ARGUMENT_ERROR); }
    auto results = new NumberFormatterResults();
    if (results == nullptr) {
        status = U_MEMORY_ALLOCATION_ERROR;
        return FormattedNumber(status);
    }
    results->quantity.setToDouble(value);
    return formatImpl(results, status);
}

FormattedNumber LocalizedNumberFormatter::formatDecimal(StringPiece value, UErrorCode &status) const {
    if (U_FAILURE(status)) { return FormattedNumber(U_ILLEGAL_ARGUMENT_ERROR); }
    auto results = new NumberFormatterResults();
    if (results == nullptr) {
        status = U_MEMORY_ALLOCATION_ERROR;
        return FormattedNumber(status);
    }
    results->quantity.setToDecNumber(value);
    return formatImpl(results, status);
}

FormattedNumber
LocalizedNumberFormatter::formatImpl(impl::NumberFormatterResults *results, UErrorCode &status) const {
    // fUnsafeCallCount contains memory to be interpreted as an atomic int, most commonly
    // std::atomic<int32_t>.  Since the type of atomic int is platform-dependent, we cast the
    // bytes in fUnsafeCallCount to u_atomic_int32_t, a typedef for the platform-dependent
    // atomic int type defined in umutex.h.
    static_assert(sizeof(u_atomic_int32_t) <= sizeof(fUnsafeCallCount),
        "Atomic integer size on this platform exceeds the size allocated by fUnsafeCallCount");
    u_atomic_int32_t* callCount = reinterpret_cast<u_atomic_int32_t*>(
        const_cast<LocalizedNumberFormatter*>(this)->fUnsafeCallCount);

    // A positive value in the atomic int indicates that the data structure is not yet ready;
    // a negative value indicates that it is ready. If, after the increment, the atomic int
    // is exactly threshold, then it is the current thread's job to build the data structure.
    // Note: We set the callCount to INT32_MIN so that if another thread proceeds to increment
    // the atomic int, the value remains below zero.
    int32_t currentCount = umtx_loadAcquire(*callCount);
    if (0 <= currentCount && currentCount <= fMacros.threshold && fMacros.threshold > 0) {
        currentCount = umtx_atomic_inc(callCount);
    }

    if (currentCount == fMacros.threshold && fMacros.threshold > 0) {
        // Build the data structure and then use it (slow to fast path).
        const NumberFormatterImpl* compiled =
            NumberFormatterImpl::fromMacros(fMacros, status);
        U_ASSERT(fCompiled == nullptr);
        const_cast<LocalizedNumberFormatter *>(this)->fCompiled = compiled;
        umtx_storeRelease(*callCount, INT32_MIN);
        compiled->apply(results->quantity, results->string, status);
    } else if (currentCount < 0) {
        // The data structure is already built; use it (fast path).
        U_ASSERT(fCompiled != nullptr);
        fCompiled->apply(results->quantity, results->string, status);
    } else {
        // Format the number without building the data structure (slow path).
        NumberFormatterImpl::applyStatic(fMacros, results->quantity, results->string, status);
    }

    // Do not save the results object if we encountered a failure.
    if (U_SUCCESS(status)) {
        return FormattedNumber(results);
    } else {
        delete results;
        return FormattedNumber(status);
    }
}

UnicodeString FormattedNumber::toString() const {
    if (fResults == nullptr) {
        // TODO: http://bugs.icu-project.org/trac/ticket/13437
        return {};
    }
    return fResults->string.toUnicodeString();
}

Appendable &FormattedNumber::appendTo(Appendable &appendable) {
    if (fResults == nullptr) {
        // TODO: http://bugs.icu-project.org/trac/ticket/13437
        return appendable;
    }
    appendable.appendString(fResults->string.chars(), fResults->string.length());
    return appendable;
}

void FormattedNumber::populateFieldPosition(FieldPosition &fieldPosition, UErrorCode &status) {
    if (U_FAILURE(status)) { return; }
    if (fResults == nullptr) {
        status = fErrorCode;
        return;
    }
    fResults->string.populateFieldPosition(fieldPosition, 0, status);
}

void
FormattedNumber::populateFieldPositionIterator(FieldPositionIterator &iterator, UErrorCode &status) {
    if (U_FAILURE(status)) { return; }
    if (fResults == nullptr) {
        status = fErrorCode;
        return;
    }
    fResults->string.populateFieldPositionIterator(iterator, status);
}

FormattedNumber::~FormattedNumber() {
    delete fResults;
}

#endif /* #if !UCONFIG_NO_FORMATTING */

// © 2018 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html

#ifndef __FORMVAL_IMPL_H__
#define __FORMVAL_IMPL_H__

#include "unicode/utypes.h"
#if !UCONFIG_NO_FORMATTING

// This file contains compliant implementations of FormattedValue which can be
// leveraged by ICU formatters.
//
// Each implementation is defined in its own cpp file in order to split
// dependencies more modularly.

#include "unicode/formattedvalue.h"
#include "capi_helper.h"
#include "fphdlimp.h"
#include "util.h"
#include "uvectr32.h"
#include "formatted_string_builder.h"


/**
 * Represents the type of constraint for ConstrainedFieldPosition.
 *
 * Constraints are used to control the behavior of iteration in FormattedValue.
 *
 * @internal
 */
typedef enum UCFPosConstraintType {
    /**
     * Represents the lack of a constraint.
     *
     * This is the value of fConstraint if no "constrain" methods were called.
     *
     * @internal
     */
    UCFPOS_CONSTRAINT_NONE = 0,

    /**
     * Represents that the field category is constrained.
     *
     * This is the value of fConstraint if constraintCategory was called.
     *
     * FormattedValue implementations should not change the field category
     * while this constraint is active.
     *
     * @internal
     */
    UCFPOS_CONSTRAINT_CATEGORY,

    /**
     * Represents that the field and field category are constrained.
     *
     * This is the value of fConstraint if constraintField was called.
     *
     * FormattedValue implementations should not change the field or field category
     * while this constraint is active.
     *
     * @internal
     */
    UCFPOS_CONSTRAINT_FIELD
} UCFPosConstraintType;


U_NAMESPACE_BEGIN


/**
 * Implementation of FormattedValue using FieldPositionHandler to accept fields.
 */
class FormattedValueFieldPositionIteratorImpl : public UMemory, public FormattedValue {
public:

    /** @param initialFieldCapacity Initially allocate space for this many fields. */
    FormattedValueFieldPositionIteratorImpl(int32_t initialFieldCapacity, UErrorCode& status);

    virtual ~FormattedValueFieldPositionIteratorImpl();

    // Implementation of FormattedValue (const):

    UnicodeString toString(UErrorCode& status) const U_OVERRIDE;
    UnicodeString toTempString(UErrorCode& status) const U_OVERRIDE;
    Appendable& appendTo(Appendable& appendable, UErrorCode& status) const U_OVERRIDE;
    UBool nextPosition(ConstrainedFieldPosition& cfpos, UErrorCode& status) const U_OVERRIDE;

    // Additional methods used during construction phase only (non-const):

    FieldPositionIteratorHandler getHandler(UErrorCode& status);
    void appendString(UnicodeString string, UErrorCode& status);

    /**
     * Computes the spans for duplicated values.
     * For example, if the string has fields:
     * 
     *     ...aa..[b.cc]..d.[bb.e.c]..a..
     *
     * then the spans will be the bracketed regions.
     *
     * Assumes that the currently known fields are sorted
     * and all in the same category.
     */
    void addOverlapSpans(UFieldCategory spanCategory, int8_t firstIndex, UErrorCode& status);

    /**
     * Sorts the fields: start index first, length second.
     */
    void sort();

private:
    UnicodeString fString;
    UVector32 fFields;
};


/**
 * Implementation of FormattedValue based on FormattedStringBuilder.
 *
 * The implementation currently revolves around numbers and number fields.
 * However, it can be generalized in the future when there is a need.
 *
 * @author sffc (Shane Carr)
 */
// Exported as U_I18N_API for tests
class U_I18N_API FormattedValueStringBuilderImpl : public UMemory, public FormattedValue {
public:

    FormattedValueStringBuilderImpl(FormattedStringBuilder::Field numericField);

    virtual ~FormattedValueStringBuilderImpl();

    // Implementation of FormattedValue (const):

    UnicodeString toString(UErrorCode& status) const U_OVERRIDE;
    UnicodeString toTempString(UErrorCode& status) const U_OVERRIDE;
    Appendable& appendTo(Appendable& appendable, UErrorCode& status) const U_OVERRIDE;
    UBool nextPosition(ConstrainedFieldPosition& cfpos, UErrorCode& status) const U_OVERRIDE;

    // Additional helper functions:
    UBool nextFieldPosition(FieldPosition& fp, UErrorCode& status) const;
    void getAllFieldPositions(FieldPositionIteratorHandler& fpih, UErrorCode& status) const;
    inline FormattedStringBuilder& getStringRef() {
        return fString;
    }
    inline const FormattedStringBuilder& getStringRef() const {
        return fString;
    }

private:
    FormattedStringBuilder fString;
    FormattedStringBuilder::Field fNumericField;

    bool nextPositionImpl(ConstrainedFieldPosition& cfpos, FormattedStringBuilder::Field numericField, UErrorCode& status) const;
    static bool isIntOrGroup(FormattedStringBuilder::Field field);
    static bool isNumericField(FormattedStringBuilder::Field field);
    int32_t trimBack(int32_t limit) const;
    int32_t trimFront(int32_t start) const;
};


// C API Helpers for FormattedValue
// Magic number as ASCII == "UFV"
struct UFormattedValueImpl;
typedef IcuCApiHelper<UFormattedValue, UFormattedValueImpl, 0x55465600> UFormattedValueApiHelper;
struct UFormattedValueImpl : public UMemory, public UFormattedValueApiHelper {
    // This pointer should be set by the child class.
    FormattedValue* fFormattedValue = nullptr;
};


/** Boilerplate to check for valid status before dereferencing the fData pointer. */
#define UPRV_FORMATTED_VALUE_METHOD_GUARD(returnExpression) \
    if (U_FAILURE(status)) { \
        return returnExpression; \
    } \
    if (fData == nullptr) { \
        status = fErrorCode; \
        return returnExpression; \
    } \


/** Implementation of the methods from U_FORMATTED_VALUE_SUBCLASS_AUTO. */
#define UPRV_FORMATTED_VALUE_SUBCLASS_AUTO_IMPL(Name) \
    Name::Name(Name&& src) U_NOEXCEPT \
            : fData(src.fData), fErrorCode(src.fErrorCode) { \
        src.fData = nullptr; \
        src.fErrorCode = U_INVALID_STATE_ERROR; \
    } \
    Name::~Name() { \
        delete fData; \
        fData = nullptr; \
    } \
    Name& Name::operator=(Name&& src) U_NOEXCEPT { \
        delete fData; \
        fData = src.fData; \
        src.fData = nullptr; \
        fErrorCode = src.fErrorCode; \
        src.fErrorCode = U_INVALID_STATE_ERROR; \
        return *this; \
    } \
    UnicodeString Name::toString(UErrorCode& status) const { \
        UPRV_FORMATTED_VALUE_METHOD_GUARD(ICU_Utility::makeBogusString()) \
        return fData->toString(status); \
    } \
    UnicodeString Name::toTempString(UErrorCode& status) const { \
        UPRV_FORMATTED_VALUE_METHOD_GUARD(ICU_Utility::makeBogusString()) \
        return fData->toTempString(status); \
    } \
    Appendable& Name::appendTo(Appendable& appendable, UErrorCode& status) const { \
        UPRV_FORMATTED_VALUE_METHOD_GUARD(appendable) \
        return fData->appendTo(appendable, status); \
    } \
    UBool Name::nextPosition(ConstrainedFieldPosition& cfpos, UErrorCode& status) const { \
        UPRV_FORMATTED_VALUE_METHOD_GUARD(FALSE) \
        return fData->nextPosition(cfpos, status); \
    }


/** Like UPRV_FORMATTED_VALUE_CAPI_AUTO_IMPL but without impl type declarations. */
#define UPRV_FORMATTED_VALUE_CAPI_NO_IMPLTYPE_AUTO_IMPL(CType, ImplType, HelperType, Prefix) \
    U_CAPI CType* U_EXPORT2 \
    Prefix ## _openResult (UErrorCode* ec) { \
        if (U_FAILURE(*ec)) { \
            return nullptr; \
        } \
        ImplType* impl = new ImplType(); \
        if (impl == nullptr) { \
            *ec = U_MEMORY_ALLOCATION_ERROR; \
            return nullptr; \
        } \
        return static_cast<HelperType*>(impl)->exportForC(); \
    } \
    U_DRAFT const UFormattedValue* U_EXPORT2 \
    Prefix ## _resultAsValue (const CType* uresult, UErrorCode* ec) { \
        const ImplType* result = HelperType::validate(uresult, *ec); \
        if (U_FAILURE(*ec)) { return nullptr; } \
        return static_cast<const UFormattedValueApiHelper*>(result)->exportConstForC(); \
    } \
    U_CAPI void U_EXPORT2 \
    Prefix ## _closeResult (CType* uresult) { \
        UErrorCode localStatus = U_ZERO_ERROR; \
        const ImplType* impl = HelperType::validate(uresult, localStatus); \
        delete impl; \
    }


/**
 * Implementation of the standard methods for a UFormattedValue "subclass" C API.
 * @param CPPType The public C++ type, like FormattedList
 * @param CType The public C type, like UFormattedList
 * @param ImplType A name to use for the implementation class
 * @param HelperType A name to use for the "mixin" typedef for C API conversion
 * @param Prefix The C API prefix, like ulistfmt
 * @param MagicNumber A unique 32-bit number to use to identify this type
 */
#define UPRV_FORMATTED_VALUE_CAPI_AUTO_IMPL(CPPType, CType, ImplType, HelperType, Prefix, MagicNumber) \
    U_NAMESPACE_BEGIN \
    class ImplType; \
    typedef IcuCApiHelper<CType, ImplType, MagicNumber> HelperType; \
    class ImplType : public UFormattedValueImpl, public HelperType { \
    public: \
        ImplType(); \
        ~ImplType(); \
        CPPType fImpl; \
    }; \
    ImplType::ImplType() { \
        fFormattedValue = &fImpl; \
    } \
    ImplType::~ImplType() {} \
    U_NAMESPACE_END \
    UPRV_FORMATTED_VALUE_CAPI_NO_IMPLTYPE_AUTO_IMPL(CType, ImplType, HelperType, Prefix)


U_NAMESPACE_END

#endif /* #if !UCONFIG_NO_FORMATTING */
#endif // __FORMVAL_IMPL_H__

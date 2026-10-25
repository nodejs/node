// © 2024 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html

#include "unicode/utypes.h"

#if !UCONFIG_NO_NORMALIZATION

#if !UCONFIG_NO_FORMATTING

#if !UCONFIG_NO_MF2

#include "unicode/messageformat2.h"
#include "messageformat2_allocation.h"
#include "messageformat2_checker.h"
#include "messageformat2_errors.h"
#include "messageformat2_evaluation.h"
#include "messageformat2_function_registry_internal.h"
#include "messageformat2_macros.h"
#include "messageformat2_parser.h"
#include "messageformat2_serializer.h"
#include "uvector.h" // U_ASSERT
#include "mutex.h"
#include "ucln_in.h"

U_NAMESPACE_BEGIN

namespace message2 {

// Singleton for the standard function registry
class MF2RegistrySingleton : public UObject {
public:
  MF2RegistrySingleton(UErrorCode &success);
  ~MF2RegistrySingleton(){}
  /** @return an alias to the standard registry */
  const MFFunctionRegistry* getStandardFunctionsRegistry() const {
    return &standardMFFunctionRegistry;
  }
private:
  MFFunctionRegistry standardMFFunctionRegistry;
};

MF2RegistrySingleton::MF2RegistrySingleton(UErrorCode &success) {
  // Set up the standard function registry
  MFFunctionRegistry::Builder standardFunctionsBuilder(success);

  LocalPointer<Function> dateTime(StandardFunctions::DateTime::dateTime(success));
  LocalPointer<Function> date(StandardFunctions::DateTime::date(success));
  LocalPointer<Function> time(StandardFunctions::DateTime::time(success));
  LocalPointer<Function> currency(StandardFunctions::Number::currency(success));
  LocalPointer<Function> number(StandardFunctions::Number::number(success));
  LocalPointer<Function> offset(StandardFunctions::Number::offset(success));
  LocalPointer<Function> percent(StandardFunctions::Number::percent(success));
  LocalPointer<Function> integer(StandardFunctions::Number::integer(success));
  LocalPointer<Function> string(StandardFunctions::String::string(success));
  LocalPointer<Function> testFunction(StandardFunctions::TestFunction::testFunction(success));
  LocalPointer<Function> testFormat(StandardFunctions::TestFunction::testFormat(success));
  LocalPointer<Function> testSelect(StandardFunctions::TestFunction::testSelect(success));
  CHECK_ERROR(success);
  standardFunctionsBuilder.adoptFunction(FunctionName(functions::DATETIME),
					 dateTime.orphan(), success)
    .adoptFunction(FunctionName(functions::DATE), date.orphan(), success)
    .adoptFunction(FunctionName(functions::TIME), time.orphan(), success)
    .adoptFunction(FunctionName(functions::CURRENCY), currency.orphan(), success)
    .adoptFunction(FunctionName(functions::NUMBER),
		   number.orphan(), success)
    .adoptFunction(FunctionName(functions::OFFSET),
                   offset.orphan(), success)
    .adoptFunction(FunctionName(functions::PER_CENT),
                   percent.orphan(), success)
    .adoptFunction(FunctionName(functions::INTEGER),
		   integer.orphan(), success)
    .adoptFunction(FunctionName(functions::STRING),
		   string.orphan(), success)
    .adoptFunction(FunctionName(functions::TEST_FUNCTION),
		   testFunction.orphan(), success)
    .adoptFunction(FunctionName(functions::TEST_FORMAT),
		   testFormat.orphan(), success)
    .adoptFunction(FunctionName(functions::TEST_SELECT),
		   testSelect.orphan(), success);
  CHECK_ERROR(success);
  standardMFFunctionRegistry = standardFunctionsBuilder.build();
  CHECK_ERROR(success);
  standardMFFunctionRegistry.checkStandard();
}

static MF2RegistrySingleton* mf2RegistrySingleton = nullptr;
static icu::UInitOnce gMF2RegistryInitOnce {};

  // Clean up shared Registry objects
  static UBool mf2_registry_cleanup() {
    if (mf2RegistrySingleton != nullptr) {
        delete mf2RegistrySingleton;
        mf2RegistrySingleton = nullptr;
    }
    gMF2RegistryInitOnce.reset();
    return true;
  }

  static void initRegistryOnce(UErrorCode& errorCode) {
    U_ASSERT(mf2RegistrySingleton == nullptr);

    mf2RegistrySingleton = new MF2RegistrySingleton( errorCode);

    if (mf2RegistrySingleton == nullptr) {
        errorCode = U_MEMORY_ALLOCATION_ERROR;
        mf2_registry_cleanup();
        return;
    }
    ucln_i18n_registerCleanup(UCLN_I18N_MF2_REGISTRY, mf2_registry_cleanup);
  }

  // ensure the registry is available
  static void initRegistry(UErrorCode& errorCode) {
    CHECK_ERROR(errorCode);
    umtx_initOnce(gMF2RegistryInitOnce, &initRegistryOnce, errorCode);
  }

  // API:
  const MFFunctionRegistry *MFFunctionRegistry::getStandardFunctionsRegistry(UErrorCode& errorCode) {
    initRegistry(errorCode);
    if (U_FAILURE(errorCode)) {
      return nullptr;
    } else if(mf2RegistrySingleton == nullptr) {
      // return error if we are about to return nullptr
      errorCode = U_MEMORY_ALLOCATION_ERROR;
      return nullptr;
    }
    return mf2RegistrySingleton->getStandardFunctionsRegistry();
  }

    // MessageFormatter::Builder

    // -------------------------------------
    // Creates a MessageFormat instance based on the pattern.

    void MessageFormatter::Builder::clearState() {
        normalizedInput.remove();
        delete errors;
        errors = nullptr;
    }

    MessageFormatter::Builder& MessageFormatter::Builder::setPattern(const UnicodeString& pat,
                                                                     UParseError& parseError,
                                                                     UErrorCode& errorCode) {
        clearState();
        // Create errors
        errors = create<StaticErrors>(StaticErrors(errorCode), errorCode);
        THIS_ON_ERROR(errorCode);

        // Parse the pattern
        MFDataModel::Builder tree(errorCode);
        Parser(pat, tree, *errors, normalizedInput, errorCode)
            .parse(parseError, errorCode);

        // Fail on syntax errors
        if (errors->hasSyntaxError()) {
            errors->checkErrors(errorCode);
            // Check that the checkErrors() method set the error code
            U_ASSERT(U_FAILURE(errorCode));
        }

        // Build the data model based on what was parsed
        dataModel = tree.build(errorCode);
        hasDataModel = true;
        hasPattern = true;
        pattern = pat;

        return *this;
    }

    // Precondition: `reg` is non-null
    // Does not adopt `reg`
    MessageFormatter::Builder& MessageFormatter::Builder::setFunctionRegistry(const MFFunctionRegistry& reg) {
        customMFFunctionRegistry = &reg;
        return *this;
    }

    MessageFormatter::Builder& MessageFormatter::Builder::setLocale(const Locale& loc) {
        locale = loc;
        return *this;
    }

    MessageFormatter::Builder& MessageFormatter::Builder::setDataModel(MFDataModel&& newDataModel) {
        clearState();
        hasPattern = false;
        hasDataModel = true;
        dataModel = std::move(newDataModel);

        return *this;
    }

    MessageFormatter::Builder&
        MessageFormatter::Builder::setErrorHandlingBehavior(
           MessageFormatter::UMFErrorHandlingBehavior type) {
               signalErrors = type == U_MF_STRICT;
               return *this;
    }

    MessageFormatter::Builder&
    MessageFormatter::Builder::setBidiIsolationStrategy(UMFBidiIsolationStrategy strategy) {
            bidiIsolationStrategy = strategy;
            return *this;
    }

    MessageFormatter::Builder&
    MessageFormatter::Builder::setBidiContext(MessageFormatter::UMFBidiContext dir) {
        msgdir = dir;
        return *this;
    }

    MessageFormatter::Builder&
    MessageFormatter::Builder::setBidiIsolationStyle(UMFBidiIsolationStyle style) {
            bidiStyle = style;
            return *this;
    }

    /*
      This build() method is non-destructive, which entails the risk that
      its borrowed MFFunctionRegistry and (if the setDataModel() method was called)
      MFDataModel pointers could become invalidated.
    */
    MessageFormatter MessageFormatter::Builder::build(UErrorCode& errorCode) const {
        return MessageFormatter(*this, errorCode);
    }

    MessageFormatter::Builder::Builder(UErrorCode& errorCode) : locale(Locale::getDefault()), customMFFunctionRegistry(nullptr) {
        // Initialize errors
        errors = new StaticErrors(errorCode);
        CHECK_ERROR(errorCode);
        if (errors == nullptr) {
            errorCode = U_MEMORY_ALLOCATION_ERROR;
        }
    }

    MessageFormatter::Builder::~Builder() {
        if (errors != nullptr) {
            delete errors;
            errors = nullptr;
        }
    }


    static UMFDirectionality
    resolveDirectionality(MessageFormatter::UMFBidiContext setting, const Locale& locale) {
        switch (setting) {
        case MessageFormatter::U_MF_BIDI_CONTEXT_LTR:
            return U_MF_DIRECTIONALITY_LTR;
        case MessageFormatter::U_MF_BIDI_CONTEXT_RTL:
            return U_MF_DIRECTIONALITY_RTL;
        default:
            return locale.isRightToLeft()
                ? U_MF_DIRECTIONALITY_RTL
                : U_MF_DIRECTIONALITY_LTR;
        }
    }

    // MessageFormatter

    MessageFormatter::MessageFormatter(const MessageFormatter::Builder& builder, UErrorCode &success) : locale(builder.locale), customMFFunctionRegistry(builder.customMFFunctionRegistry) {
        standardMFFunctionRegistry = MFFunctionRegistry::getStandardFunctionsRegistry(success);
        CHECK_ERROR(success);

        normalizedInput = builder.normalizedInput;
        signalErrors = builder.signalErrors;
        bidiIsolationStrategy = builder.bidiIsolationStrategy;
        bidiIsolationStyle = builder.bidiStyle;
        msgdir = resolveDirectionality(builder.msgdir, locale);

        // Build data model
        // First, check that there is a data model
        // (which might have been set by setDataModel(), or to
        // the data model parsed from the pattern by setPattern())

        if (!builder.hasDataModel) {
            success = U_INVALID_STATE_ERROR;
            return;
        }

        dataModel = builder.dataModel;
        if (builder.errors != nullptr) {
            errors = new StaticErrors(*builder.errors, success);
        } else {
            // Initialize errors
            LocalPointer<StaticErrors> errorsNew(new StaticErrors(success));
            CHECK_ERROR(success);
            errors = errorsNew.orphan();
        }

        // Note: we currently evaluate variables lazily,
        // without memoization. This call is still necessary
        // to check out-of-scope uses of local variables in
        // right-hand sides (unresolved variable errors can
        // only be checked when arguments are known)

        // Check for resolution errors
        Checker(dataModel, *errors, *this).check(success);
    }

    void MessageFormatter::cleanup() noexcept {
        if (errors != nullptr) {
            delete errors;
            errors = nullptr;
        }
    }

    MessageFormatter& MessageFormatter::operator=(MessageFormatter&& other) noexcept {
        cleanup();

        locale = std::move(other.locale);
        standardMFFunctionRegistry = std::move(other.standardMFFunctionRegistry);
        customMFFunctionRegistry = other.customMFFunctionRegistry;
        dataModel = std::move(other.dataModel);
        normalizedInput = std::move(other.normalizedInput);
        signalErrors = other.signalErrors;
        bidiIsolationStrategy = other.bidiIsolationStrategy;
        msgdir = other.msgdir;
        bidiIsolationStyle = other.bidiIsolationStyle;
        errors = other.errors;
        other.errors = nullptr;
        return *this;
    }

    const MFDataModel& MessageFormatter::getDataModel() const { return dataModel; }

    UnicodeString MessageFormatter::getPattern() const {
        // Converts the current data model back to a string
        UnicodeString result;
        Serializer serializer(getDataModel(), result);
        serializer.serialize();
        return result;
    }

    // Precondition: custom function registry exists
    const MFFunctionRegistry& MessageFormatter::getCustomMFFunctionRegistry() const {
        U_ASSERT(hasCustomMFFunctionRegistry());
        return *customMFFunctionRegistry;
    }

    MessageFormatter::~MessageFormatter() {
        cleanup();
    }

    // ---------------------------------------------------
    // Function registry

    bool MessageFormatter::isBuiltInFunction(const FunctionName& functionName) const {
        return standardMFFunctionRegistry->hasFunction(functionName);
    }

    const Function*
    MessageFormatter::lookupFunction(const FunctionName& functionName,
                                     UErrorCode& status) const {
        NULL_ON_ERROR(status);

        if (hasCustomMFFunctionRegistry()) {
            const MFFunctionRegistry& customMFFunctionRegistry = getCustomMFFunctionRegistry();
            const Function* function = customMFFunctionRegistry.getFunction(functionName);
            if (function != nullptr) {
                return function;
            }
        }
        if (isBuiltInFunction(functionName)) {
            return standardMFFunctionRegistry->getFunction(functionName);
        }
        // Either there is no custom function registry and the function
        // isn't built-in, or the function doesn't exist in either the built-in
        // or custom registry.
        // Unknown function error
        status = U_MF_UNKNOWN_FUNCTION_ERROR;
        return nullptr;
    }

    bool MessageFormatter::getDefaultFormatterNameByType(const UnicodeString& tag,
                                                         FunctionName& result) const {
        if (hasCustomMFFunctionRegistry()) {
            const MFFunctionRegistry& customMFFunctionRegistry = getCustomMFFunctionRegistry();
            return customMFFunctionRegistry.getDefaultFormatterNameByType(tag, result);
        }
        return false;
    }

    bool MessageFormatter::isCustomFunction(const FunctionName& fn) const {
        return hasCustomMFFunctionRegistry() && getCustomMFFunctionRegistry().getFunction(fn) != nullptr;
    }

} // namespace message2

U_NAMESPACE_END

#endif /* #if !UCONFIG_NO_MF2 */

#endif /* #if !UCONFIG_NO_FORMATTING */

#endif /* #if !UCONFIG_NO_NORMALIZATION */

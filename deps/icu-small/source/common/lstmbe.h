// © 2021 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html

#ifndef LSTMBE_H
#define LSTMBE_H

#include "unicode/utypes.h"

#if !UCONFIG_NO_BREAK_ITERATION

#include "unicode/uniset.h"
#include "unicode/ures.h"
#include "unicode/utext.h"
#include "unicode/utypes.h"

#include "brkeng.h"
#include "dictbe.h"
#include "uvectr32.h"

U_NAMESPACE_BEGIN

class Vectorizer;
struct LSTMData;

/*******************************************************************
 * LSTMBreakEngine
 */

/**
 * <p>LSTMBreakEngine is a kind of DictionaryBreakEngine that uses a
 * LSTM to determine language-specific breaks.</p>
 *
 * <p>After it is constructed a LSTMBreakEngine may be shared between
 * threads without synchronization.</p>
 */
class LSTMBreakEngine : public DictionaryBreakEngine {
public:
    /**
     * <p>Constructor.</p>
     */
    LSTMBreakEngine(const LSTMData* data, const UnicodeSet& set, UErrorCode &status);

    /**
     * <p>Virtual destructor.</p>
     */
    virtual ~LSTMBreakEngine();

    virtual const UChar* name() const;

protected:
    /**
     * <p>Divide up a range of known dictionary characters handled by this break engine.</p>
     *
     * @param text A UText representing the text
     * @param rangeStart The start of the range of dictionary characters
     * @param rangeEnd The end of the range of dictionary characters
     * @param foundBreaks Output of C array of int32_t break positions, or 0
     * @param status Information on any errors encountered.
     * @return The number of breaks found
     */
     virtual int32_t divideUpDictionaryRange(UText *text,
                                             int32_t rangeStart,
                                             int32_t rangeEnd,
                                             UVector32 &foundBreaks,
                                             UErrorCode& status) const override;
private:
    const LSTMData* fData;
    const Vectorizer* fVectorizer;
};

U_CAPI const LanguageBreakEngine* U_EXPORT2 CreateLSTMBreakEngine(
    UScriptCode script, const LSTMData* data, UErrorCode& status);

U_CAPI const LSTMData* U_EXPORT2 CreateLSTMData(
    UResourceBundle* rb, UErrorCode& status);

U_CAPI const LSTMData* U_EXPORT2 CreateLSTMDataForScript(
    UScriptCode script, UErrorCode& status);

U_CAPI void U_EXPORT2 DeleteLSTMData(const LSTMData* data);
U_CAPI const UChar* U_EXPORT2 LSTMDataName(const LSTMData* data);

U_NAMESPACE_END

#endif /* #if !UCONFIG_NO_BREAK_ITERATION */

#endif  /* LSTMBE_H */

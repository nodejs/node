// © 2016 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html
/******************************************************************************
 *   Copyright (C) 2009-2015, International Business Machines
 *   Corporation and others.  All Rights Reserved.
 *******************************************************************************
 */

#include "flagparser.h"
#include "filestrm.h"
#include "cstring.h"
#include "cmemory.h"

#define DEFAULT_BUFFER_SIZE 512

static int32_t currentBufferSize = DEFAULT_BUFFER_SIZE;

static int32_t extractFlag(char* buffer, int32_t bufferSize, char* flag, int32_t flagSize, const char ** flagNames, int32_t numOfFlags, UErrorCode *status);
static int32_t getFlagOffset(const char *buffer, int32_t bufferSize);

/*
 * Opens the given fileName and reads in the information storing the data in flagBuffer.
 */
U_CAPI int32_t U_EXPORT2
parseFlagsFile(const char *fileName, char **flagBuffer, int32_t flagBufferSize, const char ** flagNames, int32_t numOfFlags, UErrorCode *status) {
    char* buffer = NULL;
    char* tmpFlagBuffer = NULL;
    UBool allocateMoreSpace = false;
    int32_t idx, i;
    int32_t result = 0;

    FileStream *f = T_FileStream_open(fileName, "r");
    if (f == NULL) {
        *status = U_FILE_ACCESS_ERROR;
        goto parseFlagsFile_cleanup;
    }

    buffer = (char *)uprv_malloc(sizeof(char) * currentBufferSize);
    tmpFlagBuffer = (char *)uprv_malloc(sizeof(char) * flagBufferSize);

    if (buffer == NULL || tmpFlagBuffer == NULL) {
        *status = U_MEMORY_ALLOCATION_ERROR;
        goto parseFlagsFile_cleanup;
    }

    do {
        if (allocateMoreSpace) {
            allocateMoreSpace = false;
            currentBufferSize *= 2;
            uprv_free(buffer);
            buffer = (char *)uprv_malloc(sizeof(char) * currentBufferSize);
            if (buffer == NULL) {
                *status = U_MEMORY_ALLOCATION_ERROR;
                goto parseFlagsFile_cleanup;
            }
        }
        for (i = 0; i < numOfFlags;) {
            if (T_FileStream_readLine(f, buffer, currentBufferSize) == NULL) {
                /* End of file reached. */
                break;
            }
            if (buffer[0] == '#') {
                continue;
            }

            if ((int32_t)uprv_strlen(buffer) == (currentBufferSize - 1) && buffer[currentBufferSize-2] != '\n') {
                /* Allocate more space for buffer if it did not read the entire line */
                allocateMoreSpace = true;
                T_FileStream_rewind(f);
                break;
            } else {
                idx = extractFlag(buffer, currentBufferSize, tmpFlagBuffer, flagBufferSize, flagNames, numOfFlags, status);
                if (U_FAILURE(*status)) {
                    if (*status == U_BUFFER_OVERFLOW_ERROR) {
                        result = currentBufferSize;
                    } else {
                        result = -1;
                    }
                    break;
                } else {
                    if (flagNames != NULL) {
                        if (idx >= 0) {
                            uprv_strcpy(flagBuffer[idx], tmpFlagBuffer);
                        } else {
                            /* No match found.  Skip it. */
                            continue;
                        }
                    } else {
                        uprv_strcpy(flagBuffer[i++], tmpFlagBuffer);
                    }
                }
            }
        }
    } while (allocateMoreSpace && U_SUCCESS(*status));

parseFlagsFile_cleanup:
    uprv_free(tmpFlagBuffer);
    uprv_free(buffer);

    T_FileStream_close(f);
    
    if (U_FAILURE(*status) && *status != U_BUFFER_OVERFLOW_ERROR) {
        return -1;
    }

    if (U_SUCCESS(*status) && result == 0) {
        currentBufferSize = DEFAULT_BUFFER_SIZE;
    }

    return result;
}


/*
 * Extract the setting after the '=' and store it in flag excluding the newline character.
 */
static int32_t extractFlag(char* buffer, int32_t bufferSize, char* flag, int32_t flagSize, const char **flagNames, int32_t numOfFlags, UErrorCode *status) {
    int32_t i, idx = -1;
    char *pBuffer;
    int32_t offset=0;
    UBool bufferWritten = false;

    if (buffer[0] != 0) {
        /* Get the offset (i.e. position after the '=') */
        offset = getFlagOffset(buffer, bufferSize);
        pBuffer = buffer+offset;
        for(i = 0;;i++) {
            if (i >= flagSize) {
                *status = U_BUFFER_OVERFLOW_ERROR;
                return -1;
            }
            if (pBuffer[i+1] == 0) {
                /* Indicates a new line character. End here. */
                flag[i] = 0;
                break;
            }

            flag[i] = pBuffer[i];
            if (i == 0) {
                bufferWritten = true;
            }
        }
    }

    if (!bufferWritten) {
        flag[0] = 0;
    }

    if (flagNames != NULL && offset>0) {
        offset--;  /* Move offset back 1 because of '='*/
        for (i = 0; i < numOfFlags; i++) {
            if (uprv_strncmp(buffer, flagNames[i], offset) == 0) {
                idx = i;
                break;
            }
        }
    }

    return idx;
}

/*
 * Get the position after the '=' character.
 */
static int32_t getFlagOffset(const char *buffer, int32_t bufferSize) {
    int32_t offset = 0;

    for (offset = 0; offset < bufferSize;offset++) {
        if (buffer[offset] == '=') {
            offset++;
            break;
        }
    }

    if (offset == bufferSize || (offset - 1) == bufferSize) {
        offset = 0;
    }

    return offset;
}

// Copyright (C) 2016 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html
/******************************************************************************
 *   Copyright (C) 2009-2013, International Business Machines
 *   Corporation and others.  All Rights Reserved.
 *******************************************************************************
 */

#include "unicode/platform.h"
#if U_PLATFORM == U_PF_MINGW
// *cough* - for struct stat
#ifdef __STRICT_ANSI__
#undef __STRICT_ANSI__
#endif
#endif

#include "filetools.h"
#include "filestrm.h"
#include "charstr.h"
#include "cstring.h"
#include "unicode/putil.h"
#include "putilimp.h"

#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <time.h>
#include <string.h>

#if U_HAVE_DIRENT_H
#include <dirent.h>
typedef struct dirent DIRENT;

#define SKIP1 "."
#define SKIP2 ".."
#endif

static int32_t whichFileModTimeIsLater(const char *file1, const char *file2);

/*
 * Goes through the given directory recursive to compare each file's modification time with that of the file given.
 * Also can be given just one file to check against. Default value for isDir is FALSE.
 */
U_CAPI UBool U_EXPORT2
isFileModTimeLater(const char *filePath, const char *checkAgainst, UBool isDir) {
    UBool isLatest = TRUE;

    if (filePath == NULL || checkAgainst == NULL) {
        return FALSE;
    }

    if (isDir == TRUE) {
#if U_HAVE_DIRENT_H
        DIR *pDir = NULL;
        if ((pDir= opendir(checkAgainst)) != NULL) {
            DIR *subDirp = NULL;
            DIRENT *dirEntry = NULL;

            while ((dirEntry = readdir(pDir)) != NULL) {
                if (uprv_strcmp(dirEntry->d_name, SKIP1) != 0 && uprv_strcmp(dirEntry->d_name, SKIP2) != 0) {
                    UErrorCode status = U_ZERO_ERROR;
                    icu::CharString newpath(checkAgainst, -1, status);
                    newpath.append(U_FILE_SEP_STRING, -1, status);
                    newpath.append(dirEntry->d_name, -1, status);
                    if (U_FAILURE(status)) {
                        fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, u_errorName(status));
                        return FALSE;
                    };

                    if ((subDirp = opendir(newpath.data())) != NULL) {
                        /* If this new path is a directory, make a recursive call with the newpath. */
                        closedir(subDirp);
                        isLatest = isFileModTimeLater(filePath, newpath.data(), isDir);
                        if (!isLatest) {
                            break;
                        }
                    } else {
                        int32_t latest = whichFileModTimeIsLater(filePath, newpath.data());
                        if (latest < 0 || latest == 2) {
                            isLatest = FALSE;
                            break;
                        }
                    }

                }
            }
            closedir(pDir);
        } else {
            fprintf(stderr, "Unable to open directory: %s\n", checkAgainst);
            return FALSE;
        }
#endif
    } else {
        if (T_FileStream_file_exists(checkAgainst)) {
            int32_t latest = whichFileModTimeIsLater(filePath, checkAgainst);
            if (latest < 0 || latest == 2) {
                isLatest = FALSE;
            }
        } else {
            isLatest = FALSE;
        }
    }

    return isLatest;
}

/* Compares the mod time of both files returning a number indicating which one is later. -1 if error ocurs. */
static int32_t whichFileModTimeIsLater(const char *file1, const char *file2) {
    int32_t result = 0;
    struct stat stbuf1, stbuf2;

    if (stat(file1, &stbuf1) == 0 && stat(file2, &stbuf2) == 0) {
        time_t modtime1, modtime2;
        double diff;

        modtime1 = stbuf1.st_mtime;
        modtime2 = stbuf2.st_mtime;

        diff = difftime(modtime1, modtime2);
        if (diff < 0.0) {
            result = 2;
        } else if (diff > 0.0) {
            result = 1;
        }

    } else {
        fprintf(stderr, "Unable to get stats from file: %s or %s\n", file1, file2);
        result = -1;
    }

    return result;
}

/* Swap the file separater character given with the new one in the file path. */
U_CAPI void U_EXPORT2
swapFileSepChar(char *filePath, const char oldFileSepChar, const char newFileSepChar) {
    for (int32_t i = 0, length = uprv_strlen(filePath); i < length; i++) {
        filePath[i] = (filePath[i] == oldFileSepChar ) ? newFileSepChar : filePath[i];
    }
}

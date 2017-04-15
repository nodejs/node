// Copyright (C) 2016 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html
/******************************************************************************
 *   Copyright (C) 2008-2015, International Business Machines
 *   Corporation and others.  All Rights Reserved.
 *******************************************************************************
 */
#include "unicode/utypes.h"
#include "unicode/localpointer.h"
#include "unicode/putil.h"
#include "cstring.h"
#include "toolutil.h"
#include "uoptions.h"
#include "uparse.h"
#include "package.h"
#include "pkg_icu.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// read a file list -------------------------------------------------------- ***

U_NAMESPACE_USE

static const struct {
    const char *suffix;
    int32_t length;
} listFileSuffixes[]={
    { ".txt", 4 },
    { ".lst", 4 },
    { ".tmp", 4 }
};

/* check for multiple text file suffixes to see if this list name is a text file name */
static UBool
isListTextFile(const char *listname) {
    const char *listNameEnd=strchr(listname, 0);
    const char *suffix;
    int32_t i, length;
    for(i=0; i<UPRV_LENGTHOF(listFileSuffixes); ++i) {
        suffix=listFileSuffixes[i].suffix;
        length=listFileSuffixes[i].length;
        if((listNameEnd-listname)>length && 0==memcmp(listNameEnd-length, suffix, length)) {
            return TRUE;
        }
    }
    return FALSE;
}

/*
 * Read a file list.
 * If the listname ends with ".txt", then read the list file
 * (in the system/ invariant charset).
 * If the listname ends with ".dat", then read the ICU .dat package file.
 * Otherwise, read the file itself as a single-item list.
 */
U_CAPI Package * U_EXPORT2
readList(const char *filesPath, const char *listname, UBool readContents, Package *listPkgIn) {
    Package *listPkg = listPkgIn;
    FILE *file;
    const char *listNameEnd;

    if(listname==NULL || listname[0]==0) {
        fprintf(stderr, "missing list file\n");
        return NULL;
    }

    if (listPkg == NULL) {
        listPkg=new Package();
        if(listPkg==NULL) {
            fprintf(stderr, "icupkg: not enough memory\n");
            exit(U_MEMORY_ALLOCATION_ERROR);
        }
    }

    listNameEnd=strchr(listname, 0);
    if(isListTextFile(listname)) {
        // read the list file
        char line[1024];
        char *end;
        const char *start;

        file=fopen(listname, "r");
        if(file==NULL) {
            fprintf(stderr, "icupkg: unable to open list file \"%s\"\n", listname);
            delete listPkg;
            exit(U_FILE_ACCESS_ERROR);
        }

        while(fgets(line, sizeof(line), file)) {
            // remove comments
            end=strchr(line, '#');
            if(end!=NULL) {
                *end=0;
            } else {
                // remove trailing CR LF
                end=strchr(line, 0);
                while(line<end && (*(end-1)=='\r' || *(end-1)=='\n')) {
                    *--end=0;
                }
            }

            // check first non-whitespace character and
            // skip empty lines and
            // skip lines starting with reserved characters
            start=u_skipWhitespace(line);
            if(*start==0 || NULL!=strchr(U_PKG_RESERVED_CHARS, *start)) {
                continue;
            }

            // take whitespace-separated items from the line
            for(;;) {
                // find whitespace after the item or the end of the line
                for(end=(char *)start; *end!=0 && *end!=' ' && *end!='\t'; ++end) {}
                if(*end==0) {
                    // this item is the last one on the line
                    end=NULL;
                } else {
                    // the item is terminated by whitespace, terminate it with NUL
                    *end=0;
                }
                if(readContents) {
                    listPkg->addFile(filesPath, start);
                } else {
                    listPkg->addItem(start);
                }

                // find the start of the next item or exit the loop
                if(end==NULL || *(start=u_skipWhitespace(end+1))==0) {
                    break;
                }
            }
        }
        fclose(file);
    } else if((listNameEnd-listname)>4 && 0==memcmp(listNameEnd-4, ".dat", 4)) {
        // read the ICU .dat package
        // Accept a .dat file whose name differs from the ToC prefixes.
        listPkg->setAutoPrefix();
        listPkg->readPackage(listname);
    } else {
        // list the single file itself
        if(readContents) {
            listPkg->addFile(filesPath, listname);
        } else {
            listPkg->addItem(listname);
        }
    }

    return listPkg;
}

U_CAPI int U_EXPORT2
writePackageDatFile(const char *outFilename, const char *outComment, const char *sourcePath, const char *addList, Package *pkg, char outType) {
    LocalPointer<Package> ownedPkg;
    LocalPointer<Package> addListPkg;

    if (pkg == NULL) {
        ownedPkg.adoptInstead(new Package);
        if(ownedPkg.isNull()) {
            fprintf(stderr, "icupkg: not enough memory\n");
            return U_MEMORY_ALLOCATION_ERROR;
        }
        pkg = ownedPkg.getAlias();

        addListPkg.adoptInstead(readList(sourcePath, addList, TRUE, NULL));
        if(addListPkg.isValid()) {
            pkg->addItems(*addListPkg);
        } else {
            return U_ILLEGAL_ARGUMENT_ERROR;
        }
    }

    pkg->writePackage(outFilename, outType, outComment);
    return 0;
}

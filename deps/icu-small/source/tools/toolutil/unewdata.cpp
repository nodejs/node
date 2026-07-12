// © 2016 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html
/*
*******************************************************************************
*
*   Copyright (C) 1999-2010, International Business Machines
*   Corporation and others.  All Rights Reserved.
*
*******************************************************************************
*   file name:  unewdata.c
*   encoding:   UTF-8
*   tab size:   8 (not used)
*   indentation:4
*
*   created on: 1999oct25
*   created by: Markus W. Scherer
*/

#include <stdio.h>
#include "unicode/utypes.h"
#include "unicode/putil.h"
#include "unicode/ustring.h"
#include "cmemory.h"
#include "cstring.h"
#include "filestrm.h"
#include "unicode/udata.h"
#include "unewdata.h"

struct UNewDataMemory {
    FileStream *file;
    uint16_t headerSize;
    uint8_t magic1, magic2;
};

U_CAPI UNewDataMemory * U_EXPORT2
udata_create(const char *dir, const char *type, const char *name,
             const UDataInfo *pInfo,
             const char *comment,
             UErrorCode *pErrorCode) {
    UNewDataMemory *pData;
    uint16_t headerSize, commentLength;
    char filename[512];
    uint8_t bytes[16];
    int32_t length;

    if(pErrorCode==nullptr || U_FAILURE(*pErrorCode)) {
        return nullptr;
    } else if(name==nullptr || *name==0 || pInfo==nullptr) {
        *pErrorCode=U_ILLEGAL_ARGUMENT_ERROR;
        return nullptr;
    }

    /* allocate the data structure */
    pData=(UNewDataMemory *)uprv_malloc(sizeof(UNewDataMemory));
    if(pData==nullptr) {
        *pErrorCode=U_MEMORY_ALLOCATION_ERROR;
        return nullptr;
    }

    char dirSepChar = U_FILE_SEP_CHAR;
#if (U_FILE_SEP_CHAR != U_FILE_ALT_SEP_CHAR)
    // We may need to append a different directory separator when building for Cygwin or MSYS2.
    if(dir && *dir) {
      if(!uprv_strchr(dir, U_FILE_SEP_CHAR) && uprv_strchr(dir, U_FILE_ALT_SEP_CHAR)) {
          dirSepChar = U_FILE_ALT_SEP_CHAR;
      }
    }
#endif

    /* Check that the full path won't be too long */
    length = 0;					/* Start with nothing */
    if(dir != nullptr  && *dir !=0)	/* Add directory length if one was given */
    {
    	length += static_cast<int32_t>(strlen(dir));

    	/* Add 1 if dir doesn't end with path sep */
        if (dir[strlen(dir) - 1]!= dirSepChar) {
            length++;
        }
	}
    length += static_cast<int32_t>(strlen(name));		/* Add the filename length */

    if(type != nullptr  && *type !=0) { /* Add directory length if  given */
        length += static_cast<int32_t>(strlen(type));
    }


     /* LDH buffer Length error check */
    if(length  > ((int32_t)sizeof(filename) - 1))
    {
   	    *pErrorCode = U_BUFFER_OVERFLOW_ERROR;
   	    uprv_free(pData);
	    return nullptr;
    }

    /* open the output file */
    if(dir!=nullptr && *dir!=0) { /* if dir has a value, we prepend it to the filename */
        char *p=filename+strlen(dir);
        uprv_strcpy(filename, dir);
        if (*(p-1)!=dirSepChar) {
            *p++=dirSepChar;
            *p=0;
        }
    } else { /* otherwise, we'll output to the current dir */
        filename[0]=0;
    }
    uprv_strcat(filename, name);
    if(type!=nullptr && *type!=0) {
        uprv_strcat(filename, ".");
        uprv_strcat(filename, type);
    }
    pData->file=T_FileStream_open(filename, "wb");
    if(pData->file==nullptr) {
        uprv_free(pData);
        *pErrorCode=U_FILE_ACCESS_ERROR;
        return nullptr;
    }

    /* write the header information */
    headerSize=(uint16_t)(pInfo->size+4);
    if(comment!=nullptr && *comment!=0) {
        commentLength=(uint16_t)(uprv_strlen(comment)+1);
        headerSize+=commentLength;
    } else {
        commentLength=0;
    }

    /* write the size of the header, take padding into account */
    pData->headerSize=(uint16_t)((headerSize+15)&~0xf);
    pData->magic1=0xda;
    pData->magic2=0x27;
    T_FileStream_write(pData->file, &pData->headerSize, 4);

    /* write the information data */
    T_FileStream_write(pData->file, pInfo, pInfo->size);

    /* write the comment */
    if(commentLength>0) {
        T_FileStream_write(pData->file, comment, commentLength);
    }

    /* write padding bytes to align the data section to 16 bytes */
    headerSize&=0xf;
    if(headerSize!=0) {
        headerSize=(uint16_t)(16-headerSize);
        uprv_memset(bytes, 0, headerSize);
        T_FileStream_write(pData->file, bytes, headerSize);
    }

    return pData;
}

U_CAPI uint32_t U_EXPORT2
udata_finish(UNewDataMemory *pData, UErrorCode *pErrorCode) {
    uint32_t fileLength=0;

    if(pErrorCode==nullptr || U_FAILURE(*pErrorCode)) {
        return 0;
    }

    if(pData!=nullptr) {
        if(pData->file!=nullptr) {
            /* fflush(pData->file);*/
            fileLength=T_FileStream_size(pData->file);
            if(T_FileStream_error(pData->file)) {
                *pErrorCode=U_FILE_ACCESS_ERROR;
            } else {
                fileLength-=pData->headerSize;
            }
            T_FileStream_close(pData->file);
        }
        uprv_free(pData);
    }

    return fileLength;
}

/* dummy UDataInfo cf. udata.h */
static const UDataInfo dummyDataInfo = {
    sizeof(UDataInfo),
    0,

    U_IS_BIG_ENDIAN,
    U_CHARSET_FAMILY,
    U_SIZEOF_UCHAR,
    0,

    { 0, 0, 0, 0 },                 /* dummy dataFormat */
    { 0, 0, 0, 0 },                 /* dummy formatVersion */
    { 0, 0, 0, 0 }                  /* dummy dataVersion */
};

U_CAPI void U_EXPORT2
udata_createDummy(const char *dir, const char *type, const char *name, UErrorCode *pErrorCode) {
    if(U_SUCCESS(*pErrorCode)) {
        udata_finish(udata_create(dir, type, name, &dummyDataInfo, nullptr, pErrorCode), pErrorCode);
        if(U_FAILURE(*pErrorCode)) {
            fprintf(stderr, "error %s writing dummy data file %s" U_FILE_SEP_STRING "%s.%s\n",
                    u_errorName(*pErrorCode), dir, name, type);
            exit(*pErrorCode);
        }
    }
}

U_CAPI void U_EXPORT2
udata_write8(UNewDataMemory *pData, uint8_t byte) {
    if(pData!=nullptr && pData->file!=nullptr) {
        T_FileStream_write(pData->file, &byte, 1);
    }
}

U_CAPI void U_EXPORT2
udata_write16(UNewDataMemory *pData, uint16_t word) {
    if(pData!=nullptr && pData->file!=nullptr) {
        T_FileStream_write(pData->file, &word, 2);
    }
}

U_CAPI void U_EXPORT2
udata_write32(UNewDataMemory *pData, uint32_t wyde) {
    if(pData!=nullptr && pData->file!=nullptr) {
        T_FileStream_write(pData->file, &wyde, 4);
    }
}

U_CAPI void U_EXPORT2
udata_writeBlock(UNewDataMemory *pData, const void *s, int32_t length) {
    if(pData!=nullptr && pData->file!=nullptr) {
        if(length>0) {
            T_FileStream_write(pData->file, s, length);
        }
    }
}

U_CAPI void U_EXPORT2
udata_writePadding(UNewDataMemory *pData, int32_t length) {
    static const uint8_t padding[16]={
        0xaa, 0xaa, 0xaa, 0xaa,
        0xaa, 0xaa, 0xaa, 0xaa,
        0xaa, 0xaa, 0xaa, 0xaa,
        0xaa, 0xaa, 0xaa, 0xaa
    };
    if(pData!=nullptr && pData->file!=nullptr) {
        while(length>=16) {
            T_FileStream_write(pData->file, padding, 16);
            length-=16;
        }
        if(length>0) {
            T_FileStream_write(pData->file, padding, length);
        }
    }
}

U_CAPI void U_EXPORT2
udata_writeString(UNewDataMemory *pData, const char *s, int32_t length) {
    if(pData!=nullptr && pData->file!=nullptr) {
        if(length==-1) {
            length=(int32_t)uprv_strlen(s);
        }
        if(length>0) {
            T_FileStream_write(pData->file, s, length);
        }
    }
}

U_CAPI void U_EXPORT2
udata_writeUString(UNewDataMemory *pData, const char16_t *s, int32_t length) {
    if(pData!=nullptr && pData->file!=nullptr) {
        if(length==-1) {
            length=u_strlen(s);
        }
        if(length>0) {
            T_FileStream_write(pData->file, s, length*sizeof(char16_t));
        }
    }
}

/*
 * Hey, Emacs, please set the following:
 *
 * Local Variables:
 * indent-tabs-mode: nil
 * End:
 *
 */


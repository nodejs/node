/*
******************************************************************************
*
*   Copyright (C) 1999-2013, International Business Machines
*   Corporation and others.  All Rights Reserved.
*
******************************************************************************/


/*----------------------------------------------------------------------------
 *
 *       Memory mapped file wrappers for use by the ICU Data Implementation
 *       All of the platform-specific implementation for mapping data files
 *         is here.  The rest of the ICU Data implementation uses only the
 *         wrapper functions.
 *
 *----------------------------------------------------------------------------*/
/* Defines _XOPEN_SOURCE for access to POSIX functions.
 * Must be before any other #includes. */
#include "uposixdefs.h"

#include "unicode/putil.h"
#include "udatamem.h"
#include "umapfile.h"

/* memory-mapping base definitions ------------------------------------------ */

#if MAP_IMPLEMENTATION==MAP_WIN32
#   define WIN32_LEAN_AND_MEAN
#   define VC_EXTRALEAN
#   define NOUSER
#   define NOSERVICE
#   define NOIME
#   define NOMCX
#   include <windows.h>
#   include "cmemory.h"

    typedef HANDLE MemoryMap;

#   define IS_MAP(map) ((map)!=NULL)
#elif MAP_IMPLEMENTATION==MAP_POSIX || MAP_IMPLEMENTATION==MAP_390DLL
    typedef size_t MemoryMap;

#   define IS_MAP(map) ((map)!=0)

#   include <unistd.h>
#   include <sys/mman.h>
#   include <sys/stat.h>
#   include <fcntl.h>

#   ifndef MAP_FAILED
#       define MAP_FAILED ((void*)-1)
#   endif

#   if MAP_IMPLEMENTATION==MAP_390DLL
        /*   No memory mapping for 390 batch mode.  Fake it using dll loading.  */
#       include <dll.h>
#       include "cstring.h"
#       include "cmemory.h"
#       include "unicode/udata.h"
#       define LIB_PREFIX "lib"
#       define LIB_SUFFIX ".dll"
        /* This is inconvienient until we figure out what to do with U_ICUDATA_NAME in utypes.h */
#       define U_ICUDATA_ENTRY_NAME "icudt" U_ICU_VERSION_SHORT U_LIB_SUFFIX_C_NAME_STRING "_dat"
#   endif
#elif MAP_IMPLEMENTATION==MAP_STDIO
#   include <stdio.h>
#   include "cmemory.h"

    typedef void *MemoryMap;

#   define IS_MAP(map) ((map)!=NULL)
#endif

/*----------------------------------------------------------------------------*
 *                                                                            *
 *   Memory Mapped File support.  Platform dependent implementation of        *
 *                           functions used by the rest of the implementation.*
 *                                                                            *
 *----------------------------------------------------------------------------*/
#if MAP_IMPLEMENTATION==MAP_NONE
    U_CFUNC UBool
    uprv_mapFile(UDataMemory *pData, const char *path) {
        UDataMemory_init(pData); /* Clear the output struct. */
        return FALSE;            /* no file access */
    }

    U_CFUNC void uprv_unmapFile(UDataMemory *pData) {
        /* nothing to do */
    }
#elif MAP_IMPLEMENTATION==MAP_WIN32
    U_CFUNC UBool
    uprv_mapFile(
         UDataMemory *pData,    /* Fill in with info on the result doing the mapping. */
                                /*   Output only; any original contents are cleared.  */
         const char *path       /* File path to be opened/mapped                      */
         )
    {
        HANDLE map;
        HANDLE file;
        SECURITY_ATTRIBUTES mappingAttributes;
        SECURITY_ATTRIBUTES *mappingAttributesPtr = NULL;
        SECURITY_DESCRIPTOR securityDesc;

        UDataMemory_init(pData); /* Clear the output struct.        */

        /* open the input file */
        file=CreateFileA(path, GENERIC_READ, FILE_SHARE_READ, NULL,
            OPEN_EXISTING,
            FILE_ATTRIBUTE_NORMAL|FILE_FLAG_RANDOM_ACCESS, NULL);
        if(file==INVALID_HANDLE_VALUE) {
            return FALSE;
        }

        /* Declare and initialize a security descriptor.
           This is required for multiuser systems on Windows 2000 SP4 and beyond */
        if (InitializeSecurityDescriptor(&securityDesc, SECURITY_DESCRIPTOR_REVISION)) {
            /* give the security descriptor a Null Dacl done using the  "TRUE, (PACL)NULL" here	*/
            if (SetSecurityDescriptorDacl(&securityDesc, TRUE, (PACL)NULL, FALSE)) {
                /* Make the security attributes point to the security descriptor */
                uprv_memset(&mappingAttributes, 0, sizeof(mappingAttributes));
                mappingAttributes.nLength = sizeof(mappingAttributes);
                mappingAttributes.lpSecurityDescriptor = &securityDesc;
                mappingAttributes.bInheritHandle = FALSE; /* object uninheritable */
                mappingAttributesPtr = &mappingAttributes;
            }
        }
        /* else creating security descriptors can fail when we are on Windows 98,
           and mappingAttributesPtr == NULL for that case. */

        /* create an unnamed Windows file-mapping object for the specified file */
        map=CreateFileMapping(file, mappingAttributesPtr, PAGE_READONLY, 0, 0, NULL);
        CloseHandle(file);
        if(map==NULL) {
            return FALSE;
        }

        /* map a view of the file into our address space */
        pData->pHeader=(const DataHeader *)MapViewOfFile(map, FILE_MAP_READ, 0, 0, 0);
        if(pData->pHeader==NULL) {
            CloseHandle(map);
            return FALSE;
        }
        pData->map=map;
        return TRUE;
    }

    U_CFUNC void
    uprv_unmapFile(UDataMemory *pData) {
        if(pData!=NULL && pData->map!=NULL) {
            UnmapViewOfFile(pData->pHeader);
            CloseHandle(pData->map);
            pData->pHeader=NULL;
            pData->map=NULL;
        }
    }



#elif MAP_IMPLEMENTATION==MAP_POSIX
    U_CFUNC UBool
    uprv_mapFile(UDataMemory *pData, const char *path) {
        int fd;
        int length;
        struct stat mystat;
        void *data;

        UDataMemory_init(pData); /* Clear the output struct.        */

        /* determine the length of the file */
        if(stat(path, &mystat)!=0 || mystat.st_size<=0) {
            return FALSE;
        }
        length=mystat.st_size;

        /* open the file */
        fd=open(path, O_RDONLY);
        if(fd==-1) {
            return FALSE;
        }

        /* get a view of the mapping */
#if U_PLATFORM != U_PF_HPUX
        data=mmap(0, length, PROT_READ, MAP_SHARED,  fd, 0);
#else
        data=mmap(0, length, PROT_READ, MAP_PRIVATE, fd, 0);
#endif
        close(fd); /* no longer needed */
        if(data==MAP_FAILED) {
            return FALSE;
        }

        pData->map = (char *)data + length;
        pData->pHeader=(const DataHeader *)data;
        pData->mapAddr = data;
#if U_PLATFORM == U_PF_IPHONE
        posix_madvise(data, length, POSIX_MADV_RANDOM);
#endif
        return TRUE;
    }

    U_CFUNC void
    uprv_unmapFile(UDataMemory *pData) {
        if(pData!=NULL && pData->map!=NULL) {
            size_t dataLen = (char *)pData->map - (char *)pData->mapAddr;
            if(munmap(pData->mapAddr, dataLen)==-1) {
            }
            pData->pHeader=NULL;
            pData->map=0;
            pData->mapAddr=NULL;
        }
    }



#elif MAP_IMPLEMENTATION==MAP_STDIO
    /* copy of the filestrm.c/T_FileStream_size() implementation */
    static int32_t
    umap_fsize(FILE *f) {
        int32_t savedPos = ftell(f);
        int32_t size = 0;

        /*Changes by Bertrand A. D. doesn't affect the current position
        goes to the end of the file before ftell*/
        fseek(f, 0, SEEK_END);
        size = (int32_t)ftell(f);
        fseek(f, savedPos, SEEK_SET);
        return size;
    }

    U_CFUNC UBool
    uprv_mapFile(UDataMemory *pData, const char *path) {
        FILE *file;
        int32_t fileLength;
        void *p;

        UDataMemory_init(pData); /* Clear the output struct.        */
        /* open the input file */
        file=fopen(path, "rb");
        if(file==NULL) {
            return FALSE;
        }

        /* get the file length */
        fileLength=umap_fsize(file);
        if(ferror(file) || fileLength<=20) {
            fclose(file);
            return FALSE;
        }

        /* allocate the memory to hold the file data */
        p=uprv_malloc(fileLength);
        if(p==NULL) {
            fclose(file);
            return FALSE;
        }

        /* read the file */
        if(fileLength!=fread(p, 1, fileLength, file)) {
            uprv_free(p);
            fclose(file);
            return FALSE;
        }

        fclose(file);
        pData->map=p;
        pData->pHeader=(const DataHeader *)p;
        pData->mapAddr=p;
        return TRUE;
    }

    U_CFUNC void
    uprv_unmapFile(UDataMemory *pData) {
        if(pData!=NULL && pData->map!=NULL) {
            uprv_free(pData->map);
            pData->map     = NULL;
            pData->mapAddr = NULL;
            pData->pHeader = NULL;
        }
    }


#elif MAP_IMPLEMENTATION==MAP_390DLL
    /*  390 specific Library Loading.
     *  This is the only platform left that dynamically loads an ICU Data Library.
     *  All other platforms use .data files when dynamic loading is required, but
     *  this turn out to be awkward to support in 390 batch mode.
     *
     *  The idea here is to hide the fact that 390 is using dll loading from the
     *   rest of ICU, and make it look like there is file loading happening.
     *
     */

    static char *strcpy_returnEnd(char *dest, const char *src)
    {
        while((*dest=*src)!=0) {
            ++dest;
            ++src;
        }
        return dest;
    }
    
    /*------------------------------------------------------------------------------
     *                                                                              
     *  computeDirPath   given a user-supplied path of an item to be opened,             
     *                         compute and return 
     *                            - the full directory path to be used 
     *                              when opening the file.
     *                            - Pointer to null at end of above returned path    
     *
     *                       Parameters:
     *                          path:        input path.  Buffer is not altered.
     *                          pathBuffer:  Output buffer.  Any contents are overwritten.
     *
     *                       Returns:
     *                          Pointer to null termination in returned pathBuffer.
     *
     *                    TODO:  This works the way ICU historically has, but the
     *                           whole data fallback search path is so complicated that
     *                           proabably almost no one will ever really understand it,
     *                           the potential for confusion is large.  (It's not just 
     *                           this one function, but the whole scheme.)
     *                            
     *------------------------------------------------------------------------------*/
    static char *uprv_computeDirPath(const char *path, char *pathBuffer)
    {
        char   *finalSlash;       /* Ptr to last dir separator in input path, or null if none. */
        int32_t pathLen;          /* Length of the returned directory path                     */
        
        finalSlash = 0;
        if (path != 0) {
            finalSlash = uprv_strrchr(path, U_FILE_SEP_CHAR);
        }
        
        *pathBuffer = 0;
        if (finalSlash == 0) {
        /* No user-supplied path.  
            * Copy the ICU_DATA path to the path buffer and return that*/
            const char *icuDataDir;
            icuDataDir=u_getDataDirectory();
            if(icuDataDir!=NULL && *icuDataDir!=0) {
                return strcpy_returnEnd(pathBuffer, icuDataDir);
            } else {
                /* there is no icuDataDir either.  Just return the empty pathBuffer. */
                return pathBuffer;
            }
        } 
        
        /* User supplied path did contain a directory portion.
        * Copy it to the output path buffer */
        pathLen = (int32_t)(finalSlash - path + 1);
        uprv_memcpy(pathBuffer, path, pathLen);
        *(pathBuffer+pathLen) = 0;
        return pathBuffer+pathLen;
    }
    

#   define DATA_TYPE "dat"

    U_CFUNC UBool uprv_mapFile(UDataMemory *pData, const char *path) {
        const char *inBasename;
        char *basename;
        char pathBuffer[1024];
        const DataHeader *pHeader;
        dllhandle *handle;
        void *val=0;

        inBasename=uprv_strrchr(path, U_FILE_SEP_CHAR);
        if(inBasename==NULL) {
            inBasename = path;
        } else {
            inBasename++;
        }
        basename=uprv_computeDirPath(path, pathBuffer);
        if(uprv_strcmp(inBasename, U_ICUDATA_NAME".dat") != 0) {
            /* must mmap file... for build */
            int fd;
            int length;
            struct stat mystat;
            void *data;
            UDataMemory_init(pData); /* Clear the output struct. */

            /* determine the length of the file */
            if(stat(path, &mystat)!=0 || mystat.st_size<=0) {
                return FALSE;
            }
            length=mystat.st_size;

            /* open the file */
            fd=open(path, O_RDONLY);
            if(fd==-1) {
                return FALSE;
            }

            /* get a view of the mapping */
            data=mmap(0, length, PROT_READ, MAP_PRIVATE, fd, 0);
            close(fd); /* no longer needed */
            if(data==MAP_FAILED) {
                return FALSE;
            }
            pData->map = (char *)data + length;
            pData->pHeader=(const DataHeader *)data;
            pData->mapAddr = data;
            return TRUE;
        }

#       ifdef OS390BATCH
            /* ### hack: we still need to get u_getDataDirectory() fixed
            for OS/390 (batch mode - always return "//"? )
            and this here straightened out with LIB_PREFIX and LIB_SUFFIX (both empty?!)
            This is probably due to the strange file system on OS/390.  It's more like
            a database with short entry names than a typical file system. */
            /* U_ICUDATA_NAME should always have the correct name */
            /* BUT FOR BATCH MODE IT IS AN EXCEPTION BECAUSE */
            /* THE FIRST THREE LETTERS ARE PREASSIGNED TO THE */
            /* PROJECT!!!!! */
            uprv_strcpy(pathBuffer, "IXMI" U_ICU_VERSION_SHORT "DA");
#       else
            /* set up the library name */
            uprv_strcpy(basename, LIB_PREFIX U_LIBICUDATA_NAME U_ICU_VERSION_SHORT LIB_SUFFIX);
#       endif

#       ifdef UDATA_DEBUG
             fprintf(stderr, "dllload: %s ", pathBuffer);
#       endif

        handle=dllload(pathBuffer);

#       ifdef UDATA_DEBUG
               fprintf(stderr, " -> %08X\n", handle );
#       endif

        if(handle != NULL) {
               /* we have a data DLL - what kind of lookup do we need here? */
               /* try to find the Table of Contents */
               UDataMemory_init(pData); /* Clear the output struct.        */
               val=dllqueryvar((dllhandle*)handle, U_ICUDATA_ENTRY_NAME);
               if(val == 0) {
                    /* failed... so keep looking */
                    return FALSE;
               }
#              ifdef UDATA_DEBUG
                    fprintf(stderr, "dllqueryvar(%08X, %s) -> %08X\n", handle, U_ICUDATA_ENTRY_NAME, val);
#              endif

               pData->pHeader=(const DataHeader *)val;
               return TRUE;
         } else {
               return FALSE; /* no handle */
         }
    }

    U_CFUNC void uprv_unmapFile(UDataMemory *pData) {
        if(pData!=NULL && pData->map!=NULL) {
            uprv_free(pData->map);
            pData->map     = NULL;
            pData->mapAddr = NULL;
            pData->pHeader = NULL;
        }   
    }

#else
#   error MAP_IMPLEMENTATION is set incorrectly
#endif

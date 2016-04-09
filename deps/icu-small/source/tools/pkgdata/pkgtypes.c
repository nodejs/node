/**************************************************************************
*
*   Copyright (C) 2000-2016, International Business Machines
*   Corporation and others.  All Rights Reserved.
*
***************************************************************************
*   file name:  pkgdata.c
*   encoding:   ANSI X3.4 (1968)
*   tab size:   8 (not used)
*   indentation:4
*
*   created on: 2000may16
*   created by: Steven \u24C7 Loomis
*
*  common types for pkgdata
*/

#include <stdio.h>
#include <stdlib.h>
#include "unicode/utypes.h"
#include "unicode/putil.h"
#include "cmemory.h"
#include "cstring.h"
#include "pkgtypes.h"
#include "putilimp.h"

const char *pkg_writeCharListWrap(FileStream *s, CharList *l, const char *delim, const char *brk, int32_t quote)
{
    int32_t ln = 0;
    char buffer[1024];
    while(l != NULL)
    {
        if(l->str)
        {
            uprv_strncpy(buffer, l->str, 1020);
            buffer[1019]=0;

            if(quote < 0) { /* remove quotes */
                if(buffer[uprv_strlen(buffer)-1] == '"') {
                    buffer[uprv_strlen(buffer)-1] = '\0';
                }
                if(buffer[0] == '"') {
                    uprv_strcpy(buffer, buffer+1);
                }
            } else if(quote > 0) { /* add quotes */
                if(l->str[0] != '"') {
                    uprv_strcpy(buffer, "\"");
                    uprv_strncat(buffer, l->str,1020);
                }
                if(l->str[uprv_strlen(l->str)-1] != '"') {
                    uprv_strcat(buffer, "\"");
                }
            }
            T_FileStream_write(s, buffer, (int32_t)uprv_strlen(buffer));

            ln += (int32_t)uprv_strlen(l->str);
        }

        if(l->next && delim)
        {
            if(ln > 60 && brk) {
                ln  = 0;
                T_FileStream_write(s, brk, (int32_t)uprv_strlen(brk));
            }
            T_FileStream_write(s, delim, (int32_t)uprv_strlen(delim));
        }
        l = l->next;
    }
    return NULL;
}


const char *pkg_writeCharList(FileStream *s, CharList *l, const char *delim, int32_t quote)
{
    char buffer[1024];
    while(l != NULL)
    {
        if(l->str)
        {
            uprv_strncpy(buffer, l->str, 1023);
            buffer[1023]=0;
            if(uprv_strlen(l->str) >= 1023)
            {
                fprintf(stderr, "%s:%d: Internal error, line too long (greater than 1023 chars)\n",
                        __FILE__, __LINE__);
                exit(0);
            }
            if(quote < 0) { /* remove quotes */
                if(buffer[uprv_strlen(buffer)-1] == '"') {
                    buffer[uprv_strlen(buffer)-1] = '\0';
                }
                if(buffer[0] == '"') {
                    uprv_strcpy(buffer, buffer+1);
                }
            } else if(quote > 0) { /* add quotes */
                if(l->str[0] != '"') {
                    uprv_strcpy(buffer, "\"");
                    uprv_strcat(buffer, l->str);
                }
                if(l->str[uprv_strlen(l->str)-1] != '"') {
                    uprv_strcat(buffer, "\"");
                }
            }
            T_FileStream_write(s, buffer, (int32_t)uprv_strlen(buffer));
        }

        if(l->next && delim)
        {
            T_FileStream_write(s, delim, (int32_t)uprv_strlen(delim));
        }
        l = l->next;
    }
    return NULL;
}


/*
 * Count items . 0 if null
 */
uint32_t pkg_countCharList(CharList *l)
{
  uint32_t c = 0;
  while(l != NULL)
  {
    c++;
    l = l->next;
  }

  return c;
}

/*
 * Prepend string to CharList
 */
CharList *pkg_prependToList(CharList *l, const char *str)
{
  CharList *newList;
  newList = uprv_malloc(sizeof(CharList));

  /* test for NULL */
  if(newList == NULL) {
    return NULL;
  }

  newList->str = str;
  newList->next = l;
  return newList;
}

/*
 * append string to CharList. *end or even end can be null if you don't
 * know it.[slow]
 * Str is adopted!
 */
CharList *pkg_appendToList(CharList *l, CharList** end, const char *str)
{
  CharList *endptr = NULL, *tmp;

  if(end == NULL)
  {
    end = &endptr;
  }

  /* FIND the end */
  if((*end == NULL) && (l != NULL))
  {
    tmp = l;
    while(tmp->next)
    {
      tmp = tmp->next;
    }

    *end = tmp;
  }

  /* Create a new empty list and append it */
  if(l == NULL)
    {
      l = pkg_prependToList(NULL, str);
    }
  else
    {
      (*end)->next = pkg_prependToList(NULL, str);
    }

  /* Move the end pointer. */
  if(*end)
    {
      (*end) = (*end)->next;
    }
  else
    {
      *end = l;
    }

  return l;
}

char * convertToNativePathSeparators(char *path) {
#if defined(U_MAKE_IS_NMAKE)
    char *itr;
    while ((itr = uprv_strchr(path, U_FILE_ALT_SEP_CHAR))) {
        *itr = U_FILE_SEP_CHAR;
    }
#endif
    return path;
}

CharList *pkg_appendUniqueDirToList(CharList *l, CharList** end, const char *strAlias) {
    char aBuf[1024];
    char *rPtr;
    rPtr = uprv_strrchr(strAlias, U_FILE_SEP_CHAR);
#if (U_FILE_SEP_CHAR != U_FILE_ALT_SEP_CHAR)
    {
        char *aPtr = uprv_strrchr(strAlias, U_FILE_ALT_SEP_CHAR);
        if(!rPtr || /* regular char wasn't found or.. */
            (aPtr && (aPtr > rPtr)))
        { /* alt ptr exists and is to the right of r ptr */
            rPtr = aPtr; /* may copy NULL which is OK */
        }
    }
#endif
    if(!rPtr) {
        return l; /* no dir path */
    }
    if((rPtr-strAlias) >= UPRV_LENGTHOF(aBuf)) {
        fprintf(stderr, "## ERR: Path too long [%d chars]: %s\n", (int)sizeof(aBuf), strAlias);
        return l;
    }
    strncpy(aBuf, strAlias,(rPtr-strAlias));
    aBuf[rPtr-strAlias]=0;  /* no trailing slash */
    convertToNativePathSeparators(aBuf);

    if(!pkg_listContains(l, aBuf)) {
        return pkg_appendToList(l, end, uprv_strdup(aBuf));
    } else {
        return l; /* already found */
    }
}

#if 0
static CharList *
pkg_appendFromStrings(CharList *l, CharList** end, const char *s, int32_t len)
{
  CharList *endptr = NULL;
  const char *p;
  char *t;
  const char *targ;
  if(end == NULL) {
    end = &endptr;
  }

  if(len==-1) {
    len = uprv_strlen(s);
  }
  targ = s+len;

  while(*s && s<targ) {
    while(s<targ&&isspace(*s)) s++;
    for(p=s;s<targ&&!isspace(*p);p++);
    if(p!=s) {
      t = uprv_malloc(p-s+1);
      uprv_strncpy(t,s,p-s);
      t[p-s]=0;
      l=pkg_appendToList(l,end,t);
      fprintf(stderr, " P %s\n", t);
    }
    s=p;
  }

  return l;
}
#endif


/*
 * Delete list
 */
void pkg_deleteList(CharList *l)
{
  CharList *tmp;
  while(l != NULL)
  {
    uprv_free((void*)l->str);
    tmp = l;
    l = l->next;
    uprv_free(tmp);
  }
}

UBool  pkg_listContains(CharList *l, const char *str)
{
  for(;l;l=l->next){
    if(!uprv_strcmp(l->str, str)) {
      return TRUE;
    }
  }

  return FALSE;
}

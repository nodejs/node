// © 2016 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html
/*
*******************************************************************************
*
*   Copyright (C) 2000-2015, International Business Machines
*   Corporation and others.  All Rights Reserved.
*
*******************************************************************************
*   file name:  uoptions.c
*   encoding:   UTF-8
*   tab size:   8 (not used)
*   indentation:4
*
*   created on: 2000apr17
*   created by: Markus W. Scherer
*
*   This file provides a command line argument parser.
*/

#include "unicode/utypes.h"
#include "cstring.h"
#include "uoptions.h"

U_CAPI int U_EXPORT2
u_parseArgs(int argc, char* argv[],
            int optionCount, UOption options[]) {
    char *arg;
    int i=1, remaining=1;
    char c, stopOptions=0;

    while(i<argc) {
        arg=argv[i];
        if(!stopOptions && *arg=='-' && (c=arg[1])!=0) {
            /* process an option */
            UOption *option=nullptr;
            arg+=2;
            if(c=='-') {
                /* process a long option */
                if(*arg==0) {
                    /* stop processing options after "--" */
                    stopOptions=1;
                } else {
                    /* search for the option string */
                    int j;
                    for(j=0; j<optionCount; ++j) {
                        if(options[j].longName && uprv_strcmp(arg, options[j].longName)==0) {
                            option=options+j;
                            break;
                        }
                    }
                    if(option==nullptr) {
                        /* no option matches */
                        return -i;
                    }
                    option->doesOccur=1;

                    if(option->hasArg!=UOPT_NO_ARG) {
                        /* parse the argument for the option, if any */
                        if(i+1<argc && !(argv[i+1][0]=='-' && argv[i+1][1]!=0)) {
                            /* argument in the next argv[], and there is not an option in there */
                            option->value=argv[++i];
                        } else if(option->hasArg==UOPT_REQUIRES_ARG) {
                            /* there is no argument, but one is required: return with error */
                            option->doesOccur=0;
                            return -i;
                        }
                    }

                    if(option->optionFn!=nullptr && option->optionFn(option->context, option)<0) {
                        /* the option function was called and returned an error */
                        option->doesOccur=0;
                        return -i;
                    }
                }
            } else {
                /* process one or more short options */
                do {
                    /* search for the option letter */
                    int j;
                    for(j=0; j<optionCount; ++j) {
                        if(c==options[j].shortName) {
                            option=options+j;
                            break;
                        }
                    }
                    if(option==nullptr) {
                        /* no option matches */
                        return -i;
                    }
                    option->doesOccur=1;

                    if(option->hasArg!=UOPT_NO_ARG) {
                        /* parse the argument for the option, if any */
                        if(*arg!=0) {
                            /* argument following in the same argv[] */
                            option->value=arg;
                            /* do not process the rest of this arg as option letters */
                            break;
                        } else if(i+1<argc && !(argv[i+1][0]=='-' && argv[i+1][1]!=0)) {
                            /* argument in the next argv[], and there is not an option in there */
                            option->value=argv[++i];
                            /* this break is redundant because we know that *arg==0 */
                            break;
                        } else if(option->hasArg==UOPT_REQUIRES_ARG) {
                            /* there is no argument, but one is required: return with error */
                            option->doesOccur=0;
                            return -i;
                        }
                    }

                    if(option->optionFn!=nullptr && option->optionFn(option->context, option)<0) {
                        /* the option function was called and returned an error */
                        option->doesOccur=0;
                        return -i;
                    }

                    /* get the next option letter */
                    option=nullptr;
                    c=*arg++;
                } while(c!=0);
            }

            /* go to next argv[] */
            ++i;
        } else {
            /* move a non-option up in argv[] */
            argv[remaining++]=arg;
            ++i;
        }
    }
    return remaining;
}

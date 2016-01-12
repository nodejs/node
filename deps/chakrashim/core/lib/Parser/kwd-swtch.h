//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
// GENERATED FILE, DO NOT HAND-MODIFY!
// Generated with the following command line: wscript jsscan.js kwd-lsc.h kwd-swtch.h
// This should be regenerated whenever the keywords change.

    case 'a':
        if (identifyKwds)
        {
            switch (p[0]) {
            case 'w':
                if (p[1] == 'a' && p[2] == 'i' && p[3] == 't' && !IsIdContinueNext(p+4, last)) {
                    p += 4;
                    if (this->m_fAwaitIsKeyword || !this->m_parser || this->m_parser->IsStrictMode()) {
                        token = tkAWAIT;
                        goto LReserved;
                    }
                    goto LIdentifier;
                }
                break;
            case 'r':
                if (p[1] == 'g' && p[2] == 'u' && p[3] == 'm' && p[4] == 'e' && p[5] == 'n' && p[6] == 't' && p[7] == 's' && !IsIdContinueNext(p+8, last)) {
                    p += 8;
                    goto LArguments;
                }
                break;
            }
        }
        goto LIdentifier;
    case 'b':
        if (identifyKwds)
        {
            if (p[0] == 'r' && p[1] == 'e' && p[2] == 'a' && p[3] == 'k' && !IsIdContinueNext(p+4, last)) {
                p += 4;
                token = tkBREAK;
                goto LReserved;
            }
        }
        goto LIdentifier;
    case 'c':
        if (identifyKwds)
        {
            switch (p[0]) {
            case 'a':
                switch (p[1]) {
                case 's':
                    if (p[2] == 'e' && !IsIdContinueNext(p+3, last)) {
                        p += 3;
                        token = tkCASE;
                        goto LReserved;
                    }
                    break;
                case 't':
                    if (p[2] == 'c' && p[3] == 'h' && !IsIdContinueNext(p+4, last)) {
                        p += 4;
                        token = tkCATCH;
                        goto LReserved;
                    }
                    break;
                }
                break;
            case 'o':
                if (p[1] == 'n') {
                    switch (p[2]) {
                    case 't':
                        if (p[3] == 'i' && p[4] == 'n' && p[5] == 'u' && p[6] == 'e' && !IsIdContinueNext(p+7, last)) {
                            p += 7;
                            token = tkCONTINUE;
                            goto LReserved;
                        }
                        break;
                    case 's':
                        if (p[3] == 't' && !IsIdContinueNext(p+4, last)) {
                            p += 4;
                            token = tkCONST;
                            goto LReserved;
                        }
                        break;
                    }
                }
                break;
            case 'l':
                if (p[1] == 'a' && p[2] == 's' && p[3] == 's' && !IsIdContinueNext(p+4, last)) {
                    p += 4;
                    token = tkCLASS;
                    goto LReserved;
                }
                break;
            }
        }
        goto LIdentifier;
    case 'd':
        if (identifyKwds)
        {
            switch (p[0]) {
            case 'e':
                switch (p[1]) {
                case 'b':
                    if (p[2] == 'u' && p[3] == 'g' && p[4] == 'g' && p[5] == 'e' && p[6] == 'r' && !IsIdContinueNext(p+7, last)) {
                        p += 7;
                        token = tkDEBUGGER;
                        goto LReserved;
                    }
                    break;
                case 'f':
                    if (p[2] == 'a' && p[3] == 'u' && p[4] == 'l' && p[5] == 't' && !IsIdContinueNext(p+6, last)) {
                        p += 6;
                        token = tkDEFAULT;
                        goto LReserved;
                    }
                    break;
                case 'l':
                    if (p[2] == 'e' && p[3] == 't' && p[4] == 'e' && !IsIdContinueNext(p+5, last)) {
                        p += 5;
                        token = tkDELETE;
                        goto LReserved;
                    }
                    break;
                }
                break;
            case 'o':
                if (!IsIdContinueNext(p+1, last)) {
                    p += 1;
                    token = tkDO;
                    goto LReserved;
                }
                break;
            }
        }
        goto LIdentifier;
    case 'e':
        if (identifyKwds)
        {
            switch (p[0]) {
            case 'l':
                if (p[1] == 's' && p[2] == 'e' && !IsIdContinueNext(p+3, last)) {
                    p += 3;
                    token = tkELSE;
                    goto LReserved;
                }
                break;
            case 'x':
                switch (p[1]) {
                case 'p':
                    if (p[2] == 'o' && p[3] == 'r' && p[4] == 't' && !IsIdContinueNext(p+5, last)) {
                        p += 5;
                        token = tkEXPORT;
                        goto LReserved;
                    }
                    break;
                case 't':
                    if (p[2] == 'e' && p[3] == 'n' && p[4] == 'd' && p[5] == 's' && !IsIdContinueNext(p+6, last)) {
                        p += 6;
                        token = tkEXTENDS;
                        goto LReserved;
                    }
                    break;
                }
                break;
            case 'n':
                if (p[1] == 'u' && p[2] == 'm' && !IsIdContinueNext(p+3, last)) {
                    p += 3;
                    token = tkENUM;
                    goto LReserved;
                }
                break;
            case 'v':
                if (p[1] == 'a' && p[2] == 'l' && !IsIdContinueNext(p+3, last)) {
                    p += 3;
                    goto LEval;
                }
                break;
            }
        }
        goto LIdentifier;
    case 'f':
        if (identifyKwds)
        {
            switch (p[0]) {
            case 'a':
                if (p[1] == 'l' && p[2] == 's' && p[3] == 'e' && !IsIdContinueNext(p+4, last)) {
                    p += 4;
                    token = tkFALSE;
                    goto LReserved;
                }
                break;
            case 'i':
                if (p[1] == 'n' && p[2] == 'a' && p[3] == 'l' && p[4] == 'l' && p[5] == 'y' && !IsIdContinueNext(p+6, last)) {
                    p += 6;
                    token = tkFINALLY;
                    goto LReserved;
                }
                break;
            case 'o':
                if (p[1] == 'r' && !IsIdContinueNext(p+2, last)) {
                    p += 2;
                    token = tkFOR;
                    goto LReserved;
                }
                break;
            case 'u':
                if (p[1] == 'n' && p[2] == 'c' && p[3] == 't' && p[4] == 'i' && p[5] == 'o' && p[6] == 'n' && !IsIdContinueNext(p+7, last)) {
                    p += 7;
                    token = tkFUNCTION;
                    goto LReserved;
                }
                break;
            }
        }
        goto LIdentifier;
    case 'i':
        if (identifyKwds)
        {
            switch (p[0]) {
            case 'f':
                if (!IsIdContinueNext(p+1, last)) {
                    p += 1;
                    token = tkIF;
                    goto LReserved;
                }
                break;
            case 'n':
                switch (p[1]) {
                case 's':
                    if (p[2] == 't' && p[3] == 'a' && p[4] == 'n' && p[5] == 'c' && p[6] == 'e' && p[7] == 'o' && p[8] == 'f' && !IsIdContinueNext(p+9, last)) {
                        p += 9;
                        token = tkINSTANCEOF;
                        goto LReserved;
                    }
                    break;
                case 't':
                    if (p[2] == 'e' && p[3] == 'r' && p[4] == 'f' && p[5] == 'a' && p[6] == 'c' && p[7] == 'e' && !IsIdContinueNext(p+8, last)) {
                        p += 8;
                        if (!this->m_parser || this->m_parser->IsStrictMode()) {
                            token = tkINTERFACE;
                            goto LReserved;
                        }
                        goto LIdentifier;
                    }
                    break;
                }
                if (!IsIdContinueNext(p+1,last)) {
                    p += 1;
                    token = tkIN;
                    goto LReserved;
                }
                break;
            case 'm':
                if (p[1] == 'p') {
                    switch (p[2]) {
                    case 'o':
                        if (p[3] == 'r' && p[4] == 't' && !IsIdContinueNext(p+5, last)) {
                            p += 5;
                            token = tkIMPORT;
                            goto LReserved;
                        }
                        break;
                    case 'l':
                        if (p[3] == 'e' && p[4] == 'm' && p[5] == 'e' && p[6] == 'n' && p[7] == 't' && p[8] == 's' && !IsIdContinueNext(p+9, last)) {
                            p += 9;
                            if (!this->m_parser || this->m_parser->IsStrictMode()) {
                                token = tkIMPLEMENTS;
                                goto LReserved;
                            }
                            goto LIdentifier;
                        }
                        break;
                    }
                }
                break;
            }
        }
        goto LIdentifier;
    case 'n':
        if (identifyKwds)
        {
            switch (p[0]) {
            case 'e':
                if (p[1] == 'w' && !IsIdContinueNext(p+2, last)) {
                    p += 2;
                    token = tkNEW;
                    goto LReserved;
                }
                break;
            case 'u':
                if (p[1] == 'l' && p[2] == 'l' && !IsIdContinueNext(p+3, last)) {
                    p += 3;
                    token = tkNULL;
                    goto LReserved;
                }
                break;
            }
        }
        goto LIdentifier;
    case 'r':
        if (identifyKwds)
        {
            if (p[0] == 'e' && p[1] == 't' && p[2] == 'u' && p[3] == 'r' && p[4] == 'n' && !IsIdContinueNext(p+5, last)) {
                p += 5;
                token = tkRETURN;
                goto LReserved;
            }
        }
        goto LIdentifier;
    case 's':
        if (identifyKwds)
        {
            switch (p[0]) {
            case 'w':
                if (p[1] == 'i' && p[2] == 't' && p[3] == 'c' && p[4] == 'h' && !IsIdContinueNext(p+5, last)) {
                    p += 5;
                    token = tkSWITCH;
                    goto LReserved;
                }
                break;
            case 'u':
                if (p[1] == 'p' && p[2] == 'e' && p[3] == 'r' && !IsIdContinueNext(p+4, last)) {
                    p += 4;
                    token = tkSUPER;
                    goto LReserved;
                }
                break;
            case 't':
                if (p[1] == 'a' && p[2] == 't' && p[3] == 'i' && p[4] == 'c' && !IsIdContinueNext(p+5, last)) {
                    p += 5;
                    if (!this->m_parser || this->m_parser->IsStrictMode()) {
                        token = tkSTATIC;
                        goto LReserved;
                    }
                    goto LIdentifier;
                }
                break;
            }
        }
        goto LIdentifier;
    case 't':
        if (identifyKwds)
        {
            switch (p[0]) {
            case 'h':
                switch (p[1]) {
                case 'i':
                    if (p[2] == 's' && !IsIdContinueNext(p+3, last)) {
                        p += 3;
                        token = tkTHIS;
                        goto LReserved;
                    }
                    break;
                case 'r':
                    if (p[2] == 'o' && p[3] == 'w' && !IsIdContinueNext(p+4, last)) {
                        p += 4;
                        token = tkTHROW;
                        goto LReserved;
                    }
                    break;
                }
                break;
            case 'r':
                switch (p[1]) {
                case 'u':
                    if (p[2] == 'e' && !IsIdContinueNext(p+3, last)) {
                        p += 3;
                        token = tkTRUE;
                        goto LReserved;
                    }
                    break;
                case 'y':
                    if (!IsIdContinueNext(p+2, last)) {
                        p += 2;
                        token = tkTRY;
                        goto LReserved;
                    }
                    break;
                }
                break;
            case 'y':
                if (p[1] == 'p' && p[2] == 'e' && p[3] == 'o' && p[4] == 'f' && !IsIdContinueNext(p+5, last)) {
                    p += 5;
                    token = tkTYPEOF;
                    goto LReserved;
                }
                break;
            case 'a':
                if (p[1] == 'r' && p[2] == 'g' && p[3] == 'e' && p[4] == 't' && !IsIdContinueNext(p+5, last)) {
                    p += 5;
                    goto LTarget;
                }
                break;
            }
        }
        goto LIdentifier;
    case 'v':
        if (identifyKwds)
        {
            switch (p[0]) {
            case 'a':
                if (p[1] == 'r' && !IsIdContinueNext(p+2, last)) {
                    p += 2;
                    token = tkVAR;
                    goto LReserved;
                }
                break;
            case 'o':
                if (p[1] == 'i' && p[2] == 'd' && !IsIdContinueNext(p+3, last)) {
                    p += 3;
                    token = tkVOID;
                    goto LReserved;
                }
                break;
            }
        }
        goto LIdentifier;
    case 'w':
        if (identifyKwds)
        {
            switch (p[0]) {
            case 'h':
                if (p[1] == 'i' && p[2] == 'l' && p[3] == 'e' && !IsIdContinueNext(p+4, last)) {
                    p += 4;
                    token = tkWHILE;
                    goto LReserved;
                }
                break;
            case 'i':
                if (p[1] == 't' && p[2] == 'h' && !IsIdContinueNext(p+3, last)) {
                    p += 3;
                    token = tkWITH;
                    goto LReserved;
                }
                break;
            }
        }
        goto LIdentifier;
    case 'y':
        if (identifyKwds)
        {
            if (p[0] == 'i' && p[1] == 'e' && p[2] == 'l' && p[3] == 'd' && !IsIdContinueNext(p+4, last)) {
                p += 4;
                if (this->m_fYieldIsKeyword || !this->m_parser || this->m_parser->IsStrictMode()) {
                    token = tkYIELD;
                    goto LReserved;
                }
                goto LIdentifier;
            }
        }
        goto LIdentifier;
    case 'l':
        if (identifyKwds)
        {
            if (p[0] == 'e' && p[1] == 't' && !IsIdContinueNext(p+2, last)) {
                p += 2;
                if (!this->m_parser || this->m_parser->IsStrictMode()) {
                    token = tkLET;
                    goto LReserved;
                }
                goto LIdentifier;
            }
        }
        goto LIdentifier;
    case 'p':
        if (identifyKwds)
        {
            switch (p[0]) {
            case 'a':
                if (p[1] == 'c' && p[2] == 'k' && p[3] == 'a' && p[4] == 'g' && p[5] == 'e' && !IsIdContinueNext(p+6, last)) {
                    p += 6;
                    if (!this->m_parser || this->m_parser->IsStrictMode()) {
                        token = tkPACKAGE;
                        goto LReserved;
                    }
                    goto LIdentifier;
                }
                break;
            case 'r':
                switch (p[1]) {
                case 'i':
                    if (p[2] == 'v' && p[3] == 'a' && p[4] == 't' && p[5] == 'e' && !IsIdContinueNext(p+6, last)) {
                        p += 6;
                        if (!this->m_parser || this->m_parser->IsStrictMode()) {
                            token = tkPRIVATE;
                            goto LReserved;
                        }
                        goto LIdentifier;
                    }
                    break;
                case 'o':
                    if (p[2] == 't' && p[3] == 'e' && p[4] == 'c' && p[5] == 't' && p[6] == 'e' && p[7] == 'd' && !IsIdContinueNext(p+8, last)) {
                        p += 8;
                        if (!this->m_parser || this->m_parser->IsStrictMode()) {
                            token = tkPROTECTED;
                            goto LReserved;
                        }
                        goto LIdentifier;
                    }
                    break;
                }
                break;
            case 'u':
                if (p[1] == 'b' && p[2] == 'l' && p[3] == 'i' && p[4] == 'c' && !IsIdContinueNext(p+5, last)) {
                    p += 5;
                    if (!this->m_parser || this->m_parser->IsStrictMode()) {
                        token = tkPUBLIC;
                        goto LReserved;
                    }
                    goto LIdentifier;
                }
                break;
            }
        }
        goto LIdentifier;

    // characters not in a reserved word

              case 'g': case 'h':           case 'j':
    case 'k':           case 'm':           case 'o':
              case 'q':
    case 'u':                     case 'x':
    case 'z':
        goto LIdentifier;

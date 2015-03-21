/* krb5_asn.c */
/*
 * Written by Vern Staats <staatsvr@asc.hpc.mil> for the OpenSSL project, **
 * using ocsp/{*.h,*asn*.c} as a starting point
 */
/* ====================================================================
 * Copyright (c) 2000 The OpenSSL Project.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 *
 * 3. All advertising materials mentioning features or use of this
 *    software must display the following acknowledgment:
 *    "This product includes software developed by the OpenSSL Project
 *    for use in the OpenSSL Toolkit. (http://www.OpenSSL.org/)"
 *
 * 4. The names "OpenSSL Toolkit" and "OpenSSL Project" must not be used to
 *    endorse or promote products derived from this software without
 *    prior written permission. For written permission, please contact
 *    licensing@OpenSSL.org.
 *
 * 5. Products derived from this software may not be called "OpenSSL"
 *    nor may "OpenSSL" appear in their names without prior written
 *    permission of the OpenSSL Project.
 *
 * 6. Redistributions of any form whatsoever must retain the following
 *    acknowledgment:
 *    "This product includes software developed by the OpenSSL Project
 *    for use in the OpenSSL Toolkit (http://www.OpenSSL.org/)"
 *
 * THIS SOFTWARE IS PROVIDED BY THE OpenSSL PROJECT ``AS IS'' AND ANY
 * EXPRESSED OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE OpenSSL PROJECT OR
 * ITS CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 * ====================================================================
 *
 * This product includes cryptographic software written by Eric Young
 * (eay@cryptsoft.com).  This product includes software written by Tim
 * Hudson (tjh@cryptsoft.com).
 *
 */
#include <openssl/asn1.h>
#include <openssl/asn1t.h>
#include <openssl/krb5_asn.h>


ASN1_SEQUENCE(KRB5_ENCDATA) = {
        ASN1_EXP(KRB5_ENCDATA, etype,           ASN1_INTEGER,     0),
        ASN1_EXP_OPT(KRB5_ENCDATA, kvno,        ASN1_INTEGER,     1),
        ASN1_EXP(KRB5_ENCDATA, cipher,          ASN1_OCTET_STRING,2)
} ASN1_SEQUENCE_END(KRB5_ENCDATA)

IMPLEMENT_ASN1_FUNCTIONS(KRB5_ENCDATA)


ASN1_SEQUENCE(KRB5_PRINCNAME) = {
        ASN1_EXP(KRB5_PRINCNAME, nametype,      ASN1_INTEGER,     0),
        ASN1_EXP_SEQUENCE_OF(KRB5_PRINCNAME, namestring, ASN1_GENERALSTRING, 1)
} ASN1_SEQUENCE_END(KRB5_PRINCNAME)

IMPLEMENT_ASN1_FUNCTIONS(KRB5_PRINCNAME)

/* [APPLICATION 1] = 0x61 */
ASN1_SEQUENCE(KRB5_TKTBODY) = {
        ASN1_EXP(KRB5_TKTBODY, tktvno,          ASN1_INTEGER,     0),
        ASN1_EXP(KRB5_TKTBODY, realm,           ASN1_GENERALSTRING, 1),
        ASN1_EXP(KRB5_TKTBODY, sname,           KRB5_PRINCNAME,   2),
        ASN1_EXP(KRB5_TKTBODY, encdata,         KRB5_ENCDATA,     3)
} ASN1_SEQUENCE_END(KRB5_TKTBODY)

IMPLEMENT_ASN1_FUNCTIONS(KRB5_TKTBODY)


ASN1_ITEM_TEMPLATE(KRB5_TICKET) =
        ASN1_EX_TEMPLATE_TYPE(ASN1_TFLG_EXPTAG|ASN1_TFLG_APPLICATION, 1,
                        KRB5_TICKET, KRB5_TKTBODY)
ASN1_ITEM_TEMPLATE_END(KRB5_TICKET)

IMPLEMENT_ASN1_FUNCTIONS(KRB5_TICKET)

/* [APPLICATION 14] = 0x6e */
ASN1_SEQUENCE(KRB5_APREQBODY) = {
        ASN1_EXP(KRB5_APREQBODY, pvno,          ASN1_INTEGER,     0),
        ASN1_EXP(KRB5_APREQBODY, msgtype,       ASN1_INTEGER,     1),
        ASN1_EXP(KRB5_APREQBODY, apoptions,     ASN1_BIT_STRING,  2),
        ASN1_EXP(KRB5_APREQBODY, ticket,        KRB5_TICKET,      3),
        ASN1_EXP(KRB5_APREQBODY, authenticator, KRB5_ENCDATA,     4),
} ASN1_SEQUENCE_END(KRB5_APREQBODY)

IMPLEMENT_ASN1_FUNCTIONS(KRB5_APREQBODY)

ASN1_ITEM_TEMPLATE(KRB5_APREQ) =
        ASN1_EX_TEMPLATE_TYPE(ASN1_TFLG_EXPTAG|ASN1_TFLG_APPLICATION, 14,
                        KRB5_APREQ, KRB5_APREQBODY)
ASN1_ITEM_TEMPLATE_END(KRB5_APREQ)

IMPLEMENT_ASN1_FUNCTIONS(KRB5_APREQ)

/*  Authenticator stuff         */

ASN1_SEQUENCE(KRB5_CHECKSUM) = {
        ASN1_EXP(KRB5_CHECKSUM, ctype,          ASN1_INTEGER,     0),
        ASN1_EXP(KRB5_CHECKSUM, checksum,       ASN1_OCTET_STRING,1)
} ASN1_SEQUENCE_END(KRB5_CHECKSUM)

IMPLEMENT_ASN1_FUNCTIONS(KRB5_CHECKSUM)


ASN1_SEQUENCE(KRB5_ENCKEY) = {
        ASN1_EXP(KRB5_ENCKEY,   ktype,          ASN1_INTEGER,     0),
        ASN1_EXP(KRB5_ENCKEY,   keyvalue,       ASN1_OCTET_STRING,1)
} ASN1_SEQUENCE_END(KRB5_ENCKEY)

IMPLEMENT_ASN1_FUNCTIONS(KRB5_ENCKEY)

/* SEQ OF SEQ; see ASN1_EXP_SEQUENCE_OF_OPT() below */
ASN1_SEQUENCE(KRB5_AUTHDATA) = {
        ASN1_EXP(KRB5_AUTHDATA, adtype,         ASN1_INTEGER,     0),
        ASN1_EXP(KRB5_AUTHDATA, addata,         ASN1_OCTET_STRING,1)
} ASN1_SEQUENCE_END(KRB5_AUTHDATA)

IMPLEMENT_ASN1_FUNCTIONS(KRB5_AUTHDATA)

/* [APPLICATION 2] = 0x62 */
ASN1_SEQUENCE(KRB5_AUTHENTBODY) = {
        ASN1_EXP(KRB5_AUTHENTBODY,      avno,   ASN1_INTEGER,     0),
        ASN1_EXP(KRB5_AUTHENTBODY,      crealm, ASN1_GENERALSTRING, 1),
        ASN1_EXP(KRB5_AUTHENTBODY,      cname,  KRB5_PRINCNAME,   2),
        ASN1_EXP_OPT(KRB5_AUTHENTBODY,  cksum,  KRB5_CHECKSUM,    3),
        ASN1_EXP(KRB5_AUTHENTBODY,      cusec,  ASN1_INTEGER,     4),
        ASN1_EXP(KRB5_AUTHENTBODY,      ctime,  ASN1_GENERALIZEDTIME, 5),
        ASN1_EXP_OPT(KRB5_AUTHENTBODY,  subkey, KRB5_ENCKEY,      6),
        ASN1_EXP_OPT(KRB5_AUTHENTBODY,  seqnum, ASN1_INTEGER,     7),
        ASN1_EXP_SEQUENCE_OF_OPT
                    (KRB5_AUTHENTBODY,  authorization,  KRB5_AUTHDATA, 8),
} ASN1_SEQUENCE_END(KRB5_AUTHENTBODY)

IMPLEMENT_ASN1_FUNCTIONS(KRB5_AUTHENTBODY)

ASN1_ITEM_TEMPLATE(KRB5_AUTHENT) =
        ASN1_EX_TEMPLATE_TYPE(ASN1_TFLG_EXPTAG|ASN1_TFLG_APPLICATION, 2,
                        KRB5_AUTHENT, KRB5_AUTHENTBODY)
ASN1_ITEM_TEMPLATE_END(KRB5_AUTHENT)

IMPLEMENT_ASN1_FUNCTIONS(KRB5_AUTHENT)

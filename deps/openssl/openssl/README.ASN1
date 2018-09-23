
OpenSSL ASN1 Revision
=====================

This document describes some of the issues relating to the new ASN1 code.

Previous OpenSSL ASN1 problems
=============================

OK why did the OpenSSL ASN1 code need revising in the first place? Well
there are lots of reasons some of which are included below...

1. The code is difficult to read and write. For every single ASN1 structure
(e.g. SEQUENCE) four functions need to be written for new, free, encode and
decode operations. This is a very painful and error prone operation. Very few
people have ever written any OpenSSL ASN1 and those that have usually wish
they hadn't.

2. Partly because of 1. the code is bloated and takes up a disproportionate
amount of space. The SEQUENCE encoder is particularly bad: it essentially
contains two copies of the same operation, one to compute the SEQUENCE length
and the other to encode it.

3. The code is memory based: that is it expects to be able to read the whole
structure from memory. This is fine for small structures but if you have a
(say) 1Gb PKCS#7 signedData structure it isn't such a good idea...

4. The code for the ASN1 IMPLICIT tag is evil. It is handled by temporarily
changing the tag to the expected one, attempting to read it, then changing it
back again. This means that decode buffers have to be writable even though they
are ultimately unchanged. This gets in the way of constification.

5. The handling of EXPLICIT isn't much better. It adds a chunk of code into 
the decoder and encoder for every EXPLICIT tag.

6. APPLICATION and PRIVATE tags aren't even supported at all.

7. Even IMPLICIT isn't complete: there is no support for implicitly tagged
types that are not OPTIONAL.

8. Much of the code assumes that a tag will fit in a single octet. This is
only true if the tag is 30 or less (mercifully tags over 30 are rare).

9. The ASN1 CHOICE type has to be largely handled manually, there aren't any
macros that properly support it.

10. Encoders have no concept of OPTIONAL and have no error checking. If the
passed structure contains a NULL in a mandatory field it will not be encoded,
resulting in an invalid structure.

11. It is tricky to add ASN1 encoders and decoders to external applications.

Template model
==============

One of the major problems with revision is the sheer volume of the ASN1 code.
Attempts to change (for example) the IMPLICIT behaviour would result in a
modification of *every* single decode function. 

I decided to adopt a template based approach. I'm using the term 'template'
in a manner similar to SNACC templates: it has nothing to do with C++
templates.

A template is a description of an ASN1 module as several constant C structures.
It describes in a machine readable way exactly how the ASN1 structure should
behave. If this template contains enough detail then it is possible to write
versions of new, free, encode, decode (and possibly others operations) that
operate on templates.

Instead of having to write code to handle each operation only a single
template needs to be written. If new operations are needed (such as a 'print'
operation) only a single new template based function needs to be written 
which will then automatically handle all existing templates.

Plans for revision
==================

The revision will consist of the following steps. Other than the first two
these can be handled in any order.
 
o Design and write template new, free, encode and decode operations, initially
memory based. *DONE*

o Convert existing ASN1 code to template form. *IN PROGRESS*

o Convert an existing ASN1 compiler (probably SNACC) to output templates
in OpenSSL form.

o Add support for BIO based ASN1 encoders and decoders to handle large
structures, initially blocking I/O.

o Add support for non blocking I/O: this is quite a bit harder than blocking
I/O.

o Add new ASN1 structures, such as OCSP, CRMF, S/MIME v3 (CMS), attribute
certificates etc etc.

Description of major changes
============================

The BOOLEAN type now takes three values. 0xff is TRUE, 0 is FALSE and -1 is
absent. The meaning of absent depends on the context. If for example the
boolean type is DEFAULT FALSE (as in the case of the critical flag for
certificate extensions) then -1 is FALSE, if DEFAULT TRUE then -1 is TRUE.
Usually the value will only ever be read via an API which will hide this from
an application.

There is an evil bug in the old ASN1 code that mishandles OPTIONAL with
SEQUENCE OF or SET OF. These are both implemented as a STACK structure. The
old code would omit the structure if the STACK was NULL (which is fine) or if
it had zero elements (which is NOT OK). This causes problems because an empty
SEQUENCE OF or SET OF will result in an empty STACK when it is decoded but when
it is encoded it will be omitted resulting in different encodings. The new code
only omits the encoding if the STACK is NULL, if it contains zero elements it
is encoded and empty. There is an additional problem though: because an empty
STACK was omitted, sometimes the corresponding *_new() function would
initialize the STACK to empty so an application could immediately use it, if
this is done with the new code (i.e. a NULL) it wont work. Therefore a new
STACK should be allocated first. One instance of this is the X509_CRL list of
revoked certificates: a helper function X509_CRL_add0_revoked() has been added
for this purpose.

The X509_ATTRIBUTE structure used to have an element called 'set' which took
the value 1 if the attribute value was a SET OF or 0 if it was a single. Due
to the behaviour of CHOICE in the new code this has been changed to a field
called 'single' which is 0 for a SET OF and 1 for single. The old field has
been deleted to deliberately break source compatibility. Since this structure
is normally accessed via higher level functions this shouldn't break too much.

The X509_REQ_INFO certificate request info structure no longer has a field
called 'req_kludge'. This used to be set to 1 if the attributes field was
(incorrectly) omitted. You can check to see if the field is omitted now by
checking if the attributes field is NULL. Similarly if you need to omit
the field then free attributes and set it to NULL.

The top level 'detached' field in the PKCS7 structure is no longer set when
a PKCS#7 structure is read in. PKCS7_is_detached() should be called instead.
The behaviour of PKCS7_get_detached() is unaffected.

The values of 'type' in the GENERAL_NAME structure have changed. This is
because the old code use the ASN1 initial octet as the selector. The new
code uses the index in the ASN1_CHOICE template.

The DIST_POINT_NAME structure has changed to be a true CHOICE type.

typedef struct DIST_POINT_NAME_st {
int type;
union {
	STACK_OF(GENERAL_NAME) *fullname;
	STACK_OF(X509_NAME_ENTRY) *relativename;
} name;
} DIST_POINT_NAME;

This means that name.fullname or name.relativename should be set
and type reflects the option. That is if name.fullname is set then
type is 0 and if name.relativename is set type is 1.

With the old code using the i2d functions would typically involve:

unsigned char *buf, *p;
int len;
/* Find length of encoding */
len = i2d_SOMETHING(x, NULL);
/* Allocate buffer */
buf = OPENSSL_malloc(len);
if(buf == NULL) {
	/* Malloc error */
}
/* Use temp variable because &p gets updated to point to end of
 * encoding.
 */
p = buf;
i2d_SOMETHING(x, &p);


Using the new i2d you can also do:

unsigned char *buf = NULL;
int len;
len = i2d_SOMETHING(x, &buf);
if(len < 0) {
	/* Malloc error */
}

and it will automatically allocate and populate a buffer with the
encoding. After this call 'buf' will point to the start of the
encoding which is len bytes long.

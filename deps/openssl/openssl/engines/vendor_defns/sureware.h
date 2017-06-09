/*-
 * Written by Corinne Dive-Reclus(cdive@baltimore.com)
 *
 * Copyright@2001 Baltimore Technologies Ltd.
 *
 * THIS FILE IS PROVIDED BY BALTIMORE TECHNOLOGIES ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL BALTIMORE TECHNOLOGIES BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#ifdef WIN32
# define SW_EXPORT       __declspec ( dllexport )
#else
# define SW_EXPORT
#endif

/*
 *       List of exposed SureWare errors
 */
#define SUREWAREHOOK_ERROR_FAILED               -1
#define SUREWAREHOOK_ERROR_FALLBACK             -2
#define SUREWAREHOOK_ERROR_UNIT_FAILURE -3
#define SUREWAREHOOK_ERROR_DATA_SIZE -4
#define SUREWAREHOOK_ERROR_INVALID_PAD -5
/*-
* -----------------WARNING-----------------------------------
* In all the following functions:
* msg is a string with at least 24 bytes free.
* A 24 bytes string will be concatenated to the existing content of msg.
*/
/*-
*       SureWare Initialisation function
*       in param threadsafe, if !=0, thread safe enabled
*       return SureWareHOOK_ERROR_UNIT_FAILURE if failure, 1 if success
*/
typedef int SureWareHook_Init_t(char *const msg, int threadsafe);
extern SW_EXPORT SureWareHook_Init_t SureWareHook_Init;
/*-
*       SureWare Finish function
*/
typedef void SureWareHook_Finish_t(void);
extern SW_EXPORT SureWareHook_Finish_t SureWareHook_Finish;
/*-
*        PRE_CONDITION:
*               DO NOT CALL ANY OF THE FOLLOWING FUNCTIONS IN CASE OF INIT FAILURE
*/
/*-
*       SureWare RAND Bytes function
*       In case of failure, the content of buf is unpredictable.
*       return 1 if success
*                       SureWareHOOK_ERROR_FALLBACK if function not available in hardware
*                       SureWareHOOK_ERROR_FAILED if error while processing
*                       SureWareHOOK_ERROR_UNIT_FAILURE if hardware failure
*                       SUREWAREHOOK_ERROR_DATA_SIZE wrong size for buf
*
*       in/out param buf : a num bytes long buffer where random bytes will be put
*       in param num : the number of bytes into buf
*/
typedef int SureWareHook_Rand_Bytes_t(char *const msg, unsigned char *buf,
                                      int num);
extern SW_EXPORT SureWareHook_Rand_Bytes_t SureWareHook_Rand_Bytes;

/*-
*       SureWare RAND Seed function
*       Adds some seed to the Hardware Random Number Generator
*       return 1 if success
*                       SureWareHOOK_ERROR_FALLBACK if function not available in hardware
*                       SureWareHOOK_ERROR_FAILED if error while processing
*                       SureWareHOOK_ERROR_UNIT_FAILURE if hardware failure
*                       SUREWAREHOOK_ERROR_DATA_SIZE wrong size for buf
*
*       in param buf : the seed to add into the HRNG
*       in param num : the number of bytes into buf
*/
typedef int SureWareHook_Rand_Seed_t(char *const msg, const void *buf,
                                     int num);
extern SW_EXPORT SureWareHook_Rand_Seed_t SureWareHook_Rand_Seed;

/*-
*       SureWare Load Private Key function
*       return 1 if success
*                       SureWareHOOK_ERROR_FAILED if error while processing
*       No hardware is contact for this function.
*
*       in param key_id :the name of the private protected key file without the extension
                                                ".sws"
*       out param hptr : a pointer to a buffer allocated by SureWare_Hook
*       out param num: the effective key length in bytes
*       out param keytype: 1 if RSA 2 if DSA
*/
typedef int SureWareHook_Load_Privkey_t(char *const msg, const char *key_id,
                                        char **hptr, unsigned long *num,
                                        char *keytype);
extern SW_EXPORT SureWareHook_Load_Privkey_t SureWareHook_Load_Privkey;

/*-
*       SureWare Info Public Key function
*       return 1 if success
*                       SureWareHOOK_ERROR_FAILED if error while processing
*       No hardware is contact for this function.
*
*       in param key_id :the name of the private protected key file without the extension
                                                ".swp"
*       out param hptr : a pointer to a buffer allocated by SureWare_Hook
*       out param num: the effective key length in bytes
*       out param keytype: 1 if RSA 2 if DSA
*/
typedef int SureWareHook_Info_Pubkey_t(char *const msg, const char *key_id,
                                       unsigned long *num, char *keytype);
extern SW_EXPORT SureWareHook_Info_Pubkey_t SureWareHook_Info_Pubkey;

/*-
*       SureWare Load Public Key function
*       return 1 if success
*                       SureWareHOOK_ERROR_FAILED if error while processing
*       No hardware is contact for this function.
*
*       in param key_id :the name of the public protected key file without the extension
                                                ".swp"
*       in param num : the bytes size of n and e
*       out param n: where to write modulus in bn format
*       out param e: where to write exponent in bn format
*/
typedef int SureWareHook_Load_Rsa_Pubkey_t(char *const msg,
                                           const char *key_id,
                                           unsigned long num,
                                           unsigned long *n,
                                           unsigned long *e);
extern SW_EXPORT SureWareHook_Load_Rsa_Pubkey_t SureWareHook_Load_Rsa_Pubkey;

/*-
*       SureWare Load DSA Public Key function
*       return 1 if success
*                       SureWareHOOK_ERROR_FAILED if error while processing
*       No hardware is contact for this function.
*
*       in param key_id :the name of the public protected key file without the extension
                                                ".swp"
*       in param num : the bytes size of n and e
*       out param pub: where to write pub key in bn format
*       out param p: where to write prime in bn format
*       out param q: where to write sunprime (length 20 bytes) in bn format
*       out param g: where to write base in bn format
*/
typedef int SureWareHook_Load_Dsa_Pubkey_t(char *const msg,
                                           const char *key_id,
                                           unsigned long num,
                                           unsigned long *pub,
                                           unsigned long *p, unsigned long *q,
                                           unsigned long *g);
extern SW_EXPORT SureWareHook_Load_Dsa_Pubkey_t SureWareHook_Load_Dsa_Pubkey;

/*-
*       SureWare Free function
*       Destroy the key into the hardware if destroy==1
*/
typedef void SureWareHook_Free_t(char *p, int destroy);
extern SW_EXPORT SureWareHook_Free_t SureWareHook_Free;

#define SUREWARE_PKCS1_PAD 1
#define SUREWARE_ISO9796_PAD 2
#define SUREWARE_NO_PAD 0
/*-
* SureWare RSA Private Decryption
* return 1 if success
*                       SureWareHOOK_ERROR_FAILED if error while processing
*                       SureWareHOOK_ERROR_UNIT_FAILURE if hardware failure
*                       SUREWAREHOOK_ERROR_DATA_SIZE wrong size for buf
*
*       in param flen : byte size of from and to
*       in param from : encrypted data buffer, should be a not-null valid pointer
*       out param tlen: byte size of decrypted data, if error, unexpected value
*       out param to : decrypted data buffer, should be a not-null valid pointer
*   in param prsa: a protected key pointer, should be a not-null valid pointer
*   int padding: padding id as follow
*                                       SUREWARE_PKCS1_PAD
*                                       SUREWARE_NO_PAD
*
*/
typedef int SureWareHook_Rsa_Priv_Dec_t(char *const msg, int flen,
                                        unsigned char *from, int *tlen,
                                        unsigned char *to, char *prsa,
                                        int padding);
extern SW_EXPORT SureWareHook_Rsa_Priv_Dec_t SureWareHook_Rsa_Priv_Dec;
/*-
* SureWare RSA Signature
* return 1 if success
*                       SureWareHOOK_ERROR_FAILED if error while processing
*                       SureWareHOOK_ERROR_UNIT_FAILURE if hardware failure
*                       SUREWAREHOOK_ERROR_DATA_SIZE wrong size for buf
*
*       in param flen : byte size of from and to
*       in param from : encrypted data buffer, should be a not-null valid pointer
*       out param tlen: byte size of decrypted data, if error, unexpected value
*       out param to : decrypted data buffer, should be a not-null valid pointer
*   in param prsa: a protected key pointer, should be a not-null valid pointer
*   int padding: padding id as follow
*                                       SUREWARE_PKCS1_PAD
*                                       SUREWARE_ISO9796_PAD
*
*/
typedef int SureWareHook_Rsa_Sign_t(char *const msg, int flen,
                                    unsigned char *from, int *tlen,
                                    unsigned char *to, char *prsa,
                                    int padding);
extern SW_EXPORT SureWareHook_Rsa_Sign_t SureWareHook_Rsa_Sign;
/*-
* SureWare DSA Signature
* return 1 if success
*                       SureWareHOOK_ERROR_FAILED if error while processing
*                       SureWareHOOK_ERROR_UNIT_FAILURE if hardware failure
*                       SUREWAREHOOK_ERROR_DATA_SIZE wrong size for buf
*
*       in param flen : byte size of from and to
*       in param from : encrypted data buffer, should be a not-null valid pointer
*       out param to : decrypted data buffer, should be a 40bytes valid pointer
*   in param pdsa: a protected key pointer, should be a not-null valid pointer
*
*/
typedef int SureWareHook_Dsa_Sign_t(char *const msg, int flen,
                                    const unsigned char *from,
                                    unsigned long *r, unsigned long *s,
                                    char *pdsa);
extern SW_EXPORT SureWareHook_Dsa_Sign_t SureWareHook_Dsa_Sign;

/*-
* SureWare Mod Exp
* return 1 if success
*                       SureWareHOOK_ERROR_FAILED if error while processing
*                       SureWareHOOK_ERROR_UNIT_FAILURE if hardware failure
*                       SUREWAREHOOK_ERROR_DATA_SIZE wrong size for buf
*
*       mod and res are mlen bytes long.
*       exp is elen bytes long
*       data is dlen bytes long
*       mlen,elen and dlen are all multiple of sizeof(unsigned long)
*/
typedef int SureWareHook_Mod_Exp_t(char *const msg, int mlen,
                                   const unsigned long *mod, int elen,
                                   const unsigned long *exponent, int dlen,
                                   unsigned long *data, unsigned long *res);
extern SW_EXPORT SureWareHook_Mod_Exp_t SureWareHook_Mod_Exp;

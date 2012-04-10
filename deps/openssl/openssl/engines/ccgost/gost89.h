/**********************************************************************
 *                        gost89.h                                    *
 *             Copyright (c) 2005-2006 Cryptocom LTD                  *
 *     This file is distributed under the same license as OpenSSL     *
 *                                                                    *
 *          Declarations for GOST 28147-89 encryption algorithm       *
 *            No OpenSSL libraries required to compile and use        *
 *                       this code                                    *
 **********************************************************************/            
#ifndef GOST89_H
#define GOST89_H

/* Typedef for unsigned 32-bit integer */
#if __LONG_MAX__ > 2147483647L 
typedef unsigned int u4; 
#else 
typedef unsigned long u4; 
#endif 
/* Typedef for unsigned 8-bit integer */
typedef unsigned char byte; 

/* Internal representation of GOST substitution blocks */
typedef struct {
		byte k8[16];
		byte k7[16];
		byte k6[16];
		byte k5[16];
		byte k4[16];
		byte k3[16];
		byte k2[16];
		byte k1[16];
} gost_subst_block;		


/* Cipher context includes key and preprocessed  substitution block */
typedef struct { 
		u4 k[8]; 
		/* Constant s-boxes -- set up in gost_init(). */ 
		u4 k87[256],k65[256],k43[256],k21[256]; 
} gost_ctx; 
/* Note: encrypt and decrypt expect full blocks--padding blocks is 
         caller's responsibility. All bulk encryption is done in 
		 ECB mode by these calls. Other modes may be added easily 
		 enough.                                            */
/* Encrypt several full blocks in ECB mode */
void gost_enc(gost_ctx *ctx, const byte *clear,byte *cipher, int blocks); 
/* Decrypt several full blocks in ECB mode */
void gost_dec(gost_ctx *ctx, const byte *cipher,byte *clear, int blocks); 
/* Encrypts several full blocks in CFB mode using 8byte IV */
void gost_enc_cfb(gost_ctx *ctx,const byte *iv,const byte *clear,byte *cipher,int  blocks);
/* Decrypts several full blocks in CFB mode using 8byte IV */
void gost_dec_cfb(gost_ctx *ctx,const byte *iv,const byte *cipher,byte *clear,int  blocks);

/* Encrypt one  block */
void gostcrypt(gost_ctx *c, const byte *in, byte *out);
/* Decrypt one  block */
void gostdecrypt(gost_ctx *c, const byte *in,byte *out);
/* Set key into context */
void gost_key(gost_ctx *ctx, const byte *key); 
/* Get key from context */
void gost_get_key(gost_ctx *ctx, byte *key);
/* Set S-blocks into context */
void gost_init(gost_ctx *ctx, const gost_subst_block *subst_block); 
/* Clean up context */
void gost_destroy(gost_ctx *ctx);
/* Intermediate function used for calculate hash */
void gost_enc_with_key(gost_ctx *,byte *key,byte *inblock,byte *outblock);
/* Compute MAC of given length in bits from data */
int gost_mac(gost_ctx *ctx,int hmac_len,const unsigned char *data,
		unsigned int data_len,unsigned char *hmac) ;
/* Compute MAC of given length in bits from data, using non-zero 8-byte
 * IV (non-standard, for use in CryptoPro key transport only */
int gost_mac_iv(gost_ctx *ctx,int hmac_len,const unsigned char *iv,const unsigned char *data,
		unsigned int data_len,unsigned char *hmac) ;
/* Perform one step of MAC calculation like gostcrypt */
void mac_block(gost_ctx *c,byte *buffer,const  byte *block); 
/* Extracts MAC value from mac state buffer */
void get_mac(byte *buffer,int nbits,byte *out);
/* Implements cryptopro key meshing algorithm. Expect IV to be 8-byte size*/
void cryptopro_key_meshing(gost_ctx *ctx, unsigned char *iv);
/* Parameter sets specified in RFC 4357 */
extern gost_subst_block GostR3411_94_TestParamSet;
extern gost_subst_block GostR3411_94_CryptoProParamSet;
extern gost_subst_block Gost28147_TestParamSet;
extern gost_subst_block Gost28147_CryptoProParamSetA;
extern gost_subst_block Gost28147_CryptoProParamSetB;
extern gost_subst_block Gost28147_CryptoProParamSetC;
extern gost_subst_block Gost28147_CryptoProParamSetD;
extern const byte CryptoProKeyMeshingKey[]; 
#if __LONG_MAX__ > 2147483647L 
typedef unsigned int word32; 
#else 
typedef unsigned long word32; 
#endif 

#endif

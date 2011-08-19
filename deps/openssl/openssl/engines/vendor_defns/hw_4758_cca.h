/**********************************************************************/
/*                                                                    */
/*  Prototypes of the CCA verbs used by the 4758 CCA openssl driver   */
/*                                                                    */
/*  Maurice Gittens <maurice@gittens.nl>                              */
/*                                                                    */
/**********************************************************************/

#ifndef __HW_4758_CCA__
#define __HW_4758_CCA__

/*
 *  Only WIN32 support for now
 */
#if defined(WIN32)

  #define CCA_LIB_NAME "CSUNSAPI"

  #define CSNDPKX   "CSNDPKX_32"
  #define CSNDKRR   "CSNDKRR_32"
  #define CSNDPKE   "CSNDPKE_32"
  #define CSNDPKD   "CSNDPKD_32"
  #define CSNDDSV   "CSNDDSV_32"
  #define CSNDDSG   "CSNDDSG_32"
  #define CSNBRNG   "CSNBRNG_32"

  #define SECURITYAPI __stdcall
#else
    /* Fixme!!         
      Find out the values of these constants for other platforms.
    */
  #define CCA_LIB_NAME "CSUNSAPI"

  #define CSNDPKX   "CSNDPKX"
  #define CSNDKRR   "CSNDKRR"
  #define CSNDPKE   "CSNDPKE"
  #define CSNDPKD   "CSNDPKD"
  #define CSNDDSV   "CSNDDSV"
  #define CSNDDSG   "CSNDDSG"
  #define CSNBRNG   "CSNBRNG"

  #define SECURITYAPI
#endif

/*
 * security API prototypes
 */

/* PKA Key Record Read */
typedef void (SECURITYAPI *F_KEYRECORDREAD)
             (long          * return_code,
              long          * reason_code,
              long          * exit_data_length,
              unsigned char * exit_data,
              long          * rule_array_count,
              unsigned char * rule_array,
              unsigned char * key_label,
              long          * key_token_length,
              unsigned char * key_token);

/* Random Number Generate */
typedef void (SECURITYAPI *F_RANDOMNUMBERGENERATE)
             (long          * return_code,
              long          * reason_code,
              long          * exit_data_length,
              unsigned char * exit_data,
              unsigned char * form,
              unsigned char * random_number);

/* Digital Signature Generate */
typedef void (SECURITYAPI *F_DIGITALSIGNATUREGENERATE)
             (long          * return_code,
              long          * reason_code,
              long          * exit_data_length,
              unsigned char * exit_data,
              long          * rule_array_count,
              unsigned char * rule_array,
              long          * PKA_private_key_id_length,
              unsigned char * PKA_private_key_id,
              long          * hash_length,
              unsigned char * hash,
              long          * signature_field_length,
              long          * signature_bit_length,
              unsigned char * signature_field);

/* Digital Signature Verify */
typedef void (SECURITYAPI *F_DIGITALSIGNATUREVERIFY)(
              long          * return_code,
              long          * reason_code,
              long          * exit_data_length,
              unsigned char * exit_data,
              long          * rule_array_count,
              unsigned char * rule_array,
              long          * PKA_public_key_id_length,
              unsigned char * PKA_public_key_id,
              long          * hash_length,
              unsigned char * hash,
              long          * signature_field_length,
              unsigned char * signature_field);

/* PKA Public Key Extract */
typedef void (SECURITYAPI *F_PUBLICKEYEXTRACT)(
              long          * return_code,
              long          * reason_code,
              long          * exit_data_length,
              unsigned char * exit_data,
              long          * rule_array_count,
              unsigned char * rule_array,
              long          * source_key_identifier_length,
              unsigned char * source_key_identifier,
              long          * target_key_token_length,
              unsigned char * target_key_token);

/* PKA Encrypt */
typedef void   (SECURITYAPI *F_PKAENCRYPT)
               (long          *  return_code,
                 long          *  reason_code,
                 long          *  exit_data_length,
                 unsigned char *  exit_data,
                 long          *  rule_array_count,
                 unsigned char *  rule_array,
                 long          *  key_value_length,
                 unsigned char *  key_value,
                 long          *  data_struct_length,
                 unsigned char *  data_struct,
                 long          *  RSA_public_key_length,
                 unsigned char *  RSA_public_key,
                 long          *  RSA_encipher_length,
                 unsigned char *  RSA_encipher );

/* PKA Decrypt */
typedef void    (SECURITYAPI *F_PKADECRYPT)
                (long          *  return_code,
                 long          *  reason_code,
                 long          *  exit_data_length,
                 unsigned char *  exit_data,
                 long          *  rule_array_count,
                 unsigned char *  rule_array,
                 long          *  enciphered_key_length,
                 unsigned char *  enciphered_key,
                 long          *  data_struct_length,
                 unsigned char *  data_struct,
                 long          *  RSA_private_key_length,
                 unsigned char *  RSA_private_key,
                 long          *  key_value_length,
                 unsigned char *  key_value    );


#endif

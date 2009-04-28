#ifndef oi_error_h
#define oi_error_h
#ifdef __cplusplus
extern "C" {
#endif 

enum oi_error_domain 
  { OI_ERROR_GNUTLS
  , OI_ERROR_EV
  , OI_ERROR_CLOSE
  , OI_ERROR_SHUTDOWN
  , OI_ERROR_OPEN
  , OI_ERROR_SEND
  , OI_ERROR_RECV
  , OI_ERROR_WRITE
  , OI_ERROR_READ
  , OI_ERROR_SENDFILE
  };

struct oi_error {
  enum oi_error_domain domain;
  int code; /* errno */
};

#ifdef __cplusplus
}
#endif 
#endif // oi_error_h

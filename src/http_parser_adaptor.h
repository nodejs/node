#ifndef SRC_HTTP_PARSER_ADAPTOR_H_
#define SRC_HTTP_PARSER_ADAPTOR_H_

#ifdef NODE_EXPERIMENTAL_HTTP
# include "llhttp.h"

typedef llhttp_type_t parser_type_t;
typedef llhttp_errno_t parser_errno_t;
typedef llhttp_settings_t parser_settings_t;
typedef llhttp_t parser_t;

#else  /* !NODE_EXPERIMENTAL_HTTP */
# include "http_parser.h"

typedef enum http_parser_type parser_type_t;
typedef enum http_errno parser_errno_t;
typedef http_parser_settings parser_settings_t;
typedef http_parser parser_t;

#define HPE_USER HPE_UNKNOWN

#endif  /* NODE_EXPERIMENTAL_HTTP */

#endif  /* SRC_HTTP_PARSER_ADAPTOR_H_ */

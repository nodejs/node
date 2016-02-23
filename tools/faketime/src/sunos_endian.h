
#ifndef SUN_OS_ENDIAN_H
#define SUN_OS_ENDIAN_H

#include <sys/byteorder.h>

#define htobe64(x) BE_64(x)
#define be64toh(x) BE_64(x)
#define htole64(x) LE_64(x)
#define le64toh(x) LE_64(x)

#endif /* SUN_OS_ENDIAN_H */
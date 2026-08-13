#ifndef FREEMODBUS_PORT_H
#define FREEMODBUS_PORT_H

#include <assert.h>
#include <stdint.h>

#define INLINE                    inline
#define PR_BEGIN_EXTERN_C         extern "C" {
#define PR_END_EXTERN_C           }

#define ENTER_CRITICAL_SECTION()
#define EXIT_CRITICAL_SECTION()

typedef unsigned char BOOL;
typedef unsigned char UCHAR;
typedef char CHAR;
typedef unsigned short USHORT;
typedef short SHORT;
typedef unsigned long ULONG;
typedef long LONG;

#ifndef TRUE
#define TRUE  1
#endif

#ifndef FALSE
#define FALSE 0
#endif

#endif /* FREEMODBUS_PORT_H */

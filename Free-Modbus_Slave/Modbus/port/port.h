/*
 * FreeModbus Libary: BARE Port
 * Copyright (C) 2006 Christian Walter <wolti@sil.at>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 * File: $Id$
 */

#ifndef _PORT_H
#define _PORT_H

#include <assert.h>
#include <inttypes.h>

#define	INLINE                      inline
#define PR_BEGIN_EXTERN_C           extern "C" {
#define	PR_END_EXTERN_C             }

#define ENTER_CRITICAL_SECTION( )   
#define EXIT_CRITICAL_SECTION( )    

typedef uint8_t BOOL;

typedef unsigned char UCHAR;
typedef char CHAR;

typedef uint16_t USHORT;
typedef int16_t SHORT;

typedef uint32_t ULONG;
typedef int32_t LONG;

#ifndef TRUE
#define TRUE            1
#endif

#ifndef FALSE
#define FALSE           0
#endif


#define REG_INPUT_SIZE 6
#define REG_HOLD_SIZE 16
#define REG_COILS_SIZE 10
#define REG_DISC_SIZE 10

typedef enum
{
	REG_INPUT = 0,
	REG_HOLD = 1,
	REG_COILS = 2,
	REG_DISC = 3,
}REG_TYPE;


//!鍥炶皟鍑芥暟鐨勫嚱鏁版寚閽?
typedef void (*FuncPtr)(uint16_t* , uint16_t* , uint16_t , uint8_t);

void Set_Register(REG_TYPE reg_type , uint16_t address , const uint16_t* buf , uint16_t len);

#endif

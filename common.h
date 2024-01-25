/*
	MS-DOS Player for Win32 console

	Author : Takeda.Toshiya
	Date   : 2009.11.09-
*/

#ifndef _COMMON_H_
#define _COMMON_H_

#ifndef _WIN32_WINNT
//#define _WIN32_WINNT 0x400	// Windows NT 4.0
#define _WIN32_WINNT 0x500	// Windows 2000
//#define _WIN32_WINNT 0x501	// Windows XP
#endif
#include <windows.h>
#include <winioctl.h>
#ifdef _MBCS
#include <mbstring.h>
#endif
#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <conio.h>
#include <locale.h>
#include <math.h>
#include <dos.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <io.h>
#include <sys/locking.h>
#include <mbctype.h>
#include <direct.h>
#include <errno.h>
#include <tlhelp32.h>
#include <psapi.h>
#include <setupapi.h>
#include <winsock.h>
#include <intrin.h>

#ifdef _DEBUG
// _malloca is defined in both intrin.h and crtdbg.h
#ifdef _malloca
#undef _malloca
#endif
#define _CRTDBG_MAP_ALLOC
#include <crtdbg.h>
#define calloc(c, s) _calloc_dbg(c, s, _NORMAL_BLOCK, __FILE__, __LINE__)
#define malloc(s) _malloc_dbg(s, _NORMAL_BLOCK, __FILE__, __LINE__)
#define new new(_NORMAL_BLOCK, __FILE__, __LINE__)
#endif

// variable scope of 'for' loop for Microsoft Visual C++ 6.0
#if defined(_MSC_VER) && (_MSC_VER == 1200)
#define for if(0);else for
#endif

// disable warnings for Microsoft Visual C++ 2005 or later
#if defined(_MSC_VER) && (_MSC_VER >= 1400)
#pragma warning( disable : 4018 )
#pragma warning( disable : 4244 )
#pragma warning( disable : 4309 )
#pragma warning( disable : 4819 )
#pragma warning( disable : 4995 )
#pragma warning( disable : 4996 )
#endif

// endian
#if !defined(__LITTLE_ENDIAN__) && !defined(__BIG_ENDIAN__)
	#if defined(__BYTE_ORDER) && (defined(__LITTLE_ENDIAN) || defined(__BIG_ENDIAN))
		#if __BYTE_ORDER == __LITTLE_ENDIAN
			#define __LITTLE_ENDIAN__
		#elif __BYTE_ORDER == __BIG_ENDIAN
			#define __BIG_ENDIAN__
		#endif
	#elif defined(WORDS_LITTLEENDIAN)
		#define __LITTLE_ENDIAN__
	#elif defined(WORDS_BIGENDIAN)
		#define __BIG_ENDIAN__
	#endif
#endif
#if !defined(__LITTLE_ENDIAN__) && !defined(__BIG_ENDIAN__)
	// Microsoft Visual C++
	#define __LITTLE_ENDIAN__
#endif

// compat for mingw32 headers
#ifndef COMMON_LVB_UNDERSCORE
#define COMMON_LVB_UNDERSCORE 0x8000
#endif

// type definition
#ifndef UINT8
typedef unsigned char UINT8;
#endif
#ifndef UINT16
typedef unsigned short UINT16;
#endif
#ifndef UINT32
typedef unsigned int UINT32;
#endif
#ifndef UINT64
typedef unsigned long long UINT64;
#endif
#ifndef SINT8
typedef signed char SINT8;
#endif
#ifndef SINT16
typedef signed short SINT16;
#endif
#ifndef SINT32
typedef signed int SINT32;
#endif
#ifndef SINT64
typedef signed long long SINT64;
#endif
#ifndef INT8
typedef signed char INT8;
#endif
#ifndef INT16
typedef signed short INT16;
#endif
#ifndef INT32
typedef signed int INT32;
#endif
#ifndef INT64
typedef signed long long INT64;
#endif

/* ----------------------------------------------------------------------------
	interfaces referred in cpu core
---------------------------------------------------------------------------- */

// interface

#define IRET_SIZE	0x100

void msdos_syscall(unsigned num);

UINT8 read_byte(UINT32 byteaddress);
UINT16 read_word(UINT32 byteaddress);
UINT32 read_dword(UINT32 byteaddress);

void write_byte(UINT32 byteaddress, UINT8 data);
void write_word(UINT32 byteaddress, UINT16 data);
void write_dword(UINT32 byteaddress, UINT32 data);

UINT8 read_io_byte(UINT32 byteaddress);
UINT16 read_io_word(UINT32 byteaddress);
UINT32 read_io_dword(UINT32 byteaddress);

void write_io_byte(UINT32 byteaddress, UINT8 data);
void write_io_word(UINT32 byteaddress, UINT16 data);
void write_io_dword(UINT32 byteaddress, UINT32 data);

void kbd_reset();

// debugger

// remove comment-out if you want to enable the internal debugger
//#define USE_DEBUGGER

#ifdef USE_DEBUGGER
#define MAX_BREAK_POINTS	8
#define MAX_CPU_TRACE		1024

typedef struct {
	struct {
		UINT32 addr;
		UINT32 seg;
		UINT32 ofs;
		int status;	// 0 = none, 1 = enabled, other = disabled
	} table[MAX_BREAK_POINTS];
	int hit;
} break_point_t;

typedef struct {
	struct {
		int int_num;
		UINT8 ah, ah_registered;
		UINT8 al, al_registered;
		int status;	// 0 = none, 1 = enabled, other = disabled
	} table[MAX_BREAK_POINTS];
	int hit;
} int_break_point_t;

typedef struct {
	UINT32 pc;
	UINT16 cs;
	UINT32 eip;
} cpu_trace_t;

void add_cpu_trace(UINT32 pc, UINT16 cs, UINT32 eip);
#endif

#endif

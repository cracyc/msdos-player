/*
	MS-DOS Player for Win32 console

	Author : Takeda.Toshiya
	Date   : 2009.11.09-
*/

#ifndef _COMMON_H_
#define _COMMON_H_

// for Microsoft Visual C++ 6.0
#if defined(_MSC_VER) && (_MSC_VER == 1200)
#define _MSC_VC6
#ifdef _WIN32_WINNT
#undef _WIN32_WINNT
#endif
#define _WIN32_WINNT 0x400	// Windows NT 4.0
#endif

#include <windows.h>
#include <winioctl.h>
#include <mbstring.h>
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
#include <setupapi.h>
#include <winsock.h>
#ifndef _MSC_VC6
#include <intrin.h>
#include <psapi.h>
#endif

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

// disable warnings for Microsoft Visual C++ 6.0 or later
#if defined(_MSC_VER) && (_MSC_VER >= 1200)
#pragma warning( disable : 4018 )
#pragma warning( disable : 4146 )
#pragma warning( disable : 4244 )
#pragma warning( disable : 4309 )
#pragma warning( disable : 4065 )
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

#ifndef ABOVE_NORMAL_PRIORITY_CLASS
#define ABOVE_NORMAL_PRIORITY_CLASS     0x00008000
#endif

#ifndef COMMON_LVB_UNDERSCORE
#define COMMON_LVB_UNDERSCORE 0x8000
#endif

#ifndef ENABLE_INSERT_MODE
#define ENABLE_INSERT_MODE              0x0020
#endif
#ifndef ENABLE_QUICK_EDIT_MODE
#define ENABLE_QUICK_EDIT_MODE          0x0040
#endif
#ifndef ENABLE_EXTENDED_FLAGS
#define ENABLE_EXTENDED_FLAGS           0x0080
#endif

#ifndef IMC_GETCONVERSIONMODE
#define IMC_GETCONVERSIONMODE           0x0001
#endif
#ifndef IMC_SETCONVERSIONMODE
#define IMC_SETCONVERSIONMODE           0x0002
#endif
#ifndef IMC_GETOPENSTATUS
#define IMC_GETOPENSTATUS               0x0005
#endif
#ifndef IMC_SETOPENSTATUS
#define IMC_SETOPENSTATUS               0x0006
#endif

#ifndef IME_CMODE_ALPHANUMERIC
#define IME_CMODE_ALPHANUMERIC          0x0000
#endif
#ifndef IME_CMODE_NATIVE
#define IME_CMODE_NATIVE                0x0001
#endif
#ifndef IME_CMODE_KATAKANA
#define IME_CMODE_KATAKANA              0x0002
#endif
#ifndef IME_CMODE_FULLSHAPE
#define IME_CMODE_FULLSHAPE             0x0008
#endif
#ifndef IME_CMODE_ROMAN
#define IME_CMODE_ROMAN                 0x0010
#endif
#ifndef IME_CMODE_CHARCODE
#define IME_CMODE_CHARCODE              0x0020
#endif
#ifndef IME_CMODE_SYMBOL
#define IME_CMODE_SYMBOL                0x0400
#endif

#ifndef VER_MINORVERSION
#define VER_MINORVERSION                0x0000001
#endif
#ifndef VER_MAJORVERSION
#define VER_MAJORVERSION                0x0000002
#endif
#ifndef VER_SERVICEPACKMINOR
#define VER_SERVICEPACKMINOR            0x0000010
#endif
#ifndef VER_SERVICEPACKMAJOR
#define VER_SERVICEPACKMAJOR            0x0000020
#endif
#ifndef VER_GREATER_EQUAL
#define VER_GREATER_EQUAL               3
#endif

#ifndef WC_NO_BEST_FIT_CHARS
#define WC_NO_BEST_FIT_CHARS            0x0400  // do not use best fit chars
#endif

#ifndef IMAGE_FILE_MACHINE_AMD64
#define IMAGE_FILE_MACHINE_AMD64        0x8664  // AMD64 (K8)
#endif

#ifndef INVALID_FILE_SIZE
#define INVALID_FILE_SIZE               ((DWORD)0xFFFFFFFF)
#endif
#ifndef INVALID_SET_FILE_POINTER
#define INVALID_SET_FILE_POINTER        ((DWORD)-1)
#endif
#ifndef INVALID_FILE_ATTRIBUTES
#define INVALID_FILE_ATTRIBUTES         ((DWORD)-1)
#endif

#ifndef LANG_AMHARIC
#define LANG_AMHARIC                     0x5e
#endif
#ifndef LANG_BOSNIAN
#define LANG_BOSNIAN                     0x1a   // Use with SUBLANG_BOSNIAN_* Sublanguage IDs
#endif
#ifndef LANG_DIVEHI
#define LANG_DIVEHI                      0x65
#endif
#ifndef LANG_GREENLANDIC
#define LANG_GREENLANDIC                 0x6f
#endif
#ifndef LANG_HAUSA
#define LANG_HAUSA                       0x68
#endif
#ifndef LANG_IRISH
#define LANG_IRISH                       0x3c   // Use with the SUBLANG_IRISH_IRELAND Sublanguage ID
#endif
#ifndef LANG_KHMER
#define LANG_KHMER                       0x53
#endif
#ifndef LANG_LUXEMBOURGISH
#define LANG_LUXEMBOURGISH               0x6e
#endif
#ifndef LANG_MALTESE
#define LANG_MALTESE                     0x3a
#endif
#ifndef LANG_MONGOLIAN
#define LANG_MONGOLIAN                   0x50
#endif
#ifndef LANG_PASHTO
#define LANG_PASHTO                      0x63
#endif
#ifndef LANG_PERSIAN
#define LANG_PERSIAN                     0x29
#endif
#ifndef LANG_SINHALESE
#define LANG_SINHALESE                   0x5b
#endif
#ifndef LANG_TSWANA
#define LANG_TSWANA                      0x32
#endif
#ifndef LANG_WOLOF
#define LANG_WOLOF                       0x88
#endif
#ifndef LANG_ZULU
#define LANG_ZULU                        0x35
#endif

#ifndef SUBLANG_ALBANIAN_ALBANIA
#define SUBLANG_ALBANIAN_ALBANIA                    0x01    // Albanian (Albania) 0x041c sq-AL
#endif
#ifndef SUBLANG_AMHARIC_ETHIOPIA
#define SUBLANG_AMHARIC_ETHIOPIA                    0x01    // Amharic (Ethiopia) 0x045e
#endif
#ifndef SUBLANG_BELARUSIAN_BELARUS
#define SUBLANG_BELARUSIAN_BELARUS                  0x01    // Belarusian (Belarus) 0x0423 be-BY
#endif
#ifndef SUBLANG_BENGALI_BANGLADESH
#define SUBLANG_BENGALI_BANGLADESH                  0x02    // Bengali (Bangladesh)
#endif
#ifndef SUBLANG_BOSNIAN_BOSNIA_HERZEGOVINA_LATIN
#define SUBLANG_BOSNIAN_BOSNIA_HERZEGOVINA_LATIN    0x05    // Bosnian (Bosnia and Herzegovina - Latin) 0x141a bs-BA-Latn
#endif
#ifndef SUBLANG_BOSNIAN_BOSNIA_HERZEGOVINA_CYRILLIC
#define SUBLANG_BOSNIAN_BOSNIA_HERZEGOVINA_CYRILLIC 0x08    // Bosnian (Bosnia and Herzegovina - Cyrillic) 0x201a bs-BA-Cyrl
#endif
#ifndef SUBLANG_BULGARIAN_BULGARIA
#define SUBLANG_BULGARIAN_BULGARIA                  0x01    // Bulgarian (Bulgaria) 0x0402
#endif
#ifndef SUBLANG_CZECH_CZECH_REPUBLIC
#define SUBLANG_CZECH_CZECH_REPUBLIC                0x01    // Czech (Czech Republic) 0x0405
#endif
#ifndef SUBLANG_DANISH_DENMARK
#define SUBLANG_DANISH_DENMARK                      0x01    // Danish (Denmark) 0x0406
#endif
#ifndef SUBLANG_DIVEHI_MALDIVES
#define SUBLANG_DIVEHI_MALDIVES                     0x01    // Divehi (Maldives) 0x0465 div-MV
#endif
#ifndef SUBLANG_ESTONIAN_ESTONIA
#define SUBLANG_ESTONIAN_ESTONIA                    0x01    // Estonian (Estonia) 0x0425 et-EE
#endif
#ifndef SUBLANG_FAEROESE_FAROE_ISLANDS
#define SUBLANG_FAEROESE_FAROE_ISLANDS              0x01    // Faroese (Faroe Islands) 0x0438 fo-FO
#endif
#ifndef SUBLANG_FINNISH_FINLAND
#define SUBLANG_FINNISH_FINLAND                     0x01    // Finnish (Finland) 0x040b
#endif
#ifndef SUBLANG_GREEK_GREECE
#define SUBLANG_GREEK_GREECE                        0x01    // Greek (Greece)
#endif
#ifndef SUBLANG_GREENLANDIC_GREENLAND
#define SUBLANG_GREENLANDIC_GREENLAND               0x01    // Greenlandic (Greenland) 0x046f kl-GL
#endif
#ifndef SUBLANG_HAUSA_NIGERIA_LATIN
#define SUBLANG_HAUSA_NIGERIA_LATIN                 0x01    // Hausa (Latin, Nigeria) 0x0468 ha-NG-Latn
#endif
#ifndef SUBLANG_HEBREW_ISRAEL
#define SUBLANG_HEBREW_ISRAEL                       0x01    // Hebrew (Israel) 0x040d
#endif
#ifndef SUBLANG_HINDI_INDIA
#define SUBLANG_HINDI_INDIA                         0x01    // Hindi (India) 0x0439 hi-IN
#endif
#ifndef SUBLANG_HUNGARIAN_HUNGARY
#define SUBLANG_HUNGARIAN_HUNGARY                   0x01    // Hungarian (Hungary) 0x040e
#endif
#ifndef SUBLANG_ICELANDIC_ICELAND
#define SUBLANG_ICELANDIC_ICELAND                   0x01    // Icelandic (Iceland) 0x040f
#endif
#ifndef SUBLANG_INDONESIAN_INDONESIA
#define SUBLANG_INDONESIAN_INDONESIA                0x01    // Indonesian (Indonesia) 0x0421 id-ID
#endif
#ifndef SUBLANG_IRISH_IRELAND
#define SUBLANG_IRISH_IRELAND                       0x02    // Irish (Ireland)
#endif
#ifndef SUBLANG_JAPANESE_JAPAN
#define SUBLANG_JAPANESE_JAPAN                      0x01    // Japanese (Japan) 0x0411
#endif
#ifndef SUBLANG_KHMER_CAMBODIA
#define SUBLANG_KHMER_CAMBODIA                      0x01    // Khmer (Cambodia) 0x0453 kh-KH
#endif
#ifndef SUBLANG_LATVIAN_LATVIA
#define SUBLANG_LATVIAN_LATVIA                      0x01    // Latvian (Latvia) 0x0426 lv-LV
#endif
#ifndef SUBLANG_LUXEMBOURGISH_LUXEMBOURG
#define SUBLANG_LUXEMBOURGISH_LUXEMBOURG            0x01    // Luxembourgish (Luxembourg) 0x046e lb-LU
#endif
#ifndef SUBLANG_MACEDONIAN_MACEDONIA
#define SUBLANG_MACEDONIAN_MACEDONIA                0x01    // Macedonian (Macedonia (FYROM)) 0x042f mk-MK
#endif
#ifndef SUBLANG_MALTESE_MALTA
#define SUBLANG_MALTESE_MALTA                       0x01    // Maltese (Malta) 0x043a mt-MT
#endif
#ifndef SUBLANG_MONGOLIAN_CYRILLIC_MONGOLIA
#define SUBLANG_MONGOLIAN_CYRILLIC_MONGOLIA         0x01    // Mongolian (Cyrillic, Mongolia)
#endif
#ifndef SUBLANG_NEPALI_NEPAL
#define SUBLANG_NEPALI_NEPAL                        0x01    // Nepali (Nepal) 0x0461 ne-NP
#endif
#ifndef SUBLANG_PASHTO_AFGHANISTAN
#define SUBLANG_PASHTO_AFGHANISTAN                  0x01    // Pashto (Afghanistan)
#endif
#ifndef SUBLANG_PERSIAN_IRAN
#define SUBLANG_PERSIAN_IRAN                        0x01    // Persian (Iran) 0x0429 fa-IR
#endif
#ifndef SUBLANG_POLISH_POLAND
#define SUBLANG_POLISH_POLAND                       0x01    // Polish (Poland) 0x0415
#endif
#ifndef SUBLANG_ROMANIAN_ROMANIA
#define SUBLANG_ROMANIAN_ROMANIA                    0x01    // Romanian (Romania) 0x0418
#endif
#ifndef SUBLANG_RUSSIAN_RUSSIA
#define SUBLANG_RUSSIAN_RUSSIA                      0x01    // Russian (Russia) 0x0419
#endif
#ifndef SUBLANG_SERBIAN_CROATIA
#define SUBLANG_SERBIAN_CROATIA                     0x01    // Croatian (Croatia) 0x041a hr-HR
#endif
#ifndef SUBLANG_SINHALESE_SRI_LANKA
#define SUBLANG_SINHALESE_SRI_LANKA                 0x01    // Sinhalese (Sri Lanka)
#endif
#ifndef SUBLANG_SLOVAK_SLOVAKIA
#define SUBLANG_SLOVAK_SLOVAKIA                     0x01    // Slovak (Slovakia) 0x041b sk-SK
#endif
#ifndef SUBLANG_SLOVENIAN_SLOVENIA
#define SUBLANG_SLOVENIAN_SLOVENIA                  0x01    // Slovenian (Slovenia) 0x0424 sl-SI
#endif
#ifndef SUBLANG_SWAHILI_KENYA
#define SUBLANG_SWAHILI_KENYA                       0x01    // Swahili (Kenya) 0x0441 sw-KE
#endif
#ifndef SUBLANG_THAI_THAILAND
#define SUBLANG_THAI_THAILAND                       0x01    // Thai (Thailand) 0x041e th-TH
#endif
#ifndef SUBLANG_TSWANA_BOTSWANA
#define SUBLANG_TSWANA_BOTSWANA                     0x02    // Botswana
#endif
#ifndef SUBLANG_TURKISH_TURKEY
#define SUBLANG_TURKISH_TURKEY                      0x01    // Turkish (Turkey) 0x041f tr-TR
#endif
#ifndef SUBLANG_UKRAINIAN_UKRAINE
#define SUBLANG_UKRAINIAN_UKRAINE                   0x01    // Ukrainian (Ukraine) 0x0422 uk-UA
#endif
#ifndef SUBLANG_VIETNAMESE_VIETNAM
#define SUBLANG_VIETNAMESE_VIETNAM                  0x01    // Vietnamese (Vietnam) 0x042a vi-VN
#endif
#ifndef SUBLANG_WOLOF_SENEGAL
#define SUBLANG_WOLOF_SENEGAL                       0x01    // Wolof (Senegal)
#endif
#ifndef SUBLANG_ZULU_SOUTH_AFRICA
#define SUBLANG_ZULU_SOUTH_AFRICA                   0x01    // isiZulu / Zulu (South Africa) 0x0435 zu-ZA
#endif

// for Microsoft Visual C++ 6.0
#ifdef _MSC_VC6
typedef struct _CONSOLE_FONT_INFO {
    DWORD  nFont;
    COORD  dwFontSize;
} CONSOLE_FONT_INFO, *PCONSOLE_FONT_INFO;

typedef struct _CONSOLE_FONT_INFOEX {
    ULONG cbSize;
    DWORD nFont;
    COORD dwFontSize;
    UINT FontFamily;
    UINT FontWeight;
    WCHAR FaceName[LF_FACESIZE];
} CONSOLE_FONT_INFOEX, *PCONSOLE_FONT_INFOEX;

typedef int errno_t;
typedef __int32 intptr_t;
typedef size_t rsize_t;

typedef unsigned __int32 ULONG_PTR;
typedef unsigned __int32 DWORD_PTR;

// variable scope of 'for' loop
#define for if(0);else for
#endif

// type definition
#ifndef _MSC_VER
//typedef long long __int64;
#endif
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
typedef unsigned __int64 UINT64;
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
typedef signed __int64 SINT64;
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
typedef signed __int64 INT64;
#endif

/* ----------------------------------------------------------------------------
	interfaces referred in cpu core
---------------------------------------------------------------------------- */

// interface

#define IRET_SIZE	0x100

void msdos_syscall(unsigned num);

UINT32 read_byte(UINT32 byteaddress);
UINT32 read_word(UINT32 byteaddress);
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

void dma_run();

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
	BOOL op32; // 32bit protected mode in i386, or 8080 mode in NEC V30
} cpu_trace_t;

void add_cpu_trace(UINT32 pc, UINT16 cs, UINT32 eip, BOOL op32);
bool in_iret_table(int num);
#endif

#endif

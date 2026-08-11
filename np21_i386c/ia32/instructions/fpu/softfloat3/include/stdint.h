/* stdint.h‚ª‚È‚¢ŠÂ‹«—p */

#pragma once

#ifndef STDINT_H
#define STDINT_H

#if defined(_MSC_VER) && (_MSC_VER < 1600)
	typedef unsigned short      uint16_t;
	typedef unsigned int        uint32_t;
	typedef unsigned __int64    uint64_t;

	typedef unsigned char       uint_least8_t;
	typedef unsigned short      uint_least16_t;
	typedef unsigned int        uint_least32_t;
	typedef unsigned __int64    uint_least64_t;

	typedef unsigned int        uint_fast8_t;
	typedef unsigned int        uint_fast16_t;
	typedef unsigned int        uint_fast32_t;
	typedef unsigned __int64    uint_fast64_t;

	typedef signed char         int8_t;
	typedef signed short        int16_t;
	typedef signed int          int32_t;
	typedef signed __int64      int64_t;

	typedef signed char         int_least8_t;
	typedef signed short        int_least16_t;
	typedef signed int          int_least32_t;
	typedef signed __int64      int_least64_t;

	typedef signed int          int_fast8_t;
	typedef signed int          int_fast16_t;
	typedef signed int          int_fast32_t;
	typedef signed __int64      int_fast64_t;

	#define INT64_C(val)        val##I64
	#define UINT64_C(val)       val##UI64
#else
	#include <stdint.h>
#endif

#endif

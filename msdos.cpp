/*
	MS-DOS Player for Win32 console

	Author : Takeda.Toshiya
	Date   : 2009.11.09-
*/

#include "common.h"
#include "msdos.h"

void exit_handler();

#define fatalerror(...) { \
	fprintf(stderr, __VA_ARGS__); \
	exit_handler(); \
	exit(1); \
}
#define error(...) fprintf(stderr, "error: " __VA_ARGS__)
#define nolog(...)

//#define ENABLE_DEBUG_LOG
#ifdef ENABLE_DEBUG_LOG
	#define EXPORT_DEBUG_TO_FILE
	#define ENABLE_DEBUG_SYSCALL
	#define ENABLE_DEBUG_UNIMPLEMENTED
	#define ENABLE_DEBUG_IOPORT
	#define ENABLE_DEBUG_OPEN_FILE
	
	#ifdef EXPORT_DEBUG_TO_FILE
		FILE* fp_debug_log = NULL;
	#else
		#define fp_debug_log stderr
	#endif
	#ifdef ENABLE_DEBUG_UNIMPLEMENTED
		#define unimplemented_10h fatalerror
		#define unimplemented_13h fatalerror
		#define unimplemented_14h fatalerror
		#define unimplemented_15h fatalerror
		#define unimplemented_16h fatalerror
		#define unimplemented_17h fatalerror
		#define unimplemented_1ah fatalerror
		#define unimplemented_21h fatalerror
		#define unimplemented_2fh fatalerror
		#define unimplemented_33h fatalerror
		#define unimplemented_67h fatalerror
		#define unimplemented_xms fatalerror
	#endif
#endif
#ifndef unimplemented_10h
	#define unimplemented_10h nolog
#endif
#ifndef unimplemented_13h
	#define unimplemented_13h nolog
#endif
#ifndef unimplemented_14h
	#define unimplemented_14h nolog
#endif
#ifndef unimplemented_15h
	#define unimplemented_15h nolog
#endif
#ifndef unimplemented_16h
	#define unimplemented_16h nolog
#endif
#ifndef unimplemented_17h
	#define unimplemented_17h nolog
#endif
#ifndef unimplemented_1ah
	#define unimplemented_1ah nolog
#endif
#ifndef unimplemented_21h
	#define unimplemented_21h nolog
#endif
#ifndef unimplemented_2fh
	#define unimplemented_2fh nolog
#endif
#ifndef unimplemented_33h
	#define unimplemented_33h nolog
#endif
#ifndef unimplemented_67h
	#define unimplemented_67h nolog
#endif
#ifndef unimplemented_xms
	#define unimplemented_xms nolog
#endif

#if 0
char *my_getenv(const char *varname)
{
	char tmp[ENV_SIZE];

	if(GetEnvironmentVariableA(varname, tmp, ENV_SIZE) != 0) {
		static char result[8][ENV_SIZE];
		static unsigned int table_index = 0;
		unsigned int output_index = (table_index++) & 7;

//		strcpy(result[output_index], tmp);
		strcpy_s(result[output_index], ENV_SIZE, tmp);
		return result[output_index];
	}
	return NULL;
}
#else
#define my_getenv(varname) getenv(varname)
#endif

#ifdef _MBCS
inline char *my_strchr(char *str, int chr)
{
	return (char *)_mbschr((unsigned char *)(str), (unsigned int)(chr));
}
inline const char *my_strchr(const char *str, int chr)
{
	return (const char *)_mbschr((const unsigned char *)(str), (unsigned int)(chr));
}
inline char *my_strrchr(char *str, int chr)
{
	return (char *)_mbsrchr((unsigned char *)(str), (unsigned int)(chr));
}
inline const char *my_strrchr(const char *str, int chr)
{
	return (const char *)_mbsrchr((const unsigned char *)(str), (unsigned int)(chr));
}
inline char *my_strtok(char *tok, const char *del)
{
	return (char *)_mbstok((unsigned char *)(tok), (const unsigned char *)(del));
}
inline char *my_strupr(char *str)
{
	return (char *)_mbsupr((unsigned char *)(str));
}
#else
#define my_strchr(str, chr) strchr((str), (chr))
#define my_strrchr(str, chr) strrchr((str), (chr))
#define my_strtok(tok, del) strtok((tok), (del))
#define my_strupr(str) _strupr((str))
#endif
#define array_length(array) (sizeof(array) / sizeof(array[0]))

#if defined(__MINGW32__)
extern "C" int _CRT_glob = 0;
#endif

/*
	kludge for "more-standardized" C++
*/
#if !defined(_MSC_VER)
inline int kludge_min(int a, int b) { return (a<b ? a:b); }
inline int kludge_max(int a, int b) { return (a>b ? a:b); }
#define min(a,b) kludge_min(a,b)
#define max(a,b) kludge_max(a,b)
#elif _MSC_VER >= 1400
void ignore_invalid_parameters(const wchar_t *, const wchar_t *, const wchar_t *, unsigned int, uintptr_t)
{
}
#endif

#define USE_VRAM_THREAD

#ifdef USE_VRAM_THREAD
static CRITICAL_SECTION vram_crit_sect;
#else
#define vram_flush()
#endif

#define VIDEO_REGEN *(UINT16 *)(mem + 0x44c)
#define SCR_BUF(y,x) scr_buf[(y) * scr_buf_size.X + (x)]

void change_console_size(int width, int height);
void clear_scr_buffer(WORD attr);

static UINT32 vram_length_char = 0, vram_length_attr = 0;
static UINT32 vram_last_length_char = 0, vram_last_length_attr = 0;
static COORD vram_coord_char, vram_coord_attr;

char temp_file_path[MAX_PATH];
bool temp_file_created = false;

bool ignore_illegal_insn = false;
bool limit_max_memory = false;
bool no_windows = false;
bool stay_busy = false;
bool support_ems = false;
#ifdef SUPPORT_XMS
bool support_xms = false;
#endif
int sio_port_number[4] = {0, 0, 0, 0};

BOOL is_winxp_or_later;
BOOL is_xp_64_or_later;
BOOL is_vista_or_later;
BOOL is_win10_or_later;

#define UPDATE_OPS 16384
#define REQUEST_HARDWRE_UPDATE() { \
	update_ops = UPDATE_OPS - 1; \
}
UINT32 update_ops = 0;
UINT32 idle_ops = 0;

inline BOOL is_sse2_ready()
{
	static int result = -1;
	int cpu_info[4];
	
	if(result == -1) {
		result = 0;
		__cpuid(cpu_info, 0);
		if(cpu_info[0] >= 1){
			__cpuid(cpu_info, 1);
			if(cpu_info[3] & (1 << 26)) {
				result = 1;
			}
		}
	}
	return(result == 1);
}

inline void maybe_idle()
{
	// if it appears to be in a tight loop, assume waiting for input
	// allow for one updated video character, for a spinning cursor
	if(!stay_busy && idle_ops < 1024 && vram_length_char <= 1 && vram_length_attr <= 1) {
		if(is_sse2_ready()) {
			_mm_pause();	// SSE2 pause
		} else if(is_xp_64_or_later) {
			Sleep(0);	// switch to other thread that is ready to run, without checking priority
		} else {
			Sleep(1);
			REQUEST_HARDWRE_UPDATE();
		}
	}
	idle_ops = 0;
}

/* ----------------------------------------------------------------------------
	cpu
---------------------------------------------------------------------------- */

// flag to exit MS-DOS Player
// this is set when the first process is terminated and jump to FFFF:0000 HALT
int msdos_exit = 0;

#ifdef USE_DEBUGGER
int msdos_int_num = -1;

cpu_trace_t cpu_trace[MAX_CPU_TRACE] = {0};
int cpu_trace_ptr = 0;
UINT32 prev_trace_pc = -1;

void add_cpu_trace(UINT32 pc, UINT16 cs, UINT32 eip)
{
	if(prev_trace_pc != pc) {
		cpu_trace[cpu_trace_ptr].pc = prev_trace_pc = pc;
		cpu_trace[cpu_trace_ptr].cs = cs;
		cpu_trace[cpu_trace_ptr].eip = eip;
		cpu_trace_ptr = (cpu_trace_ptr + 1) & (MAX_CPU_TRACE - 1);
	}
}
#endif

#if defined(HAS_IA32)
	UINT32 msdos_int6h_eip;
	int cpu_type, cpu_step;
	#include "np21_i386.cpp"
#elif defined(HAS_I386)
	UINT32 msdos_int6h_eip;
	int cpu_type, cpu_step;
	#include "mame_i386.cpp"
#else
	UINT32 msdos_int6h_pc;
	#include "mame_i286.cpp"
#endif

/* ----------------------------------------------------------------------------
	memory accessors
---------------------------------------------------------------------------- */

// read accessors
UINT8 read_byte(UINT32 byteaddress)
#ifdef USE_DEBUGGER
{
	if(now_debugging) {
		for(int i = 0; i < MAX_BREAK_POINTS; i++) {
			if(rd_break_point.table[i].status == 1) {
				if(byteaddress == rd_break_point.table[i].addr) {
					rd_break_point.hit = i + 1;
					now_suspended = true;
					break;
				}
			}
		}
	}
	return(debugger_read_byte(byteaddress));
}
UINT8 debugger_read_byte(UINT32 byteaddress)
#endif
{
#ifdef SUPPORT_GRAPHIC_SCREEN
	if((byteaddress >= VGA_VRAM_TOP) && (byteaddress <= VGA_VRAM_LAST) && (mem[0x449] > 3)) {
		return vga_read(byteaddress - VGA_VRAM_TOP, 1);
	}
#endif
#if defined(HAS_I386)
	if(byteaddress < MAX_MEM) {
		return mem[byteaddress];
//	} else if((byteaddress & 0xfffffff0) == 0xfffffff0) {
//		return read_byte(byteaddress & 0xfffff);
	}
	return 0;
#else
	return mem[byteaddress];
#endif
}

UINT16 read_word(UINT32 byteaddress)
#ifdef USE_DEBUGGER
{
	if(now_debugging) {
		for(int i = 0; i < MAX_BREAK_POINTS; i++) {
			if(rd_break_point.table[i].status == 1) {
				if(byteaddress >= rd_break_point.table[i].addr && byteaddress < rd_break_point.table[i].addr + 2) {
					rd_break_point.hit = i + 1;
					now_suspended = true;
					break;
				}
			}
		}
	}
	return(debugger_read_word(byteaddress));
}
UINT16 debugger_read_word(UINT32 byteaddress)
#endif
{
	if(byteaddress == 0x41c) {
		// pointer to first free slot in keyboard buffer
		if(key_buf_char != NULL && key_buf_scan != NULL) {
#ifdef USE_SERVICE_THREAD
			EnterCriticalSection(&key_buf_crit_sect);
#endif
			bool empty = pcbios_is_key_buffer_empty();
#ifdef USE_SERVICE_THREAD
			LeaveCriticalSection(&key_buf_crit_sect);
#endif
			if(empty) maybe_idle();
		}
	}
#ifdef SUPPORT_GRAPHIC_SCREEN
	if((byteaddress >= VGA_VRAM_TOP) && (byteaddress <= VGA_VRAM_LAST) && (mem[0x449] > 3)) {
		if(byteaddress <= VGA_VRAM_LAST - 1) {
			return vga_read(byteaddress - VGA_VRAM_TOP, 2);
		} else {
			UINT16 value;
			value  = read_byte(byteaddress    );
			value |= read_byte(byteaddress + 1) << 8;
			return value;
		}
	}
#endif
#if defined(HAS_I386)
//	if(byteaddress < MAX_MEM - 1) {
	if(byteaddress < MAX_MEM) {
		return *(UINT16 *)(mem + byteaddress);
//	} else if((byteaddress & 0xfffffff0) == 0xfffffff0) {
//		return read_word(byteaddress & 0xfffff);
	}
	return 0;
#else
	return *(UINT16 *)(mem + byteaddress);
#endif
}

UINT32 read_dword(UINT32 byteaddress)
#ifdef USE_DEBUGGER
{
	if(now_debugging) {
		for(int i = 0; i < MAX_BREAK_POINTS; i++) {
			if(rd_break_point.table[i].status == 1) {
				if(byteaddress >= rd_break_point.table[i].addr && byteaddress < rd_break_point.table[i].addr + 4) {
					rd_break_point.hit = i + 1;
					now_suspended = true;
					break;
				}
			}
		}
	}
	return(debugger_read_dword(byteaddress));
}
UINT32 debugger_read_dword(UINT32 byteaddress)
#endif
{
#ifdef SUPPORT_GRAPHIC_SCREEN
	if((byteaddress >= VGA_VRAM_TOP) && (byteaddress <= VGA_VRAM_LAST) && (mem[0x449] > 3)) {
		if(byteaddress <= VGA_VRAM_LAST - 3) {
			return vga_read(byteaddress - VGA_VRAM_TOP, 4);
		} else {
			UINT16 value;
			if(byteaddress & 1) {
				value  = read_byte(byteaddress    );
				value |= read_word(byteaddress + 1) <<  8;
				value |= read_byte(byteaddress + 3) << 24;
			} else {
				value  = read_word(byteaddress    );
				value |= read_word(byteaddress + 2) << 16;
			}
			return value;
		}
	}
#endif
#if defined(HAS_I386)
//	if(byteaddress < MAX_MEM - 3) {
	if(byteaddress < MAX_MEM) {
		return *(UINT32 *)(mem + byteaddress);
//	} else if((byteaddress & 0xfffffff0) == 0xfffffff0) {
//		return read_dword(byteaddress & 0xfffff);
	}
	return 0;
#else
	return *(UINT32 *)(mem + byteaddress);
#endif
}

// write accessors

#if 0
static const CHAR box_drawings_char[] = {
	/*0x00*/ 0,
	/*0x01*/ '+',	// BOX DRAWINGS DOUBLE DOWN AND RIGHT
	/*0x02*/ '+',	// BOX DRAWINGS DOUBLE DOWN AND LEFT
	/*0x03*/ '+',	// BOX DRAWINGS DOUBLE UP AND RIGHT
	/*0x04*/ '+',	// BOX DRAWINGS DOUBLE UP AND LEFT
	/*0x05*/ '|',	// BOX DRAWINGS DOUBLE VERTICAL
	/*0x06*/ '-',	// BOX DRAWINGS DOUBLE HORIZONTAL
	/*0x07*/ 0,
	/*0x08*/ 0,
	/*0x09*/ 0,
	/*0x0A*/ 0,
	/*0x0B*/ 0,
	/*0x0C*/ 0,
	/*0x0D*/ 0,
	/*0x0E*/ 0,
	/*0x0F*/ 0,
	/*0x10*/ '+',	// BOX DRAWINGS DOUBLE VERTICAL AND HORIZONTAL
	/*0x11*/ 0,
	/*0x12*/ 0,
	/*0x13*/ 0,
	/*0x14*/ 0,
	/*0x15*/ '-',	// BOX DRAWINGS DOUBLE UP AND HORIZONTAL
	/*0x16*/ '-',	// BOX DRAWINGS DOUBLE DOWN AND HORIZONTAL
	/*0x17*/ '|',	// BOX DRAWINGS DOUBLE VERTICAL AND LEFT
	/*0x18*/ 0,
	/*0x19*/ '|',	// BOX DRAWINGS DOUBLE VERTICAL AND RIGHT
};

COORD shift_coord(COORD origin, DWORD pos)
{
	COORD co;
	
	co.X = origin.X + pos;
	co.Y = origin.Y;
	
	while(co.X >= scr_width) {
		co.X -= scr_width;
		co.Y += 1;
	}
	return co;
}

BOOL MyWriteConsoleOutputCharacterA(HANDLE hConsoleOutput, LPCSTR lpCharacter, DWORD nLength, COORD dwWriteCoord, LPDWORD lpNumberOfCharsWritten)
{
	if(is_win10_or_later && active_code_page == 932) {
		DWORD written = 0, written_tmp;
		
		for(DWORD pos = 0, start_pos = 0; pos < nLength; pos++) {
			CHAR code = lpCharacter[pos], replace;
			
			if(code >= 0x01 && code <= 0x19 && (replace = box_drawings_char[code]) != 0) {
				if(pos > start_pos) {
					WriteConsoleOutputCharacterA(hConsoleOutput, lpCharacter + start_pos, pos - start_pos, shift_coord(dwWriteCoord, start_pos), &written_tmp);
					written += written_tmp;
				}
				WriteConsoleOutputCharacterA(hConsoleOutput, &replace, 1, shift_coord(dwWriteCoord, pos), &written_tmp);
				written += written_tmp;
				start_pos = pos + 1;
			} else if(pos == nLength - 1) {
				if(pos > start_pos) {
					WriteConsoleOutputCharacterA(hConsoleOutput, lpCharacter + start_pos, pos - start_pos + 1, shift_coord(dwWriteCoord, start_pos), &written_tmp);
					written += written_tmp;
				}
			}
		}
		*lpNumberOfCharsWritten = written;
		return TRUE;
	} else {
		return WriteConsoleOutputCharacterA(hConsoleOutput, lpCharacter, nLength, dwWriteCoord, lpNumberOfCharsWritten);
	}
}
#else
#define MyWriteConsoleOutputCharacterA WriteConsoleOutputCharacterA
#endif

#ifdef USE_VRAM_THREAD
void vram_flush_char()
{
	if(vram_length_char != 0) {
		DWORD num;
		MyWriteConsoleOutputCharacterA(GetStdHandle(STD_OUTPUT_HANDLE), scr_char, vram_length_char, vram_coord_char, &num);
		vram_length_char = vram_last_length_char = 0;
	}
}

void vram_flush_attr()
{
	if(vram_length_attr != 0) {
		DWORD num;
		WriteConsoleOutputAttribute(GetStdHandle(STD_OUTPUT_HANDLE), scr_attr, vram_length_attr, vram_coord_attr, &num);
		vram_length_attr = vram_last_length_attr = 0;
	}
}

void vram_flush()
{
	if(vram_length_char != 0 || vram_length_attr != 0) {
		EnterCriticalSection(&vram_crit_sect);
		vram_flush_char();
		vram_flush_attr();
		LeaveCriticalSection(&vram_crit_sect);
	}
}
#endif

void write_text_vram_char(UINT32 offset, UINT8 data)
{
#ifdef USE_VRAM_THREAD
	static UINT32 first_offset_char, last_offset_char;
	
	if(vram_length_char != 0) {
		if(offset <= last_offset_char && offset >= first_offset_char) {
			scr_char[(offset - first_offset_char) >> 1] = data;
			return;
		}
		if(offset != last_offset_char + 2) {
			vram_flush_char();
		}
	}
	if(vram_length_char == 0) {
		first_offset_char = offset;
		vram_coord_char.X = (offset >> 1) % scr_width;
		vram_coord_char.Y = (offset >> 1) / scr_width + scr_top;
	}
	scr_char[vram_length_char++] = data;
	last_offset_char = offset;
#else
	COORD co;
	DWORD num;
	
	co.X = (offset >> 1) % scr_width;
	co.Y = (offset >> 1) / scr_width;
	scr_char[0] = data;
	MyWriteConsoleOutputCharacterA(GetStdHandle(STD_OUTPUT_HANDLE), scr_char, 1, co, &num);
#endif
}

void write_text_vram_attr(UINT32 offset, UINT8 data)
{
#ifdef USE_VRAM_THREAD
	static UINT32 first_offset_attr, last_offset_attr;
	
	if(vram_length_attr != 0) {
		if(offset <= last_offset_attr && offset >= first_offset_attr) {
			scr_attr[(offset - first_offset_attr) >> 1] = data;
			return;
		}
		if(offset != last_offset_attr + 2) {
			vram_flush_attr();
		}
	}
	if(vram_length_attr == 0) {
		first_offset_attr = offset;
		vram_coord_attr.X = (offset >> 1) % scr_width;
		vram_coord_attr.Y = (offset >> 1) / scr_width + scr_top;
	}
	scr_attr[vram_length_attr++] = data;
	last_offset_attr = offset;
#else
	COORD co;
	DWORD num;
	
	co.X = (offset >> 1) % scr_width;
	co.Y = (offset >> 1) / scr_width;
	scr_attr[0] = data;
	WriteConsoleOutputAttribute(GetStdHandle(STD_OUTPUT_HANDLE), scr_attr, 1, co, &num);
#endif
}

void write_text_vram_byte(UINT32 offset, UINT8 data)
{
#ifdef USE_VRAM_THREAD
	EnterCriticalSection(&vram_crit_sect);
#endif
	if(offset & 1) {
		write_text_vram_attr(offset, data);
	} else {
		write_text_vram_char(offset, data);
	}
#ifdef USE_VRAM_THREAD
	LeaveCriticalSection(&vram_crit_sect);
#endif
}

void write_text_vram_word(UINT32 offset, UINT16 data)
{
#ifdef USE_VRAM_THREAD
	EnterCriticalSection(&vram_crit_sect);
#endif
	if(offset & 1) {
		write_text_vram_attr(offset    , (data     ) & 0xff);
		write_text_vram_char(offset + 1, (data >> 8) & 0xff);
	} else {
		write_text_vram_char(offset    , (data     ) & 0xff);
		write_text_vram_attr(offset + 1, (data >> 8) & 0xff);
	}
#ifdef USE_VRAM_THREAD
	LeaveCriticalSection(&vram_crit_sect);
#endif
}

void write_text_vram_dword(UINT32 offset, UINT32 data)
{
#ifdef USE_VRAM_THREAD
	EnterCriticalSection(&vram_crit_sect);
#endif
	if(offset & 1) {
		write_text_vram_attr(offset    , (data      ) & 0xff);
		write_text_vram_char(offset + 1, (data >>  8) & 0xff);
		write_text_vram_attr(offset + 2, (data >> 16) & 0xff);
		write_text_vram_char(offset + 3, (data >> 24) & 0xff);
	} else {
		write_text_vram_char(offset    , (data      ) & 0xff);
		write_text_vram_attr(offset + 1, (data >>  8) & 0xff);
		write_text_vram_char(offset + 2, (data >> 16) & 0xff);
		write_text_vram_attr(offset + 3, (data >> 24) & 0xff);
	}
#ifdef USE_VRAM_THREAD
	LeaveCriticalSection(&vram_crit_sect);
#endif
}

void write_byte(UINT32 byteaddress, UINT8 data)
#ifdef USE_DEBUGGER
{
	if(now_debugging) {
		for(int i = 0; i < MAX_BREAK_POINTS; i++) {
			if(wr_break_point.table[i].status == 1) {
				if(byteaddress == wr_break_point.table[i].addr) {
					wr_break_point.hit = i + 1;
					now_suspended = true;
					break;
				}
			}
		}
	}
	debugger_write_byte(byteaddress, data);
}
void debugger_write_byte(UINT32 byteaddress, UINT8 data)
#endif
{
	if(byteaddress < MEMORY_END) {
		mem[byteaddress] = data;
#ifdef SUPPORT_GRAPHIC_SCREEN
	} else if((byteaddress >= VGA_VRAM_TOP) && (byteaddress <= VGA_VRAM_LAST) && (mem[0x449] > 3) && (mem[0x449] != 7)) {
		vga_write(byteaddress - VGA_VRAM_TOP, data, 1);
#endif
	} else if(byteaddress >= text_vram_top_address && byteaddress < text_vram_end_address) {
		if(!restore_console_size) {
			change_console_size(scr_width, scr_height);
		}
		write_text_vram_byte(byteaddress - text_vram_top_address, data);
		mem[byteaddress] = data;
	} else if(byteaddress >= shadow_buffer_top_address && byteaddress < shadow_buffer_end_address) {
		if(int_10h_feh_called && !int_10h_ffh_called) {
			write_text_vram_byte(byteaddress - shadow_buffer_top_address, data);
		}
		mem[byteaddress] = data;
#if defined(HAS_I386)
	} else if(byteaddress < MAX_MEM) {
#else
	} else {
#endif
		mem[byteaddress] = data;
	}
}

void write_word(UINT32 byteaddress, UINT16 data)
#ifdef USE_DEBUGGER
{
	if(now_debugging) {
		for(int i = 0; i < MAX_BREAK_POINTS; i++) {
			if(wr_break_point.table[i].status == 1) {
				if(byteaddress >= wr_break_point.table[i].addr && byteaddress < wr_break_point.table[i].addr + 2) {
					wr_break_point.hit = i + 1;
					now_suspended = true;
					break;
				}
			}
		}
	}
	debugger_write_word(byteaddress, data);
}
void debugger_write_word(UINT32 byteaddress, UINT16 data)
#endif
{
	if(byteaddress < MEMORY_END - 1) {
		if(byteaddress == cursor_position_address) {
			if(*(UINT16 *)(mem + byteaddress) != data) {
				COORD co;
				co.X = data & 0xff;
				co.Y = (data >> 8) + scr_top;
				SetConsoleCursorPosition(GetStdHandle(STD_OUTPUT_HANDLE), co);
				cursor_moved = false;
				cursor_moved_by_crtc = false;
			}
		}
		*(UINT16 *)(mem + byteaddress) = data;
	} else if(byteaddress < MEMORY_END) {
		write_byte(byteaddress    , (data     ) & 0xff);
		write_byte(byteaddress + 1, (data >> 8) & 0xff);
#ifdef SUPPORT_GRAPHIC_SCREEN
	} else if((byteaddress >= VGA_VRAM_TOP) && (byteaddress <= VGA_VRAM_LAST) && (mem[0x449] > 3) && (mem[0x449] != 7)) {
		if(byteaddress <= VGA_VRAM_LAST - 1) {
			vga_write(byteaddress - VGA_VRAM_TOP, data, 2);
		} else {
			write_byte(byteaddress    , (data     ) & 0xff);
			write_byte(byteaddress + 1, (data >> 8) & 0xff);
		}
#endif
	} else if(byteaddress >= text_vram_top_address && byteaddress < text_vram_end_address) {
		if(byteaddress < text_vram_end_address - 1) {
			if(!restore_console_size) {
				change_console_size(scr_width, scr_height);
			}
			write_text_vram_word(byteaddress - text_vram_top_address, data);
			*(UINT16 *)(mem + byteaddress) = data;
		} else {
			write_byte(byteaddress    , (data     ) & 0xff);
			write_byte(byteaddress + 1, (data >> 8) & 0xff);
		}
	} else if(byteaddress >= shadow_buffer_top_address && byteaddress < shadow_buffer_end_address) {
		if(byteaddress < shadow_buffer_end_address - 1) {
			if(int_10h_feh_called && !int_10h_ffh_called) {
				write_text_vram_word(byteaddress - shadow_buffer_top_address, data);
			}
			*(UINT16 *)(mem + byteaddress) = data;
		} else {
			write_byte(byteaddress    , (data     ) & 0xff);
			write_byte(byteaddress + 1, (data >> 8) & 0xff);
		}
#if defined(HAS_I386)
//	} else if(byteaddress < MAX_MEM - 1) {
	} else if(byteaddress < MAX_MEM) {
#else
	} else {
#endif
		*(UINT16 *)(mem + byteaddress) = data;
	}
}

void write_dword(UINT32 byteaddress, UINT32 data)
#ifdef USE_DEBUGGER
{
	if(now_debugging) {
		for(int i = 0; i < MAX_BREAK_POINTS; i++) {
			if(wr_break_point.table[i].status == 1) {
				if(byteaddress >= wr_break_point.table[i].addr && byteaddress < wr_break_point.table[i].addr + 4) {
					wr_break_point.hit = i + 1;
					now_suspended = true;
					break;
				}
			}
		}
	}
	debugger_write_dword(byteaddress, data);
}
void debugger_write_dword(UINT32 byteaddress, UINT32 data)
#endif
{
	if(byteaddress < MEMORY_END - 3) {
		*(UINT32 *)(mem + byteaddress) = data;
	} else if(byteaddress < MEMORY_END) {
		if(byteaddress & 1) {
			write_byte(byteaddress    , (data      ) & 0x00ff);
			write_word(byteaddress + 1, (data >>  8) & 0xffff);
			write_byte(byteaddress + 3, (data >> 24) & 0x00ff);
		} else {
			write_word(byteaddress    , (data      ) & 0xffff);
			write_word(byteaddress + 2, (data >> 16) & 0xffff);
		}
#ifdef SUPPORT_GRAPHIC_SCREEN
	} else if((byteaddress >= VGA_VRAM_TOP) && (byteaddress <= VGA_VRAM_LAST) && (mem[0x449] > 3) && (mem[0x449] != 7)) {
		if(byteaddress <= VGA_VRAM_LAST -3) {
			vga_write(byteaddress - VGA_VRAM_TOP, data, 4);
		} else {
			if(byteaddress & 1) {
				write_byte(byteaddress    , (data      ) & 0x00ff);
				write_word(byteaddress + 1, (data >>  8) & 0xffff);
				write_byte(byteaddress + 3, (data >> 24) & 0x00ff);
			} else {
				write_word(byteaddress    , (data      ) & 0xffff);
				write_word(byteaddress + 2, (data >> 16) & 0xffff);
			}
		}
#endif
	} else if(byteaddress >= text_vram_top_address && byteaddress < text_vram_end_address) {
		if(byteaddress < text_vram_end_address - 3) {
			if(!restore_console_size) {
				change_console_size(scr_width, scr_height);
			}
			write_text_vram_dword(byteaddress - text_vram_top_address, data);
			*(UINT32 *)(mem + byteaddress) = data;
		} else {
			if(byteaddress & 1) {
				write_byte(byteaddress    , (data      ) & 0x00ff);
				write_word(byteaddress + 1, (data >>  8) & 0xffff);
				write_byte(byteaddress + 3, (data >> 24) & 0x00ff);
			} else {
				write_word(byteaddress    , (data      ) & 0xffff);
				write_word(byteaddress + 2, (data >> 16) & 0xffff);
			}
		}
	} else if(byteaddress >= shadow_buffer_top_address && byteaddress < shadow_buffer_end_address) {
		if(byteaddress < shadow_buffer_end_address - 3) {
			if(int_10h_feh_called && !int_10h_ffh_called) {
				write_text_vram_dword(byteaddress - shadow_buffer_top_address, data);
			}
			*(UINT32 *)(mem + byteaddress) = data;
		} else {
			if(byteaddress & 1) {
				write_byte(byteaddress    , (data      ) & 0x00ff);
				write_word(byteaddress + 1, (data >>  8) & 0xffff);
				write_byte(byteaddress + 3, (data >> 24) & 0x00ff);
			} else {
				write_word(byteaddress    , (data      ) & 0xffff);
				write_word(byteaddress + 2, (data >> 16) & 0xffff);
			}
		}
#if defined(HAS_I386)
//	} else if(byteaddress < MAX_MEM - 3) {
	} else if(byteaddress < MAX_MEM) {
#else
	} else {
#endif
		*(UINT32 *)(mem + byteaddress) = data;
	}
}

/* ----------------------------------------------------------------------------
	debugger
---------------------------------------------------------------------------- */

#ifdef USE_DEBUGGER
#define TELNET_BLUE      0x0004 // text color contains blue.
#define TELNET_GREEN     0x0002 // text color contains green.
#define TELNET_RED       0x0001 // text color contains red.
#define TELNET_INTENSITY 0x0008 // text color is intensified.

int svr_socket = 0;
int cli_socket = 0;

process_t *msdos_process_info_get(UINT16 psp_seg, bool show_error);

void debugger_init()
{
	now_debugging = false;
	now_going = false;
	now_suspended = false;
	force_suspend = false;
	
	memset(&break_point, 0, sizeof(break_point_t));
	memset(&rd_break_point, 0, sizeof(break_point_t));
	memset(&wr_break_point, 0, sizeof(break_point_t));
	memset(&in_break_point, 0, sizeof(break_point_t));
	memset(&out_break_point, 0, sizeof(break_point_t));
	memset(&int_break_point, 0, sizeof(int_break_point_t));
}

bool check_file_extension(const char *file_path, const char *ext)
{
	int nam_len = strlen(file_path);
	int ext_len = strlen(ext);
	
	return (nam_len >= ext_len && strnicmp(&file_path[nam_len - ext_len], ext, ext_len) == 0);
}

void telnet_send(const char *string)
{
	char buffer[8192], *ptr;
	strcpy(buffer, string);
	while((ptr = strstr(buffer, "\n")) != NULL) {
		char tmp[8192];
		*ptr = '\0';
		sprintf(tmp, "%s\033E%s", buffer, ptr + 1);
		strcpy(buffer, tmp);
	}
	
	int len = strlen(buffer), res;
	ptr = buffer;
	while(len > 0) {
		if((res = send(cli_socket, ptr, len, 0)) > 0) {
			len -= res;
			ptr += res;
		}
	}
}

void telnet_command(const char *format, ...)
{
	char buffer[1024];
	va_list ap;
	va_start(ap, format);
	vsprintf(buffer, format, ap);
	va_end(ap);
	
	telnet_send(buffer);
}

void telnet_printf(const char *format, ...)
{
	char buffer[1024];
	va_list ap;
	va_start(ap, format);
	vsprintf(buffer, format, ap);
	va_end(ap);
	
	if(fp_debugger != NULL) {
		fprintf(fp_debugger, "%s", buffer);
	}
	telnet_send(buffer);
}

bool telnet_gets(char *str, int n)
{
	char buffer[1024];
	int ptr = 0;
	
	telnet_command("\033[12l"); // local echo on
	telnet_command("\033[2l");  // key unlock
	
	while(!msdos_exit) {
		int len = recv(cli_socket, buffer, sizeof(buffer), 0);
		
		if(len > 0 && buffer[0] != 0xff) {
			for(int i = 0; i < len; i++) {
				if(buffer[i] == 0x0d || buffer[i] == 0x0a) {
					str[ptr] = 0;
					telnet_command("\033[2h");  // key lock
					telnet_command("\033[12h"); // local echo off
					return(!msdos_exit);
				} else if(buffer[i] == 0x08 || buffer[i] == 0x7f) {
					if(ptr > 0) {
						telnet_command("\033[0K"); // erase from cursor position
						ptr--;
					} else {
						telnet_command("\033[1C"); // move cursor forward
					}
				} else if(ptr < n - 1) {
					if(buffer[i] >= 0x20 && buffer[i] <= 0x7e) {
						str[ptr++] = buffer[i];
					}
				} else {
					telnet_command("\033[1D\033[0K"); // move cursor backward and erase from cursor position
				}
			}
		} else if(len == -1) {
			if(WSAGetLastError() != WSAEWOULDBLOCK) {
				return(false);
			}
		} else if(len == 0) {
			return(false);
		}
		Sleep(10);
	}
	return(!msdos_exit);
}

bool telnet_kbhit()
{
	char buffer[1024];
	
	if(!msdos_exit) {
		int len = recv(cli_socket, buffer, sizeof(buffer), 0);
		
		if(len > 0) {
			for(int i = 0; i < len; i++) {
				if(buffer[i] == 0x0d || buffer[i] == 0x0a) {
					return(true);
				}
			}
		} else if(len == 0) {
			return(true); // disconnected
		}
	}
	return(false);
}

bool telnet_disconnected()
{
	char buffer[1024];
	int len = recv(cli_socket, buffer, sizeof(buffer), 0);
	
	if(len == 0) {
		return(true);
	} else if(len == -1) {
		if(WSAGetLastError() != WSAEWOULDBLOCK) {
			return(true);
		}
	}
	return(false);
}

void telnet_set_color(int color)
{
	telnet_command("\033[%dm\033[3%dm", (color >> 3) & 1, (color & 7));
}

int debugger_dasm(char *buffer, size_t buffer_len, UINT32 pc, UINT32 eip)
{
//	UINT8 *oprom = mem + (pc & ADDR_MASK);
	UINT8 oprom[16];
	
	for(int i = 0; i < 16; i++) {
		oprom[i] = debugger_read_byte((pc++) & ADDR_MASK);
	}
	
#if defined(HAS_I386)
	if(CPU_INST_OP32) {
		return CPU_DISASSEMBLE(oprom, eip, true, buffer, buffer_len);
	} else
#endif
	return CPU_DISASSEMBLE(oprom, eip, false, buffer, buffer_len);
}

void debugger_regs_info(char *buffer)
{
	UINT32 flags = CPU_EFLAG;
	
#if defined(HAS_I386)
	if(CPU_INST_OP32) {
		sprintf(buffer, "EAX=%08X  EBX=%08X  ECX=%08X  EDX=%08X\nESP=%08X  EBP=%08X  ESI=%08X  EDI=%08X\nEIP=%08X  DS=%04X  ES=%04X  SS=%04X  CS=%04X  FLAG=[%c %c%c%c%c%c%c%c%c%c%c%c%c%c%c%c]\n",
		CPU_EAX, CPU_EBX, CPU_ECX, CPU_EDX, CPU_ESP, CPU_EBP, CPU_ESI, CPU_EDI, CPU_EIP, CPU_DS, CPU_ES, CPU_SS, CPU_CS,
		CPU_STAT_PM ? "PE" : "--",
		(flags & 0x40000) ? 'A' : '-',
		(flags & 0x20000) ? 'V' : '-',
		(flags & 0x10000) ? 'R' : '-',
		(flags & 0x04000) ? 'N' : '-',
		(flags & 0x02000) ? '1' : '0',
		(flags & 0x01000) ? '1' : '0',
		(flags & 0x00800) ? 'O' : '-',
		(flags & 0x00400) ? 'D' : '-',
		(flags & 0x00200) ? 'I' : '-',
		(flags & 0x00100) ? 'T' : '-',
		(flags & 0x00080) ? 'S' : '-',
		(flags & 0x00040) ? 'Z' : '-',
		(flags & 0x00010) ? 'A' : '-',
		(flags & 0x00004) ? 'P' : '-',
		(flags & 0x00001) ? 'C' : '-');
	} else {
#endif
		sprintf(buffer, "AX=%04X  BX=%04X  CX=%04X  DX=%04X  SP=%04X  BP=%04X  SI=%04X  DI=%04X\nIP=%04X  DS=%04X  ES=%04X  SS=%04X  CS=%04X  FLAG=[%c %c%c%c%c%c%c%c%c%c%c%c%c%c%c%c]\n",
		CPU_AX, CPU_BX, CPU_CX, CPU_DX, CPU_SP, CPU_BP, CPU_SI, CPU_DI, CPU_EIP, CPU_DS, CPU_ES, CPU_SS, CPU_CS,
#if defined(HAS_I386)
		CPU_STAT_PM ? "PE" : "--",
#else
		"--",
#endif
		(flags & 0x40000) ? 'A' : '-',
		(flags & 0x20000) ? 'V' : '-',
		(flags & 0x10000) ? 'R' : '-',
		(flags & 0x04000) ? 'N' : '-',
		(flags & 0x02000) ? '1' : '0',
		(flags & 0x01000) ? '1' : '0',
		(flags & 0x00800) ? 'O' : '-',
		(flags & 0x00400) ? 'D' : '-',
		(flags & 0x00200) ? 'I' : '-',
		(flags & 0x00100) ? 'T' : '-',
		(flags & 0x00080) ? 'S' : '-',
		(flags & 0x00040) ? 'Z' : '-',
		(flags & 0x00010) ? 'A' : '-',
		(flags & 0x00004) ? 'P' : '-',
		(flags & 0x00001) ? 'C' : '-');
#if defined(HAS_I386)
	}
#endif
}

void debugger_process_info(char *buffer)
{
	UINT16 psp_seg = current_psp;
	process_t *process;
	bool check[0x10000] = {0};
	
	buffer[0] = '\0';
	
	while(!check[psp_seg] && (process  = msdos_process_info_get(psp_seg, false)) != NULL) {
		psp_t *psp = (psp_t *)(mem + (psp_seg << 4));
		char *file = process->module_path, *s;
		char tmp[8192];
		
		while((s = strstr(file, "\\")) != NULL) {
			file = s + 1;
		}
		sprintf(tmp, "PSP=%04X  ENV=%04X  RETURN=%04X:%04X  PROGRAM=%s\n", psp_seg, psp->env_seg, psp->int_22h.w.h, psp->int_22h.w.l, my_strupr(file));
		strcat(tmp, buffer);
		strcpy(buffer, tmp);
		
		check[psp_seg] = true;
		psp_seg = psp->parent_psp;
	}
}

UINT32 debugger_get_val(const char *str)
{
	char tmp[1024];
	
	if(str == NULL || strlen(str) == 0) {
		return(0);
	}
	strcpy(tmp, str);
	
	if(strlen(tmp) == 3 && tmp[0] == '\'' && tmp[2] == '\'') {
		// ank
		return(tmp[1] & 0xff);
	} else if(tmp[0] == '%') {
		// decimal
		return(strtoul(tmp + 1, NULL, 10));
	}
	return(strtoul(tmp, NULL, 16));
}

UINT32 debugger_get_seg(const char *str, UINT32 val)
{
	char tmp[1024], *s;
	
	if(str == NULL || strlen(str) == 0) {
		return(val);
	}
	strcpy(tmp, str);
	
	if((s = strstr(tmp, ":")) != NULL) {
		// 0000:0000
		*s = '\0';
		return(debugger_get_val(tmp));
	}
	return(val);
}

UINT32 debugger_get_ofs(const char *str)
{
	char tmp[1024], *s;
	
	if(str == NULL || strlen(str) == 0) {
		return(0);
	}
	strcpy(tmp, str);
	
	if((s = strstr(tmp, ":")) != NULL) {
		// 0000:0000
		return(debugger_get_val(s + 1));
	}
	return(debugger_get_val(tmp));
}

UINT8 debugger_hexatob(char *value)
{
	char tmp[3];
	tmp[0] = value[0];
	tmp[1] = value[1];
	tmp[2] = '\0';
	return (UINT8)strtoul(tmp, NULL, 16);
}

UINT16 debugger_hexatow(char *value)
{
	char tmp[5];
	tmp[0] = value[0];
	tmp[1] = value[1];
	tmp[2] = value[2];
	tmp[3] = value[3];
	tmp[4] = '\0';
	return (UINT16)strtoul(tmp, NULL, 16);
}

void debugger_main()
{
	telnet_command("\033[20h"); // cr-lf
	
	force_suspend = true;
	now_going = false;
	now_debugging = true;
	Sleep(100);
	
	if(!msdos_exit && !now_suspended) {
		telnet_set_color(TELNET_RED | TELNET_GREEN | TELNET_BLUE | TELNET_INTENSITY);
		telnet_printf("waiting until cpu is suspended...\n");
	}
	while(!msdos_exit && !now_suspended) {
		if(telnet_disconnected()) {
			break;
		}
		Sleep(10);
	}
	
	char buffer[8192];
	
	telnet_set_color(TELNET_RED | TELNET_GREEN | TELNET_BLUE | TELNET_INTENSITY);
	debugger_process_info(buffer);
	telnet_printf("%s", buffer);
	debugger_regs_info(buffer);
	telnet_printf("%s", buffer);
	telnet_set_color(TELNET_RED | TELNET_INTENSITY);
	telnet_printf("breaked at %08X(%04X:%04X)\n", CPU_GET_NEXT_PC(), CPU_CS, CPU_EIP);
	telnet_set_color(TELNET_GREEN | TELNET_BLUE | TELNET_INTENSITY);
	debugger_dasm(buffer, sizeof(buffer), CPU_GET_NEXT_PC(), CPU_EIP);
	telnet_printf("next\t%08X(%04X:%04X)  %s\n", CPU_GET_NEXT_PC(), CPU_CS, CPU_EIP, buffer);
	telnet_set_color(TELNET_RED | TELNET_GREEN | TELNET_BLUE | TELNET_INTENSITY);
	
	#define MAX_COMMAND_LEN	64
	
	char command[MAX_COMMAND_LEN + 1];
	char prev_command[MAX_COMMAND_LEN + 1] = {0};
	char file_path[_MAX_PATH] = "debug.bin";
	
	UINT32 data_seg = CPU_DS;
	UINT32 data_ofs = 0;
	UINT32 dasm_seg = CPU_CS;
	UINT32 dasm_ofs = CPU_EIP;
	UINT32 dasm_adr = CPU_GET_NEXT_PC();
	
	while(!msdos_exit) {
		telnet_printf("- ");
		command[0] = '\0';
		
		if(fi_debugger != NULL) {
			while(command[0] == '\0') {
				if(fgets(command, sizeof(command), fi_debugger) == NULL) {
					break;
				}
				while(strlen(command) > 0 && (command[strlen(command) - 1] == 0x0d || command[strlen(command) - 1] == 0x0a)) {
					command[strlen(command) - 1] = '\0';
				}
			}
			if(command[0] != '\0') {
				telnet_command("%s\n", command);
			}
		}
		if(command[0] == '\0') {
			if(!telnet_gets(command, sizeof(command))) {
				break;
			}
		}
		if(command[0] == '\0') {
			strcpy(command, prev_command);
		} else {
			strcpy(prev_command, command);
		}
		if(fp_debugger != NULL) {
			fprintf(fp_debugger, "%s\n", command);
		}
		
		if(!msdos_exit && command[0] != 0) {
			char *params[32], *token = NULL, *context = NULL;
			int num = 0;
			FILE *fp = NULL;
			
			if((token = strtok(command, " ")) != NULL) {
				params[num++] = token;
				while(num < 32 && (token = strtok(NULL, " ")) != NULL) {
					params[num++] = token;
				}
			}
			if(stricmp(params[0], "D") == 0) {
				if(num <= 3) {
					if(num >= 2) {
						data_seg = debugger_get_seg(params[1], data_seg);
						data_ofs = debugger_get_ofs(params[1]);
					}
					UINT32 end_seg = data_seg;
					UINT32 end_ofs = data_ofs + 8 * 16 - 1;
					if(num == 3) {
						end_seg = debugger_get_seg(params[2], data_seg);
						end_ofs = debugger_get_ofs(params[2]);
					}
					UINT64 start_addr = (data_seg << 4) + data_ofs;
					UINT64 end_addr = (end_seg << 4) + end_ofs;
//					bool is_sjis = false;
					
					for(UINT64 addr = (start_addr & ~0x0f); addr <= (end_addr | 0x0f); addr++) {
						if((addr & 0x0f) == 0) {
							if((data_ofs = addr - (data_seg << 4)) > 0xffff) {
								data_seg += 0x1000;
								data_ofs -= 0x10000;
							}
							telnet_printf("%06X:%04X ", data_seg, data_ofs);
							memset(buffer, 0, sizeof(buffer));
						}
						if(addr < start_addr || addr > end_addr) {
							telnet_printf("   ");
							buffer[addr & 0x0f] = ' ';
						} else {
							UINT8 data = debugger_read_byte(addr & ADDR_MASK);
							telnet_printf(" %02X", data);
//							if(is_sjis) {
//								buffer[addr & 0x0f] = data;
//								is_sjis = false;
//							} else if(((data >= 0x81 && data <= 0x9f) || (data >= 0xe0 && data <= 0xef)) && (addr & 0x0f) < 0x0f) {
//								buffer[addr & 0x0f] = data;
//								is_sjis = true;
//							} else
							if((data >= 0x20 && data <= 0x7e)/* || (data >= 0xa1 && data <= 0xdf)*/) {
								buffer[addr & 0x0f] = data;
							} else {
								buffer[addr & 0x0f] = '.';
							}
						}
						if((addr & 0x0f) == 0x0f) {
							telnet_printf("  %s\n", buffer);
						}
					}
					if((data_ofs = (end_addr + 1) - (data_seg << 4)) > 0xffff) {
						data_seg += 0x1000;
						data_ofs -= 0x10000;
					}
					prev_command[1] = '\0'; // remove parameters to dump continuously
				} else {
					telnet_printf("invalid parameter number\n");
				}
			} else if(stricmp(params[0], "E") == 0 || stricmp(params[0], "EB") == 0) {
				if(num >= 3) {
					UINT32 seg = debugger_get_seg(params[1], data_seg);
					UINT32 ofs = debugger_get_ofs(params[1]);
					for(int i = 2, j = 0; i < num; i++, j++) {
						debugger_write_byte(((seg << 4) + (ofs + j)) & ADDR_MASK, debugger_get_val(params[i]) & 0xff);
					}
				} else {
					telnet_printf("invalid parameter number\n");
				}
			} else if(stricmp(params[0], "EW") == 0) {
				if(num >= 3) {
					UINT32 seg = debugger_get_seg(params[1], data_seg);
					UINT32 ofs = debugger_get_ofs(params[1]);
					for(int i = 2, j = 0; i < num; i++, j += 2) {
						debugger_write_word(((seg << 4) + (ofs + j)) & ADDR_MASK, debugger_get_val(params[i]) & 0xffff);
					}
				} else {
					telnet_printf("invalid parameter number\n");
				}
			} else if(stricmp(params[0], "ED") == 0) {
				if(num >= 3) {
					UINT32 seg = debugger_get_seg(params[1], data_seg);
					UINT32 ofs = debugger_get_ofs(params[1]);
					for(int i = 2, j = 0; i < num; i++, j += 4) {
						debugger_write_dword(((seg << 4) + (ofs + j)) & ADDR_MASK, debugger_get_val(params[i]));
					}
				} else {
					telnet_printf("invalid parameter number\n");
				}
			} else if(stricmp(params[0], "EA") == 0) {
				if(num >= 3) {
					UINT32 seg = debugger_get_seg(params[1], data_seg);
					UINT32 ofs = debugger_get_ofs(params[1]);
					strcpy(buffer, prev_command);
					if((token = strtok(buffer, "\"")) != NULL && (token = strtok(NULL, "\"")) != NULL) {
						int len = strlen(token);
						for(int i = 0; i < len; i++) {
							debugger_write_byte(((seg << 4) + (ofs + i)) & ADDR_MASK, token[i] & 0xff);
						}
					} else {
						telnet_printf("invalid parameter\n");
					}
				} else {
					telnet_printf("invalid parameter number\n");
				}
			} else if(stricmp(params[0], "I") == 0 || stricmp(params[0], "IB") == 0) {
				if(num == 2) {
					telnet_printf("%02X\n", debugger_read_io_byte(debugger_get_val(params[1])) & 0xff);
				} else {
					telnet_printf("invalid parameter number\n");
				}
			} else if(stricmp(params[0], "IW") == 0) {
				if(num == 2) {
					telnet_printf("%04X\n", debugger_read_io_word(debugger_get_val(params[1])) & 0xffff);
				} else {
					telnet_printf("invalid parameter number\n");
				}
			} else if(stricmp(params[0], "ID") == 0) {
				if(num == 2) {
					telnet_printf("%08X\n", debugger_read_io_dword(debugger_get_val(params[1])));
				} else {
					telnet_printf("invalid parameter number\n");
				}
			} else if(stricmp(params[0], "O") == 0 || stricmp(params[0], "OB") == 0) {
				if(num == 3) {
					debugger_write_io_byte(debugger_get_val(params[1]), debugger_get_val(params[2]) & 0xff);
				} else {
					telnet_printf("invalid parameter number\n");
				}
			} else if(stricmp(params[0], "OW") == 0) {
				if(num == 3) {
					debugger_write_io_word(debugger_get_val(params[1]), debugger_get_val(params[2]) & 0xffff);
				} else {
					telnet_printf("invalid parameter number\n");
				}
			} else if(stricmp(params[0], "OD") == 0) {
				if(num == 3) {
					debugger_write_io_dword(debugger_get_val(params[1]), debugger_get_val(params[2]));
				} else {
					telnet_printf("invalid parameter number\n");
				}
			} else if(stricmp(params[0], "R") == 0) {
				if(num == 1) {
					debugger_regs_info(buffer);
					telnet_printf("%s", buffer);
				} else if(num == 3) {
#if defined(HAS_I386)
					if(stricmp(params[1], "EAX") == 0) {
						CPU_EAX = debugger_get_val(params[2]);
					} else if(stricmp(params[1], "EBX") == 0) {
						CPU_EBX = debugger_get_val(params[2]);
					} else if(stricmp(params[1], "ECX") == 0) {
						CPU_ECX = debugger_get_val(params[2]);
					} else if(stricmp(params[1], "EDX") == 0) {
						CPU_EDX = debugger_get_val(params[2]);
					} else if(stricmp(params[1], "ESP") == 0) {
						CPU_ESP = debugger_get_val(params[2]);
					} else if(stricmp(params[1], "EBP") == 0) {
						CPU_EBP = debugger_get_val(params[2]);
					} else if(stricmp(params[1], "ESI") == 0) {
						CPU_ESI = debugger_get_val(params[2]);
					} else if(stricmp(params[1], "EDI") == 0) {
						CPU_EDI = debugger_get_val(params[2]);
					} else
#endif
					if(stricmp(params[1], "AX") == 0) {
						CPU_AX = debugger_get_val(params[2]);
					} else if(stricmp(params[1], "BX") == 0) {
						CPU_BX = debugger_get_val(params[2]);
					} else if(stricmp(params[1], "CX") == 0) {
						CPU_CX = debugger_get_val(params[2]);
					} else if(stricmp(params[1], "DX") == 0) {
						CPU_DX = debugger_get_val(params[2]);
					} else if(stricmp(params[1], "SP") == 0) {
						CPU_SP = debugger_get_val(params[2]);
					} else if(stricmp(params[1], "BP") == 0) {
						CPU_BP = debugger_get_val(params[2]);
					} else if(stricmp(params[1], "SI") == 0) {
						CPU_SI = debugger_get_val(params[2]);
					} else if(stricmp(params[1], "DI") == 0) {
						CPU_DI = debugger_get_val(params[2]);
					} else if(stricmp(params[1], "IP") == 0 || stricmp(params[1], "EIP") == 0) {
#if defined(HAS_I386)
						if(CPU_INST_OP32) {
							CPU_SET_EIP(debugger_get_val(params[2]));
						} else {
							CPU_SET_EIP(debugger_get_val(params[2]) & 0xffff);
						}
#else
						CPU_SET_PC((CPU_CS_BASE + (debugger_get_val(params[2]) & 0xffff)) & ADDR_MASK);
#endif
					} else if(stricmp(params[1], "AL") == 0) {
						CPU_AL = debugger_get_val(params[2]);
					} else if(stricmp(params[1], "AH") == 0) {
						CPU_AH = debugger_get_val(params[2]);
					} else if(stricmp(params[1], "BL") == 0) {
						CPU_BL = debugger_get_val(params[2]);
					} else if(stricmp(params[1], "BH") == 0) {
						CPU_BH = debugger_get_val(params[2]);
					} else if(stricmp(params[1], "CL") == 0) {
						CPU_CL = debugger_get_val(params[2]);
					} else if(stricmp(params[1], "CH") == 0) {
						CPU_CH = debugger_get_val(params[2]);
					} else if(stricmp(params[1], "DL") == 0) {
						CPU_DL = debugger_get_val(params[2]);
					} else if(stricmp(params[1], "DH") == 0) {
						CPU_DH = debugger_get_val(params[2]);
					} else {
						telnet_printf("unknown register %s\n", params[1]);
					}
				} else {
					telnet_printf("invalid parameter number\n");
				}
			} else if(stricmp(params[0], "S") == 0) {
				if(num >= 4) {
					UINT32 cur_seg = debugger_get_seg(params[1], data_seg);
					UINT32 cur_ofs = debugger_get_ofs(params[1]);
					UINT32 end_seg = debugger_get_seg(params[2], cur_seg);
					UINT32 end_ofs = debugger_get_ofs(params[2]);
					UINT8 list[32];
					
					for(int i = 3, j = 0; i < num && j < 32; i++, j++) {
						list[j] = debugger_get_val(params[i]);
					}
					while((cur_seg << 4) + cur_ofs <= (end_seg << 4) + end_ofs) {
						bool found = true;
						for(int i = 3, j = 0; i < num && j < 32; i++, j++) {
							if(debugger_read_byte(((cur_seg << 4) + (cur_ofs + j)) & ADDR_MASK) != list[j]) {
								found = false;
								break;
							}
						}
						if(found) {
							telnet_printf("%04X:%04X\n", cur_seg, cur_ofs);
						}
						if((cur_ofs += 1) > 0xffff) {
							cur_seg += 0x1000;
							cur_ofs -= 0x10000;
						}
					}
				} else {
					telnet_printf("invalid parameter number\n");
				}
			} else if(stricmp(params[0], "U") == 0) {
				if(num <= 3) {
					if(num >= 2) {
						dasm_seg = debugger_get_seg(params[1], dasm_seg);
						dasm_ofs = debugger_get_ofs(params[1]);
						dasm_adr = CPU_TRANS_CODE_ADDR(dasm_seg, dasm_ofs);
					}
					if(num == 3) {
						UINT32 end_seg = debugger_get_seg(params[2], dasm_seg);
						UINT32 end_ofs = debugger_get_ofs(params[2]);
						UINT32 end_adr = CPU_TRANS_CODE_ADDR(end_seg, end_ofs);
						
						while(dasm_adr <= end_adr) {
							int len = debugger_dasm(buffer, sizeof(buffer), dasm_adr, dasm_ofs);
							telnet_printf("%08X(%04X:%04X)  ", dasm_adr, dasm_seg, dasm_ofs);
							for(int i = 0; i < len; i++) {
								telnet_printf("%02X", debugger_read_byte((dasm_adr + i) & ADDR_MASK));
							}
							for(int i = len; i < 8; i++) {
								telnet_printf("  ");
							}
							telnet_printf("  %s\n", buffer);
							if((dasm_ofs += len) > 0xffff) {
								dasm_seg += 0x1000;
								dasm_ofs -= 0x10000;
							}
							dasm_adr += len;
						}
					} else {
						for(int i = 0; i < 16; i++) {
							int len = debugger_dasm(buffer, sizeof(buffer), dasm_adr, dasm_ofs);
							telnet_printf("%08X(%04X:%04X)  ", dasm_adr, dasm_seg, dasm_ofs);
							for(int i = 0; i < len; i++) {
								telnet_printf("%02X", debugger_read_byte((dasm_adr + i) & ADDR_MASK));
							}
							for(int i = len; i < 8; i++) {
								telnet_printf("  ");
							}
							telnet_printf("  %s\n", buffer);
							if((dasm_ofs += len) > 0xffff) {
								dasm_seg += 0x1000;
								dasm_ofs -= 0x10000;
							}
							dasm_adr += len;
						}
					}
					prev_command[1] = '\0'; // remove parameters to disassemble continuously
				} else {
					telnet_printf("invalid parameter number\n");
				}
			} else if(stricmp(params[0], "UT") == 0) {
				if(num <= 3) {
					int steps = 128;
					if(num >= 2) {
						steps = min((int)debugger_get_val(params[1]), MAX_CPU_TRACE);
					}
					for(int i = MAX_CPU_TRACE - steps; i < MAX_CPU_TRACE; i++) {
						int index = (cpu_trace_ptr + i) & (MAX_CPU_TRACE - 1);
						if(cpu_trace[index].pc != 0) {
							int len = debugger_dasm(buffer, sizeof(buffer), cpu_trace[index].pc, cpu_trace[index].eip);
							telnet_printf("%08X(%04X:%04X)  ", cpu_trace[index].pc, cpu_trace[index].cs, cpu_trace[index].eip);
							for(int i = 0; i < len; i++) {
								telnet_printf("%02X", debugger_read_byte((cpu_trace[index].pc + i) & ADDR_MASK));
							}
							for(int i = len; i < 8; i++) {
								telnet_printf("  ");
							}
							telnet_printf("  %s\n", buffer);
						}
					}
				} else {
					telnet_printf("invalid parameter number\n");
				}
			} else if(stricmp(params[0], "H") == 0) {
				if(num == 3) {
					UINT32 l = debugger_get_val(params[1]);
					UINT32 r = debugger_get_val(params[2]);
					telnet_printf("%08X  %08X\n", l + r, l - r);
				} else {
					telnet_printf("invalid parameter number\n");
				}
			} else if(stricmp(params[0], "N") == 0) {
				if(num >= 2 && params[1][0] == '\"') {
					strcpy_s(buffer, sizeof(buffer), prev_command);
					if((token = strtok_s(buffer, "\"", &context)) != NULL && (token = strtok_s(NULL, "\"", &context)) != NULL) {
						strcpy_s(file_path, _MAX_PATH, token);
					} else {
						telnet_printf("invalid parameter\n");
					}
				} else if(num == 2) {
					strcpy_s(file_path, _MAX_PATH, params[1]);
				} else {
					telnet_printf("invalid parameter number\n");
				}
			} else if(stricmp(params[0], "L") == 0) {
				if(check_file_extension(file_path, ".hex")) {
					if((fp = fopen(file_path, "r")) != NULL) {
						UINT32 start_seg = data_seg;
						UINT32 start_ofs = 0; // temporary
						if(num >= 2) {
							start_seg = debugger_get_seg(params[1], start_seg);
							start_ofs = debugger_get_ofs(params[1]);
						}
						UINT32 start_addr = (start_seg << 4) + start_ofs;
						UINT32 linear = 0, segment = 0;
						char line[1024];
						while(fgets(line, sizeof(line), fp) != NULL) {
							if(line[0] != ':') continue;
							int type = debugger_hexatob(line + 7);
							if(type == 0x00) {
								UINT32 bytes = debugger_hexatob(line + 1);
								UINT32 addr = debugger_hexatow(line + 3) + start_addr + linear + segment;
								for(UINT32 i = 0; i < bytes; i++) {
									debugger_write_byte((addr + i) & ADDR_MASK, debugger_hexatob(line + 9 + 2 * i));
								}
							} else if(type == 0x01) {
								break;
							} else if(type == 0x02) {
								segment = debugger_hexatow(line + 9) << 4;
								start_addr = 0;
							} else if(type == 0x04) {
								linear = debugger_hexatow(line + 9) << 16;
								start_addr = 0;
							}
						}
						fclose(fp);
					} else {
						telnet_printf("can't open %s\n", file_path);
					}
				} else {
					if((fp = fopen(file_path, "rb")) != NULL) {
						UINT32 start_seg = data_seg;
						UINT32 start_ofs = 0x100; // temporary
						if(num >= 2) {
							start_seg = debugger_get_seg(params[1], start_seg);
							start_ofs = debugger_get_ofs(params[1]);
						}
						UINT32 end_seg = start_seg;
						UINT32 end_ofs = 0xffff;
						if(num == 3) {
							end_seg = debugger_get_seg(params[2], end_seg);
							end_ofs = debugger_get_ofs(params[2]);
						}
						UINT32 start_addr = (start_seg << 4) + start_ofs;
						UINT32 end_addr = (end_seg << 4) + end_ofs;
						for(UINT32 addr = start_addr; addr <= end_addr; addr++) {
							int data = fgetc(fp);
							if(data == EOF) {
								break;
							}
							debugger_write_byte(addr & ADDR_MASK, data);
						}
						fclose(fp);
					} else {
						telnet_printf("can't open %s\n", file_path);
					}
				}
			} else if(stricmp(params[0], "W") == 0) {
				if(num == 3) {
					UINT32 start_seg = debugger_get_seg(params[1], data_seg);
					UINT32 start_ofs = debugger_get_ofs(params[1]);
					UINT32 end_seg = debugger_get_seg(params[2], start_seg);
					UINT32 end_ofs = debugger_get_ofs(params[2]);
					UINT32 start_addr = (start_seg << 4) + start_ofs;
					UINT32 end_addr = (end_seg << 4) + end_ofs;
					if(check_file_extension(file_path, ".hex")) {
						if((fp = fopen(file_path, "w")) != NULL) {
							UINT32 addr = start_addr;
							while(addr <= end_addr) {
								UINT32 len = min(end_addr - addr + 1, (UINT32)16);
								UINT32 sum = len + ((addr >> 8) & 0xff) + (addr & 0xff) + 0x00;
								fprintf(fp, ":%02X%04X%02X", len, addr & 0xffff, 0x00);
								for(UINT32 i = 0; i < len; i++) {
									UINT8 data = debugger_read_byte((addr++) & ADDR_MASK);
									sum += data;
									fprintf(fp, "%02X", data);
								}
								fprintf(fp, "%02X\n", (0x100 - (sum & 0xff)) & 0xff);
							}
							fprintf(fp, ":00000001FF\n");
							fclose(fp);
						} else {
							telnet_printf("can't open %s\n", file_path);
						}
					} else {
						if((fp = fopen(file_path, "wb")) != NULL) {
							for(UINT32 addr = start_addr; addr <= end_addr; addr++) {
								fputc(debugger_read_byte(addr & ADDR_MASK),fp);
							}
							fclose(fp);
						} else {
							telnet_printf("can't open %s\n", file_path);
						}
					}
				} else {
					telnet_printf("invalid parameter number\n");
				}
			} else if(stricmp(params[0], "BP") == 0 || stricmp(params[0], "RBP") == 0 || stricmp(params[0], "WBP") == 0) {
				break_point_t *break_point_ptr;
				#define GET_BREAK_POINT_PTR() { \
					if(params[0][0] == 'R' || params[0][0] == 'r') { \
						break_point_ptr = &rd_break_point; \
					} else if(params[0][0] == 'W' || params[0][0] == 'w') { \
						break_point_ptr = &wr_break_point; \
					} else if(params[0][0] == 'I' || params[0][0] == 'i') { \
						break_point_ptr = &in_break_point; \
					} else if(params[0][0] == 'O' || params[0][0] == 'o') { \
						break_point_ptr = &out_break_point; \
					} else { \
						break_point_ptr = &break_point; \
					} \
				}
				GET_BREAK_POINT_PTR();
				if(num == 2) {
					UINT32 seg = 0;
					if(params[0][0] == 'R' || params[0][0] == 'r' || params[0][0] == 'W' || params[0][0] == 'w') {
						seg = debugger_get_seg(params[1], data_seg);
					} else {
						seg = debugger_get_seg(params[1], CPU_CS);
					}
					UINT32 ofs = debugger_get_ofs(params[1]);
					bool found = false;
					for(int i = 0; i < MAX_BREAK_POINTS && !found; i++) {
						if(break_point_ptr->table[i].status == 0 || (break_point_ptr->table[i].seg == seg && break_point_ptr->table[i].ofs == ofs)) {
							break_point_ptr->table[i].addr = CPU_TRANS_CODE_ADDR(seg, ofs);
							break_point_ptr->table[i].seg = seg;
							break_point_ptr->table[i].ofs = ofs;
							break_point_ptr->table[i].status = 1;
							found = true;
						}
					}
					if(!found) {
						telnet_printf("too many break points\n");
					}
				} else {
					telnet_printf("invalid parameter number\n");
				}
			} else if(stricmp(params[0], "IBP") == 0 || stricmp(params[0], "OBP") == 0) {
				break_point_t *break_point_ptr;
				GET_BREAK_POINT_PTR();
				if(num == 2) {
					UINT32 addr = debugger_get_val(params[1]);
					bool found = false;
					for(int i = 0; i < MAX_BREAK_POINTS && !found; i++) {
						if(break_point_ptr->table[i].status == 0 || break_point_ptr->table[i].addr == addr) {
							break_point_ptr->table[i].addr = addr;
							break_point_ptr->table[i].status = 1;
							found = true;
						}
					}
					if(!found) {
						telnet_printf("too many break points\n");
					}
				} else {
					telnet_printf("invalid parameter number\n");
				}
			} else if(stricmp(params[0], "BC") == 0 || stricmp(params[0], "RBC") == 0 || stricmp(params[0], "WBC") == 0 || stricmp(params[0], "IBC") == 0 || stricmp(params[0], "OBC") == 0) {
				break_point_t *break_point_ptr;
				GET_BREAK_POINT_PTR();
				if(num == 2 && (stricmp(params[1], "*") == 0 || stricmp(params[1], "ALL") == 0)) {
					memset(break_point_ptr, 0, sizeof(break_point_t));
				} else if(num >= 2) {
					for(int i = 1; i < num; i++) {
						int index = debugger_get_val(params[i]);
						if(!(index >= 1 && index <= MAX_BREAK_POINTS)) {
							telnet_printf("invalid index %x\n", index);
						} else {
							break_point_ptr->table[index - 1].addr = 0;
							break_point_ptr->table[index - 1].seg = 0;
							break_point_ptr->table[index - 1].ofs = 0;
							break_point_ptr->table[index - 1].status = 0;
						}
					}
				} else {
					telnet_printf("invalid parameter number\n");
				}
			} else if(stricmp(params[0], "BD") == 0 || stricmp(params[0], "RBD") == 0 || stricmp(params[0], "WBD") == 0 || stricmp(params[0], "IBD") == 0 || stricmp(params[0], "OBD") == 0 ||
			          stricmp(params[0], "BE") == 0 || stricmp(params[0], "RBE") == 0 || stricmp(params[0], "WBE") == 0 || stricmp(params[0], "IBE") == 0 || stricmp(params[0], "OBE") == 0) {
				break_point_t *break_point_ptr;
				GET_BREAK_POINT_PTR();
				bool enabled = (params[0][strlen(params[0]) - 1] == 'E' || params[0][strlen(params[0]) - 1] == 'e');
				if(num == 2 && (stricmp(params[1], "*") == 0 || stricmp(params[1], "ALL") == 0)) {
					for(int i = 0; i < MAX_BREAK_POINTS; i++) {
						if(break_point_ptr->table[i].status != 0) {
							break_point_ptr->table[i].status = enabled ? 1 : -1;
						}
					}
				} else if(num >= 2) {
					for(int i = 1; i < num; i++) {
						int index = debugger_get_val(params[i]);
						if(!(index >= 1 && index <= MAX_BREAK_POINTS)) {
							telnet_printf("invalid index %x\n", index);
						} else if(break_point_ptr->table[index - 1].status == 0) {
							telnet_printf("break point %x is null\n", index);
						} else {
							break_point_ptr->table[index - 1].status = enabled ? 1 : -1;
						}
					}
				} else {
					telnet_printf("invalid parameter number\n");
				}
			} else if(stricmp(params[0], "BL") == 0 || stricmp(params[0], "RBL") == 0 || stricmp(params[0], "WBL") == 0) {
				break_point_t *break_point_ptr;
				GET_BREAK_POINT_PTR();
				if(num == 1) {
					for(int i = 0; i < MAX_BREAK_POINTS; i++) {
						if(break_point_ptr->table[i].status) {
							telnet_printf("%d %c %08X(%04X:%04X)\n", i + 1,
								break_point_ptr->table[i].status == 1 ? 'e' : 'd',
								break_point_ptr->table[i].addr,
								break_point_ptr->table[i].seg,
								break_point_ptr->table[i].ofs
							);
						}
					}
				} else {
					telnet_printf("invalid parameter number\n");
				}
			} else if(stricmp(params[0], "IBL") == 0 || stricmp(params[0], "OBL") == 0) {
				break_point_t *break_point_ptr;
				GET_BREAK_POINT_PTR();
				if(num == 1) {
					for(int i = 0; i < MAX_BREAK_POINTS; i++) {
						if(break_point_ptr->table[i].status) {
							telnet_printf("%d %c %04X\n", i + 1, break_point_ptr->table[i].status == 1 ? 'e' : 'd', break_point_ptr->table[i].addr);
						}
					}
				} else {
					telnet_printf("invalid parameter number\n");
				}
			} else if(stricmp(params[0], "INTBP") == 0) {
				if(num >= 2 && num <= 4) {
					int int_num = debugger_get_val(params[1]);
					UINT8 ah = 0, ah_registered = 0;
					UINT8 al = 0, al_registered = 0;
					if(num >= 3) {
						ah = debugger_get_val(params[2]);
						ah_registered = 1;
					}
					if(num == 4) {
						al = debugger_get_val(params[3]);
						al_registered = 1;
					}
					bool found = false;
					for(int i = 0; i < MAX_BREAK_POINTS && !found; i++) {
						if(int_break_point.table[i].status == 0 || (
						   int_break_point.table[i].int_num == int_num &&
						   int_break_point.table[i].ah == ah && int_break_point.table[i].ah_registered == ah_registered &&
						   int_break_point.table[i].al == al && int_break_point.table[i].al_registered == al_registered)) {
							int_break_point.table[i].int_num = int_num;
							int_break_point.table[i].ah = ah;
							int_break_point.table[i].ah_registered = ah_registered;
							int_break_point.table[i].al = al;
							int_break_point.table[i].al_registered = al_registered;
							int_break_point.table[i].status = 1;
							found = true;
						}
					}
					if(!found) {
						telnet_printf("too many break points\n");
					}
				} else {
					telnet_printf("invalid parameter number\n");
				}
			} else if(stricmp(params[0], "INTBC") == 0) {
				if(num == 2 && (stricmp(params[1], "*") == 0 || stricmp(params[1], "ALL") == 0)) {
					memset(&int_break_point, 0, sizeof(int_break_point_t));
				} else if(num >= 2) {
					for(int i = 1; i < num; i++) {
						int index = debugger_get_val(params[i]);
						if(!(index >= 1 && index <= MAX_BREAK_POINTS)) {
							telnet_printf("invalid index %x\n", index);
						} else {
							int_break_point.table[index - 1].int_num = 0;
							int_break_point.table[index - 1].ah = 0;
							int_break_point.table[index - 1].ah_registered = 0;
							int_break_point.table[index - 1].al = 0;
							int_break_point.table[index - 1].al_registered = 0;
							int_break_point.table[index - 1].status = 0;
						}
					}
				} else {
					telnet_printf("invalid parameter number\n");
				}
			} else if(stricmp(params[0], "INTBD") == 0 || stricmp(params[0], "INTBE") == 0) {
				bool enabled = (params[0][strlen(params[0]) - 1] == 'E' || params[0][strlen(params[0]) - 1] == 'e');
				if(num == 2 && (stricmp(params[1], "*") == 0 || stricmp(params[1], "ALL") == 0)) {
					for(int i = 0; i < MAX_BREAK_POINTS; i++) {
						if(int_break_point.table[i].status != 0) {
							int_break_point.table[i].status = enabled ? 1 : -1;
						}
					}
				} else if(num >= 2) {
					for(int i = 1; i < num; i++) {
						int index = debugger_get_val(params[i]);
						if(!(index >= 1 && index <= MAX_BREAK_POINTS)) {
							telnet_printf("invalid index %x\n", index);
						} else if(int_break_point.table[index - 1].status == 0) {
							telnet_printf("break point %x is null\n", index);
						} else {
							int_break_point.table[index - 1].status = enabled ? 1 : -1;
						}
					}
				} else {
					telnet_printf("invalid parameter number\n");
				}
			} else if(stricmp(params[0], "INTBL") == 0) {
				if(num == 1) {
					for(int i = 0; i < MAX_BREAK_POINTS; i++) {
						if(int_break_point.table[i].status) {
							telnet_printf("%d %c %02X", i + 1, int_break_point.table[i].status == 1 ? 'e' : 'd', int_break_point.table[i].int_num);
							if(int_break_point.table[i].ah_registered) {
								telnet_printf(" %02X", int_break_point.table[i].ah);
							}
							if(int_break_point.table[i].al_registered) {
								telnet_printf(" %02X", int_break_point.table[i].al);
							}
							telnet_printf("\n");
						}
					}
				} else {
					telnet_printf("invalid parameter number\n");
				}
			} else if(stricmp(params[0], "G") == 0 || stricmp(params[0], "P") == 0) {
				if(num == 1 || num == 2) {
					break_point_t break_point_stored;
					bool break_points_stored = false;
					
					if(stricmp(params[0], "P") == 0) {
						memcpy(&break_point_stored, &break_point, sizeof(break_point_t));
						memset(&break_point, 0, sizeof(break_point_t));
						break_points_stored = true;
						
						break_point.table[0].addr = CPU_GET_NEXT_PC() + debugger_dasm(buffer, sizeof(buffer), CPU_GET_NEXT_PC(), CPU_EIP);
						break_point.table[0].status = 1;
					} else if(num >= 2) {
						memcpy(&break_point_stored, &break_point, sizeof(break_point_t));
						memset(&break_point, 0, sizeof(break_point_t));
						break_points_stored = true;
						
						UINT32 seg = debugger_get_seg(params[1], CPU_CS);
						UINT32 ofs = debugger_get_ofs(params[1]);
						break_point.table[0].addr = CPU_TRANS_CODE_ADDR(seg, ofs);
						break_point.table[0].seg = seg;
						break_point.table[0].ofs = ofs;
						break_point.table[0].status = 1;
					}
					break_point.hit = rd_break_point.hit = wr_break_point.hit = in_break_point.hit = out_break_point.hit = int_break_point.hit = 0;
					now_going = true;
					now_suspended = false;
					
					telnet_command("\033[2l"); // key unlock
					while(!msdos_exit && !now_suspended) {
						if(telnet_kbhit()) {
							break;
						}
						Sleep(10);
					}
					now_going = false;
					telnet_command("\033[2h"); // key lock
					
					if(!(break_point.hit || rd_break_point.hit || wr_break_point.hit || in_break_point.hit || out_break_point.hit || int_break_point.hit)) {
						Sleep(100);
						if(!msdos_exit && !now_suspended) {
							telnet_set_color(TELNET_RED | TELNET_GREEN | TELNET_BLUE | TELNET_INTENSITY);
							telnet_printf("waiting until cpu is suspended...\n");
						}
					}
					while(!msdos_exit && !now_suspended) {
						if(telnet_disconnected()) {
							break;
						}
						Sleep(10);
					}
					dasm_seg = CPU_CS;
					dasm_ofs = CPU_EIP;
					dasm_adr = CPU_GET_NEXT_PC();
					
					telnet_set_color(TELNET_GREEN | TELNET_BLUE | TELNET_INTENSITY);
					debugger_dasm(buffer, sizeof(buffer), CPU_GET_PREV_PC(), CPU_PREV_EIP);
					telnet_printf("done\t%08X(%04X:%04X)  %s\n", CPU_GET_PREV_PC(), CPU_PREV_CS, CPU_PREV_EIP, buffer);
					
					telnet_set_color(TELNET_RED | TELNET_GREEN | TELNET_BLUE | TELNET_INTENSITY);
					debugger_regs_info(buffer);
					telnet_printf("%s", buffer);
					
					if(break_point.hit) {
						if(stricmp(params[0], "G") == 0 && num == 1) {
							telnet_set_color(TELNET_RED | TELNET_INTENSITY);
							telnet_printf("breaked at %08X(%04X:%04X): break point is hit\n", CPU_GET_NEXT_PC(), CPU_CS, CPU_EIP);
						}
					} else if(rd_break_point.hit) {
						telnet_set_color(TELNET_RED | TELNET_INTENSITY);
						telnet_printf("breaked at %08X(%04X:%04X): memory %04X:%04X was read at %04X:%04X\n", CPU_GET_NEXT_PC(), CPU_CS, CPU_EIP,
						rd_break_point.table[rd_break_point.hit - 1].seg, rd_break_point.table[rd_break_point.hit - 1].ofs,
						CPU_PREV_CS, CPU_PREV_EIP);
					} else if(wr_break_point.hit) {
						telnet_set_color(TELNET_RED | TELNET_INTENSITY);
						telnet_printf("breaked at %08X(%04X:%04X): memory %04X:%04X was written at %04X:%04X\n", CPU_GET_NEXT_PC(), CPU_CS, CPU_EIP,
						wr_break_point.table[wr_break_point.hit - 1].seg, wr_break_point.table[wr_break_point.hit - 1].ofs,
						CPU_PREV_CS, CPU_PREV_EIP);
					} else if(in_break_point.hit) {
						telnet_set_color(TELNET_RED | TELNET_INTENSITY);
						telnet_printf("breaked at %08X(%04X:%04X): port %04X was read at %04X:%04X\n", CPU_GET_NEXT_PC(), CPU_CS, CPU_EIP,
						in_break_point.table[in_break_point.hit - 1].addr,
						CPU_PREV_CS, CPU_PREV_EIP);
					} else if(out_break_point.hit) {
						telnet_set_color(TELNET_RED | TELNET_INTENSITY);
						telnet_printf("breaked at %08X(%04X:%04X): port %04X was written at %04X:%04X\n", CPU_GET_NEXT_PC(), CPU_CS, CPU_EIP,
						out_break_point.table[out_break_point.hit - 1].addr,
						CPU_PREV_CS, CPU_PREV_EIP);
					} else if(int_break_point.hit) {
						telnet_set_color(TELNET_RED | TELNET_INTENSITY);
						telnet_printf("breaked at %08X(%04X:%04X): INT %02x", CPU_GET_NEXT_PC(), CPU_CS, CPU_EIP, int_break_point.table[int_break_point.hit - 1].int_num);
						if(int_break_point.table[int_break_point.hit - 1].ah_registered) {
							telnet_printf(" AH=%02x", int_break_point.table[int_break_point.hit - 1].ah);
						}
						if(int_break_point.table[int_break_point.hit - 1].al_registered) {
							telnet_printf(" AL=%02x", int_break_point.table[int_break_point.hit - 1].al);
						}
						telnet_printf(" is raised at %08X(%04X:%04X)\n", CPU_GET_PREV_PC(), CPU_PREV_CS, CPU_PREV_EIP);
					} else {
						telnet_set_color(TELNET_RED | TELNET_INTENSITY);
						telnet_printf("breaked at %08X(%04X:%04X): enter key was pressed\n", CPU_GET_NEXT_PC(), CPU_CS, CPU_EIP);
					}
					if(break_points_stored) {
						memcpy(&break_point, &break_point_stored, sizeof(break_point_t));
					}
					telnet_set_color(TELNET_GREEN | TELNET_BLUE | TELNET_INTENSITY);
					debugger_dasm(buffer, sizeof(buffer), CPU_GET_NEXT_PC(), CPU_EIP);
					telnet_printf("next\t%08X(%04X:%04X)  %s\n", CPU_GET_NEXT_PC(), CPU_CS, CPU_EIP, buffer);
					telnet_set_color(TELNET_RED | TELNET_GREEN | TELNET_BLUE | TELNET_INTENSITY);
				} else {
					telnet_printf("invalid parameter number\n");
				}
			} else if(stricmp(params[0], "T") == 0) {
				if(num == 1 || num == 2) {
					int steps = 1;
					if(num >= 2) {
						steps = debugger_get_val(params[1]);
					}
					
					telnet_command("\033[2l"); // key unlock
					while(steps-- > 0) {
						break_point.hit = rd_break_point.hit = wr_break_point.hit = in_break_point.hit = out_break_point.hit = int_break_point.hit = 0;
						now_going = false;
						now_suspended = false;
						
						while(!msdos_exit && !now_suspended) {
							if(telnet_disconnected()) {
								break;
							}
							Sleep(10);
						}
						dasm_seg = CPU_CS;
						dasm_ofs = CPU_EIP;
						dasm_adr = CPU_GET_NEXT_PC();
						
						telnet_set_color(TELNET_GREEN | TELNET_BLUE | TELNET_INTENSITY);
						debugger_dasm(buffer, sizeof(buffer), CPU_GET_PREV_PC(), CPU_PREV_EIP);
						telnet_printf("done\t%08X(%04X:%04X)  %s\n", CPU_GET_PREV_PC(), CPU_PREV_CS, CPU_PREV_EIP, buffer);
						
						telnet_set_color(TELNET_RED | TELNET_GREEN | TELNET_BLUE | TELNET_INTENSITY);
						debugger_regs_info(buffer);
						telnet_printf("%s", buffer);
						
						if(break_point.hit || rd_break_point.hit || wr_break_point.hit || in_break_point.hit || out_break_point.hit || int_break_point.hit || telnet_kbhit()) {
							break;
						}
					}
					telnet_command("\033[2h"); // key lock
					
					if(!(break_point.hit || rd_break_point.hit || wr_break_point.hit || in_break_point.hit || out_break_point.hit || int_break_point.hit)) {
						Sleep(100);
						if(!msdos_exit && !now_suspended) {
							telnet_set_color(TELNET_RED | TELNET_GREEN | TELNET_BLUE | TELNET_INTENSITY);
							telnet_printf("waiting until cpu is suspended...\n");
						}
					}
					while(!msdos_exit && !now_suspended) {
						if(telnet_disconnected()) {
							break;
						}
						Sleep(10);
					}
					if(break_point.hit) {
						telnet_set_color(TELNET_RED | TELNET_INTENSITY);
						telnet_printf("breaked at %08X(%04X:%04X): break point is hit\n", CPU_GET_NEXT_PC(), CPU_CS, CPU_EIP);
					} else if(rd_break_point.hit) {
						telnet_set_color(TELNET_RED | TELNET_INTENSITY);
						telnet_printf("breaked at %08X(%04X:%04X): memory %04X:%04X was read at %04X:%04X\n", CPU_GET_NEXT_PC(), CPU_CS, CPU_EIP,
						rd_break_point.table[rd_break_point.hit - 1].seg, rd_break_point.table[rd_break_point.hit - 1].ofs,
						CPU_PREV_CS, CPU_PREV_EIP);
					} else if(wr_break_point.hit) {
						telnet_set_color(TELNET_RED | TELNET_INTENSITY);
						telnet_printf("breaked at %08X(%04X:%04X): memory %04X:%04X was written at %04X:%04X\n", CPU_GET_NEXT_PC(), CPU_CS, CPU_EIP,
						wr_break_point.table[wr_break_point.hit - 1].seg, wr_break_point.table[wr_break_point.hit - 1].ofs,
						CPU_PREV_CS, CPU_PREV_EIP);
					} else if(in_break_point.hit) {
						telnet_set_color(TELNET_RED | TELNET_INTENSITY);
						telnet_printf("breaked at %08X(%04X:%04X): port %04X was read at %04X:%04X\n", CPU_GET_NEXT_PC(), CPU_CS, CPU_EIP,
						in_break_point.table[in_break_point.hit - 1].addr,
						CPU_PREV_CS, CPU_PREV_EIP);
					} else if(out_break_point.hit) {
						telnet_set_color(TELNET_RED | TELNET_INTENSITY);
						telnet_printf("breaked at %08X(%04X:%04X): port %04X was written at %04X:%04X\n", CPU_GET_NEXT_PC(), CPU_CS, CPU_EIP,
						out_break_point.table[out_break_point.hit - 1].addr,
						CPU_PREV_CS, CPU_PREV_EIP);
					} else if(int_break_point.hit) {
						telnet_set_color(TELNET_RED | TELNET_INTENSITY);
						telnet_printf("breaked at %08X(%04X:%04X): INT %02x", CPU_GET_NEXT_PC(), CPU_CS, CPU_EIP, int_break_point.table[int_break_point.hit - 1].int_num);
						if(int_break_point.table[int_break_point.hit - 1].ah_registered) {
							telnet_printf(" AH=%02x", int_break_point.table[int_break_point.hit - 1].ah);
						}
						if(int_break_point.table[int_break_point.hit - 1].al_registered) {
							telnet_printf(" AL=%02x", int_break_point.table[int_break_point.hit - 1].al);
						}
						telnet_printf(" is raised at %08X(%04X:%04X)\n", CPU_GET_PREV_PC(), CPU_PREV_CS, CPU_PREV_EIP);
					} else if(steps > 0) {
						telnet_set_color(TELNET_RED | TELNET_INTENSITY);
						telnet_printf("breaked at %08X(%04X:%04X): enter key was pressed\n", CPU_GET_NEXT_PC(), CPU_CS, CPU_EIP);
					}
					telnet_set_color(TELNET_GREEN | TELNET_BLUE | TELNET_INTENSITY);
					debugger_dasm(buffer, sizeof(buffer), CPU_GET_NEXT_PC(), CPU_EIP);
					telnet_printf("next\t%08X(%04X:%04X)  %s\n", CPU_GET_NEXT_PC(), CPU_CS, CPU_EIP, buffer);
					telnet_set_color(TELNET_RED | TELNET_GREEN | TELNET_BLUE | TELNET_INTENSITY);
				} else {
					telnet_printf("invalid parameter number\n");
				}
			} else if(stricmp(params[0], "Q") == 0) {
				break;
			} else if(stricmp(params[0], "X") == 0) {
				debugger_process_info(buffer);
				telnet_printf("%s", buffer);
			} else if(stricmp(params[0], ">") == 0) {
				if(num == 2) {
					if(fp_debugger != NULL) {
						fclose(fp_debugger);
						fp_debugger = NULL;
					}
					fp_debugger = fopen(params[1], "w");
				} else {
					telnet_printf("invalid parameter number\n");
				}
			} else if(stricmp(params[0], "<") == 0) {
				if(num == 2) {
					if(fi_debugger != NULL) {
						fclose(fi_debugger);
						fi_debugger = NULL;
					}
					fi_debugger = fopen(params[1], "r");
				} else {
					telnet_printf("invalid parameter number\n");
				}
			} else if(stricmp(params[0], "?") == 0) {
				telnet_printf("D [<start> [<end>]] - dump memory\n");
				telnet_printf("E[{B,W,D}] <address> <list> - edit memory (byte,word,dword)\n");
				telnet_printf("EA <address> \"<value>\" - edit memory (ascii)\n");
				telnet_printf("I[{B,W,D}] <port> - input port (byte,word,dword)\n");
				telnet_printf("O[{B,W,D}] <port> <value> - output port (byte,word,dword)\n");
				
				telnet_printf("R - show registers\n");
				telnet_printf("R <reg> <value> - edit register\n");
				telnet_printf("S <start> <end> <list> - search\n");
				telnet_printf("U [<start> [<end>]] - unassemble\n");
				telnet_printf("UT [<steps>] - unassemble trace\n");
				
				telnet_printf("H <value> <value> - hexadd\n");
				telnet_printf("N <filename> - name\n");
				telnet_printf("L [<range>] - load binary/hex file\n");
				telnet_printf("W <range> - write binary/hex file\n");
				
				telnet_printf("BP <address> - set breakpoint\n");
				telnet_printf("{R,W}BP <address> - set breakpoint (break at memory access)\n");
				telnet_printf("{I,O}BP <port> - set breakpoint (break at i/o access)\n");
				telnet_printf("INTBP <num> [<ah> [<al>]]- set breakpoint (break at interrupt)\n");
				telnet_printf("[{R,W,I,O,INT}]B{C,D,E} {*,<list>} - clear/disable/enable breakpoint(s)\n");
				telnet_printf("[{R,W,I,O,INT}]BL - list breakpoint(s)\n");
				
				telnet_printf("G - go (press enter key to break)\n");
				telnet_printf("G <address> - go and break at address\n");
				telnet_printf("P - trace one opcode (step over)\n");
				telnet_printf("T [<count>] - trace (step in)\n");
				telnet_printf("Q - quit\n");
				telnet_printf("X - show dos process info\n");
				
				telnet_printf("> <filename> - output logfile\n");
				telnet_printf("< <filename> - input commands from file\n");
				
				telnet_printf("<value> - hexa, decimal(%%d), ascii('a')\n");
				telnet_printf("<list> - <value> [<value> [<value> [...]]]\n");
			} else {
				telnet_printf("unknown command %s\n", params[0]);
			}
		}
	}
	if(fp_debugger != NULL) {
		fclose(fp_debugger);
		fp_debugger = NULL;
	}
	if(fi_debugger != NULL) {
		fclose(fi_debugger);
		fi_debugger = NULL;
	}
	now_debugging = now_going = now_suspended = force_suspend = false;
	closesocket(cli_socket);
}

const char *debugger_get_ttermpro_path()
{
	static char path[MAX_PATH] = {0};
	
	if(getenv("ProgramFiles")) {
		sprintf(path, "%s\\teraterm\\ttermpro.exe", getenv("ProgramFiles"));
	}
	return(path);
}

const char *debugger_get_ttermpro_x86_path()
{
	static char path[MAX_PATH] = {0};
	
	if(getenv("ProgramFiles(x86)")) {
		sprintf(path, "%s\\teraterm\\ttermpro.exe", getenv("ProgramFiles(x86)"));
	}
	return(path);
}

const char *debugger_get_putty_path()
{
	static char path[MAX_PATH] = {0};
	
	if(getenv("ProgramFiles")) {
		sprintf(path, "%s\\PuTTY\\putty.exe", getenv("ProgramFiles"));
	}
	return(path);
}

const char *debugger_get_putty_x86_path()
{
	static char path[MAX_PATH] = {0};
	
	if(getenv("ProgramFiles(x86)")) {
		sprintf(path, "%s\\PuTTY\\putty.exe", getenv("ProgramFiles(x86)"));
	}
	return(path);
}

const char *debugger_get_telnet_path()
{
	static char path[MAX_PATH] = {0};
	
	if(getenv("windir") != NULL) {
#ifdef _WIN64
		sprintf(path, "%s\\System32\\telnet.exe", getenv("windir"));
#else
		// prevent System32 is redirected to SysWOW64 in 32bit process on Windows x64
		sprintf(path, "%s\\Sysnative\\telnet.exe", getenv("windir"));
#endif
	}
	return(path);
}

const char *debugger_get_telnet_x86_path()
{
	static char path[MAX_PATH] = {0};
	
	if(getenv("windir") != NULL) {
#ifdef _WIN64
		sprintf(path, "%s\\SysWOW64\\telnet.exe", getenv("windir"));
#else
		// System32 will be redirected to SysWOW64 in 32bit process on Windows x64
		sprintf(path, "%s\\System32\\telnet.exe", getenv("windir"));
#endif
	}
	return(path);
}

DWORD WINAPI debugger_thread(LPVOID)
{
	WSADATA was_data;
	struct sockaddr_in svr_addr;
	struct sockaddr_in cli_addr;
	int cli_addr_len = sizeof(cli_addr);
	int port = 23;
	int bind_stat = SOCKET_ERROR;
	struct timeval timeout;
	
	WSAStartup(MAKEWORD(2,0), &was_data);
	
	if((svr_socket = socket(AF_INET, SOCK_STREAM, 0)) != INVALID_SOCKET) {
		memset(&svr_addr, 0, sizeof(svr_addr));
		svr_addr.sin_family = AF_INET;
		svr_addr.sin_addr.s_addr = htonl(INADDR_ANY);
		
		while(!msdos_exit && port < 10000) {
			svr_addr.sin_port = htons(port);
			if((bind_stat = bind(svr_socket, (struct sockaddr *)&svr_addr, sizeof(svr_addr))) == 0) {
				break;
			} else {
				port = (port == 23) ? 9000 : (port + 1);
			}
		}
		if(bind_stat == 0) {
			timeout.tv_sec = 1;
			timeout.tv_usec = 0;
			setsockopt(svr_socket, SOL_SOCKET, SO_RCVTIMEO, (char *)&timeout, sizeof(timeout));
			
			listen(svr_socket, 1);
			
			char command[MAX_PATH] = {0};
			STARTUPINFOA si;
			PROCESS_INFORMATION pi;
			
			if(_access(debugger_get_ttermpro_path(), 0) == 0) {
				sprintf(command, "%s localhost:%d /T=1", debugger_get_ttermpro_path(), port);
			} else if(_access(debugger_get_ttermpro_x86_path(), 0) == 0) {
				sprintf(command, "%s localhost:%d /T=1", debugger_get_ttermpro_x86_path(), port);
			} else if(_access(debugger_get_putty_path(), 0) == 0) {
				sprintf(command, "%s -telnet localhost %d", debugger_get_putty_path(), port);
			} else if(_access(debugger_get_putty_x86_path(), 0) == 0) {
				sprintf(command, "%s -telnet localhost %d", debugger_get_putty_x86_path(), port);
			} else if(_access(debugger_get_telnet_path(), 0) == 0) {
				sprintf(command, "%s -t vt100 localhost %d", debugger_get_telnet_path(), port);
			} else if(_access(debugger_get_telnet_x86_path(), 0) == 0) {
				sprintf(command, "%s -t vt100 localhost %d", debugger_get_telnet_x86_path(), port);
			}
			if(command[0] != '\0') {
				memset(&si, 0, sizeof(STARTUPINFOA));
				memset(&pi, 0, sizeof(PROCESS_INFORMATION));
				CreateProcessA(NULL, command, NULL, NULL, FALSE, CREATE_NEW_CONSOLE, NULL, NULL, &si, &pi);
			}
			
			while(!msdos_exit) {
				if((cli_socket = accept(svr_socket, (struct sockaddr *) &cli_addr, &cli_addr_len)) != INVALID_SOCKET) {
					u_long val = 1;
					ioctlsocket(cli_socket, FIONBIO, &val);
					debugger_main();
				}
			}
		}
	}
	WSACleanup();
	return(0);
}
#endif

/* ----------------------------------------------------------------------------
	main
---------------------------------------------------------------------------- */

BOOL WINAPI ctrl_handler(DWORD dwCtrlType)
{
	if(dwCtrlType == CTRL_BREAK_EVENT) {
		if(key_buf_char != NULL && key_buf_scan != NULL) {
#ifdef USE_SERVICE_THREAD
			EnterCriticalSection(&key_buf_crit_sect);
#endif
			pcbios_clear_key_buffer();
#ifdef USE_SERVICE_THREAD
			LeaveCriticalSection(&key_buf_crit_sect);
#endif
		}
//		key_code = key_recv = 0;
		return TRUE;
	} else if(dwCtrlType == CTRL_C_EVENT) {
		return TRUE;
	} else if(dwCtrlType == CTRL_CLOSE_EVENT) {
		// this program will be terminated abnormally, do minimum end process
		exit_handler();
		exit(1);
	}
	return FALSE;
}

void exit_handler()
{
	if(temp_file_created) {
		DeleteFileA(temp_file_path);
		temp_file_created = false;
	}
	if(key_buf_char != NULL) {
		key_buf_char->release();
		delete key_buf_char;
		key_buf_char = NULL;
	}
	if(key_buf_scan != NULL) {
		key_buf_scan->release();
		delete key_buf_scan;
		key_buf_scan = NULL;
	}
	if(key_buf_data != NULL) {
		key_buf_data->release();
		delete key_buf_data;
		key_buf_data = NULL;
	}
#ifdef SUPPORT_XMS
	msdos_xms_release();
#endif
	hardware_release();
}

#ifdef USE_VRAM_THREAD
DWORD WINAPI vram_thread(LPVOID)
{
	while(!msdos_exit) {
		EnterCriticalSection(&vram_crit_sect);
		if(vram_length_char != 0 && vram_length_char == vram_last_length_char) {
			vram_flush_char();
		}
		if(vram_length_attr != 0 && vram_length_attr == vram_last_length_attr) {
			vram_flush_attr();
		}
		vram_last_length_char = vram_length_char;
		vram_last_length_attr = vram_length_attr;
		LeaveCriticalSection(&vram_crit_sect);
		// this is about half the maximum keyboard repeat rate - any
		// lower tends to be jerky, any higher misses updates
		Sleep(15);
	}
	return 0;
}
#endif

long get_section_in_exec_file(FILE *fp, const char *name)
{
	UINT8 header[0x400];
	
	long position = ftell(fp);
	fseek(fp, 0, SEEK_SET);
	fread(header, sizeof(header), 1, fp);
	fseek(fp, position, SEEK_SET);
	
	try {
		_IMAGE_DOS_HEADER *dosHeader = (_IMAGE_DOS_HEADER *)(header + 0);
		DWORD dwTopOfSignature = dosHeader->e_lfanew;
		DWORD dwTopOfCoffHeader = dwTopOfSignature + 4;
		_IMAGE_FILE_HEADER *coffHeader = (_IMAGE_FILE_HEADER *)(header + dwTopOfCoffHeader);
		DWORD dwTopOfOptionalHeader = dwTopOfCoffHeader + sizeof(_IMAGE_FILE_HEADER);
		DWORD dwTopOfFirstSectionHeader = dwTopOfOptionalHeader + coffHeader->SizeOfOptionalHeader;
		
		for(int i = 0; i < coffHeader->NumberOfSections; i++) {
			_IMAGE_SECTION_HEADER *sectionHeader = (_IMAGE_SECTION_HEADER *)(header + dwTopOfFirstSectionHeader + IMAGE_SIZEOF_SECTION_HEADER * i);
			if(memcmp(sectionHeader->Name, name, strlen(name)) == 0) {
				return(sectionHeader->PointerToRawData);
			}
		}
	} catch(...) {
	}
	return(0);
}

bool is_started_from(const char *name)
{
	HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
	bool result = false;
	
	if(hSnapshot != INVALID_HANDLE_VALUE) {
		DWORD dwParentProcessID = 0;
		PROCESSENTRY32 pe32;
		pe32.dwSize = sizeof(PROCESSENTRY32);
		if(Process32First(hSnapshot, &pe32)) {
			do {
				if(pe32.th32ProcessID == GetCurrentProcessId()) {
					dwParentProcessID = pe32.th32ParentProcessID;
					break;
				}
			} while(Process32Next(hSnapshot, &pe32));
		}
		if(dwParentProcessID != 0) {
			HANDLE hProcess = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, dwParentProcessID);
			if(hProcess != NULL) {
				char module_path[MAX_PATH];
				if(GetProcessImageFileNameA(hProcess, module_path, MAX_PATH)) {
					int path_len = strlen(module_path);
					int name_len = strlen(name);
					if(path_len >= name_len) {
						result = (_strnicmp(module_path + path_len - name_len, name, name_len) == 0);
					}
				}
				CloseHandle(hProcess);
			}
		}
		CloseHandle(hSnapshot);
	}
	return(result);
}

bool is_started_from_console()
{
	bool result = false;
	
	if(is_winxp_or_later) {
		HMODULE hLibrary = LoadLibraryA("Kernel32.dll");
		
		if(hLibrary) {
			typedef DWORD (WINAPI *GetConsoleProcessListFunction)(__out LPDWORD lpdwProcessList, __in DWORD dwProcessCount);
			GetConsoleProcessListFunction lpfnGetConsoleProcessList;
			lpfnGetConsoleProcessList = reinterpret_cast<GetConsoleProcessListFunction>(::GetProcAddress(hLibrary, "GetConsoleProcessList"));
			if(lpfnGetConsoleProcessList) { // Windows XP or later
				DWORD dwProcessList[32];
				result = (lpfnGetConsoleProcessList(dwProcessList, 32) > 1);
				FreeLibrary(hLibrary);
				return(result);
			}
			FreeLibrary(hLibrary);
		}
	}
	return is_started_from("cmd.exe") || is_started_from("powershell.exe");
}

BOOL is_greater_windows_version(DWORD dwMajorVersion, DWORD dwMinorVersion, WORD wServicePackMajor, WORD wServicePackMinor)
{
	HMODULE hLibrary = LoadLibraryA("Kernel32.dll");
	
	if(hLibrary) {
		typedef ULONGLONG (WINAPI* VerSetConditionMaskFunction)(ULONGLONG, DWORD, BYTE);
		typedef BOOL(WINAPI* VerifyVersionInfoFunction)(LPOSVERSIONINFOEXA, DWORD, DWORDLONG);
		
		VerSetConditionMaskFunction lpfnVerSetConditionMask = reinterpret_cast<VerSetConditionMaskFunction>(::GetProcAddress(hLibrary, "VerSetConditionMask"));
		VerifyVersionInfoFunction lpfnVerifyVersionInfo = reinterpret_cast<VerifyVersionInfoFunction>(::GetProcAddress(hLibrary, "VerifyVersionInfoA"));
		
		if(lpfnVerSetConditionMask && lpfnVerifyVersionInfo) { // Windows 2000 or later
			// https://msdn.microsoft.com/en-us/library/windows/desktop/ms725491(v=vs.85).aspx
			OSVERSIONINFOEXA osvi;
			DWORDLONG dwlConditionMask = 0;
			int op = VER_GREATER_EQUAL;
			
			// Initialize the OSVERSIONINFOEXA structure.
			ZeroMemory(&osvi, sizeof(OSVERSIONINFOEXA));
			osvi.dwOSVersionInfoSize = sizeof(OSVERSIONINFOEXA);
			osvi.dwMajorVersion = dwMajorVersion;
			osvi.dwMinorVersion = dwMinorVersion;
			osvi.wServicePackMajor = wServicePackMajor;
			osvi.wServicePackMinor = wServicePackMinor;
			
			 // Initialize the condition mask.
			#define MY_VER_SET_CONDITION(_m_,_t_,_c_) ((_m_)=lpfnVerSetConditionMask((_m_),(_t_),(_c_)))
			
			MY_VER_SET_CONDITION( dwlConditionMask, VER_MAJORVERSION, op );
			MY_VER_SET_CONDITION( dwlConditionMask, VER_MINORVERSION, op );
			MY_VER_SET_CONDITION( dwlConditionMask, VER_SERVICEPACKMAJOR, op );
			MY_VER_SET_CONDITION( dwlConditionMask, VER_SERVICEPACKMINOR, op );
			
			// Perform the test.
			BOOL result = lpfnVerifyVersionInfo(&osvi, VER_MAJORVERSION | VER_MINORVERSION | VER_SERVICEPACKMAJOR | VER_SERVICEPACKMINOR, dwlConditionMask);
			FreeLibrary(hLibrary);
			return(result);
		}
		FreeLibrary(hLibrary);
	}
	
	OSVERSIONINFOA osvi;
	osvi.dwOSVersionInfoSize = sizeof(OSVERSIONINFOA);
	
	if(GetVersionExA((LPOSVERSIONINFOA)&osvi)) {
		if(osvi.dwPlatformId != VER_PLATFORM_WIN32_NT) {
			return(false);
		} else if(osvi.dwMajorVersion > dwMajorVersion) {
			return(true);
		} else if(osvi.dwMajorVersion < dwMajorVersion) {
			return(false);
		} else if(osvi.dwMinorVersion > dwMinorVersion) {
			return(true);
		} else if(osvi.dwMinorVersion < dwMinorVersion) {
			return(false);
		}
		// FIXME: check wServicePackMajor and wServicePackMinor :-(
		return(true);
	}
	return(false);
}

HWND get_console_window_handle()
{
	static HWND hwndFound = 0;
	
	if(hwndFound == 0) {
		// https://support.microsoft.com/en-us/help/124103/how-to-obtain-a-console-window-handle-hwnd
		char pszNewWindowTitle[1024];
		char pszOldWindowTitle[1024];
		
		GetConsoleTitleA(pszOldWindowTitle, 1024);
		wsprintfA(pszNewWindowTitle, "%d/%d", GetTickCount(), GetCurrentProcessId());
		SetConsoleTitleA(pszNewWindowTitle);
		Sleep(100);
		hwndFound = FindWindowA(NULL, pszNewWindowTitle);
		SetConsoleTitleA(pszOldWindowTitle);
	}
	return hwndFound;
}

HDC get_console_window_device_context()
{
	return GetDC(get_console_window_handle());
}

UINT get_input_code_page()
{
	return GetConsoleCP();
}

BOOL set_input_code_page(UINT cp)
{
	restore_input_cp = (input_cp != cp);
	return SetConsoleCP(cp);
}

UINT get_output_code_page()
{
	return GetConsoleOutputCP();
}

BOOL set_output_code_page(UINT cp)
{
	restore_output_cp = (output_cp != cp);
	return SetConsoleOutputCP(cp);
}

int get_multibyte_code_page()
{
	return _getmbcp();
}

int set_multibyte_code_page(int cp)
{
	restore_multibyte_cp = (multibyte_cp != cp);
	return _setmbcp(cp);
}

void set_default_console_font_info(CONSOLE_FONT_INFOEX *fi)
{
	fi->cbSize = sizeof(CONSOLE_FONT_INFOEX);
	fi->nFont = 0;
	fi->dwFontSize.X = 8;
	fi->dwFontSize.Y = 18;
	fi->FontFamily = 0;
	fi->FontWeight = 400;
	wcscpy_s(fi->FaceName, LF_FACESIZE, L"Terminal");
}

bool get_console_font_info(CONSOLE_FONT_INFOEX *fi)
{
	HANDLE hStdout = GetStdHandle(STD_OUTPUT_HANDLE);
	bool result = false;
	
	set_default_console_font_info(fi);
	
	if(is_vista_or_later) {
		HMODULE hLibrary = LoadLibraryA("Kernel32.dll");
		if(hLibrary) {
			typedef BOOL (WINAPI* GetCurrentConsoleFontExFunction)(HANDLE, BOOL, PCONSOLE_FONT_INFOEX);
			GetCurrentConsoleFontExFunction lpfnGetCurrentConsoleFontEx = reinterpret_cast<GetCurrentConsoleFontExFunction>(::GetProcAddress(hLibrary, "GetCurrentConsoleFontEx"));
			if(lpfnGetCurrentConsoleFontEx) {
				if(lpfnGetCurrentConsoleFontEx(hStdout, FALSE, fi)) {
					if(is_started_from("cmd.exe")) {
						char mbs[LF_FACESIZE];
						WideCharToMultiByte(CP_THREAD_ACP, 0, fi->FaceName, wcslen(fi->FaceName) + 1, mbs, LF_FACESIZE,NULL,NULL);
						mbstowcs(fi->FaceName, mbs, LF_FACESIZE);
					}
					result = true;
				}
			}
			FreeLibrary(hLibrary);
		}
	} else if(is_winxp_or_later) {
		HMODULE hLibrary = LoadLibraryA("Kernel32.dll");
		if(hLibrary) {
			typedef BOOL (WINAPI* GetCurrentConsoleFontFunction)(HANDLE, BOOL, PCONSOLE_FONT_INFO);
			GetCurrentConsoleFontFunction lpfnGetCurrentConsoleFont = reinterpret_cast<GetCurrentConsoleFontFunction>(::GetProcAddress(hLibrary, "GetCurrentConsoleFont"));
			if(lpfnGetCurrentConsoleFont) { // Windows XP or later
				CONSOLE_FONT_INFO fi_tmp;
				if(lpfnGetCurrentConsoleFont(hStdout, FALSE, &fi_tmp)) {
					fi->nFont = fi_tmp.nFont;
					fi->dwFontSize.X = fi_tmp.dwFontSize.X;
					fi->dwFontSize.Y = fi_tmp.dwFontSize.Y;
					result = true;
				}
			}
			FreeLibrary(hLibrary);
		}
	}
	if(!result) {
		CONSOLE_SCREEN_BUFFER_INFO csbi;
		RECT rect;
		if(GetConsoleScreenBufferInfo(hStdout, &csbi) && GetClientRect(get_console_window_handle(), &rect)) {
			int cols = csbi.srWindow.Right - csbi.srWindow.Left + 1;
			int rows = csbi.srWindow.Bottom - csbi.srWindow.Top + 1;
			fi->dwFontSize.X = rect.right / cols;
			fi->dwFontSize.Y = rect.bottom / rows;
			result = true;
		}
	}
	return(result);
}

bool set_console_font_info(CONSOLE_FONT_INFOEX *fi)
{
	HANDLE hStdout = GetStdHandle(STD_OUTPUT_HANDLE);
	bool result = false;
	HMODULE hLibrary = LoadLibraryA("Kernel32.dll");
	
	if(hLibrary) {
		if(is_vista_or_later) {
			typedef BOOL (WINAPI* SetCurrentConsoleFontExFunction)(HANDLE, BOOL, PCONSOLE_FONT_INFOEX);
			typedef BOOL (WINAPI* GetCurrentConsoleFontExFunction)(HANDLE, BOOL, PCONSOLE_FONT_INFOEX);
			SetCurrentConsoleFontExFunction lpfnSetCurrentConsoleFontEx = reinterpret_cast<SetCurrentConsoleFontExFunction>(::GetProcAddress(hLibrary, "SetCurrentConsoleFontEx"));
			GetCurrentConsoleFontExFunction lpfnGetCurrentConsoleFontEx = reinterpret_cast<GetCurrentConsoleFontExFunction>(::GetProcAddress(hLibrary, "GetCurrentConsoleFontEx"));
			if(lpfnSetCurrentConsoleFontEx && lpfnGetCurrentConsoleFontEx) {
				if(lpfnSetCurrentConsoleFontEx(hStdout, FALSE, fi)) {
					CONSOLE_FONT_INFOEX fi_tmp;
					if(get_console_font_info(&fi_tmp)) {
						if(wcscmp(fi->FaceName, fi_tmp.FaceName) == 0 && fi->dwFontSize.X == fi_tmp.dwFontSize.X && fi->dwFontSize.Y == fi_tmp.dwFontSize.Y) {
							result = true;
						}
					}
				}
			}
		}
//		if(!result) {
		else {
			typedef BOOL (WINAPI* GetConsoleFontInfoFunction)(HANDLE, BOOL, DWORD, PCONSOLE_FONT_INFO);
			typedef DWORD (WINAPI* GetNumberOfConsoleFontsFunction)(VOID);
			typedef COORD (WINAPI* GetConsoleFontSizeFunction)(HANDLE, DWORD);
			typedef BOOL (WINAPI* SetConsoleFontFunction)(HANDLE, DWORD);
			typedef BOOL (WINAPI* GetCurrentConsoleFontFunction)(HANDLE, BOOL, PCONSOLE_FONT_INFO);
			
			GetConsoleFontInfoFunction lpfnGetConsoleFontInfo = reinterpret_cast<GetConsoleFontInfoFunction>(::GetProcAddress(hLibrary, "GetConsoleFontInfo"));
			GetNumberOfConsoleFontsFunction lpfnGetNumberOfConsoleFonts = reinterpret_cast<GetNumberOfConsoleFontsFunction>(::GetProcAddress(hLibrary, "GetNumberOfConsoleFonts"));
			GetConsoleFontSizeFunction lpfnGetConsoleFontSize = reinterpret_cast<GetConsoleFontSizeFunction>(::GetProcAddress(hLibrary, "GetConsoleFontSize"));
			SetConsoleFontFunction lpfnSetConsoleFont = reinterpret_cast<SetConsoleFontFunction>(::GetProcAddress(hLibrary, "SetConsoleFont"));
			GetCurrentConsoleFontFunction lpfnGetCurrentConsoleFont = reinterpret_cast<GetCurrentConsoleFontFunction>(::GetProcAddress(hLibrary, "GetCurrentConsoleFont"));
			
			if(lpfnGetConsoleFontInfo && lpfnGetNumberOfConsoleFonts && lpfnSetConsoleFont) { // Windows 2000 or later
				DWORD dwFontNum = lpfnGetNumberOfConsoleFonts();
				if(dwFontNum) {
					CONSOLE_SCREEN_BUFFER_INFO csbi;
					RECT rect;
					GetConsoleScreenBufferInfo(hStdout, &csbi);
					int cols = csbi.srWindow.Right - csbi.srWindow.Left + 1;
					int rows = csbi.srWindow.Bottom - csbi.srWindow.Top + 1;
					
					CONSOLE_FONT_INFO* fonts = (CONSOLE_FONT_INFO*)malloc(sizeof(CONSOLE_FONT_INFO) * dwFontNum);
					lpfnGetConsoleFontInfo(hStdout, FALSE, dwFontNum, fonts);
					
					for(int i = 0; i < dwFontNum; i++) {
						fonts[i].dwFontSize = lpfnGetConsoleFontSize(hStdout, fonts[i].nFont);
						if(fonts[i].dwFontSize.X == fi->dwFontSize.X && fonts[i].dwFontSize.Y == fi->dwFontSize.Y) {
							if(lpfnSetConsoleFont(hStdout, fonts[i].nFont)) {
								if(is_winxp_or_later && lpfnGetCurrentConsoleFont) { // Windows XP or later
									CONSOLE_FONT_INFO fi_tmp;
									if(lpfnGetCurrentConsoleFont(hStdout, FALSE, &fi_tmp)) {
										if(fonts[i].dwFontSize.X == fi_tmp.dwFontSize.X && fonts[i].dwFontSize.Y == fi_tmp.dwFontSize.Y) {
											result = true;
											break;
										}
									}
								} else {
									Sleep(10);
									if(GetClientRect(get_console_window_handle(), &rect)) {
										if(fonts[i].dwFontSize.X * cols == rect.right && fonts[i].dwFontSize.Y * rows == rect.bottom) {
											result = true;
											break;
										}
									}
								}
							}
						}
					}
					free(fonts);
				}
			}
		}
		FreeLibrary(hLibrary);
	}
	restore_console_font = true;
	return(result);
}

bool set_console_font_size(int width, int height)
{
	if(is_win10_or_later && active_code_page == 932) {
		set_default_console_font_info(&fi_new);
	}
	fi_new.dwFontSize.X = width;
	fi_new.dwFontSize.Y = height;
	
	if(!set_console_font_info(&fi_new)) {
		if(is_win10_or_later && active_code_page == 932) {
			set_default_console_font_info(&fi_new);
			set_console_font_info(&fi_new);
		}
		return(false);
	}
	return(true);
}

bool is_cursor_blink_off()
{
	static int result = -1;
	HKEY hKey;
	char chData[64];
	DWORD dwSize = sizeof(chData);
	
	if(result == -1) {
		result = 0;
		if(RegOpenKeyExA(HKEY_CURRENT_USER, "Control Panel\\Desktop", 0, KEY_READ, &hKey) == ERROR_SUCCESS) {
			if(RegQueryValueExA(hKey, "CursorBlinkRate", NULL, NULL, (LPBYTE)chData, &dwSize) == ERROR_SUCCESS) {
				if(strncmp(chData, "-1", 2) == 0) {
					result = 1;
				}
			}
			RegCloseKey(hKey);
		}
	}
	return(result != 0);
}

void get_sio_port_numbers()
{
	SP_DEVINFO_DATA DeviceInfoData = {sizeof(SP_DEVINFO_DATA)};
	HDEVINFO hDevInfo = 0;
	HKEY hKey = 0;
	if((hDevInfo = SetupDiGetClassDevsA(&GUID_DEVINTERFACE_COMPORT, NULL, NULL, (DIGCF_PRESENT | DIGCF_DEVICEINTERFACE))) != 0) {
		for(int i = 0; SetupDiEnumDeviceInfo(hDevInfo, i, &DeviceInfoData); i++) {
			if((hKey = SetupDiOpenDevRegKey(hDevInfo, &DeviceInfoData, DICS_FLAG_GLOBAL, 0, DIREG_DEV, KEY_QUERY_VALUE)) != INVALID_HANDLE_VALUE) {
				char chData[256];
				DWORD dwType = 0;
				DWORD dwSize = sizeof(chData);
				int port_number = 0;
				
				if(RegQueryValueExA(hKey, "PortName", NULL, &dwType, (BYTE *)chData, &dwSize) == ERROR_SUCCESS) {
					if(_strnicmp(chData, "COM", 3) == 0) {
						port_number = atoi(chData + 3);
					}
				}
				RegCloseKey(hKey);
				
				if(sio_port_number[0] == port_number || sio_port_number[1] == port_number || sio_port_number[2] == port_number || sio_port_number[3] == port_number) {
					continue;
				}
				if(sio_port_number[0] == 0) {
					sio_port_number[0] = port_number;
				} else if(sio_port_number[1] == 0) {
					sio_port_number[1] = port_number;
				} else if(sio_port_number[2] == 0) {
					sio_port_number[2] = port_number;
				} else if(sio_port_number[3] == 0) {
					sio_port_number[3] = port_number;
				}
				if(sio_port_number[0] != 0 && sio_port_number[1] != 0 && sio_port_number[2] != 0 && sio_port_number[3] != 0) {
					break;
				}
			}
		}
	}
}

#define IS_NUMERIC(c) ((c) >= '0' && (c) <= '9')

int main(int argc, char *argv[], char *envp[])
{
	is_winxp_or_later = is_greater_windows_version( 5, 1, 0, 0);
	is_xp_64_or_later = is_greater_windows_version( 5, 2, 0, 0);
	is_vista_or_later = is_greater_windows_version( 6, 0, 0, 0);
	is_win10_or_later = is_greater_windows_version(10, 0, 0, 0);
	
	input_cp = get_input_code_page();
	output_cp = get_output_code_page();
	multibyte_cp = get_multibyte_code_page();
	
	int arg_offset = 0;
	int standard_env = 0;
	int buf_width = 0, buf_height = 0;
	bool get_console_buffer_success = false;
	bool get_console_cursor_success = false;
	bool get_console_font_success = false;
	bool screen_size_changed = false;
	
	char dummy_argv_0[] = "msdos.exe";
	char dummy_argv_1[MAX_PATH];
	char *dummy_argv[256] = {dummy_argv_0, dummy_argv_1, 0};
	char new_exec_file[MAX_PATH];
	bool convert_cmd_file = false;
	unsigned int code_page = 0;
	
	char path[MAX_PATH], full[MAX_PATH], *name = NULL;
	
	GetModuleFileNameA(NULL, path, MAX_PATH);
	GetFullPathNameA(path, MAX_PATH, full, &name);
	
	if(name != NULL && stricmp(name, "msdos.exe") != 0) {
		// check if command file is embedded to this execution file
		// if this execution file name is msdos.exe, don't check
		FILE* fp = fopen(full, "rb");
		long offset = get_section_in_exec_file(fp, ".msdos");
		if(offset != 0) {
			UINT8 buffer[16];
			fseek(fp, offset, SEEK_SET);
			fread(buffer, sizeof(buffer), 1, fp);
			
			// restore flags
			stay_busy           = ((buffer[0] & 0x01) != 0);
			no_windows          = ((buffer[0] & 0x02) != 0);
			standard_env        = ((buffer[0] & 0x04) != 0);
			ignore_illegal_insn = ((buffer[0] & 0x08) != 0);
			limit_max_memory    = ((buffer[0] & 0x10) != 0);
			if((buffer[0] & 0x20) != 0) {
				get_sio_port_numbers();
			}
			if((buffer[0] & 0x40) != 0) {
				UMB_TOP = EMS_TOP + EMS_SIZE;
				support_ems = true;
			}
#ifdef SUPPORT_XMS
			if((buffer[0] & 0x80) != 0) {
				support_xms = true;
			}
#endif
			if((buffer[1] != 0 || buffer[2] != 0) && (buffer[3] != 0 || buffer[4] != 0)) {
				buf_width  = buffer[1] | (buffer[2] << 8);
				buf_height = buffer[3] | (buffer[4] << 8);
			}
			if(buffer[5] != 0) {
				dos_major_version = buffer[5];
				dos_minor_version = buffer[6];
			}
			if(buffer[7] != 0) {
				win_major_version = buffer[7];
				win_minor_version = buffer[8];
			}
			if((code_page = buffer[9] | (buffer[10] << 8)) != 0) {
				set_input_code_page(code_page);
				set_output_code_page(code_page);
			}
			int name_len = buffer[11];
			int file_len = buffer[12] | (buffer[13] << 8) | (buffer[14] << 16) | (buffer[15] << 24);
			
			// restore command file name
			memset(dummy_argv_1, 0, sizeof(dummy_argv_1));
			fread(dummy_argv_1, name_len, 1, fp);
			
			if(name_len != 0 && _access(dummy_argv_1, 0) == 0) {
				// if original command file exists, create a temporary file name
				if(GetTempFileNameA(".", "DOS", 0, dummy_argv_1) != 0) {
					// create a temporary command file in the current director
					DeleteFileA(dummy_argv_1);
				} else {
					// create a temporary command file in the temporary folder
					GetTempPathA(MAX_PATH, path);
					if(GetTempFileNameA(path, "DOS", 0, dummy_argv_1) != 0) {
						DeleteFileA(dummy_argv_1);
					} else {
						sprintf(dummy_argv_1, "%s$DOSPRG$.TMP", path);
					}
				}
				// check the command file type
				fread(buffer, 2, 1, fp);
				fseek(fp, -2, SEEK_CUR);
				if(memcmp(buffer, "MZ", 2) != 0) {
					memcpy(dummy_argv_1 + strlen(dummy_argv_1) - 4, ".COM", 4);
				} else {
					memcpy(dummy_argv_1 + strlen(dummy_argv_1) - 4, ".EXE", 4);
				}
			}
			
			// restore command file
			FILE* fo = fopen(dummy_argv_1, "wb");
			for(int i = 0; i < file_len; i++) {
				fputc(fgetc(fp), fo);
			}
			fclose(fo);
			
			GetFullPathNameA(dummy_argv_1, MAX_PATH, temp_file_path, NULL);
			temp_file_created = true;
			SetFileAttributesA(temp_file_path, FILE_ATTRIBUTE_HIDDEN);
			
			// adjust argc/argv
			for(int i = 1; i < argc && (i + 1) < 256; i++) {
				dummy_argv[i + 1] = argv[i];
			}
			argc++;
			argv = dummy_argv;
		}
		fclose(fp);
	}
	for(int i = 1; i < argc; i++) {
		if(_strnicmp(argv[i], "-b", 2) == 0) {
			stay_busy = true;
			arg_offset++;
		} else if(_strnicmp(argv[i], "-c", 2) == 0) {
			if(argv[i][2] != '\0') {
				strcpy(new_exec_file, &argv[i][2]);
			} else {
				strcpy(new_exec_file, "new_exec_file.exe");
			}
			convert_cmd_file = true;
			arg_offset++;
		} else if(_strnicmp(argv[i], "-p", 2) == 0) {
			if(IS_NUMERIC(argv[i][2])) {
				code_page = atoi(&argv[i][2]);
			} else {
				code_page = get_input_code_page();
			}
			arg_offset++;
		} else if(_strnicmp(argv[i], "-d", 2) == 0) {
			no_windows = true;
			arg_offset++;
		} else if(_strnicmp(argv[i], "-e", 2) == 0) {
			standard_env = 1;
			arg_offset++;
		} else if(_strnicmp(argv[i], "-i", 2) == 0) {
			ignore_illegal_insn = true;
			arg_offset++;
		} else if(_strnicmp(argv[i], "-m", 2) == 0) {
			limit_max_memory = true;
			arg_offset++;
		} else if(_strnicmp(argv[i], "-n", 2) == 0) {
			int result = sscanf(argv[i] + 2, "%d,%d", &buf_height, &buf_width);
			if(result == 1) {
				buf_width = 0;
			} else if(result != 2) {
				buf_width = buf_height = 0;
			}
			if(buf_width <= 0 || buf_width > 0x7fff) {
				buf_width = 80;
			}
			if(buf_height <= 0 || buf_height > 0x7fff) {
				buf_height = 25;
			}
			arg_offset++;
		} else if(_strnicmp(argv[i], "-s", 2) == 0) {
			if(IS_NUMERIC(argv[i][2])) {
				char *p0 = &argv[i][2], *p1, *p2, *p3;
				if((p1 = strchr(p0, ',')) != NULL && IS_NUMERIC(p1[1])) {
					sio_port_number[1] = atoi(p1 + 1);
					if((p2 = strchr(p1, ',')) != NULL && IS_NUMERIC(p2[1])) {
						sio_port_number[2] = atoi(p2 + 1);
						if((p3 = strchr(p2, ',')) != NULL && IS_NUMERIC(p3[1])) {
							sio_port_number[3] = atoi(p3 + 1);
						}
					}
				}
				sio_port_number[0] = atoi(p0);
			}
			if(sio_port_number[0] == 0 || sio_port_number[1] == 0 || sio_port_number[2] == 0 || sio_port_number[3] == 0) {
				get_sio_port_numbers();
			}
			arg_offset++;
		} else if(_strnicmp(argv[i], "-v", 2) == 0) {
			if(strlen(argv[i]) >= 5 && IS_NUMERIC(argv[i][2]) && argv[i][3] == '.' && IS_NUMERIC(argv[i][4]) && (argv[i][5] == '\0' || IS_NUMERIC(argv[i][5]))) {
				dos_major_version = argv[i][2] - '0';
				dos_minor_version = (argv[i][4] - '0') * 10 + (argv[i][5] ? (argv[i][5] - '0') : 0);
			}
			arg_offset++;
		} else if(_strnicmp(argv[i], "-w", 2) == 0) {
			if(strlen(argv[i]) >= 5 && IS_NUMERIC(argv[i][2]) && argv[i][3] == '.' && IS_NUMERIC(argv[i][4]) && (argv[i][5] == '\0' || IS_NUMERIC(argv[i][5]))) {
				win_major_version = argv[i][2] - '0';
				win_minor_version = (argv[i][4] - '0') * 10 + (argv[i][5] ? (argv[i][5] - '0') : 0);
			}
			arg_offset++;
		} else if(_strnicmp(argv[i], "-x", 2) == 0) {
			UMB_TOP = EMS_TOP + EMS_SIZE;
			support_ems = true;
#ifdef SUPPORT_XMS
			support_xms = true;
#endif
			arg_offset++;
		} else {
			break;
		}
	}
	if(argc < 2 + arg_offset) {
#ifdef _WIN64
	#ifdef HAS_IA32
		fprintf(stderr, "MS-DOS Player (ia32) for Win32-x64 console\n\n");
	#else
		fprintf(stderr, "MS-DOS Player (" CPU_MODEL_NAME(CPU_MODEL) ") for Win32-x64 console\n\n");
	#endif
#else
	#ifdef HAS_IA32
		fprintf(stderr, "MS-DOS Player (ia32) for Win32 console\n\n");
	#else
		fprintf(stderr, "MS-DOS Player (" CPU_MODEL_NAME(CPU_MODEL) ") for Win32 console\n\n");
	#endif
#endif
		fprintf(stderr,
			"Usage: MSDOS [-b] [-c[(new exec file)] [-p[P]]] [-d] [-e] [-i] [-m] [-n[L[,C]]]\n"
			"             [-s[P1[,P2[,P3[,P4]]]]] [-vX.XX] [-wX.XX] [-x] (command) [options]\n"
			"\n"
			"\t-b\tstay busy during keyboard polling\n"
#ifdef _WIN64
			"\t-c\tconvert command file to 64bit execution file\n"
#else
			"\t-c\tconvert command file to 32bit execution file\n"
#endif
			"\t-p\trecord current code page when convert command file\n"
			"\t-d\tpretend running under straight DOS, not Windows\n"
			"\t-e\tuse a reduced environment block\n"
			"\t-i\tignore invalid instructions\n"
			"\t-m\trestrict free memory to 0x7FFF paragraphs\n"
			"\t-n\tcreate a new buffer (25 lines, 80 columns by default)\n"
			"\t-s\tenable serial I/O and set host's COM port numbers\n"
			"\t-v\tset the DOS version\n"
			"\t-w\tset the Windows version\n"
#if defined(SUPPORT_VCPI)
			"\t-x\tenable LIM EMS, VCPI, and XMS\n"
#elif defined(SUPPORT_XMS)
			"\t-x\tenable LIM EMS and XMS\n"
#else
			"\t-x\tenable LIM EMS\n"
#endif
		);
		
		if(!is_started_from_console()) {
			fprintf(stderr, "\nStart this program from a command prompt!\n\nHit any key to quit...");
			while(!_kbhit()) {
				Sleep(10);
			}
		}
#ifdef _DEBUG
		_CrtDumpMemoryLeaks();
#endif
		return(EXIT_FAILURE);
	}
	if(convert_cmd_file) {
		retval = EXIT_FAILURE;
		if(name != NULL/* && stricmp(name, "msdos.exe") == 0*/) {
			FILE *fp = NULL, *fs = NULL, *fo = NULL;
			int len = strlen(argv[arg_offset + 1]), data;
			
			if(!(len > 4 && (stricmp(argv[arg_offset + 1] + len - 4, ".COM") == 0 || stricmp(argv[arg_offset + 1] + len - 4, ".EXE") == 0))) {
				fprintf(stderr, "Specify command file with extenstion (.COM or .EXE)\n");
			} else if((fp = fopen(full, "rb")) == NULL) {
				fprintf(stderr, "Can't open '%s'\n", name);
			} else {
				long offset = get_section_in_exec_file(fp, ".msdos");
				if(offset != 0) {
					UINT8 buffer[14];
					fseek(fp, offset, SEEK_SET);
					fread(buffer, sizeof(buffer), 1, fp);
					memset(path, 0, sizeof(path));
					fread(path, buffer[9], 1, fp);
					fprintf(stderr, "Command file '%s' was already embedded to '%s'\n", path, name);
				} else if((fs = fopen(argv[arg_offset + 1], "rb")) == NULL) {
					fprintf(stderr, "Can't open '%s'\n", argv[arg_offset + 1]);
				} else if((fo = fopen(new_exec_file, "wb")) == NULL) {
					fprintf(stderr, "Can't open '%s'\n", new_exec_file);
				} else {
					// read pe header of msdos.exe
					UINT8 header[0x400];
					fseek(fp, 0, SEEK_SET);
					fread(header, sizeof(header), 1, fp);
					
					_IMAGE_DOS_HEADER *dosHeader = (_IMAGE_DOS_HEADER *)(header + 0);
					DWORD dwTopOfSignature = dosHeader->e_lfanew;
					DWORD dwTopOfCoffHeader = dwTopOfSignature + 4;
					_IMAGE_FILE_HEADER *coffHeader = (_IMAGE_FILE_HEADER *)(header + dwTopOfCoffHeader);
					DWORD dwTopOfOptionalHeader = dwTopOfCoffHeader + sizeof(_IMAGE_FILE_HEADER);
					_IMAGE_OPTIONAL_HEADER *optionalHeader = (_IMAGE_OPTIONAL_HEADER *)(header + dwTopOfOptionalHeader);
					DWORD dwTopOfFirstSectionHeader = dwTopOfOptionalHeader + coffHeader->SizeOfOptionalHeader;
					
					_IMAGE_SECTION_HEADER *lastSectionHeader = (_IMAGE_SECTION_HEADER *)(header + dwTopOfFirstSectionHeader + IMAGE_SIZEOF_SECTION_HEADER * (coffHeader->NumberOfSections - 1));
					DWORD dwEndOfFile = lastSectionHeader->PointerToRawData + lastSectionHeader->SizeOfRawData;
					DWORD dwLastSectionSize = lastSectionHeader->SizeOfRawData;
					DWORD dwExtraLastSectionBytes = dwLastSectionSize % optionalHeader->SectionAlignment;
					if(dwExtraLastSectionBytes != 0) {
						DWORD dwRemain = optionalHeader->SectionAlignment - dwExtraLastSectionBytes;
						dwLastSectionSize += dwRemain;
					}
					DWORD dwVirtualAddress = lastSectionHeader->VirtualAddress + dwLastSectionSize;
					
					// store msdos.exe
					fseek(fp, 0, SEEK_SET);
					for(int i = 0; i < dwEndOfFile; i++) {
						if((data = fgetc(fp)) != EOF) {
							fputc(data, fo);
						} else {
							// we should not reach here :-(
							fputc(0, fo);
						}
					}
					
					// store options
					UINT8 flags = 0;
					if(stay_busy) {
						flags |= 0x01;
					}
					if(no_windows) {
						flags |= 0x02;
					}
					if(standard_env) {
						flags |= 0x04;
					}
					if(ignore_illegal_insn) {
						flags |= 0x08;
					}
					if(limit_max_memory) {
						flags |= 0x10;
					}
					if(sio_port_number[0] != 0 || sio_port_number[1] != 0 || sio_port_number[2] != 0 || sio_port_number[3] != 0) {
						flags |= 0x20;
					}
					if(support_ems) {
						flags |= 0x40;
					}
#ifdef SUPPORT_XMS
					if(support_xms) {
						flags |= 0x80;
					}
#endif
					
					fputc(flags, fo);
					fputc((buf_width  >> 0) & 0xff, fo);
					fputc((buf_width  >> 8) & 0xff, fo);
					fputc((buf_height >> 0) & 0xff, fo);
					fputc((buf_height >> 8) & 0xff, fo);
					fputc(dos_major_version, fo);
					fputc(dos_minor_version, fo);
					fputc(win_major_version, fo);
					fputc(win_minor_version, fo);
					fputc((code_page >> 0) & 0xff, fo);
					fputc((code_page >> 8) & 0xff, fo);
					
					// store command file info
					GetFullPathNameA(argv[arg_offset + 1], MAX_PATH, full, &name);
					int name_len = strlen(name);
					fseek(fs, 0, SEEK_END);
					long file_size = ftell(fs);
					
					fputc(name_len, fo);
					fputc((file_size >>  0) & 0xff, fo);
					fputc((file_size >>  8) & 0xff, fo);
					fputc((file_size >> 16) & 0xff, fo);
					fputc((file_size >> 24) & 0xff, fo);
					fwrite(name, name_len, 1, fo);
					
					// store command file
					fseek(fs, 0, SEEK_SET);
					for(int i = 0; i < file_size; i++) {
						if((data = fgetc(fs)) != EOF) {
							fputc(data, fo);
						} else {
							// we should not reach here :-(
							fputc(0, fo);
						}
					}
					
					// store padding data and update pe header
					_IMAGE_SECTION_HEADER *newSectionHeader = (_IMAGE_SECTION_HEADER *)(header + dwTopOfFirstSectionHeader + IMAGE_SIZEOF_SECTION_HEADER * coffHeader->NumberOfSections);
					coffHeader->NumberOfSections++;
					memset(newSectionHeader, 0, IMAGE_SIZEOF_SECTION_HEADER);
					memcpy(newSectionHeader->Name, ".msdos", 6);
					newSectionHeader->VirtualAddress = dwVirtualAddress;
					newSectionHeader->PointerToRawData = dwEndOfFile;
					newSectionHeader->Characteristics = IMAGE_SCN_CNT_INITIALIZED_DATA | IMAGE_SCN_MEM_DISCARDABLE;
					newSectionHeader->SizeOfRawData = 14 + name_len + file_size;
					DWORD dwExtraRawBytes = newSectionHeader->SizeOfRawData % optionalHeader->FileAlignment;
					if(dwExtraRawBytes != 0) {
						static const char padding[] = "PADDINGXXPADDING";
						DWORD dwRemain = optionalHeader->FileAlignment - dwExtraRawBytes;
						for(int i = 0; i < dwRemain; i++) {
							if(i < 2) {
								fputc(padding[i & 15], fo);
							} else {
								fputc(padding[(i - 2) & 15], fo);
							}
						}
						newSectionHeader->SizeOfRawData += dwRemain;
					}
					newSectionHeader->Misc.VirtualSize = newSectionHeader->SizeOfRawData;
					
					DWORD dwNewSectionSize = newSectionHeader->SizeOfRawData;
					DWORD dwExtraNewSectionBytes = dwNewSectionSize % optionalHeader->SectionAlignment;
					if(dwExtraNewSectionBytes != 0) {
						DWORD dwRemain = optionalHeader->SectionAlignment - dwExtraNewSectionBytes;
						dwNewSectionSize += dwRemain;
					}
					optionalHeader->SizeOfImage += dwNewSectionSize;
					
					fseek(fo, 0, SEEK_SET);
					fwrite(header, sizeof(header), 1, fo);
					
					fprintf(stderr, "'%s' is successfully created\n", new_exec_file);
					retval = EXIT_SUCCESS;
				}
			}
			if(fp != NULL) {
				fclose(fp);
			}
			if(fs != NULL) {
				fclose(fs);
			}
			if(fo != NULL) {
				fclose(fo);
			}
		}
#ifdef _DEBUG
		_CrtDumpMemoryLeaks();
#endif
		return(retval);
	}
	
	HANDLE hStdout = GetStdHandle(STD_OUTPUT_HANDLE);
	CONSOLE_SCREEN_BUFFER_INFO csbi;
	CONSOLE_CURSOR_INFO ci;
	CONSOLE_FONT_INFOEX fi;
	
	GetConsoleMode(GetStdHandle(STD_INPUT_HANDLE), &dwConsoleMode);
	
	get_console_buffer_success = (GetConsoleScreenBufferInfo(hStdout, &csbi) != 0);
	get_console_cursor_success = (GetConsoleCursorInfo(hStdout, &ci) != 0);
	get_console_font_success = get_console_font_info(&fi);
	
	ci_old = ci_new = ci;
	fi_new = fi;
	font_width  = fi.dwFontSize.X;
	font_height = fi.dwFontSize.Y;
	
	for(int y = 0; y < SCR_BUF_WIDTH; y++) {
		for(int x = 0; x < SCR_BUF_HEIGHT; x++) {
			SCR_BUF(y,x).Char.AsciiChar = ' ';
			SCR_BUF(y,x).Attributes = FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE;
		}
	}
	if(get_console_buffer_success) {
		scr_width = csbi.dwSize.X;
		scr_height = csbi.srWindow.Bottom - csbi.srWindow.Top + 1;
		
		// v-text shadow buffer size must be lesser than 0x7fd0
		if((scr_width > SCR_BUF_WIDTH) || (scr_height > SCR_BUF_HEIGHT) || (scr_width * scr_height * 2 > 0x7fd0) ||
		   (buf_width != 0 && buf_width != scr_width) || (buf_height != 0 && buf_height != scr_height)) {
			scr_width = min(buf_width != 0 ? buf_width : scr_width, SCR_BUF_WIDTH);
			scr_height = min(buf_height != 0 ? buf_height : scr_height, SCR_BUF_HEIGHT);
			if(scr_width * scr_height * 2 > 0x7fd0) {
				scr_width = 80;
				scr_height = 25;
			}
			screen_size_changed = true;
		}
	} else {
		// for a proof (not a console)
		scr_width = 80;
		scr_height = 25;
	}
	scr_buf_size.X = scr_width;
	scr_buf_size.Y = scr_height;
	scr_buf_pos.X = scr_buf_pos.Y = 0;
	scr_top = csbi.srWindow.Top;
	cursor_moved = false;
	cursor_moved_by_crtc = false;
	
	SetPriorityClass(GetCurrentProcess(), ABOVE_NORMAL_PRIORITY_CLASS);
	
#ifdef USE_SERVICE_THREAD
	InitializeCriticalSection(&input_crit_sect);
	InitializeCriticalSection(&key_buf_crit_sect);
	InitializeCriticalSection(&putch_crit_sect);
	main_thread_id = GetCurrentThreadId();
#endif
	
	key_buf_char = new FIFO(256);
	key_buf_scan = new FIFO(256);
	key_buf_data = new FIFO(256);
	
	hardware_init();
	
#ifdef USE_DEBUGGER
	debugger_init();
#endif
	
	if(msdos_init(argc - (arg_offset + 1), argv + (arg_offset + 1), envp, standard_env)) {
		retval = EXIT_FAILURE;
	} else {
#if defined(_MSC_VER) && (_MSC_VER >= 1400)
		_set_invalid_parameter_handler((_invalid_parameter_handler)ignore_invalid_parameters);
#endif
		SetConsoleCtrlHandler(ctrl_handler, TRUE);
		
		if(screen_size_changed) {
			change_console_size(scr_width, scr_height);
		}
		TIMECAPS caps;
		timeGetDevCaps(&caps, sizeof(TIMECAPS));
		timeBeginPeriod(caps.wPeriodMin);
#ifdef USE_VRAM_THREAD
		InitializeCriticalSection(&vram_crit_sect);
		CloseHandle(CreateThread(NULL, 4096, vram_thread, NULL, 0, NULL));
#endif
#ifdef USE_DEBUGGER
		CloseHandle(CreateThread(NULL, 0, debugger_thread, NULL, 0, NULL));
		// wait until telnet client starts and connects to me
		if(_access(debugger_get_ttermpro_path(), 0) == 0 ||
		   _access(debugger_get_ttermpro_x86_path(), 0) == 0 ||
		   _access(debugger_get_putty_path(), 0) == 0 ||
		   _access(debugger_get_putty_x86_path(), 0) == 0 ||
		   _access(debugger_get_telnet_path(), 0) == 0 ||
		   _access(debugger_get_telnet_x86_path(), 0) == 0) {
			for(int i = 0; i < 100 && cli_socket == 0; i++) {
				Sleep(100);
			}
		}
#endif
		hardware_run();
#ifdef USE_VRAM_THREAD
		vram_flush();
		DeleteCriticalSection(&vram_crit_sect);
#endif
		timeEndPeriod(caps.wPeriodMin);
		
		// hStdin/hStdout (and all handles) will be closed in msdos_finish()...
		hStdout = GetStdHandle(STD_OUTPUT_HANDLE);
		
		// restore console settings
		if(restore_multibyte_cp) {
			set_multibyte_code_page(multibyte_cp);
		}
		if(restore_input_cp) {
			set_input_code_page(input_cp);
		}
		if(restore_output_cp) {
			set_output_code_page(output_cp);
		}
		if(get_console_buffer_success) {
			if(restore_console_size) {
				// window can't be bigger than buffer,
				// buffer can't be smaller than window,
				// so make a tiny window,
				// set the required buffer,
				// then set the required window
				CONSOLE_SCREEN_BUFFER_INFO cur_csbi;
				SMALL_RECT rect;
				GetConsoleScreenBufferInfo(hStdout, &cur_csbi);
				int min_width  = min(cur_csbi.srWindow.Right - cur_csbi.srWindow.Left + 1, csbi.srWindow.Right - csbi.srWindow.Left + 1);
				int min_height = min(cur_csbi.srWindow.Bottom - cur_csbi.srWindow.Top + 1, csbi.srWindow.Bottom - csbi.srWindow.Top + 1);
				
				SET_RECT(rect, 0, cur_csbi.srWindow.Top, min_width - 1, cur_csbi.srWindow.Top + min_height - 1);
				SetConsoleWindowInfo(hStdout, TRUE, &rect);
				SetConsoleScreenBufferSize(hStdout, csbi.dwSize);
				SET_RECT(rect, 0, 0, csbi.srWindow.Right - csbi.srWindow.Left, csbi.srWindow.Bottom - csbi.srWindow.Top);
				if(!SetConsoleWindowInfo(hStdout, TRUE, &rect)) {
					SetWindowPos(get_console_window_handle(), NULL, 0, 0, 0, 0, SWP_NOSIZE | SWP_NOZORDER);
					SetConsoleWindowInfo(hStdout, TRUE, &rect);
				}
			}
		}
		if(get_console_font_success) {
			if(restore_console_font) {
				set_console_font_info(&fi);
			}
		}
		if(get_console_buffer_success) {
			if(restore_console_size) {
				SMALL_RECT rect;
				SetConsoleScreenBufferSize(hStdout, csbi.dwSize);
				SET_RECT(rect, 0, 0, csbi.srWindow.Right - csbi.srWindow.Left, csbi.srWindow.Bottom - csbi.srWindow.Top);
				if(!SetConsoleWindowInfo(hStdout, TRUE, &rect)) {
					SetWindowPos(get_console_window_handle(), NULL, 0, 0, 0, 0, SWP_NOSIZE | SWP_NOZORDER);
					SetConsoleWindowInfo(hStdout, TRUE, &rect);
				}
			}
			SetConsoleTextAttribute(hStdout, csbi.wAttributes);
		}
		if(get_console_cursor_success) {
			if(restore_console_cursor) {
				SetConsoleCursorInfo(hStdout, &ci);
			}
		}
		if(dwConsoleMode & (ENABLE_INSERT_MODE | ENABLE_QUICK_EDIT_MODE)) {
			SetConsoleMode(GetStdHandle(STD_INPUT_HANDLE), dwConsoleMode | ENABLE_EXTENDED_FLAGS);
		} else {
			SetConsoleMode(GetStdHandle(STD_INPUT_HANDLE), dwConsoleMode);
		}
		
		msdos_finish();
		
		SetConsoleCtrlHandler(ctrl_handler, FALSE);
	}
	if(temp_file_created) {
		DeleteFileA(temp_file_path);
		temp_file_created = false;
	}
	hardware_finish();
	
	if(key_buf_char != NULL) {
		key_buf_char->release();
		delete key_buf_char;
		key_buf_char = NULL;
	}
	if(key_buf_scan != NULL) {
		key_buf_scan->release();
		delete key_buf_scan;
		key_buf_scan = NULL;
	}
	if(key_buf_data != NULL) {
		key_buf_data->release();
		delete key_buf_data;
		key_buf_data = NULL;
	}
#ifdef USE_SERVICE_THREAD
	DeleteCriticalSection(&input_crit_sect);
	DeleteCriticalSection(&key_buf_crit_sect);
	DeleteCriticalSection(&putch_crit_sect);
#endif
#ifdef _DEBUG
	_CrtDumpMemoryLeaks();
#endif
	return(retval);
}

/* ----------------------------------------------------------------------------
	console
---------------------------------------------------------------------------- */

void change_console_size(int width, int height)
{
	HANDLE hStdout = GetStdHandle(STD_OUTPUT_HANDLE);
	CONSOLE_SCREEN_BUFFER_INFO csbi;
	SMALL_RECT rect;
	COORD co;
	
	if(is_win10_or_later && active_code_page == 932) {
		if(wcscmp(fi_new.FaceName, L"Terminal") != 0) {
			set_console_font_size(font_width, font_height);
		}
	}
	
	GetConsoleScreenBufferInfo(hStdout, &csbi);
	if(csbi.srWindow.Top != 0 || csbi.dwCursorPosition.Y > height - 1) {
		if(csbi.srWindow.Right - csbi.srWindow.Left + 1 == width && csbi.srWindow.Bottom - csbi.srWindow.Top + 1 == height) {
			ReadConsoleOutputA(hStdout, scr_buf, scr_buf_size, scr_buf_pos, &csbi.srWindow);
			SET_RECT(rect, 0, 0, width - 1, height - 1);
			WriteConsoleOutputA(hStdout, scr_buf, scr_buf_size, scr_buf_pos, &rect);
		} else if(csbi.dwCursorPosition.Y > height - 1) {
			SET_RECT(rect, 0, csbi.dwCursorPosition.Y - (height - 1), width - 1, csbi.dwCursorPosition.Y);
			ReadConsoleOutputA(hStdout, scr_buf, scr_buf_size, scr_buf_pos, &rect);
			SET_RECT(rect, 0, 0, width - 1, height - 1);
			WriteConsoleOutputA(hStdout, scr_buf, scr_buf_size, scr_buf_pos, &rect);
		}
	}
	if(csbi.dwCursorPosition.Y > height - 1) {
		co.X = csbi.dwCursorPosition.X;
		co.Y = min(height - 1, csbi.dwCursorPosition.Y - csbi.srWindow.Top);
		SetConsoleCursorPosition(hStdout, co);
		cursor_moved = true;
		cursor_moved_by_crtc = false;
	}
	
	// window can't be bigger than buffer,
	// buffer can't be smaller than window,
	// so make a tiny window,
	// set the required buffer,
	// then set the required window
	int min_width  = min(csbi.srWindow.Right - csbi.srWindow.Left + 1, width);
	int min_height = min(csbi.srWindow.Bottom - csbi.srWindow.Top + 1, height);
	
	SET_RECT(rect, 0, csbi.srWindow.Top, min_width - 1, csbi.srWindow.Top + min_height - 1);
	SetConsoleWindowInfo(hStdout, TRUE, &rect);
	co.X = width;
	co.Y = height;
	SetConsoleScreenBufferSize(hStdout, co);
	SET_RECT(rect, 0, 0, width - 1, height - 1);
	if(!SetConsoleWindowInfo(hStdout, TRUE, &rect)) {
		SetWindowPos(get_console_window_handle(), NULL, 0, 0, 0, 0, SWP_NOSIZE | SWP_NOZORDER);
		SetConsoleWindowInfo(hStdout, TRUE, &rect);
	}
	
	scr_width = scr_buf_size.X = width;
	scr_height = scr_buf_size.Y = height;
	scr_top = 0;
	
	clear_scr_buffer(FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE);
	
	int regen = min(scr_width * scr_height * 2, 0x8000);
	text_vram_end_address = text_vram_top_address + regen;
	shadow_buffer_end_address = shadow_buffer_top_address + regen;
	
	if(regen > 0x4000) {
		regen = 0x8000;
		vram_pages = 1;
	} else if(regen > 0x2000) {
		regen = 0x4000;
		vram_pages = 2;
	} else if(regen > 0x1000) {
		regen = 0x2000;
		vram_pages = 4;
	} else {
		regen = 0x1000;
		vram_pages = 8;
	}
	*(UINT16 *)(mem + 0x44a) = scr_width;
	*(UINT16 *)(mem + 0x44c) = regen;
	*(UINT8  *)(mem + 0x484) = scr_height - 1;
	
	mouse.min_position.x = 0;
	mouse.min_position.y = 0;
	mouse.max_position.x = 8 * (scr_width  - 1);
	mouse.max_position.y = 8 * (scr_height - 1);
	
	restore_console_size = true;
}

void clear_scr_buffer(WORD attr)
{
	for(int y = 0; y < scr_height; y++) {
		for(int x = 0; x < scr_width; x++) {
			SCR_BUF(y,x).Char.AsciiChar = ' ';
			SCR_BUF(y,x).Attributes = attr;
		}
	}
}

bool update_console_input()
{
#ifdef USE_SERVICE_THREAD
	EnterCriticalSection(&input_crit_sect);
#endif
	HANDLE hStdin = GetStdHandle(STD_INPUT_HANDLE);
	DWORD dwNumberOfEvents = 0;
	DWORD dwRead;
	INPUT_RECORD ir[16];
	CONSOLE_SCREEN_BUFFER_INFO csbi = {0};
	bool result = false;
	
	if(GetNumberOfConsoleInputEvents(hStdin, &dwNumberOfEvents) && dwNumberOfEvents != 0) {
		if(ReadConsoleInputA(hStdin, ir, 16, &dwRead)) {
			for(int i = 0; i < dwRead; i++) {
				if(ir[i].EventType & MOUSE_EVENT) {
					if(ir[i].Event.MouseEvent.dwEventFlags & MOUSE_MOVED) {
						if(mouse.hidden == 0 || (mouse.call_addr_ps2.dw && mouse.enabled_ps2)) {
							// NOTE: if restore_console_size, console is not scrolled
							if(!restore_console_size && csbi.srWindow.Bottom == 0) {
								GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE), &csbi);
							}
							// FIXME: character size is always 8x8 ???
							int x = 8 * (ir[i].Event.MouseEvent.dwMousePosition.X);
							int y = 8 * (ir[i].Event.MouseEvent.dwMousePosition.Y - csbi.srWindow.Top);
							
							if(mouse.position.x != x || mouse.position.y != y) {
								mouse.position.x = x;
								mouse.position.y = y;
								mouse.status |= 1;
								mouse.status_alt |= 1;
							}
						}
					} else if(ir[i].Event.MouseEvent.dwEventFlags == 0) {
						for(int j = 0; j < MAX_MOUSE_BUTTONS; j++) {
							static const DWORD bits[] = {
								FROM_LEFT_1ST_BUTTON_PRESSED,	// left
								RIGHTMOST_BUTTON_PRESSED,	// right
								FROM_LEFT_2ND_BUTTON_PRESSED,	// middle
							};
							bool prev_status = mouse.buttons[j].status;
							mouse.buttons[j].status = ((ir[i].Event.MouseEvent.dwButtonState & bits[j]) != 0);
							
							if(!prev_status && mouse.buttons[j].status) {
								mouse.buttons[j].pressed_times++;
								mouse.buttons[j].pressed_position.x = mouse.position.x;
								mouse.buttons[j].pressed_position.y = mouse.position.x;
								if(j < 2) {
									mouse.status_alt |= 2 << (j * 2);
								}
								mouse.status |= 2 << (j * 2);
							} else if(prev_status && !mouse.buttons[j].status) {
								mouse.buttons[j].released_times++;
								mouse.buttons[j].released_position.x = mouse.position.x;
								mouse.buttons[j].released_position.y = mouse.position.x;
								if(j < 2) {
									mouse.status_alt |= 4 << (j * 2);
								}
								mouse.status |= 4 << (j * 2);
							}
						}
					}
				} else if(ir[i].EventType & KEY_EVENT) {
					// update keyboard flags in bios data area
					if(ir[i].Event.KeyEvent.dwControlKeyState & CAPSLOCK_ON) {
						mem[0x417] |= 0x40;
					} else {
						mem[0x417] &= ~0x40;
					}
					if(ir[i].Event.KeyEvent.dwControlKeyState & NUMLOCK_ON) {
						mem[0x417] |= 0x20;
					} else {
						mem[0x417] &= ~0x20;
					}
					if(ir[i].Event.KeyEvent.dwControlKeyState & SCROLLLOCK_ON) {
						mem[0x417] |= 0x10;
					} else {
						mem[0x417] &= ~0x10;
					}
					if(ir[i].Event.KeyEvent.dwControlKeyState & (LEFT_ALT_PRESSED | RIGHT_ALT_PRESSED)) {
						if(mouse.buttons[0].status || mouse.buttons[1].status) {
							mouse.status_alt |= 0x80;
						}
						mem[0x417] |= 0x08;
					} else {
						mem[0x417] &= ~0x08;
					}
					if(ir[i].Event.KeyEvent.dwControlKeyState & (LEFT_CTRL_PRESSED | RIGHT_CTRL_PRESSED)) {
						if(mouse.buttons[0].status || mouse.buttons[1].status) {
							mouse.status_alt |= 0x40;
						}
						mem[0x417] |= 0x04;
					} else {
						mem[0x417] &= ~0x04;
					}
					if(ir[i].Event.KeyEvent.dwControlKeyState & SHIFT_PRESSED) {
						if(mouse.buttons[0].status || mouse.buttons[1].status) {
							mouse.status_alt |= 0x20;
						}
						if(!(mem[0x417] & 0x03)) {
							mem[0x417] |= 0x02; // left shift
						}
					} else {
						mem[0x417] &= ~0x03;
					}
					if(ir[i].Event.KeyEvent.dwControlKeyState & LEFT_ALT_PRESSED) {
						mem[0x418] |= 0x02;
					} else {
						mem[0x418] &= ~0x02;
					}
					if(ir[i].Event.KeyEvent.dwControlKeyState & LEFT_CTRL_PRESSED) {
						mem[0x418] |= 0x01;
					} else {
						mem[0x418] &= ~0x01;
					}
					
					// set scan code of last pressed/release key to kbd_data (in-port 60h)
//					kbd_data = ir[i].Event.KeyEvent.wVirtualScanCode;
//					kbd_status |= 1;
					UINT8 tmp_data = ir[i].Event.KeyEvent.wVirtualScanCode;
					
					// update dos key buffer
					UINT8 chr = ir[i].Event.KeyEvent.uChar.AsciiChar;
					UINT8 scn = ir[i].Event.KeyEvent.wVirtualScanCode & 0xff;
					UINT8 scn_old = scn;
					
					if(ir[i].Event.KeyEvent.bKeyDown) {
						// make
						tmp_data &= 0x7f;
						
						if(ir[i].Event.KeyEvent.dwControlKeyState & (LEFT_ALT_PRESSED | RIGHT_ALT_PRESSED)) {
							if(scn == 0x0e) {
								chr = 0x00; // Alt + Back
							} else if(scn == 0x35 && (ir[i].Event.KeyEvent.dwControlKeyState & ENHANCED_KEY)) {
								chr = 0x00; // Alt + [K] /
							}
						} else if(ir[i].Event.KeyEvent.dwControlKeyState & (LEFT_CTRL_PRESSED | RIGHT_CTRL_PRESSED | SHIFT_PRESSED)) {
							if(scn == 0x0f) {
								chr = 0x00; // Shift/Ctrl + Tab
							}
						}
						if(chr == 0x00) {
							if(ir[i].Event.KeyEvent.dwControlKeyState & (LEFT_ALT_PRESSED | RIGHT_ALT_PRESSED)) {
								if(scn == 0x35) {
									scn = 0xa4;		// Keypad /
								} else if(scn >= 0x3b && scn <= 0x44) {
									scn += 0x68 - 0x3b;	// F1 to F10
								} else if(scn == 0x57 || scn == 0x58) {
									scn += 0x8b - 0x57;	// F11 & F12
								} else if(scn >= 0x47 && scn <= 0x53) {
									scn += 0x97 - 0x47;	// Edit/Arrow clusters
								}
							} else if(ir[i].Event.KeyEvent.dwControlKeyState & (LEFT_CTRL_PRESSED | RIGHT_CTRL_PRESSED)) {
								if(scn == 0x07) {
									chr = 0x1e;	// Ctrl + ^
								} else if(scn == 0x0c) {
									chr = 0x1f;	// Ctrl + _
								} else if(scn == 0x0f) {
									scn = 0x94;	// Ctrl + Tab
								} else if(scn >= 0x35 && scn <= 0x58) {
									static const UINT8 ctrl_map[] = {
										0x95,	// Keypad /
										0,
										0x96,	// Keypad *
										0, 0, 0,
										0x5e,	// F1
										0x5f,	// F2
										0x60,	// F3
										0x61,	// F4
										0x62,	// F5
										0x63,	// F6
										0x64,	// F7
										0x65,	// F8
										0x66,	// F9
										0x67,	// F10
										0,
										0,
										0x77,	// Home
										0x8d,	// Up
										0x84,	// PgUp
										0x8e,	// keypad -
										0x73,	// Left
										0x8f,	// Keypad Center
										0x74,	// Right
										0x90,	// Keypad +
										0x75,	// End
										0x91,	// Down
										0x76,	// PgDn
										0x92,	// Insert
										0x93,	// Delete
										0, 0, 0,
										0x89,	// F11
										0x8a,	// F12
									};
									scn = ctrl_map[scn - 0x35];
								}
							} else if(ir[i].Event.KeyEvent.dwControlKeyState & SHIFT_PRESSED) {
								if(scn >= 0x3b && scn <= 0x44) {
									scn += 0x54 - 0x3b;	// F1 to F10
								} else if(scn == 0x57 || scn == 0x58) {
									scn += 0x87 - 0x57;	// F11 & F12
								}
							} else if(scn == 0x57 || scn == 0x58) {
								scn += 0x85 - 0x57;
							}
							// ignore shift, ctrl, alt, win and menu keys
							if(scn != 0x1d && scn != 0x2a && scn != 0x36 && scn != 0x38 && !(scn >= 0x5b && scn <= 0x5d && scn == scn_old)) {
								if(key_buf_char != NULL && key_buf_scan != NULL) {
#ifdef USE_SERVICE_THREAD
									EnterCriticalSection(&key_buf_crit_sect);
#endif
									if(chr == 0) {
										if(scn >= 0x78 && scn != 0x84) {
											pcbios_set_key_buffer(0x00, 0x00);
										} else {
											pcbios_set_key_buffer(0x00, ir[i].Event.KeyEvent.dwControlKeyState & ENHANCED_KEY ? 0xe0 : 0x00);
										}
									}
									pcbios_set_key_buffer(chr, scn);
#ifdef USE_SERVICE_THREAD
									LeaveCriticalSection(&key_buf_crit_sect);
#endif
								}
							}
						} else {
							if(ir[i].Event.KeyEvent.dwControlKeyState & (LEFT_ALT_PRESSED | RIGHT_ALT_PRESSED)) {
								if(scn >= 0x02 && scn <= 0x0e) {
									scn += 0x78 - 0x02;	// 1 to 0 - =
								}
								chr = 0;
							} else if(ir[i].Event.KeyEvent.dwControlKeyState & (LEFT_CTRL_PRESSED | RIGHT_CTRL_PRESSED)) {
								if(scn == 0x0e) {
									chr = 0x7f;	// Ctrl + Back
								} else if(scn == 0x1c) {
									chr = 0x0a;	// Ctrl + Enter
								}
							}
							if(ir[i].Event.KeyEvent.dwControlKeyState & ENHANCED_KEY) {
								if(scn == 0x1c || scn == 0x35) {
									scn = 0xe0;	// Keypad /, Enter
								}
							}
							if(key_buf_char != NULL && key_buf_scan != NULL) {
#ifdef USE_SERVICE_THREAD
								EnterCriticalSection(&key_buf_crit_sect);
#endif
								pcbios_set_key_buffer(chr, scn);
#ifdef USE_SERVICE_THREAD
								LeaveCriticalSection(&key_buf_crit_sect);
#endif
							}
						}
					} else {
						if(chr == 0x03 && (ir[i].Event.KeyEvent.dwControlKeyState & (LEFT_CTRL_PRESSED | RIGHT_CTRL_PRESSED))) {
							// Ctrl + Break, Ctrl + C
							if(scn == 0x46) {
								if(key_buf_char != NULL && key_buf_scan != NULL) {
#ifdef USE_SERVICE_THREAD
									EnterCriticalSection(&key_buf_crit_sect);
#endif
									pcbios_set_key_buffer(0x00, 0x00);
#ifdef USE_SERVICE_THREAD
									LeaveCriticalSection(&key_buf_crit_sect);
#endif
								}
								ctrl_break_pressed = true;
								mem[0x471] = 0x80;
								raise_int_1bh = true;
							} else {
								if(key_buf_char != NULL && key_buf_scan != NULL) {
#ifdef USE_SERVICE_THREAD
									EnterCriticalSection(&key_buf_crit_sect);
#endif
									pcbios_set_key_buffer(chr, scn);
#ifdef USE_SERVICE_THREAD
									LeaveCriticalSection(&key_buf_crit_sect);
#endif
								}
								ctrl_c_pressed = (scn == 0x2e);
							}
						}
						// break
						tmp_data |= 0x80;
					}
					if(!(kbd_status & 1)) {
						kbd_data = tmp_data;
						kbd_status |= 1;
					} else {
						if(key_buf_data != NULL) {
#ifdef USE_SERVICE_THREAD
							EnterCriticalSection(&key_buf_crit_sect);
#endif
							key_buf_data->write(tmp_data);
#ifdef USE_SERVICE_THREAD
							LeaveCriticalSection(&key_buf_crit_sect);
#endif
						}
					}
					result = key_changed = true;
					// IME may be on and it may causes screen scroll up and cursor position change
					cursor_moved = true;
				}
			}
		}
	}
#ifdef USE_SERVICE_THREAD
	LeaveCriticalSection(&input_crit_sect);
#endif
	return(result);
}

bool update_key_buffer()
{
	if(update_console_input()) {
		return(true);
	}
	if(key_buf_char != NULL && key_buf_scan != NULL) {
#ifdef USE_SERVICE_THREAD
		EnterCriticalSection(&key_buf_crit_sect);
#endif
		bool empty = pcbios_is_key_buffer_empty();
#ifdef USE_SERVICE_THREAD
		LeaveCriticalSection(&key_buf_crit_sect);
#endif
		if(!empty) return(true);
	}
	return(false);
}

/* ----------------------------------------------------------------------------
	MS-DOS virtual machine
---------------------------------------------------------------------------- */

static const struct {
	char *name;
	DWORD lcid;
	char *std;
	char *dlt;
} tz_table[] = {
	// https://science.ksc.nasa.gov/software/winvn/userguide/3_1_4.htm
//	0	GMT		Greenwich Mean Time		GMT0
	{"GMT Standard Time",			0x0809, "GMT", "BST"},		// (UTC+00:00) GB London (en-gb)
	{"GMT Standard Time",			0x1809, "GMT", "IST"},		// (UTC+00:00) IE Dublin (en-ie)
	{"GMT Standard Time",			0x0000, "WET", "WES"},		// (UTC+00:00) PT Lisbon
	{"Greenwich Standard Time",		0x0000, "GMT", "GST"},		// (UTC+00:00) IS Reykjavik
//	2	FST	FDT	Fernando De Noronha Std		FST2FDT
	{"Mid-Atlantic Standard Time",		0x0416, "FST", "FDT"},		// (UTC-02:00) BR Noronha (pt-br)
	{"UTC-02",				0x0416, "FST", "FDT"},		// (UTC-02:00) BR Noronha (pt-br)
//	3	BST		Brazil Standard Time		BST3
	{"Bahia Standard Time",			0x0000, "BST", "BDT"},		// (UTC-03:00) BR Bahia
	{"SA Eastern Standard Time",		0x0000, "BST", "BDT"},		// (UTC-03:00) BR Fortaleza
	{"Tocantins Standard Time",		0x0000, "BST", "BDT"},		// (UTC-03:00) BR Palmas
//	3	EST	EDT	Eastern Standard (Brazil)	EST3EDT
	{"E. South America Standard Time",	0x0000, "EST", "EDT"},		// (UTC-03:00) BR Sao Paulo
//	3	GST		Greenland Standard Time		GST3
	{"Greenland Standard Time",		0x0000, "GST", "GDT"},		// (UTC-03:00) GL Godthab
//	3:30	NST	NDT	Newfoundland Standard Time	NST3:30NDT
	{"Newfoundland Standard Time",		0x0000, "NST", "NDT"},		// (UTC-03:30) CA St.Johns
//	4	AST	ADT	Atlantic Standard Time		AST4ADT
	{"Atlantic Standard Time",		0x0000, "AST", "ADT"},		// (UTC-04:00) CA Halifax
//	4	WST	WDT	Western Standard (Brazil)	WST4WDT
	{"Central Brazilian Standard Time",	0x0000, "WST", "WDT"},		// (UTC-04:00) BR Cuiaba
	{"SA Western Standard Time",		0x0000, "WST", "WDT"},		// (UTC-04:00) BR Manaus
//	5	EST	EDT	Eastern Standard Time	 	EST5EDT
	{"Eastern Standard Time",		0x0000, "EST", "EDT"},		// (UTC-05:00) US New York
	{"Eastern Standard Time (Mexico)",	0x0000, "EST", "EDT"},		// (UTC-05:00) MX Cancun
	{"US Eastern Standard Time",		0x0000, "EST", "EDT"},		// (UTC-05:00) US Indianapolis
//	5	CST	CDT	Chile Standard Time		CST5CDT
	{"Pacific SA Standard Time",		0x0000, "CST", "CDT"},		// (UTC-04:00) CL Santiago
//	5	AST	ADT	Acre Standard Time		AST5ADT
	{"SA Pacific Standard Time",		0x0000, "AST", "ADT"},		// (UTC-05:00) BR Rio Branco
//	5	CST	CDT	Cuba Standard Time		CST5CDT
	{"Cuba Standard Time",			0x0000, "CST", "CDT"},		// (UTC-05:00) CU Havana
//	6	CST	CDT	Central Standard Time		CST6CDT
	{"Canada Central Standard Time",	0x0000, "CST", "CDT"},		// (UTC-06:00) CA Regina
	{"Central Standard Time",		0x0000, "CST", "CDT"},		// (UTC-06:00) US Chicago
	{"Central Standard Time (Mexico)",	0x0000, "CST", "CDT"},		// (UTC-06:00) MX Mexico City
//	6	EST	EDT	Easter Island Standard		EST6EDT
	{"Easter Island Standard Time",		0x0000, "EST", "EDT"},		// (UTC-06:00) CL Easter
//	7	MST	MDT	Mountain Standard Time		MST7MDT
	{"Mountain Standard Time",		0x0000, "MST", "MDT"},		// (UTC-07:00) US Denver
	{"Mountain Standard Time (Mexico)",	0x0000, "MST", "MDT"},		// (UTC-07:00) MX Chihuahua
	{"US Mountain Standard Time",		0x0000, "MST", "MDT"},		// (UTC-07:00) US Phoenix
//	8	PST	PDT	Pacific Standard Time		PST8PDT
	{"Pacific Standard Time",		0x0000, "PST", "PDT"},		// (UTC-08:00) US Los Angeles
	{"Pacific Standard Time (Mexico)",	0x0000, "PST", "PDT"},		// (UTC-08:00) MX Tijuana
//	9	AKS	AKD	Alaska Standard Time		AKS9AKD
//	9	YST	YDT	Yukon Standard Time		YST9YST
	{"Alaskan Standard Time",		0x0000, "AKS", "AKD"},		// (UTC-09:00) US Anchorage
//	10	HST	HDT	Hawaii Standard Time		HST10HDT
	{"Aleutian Standard Time",		0x0000, "HST", "HDT"},		// (UTC-10:00) US Aleutian
	{"Hawaiian Standard Time",		0x0000, "HST", "HDT"},		// (UTC-10:00) US Honolulu
//	11	SST		Samoa Standard Time		SST11
	{"Samoa Standard Time",			0x0000, "SST", "SDT"},		// (UTC-11:00) US Samoa
//	-12	NZS	NZD	New Zealand Standard Time	NZS-12NZD
	{"New Zealand Standard Time",		0x0000, "NZS", "NZD"},		// (UTC+12:00) NZ Auckland
//	-10	GST		Guam Standard Time		GST-10
	{"West Pacific Standard Time",		0x0000, "GST", "GDT"},		// (UTC+10:00) GU Guam
//	-10	EAS	EAD	Eastern Australian Standard	EAS-10EAD
	{"AUS Eastern Standard Time",		0x0000, "EAS", "EAD"},		// (UTC+10:00) AU Sydney
	{"E. Australia Standard Time",		0x0000, "EAS", "EAD"},		// (UTC+10:00) AU Brisbane
	{"Tasmania Standard Time",		0x0000, "EAS", "EAD"},		// (UTC+10:00) AU Hobart
//	-9:30	CAS	CAD	Central Australian Standard	CAS-9:30CAD
	{"AUS Central Standard Time",		0x0000, "CAS", "CAD"},		// (UTC+09:30) AU Darwin
	{"Cen. Australia Standard Time",	0x0000, "CAS", "CAD"},		// (UTC+09:30) AU Adelaide
//	-9	JST		Japan Standard Time		JST-9
	{"Tokyo Standard Time",			0x0000, "JST", "JDT"},		// (UTC+09:00) JP Tokyo
//	-9	KST	KDT	Korean Standard Time		KST-9KDT
	{"Korea Standard Time",			0x0000, "KST", "KDT"},		// (UTC+09:00) KR Seoul
	{"North Korea Standard Time",		0x0000, "KST", "KDT"},		// (UTC+09:00) KP Pyongyang
//	-8	HKT		Hong Kong Time			HKT-8
	{"China Standard Time",			0x0C04, "HKT", "HKS"},		// (UTC+08:00) HK Hong Kong (zh-hk)
//	-8	CCT		China Coast Time		CCT-8
	{"China Standard Time",			0x0000, "CCT", "CDT"},		// (UTC+08:00) CN Shanghai
	{"Taipei Standard Time",		0x0000, "CCT", "CDT"},		// (UTC+08:00) TW Taipei
//	-8	SST		Singapore Standard Time		SST-8
	{"Singapore Standard Time",		0x0000, "SST", "SDT"},		// (UTC+08:00) SG Singapore
//	-8	WAS	WAD	Western Australian Standard	WAS-8WAD
	{"Aus Central W. Standard Time",	0x0000, "WAS", "WAD"},		// (UTC+08:45) AU Eucla
	{"W. Australia Standard Time",		0x0000, "WAS", "WAD"},		// (UTC+08:00) AU Perth
//	-7:30	JT		Java Standard Time		JST-7:30
//	-7	NST		North Sumatra Time		NST-7
	{"SE Asia Standard Time",		0x0000, "NST", "NDT"},		// (UTC+07:00) ID Jakarta
//	-5:30	IST		Indian Standard Time		IST-5:30
	{"India Standard Time",			0x0000, "IST", "IDT"},		// (UTC+05:30) IN Calcutta
//	-3:30	IST	IDT	Iran Standard Time		IST-3:30IDT
	{"Iran Standard Time",			0x0000, "IST", "IDT"},		// (UTC+03:30) IR Tehran
//	-3	MSK	MSD	Moscow Winter Time		MSK-3MSD
	{"Belarus Standard Time",		0x0000, "MSK", "MSD"},		// (UTC+03:00) BY Minsk
	{"Russian Standard Time",		0x0000, "MSK", "MSD"},		// (UTC+03:00) RU Moscow
//	-2	EET		Eastern Europe Time		EET-2
	{"E. Europe Standard Time",		0x0000, "EET", "EES"},		// (UTC+02:00) MD Chisinau
	{"FLE Standard Time",			0x0000, "EET", "EES"},		// (UTC+02:00) UA Kiev
	{"GTB Standard Time",			0x0000, "EET", "EES"},		// (UTC+02:00) RO Bucharest
	{"Kaliningrad Standard Time",		0x0000, "EET", "EES"},		// (UTC+02:00) RU Kaliningrad
//	-2	IST	IDT	Israel Standard Time		IST-2IDT
	{"Israel Standard Time",		0x0000, "IST", "IDT"},		// (UTC+02:00) IL Jerusalem
//	-1	MEZ	MES	Middle European Time		MEZ-1MES
//	-1	SWT	SST	Swedish Winter Time		SWT-1SST
//	-1	FWT	FST	French Winter Time		FWT-1FST
//	-1	CET	CES	Central European Time		CET-1CES
	{"Central Europe Standard Time",	0x0000, "CET", "CES"},		// (UTC+01:00) HU Budapest
	{"Central European Standard Time",	0x0000, "CET", "CES"},		// (UTC+01:00) PL Warsaw
	{"Romance Standard Time",		0x0000, "CET", "CES"},		// (UTC+01:00) FR Paris
	{"W. Europe Standard Time",		0x0000, "CET", "CES"},		// (UTC+01:00) DE Berlin
//	-1	WAT	 	West African Time		WAT-1
	{"Namibia Standard Time",		0x0000, "WAT", "WAS"},		// (UTC+01:00) NA Windhoek
	{"W. Central Africa Standard Time",	0x0000, "WAT", "WAS"},		// (UTC+01:00) NG Lagos
//	0	UTC		Universal Coordinated Time	UTC0
	{"UTC",					0x0000, "UTC", ""   },		// (UTC+00:00) GMT+0
	{"UTC-02",				0x0000, "UTC", ""   },		// (UTC-02:00) GMT+2
	{"UTC-08",				0x0000, "UTC", ""   },		// (UTC-08:00) GMT+8
	{"UTC-09",				0x0000, "UTC", ""   },		// (UTC-09:00) GMT+9
	{"UTC-11",				0x0000, "UTC", ""   },		// (UTC-11:00) GMT+11
	{"UTC+12",				0x0000, "UTC", ""   },		// (UTC+12:00) GMT-12
};

// FIXME: consider to build on non-Japanese environment :-(
// message_japanese string must be in shift-jis

static const struct {
	UINT16 code;
	char *message_english;
	char *message_japanese;
} standard_error_table[] = {
	{0x01,	"Invalid function", "無効なファンクションです."},
	{0x02,	"File not found", "ファイルが見つかりません."},
	{0x03,	"Path not found", "パスが見つかりません."},
	{0x04,	"Too many open files", "開かれているファイルが多すぎます."},
	{0x05,	"Access denied", "アクセスは拒否されました."},
	{0x06,	"Invalid handle", "無効なハンドルです."},
	{0x07,	"Memory control blocks destroyed", "メモリ制御ブロックが破棄されました."},
	{0x08,	"Insufficient memory", "メモリが足りません."},
	{0x09,	"Invalid memory block address", "無効なメモリブロックのアドレスです."},
	{0x0A,	"Invalid Environment", "無効な環境です."},
	{0x0B,	"Invalid format", "無効なフォーマットです."},
	{0x0C,	"Invalid function parameter", "無効なファンクションパラメータです."},
	{0x0D,	"Invalid data", "無効なデータです."},
	{0x0F,	"Invalid drive specification", "無効なドライブの指定です."},
	{0x10,	"Attempt to remove current directory", "現在のディレクトリを削除しようとしました."},
	{0x11,	"Not same device", "同じデバイスではありません."},
	{0x12,	"No more files", "ファイルはこれ以上ありません."},
	{0x13,	"Write protect error", "書き込み保護エラーです."},
	{0x14,	"Invalid unit", "無効なユニットです."},
	{0x15,	"Not ready", "準備ができていません."},
	{0x16,	"Invalid device request", "無効なデバイス要求です."},
	{0x17,	"Data error", "データエラーです."},
	{0x18,	"Invalid device request parameters", "無効なデバイス要求のパラメータです."},
	{0x19,	"Seek error", "シークエラーです."},
	{0x1A,	"Invalid media type", "無効なメディアの種類です."},
	{0x1B,	"Sector not found", "セクタが見つかりません."},
	{0x1C,	"Printer out of paper error", "プリンタの用紙がありません."},
	{0x1D,	"Write fault error", "書き込みエラーです."},
	{0x1E,	"Read fault error", "読み取りエラーです."},
	{0x1F,	"General failure", "エラーです."},
	{0x20,	"Sharing violation", "共有違反です."},
	{0x21,	"Lock violation", "ロック違反です."},
	{0x22,	"Invalid disk change", "無効なディスクの交換です."},
	{0x23,	"FCB unavailable", "FCB は使えません."},
	{0x24,	"System resource exhausted", "システムリソースはもう使えません."},
	{0x25,	"Code page mismatch", "コード ページが一致しません."},
	{0x26,	"Out of input", "入力が終わりました."},
	{0x27,	"Insufficient disk space", "ディスクの空き領域が足りません."},
/*
	{0x32,	"Network request not supported", NULL},
	{0x33,	"Remote computer not listening", NULL},
	{0x34,	"Duplicate name on network", NULL},
	{0x35,	"Network name not found", NULL},
	{0x36,	"Network busy", NULL},
	{0x37,	"Network device no longer exists", NULL},
	{0x38,	"Network BIOS command limit exceeded", NULL},
	{0x39,	"Network adapter hardware error", NULL},
	{0x3A,	"Incorrect response from network", NULL},
	{0x3B,	"Unexpected network error", NULL},
	{0x3C,	"Incompatible remote adapter", NULL},
	{0x3D,	"Print queue full", NULL},
	{0x3E,	"Queue not full", NULL},
	{0x3F,	"Not enough space to print file", NULL},
	{0x40,	"Network name was deleted", NULL},
	{0x41,	"Network: Access denied", NULL},
	{0x42,	"Network device type incorrect", NULL},
	{0x43,	"Network name not found", NULL},
	{0x44,	"Network name limit exceeded", NULL},
	{0x45,	"Network BIOS session limit exceeded", NULL},
	{0x46,	"Temporarily paused", NULL},
	{0x47,	"Network request not accepted", NULL},
	{0x48,	"Network print/disk redirection paused", NULL},
	{0x49,	"Network software not installed", NULL},
	{0x4A,	"Unexpected adapter close", NULL},
*/
	{0x50,	"File exists", "ファイルは存在します."},
	{0x52,	"Cannot make directory entry", "ディレクトリエントリを作成できません."},
	{0x53,	"Fail on INT 24", "INT 24 で失敗しました."},
	{0x54,	"Too many redirections", "リダイレクトが多すぎます."},
	{0x55,	"Duplicate redirection", "リダイレクトが重複しています."},
	{0x56,	"Invalid password", "パスワードが違います."},
	{0x57,	"Invalid parameter", "パラメータの指定が違います."},
	{0x58,	"Network data fault", "ネットワークデータのエラーです."},
	{0x59,	"Function not supported by network", "ファンクションはネットワークでサポートされていません."},
	{0x5A,	"Required system component not installe", "必要なシステム コンポーネントが組み込まれていません."},
#ifdef SUPPORT_MSCDEX
	{0x64,	"Unknown error", "不明なエラーです."},
	{0x65,	"Not ready", "準備ができていません."},
	{0x66,	"EMS memory no longer valid", "EMS メモリはもう有効ではありません."},
	{0x67,	"CDROM not High Sierra or ISO-9660 format", "CDROM は High Sierra または ISO-9660 フォーマットではありません."},
	{0x68,	"Door open", "レバーが閉まっていません."
#endif
	{0xB0,	"Volume is not locked", "ボリュームがロックされていません."},
	{0xB1,	"Volume is locked in drive", "ボリュームがロックされています."},
	{0xB2,	"Volume is not removable", "ボリュームは取り外しできません."},
	{0xB4,	"Lock count has been exceeded", "ボリュームをこれ以上ロックできません."},
	{0xB5,	"A valid eject request failed", "取り出しに失敗しました."},
	{-1  ,	"Unknown error", "不明なエラーです."},
};

static const struct {
	UINT16 code;
	char *message_english;
	char *message_japanese;
} param_error_table[] = {
	{0x01,	"Too many parameters", "パラメータが多すぎます."},
	{0x02,	"Required parameter missing", "パラメータが足りません."},
	{0x03,	"Invalid switch", "無効なスイッチです."},
	{0x04,	"Invalid keyword", "無効なキーワードです."},
	{0x06,	"Parameter value not in allowed range", "パラメータの値が範囲を超えています."},
	{0x07,	"Parameter value not allowed", "そのパラメータの値は使えません."},
	{0x08,	"Parameter value not allowed", "そのパラメータの値は使えません."},
	{0x09,	"Parameter format not correct", "パラメータの書式が違います."},
	{0x0A,	"Invalid parameter", "無効なパラメータです."},
	{0x0B,	"Invalid parameter combination", "無効なパラメータの組み合わせです."},
	{-1  ,	"Unknown error", "不明なエラーです."},
};

static const struct {
	UINT16 code;
	char *message_english;
	char *message_japanese;
} critical_error_table[] = {
	{0x00,	"Write protect error", "書き込み保護エラーです."},
	{0x01,	"Invalid unit", "無効なユニットです."},
	{0x02,	"Not ready", "準備ができていません."},
	{0x03,	"Invalid device request", "無効なデバイス要求です."},
	{0x04,	"Data error", "データエラーです."},
	{0x05,	"Invalid device request parameters", "無効なデバイス要求のパラメータです."},
	{0x06,	"Seek error", "シークエラーです."},
	{0x07,	"Invalid media type", "無効なメディアの種類です."},
	{0x08,	"Sector not found", "セクタが見つかりません."},
	{0x09,	"Printer out of paper error", "プリンタの用紙がありません."},
	{0x0A,	"Write fault error", "書き込みエラーです."},
	{0x0B,	"Read fault error", "読み取りエラーです."},
	{0x0C,	"General failure", "エラーです."},
	{0x0D,	"Sharing violation", "共有違反です."},
	{0x0E,	"Lock violation", "ロック違反です."},
	{0x0F,	"Invalid disk change", "無効なディスクの交換です."},
	{0x10,	"FCB unavailable", "FCB は使えません."},
	{0x11,	"System resource exhausted", "システムリソースはもう使えません."},
	{0x12,	"Code page mismatch", "コード ページが一致しません."},
	{0x13,	"Out of input", "入力が終わりました."},
	{0x14,	"Insufficient disk space", "ディスクの空き領域が足りません."},
	{-1  ,	"Critical error", "致命的なエラーです."},
};

void msdos_psp_set_file_table(int fd, UINT8 value, int psp_seg);
int msdos_psp_get_file_table(int fd, int psp_seg);
void msdos_putch(UINT8 data, unsigned int_num, UINT8 reg_ah);
void msdos_putch_fast(UINT8 data, unsigned int_num, UINT8 reg_ah);
#ifdef USE_SERVICE_THREAD
void msdos_putch_tmp(UINT8 data, unsigned int_num, UINT8 reg_ah);
#endif
const char *msdos_short_path(const char *path);
const char *msdos_short_full_dir(const char *path);
bool msdos_is_valid_drive(int drv);
bool msdos_is_removable_drive(int drv);
bool msdos_is_cdrom_drive(int drv);
bool msdos_is_remote_drive(int drv);
bool msdos_is_subst_drive(int drv);

// process info

process_t *msdos_process_info_create(UINT16 psp_seg, const char *path)
{
	for(int i = 0; i < MAX_PROCESS; i++) {
		if(process[i].psp == 0 || process[i].psp == psp_seg) {
			memset(&process[i], 0, sizeof(process_t));
			process[i].psp = psp_seg;

			strcpy(process[i].module_dir, msdos_short_full_dir(path));
//#ifdef USE_DEBUGGER
			strcpy(process[i].module_path, path);
//#endif
			process[i].dta.w.l = 0x80;
			process[i].dta.w.h = psp_seg;
			process[i].switchar = '/';
			process[i].max_files = 20;
			process[i].parent_int_10h_feh_called = int_10h_feh_called;
			process[i].parent_int_10h_ffh_called = int_10h_ffh_called;
			process[i].parent_ds = CPU_DS;
			process[i].parent_es = CPU_ES;

			return(&process[i]);
		}
	}
	fatalerror("too many processes\n");
	return(NULL);
}

process_t *msdos_process_info_get(UINT16 psp_seg, bool show_error = true)
{
	for(int i = 0; i < MAX_PROCESS; i++) {
		if(process[i].psp == psp_seg) {
			return(&process[i]);
		}
	}
	if(show_error) {
		fatalerror("invalid psp address\n");
	}
	return(NULL);
}

void msdos_sda_update(int psp_seg)
{
	sda_t *sda = (sda_t *)(mem + SDA_TOP);
	
	for(int i = 0; i < MAX_PROCESS; i++) {
		if(process[i].psp == psp_seg) {
			sda->switchar = process[i].switchar;
			sda->current_dta.w.l = process[i].dta.w.l;
			sda->current_dta.w.h = process[i].dta.w.h;
			sda->current_psp = process[i].psp;
			break;
		}
	}
	sda->malloc_strategy = malloc_strategy;
	sda->return_code = retval;
	sda->current_drive = _getdrive();
}

// dta info

void msdos_dta_info_init()
{
	for(int i = 0; i < MAX_DTAINFO; i++) {
		dtalist[i].find_handle = INVALID_HANDLE_VALUE;
	}
}

static bool msdos_dta_info_clean()
{
	find_t *dta;
	bool ret = false;
	for(int i = 0; i < MAX_DTAINFO; i++) {
		if(dtalist[i].dta > EMB_TOP) {
			ret = true;
			FindClose(dtalist[i].find_handle);
			dtalist[i].find_handle = INVALID_HANDLE_VALUE;
		} else {
			dta = (find_t *)(mem + dtalist[i].dta);
			if(dta->find_magic != FIND_MAGIC) {
				ret = true;
				FindClose(dtalist[i].find_handle);
				dtalist[i].find_handle = INVALID_HANDLE_VALUE;
			}
		}
	}
	return ret;
}

dtainfo_t *msdos_dta_info_get(UINT16 psp_seg, UINT32 dta_laddr)
{
	dtainfo_t *free_dta = NULL;
	for(int i = 0; i < MAX_DTAINFO; i++) {
		if(dtalist[i].find_handle == INVALID_HANDLE_VALUE) {
			if(free_dta == NULL) {
				free_dta = &dtalist[i];
			}
		} else if(dta_laddr < LFN_DTA_LADDR && dtalist[i].dta == dta_laddr) {
			return(&dtalist[i]);
		}
	}
	if(free_dta) {
		free_dta->psp = psp_seg;
		free_dta->dta = dta_laddr;
		return(free_dta);
	}
	if(msdos_dta_info_clean()) {
		return msdos_dta_info_get(psp_seg, dta_laddr);
	}
	fatalerror("too many dta\n");
	return(NULL);
}

void msdos_dta_info_free(UINT16 psp_seg)
{
	for(int i = 0; i < MAX_DTAINFO; i++) {
		if(dtalist[i].psp == psp_seg && dtalist[i].find_handle != INVALID_HANDLE_VALUE) {
			FindClose(dtalist[i].find_handle);
			dtalist[i].find_handle = INVALID_HANDLE_VALUE;
		}
	}
}

void msdos_cds_update(int drv)
{
	cds_t *cds = (cds_t *)(mem + CDS_TOP + 88 * drv);
	
	memset(cds, 0, 88);
	
	if(msdos_is_valid_drive(drv)) {
		char path[MAX_PATH];
		if(msdos_is_remote_drive(drv)) {
			cds->drive_attrib = 0xc000;	// network drive
		} else if(msdos_is_subst_drive(drv)) {
			cds->drive_attrib = 0x5000;	// subst drive
		} else {
			cds->drive_attrib = 0x4000;	// physical drive
		}
		if(_getdcwd(drv + 1, path, MAX_PATH) != NULL) {
			strcpy_s(cds->path_name, sizeof(cds->path_name), msdos_short_path(path));
		}
	}
	if(cds->path_name[0] == '\0') {
		sprintf(cds->path_name, "%c:\\", 'A' + drv);
	}
	cds->dpb_ptr.w.h = DPB_TOP >> 4;
	cds->dpb_ptr.w.l = sizeof(dpb_t) * drv;
	cds->word_1 = cds->word_2 = 0xffff;
	cds->word_3 = 0xffff; // stored user data from INT 21/AX=5F03h if this is network drive
	cds->bs_offset = 2;
}

void msdos_cds_update(int drv, const char *path)
{
	cds_t *cds = (cds_t *)(mem + CDS_TOP + 88 * drv);
	
	strcpy_s(cds->path_name, sizeof(cds->path_name), msdos_short_path(path));
}

// nls information tables

// uppercase table (func 6502h)
void msdos_upper_table_update()
{
	*(UINT16 *)(mem + UPPERTABLE_TOP) = 0x80;
	for(unsigned i = 0; i < 0x80; ++i) {
		UINT8 c[4];
		*(UINT32 *)c = 0;		// reset internal conversion state
		CharUpperBuffA((LPSTR)c, 4);	// (workaround for MBCS codepage)
		c[0] = 0x80 + i;
		DWORD rc = CharUpperBuffA((LPSTR)c, 1);
		mem[UPPERTABLE_TOP + 2 + i] = (rc == 1 && c[0]) ? c[0] : 0x80 + i;
	}
}

// lowercase table (func 6503h)
void msdos_lower_table_update()
{
	*(UINT16 *)(mem + LOWERTABLE_TOP) = 0x80;
	for(unsigned i = 0; i < 0x80; ++i) {
		UINT8 c[4];
		*(UINT32 *)c = 0;		// reset internal conversion state
		CharLowerBuffA((LPSTR)c, 4);	// (workaround for MBCS codepage)
		c[0] = 0x80 + i;
		DWORD rc = CharLowerBuffA((LPSTR)c, 1);
		mem[LOWERTABLE_TOP + 2 + i] = (rc == 1 && c[0]) ? c[0] : 0x80 + i;
	}
}

// filename uppercase table (func 6504h)
void msdos_filename_upper_table_init()
{
	// depended on (file)system, not on active codepage
	// temporary solution: just filling data
	*(UINT16 *)(mem + FILENAME_UPPERTABLE_TOP) = 0x80;
	for(unsigned i = 0; i < 0x80; ++i) {
		mem[FILENAME_UPPERTABLE_TOP + 2 + i] = 0x80 + i;
	}
}

// filaname terminator table (func 6505h)
void msdos_filename_terminator_table_init()
{
	const char illegal_chars[] = ".\"/\\[]:|<>+=;,";	// for standard MS-DOS fs.
	UINT8 *data = mem + FILENAME_TERMINATOR_TOP;
	
	data[2] = 1;		// marker? (permissible character value)
	data[3] = 0x00;		// 00h...FFh
	data[4] = 0xff;
	data[5] = 0;		// marker? (excluded character)
	data[6] = 0x00;		// 00h...20h
	data[7] = 0x20;
	data[8] = 2;		// marker? (illegal characters for filename)
	data[9] = (UINT8)strlen(illegal_chars);
	memcpy(data + 10, illegal_chars, data[9]);
	
	// total length
	*(UINT16 *)data = (10 - 2) + data[9];
}

// collating table (func 6506h)
void msdos_collating_table_update()
{
	// temporary solution: just filling data
	*(UINT16 *)(mem + COLLATING_TABLE_TOP) = 0x100;
	for(unsigned i = 0; i < 256; ++i) {
		mem[COLLATING_TABLE_TOP + 2 + i] = i;
	}
}

// dbcs

void msdos_dbcs_table_update()
{
	UINT8 dbcs_data[DBCS_SIZE];
	memset(dbcs_data, 0, sizeof(dbcs_data));
	
	CPINFO info;
	GetCPInfo(active_code_page, &info);
	
	if(info.MaxCharSize != 1) {
		for(int i = 0;; i += 2) {
			UINT8 lo = info.LeadByte[i + 0];
			UINT8 hi = info.LeadByte[i + 1];
			dbcs_data[2 + i + 0] = lo;
			dbcs_data[2 + i + 1] = hi;
			if(lo == 0 && hi == 0) {
				dbcs_data[0] = i + 2;
				break;
			}
		}
	} else {
		dbcs_data[0] = 2;	// ???
	}
	memcpy(mem + DBCS_TOP, dbcs_data, sizeof(dbcs_data));
}

void msdos_dbcs_table_finish()
{
	// we will restore code pages later in main(), so we don't need to do here :-)
#if 0
	if(system_code_page != get_multibyte_code_page()) {
		set_multibyte_code_page(system_code_page);
	}
	if(console_code_page != get_input_code_page()) {
		set_input_code_page(console_code_page);
		set_output_code_page(console_code_page);
	}
#endif
}

void msdos_nls_tables_init()
{
	active_code_page = console_code_page = get_input_code_page();
	system_code_page = get_multibyte_code_page();
	
	if(active_code_page != system_code_page) {
		if(set_multibyte_code_page(active_code_page) != 0) {
			active_code_page = system_code_page;
		}
	}
	
	msdos_upper_table_update();
	msdos_lower_table_update();
	msdos_filename_terminator_table_init();
	msdos_filename_upper_table_init();
	msdos_collating_table_update();
	msdos_dbcs_table_update();
}

void msdos_nls_tables_update()
{
	msdos_dbcs_table_update();
	msdos_upper_table_update();
	msdos_lower_table_update();
//	msdos_collating_table_update();
}

int msdos_lead_byte_check(UINT8 code)
{
	UINT8 *dbcs_table = mem + DBCS_TABLE;
	
	for(int i = 0;; i += 2) {
		UINT8 lo = dbcs_table[i + 0];
		UINT8 hi = dbcs_table[i + 1];
		if(lo == 0 && hi == 0) {
			break;
		}
		if(lo <= code && code <= hi) {
			return(1);
		}
	}
	return(0);
}

int msdos_ctrl_code_check(UINT8 code, unsigned int_num, UINT8 reg_ah)
{
//	if(active_code_page != 437) {
		if(int_num == 0x21 && (reg_ah == 0x0a || reg_ah == 0x3f)) {
			return (code >= 0x01 && code <= 0x1a && code != 0x07 && code != 0x08 && code != 0x09 && code != 0x0a && code != 0x0d);
		}
//	}
	return 0;
}

int msdos_symbol_code_check(UINT8 code, unsigned int_num, UINT8 reg_ah)
{
/*
 note for tables:
   --  means outpus as normal symbol (show AS IS)
   ++  means that is control/special symbol

                           ----------------------------------------------
     MSDOS 6.20            |                ASCII codes                 |
     WinXP NTVDM           | \0 | \a | \b | \t | \n | \r | ^Z | ^[ |    |
                           |0x00|0x07|0x08|0x09|0x0A|0x0D|0x1A|0x1B|0xFF|
 ------------------------------------------------------------------------
 | Fn 02h Int 21h          | ?  | ++ | ++ | ++ | ++ | ++ | -- | -- | -- |
 | Fn 06h Int 21h          | ?  | ++ | ++ | -- | ++ | ++ | -- | -- | ++ |
 | Fn 40h Int 21h          | ?  | ++ | ++ | ++ | ++ | ++ | ++ | -- | -- |
 ------------------------------------------------------------------------
 | Int 29h                 | ?  | ++ | ++ | -- | ++ | ++ | -- | -- | -- |
 ------------------------------------------------------------------------
 | Fn 0Ah Int 10h (BIOS)   | -- | -- | -- | -- | -- | -- | -- | -- | -- |
 ------------------------------------------------------------------------
*/
//	if(active_code_page == 437) {
		if(int_num == 0x21) {
			if(reg_ah == 0x02) {
				return (code == 0x1a || code == 0x1b || code == 0xff);
			} else if(reg_ah == 0x06) {
				return (code == 0x09 || code == 0x1a || code == 0x1b);
			} else if(reg_ah == 0x40) {
				return (code == 0x1b || code == 0xff);
			}
		} else if(int_num == 0x29) {
			return (code == 0x09 || code == 0x1a || code == 0x1b || code == 0xff);
		}
//	}
	return 0;
}

int msdos_kanji_2nd_byte_check(UINT8 *buf, int n)
{
	int is_kanji_1st = 0;
	int is_kanji_2nd = 0;
	
	for(int p = 0;; p++) {
		if(is_kanji_1st) {
			is_kanji_1st = 0;
			is_kanji_2nd = 1;
		} else if(msdos_lead_byte_check(buf[p])) {
			is_kanji_1st = 1;
		}
		if(p == n) {
			return(is_kanji_2nd);
		}
		is_kanji_2nd = 0;
	}
}

// file control

const char *msdos_remove_double_quote(const char *path)
{
	static char tmp[MAX_PATH];
	
	if(strlen(path) >= 2 && path[0] == '"' && path[strlen(path) - 1] == '"') {
//		memset(tmp, 0, sizeof(tmp));
//		memcpy(tmp, path + 1, strlen(path) - 2);
		strcpy(tmp, path + 1);
		tmp[strlen(tmp) - 1] = '\0';
	} else {
		strcpy(tmp, path);
	}
	return(tmp);
}

const char *msdos_remove_end_separator(const char *path)
{
	static char tmp[MAX_PATH];
	
	strcpy(tmp, path);
	
	// for example "C:\" case, the end separator should not be removed
	if(strlen(tmp) > 3 && tmp[strlen(tmp) - 1] == '\\') {
		tmp[strlen(tmp) - 1] = '\0';
	}
	return(tmp);
}

const char *msdos_combine_path(const char *dir, const char *file)
{
	static char tmp[MAX_PATH];
	const char *tmp_dir = msdos_remove_double_quote(dir);
	
	if(strlen(tmp_dir) == 0) {
		strcpy(tmp, file);
	} else if(tmp_dir[strlen(tmp_dir) - 1] == '\\') {
		sprintf(tmp, "%s%s", tmp_dir, file);
	} else {
		sprintf(tmp, "%s\\%s", tmp_dir, file);
	}
	return(tmp);
}

const char *msdos_trimmed_path(const char *path, int lfn = 0, int dir = 0);

const char *msdos_trimmed_path(const char *path, int lfn, int dir)
{
	static char tmp[MAX_PATH];
	
	if(lfn) {
		strcpy(tmp, path);
	} else {
		// remove space in the path
		const char *src = path;
		char *dst = tmp;
		
		while(*src != '\0') {
			if(msdos_lead_byte_check(*src)) {
				*dst++ = *src++;
				*dst++ = *src++;
			} else if(*src != ' ') {
				*dst++ = *src++;
			} else {
				src++;	// skip space
			}
		}
		*dst = '\0';
	}
	if(_stricmp(tmp, "C:\\COMMAND.COM") == 0) {
		// redirect C:\COMMAND.COM to comspec_path
		strcpy(tmp, comspec_path);
	} else if(is_vista_or_later && _access(tmp, 0) != 0 && !dir) {
		// redirect new files (without wildcards) in C:\ to %TEMP%, since C:\ is not usually writable
		static int root_drive_protected = -1;
		char temp[MAX_PATH], name[MAX_PATH], *name_temp = NULL;
		dos_info_t *dos_info = (dos_info_t *)(mem + DOS_INFO_TOP);
		
		if(GetFullPathNameA(tmp, MAX_PATH, temp, &name_temp) != 0 &&
		   name_temp != NULL && strstr(name_temp, "?") == NULL && strstr(name_temp, "*") == NULL) {
			strcpy(name, name_temp);
			name_temp[0] = '\0';
			
			if((temp[0] == 'A' + dos_info->boot_drive - 1 || temp[0] == 'a' + dos_info->boot_drive - 1) &&
			   (temp[1] == ':') && (temp[2] == '\\' || temp[2] == '/') && (temp[3] == '\0')) {
				if(root_drive_protected == -1) {
					FILE *fp = NULL;
					
					sprintf(temp, "%c:\\MS-DOS_Player.$$$", 'A' + dos_info->boot_drive - 1);
					root_drive_protected = 1;
					try {
						if((fp = fopen(temp, "w")) != NULL) {
							if(fprintf(fp, "TEST") == 4) {
								root_drive_protected = 0;
							}
						}
					} catch(...) {
					}
					if(fp != NULL) {
						fclose(fp);
					}
					if(_access(temp, 0) == 0) {
						remove(temp);
					}
				}
				if(root_drive_protected == 1) {
					if(GetEnvironmentVariableA("TEMP", temp, MAX_PATH) != 0 ||
					   GetEnvironmentVariableA("TMP",  temp, MAX_PATH) != 0) {
						strcpy(tmp, msdos_combine_path(temp, name));
					}
				}
			}
		}
	}
	return(tmp);
}

const char *msdos_get_multiple_short_path(const char *src)
{
	// "LONGPATH\";"LONGPATH\";"LONGPATH\" to SHORTPATH;SHORTPATH;SHORTPATH
	static char env_path[ENV_SIZE];
	char tmp[ENV_SIZE], *token;
	
	memset(env_path, 0, sizeof(env_path));
	strcpy(tmp, src);
	token = my_strtok(tmp, ";");
	
	while(token != NULL) {
		if(token[0] != '\0') {
			const char *path = msdos_remove_double_quote(token);
			char short_path[MAX_PATH];
			if(path != NULL && strlen(path) != 0) {
				if(env_path[0] != '\0') {
					strcat(env_path, ";");
				}
				if(GetShortPathNameA(path, short_path, MAX_PATH) == 0) {
					strcat(env_path, msdos_remove_end_separator(path));
				} else {
					my_strupr(short_path);
					strcat(env_path, msdos_remove_end_separator(short_path));
				}
			}
		}
		token = my_strtok(NULL, ";");
	}
	return(env_path);
}

bool match(const char *text, const char *pattern)
{
	// http://www.prefield.com/algorithm/string/wildcard.html
	switch(*pattern) {
	case '\0':
		return !*text;
	case '*':
		return match(text, pattern + 1) || (*text && match(text + 1, pattern));
	case '?':
		return *text && match(text + 1, pattern + 1);
	default:
		return (*text == *pattern) && match(text + 1, pattern + 1);
	}
}

bool msdos_match_volume_label(const char *path, const char *volume)
{
	const char *p = NULL;
	
	if(!*volume) {
		return false;
	} else if((p = my_strchr(path, ':')) != NULL) {
		return msdos_match_volume_label(p + 1, volume);
	} else if((p = my_strchr(path, '\\')) != NULL) {
		return msdos_match_volume_label(p + 1, volume);
	} else if((p = my_strchr(path, '.')) != NULL) {
		char tmp[MAX_PATH];
		sprintf(tmp, "%.*s%s", (int)(p - path), path, p + 1);
		return match(volume, tmp);
	} else {
		return match(volume, path);
	}
}

const char *msdos_fcb_path(fcb_t *fcb)
{
	static char tmp[MAX_PATH];
	char name[9], ext[4];
	
	memset(name, 0, sizeof(name));
	memcpy(name, fcb->file_name, 8);
	strcpy(name, msdos_trimmed_path(name));
	
	memset(ext, 0, sizeof(ext));
	memcpy(ext, fcb->file_name + 8, 3);
	strcpy(ext, msdos_trimmed_path(ext));
	
	if(name[0] == '\0' || strcmp(name, "????????") == 0) {
		strcpy(name, "*");
	}
	if(ext[0] == '\0') {
		strcpy(tmp, name);
	} else {
		if(strcmp(ext, "???") == 0) {
			strcpy(ext, "*");
		}
		sprintf(tmp, "%s.%s", name, ext);
	}
	return(tmp);
}

void msdos_set_fcb_path(fcb_t *fcb, const char *path)
{
	char tmp[MAX_PATH];
	strcpy(tmp, path);
	char *ext = my_strchr(tmp, '.');
	
	memset(fcb->file_name, 0x20, 8 + 3);
	if(ext != NULL && tmp[0] != '.') {
		*ext = '\0';
		memcpy(fcb->file_name + 8, ext + 1, strlen(ext + 1));
	}
	memcpy(fcb->file_name, tmp, strlen(tmp));
}

const char *msdos_short_path(const char *path)
{
	static char tmp[MAX_PATH];
	
	if(GetShortPathNameA(path, tmp, MAX_PATH) == 0) {
		strcpy(tmp, path);
	}
	my_strupr(tmp);
	return(tmp);
}

const char *msdos_short_name(WIN32_FIND_DATAA *fd)
{
	static char tmp[MAX_PATH];
	
	if(fd->cAlternateFileName[0]) {
		strcpy(tmp, fd->cAlternateFileName);
	} else {
		strcpy(tmp, fd->cFileName);
	}
	my_strupr(tmp);
	return(tmp);
}

const char *msdos_short_full_path(const char *path)
{
	static char tmp[MAX_PATH];
	char full[MAX_PATH], *name;
	
	// Full works with non-existent files, but Short does not
	GetFullPathNameA(path, MAX_PATH, full, &name);
	*tmp = '\0';
	if(GetShortPathNameA(full, tmp, MAX_PATH) == 0 && name > path) {
		name[-1] = '\0';
		DWORD len = GetShortPathNameA(full, tmp, MAX_PATH);
		if(len == 0) {
			strcpy(tmp, full);
		} else {
			tmp[len++] = '\\';
			strcpy(tmp + len, name);
		}
	}
	my_strupr(tmp);
	return(tmp);
}

const char *msdos_short_full_dir(const char *path)
{
	static char tmp[MAX_PATH];
	char full[MAX_PATH], *name;
	
	GetFullPathNameA(path, MAX_PATH, full, &name);
	name[-1] = '\0';
	if(GetShortPathNameA(full, tmp, MAX_PATH) == 0) {
		strcpy(tmp, full);
	}
	my_strupr(tmp);
	return(tmp);
}

const char *msdos_local_file_path(const char *path, int lfn)
{
	static char trimmed[MAX_PATH];
	
	strcpy(trimmed, msdos_trimmed_path(path, lfn));
#if 0
	// I have forgotten the reason of this routine... :-(
	if(_access(trimmed, 0) != 0) {
		process_t *process = msdos_process_info_get(current_psp);
		static char tmp[MAX_PATH];
		
		sprintf(tmp, "%s\\%s", process->module_dir, trimmed);
		if(_access(tmp, 0) == 0) {
			return(tmp);
		}
	}
#endif
	return(trimmed);
}

bool msdos_is_device_path(const char *path)
{
	char full[MAX_PATH], *name;
	
	if(GetFullPathNameA(path, MAX_PATH, full, &name) != 0) {
		if(_stricmp(full, "\\\\.\\AUX" ) == 0 ||
		   _stricmp(full, "\\\\.\\CON" ) == 0 ||
		   _stricmp(full, "\\\\.\\NUL" ) == 0 ||
		   _stricmp(full, "\\\\.\\PRN" ) == 0 ||
		   _stricmp(full, "\\\\.\\COM1") == 0 ||
		   _stricmp(full, "\\\\.\\COM2") == 0 ||
		   _stricmp(full, "\\\\.\\COM3") == 0 ||
		   _stricmp(full, "\\\\.\\COM4") == 0 ||
		   _stricmp(full, "\\\\.\\COM5") == 0 ||
		   _stricmp(full, "\\\\.\\COM6") == 0 ||
		   _stricmp(full, "\\\\.\\COM7") == 0 ||
		   _stricmp(full, "\\\\.\\COM8") == 0 ||
		   _stricmp(full, "\\\\.\\COM9") == 0 ||
		   _stricmp(full, "\\\\.\\LPT1") == 0 ||
		   _stricmp(full, "\\\\.\\LPT2") == 0 ||
		   _stricmp(full, "\\\\.\\LPT3") == 0 ||
		   _stricmp(full, "\\\\.\\LPT4") == 0 ||
		   _stricmp(full, "\\\\.\\LPT5") == 0 ||
		   _stricmp(full, "\\\\.\\LPT6") == 0 ||
		   _stricmp(full, "\\\\.\\LPT7") == 0 ||
		   _stricmp(full, "\\\\.\\LPT8") == 0 ||
		   _stricmp(full, "\\\\.\\LPT9") == 0) {
			return(true);
		} else if(name != NULL) {
			if(_stricmp(name, "CLOCK$"  ) == 0 ||
			   _stricmp(name, "CONFIG$" ) == 0 ||
			   _stricmp(name, "EMMXXXX0") == 0 ||
//			   _stricmp(name, "SCSIMGR$") == 0 ||
			   _stricmp(name, "$IBMAIAS") == 0) {
				return(true);
			}
		}
	}
	return(false);
}

bool msdos_is_con_path(const char *path)
{
	char full[MAX_PATH], *name;
	
	if(GetFullPathNameA(path, MAX_PATH, full, &name) != 0) {
		return(_stricmp(full, "\\\\.\\CON") == 0);
	}
	return(false);
}

int msdos_is_comm_path(const char *path)
{
	char full[MAX_PATH], *name;
	
	if(GetFullPathNameA(path, MAX_PATH, full, &name) != 0) {
		if(_stricmp(full, "\\\\.\\COM1") == 0) {
			return(1);
		} else if(_stricmp(full, "\\\\.\\COM2") == 0) {
			return(2);
		} else if(_stricmp(full, "\\\\.\\COM3") == 0) {
			return(3);
		} else if(_stricmp(full, "\\\\.\\COM4") == 0) {
			return(4);
		}
	}
	return(0);
}

void msdos_set_comm_params(int sio_port, const char *path)
{
	// COM1:{110,150,300,600,1200,2400,4800,9600},{N,O,E,M,S},{8,7,6,5},{1,1.5,2}
	const char *p = NULL;
	
	if((p = strstr(path, ":")) != NULL) {
		UINT8 selector = sio_read(sio_port - 1, 3);
		
		// baud rate
		int baud = max(110, min(9600, atoi(p + 1)));
		UINT16 divisor = 115200 / baud;
		
		if((p = strstr(p + 1, ",")) != NULL) {
			// parity
			if(p[1] == 'N' || p[1] == 'n') {
				selector = (selector & ~0x38) | 0x00;
			} else if(p[1] == 'O' || p[1] == 'o') {
				selector = (selector & ~0x38) | 0x08;
			} else if(p[1] == 'E' || p[1] == 'e') {
				selector = (selector & ~0x38) | 0x18;
			} else if(p[1] == 'M' || p[1] == 'm') {
				selector = (selector & ~0x38) | 0x28;
			} else if(p[1] == 'S' || p[1] == 's') {
				selector = (selector & ~0x38) | 0x38;
			}
			if((p = strstr(p + 1, ",")) != NULL) {
				// word length
				if(p[1] == '8') {
					selector = (selector & ~0x03) | 0x03;
				} else if(p[1] == '7') {
					selector = (selector & ~0x03) | 0x02;
				} else if(p[1] == '6') {
					selector = (selector & ~0x03) | 0x01;
				} else if(p[1] == '5') {
					selector = (selector & ~0x03) | 0x00;
				}
				if((p = strstr(p + 1, ",")) != NULL) {
					// stop bits
					float bits = atof(p + 1);
					if(bits > 1.0F) {
						selector |= 0x04;
					} else {
						selector &= ~0x04;
					}
				}
			}
		}
		sio_write(sio_port - 1, 3, selector | 0x80);
		sio_write(sio_port - 1, 0, divisor & 0xff);
		sio_write(sio_port - 1, 1, divisor >> 8);
		sio_write(sio_port - 1, 3, selector);
	}
}

int msdos_is_prn_path(const char *path)
{
	char full[MAX_PATH], *name;
	
	if(GetFullPathNameA(path, MAX_PATH, full, &name) != 0) {
		if(_stricmp(full, "\\\\.\\PRN") == 0) {
			return(1);
		} else if(_stricmp(full, "\\\\.\\LPT1") == 0) {
			return(1);
		} else if(_stricmp(full, "\\\\.\\LPT2") == 0) {
			return(2);
		} else if(_stricmp(full, "\\\\.\\LPT3") == 0) {
			return(3);
		}
	}
	return(0);
}

bool msdos_is_valid_drive(int drv)
{
	return(drv >= 0 && drv < 26 && (GetLogicalDrives() & (1 << drv)) != 0);
}

bool msdos_is_removable_drive(int drv)
{
	char volume[] = "A:\\";
	
	volume[0] = 'A' + drv;
	
	return(GetDriveTypeA(volume) == DRIVE_REMOVABLE);
}

bool msdos_is_cdrom_drive(int drv)
{
	char volume[] = "A:\\";
	
	volume[0] = 'A' + drv;
	
	return(GetDriveTypeA(volume) == DRIVE_CDROM);
}

bool msdos_is_remote_drive(int drv)
{
	char volume[] = "A:\\";
	
	volume[0] = 'A' + drv;
	
	return(GetDriveTypeA(volume) == DRIVE_REMOTE);
}

bool msdos_is_subst_drive(int drv)
{
	char device[] = "A:", path[MAX_PATH];
	
	device[0] = 'A' + drv;
	
	if(QueryDosDeviceA(device, path, MAX_PATH)) {
		if(strncmp(path, "\\??\\", 4) == 0) {
			return(true);
		}
	}
	return(false);
}

bool msdos_is_existing_file(const char *path)
{
	// http://d.hatena.ne.jp/yu-hr/20100317/1268826458
	WIN32_FIND_DATAA fd;
	HANDLE hFind;
	
	if((hFind = FindFirstFileA(path, &fd)) != INVALID_HANDLE_VALUE) {
		FindClose(hFind);
		return((fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) == 0);
	}
	return(false);
}

bool msdos_is_existing_dir(const char *path)
{
	// http://d.hatena.ne.jp/yu-hr/20100317/1268826458
	WIN32_FIND_DATAA fd;
	HANDLE hFind;
	
	if((hFind = FindFirstFileA(path, &fd)) != INVALID_HANDLE_VALUE) {
		FindClose(hFind);
		return((fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0);
	}
	return(false);
}

const char *msdos_search_command_com(const char *command_path, const char *env_path)
{
	static char tmp[MAX_PATH];
	char path[ENV_SIZE], *file_name;
	
	// check if COMMAND.COM is in the same directory as the target program file
	if(GetFullPathNameA(command_path, MAX_PATH, tmp, &file_name) != 0) {
		sprintf(file_name, "COMMAND.COM");
		if(_access(tmp, 0) == 0) {
			return(tmp);
		}
	}
	
	// check if COMMAND.COM is in the same directory as the running msdos.exe
	if(GetModuleFileNameA(NULL, path, MAX_PATH) != 0 && GetFullPathNameA(path, MAX_PATH, tmp, &file_name) != 0) {
		sprintf(file_name, "COMMAND.COM");
		if(_access(tmp, 0) == 0) {
			return(tmp);
		}
	}
	
	// check if COMMAND.COM is in the current directory
	if(GetFullPathNameA("COMMAND.COM", MAX_PATH, tmp, &file_name) != 0) {
		if(_access(tmp, 0) == 0) {
			return(tmp);
		}
	}
	
	// cehck if COMMAND.COM is in the directory that is in MSDOS_PATH and PATH environment variables
	strcpy(path, env_path);
	char *token = my_strtok(path, ";");
	while(token != NULL) {
		if(strlen(token) != 0 && token[0] != '%') {
			strcpy(tmp, msdos_combine_path(token, "COMMAND.COM"));
			if(_access(tmp, 0) == 0) {
				return(tmp);
			}
		}
		token = my_strtok(NULL, ";");
	}
	return(NULL);
}

int msdos_drive_number(const char *path)
{
	char tmp[MAX_PATH], *name;
	
	if(GetFullPathNameA(path, MAX_PATH, tmp, &name) >= 2 && tmp[1] == ':') {
		if(tmp[0] >= 'a' && tmp[0] <= 'z') {
			return(tmp[0] - 'a');
		} else if(tmp[0] >= 'A' && tmp[0] <= 'Z') {
			return(tmp[0] - 'A');
		}
	}
//	return(msdos_drive_number("."));
	return(_getdrive() - 1);
}

const char *msdos_volume_label(const char *path)
{
	static char tmp[MAX_PATH];
	char volume[] = "A:\\";
	
	if(path[1] == ':') {
		volume[0] = path[0];
	} else {
		volume[0] = 'A' + _getdrive() - 1;
	}
	if(!GetVolumeInformationA(volume, tmp, MAX_PATH, NULL, NULL, NULL, NULL, 0)) {
		memset(tmp, 0, sizeof(tmp));
	}
	return(tmp);
}

const char *msdos_short_volume_label(const char *label)
{
	static char tmp[(8 + 1 + 3) + 1];
	const char *src = label;
	int remain = strlen(label);
	char *dst_n = tmp;
	char *dst_e = tmp + 9;
	
	strcpy(tmp, "        .   ");
	for(int i = 0; i < 8 && remain > 0; i++) {
		if(msdos_lead_byte_check(*src)) {
			if(++i == 8) {
				break;
			}
			*dst_n++ = *src++;
			remain--;
		}
		*dst_n++ = *src++;
		remain--;
	}
	if(remain > 0) {
		for(int i = 0; i < 3 && remain > 0; i++) {
			if(msdos_lead_byte_check(*src)) {
				if(++i == 3) {
					break;
				}
				*dst_e++ = *src++;
				remain--;
			}
			*dst_e++ = *src++;
			remain--;
		}
		*dst_e = '\0';
	} else {
		*dst_n = '\0';
	}
	my_strupr(tmp);
	return(tmp);
}

static inline UINT16 msdos_maperr(unsigned long oserrno)
{
	switch(oserrno) {
	case ERROR_FILE_EXISTS:            // 80
	case ERROR_ALREADY_EXISTS:         // 183
		return ERROR_ACCESS_DENIED;
	}
	return (UINT16)oserrno;
}

int msdos_open(const char *path, int oflag)
{
	if((oflag & (_O_RDONLY | _O_WRONLY | _O_RDWR)) != _O_RDONLY) {
		return(_open(path, oflag));
	}
	
	SECURITY_ATTRIBUTES sa = { sizeof(SECURITY_ATTRIBUTES), NULL, !(oflag & _O_NOINHERIT) };
	DWORD disposition;
	switch(oflag & (_O_CREAT | _O_EXCL | _O_TRUNC)) {
	default:
	case _O_EXCL:
		disposition = OPEN_EXISTING;
		break;
	case _O_CREAT:
		disposition = OPEN_ALWAYS;
		break;
	case _O_CREAT | _O_EXCL:
	case _O_CREAT | _O_TRUNC | _O_EXCL:
		disposition = CREATE_NEW;
		break;
	case _O_TRUNC:
	case _O_TRUNC | _O_EXCL:
		disposition = TRUNCATE_EXISTING;
		break;
	case _O_CREAT | _O_TRUNC:
		disposition = CREATE_ALWAYS;
		break;
	}
	
	HANDLE h = CreateFileA(path, GENERIC_READ | FILE_WRITE_ATTRIBUTES,
		FILE_SHARE_READ | FILE_SHARE_WRITE, &sa, disposition,
		FILE_ATTRIBUTE_NORMAL, NULL);
	if(h == INVALID_HANDLE_VALUE) {
		// FILE_WRITE_ATTRIBUTES may not be granted for standard users.
		// Retry without FILE_WRITE_ATTRIBUTES.
		h = CreateFileA(path, GENERIC_READ,
			FILE_SHARE_READ | FILE_SHARE_WRITE, &sa, disposition,
			FILE_ATTRIBUTE_NORMAL, NULL);
		if(h == INVALID_HANDLE_VALUE) {
			_doserrno = msdos_maperr(GetLastError());
			return(-1);
		}
	}
	
	int fd = _open_osfhandle((intptr_t) h, oflag);
	if(fd == -1) {
		CloseHandle(h);
	}
	return(fd);
}

int msdos_open_device(const char *path, int oflag, int *sio_port, int *lpt_port)
{
	int fd = -1;
	
	*sio_port = *lpt_port = 0;
	
	if(msdos_is_con_path(path)) {
		// MODE.COM opens CON device with read/write mode :-(
		if((oflag & (_O_RDONLY | _O_WRONLY | _O_RDWR)) == _O_RDWR) {
			oflag &= ~(_O_RDONLY | _O_WRONLY | _O_RDWR);
			oflag |= _O_RDONLY;
		}
		if((fd = msdos_open("CON", oflag)) == -1) {
//			fd = msdos_open("NUL", oflag);
		}
	} else if((*sio_port = msdos_is_comm_path(path)) != 0) {
		fd = msdos_open("NUL", oflag);
		msdos_set_comm_params(*sio_port, path);
	} else if((*lpt_port = msdos_is_prn_path(path)) != 0) {
		fd = msdos_open("NUL", oflag);
	} else if(msdos_is_device_path(path)) {
		fd = msdos_open("NUL", oflag);
//	} else if(oflag & _O_CREAT) {
//		fd = _open(path, oflag, _S_IREAD | _S_IWRITE);
//	} else {
//		fd = _open(path, oflag);
	}
	return(fd);
}

UINT16 msdos_device_info(const char *path)
{
	if(msdos_is_con_path(path)) {
		return(0x80d3);
	} else if(msdos_is_comm_path(path)) {
		return(0x80a0);
	} else if(msdos_is_prn_path(path)) {
//		return(0xa8c0);
		return(0x80a0);
	} else if(msdos_is_device_path(path)) {
		if(strstr(path, "EMMXXXX0") != NULL && support_ems) {
			return(0xc0c0);
		} else if(strstr(path, "MSCD001") != NULL) {
			return(0xc880);
		} else {
			return(0x8084);
		}
	} else {
		return(msdos_drive_number(path));
	}
}

void msdos_file_handler_open(int fd, const char *path, int atty, int mode, UINT16 info, UINT16 psp_seg, int sio_port = 0, int lpt_port = 0)
{
	static int id = 0;
	char full[MAX_PATH], *name;
	
	if(GetFullPathNameA(path, MAX_PATH, full, &name) != 0) {
		strcpy(file_handler[fd].path, full);
	} else {
		strcpy(file_handler[fd].path, path);
	}
	// isatty makes no distinction between CON & NUL
	// GetFileSize fails on CON, succeeds on NUL
	if(atty && (info != 0x80d3 || GetFileSize((HANDLE)_get_osfhandle(fd), NULL) == 0)) {
		if(info == 0x80d3) {
			info = 0x8084;
		}
		atty = 0;
	} else if(!atty && info == 0x80d3) {
//		info = msdos_drive_number(".");
		info = msdos_drive_number(path);
	}
	file_handler[fd].valid = 1;
	file_handler[fd].id = id++;	// dummy id for int 21h ax=71a6h
	file_handler[fd].atty = (sio_port == 0 && lpt_port == 0) ? atty : 0;
	file_handler[fd].mode = mode;
	file_handler[fd].info = info;
	file_handler[fd].psp = psp_seg;
	file_handler[fd].sio_port = sio_port;
	file_handler[fd].lpt_port = lpt_port;
	
	// init system file table
	if(fd < 20) {
		UINT8 *sft = mem + SFT_TOP + 6 + 0x3b * fd;
		
		memset(sft, 0, 0x3b);
		
		*(UINT16 *)(sft + 0x00) = 1;
		*(UINT16 *)(sft + 0x02) = file_handler[fd].mode;
		*(UINT8  *)(sft + 0x04) = GetFileAttributesA(file_handler[fd].path) & 0xff;
		*(UINT16 *)(sft + 0x05) = file_handler[fd].info & 0xff;
		
		if(!(file_handler[fd].info & 0x80)) {
			*(UINT16 *)(sft + 0x07) = sizeof(dpb_t) * (file_handler[fd].info & 0x1f);
			*(UINT16 *)(sft + 0x09) = DPB_TOP >> 4;
			
			FILETIME time, local;
			HANDLE hHandle;
			WORD dos_date = 0, dos_time = 0;
			DWORD file_size = 0;
			if((hHandle = (HANDLE)_get_osfhandle(fd)) != INVALID_HANDLE_VALUE) {
				if(GetFileTime(hHandle, NULL, NULL, &time)) {
					FileTimeToLocalFileTime(&time, &local);
					FileTimeToDosDateTime(&local, &dos_date, &dos_time);
				}
				file_size = GetFileSize(hHandle, NULL);
			}
			*(UINT16 *)(sft + 0x0d) = dos_time;
			*(UINT16 *)(sft + 0x0f) = dos_date;
			*(UINT32 *)(sft + 0x11) = file_size;
		}
		
		char fname[MAX_PATH] = {0}, ext[MAX_PATH] = {0};
		_splitpath(file_handler[fd].path, NULL, NULL, fname, ext);
		my_strupr(fname);
		my_strupr(ext);
		memset(sft + 0x20, 0x20, 11);
		memcpy(sft + 0x20, fname, min(strlen(fname), 8));
		memcpy(sft + 0x28, ext + 1, min(strlen(ext + 1), 3));
		
		*(UINT16 *)(sft + 0x31) = psp_seg;
	}
}

void msdos_file_handler_dup(int dst, int src, UINT16 psp_seg)
{
	strcpy(file_handler[dst].path, file_handler[src].path);
	file_handler[dst].valid = 1;
	file_handler[dst].id = file_handler[src].id;
	file_handler[dst].atty = file_handler[src].atty;
	file_handler[dst].mode = file_handler[src].mode;
	file_handler[dst].info = file_handler[src].info;
	file_handler[dst].psp = psp_seg;
	file_handler[dst].sio_port = file_handler[src].sio_port;
	file_handler[dst].lpt_port = file_handler[src].lpt_port;
}

int msdos_file_handler_close(int fd)
{
	// don't close the standard streams even if a program wants to
	if((fd > 2) || (file_handler[fd].valid > 1)) {
		file_handler[fd].valid--;
	}
	if((!file_handler[fd].valid) && fd < 20) {
		memset(mem + SFT_TOP + 6 + 0x3b * fd, 0, 0x3b);
	}
	return file_handler[fd].valid;
}

inline int msdos_file_attribute_create(UINT16 new_attr)
{
	return(new_attr & (FILE_ATTRIBUTE_READONLY | FILE_ATTRIBUTE_HIDDEN |
	                   FILE_ATTRIBUTE_SYSTEM | FILE_ATTRIBUTE_ARCHIVE |
	                   FILE_ATTRIBUTE_DIRECTORY));
}

// find file

int msdos_find_file_check_attribute(int attribute, int allowed_mask, int required_mask)
{
	if((allowed_mask & 0x08) && !(attribute & FILE_ATTRIBUTE_DIRECTORY)) {
		return(0);	// search directory only !!!
	} else if(!(allowed_mask & 0x02) && (attribute & FILE_ATTRIBUTE_HIDDEN)) {
		return(0);
	} else if(!(allowed_mask & 0x04) && (attribute & FILE_ATTRIBUTE_SYSTEM)) {
		return(0);
	} else if(!(allowed_mask & 0x10) && (attribute & FILE_ATTRIBUTE_DIRECTORY)) {
		return(0);
	} else if((attribute & required_mask) != required_mask) {
		return(0);
	} else {
		return(1);
	}
}

int msdos_find_file_has_8dot3name(WIN32_FIND_DATAA *fd)
{
	if(fd->cAlternateFileName[0]) {
		return(1);
	}
	size_t len = strlen(fd->cFileName);
	if(len > 12) {
		return(0);
	}
	const char *ext = strrchr(fd->cFileName, '.');
	if((ext ? ext - fd->cFileName : len) > 8) {
		return(0);
	}
	return(1);
}

void msdos_find_file_conv_local_time(WIN32_FIND_DATAA *fd)
{
	FILETIME local;
	
	FileTimeToLocalFileTime(&fd->ftCreationTime, &local);
	fd->ftCreationTime.dwLowDateTime = local.dwLowDateTime;
	fd->ftCreationTime.dwHighDateTime = local.dwHighDateTime;
	
	FileTimeToLocalFileTime(&fd->ftLastAccessTime, &local);
	fd->ftLastAccessTime.dwLowDateTime = local.dwLowDateTime;
	fd->ftLastAccessTime.dwHighDateTime = local.dwHighDateTime;
	
	FileTimeToLocalFileTime(&fd->ftLastWriteTime, &local);
	fd->ftLastWriteTime.dwLowDateTime = local.dwLowDateTime;
	fd->ftLastWriteTime.dwHighDateTime = local.dwHighDateTime;
}

// i/o

void msdos_stdio_reopen()
{
	if(!file_handler[0].valid) {
		_dup2(DUP_STDIN, 0);
		msdos_file_handler_open(0, "STDIN", _isatty(0), 0, 0x80d3, 0);
	}
	if(!file_handler[1].valid) {
		_dup2(DUP_STDOUT, 1);
		msdos_file_handler_open(1, "STDOUT", _isatty(1), 1, 0x80d3, 0);
	}
	if(!file_handler[2].valid) {
		_dup2(DUP_STDERR, 2);
		msdos_file_handler_open(2, "STDERR", _isatty(2), 1, 0x80d3, 0);
	}
	if(!file_handler[3].valid) {
		_dup2(DUP_STDAUX, 3);
		msdos_file_handler_open(3, "STDAUX", 0, 2, 0x80c0, 0);
	}
	if(!file_handler[4].valid) {
		_dup2(DUP_STDPRN, 4);
//		msdos_file_handler_open(4, "STDPRN", 0, 1, 0xa8c0, 0, 0, 1); // LPT1
		msdos_file_handler_open(4, "STDPRN", 0, 1, 0x80a0, 0, 0, 1); // LPT1
	}
	for(int i = 0; i < 5; i++) {
		if(msdos_psp_get_file_table(i, current_psp) == 0xff) {
			msdos_psp_set_file_table(i, i, current_psp);
		}
	}
}

int msdos_read(int fd, void *buffer, unsigned int count)
{
	if(fd < process->max_files && file_handler[fd].valid && file_handler[fd].sio_port >= 1 && file_handler[fd].sio_port <= 4) {
		// read from serial port
		int read = 0;
		if(sio_port_number[file_handler[fd].sio_port - 1] != 0) {
			UINT8 *buf = (UINT8 *)buffer;
			UINT8 selector = sio_read(file_handler[fd].sio_port - 1, 3);
			sio_write(file_handler[fd].sio_port - 1, 3, selector & ~0x80);
			DWORD timeout = timeGetTime() + 1000;
			while(read < count) {
				if(sio_read(file_handler[fd].sio_port - 1, 5) & 0x01) {
					buf[read++] = sio_read(file_handler[fd].sio_port - 1, 0);
					timeout = timeGetTime() + 1000;
				} else {
					if(timeGetTime() > timeout) {
						break;
					}
					Sleep(10);
				}
			}
			sio_write(file_handler[fd].sio_port - 1, 3, selector);
		}
		return(read);
	}
	return(_read(fd, buffer, count));
}

int msdos_kbhit()
{
	msdos_stdio_reopen();
	
	process_t *process = msdos_process_info_get(current_psp);
	int fd = msdos_psp_get_file_table(0, current_psp);
	
	if(fd < process->max_files && file_handler[fd].valid && !file_handler[fd].atty) {
		// stdin is redirected to file
		return(eof(fd) == 0);
	}
	
	// check keyboard status
	if(key_recv != 0) {
		return(1);
	}
	if(key_buf_char != NULL && key_buf_scan != NULL) {
#ifdef USE_SERVICE_THREAD
		EnterCriticalSection(&key_buf_crit_sect);
#endif
		bool empty = pcbios_is_key_buffer_empty();
#ifdef USE_SERVICE_THREAD
		LeaveCriticalSection(&key_buf_crit_sect);
#endif
		if(!empty) return(1);
	}
	return(_kbhit());
}

int msdos_getch_ex(int echo, unsigned int_num, UINT8 reg_ah)
{
	static char prev = 0;
	
	msdos_stdio_reopen();
	
	process_t *process = msdos_process_info_get(current_psp);
	int fd = msdos_psp_get_file_table(0, current_psp);
	
	if(fd < process->max_files && file_handler[fd].valid && !file_handler[fd].atty) {
		// stdin is redirected to file
retry:
		char data;
		if(msdos_read(fd, &data, 1) == 1) {
			char tmp = data;
			if(data == 0x0a) {
				if(prev == 0x0d) {
					goto retry; // CRLF -> skip LF
				} else {
					data = 0x0d; // LF only -> CR
				}
			}
			prev = tmp;
			return(data);
		}
		return(EOF);
	}
	
	// input from console
	int key_char, key_scan;
	if(key_recv != 0) {
		key_char = (key_code >> 0) & 0xff;
		key_scan = (key_code >> 8) & 0xff;
		key_code >>= 16;
		key_recv >>= 16;
	} else {
		while(key_buf_char != NULL && key_buf_scan != NULL && !msdos_exit) {
			if(key_buf_char != NULL && key_buf_scan != NULL) {
#ifdef USE_SERVICE_THREAD
				EnterCriticalSection(&key_buf_crit_sect);
#endif
				bool empty = pcbios_is_key_buffer_empty();
#ifdef USE_SERVICE_THREAD
				LeaveCriticalSection(&key_buf_crit_sect);
#endif
				if(!empty) break;
			}
			if(!(fd < process->max_files && file_handler[fd].valid && file_handler[fd].atty && file_mode[file_handler[fd].mode].in)) {
				// NOTE: stdin is redirected to stderr when we do "type (file) | more" on freedos's command.com
				if(_kbhit()) {
					if(key_buf_char != NULL && key_buf_scan != NULL) {
#ifdef USE_SERVICE_THREAD
						EnterCriticalSection(&key_buf_crit_sect);
#endif
						pcbios_set_key_buffer(_getch(), 0x00);
#ifdef USE_SERVICE_THREAD
						LeaveCriticalSection(&key_buf_crit_sect);
#endif
					}
				} else {
					Sleep(10);
				}
			} else {
				if(!update_key_buffer()) {
					Sleep(10);
				}
			}
		}
		if(msdos_exit) {
			// insert CR to terminate input loops
			key_char = 0x0d;
			key_scan = 0;
		} else if(key_buf_char != NULL && key_buf_scan != NULL) {
#ifdef USE_SERVICE_THREAD
			EnterCriticalSection(&key_buf_crit_sect);
#endif
			pcbios_get_key_buffer(&key_char, &key_scan);
#ifdef USE_SERVICE_THREAD
			LeaveCriticalSection(&key_buf_crit_sect);
#endif
		}
	}
	if(echo && key_char) {
		msdos_putch(key_char, int_num, reg_ah);
	}
	return key_char ? key_char : (key_scan != 0xe0) ? key_scan : 0;
}

inline int msdos_getch(unsigned int_num, UINT8 reg_ah)
{
	return(msdos_getch_ex(0, int_num, reg_ah));
}

inline int msdos_getche(unsigned int_num, UINT8 reg_ah)
{
	return(msdos_getch_ex(1, int_num, reg_ah));
}

int msdos_write(int fd, const void *buffer, unsigned int count)
{
	if(fd < process->max_files && file_handler[fd].valid && file_handler[fd].sio_port >= 1 && file_handler[fd].sio_port <= 4) {
		// write to serial port
		int written = 0;
		if(sio_port_number[file_handler[fd].sio_port - 1] != 0) {
			UINT8 *buf = (UINT8 *)buffer;
			UINT8 selector = sio_read(file_handler[fd].sio_port - 1, 3);
			sio_write(file_handler[fd].sio_port - 1, 3, selector & ~0x80);
			DWORD timeout = timeGetTime() + 1000;
			while(written < count) {
				if(sio_read(file_handler[fd].sio_port - 1, 5) & 0x20) {
					sio_write(file_handler[fd].sio_port - 1, 0, buf[written++]);
					timeout = timeGetTime() + 1000;
				} else {
					if(timeGetTime() > timeout) {
						break;
					}
					Sleep(10);
				}
			}
			sio_write(file_handler[fd].sio_port - 1, 3, selector);
		}
		return(written);
	} else if(fd < process->max_files && file_handler[fd].valid && file_handler[fd].lpt_port >= 1 && file_handler[fd].lpt_port <= 3) {
		// write to printer port
		UINT8 *buf = (UINT8 *)buffer;
		for(unsigned int i = 0; i < count; i++) {
//			printer_out(file_handler[fd].lpt_port - 1, buf[i]);
			pcbios_printer_out(file_handler[fd].lpt_port - 1, buf[i]);
		}
		return(count);
	} else if(fd == 1 && file_handler[1].valid && !file_handler[1].atty) {
		// CR+LF -> LF
		static int is_cr = 0;
		UINT8 *buf = (UINT8 *)buffer;
		for(unsigned int i = 0; i < count; i++) {
			UINT8 data = buf[i];
			if(is_cr) {
				if(data != 0x0a) {
					UINT8 tmp = 0x0d;
					_write(1, &tmp, 1);
				}
				_write(1, &data, 1);
				is_cr = 0;
			} else if(data == 0x0d) {
				is_cr = 1;
			} else {
				_write(1, &data, 1);
			}
		}
		return(count);
	}
	vram_flush();
	return(_write(fd, buffer, count));
}

void msdos_putch(UINT8 data, unsigned int_num, UINT8 reg_ah)
{
	msdos_stdio_reopen();
	
	process_t *process = msdos_process_info_get(current_psp);
	int fd = msdos_psp_get_file_table(1, current_psp);
	
	if(fd < process->max_files && file_handler[fd].valid && !file_handler[fd].atty) {
		// stdout is redirected to file
		msdos_write(fd, &data, 1);
		return;
	}
	
	// call int 29h ?
	if(*(UINT16 *)(mem + 4 * 0x29 + 0) == (IRET_SIZE + 5 * 0x29) &&
	   *(UINT16 *)(mem + 4 * 0x29 + 2) == (IRET_TOP >> 4)) {
		// int 29h is not hooked, no need to call int 29h
		msdos_putch_fast(data, int_num, reg_ah);
#ifdef USE_SERVICE_THREAD
	} else if(in_service && main_thread_id != GetCurrentThreadId()) {
		// XXX: in usually we should not reach here
		// this is called from service thread to echo the input
		// we can not call int 29h because it causes a critial issue to control cpu running in main thread :-(
		msdos_putch_fast(data, int_num, reg_ah);
#endif
	} else if(in_service_29h) {
		// disallow reentering call int 29h routine to prevent an infinite loop :-(
		msdos_putch_fast(data, int_num, reg_ah);
	} else {
		// this is called from main thread, so we can call int 29h :-)
		in_service_29h = true;
		try {
			UINT16 tmp_cs = CPU_CS;
			UINT32 tmp_eip = CPU_EIP;
			UINT16 tmp_ax = CPU_AX;
			UINT32 tmp_bx = CPU_BX; // BX may be destroyed by some versions of DOS 3.3
			
			// call int 29h routine is at fffc:0027
			CPU_CALL_FAR(DUMMY_TOP >> 4, 0x0027);
			CPU_AL = data;
			
			// run cpu until call int 29h routine is done
			while(!msdos_exit && !(tmp_cs == CPU_CS && tmp_eip == CPU_EIP)) {
				try {
					hardware_run_cpu();
				} catch(...) {
				}
			}
			CPU_AX = tmp_ax;
			CPU_BX = tmp_bx;
		} catch(...) {
		}
		in_service_29h = false;
	}
}

void msdos_putch_fast(UINT8 data, unsigned int_num, UINT8 reg_ah)
#ifdef USE_SERVICE_THREAD
{
	EnterCriticalSection(&putch_crit_sect);
	msdos_putch_tmp(data, int_num, reg_ah);
	LeaveCriticalSection(&putch_crit_sect);
}
void msdos_putch_tmp(UINT8 data, unsigned int_num, UINT8 reg_ah)
#endif
{
	CONSOLE_SCREEN_BUFFER_INFO csbi;
	SMALL_RECT rect;
	COORD co;
	static int p = 0;
	static int is_kanji = 0;
	static int is_esc = 0;
	static int stored_x;
	static int stored_y;
	static WORD stored_a;
	static char tmp[64], out[64];
	
	HANDLE hStdout = GetStdHandle(STD_OUTPUT_HANDLE);
	
	// output to console
	tmp[p++] = data;
	
	vram_flush();
	
	if(is_kanji) {
		// kanji character
		is_kanji = 0;
	} else if(is_esc) {
		// escape sequense
		if((tmp[1] == ')' || tmp[1] == '(') && p == 3) {
			p = is_esc = 0;
		} else if(tmp[1] == '=' && p == 4) {
			co.X = tmp[3] - 0x20;
			co.Y = tmp[2] - 0x20 + scr_top;
			SetConsoleCursorPosition(hStdout, co);
			mem[0x450 + mem[0x462] * 2] = co.X;
			mem[0x451 + mem[0x462] * 2] = co.Y - scr_top;
			cursor_moved = false;
			cursor_moved_by_crtc = false;
			p = is_esc = 0;
		} else if((data >= 'a' && data <= 'z') || (data >= 'A' && data <= 'Z') || data == '*') {
			if(cursor_moved_by_crtc) {
				if(!restore_console_size) {
					GetConsoleScreenBufferInfo(hStdout, &csbi);
					scr_top = csbi.srWindow.Top;
				}
				co.X = mem[0x450 + CPU_BH * 2];
				co.Y = mem[0x451 + CPU_BH * 2] + scr_top;
				SetConsoleCursorPosition(hStdout, co);
				cursor_moved_by_crtc = false;
			}
			GetConsoleScreenBufferInfo(hStdout, &csbi);
			co.X = csbi.dwCursorPosition.X;
			co.Y = csbi.dwCursorPosition.Y;
			WORD wAttributes = csbi.wAttributes;
			
			if(tmp[1] == 'D') {
				co.Y++;
			} else if(tmp[1] == 'E') {
				co.X = 0;
				co.Y++;
			} else if(tmp[1] == 'M') {
				co.Y--;
			} else if(tmp[1] == '*') {
				SET_RECT(rect, 0, csbi.srWindow.Top, csbi.dwSize.X - 1, csbi.srWindow.Bottom);
				WriteConsoleOutputA(hStdout, scr_buf, scr_buf_size, scr_buf_pos, &rect);
				co.X = 0;
				co.Y = csbi.srWindow.Top;
			} else if(tmp[1] == '[') {
				int param[256], params = 0;
				memset(param, 0, sizeof(param));
				for(int i = 2; i < p; i++) {
					if(tmp[i] >= '0' && tmp[i] <= '9') {
						param[params] *= 10;
						param[params] += tmp[i] - '0';
					} else {
						params++;
					}
				}
				if(data == 'A') {
					co.Y -= (params == 0) ? 1 : param[0];
				} else if(data == 'B') {
					co.Y += (params == 0) ? 1 : param[0];
				} else if(data == 'C') {
					co.X += (params == 0) ? 1 : param[0];
				} else if(data == 'D') {
					co.X -= (params == 0) ? 1 : param[0];
				} else if(data == 'H' || data == 'f') {
					co.X = (param[1] == 0 ? 1 : param[1]) - 1;
					co.Y = (param[0] == 0 ? 1 : param[0]) - 1 + csbi.srWindow.Top;
				} else if(data == 'J') {
					clear_scr_buffer(csbi.wAttributes);
					if(param[0] == 0) {
						SET_RECT(rect, co.X, co.Y, csbi.dwSize.X - 1, co.Y);
						WriteConsoleOutputA(hStdout, scr_buf, scr_buf_size, scr_buf_pos, &rect);
						if(co.Y < csbi.srWindow.Bottom) {
							SET_RECT(rect, 0, co.Y + 1, csbi.dwSize.X - 1, csbi.srWindow.Bottom);
							WriteConsoleOutputA(hStdout, scr_buf, scr_buf_size, scr_buf_pos, &rect);
						}
					} else if(param[0] == 1) {
						if(co.Y > csbi.srWindow.Top) {
							SET_RECT(rect, 0, csbi.srWindow.Top, csbi.dwSize.X - 1, co.Y - 1);
							WriteConsoleOutputA(hStdout, scr_buf, scr_buf_size, scr_buf_pos, &rect);
						}
						SET_RECT(rect, 0, co.Y, co.X, co.Y);
						WriteConsoleOutputA(hStdout, scr_buf, scr_buf_size, scr_buf_pos, &rect);
					} else if(param[0] == 2) {
						SET_RECT(rect, 0, csbi.srWindow.Top, csbi.dwSize.X - 1, csbi.srWindow.Bottom);
						WriteConsoleOutputA(hStdout, scr_buf, scr_buf_size, scr_buf_pos, &rect);
						co.X = co.Y = 0;
					}
				} else if(data == 'K') {
					clear_scr_buffer(csbi.wAttributes);
					if(param[0] == 0) {
						SET_RECT(rect, co.X, co.Y, csbi.dwSize.X - 1, co.Y);
						WriteConsoleOutputA(hStdout, scr_buf, scr_buf_size, scr_buf_pos, &rect);
					} else if(param[0] == 1) {
						SET_RECT(rect, 0, co.Y, co.X, co.Y);
						WriteConsoleOutputA(hStdout, scr_buf, scr_buf_size, scr_buf_pos, &rect);
					} else if(param[0] == 2) {
						SET_RECT(rect, 0, co.Y, csbi.dwSize.X - 1, co.Y);
						WriteConsoleOutputA(hStdout, scr_buf, scr_buf_size, scr_buf_pos, &rect);
					}
				} else if(data == 'L') {
					if(params == 0) {
						param[0] = 1;
					}
					SET_RECT(rect, 0, co.Y, csbi.dwSize.X - 1, csbi.srWindow.Bottom);
					ReadConsoleOutputA(hStdout, scr_buf, scr_buf_size, scr_buf_pos, &rect);
					SET_RECT(rect, 0, co.Y + param[0], csbi.dwSize.X - 1, csbi.srWindow.Bottom);
					WriteConsoleOutputA(hStdout, scr_buf, scr_buf_size, scr_buf_pos, &rect);
					clear_scr_buffer(csbi.wAttributes);
					SET_RECT(rect, 0, co.Y, csbi.dwSize.X - 1, co.Y + param[0] - 1);
					WriteConsoleOutputA(hStdout, scr_buf, scr_buf_size, scr_buf_pos, &rect);
					co.X = 0;
				} else if(data == 'M') {
					if(params == 0) {
						param[0] = 1;
					}
					if(co.Y + param[0] > csbi.srWindow.Bottom) {
						clear_scr_buffer(csbi.wAttributes);
						SET_RECT(rect, 0, co.Y, csbi.dwSize.X - 1, csbi.srWindow.Bottom);
						WriteConsoleOutputA(hStdout, scr_buf, scr_buf_size, scr_buf_pos, &rect);
					} else {
						SET_RECT(rect, 0, co.Y + param[0], csbi.dwSize.X - 1, csbi.srWindow.Bottom);
						ReadConsoleOutputA(hStdout, scr_buf, scr_buf_size, scr_buf_pos, &rect);
						SET_RECT(rect, 0, co.Y, csbi.dwSize.X - 1, csbi.srWindow.Bottom);
						WriteConsoleOutputA(hStdout, scr_buf, scr_buf_size, scr_buf_pos, &rect);
						clear_scr_buffer(csbi.wAttributes);
					}
					co.X = 0;
				} else if(data == 'h') {
					if(tmp[2] == '>' && tmp[3] == '5') {
						ci_new.bVisible = FALSE;
					}
				} else if(data == 'l') {
					if(tmp[2] == '>' && tmp[3] == '5') {
						ci_new.bVisible = TRUE;
					}
				} else if(data == 'm') {
					wAttributes = FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE;
					int reverse = 0, hidden = 0;
					for(int i = 0; i < params; i++) {
						if(param[i] == 1) {
							wAttributes |= FOREGROUND_INTENSITY;
						} else if(param[i] == 4) {
							wAttributes |= COMMON_LVB_UNDERSCORE;
						} else if(param[i] == 7) {
							reverse = 1;
						} else if(param[i] == 8 || param[i] == 16) {
							hidden = 1;
						} else if((param[i] >= 17 && param[i] <= 23) || (param[i] >= 30 && param[i] <= 37)) {
							wAttributes &= ~(FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE);
							if(param[i] >= 17 && param[i] <= 23) {
								param[i] -= 16;
							} else {
								param[i] -= 30;
							}
							if(param[i] & 1) {
								wAttributes |= FOREGROUND_RED;
							}
							if(param[i] & 2) {
								wAttributes |= FOREGROUND_GREEN;
							}
							if(param[i] & 4) {
								wAttributes |= FOREGROUND_BLUE;
							}
						} else if(param[i] >= 40 && param[i] <= 47) {
							wAttributes &= ~(BACKGROUND_RED | BACKGROUND_GREEN | BACKGROUND_BLUE);
							if((param[i] - 40) & 1) {
								wAttributes |= BACKGROUND_RED;
							}
							if((param[i] - 40) & 2) {
								wAttributes |= BACKGROUND_GREEN;
							}
							if((param[i] - 40) & 4) {
								wAttributes |= BACKGROUND_BLUE;
							}
						}
					}
					if(reverse) {
						wAttributes &= ~0xff;
						wAttributes |= BACKGROUND_RED | BACKGROUND_GREEN | BACKGROUND_BLUE;
					}
					if(hidden) {
						wAttributes &= ~0x0f;
						wAttributes |= (wAttributes >> 4) & 0x0f;
					}
				} else if(data == 'n') {
					if(param[0] == 6) {
						char tmp[16];
						sprintf(tmp, "\x1b[%d;%dR", co.Y + 1, co.X + 1);
						int len = strlen(tmp);
						if(key_buf_char != NULL && key_buf_scan != NULL) {
#ifdef USE_SERVICE_THREAD
							EnterCriticalSection(&key_buf_crit_sect);
#endif
							for(int i = 0; i < len; i++) {
								pcbios_set_key_buffer(tmp[i], 0x00);
							}
#ifdef USE_SERVICE_THREAD
							LeaveCriticalSection(&key_buf_crit_sect);
#endif
						}
					}
				} else if(data == 's') {
					stored_x = co.X;
					stored_y = co.Y;
					stored_a = wAttributes;
				} else if(data == 'u') {
					co.X = stored_x;
					co.Y = stored_y;
					wAttributes = stored_a;
				}
			}
			if(co.X < 0) {
				co.X = 0;
			} else if(co.X >= csbi.dwSize.X) {
				co.X = csbi.dwSize.X - 1;
			}
			if(co.Y < csbi.srWindow.Top) {
				co.Y = csbi.srWindow.Top;
			} else if(co.Y > csbi.srWindow.Bottom) {
				co.Y = csbi.srWindow.Bottom;
			}
			if(co.X != csbi.dwCursorPosition.X || co.Y != csbi.dwCursorPosition.Y) {
				SetConsoleCursorPosition(hStdout, co);
				mem[0x450 + mem[0x462] * 2] = co.X;
				mem[0x451 + mem[0x462] * 2] = co.Y - csbi.srWindow.Top;
				cursor_moved = false;
			}
			if(wAttributes != csbi.wAttributes) {
				SetConsoleTextAttribute(hStdout, wAttributes);
			}
			p = is_esc = 0;
		}
		return;
	} else {
		if(msdos_lead_byte_check(data)) {
			is_kanji = 1;
			return;
		} else if(data == 0x1b && !msdos_symbol_code_check(data, int_num, reg_ah)) {
			is_esc = 1;
			return;
		}
	}
	
	DWORD q = 0, num;
	is_kanji = 0;
	for(int i = 0; i < p; i++) {
		UINT8 c = tmp[i];
		if(is_kanji) {
			is_kanji = 0;
		} else if(msdos_lead_byte_check(data)) {
			is_kanji = 1;
		} else if(msdos_ctrl_code_check(data, int_num, reg_ah)) {
			out[q++] = '^';
			c += 'A' - 1;
		}
		out[q++] = c;
	}
	out[q] = '\0';
	
	if(cursor_moved_by_crtc) {
		if(!restore_console_size) {
			GetConsoleScreenBufferInfo(hStdout, &csbi);
			scr_top = csbi.srWindow.Top;
		}
		co.X = mem[0x450 + CPU_BH * 2];
		co.Y = mem[0x451 + CPU_BH * 2] + scr_top;
		SetConsoleCursorPosition(hStdout, co);
		cursor_moved_by_crtc = false;
	}
	if(q == 1 && msdos_symbol_code_check(out[0], int_num, reg_ah)) {
		const char *dummy = " ";
		WriteConsoleA(hStdout, dummy, 1, &num, NULL);
		GetConsoleScreenBufferInfo(hStdout, &csbi);
		if(csbi.dwCursorPosition.X > 0) {
			co.X = csbi.dwCursorPosition.X - 1;
			co.Y = csbi.dwCursorPosition.Y;
		} else if(csbi.dwCursorPosition.Y > 0) {
			co.X = csbi.dwSize.X - 1;
			co.Y = csbi.dwCursorPosition.Y - 1;
		} else {
			// XXX: in usually we should not reach here
			co.X = co.Y = 0;
		}
		MyWriteConsoleOutputCharacterA(hStdout, out, 1, co, &num);
	} else if(q == 1 && out[0] == 0x08) {
		// back space
		GetConsoleScreenBufferInfo(hStdout, &csbi);
		if(csbi.dwCursorPosition.X > 0) {
			co.X = csbi.dwCursorPosition.X - 1;
			co.Y = csbi.dwCursorPosition.Y;
			SetConsoleCursorPosition(hStdout, co);
		} else if(csbi.dwCursorPosition.Y > 0) {
			co.X = csbi.dwSize.X - 1;
			co.Y = csbi.dwCursorPosition.Y - 1;
			SetConsoleCursorPosition(hStdout, co);
		} else {
			WriteConsoleA(hStdout, out, 1, &num, NULL); // to make sure
		}
	} else {
		WriteConsoleA(hStdout, out, q, &num, NULL);
	}
	p = 0;
	
	if(!restore_console_size) {
		GetConsoleScreenBufferInfo(hStdout, &csbi);
		scr_top = csbi.srWindow.Top;
	}
	cursor_moved = true;
}

int msdos_aux_in()
{
	msdos_stdio_reopen();
	
	process_t *process = msdos_process_info_get(current_psp);
	int fd = msdos_psp_get_file_table(3, current_psp);
	
	if(fd < process->max_files && file_handler[fd].valid && !eof(fd)) {
		char data = 0;
		msdos_read(fd, &data, 1);
		return(data);
	} else {
		return(EOF);
	}
}

void msdos_aux_out(char data)
{
	msdos_stdio_reopen();
	
	process_t *process = msdos_process_info_get(current_psp);
	int fd = msdos_psp_get_file_table(3, current_psp);
	
	if(fd < process->max_files && file_handler[fd].valid) {
		msdos_write(fd, &data, 1);
	}
}

void msdos_prn_out(char data)
{
	msdos_stdio_reopen();
	
	process_t *process = msdos_process_info_get(current_psp);
	int fd = msdos_psp_get_file_table(4, current_psp);
	
	if(fd < process->max_files && file_handler[fd].valid) {
		msdos_write(fd, &data, 1);
	}
}

// memory control

mcb_t *msdos_mcb_create(int mcb_seg, UINT8 mz, UINT16 psp, int paragraphs, const char *prog_name = NULL)
{
	mcb_t *mcb = (mcb_t *)(mem + (mcb_seg << 4));
	
	mcb->mz = mz;
	mcb->psp = psp;
	mcb->paragraphs = paragraphs;
	
	if(prog_name != NULL) {
		memset(mcb->prog_name, 0, 8);
		memcpy(mcb->prog_name, prog_name, min(8, strlen(prog_name)));
	}
	return(mcb);
}

void msdos_mcb_check(mcb_t *mcb)
{
	if(!(mcb->mz == 'M' || mcb->mz == 'Z')) {
		#if 0
			// shutdown now !!!
			fatalerror("broken memory control block\n");
		#else
			// return error code and continue
			throw(0x07); // broken memory control block
		#endif
	}
}

void msdos_mem_split(int seg, int paragraphs)
{
	int mcb_seg = seg - 1;
	mcb_t *mcb = (mcb_t *)(mem + (mcb_seg << 4));
	msdos_mcb_check(mcb);
	
	if(mcb->paragraphs > paragraphs) {
		int new_seg = mcb_seg + 1 + paragraphs;
		int new_paragraphs = mcb->paragraphs - paragraphs - 1;
		
		msdos_mcb_create(new_seg, mcb->mz, 0, new_paragraphs);
		mcb->mz = 'M';
		mcb->paragraphs = paragraphs;
	}
}

void msdos_mem_merge(int seg)
{
	int mcb_seg = seg - 1;
	mcb_t *mcb = (mcb_t *)(mem + (mcb_seg << 4));
	msdos_mcb_check(mcb);
	
	while(1) {
		if(mcb->mz == 'Z') {
			break;
		}
		int next_seg = mcb_seg + 1 + mcb->paragraphs;
		mcb_t *next_mcb = (mcb_t *)(mem + (next_seg << 4));
		msdos_mcb_check(next_mcb);
		
		if(next_mcb->psp != 0) {
			break;
		}
		mcb->mz = next_mcb->mz;
		mcb->paragraphs = mcb->paragraphs + 1 + next_mcb->paragraphs;
	}
}

int msdos_mem_alloc(int mcb_seg, int paragraphs, int new_process)
{
	while(1) {
		mcb_t *mcb = (mcb_t *)(mem + (mcb_seg << 4));
		bool last_block;
		int found_seg = 0;
		
		if(mcb->psp == 0) {
			msdos_mem_merge(mcb_seg + 1);
		} else {
			msdos_mcb_check(mcb);
		}
		if(!(last_block = (mcb->mz == 'Z'))) {
			// check if the next is dummy mcb to link to umb
			if((malloc_strategy & 0x0f) >= 2 && (mcb->paragraphs >= paragraphs) && !mcb->psp) {
				found_seg = mcb_seg;
			}
			int next_seg = mcb_seg + 1 + mcb->paragraphs;
			mcb_t *next_mcb = (mcb_t *)(mem + (next_seg << 4));
			last_block = (next_mcb->mz == 'Z' && next_mcb->paragraphs == 0);
		}
		if(!(new_process && !last_block)) {
			if((malloc_strategy & 0x0f) >= 2 && found_seg) {
				mcb = (mcb_t *)(mem + (found_seg << 4));
				msdos_mem_split(found_seg + 1, mcb->paragraphs - paragraphs + 1);
				int next_seg = found_seg + 1 + mcb->paragraphs;
				((mcb_t *)(mem + (next_seg << 4)))->psp = current_psp;
				return(next_seg + 1);
			}
			if(mcb->psp == 0 && mcb->paragraphs >= paragraphs) {
				msdos_mem_split(mcb_seg + 1, paragraphs);
				mcb->psp = current_psp;
				return(mcb_seg + 1);
			}
		}
		if(mcb->mz == 'Z') {
			break;
		}
		mcb_seg += 1 + mcb->paragraphs;
	}
	return(-1);
}

int msdos_mem_realloc(int seg, int paragraphs, int *max_paragraphs)
{
	int mcb_seg = seg - 1;
	mcb_t *mcb = (mcb_t *)(mem + (mcb_seg << 4));
	msdos_mcb_check(mcb);
	int current_paragraphs = mcb->paragraphs;
	
	msdos_mem_merge(seg);
	if(paragraphs > mcb->paragraphs) {
		if(max_paragraphs) {
			*max_paragraphs = mcb->paragraphs;
		}
		msdos_mem_split(seg, current_paragraphs);
		return(-1);
	}
	msdos_mem_split(seg, paragraphs);
	return(0);
}

void msdos_mem_free(int seg)
{
	int mcb_seg = seg - 1;
	mcb_t *mcb = (mcb_t *)(mem + (mcb_seg << 4));
	msdos_mcb_check(mcb);
	
	mcb->psp = 0;
	msdos_mem_merge(seg);
}

int msdos_mem_get_free(int mcb_seg, int new_process)
{
	int max_paragraphs = 0;
	
	while(1) {
		mcb_t *mcb = (mcb_t *)(mem + (mcb_seg << 4));
		bool last_block;
		
		msdos_mcb_check(mcb);
		
		if(!(last_block = (mcb->mz == 'Z'))) {
			// check if the next is dummy mcb to link to umb
			int next_seg = mcb_seg + 1 + mcb->paragraphs;
			mcb_t *next_mcb = (mcb_t *)(mem + (next_seg << 4));
			last_block = (next_mcb->mz == 'Z' && next_mcb->paragraphs == 0);
		}
		if(!(new_process && !last_block)) {
			if(mcb->psp == 0 && mcb->paragraphs > max_paragraphs) {
				max_paragraphs = mcb->paragraphs;
			}
		}
		if(mcb->mz == 'Z') {
			break;
		}
		mcb_seg += 1 + mcb->paragraphs;
	}
	return(max_paragraphs > 0x7fff && limit_max_memory ? 0x7fff : max_paragraphs);
}

int msdos_mem_get_last_mcb(int mcb_seg, UINT16 psp)
{
	int last_seg = -1;
	
	while(1) {
		mcb_t *mcb = (mcb_t *)(mem + (mcb_seg << 4));
		msdos_mcb_check(mcb);
		
		if(mcb->psp == psp) {
			last_seg = mcb_seg;
		}
		if(mcb->mz == 'Z') {
			break;
		}
		mcb_seg += 1 + mcb->paragraphs;
	}
	return(last_seg);
}

int msdos_mem_get_umb_linked()
{
	mcb_t *mcb = (mcb_t *)(mem + MEMORY_END - 16);
	msdos_mcb_check(mcb);
	
	if(mcb->mz == 'M') {
		return(-1);
	}
	return(0);
}

void msdos_mem_link_umb()
{
	mcb_t *mcb = (mcb_t *)(mem + MEMORY_END - 16);
	msdos_mcb_check(mcb);
	
	mcb->mz = 'M';
	mcb->paragraphs = (UMB_TOP >> 4) - (MEMORY_END >> 4);
	
	((dos_info_t *)(mem + DOS_INFO_TOP))->umb_linked |= 0x01;
}

void msdos_mem_unlink_umb()
{
	mcb_t *mcb = (mcb_t *)(mem + MEMORY_END - 16);
	msdos_mcb_check(mcb);
	
	mcb->mz = 'Z';
	mcb->paragraphs = 0;
	
	((dos_info_t *)(mem + DOS_INFO_TOP))->umb_linked &= ~0x01;
}

#ifdef SUPPORT_HMA

hma_mcb_t *msdos_hma_mcb_create(int offset, int owner, int size, int next)
{
	hma_mcb_t *mcb = (hma_mcb_t *)(mem + 0xffff0 + offset);
	
	mcb->ms[0] = 'M';
	mcb->ms[1] = 'S';
	mcb->owner = owner;
	mcb->size = size;
	mcb->next = next;
	return(mcb);
}

bool msdos_is_hma_mcb_valid(hma_mcb_t *mcb)
{
	return(mcb->ms[0] == 'M' && mcb->ms[1] == 'S');
}

int msdos_hma_mem_split(int offset, int size)
{
	hma_mcb_t *mcb = (hma_mcb_t *)(mem + 0xffff0 + offset);
	
	if(!msdos_is_hma_mcb_valid(mcb)) {
		return(-1);
	}
	if(mcb->size >= size + 0x10) {
		int new_offset = offset + 0x10 + size;
		int new_size = mcb->size - 0x10 - size;
		
		msdos_hma_mcb_create(new_offset, 0, new_size, mcb->next);
		mcb->size = size;
		mcb->next = new_offset;
		return(0);
	}
	return(-1);
}

void msdos_hma_mem_merge(int offset)
{
	hma_mcb_t *mcb = (hma_mcb_t *)(mem + 0xffff0 + offset);
	
	if(!msdos_is_hma_mcb_valid(mcb)) {
		return;
	}
	while(1) {
		if(mcb->next == 0) {
			break;
		}
		hma_mcb_t *next_mcb = (hma_mcb_t *)(mem + 0xffff0 + mcb->next);
		
		if(!msdos_is_hma_mcb_valid(next_mcb)) {
			return;
		}
		if(next_mcb->owner != 0) {
			break;
		}
		mcb->size += 0x10 + next_mcb->size;
		mcb->next = next_mcb->next;
	}
}

int msdos_hma_mem_alloc(int size, UINT16 owner)
{
	int offset = 0x10; // first mcb in HMA
	
	while(1) {
		hma_mcb_t *mcb = (hma_mcb_t *)(mem + 0xffff0 + offset);
		
		if(!msdos_is_hma_mcb_valid(mcb)) {
			return(-1);
		}
		if(mcb->owner == 0) {
			msdos_hma_mem_merge(offset);
		}
		if(mcb->owner == 0 && mcb->size >= size) {
			msdos_hma_mem_split(offset, size);
			mcb->owner = owner;
			return(offset);
		}
		if(mcb->next == 0) {
			break;
		}
		offset = mcb->next;
	}
	return(-1);
}

int msdos_hma_mem_realloc(int offset, int size)
{
	hma_mcb_t *mcb = (hma_mcb_t *)(mem + 0xffff0 + offset);
	
	if(!msdos_is_hma_mcb_valid(mcb)) {
		return(-1);
	}
	if(mcb->size < size) {
		return(-1);
	}
	msdos_hma_mem_split(offset, size);
	return(0);
}

void msdos_hma_mem_free(int offset)
{
	hma_mcb_t *mcb = (hma_mcb_t *)(mem + 0xffff0 + offset);
	
	if(!msdos_is_hma_mcb_valid(mcb)) {
		return;
	}
	mcb->owner = 0;
	msdos_hma_mem_merge(offset);
}

int msdos_hma_mem_get_free(int *available_offset)
{
	int offset = 0x10; // first mcb in HMA
	int size = 0;
	
	while(1) {
		hma_mcb_t *mcb = (hma_mcb_t *)(mem + 0xffff0 + offset);
		
		if(!msdos_is_hma_mcb_valid(mcb)) {
			return(0);
		}
		if(mcb->owner == 0 && size < mcb->size) {
			if(available_offset != NULL) {
				*available_offset = offset;
			}
			size = mcb->size;
		}
		if(mcb->next == 0) {
			break;
		}
		offset = mcb->next;
	}
	return(size);
}

#endif

// environment

void msdos_env_set_argv(int env_seg, const char *argv)
{
	char *dst = (char *)(mem + (env_seg << 4));
	
	while(1) {
		if(dst[0] == 0) {
			break;
		}
		dst += strlen(dst) + 1;
	}
	*dst++ = 0; // end of environment
	*dst++ = 1; // top of argv[0]
	*dst++ = 0;
	memcpy(dst, argv, strlen(argv));
	dst += strlen(argv);
	*dst++ = 0;
	*dst++ = 0;
}

const char *msdos_env_get_argv(int env_seg)
{
	static char env[ENV_SIZE];
	char *src = env;
	
	memcpy(src, mem + (env_seg << 4), ENV_SIZE);
	while(1) {
		if(src[0] == 0) {
			if(src[1] == 1) {
				return(src + 3);
			}
			break;
		}
		src += strlen(src) + 1;
	}
	return(NULL);
}

const char *msdos_env_get(int env_seg, const char *name)
{
	static char env[ENV_SIZE];
	char *src = env;
	
	memcpy(src, mem + (env_seg << 4), ENV_SIZE);
	while(1) {
		if(src[0] == 0) {
			break;
		}
		int len = strlen(src);
		char *n = my_strtok(src, "=");
		char *v = src + strlen(n) + 1;
		
		if(_stricmp(name, n) == 0) {
			return(v);
		}
		src += len + 1;
	}
	return(NULL);
}

void msdos_env_set(int env_seg, const char *name, const char *value)
{
	char env[ENV_SIZE];
	char *src = env;
	char *dst = (char *)(mem + (env_seg << 4));
	const char *argv = msdos_env_get_argv(env_seg);
	int done = 0;
	
	memcpy(src, dst, ENV_SIZE);
	memset(dst, 0, ENV_SIZE);
	while(1) {
		if(src[0] == 0) {
			break;
		}
		int len = strlen(src);
		char *n = my_strtok(src, "=");
		char *v = src + strlen(n) + 1;
		char tmp[1024];
		
		if(_stricmp(name, n) == 0) {
			sprintf(tmp, "%s=%s", n, value);
			done = 1;
		} else {
			sprintf(tmp, "%s=%s", n, v);
		}
		memcpy(dst, tmp, strlen(tmp));
		dst += strlen(tmp) + 1;
		src += len + 1;
	}
	if(!done) {
		char tmp[1024];
		
		sprintf(tmp, "%s=%s", name, value);
		memcpy(dst, tmp, strlen(tmp));
		dst += strlen(tmp) + 1;
	}
	if(argv) {
		*dst++ = 0; // end of environment
		*dst++ = 1; // top of argv[0]
		*dst++ = 0;
		memcpy(dst, argv, strlen(argv));
		dst += strlen(argv);
		*dst++ = 0;
		*dst++ = 0;
	}
}

// process

psp_t *msdos_psp_create(int psp_seg, UINT16 mcb_seg, UINT16 parent_psp, UINT16 env_seg)
{
	psp_t *psp = (psp_t *)(mem + (psp_seg << 4));
	
	memset(psp, 0, PSP_SIZE);
	psp->exit[0] = 0xcd;
	psp->exit[1] = 0x20;
	psp->first_mcb = mcb_seg;
#if 1
	psp->call5[0] = 0xcd;	// int 30h
	psp->call5[1] = 0x30;
	psp->call5[2] = 0xc3;	// ret
#else
	psp->call5[0] = 0x8a;	// mov ah, cl
	psp->call5[1] = 0xe1;
	psp->call5[2] = 0xcd;	// int 21h
	psp->call5[3] = 0x21;
	psp->call5[4] = 0xc3;	// ret
#endif
	psp->int_22h.dw = *(UINT32 *)(mem + 4 * 0x22);
	psp->int_23h.dw = *(UINT32 *)(mem + 4 * 0x23);
	psp->int_24h.dw = *(UINT32 *)(mem + 4 * 0x24);
	psp->parent_psp = parent_psp;
	if(parent_psp == (UINT16)-1) {
		for(int i = 0; i < 20; i++) {
			if(file_handler[i].valid) {
				psp->file_table[i] = i;
			} else {
				psp->file_table[i] = 0xff;
			}
		}
	} else {
		memcpy(psp->file_table, ((psp_t *)(mem + (parent_psp << 4)))->file_table, 20);
	}
	psp->env_seg = env_seg;
	psp->stack.w.l = CPU_SP;
	psp->stack.w.h = CPU_SS;
	psp->file_table_size = 20;
	psp->file_table_ptr.w.l = 0x18;
	psp->file_table_ptr.w.h = psp_seg;
	psp->service[0] = 0xcd;
	psp->service[1] = 0x21;
	psp->service[2] = 0xcb;
	return(psp);
}

void msdos_psp_set_file_table(int fd, UINT8 value, int psp_seg)
{
	if(psp_seg && fd < 20) {
		psp_t *psp = (psp_t *)(mem + (psp_seg << 4));
		psp->file_table[fd] = value;
	}
}

int msdos_psp_get_file_table(int fd, int psp_seg)
{
	if(psp_seg && fd < 20) {
		psp_t *psp = (psp_t *)(mem + (psp_seg << 4));
		fd = psp->file_table[fd];
	}
	return fd;
}

int msdos_process_exec(const char *cmd, param_block_t *param, UINT8 al, bool first_process = false)
{
	// load command file
	int fd = -1;
	int sio_port = 0;
	int lpt_port = 0;
	int dos_command = 0;
	char command[MAX_PATH], path[MAX_PATH], opt[MAX_PATH], *name = NULL, name_tmp[MAX_PATH];
	char pipe_stdin_path[MAX_PATH] = {0};
	char pipe_stdout_path[MAX_PATH] = {0};
	char pipe_stderr_path[MAX_PATH] = {0};
	
	int opt_ofs = (param->cmd_line.w.h << 4) + param->cmd_line.w.l;
	int opt_len = mem[opt_ofs];
	memset(opt, 0, sizeof(opt));
	memcpy(opt, mem + opt_ofs + 1, opt_len);
	
	if(strlen(cmd) >= 5 && _stricmp(&cmd[strlen(cmd) - 4], ".BAT") == 0) {
		// this is a batch file, run command.com
		char tmp[MAX_PATH];
		if(opt_len != 0) {
			sprintf(tmp, "/C %s %s", cmd, opt);
		} else {
			sprintf(tmp, "/C %s", cmd);
		}
		strcpy(opt, tmp);
		opt_len = strlen(opt);
		mem[opt_ofs] = opt_len;
		sprintf((char *)(mem + opt_ofs + 1), "%s\x0d", opt);
		strcpy(command, comspec_path);
		strcpy(name_tmp, "COMMAND.COM");
	} else {
		if(_stricmp(cmd, "C:\\COMMAND.COM") == 0) {
			// redirect C:\COMMAND.COM to comspec_path
			strcpy(command, comspec_path);
		} else {
			strcpy(command, cmd);
		}
		if(GetFullPathNameA(command, MAX_PATH, path, &name) == 0) {
			return(-1);
		}
		memset(name_tmp, 0, sizeof(name_tmp));
		strcpy(name_tmp, name);
		
		// check command.com
		if((_stricmp(name, "COMMAND.COM") == 0 || _stricmp(name, "COMMAND") == 0) && _access(comspec_path, 0) != 0) {
			// we can not load command.com, so run program directly if "command /c (program)" is specified
			if(opt_len == 0) {
//				process_t *current_process = msdos_process_info_get(current_psp);
				process_t *current_process = NULL;
				for(int i = 0; i < MAX_PROCESS; i++) {
					if(process[i].psp == current_psp) {
						current_process = &process[i];
						break;
					}
				}
				if(current_process != NULL) {
					param->cmd_line.dw = current_process->dta.dw;
					opt_ofs = (param->cmd_line.w.h << 4) + param->cmd_line.w.l;
					opt_len = mem[opt_ofs];
					memset(opt, 0, sizeof(opt));
					memcpy(opt, mem + opt_ofs + 1, opt_len);
				}
			}
			for(int i = 0; i < opt_len; i++) {
				if(opt[i] == ' ') {
					continue;
				}
				if(opt[i] == '/' && (opt[i + 1] == 'c' || opt[i + 1] == 'C') && opt[i + 2] == ' ') {
					for(int j = i + 3; j < opt_len; j++) {
						if(opt[j] == ' ') {
							continue;
						}
						char *token = my_strtok(opt + j, " ");
						
						strcpy(command, token);
						char tmp[MAX_PATH];
						strcpy(tmp, token + strlen(token) + 1);
						strcpy(opt, "");
						for(int i = 0; i < strlen(tmp); i++) {
							if(tmp[i] != ' ') {
								strcpy(opt, tmp + i);
								break;
							}
						}
						strcpy(tmp, opt);
						
						if(al == 0x00) {
							#define GET_FILE_PATH() { \
								if(token[0] != '>' && token[0] != '<') { \
									token++; \
								} \
								token++; \
								while(*token == ' ') { \
									token++; \
								} \
								char *ptr = token; \
								while(*ptr != ' ' && *ptr != '\r' && *ptr != '\0') { \
									ptr++; \
								} \
								*ptr = '\0'; \
							}
							if((token = strstr(opt, "0<")) != NULL || (token = strstr(opt, "<")) != NULL) {
								GET_FILE_PATH();
								strcpy(pipe_stdin_path, token);
								strcpy(opt, tmp);
							}
							if((token = strstr(opt, "1>")) != NULL || (token = strstr(opt, ">")) != NULL) {
								GET_FILE_PATH();
								strcpy(pipe_stdout_path, token);
								strcpy(opt, tmp);
							}
							if((token = strstr(opt, "2>")) != NULL) {
								GET_FILE_PATH();
								strcpy(pipe_stderr_path, token);
								strcpy(opt, tmp);
							}
							#undef GET_FILE_PATH
							
							if((token = strstr(opt, "0<")) != NULL) {
								*token = '\0';
							}
							if((token = strstr(opt, "1>")) != NULL) {
								*token = '\0';
							}
							if((token = strstr(opt, "2>")) != NULL) {
								*token = '\0';
							}
							if((token = strstr(opt, "<")) != NULL) {
								*token = '\0';
							}
							if((token = strstr(opt, ">")) != NULL) {
								*token = '\0';
							}
						}
						for(int i = strlen(opt) - 1; i >= 0 && opt[i] == ' '; i--) {
							opt[i] = '\0';
						}
						opt_len = strlen(opt);
						mem[opt_ofs] = opt_len;
						sprintf((char *)(mem + opt_ofs + 1), "%s\x0d", opt);
						dos_command = 1;
						break;
					}
				}
				break;
			}
		}
	}
	
	// load command file
	strcpy(path, command);
	if((fd = _open(path, _O_RDONLY | _O_BINARY)) == -1) {
		sprintf(path, "%s.COM", command);
		if((fd = _open(path, _O_RDONLY | _O_BINARY)) == -1) {
			sprintf(path, "%s.EXE", command);
			if((fd = _open(path, _O_RDONLY | _O_BINARY)) == -1) {
				sprintf(path, "%s.BAT", command);
				if(_access(path, 0) == 0) {
					// this is a batch file, run command.com
					char tmp[MAX_PATH];
					if(opt_len != 0) {
						sprintf(tmp, "/C %s %s", path, opt);
					} else {
						sprintf(tmp, "/C %s", path);
					}
					strcpy(opt, tmp);
					opt_len = strlen(opt);
					mem[opt_ofs] = opt_len;
					sprintf((char *)(mem + opt_ofs + 1), "%s\x0d", opt);
					strcpy(path, comspec_path);
					strcpy(name_tmp, "COMMAND.COM");
					fd = _open(path, _O_RDONLY | _O_BINARY);
				} else {
					// search path in parent environments
					psp_t *parent_psp = (psp_t *)(mem + (current_psp << 4));
					const char *env = msdos_env_get(parent_psp->env_seg, "PATH");
					if(env != NULL) {
						char env_path[4096];
						strcpy(env_path, env);
						char *token = my_strtok(env_path, ";");
						
						while(token != NULL) {
							if(strlen(token) != 0) {
								sprintf(path, "%s", msdos_combine_path(token, command));
								if((fd = _open(path, _O_RDONLY | _O_BINARY)) != -1) {
									break;
								}
								sprintf(path, "%s.COM", msdos_combine_path(token, command));
								if((fd = _open(path, _O_RDONLY | _O_BINARY)) != -1) {
									break;
								}
								sprintf(path, "%s.EXE", msdos_combine_path(token, command));
								if((fd = _open(path, _O_RDONLY | _O_BINARY)) != -1) {
									break;
								}
								sprintf(path, "%s.BAT", msdos_combine_path(token, command));
								if(_access(path, 0) == 0) {
									// this is a batch file, run command.com
									char tmp[MAX_PATH];
									if(opt_len != 0) {
										sprintf(tmp, "/C %s %s", path, opt);
									} else {
										sprintf(tmp, "/C %s", path);
									}
									strcpy(opt, tmp);
									opt_len = strlen(opt);
									mem[opt_ofs] = opt_len;
									sprintf((char *)(mem + opt_ofs + 1), "%s\x0d", opt);
									strcpy(path, comspec_path);
									strcpy(name_tmp, "COMMAND.COM");
									fd = _open(path, _O_RDONLY | _O_BINARY);
									break;
								}
							}
							token = my_strtok(NULL, ";");
						}
					}
				}
			}
		}
	}
#ifdef ENABLE_DEBUG_OPEN_FILE
	if(fp_debug_log != NULL) {
		fprintf(fp_debug_log, "process_exec\tfd=%d\tr\t%s\n", fd, cmd);
	}
#endif
	if(fd == -1) {
		// we can not find command.com in the path, so open comspec_path
		if(_stricmp(command, "COMMAND.COM") == 0 || _stricmp(command, "COMMAND") == 0) {
			strcpy(command, comspec_path);
			strcpy(path, command);
			fd = _open(path, _O_RDONLY | _O_BINARY);
		}
	}
	if(fd == -1) {
		if(!first_process && al == 0 && dos_command) {
			// may be dos command
			char tmp[MAX_PATH];
			if(opt_len != 0) {
				sprintf(tmp, "%s %s", command, opt);
			} else {
				sprintf(tmp, "%s", command);
			}
			retval = system(tmp);
			return(0);
		} else {
			return(-1);
		}
	}
	memset(file_buffer, 0, sizeof(file_buffer));
	_read(fd, file_buffer, sizeof(file_buffer));
	_close(fd);
	
	// check if this is win32 program
	if(!first_process && al == 0) {
		UINT16 sign_dos = *(UINT16 *)(file_buffer + 0x00);
		UINT32 e_lfanew = *(UINT32 *)(file_buffer + 0x3c);
		if(sign_dos == IMAGE_DOS_SIGNATURE && e_lfanew >= 0x40 && e_lfanew < 0x400) {
			UINT32 sign_nt = *(UINT32 *)(file_buffer + e_lfanew + 0x00);
			UINT16 machine = *(UINT16 *)(file_buffer + e_lfanew + 0x04);
			if(sign_nt == IMAGE_NT_SIGNATURE && (machine == IMAGE_FILE_MACHINE_I386 || machine == IMAGE_FILE_MACHINE_AMD64)) {
				char tmp[MAX_PATH];
				if(opt_len != 0) {
					sprintf(tmp, "\"%s\" %s", path, opt);
				} else {
					sprintf(tmp, "\"%s\"", path);
				}
				retval = system(tmp);
				return(0);
			}
		}
	}
	
	// copy environment
	int umb_linked, env_seg, psp_seg;
	
	if((umb_linked = msdos_mem_get_umb_linked()) != 0) {
		msdos_mem_unlink_umb();
	}
	if((env_seg = msdos_mem_alloc(first_mcb, ENV_SIZE >> 4, 1)) == -1) {
		if((env_seg = msdos_mem_alloc(UMB_TOP >> 4, ENV_SIZE >> 4, 1)) == -1) {
			if(umb_linked != 0) {
				msdos_mem_link_umb();
			}
			return(-1);
		}
	}
	if(param->env_seg == 0) {
		psp_t *parent_psp = (psp_t *)(mem + (current_psp << 4));
		memcpy(mem + (env_seg << 4), mem + (parent_psp->env_seg << 4), ENV_SIZE);
	} else {
		memcpy(mem + (env_seg << 4), mem + (param->env_seg << 4), ENV_SIZE);
	}
	msdos_env_set_argv(env_seg, msdos_short_full_path(path));
	
	// check exe header
	exe_header_t *header = (exe_header_t *)file_buffer;
	int paragraphs, free_paragraphs = msdos_mem_get_free(first_mcb, 1);
	UINT16 cs, ss, ip, sp;
	int start_seg = 0;
	
	if(header->mz == 0x4d5a || header->mz == 0x5a4d) {
		// memory allocation
		int header_size = header->header_size * 16;
		int load_size = (header->pages & 0x7ff) * 512 - header_size;
		if(header_size + load_size < 512) {
			load_size = 512 - header_size;
		}
		paragraphs = (PSP_SIZE + load_size) >> 4;
		if(paragraphs + header->min_alloc > free_paragraphs) {
			msdos_mem_free(env_seg);
			return(-1);
		}
		paragraphs += header->max_alloc ? header->max_alloc : header->min_alloc;
		if(paragraphs > free_paragraphs) {
			paragraphs = free_paragraphs;
		}
		if(!header->min_alloc && !header->max_alloc) {
			psp_seg = msdos_mem_alloc(first_mcb, free_paragraphs, 1);
			start_seg = psp_seg + free_paragraphs - (load_size >> 4);
		} else if((psp_seg = msdos_mem_alloc(first_mcb, paragraphs, 1)) == -1) {
			if((psp_seg = msdos_mem_alloc(UMB_TOP >> 4, paragraphs, 1)) == -1) {
				if(umb_linked != 0) {
					msdos_mem_link_umb();
				}
				msdos_mem_free(env_seg);
				return(-1);
			}
		}
		// relocation
		if(!start_seg) {
			start_seg = psp_seg + (PSP_SIZE >> 4);
		}
		for(int i = 0; i < header->relocations; i++) {
			int ofs = *(UINT16 *)(file_buffer + header->relocation_table + i * 4 + 0);
			int seg = *(UINT16 *)(file_buffer + header->relocation_table + i * 4 + 2);
			*(UINT16 *)(file_buffer + header_size + (seg << 4) + ofs) += start_seg;
		}
		memcpy(mem + (start_seg << 4), file_buffer + header_size, load_size);
		// segments
		cs = header->init_cs + start_seg;
		ss = header->init_ss + start_seg;
		ip = header->init_ip;
		sp = header->init_sp - 2; // for symdeb
	} else {
		// memory allocation
		paragraphs = free_paragraphs;
		if((psp_seg = msdos_mem_alloc(first_mcb, paragraphs, 1)) == -1) {
			if((psp_seg = msdos_mem_alloc(UMB_TOP >> 4, paragraphs, 1)) == -1) {
				if(umb_linked != 0) {
					msdos_mem_link_umb();
				}
				msdos_mem_free(env_seg);
				return(-1);
			}
		}
		start_seg = psp_seg + (PSP_SIZE >> 4);
		memcpy(mem + (start_seg << 4), file_buffer, 0x10000 - PSP_SIZE);
		// segments
		cs = ss = psp_seg;
		ip = 0x100;
		sp = 0xfffe;
	}
	if(umb_linked != 0) {
		msdos_mem_link_umb();
	}
	
	// create psp
	*(UINT16 *)(mem + 4 * 0x22 + 0) = CPU_EIP;
	*(UINT16 *)(mem + 4 * 0x22 + 2) = CPU_CS;
	psp_t *psp = msdos_psp_create(psp_seg, start_seg - (PSP_SIZE >> 4) + paragraphs, current_psp, env_seg);
	memcpy(psp->fcb1, mem + (param->fcb1.w.h << 4) + param->fcb1.w.l, sizeof(psp->fcb1));
	memcpy(psp->fcb2, mem + (param->fcb2.w.h << 4) + param->fcb2.w.l, sizeof(psp->fcb2));
	memcpy(psp->buffer, mem + (param->cmd_line.w.h << 4) + param->cmd_line.w.l, sizeof(psp->buffer));
	
	mcb_t *mcb_env = (mcb_t *)(mem + ((env_seg - 1) << 4));
	mcb_t *mcb_psp = (mcb_t *)(mem + ((psp_seg - 1) << 4));
	mcb_psp->psp = mcb_env->psp = psp_seg;
	
	for(int i = 0; i < 8; i++) {
		if(name_tmp[i] == '.') {
			mcb_psp->prog_name[i] = '\0';
			break;
		} else if(i < 7 && msdos_lead_byte_check(name_tmp[i])) {
			mcb_psp->prog_name[i] = name_tmp[i];
			i++;
			mcb_psp->prog_name[i] = name_tmp[i];
		} else if(name_tmp[i] >= 'a' && name_tmp[i] <= 'z') {
			mcb_psp->prog_name[i] = name_tmp[i] - 'a' + 'A';
		} else {
			mcb_psp->prog_name[i] = name_tmp[i];
		}
	}
	
	// process info
	process_t *process = msdos_process_info_create(psp_seg, path);
	current_psp = psp_seg;
	msdos_sda_update(current_psp);
	
	if(al == 0x00) {
		int_10h_feh_called = int_10h_ffh_called = false;
		
		// pipe
		if(pipe_stdin_path[0] != '\0') {
//			if((fd = _open(pipe_stdin_path, _O_RDONLY | _O_BINARY)) != -1) {
			if(msdos_is_device_path(pipe_stdin_path)) {
				fd = msdos_open_device(pipe_stdin_path, _O_RDONLY | _O_BINARY, &sio_port, &lpt_port);
			} else {
				fd = _open(pipe_stdin_path, _O_RDONLY | _O_BINARY);
			}
			if(fd != -1) {
				msdos_file_handler_open(fd, pipe_stdin_path, _isatty(fd), 0, msdos_device_info(pipe_stdin_path), current_psp, sio_port, lpt_port);
				psp->file_table[0] = fd;
				msdos_psp_set_file_table(fd, fd, current_psp);
			}
		}
		if(pipe_stdout_path[0] != '\0') {
			if(_access(pipe_stdout_path, 0) == 0) {
				SetFileAttributesA(pipe_stdout_path, FILE_ATTRIBUTE_NORMAL);
				DeleteFileA(pipe_stdout_path);
			}
//			if((fd = _open(pipe_stdout_path, _O_WRONLY | _O_BINARY | _O_CREAT | _O_TRUNC, _S_IREAD | _S_IWRITE)) != -1) {
			if(msdos_is_device_path(pipe_stdout_path)) {
				fd = msdos_open_device(pipe_stdout_path, _O_WRONLY | _O_BINARY, &sio_port, &lpt_port);
			} else {
				fd = _open(pipe_stdout_path, _O_WRONLY | _O_BINARY | _O_CREAT | _O_TRUNC, _S_IREAD | _S_IWRITE);
			}
			if(fd != -1) {
				msdos_file_handler_open(fd, pipe_stdout_path, _isatty(fd), 1, msdos_device_info(pipe_stdout_path), current_psp, sio_port, lpt_port);
				psp->file_table[1] = fd;
				msdos_psp_set_file_table(fd, fd, current_psp);
			}
		}
		if(pipe_stderr_path[0] != '\0') {
			if(_access(pipe_stderr_path, 0) == 0) {
				SetFileAttributesA(pipe_stderr_path, FILE_ATTRIBUTE_NORMAL);
				DeleteFileA(pipe_stderr_path);
			}
//			if((fd = _open(pipe_stderr_path, _O_WRONLY | _O_BINARY | _O_CREAT | _O_TRUNC, _S_IREAD | _S_IWRITE)) != -1) {
			if(msdos_is_device_path(pipe_stderr_path)) {
				fd = msdos_open_device(pipe_stderr_path, _O_WRONLY | _O_BINARY, &sio_port, &lpt_port);
			} else {
				fd = _open(pipe_stdout_path, _O_WRONLY | _O_BINARY | _O_CREAT | _O_TRUNC, _S_IREAD | _S_IWRITE);
			}
			if(fd != -1) {
				msdos_file_handler_open(fd, pipe_stderr_path, _isatty(fd), 1, msdos_device_info(pipe_stderr_path), current_psp, sio_port, lpt_port);
				psp->file_table[2] = fd;
				msdos_psp_set_file_table(fd, fd, current_psp);
			}
		}
		
		// registers and segments
		CPU_AX = CPU_BX = 0x00;
		CPU_CX = 0xff;
		CPU_DX = psp_seg;
		CPU_SI = ip;
		CPU_DI = sp;
		CPU_SP = sp;
		CPU_LOAD_SREG(CPU_DS_INDEX, psp_seg);
		CPU_LOAD_SREG(CPU_ES_INDEX, psp_seg);
		CPU_LOAD_SREG(CPU_SS_INDEX, ss);
		
		*(UINT16 *)(mem + (ss << 4) + sp) = 0;
		CPU_JMP_FAR(cs, ip);
	} else if(al == 0x01) {
		// copy ss:sp and cs:ip to param block
		param->sp = sp;
		param->ss = ss;
		param->ip = ip;
		param->cs = cs;
		
		// the AX value to be passed to the child program is put on top of the child's stack
		*(UINT16 *)(mem + (ss << 4) + sp) = CPU_AX;
	}
	return(0);
}

void msdos_process_terminate(int psp_seg, int ret, int mem_free)
{
	psp_t *psp = (psp_t *)(mem + (psp_seg << 4));
	
	*(UINT32 *)(mem + 4 * 0x22) = psp->int_22h.dw;
	*(UINT32 *)(mem + 4 * 0x23) = psp->int_23h.dw;
	*(UINT32 *)(mem + 4 * 0x24) = psp->int_24h.dw;
	
	CPU_LOAD_SREG(CPU_SS_INDEX, psp->stack.w.h);
	CPU_SP = psp->stack.w.l;
	CPU_JMP_FAR(psp->int_22h.w.h, psp->int_22h.w.l);
	
//	process_t *current_process = msdos_process_info_get(psp_seg);
	process_t *current_process = NULL;
	for(int i = 0; i < MAX_PROCESS; i++) {
		if(process[i].psp == psp_seg) {
			current_process = &process[i];
			break;
		}
	}
	if(current_process == NULL) {
		throw(0x1f); // general failure
	}
	int_10h_feh_called = current_process->parent_int_10h_feh_called;
	int_10h_ffh_called = current_process->parent_int_10h_ffh_called;
	if(current_process->called_by_int2eh) {
		CPU_AX = ret;
	}
	CPU_LOAD_SREG(CPU_DS_INDEX, current_process->parent_ds);
	CPU_LOAD_SREG(CPU_ES_INDEX, current_process->parent_es);
	
	if(mem_free) {
		int mcb_seg;
		while((mcb_seg = msdos_mem_get_last_mcb(first_mcb, psp_seg)) != -1) {
			msdos_mem_free(mcb_seg + 1);
		}
		while((mcb_seg = msdos_mem_get_last_mcb(UMB_TOP >> 4, psp_seg)) != -1) {
			msdos_mem_free(mcb_seg + 1);
		}
		
		for(int i = 0; i < MAX_FILES; i++) {
			if(file_handler[i].valid && file_handler[i].psp == psp_seg) {
				if(!msdos_file_handler_close(i)) {
					_close(i);
				}
				msdos_psp_set_file_table(i, 0x0ff, psp_seg); // FIXME: consider duplicated file handles
			}
		}
		msdos_dta_info_free(psp_seg);
	}
	msdos_stdio_reopen();
	
	memset(current_process, 0, sizeof(process_t));
	
	current_psp = psp->parent_psp;
	retval = ret;
	msdos_sda_update(current_psp);
}

// drive

int pcbios_update_drive_param(int drive_num, int force_update);

int msdos_drive_param_block_update(int drive_num, UINT16 *seg, UINT16 *ofs, int force_update)
{
	if(!(drive_num >= 0 && drive_num < 26)) {
		return(0);
	}
	pcbios_update_drive_param(drive_num, force_update);
	
	drive_param_t *drive_param = &drive_params[drive_num];
	*seg = DPB_TOP >> 4;
	*ofs = sizeof(dpb_t) * drive_num;
	dpb_t *dpb = (dpb_t *)(mem + (*seg << 4) + *ofs);
	
	memset(dpb, 0, sizeof(dpb_t));
	
	dpb->drive_num = drive_num;
	dpb->unit_num = drive_num;
	
	if(drive_param->valid) {
		DISK_GEOMETRY *geo = &drive_param->geometry;
		
		dpb->bytes_per_sector = (UINT16)geo->BytesPerSector;
		dpb->highest_sector_num = (UINT8)(geo->SectorsPerTrack - 1);
		dpb->highest_cluster_num = (UINT16)(geo->TracksPerCylinder * geo->Cylinders.QuadPart + 1);
		dpb->maximum_cluster_num = (UINT32)(geo->TracksPerCylinder * geo->Cylinders.QuadPart + 1);
		switch(geo->MediaType) {
		case F5_320_512:	// floppy, double-sided, 8 sectors per track (320K)
			dpb->media_type = 0xff;
			break;
		case F5_160_512:	// floppy, single-sided, 8 sectors per track (160K)
			dpb->media_type = 0xfe;
			break;
		case F5_360_512:	// floppy, double-sided, 9 sectors per track (360K)
			dpb->media_type = 0xfd;
			break;
		case F5_180_512:	// floppy, single-sided, 9 sectors per track (180K)
			dpb->media_type = 0xfc;
			break;
		case F5_1Pt2_512:	// floppy, double-sided, 15 sectors per track (1.2M)
		case F3_1Pt2_512:
		case F3_720_512:	// floppy, double-sided, 9 sectors per track (720K,3.5")
		case F5_720_512:
			dpb->media_type = 0xf9;
			break;
		case FixedMedia:	// hard disk
		case RemovableMedia:
		case Unknown:
			dpb->media_type = 0xf8;
			break;
		default:
			dpb->media_type = 0xf0;
			break;
		}
	}
	dpb->next_dpb_ofs = (drive_num == 25) ? 0xffff : *ofs + sizeof(dpb_t);
	dpb->next_dpb_seg = (drive_num == 25) ? 0xffff : *seg;
	dpb->info_sector = 0xffff;
	dpb->backup_boot_sector = 0xffff;
	dpb->free_clusters = 0xffff;
	dpb->free_search_cluster = 0xffffffff;
	
	return(drive_param->valid);
}

// pc bios

void prepare_service_loop()
{
	CPU_AX_in_service = CPU_AX;
	CPU_CX_in_service = CPU_CX;
	CPU_DX_in_service = CPU_DX;
	CPU_DS_BASE_in_service = CPU_DS_BASE;
}

void cleanup_service_loop()
{
	CPU_AX = CPU_AX_in_service;
//	CPU_CX = CPU_CX_in_service;
//	CPU_DX = CPU_DX_in_service;
//	CPU_DS_BASE = CPU_DS_BASE_in_service;
}

#ifdef USE_SERVICE_THREAD
void start_service_loop(LPTHREAD_START_ROUTINE lpStartAddress)
{
	if(CPU_S_FLAG) {
		CPU_SET_S_FLAG(0);
		mem[DUMMY_TOP + 0x15] = 0x79;	// jns -4
	} else {
		CPU_SET_S_FLAG(1);
		mem[DUMMY_TOP + 0x15] = 0x78;	// js -4
	}
	
	// dummy loop to wait BIOS/DOS service is done is at fffc:0013
	UINT16 tmp_cs = CPU_CS;
	UINT32 tmp_eip = CPU_EIP;
	
	prepare_service_loop();
	
	CPU_CALL_FAR(DUMMY_TOP >> 4, 0x0013);
	in_service = true;
	service_exit = false;
	CloseHandle(CreateThread(NULL, 0, lpStartAddress, NULL, 0, NULL));

	// run cpu until dummy loop routine is done
	while(!msdos_exit && !(tmp_cs == CPU_CS && tmp_eip == CPU_EIP)) {
		try {
			hardware_run_cpu();
		} catch(...) {
		}
	}
	cleanup_service_loop();
}

void finish_service_loop()
{
	if(in_service && service_exit) {
		if(CPU_S_FLAG) {
			CPU_SET_S_FLAG(0);
		} else {
			CPU_SET_S_FLAG(1);
		}
		in_service = false;
	}
}
#endif

UINT32 get_ticks_since_midnight(UINT32 cur_msec)
{
	static unsigned __int64 start_msec_since_midnight = 0;
	static unsigned __int64 start_msec_since_hostboot = 0;
	
	if(start_msec_since_midnight == 0) {
		SYSTEMTIME time;
		GetLocalTime(&time);
		start_msec_since_midnight = ((time.wHour * 60 + time.wMinute) * 60 + time.wSecond) * 1000 + time.wMilliseconds;
		start_msec_since_hostboot = cur_msec;
	}
	unsigned __int64 msec = (start_msec_since_midnight + cur_msec - start_msec_since_hostboot) % (24 * 60 * 60 * 1000);
	unsigned __int64 tick = msec * 0x1800b0 / (24 * 60 * 60 * 1000);
	return (UINT32)tick;
}

void pcbios_update_daily_timer_counter(UINT32 cur_msec)
{
	UINT32 prev_tick = *(UINT32 *)(mem + 0x46c);
	UINT32 next_tick = get_ticks_since_midnight(cur_msec);
	
	if(prev_tick > next_tick) {
		mem[0x470] = 1;
	}
	*(UINT32 *)(mem + 0x46c) = next_tick;
}

inline void pcbios_irq0()
{
	//++*(UINT32 *)(mem + 0x46c);
	pcbios_update_daily_timer_counter(timeGetTime());
}

int pcbios_get_text_vram_address(int page)
{
	if(/*mem[0x449] == 0x03 || */mem[0x449] == 0x70 || mem[0x449] == 0x71 || mem[0x449] == 0x73) {
		return TEXT_VRAM_TOP;
	} else {
		return TEXT_VRAM_TOP + VIDEO_REGEN * (page % vram_pages);
	}
}

int pcbios_get_shadow_buffer_address(int page)
{
	if(!int_10h_feh_called) {
		return pcbios_get_text_vram_address(page);
	} else if(/*mem[0x449] == 0x03 || */mem[0x449] == 0x70 || mem[0x449] == 0x71 || mem[0x449] == 0x73) {
		return SHADOW_BUF_TOP;
	} else {
		return SHADOW_BUF_TOP + VIDEO_REGEN * (page % vram_pages);
	}
}

int pcbios_get_shadow_buffer_address(int page, int x, int y)
{
	return pcbios_get_shadow_buffer_address(page) + (x + y * scr_width) * 2;
}

bool pcbios_set_font_size(int width, int height)
{
	if(set_console_font_size(width, height)) {
		*(UINT16 *)(mem + 0x485) = height;
		return(true);
	}
	return(false);
}

void pcbios_set_console_size(int width, int height, bool clr_screen)
{
	// clear the existing screen, not just the new one
	int clr_height = max(height, scr_height);
	
	if(scr_width != width || scr_height != height) {
		change_console_size(width, height);
	}
	mem[0x462] = 0;
	*(UINT16 *)(mem + 0x44e) = 0;
	
	text_vram_top_address = pcbios_get_text_vram_address(0);
	text_vram_end_address = text_vram_top_address + width * height * 2;
	shadow_buffer_top_address = pcbios_get_shadow_buffer_address(0);
	shadow_buffer_end_address = shadow_buffer_top_address + width * height * 2;
	cursor_position_address = 0x450 + mem[0x462] * 2;
	
	HANDLE hStdout = GetStdHandle(STD_OUTPUT_HANDLE);
	if(clr_screen) {
		for(int ofs = shadow_buffer_top_address; ofs < shadow_buffer_end_address;) {
			mem[ofs++] = 0x20;
			mem[ofs++] = 0x07;
		}
		
#ifdef USE_VRAM_THREAD
		EnterCriticalSection(&vram_crit_sect);
#endif
		for(int y = 0; y < clr_height; y++) {
			for(int x = 0; x < scr_width; x++) {
				SCR_BUF(y,x).Char.AsciiChar = ' ';
				SCR_BUF(y,x).Attributes = FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE;
			}
		}
		SMALL_RECT rect;
		SET_RECT(rect, 0, scr_top, scr_width - 1, scr_top + clr_height - 1);
		WriteConsoleOutputA(hStdout, scr_buf, scr_buf_size, scr_buf_pos, &rect);
		vram_length_char = vram_last_length_char = 0;
		vram_length_attr = vram_last_length_attr = 0;
#ifdef USE_VRAM_THREAD
		LeaveCriticalSection(&vram_crit_sect);
#endif
	}
	COORD co;
	co.X = 0;
	co.Y = scr_top;
	SetConsoleCursorPosition(hStdout, co);
	cursor_moved = true;
	cursor_moved_by_crtc = false;
	SetConsoleTextAttribute(hStdout, FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE);
}

void pcbios_update_cursor_position()
{
	CONSOLE_SCREEN_BUFFER_INFO csbi;
	GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE), &csbi);
	if(!restore_console_size) {
		scr_top = csbi.srWindow.Top;
	}
	mem[0x450 + mem[0x462] * 2] = csbi.dwCursorPosition.X;
	mem[0x451 + mem[0x462] * 2] = csbi.dwCursorPosition.Y - scr_top;
}

#ifdef SUPPORT_GRAPHIC_SCREEN
static UINT32 vga_read(UINT32 addr, int size)
{
	UINT32 ret = 0;
	return ret;
}

static void vga_write(UINT32 addr, UINT32 data, int size)
{
}
#endif

inline void pcbios_int_10h_00h()
{
	switch(CPU_AL & 0x7f) {
	case 0x70: // v-text mode
	case 0x71: // extended cga v-text mode
		pcbios_set_console_size(scr_width, scr_height, !(CPU_AL & 0x80));
		break;
	case 0x73:
	case 0x03:
		change_console_size(80, 25); // for Windows10
		pcbios_set_font_size(font_width, font_height);
		pcbios_set_console_size(80, 25, !(CPU_AL & 0x80));
		break;
	}
	if(CPU_AL & 0x80) {
		mem[0x487] |= 0x80;
	} else {
		mem[0x487] &= ~0x80;
	}
	mem[0x449] = CPU_AL & 0x7f;
}

inline void pcbios_int_10h_01h()
{
	mem[0x460] = CPU_CL;
	mem[0x461] = CPU_CH;
	
	int size = (int)(CPU_CL & 7) - (int)(CPU_CH & 7) + 1;
	
	if(!((CPU_CH & 0x20) != 0 || size < 0)) {
		ci_new.bVisible = TRUE;
		ci_new.dwSize = (size + 2) * 100 / (8 + 2);
	} else {
		ci_new.bVisible = FALSE;
	}
}

inline void pcbios_int_10h_02h()
{
	// continuously setting the cursor effectively stops it blinking
	if(mem[0x462] == CPU_BH && (CPU_DL != mem[0x450 + CPU_BH * 2] || CPU_DH != mem[0x451 + CPU_BH * 2] || cursor_moved_by_crtc)) {
		COORD co;
		co.X = CPU_DL;
		co.Y = CPU_DH + scr_top;
		
		// some programs hide the cursor by moving it off screen
		static bool hidden = false;
		HANDLE hStdout = GetStdHandle(STD_OUTPUT_HANDLE);
		
		if(CPU_DH >= scr_height || !SetConsoleCursorPosition(hStdout, co)) {
			if(ci_new.bVisible) {
				ci_new.bVisible = FALSE;
				hidden = true;
			}
		} else if(hidden) {
			if(!ci_new.bVisible) {
				ci_new.bVisible = TRUE;
			}
			hidden = false;
		}
		cursor_moved_by_crtc = false;
	}
	mem[0x450 + (CPU_BH % vram_pages) * 2] = CPU_DL;
	mem[0x451 + (CPU_BH % vram_pages) * 2] = CPU_DH;
}

inline void pcbios_int_10h_03h()
{
	CPU_DL = mem[0x450 + (CPU_BH % vram_pages) * 2];
	CPU_DH = mem[0x451 + (CPU_BH % vram_pages) * 2];
	CPU_CL = mem[0x460];
	CPU_CH = mem[0x461];
}

inline void pcbios_int_10h_05h()
{
	if(CPU_AL >= vram_pages) {
		return;
	}
	if(mem[0x462] != CPU_AL) {
		vram_flush();
		
		HANDLE hStdout = GetStdHandle(STD_OUTPUT_HANDLE);
		SMALL_RECT rect;
		SET_RECT(rect, 0, scr_top, scr_width - 1, scr_top + scr_height - 1);
		ReadConsoleOutputA(hStdout, scr_buf, scr_buf_size, scr_buf_pos, &rect);
		
		for(int y = 0, ofs = pcbios_get_shadow_buffer_address(mem[0x462]); y < scr_height; y++) {
			for(int x = 0; x < scr_width; x++) {
				mem[ofs++] = SCR_BUF(y,x).Char.AsciiChar;
				mem[ofs++] = SCR_BUF(y,x).Attributes;
			}
		}
		for(int y = 0, ofs = pcbios_get_shadow_buffer_address(CPU_AL); y < scr_height; y++) {
			for(int x = 0; x < scr_width; x++) {
				SCR_BUF(y,x).Char.AsciiChar = mem[ofs++];
				SCR_BUF(y,x).Attributes = mem[ofs++];
			}
		}
		WriteConsoleOutputA(hStdout, scr_buf, scr_buf_size, scr_buf_pos, &rect);
		
		COORD co;
		co.X = mem[0x450 + CPU_AL * 2];
		co.Y = mem[0x451 + CPU_AL * 2] + scr_top;
		if(co.Y < scr_top + scr_height) {
			SetConsoleCursorPosition(hStdout, co);
		}
		cursor_moved_by_crtc = false;
	}
	mem[0x462] = CPU_AL;
	*(UINT16 *)(mem + 0x44e) = CPU_AL * VIDEO_REGEN;
	int regen = min(scr_width * scr_height * 2, 0x8000);
	text_vram_top_address = pcbios_get_text_vram_address(mem[0x462]);
	text_vram_end_address = text_vram_top_address + regen;
	shadow_buffer_top_address = pcbios_get_shadow_buffer_address(mem[0x462]);
	shadow_buffer_end_address = shadow_buffer_top_address + regen;
	cursor_position_address = 0x450 + mem[0x462] * 2;
}

inline void pcbios_int_10h_06h()
{
	if(CPU_CH >= scr_height || CPU_CH > CPU_DH ||
	   CPU_CL >= scr_width  || CPU_CL > CPU_DL) {
		return;
	}
	vram_flush();
	
	HANDLE hStdout = GetStdHandle(STD_OUTPUT_HANDLE);
	SMALL_RECT rect;
	SET_RECT(rect, 0, scr_top, scr_width - 1, scr_top + scr_height - 1);
	ReadConsoleOutputA(hStdout, scr_buf, scr_buf_size, scr_buf_pos, &rect);
	
	int right = min(CPU_DL, scr_width - 1);
	int bottom = min(CPU_DH, scr_height - 1);
	
	if(CPU_AL == 0) {
		for(int y = CPU_CH; y <= bottom; y++) {
			for(int x = CPU_CL, ofs = pcbios_get_shadow_buffer_address(mem[0x462], CPU_CL, y); x <= right; x++) {
				mem[ofs++] = SCR_BUF(y,x).Char.AsciiChar = ' ';
				mem[ofs++] = SCR_BUF(y,x).Attributes = CPU_BH;
			}
		}
	} else {
		for(int y = CPU_CH, y2 = min(CPU_CH + CPU_AL, bottom + 1); y <= bottom; y++, y2++) {
			for(int x = CPU_CL, ofs = pcbios_get_shadow_buffer_address(mem[0x462], CPU_CL, y); x <= right; x++) {
				if(y2 <= bottom) {
					SCR_BUF(y,x) = SCR_BUF(y2,x);
				} else {
					SCR_BUF(y,x).Char.AsciiChar = ' ';
					SCR_BUF(y,x).Attributes = CPU_BH;
				}
				mem[ofs++] = SCR_BUF(y,x).Char.AsciiChar;
				mem[ofs++] = SCR_BUF(y,x).Attributes;
			}
		}
	}
	WriteConsoleOutputA(hStdout, scr_buf, scr_buf_size, scr_buf_pos, &rect);
}

inline void pcbios_int_10h_07h()
{
	if(CPU_CH >= scr_height || CPU_CH > CPU_DH ||
	   CPU_CL >= scr_width  || CPU_CL > CPU_DL) {
		return;
	}
	vram_flush();
	
	HANDLE hStdout = GetStdHandle(STD_OUTPUT_HANDLE);
	SMALL_RECT rect;
	SET_RECT(rect, 0, scr_top, scr_width - 1, scr_top + scr_height - 1);
	ReadConsoleOutputA(hStdout, scr_buf, scr_buf_size, scr_buf_pos, &rect);
	
	int right = min(CPU_DL, scr_width - 1);
	int bottom = min(CPU_DH, scr_height - 1);
	
	if(CPU_AL == 0) {
		for(int y = CPU_CH; y <= bottom; y++) {
			for(int x = CPU_CL, ofs = pcbios_get_shadow_buffer_address(mem[0x462], CPU_CL, y); x <= right; x++) {
				mem[ofs++] = SCR_BUF(y,x).Char.AsciiChar = ' ';
				mem[ofs++] = SCR_BUF(y,x).Attributes = CPU_BH;
			}
		}
	} else {
		for(int y = bottom, y2 = max(CPU_CH - 1, bottom - CPU_AL); y >= CPU_CH; y--, y2--) {
			for(int x = CPU_CL, ofs = pcbios_get_shadow_buffer_address(mem[0x462], CPU_CL, y); x <= right; x++) {
				if(y2 >= CPU_CH) {
					SCR_BUF(y,x) = SCR_BUF(y2,x);
				} else {
					SCR_BUF(y,x).Char.AsciiChar = ' ';
					SCR_BUF(y,x).Attributes = CPU_BH;
				}
				mem[ofs++] = SCR_BUF(y,x).Char.AsciiChar;
				mem[ofs++] = SCR_BUF(y,x).Attributes;
			}
		}
	}
	WriteConsoleOutputA(hStdout, scr_buf, scr_buf_size, scr_buf_pos, &rect);
}

inline void pcbios_int_10h_08h()
{
	COORD co;
	DWORD num;
	
	co.X = mem[0x450 + (CPU_BH % vram_pages) * 2];
	co.Y = mem[0x451 + (CPU_BH % vram_pages) * 2];
	
	if(mem[0x462] == CPU_BH) {
		HANDLE hStdout = GetStdHandle(STD_OUTPUT_HANDLE);
		co.Y += scr_top;
		vram_flush();
		ReadConsoleOutputCharacterA(hStdout, scr_char, 1, co, &num);
		ReadConsoleOutputAttribute(hStdout, scr_attr, 1, co, &num);
		CPU_AL = scr_char[0];
		CPU_AH = scr_attr[0];
	} else {
		CPU_AX = *(UINT16 *)(mem + pcbios_get_shadow_buffer_address(CPU_BH, co.X, co.Y));
	}
}

inline void pcbios_int_10h_09h()
{
	COORD co;
	
	co.X = mem[0x450 + (CPU_BH % vram_pages) * 2];
	co.Y = mem[0x451 + (CPU_BH % vram_pages) * 2];
	
	int dest = pcbios_get_shadow_buffer_address(CPU_BH, co.X, co.Y);
	int end = min(dest + CPU_CX * 2, pcbios_get_shadow_buffer_address(CPU_BH, 0, scr_height));
	
	if(mem[0x462] == CPU_BH) {
#ifdef USE_VRAM_THREAD
		EnterCriticalSection(&vram_crit_sect);
#endif
		int vram = pcbios_get_shadow_buffer_address(CPU_BH);
		while(dest < end) {
			write_text_vram_char(dest - vram, CPU_AL);
			mem[dest++] = CPU_AL;
			write_text_vram_attr(dest - vram, CPU_BL);
			mem[dest++] = CPU_BL;
		}
#ifdef USE_VRAM_THREAD
		LeaveCriticalSection(&vram_crit_sect);
#endif
	} else {
		while(dest < end) {
			mem[dest++] = CPU_AL;
			mem[dest++] = CPU_BL;
		}
	}
}

inline void pcbios_int_10h_0ah()
{
	COORD co;
	
	co.X = mem[0x450 + (CPU_BH % vram_pages) * 2];
	co.Y = mem[0x451 + (CPU_BH % vram_pages) * 2];
	
	int dest = pcbios_get_shadow_buffer_address(CPU_BH, co.X, co.Y);
	int end = min(dest + CPU_CX * 2, pcbios_get_shadow_buffer_address(CPU_BH, 0, scr_height));
	
	if(mem[0x462] == CPU_BH) {
#ifdef USE_VRAM_THREAD
		EnterCriticalSection(&vram_crit_sect);
#endif
		int vram = pcbios_get_shadow_buffer_address(CPU_BH);
		while(dest < end) {
			write_text_vram_char(dest - vram, CPU_AL);
			mem[dest++] = CPU_AL;
			dest++;
		}
#ifdef USE_VRAM_THREAD
		LeaveCriticalSection(&vram_crit_sect);
#endif
	} else {
		while(dest < end) {
			mem[dest++] = CPU_AL;
			dest++;
		}
	}
}

inline void pcbios_int_10h_0ch()
{
	HDC hdc = get_console_window_device_context();
	
	if(hdc != NULL) {
		BYTE r = (CPU_AL & 2) ? 255 : 0;
		BYTE g = (CPU_AL & 4) ? 255 : 0;
		BYTE b = (CPU_AL & 1) ? 255 : 0;
		
		if(CPU_AL & 0x80) {
			COLORREF color = GetPixel(hdc, CPU_CX, CPU_DX);
			if(color != CLR_INVALID) {
				r ^= ((DWORD)color & 0x0000ff) ? 255 : 0;
				g ^= ((DWORD)color & 0x00ff00) ? 255 : 0;
				b ^= ((DWORD)color & 0xff0000) ? 255 : 0;
			}
		}
		SetPixel(hdc, CPU_CX, CPU_DX, RGB(r, g, b));
	}
}

inline void pcbios_int_10h_0dh()
{
	HDC hdc = get_console_window_device_context();
	BYTE r = 0;
	BYTE g = 0;
	BYTE b = 0;
	
	if(hdc != NULL) {
		COLORREF color = GetPixel(hdc, CPU_CX, CPU_DX);
		if(color != CLR_INVALID) {
			r = ((DWORD)color & 0x0000ff) ? 255 : 0;
			g = ((DWORD)color & 0x00ff00) ? 255 : 0;
			b = ((DWORD)color & 0xff0000) ? 255 : 0;
		}
	}
	CPU_AL = ((b != 0) ? 1 : 0) | ((r != 0) ? 2 : 0) | ((g != 0) ? 4 : 0);
}

inline void pcbios_int_10h_0eh()
{
	HANDLE hStdout = GetStdHandle(STD_OUTPUT_HANDLE);
	CONSOLE_SCREEN_BUFFER_INFO csbi;
	DWORD num;
	COORD co;
	
	if(cursor_moved_by_crtc) {
		if(!restore_console_size) {
			GetConsoleScreenBufferInfo(hStdout, &csbi);
			scr_top = csbi.srWindow.Top;
		}
		co.X = mem[0x450 + CPU_BH * 2];
		co.Y = mem[0x451 + CPU_BH * 2] + scr_top;
		SetConsoleCursorPosition(hStdout, co);
		cursor_moved_by_crtc = false;
	}
	co.X = mem[0x450 + mem[0x462] * 2];
	co.Y = mem[0x451 + mem[0x462] * 2];
	
	if(CPU_AL == 7) {
		//MessageBeep(-1);
	} else if(CPU_AL == 8 || CPU_AL == 10 || CPU_AL == 13) {
		if(CPU_AL == 10) {
			vram_flush();
		}
		WriteConsoleA(GetStdHandle(STD_OUTPUT_HANDLE), &CPU_AL, 1, &num, NULL);
		cursor_moved = true;
	} else {
		int dest = pcbios_get_shadow_buffer_address(mem[0x462], co.X, co.Y);
#ifdef USE_VRAM_THREAD
		EnterCriticalSection(&vram_crit_sect);
#endif
		int vram = pcbios_get_shadow_buffer_address(mem[0x462]);
		write_text_vram_char(dest - vram, CPU_AL);
#ifdef USE_VRAM_THREAD
		LeaveCriticalSection(&vram_crit_sect);
#endif
		
		if(++co.X == scr_width) {
			co.X = 0;
			if(++co.Y == scr_height) {
				vram_flush();
				WriteConsoleA(hStdout, "\n", 1, &num, NULL);
				cursor_moved = true;
			}
		}
		if(!cursor_moved) {
			co.Y += scr_top;
			SetConsoleCursorPosition(hStdout, co);
			cursor_moved = true;
		}
		mem[dest] = CPU_AL;
	}
}

inline void pcbios_int_10h_0fh()
{
	CPU_AL = mem[0x449];
	CPU_AH = mem[0x44a];
	CPU_BH = mem[0x462];
}

inline void pcbios_int_10h_11h()
{
	switch(CPU_AL) {
	case 0x00:
	case 0x10:
		if(CPU_BH) {
			change_console_size(80, 25); // for Windows10
			if(!pcbios_set_font_size(font_width, (int)CPU_BH)) {
				for(int h = min(font_height, (int)CPU_BH); h <= max(font_height, (int)CPU_BH); h++) {
					if(h != (int)CPU_BH) {
						if(pcbios_set_font_size(font_width, h)) {
							break;
						}
					}
				}
			}
			pcbios_set_console_size(80, (25 * 16) / CPU_BH, true);
		}
		break;
	case 0x01:
	case 0x11:
		change_console_size(80, 28); // for Windows10
		if(!pcbios_set_font_size(font_width, 14)) {
			for(int h = min(font_height, 14); h <= max(font_height, 14); h++) {
				if(h != 14) {
					if(pcbios_set_font_size(font_width, h)) {
						break;
					}
				}
			}
		}
		pcbios_set_console_size(80, 28, true); // 28 = 25 * 16 / 14
		break;
	case 0x02:
	case 0x12:
		change_console_size(80, 25); // for Windows10
		if(!pcbios_set_font_size(8, 8)) {
			bool success = false;
			for(int y = 8; y <= 14; y++) {
				for(int x = min(font_width, 6); x <= max(font_width, 8); x++) {
					if(pcbios_set_font_size(x, y)) {
						success = true;
						break;
					}
				}
			}
			if(!success) {
				pcbios_set_font_size(font_width, font_height);
			}
		}
		pcbios_set_console_size(80, 50, true); // 50 = 25 * 16 / 8
		break;
	case 0x04:
	case 0x14:
		change_console_size(80, 25); // for Windows10
		pcbios_set_font_size(font_width, font_height);
		pcbios_set_console_size(80, 25, true);
		break;
	case 0x18:
		change_console_size(80, 25); // for Windows10
		pcbios_set_font_size(font_width, font_height);
		pcbios_set_console_size(80, 25, true);
		break;
	case 0x30:
		CPU_LOAD_SREG(CPU_ES_INDEX, 0x0000);
		CPU_BP = 0;
		CPU_CX = mem[0x485];
		CPU_DL = mem[0x484];
		break;
	default:
		unimplemented_10h("int %02Xh (AX=%04X BX=%04X CX=%04X DX=%04X SI=%04X DI=%04X DS=%04X ES=%04X)\n", 0x10, CPU_AX, CPU_BX, CPU_CX, CPU_DX, CPU_SI, CPU_DI, CPU_DS, CPU_ES);
		CPU_SET_C_FLAG(1);
		break;
	}
}

inline void pcbios_int_10h_12h()
{
	switch(CPU_BL) {
	case 0x10:
		CPU_BX = 0x0003;
		CPU_CX = 0x0009;
		break;
	}
}

inline void pcbios_int_10h_13h()
{
	int ofs = CPU_ES_BASE + CPU_BP;
	COORD co;
	DWORD num;
	
	co.X = CPU_DL;
	co.Y = CPU_DH + scr_top;
	
	vram_flush();
	
	switch(CPU_AL) {
	case 0x00:
	case 0x01:
		if(mem[0x462] == CPU_BH) {
			HANDLE hStdout = GetStdHandle(STD_OUTPUT_HANDLE);
			CONSOLE_SCREEN_BUFFER_INFO csbi;
			GetConsoleScreenBufferInfo(hStdout, &csbi);
			SetConsoleCursorPosition(hStdout, co);
			
			if(csbi.wAttributes != CPU_BL) {
				SetConsoleTextAttribute(hStdout, CPU_BL);
			}
			WriteConsoleA(hStdout, &mem[ofs], CPU_CX, &num, NULL);
			
			if(csbi.wAttributes != CPU_BL) {
				SetConsoleTextAttribute(hStdout, csbi.wAttributes);
			}
			if(CPU_AL == 0x00) {
				if(!restore_console_size) {
					GetConsoleScreenBufferInfo(hStdout, &csbi);
					scr_top = csbi.srWindow.Top;
				}
				co.X = mem[0x450 + CPU_BH * 2];
				co.Y = mem[0x451 + CPU_BH * 2] + scr_top;
				SetConsoleCursorPosition(hStdout, co);
			} else {
				cursor_moved = true;
			}
			cursor_moved_by_crtc = false;
		} else {
			CPU_SET_C_FLAG(1);
		}
		break;
	case 0x02:
	case 0x03:
		if(mem[0x462] == CPU_BH) {
			HANDLE hStdout = GetStdHandle(STD_OUTPUT_HANDLE);
			CONSOLE_SCREEN_BUFFER_INFO csbi;
			GetConsoleScreenBufferInfo(hStdout, &csbi);
			SetConsoleCursorPosition(hStdout, co);
			
			WORD wAttributes = csbi.wAttributes;
			for(int i = 0; i < CPU_CX; i++, ofs += 2) {
				if(wAttributes != mem[ofs + 1]) {
					SetConsoleTextAttribute(hStdout, mem[ofs + 1]);
					wAttributes = mem[ofs + 1];
				}
				WriteConsoleA(hStdout, &mem[ofs], 1, &num, NULL);
			}
			if(csbi.wAttributes != wAttributes) {
				SetConsoleTextAttribute(hStdout, csbi.wAttributes);
			}
			if(CPU_AL == 0x02) {
				co.X = mem[0x450 + CPU_BH * 2];
				co.Y = mem[0x451 + CPU_BH * 2] + scr_top;
				SetConsoleCursorPosition(hStdout, co);
			} else {
				cursor_moved = true;
			}
			cursor_moved_by_crtc = false;
		} else {
			CPU_SET_C_FLAG(1);
		}
		break;
	case 0x10:
	case 0x11:
		if(mem[0x462] == CPU_BH) {
			HANDLE hStdout = GetStdHandle(STD_OUTPUT_HANDLE);
			ReadConsoleOutputCharacterA(hStdout, scr_char, CPU_CX, co, &num);
			ReadConsoleOutputAttribute(hStdout, scr_attr, CPU_CX, co, &num);
			for(int i = 0; i < num; i++) {
				mem[ofs++] = scr_char[i];
				mem[ofs++] = scr_attr[i];
				if(CPU_AL & 0x01) {
					mem[ofs++] = 0;
					mem[ofs++] = 0;
				}
			}
		} else {
			for(int i = 0, src = pcbios_get_shadow_buffer_address(CPU_BH, co.X, co.Y - scr_top); i < CPU_CX; i++) {
				mem[ofs++] = mem[src++];
				mem[ofs++] = mem[src++];
				if(CPU_AL & 0x01) {
					mem[ofs++] = 0;
					mem[ofs++] = 0;
				}
				if(++co.X == scr_width) {
					if(++co.Y == scr_height) {
						break;
					}
					co.X = 0;
				}
			}
		}
		break;
	case 0x12: // ???
	case 0x13: // ???
	case 0x20:
	case 0x21:
		if(mem[0x462] == CPU_BH) {
			HANDLE hStdout = GetStdHandle(STD_OUTPUT_HANDLE);
			int len = min(CPU_CX, scr_width * scr_height);
			for(int i = 0; i < len; i++) {
				scr_char[i] = mem[ofs++];
				scr_attr[i] = mem[ofs++];
				if(CPU_AL & 0x01) {
					ofs += 2;
				}
			}
			MyWriteConsoleOutputCharacterA(hStdout, scr_char, len, co, &num);
			WriteConsoleOutputAttribute(hStdout, scr_attr, len, co, &num);
		} else {
			for(int i = 0, dest = pcbios_get_shadow_buffer_address(CPU_BH, co.X, co.Y - scr_top); i < CPU_CX; i++) {
				mem[dest++] = mem[ofs++];
				mem[dest++] = mem[ofs++];
				if(CPU_AL & 0x01) {
					ofs += 2;
				}
				if(++co.X == scr_width) {
					if(++co.Y == scr_height) {
						break;
					}
					co.X = 0;
				}
			}
		}
		break;
	default:
		unimplemented_10h("int %02Xh (AX=%04X BX=%04X CX=%04X DX=%04X SI=%04X DI=%04X DS=%04X ES=%04X)\n", 0x10, CPU_AX, CPU_BX, CPU_CX, CPU_DX, CPU_SI, CPU_DI, CPU_DS, CPU_ES);
		CPU_SET_C_FLAG(1);
		break;
	}
}

inline void pcbios_int_10h_18h()
{
	switch(CPU_AL) {
	case 0x00:
	case 0x01:
//		CPU_AL = 0x86;
		CPU_AL = 0x00;
		break;
	default:
		unimplemented_10h("int %02Xh (AX=%04X BX=%04X CX=%04X DX=%04X SI=%04X DI=%04X DS=%04X ES=%04X)\n", 0x10, CPU_AX, CPU_BX, CPU_CX, CPU_DX, CPU_SI, CPU_DI, CPU_DS, CPU_ES);
		CPU_SET_C_FLAG(1);
		break;
	}
}

inline void pcbios_int_10h_1ah()
{
	switch(CPU_AL) {
	case 0x00:
		CPU_AL = 0x1a;
		CPU_BL = 0x08;
		CPU_BH = 0x00;
		break;
	default:
		unimplemented_10h("int %02Xh (AX=%04X BX=%04X CX=%04X DX=%04X SI=%04X DI=%04X DS=%04X ES=%04X)\n", 0x10, CPU_AX, CPU_BX, CPU_CX, CPU_DX, CPU_SI, CPU_DI, CPU_DS, CPU_ES);
		CPU_SET_C_FLAG(1);
		break;
	}
}

inline void pcbios_int_10h_1bh()
{
	if(CPU_BX) {
		unimplemented_10h("int %02Xh (AX=%04X BX=%04X CX=%04X DX=%04X SI=%04X DI=%04X DS=%04X ES=%04X)\n", 0x10, CPU_AX, CPU_BX, CPU_CX, CPU_DX, CPU_SI, CPU_DI, CPU_DS, CPU_ES);
		CPU_SET_C_FLAG(1);
		return;
	}
	int offs = CPU_ES_BASE + CPU_DI;
	memset(mem + offs, 0, 64);
	memcpy(mem + offs + 4, mem + 0x449, 0x19);
	mem[offs + 0x22] = mem[0x484];
	CPU_AL = 0x1b;
}

inline void pcbios_int_10h_1dh()
{
	switch(CPU_AL) {
	case 0x00:
		// DOS/V Shift Status Line Control is not supported
		CPU_SET_C_FLAG(1);
		break;
	case 0x01:
		break;
	case 0x02:
		CPU_BX = 0;
		break;
	default:
		unimplemented_10h("int %02Xh (AX=%04X BX=%04X CX=%04X DX=%04X SI=%04X DI=%04X DS=%04X ES=%04X)\n", 0x10, CPU_AX, CPU_BX, CPU_CX, CPU_DX, CPU_SI, CPU_DI, CPU_DS, CPU_ES);
		CPU_SET_C_FLAG(1);
		break;
	}
}

inline void pcbios_int_10h_4fh()
{
	switch(CPU_AL) {
	case 0x00:
		CPU_AH = 0x02; // not supported
		break;
	case 0x01:
	case 0x02:
	case 0x03:
	case 0x04:
	case 0x05:
	case 0x06:
	case 0x07:
	case 0x08:
	case 0x09:
	case 0x0a:
	case 0x0b:
	case 0x0c:
		CPU_AH = 0x01; // failed
		break;
	default:
		unimplemented_10h("int %02Xh (AX=%04X BX=%04X CX=%04X DX=%04X SI=%04X DI=%04X DS=%04X ES=%04X)\n", 0x10, CPU_AX, CPU_BX, CPU_CX, CPU_DX, CPU_SI, CPU_DI, CPU_DS, CPU_ES);
		CPU_SET_C_FLAG(1);
		break;
	}
}

inline void pcbios_int_10h_82h()
{
	static UINT8 mode = 0;
	
	switch(CPU_AL) {
	case 0x00:
		if(CPU_BL != 0xff) {
			mode = CPU_BL;
		}
		CPU_AL = mode;
		break;
	default:
		unimplemented_10h("int %02Xh (AX=%04X BX=%04X CX=%04X DX=%04X SI=%04X DI=%04X DS=%04X ES=%04X)\n", 0x10, CPU_AX, CPU_BX, CPU_CX, CPU_DX, CPU_SI, CPU_DI, CPU_DS, CPU_ES);
		CPU_SET_C_FLAG(1);
		break;
	}
}

inline void pcbios_int_10h_83h()
{
	static UINT8 mode = 0;
	
	switch(CPU_AL) {
	case 0x00:
		CPU_AX = 0; // offset???
		CPU_LOAD_SREG(CPU_ES_INDEX, SHADOW_BUF_TOP >> 4);
		CPU_BX = (SHADOW_BUF_TOP & 0x0f);
		break;
	default:
		unimplemented_10h("int %02Xh (AX=%04X BX=%04X CX=%04X DX=%04X SI=%04X DI=%04X DS=%04X ES=%04X)\n", 0x10, CPU_AX, CPU_BX, CPU_CX, CPU_DX, CPU_SI, CPU_DI, CPU_DS, CPU_ES);
		CPU_SET_C_FLAG(1);
		break;
	}
}

inline void pcbios_int_10h_90h()
{
	CPU_AL = mem[0x449];
}

inline void pcbios_int_10h_91h()
{
	CPU_AL = 0x04; // VGA
}

inline void pcbios_int_10h_efh()
{
	CPU_DX = 0xffff;
}

inline void pcbios_int_10h_feh()
{
	if(mem[0x449] == 0x03 || mem[0x449] == 0x70 || mem[0x449] == 0x71 || mem[0x449] == 0x73) {
		CPU_LOAD_SREG(CPU_ES_INDEX, SHADOW_BUF_TOP >> 4);
		CPU_DI = (SHADOW_BUF_TOP & 0x0f);
	}
	int_10h_feh_called = true;
}

inline void pcbios_int_10h_ffh()
{
	if(mem[0x449] == 0x03 || mem[0x449] == 0x70 || mem[0x449] == 0x71 || mem[0x449] == 0x73) {
		HANDLE hStdout = GetStdHandle(STD_OUTPUT_HANDLE);
		COORD co;
		DWORD num;
		
		vram_flush();
		
		co.X = (CPU_DI >> 1) % scr_width;
		co.Y = (CPU_DI >> 1) / scr_width;
		int ofs = pcbios_get_shadow_buffer_address(0, co.X, co.Y);
		int end = min(ofs + CPU_CX * 2, pcbios_get_shadow_buffer_address(0, 0, scr_height));
		int len;
		for(len = 0; ofs < end; len++) {
			scr_char[len] = mem[ofs++];
			scr_attr[len] = mem[ofs++];
		}
		co.Y += scr_top;
		MyWriteConsoleOutputCharacterA(hStdout, scr_char, len, co, &num);
		WriteConsoleOutputAttribute(hStdout, scr_attr, len, co, &num);
	}
	int_10h_ffh_called = true;
}

int pcbios_update_drive_param(int drive_num, int force_update)
{
	if(drive_num >= 0 && drive_num < 26) {
		drive_param_t *drive_param = &drive_params[drive_num];
		
		if(force_update || !drive_param->initialized) {
			drive_param->valid = 0;
			char dev[64];
			sprintf(dev, "\\\\.\\%c:", 'A' + drive_num);
			
			HANDLE hFile = CreateFileA(dev, 0, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
			if(hFile != INVALID_HANDLE_VALUE) {
				DWORD dwSize;
				if(DeviceIoControl(hFile, IOCTL_DISK_GET_DRIVE_GEOMETRY, NULL, 0, &drive_param->geometry, sizeof(DISK_GEOMETRY), &dwSize, NULL)) {
					drive_param->valid = 1;
				}
				CloseHandle(hFile);
			}
			drive_param->initialized = 1;
		}
		return(drive_param->valid);
	}
	return(0);
}

inline void pcbios_int_13h_00h()
{
	int drive_num = (CPU_DL & 0x80) ? ((CPU_DL & 0x7f) + 2) : (CPU_DL < 2) ? CPU_DL : -1;
	
	if(pcbios_update_drive_param(drive_num, 1)) {
		CPU_AH = 0x00; // successful completion
	} else {
		if(CPU_DL & 0x80) {
			CPU_AH = 0x05; // reset failed (hard disk)
		} else {
			CPU_AH = 0x80; // timeout (not ready)
		}
		CPU_SET_C_FLAG(1);
	}
}

inline void pcbios_int_13h_02h()
{
	int drive_num = (CPU_DL & 0x80) ? ((CPU_DL & 0x7f) + 2) : (CPU_DL < 2) ? CPU_DL : -1;
	
	if(CPU_AL == 0) {
		CPU_AH = 0x01; // invalid function in AH or invalid parameter
		CPU_SET_C_FLAG(1);
	} else if(!pcbios_update_drive_param(drive_num, 0)) {
		CPU_AH = 0xff; // sense operation failed (hard disk)
		CPU_SET_C_FLAG(1);
	} else {
		drive_param_t *drive_param = &drive_params[drive_num];
		DISK_GEOMETRY *geo = &drive_param->geometry;
		char dev[64];
		sprintf(dev, "\\\\.\\%c:", 'A' + drive_num);
		
		HANDLE hFile = CreateFileA(dev, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL | FILE_FLAG_NO_BUFFERING, NULL);
		if(hFile == INVALID_HANDLE_VALUE) {
			CPU_AH = 0xff; // sense operation failed (hard disk)
			CPU_SET_C_FLAG(1);
		} else {
			UINT32 sector_num = CPU_AL;
			UINT32 cylinder = CPU_CH | ((CPU_CL & 0xc0) << 2);
			UINT32 head = CPU_DH;
			UINT32 sector = CPU_CL & 0x3f;
			UINT32 top_sector = ((cylinder * drive_param->head_num() + head) * geo->SectorsPerTrack) + sector - 1;
			UINT32 buffer_addr = CPU_ES_BASE + CPU_BX;
			DWORD dwSize;
			
//			if(DeviceIoControl(hFile, FSCTL_LOCK_VOLUME, NULL, 0, NULL, 0, &dwSize, NULL) == 0) {
//				CPU_AH = 0xff; // sense operation failed (hard disk)
//				CPU_SET_C_FLAG(1);
//			} else 
			if(SetFilePointer(hFile, top_sector * geo->BytesPerSector, NULL, FILE_BEGIN) == INVALID_SET_FILE_POINTER) {
				CPU_AH = 0x04; // sector not found/read error
				CPU_SET_C_FLAG(1);
			} else if(ReadFile(hFile, mem + buffer_addr, sector_num * geo->BytesPerSector, &dwSize, NULL) == 0) {
				CPU_AH = 0x04; // sector not found/read error
				CPU_SET_C_FLAG(1);
			} else {
				CPU_AH = 0x00; // successful completion
			}
			CloseHandle(hFile);
		}
	}
}

inline void pcbios_int_13h_03h()
{
	// this operation may cause serious damage for drives, so support only floppy disk...
	int drive_num = (CPU_DL & 0x80) ? ((CPU_DL & 0x7f) + 2) : (CPU_DL < 2) ? CPU_DL : -1;
	
	if(CPU_AL == 0) {
		CPU_AH = 0x01; // invalid function in AH or invalid parameter
		CPU_SET_C_FLAG(1);
	} else if(!pcbios_update_drive_param(drive_num, 0)) {
		CPU_AH = 0xff; // sense operation failed (hard disk)
		CPU_SET_C_FLAG(1);
	} else if(!drive_params[drive_num].is_fdd()) {
		CPU_AH = 0xff; // sense operation failed (hard disk)
		CPU_SET_C_FLAG(1);
	} else {
		drive_param_t *drive_param = &drive_params[drive_num];
		DISK_GEOMETRY *geo = &drive_param->geometry;
		char dev[64];
		sprintf(dev, "\\\\.\\%c:", 'A' + drive_num);
		
		HANDLE hFile = CreateFileA(dev, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL | FILE_FLAG_NO_BUFFERING, NULL);
		if(hFile == INVALID_HANDLE_VALUE) {
			CPU_AH = 0xff; // sense operation failed (hard disk)
			CPU_SET_C_FLAG(1);
		} else {
			UINT32 sector_num = CPU_AL;
			UINT32 cylinder = CPU_CH | ((CPU_CL & 0xc0) << 2);
			UINT32 head = CPU_DH;
			UINT32 sector = CPU_CL & 0x3f;
			UINT32 top_sector = ((cylinder * drive_param->head_num() + head) * geo->SectorsPerTrack) + sector - 1;
			UINT32 buffer_addr = CPU_ES_BASE + CPU_BX;
			DWORD dwSize;
			
//			if(DeviceIoControl(hFile, FSCTL_LOCK_VOLUME, NULL, 0, NULL, 0, &dwSize, NULL) == 0) {
//				CPU_AH = 0xff; // sense operation failed (hard disk)
//				CPU_SET_C_FLAG(1);
//			} else 
			if(SetFilePointer(hFile, top_sector * geo->BytesPerSector, NULL, FILE_BEGIN) == INVALID_SET_FILE_POINTER) {
				CPU_AH = 0x04; // sector not found/read error
				CPU_SET_C_FLAG(1);
			} else if(WriteFile(hFile, mem + buffer_addr, sector_num * geo->BytesPerSector, &dwSize, NULL) == 0) {
				CPU_AH = 0x04; // sector not found/read error
				CPU_SET_C_FLAG(1);
			} else {
				CPU_AH = 0x00; // successful completion
			}
			CloseHandle(hFile);
		}
	}
}

inline void pcbios_int_13h_04h()
{
	int drive_num = (CPU_DL & 0x80) ? ((CPU_DL & 0x7f) + 2) : (CPU_DL < 2) ? CPU_DL : -1;
	
	if(CPU_AL == 0) {
		CPU_AH = 0x01; // invalid function in AH or invalid parameter
		CPU_SET_C_FLAG(1);
	} else if(!pcbios_update_drive_param(drive_num, 0)) {
		CPU_AH = 0xff; // sense operation failed (hard disk)
		CPU_SET_C_FLAG(1);
	} else {
		drive_param_t *drive_param = &drive_params[drive_num];
		DISK_GEOMETRY *geo = &drive_param->geometry;
		char dev[64];
		sprintf(dev, "\\\\.\\%c:", 'A' + drive_num);
		
		HANDLE hFile = CreateFileA(dev, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL | FILE_FLAG_NO_BUFFERING, NULL);
		if(hFile == INVALID_HANDLE_VALUE) {
			CPU_AH = 0xff; // sense operation failed (hard disk)
			CPU_SET_C_FLAG(1);
		} else {
			UINT32 sector_num = CPU_AL;
			UINT32 cylinder = CPU_CH | ((CPU_CL & 0xc0) << 2);
			UINT32 head = CPU_DH;
			UINT32 sector = CPU_CL & 0x3f;
			UINT32 top_sector = ((cylinder * drive_param->head_num() + head) * geo->SectorsPerTrack) + sector - 1;
			UINT32 buffer_addr = CPU_ES_BASE + CPU_BX;
			DWORD dwSize;
			UINT8 *tmp_buffer = (UINT8 *)malloc(sector_num * geo->BytesPerSector);
			
//			if(DeviceIoControl(hFile, FSCTL_LOCK_VOLUME, NULL, 0, NULL, 0, &dwSize, NULL) == 0) {
//				CPU_AH = 0xff; // sense operation failed (hard disk)
//				CPU_SET_C_FLAG(1);
//			} else 
			if(SetFilePointer(hFile, top_sector * geo->BytesPerSector, NULL, FILE_BEGIN) == INVALID_SET_FILE_POINTER) {
				CPU_AH = 0x04; // sector not found/read error
				CPU_SET_C_FLAG(1);
			} else if(ReadFile(hFile, tmp_buffer, sector_num * geo->BytesPerSector, &dwSize, NULL) == 0) {
				CPU_AH = 0x04; // sector not found/read error
				CPU_SET_C_FLAG(1);
			} else if(memcmp(mem + buffer_addr, tmp_buffer, sector_num * geo->BytesPerSector) != 0) {
				CPU_AH = 0x05; // data did not verify correctly (TI Professional PC)
				CPU_SET_C_FLAG(1);
			} else {
				CPU_AH = 0x00; // successful completion
			}
			free(tmp_buffer);
			CloseHandle(hFile);
		}
	}
}

inline void pcbios_int_13h_08h()
{
	int drive_num = (CPU_DL & 0x80) ? ((CPU_DL & 0x7f) + 2) : (CPU_DL < 2) ? CPU_DL : -1;
	
	if(pcbios_update_drive_param(drive_num, 1)) {
		drive_param_t *drive_param = &drive_params[drive_num];
		DISK_GEOMETRY *geo = &drive_param->geometry;
		
		CPU_AX = 0x0000;
		switch(geo->MediaType) {
		case F5_360_512:
		case F5_320_512:
		case F5_320_1024:
		case F5_180_512:
		case F5_160_512:
			CPU_BL = 0x01; // 320K/360K disk
			break;
		case F5_1Pt2_512:
		case F3_1Pt2_512:
		case F3_1Pt23_1024:
		case F5_1Pt23_1024:
			CPU_BL = 0x02; // 1.2M disk
			break;
		case F3_720_512:
		case F3_640_512:
		case F5_640_512:
		case F5_720_512:
			CPU_BL = 0x03; // 720K disk
			break;
		case F3_1Pt44_512:
			CPU_BL = 0x04; // 1.44M disk
			break;
		case F3_2Pt88_512:
			CPU_BL = 0x06; // 2.88M disk
			break;
		case RemovableMedia:
			CPU_BL = 0x10; // ATAPI Removable Media Device
			break;
		default:
			CPU_BL = 0x00; // unknown
			break;
		}
		if(CPU_DL & 0x80) {
			switch(GetLogicalDrives() & 0x0c) {
			case 0x00: CPU_DL = 0x00; break;
			case 0x04:
			case 0x08: CPU_DL = 0x01; break;
			case 0x0c: CPU_DL = 0x02; break;
			}
		} else {
			switch(GetLogicalDrives() & 0x03) {
			case 0x00: CPU_DL = 0x00; break;
			case 0x01:
			case 0x02: CPU_DL = 0x01; break;
			case 0x03: CPU_DL = 0x02; break;
			}
		}
		CPU_DH = drive_param->head_num();
		int cyl = (geo->Cylinders.QuadPart > 0x3ff) ? 0x3ff : geo->Cylinders.QuadPart;
		int sec = (geo->SectorsPerTrack > 0x3f) ? 0x3f : geo->SectorsPerTrack;
		CPU_CH = cyl & 0xff;
		CPU_CL = (sec & 0x3f) | ((cyl & 0x300) >> 2);
	} else {
		CPU_AH = 0x07;
		CPU_SET_C_FLAG(1);
	}
}

inline void pcbios_int_13h_10h()
{
	int drive_num = (CPU_DL & 0x80) ? ((CPU_DL & 0x7f) + 2) : (CPU_DL < 2) ? CPU_DL : -1;
	
	if(pcbios_update_drive_param(drive_num, 1)) {
		CPU_AH = 0x00; // successful completion
	} else {
		if(CPU_DL & 0x80) {
			CPU_AH = 0xaa; // drive not ready (hard disk)
		} else {
			CPU_AH = 0x80; // timeout (not ready)
		}
		CPU_SET_C_FLAG(1);
	}
}

inline void pcbios_int_13h_15h()
{
	int drive_num = (CPU_DL & 0x80) ? ((CPU_DL & 0x7f) + 2) : (CPU_DL < 2) ? CPU_DL : -1;
	
	if(pcbios_update_drive_param(drive_num, 1)) {
		if(CPU_DL & 0x80) {
			CPU_AH = 0x02; // floppy (or other removable drive) with change-line support
		} else {
			CPU_AH = 0x03; // hard disk
		}
	} else {
		CPU_AH = 0x00; // no such drive
	}
}

inline void pcbios_int_13h_41h()
{
	if(CPU_BX == 0x55aa) {
		// IBM/MS INT 13 Extensions is not installed
		CPU_AH = 0x01;
		CPU_SET_C_FLAG(1);
	} else {
		unimplemented_13h("int %02Xh (AX=%04X BX=%04X CX=%04X DX=%04X SI=%04X DI=%04X DS=%04X ES=%04X)\n", 0x13, CPU_AX, CPU_BX, CPU_CX, CPU_DX, CPU_SI, CPU_DI, CPU_DS, CPU_ES);
		CPU_AH = 0x01;
		CPU_SET_C_FLAG(1);
	}
}

inline void pcbios_int_14h_00h()
{
	if(CPU_DX < 4) {
		static const unsigned int rate[] = {110, 150, 300, 600, 1200, 2400, 4800, 9600};
		UINT8 selector = sio_read(CPU_DX, 3);
		selector &= ~0x3f;
		selector |= CPU_AL & 0x1f;
		UINT16 divisor = 115200 / rate[CPU_AL >> 5];
		sio_write(CPU_DX, 3, selector | 0x80);
		sio_write(CPU_DX, 0, divisor & 0xff);
		sio_write(CPU_DX, 1, divisor >> 8);
		sio_write(CPU_DX, 3, selector);
		CPU_AH = sio_read(CPU_DX, 5);
		CPU_AL = sio_read(CPU_DX, 6);
	} else {
		CPU_AH = 0x80;
	}
}

inline void pcbios_int_14h_01h()
{
	if(CPU_DX < 4) {
		UINT8 selector = sio_read(CPU_DX, 3);
		sio_write(CPU_DX, 3, selector & ~0x80);
		sio_write(CPU_DX, 0, CPU_AL);
		sio_write(CPU_DX, 3, selector);
		CPU_AH = sio_read(CPU_DX, 5);
	} else {
		CPU_AH = 0x80;
	}
}

inline void pcbios_int_14h_02h()
{
	if(CPU_DX < 4) {
		UINT8 selector = sio_read(CPU_DX, 3);
		sio_write(CPU_DX, 3, selector & ~0x80);
		CPU_AL = sio_read(CPU_DX, 0);
		sio_write(CPU_DX, 3, selector);
		CPU_AH = sio_read(CPU_DX, 5);
	} else {
		CPU_AH = 0x80;
	}
}

inline void pcbios_int_14h_03h()
{
	if(CPU_DX < 4) {
		CPU_AH = sio_read(CPU_DX, 5);
		CPU_AL = sio_read(CPU_DX, 6);
	} else {
		CPU_AH = 0x80;
	}
}

inline void pcbios_int_14h_04h()
{
	if(CPU_DX < 4) {
		UINT8 selector = sio_read(CPU_DX, 3);
		if(CPU_CH <= 0x03) {
			selector = (selector & ~0x03) | CPU_CH;
		}
		if(CPU_BL == 0x00) {
			selector &= ~0x04;
		} else if(CPU_BL == 0x01) {
			selector |= 0x04;
		}
		if(CPU_BH == 0x00) {
			selector = (selector & ~0x38) | 0x00;
		} else if(CPU_BH == 0x01) {
			selector = (selector & ~0x38) | 0x08;
		} else if(CPU_BH == 0x02) {
			selector = (selector & ~0x38) | 0x18;
		} else if(CPU_BH == 0x03) {
			selector = (selector & ~0x38) | 0x28;
		} else if(CPU_BH == 0x04) {
			selector = (selector & ~0x38) | 0x38;
		}
		if(CPU_AL == 0x00) {
			selector |= 0x40;
		} else if(CPU_AL == 0x01) {
			selector &= ~0x40;
		}
		if(CPU_CL <= 0x0b) {
			static const unsigned int rate[] = {110, 150, 300, 600, 1200, 2400, 4800, 9600, 19200, 38400, 57600, 115200};
			UINT16 divisor = 115200 / rate[CPU_CL];
			sio_write(CPU_DX, 3, selector | 0x80);
			sio_write(CPU_DX, 0, divisor & 0xff);
			sio_write(CPU_DX, 1, divisor >> 8);
		}
		sio_write(CPU_DX, 3, selector);
		CPU_AH = sio_read(CPU_DX, 5);
		CPU_AL = sio_read(CPU_DX, 6);
	} else {
		CPU_AH = 0x80;
	}
}

inline void pcbios_int_14h_05h()
{
	if(CPU_DX < 4) {
		if(CPU_AL == 0x00) {
			CPU_BL = sio_read(CPU_DX, 4);
			CPU_AH = sio_read(CPU_DX, 5);
			CPU_AL = sio_read(CPU_DX, 6);
		} else if(CPU_AL == 0x01) {
			sio_write(CPU_DX, 4, CPU_BL);
			CPU_AH = sio_read(CPU_DX, 5);
			CPU_AL = sio_read(CPU_DX, 6);
		} else {
			unimplemented_14h("int %02Xh (AX=%04X BX=%04X CX=%04X DX=%04X SI=%04X DI=%04X DS=%04X ES=%04X)\n", 0x14, CPU_AX, CPU_BX, CPU_CX, CPU_DX, CPU_SI, CPU_DI, CPU_DS, CPU_ES);
		}
	} else {
		CPU_AH = 0x80;
	}
}

inline void pcbios_int_15h_23h()
{
	switch(CPU_AL) {
	case 0x00:
		CPU_CL = cmos_read(0x2d);
		CPU_CH = cmos_read(0x2e);
		break;
	case 0x01:
		cmos_write(0x2d, CPU_CL);
		cmos_write(0x2e, CPU_CH);
		break;
	default:
		unimplemented_15h("int %02Xh (AX=%04X BX=%04X CX=%04X DX=%04X SI=%04X DI=%04X DS=%04X ES=%04X)\n", 0x15, CPU_AX, CPU_BX, CPU_CX, CPU_DX, CPU_SI, CPU_DI, CPU_DS, CPU_ES);
		CPU_AH = 0x86;
		CPU_SET_C_FLAG(1);
		break;
	}
}

inline void pcbios_int_15h_24h()
{
	switch(CPU_AL) {
	case 0x00:
		CPU_A20_LINE(0);
		CPU_AH = 0;
		break;
	case 0x01:
		CPU_A20_LINE(1);
		CPU_AH = 0;
		break;
	case 0x02:
		CPU_AH = 0;
		CPU_AL = (CPU_ADRSMASK >> 20) & 1;
		CPU_CX = 0;
		break;
	case 0x03:
		CPU_AX = 0;
		CPU_BX = 0;
		break;
	default:
		unimplemented_15h("int %02Xh (AX=%04X BX=%04X CX=%04X DX=%04X SI=%04X DI=%04X DS=%04X ES=%04X)\n", 0x15, CPU_AX, CPU_BX, CPU_CX, CPU_DX, CPU_SI, CPU_DI, CPU_DS, CPU_ES);
		CPU_AH = 0x86;
		CPU_SET_C_FLAG(1);
		break;
	}
}

inline void pcbios_int_15h_49h()
{
	CPU_AH = 0x00;
	CPU_BL = 0x00; // DOS/V
//	CPU_BL = 0x01; // standard DBCS DOS (hardware DBCS support)
}

inline void pcbios_int_15h_50h()
{
	switch(CPU_AL) {
	case 0x00:
	case 0x01:
		if(CPU_BH != 0x00 && CPU_BH != 0x01) {
			CPU_AH = 0x01; // invalid font type in bh
			CPU_SET_C_FLAG(1);
		} else if(CPU_BL != 0x00) {
			CPU_AH = 0x02; // bl not zero
			CPU_SET_C_FLAG(1);
		} else if(CPU_BP != 0 && CPU_BP != 437 && CPU_BP != 932 && CPU_BP != 934 && CPU_BP != 936 && CPU_BP != 938) {
			CPU_AH = 0x04; // invalid code page
			CPU_SET_C_FLAG(1);
		} else if(CPU_AL == 0x01) {
			CPU_AH = 0x06; // font is read only
			CPU_SET_C_FLAG(1);
		} else {
			// dummy font read routine is at fffc:000d
			CPU_LOAD_SREG(CPU_ES_INDEX, DUMMY_TOP >> 4);
			CPU_BX = 0x000d;
			CPU_AH = 0x00; // success
		}
		break;
	default:
		unimplemented_15h("int %02Xh (AX=%04X BX=%04X CX=%04X DX=%04X SI=%04X DI=%04X DS=%04X ES=%04X)\n", 0x15, CPU_AX, CPU_BX, CPU_CX, CPU_DX, CPU_SI, CPU_DI, CPU_DS, CPU_ES);
		CPU_AH = 0x86;
		CPU_SET_C_FLAG(1);
		break;
	}
}

inline void pcbios_int_15h_53h()
{
	switch(CPU_AL) {
	case 0x00:
		// APM is not installed
		CPU_AH = 0x86;
		CPU_SET_C_FLAG(1);
		break;
	default:
		unimplemented_15h("int %02Xh (AX=%04X BX=%04X CX=%04X DX=%04X SI=%04X DI=%04X DS=%04X ES=%04X)\n", 0x15, CPU_AX, CPU_BX, CPU_CX, CPU_DX, CPU_SI, CPU_DI, CPU_DS, CPU_ES);
		CPU_AH = 0x86;
		CPU_SET_C_FLAG(1);
		break;
	}
}

inline void pcbios_int_15h_84h()
{
	// joystick support (from DOSBox)
	switch(CPU_DX) {
	case 0x00:
		CPU_AX = 0x00f0;
		CPU_DX = 0x0201;
		CPU_SET_C_FLAG(1);
		break;
	case 0x01:
		CPU_AX = CPU_BX = CPU_CX = CPU_DX = 0x0000;
		CPU_SET_C_FLAG(1);
		break;
	default:
		unimplemented_15h("int %02Xh (AX=%04X BX=%04X CX=%04X DX=%04X SI=%04X DI=%04X DS=%04X ES=%04X)\n", 0x15, CPU_AX, CPU_BX, CPU_CX, CPU_DX, CPU_SI, CPU_DI, CPU_DS, CPU_ES);
		CPU_AH = 0x86;
		CPU_SET_C_FLAG(1);
		break;
	}
}

DWORD WINAPI pcbios_int_15h_86h_thread(LPVOID)
{
	UINT32 usec = (CPU_CX_in_service << 16) | CPU_DX_in_service;
	UINT32 msec = usec / 1000;
	
	while(msec && !msdos_exit) {
		UINT32 tmp = min(msec, 100);
		if(msec - tmp < 10) {
			tmp = msec;
		}
		Sleep(tmp);
		msec -= tmp;
	}
	
#ifdef USE_SERVICE_THREAD
	service_exit = true;
#endif
	return(0);
}

inline void pcbios_int_15h_86h()
{
	if(!(CPU_CX == 0 && CPU_DX == 0)) {
#ifdef USE_SERVICE_THREAD
		if(!in_service && !in_service_29h) {
			start_service_loop(pcbios_int_15h_86h_thread);
		} else {
#endif
			prepare_service_loop();
			pcbios_int_15h_86h_thread(NULL);
			cleanup_service_loop();
			REQUEST_HARDWRE_UPDATE();
#ifdef USE_SERVICE_THREAD
		}
#endif
	}
}

inline void pcbios_int_15h_87h()
{
	// copy extended memory (from DOSBox)
	int len = CPU_CX * 2;
	int ofs = CPU_ES_BASE + CPU_SI;
#if defined(HAS_I386)
	int src = (*(UINT32 *)(mem + ofs + 0x12) & 0xffffff) + ((UINT32)mem[ofs + 0x16] << 24);
	int dst = (*(UINT32 *)(mem + ofs + 0x1a) & 0xffffff) + ((UINT32)mem[ofs + 0x1e] << 24);
#else
	int src = (*(UINT32 *)(mem + ofs + 0x12) & 0xffffff); // + ((UINT32)mem[ofs + 0x16] << 24);
	int dst = (*(UINT32 *)(mem + ofs + 0x1a) & 0xffffff); // + ((UINT32)mem[ofs + 0x1e] << 24);
#endif
#if 1
	UINT8 *buffer = (UINT8 *)malloc(len);
	memcpy(buffer, mem + src, len);
	memcpy(mem + dst, buffer, len);
	free(buffer);
#else
	memcpy(mem + dst, mem + src, len);
#endif
//	CPU_AH = 0x00;
	CPU_AX = 0x00; // from DOSBox
}

inline void pcbios_int_15h_88h()
{
	if(support_ems) {
		CPU_AX = 0x00; // from DOSBox
	} else {
		CPU_AX = ((min(MAX_MEM, 0x4000000) - 0x100000) >> 10);
	}
}

inline void pcbios_int_15h_89h()
{
#if defined(HAS_I286) || defined(HAS_I386)
	// switch to protected mode (from DOSBox)
	write_io_byte(0x20, 0x10);	// icw1
	write_io_byte(0x21, CPU_BH);	// icw2
	write_io_byte(0x21, 0x00);	// icw3
//	write_io_byte(0x21, 0xff);
	write_io_byte(0xa0, 0x10);	// icw1
	write_io_byte(0xa1, CPU_BL);	// icw2
	write_io_byte(0xa1, 0x00);	// icw3
//	write_io_byte(0xa1, 0xff);
	CPU_A20_LINE(1);
	CPU_GDTR_LIMIT = *(UINT16 *)(mem + CPU_ES_BASE + CPU_SI + 0x08);
	CPU_GDTR_BASE  = *(UINT32 *)(mem + CPU_ES_BASE + CPU_SI + 0x08 + 0x02) & 0xffffff;
	CPU_IDTR_LIMIT = *(UINT16 *)(mem + CPU_ES_BASE + CPU_SI + 0x10);
	CPU_IDTR_BASE  = *(UINT32 *)(mem + CPU_ES_BASE + CPU_SI + 0x10 + 0x02) & 0xffffff;
#if defined(HAS_I386)
	CPU_SET_CR0(CPU_CR0 | 1);
#else
	CPU_MSW |= 1;
#endif
	CPU_LOAD_SREG(CPU_DS_INDEX, 0x0018);
	CPU_LOAD_SREG(CPU_ES_INDEX, 0x0020);
	CPU_LOAD_SREG(CPU_SS_INDEX, 0x0028);
//	UINT16 offset = *(UINT16 *)(mem + CPU_SS_BASE + CPU_SP);
	CPU_SP += 6; // clear stack of interrupt frame
	UINT32 flags = CPU_EFLAG;
	flags &= ~0x247fd5; // clear CF,PF,AF,ZF,SF,TF,IF,DF,OF,IOPL,NT,AC,ID
	CPU_SET_EFLAG(flags);
	CPU_AX = 0x00;
	CPU_JMP_FAR(0x30, CPU_CX/*offset*/);
#else
	// i86/i186/v30: protected mode is not supported
	CPU_AH = 0x86;
	CPU_SET_C_FLAG(1);
#endif
}

inline void pcbios_int_15h_8ah()
{
	UINT32 size = MAX_MEM - 0x100000;
	CPU_AX = size & 0xffff;
	CPU_DX = size >> 16;
}

inline void pcbios_int_15h_c0h()
{
	CPU_LOAD_SREG(CPU_ES_INDEX, BIOS_TOP >> 4);
	CPU_BX = 0xac;
}

#ifdef EXT_BIOS_TOP
inline void pcbios_int_15h_c1h()
{
	CPU_LOAD_SREG(CPU_ES_INDEX, EXT_BIOS_TOP >> 4);
}
#endif

void pcbios_read_from_ps2_mouse(UINT16 *data_1st, UINT16 *data_2nd, UINT16 *data_3rd)
{
	// from DOSBox DoPS2Callback()
	UINT16 mdat = 0x08;
	INT16 xdiff = mouse.position.x - mouse.prev_position.x;
	INT16 ydiff = mouse.prev_position.y - mouse.position.y;
	
#if 1
	if(xdiff > +16) xdiff = +16;
	if(xdiff < -16) xdiff = -16;
	if(ydiff > +16) ydiff = +16;
	if(ydiff < -16) ydiff = -16;
#endif
	
	if(mouse.buttons[0].status) {
		mdat |= 0x01;
	}
	if(mouse.buttons[1].status) {
		mdat |= 0x02;
	}
	mouse.prev_position.x = mouse.position.x;
	mouse.prev_position.y = mouse.position.y;
	
	if((xdiff > 0xff) || (xdiff < -0xff)) {
		mdat |= 0x40;	// x overflow
	}
	if((ydiff > 0xff) || (ydiff < -0xff)) {
		mdat |= 0x80;	// y overflow
	}
	xdiff %= 256;
	ydiff %= 256;
	if(xdiff < 0) {
		xdiff = (0x100 + xdiff);
		mdat |= 0x10;
	}
	if(ydiff < 0) {
		ydiff = (0x100 + ydiff);
		mdat |= 0x20;
	}
	*data_1st = (UINT16)mdat;
	*data_2nd = (UINT16)(xdiff % 256);
	*data_3rd = (UINT16)(ydiff % 256);
}

inline void pcbios_int_15h_c2h()
{
	static UINT8 sampling_rate = 5;
	static UINT8 resolution = 2;
	static UINT8 scaling = 1;
	
	switch(CPU_AL) {
	case 0x00:
		if(CPU_BH == 0x00) {
			if(dwConsoleMode & (ENABLE_INSERT_MODE | ENABLE_QUICK_EDIT_MODE)) {
				SetConsoleMode(GetStdHandle(STD_INPUT_HANDLE), dwConsoleMode | ENABLE_EXTENDED_FLAGS);
			} else {
				SetConsoleMode(GetStdHandle(STD_INPUT_HANDLE), dwConsoleMode);
			}
			pic[1].imr |= 0x10; // disable irq12
			mouse.enabled_ps2 = false;
			CPU_AH = 0x00; // successful
		} else if(CPU_BH == 0x01) {
			SetConsoleMode(GetStdHandle(STD_INPUT_HANDLE), (dwConsoleMode | ENABLE_MOUSE_INPUT | ENABLE_EXTENDED_FLAGS) & ~ENABLE_QUICK_EDIT_MODE);
			pic[1].imr &= ~0x10; // enable irq12
			mouse.enabled_ps2 = true;
			CPU_AH = 0x00; // successful
		} else {
			CPU_AH = 0x01; // invalid function
			CPU_SET_C_FLAG(1);
		}
		break;
	case 0x01:
		CPU_BH = 0x00; // device id
		CPU_BL = 0xaa; // mouse
	case 0x05:
		if(dwConsoleMode & (ENABLE_INSERT_MODE | ENABLE_QUICK_EDIT_MODE)) {
			SetConsoleMode(GetStdHandle(STD_INPUT_HANDLE), dwConsoleMode | ENABLE_EXTENDED_FLAGS);
		} else {
			SetConsoleMode(GetStdHandle(STD_INPUT_HANDLE), dwConsoleMode);
		}
		pic[1].imr |= 0x10; // disable irq12
		mouse.enabled_ps2 = false;
		sampling_rate = 5;
		resolution = 2;
		scaling = 1;
		CPU_AH = 0x00; // successful
		break;
	case 0x02:
		sampling_rate = CPU_BH;
		CPU_AH = 0x00; // successful
		break;
	case 0x03:
		resolution = CPU_BH;
		CPU_AH = 0x00; // successful
		break;
	case 0x04:
		CPU_BH = 0x00; // device id
		CPU_AH = 0x00; // successful
		break;
	case 0x06:
		switch(CPU_BH) {
		case 0x00:
			CPU_BL = 0x00;
			if(mouse.buttons[1].status) {
				CPU_BL |= 0x01;
			}
			if(mouse.buttons[0].status) {
				CPU_BL |= 0x04;
			}
			if(scaling == 2) {
				CPU_BL |= 0x10;
			}
			CPU_CL = resolution;
			switch(sampling_rate) {
			case 0:  CPU_DL =  10; break;
			case 1:  CPU_DL =  20; break;
			case 2:  CPU_DL =  40; break;
			case 3:  CPU_DL =  60; break;
			case 4:  CPU_DL =  80; break;
//			case 5:  CPU_DL = 100; break;
			case 6:  CPU_DL = 200; break;
			default: CPU_DL = 100; break;
			}
			CPU_AH = 0x00; // successful
			break;
		case 0x01:
		case 0x02:
			scaling = CPU_BH;
			CPU_AH = 0x00; // successful
			break;
		default:
			unimplemented_15h("int %02Xh (AX=%04X BX=%04X CX=%04X DX=%04X SI=%04X DI=%04X DS=%04X ES=%04X)\n", 0x15, CPU_AX, CPU_BX, CPU_CX, CPU_DX, CPU_SI, CPU_DI, CPU_DS, CPU_ES);
			CPU_AH = 0x01; // invalid function
			CPU_SET_C_FLAG(1);
			break;
		}
		break;
	case 0x07: // set device handler addr
		mouse.call_addr_ps2.w.l = CPU_BX;
		mouse.call_addr_ps2.w.h = CPU_ES;
		CPU_AH = 0x00; // successful
		break;
	case 0x08:
		CPU_AH = 0x00; // successful
		break;
	case 0x09:
		{
			UINT16 data_1st, data_2nd, data_3rd;
			pcbios_read_from_ps2_mouse(&data_1st, &data_2nd, &data_3rd);
			CPU_BL = (UINT8)(data_1st & 0xff);
			CPU_CL = (UINT8)(data_2nd & 0xff);
			CPU_DL = (UINT8)(data_3rd & 0xff);
		}
		CPU_AH = 0x00; // successful
		break;
	default:
		unimplemented_15h("int %02Xh (AX=%04X BX=%04X CX=%04X DX=%04X SI=%04X DI=%04X DS=%04X ES=%04X)\n", 0x15, CPU_AX, CPU_BX, CPU_CX, CPU_DX, CPU_SI, CPU_DI, CPU_DS, CPU_ES);
//		CPU_AH = 0x86;
		CPU_AH = 0x01; // invalid function
		CPU_SET_C_FLAG(1);
		break;
	}
}

#if defined(HAS_I386)
inline void pcbios_int_15h_c9h()
{
	CPU_AH = 0x00;
	CPU_CH = cpu_type;
	CPU_CL = cpu_step;
}
#endif

inline void pcbios_int_15h_cah()
{
	switch(CPU_AL) {
	case 0x00:
		if(CPU_BL > 0x3f) {
			CPU_AH = 0x03;
			CPU_SET_C_FLAG(1);
		} else if(CPU_BL < 0x0e) {
			CPU_AH = 0x04;
			CPU_SET_C_FLAG(1);
		} else {
			CPU_CL = cmos_read(CPU_BL);
		}
		break;
	case 0x01:
		if(CPU_BL > 0x3f) {
			CPU_AH = 0x03;
			CPU_SET_C_FLAG(1);
		} else if(CPU_BL < 0x0e) {
			CPU_AH = 0x04;
			CPU_SET_C_FLAG(1);
		} else {
			cmos_write(CPU_BL, CPU_CL);
		}
		break;
	default:
		unimplemented_15h("int %02Xh (AX=%04X BX=%04X CX=%04X DX=%04X SI=%04X DI=%04X DS=%04X ES=%04X)\n", 0x15, CPU_AX, CPU_BX, CPU_CX, CPU_DX, CPU_SI, CPU_DI, CPU_DS, CPU_ES);
		CPU_AH = 0x86;
		CPU_SET_C_FLAG(1);
		break;
	}
}

inline void pcbios_int_15h_e8h()
{
	switch(CPU_AL) {
	case 0x01:
		CPU_AX = CPU_CX = ((min(MAX_MEM, 0x1000000) - 0x0100000) >> 10);
		CPU_BX = CPU_DX = ((max(MAX_MEM, 0x1000000) - 0x1000000) >> 16);
		break;
#if defined(HAS_I386)
	case 0x20:
		if(CPU_EDX == 0x534d4150 && CPU_ECX >= 20) {
			if(CPU_EBX < 3) {
				UINT32 base = 0, len = 0, type = 0;
				switch(CPU_EBX) {
				case 0:
					base = 0x000000;
					len  = MEMORY_END;
					type = 1;
					break;
				case 1:
					base = DUMMY_TOP;
					len  = 0x100000 - DUMMY_TOP;
					type = 2;
					break;
				case 2:
					base = 0x100000;
					len  = MAX_MEM - 0x100000;
					type = 1;
					break;
				}
				*(UINT32 *)(mem + CPU_ES_BASE + CPU_DI + 0x00) = base;
				*(UINT32 *)(mem + CPU_ES_BASE + CPU_DI + 0x04) = 0;
				*(UINT32 *)(mem + CPU_ES_BASE + CPU_DI + 0x08) = len;
				*(UINT32 *)(mem + CPU_ES_BASE + CPU_DI + 0x0c) = 0;
				*(UINT32 *)(mem + CPU_ES_BASE + CPU_DI + 0x10) = type;
				
				if(++CPU_EBX >= 3) {
					CPU_EBX = 0;
				}
				CPU_ECX = 20;
			} else {
				CPU_SET_C_FLAG(1);
			}
			CPU_EAX = 0x534d4150;
			break;
		}
	case 0x81:
		CPU_EAX = CPU_ECX = ((min(MAX_MEM, 0x1000000) - 0x0100000) >> 10);
		CPU_EBX = CPU_EDX = ((max(MAX_MEM, 0x1000000) - 0x1000000) >> 16);
		break;
#endif
	default:
		unimplemented_15h("int %02Xh (AX=%04X BX=%04X CX=%04X DX=%04X SI=%04X DI=%04X DS=%04X ES=%04X)\n", 0x15, CPU_AX, CPU_BX, CPU_CX, CPU_DX, CPU_SI, CPU_DI, CPU_DS, CPU_ES);
		CPU_AH = 0x86;
		CPU_SET_C_FLAG(1);
		break;
	}
}

bool pcbios_is_key_buffer_empty()
{
	return(*(UINT16 *)(mem + 0x41a) == *(UINT16 *)(mem + 0x41c));
}

void pcbios_clear_key_buffer()
{
	key_buf_char->clear();
	key_buf_scan->clear();
	
	// update key buffer
	*(UINT16 *)(mem + 0x41a) = *(UINT16 *)(mem + 0x41c); // head = tail
}

void pcbios_set_key_buffer(int key_char, int key_scan)
{
	// update key buffer
	UINT16 head = *(UINT16 *)(mem + 0x41a);
	UINT16 tail = *(UINT16 *)(mem + 0x41c);
	UINT16 next = tail + 2;
	if(next >= *(UINT16 *)(mem + 0x482)) {
		next = *(UINT16 *)(mem + 0x480);
	}
	if(next != head) {
		*(UINT16 *)(mem + 0x41c) = next;
		mem[0x400 + (tail++)] = key_char;
		mem[0x400 + (tail++)] = key_scan;
	} else {
		// store to extra key buffer
		if(key_buf_char != NULL && key_buf_scan != NULL) {
			key_buf_char->write(key_char);
			key_buf_scan->write(key_scan);
		}
	}
}

bool pcbios_get_key_buffer(int *key_char, int *key_scan)
{
	// update key buffer
	UINT16 head = *(UINT16 *)(mem + 0x41a);
	UINT16 tail = *(UINT16 *)(mem + 0x41c);
	UINT16 next = head + 2;
	if(next >= *(UINT16 *)(mem + 0x482)) {
		next = *(UINT16 *)(mem + 0x480);
	}
	if(head != tail) {
		*(UINT16 *)(mem + 0x41a) = next;
		*key_char = mem[0x400 + (head++)];
		*key_scan = mem[0x400 + (head++)];
		
		// restore from extra key buffer
		if(key_buf_char != NULL && key_buf_scan != NULL) {
			if(!key_buf_char->empty()) {
				pcbios_set_key_buffer(key_buf_char->read(), key_buf_scan->read());
			}
		}
		return(true);
	} else {
		*key_char = 0x00;
		*key_scan = 0x00;
		return(false);
	}
}

bool pcbios_check_key_buffer(int *key_char, int *key_scan)
{
	// do not remove from key buffer
	UINT16 head = *(UINT16 *)(mem + 0x41a);
	UINT16 tail = *(UINT16 *)(mem + 0x41c);
	if(head != tail) {
		*key_char = mem[0x400 + (head++)];
		*key_scan = mem[0x400 + (head++)];
		return(true);
	} else {
		*key_char = 0x00;
		*key_scan = 0x00;
		return(false);
	}
}

void pcbios_update_key_code(bool wait)
{
	if(key_buf_char != NULL && key_buf_scan != NULL) {
#ifdef USE_SERVICE_THREAD
		EnterCriticalSection(&key_buf_crit_sect);
#endif
		bool empty = pcbios_is_key_buffer_empty();
#ifdef USE_SERVICE_THREAD
		LeaveCriticalSection(&key_buf_crit_sect);
#endif
		if(empty) {
			if(!update_key_buffer()) {
				if(wait) {
					Sleep(10);
				} else {
					maybe_idle();
				}
			}
		}
	}
	if(key_buf_char != NULL && key_buf_scan != NULL) {
#ifdef USE_SERVICE_THREAD
		EnterCriticalSection(&key_buf_crit_sect);
#endif
		int key_char, key_scan;
		if(pcbios_get_key_buffer(&key_char, &key_scan)) {
			key_code  = key_char << 0;
			key_code |= key_scan << 8;
			key_recv  = 0x0000ffff;
		}
		if(pcbios_get_key_buffer(&key_char, &key_scan)) {
			key_code |= key_char << 16;
			key_code |= key_scan << 24;
			key_recv |= 0xffff0000;
		}
#ifdef USE_SERVICE_THREAD
		LeaveCriticalSection(&key_buf_crit_sect);
#endif
	}
}

DWORD WINAPI pcbios_int_16h_00h_thread(LPVOID)
{
	while(key_recv == 0 && !msdos_exit) {
		pcbios_update_key_code(true);
	}
	if((key_recv & 0x0000ffff) && (key_recv & 0xffff0000)) {
		if((key_code & 0xffff) == 0x0000 || (key_code & 0xffff) == 0xe000) {
			if((CPU_AX_in_service >> 8) == 0x10) {
				key_code = ((key_code >> 8) & 0xff) | ((key_code >> 16) & 0xff00);
			} else {
				key_code = ((key_code >> 16) & 0xff00);
			}
			key_recv >>= 16;
		}
	}
	CPU_AX_in_service = key_code & 0xffff;
	
	key_code >>= 16;
	key_recv >>= 16;
	
#ifdef USE_SERVICE_THREAD
	service_exit = true;
#endif
	return(0);
}

inline void pcbios_int_16h_00h()
{
#ifdef USE_SERVICE_THREAD
	if(!in_service && !in_service_29h) {
		start_service_loop(pcbios_int_16h_00h_thread);
	} else {
#endif
		prepare_service_loop();
		pcbios_int_16h_00h_thread(NULL);
		cleanup_service_loop();
		REQUEST_HARDWRE_UPDATE();
#ifdef USE_SERVICE_THREAD
	}
#endif
}

inline void pcbios_int_16h_01h()
{
	if(key_recv == 0) {
		pcbios_update_key_code(false);
	}
	if(key_recv != 0) {
		UINT32 key_code_tmp = key_code;
		if((key_recv & 0x0000ffff) && (key_recv & 0xffff0000)) {
			if((key_code_tmp & 0xffff) == 0x0000 || (key_code_tmp & 0xffff) == 0xe000) {
				if(CPU_AH == 0x11) {
					key_code_tmp = ((key_code_tmp >> 8) & 0xff) | ((key_code_tmp >> 16) & 0xff00);
				} else {
					key_code_tmp = ((key_code_tmp >> 16) & 0xff00);
				}
			}
		}
		CPU_AX = key_code_tmp & 0xffff;
		CPU_SET_Z_FLAG(0);
	} else {
		CPU_SET_Z_FLAG(1);
	}
}

inline void pcbios_int_16h_02h()
{
	CPU_AL  = (GetAsyncKeyState(VK_INSERT ) & 0x0001) ? 0x80 : 0;
	CPU_AL |= (GetAsyncKeyState(VK_CAPITAL) & 0x0001) ? 0x40 : 0;
	CPU_AL |= (GetAsyncKeyState(VK_NUMLOCK) & 0x0001) ? 0x20 : 0;
	CPU_AL |= (GetAsyncKeyState(VK_SCROLL ) & 0x0001) ? 0x10 : 0;
	CPU_AL |= (GetAsyncKeyState(VK_MENU   ) & 0x8000) ? 0x08 : 0;
	CPU_AL |= (GetAsyncKeyState(VK_CONTROL) & 0x8000) ? 0x04 : 0;
	CPU_AL |= (GetAsyncKeyState(VK_LSHIFT ) & 0x8000) ? 0x02 : 0;
	CPU_AL |= (GetAsyncKeyState(VK_RSHIFT ) & 0x8000) ? 0x01 : 0;
}

inline void pcbios_int_16h_03h()
{
	static UINT16 status = 0;
	
	switch(CPU_AL) {
	case 0x05:
		status = CPU_BX;
		break;
	case 0x06:
		CPU_BX = status;
		break;
	default:
		CPU_SET_C_FLAG(1);
		break;
	}
}

inline void pcbios_int_16h_05h()
{
	if(key_buf_char != NULL && key_buf_scan != NULL) {
#ifdef USE_SERVICE_THREAD
		EnterCriticalSection(&key_buf_crit_sect);
#endif
		pcbios_set_key_buffer(CPU_CL, CPU_CH);
#ifdef USE_SERVICE_THREAD
		LeaveCriticalSection(&key_buf_crit_sect);
#endif
	}
	CPU_AL = 0x00;
}

inline void pcbios_int_16h_09h()
{
	CPU_AL  = 0x00;
//	CPU_AL |= 0x01;	// INT 16/AX=0300h supported	(set default delay and rate (PCjr and some PS/2))
//	CPU_AL |= 0x02;	// INT 16/AX=0304h supported	(turn off typematic repeat (PCjr and some PS/2))
	CPU_AL |= 0x04;	// INT 16/AX=0305h supported	(set repeat rate and delay (AT,PS))
	CPU_AL |= 0x08;	// INT 16/AX=0306h supported	(get current typematic rate and delay (newer PS/2s))
	CPU_AL |= 0x10;	// INT 16/AH=0Ah supported	(get keyboard id)
	CPU_AL |= 0x20;	// INT 16/AH=10h-12h supported	(enhanced keyboard support)
//	CPU_AL |= 0x40;	// INT 16/AH=20h-22h supported	(122-key keyboard support)
//	CPU_AL |= 0x80;	// reserved
}

inline void pcbios_int_16h_0ah()
{
//	CPU_BX = 0x41ab;	// MF2 Keyboard (usually in translate mode)
	CPU_BX = 0x83ab;	// MF2 Keyboard (pass-through mode)
}

inline void pcbios_int_16h_11h()
{
	int key_char, key_scan;
	
#ifdef USE_SERVICE_THREAD
	EnterCriticalSection(&key_buf_crit_sect);
#endif
	if(pcbios_check_key_buffer(&key_char, &key_scan)) {
		CPU_AL = key_char;
		CPU_AH = key_scan;
		CPU_SET_Z_FLAG(0);
	} else {
		CPU_SET_Z_FLAG(1);
	}
#ifdef USE_SERVICE_THREAD
	LeaveCriticalSection(&key_buf_crit_sect);
#endif
}

inline void pcbios_int_16h_12h()
{
	pcbios_int_16h_02h();
	
	CPU_AH  = 0;//(GetAsyncKeyState(VK_SYSREQ  ) & 0x8000) ? 0x80 : 0;
	CPU_AH |= (GetAsyncKeyState(VK_CAPITAL ) & 0x8000) ? 0x40 : 0;
	CPU_AH |= (GetAsyncKeyState(VK_NUMLOCK ) & 0x8000) ? 0x20 : 0;
	CPU_AH |= (GetAsyncKeyState(VK_SCROLL  ) & 0x8000) ? 0x10 : 0;
	CPU_AH |= (GetAsyncKeyState(VK_RMENU   ) & 0x8000) ? 0x08 : 0;
	CPU_AH |= (GetAsyncKeyState(VK_RCONTROL) & 0x8000) ? 0x04 : 0;
	CPU_AH |= (GetAsyncKeyState(VK_LMENU   ) & 0x8000) ? 0x02 : 0;
	CPU_AH |= (GetAsyncKeyState(VK_LCONTROL) & 0x8000) ? 0x01 : 0;
}

inline void pcbios_int_16h_13h()
{
	static UINT16 status = 0;
	
	switch(CPU_AL) {
	case 0x00:
		status = CPU_DX;
		break;
	case 0x01:
		CPU_DX = status;
		break;
	default:
		unimplemented_16h("int %02Xh (AX=%04X BX=%04X CX=%04X DX=%04X SI=%04X DI=%04X DS=%04X ES=%04X)\n", 0x16, CPU_AX, CPU_BX, CPU_CX, CPU_DX, CPU_SI, CPU_DI, CPU_DS, CPU_ES);
		CPU_SET_C_FLAG(1);
		break;
	}
}

inline void pcbios_int_16h_14h()
{
	static UINT8 status = 0;
	
	switch(CPU_AL) {
	case 0x00:
	case 0x01:
		status = CPU_AL;
		break;
	case 0x02:
		CPU_AL = status;
		break;
	default:
		unimplemented_16h("int %02Xh (AX=%04X BX=%04X CX=%04X DX=%04X SI=%04X DI=%04X DS=%04X ES=%04X)\n", 0x16, CPU_AX, CPU_BX, CPU_CX, CPU_DX, CPU_SI, CPU_DI, CPU_DS, CPU_ES);
		CPU_SET_C_FLAG(1);
		break;
	}
}

inline void pcbios_int_16h_51h()
{
	// http://radioc.web.fc2.com/column/ax/kb_bios.htm
	pcbios_int_16h_02h();
	
	CPU_AH = (GetAsyncKeyState(VK_KANA) & 0x8000) ? 0x02 : 0;
}

inline void pcbios_int_16h_55h()
{
	switch(CPU_AL) {
	case 0x00:
		// keyboard tsr is not present
		break;
	case 0xfe:
		// handlers for INT 08, INT 09, INT 16, INT 1B, and INT 1C are installed
		break;
	case 0xff:
		break;
	default:
		unimplemented_16h("int %02Xh (AX=%04X BX=%04X CX=%04X DX=%04X SI=%04X DI=%04X DS=%04X ES=%04X)\n", 0x16, CPU_AX, CPU_BX, CPU_CX, CPU_DX, CPU_SI, CPU_DI, CPU_DS, CPU_ES);
		CPU_SET_C_FLAG(1);
		break;
	}
}

inline void pcbios_int_16h_6fh()
{
	switch(CPU_AL) {
	case 0x00:
		// HP Vectra EX-BIOS - "F16_INQUIRE" - Extended BIOS is not installed
		break;
	default:
		unimplemented_16h("int %02Xh (AX=%04X BX=%04X CX=%04X DX=%04X SI=%04X DI=%04X DS=%04X ES=%04X)\n", 0x16, CPU_AX, CPU_BX, CPU_CX, CPU_DX, CPU_SI, CPU_DI, CPU_DS, CPU_ES);
		CPU_SET_C_FLAG(1);
		break;
	}
}

UINT16 pcbios_printer_jis2sjis(UINT16 jis)
{
	UINT8 hi = jis >> 8;
	UINT8 lo = jis & 0xff;
	
	lo = (hi & 0x01) ? lo - 0x1f : lo + 0x7d;
	hi = (hi - 0x21) / 2 + 0x81;
	hi = (hi >= 0xa0) ? hi + 0x40 : hi;
	lo = (lo >= 0x7f) ? lo + 0x01 : lo;
	
	return((hi << 8) + lo);
}

UINT16 pcbios_printer_sjis2jis(UINT16 sjis)
{
	UINT8 hi = sjis >> 8;
	UINT8 lo = sjis & 0xff;
	
	if(hi == 0x80 || (hi >= 0xeb && hi <= 0xef) || (hi >= 0xf4 && hi <= 0xff)) {
		return(0x2121);
	}
	if((lo >= 0x00 && lo <= 0x3f) || lo == 0x7f || (lo >= 0xfd && lo <= 0xff)) {
		return(0x2121);
	}
	if(hi >= 0xf0 && hi <= 0xf3) {
		// gaiji
		if(lo >= 0x40 && lo <= 0x7e) {
			return(0x7721 + lo - 0x40 + (hi - 0xf0) * 0x200);
		}
		if(lo >= 0x80 && lo <= 0x9e) {
			return(0x7760 + lo - 0x80 + (hi - 0xf0) * 0x200);
		}
		if(lo >= 0x9f && lo <= 0xfc) {
			return(0x7821 + lo - 0x9f + (hi - 0xf0) * 0x200);
		}
	}
	hi = (hi >= 0xe0) ? hi - 0x40 : hi;
	lo = (lo >= 0x80) ? lo - 0x01 : lo;
	hi = ((lo >= 0x9e) ? 1 : 0) + (hi - 0x81) * 2 + 0x21;
	lo = ((lo >= 0x9e) ? lo - 0x9e : lo - 0x40) + 0x21;
	
	return((hi << 8) + lo);
}

// AXテクニカルリファレンスガイド 1989
// 付録10 日本語拡張PRINTER DRIVER NIOS概仕様
// 6. コントロールコードの解析

void pcbios_printer_out(int c, UINT8 data)
{
	if(pio[c].conv_mode) {
		if(pio[c].sjis_hi != 0) {
			if(!pio[c].jis_mode) {
				printer_out(c, 0x1c);
				printer_out(c, 0x26);
				pio[c].jis_mode = true;
			}
			UINT16 jis = pcbios_printer_sjis2jis((pio[c].sjis_hi << 8) | data);
			printer_out(c, jis >> 8);
			printer_out(c, jis & 0xff);
			pio[c].sjis_hi = 0;
		} else if(pio[c].esc_buf[0] == 0x1b) {
			printer_out(c, data);
			if(pio[c].esc_len < sizeof(pio[c].esc_buf)) {
				pio[c].esc_buf[pio[c].esc_len] = data;
			}
			pio[c].esc_len++;
			
			switch(pio[c].esc_buf[1]) {
			case 0x33: // 1Bh 33h XX
			case 0x4a: // 1Bh 4Ah XX
			case 0x4e: // 1Bh 4Eh XX
			case 0x51: // 1Bh 51h XX
			case 0x55: // 1Bh 55h XX
			case 0x6c: // 1Bh 6Ch XX
			case 0x71: // 1Bh 71h XX
			case 0x72: // 1Bh 72h XX
				if(pio[c].esc_len == 3) {
					pio[c].esc_buf[0] = 0x00;
				}
				break;
			case 0x24: // 1Bh 24h XX XX
			case 0x5c: // 1Bh 5Ch XX XX
				if(pio[c].esc_len == 4) {
					pio[c].esc_buf[0] = 0x00;
				}
				break;
			case 0x2a: // 1Bh 2Ah XX XX XX data
				if(pio[c].esc_len >= 3) {
					switch(pio[c].esc_buf[2]) {
					case 0: case 1: case 2: case 3: case 4: case 6:
						if(pio[c].esc_len >= 5 && pio[c].esc_len == 5 + (pio[c].esc_buf[3] + pio[c].esc_buf[4] * 256) * 1) {
							pio[c].esc_buf[0] = 0x00;
						}
						break;
					case 32: case 33: case 38: case 39: case 40:
						if(pio[c].esc_len >= 5 && pio[c].esc_len == 5 + (pio[c].esc_buf[3] + pio[c].esc_buf[4] * 256) * 3) {
							pio[c].esc_buf[0] = 0x00;
						}
						break;
					case 71: case 72: case 73:
						if(pio[c].esc_len >= 5 && pio[c].esc_len == 5 + (pio[c].esc_buf[3] + pio[c].esc_buf[4] * 256) * 6) {
							pio[c].esc_buf[0] = 0x00;
						}
						break;
					default:
						pio[c].esc_buf[0] = 0x00;
						break;
					}
				}
				break;
			case 0x40: // 1Bh 40h
				if(pio[c].jis_mode) {
					printer_out(c, 0x1c);
					printer_out(c, 0x2e);
					pio[c].jis_mode = false;
				}
				pio[c].esc_buf[0] = 0x00;
				break;
			case 0x42: // 1Bh 42h data 00h
			case 0x44: // 1Bh 44h data 00h
				if(pio[c].esc_len >= 3 && data == 0) {
					pio[c].esc_buf[0] = 0x00;
				}
				break;
			case 0x43: // 1Bh 43h (00h) XX
				if(pio[c].esc_len >= 3 && data != 0) {
					pio[c].esc_buf[0] = 0x00;
				}
				break;
			default: // 1Bh XX
				pio[c].esc_buf[0] = 0x00;
				break;
			}
		} else if(pio[c].esc_buf[0] == 0x1c) {
			printer_out(c, data);
			if(pio[c].esc_len < sizeof(pio[c].esc_buf)) {
				pio[c].esc_buf[pio[c].esc_len] = data;
			}
			pio[c].esc_len++;
			
			switch(pio[c].esc_buf[1]) {
			case 0x21: // 1Ch 21h XX
			case 0x2d: // 1Ch 2Dh XX
			case 0x57: // 1Ch 57h XX
			case 0x6b: // 1Ch 6Bh XX
			case 0x72: // 1Ch 72h XX
			case 0x78: // 1Ch 78h XX
				if(pio[c].esc_len == 3) {
					pio[c].esc_buf[0] = 0x00;
				}
				break;
			case 0x26: // 1Ch 26h
				pio[c].jis_mode = true;
				pio[c].esc_buf[0] = 0x00;
				break;
			case 0x2e: // 1Ch 2Eh
				pio[c].jis_mode = false;
				pio[c].esc_buf[0] = 0x00;
				break;
			case 0x32: // 1Ch 32h XX XX data
				if(pio[c].esc_len == 76) {
					pio[c].esc_buf[0] = 0x00;
				}
				break;
			case 0x44: // 1Bh 44h data 00h
				if(pio[c].esc_len == 6) {
					pio[c].esc_buf[0] = 0x00;
				}
				break;
			case 0x53: // 1Ch 53h XX XX
			case 0x54: // 1Ch 54h XX XX
				if(pio[c].esc_len == 4) {
					pio[c].esc_buf[0] = 0x00;
				}
				break;
			default: // 1Ch XX
				pio[c].esc_buf[0] = 0x00;
				break;
			}
		} else if(data == 0x1b || data == 0x1c) {
			printer_out(c, data);
			pio[c].esc_buf[0] = data;
			pio[c].esc_len = 1;
		} else if((data >= 0x80 && data <= 0x9f) || (data >= 0xe0 && data <= 0xff)) {
			pio[c].sjis_hi = data;
		} else {
			if(pio[c].jis_mode) {
				printer_out(c, 0x1c);
				printer_out(c, 0x2e);
				pio[c].jis_mode = false;
			}
			printer_out(c, data);
		}
	} else {
		if(pio[c].jis_mode) {
			printer_out(c, 0x1c);
			printer_out(c, 0x2e);
			pio[c].jis_mode = false;
		}
		printer_out(c, data);
	}
}

inline void pcbios_int_17h_00h()
{
	if(CPU_DX < 3) {
		pcbios_printer_out(CPU_DX, CPU_AL);
		CPU_AH = 0xd0;
	}
}

inline void pcbios_int_17h_01h()
{
	if(CPU_DX < 3) {
		CPU_AH = 0xd0;
	}
}

inline void pcbios_int_17h_02h()
{
	if(CPU_DX < 3) {
		CPU_AH = 0xd0;
	}
}

inline void pcbios_int_17h_03h()
{
	switch(CPU_AL) {
	case 0x00:
		if(CPU_DX < 3) {
			if(pio[CPU_DX].jis_mode) {
				printer_out(CPU_DX, 0x1c);
				printer_out(CPU_DX, 0x2e);
				pio[CPU_DX].jis_mode = false;
			}
			for(UINT16 i = 0; i < CPU_CX; i++) {
				printer_out(CPU_DX, mem[CPU_ES_BASE + CPU_BX + i]);
			}
			CPU_CX = 0x0000;
			CPU_AH = 0xd0;
		}
		break;
	default:
		unimplemented_17h("int %02Xh (AX=%04X BX=%04X CX=%04X DX=%04X SI=%04X DI=%04X DS=%04X ES=%04X)\n", 0x17, CPU_AX, CPU_BX, CPU_CX, CPU_DX, CPU_SI, CPU_DI, CPU_DS, CPU_ES);
		break;
	}
}

inline void pcbios_int_17h_50h()
{
	switch(CPU_AL) {
	case 0x00:
		if(CPU_DX < 3) {
			if(CPU_BX = 0x0001) {
				pio[CPU_DX].conv_mode = false;
				CPU_AL = 0x00;
			} else if(CPU_BX = 0x0051) {
				pio[CPU_DX].conv_mode = true;
				CPU_AL = 0x00;
			} else {
				CPU_AL = 0x01;
			}
		} else {
			CPU_AL = 0x02;
		}
		break;
	case 0x01:
		if(CPU_DX < 3) {
			if(pio[CPU_DX].conv_mode) {
				CPU_BX = 0x0051;
			} else {
				CPU_BX = 0x0001;
			}
			CPU_AL = 0x00;
		} else {
			CPU_AL = 0x02;
		}
		break;
	default:
		unimplemented_17h("int %02Xh (AX=%04X BX=%04X CX=%04X DX=%04X SI=%04X DI=%04X DS=%04X ES=%04X)\n", 0x17, CPU_AX, CPU_BX, CPU_CX, CPU_DX, CPU_SI, CPU_DI, CPU_DS, CPU_ES);
		break;
	}
}

inline void pcbios_int_17h_51h()
{
	if(CPU_DH >= 0x21 && CPU_DH <= 0x7e && CPU_DL >= 0x21 && CPU_DL <= 0x7e) {
		CPU_DX = pcbios_printer_jis2sjis(CPU_DX);
	} else {
		CPU_DX = 0x0000;
	}
}

inline void pcbios_int_17h_52h()
{
	if(((CPU_DH >= 0x81 && CPU_DH <= 0x9f) || (CPU_DH >= 0xe0 && CPU_DH <= 0xfc)) && (CPU_DL >= 0x40 && CPU_DL <= 0xfc && CPU_DL != 0x7f)) {
		CPU_DX = pcbios_printer_sjis2jis(CPU_DX);
	} else {
		CPU_DX = 0x0000;
	}
}

inline void pcbios_int_17h_84h()
{
	if(CPU_DX < 3) {
		if(pio[CPU_DX].jis_mode) {
			printer_out(CPU_DX, 0x1c);
			printer_out(CPU_DX, 0x2e);
			pio[CPU_DX].jis_mode = false;
		}
		printer_out(CPU_DX, CPU_AL);
		CPU_AH = 0xd0;
	}
}

inline void pcbios_int_17h_85h()
{
	pio[0].conv_mode = (CPU_AL == 0x00);
}

inline void pcbios_int_1ah_00h()
{
	pcbios_update_daily_timer_counter(timeGetTime());
	CPU_CX = *(UINT16 *)(mem + 0x46e);
	CPU_DX = *(UINT16 *)(mem + 0x46c);
	CPU_AL = mem[0x470];
	mem[0x470] = 0;
}

inline int to_bcd(int t)
{
	int u = (t % 100) / 10;
	return (u << 4) | (t % 10);
}

inline void pcbios_int_1ah_02h()
{
	SYSTEMTIME time;
	
	GetLocalTime(&time);
	CPU_CH = to_bcd(time.wHour);
	CPU_CL = to_bcd(time.wMinute);
	CPU_DH = to_bcd(time.wSecond);
	CPU_DL = 0x00;
}

inline void pcbios_int_1ah_04h()
{
	SYSTEMTIME time;
	
	GetLocalTime(&time);
	CPU_CH = to_bcd(time.wYear / 100);
	CPU_CL = to_bcd(time.wYear);
	CPU_DH = to_bcd(time.wMonth);
	CPU_DL = to_bcd(time.wDay);
}

inline void pcbios_int_1ah_0ah()
{
	SYSTEMTIME time;
	FILETIME file_time;
	WORD dos_date, dos_time;
	
	GetLocalTime(&time);
	SystemTimeToFileTime(&time, &file_time);
	FileTimeToDosDateTime(&file_time, &dos_date, &dos_time);
	CPU_CX = dos_date;
}

// msdos system call

inline void msdos_int_21h_56h(int lfn);

inline void msdos_int_21h_00h()
{
	msdos_process_terminate(CPU_CS, retval, 1);
}

DWORD WINAPI msdos_int_21h_01h_thread(LPVOID)
{
	CPU_AX_in_service &= 0xff00;
	CPU_AX_in_service |= msdos_getche(0x21, 0x01);
	
	ctrl_break_detected = ctrl_break_pressed;
	
#ifdef USE_SERVICE_THREAD
	service_exit = true;
#endif
	return(0);
}

inline void msdos_int_21h_01h()
{
#ifdef USE_SERVICE_THREAD
	if(!in_service && !in_service_29h &&
	   *(UINT16 *)(mem + 4 * 0x29 + 0) == (IRET_SIZE + 5 * 0x29) &&
	   *(UINT16 *)(mem + 4 * 0x29 + 2) == (IRET_TOP >> 4)) {
		// msdos_putch() will be used in this service
		// if int 29h is hooked, run this service in main thread to call int 29h
		start_service_loop(msdos_int_21h_01h_thread);
	} else {
#endif
		prepare_service_loop();
		msdos_int_21h_01h_thread(NULL);
		cleanup_service_loop();
		REQUEST_HARDWRE_UPDATE();
#ifdef USE_SERVICE_THREAD
	}
#endif
}

inline void msdos_int_21h_02h()
{
	UINT8 data = CPU_DL;
	msdos_putch(data, 0x21, 0x02);
	CPU_AL = data;
	ctrl_break_detected = ctrl_break_pressed;
}

inline void msdos_int_21h_03h()
{
	CPU_AL = msdos_aux_in();
}

inline void msdos_int_21h_04h()
{
	msdos_aux_out(CPU_DL);
}

inline void msdos_int_21h_05h()
{
	msdos_prn_out(CPU_DL);
}

inline void msdos_int_21h_06h()
{
	if(CPU_DL == 0xff) {
		if(msdos_kbhit()) {
			CPU_AL = msdos_getch(0x21, 0x06);
			CPU_SET_Z_FLAG(0);
		} else {
			CPU_AL = 0;
			CPU_SET_Z_FLAG(1);
			maybe_idle();
		}
	} else {
		UINT8 data = CPU_DL;
		msdos_putch(data, 0x21, 0x06);
		CPU_AL = data;
	}
}

DWORD WINAPI msdos_int_21h_07h_thread(LPVOID)
{
	CPU_AX_in_service &= 0xff00;
	CPU_AX_in_service |= msdos_getch(0x21, 0x07);
	
#ifdef USE_SERVICE_THREAD
	service_exit = true;
#endif
	return(0);
}

inline void msdos_int_21h_07h()
{
#ifdef USE_SERVICE_THREAD
	if(!in_service && !in_service_29h) {
		start_service_loop(msdos_int_21h_07h_thread);
	} else {
#endif
		prepare_service_loop();
		msdos_int_21h_07h_thread(NULL);
		cleanup_service_loop();
		REQUEST_HARDWRE_UPDATE();
#ifdef USE_SERVICE_THREAD
	}
#endif
}

DWORD WINAPI msdos_int_21h_08h_thread(LPVOID)
{
	CPU_AX_in_service &= 0xff00;
	CPU_AX_in_service |= msdos_getch(0x21, 0x08);
	
	ctrl_break_detected = ctrl_break_pressed;
	
#ifdef USE_SERVICE_THREAD
	service_exit = true;
#endif
	return(0);
}

inline void msdos_int_21h_08h()
{
#ifdef USE_SERVICE_THREAD
	if(!in_service && !in_service_29h) {
		start_service_loop(msdos_int_21h_08h_thread);
	} else {
#endif
		prepare_service_loop();
		msdos_int_21h_08h_thread(NULL);
		cleanup_service_loop();
		REQUEST_HARDWRE_UPDATE();
#ifdef USE_SERVICE_THREAD
	}
#endif
}

inline void msdos_int_21h_09h()
{
	msdos_stdio_reopen();
	
	process_t *process = msdos_process_info_get(current_psp);
	int fd = msdos_psp_get_file_table(1, current_psp);
	
	char *str = (char *)(mem + CPU_DS_BASE + CPU_DX);
	int len = 0;
	
	while(str[len] != '$' && len < 0x10000) {
		len++;
	}
	if(fd < process->max_files && file_handler[fd].valid && !file_handler[fd].atty) {
		// stdout is redirected to file
		msdos_write(fd, str, len);
	} else {
		for(int i = 0; i < len; i++) {
			msdos_putch(str[i], 0x21, 0x09);
		}
	}
	CPU_AL = '$';
	ctrl_break_detected = ctrl_break_pressed;
}

DWORD WINAPI msdos_int_21h_0ah_thread(LPVOID)
{
	int ofs = CPU_DS_BASE_in_service + CPU_DX_in_service;
	int max = mem[ofs] - 1;
	UINT8 *buf = mem + ofs + 2;
	int chr, p = 0;
	
	while((chr = msdos_getch(0x21, 0x0a)) != 0x0d) {
		if((ctrl_break_checking && ctrl_break_pressed) || ctrl_c_pressed) {
			p = 0;
			msdos_putch(0x03, 0x21, 0x0a);
			msdos_putch(0x0d, 0x21, 0x0a);
			msdos_putch(0x0a, 0x21, 0x0a);
			break;
		} else if(ctrl_break_pressed) {
			// skip this byte
		} else if(chr == 0x00) {
			// skip 2nd byte
			msdos_getch(0x21, 0x0a);
		} else if(chr == 0x08) {
			// back space
			if(p > 0) {
				p--;
				if(msdos_ctrl_code_check(buf[p], 0x21, 0x0a)) {
					msdos_putch(0x08, 0x21, 0x0a);
					msdos_putch(0x08, 0x21, 0x0a);
					msdos_putch(0x20, 0x21, 0x0a);
					msdos_putch(0x20, 0x21, 0x0a);
					msdos_putch(0x08, 0x21, 0x0a);
					msdos_putch(0x08, 0x21, 0x0a);
				} else if(p > 0 && msdos_kanji_2nd_byte_check(buf, p)) {
					p--;
					msdos_putch(0x08, 0x21, 0x0a);
					msdos_putch(0x08, 0x21, 0x0a);
					msdos_putch(0x20, 0x21, 0x0a);
					msdos_putch(0x20, 0x21, 0x0a);
					msdos_putch(0x08, 0x21, 0x0a);
					msdos_putch(0x08, 0x21, 0x0a);
				} else {
					msdos_putch(0x08, 0x21, 0x0a);
					msdos_putch(0x20, 0x21, 0x0a);
					msdos_putch(0x08, 0x21, 0x0a);
				}
			}
		} else if(chr == 0x1b) {
			// escape
			while(p > 0) {
				p--;
				if(msdos_ctrl_code_check(buf[p], 0x21, 0x0a)) {
					msdos_putch(0x08, 0x21, 0x0a);
					msdos_putch(0x08, 0x21, 0x0a);
					msdos_putch(0x20, 0x21, 0x0a);
					msdos_putch(0x20, 0x21, 0x0a);
					msdos_putch(0x08, 0x21, 0x0a);
					msdos_putch(0x08, 0x21, 0x0a);
				} else {
					msdos_putch(0x08, 0x21, 0x0a);
					msdos_putch(0x20, 0x21, 0x0a);
					msdos_putch(0x08, 0x21, 0x0a);
				}
			}
		} else if(p < max) {
			buf[p++] = chr;
			msdos_putch(chr, 0x21, 0x0a);
		}
	}
	buf[p] = 0x0d;
	msdos_putch(0x0d, 0x21, 0x0a);
	mem[ofs + 1] = p;
	ctrl_break_detected = ctrl_break_pressed;
	
#ifdef USE_SERVICE_THREAD
	service_exit = true;
#endif
	return(0);
}

inline void msdos_int_21h_0ah()
{
	if(mem[CPU_DS_BASE + CPU_DX] != 0x00) {
#ifdef USE_SERVICE_THREAD
		if(!in_service && !in_service_29h &&
		   *(UINT16 *)(mem + 4 * 0x29 + 0) == (IRET_SIZE + 5 * 0x29) &&
		   *(UINT16 *)(mem + 4 * 0x29 + 2) == (IRET_TOP >> 4)) {
			// msdos_putch() will be used in this service
			// if int 29h is hooked, run this service in main thread to call int 29h
			start_service_loop(msdos_int_21h_0ah_thread);
		} else {
#endif
			prepare_service_loop();
			msdos_int_21h_0ah_thread(NULL);
			cleanup_service_loop();
			REQUEST_HARDWRE_UPDATE();
#ifdef USE_SERVICE_THREAD
		}
#endif
	}
}

inline void msdos_int_21h_0bh()
{
	if(msdos_kbhit()) {
		CPU_AL = 0xff;
	} else {
		CPU_AL = 0x00;
		maybe_idle();
	}
	ctrl_break_detected = ctrl_break_pressed;
}

inline void msdos_int_21h_0ch()
{
	// clear key buffer
	msdos_stdio_reopen();
	
	process_t *process = msdos_process_info_get(current_psp);
	int fd = msdos_psp_get_file_table(0, current_psp);
	
	if(fd < process->max_files && file_handler[fd].valid && !file_handler[fd].atty) {
		// stdin is redirected to file
	} else {
		while(msdos_kbhit()) {
			msdos_getch(0x21, 0x0c);
		}
	}
	
	switch(CPU_AL) {
	case 0x01:
		msdos_int_21h_01h();
		break;
	case 0x06:
		msdos_int_21h_06h();
		break;
	case 0x07:
		msdos_int_21h_07h();
		break;
	case 0x08:
		msdos_int_21h_08h();
		break;
	case 0x0a:
		msdos_int_21h_0ah();
		break;
	default:
		// the buffer is flushed but no input is attempted
		break;
	}
}

inline void msdos_int_21h_0dh()
{
}

inline void msdos_int_21h_0eh()
{
	if(CPU_DL < 26) {
		_chdrive(CPU_DL + 1);
		msdos_cds_update(CPU_DL);
		msdos_sda_update(current_psp);
	}
	CPU_AL = 26; // zdrive
}

inline void msdos_int_21h_0fh()
{
	ext_fcb_t *ext_fcb = (ext_fcb_t *)(mem + CPU_DS_BASE + CPU_DX);
	fcb_t *fcb = (fcb_t *)(ext_fcb + (ext_fcb->flag == 0xff ? 1 : 0));
	const char *path = msdos_fcb_path(fcb);
	HANDLE hFile = CreateFileA(path, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
	
	if(hFile == INVALID_HANDLE_VALUE) {
		CPU_AL = 0xff;
	} else {
		CPU_AL = 0;
		fcb->current_block = 0;
		fcb->record_size = 128;
		fcb->file_size = GetFileSize(hFile, NULL);
		fcb->handle = hFile;
	}
}

inline void msdos_int_21h_10h()
{
	ext_fcb_t *ext_fcb = (ext_fcb_t *)(mem + CPU_DS_BASE + CPU_DX);
	fcb_t *fcb = (fcb_t *)(ext_fcb + (ext_fcb->flag == 0xff ? 1 : 0));
	
	CPU_AL = CloseHandle(fcb->handle) ? 0 : 0xff;
}

inline void msdos_int_21h_11h()
{
	ext_fcb_t *ext_fcb = (ext_fcb_t *)(mem + CPU_DS_BASE + CPU_DX);
	fcb_t *fcb = (fcb_t *)(mem + CPU_DS_BASE + CPU_DX + (ext_fcb->flag == 0xff ? 7 : 0));
	
	process_t *process = msdos_process_info_get(current_psp);
	UINT32 dta_laddr = (process->dta.w.h << 4) + process->dta.w.l;
	ext_fcb_t *ext_find = (ext_fcb_t *)(mem + dta_laddr);
	find_fcb_t *find = (find_fcb_t *)(mem + dta_laddr + (ext_fcb->flag == 0xff ? 7 : 0));
	const char *path = msdos_fcb_path(fcb);
	WIN32_FIND_DATAA fd;
	
	dtainfo_t *dtainfo = msdos_dta_info_get(current_psp, dta_laddr);
	if(dtainfo->find_handle != INVALID_HANDLE_VALUE) {
		FindClose(dtainfo->find_handle);
		dtainfo->find_handle = INVALID_HANDLE_VALUE;
	}
	strcpy(process->volume_label, msdos_volume_label(path));
	dtainfo->allowable_mask = (ext_fcb->flag == 0xff) ? ext_fcb->attribute : 0x20;
	bool label_only = (dtainfo->allowable_mask == 8);
	
	if((dtainfo->allowable_mask & 8) && !msdos_match_volume_label(path, msdos_short_volume_label(process->volume_label))) {
		dtainfo->allowable_mask &= ~8;
	}
	if(!label_only && (dtainfo->find_handle = FindFirstFileA(path, &fd)) != INVALID_HANDLE_VALUE) {
		while(!msdos_find_file_check_attribute(fd.dwFileAttributes, dtainfo->allowable_mask, 0) ||
		      !msdos_find_file_has_8dot3name(&fd)) {
			if(!FindNextFileA(dtainfo->find_handle, &fd)) {
				FindClose(dtainfo->find_handle);
				dtainfo->find_handle = INVALID_HANDLE_VALUE;
				break;
			}
		}
	}
	if(dtainfo->find_handle != INVALID_HANDLE_VALUE) {
		if(ext_fcb->flag == 0xff) {
			ext_find->flag = 0xff;
			memset(ext_find->reserved, 0, 5);
			ext_find->attribute = (UINT8)(fd.dwFileAttributes & 0x3f);
		}
		find->drive = _getdrive();
		msdos_set_fcb_path((fcb_t *)find, msdos_short_name(&fd));
		find->attribute = (UINT8)(fd.dwFileAttributes & 0x3f);
		find->nt_res = 0;
		msdos_find_file_conv_local_time(&fd);
		find->create_time_ms = 0;
		FileTimeToDosDateTime(&fd.ftCreationTime, &find->creation_date, &find->creation_time);
		FileTimeToDosDateTime(&fd.ftLastAccessTime, &find->last_access_date, &find->last_write_time);
		FileTimeToDosDateTime(&fd.ftLastWriteTime, &find->last_write_date, &find->last_write_time);
		find->cluster_hi = find->cluster_lo = 0;
		find->file_size = fd.nFileSizeLow;
		CPU_AL = 0x00;
	} else if(dtainfo->allowable_mask & 8) {
		if(ext_fcb->flag == 0xff) {
			ext_find->flag = 0xff;
			memset(ext_find->reserved, 0, 5);
			ext_find->attribute = 8;
		}
		find->drive = _getdrive();
		msdos_set_fcb_path((fcb_t *)find, msdos_short_volume_label(process->volume_label));
		find->attribute = 8;
		find->nt_res = 0;
		msdos_find_file_conv_local_time(&fd);
		find->create_time_ms = 0;
		FileTimeToDosDateTime(&fd.ftCreationTime, &find->creation_date, &find->creation_time);
		FileTimeToDosDateTime(&fd.ftLastAccessTime, &find->last_access_date, &find->last_write_time);
		FileTimeToDosDateTime(&fd.ftLastWriteTime, &find->last_write_date, &find->last_write_time);
		find->cluster_hi = find->cluster_lo = 0;
		find->file_size = 0;
		dtainfo->allowable_mask &= ~8;
		CPU_AL = 0x00;
	} else {
		CPU_AL = 0xff;
	}
}

inline void msdos_int_21h_12h()
{
	ext_fcb_t *ext_fcb = (ext_fcb_t *)(mem + CPU_DS_BASE + CPU_DX);
//	fcb_t *fcb = (fcb_t *)(mem + CPU_DS_BASE + CPU_DX + (ext_fcb->flag == 0xff ? 7 : 0));
	
	process_t *process = msdos_process_info_get(current_psp);
	UINT32 dta_laddr = (process->dta.w.h << 4) + process->dta.w.l;
	ext_fcb_t *ext_find = (ext_fcb_t *)(mem + dta_laddr);
	find_fcb_t *find = (find_fcb_t *)(mem + dta_laddr + (ext_fcb->flag == 0xff ? 7 : 0));
	WIN32_FIND_DATAA fd;
	
	dtainfo_t *dtainfo = msdos_dta_info_get(current_psp, dta_laddr);
	if(dtainfo->find_handle != INVALID_HANDLE_VALUE) {
		if(FindNextFileA(dtainfo->find_handle, &fd)) {
			while(!msdos_find_file_check_attribute(fd.dwFileAttributes, dtainfo->allowable_mask, 0) ||
			      !msdos_find_file_has_8dot3name(&fd)) {
				if(!FindNextFileA(dtainfo->find_handle, &fd)) {
					FindClose(dtainfo->find_handle);
					dtainfo->find_handle = INVALID_HANDLE_VALUE;
					break;
				}
			}
		} else {
			FindClose(dtainfo->find_handle);
			dtainfo->find_handle = INVALID_HANDLE_VALUE;
		}
	}
	if(dtainfo->find_handle != INVALID_HANDLE_VALUE) {
		if(ext_fcb->flag == 0xff) {
			ext_find->flag = 0xff;
			memset(ext_find->reserved, 0, 5);
			ext_find->attribute = (UINT8)(fd.dwFileAttributes & 0x3f);
		}
		find->drive = _getdrive();
		msdos_set_fcb_path((fcb_t *)find, msdos_short_name(&fd));
		find->attribute = (UINT8)(fd.dwFileAttributes & 0x3f);
		find->nt_res = 0;
		msdos_find_file_conv_local_time(&fd);
		find->create_time_ms = 0;
		FileTimeToDosDateTime(&fd.ftCreationTime, &find->creation_date, &find->creation_time);
		FileTimeToDosDateTime(&fd.ftLastAccessTime, &find->last_access_date, &find->last_write_time);
		FileTimeToDosDateTime(&fd.ftLastWriteTime, &find->last_write_date, &find->last_write_time);
		find->cluster_hi = find->cluster_lo = 0;
		find->file_size = fd.nFileSizeLow;
		CPU_AL = 0x00;
	} else if(dtainfo->allowable_mask & 8) {
		if(ext_fcb->flag == 0xff) {
			ext_find->flag = 0xff;
			memset(ext_find->reserved, 0, 5);
			ext_find->attribute = 8;
		}
		find->drive = _getdrive();
		msdos_set_fcb_path((fcb_t *)find, msdos_short_volume_label(process->volume_label));
		find->attribute = 8;
		find->nt_res = 0;
		msdos_find_file_conv_local_time(&fd);
		find->create_time_ms = 0;
		FileTimeToDosDateTime(&fd.ftCreationTime, &find->creation_date, &find->creation_time);
		FileTimeToDosDateTime(&fd.ftLastAccessTime, &find->last_access_date, &find->last_write_time);
		FileTimeToDosDateTime(&fd.ftLastWriteTime, &find->last_write_date, &find->last_write_time);
		find->cluster_hi = find->cluster_lo = 0;
		find->file_size = 0;
		dtainfo->allowable_mask &= ~8;
		CPU_AL = 0x00;
	} else {
		CPU_AL = 0xff;
	}
}

inline void msdos_int_21h_13h()
{
	if(remove(msdos_fcb_path((fcb_t *)(mem + CPU_DS_BASE + CPU_DX)))) {
		CPU_AL = 0xff;
	} else {
		CPU_AL = 0x00;
	}
}

inline void msdos_int_21h_14h()
{
	ext_fcb_t *ext_fcb = (ext_fcb_t *)(mem + CPU_DS_BASE + CPU_DX);
	fcb_t *fcb = (fcb_t *)(ext_fcb + (ext_fcb->flag == 0xff ? 1 : 0));
	process_t *process = msdos_process_info_get(current_psp);
	UINT32 dta_laddr = (process->dta.w.h << 4) + process->dta.w.l;
	DWORD num = 0;
	
	memset(mem + dta_laddr, 0, fcb->record_size);
	SetFilePointer(fcb->handle, fcb->record_size * (fcb->current_block * 128 + fcb->cur_record), NULL, FILE_BEGIN);
	
	if(!ReadFile(fcb->handle, mem + dta_laddr, fcb->record_size, &num, NULL) || num == 0) {
		CPU_AL = 1;
	} else {
		if(++fcb->cur_record >= 128) {
			fcb->current_block += fcb->cur_record / 128;
			fcb->cur_record %= 128;
		}
		CPU_AL = (num == fcb->record_size) ? 0 : 3;
	}
}

inline void msdos_int_21h_15h()
{
	ext_fcb_t *ext_fcb = (ext_fcb_t *)(mem + CPU_DS_BASE + CPU_DX);
	fcb_t *fcb = (fcb_t *)(ext_fcb + (ext_fcb->flag == 0xff ? 1 : 0));
	process_t *process = msdos_process_info_get(current_psp);
	UINT32 dta_laddr = (process->dta.w.h << 4) + process->dta.w.l;
	DWORD num = 0;
	
	SetFilePointer(fcb->handle, fcb->record_size * (fcb->current_block * 128 + fcb->cur_record), NULL, FILE_BEGIN);
	WriteFile(fcb->handle, mem + dta_laddr, fcb->record_size, &num, NULL);
	fcb->file_size = GetFileSize(fcb->handle, NULL);
	
	if(num != fcb->record_size) {
		CPU_AL = 1;
	} else {
		if(++fcb->cur_record >= 128) {
			fcb->current_block += fcb->cur_record / 128;
			fcb->cur_record %= 128;
		}
		CPU_AL = 0;
	}
}

inline void msdos_int_21h_16h()
{
	ext_fcb_t *ext_fcb = (ext_fcb_t *)(mem + CPU_DS_BASE + CPU_DX);
	fcb_t *fcb = (fcb_t *)(ext_fcb + (ext_fcb->flag == 0xff ? 1 : 0));
	const char *path = msdos_fcb_path(fcb);
	HANDLE hFile = CreateFileA(path, GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, CREATE_ALWAYS, ext_fcb->flag == 0xff ? ext_fcb->attribute : FILE_ATTRIBUTE_NORMAL, NULL);
	
	if(hFile == INVALID_HANDLE_VALUE) {
		CPU_AL = 0xff;
	} else {
		CPU_AL = 0;
		fcb->current_block = 0;
		fcb->record_size = 128;
		fcb->file_size = 0;
		fcb->handle = hFile;
	}
}

inline void msdos_int_21h_17h()
{
	ext_fcb_t *ext_fcb_src = (ext_fcb_t *)(mem + CPU_DS_BASE + CPU_DX);
	fcb_t *fcb_src = (fcb_t *)(ext_fcb_src + (ext_fcb_src->flag == 0xff ? 1 : 0));
//	const char *path_src = msdos_fcb_path(fcb_src);
	char path_src[MAX_PATH];
	strcpy(path_src, msdos_fcb_path(fcb_src));
	
	fcb_t *fcb_dst = (fcb_t *)(mem + CPU_DS_BASE + CPU_DX + 16 + (ext_fcb_src->flag == 0xff ? 7 : 0));
//	const char *path_dst = msdos_fcb_path(fcb_dst);
	char path_dst[MAX_PATH];
	strcpy(path_dst, msdos_fcb_path(fcb_dst));
	
	if(rename(path_src, path_dst)) {
		CPU_AL = 0xff;
	} else {
		CPU_AL = 0;
	}
}

inline void msdos_int_21h_18h()
{
	CPU_AL = 0x00;
}

inline void msdos_int_21h_19h()
{
	CPU_AL = _getdrive() - 1;
}

inline void msdos_int_21h_1ah()
{
	process_t *process = msdos_process_info_get(current_psp);
	
	process->dta.w.l = CPU_DX;
	process->dta.w.h = CPU_DS;
	msdos_sda_update(current_psp);
}

inline void msdos_int_21h_1bh()
{
	int drive_num = _getdrive() - 1;
	UINT16 seg, ofs;
	
	if(msdos_drive_param_block_update(drive_num, &seg, &ofs, 1)) {
		dpb_t *dpb = (dpb_t *)(mem + (seg << 4) + ofs);
		CPU_AL = dpb->highest_sector_num + 1;
		CPU_CX = dpb->bytes_per_sector;
		CPU_DX = dpb->highest_cluster_num - 1;
		*(UINT8 *)(mem + CPU_DS_BASE + CPU_BX) = dpb->media_type;
	} else {
		CPU_AL = 0xff;
		CPU_SET_C_FLAG(1);
	}

}

inline void msdos_int_21h_1ch()
{
	int drive_num = (CPU_DL == 0) ? (_getdrive() - 1) : (CPU_DL - 1);
	UINT16 seg, ofs;
	
	if(msdos_drive_param_block_update(drive_num, &seg, &ofs, 1)) {
		dpb_t *dpb = (dpb_t *)(mem + (seg << 4) + ofs);
		CPU_AL = dpb->highest_sector_num + 1;
		CPU_CX = dpb->bytes_per_sector;
		CPU_DX = dpb->highest_cluster_num - 1;
		*(UINT8 *)(mem + CPU_DS_BASE + CPU_BX) = dpb->media_type;
	} else {
		CPU_AL = 0xff;
		CPU_SET_C_FLAG(1);
	}

}

inline void msdos_int_21h_1dh()
{
	CPU_AL = 0;
}

inline void msdos_int_21h_1eh()
{
	CPU_AL = 0;
}

inline void msdos_int_21h_1fh()
{
	int drive_num = _getdrive() - 1;
	UINT16 seg, ofs;
	
	if(msdos_drive_param_block_update(drive_num, &seg, &ofs, 1)) {
		CPU_AL = 0;
		CPU_LOAD_SREG(CPU_DS_INDEX, seg);
		CPU_BX = ofs;
	} else {
		CPU_AL = 0xff;
		CPU_SET_C_FLAG(1);
	}
}

inline void msdos_int_21h_20h()
{
	CPU_AL = 0;
}

inline void msdos_int_21h_21h()
{
	ext_fcb_t *ext_fcb = (ext_fcb_t *)(mem + CPU_DS_BASE + CPU_DX);
	fcb_t *fcb = (fcb_t *)(ext_fcb + (ext_fcb->flag == 0xff ? 1 : 0));
	UINT32 rec = (fcb->record_size >= 64) ? fcb->rand_record & 0xffffff : fcb->rand_record;
	
	fcb->current_block = rec / 128;
	fcb->cur_record = rec % 128;
	
	if(SetFilePointer(fcb->handle, fcb->record_size * rec, NULL, FILE_BEGIN) == INVALID_SET_FILE_POINTER) {
		CPU_AL = 1;
	} else {
		process_t *process = msdos_process_info_get(current_psp);
		UINT32 dta_laddr = (process->dta.w.h << 4) + process->dta.w.l;
		memset(mem + dta_laddr, 0, fcb->record_size);
		DWORD num = 0;
		if(!ReadFile(fcb->handle, mem + dta_laddr, fcb->record_size, &num, NULL) || num == 0) {
			CPU_AL = 1;
		} else {
			CPU_AL = (num == fcb->record_size) ? 0 : 3;
		}
	}
}

inline void msdos_int_21h_22h()
{
	ext_fcb_t *ext_fcb = (ext_fcb_t *)(mem + CPU_DS_BASE + CPU_DX);
	fcb_t *fcb = (fcb_t *)(ext_fcb + (ext_fcb->flag == 0xff ? 1 : 0));
	UINT32 rec = (fcb->record_size >= 64) ? fcb->rand_record & 0xffffff : fcb->rand_record;
	
	fcb->current_block = rec / 128;
	fcb->cur_record = rec % 128;
	
	if(SetFilePointer(fcb->handle, fcb->record_size * rec, NULL, FILE_BEGIN) == INVALID_SET_FILE_POINTER) {
		CPU_AL = 1;
	} else {
		process_t *process = msdos_process_info_get(current_psp);
		UINT32 dta_laddr = (process->dta.w.h << 4) + process->dta.w.l;
		DWORD num = 0;
		WriteFile(fcb->handle, mem + dta_laddr, fcb->record_size, &num, NULL);
		fcb->file_size = GetFileSize(fcb->handle, NULL);
		CPU_AL = (num == fcb->record_size) ? 0 : 1;
	}
}

inline void msdos_int_21h_23h()
{
	ext_fcb_t *ext_fcb = (ext_fcb_t *)(mem + CPU_DS_BASE + CPU_DX);
	fcb_t *fcb = (fcb_t *)(ext_fcb + (ext_fcb->flag == 0xff ? 1 : 0));
	const char *path = msdos_fcb_path(fcb);
	HANDLE hFile = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
	
	if(hFile == INVALID_HANDLE_VALUE) {
		CPU_AL = 0xff;
	} else {
		UINT32 size = GetFileSize(hFile, NULL);
		UINT32 rec = size / fcb->record_size + ((size % fcb->record_size) != 0);
		fcb->rand_record = (fcb->record_size >= 64) ? (fcb->rand_record & 0xff000000) | (rec & 0xffffff) : rec;
		CloseHandle(hFile);
		CPU_AL = 0;
	}
}

inline void msdos_int_21h_24h()
{
	ext_fcb_t *ext_fcb = (ext_fcb_t *)(mem + CPU_DS_BASE + CPU_DX);
	fcb_t *fcb = (fcb_t *)(ext_fcb + (ext_fcb->flag == 0xff ? 1 : 0));
	UINT32 rec = fcb->current_block * 128 + fcb->cur_record;
	
	fcb->rand_record = (fcb->record_size >= 64) ? (fcb->rand_record & 0xff000000) | (rec & 0xffffff) : rec;
}

inline void msdos_int_21h_25h()
{
	*(UINT16 *)(mem + 4 * CPU_AL + 0) = CPU_DX;
	*(UINT16 *)(mem + 4 * CPU_AL + 2) = CPU_DS;
}

inline void msdos_int_21h_26h()
{
	psp_t *psp = (psp_t *)(mem + (CPU_DX << 4));
	
	memcpy(mem + (CPU_DX << 4), mem + (current_psp << 4), sizeof(psp_t));
	psp->first_mcb = CPU_DX + (PSP_SIZE >> 4);
	psp->int_22h.dw = *(UINT32 *)(mem + 4 * 0x22);
	psp->int_23h.dw = *(UINT32 *)(mem + 4 * 0x23);
	psp->int_24h.dw = *(UINT32 *)(mem + 4 * 0x24);
	psp->parent_psp = 0;
}

inline void msdos_int_21h_27h()
{
	ext_fcb_t *ext_fcb = (ext_fcb_t *)(mem + CPU_DS_BASE + CPU_DX);
	fcb_t *fcb = (fcb_t *)(ext_fcb + (ext_fcb->flag == 0xff ? 1 : 0));
	UINT32 rec = (fcb->record_size >= 64) ? fcb->rand_record & 0xffffff : fcb->rand_record;
	
	if(SetFilePointer(fcb->handle, fcb->record_size * rec, NULL, FILE_BEGIN) == INVALID_SET_FILE_POINTER) {
		CPU_AL = 1;
	} else if(CPU_CX == 0) {
		CPU_AL = 0;
	} else {
		process_t *process = msdos_process_info_get(current_psp);
		UINT32 dta_laddr = (process->dta.w.h << 4) + process->dta.w.l;
		UINT32 len = fcb->record_size * CPU_CX;
		memset(mem + dta_laddr, 0, len);
		DWORD num = 0;
		if(!ReadFile(fcb->handle, mem + dta_laddr, len, &num, NULL) || num == 0) {
			CPU_AL = 1;
		} else {
			UINT16 nrec = num / fcb->record_size + ((num % fcb->record_size) != 0);
			rec += nrec;
			fcb->rand_record = (fcb->record_size >= 64) ? (fcb->rand_record & 0xff000000) | (rec & 0xffffff) : rec;
			CPU_AL = (num == len) ? 0 : 3;
			CPU_CX = nrec;
		}
	}
	fcb->current_block = rec / 128;
	fcb->cur_record = rec % 128;
}

inline void msdos_int_21h_28h()
{
	ext_fcb_t *ext_fcb = (ext_fcb_t *)(mem + CPU_DS_BASE + CPU_DX);
	fcb_t *fcb = (fcb_t *)(ext_fcb + (ext_fcb->flag == 0xff ? 1 : 0));
	UINT32 rec = (fcb->record_size >= 64) ? fcb->rand_record & 0xffffff : fcb->rand_record;
	
	if(SetFilePointer(fcb->handle, fcb->record_size * rec, NULL, FILE_BEGIN) == INVALID_SET_FILE_POINTER) {
		CPU_AL = 1;
	} else if(CPU_CX == 0) {
		if(!SetEndOfFile(fcb->handle)) {
			CPU_AL = 1;
		} else {
			fcb->file_size = GetFileSize(fcb->handle, NULL);
			CPU_AL = 0;
		}
	} else {
		process_t *process = msdos_process_info_get(current_psp);
		UINT32 dta_laddr = (process->dta.w.h << 4) + process->dta.w.l;
		UINT32 len = fcb->record_size * CPU_CX;
		DWORD num = 0;
		WriteFile(fcb->handle, mem + dta_laddr, len, &num, NULL);
		fcb->file_size = GetFileSize(fcb->handle, NULL);
		UINT16 nrec = num / fcb->record_size + ((num % fcb->record_size) != 0);
		rec += nrec;
		fcb->rand_record = (fcb->record_size >= 64) ? (fcb->rand_record & 0xff000000) | (rec & 0xffffff) : rec;
		CPU_AL = (num == len) ? 0 : 1;
		CPU_CX = nrec;
	}
	fcb->current_block = rec / 128;
	fcb->cur_record = rec % 128;
}

inline void msdos_int_21h_29h()
{
	int ofs = 0;//CPU_DS_BASE + CPU_SI;
	char buffer[1024], name[MAX_PATH], ext[MAX_PATH];
	UINT8 drv = 0;
	char sep_chars[] = ":.;,=+";
	char end_chars[] = "\\<>|/\"[]";
	char spc_chars[] = " \t";
	
	memcpy(buffer, mem + CPU_DS_BASE + CPU_SI, 1023);
	buffer[1023] = 0;
	memset(name, 0x20, sizeof(name));
	memset(ext, 0x20, sizeof(ext));
	
	if(CPU_AL & 1) {
		ofs += strspn((char *)(buffer + ofs), spc_chars);
		if(my_strchr(sep_chars, buffer[ofs]) && buffer[ofs] != '\0') {
			ofs++;
		}
	}
	ofs += strspn((char *)(buffer + ofs), spc_chars);
	
	if(buffer[ofs + 1] == ':') {
		if(buffer[ofs] >= 'a' && buffer[ofs] <= 'z') {
			drv = buffer[ofs] - 'a' + 1;
			ofs += 2;
			if(buffer[ofs] == '\\' || buffer[ofs] == '/') {
				ofs++;
			}
		} else if(buffer[ofs] >= 'A' && buffer[ofs] <= 'Z') {
			drv = buffer[ofs] - 'A' + 1;
			ofs += 2;
			if(buffer[ofs] == '\\' || buffer[ofs] == '/') {
				ofs++;
			}
		}
	}
	for(int i = 0, is_kanji = 0; i < MAX_PATH; i++) {
		UINT8 c = buffer[ofs];
		if(is_kanji) {
			is_kanji = 0;
		} else if(msdos_lead_byte_check(c)) {
			is_kanji = 1;
		} else if(c <= 0x20 || my_strchr(end_chars, c) || my_strchr(sep_chars, c)) {
			break;
		} else if(c >= 'a' && c <= 'z') {
			c -= 0x20;
		}
		ofs++;
		name[i] = c;
	}
	if(buffer[ofs] == '.') {
		ofs++;
		for(int i = 0, is_kanji = 0; i < MAX_PATH; i++) {
			UINT8 c = buffer[ofs];
			if(is_kanji) {
				is_kanji = 0;
			} else if(msdos_lead_byte_check(c)) {
				is_kanji = 1;
			} else if(c <= 0x20 || my_strchr(end_chars, c) || my_strchr(sep_chars, c)) {
				break;
			} else if(c >= 'a' && c <= 'z') {
				c -= 0x20;
			}
			ofs++;
			ext[i] = c;
		}
	}
	int si = CPU_SI + ofs;
	int ds = CPU_DS;
	while(si > 0xffff) {
		si -= 0x10;
		ds++;
	}
	CPU_SI = si;
	CPU_LOAD_SREG(CPU_DS_INDEX, ds);
	
	UINT8 *fcb = mem + CPU_ES_BASE + CPU_DI;
	if(!(CPU_AL & 2) || drv != 0) {
		fcb[0] = drv;
	}
	if(!(CPU_AL & 4) || name[0] != 0x20) {
		memcpy(fcb + 1, name, 8);
	}
	if(!(CPU_AL & 8) || ext[0] != 0x20) {
		memcpy(fcb + 9, ext, 3);
	}
	for(int i = 1, found_star = 0; i < 1 + 8; i++) {
		if(fcb[i] == '*') {
			found_star = 1;
		}
		if(found_star) {
			fcb[i] = '?';
		}
	}
	for(int i = 9, found_star = 0; i < 9 + 3; i++) {
		if(fcb[i] == '*') {
			found_star = 1;
		}
		if(found_star) {
			fcb[i] = '?';
		}
	}
	
	if(drv == 0 || msdos_is_valid_drive(drv - 1)) {
		if(memchr(fcb + 1, '?', 8 + 3)) {
			CPU_AL = 0x01;
		} else {
			CPU_AL = 0x00;
		}
	} else {
		CPU_AL = 0xff;
	}
}

inline void msdos_int_21h_2ah()
{
	SYSTEMTIME sTime;
	
	GetLocalTime(&sTime);
	CPU_CX = sTime.wYear;
	CPU_DH = (UINT8)sTime.wMonth;
	CPU_DL = (UINT8)sTime.wDay;
	CPU_AL = (UINT8)sTime.wDayOfWeek;
}

inline void msdos_int_21h_2bh()
{
	CPU_AL = 0xff;
}

inline void msdos_int_21h_2ch()
{
	SYSTEMTIME sTime;
	
	GetLocalTime(&sTime);
	CPU_CH = (UINT8)sTime.wHour;
	CPU_CL = (UINT8)sTime.wMinute;
	CPU_DH = (UINT8)sTime.wSecond;
	CPU_DL = (UINT8)(sTime.wMilliseconds / 10);
}

inline void msdos_int_21h_2dh()
{
	CPU_AL = 0x00;
}

inline void msdos_int_21h_2eh()
{
	process_t *process = msdos_process_info_get(current_psp);
	
	process->verify = CPU_AL;
}

inline void msdos_int_21h_2fh()
{
	process_t *process = msdos_process_info_get(current_psp);
	
	CPU_BX = process->dta.w.l;
	CPU_LOAD_SREG(CPU_ES_INDEX, process->dta.w.h);
}

inline void msdos_int_21h_30h()
{
	// Version Flag / OEM
	if(CPU_AL == 0x01) {
#ifdef SUPPORT_HMA
		CPU_BX = 0x0000;
#else
		CPU_BX = 0x1000; // DOS is in HMA
#endif
	} else {
		// NOTE: EXDEB invites BL shows the machine type (0=PC-98, 1=PC/AT, 2=FMR),
		// but this is not correct on Windows 98 SE
//		CPU_BX = 0xff01;	// OEM = Microsoft, PC/AT
		CPU_BX = 0xff00;	// OEM = Microsoft
	}
	CPU_CX = 0x0000;
	CPU_AL = dos_major_version;	// 7
	CPU_AH = dos_minor_version;	// 10
}

inline void msdos_int_21h_31h()
{
	try {
		msdos_mem_realloc(current_psp, CPU_DX, NULL);
	} catch(...) {
		// recover the broken mcb
		int mcb_seg = current_psp - 1;
		mcb_t *mcb = (mcb_t *)(mem + (mcb_seg << 4));
		
		if(mcb_seg < (MEMORY_END >> 4)) {
			mcb->mz = 'M';
			mcb->paragraphs = (MEMORY_END >> 4) - mcb_seg - 2;
			
			if(((dos_info_t *)(mem + DOS_INFO_TOP))->umb_linked & 0x01) {
				msdos_mcb_create((MEMORY_END >> 4) - 1, 'M', PSP_SYSTEM, (UMB_TOP >> 4) - (MEMORY_END >> 4), "SC");
			} else {
				msdos_mcb_create((MEMORY_END >> 4) - 1, 'Z', PSP_SYSTEM, 0, "SC");
			}
		} else {
			mcb->mz = 'Z';
			mcb->paragraphs = (UMB_END >> 4) - mcb_seg - 1;
		}
		msdos_mem_realloc(current_psp, CPU_DX, NULL);
	}
	msdos_process_terminate(current_psp, CPU_AL | 0x300, 0);
}

inline void msdos_int_21h_32h()
{
	int drive_num = (CPU_DL == 0) ? (_getdrive() - 1) : (CPU_DL - 1);
	UINT16 seg, ofs;
	
	if(msdos_drive_param_block_update(drive_num, &seg, &ofs, 1)) {
		CPU_AL = 0;
		CPU_LOAD_SREG(CPU_DS_INDEX, seg);
		CPU_BX = ofs;
	} else {
		CPU_AL = 0xff;
		CPU_SET_C_FLAG(1);
	}
}

inline void msdos_int_21h_33h()
{
	char path[MAX_PATH];
	char drive = 3; // C:
	
	switch(CPU_AL) {
	case 0x00:
		CPU_DL = ctrl_break_checking;
		break;
	case 0x01:
		ctrl_break_checking = CPU_DL;
		break;
	case 0x02:
		{
			UINT8 old = ctrl_break_checking;
			ctrl_break_checking = CPU_DL;
			CPU_DL = old;
		}
		break;
	case 0x03:
	case 0x04:
		// DOS 4.0+ - Unused
		break;
	case 0x05:
		if(GetSystemDirectoryA(path, MAX_PATH) != 0) {
			if(path[0] >= 'a' && path[0] <= 'z') {
				drive = path[0] - 'a' + 1;
			} else if(path[0] >= 'A' && path[0] <= 'Z') {
				drive = path[0] - 'A' + 1;
			}
		}
		CPU_DL = (UINT8)drive;
		break;
	case 0x06:
		// MS-DOS version (7.10)
		CPU_BL = 7;
		CPU_BH = 10;
		CPU_DL = 0;
#ifdef SUPPORT_HMA
		CPU_DH = 0x00;
#else
		CPU_DH = 0x10; // DOS is in HMA
#endif
		break;
	case 0x07:
		if(CPU_DL == 0) {
			((dos_info_t *)(mem + DOS_INFO_TOP))->dos_flag &= ~0x20;
		} else if(CPU_DL == 1) {
			((dos_info_t *)(mem + DOS_INFO_TOP))->dos_flag |= 0x20;
		}
		break;
	default:
		unimplemented_21h("int %02Xh (AX=%04X BX=%04X CX=%04X DX=%04X SI=%04X DI=%04X DS=%04X ES=%04X)\n", 0x21, CPU_AX, CPU_BX, CPU_CX, CPU_DX, CPU_SI, CPU_DI, CPU_DS, CPU_ES);
//		CPU_AX = 0x01;
//		CPU_SET_C_FLAG(1);
		CPU_AL = 0xff;
		break;
	}
}

inline void msdos_int_21h_34h()
{
	CPU_LOAD_SREG(CPU_ES_INDEX, SDA_TOP >> 4);
	CPU_BX = offsetof(sda_t, indos_flag);
}

inline void msdos_int_21h_35h()
{
	CPU_BX = *(UINT16 *)(mem + 4 * CPU_AL + 0);
	CPU_LOAD_SREG(CPU_ES_INDEX, *(UINT16 *)(mem + 4 * CPU_AL + 2));
}

inline void msdos_int_21h_36h()
{
	struct _diskfree_t df = {0};
	
	if(_getdiskfree(CPU_DL, &df) == 0) {
		CPU_AX = (UINT16)df.sectors_per_cluster;
		CPU_CX = (UINT16)df.bytes_per_sector;
		CPU_BX = df.avail_clusters > 0xFFFF ? 0xFFFF : (UINT16)df.avail_clusters;
		CPU_DX = df.total_clusters > 0xFFFF ? 0xFFFF : (UINT16)df.total_clusters;
	} else {
		CPU_AX = 0xffff;
	}
}

inline void msdos_int_21h_37h()
{
	static UINT8 dev_flag = 0xff;
	
	switch(CPU_AL) {
	case 0x00:
		{
			process_t *process = msdos_process_info_get(current_psp);
			CPU_AL = 0x00;
			CPU_DL = process->switchar;
		}
		break;
	case 0x01:
		{
			process_t *process = msdos_process_info_get(current_psp);
			CPU_AL = 0x00;
			process->switchar = CPU_DL;
			msdos_sda_update(current_psp);
		}
		break;
	case 0x02:
		CPU_DL = dev_flag;
		break;
	case 0x03:
		dev_flag = CPU_DL;
		break;
	case 0xd0:
	case 0xd1:
	case 0xd2:
	case 0xd3:
	case 0xd4:
	case 0xd5:
	case 0xd6:
	case 0xd7:
	case 0xdc:
	case 0xdd:
	case 0xde:
	case 0xdf:
		// DIET v1.43e
//		CPU_AX = 1;
		CPU_AL = 0xff;
		break;
	default:
		unimplemented_21h("int %02Xh (AX=%04X BX=%04X CX=%04X DX=%04X SI=%04X DI=%04X DS=%04X ES=%04X)\n", 0x21, CPU_AX, CPU_BX, CPU_CX, CPU_DX, CPU_SI, CPU_DI, CPU_DS, CPU_ES);
//		CPU_AX = 1;
		CPU_AL = 0xff;
		break;
	}
}

int get_country_info(country_info_t *ci, LCID locale = LOCALE_USER_DEFAULT)
{
	char LCdata[80];
	
	ZeroMemory(ci, offsetof(country_info_t, reserved));
	GetLocaleInfoA(locale, LOCALE_ICURRDIGITS, LCdata, sizeof(LCdata));
	ci->currency_dec_digits = atoi(LCdata);
	GetLocaleInfoA(locale, LOCALE_ICURRENCY, LCdata, sizeof(LCdata));
	ci->currency_format = *LCdata - '0';
	GetLocaleInfoA(locale, LOCALE_IDATE, LCdata, sizeof(LCdata));
	ci->date_format = *LCdata - '0';
	GetLocaleInfoA(locale, LOCALE_SCURRENCY, LCdata, sizeof(LCdata));
	memcpy(&ci->currency_symbol, LCdata, 4);
	GetLocaleInfoA(locale, LOCALE_SDATE, LCdata, sizeof(LCdata));
	*ci->date_sep = *LCdata;
	GetLocaleInfoA(locale, LOCALE_SDECIMAL, LCdata, sizeof(LCdata));
	*ci->dec_sep = *LCdata;
	GetLocaleInfoA(locale, LOCALE_SLIST, LCdata, sizeof(LCdata));
	*ci->list_sep = *LCdata;
	GetLocaleInfoA(locale, LOCALE_STHOUSAND, LCdata, sizeof(LCdata));
	*ci->thou_sep = *LCdata;
	GetLocaleInfoA(locale, LOCALE_STIME, LCdata, sizeof(LCdata));
	*ci->time_sep = *LCdata;
	GetLocaleInfoA(locale, LOCALE_STIMEFORMAT, LCdata, sizeof(LCdata));
	if(strchr(LCdata, 'H') != NULL) {
		ci->time_format = 1;
	}
	ci->case_map.w.l = 0x000a; // dummy case map routine is at fffc:000a
	ci->case_map.w.h = DUMMY_TOP >> 4;
	GetLocaleInfoA(locale, LOCALE_ICOUNTRY, LCdata, sizeof(LCdata));
	return atoi(LCdata);
}

int get_country_info(country_info_t *ci, USHORT usPrimaryLanguage, USHORT usSubLanguage)
{
	return get_country_info(ci, MAKELCID(MAKELANGID(usPrimaryLanguage, usSubLanguage), SORT_DEFAULT));
}

void set_country_info(country_info_t *ci, int size)
{
	char LCdata[80];
	
	if(size >= 0x00 + 2) {
		memset(LCdata, 0, sizeof(LCdata));
		*LCdata = '0' + ci->date_format;
		SetLocaleInfoA(LOCALE_USER_DEFAULT, LOCALE_IDATE, LCdata);
	}
	if(size >= 0x02 + 5) {
		memset(LCdata, 0, sizeof(LCdata));
		memcpy(LCdata, &ci->currency_symbol, 4);
		SetLocaleInfoA(LOCALE_USER_DEFAULT, LOCALE_SCURRENCY, LCdata);
	}
	if(size >= 0x07 + 2) {
		memset(LCdata, 0, sizeof(LCdata));
	 	*LCdata = *ci->thou_sep;
		SetLocaleInfoA(LOCALE_USER_DEFAULT, LOCALE_STHOUSAND, LCdata);
	}
	if(size >= 0x09 + 2) {
		memset(LCdata, 0, sizeof(LCdata));
	 	*LCdata = *ci->dec_sep;
		SetLocaleInfoA(LOCALE_USER_DEFAULT, LOCALE_SDECIMAL, LCdata);
	}
	if(size >= 0x0b + 2) {
		memset(LCdata, 0, sizeof(LCdata));
	 	*LCdata = *ci->date_sep;
		SetLocaleInfoA(LOCALE_USER_DEFAULT, LOCALE_SDATE, LCdata);
	}
	if(size >= 0x0d + 2) {
		memset(LCdata, 0, sizeof(LCdata));
	 	*LCdata = *ci->time_sep;
		SetLocaleInfoA(LOCALE_USER_DEFAULT, LOCALE_STIME, LCdata);
	}
	if(size >= 0x0f + 1) {
		memset(LCdata, 0, sizeof(LCdata));
		*LCdata = '0' + ci->currency_format;
		SetLocaleInfoA(LOCALE_USER_DEFAULT, LOCALE_ICURRENCY, LCdata);
	}
	if(size >= 0x10 + 1) {
		sprintf(LCdata, "%d", ci->currency_dec_digits);
		SetLocaleInfoA(LOCALE_USER_DEFAULT, LOCALE_ICURRDIGITS, LCdata);
	}
	if(size >= 0x11 + 1) {
		// FIXME: is time format always H/h:mm:ss ???
		if(ci->time_format & 1) {
			sprintf(LCdata, "H%cmm%css", *ci->time_sep, *ci->time_sep);
		} else {
			sprintf(LCdata, "h%cmm%css", *ci->time_sep, *ci->time_sep);
		}
		SetLocaleInfoA(LOCALE_USER_DEFAULT, LOCALE_STIMEFORMAT, LCdata);
	}
	if(size >= 0x12 + 4) {
		// 12h	DWORD	address of case map routine
		//		(FAR CALL, AL = character to map to upper case [>= 80h])
	}
	if(size >= 0x16 + 2) {
		memset(LCdata, 0, sizeof(LCdata));
	 	*LCdata = *ci->list_sep;
		SetLocaleInfoA(LOCALE_USER_DEFAULT, LOCALE_SLIST, LCdata);
	}
}

#ifndef SUBLANG_SWAHILI
	#define SUBLANG_SWAHILI 0x01
#endif
#ifndef SUBLANG_TSWANA_BOTSWANA
	#define SUBLANG_TSWANA_BOTSWANA 0x02
#endif
#ifndef SUBLANG_LITHUANIAN_LITHUANIA
	#define SUBLANG_LITHUANIAN_LITHUANIA 0x01
#endif
#ifndef LANG_BANGLA
	#define LANG_BANGLA 0x45
#endif
#ifndef SUBLANG_BANGLA_BANGLADESH
	#define SUBLANG_BANGLA_BANGLADESH 0x02
#endif

static const struct {
	int code;
	USHORT usPrimaryLanguage;
	USHORT usSubLanguage;
} country_table[] = {
	{0x001, LANG_ENGLISH, SUBLANG_ENGLISH_US},				// United States
	{0x002, LANG_FRENCH, SUBLANG_FRENCH_CANADIAN},				// Canadian-French
	{0x004, LANG_ENGLISH, SUBLANG_ENGLISH_CAN},				// Canada (English)
	{0x007, LANG_RUSSIAN, SUBLANG_RUSSIAN_RUSSIA},				// Russia
	{0x014, LANG_ARABIC, SUBLANG_ARABIC_EGYPT},				// Egypt
	{0x01B, LANG_ZULU, SUBLANG_ZULU_SOUTH_AFRICA},				// South Africa
//	{0x01B, LANG_XHOSA, SUBLANG_XHOSA_SOUTH_AFRICA},			// South Africa
//	{0x01B, LANG_AFRIKAANS, SUBLANG_AFRIKAANS_SOUTH_AFRICA},		// South Africa
//	{0x01B, LANG_ENGLISH, SUBLANG_ENGLISH_SOUTH_AFRICA},			// South Africa
//	{0x01B, LANG_SOTHO, SUBLANG_SOTHO_NORTHERN_SOUTH_AFRICA},		// South Africa
//	{0x01B, LANG_TSWANA, SUBLANG_TSWANA_SOUTH_AFRICA},			// South Africa
	{0x01E, LANG_GREEK, SUBLANG_GREEK_GREECE},				// Greece
	{0x01F, LANG_DUTCH, SUBLANG_DUTCH},					// Netherlands
//	{0x01F, LANG_FRISIAN, SUBLANG_FRISIAN_NETHERLANDS},			// Netherlands
	{0x020, LANG_DUTCH, SUBLANG_DUTCH_BELGIAN},				// Belgium
//	{0x020, LANG_FRENCH, SUBLANG_FRENCH_BELGIAN},				// Belgium
	{0x021, LANG_FRENCH, SUBLANG_FRENCH},					// France
	{0x022, LANG_SPANISH, SUBLANG_SPANISH},					// Spain
	{0x023, LANG_BULGARIAN, SUBLANG_BULGARIAN_BULGARIA},			// Bulgaria???
	{0x024, LANG_HUNGARIAN, SUBLANG_HUNGARIAN_HUNGARY},			// Hungary (not supported by DR DOS 5.0)
	{0x027, LANG_ITALIAN, SUBLANG_ITALIAN},					// Italy / San Marino / Vatican City
	{0x028, LANG_ROMANIAN, SUBLANG_ROMANIAN_ROMANIA},			// Romania
	{0x029, LANG_GERMAN, SUBLANG_GERMAN_SWISS},				// Switzerland / Liechtenstein
//	{0x029, LANG_FRENCH, SUBLANG_FRENCH_SWISS},				// Switzerland / Liechtenstein
//	{0x029, LANG_ITALIAN, SUBLANG_ITALIAN_SWISS},				// Switzerland / Liechtenstein
	{0x02A, LANG_SLOVAK, SUBLANG_SLOVAK_SLOVAKIA},				// Czechoslovakia / Tjekia / Slovakia (not supported by DR DOS 5.0)
	{0x02B, LANG_GERMAN, SUBLANG_GERMAN_AUSTRIAN},				// Austria (DR DOS 5.0)
	{0x02C, LANG_ENGLISH, SUBLANG_ENGLISH_UK},				// United Kingdom
	{0x02D, LANG_DANISH, SUBLANG_DANISH_DENMARK},				// Denmark
	{0x02E, LANG_SWEDISH, SUBLANG_SWEDISH},					// Sweden
//	{0x02E, LANG_SAMI, SUBLANG_SAMI_NORTHERN_SWEDEN},			// Sweden
//	{0x02E, LANG_SAMI, SUBLANG_SAMI_LULE_SWEDEN},				// Sweden
//	{0x02E, LANG_SAMI, SUBLANG_SAMI_SOUTHERN_SWEDEN},			// Sweden
	{0x02F, LANG_NORWEGIAN, SUBLANG_NORWEGIAN_BOKMAL},			// Norway
//	{0x02F, LANG_NORWEGIAN, SUBLANG_NORWEGIAN_NYNORSK},			// Norway
//	{0x02F, LANG_SAMI, SUBLANG_SAMI_NORTHERN_NORWAY},			// Norway
//	{0x02F, LANG_SAMI, SUBLANG_SAMI_LULE_NORWAY},				// Norway
//	{0x02F, LANG_SAMI, SUBLANG_SAMI_SOUTHERN_NORWAY},			// Norway
	{0x030, LANG_POLISH, SUBLANG_POLISH_POLAND},				// Poland (not supported by DR DOS 5.0)
	{0x031, LANG_GERMAN, SUBLANG_GERMAN},					// Germany
	{0x033, LANG_SPANISH, SUBLANG_SPANISH_PERU},				// Peru
//	{0x033, LANG_QUECHUA, SUBLANG_QUECHUA_PERU},				// Peru
	{0x034, LANG_SPANISH, SUBLANG_SPANISH_MEXICAN},				// Mexico
	{0x035, LANG_SPANISH, SUBLANG_NEUTRAL},					// Cuba
	{0x036, LANG_SPANISH, SUBLANG_SPANISH_ARGENTINA},			// Argentina
	{0x037, LANG_PORTUGUESE, SUBLANG_PORTUGUESE_BRAZILIAN},			// Brazil (not supported by DR DOS 5.0)
	{0x038, LANG_SPANISH, SUBLANG_SPANISH_CHILE},				// Chile
	{0x039, LANG_SPANISH, SUBLANG_SPANISH_COLOMBIA},			// Columbia
	{0x03A, LANG_SPANISH, SUBLANG_SPANISH_VENEZUELA},			// Venezuela
	{0x03C, LANG_MALAY, SUBLANG_MALAY_MALAYSIA},				// Malaysia
	{0x03D, LANG_ENGLISH, SUBLANG_ENGLISH_AUS},				// International English / Australia
	{0x03E, LANG_INDONESIAN, SUBLANG_INDONESIAN_INDONESIA},			// Indonesia / East Timor
	{0x03F, LANG_ENGLISH, SUBLANG_ENGLISH_PHILIPPINES},			// Philippines
	{0x040, LANG_ENGLISH, SUBLANG_ENGLISH_NZ},				// New Zealand
	{0x041, LANG_CHINESE, SUBLANG_CHINESE_SINGAPORE},			// Singapore
//	{0x041, LANG_ENGLISH, SUBLANG_ENGLISH_SINGAPORE},			// Singapore
	{0x042, LANG_CHINESE_TRADITIONAL, SUBLANG_CHINESE_TRADITIONAL},		// Taiwan???
	{0x051, LANG_JAPANESE, SUBLANG_JAPANESE_JAPAN},				// Japan (DR DOS 5.0, MS-DOS 5.0+)
	{0x052, LANG_KOREAN, SUBLANG_KOREAN},					// South Korea (DR DOS 5.0)
	{0x054, LANG_VIETNAMESE, SUBLANG_VIETNAMESE_VIETNAM},			// Vietnam
	{0x056, LANG_CHINESE_SIMPLIFIED, SUBLANG_CHINESE_SIMPLIFIED},		// China (MS-DOS 5.0+)
	{0x058, LANG_CHINESE_TRADITIONAL, SUBLANG_CHINESE_TRADITIONAL},		// Taiwan (MS-DOS 5.0+)
	{0x05A, LANG_TURKISH, SUBLANG_TURKISH_TURKEY},				// Turkey (MS-DOS 5.0+)
	{0x05B, LANG_HINDI, SUBLANG_HINDI_INDIA},				// India
	{0x05C, LANG_URDU, SUBLANG_URDU_PAKISTAN},				// Pakistan
	{0x05D, LANG_PASHTO, SUBLANG_PASHTO_AFGHANISTAN},			// Afghanistan
//	{0x05D, LANG_DARI, SUBLANG_DARI_AFGHANISTAN},				// Afghanistan
	{0x05E, LANG_SINHALESE, SUBLANG_SINHALESE_SRI_LANKA},			// Sri Lanka
//	{0x05E, LANG_TAMIL, SUBLANG_TAMIL_SRI_LANKA},				// Sri Lanka
	{0x062, LANG_PERSIAN, SUBLANG_PERSIAN_IRAN},				// Iran
	{0x070, LANG_BELARUSIAN, SUBLANG_BELARUSIAN_BELARUS},			// Belarus
	{0x0C8, LANG_THAI, SUBLANG_THAI_THAILAND},				// Thailand
	{0x0D4, LANG_ARABIC, SUBLANG_ARABIC_MOROCCO},				// Morocco
	{0x0D5, LANG_ARABIC, SUBLANG_ARABIC_ALGERIA},				// Algeria
	{0x0D8, LANG_ARABIC, SUBLANG_ARABIC_TUNISIA},				// Tunisia
	{0x0DA, LANG_ARABIC, SUBLANG_ARABIC_LIBYA},				// Libya
	{0x0DD, LANG_WOLOF, SUBLANG_WOLOF_SENEGAL},				// Senegal
//	{0x0DD, LANG_PULAR, SUBLANG_PULAR_SENEGAL},				// Senegal
	{0x0EA, LANG_HAUSA, SUBLANG_HAUSA_NIGERIA_LATIN},			// Nigeria
//	{0x0EA, LANG_YORUBA, SUBLANG_YORUBA_NIGERIA},				// Nigeria
//	{0x0EA, LANG_IGBO, SUBLANG_IGBO_NIGERIA},				// Nigeria
	{0x0FB, LANG_AMHARIC, SUBLANG_AMHARIC_ETHIOPIA},			// Ethiopia
//	{0x0FB, LANG_TIGRINYA, SUBLANG_TIGRINYA_ETHIOPIA},			// Ethiopia
	{0x0FE, LANG_SWAHILI, SUBLANG_SWAHILI},					// Kenya
	{0x107, LANG_ENGLISH, SUBLANG_ENGLISH_ZIMBABWE},			// Zimbabwe
	{0x10B, LANG_TSWANA, SUBLANG_TSWANA_BOTSWANA},				// Botswana
	{0x12A, LANG_FAEROESE, SUBLANG_FAEROESE_FAROE_ISLANDS},			// Faroe Islands
	{0x12B, LANG_GREENLANDIC, SUBLANG_GREENLANDIC_GREENLAND},		// Greenland
	{0x15F, LANG_PORTUGUESE, SUBLANG_PORTUGUESE},				// Portugal
	{0x160, LANG_LUXEMBOURGISH, SUBLANG_LUXEMBOURGISH_LUXEMBOURG},		// Luxembourg
//	{0x160, LANG_GERMAN, SUBLANG_GERMAN_LUXEMBOURG},			// Luxembourg
//	{0x160, LANG_FRENCH, SUBLANG_FRENCH_LUXEMBOURG},			// Luxembourg
	{0x161, LANG_IRISH, SUBLANG_IRISH_IRELAND},				// Ireland
//	{0x161, LANG_ENGLISH, SUBLANG_ENGLISH_IRELAND},				// Ireland
	{0x162, LANG_ICELANDIC, SUBLANG_ICELANDIC_ICELAND},			// Iceland
	{0x163, LANG_ALBANIAN, SUBLANG_ALBANIAN_ALBANIA},			// Albania
	{0x164, LANG_MALTESE, SUBLANG_MALTESE_MALTA},				// Malta
	{0x166, LANG_FINNISH, SUBLANG_FINNISH_FINLAND},				// Finland
//	{0x166, LANG_SAMI, SUBLANG_SAMI_NORTHERN_FINLAND},			// Finland
//	{0x166, LANG_SAMI, SUBLANG_SAMI_SKOLT_FINLAND},				// Finland
//	{0x166, LANG_SAMI, SUBLANG_SAMI_INARI_FINLAND},				// Finland
	{0x167, LANG_BULGARIAN, SUBLANG_BULGARIAN_BULGARIA},			// Bulgaria
	{0x172, LANG_LITHUANIAN, SUBLANG_LITHUANIAN_LITHUANIA},			// Lithuania
	{0x173, LANG_LATVIAN, SUBLANG_LATVIAN_LATVIA},				// Latvia
	{0x174, LANG_ESTONIAN, SUBLANG_ESTONIAN_ESTONIA},			// Estonia
	{0x17D, LANG_SERBIAN, SUBLANG_SERBIAN_LATIN},				// Serbia / Montenegro
	{0x180, LANG_SERBIAN, SUBLANG_SERBIAN_CROATIA},				// Croatia???
	{0x181, LANG_SERBIAN, SUBLANG_SERBIAN_CROATIA},				// Croatia
	{0x182, LANG_SLOVENIAN, SUBLANG_SLOVENIAN_SLOVENIA},			// Slovenia
	{0x183, LANG_BOSNIAN, SUBLANG_BOSNIAN_BOSNIA_HERZEGOVINA_LATIN},	// Bosnia-Herzegovina (Latin)
	{0x184, LANG_BOSNIAN, SUBLANG_BOSNIAN_BOSNIA_HERZEGOVINA_CYRILLIC},	// Bosnia-Herzegovina (Cyrillic)
	{0x185, LANG_MACEDONIAN, SUBLANG_MACEDONIAN_MACEDONIA},			// FYR Macedonia
	{0x1A5, LANG_CZECH, SUBLANG_CZECH_CZECH_REPUBLIC},			// Czech Republic
	{0x1A6, LANG_SLOVAK, SUBLANG_SLOVAK_SLOVAKIA},				// Slovakia
	{0x1F5, LANG_ENGLISH, SUBLANG_ENGLISH_BELIZE},				// Belize
	{0x1F6, LANG_SPANISH, SUBLANG_SPANISH_GUATEMALA},			// Guatemala
	{0x1F7, LANG_SPANISH, SUBLANG_SPANISH_EL_SALVADOR},			// El Salvador
	{0x1F8, LANG_SPANISH, SUBLANG_SPANISH_HONDURAS},			// Honduras
	{0x1F9, LANG_SPANISH, SUBLANG_SPANISH_NICARAGUA},			// Nicraragua
	{0x1FA, LANG_SPANISH, SUBLANG_SPANISH_COSTA_RICA},			// Costa Rica
	{0x1FB, LANG_SPANISH, SUBLANG_SPANISH_PANAMA},				// Panama
	{0x24F, LANG_SPANISH, SUBLANG_SPANISH_BOLIVIA},				// Bolivia
//	{0x24F, LANG_QUECHUA, SUBLANG_QUECHUA_BOLIVIA},				// Bolivia
	{0x251, LANG_SPANISH, SUBLANG_SPANISH_ECUADOR},				// Ecuador
//	{0x251, LANG_QUECHUA, SUBLANG_QUECHUA_ECUADOR},				// Ecuador
	{0x253, LANG_SPANISH, SUBLANG_SPANISH_PARAGUAY},			// Paraguay
	{0x256, LANG_SPANISH, SUBLANG_SPANISH_URUGUAY},				// Uruguay
	{0x2A1, LANG_MALAY, SUBLANG_MALAY_BRUNEI_DARUSSALAM},			// Brunei Darussalam
	{0x311, LANG_ARABIC, SUBLANG_NEUTRAL},					// Arabic (Middle East/Saudi Arabia/etc.)
	{0x324, LANG_UKRAINIAN, SUBLANG_UKRAINIAN_UKRAINE},			// Ukraine
	{0x352, LANG_KOREAN, SUBLANG_KOREAN},					// North Korea
	{0x354, LANG_CHINESE, SUBLANG_CHINESE_HONGKONG},			// Hong Kong
	{0x355, LANG_CHINESE, SUBLANG_CHINESE_MACAU},				// Macao
	{0x357, LANG_KHMER, SUBLANG_KHMER_CAMBODIA},				// Cambodia
	{0x370, LANG_BANGLA, SUBLANG_BANGLA_BANGLADESH},			// Bangladesh
	{0x376, LANG_CHINESE_TRADITIONAL, SUBLANG_CHINESE_TRADITIONAL},		// Taiwan (DOS 6.22+)
	{0x3C0, LANG_DIVEHI, SUBLANG_DIVEHI_MALDIVES},				// Maldives
	{0x3C1, LANG_ARABIC, SUBLANG_ARABIC_LEBANON},				// Lebanon
	{0x3C2, LANG_ARABIC, SUBLANG_ARABIC_JORDAN},				// Jordan
	{0x3C3, LANG_ARABIC, SUBLANG_ARABIC_SYRIA},				// Syrian Arab Republic
	{0x3C4, LANG_ARABIC, SUBLANG_ARABIC_IRAQ},				// Ireq
	{0x3C5, LANG_ARABIC, SUBLANG_ARABIC_KUWAIT},				// Kuwait
	{0x3C6, LANG_ARABIC, SUBLANG_ARABIC_SAUDI_ARABIA},			// Saudi Arabia
	{0x3C7, LANG_ARABIC, SUBLANG_ARABIC_YEMEN},				// Yemen
	{0x3C8, LANG_ARABIC, SUBLANG_ARABIC_OMAN},				// Oman
	{0x3CB, LANG_ARABIC, SUBLANG_ARABIC_UAE},				// United Arab Emirates
	{0x3CC, LANG_HEBREW, SUBLANG_HEBREW_ISRAEL},				// Israel (Hebrew) (DR DOS 5.0,MS-DOS 5.0+)
	{0x3CD, LANG_ARABIC, SUBLANG_ARABIC_BAHRAIN},				// Bahrain
	{0x3CE, LANG_ARABIC, SUBLANG_ARABIC_QATAR},				// Qatar
	{0x3D0, LANG_MONGOLIAN, SUBLANG_MONGOLIAN_CYRILLIC_MONGOLIA},		// Mongolia
//	{0x3D0, LANG_MONGOLIAN, SUBLANG_MONGOLIAN_PRC},				// Mongolia
	{0x3D1, LANG_NEPALI, SUBLANG_NEPALI_NEPAL},				// Nepal
	{-1, 0, 0},
};

inline void msdos_int_21h_38h()
{
	switch(CPU_AL) {
	case 0x00:
		CPU_BX = get_country_info((country_info_t *)(mem + CPU_DS_BASE + CPU_DX));
		break;
	default:
		for(int i = 0;; i++) {
			if(country_table[i].code == (CPU_AL != 0xff ? CPU_AL : CPU_BX)) {
				CPU_BX = get_country_info((country_info_t *)(mem + CPU_DS_BASE + CPU_DX), country_table[i].usPrimaryLanguage, country_table[i].usSubLanguage);
				break;
			} else if(country_table[i].code == -1) {
//				unimplemented_21h("int %02Xh (AX=%04X BX=%04X CX=%04X DX=%04X SI=%04X DI=%04X DS=%04X ES=%04X)\n", 0x21, CPU_AX, CPU_BX, CPU_CX, CPU_DX, CPU_SI, CPU_DI, CPU_DS, CPU_ES);
//				CPU_AX = 2;
//				CPU_SET_C_FLAG(1);
				// get current coutry info
				CPU_BX = get_country_info((country_info_t *)(mem + CPU_DS_BASE + CPU_DX));
				break;
			}
		}
		break;
	}
}

inline void msdos_int_21h_39h(int lfn)
{
	if(_mkdir(msdos_trimmed_path((char *)(mem + CPU_DS_BASE + CPU_DX), lfn, 1))) {
		CPU_AX = msdos_maperr(_doserrno);
		CPU_SET_C_FLAG(1);
	}
}

inline void msdos_int_21h_3ah(int lfn)
{
	if(_rmdir(msdos_trimmed_path((char *)(mem + CPU_DS_BASE + CPU_DX), lfn, 1))) {
		CPU_AX = msdos_maperr(_doserrno);
		CPU_SET_C_FLAG(1);
	}
}

inline void msdos_int_21h_3bh(int lfn)
{
	const char *path = msdos_trimmed_path((char *)(mem + CPU_DS_BASE + CPU_DX), lfn, 1);
	
	if(_chdir(path)) {
		CPU_AX = 3;	// must be 3 (path not found)
		CPU_SET_C_FLAG(1);
	} else {
		int drv = _getdrive() - 1;
		if(path[1] == ':') {
			if(path[0] >= 'A' && path[0] <= 'Z') {
				drv = path[0] - 'A';
			} else if(path[0] >= 'a' && path[0] <= 'z') {
				drv = path[0] - 'a';
			}
		}
		msdos_cds_update(drv, path);
	}
}

inline void msdos_int_21h_3ch()
{
	const char *path = msdos_local_file_path((char *)(mem + CPU_DS_BASE + CPU_DX), 0);
	int attr = GetFileAttributesA(path);
	int fd = -1;
	int sio_port = 0;
	int lpt_port = 0;
	
	if(msdos_is_device_path(path)) {
		fd = msdos_open_device(path, _O_WRONLY | _O_BINARY, &sio_port, &lpt_port);
#ifdef ENABLE_DEBUG_OPEN_FILE
		fprintf(fp_debug_log, "int21h_3ch\tfd=%d\tw\t%s\n", fd, path);
#endif
	} else {
		fd = _open(path, _O_RDWR | _O_BINARY | _O_CREAT | _O_TRUNC, _S_IREAD | _S_IWRITE);
#ifdef ENABLE_DEBUG_OPEN_FILE
		fprintf(fp_debug_log, "int21h_3ch\tfd=%d\trw\t%s\n", fd, path);
#endif
	}
	if(fd != -1) {
		if(attr == -1) {
			attr = msdos_file_attribute_create(CPU_CX) & ~FILE_ATTRIBUTE_READONLY;
		}
		SetFileAttributesA(path, attr);
		CPU_AX = fd;
		msdos_file_handler_open(fd, path, _isatty(fd), 2, msdos_device_info(path), current_psp, sio_port, lpt_port);
		msdos_psp_set_file_table(fd, fd, current_psp);
	} else {
		CPU_AX = msdos_maperr(_doserrno);
		CPU_SET_C_FLAG(1);
	}
}

inline void msdos_int_21h_3dh()
{
	const char *path = msdos_local_file_path((char *)(mem + CPU_DS_BASE + CPU_DX), 0);
	int mode = CPU_AL & 0x03;
	int fd = -1;
	int sio_port = 0;
	int lpt_port = 0;
	
	if(mode < 0x03) {
		if(msdos_is_device_path(path)) {
			fd = msdos_open_device(path, file_mode[mode].mode, &sio_port, &lpt_port);
		} else {
			fd = msdos_open(path, file_mode[mode].mode);
		}
#ifdef ENABLE_DEBUG_OPEN_FILE
		fprintf(fp_debug_log, "int21h_3dh\tfd=%d\t%s\t%s\n", fd, file_mode[mode].str, path);
#endif
		if(fd != -1) {
			CPU_AX = fd;
			msdos_file_handler_open(fd, path, _isatty(fd), mode, msdos_device_info(path), current_psp, sio_port, lpt_port);
			msdos_psp_set_file_table(fd, fd, current_psp);
		} else {
			CPU_AX = msdos_maperr(_doserrno);
			CPU_SET_C_FLAG(1);
		}
	} else {
		CPU_AX = 0x0c;
		CPU_SET_C_FLAG(1);
	}
}

inline void msdos_int_21h_3eh()
{
	process_t *process = msdos_process_info_get(current_psp);
	int fd = msdos_psp_get_file_table(CPU_BX, current_psp);
	
	if(fd < process->max_files && file_handler[fd].valid) {
		if(!msdos_file_handler_close(fd)) {
			_close(fd);
		}
		msdos_psp_set_file_table(CPU_BX, 0x0ff, current_psp);
	} else {
		CPU_AX = 0x06;
		CPU_SET_C_FLAG(1);
	}
}

DWORD WINAPI msdos_int_21h_3fh_thread(LPVOID)
{
	UINT8 *buf = mem + CPU_DS_BASE_in_service + CPU_DX_in_service;
	int max = CPU_CX_in_service;
	int p = 0;
	
	while(max > p) {
		int chr = msdos_getch(0x21, 0x3f);
		
		if((ctrl_break_checking && ctrl_break_pressed) || ctrl_c_pressed) {
			p = 0;
			buf[p++] = 0x0d;
			if(max > p) {
				buf[p++] = 0x0a;
			}
			msdos_putch(0x03, 0x21, 0x3f);
			msdos_putch(0x0d, 0x21, 0x3f);
			msdos_putch(0x0a, 0x21, 0x3f);
			break;
		} else if(ctrl_break_pressed) {
			// skip this byte
		} else if(chr == 0x00) {
			// skip 2nd byte
			msdos_getch(0x21, 0x3f);
		} else if(chr == 0x0d) {
			// carriage return
			buf[p++] = 0x0d;
			if(max > p) {
				buf[p++] = 0x0a;
			}
			msdos_putch('\n', 0x21, 0x3f);
			break;
		} else if(chr == 0x08) {
			// back space
			if(p > 0) {
				p--;
				if(msdos_ctrl_code_check(buf[p], 0x21, 0x3f)) {
					msdos_putch(0x08, 0x21, 0x3f);
					msdos_putch(0x08, 0x21, 0x3f);
					msdos_putch(0x20, 0x21, 0x3f);
					msdos_putch(0x20, 0x21, 0x3f);
					msdos_putch(0x08, 0x21, 0x3f);
					msdos_putch(0x08, 0x21, 0x3f);
				} else if(p > 0 && msdos_kanji_2nd_byte_check(buf, p)) {
					p--;
					msdos_putch(0x08, 0x21, 0x3f);
					msdos_putch(0x08, 0x21, 0x3f);
					msdos_putch(0x20, 0x21, 0x3f);
					msdos_putch(0x20, 0x21, 0x3f);
					msdos_putch(0x08, 0x21, 0x3f);
					msdos_putch(0x08, 0x21, 0x3f);
				} else {
					msdos_putch(0x08, 0x21, 0x3f);
					msdos_putch(0x20, 0x21, 0x3f);
					msdos_putch(0x08, 0x21, 0x3f);
				}
			}
		} else if(chr == 0x1b) {
			// escape
			while(p > 0) {
				p--;
				if(msdos_ctrl_code_check(buf[p], 0x21, 0x3f)) {
					msdos_putch(0x08, 0x21, 0x3f);
					msdos_putch(0x08, 0x21, 0x3f);
					msdos_putch(0x20, 0x21, 0x3f);
					msdos_putch(0x20, 0x21, 0x3f);
					msdos_putch(0x08, 0x21, 0x3f);
					msdos_putch(0x08, 0x21, 0x3f);
				} else {
					msdos_putch(0x08, 0x21, 0x3f);
					msdos_putch(0x20, 0x21, 0x3f);
					msdos_putch(0x08, 0x21, 0x3f);
				}
			}
		} else {
			buf[p++] = chr;
			msdos_putch(chr, 0x21, 0x3f);
		}
	}
	CPU_AX_in_service = p;
	
#ifdef USE_SERVICE_THREAD
	service_exit = true;
#endif
	return(0);
}

inline void msdos_int_21h_3fh()
{
	process_t *process = msdos_process_info_get(current_psp);
	int fd = msdos_psp_get_file_table(CPU_BX, current_psp);
	
	if(fd < process->max_files && file_handler[fd].valid) {
		if(file_mode[file_handler[fd].mode].in) {
			if(file_handler[fd].atty) {
				// BX is stdin or is redirected to stdin
				if(CPU_CX != 0) {
#ifdef USE_SERVICE_THREAD
					if(!in_service && !in_service_29h &&
					   *(UINT16 *)(mem + 4 * 0x29 + 0) == (IRET_SIZE + 5 * 0x29) &&
					   *(UINT16 *)(mem + 4 * 0x29 + 2) == (IRET_TOP >> 4)) {
						// msdos_putch() will be used in this service
						// if int 29h is hooked, run this service in main thread to call int 29h
						start_service_loop(msdos_int_21h_3fh_thread);
					} else {
#endif
						prepare_service_loop();
						msdos_int_21h_3fh_thread(NULL);
						cleanup_service_loop();
						REQUEST_HARDWRE_UPDATE();
#ifdef USE_SERVICE_THREAD
					}
#endif
				} else {
					CPU_AX = 0;
				}
			} else {
				if(CPU_CX != 0) {
#if defined(HAS_I386)
					if(CPU_STAT_PM && (CPU_CR0 & CPU_CR0_PG)) {
						UINT32 addr = CPU_DS_BASE + CPU_DX;
						int count = CPU_CX;
						int total = 0;
						int page_count = min(count, 0x1000 - (addr & 0xfff));
						
						while(count > 0) {
							// Mr.cracyc's original code considered the page fault
							int page_total = msdos_read(fd, mem + CPU_TRANS_PAGING_ADDR(addr), page_count);
							total += page_total;
							if(page_total != page_count) {
								break;
							}
							addr += page_total;
							count  -= page_total;
							page_count = min(count, 0x1000);
						}
						CPU_AX = total;
					} else
#endif
					CPU_AX = msdos_read(fd, mem + CPU_DS_BASE + CPU_DX, CPU_CX);
				} else {
					CPU_AX = 0;
				}
			}
		} else {
			CPU_AX = 0x05;
			CPU_SET_C_FLAG(1);
		}
	} else {
		CPU_AX = 0x06;
		CPU_SET_C_FLAG(1);
	}
}

inline void msdos_int_21h_40h()
{
	process_t *process = msdos_process_info_get(current_psp);
	int fd = msdos_psp_get_file_table(CPU_BX, current_psp);
	
	if(fd < process->max_files && file_handler[fd].valid) {
		if(file_mode[file_handler[fd].mode].out) {
			if(CPU_CX) {
				if(file_handler[fd].atty) {
					// BX is stdout/stderr or is redirected to stdout
					for(int i = 0; i < CPU_CX; i++) {
						msdos_putch(mem[CPU_DS_BASE + CPU_DX + i], 0x21, 0x40);
					}
					CPU_AX = CPU_CX;
				} else {
					CPU_AX = msdos_write(fd, mem + CPU_DS_BASE + CPU_DX, CPU_CX);
				}
			} else {
				UINT32 pos = _tell(fd);
				_lseek(fd, 0, SEEK_END);
				UINT32 size = _tell(fd);
				if(pos < size) {
					_lseek(fd, pos, SEEK_SET);
					SetEndOfFile((HANDLE)_get_osfhandle(fd));
				} else {
					for(UINT32 i = size; i < pos; i++) {
						UINT8 tmp = 0;
						msdos_write(fd, &tmp, 1);
					}
					_lseek(fd, pos, SEEK_SET);
				}
				CPU_AX = 0;
			}
		} else {
			CPU_AX = 0x05;
			CPU_SET_C_FLAG(1);
		}
	} else {
		CPU_AX = 0x06;
		CPU_SET_C_FLAG(1);
	}
}

inline void msdos_int_21h_41h(int lfn)
{
	if(remove(msdos_trimmed_path((char *)(mem + CPU_DS_BASE + CPU_DX), lfn))) {
		// When the file is currently open, this function does not fail. (In DR-DOS case, it fails)
		// In this time, the file is deleted but the handle is not closed.
		// Here I only clear the sharing violation error though the file cannot be deleted.
		if(_doserrno != ERROR_SHARING_VIOLATION) {
			CPU_AX = msdos_maperr(_doserrno);
			CPU_SET_C_FLAG(1);
		}
	}
}

inline void msdos_int_21h_42h()
{
	process_t *process = msdos_process_info_get(current_psp);
	int fd = msdos_psp_get_file_table(CPU_BX, current_psp);
	
	if(fd < process->max_files && file_handler[fd].valid) {
		if(CPU_AL < 0x03) {
			static const int ptrname[] = { SEEK_SET, SEEK_CUR, SEEK_END };
			_lseek(fd, (CPU_CX << 16) | CPU_DX, ptrname[CPU_AL]);
			UINT32 pos = _tell(fd);
			CPU_AX = pos & 0xffff;
			CPU_DX = (pos >> 16);
		} else {
			CPU_AX = 0x01;
			CPU_SET_C_FLAG(1);
		}
	} else {
		CPU_AX = 0x06;
		CPU_SET_C_FLAG(1);
	}
}

inline void msdos_int_21h_43h(int lfn)
{
	const char *path = msdos_local_file_path((char *)(mem + CPU_DS_BASE + CPU_DX), lfn);
	int attr;
	
	if(!lfn && CPU_AL > 2) {
		CPU_AX = 0x01;
		CPU_SET_C_FLAG(1);
		return;
	}
	switch(lfn ? CPU_BL : CPU_AL) {
	case 0x00:
		if((attr = GetFileAttributesA(path)) != -1) {
			CPU_CX = (UINT16)msdos_file_attribute_create((UINT16)attr);
		} else {
			CPU_AX = msdos_maperr(GetLastError());
			CPU_SET_C_FLAG(1);
		}
		break;
	case 0x01:
		if(!SetFileAttributesA(path, msdos_file_attribute_create(CPU_CX))) {
			CPU_AX = msdos_maperr(GetLastError());
			CPU_SET_C_FLAG(1);
		}
		break;
	case 0x02:
		{
			DWORD compressed_size = GetCompressedFileSizeA(path, NULL), file_size = 0;
			if(compressed_size != INVALID_FILE_SIZE) {
				if(compressed_size != 0) {
					HANDLE hFile = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
					if(hFile != INVALID_HANDLE_VALUE) {
						file_size = GetFileSize(hFile, NULL);
						CloseHandle(hFile);
					}
					if(compressed_size == file_size) {
						DWORD sectors_per_cluster, bytes_per_sector, free_clusters, total_clusters;
						// this isn't correct if the file is in the NTFS MFT
						if(GetDiskFreeSpaceA(path, &sectors_per_cluster, &bytes_per_sector, &free_clusters, &total_clusters)) {
							compressed_size = ((compressed_size - 1) | (sectors_per_cluster * bytes_per_sector - 1)) + 1;
						}
					}
				}
				CPU_AX = LOWORD(compressed_size);
				CPU_DX = HIWORD(compressed_size);
			} else {
				CPU_AX = msdos_maperr(GetLastError());
				CPU_SET_C_FLAG(1);
			}
		}
		break;
	case 0x03:
	case 0x05:
	case 0x07:
		if(lfn) {
			HANDLE hFile = CreateFileA(path, FILE_WRITE_ATTRIBUTES, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
			if(hFile != INVALID_HANDLE_VALUE) {
				FILETIME local, time;
				DosDateTimeToFileTime(CPU_DI, /*CPU_BL == 5 ? 0 : */CPU_CX, &local);
				if(CPU_BL == 7) {
					ULARGE_INTEGER hund;
					hund.LowPart = local.dwLowDateTime;
					hund.HighPart = local.dwHighDateTime;
					hund.QuadPart += CPU_SI * 100000;
					local.dwLowDateTime = hund.LowPart;
					local.dwHighDateTime = hund.HighPart;
				}
				LocalFileTimeToFileTime(&local, &time);
				if(!SetFileTime(hFile, CPU_BL == 0x07 ? &time : NULL,
						       CPU_BL == 0x05 ? &time : NULL,
						       CPU_BL == 0x03 ? &time : NULL)) {
					CPU_AX = msdos_maperr(GetLastError());
					CPU_SET_C_FLAG(1);
				}
				CloseHandle(hFile);
			} else {
				CPU_AX = msdos_maperr(GetLastError());
				CPU_SET_C_FLAG(1);
			}
		} else {
			// 214303 DR DOS 3.41+ internal - Set Access Rights And Password
			// 214305 DR DOS 5.0-6.0 internal - Set Extended File Attributes
			// 214307 DR DOS 6.0 - Set File Owner
//			unimplemented_21h("int %02Xh (AX=%04X BX=%04X CX=%04X DX=%04X SI=%04X DI=%04X DS=%04X ES=%04X)\n", 0x21, CPU_AX, CPU_BX, CPU_CX, CPU_DX, CPU_SI, CPU_DI, CPU_DS, CPU_ES);
			CPU_AX = 0x01;
			CPU_SET_C_FLAG(1);
		}
		break;
	case 0x04:
	case 0x06:
	case 0x08:
		if(lfn) {
			WIN32_FILE_ATTRIBUTE_DATA fad;
			if(GetFileAttributesExA(path, GetFileExInfoStandard, (LPVOID)&fad)) {
				FILETIME *time, local;
				time = CPU_BL == 0x04 ? &fad.ftLastWriteTime :
						   0x06 ? &fad.ftLastAccessTime :
							  &fad.ftCreationTime;
				FileTimeToLocalFileTime(time, &local);
				FileTimeToDosDateTime(&local, &CPU_DI, &CPU_CX);
				if(CPU_BL == 0x08) {
					ULARGE_INTEGER hund;
					hund.LowPart = local.dwLowDateTime;
					hund.HighPart = local.dwHighDateTime;
					hund.QuadPart /= 100000;
					CPU_SI = (UINT16)(hund.QuadPart % 200);
				}
			} else {
				CPU_AX = msdos_maperr(GetLastError());
				CPU_SET_C_FLAG(1);
			}
		} else {
			// 214304 DR DOS 5.0-6.0 internal - Get Encrypted Password
			// 214306 DR DOS 6.0 - Get File Owner
//			unimplemented_21h("int %02Xh (AX=%04X BX=%04X CX=%04X DX=%04X SI=%04X DI=%04X DS=%04X ES=%04X)\n", 0x21, CPU_AX, CPU_BX, CPU_CX, CPU_DX, CPU_SI, CPU_DI, CPU_DS, CPU_ES);
			CPU_AX = 0x01;
			CPU_SET_C_FLAG(1);
		}
		break;
	case 0xff:
		if(!lfn && CPU_BP == 0x5053) {
			if(CPU_CL == 0x39) {
				msdos_int_21h_39h(1);
				break;
			} else if(CPU_CL == 0x56) {
				msdos_int_21h_56h(1);
				break;
			}
		}
	default:
		unimplemented_21h("int %02Xh (AX=%04X BX=%04X CX=%04X DX=%04X SI=%04X DI=%04X DS=%04X ES=%04X)\n", 0x21, CPU_AX, CPU_BX, CPU_CX, CPU_DX, CPU_SI, CPU_DI, CPU_DS, CPU_ES);
		CPU_AX = lfn ? 0x7100 : 0x01;
		CPU_SET_C_FLAG(1);
		break;
	}
}

inline void msdos_int_21h_44h()
{
	static UINT16 iteration_count = 0;
	
	process_t *process;
	int fd, drv;
	
	switch(CPU_AL) {
	case 0x00:
	case 0x01:
	case 0x02:
	case 0x03:
	case 0x04:
	case 0x05:
	case 0x06:
	case 0x07:
		process = msdos_process_info_get(current_psp);
		fd = msdos_psp_get_file_table(CPU_BX, current_psp);
		if(fd >= process->max_files || !file_handler[fd].valid) {
			CPU_AX = 0x06;
			CPU_SET_C_FLAG(1);
			return;
		}
		break;
	case 0x08:
	case 0x09:
		drv = (CPU_BL ? CPU_BL : _getdrive()) - 1;
		if(!msdos_is_valid_drive(drv)) {
			// invalid drive
			CPU_AX = 0x0f;
			CPU_SET_C_FLAG(1);
			return;
		}
		break;
	}
	switch(CPU_AL) {
	case 0x00: // Get Device Information
		CPU_DX = file_handler[fd].info;
		break;
	case 0x01: // Set Device Information
		if(CPU_DH != 0) {
//			CPU_AX = 0x0d; // data invalid
//			CPU_SET_C_FLAG(1);
			file_handler[fd].info = CPU_DX;
		} else {
			file_handler[fd].info &= 0xff00;
			file_handler[fd].info |= CPU_DL;
		}
		break;
	case 0x02: // Read From Character Device Control Channel
		if(strstr(file_handler[fd].path, "EMMXXXX0") != NULL && support_ems) {
			// from DOSBox
			switch(*(UINT8 *)(mem + CPU_DS_BASE + CPU_DX)) {
			case 0x00:
				if(CPU_CX >= 6) {
					*(UINT16 *)(mem + CPU_DS_BASE + CPU_DX + 0) = 0x0023;
					*(UINT32 *)(mem + CPU_DS_BASE + CPU_DX + 2) = 0x0000;
					CPU_AX = 6; // number of bytes actually read
				} else {
					CPU_AX = 0x0d; // data invalid
					CPU_SET_C_FLAG(1);
				}
				break;
			case 0x01:
				if(CPU_CX >= 6) {
					*(UINT16 *)(mem + XMS_TOP + 0x18 + 0x000) = 0x0004;	// flags
					*(UINT16 *)(mem + XMS_TOP + 0x18 + 0x002) = 0x019d;	// size of this structure
					*(UINT16 *)(mem + XMS_TOP + 0x18 + 0x004) = 0x0001;	// version 1.0
					*(UINT32 *)(mem + XMS_TOP + 0x18 + 0x006) = 0x00000000;	// reserved
					for(int addr = 0, ofs = 0x00a; addr < 0x100000; addr += 0x4000, ofs += 6) {
						if(addr >= EMS_TOP && addr < EMS_TOP + EMS_SIZE) {
							int page = (addr - EMS_TOP) / 0x4000;
							*(UINT8  *)(mem + XMS_TOP + 0x18 + ofs + 0) = 0x03;	// frame type: EMS frame in 64k page
							*(UINT8  *)(mem + XMS_TOP + 0x18 + ofs + 1) = 0xff;	// owner: NONE
							*(UINT16 *)(mem + XMS_TOP + 0x18 + ofs + 2) = 0x7fff;	// no logical page number
							*(UINT8  *)(mem + XMS_TOP + 0x18 + ofs + 4) = page;	// physical EMS page number
							*(UINT8  *)(mem + XMS_TOP + 0x18 + ofs + 5) = 0x00;	// flags: EMS frame
						} else {
							*(UINT8  *)(mem + XMS_TOP + 0x18 + ofs + 0) = 0x00;	// frame type: NONE
							*(UINT8  *)(mem + XMS_TOP + 0x18 + ofs + 1) = 0xff;	// owner: NONE
							*(UINT16 *)(mem + XMS_TOP + 0x18 + ofs + 2) = 0xffff;	// non-EMS frame
							*(UINT8  *)(mem + XMS_TOP + 0x18 + ofs + 4) = 0xff;	// EMS page number (NONE)
							*(UINT8  *)(mem + XMS_TOP + 0x18 + ofs + 5) = 0xaa;	// flags: direct mapping
						}
					}
					*(UINT8  *)(mem + XMS_TOP + 0x18 + 0x18a) = 0x74;	// ??
					*(UINT8  *)(mem + XMS_TOP + 0x18 + 0x18b) = 0x00;	// no UMB descriptors following
					*(UINT8  *)(mem + XMS_TOP + 0x18 + 0x18c) = 0x01;	// 1 EMS handle info record
					*(UINT16 *)(mem + XMS_TOP + 0x18 + 0x17d) = 0x0000;	// system handle
					*(UINT32 *)(mem + XMS_TOP + 0x18 + 0x18f) = 0x00000000;	// handle name
					*(UINT32 *)(mem + XMS_TOP + 0x18 + 0x193) = 0x00000000;	// handle name
					*(UINT16 *)(mem + XMS_TOP + 0x18 + 0x197) = 0x0001;	// system handle
					*(UINT32 *)(mem + XMS_TOP + 0x18 + 0x199) = 0x00110000;	// physical address
					
					*(UINT16 *)(mem + CPU_DS_BASE + CPU_DX + 0) = 0x0018;
					*(UINT16 *)(mem + CPU_DS_BASE + CPU_DX + 2) = XMS_TOP >> 4;
					*(UINT16 *)(mem + CPU_DS_BASE + CPU_DX + 4) = 0x0001; // version 1.0
					CPU_AX = 6; // number of bytes actually read
				} else {
					CPU_AX = 0x0d; // data invalid
					CPU_SET_C_FLAG(1);
				}
				break;
			case 0x02:
				if(CPU_CX >= 2) {
					*(UINT8 *)(mem + CPU_DS_BASE + CPU_DX + 0) = 0x40; // EMS 4.0
					*(UINT8 *)(mem + CPU_DS_BASE + CPU_DX + 1) = 0x00;
					CPU_AX = 2; // number of bytes actually read
				} else {
					CPU_AX = 0x0d; // data invalid
					CPU_SET_C_FLAG(1);
				}
				break;
			case 0x03:
				if(CPU_CX >= 4) {
					*(UINT16 *)(mem + CPU_DS_BASE + CPU_DX + 0) = MAX_EMS_PAGES * 16; // max size (kb)
					*(UINT16 *)(mem + CPU_DS_BASE + CPU_DX + 2) = 0x0080; // min size (kb)
					CPU_AX = 4; // number of bytes actually read
				} else {
					CPU_AX = 0x0d; // data invalid
					CPU_SET_C_FLAG(1);
				}
				break;
			default:
				CPU_AX = 0x01; // function number invalid
				CPU_SET_C_FLAG(1);
			}
		} else if(strstr(file_handler[fd].path, "CONFIG$") != NULL) {
			if(CPU_CX >= 5) {
				memset(mem + CPU_DS_BASE + CPU_DX, 0, 5);
				CPU_AX = 5; // number of bytes actually read
			} else {
				CPU_AX = 0x0d; // data invalid
				CPU_SET_C_FLAG(1);
			}
		} else {
//			memset(mem + CPU_DS_BASE + CPU_DX, 0, CPU_CX);
//			CPU_AX = CPU_CX;
			CPU_AX = 0x05; // access denied
			CPU_SET_C_FLAG(1);
		}
		break;
	case 0x03: // Write To Character Device Control Channel
//		CPU_AX = 0x05;
//		CPU_SET_C_FLAG(1);
		CPU_AX = 0x00; // success
		break;
	case 0x04: // Read From Block Device Control Channel
	case 0x05: // Write To Block Device Control Channel
		CPU_AX = 0x05;
		CPU_SET_C_FLAG(1);
		break;
	case 0x06: // Get Input Status
		if(file_mode[file_handler[fd].mode].in) {
			if(file_handler[fd].atty) {
				CPU_AL = msdos_kbhit() ? 0xff : 0x00;
			} else {
				CPU_AL = eof(fd) ? 0x00 : 0xff;
			}
		} else {
			CPU_AL = 0x00;
		}
		break;
	case 0x07: // Get Output Status
		if(file_mode[file_handler[fd].mode].out) {
			CPU_AL = 0xff;
		} else {
			CPU_AL = 0x00;
		}
		break;
	case 0x08: // Check If Block Device Removable
		if(msdos_is_removable_drive(drv) || msdos_is_cdrom_drive(drv)) {
			// removable drive
			CPU_AX = 0x00;
		} else {
			// fixed drive
			CPU_AX = 0x01;
		}
		break;
	case 0x09: // Check If Block Device Remote
		if(msdos_is_remote_drive(drv)) {
			// remote drive
			CPU_DX = 0x1000;
		} else if(msdos_is_subst_drive(drv)) {
			// subst drive
			CPU_DX = 0x8000;
		} else {
			// local drive
			CPU_DX = 0x0000;
		}
		break;
	case 0x0a: // Check If Handle Is Remote
		if(!(file_handler[fd].info & 0x8000) && msdos_is_remote_drive(msdos_drive_number(file_handler[fd].path))) {
			CPU_DX = 0x8000;
		} else {
			CPU_DX = 0x0000;
		}
		break;
	case 0x0b: // Set Sharing Retry Count
		break;
	case 0x0c: // Generic Character Device Request
		if(CPU_CL == 0x45) {
			// set iteration (retry) count
			iteration_count = *(UINT8 *)(mem + CPU_DS_BASE + CPU_DX);
		} else if(CPU_CL == 0x4a) {
			// select code page
			active_code_page = *(UINT16 *)(mem + CPU_DS_BASE + CPU_DX + 2);
			msdos_nls_tables_update();
		} else if(CPU_CL == 0x4c) {
			// start code-page preparation
			int ids[3] = {437, 0, 0}; // 437: US English
			int count = 1, offset = 0;
			if(active_code_page != 437) {
				ids[count++] = active_code_page;
			}
			if(system_code_page != 437 && system_code_page != active_code_page) {
				ids[count++] = system_code_page;
			}
			*(UINT16 *)(mem + CPU_DS_BASE + CPU_DX + 2 * (offset++)) = 0;
			*(UINT16 *)(mem + CPU_DS_BASE + CPU_DX + 2 * (offset++)) = 2 + 2 * count;
			*(UINT16 *)(mem + CPU_DS_BASE + CPU_DX + 2 * (offset++)) = count;
			for(int i = 0; i < count; i++) {
				*(UINT16 *)(mem + CPU_DS_BASE + CPU_DX + 2 * (offset++)) = ids[i];
			}
		} else if(CPU_CL == 0x4d) {
			// end code-page preparation
			*(UINT16 *)(mem + CPU_DS_BASE + CPU_DX + 0) = 2;
			*(UINT16 *)(mem + CPU_DS_BASE + CPU_DX + 2) = active_code_page;
		} else if(CPU_CL == 0x5f) {
			// set display information
			if(*(UINT16 *)(mem + CPU_DS_BASE + CPU_DX + 0x02) >= 14) {
				int cur_width  = *(UINT16 *)(mem + 0x44a) + 0;
				int cur_height = *(UINT8  *)(mem + 0x484) + 1;
				int new_width  = *(UINT16 *)(mem + CPU_DS_BASE + CPU_DX + 0x0e);	// character columns
				int new_height = *(UINT16 *)(mem + CPU_DS_BASE + CPU_DX + 0x10);	// character rows
				
				if(cur_width != new_width || cur_height != new_height) {
					pcbios_set_console_size(new_width, new_height, true);
				}
			}
		} else if(CPU_CL == 0x65) {
			// get iteration (retry) count
			*(UINT8 *)(mem + CPU_DS_BASE + CPU_DX) = iteration_count;
		} else if(CPU_CL == 0x6a) {
			// query selected code page
			*(UINT16 *)(mem + CPU_DS_BASE + CPU_DX + 0) = 2; // FIXME
			*(UINT16 *)(mem + CPU_DS_BASE + CPU_DX + 2) = active_code_page;
			
			CPINFO info;
			GetCPInfo(active_code_page, &info);
			
			if(info.MaxCharSize != 1) {
				for(int i = 0;; i++) {
					UINT8 lo = info.LeadByte[2 * i + 0];
					UINT8 hi = info.LeadByte[2 * i + 1];
					
					*(UINT16 *)(mem + CPU_DS_BASE + CPU_DX + 4 + 4 * i + 0) = lo;
					*(UINT16 *)(mem + CPU_DS_BASE + CPU_DX + 4 + 4 * i + 2) = hi;
					*(UINT16 *)(mem + CPU_DS_BASE + CPU_DX + 0) = *(UINT16 *)(mem + CPU_DS_BASE + CPU_DX + 0) + 4;
					
					if(lo == 0 && hi == 0) {
						break;
					}
				}
			}
		} else if(CPU_CL == 0x6b) {
			// query prepare list
			int ids[3] = {437, 0, 0}; // 437: US English
			int count = 1, offset = 0;
			if(active_code_page != 437) {
				ids[count++] = active_code_page;
			}
			if(system_code_page != 437 && system_code_page != active_code_page) {
				ids[count++] = system_code_page;
			}
			*(UINT16 *)(mem + CPU_DS_BASE + CPU_DX + 2 * (offset++)) = 2 * (2 + 2 * count);
			*(UINT16 *)(mem + CPU_DS_BASE + CPU_DX + 2 * (offset++)) = count;
			for(int i = 0; i < count; i++) {
				*(UINT16 *)(mem + CPU_DS_BASE + CPU_DX + 2 * (offset++)) = ids[i];
			}
			*(UINT16 *)(mem + CPU_DS_BASE + CPU_DX + 2 * (offset++)) = count;
			for(int i = 0; i < count; i++) {
				*(UINT16 *)(mem + CPU_DS_BASE + CPU_DX + 2 * (offset++)) = ids[i];
			}
		} else if(CPU_CL == 0x7f) {
			// get display information
			*(UINT8  *)(mem + CPU_DS_BASE + CPU_DX + 0x00) = 0;	// level (0 for DOS 4.x-6.0)
			*(UINT8  *)(mem + CPU_DS_BASE + CPU_DX + 0x01) = 0;	// reserved (0)
			*(UINT16 *)(mem + CPU_DS_BASE + CPU_DX + 0x02) = 14;	// length of following data (14)
			*(UINT16 *)(mem + CPU_DS_BASE + CPU_DX + 0x04) = 1;	// bit 0 set for blink, clear for intensity
			*(UINT8  *)(mem + CPU_DS_BASE + CPU_DX + 0x06) = 1;	// mode type (1=text, 2=graphics)
			*(UINT8  *)(mem + CPU_DS_BASE + CPU_DX + 0x07) = 0;	// reserved (0)
			*(UINT16 *)(mem + CPU_DS_BASE + CPU_DX + 0x08) = 4;	// 4 bits per pixel
			*(UINT16 *)(mem + CPU_DS_BASE + CPU_DX + 0x0a) =  8 * (*(UINT16 *)(mem + 0x44a) + 0);	// pixel columns
			*(UINT16 *)(mem + CPU_DS_BASE + CPU_DX + 0x0c) = 16 * (*(UINT8  *)(mem + 0x484) + 1);	// pixel rows
			*(UINT16 *)(mem + CPU_DS_BASE + CPU_DX + 0x0e) = *(UINT16 *)(mem + 0x44a) + 0;		// character columns
			*(UINT16 *)(mem + CPU_DS_BASE + CPU_DX + 0x10) = *(UINT8  *)(mem + 0x484) + 1;		// character rows
		} else {
			unimplemented_21h("int %02Xh (AX=%04X BX=%04X CX=%04X DX=%04X SI=%04X DI=%04X DS=%04X ES=%04X)\n", 0x21, CPU_AX, CPU_BX, CPU_CX, CPU_DX, CPU_SI, CPU_DI, CPU_DS, CPU_ES);
			CPU_AX = 0x01; // invalid function
			CPU_SET_C_FLAG(1);
		}
		break;
	case 0x0d: // Generic Block Device Request
		if(CPU_CL == 0x40) {
			// set device parameters
//		} else if(CPU_CL == 0x41) {
//			// write logical device track
//		} else if(CPU_CL == 0x42) {
//			// format and verify logical device track
		} else if(CPU_CL == 0x46) {
			// set volume serial number
		} else if(CPU_CL == 0x47) {
			// set access flag
//		} else if(CPU_CL == 0x48) {
//			// set media lock state
//		} else if(CPU_CL == 0x49) {
//			// eject media in drive
		} else if(CPU_CL == 0x4a) {
			// lock logical volume
		} else if(CPU_CL == 0x4b) {
			// lock physical volume
		} else if(CPU_CL == 0x60) {
			// get device parameters
			int drive_num = (CPU_BL ? CPU_BL : _getdrive()) - 1;
			
			if(pcbios_update_drive_param(drive_num, 1)) {
				drive_param_t *drive_param = &drive_params[drive_num];
				DISK_GEOMETRY *geo = &drive_param->geometry;
				
				*(UINT8 *)(mem + CPU_DS_BASE + CPU_SI + 0x00) = 0x07; // ???
				switch(geo->MediaType) {
				case F5_360_512:
				case F5_320_512:
				case F5_320_1024:
				case F5_180_512:
				case F5_160_512:
					*(UINT8 *)(mem + CPU_DS_BASE + CPU_SI + 0x01) = 0x00; // 320K/360K disk
					break;
				case F5_1Pt2_512:
				case F3_1Pt2_512:
				case F3_1Pt23_1024:
				case F5_1Pt23_1024:
					*(UINT8 *)(mem + CPU_DS_BASE + CPU_SI + 0x01) = 0x01; // 1.2M disk
					break;
				case F3_720_512:
				case F3_640_512:
				case F5_640_512:
				case F5_720_512:
					*(UINT8 *)(mem + CPU_DS_BASE + CPU_SI + 0x01) = 0x02; // 720K disk
					break;
				case F8_256_128:
					*(UINT8 *)(mem + CPU_DS_BASE + CPU_SI + 0x01) = 0x03; // single-density 8-inch disk
					break;
				case FixedMedia:
					*(UINT8 *)(mem + CPU_DS_BASE + CPU_SI + 0x01) = 0x05; // fixed disk
					break;
				case F3_1Pt44_512:
					*(UINT8 *)(mem + CPU_DS_BASE + CPU_SI + 0x01) = 0x07; // (DOS 3.3+) other type of block device, normally 1.44M floppy
					break;
				case F3_2Pt88_512:
					*(UINT8 *)(mem + CPU_DS_BASE + CPU_SI + 0x01) = 0x09; // (DOS 5+) 2.88M floppy
					break;
				default:
					*(UINT8 *)(mem + CPU_DS_BASE + CPU_SI + 0x01) = 0x05; // fixed disk
//					*(UINT8 *)(mem + CPU_DS_BASE + CPU_SI + 0x01) = 0x05; // (DOS 3.3+) other type of block device, normally 1.44M floppy
					break;
				}
				*(UINT16 *)(mem + CPU_DS_BASE + CPU_SI + 0x02) = (geo->MediaType == FixedMedia) ? 0x01 : 0x00;
				*(UINT16 *)(mem + CPU_DS_BASE + CPU_SI + 0x04) = (geo->Cylinders.QuadPart > 0xffff) ? 0xffff : geo->Cylinders.QuadPart;
				switch(geo->MediaType) {
				case F5_360_512:
				case F5_320_512:
				case F5_320_1024:
				case F5_180_512:
				case F5_160_512:
					*(UINT8 *)(mem + CPU_DS_BASE + CPU_SI + 0x06) = 0x01; // 320K/360K disk
					break;
				default:
					*(UINT8 *)(mem + CPU_DS_BASE + CPU_SI + 0x06) = 0x00; // other drive types
					break;
				}
				*(UINT16 *)(mem + CPU_DS_BASE + CPU_SI + 0x07 + 0x00) = geo->BytesPerSector;
				*(UINT8  *)(mem + CPU_DS_BASE + CPU_SI + 0x07 + 0x02) = geo->SectorsPerTrack * geo->TracksPerCylinder;
				*(UINT16 *)(mem + CPU_DS_BASE + CPU_SI + 0x07 + 0x03) = 0;
				*(UINT8  *)(mem + CPU_DS_BASE + CPU_SI + 0x07 + 0x05) = 0;
				*(UINT16 *)(mem + CPU_DS_BASE + CPU_SI + 0x07 + 0x06) = 0;
				*(UINT16 *)(mem + CPU_DS_BASE + CPU_SI + 0x07 + 0x08) = 0;
				switch(geo->MediaType) {
				case F5_320_512:	// floppy, double-sided, 8 sectors per track (320K)
					*(UINT8 *)(mem + CPU_DS_BASE + CPU_SI + 0x07 + 0x0a) = 0xff;
					break;
				case F5_160_512:	// floppy, single-sided, 8 sectors per track (160K)
					*(UINT8 *)(mem + CPU_DS_BASE + CPU_SI + 0x07 + 0x0a) = 0xfe;
					break;
				case F5_360_512:	// floppy, double-sided, 9 sectors per track (360K)
					*(UINT8 *)(mem + CPU_DS_BASE + CPU_SI + 0x07 + 0x0a) = 0xfd;
					break;
				case F5_180_512:	// floppy, single-sided, 9 sectors per track (180K)
					*(UINT8 *)(mem + CPU_DS_BASE + CPU_SI + 0x07 + 0x0a) = 0xfc;
					break;
				case F5_1Pt2_512:	// floppy, double-sided, 15 sectors per track (1.2M)
				case F3_1Pt2_512:
				case F3_720_512:	// floppy, double-sided, 9 sectors per track (720K,3.5")
				case F5_720_512:
					*(UINT8 *)(mem + CPU_DS_BASE + CPU_SI + 0x07 + 0x0a) = 0xf9;
					break;
				case FixedMedia:	// hard disk
				case RemovableMedia:
				case Unknown:
					*(UINT8 *)(mem + CPU_DS_BASE + CPU_SI + 0x07 + 0x0a) = 0xf8;
					break;
				default:
					*(UINT8 *)(mem + CPU_DS_BASE + CPU_SI + 0x07 + 0x0a) = 0xf0;
					break;
				}
				*(UINT16 *)(mem + CPU_DS_BASE + CPU_SI + 0x07 + 0x0d) = geo->SectorsPerTrack;
				*(UINT16 *)(mem + CPU_DS_BASE + CPU_SI + 0x07 + 0x0f) = 1;
				*(UINT32 *)(mem + CPU_DS_BASE + CPU_SI + 0x07 + 0x11) = 0;
				*(UINT32 *)(mem + CPU_DS_BASE + CPU_SI + 0x07 + 0x15) = geo->TracksPerCylinder * geo->Cylinders.QuadPart;
				*(UINT16 *)(mem + CPU_DS_BASE + CPU_SI + 0x07 + 0x1f) = geo->Cylinders.QuadPart;
				// 21h	BYTE	device type
				// 22h	WORD	device attributes (removable or not, etc)
			} else {
				CPU_AX = 0x0f; // invalid drive
				CPU_SET_C_FLAG(1);
			}
//		} else if(CPU_CL == 0x61) {
//			// read logical device track
//		} else if(CPU_CL == 0x62) {
//			// verify logical device track
		} else if(CPU_CL == 0x66) {
			// get volume serial number
			char path[] = "A:\\";
			char volume_label[MAX_PATH];
			DWORD serial_number = 0;
			char file_system[MAX_PATH];
			
			path[0] = 'A' + (CPU_BL ? CPU_BL : _getdrive()) - 1;
			
			if(GetVolumeInformationA(path, volume_label, MAX_PATH, &serial_number, NULL, NULL, file_system, MAX_PATH)) {
				*(UINT16 *)(mem + CPU_DS_BASE + CPU_SI + 0x00) = 0;
				*(UINT32 *)(mem + CPU_DS_BASE + CPU_SI + 0x02) = serial_number;
				memset(mem + CPU_DS_BASE + CPU_SI + 0x06, 0x20, 11);
				memcpy(mem + CPU_DS_BASE + CPU_SI + 0x06, volume_label, min(strlen(volume_label), 11));
				memset(mem + CPU_DS_BASE + CPU_SI + 0x11, 0x20,  8);
				memcpy(mem + CPU_DS_BASE + CPU_SI + 0x11, file_system, min(strlen(file_system), 8));
			} else {
				CPU_AX = 0x0f; // invalid drive
				CPU_SET_C_FLAG(1);
			}
		} else if(CPU_CL == 0x67) {
			// get access flag
			*(UINT8 *)(mem + CPU_DS_BASE + CPU_SI + 0x00) = 0;
			*(UINT8 *)(mem + CPU_DS_BASE + CPU_SI + 0x01) = 1;
		} else if(CPU_CL == 0x68) {
			// sense media type
			int drive_num = (CPU_BL ? CPU_BL : _getdrive()) - 1;
			
			if(pcbios_update_drive_param(drive_num, 1)) {
				drive_param_t *drive_param = &drive_params[drive_num];
				DISK_GEOMETRY *geo = &drive_param->geometry;
				
				switch(geo->MediaType) {
				case F3_720_512:
				case F5_720_512:
					*(UINT8 *)(mem + CPU_DS_BASE + CPU_SI + 0x00) = 0x01;
					*(UINT8 *)(mem + CPU_DS_BASE + CPU_SI + 0x01) = 0x02;
					break;
				case F3_1Pt44_512:
					*(UINT8 *)(mem + CPU_DS_BASE + CPU_SI + 0x00) = 0x01;
					*(UINT8 *)(mem + CPU_DS_BASE + CPU_SI + 0x01) = 0x07;
					break;
				case F3_2Pt88_512:
					*(UINT8 *)(mem + CPU_DS_BASE + CPU_SI + 0x00) = 0x01;
					*(UINT8 *)(mem + CPU_DS_BASE + CPU_SI + 0x01) = 0x09;
					break;
				default:
					*(UINT8 *)(mem + CPU_DS_BASE + CPU_SI + 0x00) = 0x00;
					*(UINT8 *)(mem + CPU_DS_BASE + CPU_SI + 0x01) = 0x00; // ???
					break;
				}
			} else {
				CPU_AX = 0x0f; // invalid drive
				CPU_SET_C_FLAG(1);
			}
		} else if(CPU_CL == 0x6a) {
			// unlock logical volume
		} else if(CPU_CL == 0x6b) {
			// unlock physical volume
//		} else if(CPU_CL == 0x6c) {
//			// get lock flag
//		} else if(CPU_CL == 0x6d) {
//			// enumerate open files
//		} else if(CPU_CL == 0x6e) {
//			// find swap file
//		} else if(CPU_CL == 0x6f) {
//			// get drive map information
//		} else if(CPU_CL == 0x70) {
//			// get current lock state
//		} else if(CPU_CL == 0x71) {
//			// get first cluster
		} else {
			unimplemented_21h("int %02Xh (AX=%04X BX=%04X CX=%04X DX=%04X SI=%04X DI=%04X DS=%04X ES=%04X)\n", 0x21, CPU_AX, CPU_BX, CPU_CX, CPU_DX, CPU_SI, CPU_DI, CPU_DS, CPU_ES);
			CPU_AX = 0x01; // invalid function
			CPU_SET_C_FLAG(1);
		}
		break;
	case 0x0e: // Get Lgical Drive Map
		if(!msdos_is_valid_drive((CPU_BL ? CPU_BL : _getdrive()) - 1)) {
			CPU_AX = 0x0f; // invalid drive
			CPU_SET_C_FLAG(1);
		} else {
			CPU_AL = 0;
		}
		break;
	case 0x0f: // Set Logical Drive Map
		if(!msdos_is_valid_drive((CPU_BL ? CPU_BL : _getdrive()) - 1)) {
			CPU_AX = 0x0f; // invalid drive
			CPU_SET_C_FLAG(1);
		}
		break;
	case 0x10: // Query Generic IOCTRL Capability (Handle)
		switch(CPU_CL) {
		case 0x45:
		case 0x4a:
		case 0x4c:
		case 0x4d:
		case 0x65:
		case 0x6a:
		case 0x6b:
		case 0x7f:
			CPU_AX = 0x0000; // supported
			break;
		default:
			CPU_AL = 0x01; // ioctl capability not available
			CPU_SET_C_FLAG(1);
			break;
		}
		break;
	case 0x11: // Query Generic IOCTRL Capability (Drive)
		switch(CPU_CL) {
		case 0x40:
		case 0x46:
		case 0x4a:
		case 0x4b:
		case 0x60:
		case 0x66:
		case 0x67:
		case 0x68:
		case 0x6a:
		case 0x6b:
			if(CPU_CH == 0x00 || CPU_CH == 0x01 || CPU_CH == 0x03 || CPU_CH == 0x05) {
				// CH = 00h	Unknown
				// CH = 01h	COMn:
				// CH = 03h	CON
				// CH = 05h	LPTn:
				CPU_AX = 0x0000; // supported
				break;
			}
		default:
			CPU_AL = 0x01; // ioctl capability not available
			CPU_SET_C_FLAG(1);
			break;
		}
		break;
	case 0x12: // DR DOS 5.0-6.0 - Determine DOS Type
	case 0x14: // DR DOS 5.0-6.0 - Set Global Password
	case 0x16: // DR DOS 5.0-6.0 - History Buffer, Share, And Hiload Control
	case 0x51: // Concurrent DOS v3.2+ - Installation Check
	case 0x52: // DR DOS 3.41+ - Determine DOS tTpe/Get DR DOS Version
	case 0x54: // DR DOS 3.41+ - Set Global Password
	case 0x56: // DR DOS 5.0+ - History Buffer Control
	case 0x57: // DR DOS 5.0-6.0 - Share/Hiload Control
	case 0x58: // DR DOS 5.0+ internal - Get Pointer To Internal Variable Table
	case 0x59: // DR Multiuser DOS 5.0 - API
		CPU_AX = 0x01; // this is not DR-DOS
		CPU_SET_C_FLAG(1);
		break;
	default:
		unimplemented_21h("int %02Xh (AX=%04X BX=%04X CX=%04X DX=%04X SI=%04X DI=%04X DS=%04X ES=%04X)\n", 0x21, CPU_AX, CPU_BX, CPU_CX, CPU_DX, CPU_SI, CPU_DI, CPU_DS, CPU_ES);
		CPU_AX = 0x01;
		CPU_SET_C_FLAG(1);
		break;
	}
}

inline void msdos_int_21h_45h()
{
	process_t *process = msdos_process_info_get(current_psp);
	int fd = msdos_psp_get_file_table(CPU_BX, current_psp);
	
	if(fd < process->max_files && file_handler[fd].valid) {
		int dup_fd = _dup(fd);
		if(dup_fd != -1) {
			CPU_AX = dup_fd;
			msdos_file_handler_dup(dup_fd, fd, current_psp);
//			msdos_psp_set_file_table(dup_fd, fd, current_psp);
			msdos_psp_set_file_table(dup_fd, dup_fd, current_psp);
		} else {
			CPU_AX = msdos_maperr(_doserrno);
			CPU_SET_C_FLAG(1);
		}
	} else {
		CPU_AX = 0x06;
		CPU_SET_C_FLAG(1);
	}
}

inline void msdos_int_21h_46h()
{
	process_t *process = msdos_process_info_get(current_psp);
	int fd = msdos_psp_get_file_table(CPU_BX, current_psp);
	int dup_fd = CPU_CX;
	int tmp_fd = msdos_psp_get_file_table(CPU_CX, current_psp);
	
	if(CPU_BX == CPU_CX) {
		CPU_AX = 0x06;
		CPU_SET_C_FLAG(1);
	} else if(fd < process->max_files && file_handler[fd].valid && dup_fd < process->max_files) {
		if(tmp_fd < process->max_files && file_handler[tmp_fd].valid) {
			if(!msdos_file_handler_close(tmp_fd)) {
				_close(tmp_fd);
			}
			msdos_psp_set_file_table(dup_fd, 0x0ff, current_psp);
		}
		if(_dup2(fd, dup_fd) != -1) {
			msdos_file_handler_dup(dup_fd, fd, current_psp);
//			msdos_psp_set_file_table(dup_fd, fd, current_psp);
			msdos_psp_set_file_table(dup_fd, dup_fd, current_psp);
		} else {
			CPU_AX = msdos_maperr(_doserrno);
			CPU_SET_C_FLAG(1);
		}
	} else {
		CPU_AX = 0x06;
		CPU_SET_C_FLAG(1);
	}
}

inline void msdos_int_21h_47h(int lfn)
{
	char path[MAX_PATH];
	
	if(_getdcwd(CPU_DL, path, MAX_PATH) != NULL) {
		if(!lfn) {
			strcpy(path, msdos_short_path(path));
		} else {
			GetLongPathNameA(path, path, MAX_PATH);
		}
		if(path[1] == ':') {
			// the returned path does not include a drive or the initial backslash
			strcpy((char *)(mem + CPU_DS_BASE + CPU_SI), path + 3);
		} else {
			strcpy((char *)(mem + CPU_DS_BASE + CPU_SI), path);
		}
	} else {
		CPU_AX = msdos_maperr(_doserrno);
		CPU_SET_C_FLAG(1);
	}
}

inline void msdos_int_21h_48h()
{
	int seg, umb_linked;
	
	if((malloc_strategy & 0xf0) == 0x00) {
		// unlink umb not to allocate memory in umb
		if((umb_linked = msdos_mem_get_umb_linked()) != 0) {
			msdos_mem_unlink_umb();
		}
		if((seg = msdos_mem_alloc(first_mcb, CPU_BX, 0)) != -1) {
			CPU_AX = seg;
		} else {
			CPU_AX = 0x08;
			CPU_BX = msdos_mem_get_free(first_mcb, 0);
			CPU_SET_C_FLAG(1);
		}
		if(umb_linked != 0) {
			msdos_mem_link_umb();
		}
	} else if((malloc_strategy & 0xf0) == 0x40) {
		if((seg = msdos_mem_alloc(UMB_TOP >> 4, CPU_BX, 0)) != -1) {
			CPU_AX = seg;
		} else {
			CPU_AX = 0x08;
			CPU_BX = msdos_mem_get_free(UMB_TOP >> 4, 0);
			CPU_SET_C_FLAG(1);
		}
	} else if((malloc_strategy & 0xf0) == 0x80) {
		if((seg = msdos_mem_alloc(UMB_TOP >> 4, CPU_BX, 0)) != -1) {
			CPU_AX = seg;
		} else if((seg = msdos_mem_alloc(first_mcb, CPU_BX, 0)) != -1) {
			CPU_AX = seg;
		} else {
			CPU_AX = 0x08;
			CPU_BX = max(msdos_mem_get_free(UMB_TOP >> 4, 0), msdos_mem_get_free(first_mcb, 0));
			CPU_SET_C_FLAG(1);
		}
	}
}

inline void msdos_int_21h_49h()
{
	int mcb_seg = CPU_ES - 1;
	mcb_t *mcb = (mcb_t *)(mem + (mcb_seg << 4));
	
	if(mcb->mz == 'M' || mcb->mz == 'Z') {
		msdos_mem_free(CPU_ES);
	} else {
		CPU_AX = 0x09; // illegal memory block address
		CPU_SET_C_FLAG(1);
	}
}

inline void msdos_int_21h_4ah()
{
	int mcb_seg = CPU_ES - 1;
	mcb_t *mcb = (mcb_t *)(mem + (mcb_seg << 4));
	int max_paragraphs;
	
	if(mcb->mz == 'M' || mcb->mz == 'Z') {
		if(!msdos_mem_realloc(CPU_ES, CPU_BX, &max_paragraphs)) {
			// from DOSBox
			CPU_AX = CPU_ES;
		} else {
			CPU_AX = 0x08;
			CPU_BX = max_paragraphs > 0x7fff && limit_max_memory ? 0x7fff : max_paragraphs;
			CPU_SET_C_FLAG(1);
		}
	} else {
		CPU_AX = 0x09; // illegal memory block address
		CPU_SET_C_FLAG(1);
	}
}

inline void msdos_int_21h_4bh()
{
	char *command = (char *)(mem + CPU_DS_BASE + CPU_DX);
	param_block_t *param = (param_block_t *)(mem + CPU_ES_BASE + CPU_BX);
	
	switch(CPU_AL) {
	case 0x00:
	case 0x01:
		if(msdos_process_exec(command, (param_block_t *)(mem + CPU_ES_BASE + CPU_BX), CPU_AL)) {
			CPU_AX = 0x02;
			CPU_SET_C_FLAG(1);
		}
		break;
	case 0x03:
		{
			int fd = _open(command, _O_RDONLY | _O_BINARY);
#ifdef ENABLE_DEBUG_OPEN_FILE
			fprintf(fp_debug_log, "int21h_4b03h\tfd=%d\tr\t%s\n", fd, command);
#endif
			if(fd == -1) {
				CPU_AX = 0x02;
				CPU_SET_C_FLAG(1);
				break;
			}
			int file_size = _read(fd, file_buffer, sizeof(file_buffer));
			_close(fd);
			
			UINT16 *overlay = (UINT16 *)(mem + CPU_ES_BASE + CPU_BX);
			
			// check exe header
			exe_header_t *header = (exe_header_t *)file_buffer;
			if(header->mz == 0x4d5a || header->mz == 0x5a4d) {
				int header_size = header->header_size * 16;
				int load_size = (header->pages & 0x7ff) * 512 - header_size;
				if(header_size + load_size < 512) {
					load_size = 512 - header_size;
				}
				load_size = min(load_size, file_size - header_size);
				// relocation
				int start_seg = overlay[1];
				for(int i = 0; i < header->relocations; i++) {
					int ofs = *(UINT16 *)(file_buffer + header->relocation_table + i * 4 + 0);
					int seg = *(UINT16 *)(file_buffer + header->relocation_table + i * 4 + 2);
					*(UINT16 *)(file_buffer + header_size + (seg << 4) + ofs) += start_seg;
				}
				memcpy(mem + (overlay[0] << 4), file_buffer + header_size, load_size);
			} else {
				memcpy(mem + (overlay[0] << 4), file_buffer, file_size);
			}
		}
		break;
	case 0x04:
		// Load And Execute In Background (European MS-DOS 4.0 only)
//	case 0x05:
//		// DOS 5+ - Set Execution State
	case 0x80:
		// DR DOS v3.41 - Run Already-Loaded Kernel File
	case 0xf0:
	case 0xf1:
		// DIET v1.10+
	case 0xfd:
	case 0xfe:
		// unknown function called in FreeCOM
		CPU_AX = 0x01;
		CPU_SET_C_FLAG(1);
		break;
	default:
		unimplemented_21h("int %02Xh (AX=%04X BX=%04X CX=%04X DX=%04X SI=%04X DI=%04X DS=%04X ES=%04X)\n", 0x21, CPU_AX, CPU_BX, CPU_CX, CPU_DX, CPU_SI, CPU_DI, CPU_DS, CPU_ES);
		CPU_AX = 0x01;
		CPU_SET_C_FLAG(1);
		break;
	}
}

inline void msdos_int_21h_4ch()
{
	msdos_process_terminate(current_psp, CPU_AL, 1);
}

inline void msdos_int_21h_4dh()
{
	CPU_AX = retval;
}

inline void msdos_int_21h_4eh()
{
	process_t *process = msdos_process_info_get(current_psp);
	UINT32 dta_laddr = (process->dta.w.h << 4) + process->dta.w.l;
	find_t *find = (find_t *)(mem + dta_laddr);
	const char *path = msdos_trimmed_path((char *)(mem + CPU_DS_BASE + CPU_DX));
	WIN32_FIND_DATAA fd;
	
	dtainfo_t *dtainfo = msdos_dta_info_get(current_psp, LFN_DTA_LADDR);
	find->find_magic = FIND_MAGIC;
	find->dta_index = dtainfo - dtalist;
	strcpy(process->volume_label, msdos_volume_label(path));
	dtainfo->allowable_mask = CPU_CL;
	// note: SO1 dir command sets 0x3f, but only directories and volue label are found if bit3 is set :-(
	if((dtainfo->allowable_mask & 0x3f) == 0x3f) {
		dtainfo->allowable_mask &= ~0x08;
	}
	bool label_only = (dtainfo->allowable_mask == 8);
	
	if((dtainfo->allowable_mask & 8) && !msdos_match_volume_label(path, msdos_short_volume_label(process->volume_label))) {
		dtainfo->allowable_mask &= ~8;
	}
	if(!label_only && (dtainfo->find_handle = FindFirstFileA(path, &fd)) != INVALID_HANDLE_VALUE) {
		while(!msdos_find_file_check_attribute(fd.dwFileAttributes, dtainfo->allowable_mask, 0) ||
		      !msdos_find_file_has_8dot3name(&fd)) {
			if(!FindNextFileA(dtainfo->find_handle, &fd)) {
				FindClose(dtainfo->find_handle);
				dtainfo->find_handle = INVALID_HANDLE_VALUE;
				break;
			}
		}
	}
	if(dtainfo->find_handle != INVALID_HANDLE_VALUE) {
		find->attrib = (UINT8)(fd.dwFileAttributes & 0x3f);
		msdos_find_file_conv_local_time(&fd);
		FileTimeToDosDateTime(&fd.ftLastWriteTime, &find->date, &find->time);
		find->size = fd.nFileSizeLow;
		strcpy(find->name, msdos_short_name(&fd));
		CPU_AX = 0;
	} else if(dtainfo->allowable_mask & 8) {
		find->attrib = 8;
		find->size = 0;
		strcpy(find->name, msdos_short_volume_label(process->volume_label));
		dtainfo->allowable_mask &= ~8;
		CPU_AX = 0;
	} else {
		CPU_AX = 0x12;	// NOTE: return 0x02 if file path is invalid
		CPU_SET_C_FLAG(1);
	}
}

inline void msdos_int_21h_4fh()
{
	process_t *process = msdos_process_info_get(current_psp);
	UINT32 dta_laddr = (process->dta.w.h << 4) + process->dta.w.l;
	find_t *find = (find_t *)(mem + dta_laddr);
	WIN32_FIND_DATAA fd;
	
	if(find->find_magic != FIND_MAGIC || find->dta_index >= MAX_DTAINFO) {
		CPU_AX = 0x12;
		CPU_SET_C_FLAG(1);
		return;
	}
	dtainfo_t *dtainfo = &dtalist[find->dta_index];
	if(dtainfo->find_handle != INVALID_HANDLE_VALUE) {
		if(FindNextFileA(dtainfo->find_handle, &fd)) {
			while(!msdos_find_file_check_attribute(fd.dwFileAttributes, dtainfo->allowable_mask, 0) ||
			      !msdos_find_file_has_8dot3name(&fd)) {
				if(!FindNextFileA(dtainfo->find_handle, &fd)) {
					FindClose(dtainfo->find_handle);
					dtainfo->find_handle = INVALID_HANDLE_VALUE;
					break;
				}
			}
		} else {
			FindClose(dtainfo->find_handle);
			dtainfo->find_handle = INVALID_HANDLE_VALUE;
		}
	}
	if(dtainfo->find_handle != INVALID_HANDLE_VALUE) {
		find->attrib = (UINT8)(fd.dwFileAttributes & 0x3f);
		msdos_find_file_conv_local_time(&fd);
		FileTimeToDosDateTime(&fd.ftLastWriteTime, &find->date, &find->time);
		find->size = fd.nFileSizeLow;
		strcpy(find->name, msdos_short_name(&fd));
		CPU_AX = 0;
	} else if(dtainfo->allowable_mask & 8) {
		find->attrib = 8;
		find->size = 0;
		strcpy(find->name, msdos_short_volume_label(process->volume_label));
		dtainfo->allowable_mask &= ~8;
		CPU_AX = 0;
	} else {
		CPU_AX = 0x12;
		CPU_SET_C_FLAG(1);
	}
}

inline void msdos_int_21h_50h()
{
	if(current_psp != CPU_BX) {
		process_t *process = msdos_process_info_get(current_psp);
		if(process != NULL) {
			process->psp = CPU_BX;
		}
		current_psp = CPU_BX;
		msdos_sda_update(current_psp);
	}
}

inline void msdos_int_21h_51h()
{
	CPU_BX = current_psp;
}

inline void msdos_int_21h_52h()
{
	CPU_LOAD_SREG(CPU_ES_INDEX, DOS_INFO_TOP >> 4);
	CPU_BX = offsetof(dos_info_t, first_dpb);
}

inline void msdos_int_21h_53h()
{
	dpb_t *dpb = (dpb_t *)(mem + CPU_ES_BASE + CPU_BP);
	bpb_t *bpb = (bpb_t *)(mem + CPU_DS_BASE + CPU_SI);
	
	dpb->bytes_per_sector = bpb->bytes_per_sector;
	dpb->highest_sector_num = bpb->sectors_per_track - 1;
	dpb->shift_count = 0;
	dpb->reserved_sectors = 0;
	dpb->fat_num = bpb->fat_num;
	dpb->root_entries = bpb->root_entries;
	dpb->first_data_sector = 0;
	if(bpb->sectors_per_cluster != 0) {
		dpb->highest_cluster_num = (UINT16)(((CPU_CX == 0x4558 && bpb->total_sectors == 0) ? bpb->ext_total_sectors : bpb->total_sectors) / bpb->sectors_per_cluster + 1);
	} else {
		dpb->highest_cluster_num = 0;
	}
	dpb->sectors_per_fat = (CPU_CX == 0x4558 && bpb->sectors_per_fat == 0) ? bpb->ext_sectors_per_fat : bpb->sectors_per_fat;
	dpb->first_dir_sector = 0;
	dpb->device_driver_header = 0;
	dpb->media_type = bpb->media_type;
	dpb->drive_accessed = 0;
	dpb->next_dpb_ofs = 0xffff;
	dpb->next_dpb_seg = 0xffff;
	dpb->first_free_cluster = 0;
	dpb->free_clusters = 0xffff;
}

inline void msdos_int_21h_54h()
{
	process_t *process = msdos_process_info_get(current_psp);
	
	CPU_AL = process->verify;
}

inline void msdos_int_21h_55h()
{
	psp_t *psp = (psp_t *)(mem + (CPU_DX << 4));
	
	memcpy(mem + (CPU_DX << 4), mem + (current_psp << 4), sizeof(psp_t));
	psp->int_22h.dw = *(UINT32 *)(mem + 4 * 0x22);
	psp->int_23h.dw = *(UINT32 *)(mem + 4 * 0x23);
	psp->int_24h.dw = *(UINT32 *)(mem + 4 * 0x24);
	psp->parent_psp = current_psp;
	
	process_t *process = msdos_process_info_get(current_psp);
	process = msdos_process_info_create(CPU_DX, process->module_path);
	current_psp = CPU_DX;
	msdos_sda_update(current_psp);
	
	//increment file ref count
	for(int i = 0; i < 20; i++) {
		int fd = msdos_psp_get_file_table(i, current_psp);
		if(fd < process->max_files) {
			file_handler[fd].valid++;
		}
	}
}

inline void msdos_int_21h_56h(int lfn)
{
	char src[MAX_PATH], dst[MAX_PATH];
	strcpy(src, msdos_trimmed_path((char *)(mem + CPU_DS_BASE + CPU_DX), lfn));
	strcpy(dst, msdos_trimmed_path((char *)(mem + CPU_ES_BASE + CPU_DI), lfn));
	
	if(msdos_is_existing_file(dst) || msdos_is_existing_dir(dst)) {
		CPU_AX = 0x05; // access denied
		CPU_SET_C_FLAG(1);
	} else if(rename(src, dst)) {
		CPU_AX = msdos_maperr(_doserrno);
		CPU_SET_C_FLAG(1);
	}
}

inline void msdos_int_21h_57h()
{
	FILETIME time, local;
	FILETIME *ctime, *atime, *mtime;
	HANDLE hHandle;
	
	if((hHandle = (HANDLE)_get_osfhandle(CPU_BX)) == INVALID_HANDLE_VALUE) {
		CPU_AX = msdos_maperr(GetLastError());
		CPU_SET_C_FLAG(1);
		return;
	}
	ctime = atime = mtime = NULL;
	
	switch(CPU_AL) {
	case 0x00:
	case 0x01:
		mtime = &time;
		break;
//	case 0x02: // DOS 4.x only - Get Extended Attributes For File
//	case 0x03: // DOS 4.x only - Get Extended Attribute Properties
//		break;
	case 0x04:
	case 0x05:
		atime = &time;
		break;
	case 0x06:
	case 0x07:
		ctime = &time;
		break;
	default:
		unimplemented_21h("int %02Xh (AX=%04X BX=%04X CX=%04X DX=%04X SI=%04X DI=%04X DS=%04X ES=%04X)\n", 0x21, CPU_AX, CPU_BX, CPU_CX, CPU_DX, CPU_SI, CPU_DI, CPU_DS, CPU_ES);
		CPU_AX = 0x01;
		CPU_SET_C_FLAG(1);
		return;
	}
	if(CPU_AL & 1) {
		DosDateTimeToFileTime(CPU_DX, CPU_CX, &local);
		LocalFileTimeToFileTime(&local, &time);
		if(!SetFileTime(hHandle, ctime, atime, mtime)) {
			CPU_AX = msdos_maperr(GetLastError());
			CPU_SET_C_FLAG(1);
		}
	} else {
		if(!GetFileTime(hHandle, ctime, atime, mtime)) {
			// assume a device and use the current time
			GetSystemTimeAsFileTime(&time);
		}
		FileTimeToLocalFileTime(&time, &local);
		FileTimeToDosDateTime(&local, &CPU_DX, &CPU_CX);
	}
}

inline void msdos_int_21h_58h()
{
	switch(CPU_AL) {
	case 0x00:
		CPU_AX = malloc_strategy;
		break;
	case 0x01:
//		switch(CPU_BX) {
		switch(CPU_BL) {
		case 0x0000:
		case 0x0001:
		case 0x0002:
		case 0x0040:
		case 0x0041:
		case 0x0042:
		case 0x0080:
		case 0x0081:
		case 0x0082:
			malloc_strategy = CPU_BX;
			msdos_sda_update(current_psp);
			break;
		default:
			unimplemented_21h("int %02Xh (AX=%04X BX=%04X CX=%04X DX=%04X SI=%04X DI=%04X DS=%04X ES=%04X)\n", 0x21, CPU_AX, CPU_BX, CPU_CX, CPU_DX, CPU_SI, CPU_DI, CPU_DS, CPU_ES);
			CPU_AX = 0x01;
			CPU_SET_C_FLAG(1);
			break;
		}
		break;
	case 0x02:
		CPU_AL = msdos_mem_get_umb_linked() ? 1 : 0;
		break;
	case 0x03:
//		switch(CPU_BX) {
		switch(CPU_BL) {
		case 0x0000:
			msdos_mem_unlink_umb();
			break;
		case 0x0001:
			msdos_mem_link_umb();
			break;
		default:
			unimplemented_21h("int %02Xh (AX=%04X BX=%04X CX=%04X DX=%04X SI=%04X DI=%04X DS=%04X ES=%04X)\n", 0x21, CPU_AX, CPU_BX, CPU_CX, CPU_DX, CPU_SI, CPU_DI, CPU_DS, CPU_ES);
			CPU_AX = 0x01;
			CPU_SET_C_FLAG(1);
			break;
		}
		break;
	default:
		unimplemented_21h("int %02Xh (AX=%04X BX=%04X CX=%04X DX=%04X SI=%04X DI=%04X DS=%04X ES=%04X)\n", 0x21, CPU_AX, CPU_BX, CPU_CX, CPU_DX, CPU_SI, CPU_DI, CPU_DS, CPU_ES);
		CPU_AX = 0x01;
		CPU_SET_C_FLAG(1);
		break;
	}
}

inline void msdos_int_21h_59h()
{
	if(CPU_BX == 0x0000) {
		sda_t *sda = (sda_t *)(mem + SDA_TOP);
		
		CPU_AX = sda->extended_error_code;
		CPU_BH  = sda->error_class;
		CPU_BL  = sda->suggested_action;
		CPU_CH  = sda->locus_of_last_error;
		// XXX: GW-BASIC 3.23 invites the registers contents of CL, DX, SI, DI, DS, and ES are not destroyed
		if(sda->int21h_5d0ah_called != 0) {
			CPU_CL  = sda->int21h_5d0ah_cl;
			CPU_DX = sda->int21h_5d0ah_dx;
//			CPU_SI = sda->int21h_5d0ah_si;
			CPU_DI = sda->last_error_pointer.w.l;
//			CPU_LOAD_SREG(CPU_DS_INDEX, sda->int21h_5d0ah_ds);
			CPU_LOAD_SREG(CPU_ES_INDEX, sda->last_error_pointer.w.h);
		}
		sda->int21h_5d0ah_called = 0;
//	} else if(CPU_BX == 0x0001) {
//		// European MS-DOS 4.0 - Get Hard Error Information
	} else {
		unimplemented_21h("int %02Xh (AX=%04X BX=%04X CX=%04X DX=%04X SI=%04X DI=%04X DS=%04X ES=%04X)\n", 0x21, CPU_AX, CPU_BX, CPU_CX, CPU_DX, CPU_SI, CPU_DI, CPU_DS, CPU_ES);
		CPU_AX = 0x01;
		CPU_SET_C_FLAG(1);
	}
}

inline void msdos_int_21h_5ah()
{
	char *path = (char *)(mem + CPU_DS_BASE + CPU_DX);
	int len = strlen(path);
	char tmp[MAX_PATH];
	
	if(GetTempFileNameA(path, "TMP", 0, tmp)) {
		int fd = _open(tmp, _O_RDWR | _O_BINARY | _O_CREAT | _O_TRUNC, _S_IREAD | _S_IWRITE);
#ifdef ENABLE_DEBUG_OPEN_FILE
		fprintf(fp_debug_log, "int21h_5ah\tfd=%d\trw\t%s\n", fd, tmp);
#endif
		
		SetFileAttributesA(tmp, msdos_file_attribute_create(CPU_CX) & ~FILE_ATTRIBUTE_READONLY);
		CPU_AX = fd;
		msdos_file_handler_open(fd, path, _isatty(fd), 2, msdos_drive_number(path), current_psp);
		msdos_psp_set_file_table(fd, fd, current_psp);
		
		strcpy(path, tmp);
		int dx = CPU_DX + len;
		int ds = CPU_DS;
		while(dx > 0xffff) {
			dx -= 0x10;
			ds++;
		}
		CPU_DX = dx;
		CPU_LOAD_SREG(CPU_DS_INDEX, ds);
	} else {
		CPU_AX = msdos_maperr(GetLastError());
		CPU_SET_C_FLAG(1);
	}
}

inline void msdos_int_21h_5bh()
{
	const char *path = msdos_local_file_path((char *)(mem + CPU_DS_BASE + CPU_DX), 0);
	
	if(msdos_is_device_path(path) || msdos_is_existing_file(path)) {
		// already exists
#ifdef ENABLE_DEBUG_OPEN_FILE
		fprintf(fp_debug_log, "int21h_5bh\tfd=%d\trw\t%s\n", -1, path);
#endif
		CPU_AX = 0x50;
		CPU_SET_C_FLAG(1);
	} else {
		int fd = _open(path, _O_RDWR | _O_BINARY | _O_CREAT | _O_TRUNC, _S_IREAD | _S_IWRITE);
#ifdef ENABLE_DEBUG_OPEN_FILE
		fprintf(fp_debug_log, "int21h_5bh\tfd=%d\trw\t%s\n", fd, path);
#endif
		
		if(fd != -1) {
			SetFileAttributesA(path, msdos_file_attribute_create(CPU_CX) & ~FILE_ATTRIBUTE_READONLY);
			CPU_AX = fd;
			msdos_file_handler_open(fd, path, _isatty(fd), 2, msdos_drive_number(path), current_psp);
			msdos_psp_set_file_table(fd, fd, current_psp);
		} else {
			CPU_AX = msdos_maperr(_doserrno);
			CPU_SET_C_FLAG(1);
		}
	}
}

inline void msdos_int_21h_5ch()
{
	process_t *process = msdos_process_info_get(current_psp);
	int fd = msdos_psp_get_file_table(CPU_BX, current_psp);
	
	if(fd < process->max_files && file_handler[fd].valid) {
		if(CPU_AL == 0 || CPU_AL == 1) {
			static const int modes[2] = {_LK_LOCK, _LK_UNLCK};
			UINT32 pos = _tell(fd);
			_lseek(fd, (CPU_CX << 16) | CPU_DX, SEEK_SET);
			if(_locking(fd, modes[CPU_AL], (CPU_SI << 16) | CPU_DI)) {
				CPU_AX = msdos_maperr(_doserrno);
				CPU_SET_C_FLAG(1);
			}
			_lseek(fd, pos, SEEK_SET);
			
			// some seconds may be passed in _locking()
			REQUEST_HARDWRE_UPDATE();
		} else {
			CPU_AX = 0x01;
			CPU_SET_C_FLAG(1);
		}
	} else {
		CPU_AX = 0x06;
		CPU_SET_C_FLAG(1);
	}
}

inline void msdos_int_21h_5dh()
{
	switch(CPU_AL) {
	case 0x00: // DOS 3.1+ internal - Server Function Call
		if(*(UINT16 *)(mem + CPU_DS_BASE + CPU_DX + 0x12) == 0x0000) {
			// current system
			static bool reenter = false;
			if(!reenter) {
				UINT32 offset = CPU_DS_BASE + CPU_DX;
				CPU_AX = *(UINT16 *)(mem + offset + 0x00);
				CPU_BX = *(UINT16 *)(mem + offset + 0x02);
				CPU_CX = *(UINT16 *)(mem + offset + 0x04);
				CPU_DX = *(UINT16 *)(mem + offset + 0x06);
				CPU_SI = *(UINT16 *)(mem + offset + 0x08);
				CPU_DI = *(UINT16 *)(mem + offset + 0x0a);
				CPU_LOAD_SREG(CPU_DS_INDEX, *(UINT16 *)(mem + offset + 0x0c));
				CPU_LOAD_SREG(CPU_ES_INDEX, *(UINT16 *)(mem + offset + 0x0e));
				reenter = true;
				try {
					msdos_syscall(0x21);
				} catch(...) {
				}
				reenter = false;
			}
		} else {
			CPU_AX = 0x49; //  network software not installed
			CPU_SET_C_FLAG(1);
		}
		break;
//	case 0x01: // DOS 3.1+ internal - Commit All Files For Specified Computer/Process
//	case 0x02: // DOS 3.1+ internal - SHARE.EXE - Close File By Name
//	case 0x03: // DOS 3.1+ internal - SHARE.EXE - Close ALL Files For Given Computer
//	case 0x04: // DOS 3.1+ internal - SHARE.EXE - Close ALL Files For Given Process
//	case 0x05: // DOS 3.1+ internal - SHARE.EXE - Get Open File List Entry
	case 0x06: // DOS 3.0+ internal - Get Address Of DOS Swappable Data Area
		CPU_LOAD_SREG(CPU_DS_INDEX, SDA_TOP >> 4);
		CPU_SI = offsetof(sda_t, crit_error_flag);
		CPU_CX = 0x80;
		CPU_DX = 0x1a;
		break;
	case 0x07: // DOS 3.1+ network - Get Redirected Printer Mode
	case 0x08: // DOS 3.1+ network - Set Redirected Printer Mode
	case 0x09: // DOS 3.1+ network - Flush Redirected Printer Output
		CPU_AX = 0x49; //  network software not installed
		CPU_SET_C_FLAG(1);
		break;
	case 0x0a: // DOS 3.1+ - Set Extended Error Information
		{
			sda_t *sda = (sda_t *)(mem + SDA_TOP);
			sda->int21h_5d0ah_called    = 1;
			sda->extended_error_code    = *(UINT16 *)(mem + CPU_DS_BASE + CPU_DX + 0x00); // AX
			// XXX: which one is correct ???
#if 1
			// Ralf Brown's Interrupt List and DR-DOS System and Programmer's Guide
			sda->suggested_action       = *(UINT8  *)(mem + CPU_DS_BASE + CPU_DX + 0x02); // BL
			sda->error_class            = *(UINT8  *)(mem + CPU_DS_BASE + CPU_DX + 0x03); // BH
			sda->int21h_5d0ah_cl        = *(UINT8  *)(mem + CPU_DS_BASE + CPU_DX + 0x04); // CL
			sda->locus_of_last_error    = *(UINT8  *)(mem + CPU_DS_BASE + CPU_DX + 0x05); // CH
#else
			// PC DOS 7 Technical Update
			sda->error_class            = *(UINT8  *)(mem + CPU_DS_BASE + CPU_DX + 0x02); // BH
			sda->suggested_action       = *(UINT8  *)(mem + CPU_DS_BASE + CPU_DX + 0x03); // BL
			sda->locus_of_last_error    = *(UINT8  *)(mem + CPU_DS_BASE + CPU_DX + 0x04); // CH
			sda->int21h_5d0ah_cl        = *(UINT8  *)(mem + CPU_DS_BASE + CPU_DX + 0x05); // CL
#endif
			sda->int21h_5d0ah_dx        = *(UINT16 *)(mem + CPU_DS_BASE + CPU_DX + 0x06); // DX
//			sda->int21h_5d0ah_si        = *(UINT16 *)(mem + CPU_DS_BASE + CPU_DX + 0x08); // SI
			sda->last_error_pointer.w.l = *(UINT16 *)(mem + CPU_DS_BASE + CPU_DX + 0x0a); // DI
//			sda->int21h_5d0ah_ds        = *(UINT16 *)(mem + CPU_DS_BASE + CPU_DX + 0x0c); // DS
			sda->last_error_pointer.w.h = *(UINT16 *)(mem + CPU_DS_BASE + CPU_DX + 0x0e); // ES
		}
		break;
	case 0x0b: // DOS 4.x only - internal - Get DOS Swappable Data Areas
		CPU_AX = 0x01;
		CPU_SET_C_FLAG(1);
		break;
	default:
		unimplemented_21h("int %02Xh (AX=%04X BX=%04X CX=%04X DX=%04X SI=%04X DI=%04X DS=%04X ES=%04X)\n", 0x21, CPU_AX, CPU_BX, CPU_CX, CPU_DX, CPU_SI, CPU_DI, CPU_DS, CPU_ES);
		CPU_AX = 0x01;
		CPU_SET_C_FLAG(1);
		break;
	}
}

inline void msdos_int_21h_5eh()
{
	switch(CPU_AL) {
	case 0x00: // DOS 3.1+ network - Get Machine Name
		{
			char name[256] = {0};
			DWORD dwSize = 256;
			
			if(GetComputerNameA(name, &dwSize)) {
				char *dest = (char *)(mem + CPU_DS_BASE + CPU_DX);
				for(int i = 0; i < 15; i++) {
					dest[i] = (i < strlen(name)) ? name[i] : ' ';
				}
				dest[15] = '\0';
				CPU_CH = 0x01; // nonzero valid
				CPU_CL = 0x01; // NetBIOS number for machine name ???
			} else {
				CPU_AX = 0x01;
				CPU_SET_C_FLAG(1);
			}
		}
		break;
//	case 0x01: // DOS 3.1+ network - Set Machine Name
//	case 0x02: // DOS 3.1+ network - Set Network Printer Setup String
//	case 0x03: // DOS 3.1+ network - Get Network Printer Setup String
//	case 0x04: // DOS 3.1+ network - Set Printer Mode
//	case 0x05: // DOS 3.1+ network - Get Printer Mode
	default:
//		unimplemented_21h("int %02Xh (AX=%04X BX=%04X CX=%04X DX=%04X SI=%04X DI=%04X DS=%04X ES=%04X)\n", 0x21, CPU_AX, CPU_BX, CPU_CX, CPU_DX, CPU_SI, CPU_DI, CPU_DS, CPU_ES);
//		CPU_AX = 0x01;
		CPU_AX = 0x49; //  network software not installed
		CPU_SET_C_FLAG(1);
		break;
	}
}

inline void msdos_int_21h_5fh()
{
	switch(CPU_AL) {
//	case 0x00: // DOS 3.1+ network - Get Redirection Mode
//	case 0x01: // DOS 3.1+ network - Set Redirection Mode
	case 0x05: // DOS 4.0+ network - Get Extended Redirection List Entry
		CPU_BP = 0;
		for(int i = 0; i < 26; i++) {
			if(msdos_is_remote_drive(i)) {
				CPU_BP++;
			}
		}
	case 0x02: // DOS 3.1+ network - Get Redirection List Entry
		for(int i = 0, index = 0; i < 26; i++) {
			if(msdos_is_remote_drive(i)) {
				if(index == CPU_BX) {
					char volume[] = "A:";
					volume[0] = 'A' + i;
					DWORD dwSize = 128;
					strcpy((char *)(mem + CPU_DS_BASE + CPU_SI), volume);
					WNetGetConnectionA(volume, (char *)(mem + CPU_ES_BASE + CPU_DI), &dwSize);
					CPU_BH = 0x00; // valid
					CPU_BL = 0x04; // disk drive
					CPU_CX = 0x00; // LANtastic
					return;
				}
				index++;
			}
		}
		CPU_AX = 0x12; // no more files
		CPU_SET_C_FLAG(1);
		break;
//	case 0x03: // DOS 3.1+ network - Redirect Device
//	case 0x04: // DOS 3.1+ network - Cancel Redirection
//	case 0x06: // Network - Get Full Redirection List
	case 0x07: // DOS 5+ - Enable Drive
		if(msdos_is_valid_drive(CPU_DL)) {
			msdos_cds_update(CPU_DL);
		} else {
			CPU_AX = 0x0f; // invalid drive
			CPU_SET_C_FLAG(1);
		}
		break;
	case 0x08: // DOS 5+ - Disable Drive
		if(msdos_is_valid_drive(CPU_DL)) {
			cds_t *cds = (cds_t *)(mem + CDS_TOP + 88 * CPU_DL);
			cds->drive_attrib = 0x0000;
		} else {
			CPU_AX = 0x0f; // invalid drive
			CPU_SET_C_FLAG(1);
		}
		break;
	default:
//		unimplemented_21h("int %02Xh (AX=%04X BX=%04X CX=%04X DX=%04X SI=%04X DI=%04X DS=%04X ES=%04X)\n", 0x21, CPU_AX, CPU_BX, CPU_CX, CPU_DX, CPU_SI, CPU_DI, CPU_DS, CPU_ES);
//		CPU_AX = 0x01;
		CPU_AX = 0x49; //  network software not installed
		CPU_SET_C_FLAG(1);
		break;
	}
}

inline void msdos_int_21h_60h(int lfn)
{
	char full[MAX_PATH];
	const char *path = NULL;
	
	if(lfn) {
		char *name;
		*full = '\0';
		GetFullPathNameA((char *)(mem + CPU_DS_BASE + CPU_SI), MAX_PATH, full, &name);
		switch(CPU_CL) {
		case 1:
			GetShortPathNameA(full, full, MAX_PATH);
			my_strupr(full);
			break;
		case 2:
			GetLongPathNameA(full, full, MAX_PATH);
			break;
		}
		path = full;
	} else {
		path = msdos_short_full_path((char *)(mem + CPU_DS_BASE + CPU_SI));
	}
	if(*path != '\0') {
		strcpy((char *)(mem + CPU_ES_BASE + CPU_DI), path);
	} else {
		CPU_AX = msdos_maperr(GetLastError());
		CPU_SET_C_FLAG(1);
	}
}

inline void msdos_int_21h_61h()
{
	CPU_AL = 0;
}

inline void msdos_int_21h_62h()
{
	CPU_BX = current_psp;
}

inline void msdos_int_21h_63h()
{
	switch(CPU_AL) {
	case 0x00:
		CPU_LOAD_SREG(CPU_DS_INDEX, DBCS_TABLE >> 4);
		CPU_SI = (DBCS_TABLE & 0x0f);
		CPU_AL = 0x00;
		break;
	case 0x01: // set korean input mode
	case 0x02: // get korean input mode
		CPU_AL = 0xff; // not supported
		break;
	default:
		unimplemented_21h("int %02Xh (AX=%04X BX=%04X CX=%04X DX=%04X SI=%04X DI=%04X DS=%04X ES=%04X)\n", 0x21, CPU_AX, CPU_BX, CPU_CX, CPU_DX, CPU_SI, CPU_DI, CPU_DS, CPU_ES);
		CPU_AX = 0x01;
		CPU_SET_C_FLAG(1);
		break;
	}
}

UINT16 get_extended_country_info(UINT8 func)
{
	switch(func) {
	case 0x01:
		if(CPU_CX >= 5) {
			UINT8 data[1 + 2 + 2 + 2 + sizeof(country_info_t)];
			if(CPU_CX > sizeof(data)) {		// cx = actual transfer size
				CPU_CX = sizeof(data);
			}
			memset(data, 0, sizeof(data));
			data[0] = 0x01;
			*(UINT16 *)(data + 1) = CPU_CX - 3;
			*(UINT16 *)(data + 3) = get_country_info((country_info_t*)(data + 7));
			*(UINT16 *)(data + 5) = active_code_page;
			memcpy(mem + CPU_ES_BASE + CPU_DI, data, CPU_CX);
//			CPU_AX = active_code_page;
		} else {
			return(0x08); // insufficient memory
		}
		break;
	case 0x02:
		*(UINT8  *)(mem + CPU_ES_BASE + CPU_DI + 0) = 0x02;
		*(UINT16 *)(mem + CPU_ES_BASE + CPU_DI + 1) = (UPPERTABLE_TOP & 0x0f);
		*(UINT16 *)(mem + CPU_ES_BASE + CPU_DI + 3) = (UPPERTABLE_TOP >> 4);
//		CPU_AX = active_code_page;
		CPU_CX = 0x05;
		break;
	case 0x03:
		*(UINT8  *)(mem + CPU_ES_BASE + CPU_DI + 0) = 0x02;
		*(UINT16 *)(mem + CPU_ES_BASE + CPU_DI + 1) = (LOWERTABLE_TOP & 0x0f);
		*(UINT16 *)(mem + CPU_ES_BASE + CPU_DI + 3) = (LOWERTABLE_TOP >> 4);
//		CPU_AX = active_code_page;
		CPU_CX = 0x05;
		break;
	case 0x04:
		*(UINT8  *)(mem + CPU_ES_BASE + CPU_DI + 0) = 0x04;
		*(UINT16 *)(mem + CPU_ES_BASE + CPU_DI + 1) = (FILENAME_UPPERTABLE_TOP & 0x0f);
		*(UINT16 *)(mem + CPU_ES_BASE + CPU_DI + 3) = (FILENAME_UPPERTABLE_TOP >> 4);
//		CPU_AX = active_code_page;
		CPU_CX = 0x05;
		break;
	case 0x05:
		*(UINT8  *)(mem + CPU_ES_BASE + CPU_DI + 0) = 0x05;
		*(UINT16 *)(mem + CPU_ES_BASE + CPU_DI + 1) = (FILENAME_TERMINATOR_TOP & 0x0f);
		*(UINT16 *)(mem + CPU_ES_BASE + CPU_DI + 3) = (FILENAME_TERMINATOR_TOP >> 4);
//		CPU_AX = active_code_page;
		CPU_CX = 0x05;
		break;
	case 0x06:
		*(UINT8  *)(mem + CPU_ES_BASE + CPU_DI + 0) = 0x06;
		*(UINT16 *)(mem + CPU_ES_BASE + CPU_DI + 1) = (COLLATING_TABLE_TOP & 0x0f);
		*(UINT16 *)(mem + CPU_ES_BASE + CPU_DI + 3) = (COLLATING_TABLE_TOP >> 4);
//		CPU_AX = active_code_page;
		CPU_CX = 0x05;
		break;
	case 0x07:
		*(UINT8  *)(mem + CPU_ES_BASE + CPU_DI + 0) = 0x07;
		*(UINT16 *)(mem + CPU_ES_BASE + CPU_DI + 1) = (DBCS_TOP & 0x0f);
		*(UINT16 *)(mem + CPU_ES_BASE + CPU_DI + 3) = (DBCS_TOP >> 4);
//		CPU_AX = active_code_page;
		CPU_CX = 0x05;
		break;
	default:
		return(0x01); // function number invalid
	}
	return(0x00);
}

inline void msdos_int_21h_65h()
{
	char tmp[0x10000];
	
	switch(CPU_AL) {
	case 0x00:
		if(CPU_CX >= 7) {
			set_country_info((country_info_t *)(mem + CPU_ES_BASE + CPU_DI), CPU_CX - 7);
			CPU_AX = system_code_page;
		} else {
			CPU_AX = 0x0c;
			CPU_SET_C_FLAG(1);
		}
		break;
	case 0x01:
	case 0x02:
	case 0x03:
	case 0x04:
	case 0x05:
	case 0x06:
	case 0x07:
		{
			UINT16 result = get_extended_country_info(CPU_AL);
			if(result) {
				CPU_AX = result;
				CPU_SET_C_FLAG(1);
			} else {
				CPU_AX = active_code_page; // FIXME: is this correct???
			}
		}
		break;
	case 0x20:
	case 0xa0:
		memset(tmp, 0, sizeof(tmp));
		tmp[0] = CPU_DL;
		my_strupr(tmp);
		CPU_DL = tmp[0];
		break;
	case 0x21:
	case 0xa1:
		memset(tmp, 0, sizeof(tmp));
		memcpy(tmp, mem + CPU_DS_BASE + CPU_DX, CPU_CX);
		my_strupr(tmp);
		memcpy(mem + CPU_DS_BASE + CPU_DX, tmp, CPU_CX);
		break;
	case 0x22:
	case 0xa2:
		my_strupr((char *)(mem + CPU_DS_BASE + CPU_DX));
		break;
	case 0x23:
		// FIXME: need to check multi-byte (kanji) charactre?
		if(CPU_DL == 'N' || CPU_DL == 'n' || (CPU_DL == 0x82 && CPU_DH == 0x6d) || (CPU_DL == 0x82 && CPU_DH == 0x8e)) {
			// 826dh/828eh: multi-byte (kanji) N and n
			CPU_AX = 0x00;
		} else if(CPU_DL == 'Y' || CPU_DL == 'y' || (CPU_DL == 0x82 && CPU_DH == 0x78) || (CPU_DL == 0x82 && CPU_DH == 0x99)) {
			// 8278h/8299h: multi-byte (kanji) Y and y
			CPU_AX = 0x01;
		} else {
			CPU_AX = 0x02;
		}
		break;
	default:
		unimplemented_21h("int %02Xh (AX=%04X BX=%04X CX=%04X DX=%04X SI=%04X DI=%04X DS=%04X ES=%04X)\n", 0x21, CPU_AX, CPU_BX, CPU_CX, CPU_DX, CPU_SI, CPU_DI, CPU_DS, CPU_ES);
		CPU_AX = 0x01;
		CPU_SET_C_FLAG(1);
		break;
	}
}

inline void msdos_int_21h_66h()
{
	switch(CPU_AL) {
	case 0x01:
		CPU_BX = active_code_page;
		CPU_DX = system_code_page;
		break;
	case 0x02:
		if(active_code_page == CPU_BX) {
			CPU_AX = 0xeb41;
		} else if(set_multibyte_code_page(CPU_BX) == 0) {
			active_code_page = CPU_BX;
			msdos_nls_tables_update();
			CPU_AX = 0xeb41;
			set_input_code_page(active_code_page);
			set_output_code_page(active_code_page);
		} else {
			CPU_AX = 0x25;
			CPU_SET_C_FLAG(1);
		}
		break;
	default:
		unimplemented_21h("int %02Xh (AX=%04X BX=%04X CX=%04X DX=%04X SI=%04X DI=%04X DS=%04X ES=%04X)\n", 0x21, CPU_AX, CPU_BX, CPU_CX, CPU_DX, CPU_SI, CPU_DI, CPU_DS, CPU_ES);
		CPU_AX = 0x01;
		CPU_SET_C_FLAG(1);
		break;
	}
}

inline void msdos_int_21h_67h()
{
	process_t *process = msdos_process_info_get(current_psp);
	
	if(CPU_BX <= MAX_FILES) {
		process->max_files = max(CPU_BX, 20);
	} else {
		CPU_AX = 0x08;
		CPU_SET_C_FLAG(1);
	}
}

inline void msdos_int_21h_68h()
{
	process_t *process = msdos_process_info_get(current_psp);
	int fd = msdos_psp_get_file_table(CPU_BX, current_psp);
	
	if(fd < process->max_files && file_handler[fd].valid) {
		// fflush(_fdopen(fd, ""));
	} else {
		CPU_AX = 0x06;
		CPU_SET_C_FLAG(1);
	}
}

inline void msdos_int_21h_69h()
{
	drive_info_t *info = (drive_info_t *)(mem + CPU_DS_BASE + CPU_DX);
	char path[] = "A:\\";
	char volume_label[MAX_PATH];
	DWORD serial_number = 0;
	char file_system[MAX_PATH];
	
	if(CPU_BL == 0) {
		path[0] = 'A' + _getdrive() - 1;
	} else {
		path[0] = 'A' + CPU_BL - 1;
	}
	
	switch(CPU_AL) {
	case 0x00:
		if(GetVolumeInformationA(path, volume_label, MAX_PATH, &serial_number, NULL, NULL, file_system, MAX_PATH)) {
			info->info_level = 0;
			info->serial_number = serial_number;
			memset(info->volume_label, 0x20, 11);
			memcpy(info->volume_label, volume_label, min(strlen(volume_label), 11));
			memset(info->file_system, 0x20, 8);
			memcpy(info->file_system, file_system, min(strlen(file_system), 8));
		} else {
			CPU_AX = msdos_maperr(GetLastError());
			CPU_SET_C_FLAG(1);
		}
		break;
	case 0x01:
		CPU_AX = 0x03;
		CPU_SET_C_FLAG(1);
		break;
	default:
		unimplemented_21h("int %02Xh (AX=%04X BX=%04X CX=%04X DX=%04X SI=%04X DI=%04X DS=%04X ES=%04X)\n", 0x21, CPU_AX, CPU_BX, CPU_CX, CPU_DX, CPU_SI, CPU_DI, CPU_DS, CPU_ES);
		CPU_AX = 0x01;
		CPU_SET_C_FLAG(1);
		break;
	}
}

inline void msdos_int_21h_6ah()
{
	CPU_AH = 0x68;
	msdos_int_21h_68h();
}

inline void msdos_int_21h_6bh()
{
	CPU_AL = 0x00;
}

inline void msdos_int_21h_6ch(int lfn)
{
	const char *path = msdos_local_file_path((char *)(mem + CPU_DS_BASE + CPU_SI), lfn);
	int mode = CPU_BL & 0x03;
	
	if(mode < 0x03) {
		if(msdos_is_device_path(path) || msdos_is_existing_file(path)) {
			// file exists
			if(CPU_DL & 1) {
				int fd = -1;
				int sio_port = 0;
				int lpt_port = 0;
				
				if(msdos_is_device_path(path)) {
					fd = msdos_open_device(path, file_mode[mode].mode, &sio_port, &lpt_port);
				} else {
					fd = msdos_open(path, file_mode[mode].mode);
				}
#ifdef ENABLE_DEBUG_OPEN_FILE
				fprintf(fp_debug_log, "int21h_6ch\tfd=%d\t%s\t%s\n", fd, file_mode[mode].str, path);
#endif
				if(fd != -1) {
					CPU_AX = fd;
					CPU_CX = 1;
					msdos_file_handler_open(fd, path, _isatty(fd), mode, msdos_device_info(path), current_psp, sio_port, lpt_port);
					msdos_psp_set_file_table(fd, fd, current_psp);
				} else {
					CPU_AX = msdos_maperr(_doserrno);
					CPU_SET_C_FLAG(1);
				}
			} else if(CPU_DL & 2) {
				int attr = GetFileAttributesA(path);
				int fd = -1;
				int sio_port = 0;
				int lpt_port = 0;
				
				if(msdos_is_device_path(path)) {
					fd = msdos_open_device(path, file_mode[mode].mode, &sio_port, &lpt_port);
				} else {
					fd = _open(path, _O_RDWR | _O_BINARY | _O_CREAT | _O_TRUNC, _S_IREAD | _S_IWRITE);
				}
#ifdef ENABLE_DEBUG_OPEN_FILE
				fprintf(fp_debug_log, "int21h_6ch\tfd=%d\t%s\t%s\n", fd, file_mode[mode].str, path);
#endif
				if(fd != -1) {
					if(attr == -1) {
						attr = msdos_file_attribute_create(CPU_CX) & ~FILE_ATTRIBUTE_READONLY;
					}
					SetFileAttributesA(path, attr);
					CPU_AX = fd;
					CPU_CX = 3;
					msdos_file_handler_open(fd, path, _isatty(fd), 2, msdos_device_info(path), current_psp, sio_port, lpt_port);
					msdos_psp_set_file_table(fd, fd, current_psp);
				} else {
					CPU_AX = msdos_maperr(_doserrno);
					CPU_SET_C_FLAG(1);
				}
			} else {
#ifdef ENABLE_DEBUG_OPEN_FILE
				fprintf(fp_debug_log, "int21h_6ch\tfd=%d\t%s\t%s\n", -1, file_mode[mode].str, path);
#endif
				CPU_AX = 0x50;
				CPU_SET_C_FLAG(1);
			}
		} else {
			// file not exists
			if(CPU_DL & 0x10) {
				int fd = _open(path, _O_RDWR | _O_BINARY | _O_CREAT | _O_TRUNC, _S_IREAD | _S_IWRITE);
#ifdef ENABLE_DEBUG_OPEN_FILE
				fprintf(fp_debug_log, "int21h_6ch\tfd=%d\t%s\t%s\n", fd, file_mode[mode].str, path);
#endif
				
				if(fd != -1) {
					SetFileAttributesA(path, msdos_file_attribute_create(CPU_CX) & ~FILE_ATTRIBUTE_READONLY);
					CPU_AX = fd;
					CPU_CX = 2;
					msdos_file_handler_open(fd, path, _isatty(fd), 2, msdos_drive_number(path), current_psp);
					msdos_psp_set_file_table(fd, fd, current_psp);
				} else {
					CPU_AX = msdos_maperr(_doserrno);
					CPU_SET_C_FLAG(1);
				}
			} else {
#ifdef ENABLE_DEBUG_OPEN_FILE
				fprintf(fp_debug_log, "int21h_6ch\tfd=%d\t%s\t%s\n", -1, file_mode[mode].str, path);
#endif
				CPU_AX = 0x02;
				CPU_SET_C_FLAG(1);
			}
		}
	} else {
		CPU_AX = 0x0c;
		CPU_SET_C_FLAG(1);
	}
}

inline void msdos_int_21h_70h()
{
	switch(CPU_AL) {
	case 0x00: // get ??? info
	case 0x01: // set above info
//		unimplemented_21h("int %02Xh (AX=%04X BX=%04X CX=%04X DX=%04X SI=%04X DI=%04X DS=%04X ES=%04X)\n", 0x21, CPU_AX, CPU_BX, CPU_CX, CPU_DX, CPU_SI, CPU_DI, CPU_DS, CPU_ES);
		CPU_AX = 0x7000;
		CPU_SET_C_FLAG(1);
		break;
	case 0x02: // set general internationalization info
		if(CPU_CX >= 7) {
			active_code_page = *(UINT16 *)(mem + CPU_DS_BASE + CPU_SI + 5);
			msdos_nls_tables_update();
			set_country_info((country_info_t *)(mem + CPU_DS_BASE + CPU_SI + 7), CPU_CX - 7);
			CPU_AX = system_code_page;
		} else {
			CPU_AX = 0x0c;
			CPU_SET_C_FLAG(1);
		}
		break;
	default:
		unimplemented_21h("int %02Xh (AX=%04X BX=%04X CX=%04X DX=%04X SI=%04X DI=%04X DS=%04X ES=%04X)\n", 0x21, CPU_AX, CPU_BX, CPU_CX, CPU_DX, CPU_SI, CPU_DI, CPU_DS, CPU_ES);
		CPU_AX = 0x7000;
		CPU_SET_C_FLAG(1);
		break;
	}
}

inline void msdos_int_21h_710dh()
{
	// reset drive
}

inline void msdos_int_21h_7141h()
{
	if(CPU_SI == 0) {
		msdos_int_21h_41h(1);
		return;
	}
	if(CPU_SI != 1) {
		CPU_AX = 5;
		CPU_SET_C_FLAG(1);
	}
	/* wild card and matching attributes... */
	char tmp[MAX_PATH * 2];
	// copy search pathname (and quick check overrun)
	memset(tmp, 0, sizeof(tmp));
	tmp[MAX_PATH - 1] = '\0';
	tmp[MAX_PATH] = 1;
	strncpy(tmp, (char *)(mem + CPU_DS_BASE + CPU_DX), MAX_PATH);
	
	if(tmp[MAX_PATH - 1] != '\0' || tmp[MAX_PATH] != 1) {
		CPU_AX = 1;
		CPU_SET_C_FLAG(1);
		return;
	}
	for(char *s = tmp; *s; ++s) {
		if(*s == '/') {
			*s = '\\';
		}
	}
	char *tmp_name = my_strrchr(tmp, '\\');
	if(tmp_name) {
		++tmp_name;
	} else {
		tmp_name = strchr(tmp, ':');
		tmp_name = tmp_name ? tmp_name + 1 : tmp;
	}
	
	WIN32_FIND_DATAA fd;
	HANDLE fh = FindFirstFileA(tmp, &fd);
	if(fh == INVALID_HANDLE_VALUE) {
		CPU_AX = 2;
		CPU_SET_C_FLAG(1);
		return;
	}
	do {
		if(msdos_find_file_check_attribute(fd.dwFileAttributes, CPU_CL, CPU_CH) && msdos_find_file_has_8dot3name(&fd)) {
			strcpy(tmp_name, fd.cFileName);
			if(remove(msdos_trimmed_path(tmp, 1))) {
				CPU_AX = 5;
				CPU_SET_C_FLAG(1);
				break;
			}
		}
	} while(FindNextFileA(fh, &fd));
	if(!CPU_C_FLAG) {
		if(GetLastError() != ERROR_NO_MORE_FILES) {
			CPU_SET_C_FLAG(1);
			CPU_AX = 2;
		}
	}
	FindClose(fh);
}

inline void msdos_int_21h_714eh()
{
	process_t *process = msdos_process_info_get(current_psp);
	find_lfn_t *find = (find_lfn_t *)(mem + CPU_ES_BASE + CPU_DI);
	char *path = (char *)(mem + CPU_DS_BASE + CPU_DX);
	WIN32_FIND_DATAA fd;
	
	dtainfo_t *dtainfo = msdos_dta_info_get(current_psp, LFN_DTA_LADDR);
	if(dtainfo->find_handle != INVALID_HANDLE_VALUE) {
		FindClose(dtainfo->find_handle);
		dtainfo->find_handle = INVALID_HANDLE_VALUE;
	}
	strcpy(process->volume_label, msdos_volume_label(path));
	dtainfo->allowable_mask = CPU_CL;
	dtainfo->required_mask = CPU_CH;
	bool label_only = (dtainfo->allowable_mask == 8);
	
	if((dtainfo->allowable_mask & 8) && !msdos_match_volume_label(path, msdos_short_volume_label(process->volume_label))) {
		dtainfo->allowable_mask &= ~8;
	}
	if(!label_only && (dtainfo->find_handle = FindFirstFileA(path, &fd)) != INVALID_HANDLE_VALUE) {
		while(!msdos_find_file_check_attribute(fd.dwFileAttributes, dtainfo->allowable_mask, dtainfo->required_mask)) {
			if(!FindNextFileA(dtainfo->find_handle, &fd)) {
				FindClose(dtainfo->find_handle);
				dtainfo->find_handle = INVALID_HANDLE_VALUE;
				break;
			}
		}
	}
	if(dtainfo->find_handle != INVALID_HANDLE_VALUE) {
		find->attrib = fd.dwFileAttributes;
		msdos_find_file_conv_local_time(&fd);
		if(CPU_SI == 0) {
			find->ctime_lo.dw = fd.ftCreationTime.dwLowDateTime;
			find->ctime_hi.dw = fd.ftCreationTime.dwHighDateTime;
			find->atime_lo.dw = fd.ftLastAccessTime.dwLowDateTime;
			find->atime_hi.dw = fd.ftLastAccessTime.dwHighDateTime;
			find->mtime_lo.dw = fd.ftLastWriteTime.dwLowDateTime;
			find->mtime_hi.dw = fd.ftLastWriteTime.dwHighDateTime;
		} else {
			FileTimeToDosDateTime(&fd.ftCreationTime, &find->ctime_lo.w.h, &find->ctime_lo.w.l);
			FileTimeToDosDateTime(&fd.ftLastAccessTime, &find->atime_lo.w.h, &find->atime_lo.w.l);
			FileTimeToDosDateTime(&fd.ftLastWriteTime, &find->mtime_lo.w.h, &find->mtime_lo.w.l);
		}
		find->size_hi = fd.nFileSizeHigh;
		find->size_lo = fd.nFileSizeLow;
		strcpy(find->full_name, fd.cFileName);
		strcpy(find->short_name, fd.cAlternateFileName);
		CPU_AX = dtainfo - dtalist + 1;
	} else if(dtainfo->allowable_mask & 8) {
		// volume label
		find->attrib = 8;
		find->size_hi = find->size_lo = 0;
		strcpy(find->full_name, process->volume_label);
		strcpy(find->short_name, msdos_short_volume_label(process->volume_label));
		dtainfo->allowable_mask &= ~8;
		CPU_AX = dtainfo - dtalist + 1;
	} else {
		CPU_AX = 0x12;	// NOTE: return 0x02 if file path is invalid
		CPU_SET_C_FLAG(1);
	}
}

inline void msdos_int_21h_714fh()
{
	process_t *process = msdos_process_info_get(current_psp);
	find_lfn_t *find = (find_lfn_t *)(mem + CPU_ES_BASE + CPU_DI);
	WIN32_FIND_DATAA fd;
	
	if(CPU_BX - 1u >= MAX_DTAINFO) {
		CPU_AX = 6;
		CPU_SET_C_FLAG(1);
		return;
	}
	dtainfo_t *dtainfo = &dtalist[CPU_BX - 1];
	if(dtainfo->find_handle != INVALID_HANDLE_VALUE) {
		if(FindNextFileA(dtainfo->find_handle, &fd)) {
			while(!msdos_find_file_check_attribute(fd.dwFileAttributes, dtainfo->allowable_mask, dtainfo->required_mask)) {
				if(!FindNextFileA(dtainfo->find_handle, &fd)) {
					FindClose(dtainfo->find_handle);
					dtainfo->find_handle = INVALID_HANDLE_VALUE;
					break;
				}
			}
		} else {
			FindClose(dtainfo->find_handle);
			dtainfo->find_handle = INVALID_HANDLE_VALUE;
		}
	}
	if(dtainfo->find_handle != INVALID_HANDLE_VALUE) {
		find->attrib = fd.dwFileAttributes;
		msdos_find_file_conv_local_time(&fd);
		if(CPU_SI == 0) {
			find->ctime_lo.dw = fd.ftCreationTime.dwLowDateTime;
			find->ctime_hi.dw = fd.ftCreationTime.dwHighDateTime;
			find->atime_lo.dw = fd.ftLastAccessTime.dwLowDateTime;
			find->atime_hi.dw = fd.ftLastAccessTime.dwHighDateTime;
			find->mtime_lo.dw = fd.ftLastWriteTime.dwLowDateTime;
			find->mtime_hi.dw = fd.ftLastWriteTime.dwHighDateTime;
		} else {
			FileTimeToDosDateTime(&fd.ftCreationTime, &find->ctime_lo.w.h, &find->ctime_lo.w.l);
			FileTimeToDosDateTime(&fd.ftLastAccessTime, &find->atime_lo.w.h, &find->atime_lo.w.l);
			FileTimeToDosDateTime(&fd.ftLastWriteTime, &find->mtime_lo.w.h, &find->mtime_lo.w.l);
		}
		find->size_hi = fd.nFileSizeHigh;
		find->size_lo = fd.nFileSizeLow;
		strcpy(find->full_name, fd.cFileName);
		strcpy(find->short_name, fd.cAlternateFileName);
	} else if(dtainfo->allowable_mask & 8) {
		// volume label
		find->attrib = 8;
		find->size_hi = find->size_lo = 0;
		strcpy(find->full_name, process->volume_label);
		strcpy(find->short_name, msdos_short_volume_label(process->volume_label));
		dtainfo->allowable_mask &= ~8;
	} else {
		CPU_AX = 0x12;
		CPU_SET_C_FLAG(1);
	}
}

inline void msdos_int_21h_71a0h()
{
	DWORD max_component_len, file_sys_flag;
	
	if(GetVolumeInformationA((char *)(mem + CPU_DS_BASE + CPU_DX), NULL, 0, NULL, &max_component_len, &file_sys_flag, CPU_CX == 0 ? NULL : (char *)(mem + CPU_ES_BASE + CPU_DI), CPU_CX)) {
		CPU_BX = (UINT16)file_sys_flag & (FILE_CASE_SENSITIVE_SEARCH | FILE_CASE_PRESERVED_NAMES | FILE_UNICODE_ON_DISK | FILE_VOLUME_IS_COMPRESSED);
		CPU_BX |= 0x4000;				// supports LFN functions
		CPU_CX = (UINT16)max_component_len;		// 255
		CPU_DX = (UINT16)max_component_len + 5;	// 260
	} else {
		CPU_AX = msdos_maperr(GetLastError());
		CPU_SET_C_FLAG(1);
	}
}

inline void msdos_int_21h_71a1h()
{
	if(CPU_BX - 1u >= MAX_DTAINFO) {
		CPU_AX = 6;
		CPU_SET_C_FLAG(1);
		return;
	}
	dtainfo_t *dtainfo = &dtalist[CPU_BX - 1];
	if(dtainfo->find_handle != INVALID_HANDLE_VALUE) {
		FindClose(dtainfo->find_handle);
		dtainfo->find_handle = INVALID_HANDLE_VALUE;
	}
}

inline void msdos_int_21h_71a6h()
{
	process_t *process = msdos_process_info_get(current_psp);
	int fd = msdos_psp_get_file_table(CPU_BX, current_psp);
	
	UINT8 *buffer = (UINT8 *)(mem + CPU_DS_BASE + CPU_DX);
	struct _stat64 status;
	DWORD serial_number = 0;
	
	if(fd < process->max_files && file_handler[fd].valid) {
		if(_fstat64(fd, &status) == 0) {
			if(file_handler[fd].path[1] == ':') {
				// NOTE: we need to consider the network file path "\\host\share\"
				char volume[] = "A:\\";
				volume[0] = file_handler[fd].path[1];
				GetVolumeInformationA(volume, NULL, 0, &serial_number, NULL, NULL, NULL, 0);
			}
			*(UINT32 *)(buffer + 0x00) = GetFileAttributesA(file_handler[fd].path);
			*(UINT32 *)(buffer + 0x04) = (UINT32)(status.st_ctime & 0xffffffff);
			*(UINT32 *)(buffer + 0x08) = (UINT32)((status.st_ctime >> 32) & 0xffffffff);
			*(UINT32 *)(buffer + 0x0c) = (UINT32)(status.st_atime & 0xffffffff);
			*(UINT32 *)(buffer + 0x10) = (UINT32)((status.st_atime >> 32) & 0xffffffff);
			*(UINT32 *)(buffer + 0x14) = (UINT32)(status.st_mtime & 0xffffffff);
			*(UINT32 *)(buffer + 0x18) = (UINT32)((status.st_mtime >> 32) & 0xffffffff);
			*(UINT32 *)(buffer + 0x1c) = serial_number;
			*(UINT32 *)(buffer + 0x20) = (UINT32)((status.st_size >> 32) & 0xffffffff);
			*(UINT32 *)(buffer + 0x24) = (UINT32)(status.st_size & 0xffffffff);
			*(UINT32 *)(buffer + 0x28) = status.st_nlink;
			// this is dummy id and it will be changed when it is reopened...
			*(UINT32 *)(buffer + 0x2c) = 0;
			*(UINT32 *)(buffer + 0x30) = file_handler[fd].id;
		} else {
			CPU_AX = msdos_maperr(_doserrno);
			CPU_SET_C_FLAG(1);
		}
	} else {
		CPU_AX = 0x06;
		CPU_SET_C_FLAG(1);
	}
}

inline void msdos_int_21h_71a7h()
{
	switch(CPU_BL) {
	case 0x00:
		if(!FileTimeToDosDateTime((FILETIME *)(mem + CPU_DS_BASE + CPU_SI), &CPU_DX, &CPU_CX)) {
			CPU_AX = msdos_maperr(GetLastError());
			CPU_SET_C_FLAG(1);
		}
		break;
	case 0x01:
		// NOTE: we need to check BH that shows 10-millisecond untils past time in CX
		if(!DosDateTimeToFileTime(CPU_DX, CPU_CX, (FILETIME *)(mem + CPU_ES_BASE + CPU_DI))) {
			CPU_AX = msdos_maperr(GetLastError());
			CPU_SET_C_FLAG(1);
		}
		break;
	default:
		unimplemented_21h("int %02Xh (AX=%04X BX=%04X CX=%04X DX=%04X SI=%04X DI=%04X DS=%04X ES=%04X)\n", 0x21, CPU_AX, CPU_BX, CPU_CX, CPU_DX, CPU_SI, CPU_DI, CPU_DS, CPU_ES);
		CPU_AX = 0x7100;
		CPU_SET_C_FLAG(1);
		break;
	}
}

inline void msdos_int_21h_71a8h()
{
	if(CPU_DH == 0) {
		char tmp[MAX_PATH], fcb[MAX_PATH];
		strcpy(tmp, msdos_short_path((char *)(mem + CPU_DS_BASE + CPU_SI)));
		memset(fcb, 0x20, sizeof(fcb));
		int len = strlen(tmp);
		for(int i = 0, pos = 0; i < len; i++) {
			if(tmp[i] == '.') {
				pos = 8;
			} else {
				if(msdos_lead_byte_check(tmp[i])) {
					fcb[pos++] = tmp[i++];
				}
				fcb[pos++] = tmp[i];
			}
		}
		memcpy((char *)(mem + CPU_ES_BASE + CPU_DI), fcb, 11);
	} else {
		strcpy((char *)(mem + CPU_ES_BASE + CPU_DI), msdos_short_path((char *)(mem + CPU_DS_BASE + CPU_SI)));
	}
}

inline void msdos_int_21h_71aah()
{
	char drv[] = "A:", path[MAX_PATH];
	char *hoge=(char *)(mem + CPU_DS_BASE + CPU_DX);
	
	if(CPU_BL == 0) {
		drv[0] = 'A' + _getdrive() - 1;
	} else {
		drv[0] = 'A' + CPU_BL - 1;
	}
	switch(CPU_BH) {
	case 0x00:
		if(msdos_is_valid_drive((CPU_BL ? CPU_BL : _getdrive()) - 1)) {
			CPU_AX = 0x0f; // invalid drive
			CPU_SET_C_FLAG(1);
		} else if(DefineDosDeviceA(0, drv, (char *)(mem + CPU_DS_BASE + CPU_DX)) == 0) {
			CPU_AX = 0x03; // path not found
			CPU_SET_C_FLAG(1);
		}
		break;
	case 0x01:
		if(!msdos_is_valid_drive((CPU_BL ? CPU_BL : _getdrive()) - 1)) {
			CPU_AX = 0x0f; // invalid drive
			CPU_SET_C_FLAG(1);
		} else if(DefineDosDeviceA(DDD_REMOVE_DEFINITION, drv, NULL) == 0) {
			CPU_AX = 0x0f; // invalid drive
			CPU_SET_C_FLAG(1);
		}
		break;
	case 0x02:
		if(!msdos_is_valid_drive((CPU_BL ? CPU_BL : _getdrive()) - 1)) {
			CPU_AX = 0x0f; // invalid drive
			CPU_SET_C_FLAG(1);
		} else if(QueryDosDeviceA(drv, path, MAX_PATH) == 0) {
			CPU_AX = 0x0f; // invalid drive
			CPU_SET_C_FLAG(1);
		} else if(strncmp(path, "\\??\\", 4) != 0) {
			CPU_AX = 0x0f; // invalid drive
			CPU_SET_C_FLAG(1);
		} else {
			strcpy((char *)(mem + CPU_DS_BASE + CPU_DX), path + 4);
		}
		break;
	default:
		unimplemented_21h("int %02Xh (AX=%04X BX=%04X CX=%04X DX=%04X SI=%04X DI=%04X DS=%04X ES=%04X)\n", 0x21, CPU_AX, CPU_BX, CPU_CX, CPU_DX, CPU_SI, CPU_DI, CPU_DS, CPU_ES);
		CPU_AX = 0x7100;
		CPU_SET_C_FLAG(1);
		break;
	}
}

inline void msdos_int_21h_7300h()
{
	CPU_AL = CPU_CL;
	CPU_AH = 0;
}

inline void msdos_int_21h_7302h()
{
	int drive_num = (CPU_DL == 0) ? (_getdrive() - 1) : (CPU_DL - 1);
	UINT16 seg, ofs;
	
	if(CPU_CX < 0x3f) {
		CPU_AL = 0x18;
		CPU_SET_C_FLAG(1);
	} else if(!msdos_drive_param_block_update(drive_num, &seg, &ofs, 1)) {
		CPU_AL = 0xff;
		CPU_SET_C_FLAG(1);
	} else {
		memcpy(mem + CPU_ES_BASE + CPU_DI + 2, mem + (seg << 4) + ofs, sizeof(dpb_t));
	}
}

inline void msdos_int_21h_7303h()
{
	char *path = (char *)(mem + CPU_DS_BASE + CPU_DX);
	ext_space_info_t *info = (ext_space_info_t *)(mem + CPU_ES_BASE + CPU_DI);
	DWORD sectors_per_cluster, bytes_per_sector, free_clusters, total_clusters;
	
	if(GetDiskFreeSpaceA(path, &sectors_per_cluster, &bytes_per_sector, &free_clusters, &total_clusters)) {
		info->size_of_structure = sizeof(ext_space_info_t);
		info->structure_version = 0;
		info->sectors_per_cluster = sectors_per_cluster;
		info->bytes_per_sector = bytes_per_sector;
		info->available_clusters_on_drive = free_clusters;
		info->total_clusters_on_drive = total_clusters;
		info->available_sectors_on_drive = sectors_per_cluster * free_clusters;
		info->total_sectors_on_drive = sectors_per_cluster * total_clusters;
		info->available_allocation_units = free_clusters;	// ???
		info->total_allocation_units = total_clusters;		// ???
	} else {
		CPU_AX = msdos_maperr(GetLastError());
		CPU_SET_C_FLAG(1);
	}
}

inline void msdos_int_21h_dbh()
{
	// Novell NetWare - Workstation - Get Number of Local Drives
	dos_info_t *dos_info = (dos_info_t *)(mem + DOS_INFO_TOP);
	CPU_AL = dos_info->last_drive;
}

inline void msdos_int_21h_dch()
{
	// Novell NetWare - Connection Services - Get Connection Number
	CPU_AL = 0x00;
}

inline void msdos_int_24h()
{
	const char *message = NULL;
	int key = 0;
	
	for(int i = 0; i < array_length(critical_error_table); i++) {
		if(critical_error_table[i].code == (CPU_DI & 0xff) || critical_error_table[i].code == (UINT16)-1) {
			if(active_code_page == 932) {
				message = critical_error_table[i].message_japanese;
			}
			if(message == NULL) {
				message = critical_error_table[i].message_english;
			}
			*(UINT8 *)(mem + WORK_TOP) = strlen(message);
			strcpy((char *)(mem + WORK_TOP + 1), message);
			
			CPU_LOAD_SREG(CPU_ES_INDEX, WORK_TOP >> 4);
			CPU_DI = 0x0000;
			break;
		}
	}
	fprintf(stderr, "\n%s", message);
	if(!(CPU_AH & 0x80)) {
		if(CPU_AH & 0x01) {
			fprintf(stderr, " %s %c", (active_code_page == 932) ? "書き込み中 ドライブ" : "writing drive", 'A' + CPU_AL);
		} else {
			fprintf(stderr, " %s %c", (active_code_page == 932) ? "読み取り中 ドライブ" : "reading drive", 'A' + CPU_AL);
		}
	}
	fprintf(stderr, "\n");
	
	{
		fprintf(stderr, "%s",   (active_code_page == 932) ? "中止 (A)" : "Abort");
	}
	if(CPU_AH & 0x10) {
		fprintf(stderr, ", %s", (active_code_page == 932) ? "再試行 (R)" : "Retry");
	}
	if(CPU_AH & 0x20) {
		fprintf(stderr, ", %s", (active_code_page == 932) ? "無視 (I)" : "Ignore");
	}
	if(CPU_AH & 0x08) {
		fprintf(stderr, ", %s", (active_code_page == 932) ? "失敗 (F)" : "Fail");
	}
	fprintf(stderr, "? ");
	
	while(1) {
		while(!_kbhit()) {
			Sleep(10);
		}
		key = _getch();
		
		if(key == 'I' || key == 'i') {
			if(CPU_AH & 0x20) {
				CPU_AL = 0;
				break;
			}
		} else if(key == 'R' || key == 'r') {
			if(CPU_AH & 0x10) {
				CPU_AL = 1;
				break;
			}
		} else if(key == 'A' || key == 'a') {
			CPU_AL = 2;
			break;
		} else if(key == 'F' || key == 'f') {
			if(CPU_AH & 0x08) {
				CPU_AL = 3;
				break;
			}
		}
	}
	fprintf(stderr, "%c\n", key);
}

inline void msdos_int_25h()
{
	UINT16 seg, ofs;
	DWORD dwSize;
	
	CPU_PUSHF();
	
	if(!(CPU_AL < 26)) {
		CPU_AL = 0x01; // unit unknown
		CPU_SET_C_FLAG(1);
	} else if(!msdos_drive_param_block_update(CPU_AL, &seg, &ofs, 0)) {
		CPU_AL = 0x02; // drive not ready
		CPU_SET_C_FLAG(1);
	} else {
		dpb_t *dpb = (dpb_t *)(mem + (seg << 4) + ofs);
		char dev[64];
		sprintf(dev, "\\\\.\\%c:", 'A' + CPU_AL);
		
		HANDLE hFile = CreateFileA(dev, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL | FILE_FLAG_NO_BUFFERING, NULL);
		if(hFile == INVALID_HANDLE_VALUE) {
			CPU_AL = 0x02; // drive not ready
			CPU_SET_C_FLAG(1);
		} else {
			UINT32 top_sector  = CPU_DX;
			UINT16 sector_num  = CPU_CX;
			UINT32 buffer_addr = CPU_DS_BASE + CPU_BX;
			
			if(sector_num == 0xffff) {
				top_sector  = *(UINT32 *)(mem + buffer_addr + 0);
				sector_num  = *(UINT16 *)(mem + buffer_addr + 4);
				UINT16 ofs  = *(UINT16 *)(mem + buffer_addr + 6);
				UINT16 seg  = *(UINT16 *)(mem + buffer_addr + 8);
				buffer_addr = (seg << 4) + ofs;
			}
//			if(DeviceIoControl(hFile, FSCTL_LOCK_VOLUME, NULL, 0, NULL, 0, &dwSize, NULL) == 0) {
//				CPU_AL = 0x02; // drive not ready
//				CPU_SET_C_FLAG(1);
//			} else 
			if(SetFilePointer(hFile, top_sector * dpb->bytes_per_sector, NULL, FILE_BEGIN) == INVALID_SET_FILE_POINTER) {
				CPU_AL = 0x08; // sector not found
				CPU_SET_C_FLAG(1);
			} else if(ReadFile(hFile, mem + buffer_addr, sector_num * dpb->bytes_per_sector, &dwSize, NULL) == 0) {
				CPU_AL = 0x0b; // read error
				CPU_SET_C_FLAG(1);
			}
			CloseHandle(hFile);
		}
	}
}

inline void msdos_int_26h()
{
	// this operation may cause serious damage for drives, so support only floppy disk...
	UINT16 seg, ofs;
	DWORD dwSize;
	
	CPU_PUSHF();
	
	if(!(CPU_AL < 26)) {
		CPU_AL = 0x01; // unit unknown
		CPU_SET_C_FLAG(1);
	} else if(!msdos_drive_param_block_update(CPU_AL, &seg, &ofs, 0)) {
		CPU_AL = 0x02; // drive not ready
		CPU_SET_C_FLAG(1);
	} else {
		dpb_t *dpb = (dpb_t *)(mem + (seg << 4) + ofs);
		char dev[64];
		sprintf(dev, "\\\\.\\%c:", 'A' + CPU_AL);
		
		if(dpb->media_type == 0xf8) {
			// this drive is not a floppy
//			if(!(((dos_info_t *)(mem + DOS_INFO_TOP))->dos_flag & 0x40)) {
//				fatalerror("This application tried the absolute disk write to drive %c:\n", 'A' + CPU_AL);
//			}
			CPU_AL = 0x02; // drive not ready
			CPU_SET_C_FLAG(1);
		} else {
			HANDLE hFile = CreateFileA(dev, GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL | FILE_FLAG_NO_BUFFERING, NULL);
			if(hFile == INVALID_HANDLE_VALUE) {
				CPU_AL = 0x02; // drive not ready
				CPU_SET_C_FLAG(1);
			} else {
				UINT32 top_sector  = CPU_DX;
				UINT16 sector_num  = CPU_CX;
				UINT32 buffer_addr = CPU_DS_BASE + CPU_BX;
				
				if(sector_num == 0xffff) {
					top_sector  = *(UINT32 *)(mem + buffer_addr + 0);
					sector_num  = *(UINT16 *)(mem + buffer_addr + 4);
					UINT16 ofs  = *(UINT16 *)(mem + buffer_addr + 6);
					UINT16 seg  = *(UINT16 *)(mem + buffer_addr + 8);
					buffer_addr = (seg << 4) + ofs;
				}
				if(DeviceIoControl(hFile, FSCTL_LOCK_VOLUME, NULL, 0, NULL, 0, &dwSize, NULL) == 0) {
					CPU_AL = 0x02; // drive not ready
					CPU_SET_C_FLAG(1);
				} else if(SetFilePointer(hFile, top_sector * dpb->bytes_per_sector, NULL, FILE_BEGIN) == INVALID_SET_FILE_POINTER) {
					CPU_AL = 0x08; // sector not found
					CPU_SET_C_FLAG(1);
				} else if(WriteFile(hFile, mem + buffer_addr, sector_num * dpb->bytes_per_sector, &dwSize, NULL) == 0) {
					CPU_AL = 0x0a; // write error
					CPU_SET_C_FLAG(1);
				}
				CloseHandle(hFile);
			}
		}
	}
}

inline void msdos_int_27h()
{
	int paragraphs = (min(CPU_DX, 0xfff0) + 15) >> 4;
	try {
		msdos_mem_realloc(CPU_CS, paragraphs, NULL);
	} catch(...) {
		// recover the broken mcb
		int mcb_seg = CPU_CS - 1;
		mcb_t *mcb = (mcb_t *)(mem + (mcb_seg << 4));
		
		if(mcb_seg < (MEMORY_END >> 4)) {
			mcb->mz = 'M';
			mcb->paragraphs = (MEMORY_END >> 4) - mcb_seg - 2;
			
			if(((dos_info_t *)(mem + DOS_INFO_TOP))->umb_linked & 0x01) {
				msdos_mcb_create((MEMORY_END >> 4) - 1, 'M', PSP_SYSTEM, (UMB_TOP >> 4) - (MEMORY_END >> 4), "SC");
			} else {
				msdos_mcb_create((MEMORY_END >> 4) - 1, 'Z', PSP_SYSTEM, 0, "SC");
			}
		} else {
			mcb->mz = 'Z';
			mcb->paragraphs = (UMB_END >> 4) - mcb_seg - 1;
		}
		msdos_mem_realloc(CPU_CS, paragraphs, NULL);
	}
	msdos_process_terminate(CPU_CS, retval | 0x300, 0);
}

inline void msdos_int_29h()
{
	msdos_putch_fast(CPU_AL, 0x29, CPU_AH);
}

inline void msdos_int_2eh()
{
	char tmp[MAX_PATH], command[MAX_PATH], opt[MAX_PATH];
	memset(tmp, 0, sizeof(tmp));
	strcpy(tmp, (char *)(mem + CPU_DS_BASE + CPU_SI));
	char *token = my_strtok(tmp, " ");
	strcpy(command, token);
	strcpy(opt, token + strlen(token) + 1);
	
	param_block_t *param = (param_block_t *)(mem + WORK_TOP);
	param->env_seg = 0;
	param->cmd_line.w.l = 44;
	param->cmd_line.w.h = (WORK_TOP >> 4);
	param->fcb1.w.l = 24;
	param->fcb1.w.h = (WORK_TOP >> 4);
	param->fcb2.w.l = 24;
	param->fcb2.w.h = (WORK_TOP >> 4);
	
	memset(mem + WORK_TOP + 24, 0x20, 20);
	
	cmd_line_t *cmd_line = (cmd_line_t *)(mem + WORK_TOP + 44);
	cmd_line->len = strlen(opt);
	strcpy(cmd_line->cmd, opt);
	cmd_line->cmd[cmd_line->len] = 0x0d;
	
	try {
		if(msdos_process_exec(command, param, 0)) {
			CPU_AX = 0xffff; // error before processing command
		} else {
			// set flag to set retval to ax when the started process is terminated
			process_t *process = msdos_process_info_get(current_psp);
			process->called_by_int2eh = true;
		}
	} catch(...) {
		CPU_AX = 0xffff; // error before processing command
	}
}

inline void msdos_int_2fh_05h()
{
	switch(CPU_AL) {
	case 0x00:
		// critical error handler is installed
		CPU_AL = 0xff;
		break;
	case 0x01:
	case 0x02:
		for(int i = 0; i < array_length(standard_error_table); i++) {
			if(standard_error_table[i].code == CPU_BX || standard_error_table[i].code == (UINT16)-1) {
				const char *message = NULL;
				if(active_code_page == 932) {
					message = standard_error_table[i].message_japanese;
				}
				if(message == NULL) {
					message = standard_error_table[i].message_english;
				}
				strcpy((char *)(mem + WORK_TOP), message);
				
				CPU_LOAD_SREG(CPU_ES_INDEX, WORK_TOP >> 4);
				CPU_DI = 0x0000;
				CPU_AL = 0x01;
				break;
			}
		}
		break;
	default:
		unimplemented_2fh("int %02Xh (AX=%04X BX=%04X CX=%04X DX=%04X SI=%04X DI=%04X DS=%04X ES=%04X)\n", 0x2f, CPU_AX, CPU_BX, CPU_CX, CPU_DX, CPU_SI, CPU_DI, CPU_DS, CPU_ES);
		CPU_AX = 0x01;
		CPU_SET_C_FLAG(1);
	}
}

inline void msdos_int_2fh_06h()
{
	switch(CPU_AL) {
	case 0x00:
		// ASSIGN is not installed
//		CPU_AL = 0x00;
		break;
	case 0x01:
		// this call is available from within MIRROR.COM even if ASSIGN is not installed
		CPU_AX = 0x01;
		CPU_SET_C_FLAG(1);
		break;
	default:
		unimplemented_2fh("int %02Xh (AX=%04X BX=%04X CX=%04X DX=%04X SI=%04X DI=%04X DS=%04X ES=%04X)\n", 0x2f, CPU_AX, CPU_BX, CPU_CX, CPU_DX, CPU_SI, CPU_DI, CPU_DS, CPU_ES);
		CPU_AX = 0x01;
		CPU_SET_C_FLAG(1);
		break;
	}
}

inline void msdos_int_2fh_11h()
{
	switch(CPU_AL) {
	case 0x00:
		if(CPU_READ_STACK() == 0xdada) {
#ifdef SUPPORT_MSCDEX
			// MSCDEX is installed
			CPU_AL = 0xff;
			CPU_WRITE_STACK(0xadad);
#else
			// MSCDEX is not installed
//			CPU_AL = 0x00;
#endif
		} else {
			// Network Redirector is not installed
//			CPU_AL = 0x00;
		}
		break;
	default:
//		unimplemented_2fh("int %02Xh (AX=%04X BX=%04X CX=%04X DX=%04X SI=%04X DI=%04X DS=%04X ES=%04X)\n", 0x2f, CPU_AX, CPU_BX, CPU_CX, CPU_DX, CPU_SI, CPU_DI, CPU_DS, CPU_ES);
		CPU_AX = 0x49; //  network software not installed
		CPU_SET_C_FLAG(1);
		break;
	}
}

inline void msdos_int_2fh_12h()
{
	switch(CPU_AL) {
	case 0x00:
		// DOS 3.0+ internal functions are installed
		CPU_AL = 0xff;
		break;
//	case 0x01: // DOS 3.0+ internal - Close Current File
	case 0x02:
		{
			UINT16 stack = CPU_READ_STACK();
			CPU_BX = *(UINT16 *)(mem + 4 * stack + 0);
			CPU_LOAD_SREG(CPU_ES_INDEX, *(UINT16 *)(mem + 4 * stack + 2));
		}
		break;
	case 0x03:
		CPU_LOAD_SREG(CPU_DS_INDEX, DEVICE_TOP >> 4);
		break;
	case 0x04:
		{
			UINT16 stack = CPU_READ_STACK();
			CPU_AL = (stack == '/') ? '\\' : stack;
			CPU_SET_Z_FLAG(CPU_AL == '\\');
		}
		break;
	case 0x05:
		{
			UINT16 c = CPU_READ_STACK();
			if((c >> 0) & 0xff) {
				msdos_putch((c >> 0) & 0xff, 0x2f, 0x12);
			}
			if((c >> 8) & 0xff) {
				msdos_putch((c >> 8) & 0xff, 0x2f, 0x12);
			}
		}
		break;
//	case 0x06: // DOS 3.0+ internal - Invoke Critical Error
//	case 0x07: // DOS 3.0+ internal - Make Disk Buffer Most Recentry Used
//	case 0x08: // DOS 3.0+ internal - Decrement SFT Reference Count
//	case 0x09: // DOS 3.0+ internal - Flush and Free Disk Buffer
//	case 0x0a: // DOS 3.0+ internal - Perform Critical Error Interrupt
//	case 0x0b: // DOS 3.0+ internal - Signal Sharing Violation to User
//	case 0x0c: // DOS 3.0+ internal - Open Device and Set SFT Owner/Mode
	case 0x0d:
		{
			SYSTEMTIME time;
			FILETIME file_time;
			WORD dos_date, dos_time;
			GetLocalTime(&time);
			SystemTimeToFileTime(&time, &file_time);
			FileTimeToDosDateTime(&file_time, &dos_date, &dos_time);
			CPU_AX = dos_date;
			CPU_DX = dos_time;
		}
		break;
//	case 0x0e: // DOS 3.0+ internal - Mark All Disk Buffers Unreferenced
//	case 0x0f: // DOS 3.0+ internal - Make Buffer Most Recentry Used
//	case 0x10: // DOS 3.0+ internal - Find Unreferenced Disk Buffer
	case 0x11:
		{
			char path[MAX_PATH], *p;
			strcpy(path, (char *)(mem + CPU_DS_BASE + CPU_SI));
			my_strupr(path);
			while((p = my_strchr(path, '/')) != NULL) {
				*p = '\\';
			}
			strcpy((char *)(mem + CPU_ES_BASE + CPU_DI), path);
		}
		break;
	case 0x12:
		CPU_CX = strlen((char *)(mem + CPU_ES_BASE + CPU_DI));
		break;
	case 0x13:
		{
			char tmp[2] = {0};
			tmp[0] = CPU_READ_STACK();
			my_strupr(tmp);
			CPU_AL = tmp[0];
		}
		break;
	case 0x14:
		CPU_SET_Z_FLAG(CPU_DS_BASE + CPU_SI == CPU_ES_BASE + CPU_DI);
		CPU_SET_C_FLAG(CPU_DS_BASE + CPU_SI != CPU_ES_BASE + CPU_DI);
		break;
//	case 0x15: // DOS 3.0+ internal - Flush Buffer
	case 0x16:
		if(CPU_BX < 20) {
			CPU_LOAD_SREG(CPU_ES_INDEX, SFT_TOP >> 4);
			CPU_DI = 6 + 0x3b * CPU_BX;
			
			// update system file table
			UINT8* sft = mem + SFT_TOP + 6 + 0x3b * CPU_BX;
			if(file_handler[CPU_BX].valid) {
				int count = 0;
				for(int i = 0; i < 20; i++) {
					if(msdos_psp_get_file_table(i, current_psp) == CPU_BX) {
						count++;
					}
				}
				*(UINT16 *)(sft + 0x00) = count ? count : 0xffff;
				*(UINT32 *)(sft + 0x15) = _tell(CPU_BX);
				_lseek(CPU_BX, 0, SEEK_END);
				*(UINT32 *)(sft + 0x11) = _tell(CPU_BX);
				_lseek(CPU_BX, *(UINT32 *)(sft + 0x15), SEEK_SET);
			} else {
				memset(sft, 0, 0x3b);
			}
		} else {
			CPU_AX = 0x06;
			CPU_SET_C_FLAG(1);
		}
		break;
	case 0x17:
		{
			UINT16 drive = CPU_READ_STACK();
			if(msdos_is_valid_drive(drive)) {
				msdos_cds_update(drive);
			}
			CPU_SI = 88 * drive;
			CPU_LOAD_SREG(CPU_DS_INDEX, CDS_TOP >> 4);
		}
		break;
//	case 0x18: // DOS 3.0+ internal - Get Caller's Registers
//	case 0x19: // DOS 3.0+ internal - Set Drive???
	case 0x1a:
		{
			char *path = (char *)(mem + CPU_DS_BASE + CPU_SI), full[MAX_PATH];
			if(path[1] == ':') {
				if(path[0] >= 'a' && path[0] <= 'z') {
					CPU_AL = path[0] - 'a' + 1;
				} else if(path[0] >= 'A' && path[0] <= 'Z') {
					CPU_AL = path[0] - 'A' + 1;
				} else {
					CPU_AL = 0xff; // invalid
				}
				strcpy(full, path);
				strcpy(path, full + 2);
			} else if(GetFullPathNameA(path, MAX_PATH, full, NULL) != 0 && full[1] == ':') {
				if(full[0] >= 'a' && full[0] <= 'z') {
					CPU_AL = full[0] - 'a' + 1;
				} else if(full[0] >= 'A' && full[0] <= 'Z') {
					CPU_AL = full[0] - 'A' + 1;
				} else {
					CPU_AL = 0xff; // invalid
				}
			} else {
				CPU_AL = 0x00; // default
			}
		}
		break;
	case 0x1b:
		{
			int year = CPU_CX + 1980;
			CPU_AL = ((year % 4) == 0 && (year % 100) != 0 && (year % 400) == 0) ? 29 : 28;
		}
		break;
//	case 0x1c: // DOS 3.0+ internal - Check Sum Memory
//	case 0x1d: // DOS 3.0+ internal - Sum Memory
	case 0x1e:
		{
			char *path_1st = (char *)(mem + CPU_DS_BASE + CPU_SI), full_1st[MAX_PATH];
			char *path_2nd = (char *)(mem + CPU_ES_BASE + CPU_DI), full_2nd[MAX_PATH];
			if(GetFullPathNameA(path_1st, MAX_PATH, full_1st, NULL) != 0 && GetFullPathNameA(path_2nd, MAX_PATH, full_2nd, NULL) != 0) {
				CPU_SET_Z_FLAG(strcmp(full_1st, full_2nd) == 0);
			} else {
				CPU_SET_Z_FLAG(strcmp(path_1st, path_2nd) == 0);
			}
		}
		break;
	case 0x1f:
		{
			UINT16 drive = CPU_READ_STACK();
			if(msdos_is_valid_drive(drive)) {
				msdos_cds_update(drive);
			}
			CPU_SI = 88 * drive;
			CPU_LOAD_SREG(CPU_ES_INDEX, CDS_TOP >> 4);
		}
		break;
	case 0x20:
		{
			int fd = msdos_psp_get_file_table(CPU_BX, current_psp);
			
			if(fd < 20) {
				CPU_LOAD_SREG(CPU_ES_INDEX, current_psp);
				CPU_DI = offsetof(psp_t, file_table) + fd;
			} else {
				CPU_AX = 0x06;
				CPU_SET_C_FLAG(1);
			}
		}
		break;
	case 0x21:
		msdos_int_21h_60h(0);
		break;
	case 0x22:
		{
			sda_t *sda = (sda_t *)(mem + SDA_TOP);
			if(*(UINT8 *)(mem + CPU_SS_BASE + CPU_SI + 0x00) != 0xff) {
				sda->extended_error_code = *(UINT8 *)(mem + CPU_SS_BASE + CPU_SI + 0x00);
			}
			if(*(UINT8 *)(mem + CPU_SS_BASE + CPU_SI + 0x01) != 0xff) {
				sda->error_class         = *(UINT8 *)(mem + CPU_SS_BASE + CPU_SI + 0x01);
			}
			if(*(UINT8 *)(mem + CPU_SS_BASE + CPU_SI + 0x02) != 0xff) {
				sda->suggested_action    = *(UINT8 *)(mem + CPU_SS_BASE + CPU_SI + 0x02);
			}
			if(*(UINT8 *)(mem + CPU_SS_BASE + CPU_SI + 0x03) != 0xff) {
				sda->locus_of_last_error = *(UINT8 *)(mem + CPU_SS_BASE + CPU_SI + 0x03);
			}
		}
		break;
//	case 0x23: // DOS 3.0+ internal - Check If Character Device
//	case 0x24: // DOS 3.0+ internal - Sharing Retry Delay
	case 0x25:
		CPU_CX = strlen((char *)(mem + CPU_DS_BASE + CPU_SI));
		break;
	case 0x26:
		CPU_AL = CPU_CL;
		msdos_int_21h_3dh();
		break;
	case 0x27:
		msdos_int_21h_3eh();
		break;
	case 0x28:
		CPU_AX = CPU_BP;
		msdos_int_21h_42h();
		break;
	case 0x29:
		msdos_int_21h_3fh();
		break;
//	case 0x2a: // DOS 3.0+ internal - Set Fast Open Entry Point
	case 0x2b:
		CPU_AX = CPU_BP;
		msdos_int_21h_44h();
		break;
	case 0x2c:
		CPU_BX = DEVICE_TOP >> 4;
		CPU_AX = 22;
		break;
	case 0x2d:
		{
			sda_t *sda = (sda_t *)(mem + SDA_TOP);
			CPU_AX = sda->extended_error_code;
		}
		break;
	case 0x2e:
		if(CPU_DL == 0x00 || CPU_DL == 0x02 || CPU_DL == 0x04 || CPU_DL == 0x06) {
			CPU_LOAD_SREG(CPU_ES_INDEX, 0x0001);
			CPU_DI = 0x00;
		} else if(CPU_DL == 0x08) {
			// dummy parameter error message read routine is at fffc:0010
			CPU_LOAD_SREG(CPU_ES_INDEX, DUMMY_TOP >> 4);
			CPU_DI = 0x0010;
		}
		break;
	case 0x2f:
		if(CPU_DX != 0) {
			dos_major_version = CPU_DL;
			dos_minor_version = CPU_DH;
		} else {
			CPU_DL = 7;
			CPU_DH = 10;
		}
		break;
//	case 0x30: // Windows95 - Find SFT Entry in Internal File Tables
//	case 0x31: // Windows95 - Set/Clear "Report Windows to DOS Programs" Flag
	default:
		unimplemented_2fh("int %02Xh (AX=%04X BX=%04X CX=%04X DX=%04X SI=%04X DI=%04X DS=%04X ES=%04X)\n", 0x2f, CPU_AX, CPU_BX, CPU_CX, CPU_DX, CPU_SI, CPU_DI, CPU_DS, CPU_ES);
		CPU_AX = 0x01;
		CPU_SET_C_FLAG(1);
		break;
	}
}

inline void msdos_int_2fh_13h()
{
	static UINT16 prevDS = 0, prevDX = 0;
	static UINT16 prevES = 0, prevBX = 0;
	
	UINT16 tmpDS = CPU_DS;
	CPU_LOAD_SREG(CPU_DS_INDEX, prevDS);
	prevDS = tmpDS;
	
	UINT16 tmpDX = CPU_DX;
	CPU_DX = prevDX;
	prevDX = tmpDX;
	
	UINT16 tmpES = CPU_ES;
	CPU_LOAD_SREG(CPU_ES_INDEX, prevES);
	prevES = tmpES;
	
	UINT16 tmpBX = CPU_BX;
	CPU_BX = prevBX;
	prevBX = tmpBX;
}

inline void msdos_int_2fh_14h()
{
	switch(CPU_AL) {
	case 0x00:
		// NLSFUNC.COM is installed
		CPU_AL = 0xff;
		break;
	case 0x01:
	case 0x03:
		CPU_AL = 0x00;
		active_code_page = CPU_BX;
		msdos_nls_tables_update();
		break;
	case 0x02:
		CPU_AL = get_extended_country_info(CPU_BP);
		break;
	case 0x04:
		for(int i = 0;; i++) {
			if(country_table[i].code == CPU_DX) {
				get_country_info((country_info_t *)(mem + CPU_ES_BASE + CPU_DI), country_table[i].usPrimaryLanguage, country_table[i].usSubLanguage);
				break;
			} else if(country_table[i].code == -1) {
				get_country_info((country_info_t *)(mem + CPU_ES_BASE + CPU_DI));
				break;
			}
		}
		CPU_AL = 0x00;
		break;
	default:
		unimplemented_2fh("int %02Xh (AX=%04X BX=%04X CX=%04X DX=%04X SI=%04X DI=%04X DS=%04X ES=%04X)\n", 0x2f, CPU_AX, CPU_BX, CPU_CX, CPU_DX, CPU_SI, CPU_DI, CPU_DS, CPU_ES);
		CPU_AX = 0x01;
		CPU_SET_C_FLAG(1);
		break;
	}
}

inline void msdos_int_2fh_15h()
{
	switch(CPU_AL) {
	case 0x00: // CD-ROM - Installation Check
		if(CPU_BX == 0x0000) {
#ifdef SUPPORT_MSCDEX
			// MSCDEX is installed
			CPU_BX = 0;
			for(int i = 0, n = 0; i < 26; i++) {
				if(msdos_is_cdrom_drive(i)) {
					if(CPU_BX == 0) {
						CPU_CX = i;
					}
					CPU_BX++;
				}
			}
#else
			// MSCDEX is not installed
//			CPU_AL = 0x00;
#endif
		} else {
			// GRAPHICS.COM is not installed
//			CPU_AL = 0x00;
		}
		break;
	case 0x0b:
		// this call is available from within DOSSHELL even if MSCDEX is not installed
		CPU_AX = msdos_is_cdrom_drive(CPU_CL) ? 0x5ad8 : 0x0000;
		CPU_BX = 0xadad;
		break;
	case 0x0d:
		for(int i = 0, n = 0; i < 26; i++) {
			if(msdos_is_cdrom_drive(i)) {
				mem[CPU_ES_BASE + CPU_BX + (n++)] = i;
			}
		}
		break;
	case 0xff:
		if(CPU_BX == 0x0000) {
			// CORELCDX is not installed
		} else {
			unimplemented_2fh("int %02Xh (AX=%04X BX=%04X CX=%04X DX=%04X SI=%04X DI=%04X DS=%04X ES=%04X)\n", 0x2f, CPU_AX, CPU_BX, CPU_CX, CPU_DX, CPU_SI, CPU_DI, CPU_DS, CPU_ES);
			CPU_AX = 0x01;
			CPU_SET_C_FLAG(1);
		}
		break;
	default:
		unimplemented_2fh("int %02Xh (AX=%04X BX=%04X CX=%04X DX=%04X SI=%04X DI=%04X DS=%04X ES=%04X)\n", 0x2f, CPU_AX, CPU_BX, CPU_CX, CPU_DX, CPU_SI, CPU_DI, CPU_DS, CPU_ES);
		CPU_AX = 0x01;
		CPU_SET_C_FLAG(1);
		break;
	}
}

inline void msdos_int_2fh_16h()
{
	switch(CPU_AL) {
	case 0x00:
		if(no_windows) {
			// neither Windows 3.x enhanced mode nor Windows/386 2.x running
//			CPU_AL = 0x00;
		} else {
			CPU_AL = win_major_version;
			CPU_AH = win_minor_version;
		}
		break;
	case 0x05: // Windows Enhanced Mode & 286 DOSX Init Broadcast
		// from DOSBox
		CPU_A20_LINE(1);
		break;
	case 0x06: // Windows Enhanced Mode & 286 DOSX Exit Broadcast
	case 0x08: // Windows Enhanced Mode Init Complete Broadcast
	case 0x09: // Windows Enhanced Mode Begin Exit Broadcast
		break;
	case 0x07:
		// Virtual Device Call API
		break;
	case 0x0a:
		if(!no_windows) {
			CPU_AX = 0x0000;
			CPU_BH = win_major_version;
			CPU_BL = win_minor_version;
//			CPU_CX = 0x0002; // standard
			CPU_CX = 0x0003; // enhanced
		}
		break;
	case 0x0b:
		// no TRS, keep ES:DI = 0000h:0000h
	case 0x0e:
	case 0x0f:
	case 0x10:
	case 0x11:
	case 0x12:
	case 0x13:
	case 0x14:
	case 0x15:
	case 0x81:
	case 0x82:
	case 0x84:
	case 0x85:
	case 0x86:
	case 0x87:
	case 0x89:
	case 0x8a:
		// function not supported, do not clear AX
		break;
	case 0x80:
		Sleep(10);
		REQUEST_HARDWRE_UPDATE();
		CPU_AL = 0x00;
		break;
	case 0x83:
		CPU_BX = 0x01; // system vm id
		break;
	case 0x8e:
		CPU_AX = 0x00; // failed
		break;
	case 0x8f:
		switch(CPU_DH) {
		case 0x01:
//			CPU_AX = 0x0000; // close command selected but not yet acknowledged
//			CPU_AX = 0x0001; // close command issued and acknowledged
			CPU_AX = 0x168f; // close command not selected -- application should continue
			break;
		default:
			CPU_AX = 0x0000; // successful
			break;
		}
		break;
	default:
		unimplemented_2fh("int %02Xh (AX=%04X BX=%04X CX=%04X DX=%04X SI=%04X DI=%04X DS=%04X ES=%04X)\n", 0x2f, CPU_AX, CPU_BX, CPU_CX, CPU_DX, CPU_SI, CPU_DI, CPU_DS, CPU_ES);
		CPU_AX = 0x01;
		CPU_SET_C_FLAG(1);
		break;
	}
}

inline void msdos_int_2fh_19h()
{
	switch(CPU_AL) {
	case 0x00:
		// SHELLB.COM is not installed
//		CPU_AL = 0x00;
		break;
	case 0x01:
	case 0x02:
	case 0x03:
	case 0x04:
		CPU_AX = 0x01;
		CPU_SET_C_FLAG(1);
		break;
	case 0x80:
		// IBM ROM-DOS v4.0 is not installed
//		CPU_AL = 0x00;
		break;
	default:
		unimplemented_2fh("int %02Xh (AX=%04X BX=%04X CX=%04X DX=%04X SI=%04X DI=%04X DS=%04X ES=%04X)\n", 0x2f, CPU_AX, CPU_BX, CPU_CX, CPU_DX, CPU_SI, CPU_DI, CPU_DS, CPU_ES);
		CPU_AX = 0x01;
		CPU_SET_C_FLAG(1);
		break;
	}
}

inline void msdos_int_2fh_1ah()
{
	switch(CPU_AL) {
	case 0x00:
		// ANSI.SYS is installed
		CPU_AL = 0xff;
		break;
	case 0x01:
		if(CPU_CL == 0x5f) {
			// set display information
			if(*(UINT16 *)(mem + CPU_DS_BASE + CPU_DX + 0x02) >= 14) {
				int cur_width  = *(UINT16 *)(mem + 0x44a) + 0;
				int cur_height = *(UINT8  *)(mem + 0x484) + 1;
				int new_width  = *(UINT16 *)(mem + CPU_DS_BASE + CPU_DX + 0x0e);	// character columns
				int new_height = *(UINT16 *)(mem + CPU_DS_BASE + CPU_DX + 0x10);	// character rows
				
				if(cur_width != new_width || cur_height != new_height) {
					pcbios_set_console_size(new_width, new_height, true);
				}
			}
		} else if(CPU_CL == 0x7f) {
			// get display information
			*(UINT8  *)(mem + CPU_DS_BASE + CPU_DX + 0x00) = 0;	// level (0 for DOS 4.x-6.0)
			*(UINT8  *)(mem + CPU_DS_BASE + CPU_DX + 0x01) = 0;	// reserved (0)
			*(UINT16 *)(mem + CPU_DS_BASE + CPU_DX + 0x02) = 14;	// length of following data (14)
			*(UINT16 *)(mem + CPU_DS_BASE + CPU_DX + 0x04) = 1;	// bit 0 set for blink, clear for intensity
			*(UINT8  *)(mem + CPU_DS_BASE + CPU_DX + 0x06) = 1;	// mode type (1=text, 2=graphics)
			*(UINT8  *)(mem + CPU_DS_BASE + CPU_DX + 0x07) = 0;	// reserved (0)
			*(UINT16 *)(mem + CPU_DS_BASE + CPU_DX + 0x08) = 4;	// 4 bits per pixel
			*(UINT16 *)(mem + CPU_DS_BASE + CPU_DX + 0x0a) =  8 * (*(UINT16 *)(mem + 0x44a) + 0);	// pixel columns
			*(UINT16 *)(mem + CPU_DS_BASE + CPU_DX + 0x0c) = 16 * (*(UINT8  *)(mem + 0x484) + 1);	// pixel rows
			*(UINT16 *)(mem + CPU_DS_BASE + CPU_DX + 0x0e) = *(UINT16 *)(mem + 0x44a) + 0;		// character columns
			*(UINT16 *)(mem + CPU_DS_BASE + CPU_DX + 0x10) = *(UINT8  *)(mem + 0x484) + 1;		// character rows
		} else {
			unimplemented_2fh("int %02Xh (AX=%04X BX=%04X CX=%04X DX=%04X SI=%04X DI=%04X DS=%04X ES=%04X)\n", 0x2f, CPU_AX, CPU_BX, CPU_CX, CPU_DX, CPU_SI, CPU_DI, CPU_DS, CPU_ES);
			CPU_AX = 0x01;
			CPU_SET_C_FLAG(1);
		}
		break;
	default:
		unimplemented_2fh("int %02Xh (AX=%04X BX=%04X CX=%04X DX=%04X SI=%04X DI=%04X DS=%04X ES=%04X)\n", 0x2f, CPU_AX, CPU_BX, CPU_CX, CPU_DX, CPU_SI, CPU_DI, CPU_DS, CPU_ES);
		CPU_AX = 0x01;
		CPU_SET_C_FLAG(1);
		break;
	}
}

inline void msdos_int_2fh_40h()
{
	switch(CPU_AL) {
	case 0x00:
		// Windows 3+ - Get Virtual Device Driver (VDD) Capabilities
		CPU_AL = 0x01; // does not virtualize video access
		break;
	case 0x07:
		// Windows 3.x - Enable VDD Trapping of Video Registers
		break;
	case 0x10:
		// OS/2 v2.0+ - Installation Check
		CPU_AX = 0x01;
		CPU_SET_C_FLAG(1);
		break;
	default:
		unimplemented_2fh("int %02Xh (AX=%04X BX=%04X CX=%04X DX=%04X SI=%04X DI=%04X DS=%04X ES=%04X)\n", 0x2f, CPU_AX, CPU_BX, CPU_CX, CPU_DX, CPU_SI, CPU_DI, CPU_DS, CPU_ES);
		CPU_AX = 0x01;
		CPU_SET_C_FLAG(1);
		break;
	}
}

inline void msdos_int_2fh_43h()
{
	switch(CPU_AL) {
	case 0x00:
		// XMS is installed ?
#ifdef SUPPORT_XMS
		if(support_xms) {
			CPU_AL = 0x80;
		}
#endif
		break;
	case 0x08:
#ifdef SUPPORT_XMS
		if(support_xms) {
			CPU_AL = 0x43;
			CPU_BL = 0x01; // IBM PC/AT
			CPU_BH = 0x01; // Fast AT A20 switch time
		}
#endif
		break;
	case 0x10:
		CPU_LOAD_SREG(CPU_ES_INDEX, XMS_TOP >> 4);
		CPU_BX = 0x15;
		break;
	case 0xe0:
		// DOS Protected Mode Services (DPMS) v1.0 is not installed
		if(CPU_BX == 0x0000 && CPU_CX == 0x4450 && CPU_DX == 0x4d53) {
			break;
		}
	default:
		unimplemented_2fh("int %02Xh (AX=%04X BX=%04X CX=%04X DX=%04X SI=%04X DI=%04X DS=%04X ES=%04X)\n", 0x2f, CPU_AX, CPU_BX, CPU_CX, CPU_DX, CPU_SI, CPU_DI, CPU_DS, CPU_ES);
		CPU_AX = 0x01;
		CPU_SET_C_FLAG(1);
		break;
	}
}

inline void msdos_int_2fh_46h()
{
	switch(CPU_AL) {
	case 0x80:
		// Windows v3.0 is not installed
//		CPU_AL = 0x00;
		break;
	default:
		unimplemented_2fh("int %02Xh (AX=%04X BX=%04X CX=%04X DX=%04X SI=%04X DI=%04X DS=%04X ES=%04X)\n", 0x2f, CPU_AX, CPU_BX, CPU_CX, CPU_DX, CPU_SI, CPU_DI, CPU_DS, CPU_ES);
		CPU_AX = 0x01;
		CPU_SET_C_FLAG(1);
		break;
	}
}

inline void msdos_int_2fh_48h()
{
	switch(CPU_AL) {
	case 0x00:
		// DOSKEY is not installed
//		CPU_AL = 0x00;
		break;
	case 0x10:
		msdos_int_21h_0ah();
		CPU_AX = 0x00;
		break;
	default:
		unimplemented_2fh("int %02Xh (AX=%04X BX=%04X CX=%04X DX=%04X SI=%04X DI=%04X DS=%04X ES=%04X)\n", 0x2f, CPU_AX, CPU_BX, CPU_CX, CPU_DX, CPU_SI, CPU_DI, CPU_DS, CPU_ES);
		CPU_AX = 0x01;
		CPU_SET_C_FLAG(1);
		break;
	}
}

inline void msdos_int_2fh_4ah()
{
	switch(CPU_AL) {
#ifdef SUPPORT_HMA
	case 0x01: // DOS 5.0+ - Query Free HMA Space
		if(!is_hma_used_by_xms && !is_hma_used_by_int_2fh) {
			if(!msdos_is_hma_mcb_valid((hma_mcb_t *)(mem + 0xffff0 + 0x10))) {
				// restore first free mcb in high memory area
				msdos_hma_mcb_create(0x10, 0, 0xffe0, 0);
			}
			int offset = 0xffff;
			if((CPU_BX = msdos_hma_mem_get_free(&offset)) != 0) {
				CPU_DI = offset + 0x10;
			} else {
				CPU_DI = 0xffff;
			}
		} else {
			// HMA is already used
			CPU_BX = 0;
			CPU_DI = 0xffff;
		}
		CPU_LOAD_SREG(CPU_ES_INDEX, 0xffff);
		break;
	case 0x02: // DOS 5.0+ - Allocate HMA Space
		if(!is_hma_used_by_xms && !is_hma_used_by_int_2fh) {
			if(!msdos_is_hma_mcb_valid((hma_mcb_t *)(mem + 0xffff0 + 0x10))) {
				// restore first free mcb in high memory area
				msdos_hma_mcb_create(0x10, 0, 0xffe0, 0);
			}
			int size = CPU_BX, offset;
			if((size % 16) != 0) {
				size &= ~15;
				size += 16;
			}
			if((offset = msdos_hma_mem_alloc(size, current_psp)) != -1) {
				CPU_BX = size;
				CPU_DI = offset + 0x10;
				is_hma_used_by_int_2fh = true;
			} else {
				CPU_BX = 0;
				CPU_DI = 0xffff;
			}
		} else {
			// HMA is already used
			CPU_BX = 0;
			CPU_DI = 0xffff;
		}
		CPU_LOAD_SREG(CPU_ES_INDEX, 0xffff);
		break;
	case 0x03: // Windows95 - (De)Allocate HMA Memory Block
		if(CPU_DL == 0x00) {
			if(!is_hma_used_by_xms) {
				if(!msdos_is_hma_mcb_valid((hma_mcb_t *)(mem + 0xffff0 + 0x10))) {
					// restore first free mcb in high memory area
					msdos_hma_mcb_create(0x10, 0, 0xffe0, 0);
					is_hma_used_by_int_2fh = false;
				}
				int size = CPU_BX, offset;
				if((size % 16) != 0) {
					size &= ~15;
					size += 16;
				}
				if((offset = msdos_hma_mem_alloc(size, CPU_CX)) != -1) {
//					CPU_BX = size;
					CPU_LOAD_SREG(CPU_ES_INDEX, 0xffff);
					CPU_DI = offset + 0x10;
					is_hma_used_by_int_2fh = true;
				} else {
					CPU_DI = 0xffff;
				}
			} else {
				CPU_DI = 0xffff;
			}
		} else if(CPU_DL == 0x01) {
			if(!is_hma_used_by_xms) {
				int size = CPU_BX;
				if((size % 16) != 0) {
					size &= ~15;
					size += 16;
				}
				if(msdos_hma_mem_realloc(CPU_ES_BASE + CPU_DI - 0xffff0 - 0x10, size) != -1) {
					// memory block address is not changed
				} else {
					CPU_DI = 0xffff;
				}
			} else {
				CPU_DI = 0xffff;
			}
		} else if(CPU_DL == 0x02) {
			if(!is_hma_used_by_xms) {
				if(!msdos_is_hma_mcb_valid((hma_mcb_t *)(mem + 0xffff0 + 0x10))) {
					// restore first free mcb in high memory area
					msdos_hma_mcb_create(0x10, 0, 0xffe0, 0);
					is_hma_used_by_int_2fh = false;
				} else {
					msdos_hma_mem_free(CPU_ES_BASE + CPU_DI - 0xffff0 - 0x10);
					if(msdos_hma_mem_get_free(NULL) == 0xffe0) {
						is_hma_used_by_int_2fh = false;
					}
				}
			}
		} else {
			unimplemented_2fh("int %02Xh (AX=%04X BX=%04X CX=%04X DX=%04X SI=%04X DI=%04X DS=%04X ES=%04X)\n", 0x2f, CPU_AX, CPU_BX, CPU_CX, CPU_DX, CPU_SI, CPU_DI, CPU_DS, CPU_ES);
			CPU_AX = 0x01;
			CPU_SET_C_FLAG(1);
		}
		break;
	case 0x04: // Windows95 - Get Start of HMA Memory Chain
		if(!is_hma_used_by_xms) {
			if(!msdos_is_hma_mcb_valid((hma_mcb_t *)(mem + 0xffff0 + 0x10))) {
				// restore first free mcb in high memory area
				msdos_hma_mcb_create(0x10, 0, 0xffe0, 0);
				is_hma_used_by_int_2fh = false;
			}
			CPU_AX = 0x0000;
			CPU_LOAD_SREG(CPU_ES_INDEX, 0xffff);
			CPU_DI = 0x10;
		}
		break;
#else
	case 0x01:
	case 0x02:
		// HMA is already used
		CPU_BX = 0x0000;
		CPU_LOAD_SREG(CPU_ES_INDEX, 0xffff);
		CPU_DI = 0xffff;
		break;
	case 0x03:
		// unable to allocate
		CPU_DI = 0xffff;
		break;
	case 0x04:
		// function not supported, do not clear AX
		break;
#endif
	case 0x10:
		switch(CPU_BX) {
		case 0x0000:
		case 0x0001:
		case 0x0002:
		case 0x0003:
		case 0x0004:
		case 0x0005:
		case 0x0006:
		case 0x0007:
		case 0x0008:
		case 0x000a:
		case 0x1234:
			// SMARTDRV v4.00+ is not installed
			break;
		default:
			unimplemented_2fh("int %02Xh (AX=%04X BX=%04X CX=%04X DX=%04X SI=%04X DI=%04X DS=%04X ES=%04X)\n", 0x2f, CPU_AX, CPU_BX, CPU_CX, CPU_DX, CPU_SI, CPU_DI, CPU_DS, CPU_ES);
			CPU_AX = 0x01;
			CPU_SET_C_FLAG(1);
			break;
		}
		break;
	case 0x11:
		switch(CPU_BX) {
		case 0x0000:
		case 0x0001:
		case 0x0002:
		case 0x0003:
		case 0x0004:
		case 0x0005:
		case 0x0006:
		case 0x0007:
		case 0x0008:
		case 0x0009:
		case 0x000a:
		case 0x000b:
		case 0xfffe:
		case 0xffff:
			// DBLSPACE.BIN is not installed
			break;
		default:
			unimplemented_2fh("int %02Xh (AX=%04X BX=%04X CX=%04X DX=%04X SI=%04X DI=%04X DS=%04X ES=%04X)\n", 0x2f, CPU_AX, CPU_BX, CPU_CX, CPU_DX, CPU_SI, CPU_DI, CPU_DS, CPU_ES);
			CPU_AX = 0x01;
			CPU_SET_C_FLAG(1);
			break;
		}
		break;
	case 0x12:
		if(CPU_CX == 0x4d52 && CPU_DX == 0x4349) {
			// Microsoft Realtime Compression Interface (MRCI) is not installed
		} else if(CPU_CX == 0x5354 && CPU_DX == 0x4143) {
			// Stacker 4 LZS Compression Interface (LZSAPI) is not installed
		} else {
			unimplemented_2fh("int %02Xh (AX=%04X BX=%04X CX=%04X DX=%04X SI=%04X DI=%04X DS=%04X ES=%04X)\n", 0x2f, CPU_AX, CPU_BX, CPU_CX, CPU_DX, CPU_SI, CPU_DI, CPU_DS, CPU_ES);
			CPU_AX = 0x01;
			CPU_SET_C_FLAG(1);
		}
		break;
	case 0x13:
		// DBLSPACE.BIN is not installed
		break;
	default:
		unimplemented_2fh("int %02Xh (AX=%04X BX=%04X CX=%04X DX=%04X SI=%04X DI=%04X DS=%04X ES=%04X)\n", 0x2f, CPU_AX, CPU_BX, CPU_CX, CPU_DX, CPU_SI, CPU_DI, CPU_DS, CPU_ES);
		CPU_AX = 0x01;
		CPU_SET_C_FLAG(1);
		break;
	}
}

inline void msdos_int_2fh_4bh()
{
	switch(CPU_AL) {
	case 0x01:
	case 0x02:
		// Task Switcher is not installed
		break;
	case 0x03:
		// this call is available from within DOSSHELL even if the task switcher is not installed
		CPU_AX = CPU_BX = 0x0000; // no more avaiable switcher id
		break;
	case 0x04:
		CPU_BX = 0x0000; // free switcher id successfully
		break;
	case 0x05:
		CPU_BX = 0x0000; // no instance data chain
		CPU_LOAD_SREG(CPU_ES_INDEX, 0x0000);
		break;
	default:
		unimplemented_2fh("int %02Xh (AX=%04X BX=%04X CX=%04X DX=%04X SI=%04X DI=%04X DS=%04X ES=%04X)\n", 0x2f, CPU_AX, CPU_BX, CPU_CX, CPU_DX, CPU_SI, CPU_DI, CPU_DS, CPU_ES);
		CPU_AX = 0x01;
		CPU_SET_C_FLAG(1);
		break;
	}
}

inline void msdos_int_2fh_4dh()
{
	switch(CPU_AL) {
	case 0x00:
		// KKCFUNC is not installed ???
		break;
	default:
//		unimplemented_2fh("int %02Xh (AX=%04X BX=%04X CX=%04X DX=%04X SI=%04X DI=%04X DS=%04X ES=%04X)\n", 0x2f, CPU_AX, CPU_BX, CPU_CX, CPU_DX, CPU_SI, CPU_DI, CPU_DS, CPU_ES);
		CPU_AX = 0x01; // invalid function
		CPU_SET_C_FLAG(1);
		break;
	}
}

inline void msdos_int_2fh_4fh()
{
	switch(CPU_AL) {
	case 0x00:
		// BILING is installed
		CPU_AX = 0x0000;
		CPU_DL = 0x01;	// major version
		CPU_DH = 0x00;	// minor version
		break;
	case 0x01:
		CPU_AX = 0x0000;
		CPU_BX = active_code_page;
		break;
	default:
		unimplemented_2fh("int %02Xh (AX=%04X BX=%04X CX=%04X DX=%04X SI=%04X DI=%04X DS=%04X ES=%04X)\n", 0x2f, CPU_AX, CPU_BX, CPU_CX, CPU_DX, CPU_SI, CPU_DI, CPU_DS, CPU_ES);
		CPU_AX = 0x01;
		CPU_SET_C_FLAG(1);
		break;
	}
}

inline void msdos_int_2fh_55h()
{
	switch(CPU_AL) {
	case 0x00:
	case 0x01:
//		unimplemented_2fh("int %02Xh (AX=%04X BX=%04X CX=%04X DX=%04X SI=%04X DI=%04X DS=%04X ES=%04X)\n", 0x2f, CPU_AX, CPU_BX, CPU_CX, CPU_DX, CPU_SI, CPU_DI, CPU_DS, CPU_ES);
		break;
	default:
		unimplemented_2fh("int %02Xh (AX=%04X BX=%04X CX=%04X DX=%04X SI=%04X DI=%04X DS=%04X ES=%04X)\n", 0x2f, CPU_AX, CPU_BX, CPU_CX, CPU_DX, CPU_SI, CPU_DI, CPU_DS, CPU_ES);
		CPU_AX = 0x01;
		CPU_SET_C_FLAG(1);
		break;
	}
}

inline void msdos_int_2fh_56h()
{
	switch(CPU_AL) {
	case 0x00:
		// INTERLNK is not installed
		break;
	case 0x01:
		// this call is available from within SCANDISK even if INTERLNK is not installed
//		if(msdos_is_remote_drive(CPU_BH)) {
//			CPU_AL = 0x00;
//		}
		break;
	default:
		unimplemented_2fh("int %02Xh (AX=%04X BX=%04X CX=%04X DX=%04X SI=%04X DI=%04X DS=%04X ES=%04X)\n", 0x2f, CPU_AX, CPU_BX, CPU_CX, CPU_DX, CPU_SI, CPU_DI, CPU_DS, CPU_ES);
		CPU_AX = 0x01;
		CPU_SET_C_FLAG(1);
		break;
	}
}

inline void msdos_int_2fh_adh()
{
	switch(CPU_AL) {
	case 0x00:
		// DISPLAY.SYS is installed
		CPU_AL = 0xff;
		CPU_BX = 0x100; // ???
		break;
	case 0x01:
		active_code_page = CPU_BX;
		msdos_nls_tables_update();
		CPU_AX = 0x01;
		break;
	case 0x02:
		CPU_BX = active_code_page;
		break;
	case 0x03:
		// FIXME
		*(UINT16 *)(mem + CPU_ES_BASE + CPU_DI + 0) = 1;
		*(UINT16 *)(mem + CPU_ES_BASE + CPU_DI + 2) = 3;
		*(UINT16 *)(mem + CPU_ES_BASE + CPU_DI + 4) = 1;
		*(UINT16 *)(mem + CPU_ES_BASE + CPU_DI + 6) = active_code_page;
		*(UINT16 *)(mem + CPU_ES_BASE + CPU_DI + 8) = active_code_page;
		break;
	case 0x80:
		// KEYB.COM is not installed
		break;
	default:
		unimplemented_2fh("int %02Xh (AX=%04X BX=%04X CX=%04X DX=%04X SI=%04X DI=%04X DS=%04X ES=%04X)\n", 0x2f, CPU_AX, CPU_BX, CPU_CX, CPU_DX, CPU_SI, CPU_DI, CPU_DS, CPU_ES);
		CPU_AX = 0x01;
		CPU_SET_C_FLAG(1);
		break;
	}
}

inline void msdos_int_2fh_aeh()
{
	switch(CPU_AL) {
	case 0x00:
		// FIXME: we need to check the given command line
		CPU_AL = 0x00; // the command should be executed as usual
//		CPU_AL = 0xff; // this command is a TSR extension to COMMAND.COM
		break;
	case 0x01:
		{
			char command[MAX_PATH];
			memset(command, 0, sizeof(command));
			memcpy(command, mem + CPU_DS_BASE + CPU_SI + 1, mem[CPU_DS_BASE + CPU_SI]);
			
			param_block_t *param = (param_block_t *)(mem + WORK_TOP);
			param->env_seg = 0;
			param->cmd_line.w.l = 44;
			param->cmd_line.w.h = (WORK_TOP >> 4);
			param->fcb1.w.l = 24;
			param->fcb1.w.h = (WORK_TOP >> 4);
			param->fcb2.w.l = 24;
			param->fcb2.w.h = (WORK_TOP >> 4);
			
			memset(mem + WORK_TOP + 24, 0x20, 20);
			
			cmd_line_t *cmd_line = (cmd_line_t *)(mem + WORK_TOP + 44);
			cmd_line->len = mem[CPU_DS_BASE + CPU_BX + 1];
			memcpy(cmd_line->cmd, mem + CPU_DS_BASE + CPU_BX + 2, cmd_line->len);
			cmd_line->cmd[cmd_line->len] = 0x0d;
			
			try {
				msdos_process_exec(command, param, 0);
			} catch(...) {
				fatalerror("failed to start '%s' by int 2Fh, AX=AE01h\n", command);
			}
		}
		break;
	default:
		unimplemented_2fh("int %02Xh (AX=%04X BX=%04X CX=%04X DX=%04X SI=%04X DI=%04X DS=%04X ES=%04X)\n", 0x2f, CPU_AX, CPU_BX, CPU_CX, CPU_DX, CPU_SI, CPU_DI, CPU_DS, CPU_ES);
		CPU_AX = 0x01;
		CPU_SET_C_FLAG(1);
		break;
	}
}

inline void msdos_int_2fh_b7h()
{
	switch(CPU_AL) {
	case 0x00:
		// APPEND is not installed
//		CPU_AL = 0x00;
		break;
	case 0x06:
		CPU_BX = 0x0000;
		break;
	case 0x07:
	case 0x11:
		// COMMAND.COM calls this service without checking APPEND is installed
		break;
	default:
		unimplemented_2fh("int %02Xh (AX=%04X BX=%04X CX=%04X DX=%04X SI=%04X DI=%04X DS=%04X ES=%04X)\n", 0x2f, CPU_AX, CPU_BX, CPU_CX, CPU_DX, CPU_SI, CPU_DI, CPU_DS, CPU_ES);
		CPU_AX = 0x01;
		CPU_SET_C_FLAG(1);
		break;
	}
}

inline void msdos_int_33h_0000h()
{
	CPU_AX = 0xffff; // hardware/driver installed
	CPU_BX = MAX_MOUSE_BUTTONS;
}

inline void msdos_int_33h_0001h()
{
	if(mouse.hidden > 0) {
		mouse.hidden--;
	}
	if(mouse.hidden == 0) {
		SetConsoleMode(GetStdHandle(STD_INPUT_HANDLE), (dwConsoleMode | ENABLE_MOUSE_INPUT | ENABLE_EXTENDED_FLAGS) & ~ENABLE_QUICK_EDIT_MODE);
		pic[1].imr &= ~0x10; // enable irq12
	}
}

inline void msdos_int_33h_0002h()
{
	mouse.hidden++;
	if(dwConsoleMode & (ENABLE_INSERT_MODE | ENABLE_QUICK_EDIT_MODE)) {
		SetConsoleMode(GetStdHandle(STD_INPUT_HANDLE), dwConsoleMode | ENABLE_EXTENDED_FLAGS);
	} else {
		SetConsoleMode(GetStdHandle(STD_INPUT_HANDLE), dwConsoleMode);
	}
	pic[1].imr |= 0x10; // disable irq12
}

inline void msdos_int_33h_0003h()
{
//	if(mouse.hidden > 0) {
		update_console_input();
//	}
	CPU_BX = mouse.get_buttons();
	CPU_CX = max(mouse.min_position.x & ~7, min(mouse.max_position.x & ~7, mouse.position.x));
	CPU_DX = max(mouse.min_position.y & ~7, min(mouse.max_position.y & ~7, mouse.position.y));
}

inline void msdos_int_33h_0004h()
{
	mouse.position.x = CPU_CX;
	mouse.position.x = CPU_DX;
}

inline void msdos_int_33h_0005h()
{
//	if(mouse.hidden > 0) {
		update_console_input();
//	}
	if(CPU_BX < MAX_MOUSE_BUTTONS) {
		int idx = CPU_BX;
		CPU_BX = min(mouse.buttons[idx].pressed_times, 0x7fff);
		CPU_CX = max(mouse.min_position.x & ~7, min(mouse.max_position.x & ~7, mouse.buttons[idx].pressed_position.x));
		CPU_DX = max(mouse.min_position.y & ~7, min(mouse.max_position.y & ~7, mouse.buttons[idx].pressed_position.y));
		mouse.buttons[idx].pressed_times = 0;
	} else {
		CPU_BX = CPU_CX = CPU_DX = 0x0000;
	}
	CPU_AX = mouse.get_buttons();
}

inline void msdos_int_33h_0006h()
{
//	if(mouse.hidden > 0) {
		update_console_input();
//	}
	if(CPU_BX < MAX_MOUSE_BUTTONS) {
		int idx = CPU_BX;
		CPU_BX = min(mouse.buttons[idx].released_times, 0x7fff);
		CPU_CX = max(mouse.min_position.x & ~7, min(mouse.max_position.x & ~7, mouse.buttons[idx].released_position.x));
		CPU_DX = max(mouse.min_position.y & ~7, min(mouse.max_position.y & ~7, mouse.buttons[idx].released_position.y));
		mouse.buttons[idx].released_times = 0;
	} else {
		CPU_BX = CPU_CX = CPU_DX = 0x0000;
	}
	CPU_AX = mouse.get_buttons();
}

inline void msdos_int_33h_0007h()
{
	mouse.min_position.x = min(CPU_CX, CPU_DX);
	mouse.max_position.x = max(CPU_CX, CPU_DX);
}

inline void msdos_int_33h_0008h()
{
	mouse.min_position.y = min(CPU_CX, CPU_DX);
	mouse.max_position.y = max(CPU_CX, CPU_DX);
}

inline void msdos_int_33h_0009h()
{
	mouse.hot_spot[0] = CPU_BX;
	mouse.hot_spot[1] = CPU_CX;
}

inline void msdos_int_33h_000ah()
{
	mouse.screen_mask = CPU_CX;
	mouse.cursor_mask = CPU_DX;
}

inline void msdos_int_33h_000bh()
{
//	if(mouse.hidden > 0) {
		update_console_input();
//	}
	int dx = (mouse.position.x - mouse.prev_position.x) * mouse.mickey.x / 8;
	int dy = (mouse.position.y - mouse.prev_position.y) * mouse.mickey.y / 8;
	mouse.prev_position.x = mouse.position.x;
	mouse.prev_position.y = mouse.position.y;
	CPU_CX = dx;
	CPU_DX = dy;
}

inline void msdos_int_33h_000ch()
{
	mouse.call_mask = CPU_CX;
	mouse.call_addr.w.l = CPU_DX;
	mouse.call_addr.w.h = CPU_ES;
}

inline void msdos_int_33h_000fh()
{
	mouse.mickey.x = CPU_CX;
	mouse.mickey.y = CPU_DX;
}

inline void msdos_int_33h_0011h()
{
	CPU_AX = 0xffff;
	CPU_BX = MAX_MOUSE_BUTTONS;
}

inline void msdos_int_33h_0014h()
{
	UINT16 old_mask = mouse.call_mask;
	UINT16 old_ofs = mouse.call_addr.w.l;
	UINT16 old_seg = mouse.call_addr.w.h;
	
	mouse.call_mask = CPU_CX;
	mouse.call_addr.w.l = CPU_DX;
	mouse.call_addr.w.h = CPU_ES;
	
	CPU_CX = old_mask;
	CPU_DX = old_ofs;
	CPU_LOAD_SREG(CPU_ES_INDEX, old_seg);
}

inline void msdos_int_33h_0015h()
{
	CPU_BX = sizeof(mouse);
}

inline void msdos_int_33h_0016h()
{
	memcpy(mem + CPU_ES_BASE + CPU_DX, &mouse, sizeof(mouse));
}

inline void msdos_int_33h_0017h()
{
	memcpy(&mouse, mem + CPU_ES_BASE + CPU_DX, sizeof(mouse));
}

inline void msdos_int_33h_0018h()
{
	for(int i = 0; i < 8; i++) {
		if(CPU_CX & (1 << i)) {
			if(mouse.call_addr_alt[i].dw && !(CPU_DX == 0 && CPU_ES == 0)) {
				// event handler already exists
				CPU_AX = 0xffff;
				break;
			}
			mouse.call_addr_alt[i].w.l = CPU_DX;
			mouse.call_addr_alt[i].w.h = CPU_ES;
		}
	}
}

inline void msdos_int_33h_0019h()
{
	UINT16 call_mask = CPU_CX;
	
	CPU_CX = 0;
	
	for(int i = 0; i < 8; i++) {
		if((call_mask & (1 << i)) && mouse.call_addr_alt[i].dw) {
			for(int j = 0; j < 8; j++) {
				if((call_mask & (1 << j)) && mouse.call_addr_alt[i].dw == mouse.call_addr_alt[j].dw) {
					CPU_CX |= (1 << j);
				}
			}
			CPU_DX = mouse.call_addr_alt[i].w.l;
			CPU_BX = mouse.call_addr_alt[i].w.h;
			break;
		}
	}
}

inline void msdos_int_33h_001ah()
{
	mouse.sensitivity[0] = CPU_BX;
	mouse.sensitivity[1] = CPU_CX;
	mouse.sensitivity[2] = CPU_DX;
}

inline void msdos_int_33h_001bh()
{
	CPU_BX = mouse.sensitivity[0];
	CPU_CX = mouse.sensitivity[1];
	CPU_DX = mouse.sensitivity[2];
}

inline void msdos_int_33h_001dh()
{
	mouse.display_page = CPU_BX;
}

inline void msdos_int_33h_001eh()
{
	CPU_BX = mouse.display_page;
}

inline void msdos_int_33h_001fh()
{
	// from DOSBox
	CPU_BX = 0x0000;
	CPU_LOAD_SREG(CPU_ES_INDEX, 0x0000);
	mouse.enabled = false;
	mouse.old_hidden = mouse.hidden;
	mouse.hidden = 1;
}

inline void msdos_int_33h_0020h()
{
	// from DOSBox
	mouse.enabled = true;
	mouse.hidden = mouse.old_hidden;
}

inline void msdos_int_33h_0021h()
{
	CPU_AX = 0xffff;
	CPU_BX = MAX_MOUSE_BUTTONS;
}

inline void msdos_int_33h_0022h()
{
	mouse.language = CPU_BX;
}

inline void msdos_int_33h_0023h()
{
	CPU_BX = mouse.language;
}

inline void msdos_int_33h_0024h()
{
	CPU_BX = 0x0805; // V8.05
	CPU_CX = 0x0400; // PS/2
}

inline void msdos_int_33h_0025h()
{
	CPU_AX = 0x8000; // driver (not TSR), software text cursor
}

inline void msdos_int_33h_0026h()
{
	CPU_BX = 0x0000;
	CPU_CX = mouse.max_position.x;
	CPU_DX = mouse.max_position.y;
}

inline void msdos_int_33h_0027h()
{
//	if(mouse.hidden > 0) {
		update_console_input();
//	}
	int dx = (mouse.position.x - mouse.prev_position.x) * mouse.mickey.x / 8;
	int dy = (mouse.position.y - mouse.prev_position.y) * mouse.mickey.y / 8;
	mouse.prev_position.x = mouse.position.x;
	mouse.prev_position.y = mouse.position.y;
	CPU_AX = mouse.screen_mask;
	CPU_BX = mouse.cursor_mask;
	CPU_CX = dx;
	CPU_DX = dy;
}

inline void msdos_int_33h_0028h()
{
	if(CPU_CX != 0) {
		UINT8 tmp = CPU_AL;
		CPU_AL = CPU_CL;
		pcbios_int_10h_00h();
		CPU_AL = tmp;
	}
	CPU_CL = 0x00; // successful
}

inline void msdos_int_33h_0029h()
{
	switch(CPU_CX) {
	case 0x0000:
		CPU_CX = 0x0003;
		sprintf((char *)(mem + WORK_TOP), "TEXT Mode (80x25)$");
		break;
	case 0x0003:
		CPU_CX = 0x0070;
		sprintf((char *)(mem + WORK_TOP), "V-TEXT Mode (%dx%d)$", scr_width, scr_height);
		break;
	case 0x0070:
		CPU_CX = 0x0071;
		sprintf((char *)(mem + WORK_TOP), "Extended CGA V-TEXT Mode (%dx%d)$", scr_width, scr_height);
		break;
	case 0x0071:
		CPU_CX = 0x0073;
		sprintf((char *)(mem + WORK_TOP), "Extended CGA TEXT Mode (80x25)$");
		break;
	default:
		CPU_CX = 0x0000;
		break;
	}
	if(CPU_CX != 0) {
		CPU_LOAD_SREG(CPU_DS_INDEX, WORK_TOP >> 4);
	} else {
		CPU_LOAD_SREG(CPU_DS_INDEX, 0x0000);
	}
	CPU_DX = 0x0000;
}

inline void msdos_int_33h_002ah()
{
	CPU_AX = -mouse.hidden;
	CPU_BX = mouse.hot_spot[0];
	CPU_CX = mouse.hot_spot[1];
	CPU_DX = 4; // PS/2
}

inline void msdos_int_33h_0031h()
{
	CPU_AX = mouse.min_position.x;
	CPU_BX = mouse.min_position.y;
	CPU_CX = mouse.max_position.x;
	CPU_DX = mouse.max_position.y;
}

inline void msdos_int_33h_0032h()
{
	CPU_AX = 0;
	CPU_AX |= 0x8000; // 0025h
	CPU_AX |= 0x4000; // 0026h
	CPU_AX |= 0x2000; // 0027h
//	CPU_AX |= 0x1000; // 0028h
//	CPU_AX |= 0x0800; // 0029h
	CPU_AX |= 0x0400; // 002ah
//	CPU_AX |= 0x0200; // 002bh
//	CPU_AX |= 0x0100; // 002ch
//	CPU_AX |= 0x0080; // 002dh
//	CPU_AX |= 0x0040; // 002eh
	CPU_AX |= 0x0020; // 002fh
//	CPU_AX |= 0x0010; // 0030h
	CPU_AX |= 0x0008; // 0031h
	CPU_AX |= 0x0004; // 0032h
//	CPU_AX |= 0x0002; // 0033h
//	CPU_AX |= 0x0001; // 0034h
}

inline void msdos_int_33h_004dh()
{
	strcpy((char *)(mem + CPU_ES_BASE + CPU_DI), "Copyright 2017 MS-DOS Player");
}

inline void msdos_int_33h_006dh()
{
	*(UINT8 *)(mem + CPU_ES_BASE + CPU_DI + 0) = 0x08; // V8.05
	*(UINT8 *)(mem + CPU_ES_BASE + CPU_DI + 1) = 0x05;
}

inline void msdos_int_67h_40h()
{
	if(!support_ems) {
		CPU_AH = 0x84;
	} else {
		CPU_AH = 0x00;
	}
}

inline void msdos_int_67h_41h()
{
	if(!support_ems) {
		CPU_AH = 0x84;
	} else {
		CPU_AH = 0x00;
		CPU_BX = EMS_TOP >> 4;
	}
}

inline void msdos_int_67h_42h()
{
	if(!support_ems) {
		CPU_AH = 0x84;
	} else {
		CPU_AH = 0x00;
		CPU_BX = free_ems_pages;
		CPU_DX = MAX_EMS_PAGES;
	}
}

inline void msdos_int_67h_43h()
{
	if(!support_ems) {
		CPU_AH = 0x84;
	} else if(CPU_BX > MAX_EMS_PAGES) {
		CPU_AH = 0x87;
	} else if(CPU_BX > free_ems_pages) {
		CPU_AH = 0x88;
	} else if(CPU_BX == 0) {
		CPU_AH = 0x89;
	} else {
		for(int i = 1; i <= MAX_EMS_HANDLES; i++) {
			if(!ems_handles[i].allocated) {
				ems_allocate_pages(i, CPU_BX);
				CPU_AH = 0x00;
				CPU_DX = i;
				return;
			}
		}
		CPU_AH = 0x85;
	}
}

inline void msdos_int_67h_44h()
{
	if(!support_ems) {
		CPU_AH = 0x84;
	} else if(!(CPU_DX >= 1 && CPU_DX <= MAX_EMS_HANDLES && ems_handles[CPU_DX].allocated)) {
		CPU_AH = 0x83;
	} else if(!(CPU_BX == 0xffff || CPU_BX < ems_handles[CPU_DX].pages)) {
		CPU_AH = 0x8a;
//	} else if(!(CPU_AL < 4)) {
//		CPU_AH = 0x8b;
	} else if(CPU_BX == 0xffff) {
		ems_unmap_page(CPU_AL & 3);
		CPU_AH = 0x00;
	} else {
		ems_map_page(CPU_AL & 3, CPU_DX, CPU_BX);
		CPU_AH = 0x00;
	}
}

inline void msdos_int_67h_45h()
{
	if(!support_ems) {
		CPU_AH = 0x84;
	} else if(!(CPU_DX >= 1 && CPU_DX <= MAX_EMS_HANDLES && ems_handles[CPU_DX].allocated)) {
		CPU_AH = 0x83;
	} else {
		ems_release_pages(CPU_DX);
		CPU_AH = 0x00;
	}
}

inline void msdos_int_67h_46h()
{
	if(!support_ems) {
		CPU_AH = 0x84;
	} else {
//		CPU_AX = 0x0032; // EMS 3.2
		CPU_AX = 0x0040; // EMS 4.0
	}
}

inline void msdos_int_67h_47h()
{
	// NOTE: the map data should be stored in the specified ems page, not process data
	process_t *process = msdos_process_info_get(current_psp);
	
	if(!support_ems) {
		CPU_AH = 0x84;
//	} else if(!(CPU_DX >= 1 && CPU_DX <= MAX_EMS_HANDLES && ems_handles[CPU_DX].allocated)) {
//		CPU_AH = 0x83;
	} else if(process->ems_pages_stored) {
		CPU_AH = 0x8d;
	} else {
		for(int i = 0; i < 4; i++) {
			process->ems_pages[i].handle = ems_pages[i].handle;
			process->ems_pages[i].page   = ems_pages[i].page;
			process->ems_pages[i].mapped = ems_pages[i].mapped;
		}
		process->ems_pages_stored = true;
		CPU_AH = 0x00;
	}
}

inline void msdos_int_67h_48h()
{
	// NOTE: the map data should be restored from the specified ems page, not process data
	process_t *process = msdos_process_info_get(current_psp);
	
	if(!support_ems) {
		CPU_AH = 0x84;
//	} else if(!(CPU_DX >= 1 && CPU_DX <= MAX_EMS_HANDLES && ems_handles[CPU_DX].allocated)) {
//		CPU_AH = 0x83;
	} else if(!process->ems_pages_stored) {
		CPU_AH = 0x8e;
	} else {
		for(int i = 0; i < 4; i++) {
			if(process->ems_pages[i].mapped) {
				ems_map_page(i, process->ems_pages[i].handle, process->ems_pages[i].page);
			} else {
				ems_unmap_page(i);
			}
		}
		process->ems_pages_stored = false;
		CPU_AH = 0x00;
	}
}

inline void msdos_int_67h_4bh()
{
	if(!support_ems) {
		CPU_AH = 0x84;
	} else {
		CPU_AH = 0x00;
		CPU_BX = 0;
		for(int i = 1; i <= MAX_EMS_HANDLES; i++) {
			if(ems_handles[i].allocated) {
				CPU_BX++;
			}
		}
	}
}

inline void msdos_int_67h_4ch()
{
	if(!support_ems) {
		CPU_AH = 0x84;
	} else if(!(CPU_DX >= 1 && CPU_DX <= MAX_EMS_HANDLES && ems_handles[CPU_DX].allocated)) {
		CPU_AH = 0x83;
	} else {
		CPU_AH = 0x00;
		CPU_BX = ems_handles[CPU_DX].pages;
	}
}

inline void msdos_int_67h_4dh()
{
	if(!support_ems) {
		CPU_AH = 0x84;
	} else {
		CPU_AH = 0x00;
		CPU_BX = 0;
		for(int i = 1; i <= MAX_EMS_HANDLES; i++) {
			if(ems_handles[i].allocated) {
				*(UINT16 *)(mem + CPU_ES_BASE + CPU_DI + 4 * CPU_BX + 0) = i;
				*(UINT16 *)(mem + CPU_ES_BASE + CPU_DI + 4 * CPU_BX + 2) = ems_handles[i].pages;
				CPU_BX++;
			}
		}
	}
}

inline void msdos_int_67h_4eh()
{
	if(!support_ems) {
		CPU_AH = 0x84;
	} else if(CPU_AL == 0x00 || CPU_AL == 0x01 || CPU_AL == 0x02) {
		if(CPU_AL == 0x00 || CPU_AL == 0x02) {
			// save page map
			for(int i = 0; i < 4; i++) {
				*(UINT16 *)(mem + CPU_ES_BASE + CPU_DI + 4 * i + 0) = ems_pages[i].mapped ? ems_pages[i].handle : 0xffff;
				*(UINT16 *)(mem + CPU_ES_BASE + CPU_DI + 4 * i + 2) = ems_pages[i].mapped ? ems_pages[i].page   : 0xffff;
			}
		}
		if(CPU_AL == 0x01 || CPU_AL == 0x02) {
			// restore page map
			for(int i = 0; i < 4; i++) {
				UINT16 handle = *(UINT16 *)(mem + CPU_DS_BASE + CPU_SI + 4 * i + 0);
				UINT16 page   = *(UINT16 *)(mem + CPU_DS_BASE + CPU_SI + 4 * i + 2);
				
				if(handle >= 1 && handle <= MAX_EMS_HANDLES && ems_handles[handle].allocated && page < ems_handles[handle].pages) {
					ems_map_page(i, handle, page);
				} else {
					ems_unmap_page(i);
				}
			}
		}
		CPU_AH = 0x00;
	} else if(CPU_AL == 0x03) {
		CPU_AH = 0x00;
		CPU_AL = 4 * 4;
	} else {
		unimplemented_67h("int %02Xh (AX=%04X BX=%04X CX=%04X DX=%04X SI=%04X DI=%04X DS=%04X ES=%04X)\n", 0x67, CPU_AX, CPU_BX, CPU_CX, CPU_DX, CPU_SI, CPU_DI, CPU_DS, CPU_ES);
		CPU_AH = 0x8f;
	}
}

inline void msdos_int_67h_4fh()
{
	if(!support_ems) {
		CPU_AH = 0x84;
	} else if(CPU_AL == 0x00) {
		int count = *(UINT16 *)(mem + CPU_DS_BASE + CPU_SI);
		
		*(UINT16 *)(mem + CPU_ES_BASE + CPU_DI) = count;
		for(int i = 0; i < count; i++) {
			UINT16 segment  = *(UINT16 *)(mem + CPU_DS_BASE + CPU_SI + 2 + 2 * i);
			UINT16 physical = ((segment << 4) - EMS_TOP) / 0x4000;
			
//			if(!(physical < 4)) {
//				CPU_AH = 0x8b;
//				return;
//			}
			*(UINT16 *)(mem + CPU_ES_BASE + CPU_DI + 2 + 6 * i + 0) = segment;
			*(UINT16 *)(mem + CPU_ES_BASE + CPU_DI + 2 + 6 * i + 2) = ems_pages[physical & 3].mapped ? ems_pages[physical & 3].handle : 0xffff;
			*(UINT16 *)(mem + CPU_ES_BASE + CPU_DI + 2 + 6 * i + 4) = ems_pages[physical & 3].mapped ? ems_pages[physical & 3].page   : 0xffff;
		}
		CPU_AH = 0x00;
	} else if(CPU_AL == 0x01) {
		int count = *(UINT16 *)(mem + CPU_DS_BASE + CPU_SI);
		
		for(int i = 0; i < count; i++) {
			UINT16 segment  = *(UINT16 *)(mem + CPU_DS_BASE + CPU_SI + 2 + 6 * i + 0);
			UINT16 physical = ((segment << 4) - EMS_TOP) / 0x4000;
			UINT16 handle   = *(UINT16 *)(mem + CPU_DS_BASE + CPU_SI + 2 + 6 * i + 2);
			UINT16 logical  = *(UINT16 *)(mem + CPU_DS_BASE + CPU_SI + 2 + 6 * i + 4);
			
//			if(!(physical < 4)) {
//				CPU_AH = 0x8b;
//				return;
//			} else
			if(handle >= 1 && handle <= MAX_EMS_HANDLES && ems_handles[handle].allocated && logical < ems_handles[handle].pages) {
				ems_map_page(physical & 3, handle, logical);
			} else {
				ems_unmap_page(physical & 3);
			}
		}
		CPU_AH = 0x00;
	} else if(CPU_AL == 0x02) {
		CPU_AH = 0x00;
		CPU_AL = 2 + CPU_BX * 6;
	} else {
		unimplemented_67h("int %02Xh (AX=%04X BX=%04X CX=%04X DX=%04X SI=%04X DI=%04X DS=%04X ES=%04X)\n", 0x67, CPU_AX, CPU_BX, CPU_CX, CPU_DX, CPU_SI, CPU_DI, CPU_DS, CPU_ES);
		CPU_AH = 0x8f;
	}
}

inline void msdos_int_67h_50h()
{
	if(!support_ems) {
		CPU_AH = 0x84;
	} else if(!(CPU_DX >= 1 && CPU_DX <= MAX_EMS_HANDLES && ems_handles[CPU_DX].allocated)) {
		CPU_AH = 0x83;
	} else if(CPU_AL == 0x00 || CPU_AL == 0x01) {
		for(int i = 0; i < CPU_CX; i++) {
			int logical  = *(UINT16 *)(mem + CPU_DS_BASE + CPU_SI + 4 * i + 0);
			int physical = *(UINT16 *)(mem + CPU_DS_BASE + CPU_SI + 4 * i + 2);
			
			if(CPU_AL == 0x01) {
				physical = ((physical << 4) - EMS_TOP) / 0x4000;
			}
//			if(!(physical < 4)) {
//				CPU_AH = 0x8b;
//				return;
//			} else
			if(logical == 0xffff) {
				ems_unmap_page(physical & 3);
			} else if(logical < ems_handles[CPU_DX].pages) {
				ems_map_page(physical & 3, CPU_DX, logical);
			} else {
				CPU_AH = 0x8a;
				return;
			}
		}
		CPU_AH = 0x00;
	} else {
		unimplemented_67h("int %02Xh (AX=%04X BX=%04X CX=%04X DX=%04X SI=%04X DI=%04X DS=%04X ES=%04X)\n", 0x67, CPU_AX, CPU_BX, CPU_CX, CPU_DX, CPU_SI, CPU_DI, CPU_DS, CPU_ES);
		CPU_AH = 0x8f;
	}
}

inline void msdos_int_67h_51h()
{
	if(!support_ems) {
		CPU_AH = 0x84;
	} else if(!(CPU_DX >= 1 && CPU_DX <= MAX_EMS_HANDLES && ems_handles[CPU_DX].allocated)) {
		CPU_AH = 0x83;
	} else if(CPU_BX > MAX_EMS_PAGES) {
		CPU_AH = 0x87;
	} else if(CPU_BX > free_ems_pages + ems_handles[CPU_DX].pages) {
		CPU_AH = 0x88;
	} else {
		ems_reallocate_pages(CPU_DX, CPU_BX);
		CPU_AH = 0x00;
	}
}

inline void msdos_int_67h_52h()
{
	if(!support_ems) {
		CPU_AH = 0x84;
//	} else if(!(CPU_DX >= 1 && CPU_DX <= MAX_EMS_HANDLES && ems_handles[CPU_DX].allocated)) {
//		CPU_AH = 0x83;
	} else if(CPU_AL == 0x00) {
		CPU_AL = 0x00; // handle is volatile
		CPU_AH = 0x00;
	} else if(CPU_AL == 0x01) {
		if(CPU_BL == 0x00) {
			CPU_AH = 0x00;
		} else {
			CPU_AH = 0x90; // undefined attribute type
		}
	} else if(CPU_AL == 0x02) {
		CPU_AL = 0x00; // only volatile handles supported
		CPU_AH = 0x00;
	} else {
		unimplemented_67h("int %02Xh (AX=%04X BX=%04X CX=%04X DX=%04X SI=%04X DI=%04X DS=%04X ES=%04X)\n", 0x67, CPU_AX, CPU_BX, CPU_CX, CPU_DX, CPU_SI, CPU_DI, CPU_DS, CPU_ES);
		CPU_AH = 0x8f;
	}
}

inline void msdos_int_67h_53h()
{
	if(!support_ems) {
		CPU_AH = 0x84;
	} else if(!(CPU_DX >= 1 && CPU_DX <= MAX_EMS_HANDLES && ems_handles[CPU_DX].allocated)) {
		CPU_AH = 0x83;
	} else if(CPU_AL == 0x00) {
		memcpy(mem + CPU_ES_BASE + CPU_DI, ems_handles[CPU_DX].name, 8);
		CPU_AH = 0x00;
	} else if(CPU_AL == 0x01) {
		for(int i = 1; i <= MAX_EMS_HANDLES; i++) {
			if(ems_handles[i].allocated && memcmp(ems_handles[CPU_DX].name, mem + CPU_DS_BASE + CPU_SI, 8) == 0) {
				CPU_AH = 0xa1;
				return;
			}
		}
		CPU_AH = 0x00;
		memcpy(ems_handles[CPU_DX].name, mem + CPU_DS_BASE + CPU_SI, 8);
	} else {
		unimplemented_67h("int %02Xh (AX=%04X BX=%04X CX=%04X DX=%04X SI=%04X DI=%04X DS=%04X ES=%04X)\n", 0x67, CPU_AX, CPU_BX, CPU_CX, CPU_DX, CPU_SI, CPU_DI, CPU_DS, CPU_ES);
		CPU_AH = 0x8f;
	}
}

inline void msdos_int_67h_54h()
{
	if(!support_ems) {
		CPU_AH = 0x84;
	} else if(CPU_AL == 0x00) {
		for(int i = 1; i <= MAX_EMS_HANDLES; i++) {
			if(ems_handles[i].allocated) {
				memcpy(mem + CPU_ES_BASE + CPU_DI + 10 * i + 2, ems_handles[i].name, 10);
			} else {
				memset(mem + CPU_ES_BASE + CPU_DI + 10 * i + 2, 0, 10);
			}
			*(UINT16 *)(mem + CPU_ES_BASE + CPU_DI + 10 * i + 0) = i;
		}
		CPU_AH = 0x00;
		CPU_AL = MAX_EMS_HANDLES;
	} else if(CPU_AL == 0x01) {
		CPU_AH = 0xa0; // not found
		for(int i = 1; i <= MAX_EMS_HANDLES; i++) {
			if(ems_handles[i].allocated && memcmp(ems_handles[CPU_DX].name, mem + CPU_DS_BASE + CPU_SI, 8) == 0) {
				CPU_AH = 0x00;
				CPU_DX = i;
				break;
			}
		}
	} else if(CPU_AL == 0x02) {
		CPU_AH = 0x00;
		CPU_BX = MAX_EMS_HANDLES;
	} else {
		unimplemented_67h("int %02Xh (AX=%04X BX=%04X CX=%04X DX=%04X SI=%04X DI=%04X DS=%04X ES=%04X)\n", 0x67, CPU_AX, CPU_BX, CPU_CX, CPU_DX, CPU_SI, CPU_DI, CPU_DS, CPU_ES);
		CPU_AH = 0x8f;
	}
}

inline void msdos_int_67h_55h()
{
	if(!support_ems) {
		CPU_AH = 0x84;
	} else if(!(CPU_DX >= 1 && CPU_DX <= MAX_EMS_HANDLES && ems_handles[CPU_DX].allocated)) {
		CPU_AH = 0x83;
	} else if(CPU_AL == 0x00 || CPU_AL == 0x01) {
		UINT16 jump_ofs = *(UINT16 *)(mem + CPU_DS_BASE + CPU_SI + 0);
		UINT16 jump_seg = *(UINT16 *)(mem + CPU_DS_BASE + CPU_SI + 2);
		UINT8  entries  = *(UINT8  *)(mem + CPU_DS_BASE + CPU_SI + 4);
		UINT16 map_ofs  = *(UINT16 *)(mem + CPU_DS_BASE + CPU_SI + 5);
		UINT16 map_seg  = *(UINT16 *)(mem + CPU_DS_BASE + CPU_SI + 7);
		
		for(int i = 0; i < (int)entries; i++) {
			int logical  = *(UINT16 *)(mem + (map_seg << 4) + map_ofs + 4 * i + 0);
			int physical = *(UINT16 *)(mem + (map_seg << 4) + map_ofs + 4 * i + 2);
			
			if(CPU_AL == 0x01) {
				physical = ((physical << 4) - EMS_TOP) / 0x4000;
			}
//			if(!(physical < 4)) {
//				CPU_AH = 0x8b;
//				return;
//			} else
			if(logical == 0xffff) {
				ems_unmap_page(physical & 3);
			} else if(logical < ems_handles[CPU_DX].pages) {
				ems_map_page(physical & 3, CPU_DX, logical);
			} else {
				CPU_AH = 0x8a;
				return;
			}
		}
		CPU_JMP_FAR(jump_seg, jump_ofs);
		CPU_AH = 0x00;
	} else {
		unimplemented_67h("int %02Xh (AX=%04X BX=%04X CX=%04X DX=%04X SI=%04X DI=%04X DS=%04X ES=%04X)\n", 0x67, CPU_AX, CPU_BX, CPU_CX, CPU_DX, CPU_SI, CPU_DI, CPU_DS, CPU_ES);
		CPU_AH = 0x8f;
	}
}

inline void msdos_int_67h_56h()
{
	if(!support_ems) {
		CPU_AH = 0x84;
	} else if(CPU_AL == 0x02) {
		CPU_BX = (2 + 2) * 4;
		CPU_AH = 0x00;
	} else if(!(CPU_DX >= 1 && CPU_DX <= MAX_EMS_HANDLES && ems_handles[CPU_DX].allocated)) {
		CPU_AH = 0x83;
	} else if(CPU_AL == 0x00 || CPU_AL == 0x01) {
		UINT16 call_ofs    = *(UINT16 *)(mem + CPU_DS_BASE + CPU_SI +  0);
		UINT16 call_seg    = *(UINT16 *)(mem + CPU_DS_BASE + CPU_SI +  2);
		UINT8  new_entries = *(UINT8  *)(mem + CPU_DS_BASE + CPU_SI +  4);
		UINT16 new_map_ofs = *(UINT16 *)(mem + CPU_DS_BASE + CPU_SI +  5);
		UINT16 new_map_seg = *(UINT16 *)(mem + CPU_DS_BASE + CPU_SI +  7);
#if 0
		UINT8  old_entries = *(UINT8  *)(mem + CPU_DS_BASE + CPU_SI +  9);
		UINT16 old_map_ofs = *(UINT16 *)(mem + CPU_DS_BASE + CPU_SI + 10);
		UINT16 old_map_seg = *(UINT16 *)(mem + CPU_DS_BASE + CPU_SI + 12);
#endif
		UINT16 handles[4], pages[4];
		
		// alter page map and call routine is at fffc:001f
		if(!(call_seg == 0 && call_ofs == 0)) {
			mem[DUMMY_TOP + 0x1f] = 0x9a;	// call far
			mem[DUMMY_TOP + 0x20] = (call_ofs >> 0) & 0xff;
			mem[DUMMY_TOP + 0x21] = (call_ofs >> 8) & 0xff;
			mem[DUMMY_TOP + 0x22] = (call_seg >> 0) & 0xff;
			mem[DUMMY_TOP + 0x23] = (call_seg >> 8) & 0xff;
		} else {
			// invalid call addr :-(
			mem[DUMMY_TOP + 0x1f] = 0x90;	// nop
			mem[DUMMY_TOP + 0x20] = 0x90;	// nop
			mem[DUMMY_TOP + 0x21] = 0x90;	// nop
			mem[DUMMY_TOP + 0x22] = 0x90;	// nop
			mem[DUMMY_TOP + 0x23] = 0x90;	// nop
		}
		// do call far (push cs/ip) in old mapping
		CPU_CALL_FAR(DUMMY_TOP >> 4, 0x001f);
		
		// get old mapping data
#if 0
		for(int i = 0; i < (int)old_entries; i++) {
			int logical  = *(UINT16 *)(mem + (old_map_seg << 4) + old_map_ofs + 4 * i + 0);
			int physical = *(UINT16 *)(mem + (old_map_seg << 4) + old_map_ofs + 4 * i + 2);
			
			if(CPU_AL == 0x01) {
				physical = ((physical << 4) - EMS_TOP) / 0x4000;
			}
//			if(!(physical < 4)) {
//				CPU_AH = 0x8b;
//				return;
//			} else
			if(logical == 0xffff) {
				ems_unmap_page(physical & 3);
			} else if(logical < ems_handles[CPU_DX].pages) {
				ems_map_page(physical & 3, CPU_DX, logical);
			} else {
				CPU_AH = 0x8a;
				return;
			}
		}
#endif
		for(int i = 0; i < 4; i++) {
			handles[i] = ems_pages[i].mapped ? ems_pages[i].handle : 0xffff;
			pages  [i] = ems_pages[i].mapped ? ems_pages[i].page   : 0xffff;
		}
		
		// set new mapping
		for(int i = 0; i < (int)new_entries; i++) {
			int logical  = *(UINT16 *)(mem + (new_map_seg << 4) + new_map_ofs + 4 * i + 0);
			int physical = *(UINT16 *)(mem + (new_map_seg << 4) + new_map_ofs + 4 * i + 2);
			
			if(CPU_AL == 0x01) {
				physical = ((physical << 4) - EMS_TOP) / 0x4000;
			}
//			if(!(physical < 4)) {
//				CPU_AH = 0x8b;
//				return;
//			} else
			if(logical == 0xffff) {
				ems_unmap_page(physical & 3);
			} else if(logical < ems_handles[CPU_DX].pages) {
				ems_map_page(physical & 3, CPU_DX, logical);
			} else {
				CPU_AH = 0x8a;
				return;
			}
		}
		
		// push old mapping data in new mapping
		for(int i = 0; i < 4; i++) {
			CPU_PUSH(handles[i]);
			CPU_PUSH(pages  [i]);
		}
		CPU_AH = 0x00;
	} else {
		unimplemented_67h("int %02Xh (AX=%04X BX=%04X CX=%04X DX=%04X SI=%04X DI=%04X DS=%04X ES=%04X)\n", 0x67, CPU_AX, CPU_BX, CPU_CX, CPU_DX, CPU_SI, CPU_DI, CPU_DS, CPU_ES);
		CPU_AH = 0x8f;
	}
}

inline void msdos_int_67h_57h_tmp()
{
	UINT32 copy_length = *(UINT32 *)(mem + CPU_DS_BASE + CPU_SI + 0x00);
	UINT8  src_type    = *(UINT8  *)(mem + CPU_DS_BASE + CPU_SI + 0x04);
	UINT16 src_handle  = *(UINT16 *)(mem + CPU_DS_BASE + CPU_SI + 0x05);
	UINT16 src_ofs     = *(UINT16 *)(mem + CPU_DS_BASE + CPU_SI + 0x07);
	UINT16 src_seg     = *(UINT16 *)(mem + CPU_DS_BASE + CPU_SI + 0x09);
	UINT8  dest_type   = *(UINT8  *)(mem + CPU_DS_BASE + CPU_SI + 0x0b);
	UINT16 dest_handle = *(UINT16 *)(mem + CPU_DS_BASE + CPU_SI + 0x0c);
	UINT16 dest_ofs    = *(UINT16 *)(mem + CPU_DS_BASE + CPU_SI + 0x0e);
	UINT16 dest_seg    = *(UINT16 *)(mem + CPU_DS_BASE + CPU_SI + 0x10);
	
	UINT8 *src_buffer = NULL, *dest_buffer = NULL;
	UINT32 src_addr, dest_addr;
	UINT32 src_addr_max, dest_addr_max;
	
	if(src_type == 0) {
		src_buffer = mem;
		src_addr = (src_seg << 4) + src_ofs;
		src_addr_max = MAX_MEM;
	} else {
		if(!(src_handle >= 1 && src_handle <= MAX_EMS_HANDLES && ems_handles[src_handle].allocated)) {
			CPU_AH = 0x83;
			return;
		} else if(!(src_seg < ems_handles[src_handle].pages)) {
			CPU_AH = 0x8a;
			return;
		}
		if(ems_handles[src_handle].buffer != NULL) {
			src_buffer = ems_handles[src_handle].buffer + 0x4000 * src_seg;
		}
		src_addr = src_ofs;
		src_addr_max = 0x4000 * (ems_handles[src_handle].pages - src_seg);
	}
	if(dest_type == 0) {
		dest_buffer = mem;
		dest_addr = (dest_seg << 4) + dest_ofs;
		dest_addr_max = MAX_MEM;
	} else {
		if(!(dest_handle >= 1 && dest_handle <= MAX_EMS_HANDLES && ems_handles[dest_handle].allocated)) {
			CPU_AH = 0x83;
			return;
		} else if(!(dest_seg < ems_handles[dest_handle].pages)) {
			CPU_AH = 0x8a;
			return;
		}
		if(ems_handles[dest_handle].buffer != NULL) {
			dest_buffer = ems_handles[dest_handle].buffer + 0x4000 * dest_seg;
		}
		dest_addr = dest_ofs;
		dest_addr_max = 0x4000 * (ems_handles[dest_handle].pages - dest_seg);
	}
	if(src_buffer != NULL && dest_buffer != NULL) {
		for(int i = 0; i < copy_length; i++) {
			if(src_addr < src_addr_max && dest_addr < dest_addr_max) {
				if(CPU_AL == 0x00) {
					dest_buffer[dest_addr++] = src_buffer[src_addr++];
				} else if(CPU_AL == 0x01) {
					UINT8 tmp = dest_buffer[dest_addr];
					dest_buffer[dest_addr++] = src_buffer[src_addr];
					src_buffer[src_addr++] = tmp;
				}
			} else {
				CPU_AH = 0x93;
				return;
			}
		}
		CPU_AH = 0x00;
	} else {
		CPU_AH = 0x80;
	}
}

inline void msdos_int_67h_57h()
{
	if(!support_ems) {
		CPU_AH = 0x84;
	} else if(CPU_AL == 0x00 || CPU_AL == 0x01) {
		struct {
			UINT16 handle;
			UINT16 page;
			bool mapped;
		} tmp_pages[4];
		
		// unmap pages to copy memory data to ems buffer
		for(int i = 0; i < 4; i++) {
			tmp_pages[i].handle = ems_pages[i].handle;
			tmp_pages[i].page   = ems_pages[i].page;
			tmp_pages[i].mapped = ems_pages[i].mapped;
			ems_unmap_page(i);
		}
		
		// run move/exchange operation
		msdos_int_67h_57h_tmp();
		
		// restore unmapped pages
		for(int i = 0; i < 4; i++) {
			if(tmp_pages[i].mapped) {
				ems_map_page(i, tmp_pages[i].handle, tmp_pages[i].page);
			}
		}
	} else {
		unimplemented_67h("int %02Xh (AX=%04X BX=%04X CX=%04X DX=%04X SI=%04X DI=%04X DS=%04X ES=%04X)\n", 0x67, CPU_AX, CPU_BX, CPU_CX, CPU_DX, CPU_SI, CPU_DI, CPU_DS, CPU_ES);
		CPU_AH = 0x8f;
	}
}

inline void msdos_int_67h_58h()
{
	if(!support_ems) {
		CPU_AH = 0x84;
	} else if(CPU_AL == 0x00) {
		for(int i = 0; i < 4; i++) {
			*(UINT16 *)(mem + CPU_ES_BASE + CPU_DI + 4 * i + 0) = (EMS_TOP + 0x4000 * i) >> 4;
			*(UINT16 *)(mem + CPU_ES_BASE + CPU_DI + 4 * i + 2) = i;
		}
		CPU_AH = 0x00;
		CPU_CX = 4;
	} else if(CPU_AL == 0x01) {
		CPU_AH = 0x00;
		CPU_CX = 4;
	} else {
		unimplemented_67h("int %02Xh (AX=%04X BX=%04X CX=%04X DX=%04X SI=%04X DI=%04X DS=%04X ES=%04X)\n", 0x67, CPU_AX, CPU_BX, CPU_CX, CPU_DX, CPU_SI, CPU_DI, CPU_DS, CPU_ES);
		CPU_AH = 0x8f;
	}
}

inline void msdos_int_67h_59h()
{
	if(!support_ems) {
		CPU_AH = 0x84;
	} else if(CPU_AL == 0x00) {
		*(UINT16 *)(mem + CPU_ES_BASE + CPU_DI + 0) = 1024;
		*(UINT16 *)(mem + CPU_ES_BASE + CPU_DI + 2) = 0;
		*(UINT16 *)(mem + CPU_ES_BASE + CPU_DI + 4) = 4 * 4;
		*(UINT16 *)(mem + CPU_ES_BASE + CPU_DI + 6) = 0;
		CPU_AH = 0x00;
//		CPU_AH = 0xa4; // access denied by operating system
	} else if(CPU_AL == 0x01) {
		CPU_AH = 0x00;
		CPU_BX = free_ems_pages;
		CPU_DX = MAX_EMS_PAGES;
	} else {
		unimplemented_67h("int %02Xh (AX=%04X BX=%04X CX=%04X DX=%04X SI=%04X DI=%04X DS=%04X ES=%04X)\n", 0x67, CPU_AX, CPU_BX, CPU_CX, CPU_DX, CPU_SI, CPU_DI, CPU_DS, CPU_ES);
		CPU_AH = 0x8f;
	}
}

inline void msdos_int_67h_5ah()
{
	if(!support_ems) {
		CPU_AH = 0x84;
	} else if(CPU_BX > MAX_EMS_PAGES) {
		CPU_AH = 0x87;
	} else if(CPU_BX > free_ems_pages) {
		CPU_AH = 0x88;
//	} else if(CPU_BX == 0) {
//		CPU_AH = 0x89;
	} else if(CPU_AL == 0x00 || CPU_AL == 0x01) {
		for(int i = 1; i <= MAX_EMS_HANDLES; i++) {
			if(!ems_handles[i].allocated) {
				ems_allocate_pages(i, CPU_BX);
				CPU_AH = 0x00;
				CPU_DX = i;
				return;
			}
		}
		CPU_AH = 0x85;
	} else {
		unimplemented_67h("int %02Xh (AX=%04X BX=%04X CX=%04X DX=%04X SI=%04X DI=%04X DS=%04X ES=%04X)\n", 0x67, CPU_AX, CPU_BX, CPU_CX, CPU_DX, CPU_SI, CPU_DI, CPU_DS, CPU_ES);
		CPU_AH = 0x8f;
	}
}

inline void msdos_int_67h_5bh()
{
	static UINT8  stored_bl = 0x00;
	static UINT16 stored_es = 0x0000;
	static UINT16 stored_di = 0x0000;
	
	if(!support_ems) {
		CPU_AH = 0x84;
	} else if(CPU_AL == 0x00) {
		if(stored_bl == 0x00) {
			if(!(stored_es == 0 && stored_di == 0)) {
				for(int i = 0; i < 4; i++) {
					*(UINT16 *)(mem + (stored_es << 4) + stored_di + 4 * i + 0) = ems_pages[i].mapped ? ems_pages[i].handle : 0xffff;
					*(UINT16 *)(mem + (stored_es << 4) + stored_di + 4 * i + 2) = ems_pages[i].mapped ? ems_pages[i].page   : 0xffff;
				}
			}
			CPU_LOAD_SREG(CPU_ES_INDEX, stored_es);
			CPU_DI = stored_di;
		} else {
			CPU_BL = stored_bl;
		}
		CPU_AH = 0x00;
	} else if(CPU_AL == 0x01) {
		if(CPU_BL == 0x00) {
			if(!(CPU_ES == 0 && CPU_DI == 0)) {
				for(int i = 0; i < 4; i++) {
					UINT16 handle = *(UINT16 *)(mem + CPU_ES_BASE + CPU_DI + 4 * i + 0);
					UINT16 page   = *(UINT16 *)(mem + CPU_ES_BASE + CPU_DI + 4 * i + 2);
					
					if(handle >= 1 && handle <= MAX_EMS_HANDLES && ems_handles[handle].allocated && page < ems_handles[handle].pages) {
						ems_map_page(i, handle, page);
					} else {
						ems_unmap_page(i);
					}
				}
			}
		}
		stored_bl = CPU_BL;
		stored_es = CPU_ES;
		stored_di = CPU_DI;
		CPU_AH = 0x00;
	} else if(CPU_AL == 0x02) {
		CPU_DX = 4 * 4;
		CPU_AH = 0x00;
	} else if(CPU_AL == 0x03) {
		CPU_BL = 0x00; // not supported
		CPU_AH = 0x00;
	} else if(CPU_AL == 0x04) {
		CPU_AH = 0x00;
	} else if(CPU_AL == 0x05) {
		CPU_BL = 0x00; // not supported
		CPU_AH = 0x00;
	} else {
		unimplemented_67h("int %02Xh (AX=%04X BX=%04X CX=%04X DX=%04X SI=%04X DI=%04X DS=%04X ES=%04X)\n", 0x67, CPU_AX, CPU_BX, CPU_CX, CPU_DX, CPU_SI, CPU_DI, CPU_DS, CPU_ES);
		CPU_AH = 0x8f;
	}
}

inline void msdos_int_67h_5dh()
{
	if(!support_ems) {
		CPU_AH = 0x84;
	} else if(CPU_AL == 0x00 || CPU_AL == 0x01 || CPU_AL == 0x02) {
		CPU_AH = 0xa4; // operating system denied access
	} else {
		unimplemented_67h("int %02Xh (AX=%04X BX=%04X CX=%04X DX=%04X SI=%04X DI=%04X DS=%04X ES=%04X)\n", 0x67, CPU_AX, CPU_BX, CPU_CX, CPU_DX, CPU_SI, CPU_DI, CPU_DS, CPU_ES);
		CPU_AH = 0x8f;
	}
}

inline void msdos_int_67h_70h()
{
	if(!support_ems) {
		CPU_AH = 0x84;
	} else if(CPU_AL == 0x00) {
		CPU_AL = 0x00;
		CPU_AH = 0x00;
	} else if(CPU_AL == 0x01) {
		CPU_AL = 0x00;
//		CPU_AH = (CPU_BL == 0x00) ? 0x00 : 0x80;
		CPU_AH = 0x00;
	} else {
		unimplemented_67h("int %02Xh (AX=%04X BX=%04X CX=%04X DX=%04X SI=%04X DI=%04X DS=%04X ES=%04X)\n", 0x67, CPU_AX, CPU_BX, CPU_CX, CPU_DX, CPU_SI, CPU_DI, CPU_DS, CPU_ES);
		CPU_AH = 0x8f;
	}
}

inline void msdos_int_67h_deh()
{
#if defined(SUPPORT_VCPI)
	// from DOSBox INT67_Handler(void) in inst/ems.cpp
	if(!support_ems) {
		CPU_AH = 0x84;
	} else if(CPU_AL == 0x00) {
		CPU_AH = 0x00;
		CPU_BX = 0x0100;
	} else if(CPU_AL == 0x01) {
		CPU_AH = 0x00;
		// from DOSBox
//		for(int ct = 0; ct < 0xff; ct++) {
		for(int ct = 0; ct < 0x100; ct++) {
			*(UINT8  *)(mem + CPU_ES_BASE + CPU_DI + ct * 4 + 0x00) = 0x67;		// access bits
			*(UINT16 *)(mem + CPU_ES_BASE + CPU_DI + ct * 4 + 0x01) = ct * 0x10;	// mapping
			*(UINT8  *)(mem + CPU_ES_BASE + CPU_DI + ct * 4 + 0x03) = 0x00;
		}
//		for(int ct = 0xff; ct < 0x100; ct++) {
//			*(UINT8  *)(mem + CPU_ES_BASE + CPU_DI + ct * 4 + 0x00) = 0x67;		// access bits
//			*(UINT16 *)(mem + CPU_ES_BASE + CPU_DI + ct * 4 + 0x01) = (ct - 0xff) * 0x10 + 0x1100;	// mapping
//			*(UINT8  *)(mem + CPU_ES_BASE + CPU_DI + ct * 4 + 0x03) = 0x00;
//		}
		CPU_DI += 0x400;		// advance pointer by 0x100*4
		
		// Set up three descriptor table entries
//		UINT32 cbseg_low  = (DUMMY_TOP & 0x00ffff) << 16;
//		UINT32 cbseg_high = (DUMMY_TOP & 0x1f0000) >> 16;
		// Descriptor 1 (code segment, callback segment)
		*(UINT32 *)(mem + CPU_DS_BASE + CPU_SI + 0x00) = 0x0000ffff;
		*(UINT32 *)(mem + CPU_DS_BASE + CPU_SI + 0x04) = 0x00409a0f;
		// Descriptor 2 (data segment, full access)
		*(UINT32 *)(mem + CPU_DS_BASE + CPU_SI + 0x08) = 0x0000ffff;
		*(UINT32 *)(mem + CPU_DS_BASE + CPU_SI + 0x0c) = 0x0000920f;
		// Descriptor 3 (full access)
		*(UINT32 *)(mem + CPU_DS_BASE + CPU_SI + 0x10) = 0x0000ffff;
		*(UINT32 *)(mem + CPU_DS_BASE + CPU_SI + 0x14) = 0x0000920f;
		// Offset in code segment of protected mode entry point
		CPU_EBX = DUMMY_TOP + 0x2a - 0xf0000; // fffc:002a
	} else if(CPU_AL == 0x02) {
		CPU_AH = 0x00;
		CPU_EDX = (MAX_MEM - 1) & 0xfffff000;
	} else if(CPU_AL == 0x03) {
		CPU_AH = 0x00;
		CPU_EDX = 0;
		for(emb_handle_t *emb_handle = emb_handle_top; emb_handle != NULL; emb_handle = emb_handle->next) {
			if(emb_handle->handle == 0) {
				CPU_EDX += emb_handle->size_kb;
			}
		}
		CPU_EDX /= 4;
	} else if(CPU_AL == 0x04) {
		emb_handle_t *emb_handle = msdos_xms_alloc_emb_handle(4);
		if(emb_handle != NULL) {
			CPU_AH = 0x00;
			CPU_EDX = emb_handle->address;
		}
	} else if(CPU_AL == 0x05) {
		for(emb_handle_t *emb_handle = emb_handle_top; emb_handle != NULL; emb_handle = emb_handle->next) {
			if(emb_handle->handle != 0 && emb_handle->address == CPU_EDX) {
				CPU_AH = 0x00;
				msdos_xms_free_emb_handle(emb_handle);
				break;
			}
		}
	} else if(CPU_AL == 0x06) {
		CPU_AH = 0x00;
		CPU_EDX = CPU_CX << 12;
	} else if(CPU_AL == 0x07) {
		CPU_AH = 0x00;
		CPU_EBX = CPU_CR0;
	} else if(CPU_AL == 0x08) {
		CPU_AH = 0x00;
		*(UINT32 *)(mem + CPU_ES_BASE + CPU_DI + 4 * 0) = CPU_DR(0);
		*(UINT32 *)(mem + CPU_ES_BASE + CPU_DI + 4 * 1) = CPU_DR(1);
		*(UINT32 *)(mem + CPU_ES_BASE + CPU_DI + 4 * 2) = CPU_DR(2);
		*(UINT32 *)(mem + CPU_ES_BASE + CPU_DI + 4 * 3) = CPU_DR(3);
		*(UINT32 *)(mem + CPU_ES_BASE + CPU_DI + 4 * 6) = CPU_DR(6);
		*(UINT32 *)(mem + CPU_ES_BASE + CPU_DI + 4 * 7) = CPU_DR(7);
	} else if(CPU_AL == 0x09) {
		CPU_AH = 0x00;
		CPU_DR(0) = *(UINT32 *)(mem + CPU_ES_BASE + CPU_DI + 4 * 0);
		CPU_DR(1) = *(UINT32 *)(mem + CPU_ES_BASE + CPU_DI + 4 * 1);
		CPU_DR(2) = *(UINT32 *)(mem + CPU_ES_BASE + CPU_DI + 4 * 2);
		CPU_DR(3) = *(UINT32 *)(mem + CPU_ES_BASE + CPU_DI + 4 * 3);
		CPU_DR(6) = *(UINT32 *)(mem + CPU_ES_BASE + CPU_DI + 4 * 6);
		CPU_DR(7) = *(UINT32 *)(mem + CPU_ES_BASE + CPU_DI + 4 * 7);
	} else if(CPU_AL == 0x0a) {
		CPU_AH = 0x00;
		CPU_BX = pic[0].icw2;
		CPU_CX = pic[1].icw2;
	} else if(CPU_AL == 0x0b) {
		CPU_AH = 0x00;  // this is just a notification
//		pic[0].icw2 = CPU_BL;
//		pic[1].icw2 = CPU_CL;
	} else if(CPU_AL == 0x0c) {
		if(!CPU_STAT_PM || CPU_STAT_VM86) {
			// from DOSBox
			CPU_SET_I_FLAG(0);
			CPU_SET_CPL(0);

			// Read data from ESI (linear address)
			UINT32 new_cr3      = *(UINT32 *)(mem + CPU_ESI + 0x00);
			UINT32 new_gdt_addr = *(UINT32 *)(mem + CPU_ESI + 0x04);
			UINT32 new_idt_addr = *(UINT32 *)(mem + CPU_ESI + 0x08);
			UINT16 new_ldt      = *(UINT16 *)(mem + CPU_ESI + 0x0c);
			UINT16 new_tr       = *(UINT16 *)(mem + CPU_ESI + 0x0e);
			UINT32 new_eip      = *(UINT32 *)(mem + CPU_ESI + 0x10);
			UINT16 new_cs       = *(UINT16 *)(mem + CPU_ESI + 0x14);

			// Get GDT and IDT entries
			UINT16 new_gdt_limit = *(UINT16 *)(mem + new_gdt_addr + 0);
			UINT32 new_gdt_base  = *(UINT32 *)(mem + new_gdt_addr + 2);
			UINT16 new_idt_limit = *(UINT16 *)(mem + new_idt_addr + 0);
			UINT32 new_idt_base  = *(UINT32 *)(mem + new_idt_addr + 2);

			// Switch to protected mode, paging enabled if necessary
			UINT32 new_cr0 = CPU_CR0 | CPU_CR0_PE;
			if(new_cr3 != 0) {
				new_cr0 |= CPU_CR0_PG;
			}
			CPU_SET_CR0(new_cr0);
			CPU_SET_CR3(new_cr3);

			*(UINT8 *)(mem + new_gdt_base + (new_tr & 0xfff8) + 5) &= 0xfd;

			// Load tables and initialize segment registers
			CPU_GDTR_LIMIT = new_gdt_limit;
			CPU_GDTR_BASE  = new_gdt_base;
			CPU_IDTR_LIMIT = new_idt_limit;
			CPU_IDTR_BASE  = new_idt_base;

			CPU_LOAD_LDTR(new_ldt);
			CPU_LOAD_TR(new_tr);

			CPU_LOAD_SREG(CPU_DS_INDEX, 0x0000);
			CPU_LOAD_SREG(CPU_ES_INDEX, 0x0000);
			CPU_LOAD_SREG(CPU_FS_INDEX, 0x0000);
			CPU_LOAD_SREG(CPU_GS_INDEX, 0x0000);

			//CPU_A20_LINE(1);

			/* Switch to protected mode */
			CPU_SET_VM_FLAG(0);
			CPU_SET_NT_FLAG(0);
			CPU_SET_IOP_FLAG(3);

			CPU_JMP_FAR(new_cs, new_eip);
		} else {
			// just cheat and switch to real mode instead of v86 mode
			// otherwise a GDT and IDT would need to be set up
			// hopefully most vcpi programs are okay with that
			UINT32 new_cr0 = CPU_CR0 & 0x7ffffffe;
			CPU_SET_CR0(new_cr0);

			UINT32 stack = CPU_TRANS_PAGING_ADDR(CPU_SS_BASE + CPU_ESP + 8);
			UINT32 *stkptr = (UINT32 *)(mem + stack);

			stkptr[2] &= ~(0x200);
			CPU_ESP = stkptr[3];
			CPU_LOAD_SREG(CPU_SS_INDEX, stkptr[4]);
			CPU_LOAD_SREG(CPU_ES_INDEX, stkptr[5]);
			CPU_LOAD_SREG(CPU_DS_INDEX, stkptr[6]);
			CPU_LOAD_SREG(CPU_FS_INDEX, stkptr[7]);
			CPU_LOAD_SREG(CPU_GS_INDEX, stkptr[8]);

			CPU_SET_CPL(0);
			CPU_SET_VM_FLAG(0);
			CPU_SET_IOP_FLAG(0);

			CPU_IDTR_LIMIT = 0x0400;
			CPU_IDTR_BASE  = 0x00000000;

			CPU_JMP_FAR(stkptr[1], stkptr[0]);
		}
	} else {
		unimplemented_67h("int %02Xh (AX=%04X BX=%04X CX=%04X DX=%04X SI=%04X DI=%04X DS=%04X ES=%04X)\n", 0x67, CPU_AX, CPU_BX, CPU_CX, CPU_DX, CPU_SI, CPU_DI, CPU_DS, CPU_ES);
		CPU_AH = 0x8f;
	}
#else
	CPU_AH = 0x84;
#endif
}

#ifdef SUPPORT_XMS

void msdos_xms_init()
{
	emb_handle_top = (emb_handle_t *)calloc(1, sizeof(emb_handle_t));
	emb_handle_top->address = EMB_TOP;
	emb_handle_top->size_kb = (EMB_END - EMB_TOP) >> 10;
	xms_a20_local_enb_count = 0;
}

void msdos_xms_finish()
{
	msdos_xms_release();
}

void msdos_xms_release()
{
	for(emb_handle_t *emb_handle = emb_handle_top; emb_handle != NULL;) {
		emb_handle_t *next_handle = emb_handle->next;
		free(emb_handle);
		emb_handle = next_handle;
	}
}

emb_handle_t *msdos_xms_get_emb_handle(int handle)
{
	if(handle != 0) {
		for(emb_handle_t *emb_handle = emb_handle_top; emb_handle != NULL; emb_handle = emb_handle->next) {
			if(emb_handle->handle == handle) {
				return(emb_handle);
			}
		}
	}
	return(NULL);
}

int msdos_xms_get_unused_emb_handle_id()
{
	for(int handle = 1;; handle++) {
		if(msdos_xms_get_emb_handle(handle) == NULL) {
			return(handle);
		}
	}
	return(0);
}

int msdos_xms_get_unused_emb_handle_count()
{
	int count = 64; //255;
	
	for(emb_handle_t *emb_handle = emb_handle_top; emb_handle != NULL; emb_handle = emb_handle->next) {
		if(emb_handle->handle != 0) {
			if(--count == 1) {
				break;
			}
		}
	}
	return(count);
}

void msdos_xms_split_emb_handle(emb_handle_t *emb_handle, int size_kb)
{
	if(emb_handle->size_kb > size_kb) {
		emb_handle_t *new_handle = (emb_handle_t *)calloc(1, sizeof(emb_handle_t));
		
		new_handle->address = emb_handle->address + size_kb * 1024;
		new_handle->size_kb = emb_handle->size_kb - size_kb;
		emb_handle->size_kb = size_kb;
		
		new_handle->prev = emb_handle;
		new_handle->next = emb_handle->next;
		if(emb_handle->next != NULL) {
			emb_handle->next->prev = new_handle;
		}
		emb_handle->next = new_handle;
	}
}

void msdos_xms_combine_emb_handles(emb_handle_t *emb_handle)
{
	emb_handle_t *next_handle = emb_handle->next;
	
	if(next_handle != NULL) {
		emb_handle->size_kb += next_handle->size_kb;
		
		if(next_handle->next != NULL) {
			next_handle->next->prev = emb_handle;
		}
		emb_handle->next = next_handle->next;
		free(next_handle);
	}
}

emb_handle_t *msdos_xms_alloc_emb_handle(int size_kb)
{
	emb_handle_t *target_handle = NULL;
	
	for(emb_handle_t *emb_handle = emb_handle_top; emb_handle != NULL; emb_handle = emb_handle->next) {
		if(emb_handle->handle == 0 && emb_handle->size_kb >= size_kb) {
			if(target_handle == NULL || target_handle->size_kb > emb_handle->size_kb) {
				target_handle = emb_handle;
			}
		}
	}
	if(target_handle != NULL) {
		if(target_handle->size_kb > size_kb) {
			msdos_xms_split_emb_handle(target_handle, size_kb);
		}
//		target_handle->handle = msdos_xms_get_unused_emb_handle_id();
		return(target_handle);
	}
	return(NULL);
}

void msdos_xms_free_emb_handle(emb_handle_t *emb_handle)
{
	emb_handle_t *prev_handle = emb_handle->prev;
	emb_handle_t *next_handle = emb_handle->next;
	
	if(prev_handle != NULL && prev_handle->handle == 0) {
		msdos_xms_combine_emb_handles(prev_handle);
		emb_handle = prev_handle;
	}
	if(next_handle != NULL && next_handle->handle == 0) {
		msdos_xms_combine_emb_handles(emb_handle);
	}
	emb_handle->handle = 0;
}

inline void msdos_call_xms_00h()
{
#if defined(HAS_I386)
	CPU_AX = 0x0300; // V3.00 (XMS Version)
	CPU_BX = 0x0395; // V3.95 (Driver Revision in BCD)
//	CPU_BX = 0x035f; // V3.95 (Driver Revision)
#else
	CPU_AX = 0x0200; // V2.00 (XMS Version)
	CPU_BX = 0x0270; // V2.70 (Driver Revision)
#endif
//	CPU_DX = 0x0000; // HMA does not exist
	CPU_DX = 0x0001; // HMA does exist
}

inline void msdos_call_xms_01h()
{
	if(CPU_AL == 0x40) {
		// HIMEM.SYS will fail function 01h with error code 91h if AL=40h and
		// DX=KB free extended memory returned by last call of function 08h
		CPU_AX = 0x0000;
		CPU_BL = 0x91;
		CPU_DX = xms_dx_after_call_08h;
	} else if(memcmp(mem + 0x100003, "VDISK", 5) == 0) {
		CPU_AX = 0x0000;
		CPU_BL = 0x81; // Vdisk was detected
#ifdef SUPPORT_HMA
	} else if(is_hma_used_by_int_2fh) {
		CPU_AX = 0x0000;
		CPU_BL = 0x90; // HMA does not exist or is not managed by XMS provider
	} else if(is_hma_used_by_xms) {
		CPU_AX = 0x0000;
		CPU_BL = 0x91; // HMA is already in use
	} else {
		CPU_AX = 0x0001;
		is_hma_used_by_xms = true;
#else
	} else {
		CPU_AX = 0x0000;
		CPU_BL = 0x91; // HMA is already in use
#endif
	}
}

inline void msdos_call_xms_02h()
{
	if(memcmp(mem + 0x100003, "VDISK", 5) == 0) {
		CPU_AX = 0x0000;
		CPU_BL = 0x81; // Vdisk was detected
#ifdef SUPPORT_HMA
	} else if(is_hma_used_by_int_2fh) {
		CPU_AX = 0x0000;
		CPU_BL = 0x90; // HMA does not exist or is not managed by XMS provider
	} else if(!is_hma_used_by_xms) {
		CPU_AX = 0x0000;
		CPU_BL = 0x93; // HMA is not allocated
	} else {
		CPU_AX = 0x0001;
		is_hma_used_by_xms = false;
		// restore first free mcb in high memory area
		msdos_hma_mcb_create(0x10, 0, 0xffe0, 0);
#else
	} else {
		CPU_AX = 0x0000;
		CPU_BL = 0x91; // HMA is already in use
#endif
	}
}

inline void msdos_call_xms_03h()
{
	CPU_A20_LINE(1);
	CPU_AX = 0x0001;
	CPU_BL = 0x00;
}

inline void msdos_call_xms_04h()
{
	CPU_A20_LINE(0);
	CPU_AX = 0x0001;
	CPU_BL = 0x00;
}

inline void msdos_call_xms_05h()
{
	CPU_A20_LINE(1);
	CPU_AX = 0x0001;
	CPU_BL = 0x00;
	xms_a20_local_enb_count++;
}

void msdos_call_xms_06h()
{
	if(xms_a20_local_enb_count > 0) {
		if(--xms_a20_local_enb_count == 0) {
			CPU_A20_LINE(0);
			CPU_AX = 0x0001;
			CPU_BL = 0x00;
		} else {
			CPU_AX = 0x0000;
			CPU_BL = 0x94;
		}
	} else {
		CPU_A20_LINE(0);
		CPU_AX = 0x0001;
		CPU_BL = 0x00;
	}
}

inline void msdos_call_xms_07h()
{
	CPU_AX = (CPU_ADRSMASK >> 20) & 1;
	CPU_BL = 0x00;
}

inline void msdos_call_xms_08h()
{
	UINT32 eax = 0, edx = 0;
	
	for(emb_handle_t *emb_handle = emb_handle_top; emb_handle != NULL; emb_handle = emb_handle->next) {
		if(emb_handle->handle == 0) {
			if(eax < emb_handle->size_kb) {
				eax = emb_handle->size_kb;
			}
			edx += emb_handle->size_kb;
		}
	}
	if(eax > 65535) {
		eax = 65535;
	}
	if(edx > 65535) {
		edx = 65535;
	}
	if(eax == 0 && edx == 0) {
		CPU_BL = 0xa0;
	} else {
		CPU_BL = 0x00;
	}
#if defined(HAS_I386)
	CPU_EAX = eax;
	CPU_EDX = edx;
#else
	CPU_AX = (UINT16)eax;
	CPU_DX = (UINT16)edx;
#endif
	xms_dx_after_call_08h = CPU_DX;
}

void msdos_call_xms_09h(int size_kb)
{
	emb_handle_t *emb_handle = msdos_xms_alloc_emb_handle(size_kb);
	
	if(emb_handle != NULL) {
		emb_handle->handle = msdos_xms_get_unused_emb_handle_id();
		
		CPU_AX = 0x0001;
		CPU_DX = emb_handle->handle;
		CPU_BL = 0x00;
	} else {
		CPU_AX = CPU_DX = 0x0000;
		CPU_BL = 0xa0;
	}
}

inline void msdos_call_xms_09h()
{
	msdos_call_xms_09h(CPU_DX);
}

inline void msdos_call_xms_0ah()
{
	emb_handle_t *emb_handle = msdos_xms_get_emb_handle(CPU_DX);
	
	if(emb_handle == NULL) {
		CPU_AX = 0x0000;
		CPU_BL = 0xa2;
//	} else if(emb_handle->lock > 0) {
//		CPU_AX = 0x0000;
//		CPU_BL = 0xab;
	} else {
		msdos_xms_free_emb_handle(emb_handle);
		
		CPU_AX = 0x0001;
		CPU_BL = 0x00;
	}
}

inline void msdos_call_xms_0bh()
{
	UINT32 copy_length = *(UINT32 *)(mem + CPU_DS_BASE + CPU_SI + 0x00);
	UINT16 src_handle  = *(UINT16 *)(mem + CPU_DS_BASE + CPU_SI + 0x04);
	UINT32 src_addr    = *(UINT32 *)(mem + CPU_DS_BASE + CPU_SI + 0x06);
	UINT16 dest_handle = *(UINT16 *)(mem + CPU_DS_BASE + CPU_SI + 0x0a);
	UINT32 dest_addr   = *(UINT32 *)(mem + CPU_DS_BASE + CPU_SI + 0x0c);
	
	UINT8 *src_buffer, *dest_buffer;
	UINT32 src_addr_max, dest_addr_max;
	emb_handle_t *emb_handle;
	
	if(src_handle == 0) {
		src_buffer = mem;
		src_addr = (((src_addr >> 16) & 0xffff) << 4) + (src_addr & 0xffff);
		src_addr_max = MAX_MEM;
	} else {
		if((emb_handle = msdos_xms_get_emb_handle(src_handle)) == NULL) {
			CPU_AX = 0x0000;
			CPU_BL = 0xa3;
			return;
		}
		src_buffer = mem + emb_handle->address;
		src_addr_max = emb_handle->size_kb * 1024;
	}
	if(dest_handle == 0) {
		dest_buffer = mem;
		dest_addr = (((dest_addr >> 16) & 0xffff) << 4) + (dest_addr & 0xffff);
		dest_addr_max = MAX_MEM;
	} else {
		if((emb_handle = msdos_xms_get_emb_handle(dest_handle)) == NULL) {
			CPU_AX = 0x0000;
			CPU_BL = 0xa5;
			return;
		}
		dest_buffer = mem + emb_handle->address;
		dest_addr_max = emb_handle->size_kb * 1024;
	}
	for(int i = 0; i < copy_length; i++) {
		if(src_addr < src_addr_max && dest_addr < dest_addr_max) {
			dest_buffer[dest_addr++] = src_buffer[src_addr++];
		} else {
			break;
		}
	}
	CPU_AX = 0x0001;
	CPU_BL = 0x00;
}

inline void msdos_call_xms_0ch()
{
	emb_handle_t *emb_handle = msdos_xms_get_emb_handle(CPU_DX);
	
	if(emb_handle == NULL) {
		CPU_AX = 0x0000;
		CPU_BL = 0xa2;
	} else {
		if(emb_handle->lock < 255) {
			emb_handle->lock++;
		}
		CPU_AX = 0x0001;
		CPU_DX = (emb_handle->address >> 16) & 0xffff;
		CPU_BX = (emb_handle->address      ) & 0xffff;
	}
}

inline void msdos_call_xms_0dh()
{
	emb_handle_t *emb_handle = msdos_xms_get_emb_handle(CPU_DX);
	
	if(emb_handle == NULL) {
		CPU_AX = 0x0000;
		CPU_BL = 0xa2;
	} else if(!(emb_handle->lock > 0)) {
		CPU_AX = 0x0000;
		CPU_BL = 0xaa;
	} else {
		emb_handle->lock--;
		CPU_AX = 0x0001;
		CPU_BL = 0x00;
	}
}

inline void msdos_call_xms_0eh()
{
	emb_handle_t *emb_handle = msdos_xms_get_emb_handle(CPU_DX);
	
	if(emb_handle == NULL) {
		CPU_AX = 0x0000;
		CPU_BL = 0xa2;
	} else {
		CPU_AX = 0x0001;
		CPU_BH = emb_handle->lock;
		CPU_BL = msdos_xms_get_unused_emb_handle_count();
		CPU_DX = emb_handle->size_kb;
	}
}

void msdos_call_xms_0fh(int size_kb)
{
	emb_handle_t *emb_handle = msdos_xms_get_emb_handle(CPU_DX);
	
	if(emb_handle == NULL) {
		CPU_AX = 0x0000;
		CPU_BL = 0xa2;
	} else if(emb_handle->lock > 0) {
		CPU_AX = 0x0000;
		CPU_BL = 0xab;
	} else {
		if(emb_handle->size_kb < size_kb) {
			if(emb_handle->next != NULL && emb_handle->next->handle == 0 && (emb_handle->size_kb + emb_handle->next->size_kb) >= size_kb) {
				msdos_xms_combine_emb_handles(emb_handle);
				if(emb_handle->size_kb > size_kb) {
					msdos_xms_split_emb_handle(emb_handle, size_kb);
				}
			} else {
				int old_handle = emb_handle->handle;
				int old_size_kb = emb_handle->size_kb;
				UINT8 *buffer = (UINT8 *)malloc(old_size_kb * 1024);
				
				memcpy(buffer, mem + emb_handle->address, old_size_kb * 1024);
				msdos_xms_free_emb_handle(emb_handle);
				
				if((emb_handle = msdos_xms_alloc_emb_handle(size_kb)) == NULL) {
					emb_handle = msdos_xms_alloc_emb_handle(old_size_kb); // should be always successed
				}
				emb_handle->handle = old_handle;
				memcpy(mem + emb_handle->address, buffer, old_size_kb * 1024);
				free(buffer);
			}
		} else if(emb_handle->size_kb > size_kb) {
			msdos_xms_split_emb_handle(emb_handle, size_kb);
		}
		if(emb_handle->size_kb != size_kb) {
			CPU_AX = 0x0000;
			CPU_BL = 0xa0;
		} else {
			CPU_AX = 0x0001;
			CPU_BL = 0x00;
		}
	}
}

inline void msdos_call_xms_0fh()
{
	msdos_call_xms_0fh(CPU_BX);
}

inline void msdos_call_xms_10h()
{
	int seg;
	
	if((seg = msdos_mem_alloc(UMB_TOP >> 4, CPU_DX, 0)) != -1) {
		CPU_AX = 0x0001;
		CPU_BX = seg;
	} else {
		CPU_AX = 0x0000;
		CPU_BL = 0xb0;
		CPU_DX = msdos_mem_get_free(UMB_TOP >> 4, 0);
	}
}

inline void msdos_call_xms_11h()
{
	int mcb_seg = CPU_DX - 1;
	mcb_t *mcb = (mcb_t *)(mem + (mcb_seg << 4));
	
	if(mcb->mz == 'M' || mcb->mz == 'Z') {
		msdos_mem_free(CPU_DX);
		CPU_AX = 0x0001;
		CPU_BL = 0x00;
	} else {
		CPU_AX = 0x0000;
		CPU_BL = 0xb2;
	}
}

inline void msdos_call_xms_12h()
{
	int mcb_seg = CPU_DX - 1;
	mcb_t *mcb = (mcb_t *)(mem + (mcb_seg << 4));
	int max_paragraphs;
	
	if(mcb->mz == 'M' || mcb->mz == 'Z') {
		if(!msdos_mem_realloc(CPU_DX, CPU_BX, &max_paragraphs)) {
			CPU_AX = 0x0001;
			CPU_BL = 0x00;
		} else {
			CPU_AX = 0x0000;
			CPU_BL = 0xb0;
			CPU_DX = max_paragraphs;
		}
	} else {
		CPU_AX = 0x0000;
		CPU_BL = 0xb2;
	}
}

#if defined(HAS_I386)

inline void msdos_call_xms_88h()
{
	CPU_EAX = CPU_EDX = 0x0000;
	
	for(emb_handle_t *emb_handle = emb_handle_top; emb_handle != NULL; emb_handle = emb_handle->next) {
		if(emb_handle->handle == 0) {
			if(CPU_EAX < emb_handle->size_kb) {
				CPU_EAX = emb_handle->size_kb;
			}
			CPU_EDX += emb_handle->size_kb;
		}
	}
	if(CPU_EAX == 0 && CPU_EDX == 0) {
		CPU_BL = 0xa0;
	} else {
		CPU_BL = 0x00;
	}
	CPU_ECX = EMB_END - 1;
}

inline void msdos_call_xms_89h()
{
	msdos_call_xms_09h(CPU_EDX);
}

inline void msdos_call_xms_8eh()
{
	emb_handle_t *emb_handle = msdos_xms_get_emb_handle(CPU_DX);
	
	if(emb_handle == NULL) {
		CPU_AX = 0x0000;
		CPU_BL = 0xa2;
	} else {
		CPU_AX = 0x0001;
		CPU_BH = emb_handle->lock;
		CPU_CX = msdos_xms_get_unused_emb_handle_count();
		CPU_EDX = emb_handle->size_kb;
	}
}

inline void msdos_call_xms_8fh()
{
	msdos_call_xms_0fh(CPU_EBX);
}

#endif
#endif

UINT16 msdos_get_equipment()
{
	static UINT16 equip = 0;
	
	if(equip == 0) {
#ifdef SUPPORT_FPU
		equip |= (1 << 1);	// 80x87 coprocessor installed
#endif
		equip |= (1 << 2);	// pointing device installed (PS/2)
		equip |= (2 << 4);	// initial video mode (80x25 color)
//		equip |= (1 << 8);	// 0 if DMA installed
		equip |= (2 << 9);	// number of serial ports
		equip |= (3 << 14);	// number of printer ports (NOTE: this number is 3 on Windows 98 SE though only LPT1 exists)
		
		// check only A: and B: if it is floppy drive
		int n = 0;
		for(int i = 0; i < 2; i++) {
			if(msdos_is_valid_drive(i) && msdos_is_removable_drive(i)) {
				n++;
			}
		}
		if(n != 0) {
			equip |= (1 << 0);	// floppy disk(s) installed
			n--;
			equip |= (n << 6);	// number of floppies installed less 1
		}
//		if(joyGetNumDevs() != 0) {
//			equip |= (1 << 12);	// game port installed
//		}
	}
	return(equip);
}

void msdos_syscall(unsigned num)
{
#ifdef ENABLE_DEBUG_SYSCALL
	if(num == 0x08 || num == 0x1c) {
		// don't log the timer interrupts
//		fprintf(fp_debug_log, "int %02Xh (AX=%04X BX=%04X CX=%04X DX=%04X SI=%04X DI=%04X DS=%04X ES=%04X)\n", num, CPU_AX, CPU_BX, CPU_CX, CPU_DX, CPU_SI, CPU_DI, CPU_DS, CPU_ES);
	} else if(num == 0x30) {
		// dummy interrupt for call 0005h (call near)
		fprintf(fp_debug_log, "call 0005h (AX=%04X BX=%04X CX=%04X DX=%04X SI=%04X DI=%04X DS=%04X ES=%04X)\n", CPU_AX, CPU_BX, CPU_CX, CPU_DX, CPU_SI, CPU_DI, CPU_DS, CPU_ES);
	} else if(num == 0x65) {
		// dummy interrupt for EMS (int 67h)
		fprintf(fp_debug_log, "int %02Xh (AX=%04X BX=%04X CX=%04X DX=%04X SI=%04X DI=%04X DS=%04X ES=%04X)\n", 0x67, CPU_AX, CPU_BX, CPU_CX, CPU_DX, CPU_SI, CPU_DI, CPU_DS, CPU_ES);
	} else if(num == 0x66) {
		// dummy interrupt for XMS (call far)
		fprintf(fp_debug_log, "call XMS (AX=%04X BX=%04X CX=%04X DX=%04X SI=%04X DI=%04X DS=%04X ES=%04X)\n", CPU_AX, CPU_BX, CPU_CX, CPU_DX, CPU_SI, CPU_DI, CPU_DS, CPU_ES);
	} else if(num >= 0x68 && num <= 0x6f) {
		// dummy interrupt
	} else {
		fprintf(fp_debug_log, "int %02Xh (AX=%04X BX=%04X CX=%04X DX=%04X SI=%04X DI=%04X DS=%04X ES=%04X)\n", num, CPU_AX, CPU_BX, CPU_CX, CPU_DX, CPU_SI, CPU_DI, CPU_DS, CPU_ES);
	}
#endif
	// update cursor position
	if(cursor_moved) {
		pcbios_update_cursor_position();
		cursor_moved = false;
	}
#ifdef USE_SERVICE_THREAD
	// this is called from dummy loop to wait until a serive that waits input is done
	if(!in_service)
#endif
	ctrl_break_detected = ctrl_break_pressed = ctrl_c_pressed = false;
	
	switch(num) {
	case 0x00:
		try {
			msdos_process_terminate(current_psp, (retval & 0xff) | 0x200, 1);
			error("division by zero\n");
		} catch(...) {
			fatalerror("division by zero detected, and failed to terminate current process\n");
		}
		break;
	case 0x04:
		try {
			msdos_process_terminate(current_psp, (retval & 0xff) | 0x200, 1);
			error("overflow\n");
		} catch(...) {
			fatalerror("overflow detected, and failed to terminate current process\n");
		}
		break;
	case 0x06:
		// NOTE: ish.com has illegal instruction...
		if(!ignore_illegal_insn) {
			try {
				msdos_process_terminate(current_psp, (retval & 0xff) | 0x200, 1);
				error("illegal instruction\n");
			} catch(...) {
				fatalerror("illegal instruction detected, and failed to terminate current process\n");
			}
		} else {
#if defined(HAS_I386)
			CPU_SET_EIP(msdos_int6h_eip);
#elif defined(HAS_I286)
			CPU_SET_PC(msdos_int6h_pc);
#else
			// 8086/80186 ignore an invalid opcode
#endif
		}
		break;
	case 0x09:
		// ctrl-break is pressed
		if(raise_int_1bh) {
			CPU_SOFT_INTERRUPT(0x1b);
			raise_int_1bh = false;
		}
	case 0x08:
//		pcbios_irq0(); // this causes too slow emulation...
	case 0x0b:
	case 0x0c:
	case 0x0d:
	case 0x0e:
	case 0x0f:
		// EOI
		pic[0].isr &= ~(1 << (num - 0x08));
		pic_update();
		break;
	case 0x10:
		// PC BIOS - Video
		if(!restore_console_size) {
			change_console_size(scr_width, scr_height);
		}
		CPU_SET_C_FLAG(0);
		switch(CPU_AH) {
		case 0x00: pcbios_int_10h_00h(); break;
		case 0x01: pcbios_int_10h_01h(); break;
		case 0x02: pcbios_int_10h_02h(); break;
		case 0x03: pcbios_int_10h_03h(); break;
		case 0x05: pcbios_int_10h_05h(); break;
		case 0x06: pcbios_int_10h_06h(); break;
		case 0x07: pcbios_int_10h_07h(); break;
		case 0x08: pcbios_int_10h_08h(); break;
		case 0x09: pcbios_int_10h_09h(); break;
		case 0x0a: pcbios_int_10h_0ah(); break;
		case 0x0b: break;
		case 0x0c: pcbios_int_10h_0ch(); break;
		case 0x0d: pcbios_int_10h_0dh(); break;
		case 0x0e: pcbios_int_10h_0eh(); break;
		case 0x0f: pcbios_int_10h_0fh(); break;
		case 0x10: break;
		case 0x11: pcbios_int_10h_11h(); break;
		case 0x12: pcbios_int_10h_12h(); break;
		case 0x13: pcbios_int_10h_13h(); break;
		case 0x18: pcbios_int_10h_18h(); break;
		case 0x1a: pcbios_int_10h_1ah(); break;
		case 0x1b: pcbios_int_10h_1bh(); break;
		case 0x1c: CPU_AL = 0x00; break; // save/restore video state is not supported
		case 0x1d: pcbios_int_10h_1dh(); break;
		case 0x1e: CPU_AL = 0x00; break; // flat-panel functions are not supported
		case 0x1f: CPU_AL = 0x00; break; // xga functions are not supported
		case 0x4f: pcbios_int_10h_4fh(); break;
		case 0x6f: break;
		case 0x80: CPU_SET_C_FLAG(1); break; // unknown
		case 0x81: CPU_SET_C_FLAG(1); break; // unknown
		case 0x82: pcbios_int_10h_82h(); break;
		case 0x83: pcbios_int_10h_83h(); break;
		case 0x8b: break;
		case 0x8c: CPU_SET_C_FLAG(1); break; // unknown
		case 0x8d: CPU_SET_C_FLAG(1); break; // unknown
		case 0x8e: CPU_SET_C_FLAG(1); break; // unknown
		case 0x90: pcbios_int_10h_90h(); break;
		case 0x91: pcbios_int_10h_91h(); break;
		case 0x92: break;
		case 0x93: break;
		case 0xef: pcbios_int_10h_efh(); break;
		case 0xfa: break; // ega register interface library is not installed
		case 0xfe: pcbios_int_10h_feh(); break;
		case 0xff: pcbios_int_10h_ffh(); break;
		default:
			unimplemented_10h("int %02Xh (AX=%04X BX=%04X CX=%04X DX=%04X SI=%04X DI=%04X DS=%04X ES=%04X)\n", num, CPU_AX, CPU_BX, CPU_CX, CPU_DX, CPU_SI, CPU_DI, CPU_DS, CPU_ES);
			CPU_SET_C_FLAG(1);
			break;
		}
		break;
	case 0x11:
		// PC BIOS - Get Equipment List
		CPU_AX = msdos_get_equipment();
		break;
	case 0x12:
		// PC BIOS - Get Memory Size
		CPU_AX = *(UINT16 *)(mem + 0x413);
		break;
	case 0x13:
		// PC BIOS - Disk I/O
		{
			static UINT8 last = 0x00;
			switch(CPU_AH) {
			case 0x00: pcbios_int_13h_00h(); break;
			case 0x01: // get last status
				CPU_AH = last;
				break;
			case 0x02: pcbios_int_13h_02h(); break;
			case 0x03: pcbios_int_13h_03h(); break;
			case 0x04: pcbios_int_13h_04h(); break;
			case 0x08: pcbios_int_13h_08h(); break;
			case 0x0a: pcbios_int_13h_02h(); break;
			case 0x0b: pcbios_int_13h_03h(); break;
			case 0x0d: pcbios_int_13h_00h(); break;
			case 0x10: pcbios_int_13h_10h(); break;
			case 0x15: pcbios_int_13h_15h(); break;
			case 0x41: pcbios_int_13h_41h(); break;
			case 0x05: // format
			case 0x06:
			case 0x07:
				CPU_AH = 0x0c; // unsupported track or invalid media
				CPU_SET_C_FLAG(1);
				break;
			case 0x09:
			case 0x0c: // seek
			case 0x11: // recalib
			case 0x14:
			case 0x17:
				CPU_AH = 0x00; // successful completion
				break;
			case 0x21: // QUICKCACHE II v4.20 - Flush Cache
			case 0xa1: // Super PC-Kwik v3.20+ - Flush Cache
				CPU_AH = 0x01; // invalid function
				CPU_SET_C_FLAG(1);
				break;
			default:
				unimplemented_13h("int %02Xh (AX=%04X BX=%04X CX=%04X DX=%04X SI=%04X DI=%04X DS=%04X ES=%04X)\n", num, CPU_AX, CPU_BX, CPU_CX, CPU_DX, CPU_SI, CPU_DI, CPU_DS, CPU_ES);
				CPU_AH = 0x01; // invalid function
				CPU_SET_C_FLAG(1);
				break;
			}
			last = CPU_AH;
		}
		break;
	case 0x14:
		// PC BIOS - Serial I/O
		switch(CPU_AH) {
		case 0x00: pcbios_int_14h_00h(); break;
		case 0x01: pcbios_int_14h_01h(); break;
		case 0x02: pcbios_int_14h_02h(); break;
		case 0x03: pcbios_int_14h_03h(); break;
		case 0x04: pcbios_int_14h_04h(); break;
		case 0x05: pcbios_int_14h_05h(); break;
		default:
			unimplemented_14h("int %02Xh (AX=%04X BX=%04X CX=%04X DX=%04X SI=%04X DI=%04X DS=%04X ES=%04X)\n", num, CPU_AX, CPU_BX, CPU_CX, CPU_DX, CPU_SI, CPU_DI, CPU_DS, CPU_ES);
			break;
		}
		break;
	case 0x15:
		// PC BIOS
		CPU_SET_C_FLAG(0);
		switch(CPU_AH) {
		case 0x23: pcbios_int_15h_23h(); break;
		case 0x24: pcbios_int_15h_24h(); break;
		case 0x41: break;
		case 0x49: pcbios_int_15h_49h(); break;
		case 0x4f: CPU_SET_C_FLAG(1); break; // from DOSBox
		case 0x50: pcbios_int_15h_50h(); break;
		case 0x53: pcbios_int_15h_53h(); break;
		case 0x84: pcbios_int_15h_84h(); break;
		case 0x86: pcbios_int_15h_86h(); break;
		case 0x87: pcbios_int_15h_87h(); break;
		case 0x88: pcbios_int_15h_88h(); break;
		case 0x89: pcbios_int_15h_89h(); break;
		case 0x8a: pcbios_int_15h_8ah(); break;
		case 0x90: CPU_AH = 0x00; break; // from DOSBox
		case 0x91: CPU_AH = 0x00; break; // from DOSBox
		case 0xc0: pcbios_int_15h_c0h(); break;
#ifndef EXT_BIOS_TOP
		case 0xc1:
#endif
		case 0xc3: // PS50+ ???
		case 0xc4:
			CPU_AH = 0x86;
			CPU_SET_C_FLAG(1);
			break;
#ifdef EXT_BIOS_TOP
		case 0xc1: pcbios_int_15h_c1h(); break;
#endif
		case 0xc2: pcbios_int_15h_c2h(); break;
#if defined(HAS_I386)
		case 0xc9: pcbios_int_15h_c9h(); break;
#endif
		case 0xca: pcbios_int_15h_cah(); break;
		case 0xe8: pcbios_int_15h_e8h(); break;
		default:
			unimplemented_15h("int %02Xh (AX=%04X BX=%04X CX=%04X DX=%04X SI=%04X DI=%04X DS=%04X ES=%04X)\n", num, CPU_AX, CPU_BX, CPU_CX, CPU_DX, CPU_SI, CPU_DI, CPU_DS, CPU_ES);
			CPU_AH = 0x86;
			CPU_SET_C_FLAG(1);
			break;
		}
		break;
	case 0x16:
		// PC BIOS - Keyboard
		CPU_SET_C_FLAG(0);
		switch(CPU_AH) {
		case 0x00: pcbios_int_16h_00h(); break;
		case 0x01: pcbios_int_16h_01h(); break;
		case 0x02: pcbios_int_16h_02h(); break;
		case 0x03: pcbios_int_16h_03h(); break;
		case 0x05: pcbios_int_16h_05h(); break;
		case 0x09: pcbios_int_16h_09h(); break;
		case 0x0a: pcbios_int_16h_0ah(); break;
		case 0x10: pcbios_int_16h_00h(); break;
		case 0x11: pcbios_int_16h_11h(); break;
		case 0x12: pcbios_int_16h_12h(); break;
		case 0x13: pcbios_int_16h_13h(); break;
		case 0x14: pcbios_int_16h_14h(); break;
		case 0x51: pcbios_int_16h_51h(); break;
		case 0x55: pcbios_int_16h_55h(); break;
		case 0x6f: pcbios_int_16h_6fh(); break;
		case 0xda: break; // unknown
		case 0xdb: break; // unknown
		case 0xff: break; // unknown
		default:
			unimplemented_16h("int %02Xh (AX=%04X BX=%04X CX=%04X DX=%04X SI=%04X DI=%04X DS=%04X ES=%04X)\n", num, CPU_AX, CPU_BX, CPU_CX, CPU_DX, CPU_SI, CPU_DI, CPU_DS, CPU_ES);
			break;
		}
		break;
	case 0x17:
		// PC BIOS - Printer
		CPU_SET_C_FLAG(0);
		switch(CPU_AH) {
		case 0x00: pcbios_int_17h_00h(); break;
		case 0x01: pcbios_int_17h_01h(); break;
		case 0x02: pcbios_int_17h_02h(); break;
		case 0x03: pcbios_int_17h_03h(); break;
		case 0x50: pcbios_int_17h_50h(); break;
		case 0x51: pcbios_int_17h_51h(); break;
		case 0x52: pcbios_int_17h_52h(); break;
		case 0x84: pcbios_int_17h_84h(); break;
		case 0x85: pcbios_int_17h_85h(); break;
		default:
			unimplemented_17h("int %02Xh (AX=%04X BX=%04X CX=%04X DX=%04X SI=%04X DI=%04X DS=%04X ES=%04X)\n", num, CPU_AX, CPU_BX, CPU_CX, CPU_DX, CPU_SI, CPU_DI, CPU_DS, CPU_ES);
			break;
		}
		break;
	case 0x1a:
		// PC BIOS - Timer
		CPU_SET_C_FLAG(0);
		switch(CPU_AH) {
		case 0x00: pcbios_int_1ah_00h(); break;
		case 0x01: break;
		case 0x02: pcbios_int_1ah_02h(); break;
		case 0x03: break;
		case 0x04: pcbios_int_1ah_04h(); break;
		case 0x05: break;
		case 0x0a: pcbios_int_1ah_0ah(); break;
		case 0x0b: break;
		case 0x35: break; // Word Perfect Third Party Interface?
		case 0x36: break; // Word Perfect Third Party Interface
		case 0x70: break; // SNAP? (Simple Network Application Protocol)
		case 0xb0: break; // Microsoft Real-Time Compression Interface (MRCI)
		case 0xb1: break; // PCI BIOS v2.0c+
		case 0xb4: break; // Intel Plug-and-Play Auto-Configuration
		default:
			unimplemented_1ah("int %02Xh (AX=%04X BX=%04X CX=%04X DX=%04X SI=%04X DI=%04X DS=%04X ES=%04X)\n", num, CPU_AX, CPU_BX, CPU_CX, CPU_DX, CPU_SI, CPU_DI, CPU_DS, CPU_ES);
			break;
		}
		break;
	case 0x1b:
		mem[0x471] = 0x00;
		break;
	case 0x20:
		try {
			msdos_process_terminate(CPU_CS, retval, 1);
		} catch(...) {
			fatalerror("failed to terminate the process (PSP=%04X) by int 20h\n", CPU_CS);
		}
		break;
	case 0x30:
		// dummy interrupt for case map routine pointed in the country info
//		if(!(CPU_CL >= 0x00 && CPU_CL <= 0x24)) {
//			CPU_AL = 0x00;
//			break;
//		}
	case 0x21:
		// MS-DOS System Call
		CPU_SET_C_FLAG(0);
		try {
			switch(num == 0x21 ? CPU_AH : CPU_CL) {
			case 0x00: msdos_int_21h_00h(); break;
			case 0x01: msdos_int_21h_01h(); break;
			case 0x02: msdos_int_21h_02h(); break;
			case 0x03: msdos_int_21h_03h(); break;
			case 0x04: msdos_int_21h_04h(); break;
			case 0x05: msdos_int_21h_05h(); break;
			case 0x06: msdos_int_21h_06h(); break;
			case 0x07: msdos_int_21h_07h(); break;
			case 0x08: msdos_int_21h_08h(); break;
			case 0x09: msdos_int_21h_09h(); break;
			case 0x0a: msdos_int_21h_0ah(); break;
			case 0x0b: msdos_int_21h_0bh(); break;
			case 0x0c: msdos_int_21h_0ch(); break;
			case 0x0d: msdos_int_21h_0dh(); break;
			case 0x0e: msdos_int_21h_0eh(); break;
			case 0x0f: msdos_int_21h_0fh(); break;
			case 0x10: msdos_int_21h_10h(); break;
			case 0x11: msdos_int_21h_11h(); break;
			case 0x12: msdos_int_21h_12h(); break;
			case 0x13: msdos_int_21h_13h(); break;
			case 0x14: msdos_int_21h_14h(); break;
			case 0x15: msdos_int_21h_15h(); break;
			case 0x16: msdos_int_21h_16h(); break;
			case 0x17: msdos_int_21h_17h(); break;
			case 0x18: msdos_int_21h_18h(); break;
			case 0x19: msdos_int_21h_19h(); break;
			case 0x1a: msdos_int_21h_1ah(); break;
			case 0x1b: msdos_int_21h_1bh(); break;
			case 0x1c: msdos_int_21h_1ch(); break;
			case 0x1d: msdos_int_21h_1dh(); break;
			case 0x1e: msdos_int_21h_1eh(); break;
			case 0x1f: msdos_int_21h_1fh(); break;
			case 0x20: msdos_int_21h_20h(); break;
			case 0x21: msdos_int_21h_21h(); break;
			case 0x22: msdos_int_21h_22h(); break;
			case 0x23: msdos_int_21h_23h(); break;
			case 0x24: msdos_int_21h_24h(); break;
			case 0x25: msdos_int_21h_25h(); break;
			case 0x26: msdos_int_21h_26h(); break;
			case 0x27: msdos_int_21h_27h(); break;
			case 0x28: msdos_int_21h_28h(); break;
			case 0x29: msdos_int_21h_29h(); break;
			case 0x2a: msdos_int_21h_2ah(); break;
			case 0x2b: msdos_int_21h_2bh(); break;
			case 0x2c: msdos_int_21h_2ch(); break;
			case 0x2d: msdos_int_21h_2dh(); break;
			case 0x2e: msdos_int_21h_2eh(); break;
			case 0x2f: msdos_int_21h_2fh(); break;
			case 0x30: msdos_int_21h_30h(); break;
			case 0x31: msdos_int_21h_31h(); break;
			case 0x32: msdos_int_21h_32h(); break;
			case 0x33: msdos_int_21h_33h(); break;
			case 0x34: msdos_int_21h_34h(); break;
			case 0x35: msdos_int_21h_35h(); break;
			case 0x36: msdos_int_21h_36h(); break;
			case 0x37: msdos_int_21h_37h(); break;
			case 0x38: msdos_int_21h_38h(); break;
			case 0x39: msdos_int_21h_39h(0); break;
			case 0x3a: msdos_int_21h_3ah(0); break;
			case 0x3b: msdos_int_21h_3bh(0); break;
			case 0x3c: msdos_int_21h_3ch(); break;
			case 0x3d: msdos_int_21h_3dh(); break;
			case 0x3e: msdos_int_21h_3eh(); break;
			case 0x3f: msdos_int_21h_3fh(); break;
			case 0x40: msdos_int_21h_40h(); break;
			case 0x41: msdos_int_21h_41h(0); break;
			case 0x42: msdos_int_21h_42h(); break;
			case 0x43: msdos_int_21h_43h(0); break;
			case 0x44: msdos_int_21h_44h(); break;
			case 0x45: msdos_int_21h_45h(); break;
			case 0x46: msdos_int_21h_46h(); break;
			case 0x47: msdos_int_21h_47h(0); break;
			case 0x48: msdos_int_21h_48h(); break;
			case 0x49: msdos_int_21h_49h(); break;
			case 0x4a: msdos_int_21h_4ah(); break;
			case 0x4b: msdos_int_21h_4bh(); break;
			case 0x4c: msdos_int_21h_4ch(); break;
			case 0x4d: msdos_int_21h_4dh(); break;
			case 0x4e: msdos_int_21h_4eh(); break;
			case 0x4f: msdos_int_21h_4fh(); break;
			case 0x50: msdos_int_21h_50h(); break;
			case 0x51: msdos_int_21h_51h(); break;
			case 0x52: msdos_int_21h_52h(); break;
			case 0x53: msdos_int_21h_53h(); break;
			case 0x54: msdos_int_21h_54h(); break;
			case 0x55: msdos_int_21h_55h(); break;
			case 0x56: msdos_int_21h_56h(0); break;
			case 0x57: msdos_int_21h_57h(); break;
			case 0x58: msdos_int_21h_58h(); break;
			case 0x59: msdos_int_21h_59h(); break;
			case 0x5a: msdos_int_21h_5ah(); break;
			case 0x5b: msdos_int_21h_5bh(); break;
			case 0x5c: msdos_int_21h_5ch(); break;
			case 0x5d: msdos_int_21h_5dh(); break;
			case 0x5e: msdos_int_21h_5eh(); break;
			case 0x5f: msdos_int_21h_5fh(); break;
			case 0x60: msdos_int_21h_60h(0); break;
			case 0x61: msdos_int_21h_61h(); break;
			case 0x62: msdos_int_21h_62h(); break;
			case 0x63: msdos_int_21h_63h(); break;
			// 0x64: Set Device Driver Lockahead Flag
			case 0x65: msdos_int_21h_65h(); break;
			case 0x66: msdos_int_21h_66h(); break;
			case 0x67: msdos_int_21h_67h(); break;
			case 0x68: msdos_int_21h_68h(); break;
			case 0x69: msdos_int_21h_69h(); break;
			case 0x6a: msdos_int_21h_6ah(); break;
			case 0x6b: msdos_int_21h_6bh(); break;
			case 0x6c: msdos_int_21h_6ch(0); break;
			case 0x6d: // Find First ROM Program
			case 0x6e: // Find Next ROM Program
			case 0x6f: // Get/Set ROM Scan Start Address
				CPU_AL = 0x00; // if not supported (DOS <5, MS-DOS 5+ non-ROM versions)
				break;
			case 0x70: msdos_int_21h_70h(); break;
			case 0x71: // Windows95 - Long Filename Functions
				switch(CPU_AL) {
				case 0x0d: msdos_int_21h_710dh(); break;
				case 0x39: msdos_int_21h_39h(1); break;
				case 0x3a: msdos_int_21h_3ah(1); break;
				case 0x3b: msdos_int_21h_3bh(1); break;
				case 0x41: msdos_int_21h_7141h(); break;
				case 0x43: msdos_int_21h_43h(1); break;
				case 0x47: msdos_int_21h_47h(1); break;
				case 0x4e: msdos_int_21h_714eh(); break;
				case 0x4f: msdos_int_21h_714fh(); break;
				case 0x56: msdos_int_21h_56h(1); break;
				case 0x60: msdos_int_21h_60h(1); break;
				case 0x6c: msdos_int_21h_6ch(1); break;
				case 0xa0: msdos_int_21h_71a0h(); break;
				case 0xa1: msdos_int_21h_71a1h(); break;
				case 0xa6: msdos_int_21h_71a6h(); break;
				case 0xa7: msdos_int_21h_71a7h(); break;
				case 0xa8: msdos_int_21h_71a8h(); break;
				case 0xa9: msdos_int_21h_6ch(1); break;
				case 0xaa: msdos_int_21h_71aah(); break;
				default:
					unimplemented_21h("int %02Xh (AX=%04X BX=%04X CX=%04X DX=%04X SI=%04X DI=%04X DS=%04X ES=%04X)\n", num, CPU_AX, CPU_BX, CPU_CX, CPU_DX, CPU_SI, CPU_DI, CPU_DS, CPU_ES);
					CPU_AX = 0x7100;
					CPU_SET_C_FLAG(1);
					break;
				}
				break;
			case 0x72: // Windows95 beta - LFN FindClose
//				unimplemented_21h("int %02Xh (AX=%04X BX=%04X CX=%04X DX=%04X SI=%04X DI=%04X DS=%04X ES=%04X)\n", 0x21, CPU_AX, CPU_BX, CPU_CX, CPU_DX, CPU_SI, CPU_DI, CPU_DS, CPU_ES);
				CPU_AX = 0x7200;
				CPU_SET_C_FLAG(1);
				break;
			case 0x73: // Windows95 - FAT32 Functions
				switch(CPU_AL) {
				case 0x00: msdos_int_21h_7300h(); break;
				// 0x01: Set Drive Locking ???
				case 0x02: msdos_int_21h_7302h(); break;
				case 0x03: msdos_int_21h_7303h(); break;
				// 0x04: Set DPB to Use for Formatting
				// 0x05: Extended Absolute Disk Read/Write
				default:
					unimplemented_21h("int %02Xh (AX=%04X BX=%04X CX=%04X DX=%04X SI=%04X DI=%04X DS=%04X ES=%04X)\n", num, CPU_AX, CPU_BX, CPU_CX, CPU_DX, CPU_SI, CPU_DI, CPU_DS, CPU_ES);
					CPU_AX = 0x7300;
					CPU_SET_C_FLAG(1);
					break;
				}
				break;
			case 0xdb: msdos_int_21h_dbh(); break;
			case 0xdc: msdos_int_21h_dch(); break;
			default:
				unimplemented_21h("int %02Xh (AX=%04X BX=%04X CX=%04X DX=%04X SI=%04X DI=%04X DS=%04X ES=%04X)\n", num, CPU_AX, CPU_BX, CPU_CX, CPU_DX, CPU_SI, CPU_DI, CPU_DS, CPU_ES);
				CPU_AX = 0x01;
				CPU_SET_C_FLAG(1);
				break;
			}
		} catch(int error) {
			CPU_AX = error;
			CPU_SET_C_FLAG(1);
		} catch(...) {
			CPU_AX = 0x1f; // general failure
			CPU_SET_C_FLAG(1);
		}
		if(CPU_C_FLAG) {
			sda_t *sda = (sda_t *)(mem + SDA_TOP);
			sda->int21h_5d0ah_called = 0;
			sda->extended_error_code = CPU_AX;
			switch(sda->extended_error_code) {
			case  4: // Too many open files
			case  8: // Insufficient memory
				sda->error_class = 1; // Out of resource
				break;
			case  5: // Access denied
				sda->error_class = 3; // Authorization
				break;
			case  7: // Memory control block destroyed
				sda->error_class = 4; // Internal
				break;
			case  2: // File not found
			case  3: // Path not found
			case 15: // Invaid drive specified
			case 18: // No more files
				sda->error_class = 8; // Not found
				break;
			case 32: // Sharing violation
			case 33: // Lock violation
				sda->error_class = 10; // Locked
				break;
//			case 16: // Removal of current directory attempted
			case 19: // Attempted write on protected disk
			case 21: // Drive not ready
//			case 29: // Write failure
//			case 30: // Read failure
//			case 82: // Cannot create subdirectory
				sda->error_class = 11; // Media
				break;
			case 80: // File already exists
				sda->error_class = 12; // Already exist
				break;
			default:
				sda->error_class = 13; // Unknown
				break;
			}
			sda->suggested_action = 1; // Retry
			sda->locus_of_last_error = 1; // Unknown
		}
		if(ctrl_break_checking && ctrl_break_detected) {
			// raise int 23h
			CPU_SOFT_INTERRUPT(0x23);
		}
		break;
	case 0x22:
		fatalerror("int 22h (terminate address)\n");
	case 0x23:
		try {
			msdos_process_terminate(current_psp, (retval & 0xff) | 0x100, 1);
		} catch(...) {
			fatalerror("failed to terminate the current process by int 23h\n");
		}
		break;
	case 0x24:
/*
		try {
			msdos_process_terminate(current_psp, (retval & 0xff) | 0x200, 1);
		} catch(...) {
			fatalerror("failed to terminate the current process by int 24h\n");
		}
*/
		msdos_int_24h();
		break;
	case 0x25:
		msdos_int_25h();
		break;
	case 0x26:
		msdos_int_26h();
		break;
	case 0x27:
		try {
			msdos_int_27h();
		} catch(...) {
			fatalerror("failed to terminate the process (PSP=%04X) by int 27h\n", CPU_CS);
		}
		break;
	case 0x28:
		Sleep(10);
		REQUEST_HARDWRE_UPDATE();
		break;
	case 0x29:
		msdos_int_29h();
		break;
	case 0x2e:
		msdos_int_2eh();
		break;
	case 0x2f:
		// multiplex interrupt
		switch(CPU_AH) {
		case 0x05: msdos_int_2fh_05h(); break;
		case 0x06: msdos_int_2fh_06h(); break;
		case 0x11: msdos_int_2fh_11h(); break;
		case 0x12: msdos_int_2fh_12h(); break;
		case 0x13: msdos_int_2fh_13h(); break;
		case 0x14: msdos_int_2fh_14h(); break;
		case 0x15: msdos_int_2fh_15h(); break;
		case 0x16: msdos_int_2fh_16h(); break;
		case 0x19: msdos_int_2fh_19h(); break;
		case 0x1a: msdos_int_2fh_1ah(); break;
		case 0x40: msdos_int_2fh_40h(); break;
		case 0x43: msdos_int_2fh_43h(); break;
		case 0x46: msdos_int_2fh_46h(); break;
		case 0x48: msdos_int_2fh_48h(); break;
		case 0x4a: msdos_int_2fh_4ah(); break;
		case 0x4b: msdos_int_2fh_4bh(); break;
		case 0x4d: msdos_int_2fh_4dh(); break;
		case 0x4f: msdos_int_2fh_4fh(); break;
		case 0x55: msdos_int_2fh_55h(); break;
		case 0x56: msdos_int_2fh_56h(); break;
		case 0xad: msdos_int_2fh_adh(); break;
		case 0xae: msdos_int_2fh_aeh(); break;
		case 0xb7: msdos_int_2fh_b7h(); break;
		default:
			switch(CPU_AL) {
			case 0x00:
				// This is not installed
//				CPU_AL = 0x00;
				break;
			case 0x01:
				// Quarterdeck RPCI - QEMM/QRAM - PCL-838.EXE is not installed
				if(CPU_AH == 0xd2 && CPU_BX == 0x5145 && CPU_CX == 0x4d4d && CPU_DX == 0x3432) {
					break;
				}
				// Banyan VINES v4+ is not installed
				if(CPU_AH == 0xd7 && CPU_BX == 0x0000) {
					break;
				}
				// Quarterdeck QDPMI.SYS v1.0 is not installed
				if(CPU_AH == 0xde && CPU_BX == 0x4450 && CPU_CX == 0x4d49 && CPU_DX == 0x8f4f) {
					break;
				}
			default:
				// NORTON UTILITIES 5.0+
				if(CPU_AH == 0xfe && CPU_DI == 0x4e55) {
					break;
				}
				// Borland C++ DPMILOAD.EXE is not installed
				if((CPU_AX == 0xfb42 && CPU_BX == 0x0014) || (CPU_AX == 0xfb43 && CPU_BX == 0x0100)) {
					break;
				}
				unimplemented_2fh("int %02Xh (AX=%04X BX=%04X CX=%04X DX=%04X SI=%04X DI=%04X DS=%04X ES=%04X)\n", 0x2f, CPU_AX, CPU_BX, CPU_CX, CPU_DX, CPU_SI, CPU_DI, CPU_DS, CPU_ES);
				CPU_AX = 0x01; // invalid function
				CPU_SET_C_FLAG(1);
				break;
			}
			break;
		}
		break;
	case 0x33:
		switch(CPU_AH) {
		case 0x00:
			// Mouse
			switch(CPU_AX) {
			case 0x0000: msdos_int_33h_0000h(); break;
			case 0x0001: msdos_int_33h_0001h(); break;
			case 0x0002: msdos_int_33h_0002h(); break;
			case 0x0003: msdos_int_33h_0003h(); break;
			case 0x0004: msdos_int_33h_0004h(); break;
			case 0x0005: msdos_int_33h_0005h(); break;
			case 0x0006: msdos_int_33h_0006h(); break;
			case 0x0007: msdos_int_33h_0007h(); break;
			case 0x0008: msdos_int_33h_0008h(); break;
			case 0x0009: msdos_int_33h_0009h(); break;
			case 0x000a: msdos_int_33h_000ah(); break;
			case 0x000b: msdos_int_33h_000bh(); break;
			case 0x000c: msdos_int_33h_000ch(); break;
			case 0x000d: break; // MS MOUSE v1.0+ - Light Pen Emulation On
			case 0x000e: break; // MS MOUSE v1.0+ - Light Pen Emulation Off
			case 0x000f: msdos_int_33h_000fh(); break;
			case 0x0010: break; // MS MOUSE v1.0+ - Define Screen Region for Updating
			case 0x0011: msdos_int_33h_0011h(); break;
			case 0x0012: CPU_AX = 0xffff; break; // MS MOUSE - Set Large Graphics Cursor Block
			case 0x0013: break; // MS MOUSE v5.0+ - Define Double-Speed Threshold
			case 0x0014: msdos_int_33h_0014h(); break;
			case 0x0015: msdos_int_33h_0015h(); break;
			case 0x0016: msdos_int_33h_0016h(); break;
			case 0x0017: msdos_int_33h_0017h(); break;
			case 0x0018: msdos_int_33h_0018h(); break;
			case 0x0019: msdos_int_33h_0019h(); break;
			case 0x001a: msdos_int_33h_001ah(); break;
			case 0x001b: msdos_int_33h_001bh(); break;
			case 0x001c: break; // MS MOUSE v6.0+ - Set Interrupt Rate
			case 0x001d: msdos_int_33h_001dh(); break;
			case 0x001e: msdos_int_33h_001eh(); break;
			case 0x001f: msdos_int_33h_001fh(); break;
			case 0x0020: msdos_int_33h_0020h(); break;
			case 0x0021: msdos_int_33h_0021h(); break;
			case 0x0022: msdos_int_33h_0022h(); break;
			case 0x0023: msdos_int_33h_0023h(); break;
			case 0x0024: msdos_int_33h_0024h(); break;
			case 0x0025: msdos_int_33h_0025h(); break;
			case 0x0026: msdos_int_33h_0026h(); break;
			case 0x0027: msdos_int_33h_0027h(); break;
			case 0x0028: msdos_int_33h_0028h(); break;
			case 0x0029: msdos_int_33h_0029h(); break;
			case 0x002a: msdos_int_33h_002ah(); break;
			// 0x002b: MS MOUSE v7.0+ - Load Acceleration Profiles
			// 0x002c: MS MOUSE v7.0+ - Get Acceleration Profiles
			// 0x002d: MS MOUSE v7.0+ - Select Acceleration Profile
			// 0x002e: MS MOUSE v8.10+ - Set Acceleration Profile Names
			case 0x002f: break; // Mouse Hardware Reset
			// 0x0030: MS MOUSE v7.04+ - Get/Set BallPoint Information
			case 0x0031: msdos_int_33h_0031h(); break;
			case 0x0032: msdos_int_33h_0032h(); break;
			// 0x0033: MS MOUSE v7.05+ - Get Switch Settings And Acceleration Profile Data
			// 0x0034: MS MOUSE v8.0+ - Get Initialization File
			// 0x0035: MS MOUSE v8.10+ - LCD Screen Large Pointer Support
			case 0x004d: msdos_int_33h_004dh(); break;
			case 0x006d: msdos_int_33h_006dh(); break;
			default:
				unimplemented_33h("int %02Xh (AX=%04X BX=%04X CX=%04X DX=%04X SI=%04X DI=%04X DS=%04X ES=%04X)\n", num, CPU_AX, CPU_BX, CPU_CX, CPU_DX, CPU_SI, CPU_DI, CPU_DS, CPU_ES);
				break;
			}
			break;
		default:
			unimplemented_33h("int %02Xh (AX=%04X BX=%04X CX=%04X DX=%04X SI=%04X DI=%04X DS=%04X ES=%04X)\n", num, CPU_AX, CPU_BX, CPU_CX, CPU_DX, CPU_SI, CPU_DI, CPU_DS, CPU_ES);
			break;
		}
		break;
	case 0x65:
		// dummy interrupt for EMS (int 67h)
		switch(CPU_AH) {
		case 0x40: msdos_int_67h_40h(); break;
		case 0x41: msdos_int_67h_41h(); break;
		case 0x42: msdos_int_67h_42h(); break;
		case 0x43: msdos_int_67h_43h(); break;
		case 0x44: msdos_int_67h_44h(); break;
		case 0x45: msdos_int_67h_45h(); break;
		case 0x46: msdos_int_67h_46h(); break;
		case 0x47: msdos_int_67h_47h(); break;
		case 0x48: msdos_int_67h_48h(); break;
		// 0x49: LIM EMS - Reserved - Get I/O Port Address (Undocumented in EMS 3.2)
		// 0x4a: LIM EMS - Reserved - Get Translation Array (Undocumented in EMS 3.2)
		case 0x4b: msdos_int_67h_4bh(); break;
		case 0x4c: msdos_int_67h_4ch(); break;
		case 0x4d: msdos_int_67h_4dh(); break;
		case 0x4e: msdos_int_67h_4eh(); break;
		case 0x4f: msdos_int_67h_4fh(); break;
		case 0x50: msdos_int_67h_50h(); break;
		case 0x51: msdos_int_67h_51h(); break;
		case 0x52: msdos_int_67h_52h(); break;
		case 0x53: msdos_int_67h_53h(); break;
		case 0x54: msdos_int_67h_54h(); break;
		case 0x55: msdos_int_67h_55h(); break;
		case 0x56: msdos_int_67h_56h(); break;
		case 0x57: msdos_int_67h_57h(); break;
		case 0x58: msdos_int_67h_58h(); break;
		case 0x59: msdos_int_67h_59h(); break;
		case 0x5a: msdos_int_67h_5ah(); break;
		case 0x5b: msdos_int_67h_5bh(); break;
		// 0x5c: LIM EMS 4.0 - Prepare Expanded Memory Hardware For Warm Boot
		case 0x5d: msdos_int_67h_5dh(); break;
		case 0x70: msdos_int_67h_70h(); break;
		// 0xde: VCPI
		case 0xde: msdos_int_67h_deh(); break;
		default:
			unimplemented_67h("int %02Xh (AX=%04X BX=%04X CX=%04X DX=%04X SI=%04X DI=%04X DS=%04X ES=%04X)\n", 0x67, CPU_AX, CPU_BX, CPU_CX, CPU_DX, CPU_SI, CPU_DI, CPU_DS, CPU_ES);
			CPU_AH = 0x84;
			break;
		}
		break;
#ifdef SUPPORT_XMS
	case 0x66:
		// dummy interrupt for XMS (call far)
		try {
			switch(CPU_AH) {
			case 0x00: msdos_call_xms_00h(); break;
			case 0x01: msdos_call_xms_01h(); break;
			case 0x02: msdos_call_xms_02h(); break;
			case 0x03: msdos_call_xms_03h(); break;
			case 0x04: msdos_call_xms_04h(); break;
			case 0x05: msdos_call_xms_05h(); break;
			case 0x06: msdos_call_xms_06h(); break;
			case 0x07: msdos_call_xms_07h(); break;
			case 0x08: msdos_call_xms_08h(); break;
			case 0x09: msdos_call_xms_09h(); break;
			case 0x0a: msdos_call_xms_0ah(); break;
			case 0x0b: msdos_call_xms_0bh(); break;
			case 0x0c: msdos_call_xms_0ch(); break;
			case 0x0d: msdos_call_xms_0dh(); break;
			case 0x0e: msdos_call_xms_0eh(); break;
			case 0x0f: msdos_call_xms_0fh(); break;
			case 0x10: msdos_call_xms_10h(); break;
			case 0x11: msdos_call_xms_11h(); break;
			case 0x12: msdos_call_xms_12h(); break;
#if defined(HAS_I386)
			case 0x88: msdos_call_xms_88h(); break;
			case 0x89: msdos_call_xms_89h(); break;
			case 0x8e: msdos_call_xms_8eh(); break;
			case 0x8f: msdos_call_xms_8fh(); break;
#endif
			default:
				unimplemented_xms("call XMS (AX=%04X BX=%04X CX=%04X DX=%04X SI=%04X DI=%04X DS=%04X ES=%04X)\n", CPU_AX, CPU_BX, CPU_CX, CPU_DX, CPU_SI, CPU_DI, CPU_DS, CPU_ES);
				CPU_AX = 0x0000;
				CPU_BL = 0x80; // function not implemented
				break;
			}
		} catch(...) {
			CPU_AX = 0x0000;
			CPU_BL = 0x8f; // unrecoverable driver error
		}
		break;
#endif
/*
	case 0x67:
		// int 67h handler is in EMS device driver (EMMXXXX0) and it calls int 65h
		// NOTE: some softwares get address of int 67h handler and recognize the address is in EMS device driver
		break;
*/
	case 0x69:
		// irq12 (mouse)
		mouse_push_ax = CPU_AX;
		mouse_push_bx = CPU_BX;
		mouse_push_cx = CPU_CX;
		mouse_push_dx = CPU_DX;
		mouse_push_si = CPU_SI;
		mouse_push_di = CPU_DI;
		
		if(mouse.status_irq && mouse.call_addr.dw) {
			CPU_AX = mouse.status_irq;
			CPU_BX = mouse.get_buttons();
			CPU_CX = max(mouse.min_position.x & ~7, min(mouse.max_position.x & ~7, mouse.position.x));
			CPU_DX = max(mouse.min_position.y & ~7, min(mouse.max_position.y & ~7, mouse.position.y));
			CPU_SI = CPU_CX * mouse.mickey.x / 8;
			CPU_DI = CPU_DX * mouse.mickey.y / 8;
			
			mem[DUMMY_TOP + 0x02] = 0x9a;	// call far
			mem[DUMMY_TOP + 0x03] = mouse.call_addr.w.l & 0xff;
			mem[DUMMY_TOP + 0x04] = mouse.call_addr.w.l >> 8;
			mem[DUMMY_TOP + 0x05] = mouse.call_addr.w.h & 0xff;
			mem[DUMMY_TOP + 0x06] = mouse.call_addr.w.h >> 8;
			mem[DUMMY_TOP + 0x07] = 0xcd;	// int 6bh (dummy)
			mem[DUMMY_TOP + 0x08] = 0x6b;
			break;
		}
		for(int i = 0; i < 8; i++) {
			if((mouse.status_irq_alt & (1 << i)) && mouse.call_addr_alt[i].dw) {
				CPU_AX = mouse.status_irq_alt;
				CPU_BX = mouse.get_buttons();
				CPU_CX = max(mouse.min_position.x & ~7, min(mouse.max_position.x & ~7, mouse.position.x));
				CPU_DX = max(mouse.min_position.y & ~7, min(mouse.max_position.y & ~7, mouse.position.y));
				CPU_SI = CPU_CX * mouse.mickey.x / 8;
				CPU_DI = CPU_DX * mouse.mickey.y / 8;
				
				mem[DUMMY_TOP + 0x02] = 0x9a;	// call far
				mem[DUMMY_TOP + 0x03] = mouse.call_addr_alt[i].w.l & 0xff;
				mem[DUMMY_TOP + 0x04] = mouse.call_addr_alt[i].w.l >> 8;
				mem[DUMMY_TOP + 0x05] = mouse.call_addr_alt[i].w.h & 0xff;
				mem[DUMMY_TOP + 0x06] = mouse.call_addr_alt[i].w.h >> 8;
				mem[DUMMY_TOP + 0x07] = 0xcd;	// int 6bh (dummy)
				mem[DUMMY_TOP + 0x08] = 0x6b;
				break;
			}
		}
		if(mouse.status_irq_ps2 && mouse.call_addr_ps2.dw && mouse.enabled_ps2) {
			UINT16 data_1st, data_2nd, data_3rd;
			pcbios_read_from_ps2_mouse(&data_1st, &data_2nd, &data_3rd);
			CPU_PUSH(data_1st);
			CPU_PUSH(data_2nd);
			CPU_PUSH(data_3rd);
			CPU_PUSH(0x0000);
			
			mem[DUMMY_TOP + 0x02] = 0x9a;	// call far
			mem[DUMMY_TOP + 0x03] = mouse.call_addr_ps2.w.l & 0xff;
			mem[DUMMY_TOP + 0x04] = mouse.call_addr_ps2.w.l >> 8;
			mem[DUMMY_TOP + 0x05] = mouse.call_addr_ps2.w.h & 0xff;
			mem[DUMMY_TOP + 0x06] = mouse.call_addr_ps2.w.h >> 8;
			mem[DUMMY_TOP + 0x07] = 0xcd;	// int 6ah (dummy)
			mem[DUMMY_TOP + 0x08] = 0x6a;
			break;
		}
		// invalid call addr :-(
		mem[DUMMY_TOP + 0x02] = 0x90;	// nop
		mem[DUMMY_TOP + 0x03] = 0x90;	// nop
		mem[DUMMY_TOP + 0x04] = 0x90;	// nop
		mem[DUMMY_TOP + 0x05] = 0x90;	// nop
		mem[DUMMY_TOP + 0x06] = 0x90;	// nop
		mem[DUMMY_TOP + 0x07] = 0xcd;	// int 6bh (dummy)
		mem[DUMMY_TOP + 0x08] = 0x6b;
		break;
	case 0x6a:
		// end of ps/2 mouse bios
		CPU_POP();
		CPU_POP();
		CPU_POP();
		CPU_POP();
	case 0x6b:
		// end of irq12 (mouse)
		CPU_AX = mouse_push_ax;
		CPU_BX = mouse_push_bx;
		CPU_CX = mouse_push_cx;
		CPU_DX = mouse_push_dx;
		CPU_SI = mouse_push_si;
		CPU_DI = mouse_push_di;
		
		// EOI
		if((pic[1].isr &= ~(1 << 4)) == 0) {
			pic[0].isr &= ~(1 << 2); // master
		}
		pic_update();
		break;
	case 0x6c:
		// dummy interrupt for case map routine pointed in the country info
		if(CPU_AL >= 0x80) {
			char tmp[2] = {0};
			tmp[0] = CPU_AL;
			my_strupr(tmp);
			CPU_AL = tmp[0];
		}
		break;
	case 0x6d:
		// dummy interrupt for font read routine pointed by int 15h, ax=5000h
		CPU_AL = 0x86; // not supported
		CPU_SET_C_FLAG(1);
		break;
	case 0x6e:
		// dummy interrupt for parameter error message read routine pointed by int 2fh, ax=122eh, dl=08h
		{
			UINT16 code = CPU_AX;
			if(code & 0xf0) {
				code = (code & 7) | ((code & 0x10) >> 1);
			}
			for(int i = 0; i < array_length(param_error_table); i++) {
				if(param_error_table[i].code == code || param_error_table[i].code == (UINT16)-1) {
					const char *message = NULL;
					if(active_code_page == 932) {
						message = param_error_table[i].message_japanese;
					}
					if(message == NULL) {
						message = param_error_table[i].message_english;
					}
					*(UINT8 *)(mem + WORK_TOP) = strlen(message);
					strcpy((char *)(mem + WORK_TOP + 1), message);
					
					CPU_LOAD_SREG(CPU_ES_INDEX, WORK_TOP >> 4);
					CPU_DI = 0x0000;
					break;
				}
			}
		}
		break;
	case 0x6f:
		// dummy interrupt for end of alter page map and call
		{
			UINT16 handles[4], pages[4];
			
			// pop old mapping data in new mapping
			for(int i = 0; i < 4; i++) {
				pages  [3 - i] = CPU_POP();
				handles[3 - i] = CPU_POP();
			}
			
			// restore old mapping
			for(int i = 0; i < 4; i++) {
				UINT16 handle = handles[i];
				UINT16 page   = pages  [i];
				
				if(handle >= 1 && handle <= MAX_EMS_HANDLES && ems_handles[handle].allocated && page < ems_handles[handle].pages) {
					ems_map_page(i, handle, page);
				} else {
					ems_unmap_page(i);
				}
			}
			// do ret_far (pop cs/ip) in old mapping
		}
		break;
	case 0x70:
	case 0x71:
	case 0x72:
	case 0x73:
	case 0x74:
	case 0x75:
	case 0x76:
	case 0x77:
		// EOI
		if((pic[1].isr &= ~(1 << (num - 0x70))) == 0) {
			pic[0].isr &= ~(1 << 2); // master
		}
		pic_update();
		break;
	default:
//		fatalerror("int %02Xh (AX=%04X BX=%04X CX=%04X DX=%04X SI=%04X DI=%04X DS=%04X ES=%04X)\n", num, CPU_AX, CPU_BX, CPU_CX, CPU_DX, CPU_SI, CPU_DI, CPU_DS, CPU_ES);
		break;
	}
	
	// update cursor position
	if(cursor_moved) {
		pcbios_update_cursor_position();
		cursor_moved = false;
	}
}

// init

int msdos_init(int argc, char *argv[], char *envp[], int standard_env)
{
	// init file handler
	memset(file_handler, 0, sizeof(file_handler));
	msdos_file_handler_open(0, "STDIN", _isatty(0), 0, 0x80d3, 0);
	msdos_file_handler_open(1, "STDOUT", _isatty(1), 1, 0x80d3, 0);
	msdos_file_handler_open(2, "STDERR", _isatty(2), 1, 0x80d3, 0);
#ifdef MAP_AUX_DEVICE_TO_FILE
	if(_open("stdaux.txt", _O_RDWR | _O_BINARY | _O_CREAT | _O_TRUNC, _S_IREAD | _S_IWRITE) == 3) {
#else
	if(_open("NUL", _O_RDWR | _O_BINARY | _O_CREAT | _O_TRUNC, _S_IREAD | _S_IWRITE) == 3) {
#endif
		msdos_file_handler_open(3, "STDAUX", 0, 2, 0x80c0, 0);
	}
	if(_open("NUL", _O_WRONLY | _O_BINARY | _O_CREAT | _O_TRUNC, _S_IREAD | _S_IWRITE) == 4) {
//		msdos_file_handler_open(4, "STDPRN", 0, 1, 0xa8c0, 0, 0, 1); // LPT1
		msdos_file_handler_open(4, "STDPRN", 0, 1, 0x80a0, 0, 0, 1); // LPT1
	}
	_dup2(0, DUP_STDIN);
	_dup2(1, DUP_STDOUT);
	_dup2(2, DUP_STDERR);
	_dup2(3, DUP_STDAUX);
	_dup2(4, DUP_STDPRN);
	
	// init mouse
	memset(&mouse, 0, sizeof(mouse));
	mouse.enabled = true;	// from DOSBox
	mouse.hidden = 1;	// hidden in default ???
	mouse.old_hidden = 1;	// from DOSBox
	mouse.max_position.x = 8 * (scr_width  - 1);
	mouse.max_position.y = 8 * (scr_height - 1);
	mouse.mickey.x = 8;
	mouse.mickey.y = 16;
	
#ifdef SUPPORT_XMS
	// init xms
	msdos_xms_init();
#endif
	
	// init process
	memset(process, 0, sizeof(process));
	
	// init dtainfo
	msdos_dta_info_init();
	
	// init memory
	memset(mem, 0, sizeof(mem));
	
	// bios data area
	HANDLE hStdout = GetStdHandle(STD_OUTPUT_HANDLE);
	CONSOLE_SCREEN_BUFFER_INFO csbi;
	GetConsoleScreenBufferInfo(hStdout, &csbi);
//	CONSOLE_FONT_INFO cfi;
//	GetCurrentConsoleFont(hStdout, FALSE, &cfi);
	
	int regen = min(scr_width * scr_height * 2, 0x8000);
	text_vram_top_address = TEXT_VRAM_TOP;
	text_vram_end_address = text_vram_top_address + regen;
	shadow_buffer_top_address = SHADOW_BUF_TOP;
	shadow_buffer_end_address = shadow_buffer_top_address + regen;
	cursor_position_address = 0x450 + mem[0x462] * 2;
	
	if(regen > 0x4000) {
		regen = 0x8000;
		vram_pages = 1;
	} else if(regen > 0x2000) {
		regen = 0x4000;
		vram_pages = 2;
	} else if(regen > 0x1000) {
		regen = 0x2000;
		vram_pages = 4;
	} else {
		regen = 0x1000;
		vram_pages = 8;
	}
	
	*(UINT16 *)(mem + 0x400) = 0x3f8; // com1 port address
	*(UINT16 *)(mem + 0x402) = 0x2f8; // com2 port address
	*(UINT16 *)(mem + 0x404) = 0x3e8; // com3 port address
	*(UINT16 *)(mem + 0x406) = 0x2e8; // com4 port address
	*(UINT16 *)(mem + 0x408) = 0x378; // lpt1 port address
	*(UINT16 *)(mem + 0x40a) = 0x278; // lpt2 port address
	*(UINT16 *)(mem + 0x40c) = 0x3bc; // lpt3 port address
#ifdef EXT_BIOS_TOP
	*(UINT16 *)(mem + 0x40e) = EXT_BIOS_TOP >> 4;
#endif
	*(UINT16 *)(mem + 0x410) = msdos_get_equipment();
	*(UINT16 *)(mem + 0x413) = MEMORY_END >> 10;
	*(UINT16 *)(mem + 0x41a) = 0x1e;
	*(UINT16 *)(mem + 0x41c) = 0x1e;
	*(UINT8  *)(mem + 0x449) = 0x03;//0x73;
	*(UINT16 *)(mem + 0x44a) = csbi.dwSize.X;
	*(UINT16 *)(mem + 0x44c) = regen;
	*(UINT16 *)(mem + 0x44e) = 0;
	*(UINT8  *)(mem + 0x450) = csbi.dwCursorPosition.X;
	*(UINT8  *)(mem + 0x451) = csbi.dwCursorPosition.Y - scr_top;
	*(UINT8  *)(mem + 0x460) = 7;
	*(UINT8  *)(mem + 0x461) = 7;
	*(UINT8  *)(mem + 0x462) = 0;
	*(UINT16 *)(mem + 0x463) = 0x3d4;
	*(UINT8  *)(mem + 0x465) = 0x09;
	*(UINT32 *)(mem + 0x46c) = get_ticks_since_midnight(timeGetTime());
	*(UINT16 *)(mem + 0x472) = 0x4321; // preserve memory in cpu reset
	*(UINT16 *)(mem + 0x480) = 0x1e;
	*(UINT16 *)(mem + 0x482) = 0x3e;
	*(UINT8  *)(mem + 0x484) = csbi.srWindow.Bottom - csbi.srWindow.Top;
	*(UINT16 *)(mem + 0x485) = font_height;
	*(UINT8  *)(mem + 0x487) = 0x60;
	*(UINT8  *)(mem + 0x496) = 0x10; // enhanced keyboard installed
	// put ROM configuration table for INT 15h, AH=C0h (Get Configuration) in reserved area
	*(UINT16 *)(mem + 0x4ac + 0) = 0x0a; // number of bytes following
	*(UINT8  *)(mem + 0x4ac + 2) = 0xfc; // model: pc/at
	*(UINT8  *)(mem + 0x4ac + 3) = 0x00; // submodel: pc/at
	*(UINT8  *)(mem + 0x4ac + 5) = 0x60; // 2nd pic, rtc
	*(UINT32 *)(mem + 0x4ac + 6) = 0x40; // int 16/ah=09h
#ifdef EXT_BIOS_TOP
	*(UINT16 *)(mem + EXT_BIOS_TOP) = 1;
#endif
	
	// initial screen
	SMALL_RECT rect;
	SET_RECT(rect, 0, csbi.srWindow.Top, csbi.dwSize.X - 1, csbi.srWindow.Bottom);
	ReadConsoleOutputA(hStdout, scr_buf, scr_buf_size, scr_buf_pos, &rect);
	for(int y = 0, ofs1 = TEXT_VRAM_TOP, ofs2 = SHADOW_BUF_TOP; y < scr_height; y++) {
		for(int x = 0; x < scr_width; x++) {
			mem[ofs1++] = mem[ofs2++] = SCR_BUF(y,x).Char.AsciiChar;
			mem[ofs1++] = mem[ofs2++] = SCR_BUF(y,x).Attributes;
		}
	}
	
	// init mcb
	int seg = MEMORY_TOP >> 4;
	
	// iret table
	// note: int 2eh vector should address the routine in command.com,
	// and some softwares invite (int 2eh vector segment) - 1 must address the mcb of command.com.
	// so move iret table into allocated memory block
	// http://www5c.biglobe.ne.jp/~ecb/assembler2/2_6.html
	msdos_mcb_create(seg++, 'M', PSP_SYSTEM, (IRET_SIZE + 5 * 128) >> 4);
	IRET_TOP = seg << 4;
	seg += (IRET_SIZE + 5 * 128) >> 4;
	memset(mem + IRET_TOP, 0xcf, IRET_SIZE); // iret
	
	// note: SO1 checks int 21h vector and if it aims iret (cfh)
	// it is recognized SO1 is not running on MS-DOS environment
	for(int i = 0; i < 128; i++) {
		// jmp far (IRET_TOP >> 4):(interrupt number)
		*(UINT8  *)(mem + IRET_TOP + IRET_SIZE + 5 * i + 0) = 0xea;
		*(UINT16 *)(mem + IRET_TOP + IRET_SIZE + 5 * i + 1) = i;
		*(UINT16 *)(mem + IRET_TOP + IRET_SIZE + 5 * i + 3) = IRET_TOP >> 4;
	}
	
	// dummy xms/ems device
	msdos_mcb_create(seg++, 'M', PSP_SYSTEM, XMS_SIZE >> 4);
	XMS_TOP = seg << 4;
	seg += XMS_SIZE >> 4;
	
	// environment
	msdos_mcb_create(seg++, 'M', PSP_SYSTEM, ENV_SIZE >> 4);
	int env_seg = seg;
	int ofs = 0;
	char env_append[ENV_SIZE] = {0}, append_added = 0;
	char comspec_added = 0;
	char lastdrive_added = 0;
	char env_msdos_path[ENV_SIZE] = {0};
	char env_path[ENV_SIZE] = {0}, path_added = 0;
	char prompt_added = 0;
	char env_temp[ENV_SIZE] = {0}, temp_added = 0, tmp_added = 0;
	char tz_added = 0;
	const char *path, *short_path;
	
	if((path = getenv("MSDOS_APPEND")) != NULL) {
		if((short_path = msdos_get_multiple_short_path(path)) != NULL && short_path[0] != '\0') {
			strcpy(env_append, short_path);
		}
	}
	if((path = getenv("APPEND")) != NULL) {
		if((short_path = msdos_get_multiple_short_path(path)) != NULL && short_path[0] != '\0') {
			if(env_append[0] != '\0') {
				strcat(env_append, ";");
			}
			strcat(env_append, short_path);
		}
	}
	
	if((path = msdos_search_command_com(argv[0], env_path)) != NULL) {
		if((short_path = msdos_get_multiple_short_path(path)) != NULL && short_path[0] != '\0') {
			strcpy(comspec_path, short_path);
		}
	}
	if((path = getenv("MSDOS_COMSPEC")) != NULL && _access(path, 0) == 0) {
		if((short_path = msdos_get_multiple_short_path(path)) != NULL && short_path[0] != '\0') {
			strcpy(comspec_path, short_path);
		}
	}
	
	if((path = getenv("MSDOS_PATH")) != NULL) {
		if((short_path = msdos_get_multiple_short_path(path)) != NULL && short_path[0] != '\0') {
			strcpy(env_msdos_path, short_path);
			strcpy(env_path, short_path);
		}
	}
	if((path = getenv("PATH")) != NULL) {
		if((short_path = msdos_get_multiple_short_path(path)) != NULL && short_path[0] != '\0') {
			if(env_path[0] != '\0') {
				strcat(env_path, ";");
			}
			strcat(env_path, short_path);
		}
	}
	
	if(GetTempPathA(ENV_SIZE, env_temp) != 0) {
		strcpy(env_temp, msdos_get_multiple_short_path(env_temp));
	}
	for(int i = 0; i < 4; i++) {
		static const char *name[4] = {"MSDOS_TEMP", "MSDOS_TMP", "TEMP", "TMP"};
		if((path = getenv(name[i])) != NULL && _access(path, 0) == 0) {
			if((short_path = msdos_get_multiple_short_path(path)) != NULL && short_path[0] != '\0') {
				strcpy(env_temp, short_path);
				break;
			}
		}
	}
	
	for(char **p = envp; p != NULL && *p != NULL; p++) {
		// lower to upper
		char tmp[ENV_SIZE], name[ENV_SIZE];
		strcpy(tmp, *p);
		for(int i = 0;; i++) {
			if(tmp[i] == '=') {
				tmp[i] = '\0';
				sprintf(name, ";%s;", tmp);
				my_strupr(name);
				tmp[i] = '=';
				break;
			} else if(tmp[i] >= 'a' && tmp[i] <= 'z') {
				tmp[i] = (tmp[i] - 'a') + 'A';
			}
		}
		if(strstr(";MSDOS_APPEND;MSDOS_COMSPEC;MSDOS_LASTDRIVE;MSDOS_TEMP;MSDOS_TMP;MSDOS_TZ;", name) != NULL) {
			// ignore MSDOS_(APPEND/COMSPEC/LASTDRIVE/TEMP/TMP/TZ)
		} else if(standard_env && strstr(";APPEND;COMSPEC;LASTDRIVE;MSDOS_PATH;PATH;PROMPT;TEMP;TMP;TZ;", name) == NULL) {
			// ignore non standard environments
		} else {
			if(strncmp(tmp, "APPEND=", 7) == 0) {
				if(env_append[0] != '\0') {
					sprintf(tmp, "APPEND=%s", env_append);
				} else {
					sprintf(tmp, "APPEND=%s", msdos_get_multiple_short_path(tmp + 7));
				}
				append_added = 1;
			} else if(strncmp(tmp, "COMSPEC=", 8) == 0) {
				strcpy(tmp, "COMSPEC=C:\\COMMAND.COM");
				comspec_added = 1;
			} else if(strncmp(tmp, "LASTDRIVE=", 10) == 0) {
				char *env = getenv("MSDOS_LASTDRIVE");
				if(env != NULL) {
					sprintf(tmp, "LASTDRIVE=%s", env);
				}
				lastdrive_added = 1;
			} else if(strncmp(tmp, "MSDOS_PATH=", 11) == 0) {
				if(env_msdos_path[0] != '\0') {
					sprintf(tmp, "MSDOS_PATH=%s", env_msdos_path);
				} else {
					sprintf(tmp, "MSDOS_PATH=%s", msdos_get_multiple_short_path(tmp + 11));
				}
			} else if(strncmp(tmp, "PATH=", 5) == 0) {
				if(env_path[0] != '\0') {
					sprintf(tmp, "PATH=%s", env_path);
				} else {
					sprintf(tmp, "PATH=%s", msdos_get_multiple_short_path(tmp + 5));
				}
				path_added = 1;
			} else if(strncmp(tmp, "PROMPT=", 7) == 0) {
				prompt_added = 1;
			} else if(strncmp(tmp, "TEMP=", 5) == 0) {
				if(env_temp[0] != '\0') {
					sprintf(tmp, "TEMP=%s", env_temp);
				} else {
					sprintf(tmp, "TEMP=%s", msdos_get_multiple_short_path(tmp + 5));
				}
				temp_added = 1;
			} else if(strncmp(tmp, "TMP=", 4) == 0) {
				if(env_temp[0] != '\0') {
					sprintf(tmp, "TMP=%s", env_temp);
				} else {
					sprintf(tmp, "TMP=%s", msdos_get_multiple_short_path(tmp + 4));
				}
				tmp_added = 1;
			} else if(strncmp(tmp, "TZ=", 3) == 0) {
				char *env = getenv("MSDOS_TZ");
				if(env != NULL) {
					sprintf(tmp, "TZ=%s", env);
				}
				tz_added = 1;
			}
			int len = strlen(tmp);
			if(ofs + len + 1 + (2 + (8 + 1 + 3)) + 2 > ENV_SIZE) {
				fatalerror("too many environments\n");
			}
			memcpy(mem + (seg << 4) + ofs, tmp, len);
			ofs += len + 1;
		}
	}
	if(!append_added && env_append[0] != '\0') {
		#define SET_ENV(name, value) { \
			char tmp[ENV_SIZE]; \
			sprintf(tmp, "%s=%s", name, value); \
			int len = strlen(tmp); \
			if(ofs + len + 1 + (2 + (8 + 1 + 3)) + 2 > ENV_SIZE) { \
				fatalerror("too many environments\n"); \
			} \
			memcpy(mem + (seg << 4) + ofs, tmp, len); \
			ofs += len + 1; \
		}
		SET_ENV("APPEND", env_append);
	}
	if(!comspec_added) {
		SET_ENV("COMSPEC", "C:\\COMMAND.COM");
	}
	if(!lastdrive_added) {
		SET_ENV("LASTDRIVE", "Z");
	}
	if(!path_added) {
		SET_ENV("PATH", env_path);
	}
	if(!prompt_added) {
		SET_ENV("PROMPT", "$P$G");
	}
	if(!temp_added) {
		SET_ENV("TEMP", env_temp);
	}
	if(!tmp_added) {
		SET_ENV("TMP", env_temp);
	}
	if(!tz_added) {
		TIME_ZONE_INFORMATION tzi;
		HKEY hKey, hSubKey;
		char tzi_std_name[64];
		char tz_std[8] = "GMT";
		char tz_dlt[8] = "GST";
		char tz_value[32];
		
		// timezone name from GetTimeZoneInformation may not be english
		bool daylight = (GetTimeZoneInformation(&tzi) != TIME_ZONE_ID_UNKNOWN);
		setlocale(LC_CTYPE, "");
		wcstombs(tzi_std_name, tzi.StandardName, sizeof(tzi_std_name));
		
		// get english timezone name from registry
		if(RegOpenKeyExA(HKEY_LOCAL_MACHINE, "SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion\\Time Zones", 0, KEY_READ, &hKey) == ERROR_SUCCESS) {
			for(DWORD i = 0; !tz_added; i++) {
				char reg_name[256], sub_key[1024], std_name[256];
				DWORD size;
				FILETIME ftTime;
				LONG result = RegEnumKeyExA(hKey, i, reg_name, &(size = array_length(reg_name)), NULL, NULL, NULL, &ftTime);
				
				if(result == ERROR_SUCCESS) {
					sprintf(sub_key, "SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion\\Time Zones\\%s", reg_name);
					if(RegOpenKeyExA(HKEY_LOCAL_MACHINE, sub_key, 0, KEY_QUERY_VALUE, &hSubKey) == ERROR_SUCCESS) {
						if(RegQueryValueExA(hSubKey, "Std", NULL, NULL, (LPBYTE)std_name, &(size = array_length(std_name))) == ERROR_SUCCESS) {
							// search english timezone name from table
							if(strcmp(std_name, tzi_std_name) == 0) {
								for(int j = 0; j < array_length(tz_table); j++) {
									if(strcmp(reg_name, tz_table[j].name) == 0 && (tz_table[j].lcid == 0 || tz_table[j].lcid == GetUserDefaultLCID())) {
										if(tz_table[j].std != NULL) {
											strcpy(tz_std, tz_table[j].std);
										}
										if(tz_table[j].dlt != NULL) {
											strcpy(tz_dlt, tz_table[j].dlt);
										}
										tz_added = 1;
										break;
									}
								}
							}
						}
						RegCloseKey(hSubKey);
					}
				} else if(result == ERROR_NO_MORE_ITEMS) {
					break;
				}
			}
			RegCloseKey(hKey);
		}
		if((tzi.Bias % 60) != 0) {
			sprintf(tz_value, "%s%d:%2d", tz_std, tzi.Bias / 60, abs(tzi.Bias % 60));
		} else {
			sprintf(tz_value, "%s%d", tz_std, tzi.Bias / 60);
		}
		if(daylight) {
			strcat(tz_value, tz_dlt);
		}
		SET_ENV("TZ", tz_value);
	}
	seg += (ENV_SIZE >> 4);
	
	// psp
	msdos_mcb_create(seg++, 'M', PSP_SYSTEM, PSP_SIZE >> 4);
	current_psp = seg;
	psp_t *psp = msdos_psp_create(seg, seg + (PSP_SIZE >> 4), -1, env_seg);
	psp->parent_psp = current_psp;
	seg += (PSP_SIZE >> 4);
	
	// first free mcb in conventional memory
	msdos_mcb_create(seg, 'M', 0, (MEMORY_END >> 4) - seg - 2);
	first_mcb = seg;
	
	// dummy mcb to link to umb
#if 0
	msdos_mcb_create((MEMORY_END >> 4) - 1, 'M', PSP_SYSTEM, (UMB_TOP >> 4) - (MEMORY_END >> 4), "SC"); // link umb
#else
	msdos_mcb_create((MEMORY_END >> 4) - 1, 'Z', PSP_SYSTEM, 0, "SC"); // unlink umb
#endif
	
	// first mcb in upper memory block
	msdos_mcb_create(UMB_TOP >> 4, 'M', PSP_SYSTEM, 0);
	// desqview expects there to be more than one mcb in the umb and the last to be the largest
	msdos_mcb_create((UMB_TOP >> 4) + 1, 'Z', 0, (UMB_END >> 4) - (UMB_TOP >> 4) - 2);
	
#ifdef SUPPORT_HMA
	// first free mcb in high memory area
	msdos_hma_mcb_create(0x10, 0, 0xffe0, 0);
#endif
	
	// interrupt vector
	for(int i = 0; i < 256; i++) {
		// 00-07: CPU exception handler
		// 08-0F: IRQ 0-7
		// 10-1F: PC BIOS
		// 20-3F: MS-DOS system call
		// 70-77: IRQ 8-15
		*(UINT16 *)(mem + 4 * i + 0) = (i <= 0x3f || (i >= 0x70 && i <= 0x77)) ? (IRET_SIZE + 5 * i) : i;
		*(UINT16 *)(mem + 4 * i + 2) = (IRET_TOP >> 4);
	}
	*(UINT16 *)(mem + 4 * 0x08 + 0) = 0x0018;	// fffc:0018 irq0 (system timer)
	*(UINT16 *)(mem + 4 * 0x08 + 2) = DUMMY_TOP >> 4;
	*(UINT16 *)(mem + 4 * 0x22 + 0) = 0x0000;	// ffff:0000 boot
	*(UINT16 *)(mem + 4 * 0x22 + 2) = 0xffff;
	*(UINT16 *)(mem + 4 * 0x67 + 0) = 0x0012;	// xxxx:0012 ems
	*(UINT16 *)(mem + 4 * 0x67 + 2) = XMS_TOP >> 4;
	*(UINT16 *)(mem + 4 * 0x74 + 0) = 0x0000;	// fffc:0000 irq12 (mouse)
	*(UINT16 *)(mem + 4 * 0x74 + 2) = DUMMY_TOP >> 4;
	*(UINT16 *)(mem + 4 * 0xbe + 0) = 0x00bd;	// dos4gw wants two vectors pointing to the same address
	*(UINT16 *)(mem + 4 * 0xbf + 0) = 0x0000;	// 123R3 wants a null vector
	*(UINT16 *)(mem + 4 * 0xbf + 2) = 0x0000;
	
	// dummy devices (NUL -> CON -> ... -> CONFIG$ -> EMMXXXX0)
	static const struct {
		UINT16 attributes;
		char *dev_name;
	} dummy_devices[] = {
		{0x8013, "CON     "},
		{0x8000, "AUX     "},
		{0xa0c0, "PRN     "},
		{0x8008, "CLOCK$  "},
		{0x8000, "COM1    "},
		{0xa0c0, "LPT1    "},
		{0xa0c0, "LPT2    "},
		{0xa0c0, "LPT3    "},
		{0x8000, "COM2    "},
		{0x8000, "COM3    "},
		{0x8000, "COM4    "},
//		{0xc000, "CONFIG$ "},
		{0xc000, "$IBMADSP"}, // for windows3.1 setup.exe
	};
	static const UINT8 dummy_device_routine[] = {
		// from NUL device of Windows 98 SE
		// or word ptr ES:[BX+03],0100
		0x26, 0x81, 0x4f, 0x03, 0x00, 0x01,
		// retf
		0xcb,
	};
	device_t *last = NULL;
	for(int i = 0; i < array_length(dummy_devices); i++) {
		device_t *device = (device_t *)(mem + DEVICE_TOP + 22 + 18 * i);
		device->next_driver.w.l = 22 + 18 * (i + 1);
		device->next_driver.w.h = DEVICE_TOP >> 4;
		device->attributes = dummy_devices[i].attributes;
		device->strategy = DEVICE_SIZE - sizeof(dummy_device_routine);
		device->interrupt = DEVICE_SIZE - sizeof(dummy_device_routine) + 6;
		memcpy(device->dev_name, dummy_devices[i].dev_name, 8);
		last = device;
	}
	if(last != NULL) {
		if(support_ems) {
			last->next_driver.w.l = 0;
			last->next_driver.w.h = XMS_TOP >> 4;
		} else {
			last->next_driver.w.l = 0xffff;
			last->next_driver.w.h = 0xffff;
		}
	}
	memcpy(mem + DEVICE_TOP + DEVICE_SIZE - sizeof(dummy_device_routine), dummy_device_routine, sizeof(dummy_device_routine));
	
	// dos info
	dos_info_t *dos_info = (dos_info_t *)(mem + DOS_INFO_TOP);
	dos_info->magic_word = 1;
	dos_info->first_mcb = MEMORY_TOP >> 4;
	dos_info->first_dpb.w.l = 0;
	dos_info->first_dpb.w.h = DPB_TOP >> 4;
	dos_info->first_sft.w.l = 0;
	dos_info->first_sft.w.h = SFT_TOP >> 4;
	dos_info->clock_device.w.l = 22 + 18 * 3;	// CLOCK$ is the 4th device in IO.SYS
	dos_info->clock_device.w.h = DEVICE_TOP >> 4;
	dos_info->con_device.w.l = 22 + 18 * 0;		// CON is the 1st device in IO.SYS
	dos_info->con_device.w.h = DEVICE_TOP >> 4;
	dos_info->max_sector_len = 512;
	dos_info->disk_buf_info.w.l = offsetof(dos_info_t, disk_buf_heads);
	dos_info->disk_buf_info.w.h = DOS_INFO_TOP >> 4;
	dos_info->cds.w.l = 0;
	dos_info->cds.w.h = CDS_TOP >> 4;
	dos_info->fcb_table.w.l = 0;
	dos_info->fcb_table.w.h = FCB_TABLE_TOP >> 4;
	dos_info->last_drive = 'Z' - 'A' + 1;
	dos_info->buffers_x = 20;
	dos_info->buffers_y = 0;
	dos_info->boot_drive = 'C' - 'A' + 1;
	dos_info->nul_device.next_driver.w.l = 22;
	dos_info->nul_device.next_driver.w.h = DEVICE_TOP >> 4;
	dos_info->nul_device.attributes = 0x8004;
	dos_info->nul_device.strategy = DOS_INFO_SIZE - sizeof(dummy_device_routine);
	dos_info->nul_device.interrupt = DOS_INFO_SIZE - sizeof(dummy_device_routine) + 6;
	memcpy(dos_info->nul_device.dev_name, "NUL     ", 8);
	dos_info->disk_buf_heads.w.l = 0;
	dos_info->disk_buf_heads.w.h = DISK_BUF_TOP >> 4;
	dos_info->umb_linked = (((mcb_t *)(mem + MEMORY_END - 16))->mz == 'M' ? 0x01 : 0x00);
	dos_info->first_umb_fcb = UMB_TOP >> 4;
	dos_info->first_mcb_2 = MEMORY_TOP >> 4;
	memcpy(mem + DOS_INFO_TOP + DOS_INFO_SIZE - sizeof(dummy_device_routine), dummy_device_routine, sizeof(dummy_device_routine));
	
	char *env;
	if((env = getenv("LASTDRIVE")) != NULL) {
		if(env[0] >= 'A' && env[0] <= 'Z') {
			dos_info->last_drive = env[0] - 'A' + 1;
		} else if(env[0] >= 'a' && env[0] <= 'z') {
			dos_info->last_drive = env[0] - 'a' + 1;
		}
	}
	if((env = getenv("windir")) != NULL) {
		if(env[0] >= 'A' && env[0] <= 'Z') {
			dos_info->boot_drive = env[0] - 'A' + 1;
		} else if(env[0] >= 'a' && env[0] <= 'z') {
			dos_info->boot_drive = env[0] - 'a' + 1;
		}
	}
#if defined(HAS_I386)
	dos_info->i386_or_later = 1;
#else
	dos_info->i386_or_later = 0;
#endif
	dos_info->ext_mem_size = (min(MAX_MEM, 0x4000000) - 0x100000) >> 10;
	
	// ems (int 67h) and xms
	if(support_ems) {
		device_t *xms_device = (device_t *)(mem + XMS_TOP);
		xms_device->next_driver.w.l = 0xffff;
		xms_device->next_driver.w.h = 0xffff;
		xms_device->attributes = 0xc000;
		xms_device->strategy = XMS_SIZE - sizeof(dummy_device_routine);
		xms_device->interrupt = XMS_SIZE - sizeof(dummy_device_routine) + 6;
		memcpy(xms_device->dev_name, "EMMXXXX0", 8);
	
		mem[XMS_TOP + 0x12] = 0xcd;	// int 65h (dummy)
		mem[XMS_TOP + 0x13] = 0x65;
		mem[XMS_TOP + 0x14] = 0xcf;	// iret
	} else {
		mem[XMS_TOP + 0x12] = 0xcf;	// iret 
	}
#ifdef SUPPORT_XMS
	if(support_xms) {
		mem[XMS_TOP + 0x15] = 0xcd;	// int 66h (dummy)
		mem[XMS_TOP + 0x16] = 0x66;
		mem[XMS_TOP + 0x17] = 0xcb;	// retf
	} else
#endif
	mem[XMS_TOP + 0x15] = 0xcb;	// retf
	memcpy(mem + XMS_TOP + XMS_SIZE - sizeof(dummy_device_routine), dummy_device_routine, sizeof(dummy_device_routine));
	
	// irq12 routine (mouse)
	mem[DUMMY_TOP + 0x00] = 0xcd;	// int 69h (dummy)
	mem[DUMMY_TOP + 0x01] = 0x69;
	mem[DUMMY_TOP + 0x02] = 0x9a;	// call far mouse
	mem[DUMMY_TOP + 0x03] = 0xff;
	mem[DUMMY_TOP + 0x04] = 0xff;
	mem[DUMMY_TOP + 0x05] = 0xff;
	mem[DUMMY_TOP + 0x06] = 0xff;
//	mem[DUMMY_TOP + 0x07] = 0xcd;	// int 6ah (dummy)
//	mem[DUMMY_TOP + 0x08] = 0x6a;
	mem[DUMMY_TOP + 0x07] = 0xcd;	// int 6bh (dummy)
	mem[DUMMY_TOP + 0x08] = 0x6b;
	mem[DUMMY_TOP + 0x09] = 0xcf;	// iret
	
	// case map routine
	mem[DUMMY_TOP + 0x0a] = 0xcd;	// int 6ch (dummy)
	mem[DUMMY_TOP + 0x0b] = 0x6c;
	mem[DUMMY_TOP + 0x0c] = 0xcb;	// retf
	
	// font read routine
	mem[DUMMY_TOP + 0x0d] = 0xcd;	// int 6dh (dummy)
	mem[DUMMY_TOP + 0x0e] = 0x6d;
	mem[DUMMY_TOP + 0x0f] = 0xcb;	// retf
	
	// error message read routine
	mem[DUMMY_TOP + 0x10] = 0xcd;	// int 6eh (dummy)
	mem[DUMMY_TOP + 0x11] = 0x6e;
	mem[DUMMY_TOP + 0x12] = 0xcb;	// retf
	
	// dummy loop to wait BIOS/DOS service is done
	mem[DUMMY_TOP + 0x13] = 0xe6;	// out f7h, al
	mem[DUMMY_TOP + 0x14] = 0xf7;
	mem[DUMMY_TOP + 0x15] = 0x78;	// js/jns -4
	mem[DUMMY_TOP + 0x16] = 0xfc;
	mem[DUMMY_TOP + 0x17] = 0xcb;	// retf
	
	// irq0 routine (system timer)
	mem[DUMMY_TOP + 0x18] = 0xcd;	// int 1ch
	mem[DUMMY_TOP + 0x19] = 0x1c;
	mem[DUMMY_TOP + 0x1a] = 0xea;	// jmp far (IRET_TOP >> 4):0008
	mem[DUMMY_TOP + 0x1b] = 0x08;
	mem[DUMMY_TOP + 0x1c] = 0x00;
	mem[DUMMY_TOP + 0x1d] = ((IRET_TOP >> 4)     ) & 0xff;
	mem[DUMMY_TOP + 0x1e] = ((IRET_TOP >> 4) >> 8) & 0xff;
	
	// alter page map and call routine
	mem[DUMMY_TOP + 0x1f] = 0x9a;	// call far
	mem[DUMMY_TOP + 0x20] = 0xff;
	mem[DUMMY_TOP + 0x21] = 0xff;
	mem[DUMMY_TOP + 0x22] = 0xff;
	mem[DUMMY_TOP + 0x23] = 0xff;
	mem[DUMMY_TOP + 0x24] = 0xcd;	// int 6fh (dummy)
	mem[DUMMY_TOP + 0x25] = 0x6f;
	mem[DUMMY_TOP + 0x26] = 0xcb;	// retf
	
	// call int 29h routine
	mem[DUMMY_TOP + 0x27] = 0xcd;	// int 29h
	mem[DUMMY_TOP + 0x28] = 0x29;
	mem[DUMMY_TOP + 0x29] = 0xcb;	// retf
	
	// VCPI entry point
	mem[DUMMY_TOP + 0x2a] = 0xcd;	// int 65h
	mem[DUMMY_TOP + 0x2b] = 0x65;
	mem[DUMMY_TOP + 0x2c] = 0xcb;	// retf
	
	// boot routine
	mem[0xffff0 + 0x00] = 0xf4;	// halt to exit MS-DOS Player
#if 1
	mem[0xffff0 + 0x05] = '0';	// rom date (same as DOSBox)
	mem[0xffff0 + 0x06] = '1';
	mem[0xffff0 + 0x07] = '/';
	mem[0xffff0 + 0x08] = '0';
	mem[0xffff0 + 0x09] = '1';
	mem[0xffff0 + 0x0a] = '/';
	mem[0xffff0 + 0x0b] = '9';
	mem[0xffff0 + 0x0c] = '2';
	mem[0xffff0 + 0x0e] = 0xfc;	// machine id (pc/at)
	mem[0xffff0 + 0x0f] = 0x55;	// signature
#else
	mem[0xffff0 + 0x05] = '0';	// rom date (same as Windows 98 SE)
	mem[0xffff0 + 0x06] = '2';
	mem[0xffff0 + 0x07] = '/';
	mem[0xffff0 + 0x08] = '2';
	mem[0xffff0 + 0x09] = '2';
	mem[0xffff0 + 0x0a] = '/';
	mem[0xffff0 + 0x0b] = '0';
	mem[0xffff0 + 0x0c] = '6';
	mem[0xffff0 + 0x0e] = 0xfc;	// machine id (pc/at)
	mem[0xffff0 + 0x0f] = 0x00;
#endif
	
	// param block
	// + 0: param block (22bytes)
	// +24: fcb1/2 (20bytes)
	// +44: command tail (128bytes)
	param_block_t *param = (param_block_t *)(mem + WORK_TOP);
	param->env_seg = 0;
	param->cmd_line.w.l = 44;
	param->cmd_line.w.h = (WORK_TOP >> 4);
	param->fcb1.w.l = 24;
	param->fcb1.w.h = (WORK_TOP >> 4);
	param->fcb2.w.l = 24;
	param->fcb2.w.h = (WORK_TOP >> 4);
	
	memset(mem + WORK_TOP + 24, 0x20, 20);
	
	cmd_line_t *cmd_line = (cmd_line_t *)(mem + WORK_TOP + 44);
	if(argc > 1) {
		sprintf(cmd_line->cmd, " %s", argv[1]);
		for(int i = 2; i < argc; i++) {
			char tmp[128];
			sprintf(tmp, "%s %s", cmd_line->cmd, argv[i]);
			strcpy(cmd_line->cmd, tmp);
		}
		cmd_line->len = (UINT8)strlen(cmd_line->cmd);
	} else {
		cmd_line->len = 0;
	}
	cmd_line->cmd[cmd_line->len] = 0x0d;
	
	// system file table
	*(UINT32 *)(mem + SFT_TOP + 0) = 0xffffffff;
	*(UINT16 *)(mem + SFT_TOP + 4) = 20;
	
	// disk buffer header (from DOSBox)
	*(UINT16 *)(mem + DISK_BUF_TOP +  0) = 0xffff;		// forward ptr
	*(UINT16 *)(mem + DISK_BUF_TOP +  2) = 0xffff;		// backward ptr
	*(UINT8  *)(mem + DISK_BUF_TOP +  4) = 0xff;		// not in use
	*(UINT8  *)(mem + DISK_BUF_TOP + 10) = 0x01;		// number of FATs
	*(UINT32 *)(mem + DISK_BUF_TOP + 13) = 0xffffffff;	// pointer to DPB
	
	// fcb table
	*(UINT32 *)(mem + FCB_TABLE_TOP + 0) = 0xffffffff;
	*(UINT16 *)(mem + FCB_TABLE_TOP + 4) = 0;
	
	// drive parameter block
	for(int i = 0; i < 2; i++) {
		// may be a floppy drive
		cds_t *cds = (cds_t *)(mem + CDS_TOP + 88 * i);
		sprintf(cds->path_name, "%c:\\", 'A' + i);
		cds->drive_attrib = 0x4000;	// physical drive
		cds->dpb_ptr.w.l = sizeof(dpb_t) * i;
		cds->dpb_ptr.w.h = DPB_TOP >> 4;
		cds->word_1 = cds->word_2 = cds->word_3 = 0xffff;
		cds->bs_offset = 2;
		
		dpb_t *dpb = (dpb_t *)(mem + DPB_TOP + sizeof(dpb_t) * i);
		dpb->drive_num = i;
		dpb->unit_num = i;
		dpb->next_dpb_ofs = /*(i == 25) ? 0xffff : */sizeof(dpb_t) * (i + 1);
		dpb->next_dpb_seg = /*(i == 25) ? 0xffff : */DPB_TOP >> 4;
	}
	for(int i = 2; i < 26; i++) {
		msdos_cds_update(i);
		UINT16 seg, ofs;
		msdos_drive_param_block_update(i, &seg, &ofs, 1);
	}
	
	// nls stuff
	msdos_nls_tables_init();
	
	// execute command
	try {
		if(msdos_process_exec(argv[0], param, 0, true)) {
			fatalerror("'%s' not found\n", argv[0]);
		}
	} catch(...) {
		// we should not reach here :-(
		fatalerror("failed to start '%s' because of unexpected exeption\n", argv[0]);
	}
	retval = 0;
	return(0);
}

#define remove_std_file(path) { \
	int fd = _open(path, _O_RDONLY | _O_BINARY); \
	if(fd != -1) { \
		_lseek(fd, 0, SEEK_END); \
		int size = _tell(fd); \
		_close(fd); \
		if(size == 0) { \
			remove(path); \
		} \
	} \
}

void msdos_finish()
{
	for(int i = 0; i < MAX_FILES; i++) {
		if(file_handler[i].valid) {
			_close(i);
		}
	}
#ifdef MAP_AUX_DEVICE_TO_FILE
	remove_std_file("stdaux.txt");
#endif
#ifdef SUPPORT_XMS
	msdos_xms_finish();
#endif
	msdos_dbcs_table_finish();
}

/* ----------------------------------------------------------------------------
	PC/AT hardware emulation
---------------------------------------------------------------------------- */

void hardware_init()
{
	CPU_INIT();
	CPU_RESET();
	CPU_SET_I_FLAG(1);
#if defined(HAS_I386)
	cpu_type = (CPU_EDX >> 8) & 0x0f;
	cpu_step = (CPU_EDX >> 0) & 0x0f;
#endif
	CPU_A20_LINE(0);
	
	ems_init();
	dma_init();
	pic_init();
	pio_init();
#ifdef PIT_ALWAYS_RUNNING
	pit_init();
#else
	pit_active = 0;
#endif
	sio_init();
	cmos_init();
	kbd_init();
}

void hardware_finish()
{
	CPU_FINISH();
	ems_finish();
	pio_finish();
	sio_finish();
}

void hardware_release()
{
	// release hardware resources when this program will be terminated abnormally
#ifdef EXPORT_DEBUG_TO_FILE
	if(fp_debug_log != NULL) {
		fclose(fp_debug_log);
		fp_debug_log = NULL;
	}
#endif
	CPU_RELEASE();
	ems_release();
	pio_release();
	sio_release();
}

void hardware_run()
{
#ifdef EXPORT_DEBUG_TO_FILE
	// open debug log file after msdos_init() is done not to use the standard file handlers
	fp_debug_log = fopen("debug.log", "w");
#endif
#ifdef USE_DEBUGGER
	msdos_int_num = -1;
#endif
	while(!msdos_exit) {
		hardware_run_cpu();
	}
#ifdef EXPORT_DEBUG_TO_FILE
	if(fp_debug_log != NULL) {
		fclose(fp_debug_log);
		fp_debug_log = NULL;
	}
#endif
}

inline void hardware_run_cpu()
{
	CPU_EXECUTE();
	if(CPU_EIP_CHANGED) {
		idle_ops++;
	}
#ifdef USE_DEBUGGER
	// Disallow reentering CPU_EXECUTE() in msdos_syscall()
	if(msdos_int_num >= 0) {
		unsigned num = (unsigned)msdos_int_num;
		msdos_int_num = -1;
		msdos_syscall(num);
	}
#endif
	if(++update_ops == UPDATE_OPS) {
		update_ops = 0;
		hardware_update();
	}
}

void hardware_update()
{
	static UINT32 prev_time = 0;
	UINT32 cur_time = timeGetTime();
	
	if(prev_time != cur_time) {
		// update pit and raise irq0
#ifndef PIT_ALWAYS_RUNNING
		if(pit_active)
#endif
		{
			if(pit_run(0, cur_time)) {
				pic_req(0, 0, 1);
			}
			pit_run(1, cur_time);
			pit_run(2, cur_time);
		}
		
		// update sio and raise irq4/3
		for(int c = 0; c < 4; c++) {
			sio_update(c);
		}
		
		// update keyboard and mouse
		static UINT32 prev_tick = 0;
		UINT32 cur_tick = cur_time / 32;
		
		if(prev_tick != cur_tick) {
			// update keyboard flags
			UINT8 state;
			state  = (GetAsyncKeyState(VK_INSERT  ) & 0x0001) ? 0x80 : 0;
			state |= (GetAsyncKeyState(VK_CAPITAL ) & 0x0001) ? 0x40 : 0;
			state |= (GetAsyncKeyState(VK_NUMLOCK ) & 0x0001) ? 0x20 : 0;
			state |= (GetAsyncKeyState(VK_SCROLL  ) & 0x0001) ? 0x10 : 0;
			state |= (GetAsyncKeyState(VK_MENU    ) & 0x8000) ? 0x08 : 0;
			state |= (GetAsyncKeyState(VK_CONTROL ) & 0x8000) ? 0x04 : 0;
			state |= (GetAsyncKeyState(VK_LSHIFT  ) & 0x8000) ? 0x02 : 0;
			state |= (GetAsyncKeyState(VK_RSHIFT  ) & 0x8000) ? 0x01 : 0;
			mem[0x417] = state;
			state  = (GetAsyncKeyState(VK_INSERT  ) & 0x8000) ? 0x80 : 0;
			state |= (GetAsyncKeyState(VK_CAPITAL ) & 0x8000) ? 0x40 : 0;
			state |= (GetAsyncKeyState(VK_NUMLOCK ) & 0x8000) ? 0x20 : 0;
			state |= (GetAsyncKeyState(VK_SCROLL  ) & 0x8000) ? 0x10 : 0;
//			state |= (GetAsyncKeyState(VK_PAUSE   ) & 0x0001) ? 0x08 : 0;
//			state |= (GetAsyncKeyState(VK_SYSREQ  ) & 0x8000) ? 0x04 : 0;
			state |= (GetAsyncKeyState(VK_LMENU   ) & 0x8000) ? 0x02 : 0;
			state |= (GetAsyncKeyState(VK_LCONTROL) & 0x8000) ? 0x01 : 0;
			mem[0x418] = state;
			
			// update console input if needed
			if(!key_changed || mouse.hidden == 0) {
				update_console_input();
			}
			if(!(kbd_status & 1)) {
				if(key_buf_data != NULL) {
#ifdef USE_SERVICE_THREAD
					EnterCriticalSection(&key_buf_crit_sect);
#endif
					if(!key_buf_data->empty()) {
						kbd_data = key_buf_data->read();
						kbd_status |= 1;
						key_changed = true;
					}
#ifdef USE_SERVICE_THREAD
					LeaveCriticalSection(&key_buf_crit_sect);
#endif
				}
			}
			
			// raise irq1 if key is pressed/released or key buffer is not empty
			if(!key_changed) {
#ifdef USE_SERVICE_THREAD
				EnterCriticalSection(&key_buf_crit_sect);
#endif
				if(!pcbios_is_key_buffer_empty()) {
/*
					if(!(kbd_status & 1)) {
						UINT16 head = *(UINT16 *)(mem + 0x41a);
						UINT16 tail = *(UINT16 *)(mem + 0x41c);
						if(head != tail) {
							int key_char = mem[0x400 + (head++)];
							int key_scan = mem[0x400 + (head++)];
							kbd_data = key_char ? key_char : key_scan;
							kbd_status |= 1;
						}
					}
*/
					key_changed = true;
				}
#ifdef USE_SERVICE_THREAD
				LeaveCriticalSection(&key_buf_crit_sect);
#endif
			}
			if(key_changed) {
				pic_req(0, 1, 1);
				key_changed = false;
			}
			
			// raise irq12 if mouse status is changed
			if((mouse.status & 0x1f) && mouse.call_addr_ps2.dw && mouse.enabled_ps2) {
				mouse.status_irq = 0; // ???
				mouse.status_irq_alt = 0; // ???
				mouse.status_irq_ps2 = mouse.status & 0x1f;
				mouse.status = 0;
				pic_req(1, 4, 1);
			} else if((mouse.status & mouse.call_mask) && mouse.call_addr.dw) {
				mouse.status_irq = mouse.status & mouse.call_mask;
				mouse.status_irq_alt = 0; // ???
				mouse.status_irq_ps2 = 0; // ???
				mouse.status &= ~mouse.call_mask;
				pic_req(1, 4, 1);
			} else {
				for(int i = 0; i < 8; i++) {
					if((mouse.status_alt & (1 << i)) && mouse.call_addr_alt[i].dw) {
						mouse.status_irq = 0; // ???
						mouse.status_irq_alt = 0;
						mouse.status_irq_ps2 = 0; // ???
						for(int j = 0; j < 8; j++) {
							if((mouse.status_alt & (1 << j)) && mouse.call_addr_alt[i].dw == mouse.call_addr_alt[j].dw) {
								mouse.status_irq_alt |=  (1 << j);
								mouse.status_alt     &= ~(1 << j);
							}
						}
						pic_req(1, 4, 1);
						break;
					}
				}
			}
			
			prev_tick = cur_tick;
		}
		
		// update cursor size/position by crtc
		if(crtc_changed[10] != 0 || crtc_changed[11] != 0) {
			int size = (int)(crtc_regs[11] & 7) - (int)(crtc_regs[10] & 7) + 1;
			if(!((crtc_regs[10] & 0x20) != 0 || size < 0)) {
				ci_new.bVisible = TRUE;
				ci_new.dwSize = (size + 2) * 100 / (8 + 2);
			} else {
				ci_new.bVisible = FALSE;
			}
			crtc_changed[10] = crtc_changed[11] = 0;
		}
		if(crtc_changed[14] != 0 || crtc_changed[15] != 0) {
			if(cursor_moved) {
				pcbios_update_cursor_position();
				cursor_moved = false;
			}
			HANDLE hStdout = GetStdHandle(STD_OUTPUT_HANDLE);
			int position = crtc_regs[14] * 256 + crtc_regs[15];
			int width = *(UINT16 *)(mem + 0x44a);
			COORD co;
			co.X = position % width;
			co.Y = position / width + scr_top;
			SetConsoleCursorPosition(hStdout, co);
			
			crtc_changed[14] = crtc_changed[15] = 0;
			cursor_moved_by_crtc = true;
		}
		
		// update cursor info
		if(!is_cursor_blink_off()) {
			ci_new.bVisible = TRUE;
		}
		if(!(ci_old.dwSize == ci_new.dwSize && ci_old.bVisible == ci_new.bVisible)) {
			HANDLE hStdout = GetStdHandle(STD_OUTPUT_HANDLE);
			SetConsoleCursorInfo(hStdout, &ci_new);
			restore_console_cursor = true;
		}
		ci_old = ci_new;
		
		// update daily timer counter
		pcbios_update_daily_timer_counter(cur_time);
		
		prev_time = cur_time;
	}
}

// ems

void ems_init()
{
	memset(ems_handles, 0, sizeof(ems_handles));
	memset(ems_pages, 0, sizeof(ems_pages));
	free_ems_pages = MAX_EMS_PAGES;
}

void ems_finish()
{
	ems_release();
}

void ems_release()
{
	for(int i = 1; i <= MAX_EMS_HANDLES; i++) {
		if(ems_handles[i].buffer != NULL) {
			free(ems_handles[i].buffer);
			ems_handles[i].buffer = NULL;
		}
	}
}

void ems_allocate_pages(int handle, int pages)
{
	if(pages > 0) {
		ems_handles[handle].buffer = (UINT8 *)calloc(1, 0x4000 * pages);
	} else {
		ems_handles[handle].buffer = NULL;
	}
	ems_handles[handle].pages = pages;
	ems_handles[handle].allocated = true;
	free_ems_pages -= pages;
}

void ems_reallocate_pages(int handle, int pages)
{
	if(ems_handles[handle].allocated) {
		if(ems_handles[handle].pages != pages) {
			UINT8 *new_buffer = NULL;
			
			if(pages > 0) {
				new_buffer = (UINT8 *)calloc(1, 0x4000 * pages);
			}
			if(ems_handles[handle].buffer != NULL) {
				if(new_buffer != NULL) {
					if(pages > ems_handles[handle].pages) {
						memcpy(new_buffer, ems_handles[handle].buffer, 0x4000 * ems_handles[handle].pages);
					} else {
						memcpy(new_buffer, ems_handles[handle].buffer, 0x4000 * pages);
					}
				}
				free(ems_handles[handle].buffer);
				ems_handles[handle].buffer = NULL;
			}
			free_ems_pages += ems_handles[handle].pages;
			
			ems_handles[handle].buffer = new_buffer;
			ems_handles[handle].pages = pages;
			free_ems_pages -= pages;
		}
	} else {
		ems_allocate_pages(handle, pages);
	}
}

void ems_release_pages(int handle)
{
	if(ems_handles[handle].allocated) {
		if(ems_handles[handle].buffer != NULL) {
			free(ems_handles[handle].buffer);
			ems_handles[handle].buffer = NULL;
		}
		free_ems_pages += ems_handles[handle].pages;
		ems_handles[handle].allocated = false;
	}
}

void ems_map_page(int physical, int handle, int logical)
{
	if(ems_pages[physical].mapped) {
		if(ems_pages[physical].handle == handle && ems_pages[physical].page == logical) {
			return;
		}
		ems_unmap_page(physical);
	}
	if(ems_handles[handle].allocated && ems_handles[handle].buffer != NULL && logical < ems_handles[handle].pages) {
		memcpy(mem + EMS_TOP + 0x4000 * physical, ems_handles[handle].buffer + 0x4000 * logical, 0x4000);
	}
	ems_pages[physical].handle = handle;
	ems_pages[physical].page = logical;
	ems_pages[physical].mapped = true;
}

void ems_unmap_page(int physical)
{
	if(ems_pages[physical].mapped) {
		int handle = ems_pages[physical].handle;
		int logical = ems_pages[physical].page;
		
		if(ems_handles[handle].allocated && ems_handles[handle].buffer != NULL && logical < ems_handles[handle].pages) {
			memcpy(ems_handles[handle].buffer + 0x4000 * logical, mem + EMS_TOP + 0x4000 * physical, 0x4000);
		}
		ems_pages[physical].mapped = false;
	}
}

// dma

void dma_init()
{
	memset(dma, 0, sizeof(dma));
	for(int c = 0; c < 2; c++) {
//		for(int ch = 0; ch < 4; ch++) {
//			dma[c].ch[ch].creg.w = dma[c].ch[ch].bcreg.w = 0xffff;
//		}
		dma_reset(c);
	}
}

void dma_reset(int c)
{
	dma[c].low_high = false;
	dma[c].cmd = dma[c].req = dma[c].tc = 0;
	dma[c].mask = 0xff;
}

void dma_write(int c, UINT32 addr, UINT8 data)
{
	int ch = (addr >> 1) & 3;
	UINT8 bit = 1 << (data & 3);
	
	switch(addr & 0x0f) {
	case 0x00: case 0x02: case 0x04: case 0x06:
		if(dma[c].low_high) {
			dma[c].ch[ch].bareg.b.h = data;
		} else {
			dma[c].ch[ch].bareg.b.l = data;
		}
		dma[c].ch[ch].areg.w = dma[c].ch[ch].bareg.w;
		dma[c].low_high = !dma[c].low_high;
		break;
	case 0x01: case 0x03: case 0x05: case 0x07:
		if(dma[c].low_high) {
			dma[c].ch[ch].bcreg.b.h = data;
		} else {
			dma[c].ch[ch].bcreg.b.l = data;
		}
		dma[c].ch[ch].creg.w = dma[c].ch[ch].bcreg.w;
		dma[c].low_high = !dma[c].low_high;
		break;
	case 0x08:
		// command register
		dma[c].cmd = data;
		break;
	case 0x09:
		// dma[c].request register
		if(data & 4) {
			if(!(dma[c].req & bit)) {
				dma[c].req |= bit;
//				dma_run(c, ch);
			}
		} else {
			dma[c].req &= ~bit;
		}
		break;
	case 0x0a:
		// single mask register
		if(data & 4) {
			dma[c].mask |= bit;
		} else {
			dma[c].mask &= ~bit;
		}
		break;
	case 0x0b:
		// mode register
		dma[c].ch[data & 3].mode = data;
		break;
	case 0x0c:
		dma[c].low_high = false;
		break;
	case 0x0d:
		// clear master
		dma_reset(c);
		break;
	case 0x0e:
		// clear mask register
		dma[c].mask = 0;
		break;
	case 0x0f:
		// all mask register
		dma[c].mask = data & 0x0f;
		break;
	}
}

UINT8 dma_read(int c, UINT32 addr)
{
	int ch = (addr >> 1) & 3;
	UINT8 val = 0xff;
	
	switch(addr & 0x0f) {
	case 0x00: case 0x02: case 0x04: case 0x06:
		if(dma[c].low_high) {
			val = dma[c].ch[ch].areg.b.h;
		} else {
			val = dma[c].ch[ch].areg.b.l;
		}
		dma[c].low_high = !dma[c].low_high;
		return(val);
	case 0x01: case 0x03: case 0x05: case 0x07:
		if(dma[c].low_high) {
			val = dma[c].ch[ch].creg.b.h;
		} else {
			val = dma[c].ch[ch].creg.b.l;
		}
		dma[c].low_high = !dma[c].low_high;
		return(val);
	case 0x08:
		// status register
		val = (dma[c].req << 4) | dma[c].tc;
		dma[c].tc = 0;
		return(val);
	case 0x0d:
		// temporary register (intel 82374 does not support)
		return(dma[c].tmp & 0xff);
	case 0x0f:
		// mask register (intel 82374 does support)
		return(dma[c].mask);
	}
	return(0xff);
}

void dma_page_write(int c, int ch, UINT8 data)
{
	dma[c].ch[ch].pagereg = data;
}

UINT8 dma_page_read(int c, int ch)
{
	return(dma[c].ch[ch].pagereg);
}

void dma_run(int c, int ch)
{
	UINT8 bit = 1 << ch;
	
	if((dma[c].req & bit) && !(dma[c].mask & bit)) {
		// execute dma
		while(dma[c].req & bit) {
			if(ch == 0 && (dma[c].cmd & 0x01)) {
				// memory -> memory
				UINT32 saddr = dma[c].ch[0].areg.w | (dma[c].ch[0].pagereg << 16);
				UINT32 daddr = dma[c].ch[1].areg.w | (dma[c].ch[1].pagereg << 16);
				
				if(c == 0) {
					dma[c].tmp = read_byte(saddr);
					write_byte(daddr, dma[c].tmp);
				} else {
					dma[c].tmp = read_word(saddr << 1);
					write_word(daddr << 1, dma[c].tmp);
				}
				if(!(dma[c].cmd & 0x02)) {
					if(dma[c].ch[0].mode & 0x20) {
						dma[c].ch[0].areg.w--;
						if(dma[c].ch[0].areg.w == 0xffff) {
							dma[c].ch[0].pagereg--;
						}
					} else {
						dma[c].ch[0].areg.w++;
						if(dma[c].ch[0].areg.w == 0) {
							dma[c].ch[0].pagereg++;
						}
					}
				}
				if(dma[c].ch[1].mode & 0x20) {
					dma[c].ch[1].areg.w--;
					if(dma[c].ch[1].areg.w == 0xffff) {
						dma[c].ch[1].pagereg--;
					}
				} else {
					dma[c].ch[1].areg.w++;
					if(dma[c].ch[1].areg.w == 0) {
						dma[c].ch[1].pagereg++;
					}
				}
				
				// check dma condition
				if(dma[c].ch[0].creg.w-- == 0) {
					if(dma[c].ch[0].mode & 0x10) {
						// self initialize
						dma[c].ch[0].areg.w = dma[c].ch[0].bareg.w;
						dma[c].ch[0].creg.w = dma[c].ch[0].bcreg.w;
					} else {
//						dma[c].mask |= bit;
					}
				}
				if(dma[c].ch[1].creg.w-- == 0) {
					// terminal count
					if(dma[c].ch[1].mode & 0x10) {
						// self initialize
						dma[c].ch[1].areg.w = dma[c].ch[1].bareg.w;
						dma[c].ch[1].creg.w = dma[c].ch[1].bcreg.w;
					} else {
						dma[c].mask |= bit;
					}
					dma[c].req &= ~bit;
					dma[c].tc |= bit;
				}
			} else {
				UINT32 addr = dma[c].ch[ch].areg.w | (dma[c].ch[ch].pagereg << 16);
				
				if((dma[c].ch[ch].mode & 0x0c) == 0x00) {
					// verify
				} else if((dma[c].ch[ch].mode & 0x0c) == 0x04) {
					// io -> memory
					if(c == 0) {
						dma[c].tmp = read_io_byte(dma[c].ch[ch].port);
						write_byte(addr, dma[c].tmp);
					} else {
						dma[c].tmp = read_io_word(dma[c].ch[ch].port);
						write_word(addr << 1, dma[c].tmp);
					}
				} else if((dma[c].ch[ch].mode & 0x0c) == 0x08) {
					// memory -> io
					if(c == 0) {
						dma[c].tmp = read_byte(addr);
						write_io_byte(dma[c].ch[ch].port, dma[c].tmp);
					} else {
						dma[c].tmp = read_word(addr << 1);
						write_io_word(dma[c].ch[ch].port, dma[c].tmp);
					}
				}
				if(dma[c].ch[ch].mode & 0x20) {
					dma[c].ch[ch].areg.w--;
					if(dma[c].ch[ch].areg.w == 0xffff) {
						dma[c].ch[ch].pagereg--;
					}
				} else {
					dma[c].ch[ch].areg.w++;
					if(dma[c].ch[ch].areg.w == 0) {
						dma[c].ch[ch].pagereg++;
					}
				}
				
				// check dma condition
				if(dma[c].ch[ch].creg.w-- == 0) {
					// terminal count
					if(dma[c].ch[ch].mode & 0x10) {
						// self initialize
						dma[c].ch[ch].areg.w = dma[c].ch[ch].bareg.w;
						dma[c].ch[ch].creg.w = dma[c].ch[ch].bcreg.w;
					} else {
						dma[c].mask |= bit;
					}
					dma[c].req &= ~bit;
					dma[c].tc |= bit;
				} else if((dma[c].ch[ch].mode & 0xc0) == 0x40) {
					// single mode
					break;
				}
			}
		}
	}
}

// pic

void pic_init()
{
	memset(pic, 0, sizeof(pic));
	pic[0].imr = pic[1].imr = 0xff;
	
	// from bochs bios
	pic_write(0, 0, 0x11);	// icw1 = 11h
	pic_write(0, 1, 0x08);	// icw2 = 08h
	pic_write(0, 1, 0x04);	// icw3 = 04h
	pic_write(0, 1, 0x01);	// icw4 = 01h
	pic_write(0, 1, 0xb8);	// ocw1 = b8h
	pic_write(1, 0, 0x11);	// icw1 = 11h
	pic_write(1, 1, 0x70);	// icw2 = 70h
	pic_write(1, 1, 0x02);	// icw3 = 02h
	pic_write(1, 1, 0x01);	// icw4 = 01h
}

void pic_write(int c, UINT32 addr, UINT8 data)
{
	if(addr & 1) {
		if(pic[c].icw2_r) {
			// icw2
			pic[c].icw2 = data;
			pic[c].icw2_r = 0;
		} else if(pic[c].icw3_r) {
			// icw3
			pic[c].icw3 = data;
			pic[c].icw3_r = 0;
		} else if(pic[c].icw4_r) {
			// icw4
			pic[c].icw4 = data;
			pic[c].icw4_r = 0;
		} else {
			// ocw1
			pic[c].imr = data;
		}
	} else {
		if(data & 0x10) {
			// icw1
			pic[c].icw1 = data;
			pic[c].icw2_r = 1;
			pic[c].icw3_r = (data & 2) ? 0 : 1;
			pic[c].icw4_r = data & 1;
			pic[c].irr = 0;
			pic[c].isr = 0;
			pic[c].imr = 0;
			pic[c].prio = 0;
			if(!(pic[c].icw1 & 1)) {
				pic[c].icw4 = 0;
			}
			pic[c].ocw3 = 0;
		} else if(data & 8) {
			// ocw3
			if(!(data & 2)) {
				data = (data & ~1) | (pic[c].ocw3 & 1);
			}
			if(!(data & 0x40)) {
				data = (data & ~0x20) | (pic[c].ocw3 & 0x20);
			}
			pic[c].ocw3 = data;
		} else {
			// ocw2
			int level = 0;
			if(data & 0x40) {
				level = data & 7;
			} else {
				if(!pic[c].isr) {
					return;
				}
				level = pic[c].prio;
				while(!(pic[c].isr & (1 << level))) {
					level = (level + 1) & 7;
				}
			}
			if(data & 0x80) {
				pic[c].prio = (level + 1) & 7;
			}
			if(data & 0x20) {
				pic[c].isr &= ~(1 << level);
			}
		}
	}
	pic_update();
}

UINT8 pic_read(int c, UINT32 addr)
{
	if(addr & 1) {
		return(pic[c].imr);
	} else {
		// polling mode is not supported...
		//if(pic[c].ocw3 & 4) {
		//	return ???;
		//}
		if(pic[c].ocw3 & 1) {
			return(pic[c].isr);
		} else {
			return(pic[c].irr);
		}
	}
}

void pic_req(int c, int level, int signal)
{
	if(signal) {
		pic[c].irr |= (1 << level);
	} else {
		pic[c].irr &= ~(1 << level);
	}
	pic_update();
}

int pic_ack()
{
	// ack (INTA=L)
	pic[pic_req_chip].isr |= pic_req_bit;
	pic[pic_req_chip].irr &= ~pic_req_bit;
	if(pic_req_chip > 0) {
		// update isr and irr of master
		UINT8 slave = 1 << (pic[pic_req_chip].icw3 & 7);
		pic[pic_req_chip - 1].isr |= slave;
		pic[pic_req_chip - 1].irr &= ~slave;
	}
	//if(pic[pic_req_chip].icw4 & 1) {
		// 8086 mode
		int vector = (pic[pic_req_chip].icw2 & 0xf8) | pic_req_level;
	//} else {
	//	// 8080 mode
	//	UINT16 addr = (UINT16)pic[pic_req_chip].icw2 << 8;
	//	if(pic[pic_req_chip].icw1 & 4) {
	//		addr |= (pic[pic_req_chip].icw1 & 0xe0) | (pic_req_level << 2);
	//	} else {
	//		addr |= (pic[pic_req_chip].icw1 & 0xc0) | (pic_req_level << 3);
	//	}
	//	vector = 0xcd | (addr << 8);
	//}
	if(pic[pic_req_chip].icw4 & 2) {
		// auto eoi
		pic[pic_req_chip].isr &= ~pic_req_bit;
	}
	return(vector);
}

void pic_update()
{
	for(int c = 0; c < 2; c++) {
		UINT8 irr = pic[c].irr;
		if(c + 1 < 2) {
			// this is master
			if(pic[c + 1].irr & (~pic[c + 1].imr)) {
				// request from slave
				irr |= 1 << (pic[c + 1].icw3 & 7);
			}
		}
		irr &= (~pic[c].imr);
		if(!irr) {
			break;
		}
		if(!(pic[c].ocw3 & 0x20)) {
			irr |= pic[c].isr;
		}
		int level = pic[c].prio;
		UINT8 bit = 1 << level;
		while(!(irr & bit)) {
			level = (level + 1) & 7;
			bit = 1 << level;
		}
		if((c + 1 < 2) && (pic[c].icw3 & bit)) {
			// check slave
			continue;
		}
		if(pic[c].isr & bit) {
			break;
		}
		// interrupt request
		pic_req_chip = c;
		pic_req_level = level;
		pic_req_bit = bit;
		CPU_IRQ_LINE(TRUE);
		return;
	}
	CPU_IRQ_LINE(FALSE);
}

// pio

void pio_init()
{
//	bool conv_mode = (get_input_code_page() == 932);
	
	memset(pio, 0, sizeof(pio));
	
	for(int c = 0; c < 2; c++) {
		pio[c].stat = 0xdf;
		pio[c].ctrl = 0x0c;
//		pio[c].conv_mode = conv_mode;
	}
}

void pio_finish()
{
	pio_release();
}

void pio_release()
{
	for(int c = 0; c < 2; c++) {
		if(pio[c].fp != NULL) {
			if(pio[c].jis_mode) {
				fputc(0x1c, pio[c].fp);
				fputc(0x2e, pio[c].fp);
			}
			fclose(pio[c].fp);
			pio[c].fp = NULL;
		}
	}
}

void pio_write(int c, UINT32 addr, UINT8 data)
{
	switch(addr & 3) {
	case 0:
		pio[c].data = data;
		break;
	case 2:
		if((pio[c].ctrl & 0x01) && !(data & 0x01)) {
			// strobe H -> L
			if(pio[c].data == 0x0d && (data & 0x02)) {
				// auto feed
				printer_out(c, 0x0d);
				printer_out(c, 0x0a);
			} else {
				printer_out(c, pio[c].data);
			}
			pio[c].stat &= ~0x40; // set ack
		}
		pio[c].ctrl = data;
		break;
	}
}

UINT8 pio_read(int c, UINT32 addr)
{
	switch(addr & 3) {
	case 0:
		if(pio[c].ctrl & 0x20) {
			// input mode
			return(0xff);
		}
		return(pio[c].data);
	case 1:
		{
			UINT8 stat = pio[c].stat;
			pio[c].stat |= 0x40; // clear ack
			return(stat);
		}
	case 2:
		return(pio[c].ctrl);
	}
	return(0xff);
}

void printer_out(int c, UINT8 data)
{
	SYSTEMTIME time;
	bool jis_mode = false;
	
	GetLocalTime(&time);
	
	if(pio[c].fp != NULL) {
		// if at least 1000ms passed from last written, close the current file
		FILETIME ftime1;
		FILETIME ftime2;
		SystemTimeToFileTime(&pio[c].time, &ftime1);
		SystemTimeToFileTime(&time, &ftime2);
		INT64 *time1 = (INT64 *)&ftime1;
		INT64 *time2 = (INT64 *)&ftime2;
		INT64 msec = (*time2 - *time1) / 10000;
		
		if(msec >= 1000) {
			if(pio[c].jis_mode) {
				fputc(0x1c, pio[c].fp);
				fputc(0x2e, pio[c].fp);
				jis_mode = true;
			}
			fclose(pio[c].fp);
			pio[c].fp = NULL;
		}
	}
	if(pio[c].fp == NULL) {
		// create a new file in the temp folder
		char file_name[MAX_PATH];
		
		sprintf(file_name, "%d-%0.2d-%0.2d_%0.2d-%0.2d-%0.2d.PRN", time.wYear, time.wMonth, time.wDay, time.wHour, time.wMinute, time.wSecond);
		if(GetTempPathA(MAX_PATH, pio[c].path)) {
			strcat(pio[c].path, file_name);
		} else {
			strcpy(pio[c].path, file_name);
		}
		pio[c].fp = fopen(pio[c].path, "w+b");
	}
	if(pio[c].fp != NULL) {
		if(jis_mode) {
			fputc(0x1c, pio[c].fp);
			fputc(0x26, pio[c].fp);
		}
		fputc(data, pio[c].fp);
		
		// reopen file if 1ch 26h 1ch 2eh (kanji-on  kanji-off) are written at the top
		if(data == 0x2e && ftell(pio[c].fp) == 4) {
			UINT8 buffer[4];
			fseek(pio[c].fp, 0, SEEK_SET);
			fread(buffer, 4, 1, pio[c].fp);
			if(buffer[0] == 0x1c && buffer[1] == 0x26 && buffer[2] == 0x1c/* && buffer[3] == 0x2e*/) {
				fclose(pio[c].fp);
				pio[c].fp = fopen(pio[c].path, "w+b");
			}
		}
		pio[c].time = time;
	}
}

// pit

#define PIT_FREQ 1193182ULL
#define PIT_COUNT_VALUE(n) ((pit[n].count_reg == 0) ? 0x10000 : (pit[n].mode == 3 && pit[n].count_reg == 1) ? 0x10001 : pit[n].count_reg)

void pit_init()
{
	memset(pit, 0, sizeof(pit));
	for(int ch = 0; ch < 3; ch++) {
		pit[ch].count = 0x10000;
		pit[ch].ctrl_reg = 0x34;
		pit[ch].mode = 3;
	}
	
	// from bochs bios
	pit_write(3, 0x34);
	pit_write(0, 0x00);
	pit_write(0, 0x00);
}

void pit_write(int ch, UINT8 val)
{
#ifndef PIT_ALWAYS_RUNNING
	if(!pit_active) {
		pit_active = 1;
		pit_init();
	}
#endif
	switch(ch) {
	case 0:
	case 1:
	case 2:
		// write count register
		if(!pit[ch].low_write && !pit[ch].high_write) {
			if(pit[ch].ctrl_reg & 0x10) {
				pit[ch].low_write = 1;
			}
			if(pit[ch].ctrl_reg & 0x20) {
				pit[ch].high_write = 1;
			}
		}
		if(pit[ch].low_write) {
			pit[ch].count_reg = val;
			pit[ch].low_write = 0;
		} else if(pit[ch].high_write) {
			if((pit[ch].ctrl_reg & 0x30) == 0x20) {
				pit[ch].count_reg = val << 8;
			} else {
				pit[ch].count_reg |= val << 8;
			}
			pit[ch].high_write = 0;
		}
		// start count
		if(!pit[ch].low_write && !pit[ch].high_write) {
			if(pit[ch].mode == 0 || pit[ch].mode == 4 || pit[ch].prev_time == 0) {
				pit[ch].count = PIT_COUNT_VALUE(ch);
				pit[ch].prev_time = timeGetTime();
				pit[ch].expired_time = pit[ch].prev_time + pit_get_expired_time(ch);
			}
		}
		break;
	case 3: // ctrl reg
		if((val & 0xc0) == 0xc0) {
			// i8254 read-back command
			for(ch = 0; ch < 3; ch++) {
				if(!(val & 0x10) && !pit[ch].status_latched) {
					pit[ch].status = pit[ch].ctrl_reg & 0x3f;
					pit[ch].status_latched = 1;
				}
				if(!(val & 0x20) && !pit[ch].count_latched) {
					pit_latch_count(ch);
				}
			}
			break;
		}
		ch = (val >> 6) & 3;
		if(val & 0x30) {
			static const int modes[8] = {0, 1, 2, 3, 4, 5, 2, 3};
			pit[ch].mode = modes[(val >> 1) & 7];
			pit[ch].count_latched = 0;
			pit[ch].low_read = pit[ch].high_read = 0;
			pit[ch].low_write = pit[ch].high_write = 0;
			pit[ch].ctrl_reg = val;
			// stop count
			pit[ch].prev_time = pit[ch].expired_time = 0;
			pit[ch].count_reg = 0;
		} else if(!pit[ch].count_latched) {
			pit_latch_count(ch);
		}
		break;
	}
}

UINT8 pit_read(int ch)
{
#ifndef PIT_ALWAYS_RUNNING
	if(!pit_active) {
		pit_active = 1;
		pit_init();
	}
#endif
	switch(ch) {
	case 0:
	case 1:
	case 2:
		if(pit[ch].status_latched) {
			pit[ch].status_latched = 0;
			return(pit[ch].status);
		}
		// if not latched, through current count
		if(!pit[ch].count_latched) {
			if(!pit[ch].low_read && !pit[ch].high_read) {
				pit_latch_count(ch);
			}
		}
		// return latched count
		if(pit[ch].low_read) {
			pit[ch].low_read = 0;
			if(!pit[ch].high_read) {
				pit[ch].count_latched = 0;
			}
			return(pit[ch].latch & 0xff);
		} else if(pit[ch].high_read) {
			pit[ch].high_read = 0;
			pit[ch].count_latched = 0;
			return((pit[ch].latch >> 8) & 0xff);
		}
	}
	return(0xff);
}

int pit_run(int ch, UINT32 cur_time)
{
	if(pit[ch].expired_time != 0 && cur_time >= pit[ch].expired_time) {
		pit[ch].count = PIT_COUNT_VALUE(ch);
		pit[ch].prev_time = pit[ch].expired_time;
		pit[ch].expired_time = pit[ch].prev_time + pit_get_expired_time(ch);
		if(cur_time >= pit[ch].expired_time) {
			pit[ch].prev_time = cur_time;
			pit[ch].expired_time = pit[ch].prev_time + pit_get_expired_time(ch);
		}
		return(1);
	}
	return(0);
}

void pit_latch_count(int ch)
{
	if(pit[ch].expired_time != 0) {
		UINT32 cur_time = timeGetTime();
		pit_run(ch, cur_time);
		UINT32 tmp = (pit[ch].count * (pit[ch].expired_time - cur_time)) / (pit[ch].expired_time - pit[ch].prev_time);
		UINT16 latch = (tmp != 0) ? (UINT16)tmp : 1;
		
		if(pit[ch].prev_latch == latch && pit[ch].expired_time > cur_time) {
			// decrement counter in 1msec period
			if(pit[ch].next_latch == 0) {
				tmp = (pit[ch].count * (pit[ch].expired_time - cur_time - 1)) / (pit[ch].expired_time - pit[ch].prev_time);
				pit[ch].next_latch = (tmp != 0) ? (UINT16)tmp : 1;
			}
			if(pit[ch].latch > pit[ch].next_latch) {
				pit[ch].latch--;
			}
		} else {
			pit[ch].prev_latch = pit[ch].latch = latch;
			pit[ch].next_latch = 0;
		}
	} else {
		pit[ch].latch = (UINT16)pit[ch].count;
		pit[ch].prev_latch = pit[ch].next_latch = 0;
	}
	pit[ch].count_latched = 1;
	if((pit[ch].ctrl_reg & 0x30) == 0x10) {
		// lower byte
		pit[ch].low_read = 1;
		pit[ch].high_read = 0;
	} else if((pit[ch].ctrl_reg & 0x30) == 0x20) {
		// upper byte
		pit[ch].low_read = 0;
		pit[ch].high_read = 1;
	} else {
		// lower -> upper
		pit[ch].low_read = pit[ch].high_read = 1;
	}
}

int pit_get_expired_time(int ch)
{
	pit[ch].accum += 1024ULL * 1000ULL * (UINT64)pit[ch].count / PIT_FREQ;
	UINT64 val = pit[ch].accum >> 10;
	pit[ch].accum -= val << 10;
	return((val != 0) ? val : 1);
}

// sio

void sio_init()
{
	memset(sio, 0, sizeof(sio));
	memset(sio_mt, 0, sizeof(sio_mt));
	
	for(int c = 0; c < 4; c++) {
		sio[c].send_buffer = new FIFO(SIO_BUFFER_SIZE);
		sio[c].recv_buffer = new FIFO(SIO_BUFFER_SIZE);
		
		sio[c].divisor.w = 12;		// 115200Hz / 9600Baud
		sio[c].line_ctrl = 0x03;	// 8bit, stop 1bit, non parity
		sio[c].modem_ctrl = 0x03;	// rts=on, dtr=on
		sio[c].set_rts = sio[c].set_dtr = true;
		sio[c].line_stat_buf = 0x60;	// send/recv buffers are empty
		sio[c].irq_identify = 0x01;	// no pending irq
		
		InitializeCriticalSection(&sio_mt[c].csSendData);
		InitializeCriticalSection(&sio_mt[c].csRecvData);
		InitializeCriticalSection(&sio_mt[c].csLineCtrl);
		InitializeCriticalSection(&sio_mt[c].csLineStat);
		InitializeCriticalSection(&sio_mt[c].csModemCtrl);
		InitializeCriticalSection(&sio_mt[c].csModemStat);
		
		if(sio_port_number[c] != 0) {
			sio[c].channel = c;
			sio_mt[c].hThread = CreateThread(NULL, 0, sio_thread, &sio[c], 0, NULL);
		}
	}
}

void sio_finish()
{
	for(int c = 0; c < 4; c++) {
		if(sio_mt[c].hThread != NULL) {
			WaitForSingleObject(sio_mt[c].hThread, INFINITE);
			CloseHandle(sio_mt[c].hThread);
			sio_mt[c].hThread = NULL;
		}
		DeleteCriticalSection(&sio_mt[c].csSendData);
		DeleteCriticalSection(&sio_mt[c].csRecvData);
		DeleteCriticalSection(&sio_mt[c].csLineCtrl);
		DeleteCriticalSection(&sio_mt[c].csLineStat);
		DeleteCriticalSection(&sio_mt[c].csModemCtrl);
		DeleteCriticalSection(&sio_mt[c].csModemStat);
	}
	sio_release();
}

void sio_release()
{
	for(int c = 0; c < 4; c++) {
		// sio_thread() may access the resources :-(
		bool running = (sio_mt[c].hThread != NULL);
		
		if(running) {
			EnterCriticalSection(&sio_mt[c].csSendData);
		}
		if(sio[c].send_buffer != NULL) {
			sio[c].send_buffer->release();
			delete sio[c].send_buffer;
			sio[c].send_buffer = NULL;
		}
		if(running) {
			LeaveCriticalSection(&sio_mt[c].csSendData);
			EnterCriticalSection(&sio_mt[c].csRecvData);
		}
		if(sio[c].recv_buffer != NULL) {
			sio[c].recv_buffer->release();
			delete sio[c].recv_buffer;
			sio[c].recv_buffer = NULL;
		}
		if(running) {
			LeaveCriticalSection(&sio_mt[c].csRecvData);
		}
	}
}

void sio_write(int c, UINT32 addr, UINT8 data)
{
	switch(addr & 7) {
	case 0:
		if(sio[c].selector & 0x80) {
			if(sio[c].divisor.b.l != data) {
				EnterCriticalSection(&sio_mt[c].csLineCtrl);
				sio[c].divisor.b.l = data;
				LeaveCriticalSection(&sio_mt[c].csLineCtrl);
			}
		} else {
			EnterCriticalSection(&sio_mt[c].csSendData);
			if(sio[c].send_buffer != NULL) {
				sio[c].send_buffer->write(data);
			}
			// transmitter holding/shift registers are not empty
			sio[c].line_stat_buf &= ~0x60;
			LeaveCriticalSection(&sio_mt[c].csSendData);
			
			if(sio[c].irq_enable & 0x02) {
				sio_update_irq(c);
			}
		}
		break;
	case 1:
		if(sio[c].selector & 0x80) {
			if(sio[c].divisor.b.h != data) {
				EnterCriticalSection(&sio_mt[c].csLineCtrl);
				sio[c].divisor.b.h = data;
				LeaveCriticalSection(&sio_mt[c].csLineCtrl);
			}
		} else {
			if(sio[c].irq_enable != data) {
				sio[c].irq_enable = data;
				sio_update_irq(c);
			}
		}
		break;
	case 3:
		{
			UINT8 line_ctrl = data & 0x3f;
			bool set_brk = ((data & 0x40) != 0);
			
			if(sio[c].line_ctrl != line_ctrl) {
				EnterCriticalSection(&sio_mt[c].csLineCtrl);
				sio[c].line_ctrl = line_ctrl;
				LeaveCriticalSection(&sio_mt[c].csLineCtrl);
			}
			if(sio[c].set_brk != set_brk) {
				EnterCriticalSection(&sio_mt[c].csModemCtrl);
				sio[c].set_brk = set_brk;
				LeaveCriticalSection(&sio_mt[c].csModemCtrl);
			}
		}
		sio[c].selector = data;
		break;
	case 4:
		{
			bool set_dtr = ((data & 0x01) != 0);
			bool set_rts = ((data & 0x02) != 0);
			
			if(sio[c].set_dtr != set_dtr || sio[c].set_rts != set_rts) {
//				EnterCriticalSection(&sio_mt[c].csModemCtrl);
				sio[c].set_dtr = set_dtr;
				sio[c].set_rts = set_rts;
//				LeaveCriticalSection(&sio_mt[c].csModemCtrl);
				
				bool state_changed = false;
				
				EnterCriticalSection(&sio_mt[c].csModemStat);
				if(set_dtr) {
					sio[c].modem_stat |= 0x20;	// dsr on
				} else {
					sio[c].modem_stat &= ~0x20;	// dsr off
				}
				if(set_rts) {
					sio[c].modem_stat |= 0x10;	// cts on
				} else {
					sio[c].modem_stat &= ~0x10;	// cts off
				}
				if((sio[c].prev_modem_stat & 0x20) != (sio[c].modem_stat & 0x20)) {
					if(!(sio[c].modem_stat & 0x02)) {
						if(sio[c].irq_enable & 0x08) {
							state_changed = true;
						}
						sio[c].modem_stat |= 0x02;
					}
				}
				if((sio[c].prev_modem_stat & 0x10) != (sio[c].modem_stat & 0x10)) {
					if(!(sio[c].modem_stat & 0x01)) {
						if(sio[c].irq_enable & 0x08) {
							state_changed = true;
						}
						sio[c].modem_stat |= 0x01;
					}
				}
				LeaveCriticalSection(&sio_mt[c].csModemStat);
				
				if(state_changed) {
					sio_update_irq(c);
				}
			}
		}
		sio[c].modem_ctrl = data;
		break;
	case 7:
		sio[c].scratch = data;
		break;
	}
}

UINT8 sio_read(int c, UINT32 addr)
{
	switch(addr & 7) {
	case 0:
		if(sio[c].selector & 0x80) {
			return(sio[c].divisor.b.l);
		} else {
			EnterCriticalSection(&sio_mt[c].csRecvData);
			UINT8 data = 0;
			if(sio[c].recv_buffer != NULL) {
				data = sio[c].recv_buffer->read();
			}
			// data is not ready
			sio[c].line_stat_buf &= ~0x01;
			LeaveCriticalSection(&sio_mt[c].csRecvData);
			
			if(sio[c].irq_enable & 0x01) {
				sio_update_irq(c);
			}
			return(data);
		}
	case 1:
		if(sio[c].selector & 0x80) {
			return(sio[c].divisor.b.h);
		} else {
			return(sio[c].irq_enable);
		}
	case 2:
		return(sio[c].irq_identify);
	case 3:
		return(sio[c].selector);
	case 4:
		return(sio[c].modem_ctrl);
	case 5:
		{
			EnterCriticalSection(&sio_mt[c].csLineStat);
			UINT8 val = sio[c].line_stat_err | sio[c].line_stat_buf;
			sio[c].line_stat_err = 0x00;
			LeaveCriticalSection(&sio_mt[c].csLineStat);
			
			bool state_changed = false;
			
			if((sio[c].line_stat_buf & 0x60) == 0x00) {
				EnterCriticalSection(&sio_mt[c].csSendData);
				if(sio[c].send_buffer != NULL && !sio[c].send_buffer->full()) {
					// transmitter holding register will be empty first
					if(sio[c].irq_enable & 0x02) {
						state_changed = true;
					}
					sio[c].line_stat_buf |= 0x20;
				}
				LeaveCriticalSection(&sio_mt[c].csSendData);
			} else if((sio[c].line_stat_buf & 0x60) == 0x20) {
				// transmitter shift register will be empty later
				sio[c].line_stat_buf |= 0x40;
			}
			if(!(sio[c].line_stat_buf & 0x01)) {
				EnterCriticalSection(&sio_mt[c].csRecvData);
				if(sio[c].recv_buffer != NULL && !sio[c].recv_buffer->empty()) {
					// data is ready
					if(sio[c].irq_enable & 0x01) {
						state_changed = true;
					}
					sio[c].line_stat_buf |= 0x01;
				}
				LeaveCriticalSection(&sio_mt[c].csRecvData);
			}
			if(state_changed) {
				sio_update_irq(c);
			}
			return(val);
		}
	case 6:
		{
			EnterCriticalSection(&sio_mt[c].csModemStat);
			UINT8 val = sio[c].modem_stat;
			sio[c].modem_stat &= 0xf0;
			sio[c].prev_modem_stat = sio[c].modem_stat;
			LeaveCriticalSection(&sio_mt[c].csModemStat);
			
			if(sio[c].modem_ctrl & 0x10) {
				// loop-back
				val &= 0x0f;
				val |= (sio[c].modem_ctrl & 0x0c) << 4;
				val |= (sio[c].modem_ctrl & 0x01) << 5;
				val |= (sio[c].modem_ctrl & 0x02) << 3;
			}
			return(val);
		}
	case 7:
		return(sio[c].scratch);
	}
	return(0xff);
}

void sio_update(int c)
{
	if((sio[c].line_stat_buf & 0x60) == 0x00) {
		EnterCriticalSection(&sio_mt[c].csSendData);
		if(sio[c].send_buffer != NULL && !sio[c].send_buffer->full()) {
			// transmitter holding/shift registers will be empty
			sio[c].line_stat_buf |= 0x60;
		}
		LeaveCriticalSection(&sio_mt[c].csSendData);
	} else if((sio[c].line_stat_buf & 0x60) == 0x20) {
		// transmitter shift register will be empty
		sio[c].line_stat_buf |= 0x40;
	}
	if(!(sio[c].line_stat_buf & 0x01)) {
		EnterCriticalSection(&sio_mt[c].csRecvData);
		if(sio[c].recv_buffer != NULL && !sio[c].recv_buffer->empty()) {
			// data is ready
			sio[c].line_stat_buf |= 0x01;
		}
		LeaveCriticalSection(&sio_mt[c].csRecvData);
	}
	sio_update_irq(c);
}

void sio_update_irq(int c)
{
	int level = -1;
	
	if(sio[c].irq_enable & 0x08) {
		EnterCriticalSection(&sio_mt[c].csModemStat);
		if((sio[c].modem_stat & 0x0f) != 0) {
			level = 0;
		}
		EnterCriticalSection(&sio_mt[c].csModemStat);
	}
	if(sio[c].irq_enable & 0x02) {
		if(sio[c].line_stat_buf & 0x20) {
			level = 1;
		}
	}
	if(sio[c].irq_enable & 0x01) {
		if(sio[c].line_stat_buf & 0x01) {
			level = 2;
		}
	}
	if(sio[c].irq_enable & 0x04) {
		EnterCriticalSection(&sio_mt[c].csLineStat);
		if(sio[c].line_stat_err != 0) {
			level = 3;
		}
		LeaveCriticalSection(&sio_mt[c].csLineStat);
	}
	
	// COM1 and COM3 shares IRQ4, COM2 and COM4 shares IRQ3
	if(level != -1) {
		sio[c].irq_identify = level << 1;
		pic_req(0, (c == 0 || c == 2) ? 4 : 3, 1);
	} else {
		sio[c].irq_identify = 1;
		pic_req(0, (c == 0 || c == 2) ? 4 : 3, 0);
	}
}

DWORD WINAPI sio_thread(void *lpx)
{
	volatile sio_t *p = (sio_t *)lpx;
	sio_mt_t *q = &sio_mt[p->channel];
	
	char name[] = "COM1";
	name[3] = '0' + sio_port_number[p->channel];
	HANDLE hComm = NULL;
	COMMPROP commProp;
	DCB dcb;
	DWORD dwSettableBaud = 0xffb; // 75, 110, 150, 300, 600, 1200, 1800, 2400, 4800, 7200, and 9600bps
	BYTE bytBuffer[SIO_BUFFER_SIZE];
	
	if((hComm = CreateFileA(name, GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, 0, NULL)) != INVALID_HANDLE_VALUE) {
		if(GetCommProperties(hComm, &commProp)) {
			dwSettableBaud = commProp.dwSettableBaud;
		}
		EscapeCommFunction(hComm, CLRBREAK);
//		EscapeCommFunction(hComm, SETRTS);
//		EscapeCommFunction(hComm, SETDTR);
		
		while(!msdos_exit) {
			// setup comm port
			bool comm_state_changed = false;
			
			EnterCriticalSection(&q->csLineCtrl);
			if((p->prev_divisor != p->divisor.w || p->prev_line_ctrl != p->line_ctrl) && p->divisor.w != 0) {
				p->prev_divisor = p->divisor.w;
				p->prev_line_ctrl = p->line_ctrl;
				comm_state_changed = true;
			}
			LeaveCriticalSection(&q->csLineCtrl);
			
			if(comm_state_changed) {
				if(GetCommState(hComm, &dcb)) {
//					dcb.BaudRate = min(9600, 115200 / p->prev_divisor);
					DWORD baud = 115200 / p->prev_divisor;
					dcb.BaudRate = 9600; // default
					
					if((dwSettableBaud & BAUD_075  ) && baud >= 75   ) dcb.BaudRate = 75;
					if((dwSettableBaud & BAUD_110  ) && baud >= 110  ) dcb.BaudRate = 110;
					// 134.5bps is not supported ???
//					if((dwSettableBaud & BAUD_134_5) && baud >= 134.5) dcb.BaudRate = 134.5;
					if((dwSettableBaud & BAUD_150  ) && baud >= 150  ) dcb.BaudRate = 150;
					if((dwSettableBaud & BAUD_300  ) && baud >= 300  ) dcb.BaudRate = 300;
					if((dwSettableBaud & BAUD_600  ) && baud >= 600  ) dcb.BaudRate = 600;
					if((dwSettableBaud & BAUD_1200 ) && baud >= 1200 ) dcb.BaudRate = 1200;
					if((dwSettableBaud & BAUD_1800 ) && baud >= 1800 ) dcb.BaudRate = 1800;
					if((dwSettableBaud & BAUD_2400 ) && baud >= 2400 ) dcb.BaudRate = 2400;
					if((dwSettableBaud & BAUD_4800 ) && baud >= 4800 ) dcb.BaudRate = 4800;
					if((dwSettableBaud & BAUD_7200 ) && baud >= 7200 ) dcb.BaudRate = 7200;
					if((dwSettableBaud & BAUD_9600 ) && baud >= 9600 ) dcb.BaudRate = 9600;
//					if((dwSettableBaud & BAUD_14400) && baud >= 14400) dcb.BaudRate = 14400;
//					if((dwSettableBaud & BAUD_19200) && baud >= 19200) dcb.BaudRate = 19200;
//					if((dwSettableBaud & BAUD_38400) && baud >= 38400) dcb.BaudRate = 38400;
					
					switch(p->prev_line_ctrl & 0x03) {
					case 0x00: dcb.ByteSize = 5; break;
					case 0x01: dcb.ByteSize = 6; break;
					case 0x02: dcb.ByteSize = 7; break;
					case 0x03: dcb.ByteSize = 8; break;
					}
					switch(p->prev_line_ctrl & 0x04) {
					case 0x00: dcb.StopBits = ONESTOPBIT; break;
					case 0x04: dcb.StopBits = (dcb.ByteSize == 5) ? ONE5STOPBITS : TWOSTOPBITS; break;
					}
					switch(p->prev_line_ctrl & 0x38) {
					case 0x08: dcb.Parity = ODDPARITY;   break;
					case 0x18: dcb.Parity = EVENPARITY;  break;
					case 0x28: dcb.Parity = MARKPARITY;  break;
					case 0x38: dcb.Parity = SPACEPARITY; break;
					default:   dcb.Parity = NOPARITY;    break;
					}
					dcb.fBinary = TRUE;
					dcb.fParity = (dcb.Parity != NOPARITY);
					dcb.fOutxCtsFlow = dcb.fOutxDsrFlow = TRUE;
					dcb.fDtrControl = DTR_CONTROL_HANDSHAKE;
					dcb.fDsrSensitivity = FALSE;//TRUE;
					dcb.fTXContinueOnXoff = TRUE;
					dcb.fOutX = dcb.fInX = FALSE;
					dcb.fErrorChar = FALSE;
					dcb.fNull = FALSE;
					dcb.fRtsControl = RTS_CONTROL_HANDSHAKE;
					dcb.fAbortOnError = FALSE;
					
					SetCommState(hComm, &dcb);
				}
				
				// check again to apply all comm state changes
				Sleep(10);
				continue;
			}
			
			// set comm pins
			bool change_brk = false;
//			bool change_rts = false;
//			bool change_dtr = false;
			
			EnterCriticalSection(&q->csModemCtrl);
			if(p->prev_set_brk != p->set_brk) {
				p->prev_set_brk = p->set_brk;
				change_brk = true;
			}
//			if(p->prev_set_rts != p->set_rts) {
//				p->prev_set_rts = p->set_rts;
//				change_rts = true;
//			}
//			if(p->prev_set_dtr != p->set_dtr) {
//				p->prev_set_dtr = p->set_dtr;
//				change_dtr = true;
//			}
			LeaveCriticalSection(&q->csModemCtrl);
			
			if(change_brk) {
				static UINT32 clear_time = 0;
				if(p->prev_set_brk) {
					EscapeCommFunction(hComm, SETBREAK);
					clear_time = timeGetTime() + 200;
				} else {
					// keep break for at least 200msec
					UINT32 cur_time = timeGetTime();
					if(clear_time > cur_time) {
						Sleep(clear_time - cur_time);
					}
					EscapeCommFunction(hComm, CLRBREAK);
				}
			}
//			if(change_rts) {
//				if(p->prev_set_rts) {
//					EscapeCommFunction(hComm, SETRTS);
//				} else {
//					EscapeCommFunction(hComm, CLRRTS);
//				}
//			}
//			if(change_dtr) {
//				if(p->prev_set_dtr) {
//					EscapeCommFunction(hComm, SETDTR);
//				} else {
//					EscapeCommFunction(hComm, CLRDTR);
//				}
//			}
			
			// get comm pins
			DWORD dwModemStat = 0;
			
			if(GetCommModemStatus(hComm, &dwModemStat)) {
				EnterCriticalSection(&q->csModemStat);
				if(dwModemStat & MS_RLSD_ON) {
					p->modem_stat |= 0x80;
				} else {
					p->modem_stat &= ~0x80;
				}
				if(dwModemStat & MS_RING_ON) {
					p->modem_stat |= 0x40;
				} else {
					p->modem_stat &= ~0x40;
				}
//				if(dwModemStat & MS_DSR_ON) {
//					p->modem_stat |= 0x20;
//				} else {
//					p->modem_stat &= ~0x20;
//				}
//				if(dwModemStat & MS_CTS_ON) {
//					p->modem_stat |= 0x10;
//				} else {
//					p->modem_stat &= ~0x10;
//				}
				if((p->prev_modem_stat & 0x80) != (p->modem_stat & 0x80)) {
					p->modem_stat |= 0x08;
				}
				if((p->prev_modem_stat & 0x40) && !(p->modem_stat & 0x40)) {
					p->modem_stat |= 0x04;
				}
//				if((p->prev_modem_stat & 0x20) != (p->modem_stat & 0x20)) {
//					p->modem_stat |= 0x02;
//				}
//				if((p->prev_modem_stat & 0x10) != (p->modem_stat & 0x10)) {
//					p->modem_stat |= 0x01;
//				}
				LeaveCriticalSection(&q->csModemStat);
			}
			
			// send data
			DWORD dwSend = 0;
			
			EnterCriticalSection(&q->csSendData);
			while(p->send_buffer != NULL && !p->send_buffer->empty()) {
				bytBuffer[dwSend++] = p->send_buffer->read();
			}
			LeaveCriticalSection(&q->csSendData);
			
			if(dwSend != 0) {
				DWORD dwWritten = 0;
				WriteFile(hComm, bytBuffer, dwSend, &dwWritten, NULL);
			}
			
			// get line status and recv data
			DWORD dwLineStat = 0;
			COMSTAT comStat;
			
			if(ClearCommError(hComm, &dwLineStat, &comStat)) {
				EnterCriticalSection(&q->csLineStat);
				if(dwLineStat & CE_BREAK) {
					p->line_stat_err |= 0x10;
				}
				if(dwLineStat & CE_FRAME) {
					p->line_stat_err |= 0x08;
				}
				if(dwLineStat & CE_RXPARITY) {
					p->line_stat_err |= 0x04;
				}
				if(dwLineStat & CE_OVERRUN) {
					p->line_stat_err |= 0x02;
				}
				LeaveCriticalSection(&q->csLineStat);
				
				if(comStat.cbInQue != 0) {
					EnterCriticalSection(&q->csRecvData);
					DWORD dwRecv = 0;
					if(p->recv_buffer != NULL) {
						dwRecv = min(comStat.cbInQue, p->recv_buffer->remain());
					}
					LeaveCriticalSection(&q->csRecvData);
					
					if(dwRecv != 0) {
						DWORD dwRead = 0;
						if(ReadFile(hComm, bytBuffer, dwRecv, &dwRead, NULL) && dwRead != 0) {
							EnterCriticalSection(&q->csRecvData);
							if(p->recv_buffer != NULL) {
								for(int i = 0; i < dwRead; i++) {
									p->recv_buffer->write(bytBuffer[i]);
								}
							}
							LeaveCriticalSection(&q->csRecvData);
						}
					}
				}
			}
			Sleep(10);
		}
		CloseHandle(hComm);
	}
	return 0;
}

// cmos

void cmos_init()
{
	memset(cmos, 0, sizeof(cmos));
	cmos_addr = 0;
	
	// from DOSBox
	cmos_write(0x0a, 0x26);
	cmos_write(0x0b, 0x02);
	cmos_write(0x0d, 0x80);
}

void cmos_write(int addr, UINT8 val)
{
	cmos[addr & 0x7f] = val;
}

#define CMOS_GET_TIME() { \
	UINT32 cur_sec = timeGetTime() / 1000 ; \
	if(prev_sec != cur_sec) { \
		GetLocalTime(&time); \
		prev_sec = cur_sec; \
	} \
}
#define CMOS_BCD(v) ((cmos[0x0b] & 4) ? (v) : to_bcd(v))

UINT8 cmos_read(int addr)
{
	static SYSTEMTIME time;
	static UINT32 prev_sec = 0;
	
	switch(addr & 0x7f) {
	case 0x00: CMOS_GET_TIME(); return(CMOS_BCD(time.wSecond));
	case 0x02: CMOS_GET_TIME(); return(CMOS_BCD(time.wMinute));
	case 0x04: CMOS_GET_TIME(); return(CMOS_BCD(time.wHour));
	case 0x06: CMOS_GET_TIME(); return(time.wDayOfWeek + 1);
	case 0x07: CMOS_GET_TIME(); return(CMOS_BCD(time.wDay));
	case 0x08: CMOS_GET_TIME(); return(CMOS_BCD(time.wMonth));
	case 0x09: CMOS_GET_TIME(); return(CMOS_BCD(time.wYear));
//	case 0x0a: return((cmos[0x0a] & 0x7f) | ((timeGetTime() % 1000) < 2 ? 0x80 : 0));	// 2msec
	case 0x0a: return((cmos[0x0a] & 0x7f) | ((timeGetTime() % 1000) < 20 ? 0x80 : 0));	// precision of timeGetTime() may not be 1msec
	case 0x15: return((MEMORY_END >> 10) & 0xff);
	case 0x16: return((MEMORY_END >> 18) & 0xff);
	case 0x17: return(((MAX_MEM - 0x100000) >> 10) & 0xff);
	case 0x18: return(((MAX_MEM - 0x100000) >> 18) & 0xff);
	case 0x30: return(((MAX_MEM - 0x100000) >> 10) & 0xff);
	case 0x31: return(((MAX_MEM - 0x100000) >> 18) & 0xff);
	case 0x32: CMOS_GET_TIME(); return(CMOS_BCD(time.wYear / 100));
	}
	return(cmos[addr & 0x7f]);
}

// kbd (a20)

void kbd_init()
{
	kbd_data = kbd_command = 0;
	kbd_status = 0x18;
}

void kbd_reset()
{
	if((cmos[0x0f] & 0x7f) == 9) {
		CPU_RESET();
		CPU_LOAD_SREG(CPU_SS_INDEX, *(UINT16 *)(mem + 0x469));
		CPU_SP = *(UINT16 *)(mem + 0x467);
		CPU_LOAD_SREG(CPU_ES_INDEX, CPU_POP());
		CPU_LOAD_SREG(CPU_DS_INDEX, CPU_POP());
		CPU_DI = CPU_POP();
		CPU_SI = CPU_POP();
		CPU_BP = CPU_POP();
		CPU_POP();
		CPU_BX = CPU_POP();
		CPU_DX = CPU_POP();
		CPU_CX = CPU_POP();
		CPU_AX = CPU_POP();
		CPU_IRET();
	} else {
		if((cmos[0x0f] & 0x7f) == 5) {
			// reset pic
			pic_init();
			pic[0].irr = pic[1].irr = 0x00;
			pic[0].imr = pic[1].imr = 0xff;
		}
		CPU_RESET();
		UINT16 address = *(UINT16 *)(mem + 0x467);
		UINT16 selector = *(UINT16 *)(mem + 0x469);
		CPU_JMP_FAR(selector, address);
	}
}

UINT8 kbd_read_data()
{
	UINT8 data = kbd_data;
	kbd_data = 0;
	kbd_status &= ~1;
	return(data);
}

void kbd_write_data(UINT8 val)
{
	switch(kbd_command) {
	case 0x00:
		switch(val) {
		case 0xed:
		case 0xf3:
			kbd_command = val;
		default:
			kbd_data = 0xfa;
			kbd_status |= 1;
			break;
		}
		break;
	case 0xd1:
		CPU_A20_LINE((val >> 1) & 1);
		kbd_command = 0;
		break;
	case 0xed:
		kbd_command = 0;
		break;
	case 0xf3:
		kbd_command = 0;
		kbd_data = 0xfa;
		kbd_status |= 1;
		break;
	}
	kbd_status &= ~8;
}

UINT8 kbd_read_status()
{
	return(kbd_status);
}

void kbd_write_command(UINT8 val)
{
	switch(val) {
	case 0xd0:
		kbd_data = ((CPU_ADRSMASK >> 19) & 2) | 1;
		kbd_status |= 1;
		break;
	case 0xd1:
		kbd_command = val;
		break;
	case 0xdd:
		CPU_A20_LINE(0);
		break;
	case 0xdf:
		CPU_A20_LINE(1);
		break;
	case 0xf0: case 0xf1: case 0xf2: case 0xf3: case 0xf4: case 0xf5: case 0xf6: case 0xf7:
	case 0xf8: case 0xf9: case 0xfa: case 0xfb: case 0xfc: case 0xfd: case 0xfe: case 0xff:
		if(!(val & 1)) {
			kbd_reset();
		}
		CPU_A20_LINE((val >> 1) & 1);
		break;
	}
	kbd_status |= 8;
}

// vga

UINT8 vga_read_status()
{
	// 60hz
	static const int period[3] = {16, 17, 17};
	static int index = 0;
	UINT32 time = timeGetTime() % period[index];
	
	index = (index + 1) % 3;
	return((time < 4 ? 0x08 : 0) | (time == 0 ? 0 : 0x01));
}

// i/o bus

// this is ugly patch for SW1US.EXE, it sometimes mistakely read/write 01h-10h for serial I/O
//#define SW1US_PATCH

UINT8 read_io_byte(UINT32 addr)
#ifdef USE_DEBUGGER
{
	if(now_debugging) {
		for(int i = 0; i < MAX_BREAK_POINTS; i++) {
			if(in_break_point.table[i].status == 1) {
				if(addr == in_break_point.table[i].addr) {
					in_break_point.hit = i + 1;
					now_suspended = true;
					break;
				}
			}
		}
	}
	return(debugger_read_io_byte(addr));
}
UINT8 debugger_read_io_byte(UINT32 addr)
#endif
{
	UINT8 val = 0xff;
	
	switch(addr) {
#ifdef SW1US_PATCH
	case 0x01: case 0x02: case 0x03: case 0x04: case 0x05: case 0x06: case 0x07: case 0x08:
	case 0x09: case 0x0a: case 0x0b: case 0x0c: case 0x0d: case 0x0e: case 0x0f: case 0x10:
		val = sio_read(0, addr - 1);
		break;
#else
	case 0x00: case 0x01: case 0x02: case 0x03: case 0x04: case 0x05: case 0x06: case 0x07:
	case 0x08: case 0x09: case 0x0a: case 0x0b: case 0x0c: case 0x0d: case 0x0e: case 0x0f:
		val = dma_read(0, addr);
		break;
#endif
	case 0x20: case 0x21:
		val = pic_read(0, addr);
		break;
	case 0x40: case 0x41: case 0x42: case 0x43:
		val = pit_read(addr & 0x03);
		break;
	case 0x60:
		val = kbd_read_data();
		break;
	case 0x61:
		val = system_port;
		break;
	case 0x64:
		val = kbd_read_status();
		break;
	case 0x71:
		val = cmos_read(cmos_addr);
		break;
	case 0x81:
		val = dma_page_read(0, 2);
		break;
	case 0x82:
		val = dma_page_read(0, 3);
		break;
	case 0x83:
		val = dma_page_read(0, 1);
		break;
	case 0x87:
		val = dma_page_read(0, 0);
		break;
	case 0x89:
		val = dma_page_read(1, 2);
		break;
	case 0x8a:
		val = dma_page_read(1, 3);
		break;
	case 0x8b:
		val = dma_page_read(1, 1);
		break;
	case 0x8f:
		val = dma_page_read(1, 0);
		break;
	case 0x92:
		val = (CPU_ADRSMASK >> 19) & 2;
		break;
	case 0xa0: case 0xa1:
		val = pic_read(1, addr);
		break;
	case 0xc0: case 0xc2: case 0xc4: case 0xc6: case 0xc8: case 0xca: case 0xcc: case 0xce:
	case 0xd0: case 0xd2: case 0xd4: case 0xd6: case 0xd8: case 0xda: case 0xdc: case 0xde:
		val = dma_read(1, (addr - 0xc0) >> 1);
		break;
	case 0x278: case 0x279: case 0x27a:
		val = pio_read(1, addr);
		break;
	case 0x2e8: case 0x2e9: case 0x2ea: case 0x2eb: case 0x2ec: case 0x2ed: case 0x2ee: case 0x2ef:
		val = sio_read(3, addr);
		break;
	case 0x2f8: case 0x2f9: case 0x2fa: case 0x2fb: case 0x2fc: case 0x2fd: case 0x2fe: case 0x2ff:
		val = sio_read(1, addr);
		break;
	case 0x378: case 0x379: case 0x37a:
		val = pio_read(0, addr);
		break;
	case 0x3ba: case 0x3da:
		val = vga_read_status();
		break;
	case 0x3bc: case 0x3bd: case 0x3be:
		val = pio_read(2, addr);
		break;
	case 0x3d5:
		if(crtc_addr < 16) {
			val = crtc_regs[crtc_addr];
		}
		break;
	case 0x3e8: case 0x3e9: case 0x3ea: case 0x3eb: case 0x3ec: case 0x3ed: case 0x3ee: case 0x3ef:
		val = sio_read(2, addr);
		break;
	case 0x3f8: case 0x3f9: case 0x3fa: case 0x3fb: case 0x3fc: case 0x3fd: case 0x3fe: case 0x3ff:
		val = sio_read(0, addr);
		break;
	default:
//		fatalerror("unknown inb %4x\n", addr);
		break;
	}
#ifdef ENABLE_DEBUG_IOPORT
	if(fp_debug_log != NULL) {
		fprintf(fp_debug_log, "inb %04X, %02X\n", addr, val);
	}
#endif
	return(val);
}

UINT16 read_io_word(UINT32 addr)
{
	return(read_io_byte(addr) | (read_io_byte(addr + 1) << 8));
}

#ifdef USE_DEBUGGER
UINT16 debugger_read_io_word(UINT32 addr)
{
	return(debugger_read_io_byte(addr) | (debugger_read_io_byte(addr + 1) << 8));
}
#endif

UINT32 read_io_dword(UINT32 addr)
{
	return(read_io_byte(addr) | (read_io_byte(addr + 1) << 8) | (read_io_byte(addr + 2) << 16) | (read_io_byte(addr + 3) << 24));
}

#ifdef USE_DEBUGGER
UINT32 debugger_read_io_dword(UINT32 addr)
{
	return(debugger_read_io_byte(addr) | (debugger_read_io_byte(addr + 1) << 8) | (debugger_read_io_byte(addr + 2) << 16) | (debugger_read_io_byte(addr + 3) << 24));
}
#endif

void write_io_byte(UINT32 addr, UINT8 val)
#ifdef USE_DEBUGGER
{
	if(now_debugging) {
		for(int i = 0; i < MAX_BREAK_POINTS; i++) {
			if(out_break_point.table[i].status == 1) {
				if(addr == out_break_point.table[i].addr) {
					out_break_point.hit = i + 1;
					now_suspended = true;
					break;
				}
			}
		}
	}
	debugger_write_io_byte(addr, val);
}
void debugger_write_io_byte(UINT32 addr, UINT8 val)
#endif
{
#ifdef ENABLE_DEBUG_IOPORT
	if(fp_debug_log != NULL) {
#ifdef USE_SERVICE_THREAD
		if(addr != 0xf7)
#endif
		fprintf(fp_debug_log, "outb %04X, %02X\n", addr, val);
	}
#endif
	switch(addr) {
#ifdef SW1US_PATCH
	case 0x01: case 0x02: case 0x03: case 0x04: case 0x05: case 0x06: case 0x07: case 0x08:
	case 0x09: case 0x0a: case 0x0b: case 0x0c: case 0x0d: case 0x0e: case 0x0f: case 0x10:
		sio_write(0, addr - 1, val);
		break;
#else
	case 0x00: case 0x01: case 0x02: case 0x03: case 0x04: case 0x05: case 0x06: case 0x07:
	case 0x08: case 0x09: case 0x0a: case 0x0b: case 0x0c: case 0x0d: case 0x0e: case 0x0f:
		dma_write(0, addr, val);
		break;
#endif
	case 0x20: case 0x21:
		pic_write(0, addr, val);
		break;
	case 0x40: case 0x41: case 0x42: case 0x43:
		pit_write(addr & 0x03, val);
		break;
	case 0x60:
		kbd_write_data(val);
		break;
	case 0x61:
		if((system_port & 3) != 3 && (val & 3) == 3) {
			// beep on
//			MessageBeep(-1);
		} else if((system_port & 3) == 3 && (val & 3) != 3) {
			// beep off
		}
		system_port = val;
		break;
	case 0x64:
		kbd_write_command(val);
		break;
	case 0x70:
		cmos_addr = val;
		break;
	case 0x71:
		cmos_write(cmos_addr, val);
		break;
	case 0x81:
		dma_page_write(0, 2, val);
	case 0x82:
		dma_page_write(0, 3, val);
	case 0x83:
		dma_page_write(0, 1, val);
	case 0x87:
		dma_page_write(0, 0, val);
	case 0x89:
		dma_page_write(1, 2, val);
	case 0x8a:
		dma_page_write(1, 3, val);
	case 0x8b:
		dma_page_write(1, 1, val);
	case 0x8f:
		dma_page_write(1, 0, val);
	case 0x92:
		CPU_A20_LINE((val >> 1) & 1);
		break;
	case 0xa0: case 0xa1:
		pic_write(1, addr, val);
		break;
	case 0xc0: case 0xc2: case 0xc4: case 0xc6: case 0xc8: case 0xca: case 0xcc: case 0xce:
	case 0xd0: case 0xd2: case 0xd4: case 0xd6: case 0xd8: case 0xda: case 0xdc: case 0xde:
		dma_write(1, (addr - 0xc0) >> 1, val);
		break;
#ifdef USE_SERVICE_THREAD
	case 0xf7:
		// dummy i/o for BIOS/DOS service
		if(in_service && cursor_moved) {
			// update cursor position before service is done
			pcbios_update_cursor_position();
			cursor_moved = false;
		}
		finish_service_loop();
		break;
#endif
	case 0x278: case 0x279: case 0x27a:
		pio_write(1, addr, val);
		break;
	case 0x2e8: case 0x2e9: case 0x2ea: case 0x2eb: case 0x2ec: case 0x2ed: case 0x2ee: case 0x2ef:
		sio_write(3, addr, val);
		break;
	case 0x2f8: case 0x2f9: case 0x2fa: case 0x2fb: case 0x2fc: case 0x2fd: case 0x2fe: case 0x2ff:
		sio_write(1, addr, val);
		break;
	case 0x378: case 0x379: case 0x37a:
		pio_write(0, addr, val);
		break;
	case 0x3bc: case 0x3bd: case 0x3be:
		pio_write(2, addr, val);
		break;
	case 0x3d4:
		crtc_addr = val;
		break;
	case 0x3d5:
		if(crtc_addr < 16) {
			if(crtc_regs[crtc_addr] != val) {
				crtc_regs[crtc_addr] = val;
				crtc_changed[crtc_addr] = 1;
			}
		}
		break;
	case 0x3e8: case 0x3e9: case 0x3ea: case 0x3eb: case 0x3ec: case 0x3ed: case 0x3ee: case 0x3ef:
		sio_write(2, addr, val);
		break;
	case 0x3f8: case 0x3f9: case 0x3fa: case 0x3fb: case 0x3fc: case 0x3fd: case 0x3fe: case 0x3ff:
		sio_write(0, addr, val);
		break;
	default:
//		fatalerror("unknown outb %4x,%2x\n", addr, val);
		break;
	}
}

void write_io_word(UINT32 addr, UINT16 val)
{
	write_io_byte(addr + 0, (val >> 0) & 0xff);
	write_io_byte(addr + 1, (val >> 8) & 0xff);
}

#ifdef USE_DEBUGGER
void debugger_write_io_word(UINT32 addr, UINT16 val)
{
	debugger_write_io_byte(addr + 0, (val >> 0) & 0xff);
	debugger_write_io_byte(addr + 1, (val >> 8) & 0xff);
}
#endif

void write_io_dword(UINT32 addr, UINT32 val)
{
	write_io_byte(addr + 0, (val >>  0) & 0xff);
	write_io_byte(addr + 1, (val >>  8) & 0xff);
	write_io_byte(addr + 2, (val >> 16) & 0xff);
	write_io_byte(addr + 3, (val >> 24) & 0xff);
}

#ifdef USE_DEBUGGER
void debugger_write_io_dword(UINT32 addr, UINT32 val)
{
	debugger_write_io_byte(addr + 0, (val >>  0) & 0xff);
	debugger_write_io_byte(addr + 1, (val >>  8) & 0xff);
	debugger_write_io_byte(addr + 2, (val >> 16) & 0xff);
	debugger_write_io_byte(addr + 3, (val >> 24) & 0xff);
}
#endif

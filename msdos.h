/*
	MS-DOS Player for Win32 console

	Author : Takeda.Toshiya
	Date   : 2009.11.09-
*/

#ifndef _MSDOS_H_
#define _MSDOS_H_

#include <windows.h>
#include <winioctl.h>
#include <tchar.h>
#include <stdlib.h>
#include <stdio.h>
#include <conio.h>
#include <dos.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <io.h>
#include <sys/locking.h>
#include <mbctype.h>
#include <direct.h>
#include <errno.h>

// variable scope of 'for' loop for microsoft visual c++ 6.0 and embedded visual c++ 4.0
#if (defined(_MSC_VER) && (_MSC_VER == 1200)) || defined(_WIN32_WCE)
#define for if(0);else for
#endif
// disable warnings C4189, C4995 and C4996 for microsoft visual c++ 2005
#if defined(_MSC_VER) && (_MSC_VER >= 1400)
#pragma warning( disable : 4819 )
#pragma warning( disable : 4995 )
#pragma warning( disable : 4996 )
#endif
// compat for mingw32 headers
#ifndef COMMON_LVB_UNDERSCORE
#define COMMON_LVB_UNDERSCORE 0x8000
#endif

// type definition
#ifndef uint8
typedef unsigned char uint8;
#endif
#ifndef uint16
typedef unsigned short uint16;
#endif
#ifndef uint32
typedef unsigned int uint32;
#endif
#ifndef int8
typedef signed char int8;
#endif
#ifndef int16
typedef signed short int16;
#endif
#ifndef int32
typedef signed int int32;
#endif

#pragma pack(1)
typedef union {
	uint32 dw;
	struct {
		uint16 l, h;
	} w;
} pair32;
#pragma pack()

/* ----------------------------------------------------------------------------
	MS-DOS virtual machine
---------------------------------------------------------------------------- */

#define VECTOR_TOP	0
#define VECTOR_SIZE	0x400
#define WORK_TOP	(VECTOR_TOP + VECTOR_SIZE)
#define WORK_SIZE	0x300
#define DPB_TOP		(WORK_TOP + WORK_SIZE)
#define DPB_SIZE	0x400
#define IRET_TOP	(DPB_TOP + DPB_SIZE)
#define IRET_SIZE	0x100
#define DBCS_TOP	(IRET_TOP + IRET_SIZE)
#define DBCS_TABLE	(DBCS_TOP + 2)
#define DBCS_SIZE	16
#define MEMORY_TOP	(DBCS_TOP + DBCS_SIZE)
#define MEMORY_END	0xffff0

//#define ENV_SIZE	0x800
#define ENV_SIZE	0x2000
#define PSP_SIZE	0x100

#define MAX_FILES	20
#define MAX_PROCESS	16

#define DUP_STDIN	29
#define DUP_STDOUT	30
#define DUP_STDERR	31

//#define SUPPORT_AUX_PRN

#pragma pack(1)
typedef struct {
	uint8 mz;
	uint16 psp;
	uint16 paragraphs;
} mcb_t;
#pragma pack()

#pragma pack(1)
typedef struct {
	uint16 env_seg;
	pair32 cmd_line;
	pair32 fcb1;
	pair32 fcb2;
	uint16 sp;
	uint16 ss;
	uint16 ip;
	uint16 cs;
} param_block_t;
#pragma pack()

#pragma pack(1)
typedef struct {
	uint8 len;
	char cmd[127];
} cmd_line_t;
#pragma pack()

#pragma pack(1)
typedef struct {
	uint8 exit[2];
	uint16 first_mcb;
	uint8 reserved_1;
	uint8 far_call;
	pair32 cpm_entry;
	pair32 int_22h;
	pair32 int_23h;
	pair32 int_24h;
	uint16 parent_psp;
	uint8 file_table[20];
	uint16 env_seg;
	pair32 stack;
	uint8 reserved_2[30];
	uint8 service[3];
	uint8 reserved_3[2];
	uint8 ex_fcb[7];
	uint8 fcb1[16];
	uint8 fcb2[20];
	uint8 buffer[128];
} psp_t;
#pragma pack()

#pragma pack(1)
typedef struct {
	uint16 mz;
	uint16 extra_bytes;
	uint16 pages;
	uint16 relocations;
	uint16 header_size;
	uint16 min_alloc;
	uint16 max_alloc;
	uint16 init_ss;
	uint16 init_sp;
	uint16 check_sum;
	uint16 init_ip;
	uint16 init_cs;
	uint16 relocation_table;
	uint16 overlay;
} exe_header_t;
#pragma pack()

#pragma pack(1)
typedef struct {
	uint8 reserved[21];
	uint8 attrib;
	uint16 time;
	uint16 date;
	uint32 size;
	char name[13];
} find_t;
#pragma pack()

#pragma pack(1)
typedef struct {
	uint32 attrib;
	pair32 ctime_lo;
	pair32 ctime_hi;
	pair32 atime_lo;
	pair32 atime_hi;
	pair32 mtime_lo;
	pair32 mtime_hi;
	uint32 size_hi;
	uint32 size_lo;
	uint8 reserved[8];
	char full_name[260];
	char short_name[14];
} find_lfn_t;
#pragma pack()

#pragma pack(1)
typedef struct {
	uint8 drive_num;
	uint8 unit_num;
	uint16 bytes_per_sector;
	uint8 highest_sector_num;
	uint8 shift_count;
	uint16 reserved_sectors;
	uint8 fat_num;
	uint16 root_entries;
	uint16 first_data_sector;
	uint16 highest_cluster_num;
	uint16 sectors_per_fat;
	uint16 first_dir_sector;
	uint32 device_driver_header;
	uint8 media_type;
	uint8 drive_accessed;
	uint16 next_dpb_ofs;
	uint16 next_dpb_seg;
	uint16 first_free_cluster;
	uint16 free_clusters;
} dpb_t;
#pragma pack()

typedef struct {
	char path[MAX_PATH];
	int valid;
	int id;
	int atty;
	int mode;
	uint16 info;
	uint16 psp;
} file_handler_t;

static const struct {
	int mode;
	int in;
	int out;
} file_mode[] = {
	{ _O_RDONLY | _O_BINARY, 1, 0 },
	{ _O_WRONLY | _O_BINARY, 0, 1 },
	{ _O_RDWR   | _O_BINARY, 1, 1 },
};

typedef struct {
	uint16 psp;
	char module_dir[MAX_PATH];
	pair32 dta;
	uint8 switchar;
	uint8 verify;
	HANDLE find_handle;
	uint8 allowable_mask;
	uint8 required_mask;
	char volume_label[MAX_PATH];
} process_t;

uint16 current_psp;

int retval = 0;

file_handler_t file_handler[MAX_FILES];
uint8 file_buffer[0x100000];

process_t process[MAX_PROCESS];

void msdos_syscall(unsigned num);
int msdos_init(int argc, char *argv[], char *envp[]);
int msdos_finish();

// console
#define SCR_BUF_SIZE	1200
#define KEY_BUF_SIZE	16
#define KEY_BUF_MASK	15

#define SET_RECT(rect, l, t, r, b) { \
	rect.Left = l; \
	rect.Top = t; \
	rect.Right = r; \
	rect.Bottom = b; \
}

HANDLE hStdout;
CHAR_INFO scr_buf[SCR_BUF_SIZE][80];
COORD scr_buf_size;
COORD scr_buf_pos;
int key_buf_cnt;
int key_buf_set;
int key_buf_get;
int key_buf[16];
//int std[3];

int code_page;

/* ----------------------------------------------------------------------------
	PC/AT hardware emulation
---------------------------------------------------------------------------- */

#define SUPPORT_HARDWARE

void hardware_init();
void hardware_run();
void hardware_update();

int ops;

// cmos

uint8 cmos[128];
uint8 cmos_addr;

// pic

typedef struct {
	uint8 imr, isr, irr, prio;
	uint8 icw1, icw2, icw3, icw4;
	uint8 ocw3;
	uint8 icw2_r, icw3_r, icw4_r;
} pic_t;

pic_t pic[2];
int pic_req_chip, pic_req_level;
uint8 pic_req_bit;

void pic_init();
void pic_write(int c, uint32 addr, uint8 data);
uint8 pic_read(int c, uint32 addr);
void pic_req(int c, int level, int signal);
int pic_ack();
void pic_update();

// pit

typedef struct {
	int prev_out;
	//int gate;
	int32 count;
	uint16 latch;
	uint16 count_reg;
	uint8 ctrl_reg;
	int count_latched;
	int low_read, high_read;
	int low_write, high_write;
	int mode;
	int delay;
	int start;
	int null_count;
	int status_latched;
	uint8 status;
	// constant clock
	uint32 input_clk;
	uint32 expired_time;
	uint32 prev_time;
} pit_t;

pit_t pit[3];
int pit_active;

void pit_init();
void pit_write(int ch, uint8 val);
uint8 pit_read(int ch);
void pit_run();
void pit_input_clock(int ch, int clock);
void pit_start_count(int ch);
void pit_stop_count(int ch);
void pit_latch_count(int ch);
void pit_set_signal(int ch, int signal);
int pit_get_next_count(int ch);
int pit_get_expired_time(int clock);

// i/o bus

uint8 IN8(uint32 addr);
void OUT8(uint32 addr, uint8 val);
uint16 IN16(uint32 addr);
void OUT16(uint32 addr, uint16 val);

/* ----------------------------------------------------------------------------
	80286 emulation (based on MAME i86 core)
---------------------------------------------------------------------------- */

#define HAS_I286
//#define HAS_V30

#ifdef HAS_I286
#define MAX_MEM	0x1000000
uint32 AMASK;
#else
#define MAX_MEM	0x100000
#define AMASK	0xfffff
#endif
uint8 mem[MAX_MEM];

#define AX	0
#define CX	1
#define DX	2
#define BX	3
#define SP	4
#define BP	5
#define SI	6
#define DI	7

#define AL	0
#define AH	1
#define CL	2
#define CH	3
#define DL	4
#define DH	5
#define BL	6
#define BH	7
#define SPL	8
#define SPH	9
#define BPL	10
#define BPH	11
#define SIL	12
#define SIH	13
#define DIL	14
#define DIH	15

#define ES	0
#define CS	1
#define SS	2
#define DS	3

#define SegBase(seg) (sregs[seg] << 4)
#define DefaultBase(seg) ((seg_prefix && (seg == DS || seg == SS)) ? prefix_base : base[seg])

#define CompressFlags() (uint16)(CF | (PF << 2) | (AF << 4) | (ZF << 6) | (SF << 7) | (TF << 8) | (IF << 9) | (DF << 10) | (OF << 11) | (MD << 15))
#define ExpandFlags(f) { \
	CarryVal = (f) & 1; \
	ParityVal = !((f) & 4); \
	AuxVal = (f) & 0x10; \
	ZeroVal = !((f) & 0x40); \
	SignVal = ((f) & 0x80) ? -1 : 0; \
	TF = ((f) & 0x100) >> 8; \
	IF = ((f) & 0x200) >> 9; \
	MF = ((f) & 0x8000) >> 15; \
	DirVal = ((f) & 0x400) ? -1 : 1; \
	OverVal = (f) & 0x800; \
}

static const uint8 parity_table[256] = {
	1, 0, 0, 1, 0, 1, 1, 0, 0, 1, 1, 0, 1, 0, 0, 1,
	0, 1, 1, 0, 1, 0, 0, 1, 1, 0, 0, 1, 0, 1, 1, 0,
	0, 1, 1, 0, 1, 0, 0, 1, 1, 0, 0, 1, 0, 1, 1, 0,
	1, 0, 0, 1, 0, 1, 1, 0, 0, 1, 1, 0, 1, 0, 0, 1,
	0, 1, 1, 0, 1, 0, 0, 1, 1, 0, 0, 1, 0, 1, 1, 0,
	1, 0, 0, 1, 0, 1, 1, 0, 0, 1, 1, 0, 1, 0, 0, 1,
	1, 0, 0, 1, 0, 1, 1, 0, 0, 1, 1, 0, 1, 0, 0, 1,
	0, 1, 1, 0, 1, 0, 0, 1, 1, 0, 0, 1, 0, 1, 1, 0,
	0, 1, 1, 0, 1, 0, 0, 1, 1, 0, 0, 1, 0, 1, 1, 0,
	1, 0, 0, 1, 0, 1, 1, 0, 0, 1, 1, 0, 1, 0, 0, 1,
	1, 0, 0, 1, 0, 1, 1, 0, 0, 1, 1, 0, 1, 0, 0, 1,
	0, 1, 1, 0, 1, 0, 0, 1, 1, 0, 0, 1, 0, 1, 1, 0,
	1, 0, 0, 1, 0, 1, 1, 0, 0, 1, 1, 0, 1, 0, 0, 1,
	0, 1, 1, 0, 1, 0, 0, 1, 1, 0, 0, 1, 0, 1, 1, 0,
	0, 1, 1, 0, 1, 0, 0, 1, 1, 0, 0, 1, 0, 1, 1, 0,
	1, 0, 0, 1, 0, 1, 1, 0, 0, 1, 1, 0, 1, 0, 0, 1
};
static const uint8 mod_reg8[256] = {
	AL, AL, AL, AL, AL, AL, AL, AL, CL, CL, CL, CL, CL, CL, CL, CL,
	DL, DL, DL, DL, DL, DL, DL, DL, BL, BL, BL, BL, BL, BL, BL, BL,
	AH, AH, AH, AH, AH, AH, AH, AH, CH, CH, CH, CH, CH, CH, CH, CH,
	DH, DH, DH, DH, DH, DH, DH, DH, BH, BH, BH, BH, BH, BH, BH, BH,
	AL, AL, AL, AL, AL, AL, AL, AL, CL, CL, CL, CL, CL, CL, CL, CL,
	DL, DL, DL, DL, DL, DL, DL, DL, BL, BL, BL, BL, BL, BL, BL, BL,
	AH, AH, AH, AH, AH, AH, AH, AH, CH, CH, CH, CH, CH, CH, CH, CH,
	DH, DH, DH, DH, DH, DH, DH, DH, BH, BH, BH, BH, BH, BH, BH, BH,
	AL, AL, AL, AL, AL, AL, AL, AL, CL, CL, CL, CL, CL, CL, CL, CL,
	DL, DL, DL, DL, DL, DL, DL, DL, BL, BL, BL, BL, BL, BL, BL, BL,
	AH, AH, AH, AH, AH, AH, AH, AH, CH, CH, CH, CH, CH, CH, CH, CH,
	DH, DH, DH, DH, DH, DH, DH, DH, BH, BH, BH, BH, BH, BH, BH, BH,
	AL, AL, AL, AL, AL, AL, AL, AL, CL, CL, CL, CL, CL, CL, CL, CL,
	DL, DL, DL, DL, DL, DL, DL, DL, BL, BL, BL, BL, BL, BL, BL, BL,
	AH, AH, AH, AH, AH, AH, AH, AH, CH, CH, CH, CH, CH, CH, CH, CH,
	DH, DH, DH, DH, DH, DH, DH, DH, BH, BH, BH, BH, BH, BH, BH, BH,
};
static const uint8 mod_reg16[256] = {
	AX, AX, AX, AX, AX, AX, AX, AX, CX, CX, CX, CX, CX, CX, CX, CX,
	DX, DX, DX, DX, DX, DX, DX, DX, BX, BX, BX, BX, BX, BX, BX, BX,
	SP, SP, SP, SP, SP, SP, SP, SP, BP, BP, BP, BP, BP, BP, BP, BP,
	SI, SI, SI, SI, SI, SI, SI, SI, DI, DI, DI, DI, DI, DI, DI, DI,
	AX, AX, AX, AX, AX, AX, AX, AX, CX, CX, CX, CX, CX, CX, CX, CX,
	DX, DX, DX, DX, DX, DX, DX, DX, BX, BX, BX, BX, BX, BX, BX, BX,
	SP, SP, SP, SP, SP, SP, SP, SP, BP, BP, BP, BP, BP, BP, BP, BP,
	SI, SI, SI, SI, SI, SI, SI, SI, DI, DI, DI, DI, DI, DI, DI, DI,
	AX, AX, AX, AX, AX, AX, AX, AX, CX, CX, CX, CX, CX, CX, CX, CX,
	DX, DX, DX, DX, DX, DX, DX, DX, BX, BX, BX, BX, BX, BX, BX, BX,
	SP, SP, SP, SP, SP, SP, SP, SP, BP, BP, BP, BP, BP, BP, BP, BP,
	SI, SI, SI, SI, SI, SI, SI, SI, DI, DI, DI, DI, DI, DI, DI, DI,
	AX, AX, AX, AX, AX, AX, AX, AX, CX, CX, CX, CX, CX, CX, CX, CX,
	DX, DX, DX, DX, DX, DX, DX, DX, BX, BX, BX, BX, BX, BX, BX, BX,
	SP, SP, SP, SP, SP, SP, SP, SP, BP, BP, BP, BP, BP, BP, BP, BP,
	SI, SI, SI, SI, SI, SI, SI, SI, DI, DI, DI, DI, DI, DI, DI, DI
};
static const uint8 mod_rm8[256] = {
	0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
	0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
	0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
	0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
	0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
	0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
	0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
	0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
	0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
	0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
	0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
	0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
	AL, CL, DL, BL, AH, CH, DH, BH, AL, CL, DL, BL, AH, CH, DH, BH,
	AL, CL, DL, BL, AH, CH, DH, BH, AL, CL, DL, BL, AH, CH, DH, BH,
	AL, CL, DL, BL, AH, CH, DH, BH, AL, CL, DL, BL, AH, CH, DH, BH,
	AL, CL, DL, BL, AH, CH, DH, BH, AL, CL, DL, BL, AH, CH, DH, BH
};
static const uint8 mod_rm16[256] = {
	0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
	0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
	0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
	0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
	0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
	0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
	0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
	0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
	0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
	0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
	0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
	0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
	AX, CX, DX, BX, SP, BP, SI, DI, AX, CX, DX, BX, SP, BP, SI, DI,
	AX, CX, DX, BX, SP, BP, SI, DI, AX, CX, DX, BX, SP, BP, SI, DI,
	AX, CX, DX, BX, SP, BP, SI, DI, AX, CX, DX, BX, SP, BP, SI, DI,
	AX, CX, DX, BX, SP, BP, SI, DI, AX, CX, DX, BX, SP, BP, SI, DI
};

#ifdef HAS_V30
static const uint16 bytes[] = {
	   1,    2,    4,    8,
	  16,   32,   64,  128,
	 256,  512, 1024, 2048,
	4096, 8192,16384,32768
};
#endif

union REGTYPE {
	uint8 b[16];
	uint16 w[8];
} regs;

uint16 sregs[4], limit[4];
uint32 base[4];
unsigned EA;
uint16 EO;
uint32 gdtr_base, idtr_base, ldtr_base, tr_base;
uint16 gdtr_limit, idtr_limit, ldtr_limit, tr_limit;
uint16 ldtr_sel, tr_sel;
uint16 flags, msw;
int32 AuxVal, OverVal, SignVal, ZeroVal, CarryVal, DirVal;
uint8 ParityVal, TF, IF, MF;
int intstat, halt;
uint32 PC;
unsigned prefix_base;
int seg_prefix;

inline uint8 RM8(uint32 addr);
inline uint16 RM16(uint32 addr);
inline void WM8(uint32 addr, uint8 val);
inline void WM16(uint32 addr, uint16 val);
inline uint8 RM8(uint32 seg, uint32 ofs);
inline uint16 RM16(uint32 seg, uint32 ofs);
inline void WM8(uint32 seg, uint32 ofs, uint8 val);
inline void WM16(uint32 seg, uint32 ofs, uint16 val);
inline uint8 FETCHOP();
inline uint8 FETCH8();
inline uint16 FETCH16();
inline void PUSH16(uint16 val);
inline uint16 POP16();

void interrupt(unsigned num);
unsigned GetEA(unsigned ModRM);
void rotate_shift_byte(unsigned ModRM, unsigned cnt);
void rotate_shift_word(unsigned ModRM, unsigned cnt);
#ifdef HAS_I286
int i286_selector_okay(uint16 selector);
void i286_data_descriptor(int reg, uint16 selector);
void i286_code_descriptor(uint16 selector, uint16 offset);
#endif

void op(uint8 code);
inline void _add_br8();
inline void _add_wr16();
inline void _add_r8b();
inline void _add_r16w();
inline void _add_ald8();
inline void _add_axd16();
inline void _push_es();
inline void _pop_es();
inline void _or_br8();
inline void _or_wr16();
inline void _or_r8b();
inline void _or_r16w();
inline void _or_ald8();
inline void _or_axd16();
inline void _push_cs();
inline void _op0f();
inline void _adc_br8();
inline void _adc_wr16();
inline void _adc_r8b();
inline void _adc_r16w();
inline void _adc_ald8();
inline void _adc_axd16();
inline void _push_ss();
inline void _pop_ss();
inline void _sbb_br8();
inline void _sbb_wr16();
inline void _sbb_r8b();
inline void _sbb_r16w();
inline void _sbb_ald8();
inline void _sbb_axd16();
inline void _push_ds();
inline void _pop_ds();
inline void _and_br8();
inline void _and_wr16();
inline void _and_r8b();
inline void _and_r16w();
inline void _and_ald8();
inline void _and_axd16();
inline void _es();
inline void _daa();
inline void _sub_br8();
inline void _sub_wr16();
inline void _sub_r8b();
inline void _sub_r16w();
inline void _sub_ald8();
inline void _sub_axd16();
inline void _cs();
inline void _das();
inline void _xor_br8();
inline void _xor_wr16();
inline void _xor_r8b();
inline void _xor_r16w();
inline void _xor_ald8();
inline void _xor_axd16();
inline void _ss();
inline void _aaa();
inline void _cmp_br8();
inline void _cmp_wr16();
inline void _cmp_r8b();
inline void _cmp_r16w();
inline void _cmp_ald8();
inline void _cmp_axd16();
inline void _ds();
inline void _aas();
inline void _inc_ax();
inline void _inc_cx();
inline void _inc_dx();
inline void _inc_bx();
inline void _inc_sp();
inline void _inc_bp();
inline void _inc_si();
inline void _inc_di();
inline void _dec_ax();
inline void _dec_cx();
inline void _dec_dx();
inline void _dec_bx();
inline void _dec_sp();
inline void _dec_bp();
inline void _dec_si();
inline void _dec_di();
inline void _push_ax();
inline void _push_cx();
inline void _push_dx();
inline void _push_bx();
inline void _push_sp();
inline void _push_bp();
inline void _push_si();
inline void _push_di();
inline void _pop_ax();
inline void _pop_cx();
inline void _pop_dx();
inline void _pop_bx();
inline void _pop_sp();
inline void _pop_bp();
inline void _pop_si();
inline void _pop_di();
inline void _pusha();
inline void _popa();
inline void _bound();
inline void _arpl();
inline void _repc(int flagval);
inline void _push_d16();
inline void _imul_d16();
inline void _push_d8();
inline void _imul_d8();
inline void _insb();
inline void _insw();
inline void _outsb();
inline void _outsw();
inline void _jo();
inline void _jno();
inline void _jb();
inline void _jnb();
inline void _jz();
inline void _jnz();
inline void _jbe();
inline void _jnbe();
inline void _js();
inline void _jns();
inline void _jp();
inline void _jnp();
inline void _jl();
inline void _jnl();
inline void _jle();
inline void _jnle();
inline void _op80();
inline void _op81();
inline void _op82();
inline void _op83();
inline void _test_br8();
inline void _test_wr16();
inline void _xchg_br8();
inline void _xchg_wr16();
inline void _mov_br8();
inline void _mov_wr16();
inline void _mov_r8b();
inline void _mov_r16w();
inline void _mov_wsreg();
inline void _lea();
inline void _mov_sregw();
inline void _popw();
inline void _nop();
inline void _xchg_axcx();
inline void _xchg_axdx();
inline void _xchg_axbx();
inline void _xchg_axsp();
inline void _xchg_axbp();
inline void _xchg_axsi();
inline void _xchg_axdi();
inline void _cbw();
inline void _cwd();
inline void _call_far();
inline void _wait();
inline void _pushf();
inline void _popf();
inline void _sahf();
inline void _lahf();
inline void _mov_aldisp();
inline void _mov_axdisp();
inline void _mov_dispal();
inline void _mov_dispax();
inline void _movsb();
inline void _movsw();
inline void _cmpsb();
inline void _cmpsw();
inline void _test_ald8();
inline void _test_axd16();
inline void _stosb();
inline void _stosw();
inline void _lodsb();
inline void _lodsw();
inline void _scasb();
inline void _scasw();
inline void _mov_ald8();
inline void _mov_cld8();
inline void _mov_dld8();
inline void _mov_bld8();
inline void _mov_ahd8();
inline void _mov_chd8();
inline void _mov_dhd8();
inline void _mov_bhd8();
inline void _mov_axd16();
inline void _mov_cxd16();
inline void _mov_dxd16();
inline void _mov_bxd16();
inline void _mov_spd16();
inline void _mov_bpd16();
inline void _mov_sid16();
inline void _mov_did16();
inline void _rotshft_bd8();
inline void _rotshft_wd8();
inline void _ret_d16();
inline void _ret();
inline void _les_dw();
inline void _lds_dw();
inline void _mov_bd8();
inline void _mov_wd16();
inline void _enter();
inline void _leav();	// _leave()
inline void _retf_d16();
inline void _retf();
inline void _int3();
inline void _int();
inline void _into();
inline void _iret();
inline void _rotshft_b();
inline void _rotshft_w();
inline void _rotshft_bcl();
inline void _rotshft_wcl();
inline void _aam();
inline void _aad();
inline void _setalc();
inline void _xlat();
inline void _escape();
inline void _loopne();
inline void _loope();
inline void _loop();
inline void _jcxz();
inline void _inal();
inline void _inax();
inline void _outal();
inline void _outax();
inline void _call_d16();
inline void _jmp_d16();
inline void _jmp_far();
inline void _jmp_d8();
inline void _inaldx();
inline void _inaxdx();
inline void _outdxal();
inline void _outdxax();
inline void _lock();
inline void _rep(int flagval);
inline void _hlt();
inline void _cmc();
inline void _opf6();
inline void _opf7();
inline void _clc();
inline void _stc();
inline void _cli();
inline void _sti();
inline void _cld();
inline void _std();
inline void _opfe();
inline void _opff();
inline void _invalid();

void cpu_init();
void cpu_interrupt(int status);

#endif

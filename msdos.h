/*
	MS-DOS Player for Win32 console

	Author : Takeda.Toshiya
	Date   : 2009.11.09-
*/

#ifndef _MSDOS_H_
#define _MSDOS_H_

#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x501
#endif
#include <windows.h>
#include <winioctl.h>
#include <tchar.h>
#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <conio.h>
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

// variable scope of 'for' loop for Microsoft Visual C++ 6.0
#if defined(_MSC_VER) && (_MSC_VER == 1200)
#define for if(0);else for
#endif

// disable warnings for Microsoft Visual C++ 2005 or later
#if defined(_MSC_VER) && (_MSC_VER >= 1400)
#pragma warning( disable : 4819 )
#pragma warning( disable : 4995 )
#pragma warning( disable : 4996 )
// for MAME i86/i386
#pragma warning( disable : 4018 )
#pragma warning( disable : 4065 )
#pragma warning( disable : 4146 )
#pragma warning( disable : 4244 )
#pragma warning( disable : 4267 )
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
#ifndef INT8
typedef signed char INT8;
#endif
#ifndef INT16
typedef signed short INT16;
#endif
#ifndef INT32
typedef signed int INT32;
#endif

#pragma pack(1)
typedef union {
	UINT32 dw;
	struct {
#ifdef __BIG_ENDIAN__
		UINT16 h, l;
#else
		UINT16 l, h;
#endif
	} w;
} PAIR32;
#pragma pack()

/* ----------------------------------------------------------------------------
	FIFO buffer
---------------------------------------------------------------------------- */

#define MAX_FIFO	256

class FIFO
{
private:
	int buf[MAX_FIFO];
	int cnt, rpt, wpt, stored[3];
public:
	FIFO() {
		cnt = rpt = wpt = 0;
	}
	void write(int val) {
		if(cnt < MAX_FIFO) {
			buf[wpt++] = val;
			if(wpt >= MAX_FIFO) {
				wpt = 0;
			}
			cnt++;
		}
	}
	int read() {
		int val = 0;
		if(cnt) {
			val = buf[rpt++];
			if(rpt >= MAX_FIFO) {
				rpt = 0;
			}
			cnt--;
		}
		return val;
	}
	int count() {
		return cnt;
	}
	void store_buffer() {
		stored[0] = cnt;
		stored[1] = rpt;
		stored[2] = wpt;
	}
	void restore_buffer() {
		cnt = stored[0];
		rpt = stored[1];
		wpt = stored[2];
	}
};

/* ----------------------------------------------------------------------------
	MS-DOS virtual machine
---------------------------------------------------------------------------- */

#define VECTOR_TOP	0
#define VECTOR_SIZE	0x400
#define BIOS_TOP	(VECTOR_TOP + VECTOR_SIZE)
#define BIOS_SIZE	0x100
#define WORK_TOP	(BIOS_TOP + BIOS_SIZE)
#define WORK_SIZE	0x300
#define IRET_TOP	(WORK_TOP + WORK_SIZE)
#define IRET_SIZE	0x100
#define DOS_INFO_TOP	(IRET_TOP + IRET_SIZE)
#define DOS_INFO_BASE	(DOS_INFO_TOP + 24)
#define DOS_INFO_SIZE	0x100
#define DPB_TOP		(DOS_INFO_TOP + DOS_INFO_SIZE)
#define DPB_SIZE	0x400
#define FILE_TABLE_TOP	(DPB_TOP + DPB_SIZE)
#define FILE_TABLE_SIZE	0x10
#define CDS_TOP		(FILE_TABLE_TOP + FILE_TABLE_SIZE)
#define CDS_SIZE	0x80
#define FCB_TABLE_TOP	(CDS_TOP + CDS_SIZE)
#define FCB_TABLE_SIZE	0x10
// nls tables
#define UPPERTABLE_TOP	(FCB_TABLE_TOP + FCB_TABLE_SIZE)
#define UPPERTABLE_SIZE	0x82
#define FILENAME_UPPERTABLE_TOP (UPPERTABLE_TOP + UPPERTABLE_SIZE)
#define FILENAME_UPPERTABLE_SIZE 0x82
#define FILENAME_TERMINATOR_TOP (FILENAME_UPPERTABLE_TOP + FILENAME_UPPERTABLE_SIZE)
#define FILENAME_TERMINATOR_SIZE 0x20	/* requirement: 10 + 14(terminate chars) */
#define COLLATING_TABLE_TOP (FILENAME_TERMINATOR_TOP + FILENAME_TERMINATOR_SIZE)
#define COLLATING_TABLE_SIZE 0x102
#define DBCS_TOP	(COLLATING_TABLE_TOP + COLLATING_TABLE_SIZE)
#define DBCS_TABLE	(DBCS_TOP + 2)
#define DBCS_SIZE	0x10
#define MSDOS_SYSTEM_DATA_END (DBCS_TOP + DBCS_SIZE)
#define MEMORY_TOP	((MSDOS_SYSTEM_DATA_END + 15) & ~15U)
#define MEMORY_END	0xb8000
#define TEXT_VRAM_TOP	0xb8000
#define UMB_TOP		0xc0000
#define UMB_END		0xf8000
#define SHADOW_BUF_TOP	0xf8000

//#define ENV_SIZE	0x800
#define ENV_SIZE	0x2000
#define PSP_SIZE	0x100

#define MAX_FILES	128
#define MAX_PROCESS	16
#define MAX_DTAINFO	128
#define LFN_DTA_LADDR	0x10FFF0
#define FIND_MAGIC	0x46696e64

#define DUP_STDIN	29
#define DUP_STDOUT	30
#define DUP_STDERR	31

//#define SUPPORT_AUX_PRN

#pragma pack(1)
typedef struct {
	UINT8 mz;
	UINT16 psp;
	UINT16 paragraphs;
	UINT8 reserved[3];
	char prog_name[8];
} mcb_t;
#pragma pack()

#pragma pack(1)
typedef struct {
	UINT16 env_seg;
	PAIR32 cmd_line;
	PAIR32 fcb1;
	PAIR32 fcb2;
	UINT16 sp;
	UINT16 ss;
	UINT16 ip;
	UINT16 cs;
} param_block_t;
#pragma pack()

#pragma pack(1)
typedef struct {
	UINT8 len;
	char cmd[127];
} cmd_line_t;
#pragma pack()

#pragma pack(1)
typedef struct {
	UINT8 exit[2];
	UINT16 first_mcb;
	UINT8 reserved_1;
	UINT8 far_call;
	PAIR32 cpm_entry;
	PAIR32 int_22h;
	PAIR32 int_23h;
	PAIR32 int_24h;
	UINT16 parent_psp;
	UINT8 file_table[20];
	UINT16 env_seg;
	PAIR32 stack;
	UINT16 file_table_size;
	PAIR32 file_table_ptr;
	UINT8 reserved_2[24];
	UINT8 service[3];
	UINT8 reserved_3[2];
	UINT8 ex_fcb[7];
	UINT8 fcb1[16];
	UINT8 fcb2[20];
	UINT8 buffer[128];
} psp_t;
#pragma pack()

#pragma pack(1)
typedef struct {
	UINT16 mz;
	UINT16 extra_bytes;
	UINT16 pages;
	UINT16 relocations;
	UINT16 header_size;
	UINT16 min_alloc;
	UINT16 max_alloc;
	UINT16 init_ss;
	UINT16 init_sp;
	UINT16 check_sum;
	UINT16 init_ip;
	UINT16 init_cs;
	UINT16 relocation_table;
	UINT16 overlay;
} exe_header_t;
#pragma pack()

#pragma pack(1)
typedef struct {
	UINT8 flag;
	UINT8 reserved[5];
	UINT8 attribute;
} ext_fcb_t;
#pragma pack()

#pragma pack(1)
typedef struct {
	UINT8 drive;
	UINT8 file_name[8 + 3];
	UINT16 current_block;
	UINT16 record_size;
	UINT32 file_size;
	UINT16 date;
	UINT16 time;
	union {
		UINT8 reserved[8];
		HANDLE handle;
	};
	UINT8 cur_record;
	UINT32 rand_record;
} fcb_t;
#pragma pack()

#pragma pack(1)
typedef struct {
	UINT8 drive;
	UINT8 file_name[8 + 3];
	UINT8 attribute;
	UINT8 nt_res;
	UINT8 create_time_ms;
	UINT16 creation_time;
	UINT16 creation_date;
	UINT16 last_access_date;
	UINT16 cluster_hi;
	UINT16 last_write_time;
	UINT16 last_write_date;
	UINT16 cluster_lo;
	UINT32 file_size;
} find_fcb_t;
#pragma pack()

#pragma pack(1)
typedef struct {
	union {
		UINT8 reserved[21];
		struct {
			UINT32 find_magic;
			UINT32 dta_index;
		};
	};
	UINT8 attrib;
	UINT16 time;
	UINT16 date;
	UINT32 size;
	char name[13];
} find_t;
#pragma pack()

#pragma pack(1)
typedef struct {
	UINT32 attrib;
	PAIR32 ctime_lo;
	PAIR32 ctime_hi;
	PAIR32 atime_lo;
	PAIR32 atime_hi;
	PAIR32 mtime_lo;
	PAIR32 mtime_hi;
	UINT32 size_hi;
	UINT32 size_lo;
	UINT8 reserved[8];
	char full_name[260];
	char short_name[14];
} find_lfn_t;
#pragma pack()

#pragma pack(1)
typedef struct {
	UINT16 info_level;
	UINT32 serial_number;
	char volume_label[11];
	char file_system[8];
} drive_info_t;
#pragma pack()

#pragma pack(1)
typedef struct {
	UINT16 size_of_structure;
	UINT16 structure_version;
	UINT32 sectors_per_cluster;
	UINT32 bytes_per_sector;
	UINT32 available_clusters_on_drive;
	UINT32 total_clusters_on_drive;
	UINT32 available_sectors_on_drive;
	UINT32 total_sectors_on_drive;
	UINT32 available_allocation_units;
	UINT32 total_allocation_units;
	UINT8 reserved[8];
} ext_space_info_t;
#pragma pack()

#pragma pack(1)
typedef struct {
	UINT8 drive_num;
	UINT8 unit_num;
	UINT16 bytes_per_sector;
	UINT8 highest_sector_num;
	UINT8 shift_count;
	UINT16 reserved_sectors;
	UINT8 fat_num;
	UINT16 root_entries;
	UINT16 first_data_sector;
	UINT16 highest_cluster_num;
	UINT16 sectors_per_fat;
	UINT16 first_dir_sector;
	UINT32 device_driver_header;
	UINT8 media_type;
	UINT8 drive_accessed;
	UINT16 next_dpb_ofs;
	UINT16 next_dpb_seg;
	UINT16 first_free_cluster;
	UINT16 free_clusters;
	// extended
	UINT16 fat_mirroring;
	UINT16 info_sector;
	UINT16 backup_boot_sector;
	UINT32 first_cluster_sector;
	UINT32 maximum_cluster_num;
	UINT32 fat_sectors;
	UINT32 root_cluster;
	UINT32 free_search_cluster;
} dpb_t;
#pragma pack()

#pragma pack(1)
typedef struct {
	char path_name[67];
	UINT16 drive_attrib;
	UINT8 physical_drive_number;
} cds_t;
#pragma pack()

#pragma pack(1)
typedef struct {
	UINT8 reserved_1[22];	// -24
	UINT16 first_mcb;	// -2
	PAIR32 first_dpb;	// +0
	PAIR32 first_sft;	// +0
	UINT8 reserved_2[14];
	PAIR32 cds;		// +22
	PAIR32 fcb_table;	// +26
	UINT8 reserved_3[3];
	UINT8 last_drive;	// +33
	UINT8 reserved_4[29];
	UINT16 buffers_x;	// +63
	UINT16 buffers_y;	// +65
	UINT8 boot_drive;	// +67
	UINT8 i386_or_later;	// +68
	UINT16 ext_mem_size;	// +69
	UINT8 reserved_5[25];
	UINT8 dos_flag;		// +96
} dos_info_t;
#pragma pack()

typedef struct {
	char path[MAX_PATH];
	int valid;
	int id;
	int atty;
	int mode;
	UINT16 info;
	UINT16 psp;
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
	UINT16 psp;
	char module_dir[MAX_PATH];
	PAIR32 dta;
	UINT8 switchar;
	UINT8 verify;
	int max_files;
	char volume_label[MAX_PATH];
	bool parent_int_10h_feh_called;
	bool parent_int_10h_ffh_called;
	UINT16 parent_ds;
} process_t;

typedef struct {
	UINT16 psp;
	UINT32 dta;
	UINT8 allowable_mask;
	UINT8 required_mask;
	HANDLE find_handle;
} dtainfo_t;

UINT8 major_version = 7;
UINT8 minor_version = 10;

UINT16 first_mcb;
UINT16 current_psp;

int retval = 0;
UINT16 error_code = 0;

file_handler_t file_handler[MAX_FILES];
UINT8 file_buffer[0x100000];

process_t process[MAX_PROCESS];
dtainfo_t dtalist[MAX_DTAINFO];

UINT16 malloc_strategy = 0;
UINT8 umb_linked = 0;

char comspec_path[MAX_PATH] = "C:\\COMMAND.COM";

void msdos_syscall(unsigned num);
int msdos_init(int argc, char *argv[], char *envp[], int standard_env);
void msdos_finish();

// console

#define SCR_BUF_WIDTH	256
#define SCR_BUF_HEIGHT	128

#define SET_RECT(rect, l, t, r, b) { \
	rect.Left = l; \
	rect.Top = t; \
	rect.Right = r; \
	rect.Bottom = b; \
}

HANDLE hStdin;
HANDLE hStdout;
CHAR_INFO scr_buf[SCR_BUF_WIDTH * SCR_BUF_HEIGHT];
char scr_char[SCR_BUF_WIDTH * SCR_BUF_HEIGHT];
WORD scr_attr[SCR_BUF_WIDTH * SCR_BUF_HEIGHT];
COORD scr_buf_size;
COORD scr_buf_pos;
int scr_width, scr_height;
int scr_top;
bool restore_console_on_exit = false;
bool cursor_moved;

FIFO *key_buf_char;
FIFO *key_buf_scan;
int key_input = 0;
UINT32 key_code = 0;

int active_code_page;
int system_code_page;

UINT32 text_vram_top_address;
UINT32 text_vram_end_address;
UINT32 shadow_buffer_top_address;
UINT32 shadow_buffer_end_address;
int vram_pages;
bool int_10h_feh_called = false;
bool int_10h_ffh_called = false;

/* ----------------------------------------------------------------------------
	MAME i86/i386
---------------------------------------------------------------------------- */

#if defined(HAS_I86)
	#define CPU_MODEL i8086
#elif defined(HAS_I186)
	#define CPU_MODEL i80186
#elif defined(HAS_V30)
	#define CPU_MODEL v30
#elif defined(HAS_I286)
	#define CPU_MODEL i80286
#elif defined(HAS_I386)
	#define CPU_MODEL i386
#else
//	#if defined(HAS_I386SX)
//		#define CPU_MODEL i386SX
//	#else
		#if defined(HAS_I486)
			#define CPU_MODEL i486
		#else
			#if defined(HAS_PENTIUM)
				#define CPU_MODEL pentium
			#elif defined(HAS_MEDIAGX)
				#define CPU_MODEL mediagx
			#elif defined(HAS_PENTIUM_PRO)
				#define CPU_MODEL pentium_pro
			#elif defined(HAS_PENTIUM_MMX)
				#define CPU_MODEL pentium_mmx
			#elif defined(HAS_PENTIUM2)
				#define CPU_MODEL pentium2
			#elif defined(HAS_PENTIUM3)
				#define CPU_MODEL pentium3
			#elif defined(HAS_PENTIUM4)
				#define CPU_MODEL pentium4
			#endif
			#define SUPPORT_RDTSC
		#endif
		#define SUPPORT_FPU
//	#endif
	#define HAS_I386
#endif

/* ----------------------------------------------------------------------------
	PC/AT hardware emulation
---------------------------------------------------------------------------- */

void hardware_init();
void hardware_finish();
void hardware_run();
void hardware_update();

// memory

#if defined(HAS_I386)
#define MAX_MEM 0x2000000	/* 32MB */
#elif defined(HAS_I286)
#define MAX_MEM 0x1000000	/* 16MB */
#else
#define MAX_MEM 0x100000	/* 1MB */
#endif
UINT8 mem[MAX_MEM + 3];

// pic

typedef struct {
	UINT8 imr, isr, irr, prio;
	UINT8 icw1, icw2, icw3, icw4;
	UINT8 ocw3;
	UINT8 icw2_r, icw3_r, icw4_r;
} pic_t;

pic_t pic[2];
int pic_req_chip, pic_req_level;
UINT8 pic_req_bit;

void pic_init();
void pic_write(int c, UINT32 addr, UINT8 data);
UINT8 pic_read(int c, UINT32 addr);
void pic_req(int c, int level, int signal);
int pic_ack();
void pic_update();

// pit

#define PIT_ALWAYS_RUNNING

typedef struct {
	INT32 count;
	UINT16 latch;
	UINT16 count_reg;
	UINT8 ctrl_reg;
	int count_latched;
	int low_read, high_read;
	int low_write, high_write;
	int mode;
	int status_latched;
	UINT8 status;
	// constant clock
	UINT32 expired_time;
	UINT32 prev_time;
} pit_t;

pit_t pit[3];
#ifndef PIT_ALWAYS_RUNNING
int pit_active;
#endif

void pit_init();
void pit_write(int ch, UINT8 val);
UINT8 pit_read(int ch);
int pit_run(int ch, UINT32 cur_time);
void pit_latch_count(int ch);
int pit_get_expired_time(int ch);

UINT8 system_port = 0;

// cmos

UINT8 cmos[128];
UINT8 cmos_addr;

void cmos_init();
void cmos_write(int addr, UINT8 val);
UINT8 cmos_read(int addr);

// kbd (a20)

UINT8 kbd_data;
UINT8 kbd_status;
UINT8 kbd_command;

void kbd_init();
UINT8 kbd_read_data();
void kbd_write_data(UINT8 val);
UINT8 kbd_read_status();
void kbd_write_command(UINT8 val);

#endif

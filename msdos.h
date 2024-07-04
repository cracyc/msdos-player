/*
	MS-DOS Player for Win32 console

	Author : Takeda.Toshiya
	Date   : 2009.11.09-
*/

#ifndef _MSDOS_H_
#define _MSDOS_H_

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

#pragma pack(1)
typedef union {
	UINT16 w;
	struct {
#ifdef __BIG_ENDIAN__
		UINT8 h, l;
#else
		UINT8 l, h;
#endif
	} b;
} PAIR16;
#pragma pack()

/* ----------------------------------------------------------------------------
	FIFO buffer
---------------------------------------------------------------------------- */

class FIFO
{
private:
	int size;
	int *buf;
	int cnt, rpt, wpt;
public:
	FIFO(int s) {
		size = s;
		buf = (int *)malloc(size * sizeof(int));
		cnt = rpt = wpt = 0;
	}
	void release()
	{
		if(buf != NULL) {
			free(buf);
			buf = NULL;
		}
	}
	void clear()
	{
		cnt = rpt = wpt = 0;
	}
	void write(int val) {
		if(cnt < size) {
			buf[wpt++] = val;
			if(wpt >= size) {
				wpt = 0;
			}
			cnt++;
		}
	}
	int read() {
		int val = 0;
		if(cnt) {
			val = buf[rpt++];
			if(rpt >= size) {
				rpt = 0;
			}
			cnt--;
		}
		return(val);
	}
	int read_not_remove(int pt) {
		if(pt >= 0 && pt < cnt) {
			pt += rpt;
			if(pt >= size) {
				pt -= size;
			}
			return buf[pt];
		}
		return(0);
	}
	int count() {
		return(cnt);
	}
	int remain() {
		return(size - cnt);
	}
	bool full()
	{
		return(cnt == size);
	}
	bool empty()
	{
		return(cnt == 0);
	}
};

/* ----------------------------------------------------------------------------
	MAME i86/i386
---------------------------------------------------------------------------- */

#if defined(HAS_IA32)
//	#define CPU_MODEL ia32
	#define SUPPORT_FPU
#elif defined(HAS_I86)
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
//			#define SUPPORT_RDTSC
		#endif
		#define SUPPORT_FPU
//	#endif
#endif

#if !(defined(HAS_I86) || defined(HAS_I186) || defined(HAS_V30) || defined(HAS_I286) || defined(HAS_I386))
	#define HAS_I386
#endif

/* ----------------------------------------------------------------------------
	debugger
---------------------------------------------------------------------------- */

#ifdef USE_DEBUGGER
bool now_debugging = false;
bool now_going = false;
bool now_suspended = false;
bool force_suspend = false;

break_point_t break_point = {0};
break_point_t rd_break_point = {0};
break_point_t wr_break_point = {0};
break_point_t in_break_point = {0};
break_point_t out_break_point = {0};

int_break_point_t int_break_point = {0};

FILE *fp_debugger = NULL;
FILE *fi_debugger = NULL;

// these read/write interfaces do not check break points,
// debugger should use them not to hit any break point mistakely
UINT8 debugger_read_byte(UINT32 byteaddress);
UINT16 debugger_read_word(UINT32 byteaddress);
UINT32 debugger_read_dword(UINT32 byteaddress);
void debugger_write_byte(UINT32 byteaddress, UINT8 data);
void debugger_write_word(UINT32 byteaddress, UINT16 data);
void debugger_write_dword(UINT32 byteaddress, UINT32 data);
UINT8 debugger_read_io_byte(UINT32 addr);
UINT16 debugger_read_io_word(UINT32 addr);
UINT32 debugger_read_io_dword(UINT32 addr);
void debugger_write_io_byte(UINT32 addr, UINT8 val);
void debugger_write_io_word(UINT32 addr, UINT16 val);
void debugger_write_io_dword(UINT32 addr, UINT32 val);
#endif

/* ----------------------------------------------------------------------------
	service thread
---------------------------------------------------------------------------- */

UINT16 CPU_AX_in_service;
UINT16 CPU_CX_in_service;
UINT16 CPU_DX_in_service;
UINT32 CPU_DS_BASE_in_service;

void prepare_service_loop();
void cleanup_service_loop();

BOOL use_service_thread = FALSE;
CRITICAL_SECTION input_crit_sect;
CRITICAL_SECTION key_buf_crit_sect;
CRITICAL_SECTION putch_crit_sect;
bool in_service = false;
bool service_exit = false;
DWORD main_thread_id;

void start_service_loop(LPTHREAD_START_ROUTINE lpStartAddress);
void finish_service_loop();

bool in_service_29h = false;

/* ----------------------------------------------------------------------------
	PC/AT hardware emulation
---------------------------------------------------------------------------- */

//#define SUPPORT_GRAPHIC_SCREEN

void hardware_init();
void hardware_finish();
void hardware_release();
void hardware_run();
void hardware_run_cpu();
void hardware_update();

// drive

typedef struct drive_param_s {
	int initialized;
	int valid;
	DISK_GEOMETRY geometry;
	
	int is_fdd()
	{
		if(initialized && valid) {
			switch(geometry.MediaType) {
			case F5_1Pt2_512:
			case F3_1Pt44_512:
			case F3_2Pt88_512:
			case F3_20Pt8_512:
			case F3_720_512:
			case F5_360_512:
			case F5_320_512:
			case F5_320_1024:
			case F5_180_512:
			case F5_160_512:
			case F3_120M_512:
			case F3_640_512:
			case F5_640_512:
			case F5_720_512:
			case F3_1Pt2_512:
			case F3_1Pt23_1024:
			case F5_1Pt23_1024:
			case F3_128Mb_512:
			case F3_230Mb_512:
			case F8_256_128:
			case F3_200Mb_512:
			case F3_240M_512:
			case F3_32M_512:
				return(1);
			}
		}
		return(0);
	}
	int head_num()
	{
		if(initialized && valid) {
			switch(geometry.MediaType) {
			case F5_1Pt2_512:
			case F3_1Pt44_512:
			case F3_2Pt88_512:
			case F3_20Pt8_512:
			case F3_720_512:
			case F5_360_512:
			case F5_320_512:
			case F5_320_1024:
//			case F5_180_512:
//			case F5_160_512:
			case F3_120M_512:
			case F3_640_512:
			case F5_640_512:
			case F5_720_512:
			case F3_1Pt2_512:
			case F3_1Pt23_1024:
			case F5_1Pt23_1024:
			case F3_128Mb_512:
			case F3_230Mb_512:
//			case F8_256_128:
			case F3_200Mb_512:
			case F3_240M_512:
			case F3_32M_512:
				return(2);
			default:
				return(1);
			}
		}
		return(0);
	}
} drive_param_t;

drive_param_t drive_params[26] = {0};

// memory

#if defined(HAS_I386)
	#define ADDR_MASK 0xffffffff
	#define MAX_MEM 0x2000000	/* 32MB */
#elif defined(HAS_I286)
	#define ADDR_MASK 0xffffff
	#define MAX_MEM 0x1000000	/* 16MB */
#else
	#define ADDR_MASK 0xfffff
	#define MAX_MEM 0x100000	/* 1MB */
#endif

#ifdef _MSC_VER
__declspec(align(4096))
#endif
UINT8 mem[MAX_MEM + 16]
#ifdef __GNUC__
__attribute__ ((aligned(4096)))
#endif
;

// ems

#define MAX_EMS_HANDLES 16
#define MAX_EMS_PAGES 2048	/* 32MB */

typedef struct {
	char name[8];
	UINT8* buffer;
	int pages;
	bool allocated;
} ems_handle_t;

typedef struct {
	UINT16 handle;
	UINT16 page;
	bool mapped;
} ems_page_t;

ems_handle_t ems_handles[MAX_EMS_HANDLES + 1] = {0};
ems_page_t ems_pages[4];
int free_ems_pages;

void ems_init();
void ems_finish();
void ems_release();
void ems_allocate_pages(int handle, int pages);
void ems_reallocate_pages(int handle, int pages);
void ems_release_pages(int handle);
void ems_map_page(int physical, int handle, int logical);
void ems_unmap_page(int physical);

// dma

typedef struct {
	struct {
		PAIR16 areg, creg, bareg, bcreg;
		UINT8 mode;
		UINT8 pagereg;
		UINT32 port;
	} ch[4];
	
	bool low_high;
	UINT8 cmd;
	UINT8 req;
	UINT8 mask;
	UINT8 tc;
	UINT16 tmp;
} dma_t;

dma_t dma[2];

void dma_init();
void dma_reset(int c);
void dma_write(int c, UINT32 addr, UINT8 data);
UINT8 dma_read(int c, UINT32 addr);
void dma_page_write(int c, int ch, UINT8 data);
UINT8 dma_page_read(int c, int ch);
void dma_run(int c, int ch);

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

// pio

typedef struct {
	UINT8 data, stat, ctrl;
	// code conversion
	bool conv_mode;
	bool jis_mode;
	UINT8 sjis_hi;
	UINT8 esc_buf[8];
	UINT32 esc_len;
	// printer to file
	char path[MAX_PATH];
	FILE *fp;
	SYSTEMTIME time;
} pio_t;

pio_t pio[2];

void pio_init();
void pio_finish();
void pio_release();
void pio_write(int c, UINT32 addr, UINT8 data);
UINT8 pio_read(int c, UINT32 addr);
void printer_out(int c, UINT8 data);
void pcbios_printer_out(int c, UINT8 data);

// pit

#define PIT_ALWAYS_RUNNING

typedef struct {
	INT32 count;
	UINT16 latch;
	UINT16 prev_latch, next_latch;
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
	UINT64 accum;
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

// sio

#define SIO_BUFFER_SIZE 1024

typedef struct {
	int channel;
	
	FIFO *send_buffer;
	FIFO *recv_buffer;
	
	PAIR16 divisor;
	UINT16 prev_divisor;
	UINT8 line_ctrl, prev_line_ctrl;
	UINT8 selector;
	
	UINT8 modem_ctrl;
	bool set_brk, set_rts, set_dtr;
	bool prev_set_brk;
	bool prev_set_rts, prev_set_dtr;
	DWORD set_brk_time;
	
	UINT8 line_stat_buf, line_stat_err;
	UINT8 modem_stat, prev_modem_stat;
	
	UINT8 irq_enable;
	UINT8 irq_identify;
	
	UINT8 scratch;
} sio_t;

typedef struct {
	HANDLE hThread;
	CRITICAL_SECTION csSendData;
	CRITICAL_SECTION csRecvData;
	CRITICAL_SECTION csLineCtrl;
	CRITICAL_SECTION csLineStat;
	CRITICAL_SECTION csSetBreak;
	CRITICAL_SECTION csModemCtrl;
	CRITICAL_SECTION csModemStat;
} sio_mt_t;

sio_t sio[4] = {0};
sio_mt_t sio_mt[4];

void sio_init();
void sio_finish();
void sio_release();
void sio_write(int c, UINT32 addr, UINT8 data);
UINT8 sio_read(int c, UINT32 addr);
void sio_update(int c);
void sio_update_irq(int c);
DWORD WINAPI sio_thread(void *lpx);
bool sio_wait_sending_complete(int c);

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
void kbd_reset();
UINT8 kbd_read_data();
void kbd_write_data(UINT8 val);
UINT8 kbd_read_status();
void kbd_write_command(UINT8 val);

// beep

WAVEFORMATEX wfe;
HWAVEOUT hWaveOut;
WAVEHDR whdr[2];
double beep_freq;
bool beep_playing;

void beep_init();
void beep_finish();
void beep_release();
void beep_update();

// crtc

UINT8 crtc_addr = 0;
UINT8 crtc_regs[16] = {0};
UINT8 crtc_changed[16] = {0};

#ifdef SUPPORT_GRAPHIC_SCREEN
// vram
static UINT32 vga_read(UINT32 addr, int size);
static void vga_write(UINT32 addr, UINT32 data, int size);
#endif

/* ----------------------------------------------------------------------------
	MS-DOS virtual machine
---------------------------------------------------------------------------- */

#if defined(HAS_I386)
#define SUPPORT_VCPI
#endif
#if defined(HAS_I286) || defined(HAS_I386)
#define SUPPORT_XMS
//#define SUPPORT_HMA
#endif
//#define SUPPORT_MSCDEX

#define VECTOR_TOP	0
#define VECTOR_SIZE	0x400
#define BIOS_TOP	(VECTOR_TOP + VECTOR_SIZE)
#define BIOS_SIZE	0x100
#define WORK_TOP	(BIOS_TOP + BIOS_SIZE)
#define WORK_SIZE	0x200
// IO.SYS 0070:0000
#define DEVICE_TOP	(WORK_TOP + WORK_SIZE)
#define DEVICE_SIZE	0x100	/* 22 + 18 * 12 + 7 */
#define DOS_INFO_TOP	(DEVICE_TOP + DEVICE_SIZE)
#define DOS_INFO_SIZE	0x100
//#define EXT_BIOS_TOP	(DOS_INFO_TOP + DOS_INFO_SIZE)
//#define EXT_BIOS_SIZE	0x400
#ifdef EXT_BIOS_TOP
#define DPB_TOP		(EXT_BIOS_TOP + EXT_BIOS_SIZE)
#else
#define DPB_TOP		(DOS_INFO_TOP + DOS_INFO_SIZE)
#endif
#define DPB_SIZE	0x600
#define SFT_TOP		(DPB_TOP + DPB_SIZE)
#define SFT_SIZE	0x4b0	/* 6 + 0x3b * 20 */
#define DISK_BUF_TOP	(SFT_TOP + SFT_SIZE)
#define DISK_BUF_SIZE	0x20
#define CDS_TOP		(DISK_BUF_TOP + DISK_BUF_SIZE)
#define CDS_SIZE	0x8F0	/* 88 * 26 */
#define FCB_TABLE_TOP	(CDS_TOP + CDS_SIZE)
#define FCB_TABLE_SIZE	0x10
#define SDA_TOP		(FCB_TABLE_TOP + FCB_TABLE_SIZE)
#define SDA_SIZE	0xb0
// nls tables
#define UPPERTABLE_TOP	(SDA_TOP + SDA_SIZE)
#define UPPERTABLE_SIZE	0x82
#define LOWERTABLE_TOP	(UPPERTABLE_TOP + UPPERTABLE_SIZE)
#define LOWERTABLE_SIZE	0x82
#define FILENAME_UPPERTABLE_TOP (LOWERTABLE_TOP + LOWERTABLE_SIZE)
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
#ifdef SUPPORT_GRAPHIC_SCREEN
#define MEMORY_END	0xa0000
#define VGA_VRAM_TOP	0xa0000
#define VGA_VRAM_END	0xc0000
#else
#define MEMORY_END	0xb0000
#endif
#define MDA_VRAM_TOP	0xb0000
#define TEXT_VRAM_TOP	0xb8000
#define EMS_TOP		0xc0000
#define EMS_SIZE	0x10000
UINT32 UMB_TOP = EMS_TOP; // EMS is disabled
#define UMB_END		0xf8000
#define SHADOW_BUF_TOP	0xf8000
// text vram size: 80x25x2 = 4000 = 0fa0h
// fffa0h-fffefh can be used for dummy routines
#define DUMMY_TOP	0xfffc0
//#define EMB_TOP	0x10fff0
#define EMB_TOP		0x110000 // align to 4KB
#define EMB_END		MAX_MEM

UINT32 IRET_TOP = 0;
//#define IRET_SIZE	0x100	// moved into common.h

UINT32 ATOK_TOP = 0;
// ATOK_TOP + 0x000	ATOK5 driver
// ATOK_TOP + 0x012	ATOK5 dummy routine
// ATOK_TOP + 0x015	"ATOK"
// ATOK_TOP + 0x019	ATOK5 driver dummy routine (at ATOK_TOP + ATOK_TOP - 7)
#define ATOK_SIZE	0x20	/* 18 + 3 + 4 + 7 */

UINT32 XMS_TOP = 0;
// XMS_TOP + 0x000	EMMXXXX0 driver
// XMS_TOP + 0x012	EMS dummy routine
// XMS_TOP + 0x015	XMS dummy routine
// XMS_TOP + 0x018	EMMXXXX0 ioctrl recv buffer
// XMS_TOP + 0x1b9	EMMXXXX0 driver dummy routine (at XMS_TOP + XMS_SIZE - 7)
#define XMS_SIZE	0x1c0	/* 18 + 6 + 413 + 7 */

//#define ENV_SIZE	0x800
#define ENV_SIZE	0x2000
#define PSP_SIZE	0x100
#define PSP_SYSTEM	0x0008

#define MAX_FILES	128
#define MAX_PROCESS	16
#define MAX_DTAINFO	128
#define LFN_DTA_LADDR	0x10FFF0
#define FIND_MAGIC	0x46696e64

#define DUP_STDIN	27
#define DUP_STDOUT	28
#define DUP_STDERR	29
#define DUP_STDAUX	30
#define DUP_STDPRN	31

//#define MAP_AUX_DEVICE_TO_FILE

#pragma pack(1)
typedef struct {
	UINT8 mz;
	UINT16 psp;
	UINT16 paragraphs;
	UINT8 reserved[3];
	char prog_name[8];
} mcb_t;
#pragma pack()

#ifdef SUPPORT_HMA
#pragma pack(1)
typedef struct {
	UINT8 ms[2];
	UINT16 owner;
	UINT16 size;
	UINT16 next;
} hma_mcb_t;
#pragma pack()
#endif

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
//	UINT8 far_call;
//	PAIR32 cpm_entry;
	UINT8 call5[5];
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
	UINT8 fcb2[16];
	UINT8 reserved_4[4];
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
	UINT16 bytes_per_sector;
	UINT8 sectors_per_cluster;
	UINT16 reserved_sectors;
	UINT8 fat_num;
	UINT16 root_entries;
	UINT16 total_sectors;
	UINT8 media_type;
	UINT16 sectors_per_fat;
	UINT16 sectors_per_track;
	UINT16 heads_num;
	UINT32 hidden_sectors;
	// extended
	UINT32 ext_total_sectors;
	UINT32 ext_sectors_per_fat;
} bpb_t;
#pragma pack()

#pragma pack(1)
typedef struct {
	char path_name[67];
	UINT16 drive_attrib;
	PAIR32 dpb_ptr;
	UINT16 word_1;
	UINT16 word_2;
	UINT16 word_3;
	UINT16 bs_offset;
} cds_t;
#pragma pack()

#pragma pack(1)
typedef struct {
//	UINT16 int21h_5d0ah_si;		// -38
//	UINT16 int21h_5d0ah_ds;		// -36
	// swappable data area
	UINT8 printer_cho_flag;		// -34
	UINT16 int21h_5d0ah_dx;		// -33
	UINT8 switchar;			// -31 current switch character
	UINT8 malloc_strategy;		// -30 current memory allocation strategy
	UINT8 int21h_5d0ah_cl;		// -29
	UINT8 int21h_5e01h_counter;	// -28
	UINT8 int21h_5e01h_name[16];	// -27
	UINT16 offset_lists[5];		// -11
	UINT8 int21h_5d0ah_called;	// -1
	// ----- from DOSBox -----
	UINT8 crit_error_flag;		// 0x00 Critical Error Flag
	UINT8 indos_flag;		// 0x01 InDOS flag (count of active INT 21 calls)
	UINT8 drive_crit_error;		// 0x02 Drive on which current critical error occurred or FFh
	UINT8 locus_of_last_error;	// 0x03 locus of last error
	UINT16 extended_error_code;	// 0x04 extended error code of last error
	UINT8 suggested_action;		// 0x06 suggested action for last error
	UINT8 error_class;		// 0x07 class of last error
	PAIR32 last_error_pointer; 	// 0x08 ES:DI pointer for last error
	PAIR32 current_dta;		// 0x0C current DTA (Disk Transfer Address)
	UINT16 current_psp; 		// 0x10 current PSP
	UINT16 sp_int_23;		// 0x12 stores SP across an INT 23
	UINT16 return_code;		// 0x14 return code from last process termination (zerod after reading with AH=4Dh)
	UINT8 current_drive;		// 0x16 current drive
	UINT8 extended_break_flag; 	// 0x17 extended break flag
	UINT8 fill[2];			// 0x18 flag: code page switching || flag: copy of previous byte in case of INT 24 Abort
} sda_t;
#pragma pack()

#pragma pack(1)
typedef struct {
	UINT8 reserved_0[4];	// -38
	UINT16 magic_word;	// -34 from DOSBox
	UINT8 reserved_1[30];	// -32
	UINT16 first_mcb;	// -2
	PAIR32 first_dpb;	// +0
	PAIR32 first_sft;	// +4
	PAIR32 clock_device;	// +8
	PAIR32 con_device;	// +12
	UINT16 max_sector_len;	// +16
	PAIR32 disk_buf_info;	// +18 from DOSBox
	PAIR32 cds;		// +22
	PAIR32 fcb_table;	// +26
	UINT8 reserved_2[3];	// +30
	UINT8 last_drive;	// +33
	struct {
		PAIR32 next_driver;	// +34
		UINT16 attributes;	// +38
		UINT16 strategy;	// +40
		UINT16 interrupt;	// +42
		char dev_name[8];	// +44
	} nul_device;
	UINT8 reserved_3[11];	// +52
	UINT16 buffers_x;	// +63
	UINT16 buffers_y;	// +65
	UINT8 boot_drive;	// +67
	UINT8 i386_or_later;	// +68
	UINT16 ext_mem_size;	// +69
	PAIR32 disk_buf_heads;	// +71 from DOSBox
	UINT8 reserved_4[21];	// +75
	UINT8 dos_flag;		// +96
	UINT8 reserved_5[2];	// +97
	UINT8 umb_linked;	// +99 from DOSBox
	UINT8 reserved_6[2];	// +100
	UINT16 first_umb_fcb;	// +102 from DOSBox
	UINT16 first_mcb_2;	// +104 from DOSBox
	UINT8 nul_device_routine[7];
} dos_info_t;
#pragma pack()

#pragma pack(1)
typedef struct {
	PAIR32 next_driver;
	UINT16 attributes;
	UINT16 strategy;
	UINT16 interrupt;
	char dev_name[8];
} device_t;
#pragma pack()

#pragma pack(1)
typedef struct {
	UINT16 date_format;
	char currency_symbol[5];
	char thou_sep[2];
	char dec_sep[2];
	char date_sep[2];
	char time_sep[2];
	char currency_format;
	char currency_dec_digits;
	char time_format;
	PAIR32 case_map;
	char list_sep[2];
	char reserved[10];
} country_info_t;
#pragma pack()

typedef struct {
	char path[MAX_PATH];
	int valid;
	int id;
	int atty;
	int mode;
	UINT16 info;
	UINT16 psp;
	int sio_port; // 1-4
	int lpt_port; // 1-3
} file_handler_t;

static const struct {
	int mode;
	int in;
	int out;
	const char *str;
} file_mode[] = {
	{ _O_RDONLY | _O_BINARY, 1, 0, "r"  },
	{ _O_WRONLY | _O_BINARY, 0, 1, "w"  },
	{ _O_RDWR   | _O_BINARY, 1, 1, "rw" },
};

typedef struct {
	UINT16 psp;
	char module_dir[MAX_PATH];
//#ifdef USE_DEBUGGER
	char module_path[MAX_PATH];
//#endif
	PAIR32 dta;
	UINT8 switchar;
	UINT8 verify;
	int max_files;
	char volume_label[MAX_PATH];
	bool parent_int_10h_feh_called;
	bool parent_int_10h_ffh_called;
	UINT16 parent_ds;
	UINT16 parent_es;
	struct {
		UINT16 handle;
		UINT16 page;
		bool mapped;
	} ems_pages[4];
	bool ems_pages_stored;
	bool called_by_int2eh;
} process_t;

typedef struct {
	UINT16 psp;
	UINT32 dta;
	UINT8 allowable_mask;
	UINT8 required_mask;
	HANDLE find_handle;
} dtainfo_t;

UINT8 dos_major_version = 7;	// Windows 98 Second Edition
UINT8 dos_minor_version = 10;
UINT8 win_major_version = 4;
UINT8 win_minor_version = 10;

UINT16 first_mcb;
UINT16 current_psp;

int retval = 0;
UINT16 error_code = 0;

file_handler_t file_handler[MAX_FILES];
UINT8 file_buffer[0x100000];

process_t process[MAX_PROCESS];
dtainfo_t dtalist[MAX_DTAINFO];

UINT16 malloc_strategy = 0;

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

DWORD dwConsoleMode = 0;

UINT input_cp, output_cp;
int multibyte_cp;
bool restore_input_cp = false;
bool restore_output_cp = false;
bool restore_multibyte_cp = false;

CONSOLE_CURSOR_INFO ci_old;
CONSOLE_CURSOR_INFO ci_new;
CONSOLE_FONT_INFOEX fi_new;
int font_width = 10;
int font_height = 18;

CHAR_INFO scr_buf[SCR_BUF_WIDTH * SCR_BUF_HEIGHT];
char scr_char[SCR_BUF_WIDTH * SCR_BUF_HEIGHT];
WORD scr_attr[SCR_BUF_WIDTH * SCR_BUF_HEIGHT];
COORD scr_buf_size;
COORD scr_buf_pos;
int scr_width, scr_height;
int scr_top;
bool restore_console_size = false;
bool restore_console_cursor = false;
bool restore_console_font = false;
bool cursor_moved;
bool cursor_moved_by_crtc;

FIFO *key_buf_char = NULL;
FIFO *key_buf_scan = NULL;
FIFO *key_buf_data = NULL;
bool key_changed = false;
UINT32 key_code = 0;
UINT32 key_recv = 0;

bool pcbios_is_key_buffer_empty();
void pcbios_clear_key_buffer();
void pcbios_set_key_buffer(int key_char, int key_scan);
bool pcbios_get_key_buffer(int *key_char, int *key_scan);

UINT8 ctrl_break_checking = 0x00; // ???
bool ctrl_break_detected = false;
bool ctrl_break_pressed = false;
bool ctrl_c_pressed = false;
bool raise_int_1bh = false;

int active_code_page;
int system_code_page;
int console_code_page;

UINT32 text_vram_top_address;
UINT32 text_vram_end_address;
UINT32 shadow_buffer_top_address;
UINT32 shadow_buffer_end_address;
UINT32 cursor_position_address;
int vram_pages;
bool int_10h_feh_called = false;
bool int_10h_ffh_called = false;

#define MAX_MOUSE_BUTTONS	2

typedef struct mouse_s {
	bool enabled;	// from DOSBox
	bool enabled_ps2;
	int hidden;
	int old_hidden;	// from DOSBox
	struct {
		int x, y;
	} position, prev_position, max_position, min_position, mickey;
	struct {
		bool status;
		int pressed_times;
		int released_times;
		struct {
			int x, y;
		} pressed_position;
		struct {
			int x, y;
		} released_position;
	} buttons[MAX_MOUSE_BUTTONS];
	int get_buttons()
	{
		int val = 0;
		for(int i = 0; i < MAX_MOUSE_BUTTONS; i++) {
			if(buttons[i].status) {
				val |= 1 << i;
			}
		}
		return(val);
	}
	UINT16 status, status_alt;
	UINT16 status_irq, status_irq_alt, status_irq_ps2;
	UINT16 call_mask;
	PAIR32 call_addr, call_addr_alt[8], call_addr_ps2;
	// dummy
	UINT16 sensitivity[3];
	UINT16 display_page;
	UINT16 language;
	UINT16 hot_spot[2];
	UINT16 screen_mask;
	UINT16 cursor_mask;
} mouse_t;

mouse_t mouse;

UINT16 mouse_push_ax;
UINT16 mouse_push_bx;
UINT16 mouse_push_cx;
UINT16 mouse_push_dx;
UINT16 mouse_push_si;
UINT16 mouse_push_di;
UINT16 mouse_push_ds;
UINT16 mouse_push_es;

// hma

#ifdef SUPPORT_HMA
bool is_hma_used_by_xms = false;
bool is_hma_used_by_int_2fh = false;
#endif

// xms

#ifdef SUPPORT_XMS
typedef struct emb_handle_s {
	UINT32 handle; // 0=allocated
	UINT32 address;
	int size_kb;
	int lock;
	struct emb_handle_s *prev;
	struct emb_handle_s *next;
} emb_handle_t;

emb_handle_t *emb_handle_top = NULL;
int xms_a20_local_enb_count;
UINT16 xms_dx_after_call_08h = 0;

void msdos_xms_init();
void msdos_xms_finish();
void msdos_xms_release();
emb_handle_t *msdos_xms_get_emb_handle(int handle);
int msdos_xms_get_unused_emb_handle_id();
int msdos_xms_get_unused_emb_handle_count();
void msdos_xms_split_emb_handle(emb_handle_t *emb_handle, int size_kb);
void msdos_xms_combine_emb_handles(emb_handle_t *emb_handle);
emb_handle_t *msdos_xms_alloc_emb_handle(int size_kb);
void msdos_xms_free_emb_handle(emb_handle_t *emb_handle);
#endif

#endif

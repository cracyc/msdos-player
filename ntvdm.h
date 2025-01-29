/*
	MS-DOS Player for Win32 console

	Author : Takeda.Toshiya
	Date   : 2025.01.11-
*/

#ifndef _NTVDM_H_
#define _NTVDM_H_

#include <windows.h>

typedef VOID (WINAPI *PVDD_MEMORY_HANDLER)(PVOID addr, DWORD mode);

typedef VOID (WINAPI *PFNVDD_INB)(WORD iport, PBYTE data);
typedef VOID (WINAPI *PFNVDD_INW)(WORD iport, PWORD data);
typedef VOID (WINAPI *PFNVDD_INSB)(WORD iport, PBYTE data, WORD count);
typedef VOID (WINAPI *PFNVDD_INSW)(WORD iport, PWORD data, WORD count);
typedef VOID (WINAPI *PFNVDD_OUTB)(WORD iport, BYTE data);
typedef VOID (WINAPI *PFNVDD_OUTW)(WORD iport, WORD data);
typedef VOID (WINAPI *PFNVDD_OUTSB)(WORD iport, PBYTE data, WORD count);
typedef VOID (WINAPI *PFNVDD_OUTSW)(WORD iport, PWORD data, WORD count);

typedef VOID (*PFNVDD_UCREATE)(USHORT pdb);
typedef VOID (*PFNVDD_UTERMINATE)(USHORT pdb);
typedef VOID (*PFNVDD_UBLOCK)();
typedef VOID (*PFNVDD_URESUME)();

// same as WOW64_FLOATING_SAVE_AREA

typedef struct _X87_FLOATING_SAVE_AREA {
	DWORD ControlWord;
	DWORD StatusWord;
	DWORD TagWord;
	DWORD ErrorOffset;
	DWORD ErrorSelector;
	DWORD DataOffset;
	DWORD DataSelector;
	BYTE  RegisterArea[80];
	DWORD Cr0NpxState;
} X87_FLOATING_SAVE_AREA;

// same as WOW64_CONTEXT

typedef struct _X86_CONTEXT {
	DWORD ContextFlags;
	DWORD Dr0;
	DWORD Dr1;
	DWORD Dr2;
	DWORD Dr3;
	DWORD Dr6;
	DWORD Dr7;
	X87_FLOATING_SAVE_AREA FloatSave;
	DWORD SegGs;
	DWORD SegFs;
	DWORD SegEs;
	DWORD SegDs;
	DWORD Edi;
	DWORD Esi;
	DWORD Ebx;
	DWORD Edx;
	DWORD Ecx;
	DWORD Eax;
	DWORD Ebp;
	DWORD Eip;
	DWORD SegCs;
	DWORD EFlags;
	DWORD Esp;
	DWORD SegSs;
	BYTE  ExtendedRegisters[512];
} X86_CONTEXT;

typedef struct _VDD_IO_HANDLERS {
	PFNVDD_INB inb_handler;
	PFNVDD_INW inw_handler;
	PFNVDD_INSB insb_handler;
	PFNVDD_INSW insw_handler;
	PFNVDD_OUTB outb_handler;
	PFNVDD_OUTW outw_handler;
	PFNVDD_OUTSB outsb_handler;
	PFNVDD_OUTSW outsw_handler;
} VDD_IO_HANDLERS, *PVDD_IO_HANDLERS;

typedef struct _VDD_IO_PORTRANGE {
	WORD First;
	WORD Last;
} VDD_IO_PORTRANGE, *PVDD_IO_PORTRANGE;

typedef struct _VDD_DMA_INFO {
	WORD addr;
	WORD count;
	WORD page;
	BYTE status;
	BYTE mode;
	BYTE mask;
} VDD_DMA_INFO, *PVDD_DMA_INFO;

typedef enum {
	VDM_V86,
	VDM_PM
} VDM_MODE;

typedef BYTE (*funcGetBYTE)();
typedef WORD (*funcGetWORD)();
typedef DWORD (*funcGetDWORD)();
typedef PVOID (*funcGetPVOID)();
typedef void (*funcSetBYTE)(BYTE val);
typedef void (*funcSetWORD)(WORD val);
typedef void (*funcSetDWORD)(DWORD val);
typedef PBYTE (*funcMGetVdmPointer)(DWORD addr, DWORD size, BOOL protmode);
typedef PBYTE (*funcVdmMapFlat)(WORD seg, DWORD ofs, VDM_MODE mode);
typedef BOOL (*funcVDDInstallMemoryHook)(HANDLE hvdd, PVOID addr, DWORD size, PVDD_MEMORY_HANDLER handler);
typedef BOOL (*funcVDDDeInstallMemoryHook)(HANDLE hvdd, PVOID addr, DWORD size);
typedef BOOL (*funcVDDAllocMem)(HANDLE hvdd, PVOID addr, DWORD size);
typedef BOOL (*funcVDDFreeMem)(HANDLE hvdd, PVOID addr, DWORD size);
typedef void (*funcVDDSimulateInterrupt)(int ms, BYTE line, int count);
typedef BOOL (*funcVDDInstallIOHook)(HANDLE hvdd, WORD cPortRange, PVDD_IO_PORTRANGE pPortRange, PVDD_IO_HANDLERS IOhandler);
typedef void (*funcVDDDeInstallIOHook)(HANDLE hvdd, WORD cPortRange, PVDD_IO_PORTRANGE pPortRange);
typedef DWORD (*funcVDDRequestDMA)(HANDLE hvdd, WORD ch, PVOID buf, DWORD len);
typedef BOOL (*funcVDDQueryDMA)(HANDLE hvdd, WORD ch, PVDD_DMA_INFO info);
typedef BOOL (*funcVDDSetDMA)(HANDLE hvdd, WORD ch, WORD flag, PVDD_DMA_INFO info);
typedef void (*funcVDDSimulate16)();
typedef void (*funcVDDTerminateVDM)(void);
typedef BOOL (*funcVDDInstallUserHook)(HANDLE hvdd, PFNVDD_UCREATE ucr_Handler, PFNVDD_UTERMINATE uterm_Handler, PFNVDD_UBLOCK ublock_handler, PFNVDD_URESUME uresume_handler);
typedef BOOL (*funcVDDDeInstallUserHook)(HANDLE hvdd);

typedef struct _VDD_FUNC_TABLE {
	funcGetBYTE getAL;
	funcGetBYTE getAH;
	funcGetWORD getAX;
	funcGetDWORD getEAX;
	funcGetBYTE getBL;
	funcGetBYTE getBH;
	funcGetWORD getBX;
	funcGetDWORD getEBX;
	funcGetBYTE getCL;
	funcGetBYTE getCH;
	funcGetWORD getCX;
	funcGetDWORD getECX;
	funcGetBYTE getDL;
	funcGetBYTE getDH;
	funcGetWORD getDX;
	funcGetDWORD getEDX;
	funcGetWORD getSP;
	funcGetDWORD getESP;
	funcGetWORD getBP;
	funcGetDWORD getEBP;
	funcGetWORD getSI;
	funcGetDWORD getESI;
	funcGetWORD getDI;
	funcGetDWORD getEDI;
	funcSetBYTE setAL;
	funcSetBYTE setAH;
	funcSetWORD setAX;
	funcSetDWORD setEAX;
	funcSetBYTE setBL;
	funcSetBYTE setBH;
	funcSetWORD setBX;
	funcSetDWORD setEBX;
	funcSetBYTE setCL;
	funcSetBYTE setCH;
	funcSetWORD setCX;
	funcSetDWORD setECX;
	funcSetBYTE setDL;
	funcSetBYTE setDH;
	funcSetWORD setDX;
	funcSetDWORD setEDX;
	funcSetWORD setSP;
	funcSetDWORD setESP;
	funcSetWORD setBP;
	funcSetDWORD setEBP;
	funcSetWORD setSI;
	funcSetDWORD setESI;
	funcSetWORD setDI;
	funcSetDWORD setEDI;
	funcGetWORD getDS;
	funcGetWORD getES;
	funcGetWORD getCS;
	funcGetWORD getSS;
	funcGetWORD getFS;
	funcGetWORD getGS;
	funcSetWORD setDS;
	funcSetWORD setES;
	funcSetWORD setCS;
	funcSetWORD setSS;
	funcSetWORD setFS;
	funcSetWORD setGS;
	funcGetWORD getIP;
	funcGetDWORD getEIP;
	funcSetWORD setIP;
	funcSetDWORD setEIP;
	funcGetDWORD getCF;
	funcGetDWORD getPF;
	funcGetDWORD getAF;
	funcGetDWORD getZF;
	funcGetDWORD getSF;
	funcGetDWORD getIF;
	funcGetDWORD getDF;
	funcGetDWORD getOF;
	funcSetDWORD setCF;
	funcSetDWORD setPF;
	funcSetDWORD setAF;
	funcSetDWORD setZF;
	funcSetDWORD setSF;
	funcSetDWORD setIF;
	funcSetDWORD setDF;
	funcSetDWORD setOF;
	funcGetDWORD getEFLAGS;
	funcSetDWORD setEFLAGS;
	funcGetWORD getMSW;
	funcSetWORD setMSW;
	funcGetPVOID getIntelRegistersPointer;
	funcMGetVdmPointer MGetVdmPointer;
	funcVdmMapFlat VdmMapFlat;
	funcVDDInstallMemoryHook VDDInstallMemoryHook;
	funcVDDDeInstallMemoryHook VDDDeInstallMemoryHook;
	funcVDDAllocMem VDDAllocMem;
	funcVDDFreeMem VDDFreeMem;
	funcVDDSimulateInterrupt VDDSimulateInterrupt;
	funcVDDInstallIOHook VDDInstallIOHook;
	funcVDDDeInstallIOHook VDDDeInstallIOHook;
	funcVDDRequestDMA VDDRequestDMA;
	funcVDDQueryDMA VDDQueryDMA;
	funcVDDSetDMA VDDSetDMA;
	funcVDDSimulate16 VDDSimulate16;
	funcVDDTerminateVDM VDDTerminateVDM;
	funcVDDInstallUserHook VDDInstallUserHook;
	funcVDDDeInstallUserHook VDDDeInstallUserHook;
} VDD_FUNC_TABLE, *PVDD_FUNC_TABLE;

#endif

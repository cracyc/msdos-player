/*
	MS-DOS Player for Win32 console

	Author : Takeda.Toshiya
	Date   : 2025.01.11-
*/

#ifndef _NTVDM_H_
#define _NTVDM_H_

#include <windows.h>

typedef VOID (WINAPI *PFNVDD_INB)(WORD iport, PBYTE data);
typedef VOID (WINAPI *PFNVDD_INW)(WORD iport, PWORD data);
typedef VOID (WINAPI *PFNVDD_INSB)(WORD iport, PBYTE data, WORD count);
typedef VOID (WINAPI *PFNVDD_INSW)(WORD iport, PWORD data, WORD count);
typedef VOID (WINAPI *PFNVDD_OUTB)(WORD iport, BYTE data);
typedef VOID (WINAPI *PFNVDD_OUTW)(WORD iport, WORD data);
typedef VOID (WINAPI *PFNVDD_OUTSB)(WORD iport, PBYTE data, WORD count);
typedef VOID (WINAPI *PFNVDD_OUTSW)(WORD iport, PWORD data, WORD count);
typedef VOID (WINAPI *PVDD_MEMORY_HANDLER)(PVOID addr, ULONG mode);

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

typedef enum {
	VDM_V86,
	VDM_PM
} VDM_MODE;

typedef BYTE (*funcGetBYTE)();
typedef WORD (*funcGetWORD)();
typedef DWORD (*funcGetDWORD)();
typedef void (*funcSetBYTE)(BYTE val);
typedef void (*funcSetWORD)(WORD val);
typedef void (*funcSetDWORD)(DWORD val);
typedef BOOL (*funcVDDInstallIOHook)(HANDLE hvdd, WORD cPortRange, PVDD_IO_PORTRANGE pPortRange, PVDD_IO_HANDLERS IOhandler);
typedef void (*funcVDDDeInstallIOHook)(HANDLE hvdd, WORD cPortRange, PVDD_IO_PORTRANGE pPortRange);
typedef BYTE* (*funcMGetVdmPointer)(DWORD addr, DWORD size, BOOL protmode);
typedef BYTE* (*funcVdmMapFlat)(WORD seg, DWORD ofs, VDM_MODE mode);
typedef void (*funcVDDTerminateVDM)(void);

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
	funcVDDInstallIOHook VDDInstallIOHook;
	funcVDDDeInstallIOHook VDDDeInstallIOHook;
	funcMGetVdmPointer MGetVdmPointer;
	funcVdmMapFlat VdmMapFlat;
	funcVDDTerminateVDM VDDTerminateVDM;
} VDD_FUNC_TABLE, *PVDD_FUNC_TABLE;

#endif

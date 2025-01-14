/*
	MS-DOS Player for Win32 console

	Author : Takeda.Toshiya
	Date   : 2025.01.11-
*/

#include <windows.h>
#include "ntvdm.h"

static VDD_FUNC_TABLE func;

BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpvReserved)
{
	if (fdwReason == DLL_PROCESS_ATTACH) {
		DisableThreadLibraryCalls(hinstDLL);
	}
	return TRUE;
}

__declspec(dllexport) void WINAPI SetFuncTable(PVDD_FUNC_TABLE ptr)
{
	func = *ptr;
}

__declspec(dllexport) BYTE WINAPI getAL()
{
	return func.getAL();
}

__declspec(dllexport) BYTE WINAPI getAH()
{
	return func.getAH();
}

__declspec(dllexport) WORD WINAPI getAX()
{
	return func.getAX();
}

__declspec(dllexport) DWORD WINAPI getEAX()
{
	return func.getEAX();
}

__declspec(dllexport) BYTE WINAPI getBL()
{
	return func.getBL();
}

__declspec(dllexport) BYTE WINAPI getBH()
{
	return func.getBH();
}

__declspec(dllexport) WORD WINAPI getBX()
{
	return func.getBX();
}

__declspec(dllexport) DWORD WINAPI getEBX()
{
	return func.getEBX();
}

__declspec(dllexport) BYTE WINAPI getCL()
{
	return func.getCL();
}

__declspec(dllexport) BYTE WINAPI getCH()
{
	return func.getCH();
}

__declspec(dllexport) WORD WINAPI getCX()
{
	return func.getCX();
}

__declspec(dllexport) DWORD WINAPI getECX()
{
	return func.getECX();
}

__declspec(dllexport) BYTE WINAPI getDL()
{
	return func.getDL();
}

__declspec(dllexport) BYTE WINAPI getDH()
{
	return func.getDH();
}

__declspec(dllexport) WORD WINAPI getDX()
{
	return func.getDX();
}

__declspec(dllexport) DWORD WINAPI getEDX()
{
	return func.getEDX();
}

__declspec(dllexport) WORD WINAPI getSP()
{
	return func.getSP();
}

__declspec(dllexport) DWORD WINAPI getESP()
{
	return func.getESP();
}

__declspec(dllexport) WORD WINAPI getBP()
{
	return func.getBP();
}

__declspec(dllexport) DWORD WINAPI getEBP()
{
	return func.getEBP();
}

__declspec(dllexport) WORD WINAPI getSI()
{
	return func.getSI();
}

__declspec(dllexport) DWORD WINAPI getESI()
{
	return func.getESI();
}

__declspec(dllexport) WORD WINAPI getDI()
{
	return func.getDI();
}

__declspec(dllexport) DWORD WINAPI getEDI()
{
	return func.getEDI();
}

__declspec(dllexport) void WINAPI setAL(BYTE val)
{
	func.setAL(val);
}

__declspec(dllexport) void WINAPI setAH(BYTE val)
{
	func.setAH(val);
}

__declspec(dllexport) void WINAPI setAX(WORD val)
{
	func.setAX(val);
}

__declspec(dllexport) void WINAPI setEAX(DWORD val)
{
	func.setEAX(val);
}

__declspec(dllexport) void WINAPI setBL(BYTE val)
{
	func.setBL(val);
}

__declspec(dllexport) void WINAPI setBH(BYTE val)
{
	func.setBH(val);
}

__declspec(dllexport) void WINAPI setBX(WORD val)
{
	func.setBX(val);
}

__declspec(dllexport) void WINAPI setEBX(DWORD val)
{
	func.setEBX(val);
}

__declspec(dllexport) void WINAPI setCL(BYTE val)
{
	func.setCL(val);
}

__declspec(dllexport) void WINAPI setCH(BYTE val)
{
	func.setCH(val);
}

__declspec(dllexport) void WINAPI setCX(WORD val)
{
	func.setCX(val);
}

__declspec(dllexport) void WINAPI setECX(DWORD val)
{
	func.setECX(val);
}

__declspec(dllexport) void WINAPI setDL(BYTE val)
{
	func.setDL(val);
}

__declspec(dllexport) void WINAPI setDH(BYTE val)
{
	func.setDH(val);
}

__declspec(dllexport) void WINAPI setDX(WORD val)
{
	func.setDX(val);
}

__declspec(dllexport) void WINAPI setEDX(DWORD val)
{
	func.setEDX(val);
}

__declspec(dllexport) void WINAPI setSP(WORD val)
{
	func.setSP(val);
}

__declspec(dllexport) void WINAPI setESP(DWORD val)
{
	func.setESP(val);
}

__declspec(dllexport) void WINAPI setBP(WORD val)
{
	func.setBP(val);
}

__declspec(dllexport) void WINAPI setEBP(DWORD val)
{
	func.setEBP(val);
}

__declspec(dllexport) void WINAPI setSI(WORD val)
{
	func.setSI(val);
}

__declspec(dllexport) void WINAPI setESI(DWORD val)
{
	func.setESI(val);
}

__declspec(dllexport) void WINAPI setDI(WORD val)
{
	func.setDI(val);
}

__declspec(dllexport) void WINAPI setEDI(DWORD val)
{
	func.setEDI(val);
}

__declspec(dllexport) WORD WINAPI getDS()
{
	return func.getDS();
}

__declspec(dllexport) WORD WINAPI getES()
{
	return func.getES();
}

__declspec(dllexport) WORD WINAPI getCS()
{
	return func.getCS();
}

__declspec(dllexport) WORD WINAPI getSS()
{
	return func.getSS();
}

__declspec(dllexport) WORD WINAPI getFS()
{
	return func.getFS();
}

__declspec(dllexport) WORD WINAPI getGS()
{
	return func.getGS();
}

__declspec(dllexport) void WINAPI setDS(WORD val)
{
	func.setDS(val);
}

__declspec(dllexport) void WINAPI setES(WORD val)
{
	func.setES(val);
}

__declspec(dllexport) void WINAPI setCS(WORD val)
{
	func.setCS(val);
}

__declspec(dllexport) void WINAPI setSS(WORD val)
{
	func.setSS(val);
}

__declspec(dllexport) void WINAPI setFS(WORD val)
{
	func.setFS(val);
}

__declspec(dllexport) void WINAPI setGS(WORD val)
{
	func.setGS(val);
}

__declspec(dllexport) WORD WINAPI getIP()
{
	return func.getIP();
}

__declspec(dllexport) DWORD WINAPI getEIP()
{
	return func.getEIP();
}

__declspec(dllexport) void WINAPI setIP(WORD val)
{
	func.setIP(val);
}

__declspec(dllexport) void WINAPI setEIP(DWORD val)
{
	func.setEIP(val);
}

__declspec(dllexport) DWORD WINAPI getCF()
{
	return func.getCF();
}

__declspec(dllexport) DWORD WINAPI getPF()
{
	return func.getPF();
}

__declspec(dllexport) DWORD WINAPI getAF()
{
	return func.getAF();
}

__declspec(dllexport) DWORD WINAPI getZF()
{
	return func.getZF();
}

__declspec(dllexport) DWORD WINAPI getSF()
{
	return func.getSF();
}

__declspec(dllexport) DWORD WINAPI getIF()
{
	return func.getIF();
}

__declspec(dllexport) DWORD WINAPI getDF()
{
	return func.getDF();
}

__declspec(dllexport) DWORD WINAPI getOF()
{
	return func.getOF();
}

__declspec(dllexport) void WINAPI setCF(DWORD val)
{
	func.setCF(val);
}

__declspec(dllexport) void WINAPI setPF(DWORD val)
{
	func.setPF(val);
}

__declspec(dllexport) void WINAPI setAF(DWORD val)
{
	func.setAF(val);
}

__declspec(dllexport) void WINAPI setZF(DWORD val)
{
	func.setZF(val);
}

__declspec(dllexport) void WINAPI setSF(DWORD val)
{
	func.setSF(val);
}

__declspec(dllexport) void WINAPI setIF(DWORD val)
{
	func.setIF(val);
}

__declspec(dllexport) void WINAPI setDF(DWORD val)
{
	func.setDF(val);
}

__declspec(dllexport) void WINAPI setOF(DWORD val)
{
	func.setOF(val);
}

__declspec(dllexport) DWORD WINAPI getEFLAGS()
{
	return func.getEFLAGS();
}

__declspec(dllexport) void WINAPI setEFLAGS(DWORD val)
{
	func.setEFLAGS(val);
}

__declspec(dllexport) WORD WINAPI getMSW()
{
	return func.getMSW();
}

__declspec(dllexport) void WINAPI setMSW(WORD val)
{
	func.setMSW(val);
}

__declspec(dllexport) BOOL WINAPI VDDInstallIOHook(HANDLE hvdd, WORD cPortRange, PVDD_IO_PORTRANGE pPortRange, PVDD_IO_HANDLERS IOhandler)
{
	return func.VDDInstallIOHook(hvdd, cPortRange, pPortRange, IOhandler);
}

__declspec(dllexport) void WINAPI VDDDeInstallIOHook(HANDLE hvdd, WORD cPortRange, PVDD_IO_PORTRANGE pPortRange)
{
	func.VDDDeInstallIOHook(hvdd, cPortRange, pPortRange);
}

__declspec(dllexport) BYTE *WINAPI MGetVdmPointer(DWORD addr, DWORD size, BOOL protmode)
{
	return func.MGetVdmPointer(addr, size, protmode);
}

__declspec(dllexport) BYTE *WINAPI Sim32pGetVDMPointer(DWORD addr, BOOL protmode)
{
	return func.MGetVdmPointer(addr, 0, protmode);
}

__declspec(dllexport) BYTE *WINAPI VdmMapFlat(WORD seg, DWORD ofs, VDM_MODE mode)
{
	return func.VdmMapFlat(seg, ofs, mode);
}

__declspec(dllexport) BOOL WINAPI VdmUnmapFlat(WORD seg, DWORD ofs, PVOID buf, VDM_MODE mode)
{
	return TRUE;
}

__declspec(dllexport) BOOL WINAPI VdmFlushCache(WORD seg, DWORD ofs, DWORD size, VDM_MODE mode)
{
	return TRUE;
}

__declspec(dllexport) BOOL WINAPI VDDInstallMemoryHook(HANDLE hVdd, PVOID addr, DWORD size, PVDD_MEMORY_HANDLER MemoryHandler)
{
	return FALSE;
}

__declspec(dllexport) BOOL WINAPI VDDDeInstallMemoryHook(HANDLE hVdd, PVOID addr, DWORD size)
{
	return FALSE;
}

__declspec(dllexport) BOOL WINAPI VDDAllocMem(HANDLE hVdd, PVOID addr, ULONG size)
{
	return FALSE;
}

__declspec(dllexport) BOOL WINAPI VDDFreeMem(HANDLE hVdd, PVOID addr, ULONG size)
{
	return FALSE;
}

__declspec(dllexport) BOOL WINAPI VDDIncludeMem(HANDLE hVdd, PVOID addr, ULONG size)
{
	return FALSE;
}

__declspec(dllexport) BOOL WINAPI VDDExcludeMem(HANDLE hVdd, PVOID addr, ULONG size)
{
	return FALSE;
}

__declspec(dllexport) void WINAPI VDDTerminateVDM(void)
{
	func.VDDTerminateVDM();
}

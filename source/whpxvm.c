#include <WinHvPlatform.h>

void whpxvm_panic(const char *fmt, ...)
{
	va_list arg;
	va_start(arg, fmt);
	vprintf(fmt, arg);
	va_end(arg);
	ExitProcess(1);
}

#define REG16(x) values[x].Reg16
#define AX EAX
#define BX EBX
#define CX ECX
#define DX EDX
#define SP ESP
#define BP EBP
#define SI ESI
#define DI EDI
#define REG32(x) values[x].Reg32
#define SREG_BASE(x) values[x].Segment.Base
#define SREG(x) values[x].Segment.Selector

#define i386_load_segment_descriptor(x) load_segdesc((segment_desc *)&values[x].Segment)
#define i386_sreg_load(x, y, z) values[y].Segment.Selector = x; load_segdesc((segment_desc *)&values[y].Segment)
#define i386_get_flags() values[EFLAGS].Reg32
#define i386_set_flags(x) values[EFLAGS].Reg32 = x
#define i386_push16 PUSH16
#define i386_pop16 POP16
#define vtlb_free(x) {}
#define I386_SREG segment_desc

#define m_eip values[EIP].Reg32
#define m_pc (values[EIP].Reg32 + values[CS].Segment.Base)
#define CR(x) values[CR ##x].Reg32
#define DR(x) values[DR ##x].Reg32
#define m_gdtr (*(segment_desc *)&values[GDTR].Segment)
#define m_idtr (*(segment_desc *)&values[IDTR].Segment)
#define m_ldtr (*(segment_desc *)&values[LDTR].Segment)
#define m_task (*(segment_desc *)&values[TR].Segment)

#define WHPXVM_STR2(s) #s
#define WHPXVM_STR(s) WHPXVM_STR2(s)
#define WHPXVM_ERR fprintf(stderr, "%s ("  WHPXVM_STR(__LINE__)  ") WHPX err.\n", __FUNCTION__)
#define WHPXVM_ERRF(fmt, ...) fprintf(stderr, "%s ("  WHPXVM_STR(__LINE__)  ") " fmt "\n", __FUNCTION__, ##__VA_ARGS__)

#define PROTECTED_MODE (values[CR0].Reg32 & 1)
#define V8086_MODE (values[EFLAGS].Reg32 & 0x20000)

#define I386OP(x) i386_ ##x
#define HOLD_LINE 1
#define CLEAR_LINE 0
#define INPUT_LINE_IRQ 1

typedef struct
{
	UINT64 base;
	UINT32 limit;
	union {
		UINT16 selector;
		UINT16 segment;
	};

	union
	{
		struct
		{
			UINT16 SegmentType:4;
			UINT16 NonSystemSegment:1;
			UINT16 DescriptorPrivilegeLevel:2;
			UINT16 Present:1;
			UINT16 Reserved:4;
			UINT16 Available:1;
			UINT16 Long:1;
			UINT16 Default:1;
			UINT16 Granularity:1;
		};

		UINT16 flags;
    };
} segment_desc;

static const WHV_REGISTER_NAME whpx_register_names[] = {
	WHvX64RegisterRax,
	WHvX64RegisterRcx,
	WHvX64RegisterRdx,
	WHvX64RegisterRbx,
	WHvX64RegisterRsp,
	WHvX64RegisterRbp,
	WHvX64RegisterRsi,
	WHvX64RegisterRdi,
	WHvX64RegisterRip,
	WHvX64RegisterRflags,

	WHvX64RegisterEs,
	WHvX64RegisterCs,
	WHvX64RegisterSs,
	WHvX64RegisterDs,
	WHvX64RegisterFs,
	WHvX64RegisterGs,

	WHvX64RegisterCr0,
	WHvX64RegisterCr2,
	WHvX64RegisterCr3,
	WHvX64RegisterCr4,

	WHvX64RegisterDr0,
	WHvX64RegisterDr1,
	WHvX64RegisterDr2,
	WHvX64RegisterDr3,
	WHvX64RegisterDr6,
	WHvX64RegisterDr7,

	WHvX64RegisterLdtr,
	WHvX64RegisterTr,
	WHvX64RegisterIdtr,
	WHvX64RegisterGdtr
};

enum {
	EAX,
	ECX,
	EDX,
	EBX,
	ESP,
	EBP,
	ESI,
	EDI,
	EIP,
	EFLAGS,
	ES,
	CS,
	SS,
	DS,
	FS,
	GS,
	CR0,
	CR2,
	CR3,
	CR4,
	DR0,
	DR1,
	DR2,
	DR3,
	DR6,
	DR7,
	LDTR,
	TR,
	IDTR,
	GDTR
};



static WHV_PARTITION_HANDLE part;
static WHV_REGISTER_VALUE values[RTL_NUMBER_OF(whpx_register_names)];
static WHV_RUN_VP_EXIT_CONTEXT exitctxt;
static UINT8 m_CF, m_SF, m_ZF, m_IF, m_IOP1, m_IOP2, m_VM, m_NT;
static UINT32 m_a20_mask = 0xffefffff;
static UINT8 cpu_type = 6; // ppro
static UINT8 cpu_step = 0x0f; // whatever
static UINT8 m_CPL = 0; // always check at cpl 0
static UINT32 m_int6h_skip_eip = 0xffff0; // TODO: ???
static UINT8 m_ext;
static UINT32 m_prev_eip;
static int saved_vector = -1;
static bool cpu_running = false;

static int instr_emu(int cnt);

enum {
	AL,
	AH,
	BL,
	BH,
	CL,
	CH,
	DL,
	DH
};

#define REG8(x) *_REG8(x)

static inline UINT8 *_REG8(int reg)
{
	switch(reg)
	{
	case AL:
		return &values[EAX].Reg8;
	case AH:
		return &values[EAX].Reg8 + 1;
	case BL:
		return &values[EBX].Reg8;
	case BH:
		return &values[EBX].Reg8 + 1;
	case CL:
		return &values[ECX].Reg8;
	case CH:
		return &values[ECX].Reg8 + 1;
	case DL:
		return &values[EDX].Reg8;
	case DH:
		return &values[EDX].Reg8 + 1;
	}
	return NULL;
}

static void CALLBACK cpu_int_cb(LPVOID arg, DWORD low, DWORD high)
{
	WHvCancelRunVirtualProcessor(part, 0, 0);
}

static DWORD CALLBACK cpu_int_th(LPVOID arg)
{
	LARGE_INTEGER when;
	HANDLE timer;

	if (!(timer = CreateWaitableTimerA( NULL, FALSE, NULL ))) return 0;

	when.u.LowPart = when.u.HighPart = 0;
	SetWaitableTimer(timer, &when, 10, cpu_int_cb, arg, FALSE);
	for (;;) SleepEx(INFINITE, TRUE);
}

static void vm_exit()
{
	WHvDeleteVirtualProcessor(part, 0);
	WHvDeletePartition(part);
}

static BOOL cpu_init_whpx()
{
	WHV_CAPABILITY cap;
	UINT32 val;
	HRESULT hr;
	if ((hr = WHvGetCapability(WHvCapabilityCodeHypervisorPresent, &cap, sizeof(cap), &val)) || !cap.HypervisorPresent)
	{
		WHPXVM_ERRF("CAPABILITY %lx", hr);
		return FALSE;
	}
	if ((hr = WHvCreatePartition(&part)))
	{
		WHPXVM_ERRF("CREATE_PARTITION %lx", hr);
		return FALSE;
	}
	val = 1;
	if ((hr = WHvSetPartitionProperty(part, WHvPartitionPropertyCodeProcessorCount, &val, sizeof(UINT32))))
	{
		WHPXVM_ERRF("SET_PROPERTY %lx", hr);
		return FALSE;
	}
	if ((hr = WHvSetupPartition(part)))
	{
		WHPXVM_ERRF("Could not create vm. %lx", hr);
		return FALSE;
	}
	if ((hr = WHvCreateVirtualProcessor(part, 0, 0)))
	{
		WHPXVM_ERRF("could not create vcpu. %lx", hr);
		return FALSE;
	}
	if ((hr = WHvMapGpaRange(part, mem, 0, MEMORY_END,
				WHvMapGpaRangeFlagRead | WHvMapGpaRangeFlagWrite | WHvMapGpaRangeFlagExecute)))
	{
		WHPXVM_ERRF("SET_RAM %lx", hr);
		return FALSE;
	}
	if ((hr = WHvMapGpaRange(part, mem + UMB_TOP, UMB_TOP, 0x100000 - UMB_TOP,
				WHvMapGpaRangeFlagRead | WHvMapGpaRangeFlagWrite | WHvMapGpaRangeFlagExecute)))
	{
		WHPXVM_ERRF("SET_RAM %lx", hr);
		return FALSE;
	}
	if ((hr = WHvMapGpaRange(part, mem, 0x100000, 0x100000,
				WHvMapGpaRangeFlagRead | WHvMapGpaRangeFlagWrite | WHvMapGpaRangeFlagExecute)))
	{
		WHPXVM_ERRF("SET_RAM %lx", hr);
		return FALSE;
	}
	if ((hr = WHvMapGpaRange(part, mem + 0x200000, 0x200000, MAX_MEM - 0x200000,
				WHvMapGpaRangeFlagRead | WHvMapGpaRangeFlagWrite | WHvMapGpaRangeFlagExecute)))
	{
		WHPXVM_ERRF("SET_RAM %lx", hr);
		return FALSE;
	}
	WHvGetVirtualProcessorRegisters(part, 0, whpx_register_names, RTL_NUMBER_OF(whpx_register_names), values);
	HANDLE thread = CreateThread(NULL, 0, cpu_int_th, NULL, 0, NULL);
	SetThreadPriority(thread, THREAD_PRIORITY_ABOVE_NORMAL);
	CloseHandle(thread);
	atexit(vm_exit);
	return TRUE;
}

static void cpu_reset_whpx()
{
}

const int TRANSLATE_READ            = 0;        // translate for read
const int TRANSLATE_WRITE           = 1;        // translate for write
const int TRANSLATE_FETCH           = 2;        // translate for instruction fetch

// TODO: mark pages dirty if necessary, check for page faults
// dos programs likly never used three level page tables
static int translate_address(int pl, int type, UINT32 *address, UINT32 *error)
{
	if(!(values[CR0].Reg32 & 0x80000000))
		return TRUE;

	UINT32 *pdbr = (UINT32 *)(mem + (values[CR3].Reg32 * 0xfffff000));
	UINT32 a = *address;
	UINT32 dir = (a >> 22) & 0x3ff;
	UINT32 table = (a >> 12) & 0x3ff;
	UINT32 page_dir = pdbr[dir];
	if(page_dir & 1)
	{
		if((page_dir & 0x80) && (values[CR4].Reg32 & 0x10))
		{
			*address = (page_dir & 0xffc00000) | (a & 0x003fffff);
			return TRUE;
		}
	}
	else
	{
		UINT32 page_entry = *(UINT32 *)(mem + (page_dir & 0xfffff000) + (table * 4));
		if(!(page_entry & 1))
			return FALSE;
		else
		{
			*address = (page_entry & 0xfffff000) | (a & 0xfff);
			return TRUE;
		}
	}
	return FALSE;
}

static BOOL i386_load_protected_mode_segment(I386_SREG *seg, UINT32 *desc)
{
	UINT32 v1,v2;
	UINT32 base, limit;
	int entry;

	if(!seg->selector)
	{
		seg->flags = 0;
		seg->base = 0;
		seg->limit = 0;
		return 0;
	}

	if ( seg->selector & 0x4 )
	{
		base = values[GDTR].Segment.Base;
		limit = values[GDTR].Segment.Limit;
	}
	else
	{
		base = values[LDTR].Segment.Base;
		limit = values[LDTR].Segment.Limit;
	}

	entry = seg->selector & ~0x7;
	if (limit == 0 || entry + 7 > limit)
		return 0;

	UINT32 address = base + entry;
	translate_address(0, TRANSLATE_READ, &address, NULL); 
	v1 = *(UINT32 *)(mem + address);
	v2 = *(UINT32 *)(mem + address + 4);

	seg->flags = (v2 >> 8) & 0xf0ff;
	seg->base = (v2 & 0xff000000) | ((v2 & 0xff) << 16) | ((v1 >> 16) & 0xffff);
	seg->limit = (v2 & 0xf0000) | (v1 & 0xffff);
	if (seg->flags & 0x8000)
		seg->limit = (seg->limit << 12) | 0xfff;

	return 1;
}

static void i386_set_descriptor_accessed(UINT16 selector)
{
	// assume the selector is valid, we don't need to check it again
	UINT32 base, addr;
	if(!(selector & ~3))
		return;

	if ( selector & 0x4 )
		base = values[LDTR].Segment.Base;
	else
		base = values[GDTR].Segment.Base;

	addr = base + (selector & ~7) + 5;
	translate_address(0, TRANSLATE_READ, &addr, NULL); 
	mem[addr] |= 1;
}

static void load_segdesc(segment_desc *seg)
{
	if (PROTECTED_MODE)
	{
		if (!V8086_MODE)
		{
			i386_load_protected_mode_segment((I386_SREG *)seg, NULL);
			if(seg->selector)
				i386_set_descriptor_accessed(seg->selector);
		}
		else
		{
			seg->base = seg->selector << 4;
			seg->limit = 0xffff;
			seg->flags = (seg == (segment_desc *)&values[CS].Segment) ? 0xfb : 0xf3;
		}
	}
	else
	{
		seg->base = seg->selector << 4;
		if(seg == (segment_desc *)&values[CS].Segment)
			seg->flags = 0x93;
	}
}

// TODO: check ss limit
static UINT32 i386_read_stack(bool dword = false)
{
	UINT32 addr = values[SS].Segment.Base;
	if(values[SS].Segment.Default)
		addr += REG32(ESP);
	else
		addr += REG16(SP) & 0xffff;
	translate_address(0, TRANSLATE_READ, &addr, NULL);
	return dword ? read_dword(addr) : read_word(addr);
}

static void i386_write_stack(UINT32 value, bool dword = false)
{
	UINT32 addr = values[SS].Segment.Base;
	if(values[SS].Segment.Default)
		addr += REG32(ESP);
	else
		addr += REG16(SP);
	translate_address(0, TRANSLATE_WRITE, &addr, NULL);
	dword ? write_dword(addr, value) : write_word(addr, value);
}

static void PUSH16(UINT16 val)
{
	if(values[SS].Segment.Default)
		REG32(ESP) -= 2;
	else
		REG16(SP) = (REG16(SP) - 2) & 0xffff;
	i386_write_stack(val);
}

static UINT16 POP16()
{
	UINT16 val = i386_read_stack();
	if(values[SS].Segment.Default)
		REG32(ESP) += 2;
	else
		REG16(SP) = (REG16(SP) + 2) & 0xffff;
	return val;
}

// pmode far calls/jmps/rets are potentially extremely complex (call gates, task switches, privilege changes)
// so bail and hope the issue never comes up
// if the destination isn't mapped, we're in trouble
static void i386_call_far(UINT16 selector, UINT32 address)
{
	if (PROTECTED_MODE)
	{
		if (!V8086_MODE)
			whpxvm_panic("i386_call_far in protected mode and !v86mode not supported");
		else
		{
			if((values[CR0].Reg32 & 0x80000000) && (selector == DUMMY_TOP))  // check that this is mapped
			{
				UINT32 addr = DUMMY_TOP + address;
				translate_address(0, TRANSLATE_READ, &addr, NULL);
				if (address != (DUMMY_TOP + address))
					whpxvm_panic("i386_call_far to dummy segment with page unmapped");
			}
		}
	}
	PUSH16(values[CS].Segment.Selector);
	PUSH16(values[EIP].Reg16);
	values[CS].Segment.Selector = selector;
	load_segdesc((segment_desc *)&values[CS].Segment);
	values[EIP].Reg32 = address;
}

static void i386_jmp_far(UINT16 selector, UINT32 address)
{
	if (PROTECTED_MODE && !V8086_MODE)
		whpxvm_panic("i386_jmp_far in protected mode and !v86mode not supported");
	values[CS].Segment.Selector = selector;
	load_segdesc((segment_desc *)&values[CS].Segment);
	values[EIP].Reg32 = address;
}

static void i386_pushf()
{
	PUSH16(values[EFLAGS].Reg16);
}

static void i386_retf16()
{
	if (PROTECTED_MODE && !V8086_MODE)
		whpxvm_panic("i386_retf16 in protected mode and !v86mode not supported");
	values[EIP].Reg32 = POP16();
	values[CS].Segment.Selector = POP16();
	load_segdesc((segment_desc *)&values[CS]);

}

static void i386_iret16()
{
	if (PROTECTED_MODE && !V8086_MODE)
		whpxvm_panic("i386_iret16 in protected mode and !v86mode not supported");
	values[EIP].Reg32 = POP16();
	values[CS].Segment.Selector = POP16();
	load_segdesc((segment_desc *)&values[CS].Segment);
	values[EFLAGS].Reg32 = (values[EFLAGS].Reg32 & 0xffff0002) | POP16();
	m_CF = values[EFLAGS].Reg16 & 1;
	m_ZF = (values[EFLAGS].Reg16 & 0x40) ? 1 : 0;
	m_SF = (values[EFLAGS].Reg16 & 0x80) ? 1 : 0;
	m_IF = (values[EFLAGS].Reg16 & 0x200) ? 1 : 0;
	m_IOP1 = (values[EFLAGS].Reg16 & 0x1000) ? 1 : 0;
	m_IOP2 = (values[EFLAGS].Reg16 & 0x2000) ? 1 : 0;
	m_NT = (values[EFLAGS].Reg16 & 0x4000) ? 1 : 0;
	m_VM = (values[EFLAGS].Reg32 & 0x20000) ? 1 : 0;
}

static void i386_set_a20_line(int state)
{
	DWORD bytes;
	UINT8 *addr;
	if(state)
	{
		m_a20_mask = 0xffffffff;
		addr = mem + 0x100000;
	}
	else
	{
		m_a20_mask = 0xffefffff;
		addr = mem;
	}
	if (WHvMapGpaRange(part, addr, 0x100000, 0x100000,
				WHvMapGpaRangeFlagRead | WHvMapGpaRangeFlagWrite | WHvMapGpaRangeFlagExecute))
		WHPXVM_ERRF("SET_RAM");
}

static void int_inject(int vector)
{
	const WHV_REGISTER_NAME reg = WHvRegisterPendingInterruption;
	WHV_REGISTER_VALUE extint = {0};
	extint.PendingInterruption.InterruptionType = WHvX64PendingInterrupt;
	extint.PendingInterruption.InterruptionPending = 1;
	extint.PendingInterruption.InterruptionVector = vector;
	WHvSetVirtualProcessorRegisters(part, 0, &reg, 1, &extint);
}

static void req_int()
{
	const WHV_REGISTER_NAME reg = WHvX64RegisterDeliverabilityNotifications;
	WHV_REGISTER_VALUE delnot = {0};
	delnot.DeliverabilityNotifications.InterruptNotification = 1;
	WHvSetVirtualProcessorRegisters(part, 0, &reg, 1, &delnot);
}

static void i386_trap(int irq, int irq_gate, int trap_level)
{
	if(exitctxt.VpContext.Rflags & 0x200)
		int_inject(irq);
	else
	{
		saved_vector = irq;
		req_int();
	}
}

static void i386_set_irq_line(int irqline, int state)
{
	if(!state)
	{
		if (saved_vector == -2)
			saved_vector = -1;
		return;
	}
	if(exitctxt.VpContext.Rflags & 0x200)
		int_inject(pic_ack());
	else
	{
		saved_vector = -2;
		req_int();
	}
}

static void cpu_execute_whpx()
{
	DWORD bytes;
	values[EFLAGS].Reg32 = (values[EFLAGS].Reg32) | (m_VM << 17) | (m_NT << 14) | (m_IOP2 << 13) | (m_IOP1 << 12)
		| (m_IF << 9) | (m_SF << 7) | (m_ZF << 6) | m_CF;
	if(WHvSetVirtualProcessorRegisters(part, 0, whpx_register_names, RTL_NUMBER_OF(whpx_register_names), values))
		WHPXVM_ERRF("SET_REGS");
	while (TRUE)
	{
		cpu_running = true;
		if (WHvRunVirtualProcessor(part, 0, (void *)&exitctxt, sizeof(WHV_RUN_VP_EXIT_CONTEXT)))
			return;
		cpu_running = false;

		switch(exitctxt.ExitReason)
		{
			case WHvRunVpExitReasonX64IoPortAccess:
			{
				WHV_X64_IO_PORT_ACCESS_CONTEXT *io = &exitctxt.IoPortAccess;
				WHV_REGISTER_VALUE vals[] = {{0}, {0}, {0}};
				const WHV_REGISTER_NAME regs[] = {WHvX64RegisterRip, WHvX64RegisterRax, WHvX64RegisterRflags};
#ifdef EXPORT_DEBUG_TO_FILE
				values[EIP].Reg64 = exitctxt.VpContext.Rip;
				values[CS].Segment = exitctxt.VpContext.Cs;
#endif
				vals[0].Reg32 = exitctxt.VpContext.Rip + exitctxt.VpContext.InstructionLength;
				vals[1].Reg64 = io->Rax;
				vals[2].Reg64 = exitctxt.VpContext.Rflags;
				if(io->PortNumber == 0xf7)
				{
					Sleep(0);
					values[EAX].Reg64 = io->Rax;
					m_SF = (exitctxt.VpContext.Rflags & 0x80) ? 1 : 0;
					write_io_byte(0xf7, 0);
					vals[1].Reg64 = values[EAX].Reg64;
					vals[2].Reg64 = (exitctxt.VpContext.Rflags & ~0x80) | (m_SF << 7);
				}
				else if(!(io->AccessInfo.StringOp))
				{
					switch(io->AccessInfo.AccessSize)
					{
						case 1:
							if(!io->AccessInfo.IsWrite)
								vals[1].Reg64 = (io->Rax & ~0xff) | read_io_byte(io->PortNumber);
							else
								write_io_byte(io->PortNumber, io->Rax);
							break;
						case 2:
							if(!io->AccessInfo.IsWrite)
								vals[1].Reg64 = (io->Rax & ~0xffff) | read_io_word(io->PortNumber);
							else
								write_io_word(io->PortNumber, io->Rax);
							break;
						case 4:
							if(!io->AccessInfo.IsWrite)
								vals[1].Reg64 = read_io_dword(io->PortNumber);
							else
								write_io_dword(io->PortNumber, io->Rax);
							break;
					}
				}
				else
				{
					UINT32 count = io->AccessInfo.RepPrefix ? io->Rcx : 1;
					UINT8 *addr;
					if(!io->AccessInfo.IsWrite)
						addr = mem + io->Ds.Base + io->Rsi;
					else
						addr = mem + io->Es.Base + io->Rdi;
					for(int i = 0; i < count; i++)
					{
						addr = exitctxt.VpContext.Rflags & 0x400 ? addr - io->AccessInfo.AccessSize : addr + io->AccessInfo.AccessSize;
						switch(io->AccessInfo.AccessSize)
						{
							case 1:
								if(!io->AccessInfo.IsWrite)
									write_io_byte(io->PortNumber, *addr);
								else
									*addr = read_io_byte(io->PortNumber);
								break;
							case 2:
								if(!io->AccessInfo.IsWrite)
									write_io_word(io->PortNumber, *(UINT16 *)addr);
								else
									*(UINT16 *)addr = read_io_word(io->PortNumber);
								break;
							case 4:
								if(!io->AccessInfo.IsWrite)
									write_io_dword(io->PortNumber, *(UINT32 *)addr);
								else
									*(UINT32 *)addr = read_io_dword(io->PortNumber);
								break;
						}
					}
				}
#ifdef EXPORT_DEBUG_TO_FILE
				fflush(fp_debug_log);
#endif
				WHvSetVirtualProcessorRegisters(part, 0, regs, 3, vals);
				continue;
			}
			case WHvRunVpExitReasonMemoryAccess:
				WHvGetVirtualProcessorRegisters(part, 0, whpx_register_names, RTL_NUMBER_OF(whpx_register_names), values);
				instr_emu(0);
				WHvSetVirtualProcessorRegisters(part, 0, whpx_register_names, RTL_NUMBER_OF(whpx_register_names), values);
				continue;
			case WHvRunVpExitReasonX64Halt:
			{
				offs_t hltaddr = exitctxt.VpContext.Cs.Base + exitctxt.VpContext.Rip - 1;
				translate_address(0, TRANSLATE_READ, &hltaddr, NULL);
				if((hltaddr >= IRET_TOP) && (hltaddr < (IRET_TOP + IRET_SIZE)))
				{
					int syscall = hltaddr - IRET_TOP;
					WHvGetVirtualProcessorRegisters(part, 0, whpx_register_names, RTL_NUMBER_OF(whpx_register_names), values);
					i386_iret16();
					msdos_syscall(syscall);
#ifdef EXPORT_DEBUG_TO_FILE
					fflush(fp_debug_log);
#endif
					values[EFLAGS].Reg32 = (values[EFLAGS].Reg32 & ~0x272c1) | (m_VM << 17) | (m_NT << 14) |
						(m_IOP2 << 13) | (m_IOP1 << 12) | (m_IF << 9) | (m_SF << 7) | (m_ZF << 6) | m_CF;
					if(WHvSetVirtualProcessorRegisters(part, 0, whpx_register_names, RTL_NUMBER_OF(whpx_register_names), values))
						WHPXVM_ERRF("SET_REGS");
					continue;
				}
				else if(hltaddr == 0xffff0)
				{
					m_exit = 1;
					return;
				}
				else if((hltaddr >= DUMMY_TOP) && (hltaddr < 0xffff0))
					return;
				else 
					whpxvm_panic("handle hlt");
			}
			case WHvRunVpExitReasonX64InterruptWindow:
				if(saved_vector == -1)
					continue;
				int_inject(saved_vector == -2 ? pic_ack() : saved_vector);
				saved_vector = -1;
				continue;
			case WHvRunVpExitReasonCanceled:
				if(exitctxt.CancelReason.CancelReason != WhvRunVpCancelReasonUser)
					whpxvm_panic("execution canceled");
				hardware_update();
				continue;
			default:
				WHPXVM_ERRF("exit status: %x %04x:%08x", exitctxt.ExitReason, exitctxt.VpContext.Cs.Selector, (UINT32)exitctxt.VpContext.Rip);
				return;
		}
	}
}

// simple x86 emulation for vga mmio
// does no privilege checks or complex instructions

/*
 * (C) Copyright 1992, ..., 2014 the "DOSEMU-Development-Team".
 *
 * for details see file COPYING in the DOSEMU distribution
 */

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * some configurable options
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

/*
 * Debug level for the Graphics Controller.
 * 0 - normal / 1 - useful / 2 - too much
 */
/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
#define COUNT  150	/* bail out when this #instructions were simulated
			   after a VGA r/w access */

#define R_WORD(a) (*((unsigned short *) &(a)))
#define R_DWORD(a) (*((unsigned *) &(a)))
#define OP_JCC(cond) eip += (cond) ? 2 + *(signed char *)MEM_BASE32(cs + eip + 1) : 2; break;
#define OP_JCCW(cond) eip += (x86->operand_size == 2) ? ((cond) ? 4 + *(signed short *)MEM_BASE32(cs + eip + 2) : 4) : \
				((cond) ? 6 + *(signed *)MEM_BASE32(cs + eip + 2) : 6); break;

#define CF  (1 <<  0)
#define PF  (1 <<  2)
#define AF  (1 <<  4)
#define ZF  (1 <<  6)
#define SF  (1 <<  7)
#define IF  (1 <<  9)
#define DF  (1 << 10)
#define OF  (1 << 11)

static unsigned char *MEM_BASE32(UINT32 address)
{
	translate_address(0, TRANSLATE_READ, &address, NULL); // TODO: check for page fault
	return mem + address;
}

typedef UINT32 dosaddr_t;
#define DOSADDR_REL(x) (x - mem)

/* assembly macros to speed up x86 on x86 emulation: the cpu helps us in setting
   the flags */

#define OPandFLAG0(eflags, insn, op1, istype) __asm__ __volatile__("\n\
		"#insn"	%0\n\
		pushf; pop	%1\n \
		" : #istype (op1), "=g" (eflags) : "0" (op1));

#define OPandFLAG1(eflags, insn, op1, istype) __asm__ __volatile__("\n\
		"#insn"	%0, %0\n\
		pushf; pop	%1\n \
		" : #istype (op1), "=g" (eflags) : "0" (op1));

#define OPandFLAG(eflags, insn, op1, op2, istype, type) __asm__ __volatile__("\n\
		"#insn"	%3, %0\n\
		pushf; pop	%1\n \
		" : #istype (op1), "=g" (eflags) : "0" (op1), #type (op2));

#define OPandFLAGC(eflags, insn, op1, op2, istype, type) __asm__ __volatile__("\n\
		shr     $1, %0\n\
		"#insn" %4, %1\n\
		pushf; pop     %0\n \
		" : "=r" (eflags), #istype (op1)  : "0" (eflags), "1" (op1), #type (op2));


#if !defined True
#define False 0
#define True 1
#endif

#ifdef ENABLE_DEBUG_LOG
#define instr_deb(x...) fprintf(fp_debug_log, "instremu: " x)
#ifdef ENABLE_DEBUG_TRACE
#define ARRAY_LENGTH(x)     (sizeof(x) / sizeof(x[0]))
const UINT32 DASMFLAG_SUPPORTED     = 0x80000000;   // are disassembly flags supported?
const UINT32 DASMFLAG_STEP_OUT      = 0x40000000;   // this instruction should be the end of a step out sequence
const UINT32 DASMFLAG_STEP_OVER     = 0x20000000;   // this instruction should be stepped over by setting a breakpoint afterwards
const UINT32 DASMFLAG_OVERINSTMASK  = 0x18000000;   // number of extra instructions to skip when stepping over
const UINT32 DASMFLAG_OVERINSTSHIFT = 27;           // bits to shift after masking to get the value
const UINT32 DASMFLAG_LENGTHMASK    = 0x0000ffff;   // the low 16-bits contain the actual length
#include "mame/emu/cpu/i386/i386dasm.c"
#define instr_deb2(x...) fprintf(fp_debug_log, "instremu: " x)
#else
#define instr_deb2(x...)
#endif
#else
#define instr_deb(x...)
#define instr_deb2(x...)
#endif

enum {REPNZ = 0, REPZ = 1, REP_NONE = 2};

typedef struct x86_emustate {
	unsigned seg_base, seg_ss_base;
	unsigned address_size; /* in bytes so either 4 or 2 */
	unsigned operand_size;
	unsigned prefixes, rep;
	unsigned (*instr_binary)(unsigned op, unsigned op1,
			unsigned op2, UINT32 *eflags);
	unsigned (*instr_read)(const unsigned char *addr);
	void (*instr_write)(unsigned char *addr, unsigned u);
	unsigned char *(*modrm)(unsigned char *cp, x86_emustate *x86, int *inst_len);
} x86_emustate;

#ifdef ENABLE_DEBUG_LOG
static char *seg_txt[7] = { "", "es: ", "cs: ", "ss: ", "ds: ", "fs: ", "gs: " };
static char *rep_txt[3] = { "", "repnz ", "repz " };
static char *lock_txt[2] = { "", "lock " };
#endif

static unsigned wordmask[5] = {0,0xff,0xffff,0xffffff,0xffffffff};

static unsigned char it[0x100] = {
	7, 7, 7, 7, 2, 3, 1, 1,    7, 7, 7, 7, 2, 3, 1, 0,
	7, 7, 7, 7, 2, 3, 1, 1,    7, 7, 7, 7, 2, 3, 1, 1,
	7, 7, 7, 7, 2, 3, 0, 1,    7, 7, 7, 7, 2, 3, 0, 1,
	7, 7, 7, 7, 2, 3, 0, 1,    7, 7, 7, 7, 2, 3, 0, 1,

	1, 1, 1, 1, 1, 1, 1, 1,    1, 1, 1, 1, 1, 1, 1, 1,
	1, 1, 1, 1, 1, 1, 1, 1,    1, 1, 1, 1, 1, 1, 1, 1,
	1, 1, 7, 7, 0, 0, 0, 0,    3, 9, 2, 8, 1, 1, 1, 1,
	2, 2, 2, 2, 2, 2, 2, 2,    2, 2, 2, 2, 2, 2, 2, 2,

	8, 9, 8, 8, 7, 7, 7, 7,    7, 7, 7, 7, 7, 7, 7, 7,
	1, 1, 1, 1, 1, 1, 1, 1,    1, 1, 6, 1, 1, 1, 1, 1,
	4, 4, 4, 4, 1, 1, 1, 1,    2, 3, 1, 1, 1, 1, 1, 1,
	2, 2, 2, 2, 2, 2, 2, 2,    3, 3, 3, 3, 3, 3, 3, 3,

	8, 8, 3, 1, 7, 7, 8, 9,    5, 1, 3, 1, 1, 2, 1, 1,
	7, 7, 7, 7, 2, 2, 1, 1,    0, 0, 0, 0, 0, 0, 0, 0,
	2, 2, 2, 2, 2, 2, 2, 2,    4, 4, 6, 2, 1, 1, 1, 1,
	0, 1, 0, 0, 1, 1, 7, 7,    1, 1, 1, 1, 1, 1, 7, 7
};

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
static unsigned seg, lock, rep;
static int count;
#define vga_base VGA_VRAM_TOP
#define vga_end VGA_VRAM_LAST + 1

static unsigned arg_len(unsigned char *, int);

static unsigned char instr_read_byte(const unsigned char *addr);
static unsigned instr_read_word(const unsigned char *addr);
static unsigned instr_read_dword(const unsigned char *addr);
static void instr_write_byte(unsigned char *addr, unsigned char u);
static void instr_write_word(unsigned char *addr, unsigned u);
static void instr_write_dword(unsigned char *addr, unsigned u);
static void instr_flags(unsigned val, unsigned smask, UINT32 *eflags);
static unsigned instr_shift(unsigned op, int op1, unsigned op2, unsigned size, UINT32 *eflags);
static unsigned char *sib(unsigned char *cp, x86_emustate *x86, int *inst_len);
static unsigned char *modrm32(unsigned char *cp, x86_emustate *x86, int *inst_len);
static unsigned char *modrm16(unsigned char *cp, x86_emustate *x86, int *inst_len);

static void dump_x86_regs()
{
	instr_deb(
			"eax=%08x ebx=%08x ecx=%08x edx=%08x esi=%08x edi=%08x ebp=%08x esp=%08x\n",
			values[EAX].Reg32, values[EBX].Reg32, values[ECX].Reg32, values[EDX].Reg32, values[ESI].Reg32, values[EDI].Reg32, values[EBP].Reg32, values[ESP].Reg32
		 );
	instr_deb(
			"eip=%08x cs=%04x/%08x ds=%04x/%08x es=%04x/%08x d=%u c=%u p=%u a=%u z=%u s=%u o=%u\n",
			values[EIP].Reg32, values[CS].Segment.Selector, (UINT32)values[CS].Segment.Base, values[DS].Segment.Selector,
			(UINT32)values[DS].Segment.Base, values[ES].Segment.Selector, (UINT32)values[ES].Segment.Base,
			(values[EFLAGS].Reg32&DF)>>10,
			values[EFLAGS].Reg32&CF,(values[EFLAGS].Reg32&PF)>>2,(values[EFLAGS].Reg32&AF)>>4,
			(values[EFLAGS].Reg32&ZF)>>6,(values[EFLAGS].Reg32&SF)>>7,(values[EFLAGS].Reg32&OF)>>11
		 );
}

static int instr_len(unsigned char *p, int is_32)
{
	unsigned u, osp, asp;
	unsigned char *p0 = p;
	int type;
#ifdef ENABLE_DEBUG_LOG
	unsigned char *p1 = p;
#endif

	seg = lock = rep = 0;
	osp = asp = is_32;

	for(u = 1; u && p - p0 < 17;) switch(*p++) {		/* get prefixes */
		case 0x26:	/* es: */
			seg = 1; break;
		case 0x2e:	/* cs: */
			seg = 2; break;
		case 0x36:	/* ss: */
			seg = 3; break;
		case 0x3e:	/* ds: */
			seg = 4; break;
		case 0x64:	/* fs: */
			seg = 5; break;
		case 0x65:	/* gs: */
			seg = 6; break;
		case 0x66:	/* operand size */
			osp ^= 1; break;
		case 0x67:	/* address size */
			asp ^= 1; break;
		case 0xf0:	/* lock */
			lock = 1; break;
		case 0xf2:	/* repnz */
			rep = 2; break;
		case 0xf3:	/* rep(z) */
			rep = 1; break;
		default:	/* no prefix */
			u = 0;
	}
	p--;

#ifdef ENABLE_DEBUG_LOG
	p1 = p;
#endif

	if(p - p0 >= 16) return 0;

	if(*p == 0x0f) {
		p++;
		switch (*p) {
			case 0xa4:
			case 0xac:
				type = 8;
				break;
			case 0xaf:
			case 0xb6:
			case 0xbe:
			case 0xbf:
				type = 7;
				break;
			case 0xba:
				p += 4;
				return p - p0;
			default:
				/* not yet */
				instr_deb("unsupported instr_len %x %x\n", p[0], p[1]);
				return 0;
		}
	}
	else
		type = it[*p];

	switch(type) {
		case 1:	/* op-code */
			p += 1; break;

		case 2:	/* op-code + byte */
			p += 2; break;

		case 3:	/* op-code + word/dword */
			p += osp ? 5 : 3; break;

		case 4:	/* op-code + [word/dword] */
			p += asp ? 5 : 3; break;

		case 5:	/* op-code + word/dword + byte */
			p += osp ? 6 : 4; break;

		case 6:	/* op-code + [word/dword] + word */
			p += asp ? 7 : 5; break;

		case 7:	/* op-code + mod + ... */
			p++;
			p += (u = arg_len(p, asp));
			if(!u) p = p0;
			break;

		case 8:	/* op-code + mod + ... + byte */
			p++;
			p += (u = arg_len(p, asp)) + 1;
			if(!u) p = p0;
			break;

		case 9:	/* op-code + mod + ... + word/dword */
			p++;
			p += (u = arg_len(p, asp)) + (osp ? 4 : 2);
			if(!u) p = p0;
			break;

		default:
			p = p0;
	}

#ifdef ENABLE_DEBUG_LOG
	if(p >= p0) {
		instr_deb("instr_len: instr = ");
		fprintf(fp_debug_log, "%s%s%s%s%s",
				osp ? "osp " : "", asp ? "asp " : "",
				lock_txt[lock], rep_txt[rep], seg_txt[seg]
			);
		if(p > p1) for(u = 0; u < p - p1; u++) {
			fprintf(fp_debug_log, "%02x ", p1[u]);
		}
		fprintf(fp_debug_log, "\n");
	}
#endif

	return p - p0;
}


static unsigned arg_len(unsigned char *p, int asp)
{
	unsigned u = 0, m, s = 0;

	m = *p & 0xc7;
	if(asp) {
		if(m == 5) {
			u = 5;
		}
		else {
			if((m >> 6) < 3 && (m & 7) == 4) s = 1;
			switch(m >> 6) {
				case 1:
					u = 2; break;
				case 2:
					u = 5; break;
				default:
					u = 1;
			}
			u += s;
		}
	}
	else {
		if(m == 6)
			u = 3;
		else
			switch(m >> 6) {
				case 1:
					u = 2; break;
				case 2:
					u = 3; break;
				default:
					u = 1;
			}
	}

	instr_deb2("arg_len: %02x %02x %02x %02x: %u bytes\n", p[0], p[1], p[2], p[3], u);

	return u;
}

/*
 * Some functions to make using the vga emulation easier.
 *
 *
 */

static unsigned char instr_read_byte(const unsigned char *address)
{
	unsigned char u;
	dosaddr_t addr = DOSADDR_REL(address);

	if(addr >= vga_base && addr < vga_end) {
		count = COUNT;
		u = read_byte(addr);
	}
	else {
		u = *address;
	}
#ifdef ENABLE_DEBUG_TRACE
	instr_deb2("Read byte 0x%x", u);
	if (addr<0x8000000) fprintf(fp_debug_log, " from address %x\n", addr); else fprintf(fp_debug_log, "\n");
#endif

	return u;
}

static unsigned instr_read_word(const unsigned char *address)
{
	unsigned u;

	/*
	 * segment wrap-arounds within a data word are not allowed since
	 * at least i286, so no problems here
	 */
	dosaddr_t addr = DOSADDR_REL(address);
	if(addr >= vga_base && addr < vga_end) {
		count = COUNT;
		u = read_word(addr);
	} else
		u = *(unsigned short *)address;

#ifdef ENABLE_DEBUG_TRACE
	instr_deb2("Read word 0x%x", u);
	if (addr<0x8000000) fprintf(fp_debug_log, " from address %x\n", addr); else fprintf(fp_debug_log, "\n");
#endif
	return u;
}

static unsigned instr_read_dword(const unsigned char *address)
{
	unsigned u;

	/*
	 * segment wrap-arounds within a data word are not allowed since
	 * at least i286, so no problems here
	 */
	dosaddr_t addr = DOSADDR_REL(address);
	if(addr >= vga_base && addr < vga_end) {
		count = COUNT;
		u = read_dword(addr);
	} else
		u = *(unsigned *)address;

#ifdef ENABLE_DEBUG_TRACE
	instr_deb2("Read word 0x%x", u);
	if (addr<0x8000000) fprintf(fp_debug_log, " from address %x\n", addr); else fprintf(fp_debug_log, "\n");
#endif
	return u;
}

static void instr_write_byte(unsigned char *address, unsigned char u)
{
	dosaddr_t addr = DOSADDR_REL(address);

	if(addr >= vga_base && addr < vga_end) {
		count = COUNT;
		write_byte(addr, u);
	}
	else {
		*address = u;
	}
#ifdef ENABLE_DEBUG_TRACE
	instr_deb2("Write byte 0x%x", u);
	if (addr<0x8000000) fprintf(fp_debug_log, " at address %x\n", addr); else fprintf(fp_debug_log, "\n");
#endif
}

static void instr_write_word(unsigned char *address, unsigned u)
{
	dosaddr_t dst = DOSADDR_REL(address);
	/*
	 * segment wrap-arounds within a data word are not allowed since
	 * at least i286, so no problems here.
	 * we assume application do not try to mix here
	 */

	if(dst >= vga_base && dst < vga_end) {
		count = COUNT;
		write_word(dst, u);
	}
	else
		*(unsigned short *)address = u;

#ifdef ENABLE_DEBUG_TRACE
	instr_deb2("Write word 0x%x", u);
	if (dst<0x8000000) fprintf(fp_debug_log, " at address %x\n", dst); else fprintf(fp_debug_log, "\n");
#endif
}

static void instr_write_dword(unsigned char *address, unsigned u)
{
	dosaddr_t dst = DOSADDR_REL(address);

	/*
	 * segment wrap-arounds within a data word are not allowed since
	 * at least i286, so no problems here.
	 * we assume application do not try to mix here
	 */

	if(dst >= vga_base && dst < vga_end) {
		count = COUNT;
		write_dword(dst, u);
	}
	else
		*(unsigned *)address = u;

#ifdef ENABLE_DEBUG_TRACE
	instr_deb2("Write word 0x%x", u);
	if (dst<0x8000000) fprintf(fp_debug_log, " at address %x\n", dst); else fprintf(fp_debug_log, "\n");
#endif
}

/* We use the cpu itself to set the flags, which is easy since we are
   emulating x86 on x86. */
static void instr_flags(unsigned val, unsigned smask, UINT32 *eflags)
{
	uintptr_t flags;

	*eflags &= ~(OF|ZF|SF|PF|CF);
	if (val & smask)
		*eflags |= SF;
	OPandFLAG1(flags, orl, val, =r);
	*eflags |= flags & (ZF|PF);
}

/* 6 logical and arithmetic "RISC" core functions
   follow
   */
static unsigned char instr_binary_byte(unsigned char op, unsigned char op1, unsigned char op2, UINT32 *eflags)
{
	uintptr_t flags;

	switch (op&0x7){
		case 1: /* or */
			OPandFLAG(flags, orb, op1, op2, =q, q);
			*eflags = (*eflags & ~(OF|ZF|SF|PF|CF)) | (flags & (OF|ZF|SF|PF|CF));
			return op1;
		case 4: /* and */
			OPandFLAG(flags, andb, op1, op2, =q, q);
			*eflags = (*eflags & ~(OF|ZF|SF|PF|CF)) | (flags & (OF|ZF|SF|PF|CF));
			return op1;
		case 6: /* xor */
			OPandFLAG(flags, xorb, op1, op2, =q, q);
			*eflags = (*eflags & ~(OF|ZF|SF|PF|CF)) | (flags & (OF|ZF|SF|PF|CF));
			return op1;
		case 0: /* add */
			*eflags &= ~CF; /* Fall through */
		case 2: /* adc */
			flags = *eflags;
			OPandFLAGC(flags, adcb, op1, op2, =q, q);
			*eflags = (*eflags & ~(OF|ZF|SF|AF|PF|CF)) | (flags & (OF|ZF|AF|SF|PF|CF));
			return op1;
		case 5: /* sub */
		case 7: /* cmp */
			*eflags &= ~CF; /* Fall through */
		case 3: /* sbb */
			flags = *eflags;
			OPandFLAGC(flags, sbbb, op1, op2, =q, q);
			*eflags = (*eflags & ~(OF|ZF|SF|AF|PF|CF)) | (flags & (OF|ZF|AF|SF|PF|CF));
			return op1;
	}
	return 0;
}

static unsigned instr_binary_word(unsigned op, unsigned op1, unsigned op2, UINT32 *eflags)
{
	uintptr_t flags;
	unsigned short opw1 = op1;
	unsigned short opw2 = op2;

	switch (op&0x7){
		case 1: /* or */
			OPandFLAG(flags, orw, opw1, opw2, =r, r);
			*eflags = (*eflags & ~(OF|ZF|SF|PF|CF)) | (flags & (OF|ZF|SF|PF|CF));
			return opw1;
		case 4: /* and */
			OPandFLAG(flags, andw, opw1, opw2, =r, r);
			*eflags = (*eflags & ~(OF|ZF|SF|PF|CF)) | (flags & (OF|ZF|SF|PF|CF));
			return opw1;
		case 6: /* xor */
			OPandFLAG(flags, xorw, opw1, opw2, =r, r);
			*eflags = (*eflags & ~(OF|ZF|SF|PF|CF)) | (flags & (OF|ZF|SF|PF|CF));
			return opw1;
		case 0: /* add */
			*eflags &= ~CF; /* Fall through */
		case 2: /* adc */
			flags = *eflags;
			OPandFLAGC(flags, adcw, opw1, opw2, =r, r);
			*eflags = (*eflags & ~(OF|ZF|SF|AF|PF|CF)) | (flags & (OF|ZF|AF|SF|PF|CF));
			return opw1;
		case 5: /* sub */
		case 7: /* cmp */
			*eflags &= ~CF; /* Fall through */
		case 3: /* sbb */
			flags = *eflags;
			OPandFLAGC(flags, sbbw, opw1, opw2, =r, r);
			*eflags = (*eflags & ~(OF|ZF|SF|AF|PF|CF)) | (flags & (OF|ZF|AF|SF|PF|CF));
			return opw1;
	}
	return 0;
}

static unsigned instr_binary_dword(unsigned op, unsigned op1, unsigned op2, UINT32 *eflags)
{
	uintptr_t flags;

	switch (op&0x7){
		case 1: /* or */
			OPandFLAG(flags, orl, op1, op2, =r, r);
			*eflags = (*eflags & ~(OF|ZF|SF|PF|CF)) | (flags & (OF|ZF|SF|PF|CF));
			return op1;
		case 4: /* and */
			OPandFLAG(flags, andl, op1, op2, =r, r);
			*eflags = (*eflags & ~(OF|ZF|SF|PF|CF)) | (flags & (OF|ZF|SF|PF|CF));
			return op1;
		case 6: /* xor */
			OPandFLAG(flags, xorl, op1, op2, =r, r);
			*eflags = (*eflags & ~(OF|ZF|SF|PF|CF)) | (flags & (OF|ZF|SF|PF|CF));
			return op1;
		case 0: /* add */
			*eflags &= ~CF; /* Fall through */
		case 2: /* adc */
			flags = *eflags;
			OPandFLAGC(flags, adcl, op1, op2, =r, r);
			*eflags = (*eflags & ~(OF|ZF|SF|AF|PF|CF)) | (flags & (OF|ZF|AF|SF|PF|CF));
			return op1;
		case 5: /* sub */
		case 7: /* cmp */
			*eflags &= ~CF; /* Fall through */
		case 3: /* sbb */
			flags = *eflags;
			OPandFLAGC(flags, sbbl, op1, op2, =r, r);
			*eflags = (*eflags & ~(OF|ZF|SF|AF|PF|CF)) | (flags & (OF|ZF|AF|SF|PF|CF));
			return op1;
	}
	return 0;
}

static unsigned instr_shift(unsigned op, int op1, unsigned op2, unsigned size, UINT32 *eflags)
{
	unsigned result, carry;
	unsigned width = size * 8;
	unsigned mask = wordmask[size];
	unsigned smask = (mask >> 1) + 1;
	op2 &= 31;

	switch (op&0x7){
		case 0: /* rol */
			op2 &= width-1;
			result = (((op1 << op2) | ((op1&mask) >> (width-op2)))) & mask;
			*eflags &= ~(CF|OF);
			*eflags |= (result & CF) | ((((result >> (width-1)) ^ result) << 11) & OF);
			return result;
		case 1:/* ror */
			op2 &= width-1;
			result = ((((op1&mask) >> op2) | (op1 << (width-op2)))) & mask;
			*eflags &= ~(CF|OF);
			carry = (result >> (width-1)) & CF;
			*eflags |=  carry |
				(((carry ^ (result >> (width-2))) << 11) & OF);
			return result;
		case 2: /* rcl */
			op2 %= width+1;
			carry = (op1>>(width-op2))&CF;
			result = (((op1 << op2) | ((op1&mask) >> (width+1-op2))) | ((*eflags&CF) << (op2-1))) & mask;
			*eflags &= ~(CF|OF);
			*eflags |= carry | ((((result >> (width-1)) ^ carry) << 11) & OF);
			return result;
		case 3:/* rcr */
			op2 %= width+1;
			carry = (op1>>(op2-1))&CF;
			result = ((((op1&mask) >> op2) | (op1 << (width+1-op2))) | ((*eflags&CF) << (width-op2))) & mask;
			*eflags &= ~(CF|OF);
			*eflags |= carry | ((((result >> (width-1)) ^ (result >> (width-2))) << 11) & OF);
			return result;
		case 4: /* shl */
			result = (op1 << op2) & mask;
			instr_flags(result, smask, eflags);
			*eflags &= ~(CF|OF);
			*eflags |= ((op1 >> (width-op2))&CF) |
				((((op1 >> (width-1)) ^ (op1 >> (width-2))) << 11) & OF);
			return result;
		case 5: /* shr */
			result = ((unsigned)(op1&mask) >> op2);
			instr_flags(result, smask, eflags);
			*eflags &= ~(CF|OF);
			*eflags |= ((op1 >> (op2-1)) & CF) | (((op1 >> (width-1)) << 11) & OF);
			return result;
		case 7: /* sar */
			result = op1 >> op2;
			instr_flags(result, smask, eflags);
			*eflags &= ~(CF|OF);
			*eflags |= (op1 >> (op2-1)) & CF;
			return result;
	}
	return 0;
}

static inline void push(unsigned val, x86_emustate *x86)
{
	if (values[SS].Segment.Default)
		values[ESP].Reg32 -= x86->operand_size;
	else
		values[ESP].Reg16 -= x86->operand_size;
	i386_write_stack(val, x86->operand_size == 4);
}

static inline void pop(unsigned *val, x86_emustate *x86)
{
	*val = i386_read_stack(x86->operand_size == 4);
	if (values[SS].Segment.Default)
		values[ESP].Reg32 += x86->operand_size;
	else
		values[ESP].Reg16 += x86->operand_size;
}

/* helper functions/macros reg8/reg/sreg/sib/modrm16/32 for instr_sim
   for address and register decoding */
enum { es_INDEX, cs_INDEX, ss_INDEX, ds_INDEX, fs_INDEX, gs_INDEX };

#define reg8(reg) ((UINT8 *)&values[reg & 3].Reg32 + ((reg & 4) ? 1 : 0))
#define reg(reg) ((UINT32 *)&values[reg & 7].Reg32)
#define sreg_idx(reg) (es_INDEX+((reg)&0x7))

static UINT16 *sreg(int reg)
{
	WHV_X64_SEGMENT_REGISTER *sreg;
	switch(reg & 7)
	{
		case es_INDEX:
			sreg = &values[ES].Segment;
			break;
		case cs_INDEX:
			sreg = &values[CS].Segment;
			break;
		case ss_INDEX:
			sreg = &values[SS].Segment;
			break;
		case ds_INDEX:
			sreg = &values[DS].Segment;
			break;
		case fs_INDEX:
			sreg = &values[FS].Segment;
			break;
		case gs_INDEX:
			sreg = &values[GS].Segment;
			break;
		default:
			return 0;
	}
	return &sreg->Selector;
}

static unsigned char *sib(unsigned char *cp, x86_emustate *x86, int *inst_len)
{
	unsigned addr = 0;

	switch(cp[1] & 0xc0) { /* decode modifier */
		case 0x40:
			addr = (int)(signed char)cp[3];
			break;
		case 0x80:
			addr = R_DWORD(cp[3]);
			break;
	}

	if ((cp[2] & 0x38) != 0x20) /* index cannot be esp */
		addr += *reg(cp[2]>>3) << (cp[2] >> 6);

	switch(cp[2] & 0x07) { /* decode address */
		case 0x00:
		case 0x01:
		case 0x02:
		case 0x03:
		case 0x06:
		case 0x07:
			return MEM_BASE32(addr + *reg(cp[2]) + x86->seg_base);
		case 0x04: /* esp */
			return MEM_BASE32(addr + values[ESP].Reg32 + x86->seg_ss_base);
		case 0x05:
			if (cp[1] >= 0x40)
				return MEM_BASE32(addr + values[EBP].Reg32 + x86->seg_ss_base);
			else {
				*inst_len += 4;
				return MEM_BASE32(addr + R_DWORD(cp[3]) + x86->seg_base);
			}
	}
	return 0; /* keep gcc happy */
}

static unsigned char *modrm16(unsigned char *cp, x86_emustate *x86, int *inst_len)
{
	unsigned addr = 0;
	*inst_len = 0;

	switch(cp[1] & 0xc0) { /* decode modifier */
		case 0x40:
			addr = (short)(signed char)cp[2];
			*inst_len = 1;
			break;
		case 0x80:
			addr = R_WORD(cp[2]);
			*inst_len = 2;
			break;
		case 0xc0:
			if (cp[0]&1) /*(d)word*/
				return (unsigned char *)reg(cp[1]);
			else
				return reg8(cp[1]);
	}


	switch(cp[1] & 0x07) { /* decode address */
		case 0x00:
			return MEM_BASE32(((addr + values[EBX].Reg32 + values[ESI].Reg32) & 0xffff) + x86->seg_base);
		case 0x01:
			return MEM_BASE32(((addr + values[EBX].Reg32 + values[EDI].Reg32) & 0xffff) + x86->seg_base);
		case 0x02:
			return MEM_BASE32(((addr + values[EBP].Reg32 + values[ESI].Reg32) & 0xffff) + x86->seg_ss_base);
		case 0x03:
			return MEM_BASE32(((addr + values[EBP].Reg32 + values[EDI].Reg32) & 0xffff) + x86->seg_ss_base);
		case 0x04:
			return MEM_BASE32(((addr + values[ESI].Reg32) & 0xffff) + x86->seg_base);
		case 0x05:
			return MEM_BASE32(((addr + values[EDI].Reg32) & 0xffff) + x86->seg_base);
		case 0x06:
			if (cp[1] >= 0x40)
				return MEM_BASE32(((addr + values[EBP].Reg32) & 0xffff) + x86->seg_ss_base);
			else {
				*inst_len += 2;
				return MEM_BASE32(R_WORD(cp[2]) + x86->seg_base);
			}
		case 0x07:
			return MEM_BASE32(((addr + values[EBX].Reg32) & 0xffff) + x86->seg_base);
	}
	return 0; /* keep gcc happy */
}

static unsigned char *modrm32(unsigned char *cp, x86_emustate *x86, int *inst_len)
{
	unsigned addr = 0;
	*inst_len = 0;

	switch(cp[1] & 0xc0) { /* decode modifier */
		case 0x40:
			addr = (int)(signed char)cp[2];
			*inst_len = 1;
			break;
		case 0x80:
			addr = R_DWORD(cp[2]);
			*inst_len = 4;
			break;
		case 0xc0:
			if (cp[0]&1) /*(d)word*/
				return ((unsigned char *)reg(cp[1]));
			else
				return reg8(cp[1]);
	}
	switch(cp[1] & 0x07) { /* decode address */
		case 0x00:
		case 0x01:
		case 0x02:
		case 0x03:
		case 0x06:
		case 0x07:
			return MEM_BASE32(addr + *reg(cp[1]) + x86->seg_base);
		case 0x04: /* sib byte follows */
			*inst_len += 1;
			return sib(cp, x86, inst_len);
		case 0x05:
			if (cp[1] >= 0x40)
				return MEM_BASE32(addr + values[EBP].Reg32 + x86->seg_ss_base);
			else {
				*inst_len += 4;
				return MEM_BASE32(R_DWORD(cp[2]) + x86->seg_base);
			}
	}
	return 0; /* keep gcc happy */
}

static int handle_prefixes(x86_emustate *x86)
{
	unsigned eip = values[EIP].Reg32;
	int prefix = 0;

	for (;; eip++) {
		switch(*(unsigned char *)MEM_BASE32(values[CS].Segment.Base + eip)) {
			/* handle (some) prefixes */
			case 0x26:
				prefix++;
				x86->seg_base = x86->seg_ss_base = values[ES].Segment.Base;
				break;
			case 0x2e:
				prefix++;
				x86->seg_base = x86->seg_ss_base = values[CS].Segment.Base;
				break;
			case 0x36:
				prefix++;
				x86->seg_base = x86->seg_ss_base = values[SS].Segment.Base;
				break;
			case 0x3e:
				prefix++;
				x86->seg_base = x86->seg_ss_base = values[DS].Segment.Base;
				break;
			case 0x64:
				prefix++;
				x86->seg_base = x86->seg_ss_base = values[FS].Segment.Base;
				break;
			case 0x65:
				prefix++;
				x86->seg_base = x86->seg_ss_base = values[GS].Segment.Base;
				break;
			case 0x66:
				prefix++;
				x86->operand_size = 6 - x86->operand_size;
				if (x86->operand_size == 4) {
					x86->instr_binary = instr_binary_dword;
					x86->instr_read = instr_read_dword;
					x86->instr_write = instr_write_dword;
				} else {
					x86->instr_binary = instr_binary_word;
					x86->instr_read = instr_read_word;
					x86->instr_write = instr_write_word;
				}
				break;
			case 0x67:
				prefix++;
				x86->address_size = 6 - x86->address_size;
				x86->modrm = (x86->address_size == 4 ? modrm32 : modrm16);
				break;
			case 0xf2:
				prefix++;
				x86->rep = REPNZ;
				break;
			case 0xf3:
				prefix++;
				x86->rep = REPZ;
				break;
			default:
				return prefix;
		}
	}
	return prefix;
}

static void prepare_x86(x86_emustate *x86)
{
	x86->seg_base = values[DS].Segment.Base;
	x86->seg_ss_base = values[SS].Segment.Base;
	x86->address_size = x86->operand_size = (values[CS].Segment.Default + 1) * 2;

	x86->modrm = (x86->address_size == 4 ? modrm32 : modrm16);
	x86->rep = REP_NONE;

	if (x86->operand_size == 4) {
		x86->instr_binary = instr_binary_dword;
		x86->instr_read = instr_read_dword;
		x86->instr_write = instr_write_dword;
	} else {
		x86->instr_binary = instr_binary_word;
		x86->instr_read = instr_read_word;
		x86->instr_write = instr_write_word;
	}
}

/* return value: 1 => instruction known; 0 => instruction not known */
static inline int instr_sim(x86_emustate *x86, int pmode)
{
	unsigned char *reg_8;
	unsigned char uc;
	unsigned short uns;
	unsigned *dstreg;
	unsigned und, und2, repcount;
	unsigned char *ptr;
	uintptr_t flags;
	int i, i2, inst_len;
	int loop_inc = (values[EFLAGS].Reg32&DF) ? -1 : 1;		// make it a char ?
	unsigned eip = values[EIP].Reg32;
	unsigned cs = values[CS].Segment.Base;

#ifdef ENABLE_DEBUG_TRACE
	{
		int refseg;
		char frmtbuf[256];
		const UINT8 *oprom = mem + cs + eip;
		refseg = values[CS].Segment.Selector;
		dump_x86_regs();
		i386_dasm_one(frmtbuf, eip, oprom, values[CS].Segment.Default ? 32 : 16);
		instr_deb("%s, %d\n", frmtbuf, count);
	}
#endif

	if (x86->prefixes) {
		prepare_x86(x86);
	}

	x86->prefixes = handle_prefixes(x86);
	eip += x86->prefixes;

	if (x86->rep != REP_NONE) {
		/* TODO: All these rep instruction can still be heavily optimized */
		i2 = 0;
		if (x86->address_size == 4) {
			repcount = values[ECX].Reg32;
			switch(*(unsigned char *)MEM_BASE32(cs + eip)) {
				case 0xa4:         /* rep movsb */
#ifdef ENABLE_DEBUG_LOG
					if (values[ES].Segment.Base >= 0xa0000 && values[ES].Segment.Base < 0xb0000 &&
							x86->seg_base >= 0xa0000 && x86->seg_base < 0xb0000)
						instr_deb("VGAEMU: Video to video memcpy, ecx=%x\n", values[ECX].Reg32);
					/* TODO: accelerate this using memcpy */
#endif
					for (i = 0, und = 0; und < repcount;
							i += loop_inc, und++)
						instr_write_byte(MEM_BASE32(values[ES].Segment.Base + values[EDI].Reg32+i),
								instr_read_byte(MEM_BASE32(x86->seg_base + values[ESI].Reg32+i)));
					values[EDI].Reg32 += i;
					values[ESI].Reg32 += i;
					break;

				case 0xa5:         /* rep movsw/d */
					/* TODO: accelerate this using memcpy */
					for (i = 0, und = 0; und < repcount;
							i += loop_inc*x86->operand_size, und++)
						x86->instr_write(MEM_BASE32(values[ES].Segment.Base + values[EDI].Reg32+i),
								x86->instr_read(MEM_BASE32(x86->seg_base + values[ESI].Reg32+i)));
					values[EDI].Reg32 += i;
					values[ESI].Reg32 += i;
					break;

				case 0xa6:         /* rep cmpsb */
					for (i = 0, und = 0; und < repcount;) {
						instr_binary_byte(7, instr_read_byte(MEM_BASE32(x86->seg_base + values[ESI].Reg32+i)),
								instr_read_byte(MEM_BASE32(values[ES].Segment.Base + values[EDI].Reg32+i)), &values[EFLAGS].Reg32);
						i += loop_inc;
						und++;
						if (((values[EFLAGS].Reg32 & ZF) >> 6) != x86->rep) /* 0xf2 repnz 0xf3 repz */ {
							i2 = 1; /* we're fine now! */
							break;
						}
					}
					values[EDI].Reg32 += i;
					values[ESI].Reg32 += i;
					break;

				case 0xa7:         /* rep cmpsw/d */
					for (i = 0, und = 0; und < repcount;) {
						x86->instr_binary(7, instr_read_byte(MEM_BASE32(x86->seg_base + values[ESI].Reg32+i)),
								x86->instr_read(MEM_BASE32(values[ES].Segment.Base + values[EDI].Reg32+i)), &values[EFLAGS].Reg32);
						i += loop_inc*x86->operand_size;
						und++;
						if (((values[EFLAGS].Reg32 & ZF) >> 6) != x86->rep) /* 0xf2 repnz 0xf3 repz */ {
							i2 = 1; /* we're fine now! */
							break;
						}
					}
					values[EDI].Reg32 += i;
					values[ESI].Reg32 += i;
					break;

				case 0xaa: /* rep stosb */
					/* TODO: accelerate this using memset */
					for (und2 = values[EDI].Reg32, und = 0; und < repcount;
							und2 += loop_inc, und++)
						instr_write_byte(MEM_BASE32(values[ES].Segment.Base + und2), values[EAX].Reg8);
					values[EDI].Reg32 = und2;
					break;

				case 0xab: /* rep stosw */
					/* TODO: accelerate this using memset */
					for (und2 = values[EDI].Reg32, und = 0; und < repcount;
							und2 += loop_inc*x86->operand_size, und++)
						x86->instr_write(MEM_BASE32(values[ES].Segment.Base + und2), values[EAX].Reg32);
					values[EDI].Reg32 = und2;
					break;

				case 0xae: /* rep scasb */
					for (und2 = values[EDI].Reg32, und = 0; und < repcount;) {
						instr_binary_byte(7, values[EAX].Reg8, instr_read_byte(MEM_BASE32(values[ES].Segment.Base + und2)), &values[EFLAGS].Reg32);
						und2 += loop_inc;
						und++;
						if (((values[EFLAGS].Reg32 & ZF) >> 6) != x86->rep) /* 0x0 repnz 0x1 repz */ {
							i2 = 1; /* we're fine now! */
							break;
						}
					}
					values[EDI].Reg32 = und2;
					break;

				case 0xaf: /* rep scasw */
					for (und2 = values[EDI].Reg32, und = 0; und < repcount;) {
						x86->instr_binary(7, values[EAX].Reg32, x86->instr_read(MEM_BASE32(values[ES].Segment.Base + und2)), &values[EFLAGS].Reg32);
						und2 += loop_inc*x86->operand_size;
						und++;
						if (((values[EFLAGS].Reg32 & ZF) >> 6) != x86->rep) /* 0x0 repnz 0x1 repz */ {
							i2 = 1; /* we're fine now! */
							break;
						}
					}
					values[EDI].Reg32 = und2;
					break;

				default:
					return 0;
			}

			values[ECX].Reg32 -= und;
			if (values[ECX].Reg32 > 0 && i2 == 0) return 1;

		} else {
			repcount = values[ECX].Reg16;
			switch(*(unsigned char *)MEM_BASE32(cs + eip)) {
				case 0xa4:         /* rep movsb */
#ifdef ENABLE_DEBUG_LOG
					if (values[ES].Segment.Base >= 0xa0000 && values[ES].Segment.Base < 0xb0000 &&
							x86->seg_base >= 0xa0000 && x86->seg_base < 0xb0000)
						instr_deb("VGAEMU: Video to video memcpy, cx=%x\n", values[ECX].Reg16);
					/* TODO: accelerate this using memcpy */
#endif
					for (i = 0, und = 0; und < repcount;
							i += loop_inc, und++)
						instr_write_byte(MEM_BASE32(values[ES].Segment.Base + ((values[EDI].Reg32+i) & 0xffff)),
								instr_read_byte(MEM_BASE32(x86->seg_base + ((values[ESI].Reg32+i) & 0xffff))));
					values[EDI].Reg16 += i;
					values[ESI].Reg16 += i;
					break;

				case 0xa5:         /* rep movsw/d */
					/* TODO: accelerate this using memcpy */
					for (i = 0, und = 0; und < repcount;
							i += loop_inc*x86->operand_size, und++)
						x86->instr_write(MEM_BASE32(values[ES].Segment.Base + ((values[EDI].Reg32+i) & 0xffff)),
								x86->instr_read(MEM_BASE32(x86->seg_base + ((values[ESI].Reg32+i) & 0xffff))));
					values[EDI].Reg16 += i;
					values[ESI].Reg16 += i;
					break;

				case 0xa6: /* rep?z cmpsb */
					for (i = 0, und = 0; und < repcount;) {
						instr_binary_byte(7, instr_read_byte(MEM_BASE32(x86->seg_base + ((values[ESI].Reg32+i) & 0xffff))),
								instr_read_byte(MEM_BASE32(values[ES].Segment.Base + ((values[EDI].Reg32+i) & 0xffff))), &values[EFLAGS].Reg32);
						i += loop_inc;
						und++;
						if (((values[EFLAGS].Reg32 & ZF) >> 6) != x86->rep) /* 0x0 repnz 0x1 repz */ {
							i2 = 1; /* we're fine now! */
							break;
						}
					}
					values[EDI].Reg16 += i;
					values[ESI].Reg16 += i;
					break;

				case 0xa7: /* rep?z cmpsw/d */
					for (i = 0, und = 0; und < repcount;) {
						x86->instr_binary(7, x86->instr_read(MEM_BASE32(x86->seg_base + ((values[ESI].Reg32+i) & 0xffff))),
								x86->instr_read(MEM_BASE32(values[ES].Segment.Base + ((values[EDI].Reg32+i) & 0xffff))), &values[EFLAGS].Reg32);
						i += loop_inc * x86->operand_size;
						und++;
						if (((values[EFLAGS].Reg32 & ZF) >> 6) != x86->rep) /* 0x0 repnz 0x1 repz */ {
							i2 = 1; /* we're fine now! */
							break;
						}
					}
					values[EDI].Reg16 += i;
					values[ESI].Reg16 += i;
					break;

				case 0xaa: /* rep stosb */
					/* TODO: accelerate this using memset */
					for (uns = values[EDI].Reg16, und = 0; und < repcount;
							uns += loop_inc, und++)
						instr_write_byte(MEM_BASE32(values[ES].Segment.Base + uns), values[EAX].Reg8);
					values[EDI].Reg16 = uns;
					break;

				case 0xab: /* rep stosw/d */
					/* TODO: accelerate this using memset */
					for (uns = values[EDI].Reg16, und = 0; und < repcount;
							uns += loop_inc*x86->operand_size, und++)
						x86->instr_write(MEM_BASE32(values[ES].Segment.Base + uns), (x86->operand_size == 4 ? values[EAX].Reg32 : values[EAX].Reg16));
					values[EDI].Reg16 = uns;
					break;

				case 0xae: /* rep scasb */
					for (uns = values[EDI].Reg16, und = 0; und < repcount;) {
						instr_binary_byte(7, values[EAX].Reg8, instr_read_byte(MEM_BASE32(values[ES].Segment.Base + uns)), &values[EFLAGS].Reg32);
						uns += loop_inc;
						und++;
						if (((values[EFLAGS].Reg32 & ZF) >> 6) != x86->rep) /* 0x0 repnz 0x1 repz */ {
							i2 = 1; /* we're fine now! */
							break;
						}
					}
					values[EDI].Reg16 = uns;
					break;

				case 0xaf: /* rep scasw/d */
					for (uns = values[EDI].Reg16, und = 0; und < repcount;) {
						x86->instr_binary(7, values[EAX].Reg16, instr_read_word(MEM_BASE32(values[ES].Segment.Base + uns)), &values[EFLAGS].Reg32);
						uns += loop_inc*x86->operand_size;
						und++;
						if (((values[EFLAGS].Reg32 & ZF) >> 6) != x86->rep) /* 0x0 repnz 0x1 repz */ {
							i2 = 1; /* we're fine now! */
							break;
						}
					}
					values[EDI].Reg16 = uns;
					break;

				default:
					return 0;
			}
			values[ECX].Reg16 -= und;
			if (values[ECX].Reg16 > 0 && i2 == 0) return 1;
		}
		eip++;
	}
	else switch(*(unsigned char *)MEM_BASE32(cs + eip)) {
		case 0x00:		/* add r/m8,reg8 */
		case 0x08:		/* or r/m8,reg8 */
		case 0x10:		/* adc r/m8,reg8 */
		case 0x18:		/* sbb r/m8,reg8 */
		case 0x20:		/* and r/m8,reg8 */
		case 0x28:		/* sub r/m8,reg8 */
		case 0x30:		/* xor r/m8,reg8 */
		case 0x38:		/* cmp r/m8,reg8 */
			ptr = x86->modrm(MEM_BASE32(cs + eip), x86, &inst_len);
			uc = instr_binary_byte((*(unsigned char *)MEM_BASE32(cs + eip))>>3,
					instr_read_byte(ptr), *reg8((*(unsigned char *)MEM_BASE32(cs + eip + 1))>>3), &values[EFLAGS].Reg32);
			if (*(unsigned char *)MEM_BASE32(cs + eip)<0x38)
				instr_write_byte(ptr, uc);
			eip += 2 + inst_len; break;

		case 0x01:		/* add r/m16,reg16 */
		case 0x09:		/* or r/m16,reg16 */
		case 0x11:		/* adc r/m16,reg16 */
		case 0x19:		/* sbb r/m16,reg16 */
		case 0x21:		/* and r/m16,reg16 */
		case 0x29:		/* sub r/m16,reg16 */
		case 0x31:		/* xor r/m16,reg16 */
		case 0x39:		/* cmp r/m16,reg16 */
			ptr = x86->modrm(MEM_BASE32(cs + eip), x86, &inst_len);
			und = x86->instr_binary(*(unsigned char *)MEM_BASE32(cs + eip)>>3, x86->instr_read(ptr), *reg(*(unsigned char *)MEM_BASE32(cs + eip + 1)>>3), &values[EFLAGS].Reg32);
			if (*(unsigned char *)MEM_BASE32(cs + eip)<0x38)
				x86->instr_write(ptr, und);
			eip += 2 + inst_len; break;

		case 0x02:		/* add reg8,r/m8 */
		case 0x0a:		/* or reg8,r/m8 */
		case 0x12:		/* adc reg8,r/m8 */
		case 0x1a:		/* sbb reg8,r/m8 */
		case 0x22:		/* and reg8,r/m8 */
		case 0x2a:		/* sub reg8,r/m8 */
		case 0x32:		/* xor reg8,r/m8 */
		case 0x3a:		/* cmp reg8,r/m8 */
			reg_8 = reg8(*(unsigned char *)MEM_BASE32(cs + eip + 1)>>3);
			uc = instr_binary_byte(*(unsigned char *)MEM_BASE32(cs + eip)>>3,
					*reg_8, instr_read_byte(x86->modrm(MEM_BASE32(cs + eip),
							x86, &inst_len)), &values[EFLAGS].Reg32);
			if (*(unsigned char *)MEM_BASE32(cs + eip)<0x38) *reg_8 = uc;
			eip += 2 + inst_len; break;

		case 0x03:		/* add reg,r/m16 */
		case 0x0b:		/* or reg,r/m16 */
		case 0x13:		/* adc reg,r/m16 */
		case 0x1b:		/* sbb reg,r/m16 */
		case 0x23:		/* and reg,r/m16 */
		case 0x2b:		/* sub reg,r/m16 */
		case 0x33:		/* xor reg,r/m16 */
		case 0x3b:		/* cmp reg,r/m16 */
			dstreg = reg(*(unsigned char *)MEM_BASE32(cs + eip + 1)>>3);
			und = x86->instr_binary(*(unsigned char *)MEM_BASE32(cs + eip)>>3,
					*dstreg, x86->instr_read(x86->modrm(MEM_BASE32(cs + eip), x86,
							&inst_len)), &values[EFLAGS].Reg32);
			if (*(unsigned char *)MEM_BASE32(cs + eip)<0x38) {
				if (x86->operand_size == 2)
					R_WORD(*dstreg) = und;
				else
					*dstreg = und;
			}
			eip += 2 + inst_len; break;

		case 0x04:		/* add al,imm8 */
		case 0x0c:		/* or al,imm8 */
		case 0x14:		/* adc al,imm8 */
		case 0x1c:		/* sbb al,imm8 */
		case 0x24:		/* and al,imm8 */
		case 0x2c:		/* sub al,imm8 */
		case 0x34:		/* xor al,imm8 */
		case 0x3c:		/* cmp al,imm8 */
			uc = instr_binary_byte(*(unsigned char *)MEM_BASE32(cs + eip)>>3, values[EAX].Reg8,
					*(unsigned char *)MEM_BASE32(cs + eip + 1), &values[EFLAGS].Reg32);
			if (*(unsigned char *)MEM_BASE32(cs + eip)<0x38) values[EAX].Reg8 = uc;
			eip += 2; break;

		case 0x05:		/* add ax,imm16 */
		case 0x0d:		/* or ax,imm16 */
		case 0x15:		/* adc ax,imm16 */
		case 0x1d:		/* sbb ax,imm16 */
		case 0x25:		/* and ax,imm16 */
		case 0x2d:		/* sub ax,imm16 */
		case 0x35:		/* xor ax,imm16 */
		case 0x3d:		/* cmp ax,imm16 */
			und = x86->instr_binary(*(unsigned char *)MEM_BASE32(cs + eip)>>3,
					values[EAX].Reg32, R_DWORD(*(unsigned char *)MEM_BASE32(cs + eip + 1)), &values[EFLAGS].Reg32);
			if (x86->operand_size == 2) {
				if (*(unsigned char *)MEM_BASE32(cs + eip)<0x38) values[EAX].Reg16 = und;
				eip += 3;
			} else {
				if (*(unsigned char *)MEM_BASE32(cs + eip)<0x38) values[EAX].Reg32 = und;
				eip += 5;
			}
			break;

		case 0x06:  /* push sreg */
		case 0x0e:
		case 0x16:
		case 0x1e:
			push(*sreg(*(unsigned char *)MEM_BASE32(cs + eip)>>3), x86);
			eip++; break;

		case 0x07:		/* pop es */
			if (pmode || x86->operand_size == 4)
				return 0;
			else {
				unsigned int seg = values[ES].Segment.Selector;
				pop(&seg, x86);
				values[ES].Segment.Selector = seg;
				values[ES].Segment.Base = seg << 4;
				eip++;
			}
			break;

		case 0x0f:
			switch (*(unsigned char *)MEM_BASE32(cs + eip + 1))
			{
				case 0x80: OP_JCCW(values[EFLAGS].Reg32 & OF);         /*jo*/
				case 0x81: OP_JCCW(!(values[EFLAGS].Reg32 & OF));      /*jno*/
				case 0x82: OP_JCCW(values[EFLAGS].Reg32 & CF);         /*jc*/
				case 0x83: OP_JCCW(!(values[EFLAGS].Reg32 & CF));      /*jnc*/
				case 0x84: OP_JCCW(values[EFLAGS].Reg32 & ZF);         /*jz*/
				case 0x85: OP_JCCW(!(values[EFLAGS].Reg32 & ZF));      /*jnz*/
				case 0x86: OP_JCCW(values[EFLAGS].Reg32 & (ZF|CF));    /*jbe*/
				case 0x87: OP_JCCW(!(values[EFLAGS].Reg32 & (ZF|CF))); /*ja*/
				case 0x88: OP_JCCW(values[EFLAGS].Reg32 & SF);         /*js*/
				case 0x89: OP_JCCW(!(values[EFLAGS].Reg32 & SF));      /*jns*/
				case 0x8a: OP_JCCW(values[EFLAGS].Reg32 & PF);         /*jp*/
				case 0x8b: OP_JCCW(!(values[EFLAGS].Reg32 & PF));      /*jnp*/
				case 0x8c: OP_JCCW((values[EFLAGS].Reg32 & SF)^((values[EFLAGS].Reg32 & OF)>>4))         /*jl*/
				case 0x8d: OP_JCCW(!((values[EFLAGS].Reg32 & SF)^((values[EFLAGS].Reg32 & OF)>>4)))      /*jnl*/
				case 0x8e: OP_JCCW((values[EFLAGS].Reg32 & (SF|ZF))^((values[EFLAGS].Reg32 & OF)>>4))    /*jle*/
				case 0x8f: OP_JCCW(!((values[EFLAGS].Reg32 & (SF|ZF))^((values[EFLAGS].Reg32 & OF)>>4))) /*jg*/
				case 0xa4:
					ptr = x86->modrm(MEM_BASE32(cs + eip + 1), x86, &inst_len);
					repcount = *(unsigned char *)MEM_BASE32(cs + eip + 3 + inst_len) & 31;
					if (!repcount) break;
					if (x86->operand_size == 2)
					{
						uns = R_WORD(*reg(*(unsigned char *)MEM_BASE32(cs + eip + 2)>>3));
						unsigned short uns2 = instr_read_word(ptr);
						__asm__ __volatile__("\n\
								shldw	%%cl, %3, %1\n\
								pushf; pop	%0\n \
								" : "=g" (flags), "+g" (uns2) : "c" (repcount), "r" (uns));
						instr_write_word(ptr, uns2);
					}
					else
					{
						und = *reg(*(unsigned char *)MEM_BASE32(cs + eip + 2)>>3);
						und2 = instr_read_dword(ptr);
						__asm__ __volatile__("\n\
								shldl	%%cl, %3, %1\n\
								pushf; pop	%0\n \
								" : "=g" (flags), "+g" (und2) : "c" (repcount), "r" (und));
						instr_write_dword(ptr, und2);
					}
					values[EFLAGS].Reg32 |= flags & (OF|ZF|SF|PF|AF|CF);
					eip += inst_len + 4;
					break;
				case 0xac:
					ptr = x86->modrm(MEM_BASE32(cs + eip + 1), x86, &inst_len);
					repcount = *(unsigned char *)MEM_BASE32(cs + eip + 3 + inst_len) & 31;
					if (!repcount) break;
					if (x86->operand_size == 2)
					{
						uns = R_WORD(*reg(*(unsigned char *)MEM_BASE32(cs + eip + 2)>>3));
						unsigned short uns2 = instr_read_word(ptr);
						__asm__ __volatile__("\n\
								shrdw	%%cl, %3, %1\n\
								pushf; pop	%0\n \
								" : "=g" (flags), "+g" (uns2) : "c" (repcount), "r" (uns));
						instr_write_word(ptr, uns2);
					}
					else
					{
						und = *reg(*(unsigned char *)MEM_BASE32(cs + eip + 2)>>3);
						und2 = instr_read_dword(ptr);
						__asm__ __volatile__("\n\
								shrdl	%%cl, %3, %1\n\
								pushf; pop	%0\n \
								" : "=g" (flags), "+g" (und2) : "c" (repcount), "r" (und));
						instr_write_dword(ptr, und2);
					}
					values[EFLAGS].Reg32 |= flags & (OF|ZF|SF|PF|AF|CF);
					eip += inst_len + 4;
					break;
				case 0xaf:
					ptr = x86->modrm(MEM_BASE32(cs + eip + 1), x86, &inst_len);
					dstreg = reg(*(unsigned char *)MEM_BASE32(cs + eip + 2)>>3);
					if (x86->operand_size == 4)
					{
						long long q = (long long)(int)*dstreg * (long long)(int)instr_read_dword(ptr);
						*dstreg = q & 0xffffffff;
						values[EFLAGS].Reg32 &= ~(CF|OF);
						if (q & ~0xffffffffull)
							values[EFLAGS].Reg32 |= (CF|OF);
					}
					else
					{
						i = (short)R_WORD(*dstreg) * (short)instr_read_word(ptr);
						R_WORD(*dstreg) = i & 0xffff;
						values[EFLAGS].Reg32 &= ~(CF|OF);
						if (i & ~0xffff)
							values[EFLAGS].Reg32 |= (CF|OF);
					}
					eip += inst_len + 3; break;
				case 0xb6:
					if (x86->operand_size == 2)
						R_WORD(*reg(*(unsigned char *)MEM_BASE32(cs + eip + 2)>>3)) =
							instr_read_byte(x86->modrm(MEM_BASE32(cs + eip + 1), x86, &inst_len));
					else
						*reg(*(unsigned char *)MEM_BASE32(cs + eip + 2)>>3) =
							instr_read_byte(x86->modrm(MEM_BASE32(cs + eip + 1), x86, &inst_len));
					eip += inst_len + 3; break;
				case 0xbe:
					if (x86->operand_size == 2)
						R_WORD(*reg(*(unsigned char *)MEM_BASE32(cs + eip + 2)>>3)) =
							(char)instr_read_byte(x86->modrm(MEM_BASE32(cs + eip + 1), x86, &inst_len));
					else
						*reg(*(unsigned char *)MEM_BASE32(cs + eip + 2)>>3) =
							(char)instr_read_byte(x86->modrm(MEM_BASE32(cs + eip + 1), x86, &inst_len));
					eip += inst_len + 3; break;
				case 0xbf:
					*reg(*(unsigned char *)MEM_BASE32(cs + eip + 2)>>3) =
						(short)instr_read_word(x86->modrm(MEM_BASE32(cs + eip + 1), x86, &inst_len));
					eip += inst_len + 3; break;
				default:
					return 0;

			}
			break;

			/* 0x17 pop ss is a bit dangerous and rarely used */

		case 0x1f:		/* pop ds */
			if (pmode || x86->operand_size == 4)
				return 0;
			else {
				unsigned int seg = values[DS].Segment.Selector;
				pop(&seg, x86);
				values[DS].Segment.Selector = seg;
				values[DS].Segment.Base = seg << 4;
				eip++;
			}
			break;

		case 0x27:  /* daa */
			if (((values[EAX].Reg8 & 0xf) > 9) || (values[EFLAGS].Reg32&AF)) {
				values[EAX].Reg8 += 6;
				values[EFLAGS].Reg32 |= AF;
			} else
				values[EFLAGS].Reg32 &= ~AF;
			if ((values[EAX].Reg8 > 0x9f) || (values[EFLAGS].Reg32&CF)) {
				values[EAX].Reg8 += 0x60;
				instr_flags(values[EAX].Reg8, 0x80, &values[EFLAGS].Reg32);
				values[EFLAGS].Reg32 |= CF;
			} else
				instr_flags(values[EAX].Reg8, 0x80, &values[EFLAGS].Reg32);
			eip++; break;

		case 0x2f:  /* das */
			if (((values[EAX].Reg8 & 0xf) > 9) || (values[EFLAGS].Reg32&AF)) {
				values[EAX].Reg8 -= 6;
				values[EFLAGS].Reg32 |= AF;
			} else
				values[EFLAGS].Reg32 &= ~AF;
			if ((values[EAX].Reg8 > 0x9f) || (values[EFLAGS].Reg32&CF)) {
				values[EAX].Reg8 -= 0x60;
				instr_flags(values[EAX].Reg8, 0x80, &values[EFLAGS].Reg32);
				values[EFLAGS].Reg32 |= CF;
			} else
				instr_flags(values[EAX].Reg8, 0x80, &values[EFLAGS].Reg32);
			eip++; break;

		case 0x37:  /* aaa */
			if (((values[EAX].Reg8 & 0xf) > 9) || (values[EFLAGS].Reg32&AF)) {
				values[EAX].Reg8 = (values[EAX].Reg32+6) & 0xf;
				(*(&values[EAX].Reg8 + 1))++;
				values[EFLAGS].Reg32 |= (CF|AF);
			} else
				values[EFLAGS].Reg32 &= ~(CF|AF);
			eip++; break;

		case 0x3f:  /* aas */
			if (((values[EAX].Reg8 & 0xf) > 9) || (values[EFLAGS].Reg32&AF)) {
				values[EAX].Reg8 = (values[EAX].Reg32-6) & 0xf;
				(*(&values[EAX].Reg8 + 1))--;
				values[EFLAGS].Reg32 |= (CF|AF);
			} else
				values[EFLAGS].Reg32 &= ~(CF|AF);
			eip++; break;

		case 0x40:
		case 0x41:
		case 0x42:
		case 0x43:
		case 0x44:
		case 0x45:
		case 0x46:
		case 0x47: /* inc reg */
			values[EFLAGS].Reg32 &= ~(OF|ZF|SF|PF|AF);
			dstreg = reg(*(unsigned char *)MEM_BASE32(cs + eip));
			if (x86->operand_size == 2) {
				OPandFLAG0(flags, incw, R_WORD(*dstreg), =r);
			} else {
				OPandFLAG0(flags, incl, *dstreg, =r);
			}
			values[EFLAGS].Reg32 |= flags & (OF|ZF|SF|PF|AF);
			eip++; break;

		case 0x48:
		case 0x49:
		case 0x4a:
		case 0x4b:
		case 0x4c:
		case 0x4d:
		case 0x4e:
		case 0x4f: /* dec reg */
			values[EFLAGS].Reg32 &= ~(OF|ZF|SF|PF|AF);
			dstreg = reg(*(unsigned char *)MEM_BASE32(cs + eip));
			if (x86->operand_size == 2) {
				OPandFLAG0(flags, decw, R_WORD(*dstreg), =r);
			} else {
				OPandFLAG0(flags, decl, *dstreg, =r);
			}
			values[EFLAGS].Reg32 |= flags & (OF|ZF|SF|PF|AF);
			eip++; break;

		case 0x50:
		case 0x51:
		case 0x52:
		case 0x53:
		case 0x54:
		case 0x55:
		case 0x56:
		case 0x57: /* push reg */
			push(*reg(*(unsigned char *)MEM_BASE32(cs + eip)), x86);
			eip++; break;

		case 0x58:
		case 0x59:
		case 0x5a:
		case 0x5b:
		case 0x5c:
		case 0x5d:
		case 0x5e:
		case 0x5f: /* pop reg */
			pop(&und, x86);
			if (x86->operand_size == 2)
				R_WORD(*reg(*(unsigned char *)MEM_BASE32(cs + eip))) = und;
			else
				*reg(*(unsigned char *)MEM_BASE32(cs + eip)) = und;
			eip++; break;

		case 0x60:
			und = values[ESP].Reg32;
			push(values[EAX].Reg32, x86);
			push(values[ECX].Reg32, x86);
			push(values[EDX].Reg32, x86);
			push(values[EBX].Reg32, x86);
			push(und, x86);
			push(values[EBP].Reg32, x86);
			push(values[ESI].Reg32, x86);
			push(values[EDI].Reg32, x86);
			eip++;
			break;

		case 0x61:
			pop(&values[EDI].Reg32, x86);
			pop(&values[ESI].Reg32, x86);
			pop(&values[EBP].Reg32, x86);
			pop(&und, x86);
			pop(&values[EBX].Reg32, x86);
			pop(&values[EDX].Reg32, x86);
			pop(&values[ECX].Reg32, x86);
			pop(&values[EAX].Reg32, x86);
			eip++;
			break;

		case 0x68: /* push imm16 */
			push(R_DWORD(*(unsigned char *)MEM_BASE32(cs + eip + 1)), x86);
			eip += x86->operand_size + 1; break;

		case 0x69:
			ptr = x86->modrm(MEM_BASE32(cs + eip), x86, &inst_len);
			dstreg = reg(*(unsigned char *)MEM_BASE32(cs + eip + 1)>>3);
			if (x86->operand_size == 4)
			{
				long long q = (long long)(int)instr_read_dword(ptr) * (long long)*(int *)MEM_BASE32(cs + eip + 2 + inst_len);
				*dstreg = q & 0xffffffff;
				values[EFLAGS].Reg32 &= ~(CF|OF);
				if (q & ~0xffffffffull)
					values[EFLAGS].Reg32 |= (CF|OF);
				eip += inst_len + 6;
			}
			else
			{
				i = (short)R_WORD(*dstreg) * *(short *)MEM_BASE32(cs + eip + 2 + inst_len);
				R_WORD(*dstreg) = i & 0xffff;
				values[EFLAGS].Reg32 &= ~(CF|OF);
				if (i & ~0xffff)
					values[EFLAGS].Reg32 |= (CF|OF);
				eip += inst_len + 4;
			}
			break;

		case 0x6a: /* push imm8 */
			push((int)*(signed char *)MEM_BASE32(cs + eip + 1), x86);
			eip += 2; break;

		case 0x70: OP_JCC(values[EFLAGS].Reg32 & OF);         /*jo*/
		case 0x71: OP_JCC(!(values[EFLAGS].Reg32 & OF));      /*jno*/
		case 0x72: OP_JCC(values[EFLAGS].Reg32 & CF);         /*jc*/
		case 0x73: OP_JCC(!(values[EFLAGS].Reg32 & CF));      /*jnc*/
		case 0x74: OP_JCC(values[EFLAGS].Reg32 & ZF);         /*jz*/
		case 0x75: OP_JCC(!(values[EFLAGS].Reg32 & ZF));      /*jnz*/
		case 0x76: OP_JCC(values[EFLAGS].Reg32 & (ZF|CF));    /*jbe*/
		case 0x77: OP_JCC(!(values[EFLAGS].Reg32 & (ZF|CF))); /*ja*/
		case 0x78: OP_JCC(values[EFLAGS].Reg32 & SF);         /*js*/
		case 0x79: OP_JCC(!(values[EFLAGS].Reg32 & SF));      /*jns*/
		case 0x7a: OP_JCC(values[EFLAGS].Reg32 & PF);         /*jp*/
		case 0x7b: OP_JCC(!(values[EFLAGS].Reg32 & PF));      /*jnp*/
		case 0x7c: OP_JCC((values[EFLAGS].Reg32 & SF)^((values[EFLAGS].Reg32 & OF)>>4))         /*jl*/
		case 0x7d: OP_JCC(!((values[EFLAGS].Reg32 & SF)^((values[EFLAGS].Reg32 & OF)>>4)))      /*jnl*/
		case 0x7e: OP_JCC((values[EFLAGS].Reg32 & (SF|ZF))^((values[EFLAGS].Reg32 & OF)>>4))    /*jle*/
		case 0x7f: OP_JCC(!((values[EFLAGS].Reg32 & (SF|ZF))^((values[EFLAGS].Reg32 & OF)>>4))) /*jg*/

		case 0x80:		/* logical r/m8,imm8 */
		case 0x82:
			   ptr = x86->modrm(MEM_BASE32(cs + eip), x86, &inst_len);
			   uc = instr_binary_byte(*(unsigned char *)MEM_BASE32(cs + eip + 1)>>3,
					   instr_read_byte(ptr), *(unsigned char *)MEM_BASE32(cs + eip + 2 + inst_len), &values[EFLAGS].Reg32);
			   if ((*(unsigned char *)MEM_BASE32(cs + eip + 1)&0x38) < 0x38)
				   instr_write_byte(ptr, uc);
			   eip += 3 + inst_len; break;

		case 0x81:		/* logical r/m,imm */
			   ptr = x86->modrm(MEM_BASE32(cs + eip), x86, &inst_len);
			   und = x86->instr_binary(*(unsigned char *)MEM_BASE32(cs + eip + 1)>>3,
					   x86->instr_read(ptr), R_DWORD(*(unsigned char *)MEM_BASE32(cs + eip + 2 + inst_len)), &values[EFLAGS].Reg32);
			   if ((*(unsigned char *)MEM_BASE32(cs + eip + 1)&0x38) < 0x38) x86->instr_write(ptr, und);
			   eip += x86->operand_size + 2 + inst_len;
			   break;

		case 0x83:		/* logical r/m,imm8 */
			   ptr = x86->modrm(MEM_BASE32(cs + eip), x86, &inst_len);
			   und = x86->instr_binary(*(unsigned char *)MEM_BASE32(cs + eip + 1)>>3,
					   x86->instr_read(ptr), (int)*(signed char *)MEM_BASE32(cs + eip + 2 + inst_len),
					   &values[EFLAGS].Reg32);
			   if ((*(unsigned char *)MEM_BASE32(cs + eip + 1)&0x38) < 0x38)
				   x86->instr_write(ptr, und);
			   eip += inst_len + 3; break;

		case 0x84: /* test r/m8, reg8 */
			   instr_flags(instr_read_byte(x86->modrm(MEM_BASE32(cs + eip), x86, &inst_len)) &
					   *reg8(*(unsigned char *)MEM_BASE32(cs + eip + 1)>>3),
					   0x80, &values[EFLAGS].Reg32);
			   eip += inst_len + 2; break;

		case 0x85: /* test r/m16, reg */
			   if (x86->operand_size == 2)
				   instr_flags(instr_read_word(x86->modrm(MEM_BASE32(cs + eip), x86,
								   &inst_len)) & R_WORD(*reg(*(unsigned char *)MEM_BASE32(cs + eip + 1)>>3)),
						   0x8000, &values[EFLAGS].Reg32);
			   else
				   instr_flags(instr_read_dword(x86->modrm(MEM_BASE32(cs + eip), x86, &inst_len)) &
						   *reg(*(unsigned char *)MEM_BASE32(cs + eip + 1)>>3),
						   0x80000000, &values[EFLAGS].Reg32);
			   eip += inst_len + 2; break;

		case 0x86:		/* xchg r/m8,reg8 */
			   reg_8 = reg8(*(unsigned char *)MEM_BASE32(cs + eip + 1)>>3);
			   ptr = x86->modrm(MEM_BASE32(cs + eip), x86, &inst_len);
			   uc = *reg_8;
			   *reg_8 = instr_read_byte(ptr);
			   instr_write_byte(ptr, uc);
			   eip += inst_len + 2; break;

		case 0x87:		/* xchg r/m16,reg */
			   dstreg = reg(*(unsigned char *)MEM_BASE32(cs + eip + 1)>>3);
			   ptr = x86->modrm(MEM_BASE32(cs + eip), x86, &inst_len);
			   und = *dstreg;
			   if (x86->operand_size == 2)
				   R_WORD(*dstreg) = instr_read_word(ptr);
			   else
				   *dstreg = instr_read_dword(ptr);
			   x86->instr_write(ptr, und);
			   eip += inst_len + 2; break;

		case 0x88:		/* mov r/m8,reg8 */
			   instr_write_byte(x86->modrm(MEM_BASE32(cs + eip), x86, &inst_len),
					   *reg8(*(unsigned char *)MEM_BASE32(cs + eip + 1)>>3));
			   eip += inst_len + 2; break;

		case 0x89:		/* mov r/m16,reg */
			   x86->instr_write(x86->modrm(MEM_BASE32(cs + eip), x86, &inst_len),
					   *reg(*(unsigned char *)MEM_BASE32(cs + eip + 1)>>3));
			   eip += inst_len + 2; break;

		case 0x8a:		/* mov reg8,r/m8 */
			   *reg8(*(unsigned char *)MEM_BASE32(cs + eip + 1)>>3) =
				   instr_read_byte(x86->modrm(MEM_BASE32(cs + eip), x86, &inst_len));
			   eip += inst_len + 2; break;

		case 0x8b:		/* mov reg,r/m16 */
			   if (x86->operand_size == 2)
				   R_WORD(*reg(*(unsigned char *)MEM_BASE32(cs + eip + 1)>>3)) =
					   instr_read_word(x86->modrm(MEM_BASE32(cs + eip), x86, &inst_len));
			   else
				   *reg(*(unsigned char *)MEM_BASE32(cs + eip + 1)>>3) =
					   instr_read_dword(x86->modrm(MEM_BASE32(cs + eip), x86, &inst_len));
			   eip += inst_len + 2; break;

		case 0x8c: /* mov r/m16,segreg */
			   ptr = x86->modrm(MEM_BASE32(cs + eip), x86, &inst_len);
			   if ((*(unsigned char *)MEM_BASE32(cs + eip + 1) & 0xc0) == 0xc0) /* compensate for mov r,segreg */
				   ptr = (unsigned char *)reg(*(unsigned char *)MEM_BASE32(cs + eip + 1));
			   instr_write_word(ptr, *sreg(*(unsigned char *)MEM_BASE32(cs + eip + 1)>>3));
			   eip += inst_len + 2; break;

		case 0x8d: /* lea */
			   {
				   unsigned ptr = x86->seg_ss_base;
				   x86->seg_ss_base = x86->seg_base;
				   if (x86->operand_size == 2)
					   R_WORD(*reg(*(unsigned char *)MEM_BASE32(cs + eip + 1)>>3)) =
						   x86->modrm(MEM_BASE32(cs + eip), x86, &inst_len) - (unsigned char *)MEM_BASE32(x86->seg_base);
				   else
					   *reg(*(unsigned char *)MEM_BASE32(cs + eip + 1)>>3) =
						   x86->modrm(MEM_BASE32(cs + eip), x86, &inst_len) - (unsigned char *)MEM_BASE32(x86->seg_base);
				   x86->seg_ss_base = ptr;
				   eip += inst_len + 2; break;
			   }

		case 0x8e:		/* mov segreg,r/m16 */
			   if (pmode || x86->operand_size == 4)
				   return 0;
			   else switch (*(unsigned char *)MEM_BASE32(cs + eip + 1)&0x38) {
				   case 0:
					   ptr = x86->modrm(MEM_BASE32(cs + eip), x86, &inst_len);
					   if ((*(unsigned char *)MEM_BASE32(cs + eip + 1) & 0xc0) == 0xc0)  /* compensate for mov r,segreg */
						   ptr = (unsigned char *)reg(*(unsigned char *)MEM_BASE32(cs + eip + 1));
					   values[ES].Segment.Selector = instr_read_word(ptr);
					   values[ES].Segment.Base = values[ES].Segment.Selector << 4;
					   eip += inst_len + 2; break;
				   case 0x18:
					   ptr = x86->modrm(MEM_BASE32(cs + eip), x86, &inst_len);
					   if ((*(unsigned char *)MEM_BASE32(cs + eip + 1) & 0xc0) == 0xc0) /* compensate for mov es,reg */
						   ptr = (unsigned char *)reg(*(unsigned char *)MEM_BASE32(cs + eip + 1));
					   values[DS].Segment.Selector = instr_read_word(ptr);
					   values[DS].Segment.Base = values[DS].Segment.Selector << 4;
					   x86->seg_base = values[DS].Segment.Base;
					   eip += inst_len + 2; break;
				   default:
					   return 0;
			   }
			   break;

		case 0x8f: /*pop*/
			   if ((*(unsigned char *)MEM_BASE32(cs + eip + 1)&0x38) == 0){
				   pop(&und, x86);
				   x86->instr_write(x86->modrm(MEM_BASE32(cs + eip), x86, &inst_len), und);
				   eip += inst_len + 2;
			   } else
				   return 0;
			   break;

		case 0x90: /* nop */
			   eip++; break;
		case 0x91:
		case 0x92:
		case 0x93:
		case 0x94:
		case 0x95:
		case 0x96:
		case 0x97: /* xchg reg, ax */
			   dstreg = reg(*(unsigned char *)MEM_BASE32(cs + eip));
			   und = values[EAX].Reg32;
			   if (x86->operand_size == 2) {
				   values[EAX].Reg16 = *dstreg;
				   R_WORD(*dstreg) = und;
			   } else {
				   values[EAX].Reg32 = *dstreg;
				   *dstreg = und;
			   }
			   eip++; break;

		case 0x98:
			   if (x86->operand_size == 2) /* cbw */
				   values[EAX].Reg16 = (short)(signed char)values[EAX].Reg8;
			   else /* cwde */
				   values[EAX].Reg32 = (int)(short)values[EAX].Reg16;
			   eip++; break;

		case 0x99:
			   if (x86->operand_size == 2) /* cwd */
				   values[EDX].Reg16 = (values[EAX].Reg16 > 0x7fff ? 0xffff : 0);
			   else /* cdq */
				   values[EDX].Reg32 = (values[EAX].Reg32 > 0x7fffffff ? 0xffffffff : 0);
			   eip++; break;

		case 0x9a: /*call far*/
			   if (pmode || x86->operand_size == 4)
				   return 0;
			   else {
				   unsigned sel = values[CS].Segment.Selector;
				   push(sel, x86);
				   push(eip + 5, x86);
				   values[CS].Segment.Selector = R_WORD(*(unsigned char *)MEM_BASE32(cs + eip + 3));
				   values[CS].Segment.Base = values[CS].Segment.Selector << 4;
				   eip = R_WORD(*(unsigned char *)MEM_BASE32(cs + eip + 1));
				   cs = values[CS].Segment.Base;
			   }
			   break;
			   /* NO: 0x9b wait 0x9c pushf 0x9d popf*/

		case 0x9e: /* sahf */
			   values[EFLAGS].Reg32 = (values[EFLAGS].Reg32 & ~0xd5) | (*(&values[EAX].Reg8 + 1) & 0xd5);
			   eip++; break;

		case 0x9f: /* lahf */
			   *(&values[EAX].Reg8 + 1) = values[EFLAGS].Reg32 & 0xff; 
			   eip++; break;

		case 0xa0:		/* mov al,moff16 */
			   values[EAX].Reg8 = instr_read_byte(MEM_BASE32((R_DWORD(*(unsigned char *)MEM_BASE32(cs + eip + 1)) &
							   wordmask[x86->address_size])+x86->seg_base));
			   eip += 1 + x86->address_size; break;

		case 0xa1:		/* mov ax,moff16 */
			   if (x86->operand_size == 2)
				   values[EAX].Reg16 = instr_read_word(MEM_BASE32((R_DWORD(*(unsigned char *)MEM_BASE32(cs + eip + 1)) &
								   wordmask[x86->address_size])+x86->seg_base));
			   else
				   values[EAX].Reg32 = instr_read_dword(MEM_BASE32((R_DWORD(*(unsigned char *)MEM_BASE32(cs + eip + 1)) &
								   wordmask[x86->address_size])+x86->seg_base));
			   eip += 1 + x86->address_size; break;

		case 0xa2:		/* mov moff16,al */
			   instr_write_byte(MEM_BASE32((R_DWORD(*(unsigned char *)MEM_BASE32(cs + eip + 1)) &
							   wordmask[x86->address_size])+x86->seg_base), values[EAX].Reg8);
			   eip += 1 + x86->address_size; break;

		case 0xa3:		/* mov moff16,ax */
			   x86->instr_write(MEM_BASE32((R_DWORD(*(unsigned char *)MEM_BASE32(cs + eip + 1)) &
							   wordmask[x86->address_size])+x86->seg_base), values[EAX].Reg32);
			   eip += 1 + x86->address_size; break;

		case 0xa4:		/* movsb */
			   if (x86->address_size == 4) {
				   instr_write_byte(MEM_BASE32(values[ES].Segment.Base + values[EDI].Reg32),
						   instr_read_byte(MEM_BASE32(x86->seg_base + values[ESI].Reg32)));
				   values[EDI].Reg32 += loop_inc;
				   values[ESI].Reg32 += loop_inc;
			   } else {
				   instr_write_byte(MEM_BASE32(values[ES].Segment.Base + values[EDI].Reg16),
						   instr_read_byte(MEM_BASE32(x86->seg_base + values[ESI].Reg16)));
				   values[EDI].Reg16 += loop_inc;
				   values[ESI].Reg16 += loop_inc;
			   }
			   eip++; break;

		case 0xa5:		/* movsw */
			   if (x86->address_size == 4) {
				   x86->instr_write(MEM_BASE32(values[ES].Segment.Base + values[EDI].Reg32),
						   x86->instr_read(MEM_BASE32(x86->seg_base + values[ESI].Reg32)));
				   values[EDI].Reg32 += loop_inc * x86->operand_size;
				   values[ESI].Reg32 += loop_inc * x86->operand_size;
			   }
			   else {
				   x86->instr_write(MEM_BASE32(values[ES].Segment.Base + values[EDI].Reg16),
						   x86->instr_read(MEM_BASE32(x86->seg_base + values[ESI].Reg16)));
				   values[EDI].Reg16 += loop_inc * x86->operand_size;
				   values[ESI].Reg16 += loop_inc * x86->operand_size;
			   }
			   eip++; break;

		case 0xa6: /*cmpsb */
			   if (x86->address_size == 4) {
				   instr_binary_byte(7, instr_read_byte(MEM_BASE32(x86->seg_base + values[ESI].Reg32)),
						   instr_read_byte(MEM_BASE32(values[ES].Segment.Base + values[EDI].Reg32)), &values[EFLAGS].Reg32);
				   values[EDI].Reg32 += loop_inc;
				   values[ESI].Reg32 += loop_inc;
			   } else {
				   instr_binary_byte(7, instr_read_byte(MEM_BASE32(x86->seg_base + values[ESI].Reg16)),
						   instr_read_byte(MEM_BASE32(values[ES].Segment.Base + values[EDI].Reg16)), &values[EFLAGS].Reg32);
				   values[EDI].Reg16 += loop_inc;
				   values[ESI].Reg16 += loop_inc;
			   }
			   eip++; break;

		case 0xa7: /* cmpsw */
			   if (x86->address_size == 4) {
				   x86->instr_binary(7, x86->instr_read(MEM_BASE32(x86->seg_base + values[ESI].Reg32)),
						   x86->instr_read(MEM_BASE32(values[ES].Segment.Base + values[EDI].Reg32)), &values[EFLAGS].Reg32);
				   values[EDI].Reg32 += loop_inc * x86->operand_size;
				   values[ESI].Reg32 += loop_inc * x86->operand_size;
			   } else {
				   x86->instr_binary(7, x86->instr_read(MEM_BASE32(x86->seg_base + values[ESI].Reg16)),
						   x86->instr_read(MEM_BASE32(values[ES].Segment.Base + values[EDI].Reg16)), &values[EFLAGS].Reg32);
				   values[EDI].Reg16 += loop_inc * x86->operand_size;
				   values[ESI].Reg16 += loop_inc * x86->operand_size;
			   }
			   eip++; break;

		case 0xa8: /* test al, imm */
			   instr_flags(values[EAX].Reg8 & *(unsigned char *)MEM_BASE32(cs + eip + 1), 0x80, &values[EFLAGS].Reg32);
			   eip += 2; break;

		case 0xa9: /* test ax, imm */
			   if (x86->operand_size == 2) {
				   instr_flags(values[EAX].Reg16 & R_WORD(*(unsigned char *)MEM_BASE32(cs + eip + 1)), 0x8000, &values[EFLAGS].Reg32);
				   eip += 3; break;
			   } else {
				   instr_flags(values[EAX].Reg32 & R_DWORD(*(unsigned char *)MEM_BASE32(cs + eip + 1)), 0x80000000, &values[EFLAGS].Reg32);
				   eip += 5; break;
			   }

		case 0xaa:		/* stosb */
			   if (x86->address_size == 4) {
				   instr_write_byte(MEM_BASE32(values[ES].Segment.Base + values[EDI].Reg32), values[EAX].Reg8);
				   values[EDI].Reg32 += loop_inc;
			   } else {
				   instr_write_byte(MEM_BASE32(values[ES].Segment.Base + values[EDI].Reg16), values[EAX].Reg8);
				   values[EDI].Reg16 += loop_inc;
			   }
			   eip++; break;

		case 0xab:		/* stosw */
			   if (x86->address_size == 4) {
				   x86->instr_write(MEM_BASE32(values[ES].Segment.Base + values[EDI].Reg32), values[EAX].Reg32);
				   values[EDI].Reg32 += loop_inc * x86->operand_size;
			   } else {
				   x86->instr_write(MEM_BASE32(values[ES].Segment.Base + values[EDI].Reg16), values[EAX].Reg32);
				   values[EDI].Reg16 += loop_inc * x86->operand_size;
			   }
			   eip++; break;

		case 0xac:		/* lodsb */
			   if (x86->address_size == 4) {
				   values[EAX].Reg8 = instr_read_byte(MEM_BASE32(x86->seg_base + values[ESI].Reg32));
				   values[ESI].Reg32 += loop_inc;
			   } else {
				   values[EAX].Reg8 = instr_read_byte(MEM_BASE32(x86->seg_base + values[ESI].Reg16));
				   values[ESI].Reg16 += loop_inc;
			   }
			   eip++; break;

		case 0xad: /* lodsw */
			   if (x86->address_size == 4) {
				   und = x86->instr_read(MEM_BASE32(x86->seg_base + values[ESI].Reg32));
				   values[ESI].Reg32 += loop_inc * x86->operand_size;
			   } else {
				   und = x86->instr_read(MEM_BASE32(x86->seg_base + values[ESI].Reg16));
				   values[ESI].Reg16 += loop_inc * x86->operand_size;
			   }
			   if (x86->operand_size == 2)
				   values[EAX].Reg16 = und;
			   else
				   values[EAX].Reg32 = und;
			   eip++; break;

		case 0xae: /* scasb */
			   if (x86->address_size == 4) {
				   instr_binary_byte(7, values[EAX].Reg8, instr_read_byte(MEM_BASE32(values[ES].Segment.Base + values[EDI].Reg32)), &values[EFLAGS].Reg32);
				   values[EDI].Reg32 += loop_inc;
			   } else {
				   instr_binary_byte(7, values[EAX].Reg8, instr_read_byte(MEM_BASE32(values[ES].Segment.Base + values[EDI].Reg16)), &values[EFLAGS].Reg32);
				   values[EDI].Reg16 += loop_inc;
			   }
			   eip++; break;

		case 0xaf: /* scasw */
			   if (x86->address_size == 4) {
				   x86->instr_binary(7, values[EAX].Reg32, x86->instr_read(MEM_BASE32(values[ES].Segment.Base + values[EDI].Reg32)), &values[EFLAGS].Reg32);
				   values[EDI].Reg32 += loop_inc * x86->operand_size;
			   } else {
				   x86->instr_binary(7, values[EAX].Reg32, x86->instr_read(MEM_BASE32(values[ES].Segment.Base + values[EDI].Reg16)), &values[EFLAGS].Reg32);
				   values[EDI].Reg16 += loop_inc * x86->operand_size;
			   }
			   eip++; break;

		case 0xb0:
		case 0xb1:
		case 0xb2:
		case 0xb3:
		case 0xb4:
		case 0xb5:
		case 0xb6:
		case 0xb7:
			   *reg8(*(unsigned char *)MEM_BASE32(cs + eip)) = *(unsigned char *)MEM_BASE32(cs + eip + 1);
			   eip += 2; break;

		case 0xb8:
		case 0xb9:
		case 0xba:
		case 0xbb:
		case 0xbc:
		case 0xbd:
		case 0xbe:
		case 0xbf:
			   if (x86->operand_size == 2) {
				   R_WORD(*reg(*(unsigned char *)MEM_BASE32(cs + eip))) =
					   R_WORD(*(unsigned char *)MEM_BASE32(cs + eip + 1));
				   eip += 3; break;
			   } else {
				   *reg(*(unsigned char *)MEM_BASE32(cs + eip)) =
					   R_DWORD(*(unsigned char *)MEM_BASE32(cs + eip + 1));
				   eip += 5; break;
			   }

		case 0xc0: /* shift byte, imm8 */
			   if ((*(unsigned char *)MEM_BASE32(cs + eip + 1)&0x38)==0x30) return 0;
			   ptr = x86->modrm(MEM_BASE32(cs + eip), x86, &inst_len);
			   instr_write_byte(ptr,instr_shift(*(unsigned char *)MEM_BASE32(cs + eip + 1)>>3, (signed char) instr_read_byte(ptr),
						   *(unsigned char *)MEM_BASE32(cs + eip + 2+inst_len), 1, &values[EFLAGS].Reg32));
			   eip += inst_len + 3; break;

		case 0xc1: /* shift word, imm8 */
			   if ((*(unsigned char *)MEM_BASE32(cs + eip + 1)&0x38)==0x30) return 0;
			   ptr = x86->modrm(MEM_BASE32(cs + eip), x86, &inst_len);
			   if (x86->operand_size == 2)
				   instr_write_word(ptr, instr_shift(*(unsigned char *)MEM_BASE32(cs + eip + 1)>>3, (short)instr_read_word(ptr),
							   *(unsigned char *)MEM_BASE32(cs + eip + 2+inst_len), 2, &values[EFLAGS].Reg32));
			   else
				   instr_write_dword(ptr, instr_shift(*(unsigned char *)MEM_BASE32(cs + eip + 1)>>3, instr_read_dword(ptr),
							   *(unsigned char *)MEM_BASE32(cs + eip + 2+inst_len), 4, &values[EFLAGS].Reg32));
			   eip += inst_len + 3; break;

		case 0xc2:		/* ret imm16*/
			   pop(&und, x86);
			   if (values[CS].Segment.Default)
				   values[ESP].Reg32 += R_WORD(*(unsigned char *)MEM_BASE32(cs + eip + 1));
			   else
				   values[ESP].Reg16 += R_WORD(*(unsigned char *)MEM_BASE32(cs + eip + 1));
			   eip = und;
			   break;

		case 0xc3:		/* ret */
			   pop(&eip, x86);
			   break;

		case 0xc4:		/* les */
			   if (pmode || x86->operand_size == 4)
				   return 0;
			   else {
				   ptr = x86->modrm(MEM_BASE32(cs + eip), x86, &inst_len);
				   values[ES].Segment.Selector = instr_read_word(ptr+2);
				   values[ES].Segment.Base = values[ES].Segment.Selector << 4;
				   R_WORD(*reg(*(unsigned char *)MEM_BASE32(cs + eip + 1) >> 3)) = instr_read_word(ptr);
				   eip += inst_len + 2; break;
			   }

		case 0xc5:		/* lds */
			   if (pmode || x86->operand_size == 4)
				   return 0;
			   else {
				   ptr = x86->modrm(MEM_BASE32(cs + eip), x86, &inst_len);
				   values[DS].Segment.Selector = instr_read_word(ptr+2);
				   values[DS].Segment.Base = x86->seg_base = values[DS].Segment.Selector << 4;
				   R_WORD(*reg(*(unsigned char *)MEM_BASE32(cs + eip + 1) >> 3)) = instr_read_word(ptr);
				   eip += inst_len + 2; break;
			   }

		case 0xc6:		/* mov r/m8,imm8 */
			   ptr = x86->modrm(MEM_BASE32(cs + eip), x86, &inst_len);
			   instr_write_byte(ptr, *(unsigned char *)MEM_BASE32(cs + eip + 2 + inst_len));
			   eip += inst_len + 3; break;

		case 0xc7:		/* mov r/m,imm */
			   ptr = x86->modrm(MEM_BASE32(cs + eip), x86, &inst_len);
			   x86->instr_write(ptr, R_DWORD(*(unsigned char *)MEM_BASE32(cs + eip + 2 + inst_len)));
			   eip += x86->operand_size + inst_len + 2;
			   break;
			   /* 0xc8 enter */

		case 0xc9: /*leave*/
			   if (values[CS].Segment.Default)
				   values[ESP].Reg32 = values[EBP].Reg32;
			   else
				   values[ESP].Reg16 = values[EBP].Reg16;
			   pop(&values[EBP].Reg32, x86);
			   eip++; break;

		case 0xca: /*retf imm 16*/
			   if (pmode || x86->operand_size == 4)
				   return 0;
			   else {
				   unsigned int sel;
				   pop(&und, x86);
				   pop(&sel, x86);
				   values[CS].Segment.Selector = sel;
				   values[CS].Segment.Base = values[CS].Segment.Selector << 4;
				   values[ESP].Reg16 += R_WORD(*(unsigned char *)MEM_BASE32(cs + eip + 1));
				   cs = values[CS].Segment.Base;
				   eip = und;
			   }
			   break;

		case 0xcb: /*retf*/
			   if (pmode || x86->operand_size == 4)
				   return 0;
			   else {
				   unsigned int sel;
				   pop(&eip, x86);
				   pop(&sel, x86);
				   values[CS].Segment.Selector = sel;
				   values[CS].Segment.Base = values[CS].Segment.Selector << 4;
				   cs = values[CS].Segment.Base;
			   }
			   break;

			   /* 0xcc int3 0xcd int 0xce into 0xcf iret */

		case 0xd0: /* shift r/m8, 1 */
			   if ((*(unsigned char *)MEM_BASE32(cs + eip + 1)&0x38)==0x30) return 0;
			   ptr = x86->modrm(MEM_BASE32(cs + eip), x86, &inst_len);
			   instr_write_byte(ptr, instr_shift(*(unsigned char *)MEM_BASE32(cs + eip + 1)>>3,
						   (signed char) instr_read_byte(ptr),
						   1, 1, &values[EFLAGS].Reg32));
			   eip += inst_len + 2; break;

		case 0xd1: /* shift r/m16, 1 */
			   if ((*(unsigned char *)MEM_BASE32(cs + eip + 1)&0x38)==0x30) return 0;
			   ptr = x86->modrm(MEM_BASE32(cs + eip), x86, &inst_len);
			   if (x86->operand_size == 2)
				   instr_write_word(ptr, instr_shift(*(unsigned char *)MEM_BASE32(cs + eip + 1)>>3, (short) instr_read_word(ptr),
							   1, 2, &values[EFLAGS].Reg32));
			   else
				   instr_write_dword(ptr, instr_shift(*(unsigned char *)MEM_BASE32(cs + eip + 1)>>3, instr_read_dword(ptr), 1, 4, &values[EFLAGS].Reg32));
			   eip += inst_len + 2; break;

		case 0xd2: /* shift r/m8, cl */
			   if ((*(unsigned char *)MEM_BASE32(cs + eip + 1)&0x38)==0x30) return 0;
			   ptr = x86->modrm(MEM_BASE32(cs + eip), x86, &inst_len);
			   instr_write_byte(ptr,instr_shift(*(unsigned char *)MEM_BASE32(cs + eip + 1)>>3, (signed char) instr_read_byte(ptr),
						   values[ECX].Reg8, 1, &values[EFLAGS].Reg32));
			   eip += inst_len + 2; break;

		case 0xd3: /* shift r/m16, cl */
			   if ((*(unsigned char *)MEM_BASE32(cs + eip + 1)&0x38)==0x30) return 0;
			   ptr = x86->modrm(MEM_BASE32(cs + eip), x86, &inst_len);
			   if (x86->operand_size == 2)
				   instr_write_word(ptr, instr_shift(*(unsigned char *)MEM_BASE32(cs + eip + 1)>>3, (short) instr_read_word(ptr),
							   values[ECX].Reg8, 2, &values[EFLAGS].Reg32));
			   else
				   instr_write_dword(ptr, instr_shift(*(unsigned char *)MEM_BASE32(cs + eip + 1)>>3, instr_read_dword(ptr),
							   values[ECX].Reg8, 4, &values[EFLAGS].Reg32));
			   eip += inst_len + 2; break;

		case 0xd4:  /* aam byte */
			   *(&values[EAX].Reg8 + 1) = values[EAX].Reg8 / *(unsigned char *)MEM_BASE32(cs + eip + 1);
			   values[EAX].Reg8 = values[EAX].Reg8 % *(unsigned char *)MEM_BASE32(cs + eip + 1);
			   instr_flags(values[EAX].Reg8, 0x80, &values[EFLAGS].Reg32);
			   eip += 2; break;

		case 0xd5:  /* aad byte */
			   values[EAX].Reg8 = *(&values[EAX].Reg8 + 1) * *(unsigned char *)MEM_BASE32(cs + eip + 1) + values[EAX].Reg8;
			   *(&values[EAX].Reg8 + 1) = 0;
			   instr_flags(values[EAX].Reg8, 0x80, &values[EFLAGS].Reg32);
			   eip += 2; break;

		case 0xd6: /* salc */
			   values[EAX].Reg8 = values[EFLAGS].Reg32 & CF ? 0xff : 0;
			   eip++; break;

		case 0xd7: /* xlat */
			   values[EAX].Reg8 =  instr_read_byte(MEM_BASE32(x86->seg_base+(values[EBX].Reg32 & wordmask[x86->address_size])+values[EAX].Reg8));
			   eip++; break;
			   /* 0xd8 - 0xdf copro */

		case 0xe0: /* loopnz */
			   eip += ( (x86->address_size == 4 ? --values[ECX].Reg32 : --values[ECX].Reg16) && !(values[EFLAGS].Reg32 & ZF) ?
					   2 + *(signed char *)MEM_BASE32(cs + eip + 1) : 2); break;

		case 0xe1: /* loopz */
			   eip += ( (x86->address_size == 4 ? --values[ECX].Reg32 : --values[ECX].Reg16) && (values[EFLAGS].Reg32 & ZF) ?
					   2 + *(signed char *)MEM_BASE32(cs + eip + 1) : 2); break;

		case 0xe2: /* loop */
			   eip += ( (x86->address_size == 4 ? --values[ECX].Reg32 : --values[ECX].Reg16) ?
					   2 + *(signed char *)MEM_BASE32(cs + eip + 1) : 2); break;

		case 0xe3:  /* jcxz */
			   eip += ((x86->address_size == 4 ? values[ECX].Reg32 : values[ECX].Reg16) ? 2 :
					   2 + *(signed char *)MEM_BASE32(cs + eip + 1));
			   break;

			   /* 0xe4 in ib 0xe5 in iw 0xe6 out ib 0xe7 out iw */

		case 0xe8: /* call near */
			   push(eip + 1 + x86->operand_size, x86);
			   /* fall through */

		case 0xe9: /* jmp near */
			   eip += x86->operand_size + 1 + (R_DWORD(*(unsigned char *)MEM_BASE32(cs + eip + 1)) & wordmask[x86->operand_size]);
			   break;

		case 0xea: /*jmp far*/
			   if (pmode || x86->operand_size == 4)
				   return 0;
			   else {
				   values[CS].Segment.Selector = R_WORD(*(unsigned char *)MEM_BASE32(cs + eip+3));
				   values[CS].Segment.Base = values[CS].Segment.Selector << 4;
				   eip = R_WORD(*(unsigned char *)MEM_BASE32(cs + eip + 1));
				   cs = values[CS].Segment.Base;
			   }
			   break;

		case 0xeb: /* jmp short */
			   eip += 2 + *(signed char *)MEM_BASE32(cs + eip + 1); break;

		case 0xec: /* in al, dx */
			   /* Note that we short circuit if we can */
			   if ((values[EDX].Reg16 >= 0x3b0) && (values[EDX].Reg16 < 0x3e0)) {
				   values[EAX].Reg8 = read_io_byte(values[EDX].Reg16);
				   eip++; break;
			   }
			   else
				   return 0;
			   /* 0xed in ax,dx */

		case 0xee: /* out dx, al */
			   /* Note that we short circuit if we can */
			   if ((values[EDX].Reg16 >= 0x3b0) && (values[EDX].Reg16 < 0x3e0)) {
				   write_io_byte(values[EDX].Reg16, values[EAX].Reg8);
				   eip++;
			   }
			   else
				   return 0;
			   break;

		case 0xef: /* out dx, ax */
			   if ((x86->operand_size == 2) &&
					   (values[EDX].Reg16 >= 0x3b0) && (values[EDX].Reg16 < 0x3e0)) {
				   write_io_word(values[EDX].Reg16, values[EAX].Reg16);
				   eip++;
			   }
			   else
				   return 0;
			   break;

			   /* 0xf0 lock 0xf1 int1 */

			   /* 0xf2 repnz 0xf3 repz handled above */
			   /* 0xf4 hlt */

		case 0xf5: /* cmc */
			   values[EFLAGS].Reg32 ^= CF;
			   eip++; break;

		case 0xf6:
			   ptr = x86->modrm(MEM_BASE32(cs + eip), x86, &inst_len);
			   switch (*(unsigned char *)MEM_BASE32(cs + eip + 1)&0x38) {
				   case 0x00: /* test ptr byte, imm */
					   instr_flags(instr_read_byte(ptr) & *(unsigned char *)MEM_BASE32(cs + eip + 2+inst_len), 0x80, &values[EFLAGS].Reg32);
					   eip += inst_len + 3; break;
				   case 0x08: return 0;
				   case 0x10: /*not byte*/
					      instr_write_byte(ptr, ~instr_read_byte(ptr));
					      eip += inst_len + 2; break;
				   case 0x18: /*neg byte*/
					      instr_write_byte(ptr, instr_binary_byte(7, 0, instr_read_byte(ptr), &values[EFLAGS].Reg32));
					      eip += inst_len + 2; break;
				   case 0x20: /*mul byte*/
					      values[EAX].Reg16 = values[EAX].Reg8 * instr_read_byte(ptr);
					      values[EFLAGS].Reg32 &= ~(CF|OF);
					      if (*(&values[EAX].Reg8 + 1))
						      values[EFLAGS].Reg32 |= (CF|OF);
					      eip += inst_len + 2; break;
				   case 0x28: /*imul byte*/
					      values[EAX].Reg16 = (signed char)values[EAX].Reg8 * (signed char)instr_read_byte(ptr);
					      values[EFLAGS].Reg32 &= ~(CF|OF);
					      if (*(&values[EAX].Reg8 + 1))
						      values[EFLAGS].Reg32 |= (CF|OF);
					      eip += inst_len + 2; break;
				   case 0x30: /*div byte*/
					      und = values[EAX].Reg16;
					      uc = instr_read_byte(ptr);
					      if (uc == 0) return 0;
					      und2 = und / uc;
					      if (und2 & 0xffffff00) return 0;
					      values[EAX].Reg8 = und2 & 0xff;
					      *(&values[EAX].Reg8 + 1) = und % uc;
					      eip += inst_len + 2; break;
				   case 0x38: /*idiv byte*/
					      i = (short)values[EAX].Reg16;
					      uc = instr_read_byte(ptr);
					      if (uc == 0) return 0;
					      i2 = i / (signed char)uc;
					      if (i2<-128 || i2>127) return 0;
					      values[EAX].Reg8 = i2 & 0xff;
					      *(&values[EAX].Reg8 + 1) = i % (signed char)uc;
					      eip += inst_len + 2; break;
			   }
			   break;

		case 0xf7:
			   ptr = x86->modrm(MEM_BASE32(cs + eip), x86, &inst_len);
			   switch (*(unsigned char *)MEM_BASE32(cs + eip + 1)&0x38) {
				   case 0x00: /* test ptr word, imm */
					   if (x86->operand_size == 4)
					   {
						instr_flags(instr_read_dword(ptr) & R_DWORD(*(unsigned char *)MEM_BASE32(cs + eip + 2+inst_len)), 0x80000000, &values[EFLAGS].Reg32);
						eip += inst_len + 6; break;
					   }
					   else
					   {
						instr_flags(instr_read_word(ptr) & R_WORD(*(unsigned char *)MEM_BASE32(cs + eip + 2+inst_len)), 0x8000, &values[EFLAGS].Reg32);
						eip += inst_len + 4; break;
					   }
				   case 0x08: return 0;
				   case 0x10: /*not word*/
					      x86->instr_write(ptr, ~x86->instr_read(ptr));
					      eip += inst_len + 2; break;
				   case 0x18: /*neg word*/
					      x86->instr_write(ptr, x86->instr_binary(7, 0, x86->instr_read(ptr), &values[EFLAGS].Reg32));
					      eip += inst_len + 2; break;
				   case 0x20: /*mul word*/
					      if (x86->operand_size == 4)
					      {
						      unsigned long long unq = values[EAX].Reg32 * instr_read_dword(ptr);
						      values[EAX].Reg32 = unq & 0xffffffff;
						      values[EDX].Reg32 = unq >> 32;
						      values[EFLAGS].Reg32 &= ~(CF|OF);
						      if (values[EDX].Reg32)
							      values[EFLAGS].Reg32 |= (CF|OF);
					      }
					      else
					      {
						      und = values[EAX].Reg16 * instr_read_word(ptr);
						      values[EAX].Reg16 = und & 0xffff;
						      values[EDX].Reg16 = und >> 16;
						      values[EFLAGS].Reg32 &= ~(CF|OF);
						      if (values[EDX].Reg16)
							      values[EFLAGS].Reg32 |= (CF|OF);
					      }
					      eip += inst_len + 2; break;
				   case 0x28: /*imul word*/
					      if (x86->operand_size == 4)
					      {
						      long long q = (long long)(int)values[EAX].Reg32 * (long long)(int)instr_read_dword(ptr);
						      values[EAX].Reg32 = q & 0xffffffff;
						      values[EDX].Reg32 = q >> 32;
						      values[EFLAGS].Reg32 &= ~(CF|OF);
						      if (values[EDX].Reg32)
							      values[EFLAGS].Reg32 |= (CF|OF);
					      }
					      else
					      {
						      i = (short)values[EAX].Reg16 * (short)instr_read_word(ptr);
						      values[EAX].Reg16 = i & 0xffff;
						      values[EDX].Reg16 = i >> 16;
						      values[EFLAGS].Reg32 &= ~(CF|OF);
						      if (values[EDX].Reg16)
							      values[EFLAGS].Reg32 |= (CF|OF);
					      }
					      eip += inst_len + 2; break;
				   case 0x30: /*div word*/
					      if (x86->operand_size == 4)
					      {
						      unsigned long long unq = (values[EDX].Reg32<<32) + values[EAX].Reg32;
						      und = instr_read_dword(ptr);
						      if (und == 0) return 0;
						      unsigned long long unq2 = unq / und;
						      if (unq2 & ~0xffffffff) return 0;
						      values[EAX].Reg32 = unq2 & 0xffffffff;
						      values[EDX].Reg32 = unq % und;
					      }
					      else
					      {
						      und = (values[EDX].Reg16<<16) + values[EAX].Reg16;
						      uns = instr_read_word(ptr);
						      if (uns == 0) return 0;
						      und2 = und / uns;
						      if (und2 & 0xffff0000) return 0;
						      values[EAX].Reg16 = und2 & 0xffff;
						      values[EDX].Reg16 = und % uns;
					      }
					      eip += inst_len + 2; break;
				   case 0x38: /*idiv word*/
					      if (x86->operand_size == 4) return 0;
					      i = ((short)values[EDX].Reg16<<16) + values[EAX].Reg16;
					      uns = instr_read_word(ptr);
					      if (uns == 0) return 0;
					      i2 = i / (short)uns;
					      if (i2<-32768 || i2>32767) return 0;
					      values[EAX].Reg16 = i2 & 0xffff;
					      values[EDX].Reg16 = i % (short)uns;
					      eip += inst_len + 2; break;
			   }
			   break;

		case 0xf8: /* clc */
			   values[EFLAGS].Reg32 &= ~CF;
			   eip++; break;;

		case 0xf9: /* stc */
			   values[EFLAGS].Reg32 |= CF;
			   eip++; break;;

		case 0xfa:
			   if (pmode)
				return 0;
			   values[EFLAGS].Reg32 &= ~IF;
			   eip++; break;

		case 0xfb:
			   if (pmode)
				return 0;
			   values[EFLAGS].Reg32 |= IF;
			   eip++; break;

		case 0xfc: /* cld */
			   values[EFLAGS].Reg32 &= ~DF;
			   loop_inc = 1;
			   eip++; break;;

		case 0xfd: /* std */
			   values[EFLAGS].Reg32 |= DF;
			   loop_inc = -1;
			   eip++; break;;

		case 0xfe: /* inc/dec ptr */
			   ptr = x86->modrm(MEM_BASE32(cs + eip), x86, &inst_len);
			   uc = instr_read_byte(ptr);
			   switch (*(unsigned char *)MEM_BASE32(cs + eip + 1)&0x38) {
				   case 0x00:
					   values[EFLAGS].Reg32 &= ~(OF|ZF|SF|PF|AF);
					   OPandFLAG0(flags, incb, uc, =q);
					   values[EFLAGS].Reg32 |= flags & (OF|ZF|SF|PF|AF);
					   instr_write_byte(ptr, uc);
					   eip += inst_len + 2; break;
				   case 0x08:
					   values[EFLAGS].Reg32 &= ~(OF|ZF|SF|PF|AF);
					   OPandFLAG0(flags, decb, uc, =q);
					   values[EFLAGS].Reg32 |= flags & (OF|ZF|SF|PF|AF);
					   instr_write_byte(ptr, uc);
					   eip += inst_len + 2; break;
				   default:
					   return 0;
			   }
			   break;

		case 0xff:
			   ptr = x86->modrm(MEM_BASE32(cs + eip), x86, &inst_len);
			   if (x86->operand_size == 2)
			   {
				   uns = instr_read_word(ptr);
				   switch (*(unsigned char *)MEM_BASE32(cs + eip + 1)&0x38) {
					   case 0x00: /* inc */
						   values[EFLAGS].Reg32 &= ~(OF|ZF|SF|PF|AF);
						   OPandFLAG0(flags, incw, uns, =r);
						   values[EFLAGS].Reg32 |= flags & (OF|ZF|SF|PF|AF);
						   instr_write_word(ptr, uns);
						   eip += inst_len + 2; break;
					   case 0x08: /* dec */
						   values[EFLAGS].Reg32 &= ~(OF|ZF|SF|PF|AF);
						   OPandFLAG0(flags, decw, uns, =r);
						   values[EFLAGS].Reg32 |= flags & (OF|ZF|SF|PF|AF);
						   instr_write_word(ptr, uns);
						   eip += inst_len + 2; break;;
					   case 0x10: /*call near*/
						   push(eip + inst_len + 2, x86);
						   eip = uns;
						   break;

					   case 0x18: /*call far*/
						   if (pmode)
							   return 0;
						   else {
							   push(values[CS].Segment.Selector, x86);
							   values[CS].Segment.Selector = instr_read_word(ptr+2);
							   push(eip + inst_len + 2, x86);
							   values[CS].Segment.Base = values[CS].Segment.Selector << 4;
							   eip = uns;
							   cs = values[CS].Segment.Base;
						   }
						   break;

					   case 0x20: /*jmp near*/
						   eip = uns;
						   break;

					   case 0x28: /*jmp far*/
						   if (pmode)
							   return 0;
						   else {
							   values[CS].Segment.Selector = instr_read_word(ptr+2);
							   values[CS].Segment.Base = values[CS].Segment.Selector << 4;
							   eip = uns;
							   cs = values[CS].Segment.Base;
						   }
						   break;

					   case 0x30: /*push*/
						   push(uns, x86);
						   eip += inst_len + 2; break;
					   default:
						   return 0;
				}
			   }
			   else
			   {
				   und = instr_read_dword(ptr);
				   switch (*(unsigned char *)MEM_BASE32(cs + eip + 1)&0x38) {
					   case 0x00: /* inc */
						   values[EFLAGS].Reg32 &= ~(OF|ZF|SF|PF|AF);
						   OPandFLAG0(flags, incl, und, =r);
						   values[EFLAGS].Reg32 |= flags & (OF|ZF|SF|PF|AF);
						   instr_write_dword(ptr, und);
						   eip += inst_len + 2; break;
					   case 0x08: /* dec */
						   values[EFLAGS].Reg32 &= ~(OF|ZF|SF|PF|AF);
						   OPandFLAG0(flags, decl, und, =r);
						   values[EFLAGS].Reg32 |= flags & (OF|ZF|SF|PF|AF);
						   instr_write_dword(ptr, und);
						   eip += inst_len + 2; break;;
					   case 0x10: /*call near*/
						   push(eip + inst_len + 2, x86);
						   eip = und;
						   break;

					   case 0x18: /*call far*/
						   return 0;

					   case 0x20: /*jmp near*/
						   eip = und;
						   break;

					   case 0x28: /*jmp far*/
						   return 0;

					   case 0x30: /*push*/
						   push(und, x86);
						   eip += inst_len + 2; break;
					   default:
						   return 0;
				}
			   }
			   break;

		default:		/* First byte doesn't match anything */
			   return 0;
	}	/* switch (cs[eip]) */

	eip &= wordmask[(values[CS].Segment.Default + 1) * 2];
	values[EIP].Reg32 = eip;

#ifdef ENABLE_DEBUG_TRACE
	dump_x86_regs();
#endif

	return 1;
}

static int instr_emu(int cnt)
{
#ifdef ENABLE_DEBUG_LOG
	unsigned int ref;
	int refseg, rc;
	unsigned char frmtbuf[256];
	instr_deb("vga_emu: entry %04x:%08x\n", values[CS].Segment.Selector, values[EIP].Reg32);
	dump_x86_regs();
#endif
	int i = 0;
	x86_emustate x86;
	count = cnt ? : COUNT + 1;
	x86.prefixes = 1;

	do {
		if (!instr_sim(&x86, !(values[EFLAGS].Reg32 & (1 << 17)) && (values[CR0].Reg32 & 1))) {
#ifdef ENABLE_DEBUG_LOG
			UINT32 cp = values[CS].Segment.Base + values[EIP].Reg32;
			unsigned int ref;
			char frmtbuf[256];
			instr_deb("vga_emu: %u bytes not simulated %d: fault addr=%08x\n",
					instr_len(MEM_BASE32(cp), values[CS].Segment.Default), count, values[CR2].Reg32);
			dump_x86_regs();
#endif
			break;
		}
		i++;
		//if (!cnt && signal_pending())
		//	break;
	} while (--count > 0);

#ifdef ENABLE_DEBUG_LOG
	instr_deb("simulated %i, left %i\n", i, count);
#endif
	if (i == 0) /* really an unknown instruction from the beginning */
		return False;

	return True;
}


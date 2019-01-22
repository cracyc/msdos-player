// from winevdm by otya128, GPL2 licensed

#include "haxmvm.h"

void haxmvm_panic(const char *fmt, ...)
{
	va_list arg;
	va_start(arg, fmt);
	vprintf(fmt, arg);
	va_end(arg);
	ExitProcess(1);
}

#define REG8(x) state.x
#define AL _al
#define AH _ah
#define BL _bl
#define BH _bh
#define CL _cl
#define CH _ch
#define DL _dl
#define DH _dh
#define REG16(x) state.x
#define AX _ax
#define BX _bx
#define CX _cx
#define DX _dx
#define SP _sp
#define BP _bp
#define SI _si
#define DI _di
#define REG32(x) state.x
#define EAX _eax
#define EBX _ebx
#define ECX _ecx
#define EDX _edx
#define ESP _esp
#define EBP _ebp
#define ESI _esi
#define EDI _edi
#define SREG_BASE(x) state.x.base
#define SREG(x) state.x.selector
#define DS _ds
#define ES _es
#define FS _fs
#define GS _gs
#define CS _cs
#define SS _ss

#define i386_load_segment_descriptor(x) load_segdesc(state.x)
#define i386_sreg_load(x, y, z) state.y.selector = x; load_segdesc(state.y)
#define i386_get_flags() state._eflags
#define i386_set_flags(x) state._eflags = x
#define i386_push16 PUSH16
#define i386_pop16 POP16
#define vtlb_free(x) {}
#define I386_SREG segment_desc_t

#define m_eip state._eip
#define m_pc (state._eip + state._cs.base)
#define CR(x) state._cr ##x
#define DR(x) state._dr ##x
#define m_gdtr state._gdt
#define m_idtr state._idt
#define m_ldtr state._ldt
#define m_task state._tr

#define HAXMVM_STR2(s) #s
#define HAXMVM_STR(s) HAXMVM_STR2(s)
#define HAXMVM_ERR fprintf(stderr, "%s ("  HAXMVM_STR(__LINE__)  ") HAXM err.\n", __FUNCTION__)
#define HAXMVM_ERRF(fmt, ...) fprintf(stderr, "%s ("  HAXMVM_STR(__LINE__)  ") " fmt "\n", __FUNCTION__, ##__VA_ARGS__)

#define PROTECTED_MODE (state._cr0 & 1)
#define V8086_MODE (state._eflags & 0x20000)

#define I386OP(x) i386_ ##x
#define HOLD_LINE 1
#define CLEAR_LINE 0
#define INPUT_LINE_IRQ 1

static HANDLE hSystem;
static HANDLE hVM;
static HANDLE hVCPU;
static struct hax_tunnel *tunnel;
static struct vcpu_state_t state;
static char *iobuf;
static UINT8 m_CF, m_SF, m_ZF, m_IF, m_IOP1, m_IOP2, m_VM, m_NT;
static UINT32 m_a20_mask = 0xffffffff;
static UINT8 cpu_type = 6; // ppro
static UINT8 cpu_step = 0x0f; // whatever
static UINT8 m_CPL = 0; // always check at cpl 0
static UINT32 m_int6h_skip_eip = 0xffff0; // TODO: ???
static UINT8 m_ext;
static UINT32 m_prev_eip;
static UINT16 saved_vector = -1;

static void CALLBACK cpu_int_cb(LPVOID arg, DWORD low, DWORD high)
{
	DWORD bytes;
	int irq = 0xf8;
	if (tunnel->ready_for_interrupt_injection)
		DeviceIoControl(hVCPU, HAX_VCPU_IOCTL_INTERRUPT, NULL, 0, &irq, sizeof(irq), &bytes, NULL);
	else
		tunnel->request_interrupt_window = 1;
}

static DWORD CALLBACK cpu_int_th(LPVOID arg)
{
	LARGE_INTEGER when;
	HANDLE timer;

	if (!(timer = CreateWaitableTimerA( NULL, FALSE, NULL ))) return 0;

	when.u.LowPart = when.u.HighPart = 0;
	SetWaitableTimer(timer, &when, 55, cpu_int_cb, arg, FALSE); // 55ms for default 8254 rate
	for (;;) SleepEx(INFINITE, TRUE);
}

static BOOL cpu_init_haxm()
{
	hSystem = CreateFileW(L"\\\\.\\HAX", 0, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
	if (hSystem == INVALID_HANDLE_VALUE)
	{
		HAXMVM_ERRF("HAXM is not installed.");
		return FALSE;
	}
	struct hax_module_version ver;
	DWORD bytes;
	if (!DeviceIoControl(hSystem, HAX_IOCTL_VERSION, NULL, 0, &ver, sizeof(ver), &bytes, NULL))
	{
		HAXMVM_ERRF("VERSION");
		return FALSE;
	}
	struct hax_capabilityinfo cap;
	if (!DeviceIoControl(hSystem, HAX_IOCTL_CAPABILITY, NULL, 0, &cap, sizeof(cap), &bytes, NULL))
	{
		HAXMVM_ERRF("CAPABILITY");
		return FALSE;
	}
	if ((cap.wstatus & HAX_CAP_WORKSTATUS_MASK) == HAX_CAP_STATUS_NOTWORKING)
	{
		HAXMVM_ERRF("Hax is disabled\n");
		return FALSE;
	}
	if (!(cap.winfo & HAX_CAP_UG))
	{
		HAXMVM_ERRF("CPU unrestricted guest support required");
		return FALSE;
	}
	
	uint32_t vm_id;
	if (!DeviceIoControl(hSystem, HAX_IOCTL_CREATE_VM, NULL, 0, &vm_id, sizeof(vm_id), &bytes, NULL))
	{
		HAXMVM_ERRF("CREATE_VM");
		return FALSE;
	}
	WCHAR buf[1000];
	swprintf_s(buf, RTL_NUMBER_OF(buf), L"\\\\.\\hax_vm%02d", vm_id);
	hVM = CreateFileW(buf, 0, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
	if (hVM == INVALID_HANDLE_VALUE)
	{
		HAXMVM_ERRF("Could not create vm.");
		return FALSE;
	}
	uint32_t vcpu_id;
	struct hax_qemu_version verq;
	/* 3~ enable fast mmio */
	verq.cur_version = 1;
	verq.least_version = 0;
	if (!DeviceIoControl(hVM, HAX_VM_IOCTL_NOTIFY_QEMU_VERSION, &verq, sizeof(verq), NULL, 0, &bytes, NULL))
	{
	}
	vcpu_id = 1;
	if (!DeviceIoControl(hVM, HAX_VM_IOCTL_VCPU_CREATE, &vcpu_id, sizeof(vcpu_id), NULL, 0, &bytes, NULL))
	{
		HAXMVM_ERRF("could not create vcpu.");
		return FALSE;
	}
	swprintf_s(buf, RTL_NUMBER_OF(buf), L"\\\\.\\hax_vm%02d_vcpu%02d", vm_id, vcpu_id);
	hVCPU = CreateFileW(buf, 0, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
	struct hax_tunnel_info tunnel_info;
	if (!DeviceIoControl(hVCPU, HAX_VCPU_IOCTL_SETUP_TUNNEL, NULL, 0, &tunnel_info, sizeof(tunnel_info), &bytes, NULL))
	{
		HAXMVM_ERRF("SETUP_TUNNEL");
		return FALSE;
	}
	/* memory mapping */
	struct hax_alloc_ram_info alloc_ram = { 0 };
	struct hax_set_ram_info ram = { 0 };
	alloc_ram.size = MAX_MEM; 
	alloc_ram.va = (uint64_t)mem;
	if (!DeviceIoControl(hVM, HAX_VM_IOCTL_ALLOC_RAM, &alloc_ram, sizeof(alloc_ram), NULL, 0, &bytes, NULL))
	{
		HAXMVM_ERRF("ALLOC_RAM");
		return FALSE;
	}
	ram.pa_start = 0;
	ram.size = MEMORY_END;
	ram.va = (uint64_t)mem;
	if (!DeviceIoControl(hVM, HAX_VM_IOCTL_SET_RAM, &ram, sizeof(ram), NULL, 0, &bytes, NULL))
	{
		HAXMVM_ERRF("SET_RAM");
		return FALSE;
	}
	ram.pa_start = UMB_TOP;
	ram.size = MAX_MEM - UMB_TOP;
	ram.va = (uint64_t)mem + UMB_TOP;
	if (!DeviceIoControl(hVM, HAX_VM_IOCTL_SET_RAM, &ram, sizeof(ram), NULL, 0, &bytes, NULL))
	{
		HAXMVM_ERRF("SET_RAM");
		return FALSE;
	}
	ram.pa_start = MEMORY_END; // TODO: adjust pages for ems and a20
	ram.size = UMB_TOP - MEMORY_END;
	ram.va = 0;
	ram.flags = HAX_RAM_INFO_INVALID;
	if (!DeviceIoControl(hVM, HAX_VM_IOCTL_SET_RAM, &ram, sizeof(ram), NULL, 0, &bytes, NULL))
	{
		HAXMVM_ERRF("SET_RAM");
		return FALSE;
	}
	tunnel = (struct hax_tunnel*)tunnel_info.va;
	iobuf = (char *)tunnel_info.io_va;
	DeviceIoControl(hVCPU, HAX_VCPU_GET_REGS, NULL, 0, &state, sizeof(state), &bytes, NULL);
	state._idt.limit = 0x400;

	CloseHandle(CreateThread(NULL, 0, cpu_int_th, NULL, 0, NULL));
	return TRUE;
}

static void cpu_reset_haxm()
{
}

static BOOL vm_exit()
{
	CloseHandle(hVCPU);
	CloseHandle(hVM);
	CloseHandle(hSystem);
	return TRUE;
}

const int TRANSLATE_READ            = 0;        // translate for read
const int TRANSLATE_WRITE           = 1;        // translate for write
const int TRANSLATE_FETCH           = 2;        // translate for instruction fetch

// TODO: mark pages dirty if necessary, check for page faults
// dos programs likly never used three level page tables
static int translate_address(int pl, int type, UINT32 *address, UINT32 *error)
{
	if(!(state._cr0 & 0x80000000))
		return TRUE;

	UINT32 *pdbr = (UINT32 *)(mem + (state._cr3 * 0xfffff000));
	UINT32 a = *address;
	UINT32 dir = (a >> 22) & 0x3ff;
	UINT32 table = (a >> 12) & 0x3ff;
	UINT32 page_dir = pdbr[dir];
	if(page_dir & 1)
	{
		if((page_dir & 0x80) && (state._cr4 & 0x10))
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
		base = state._ldt.base;
		limit = state._ldt.limit;
	}
	else
	{
		base = state._gdt.base;
		limit = state._gdt.limit;
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
		base = state._ldt.base;
	else
		base = state._gdt.base;

	addr = base + (selector & ~7) + 5;
	translate_address(0, TRANSLATE_READ, &addr, NULL); 
	mem[addr] |= 1;
}

static void load_segdesc(segment_desc_t &seg)
{
	if (PROTECTED_MODE)
	{
		if (!V8086_MODE)
		{
			i386_load_protected_mode_segment((I386_SREG *)&seg, NULL);
			if(seg.selector)
				i386_set_descriptor_accessed(seg.selector);
		}
		else
		{
			seg.base = seg.selector << 4;
			seg.limit = 0xffff;
			seg.ar = (&seg == &state._cs) ? 0xfb : 0xf3;
		}
	}
	else
	{
		seg.base = seg.selector << 4;
		if(&seg == &state._cs)
			seg.ar = 0x93;
	}
}

// TODO: check ss limit
static UINT32 i386_read_stack(bool dword = false)
{
	UINT32 addr = state._ss.base;
	if(state._ss.operand_size)
		addr += REG32(ESP);
	else
		addr += REG16(SP) & 0xffff;
	translate_address(0, TRANSLATE_READ, &addr, NULL);
	return dword ? read_dword(addr) : read_word(addr);
}

static void i386_write_stack(UINT32 value, bool dword = false)
{
	UINT32 addr = state._ss.base;
	if(state._ss.operand_size)
		addr += REG32(ESP);
	else
		addr += REG16(SP);
	translate_address(0, TRANSLATE_WRITE, &addr, NULL);
	dword ? write_dword(addr, value) : write_word(addr, value);
}

static void PUSH16(UINT16 val)
{
	if(state._ss.operand_size)
		REG32(ESP) -= 2;
	else
		REG16(SP) = (REG16(SP) - 2) & 0xffff;
	i386_write_stack(val);
}

static UINT16 POP16()
{
	UINT16 val = i386_read_stack();
	if(state._ss.operand_size)
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
			haxmvm_panic("i386_call_far in protected mode and !v86mode not supported");
		else
		{
			if((state._cr0 & 0x80000000) && (selector == DUMMY_TOP))  // check that this is mapped
			{
				UINT32 addr = DUMMY_TOP + address;
				translate_address(0, TRANSLATE_READ, &addr, NULL);
				if (address != (DUMMY_TOP + address))
					haxmvm_panic("i386_call_far to dummy segment with page unmapped");
			}
		}
	}
	PUSH16(state._cs.selector);
	PUSH16(state._eip);
	state._cs.selector = selector;
	load_segdesc(state._cs);
	state._eip = address;
}

static void i386_jmp_far(UINT16 selector, UINT32 address)
{
	if (PROTECTED_MODE && !V8086_MODE)
		haxmvm_panic("i386_jmp_far in protected mode and !v86mode not supported");
	state._cs.selector = selector;
	load_segdesc(state._cs);
	state._eip = address;
}

static void i386_pushf()
{
	PUSH16(state._eflags);
}

static void i386_retf16()
{
	if (PROTECTED_MODE && !V8086_MODE)
		haxmvm_panic("i386_retf16 in protected mode and !v86mode not supported");
	state._eip = POP16();
	state._cs.selector = POP16();
	load_segdesc(state._cs);

}

static void i386_iret16()
{
	if (PROTECTED_MODE && !V8086_MODE)
		haxmvm_panic("i386_retf16 in protected mode and !v86mode not supported");
	state._eip = POP16();
	state._cs.selector = POP16();
	load_segdesc(state._cs);
	state._eflags = POP16();
}

static void i386_set_a20_line(int state)
{
	// TODO: implement this with page mapping
}

static void i386_trap(int irq, int irq_gate, int trap_level)
{
	DWORD bytes;
	if (tunnel->ready_for_interrupt_injection)
		DeviceIoControl(hVCPU, HAX_VCPU_IOCTL_INTERRUPT, NULL, 0, &irq, sizeof(irq), &bytes, NULL);
	else
	{
		saved_vector = irq;
		tunnel->request_interrupt_window = 1;
	}
}

static void i386_set_irq_line(int irqline, int state)
{
	if (state)
	{
		if (tunnel->ready_for_interrupt_injection)
		{
			DWORD bytes, irq = pic_ack();
			DeviceIoControl(hVCPU, HAX_VCPU_IOCTL_INTERRUPT, NULL, 0, &irq, sizeof(irq), &bytes, NULL);
		}
		else
		{
			saved_vector = -2;
			tunnel->request_interrupt_window = 1;
		}
	}
}

static void cpu_execute_haxm()
{
	DWORD bytes;
	while (TRUE)
	{
		state._eflags = (state._eflags & ~0x2e2c1) | (m_VM << 17) | (m_NT << 14) | (m_IOP2 << 13) | (m_IOP1 << 12)
							   | (m_IF << 9) | (m_SF << 7) | (m_ZF << 6) | m_CF;
		m_prev_eip = state._eip;
		if (!DeviceIoControl(hVCPU, HAX_VCPU_SET_REGS, &state, sizeof(state), NULL, 0, &bytes, NULL))
			HAXMVM_ERRF("SET_REGS");
		if (!DeviceIoControl(hVCPU, HAX_VCPU_IOCTL_RUN, NULL, 0, NULL, 0, &bytes, NULL))
			return;
		DeviceIoControl(hVCPU, HAX_VCPU_GET_REGS, NULL, 0, &state, sizeof(state), &bytes, NULL);
		m_CF = state._eflags & 1;
		m_ZF = (state._eflags & 0x40) ? 1 : 0;
		m_SF = (state._eflags & 0x80) ? 1 : 0;
		m_IF = (state._eflags & 0x200) ? 1 : 0;
		m_IOP1 = (state._eflags & 0x1000) ? 1 : 0;
		m_IOP2 = (state._eflags & 0x2000) ? 1 : 0;
		m_NT = (state._eflags & 0x4000) ? 1 : 0;
		m_VM = (state._eflags & 0x20000) ? 1 : 0;

		switch(tunnel->_exit_status)
		{
			case HAX_EXIT_IO:
				if(!(tunnel->io._flags & 1))
				{
					switch(tunnel->io._size)
					{
						case 1:
							if(tunnel->io._direction == HAX_IO_IN)
								state._al = read_io_byte(tunnel->io._port);
							else
								write_io_byte(tunnel->io._port, state._al);
							break;
						case 2:
							if(tunnel->io._direction == HAX_IO_IN)
								state._ax = read_io_word(tunnel->io._port);
							else
								write_io_word(tunnel->io._port, state._ax);
							break;
						case 4:
							if(tunnel->io._direction == HAX_IO_IN)
								state._eax = read_io_dword(tunnel->io._port);
							else
								write_io_dword(tunnel->io._port, state._eax);
							break;
					}
				}
				else
				{
					char* addr = iobuf;
					addr -= tunnel->io._df ? tunnel->io._count * tunnel->io._size : 0;
					for(int i = 0; i < tunnel->io._count; i++)
					{
						addr = tunnel->io._df ? addr - tunnel->io._size : addr + tunnel->io._size;
						switch(tunnel->io._size)
						{
							case 1:
								if(tunnel->io._direction == HAX_IO_OUT)
									write_io_byte(tunnel->io._port, *(UINT8 *)addr);
								else
									*(UINT8 *)addr = read_io_byte(tunnel->io._port);
								break;
							case 2:
								if(tunnel->io._direction == HAX_IO_OUT)
									write_io_word(tunnel->io._port, *(UINT16 *)addr);
								else
									*(UINT16 *)addr = read_io_word(tunnel->io._port);
								break;
							case 4:
								if(tunnel->io._direction == HAX_IO_OUT)
									write_io_dword(tunnel->io._port, *(UINT32 *)addr);
								else
									*(UINT32 *)addr = read_io_dword(tunnel->io._port);
								break;
						}
					}
				}
#ifdef EXPORT_DEBUG_TO_FILE
				fflush(fp_debug_log);
#endif
				continue;
			case HAX_EXIT_FAST_MMIO:
			{
				struct hax_fastmmio *hft = (struct hax_fastmmio *)iobuf;
				UINT32 val = (hft->direction == 1) ? hft->value : 0;
				UINT32 gpaw = (hft->direction == 2) ? hft->gpa2 : hft->gpa;
				if(hft->direction != 1)
				{
					switch(hft->size)
					{
						case 1:
							val = read_byte(hft->gpa);
							break;
						case 2:
							val = read_word(hft->gpa);
							break;
						case 4:
							val = read_dword(hft->gpa);
							break;
					}
				}
				if(hft->direction != 0)
				{
					switch(hft->size)
					{
						case 1:
							write_byte(gpaw, val);
							break;
						case 2:
							write_word(gpaw, val);
							break;
						case 4:
							write_dword(gpaw, val);
							break;
					}
				}
				continue;
			}
			case HAX_EXIT_HLT:
			{
				offs_t hltaddr = state._cs.base + state._eip - 1;
				translate_address(0, TRANSLATE_READ, &hltaddr, NULL);
				if((hltaddr > IRET_TOP) && (hltaddr < (IRET_TOP + IRET_SIZE)))
				{
					int syscall = hltaddr - IRET_TOP;
					i386_iret16();
					if(syscall == 0xf8)
						hardware_update();
					else
						msdos_syscall(syscall);
#ifdef EXPORT_DEBUG_TO_FILE
					fflush(fp_debug_log);
#endif
					continue;
				}
				else if(hltaddr == 0xffff1)
				{
					m_exit = 1;
					return;
				}
				else if((hltaddr > DUMMY_TOP) && (hltaddr < 0xffff0))
					return;
				else 
					haxmvm_panic("handle hlt");
			}
			case HAX_EXIT_STATECHANGE:
				haxmvm_panic("hypervisor is panicked!!!");
				return;
			case HAX_EXIT_INTERRUPT:
				tunnel->request_interrupt_window = 0;
				hardware_update();
				if(saved_vector == -1)
					continue; // ?
				if(saved_vector == -2)
					saved_vector = pic_ack();
				DeviceIoControl(hVCPU, HAX_VCPU_IOCTL_INTERRUPT, NULL, 0, &saved_vector, sizeof(saved_vector), &bytes, NULL);
				saved_vector = -1;
				continue;
			default:
				HAXMVM_ERRF("exit status: %d %04x:%04x", tunnel->_exit_status, state._cs.selector, state._eip);
				return;
		}
	}
}

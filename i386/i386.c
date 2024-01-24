/*
    Intel 386 emulator

    Written by Ville Linde

    Currently supports:
        Intel 386
        Intel 486
        Intel Pentium
        Cyrix MediaGX
        Intel Pentium MMX
        Intel Pentium Pro
        Intel Pentium II
        Intel Pentium III
        Intel Pentium 4
*/

//#include "emu.h"
//#include "debugger.h"
#include "i386priv.h"
//#include "i386.h"

//#include "debug/debugcpu.h"

/* seems to be defined on mingw-gcc */
#undef i386

static CPU_RESET( CPU_MODEL );

int i386_parity_table[256];
MODRM_TABLE i386_MODRM_table[256];

static void i386_trap_with_error(i386_state* cpustate, int irq, int irq_gate, int trap_level, UINT32 err);
static void i286_task_switch(i386_state* cpustate, UINT16 selector, UINT8 nested);
static void i386_task_switch(i386_state* cpustate, UINT16 selector, UINT8 nested);

#define FAULT(fault,error) {cpustate->ext = 1; i386_trap_with_error(cpustate,fault,0,0,error); return;}
#define FAULT_EXP(fault,error) {cpustate->ext = 1; i386_trap_with_error(cpustate,fault,0,trap_level+1,error); return;}

/*************************************************************************/

#define INT_DEBUG	1

static UINT32 i386_load_protected_mode_segment(i386_state *cpustate, I386_SREG *seg, UINT64 *desc )
{
	UINT32 v1,v2;
	UINT32 base, limit;
	int entry;

	if ( seg->selector & 0x4 )
	{
		base = cpustate->ldtr.base;
		limit = cpustate->ldtr.limit;
	} else {
		base = cpustate->gdtr.base;
		limit = cpustate->gdtr.limit;
	}

	entry = seg->selector & ~0x7;
	if (limit == 0 || entry + 7 > limit)
		return 0;

	v1 = READ32PL0(cpustate, base + entry );
	v2 = READ32PL0(cpustate, base + entry + 4 );

	seg->flags = (v2 >> 8) & 0xf0ff;
	seg->base = (v2 & 0xff000000) | ((v2 & 0xff) << 16) | ((v1 >> 16) & 0xffff);
	seg->limit = (v2 & 0xf0000) | (v1 & 0xffff);
	if (seg->flags & 0x8000)
		seg->limit = (seg->limit << 12) | 0xfff;
	seg->d = (seg->flags & 0x4000) ? 1 : 0;
	seg->valid = (seg->selector & ~3)?(true):(false);

	if(desc)
		*desc = ((UINT64)v2<<32)|v1;
	return 1;
}

static void i386_load_call_gate(i386_state* cpustate, I386_CALL_GATE *gate)
{
	UINT32 v1,v2;
	UINT32 base,limit;
	int entry;

	if ( gate->segment & 0x4 )
	{
		base = cpustate->ldtr.base;
		limit = cpustate->ldtr.limit;
	} else {
		base = cpustate->gdtr.base;
		limit = cpustate->gdtr.limit;
	}

	entry = gate->segment & ~0x7;
	if (limit == 0 || entry + 7 > limit)
		return;

	v1 = READ32PL0(cpustate, base + entry );
	v2 = READ32PL0(cpustate, base + entry + 4 );

	/* Note that for task gates, offset and dword_count are not used */
	gate->selector = (v1 >> 16) & 0xffff;
	gate->offset = (v1 & 0x0000ffff) | (v2 & 0xffff0000);
	gate->ar = (v2 >> 8) & 0xff;
	gate->dword_count = v2 & 0x001f;
	gate->present = (gate->ar >> 7) & 0x01;
	gate->dpl = (gate->ar >> 5) & 0x03;
}

static void i386_set_descriptor_accessed(i386_state *cpustate, UINT16 selector)
{
	// assume the selector is valid, we don't need to check it again
	UINT32 base, addr, error;
	UINT8 rights;
	if(!(selector & ~3))
		return;

	if ( selector & 0x4 )
		base = cpustate->ldtr.base;
	else
		base = cpustate->gdtr.base;

	addr = base + (selector & ~7) + 5;
	translate_address(cpustate, -2, &addr, &error);
	rights = /*cpustate->program->*/read_byte(addr);
	// Should a fault be thrown if the table is read only?
	/*cpustate->program->*/write_byte(addr, rights | 1);
}

static void i386_load_segment_descriptor(i386_state *cpustate, int segment )
{
	if (PROTECTED_MODE)
	{
		if (!V8086_MODE)
		{
			i386_load_protected_mode_segment(cpustate, &cpustate->sreg[segment], NULL );
			i386_set_descriptor_accessed(cpustate, cpustate->sreg[segment].selector);
		}
		else
		{
			cpustate->sreg[segment].base = cpustate->sreg[segment].selector << 4;
			cpustate->sreg[segment].limit = 0xffff;
			cpustate->sreg[segment].flags = (segment == CS) ? 0x009a : 0x0092;
			cpustate->sreg[segment].d = 0;
			cpustate->sreg[segment].valid = true;
		}
	}
	else
	{
		cpustate->sreg[segment].base = cpustate->sreg[segment].selector << 4;
		cpustate->sreg[segment].d = 0;
		cpustate->sreg[segment].valid = true;

		if( segment == CS && !cpustate->performed_intersegment_jump )
			cpustate->sreg[segment].base |= 0xfff00000;
	}
}

/* Retrieves the stack selector located in the current TSS */
static UINT32 i386_get_stack_segment(i386_state* cpustate, UINT8 privilege)
{
	UINT32 ret;
	if(privilege >= 3)
		return 0;

	if(cpustate->task.flags & 8)
		ret = READ32PL0(cpustate,(cpustate->task.base+8) + (8*privilege));
	else
		ret = READ16PL0(cpustate,(cpustate->task.base+4) + (4*privilege));

	return ret;
}

/* Retrieves the stack pointer located in the current TSS */
static UINT32 i386_get_stack_ptr(i386_state* cpustate, UINT8 privilege)
{
	UINT32 ret;
	if(privilege >= 3)
		return 0;

	if(cpustate->task.flags & 8)
		ret = READ32PL0(cpustate,(cpustate->task.base+4) + (8*privilege));
	else
		ret = READ16PL0(cpustate,(cpustate->task.base+2) + (4*privilege));

	return ret;
}

static UINT32 get_flags(i386_state *cpustate)
{
	UINT32 f = 0x2;
	f |= cpustate->CF;
	f |= cpustate->PF << 2;
	f |= cpustate->AF << 4;
	f |= cpustate->ZF << 6;
	f |= cpustate->SF << 7;
	f |= cpustate->TF << 8;
	f |= cpustate->IF << 9;
	f |= cpustate->DF << 10;
	f |= cpustate->OF << 11;
	f |= cpustate->IOP1 << 12;
	f |= cpustate->IOP2 << 13;
	f |= cpustate->NT << 14;
	f |= cpustate->RF << 16;
	f |= cpustate->VM << 17;
	f |= cpustate->AC << 18;
	f |= cpustate->VIF << 19;
	f |= cpustate->VIP << 20;
	f |= cpustate->ID << 21;
	return (cpustate->eflags & ~cpustate->eflags_mask) | (f & cpustate->eflags_mask);
}

static void set_flags(i386_state *cpustate, UINT32 f )
{
	cpustate->CF = (f & 0x1) ? 1 : 0;
	cpustate->PF = (f & 0x4) ? 1 : 0;
	cpustate->AF = (f & 0x10) ? 1 : 0;
	cpustate->ZF = (f & 0x40) ? 1 : 0;
	cpustate->SF = (f & 0x80) ? 1 : 0;
	cpustate->TF = (f & 0x100) ? 1 : 0;
	cpustate->IF = (f & 0x200) ? 1 : 0;
	cpustate->DF = (f & 0x400) ? 1 : 0;
	cpustate->OF = (f & 0x800) ? 1 : 0;
	cpustate->IOP1 = (f & 0x1000) ? 1 : 0;
	cpustate->IOP2 = (f & 0x2000) ? 1 : 0;
	cpustate->NT = (f & 0x4000) ? 1 : 0;
	cpustate->RF = (f & 0x10000) ? 1 : 0;
	cpustate->VM = (f & 0x20000) ? 1 : 0;
	cpustate->AC = (f & 0x40000) ? 1 : 0;
	cpustate->VIF = (f & 0x80000) ? 1 : 0;
	cpustate->VIP = (f & 0x100000) ? 1 : 0;
	cpustate->ID = (f & 0x200000) ? 1 : 0;
	cpustate->eflags = f & cpustate->eflags_mask;
}

static void sib_byte(i386_state *cpustate,UINT8 mod, UINT32* out_ea, UINT8* out_segment)
{
	UINT32 ea = 0;
	UINT8 segment = 0;
	UINT8 scale, i, base;
	UINT8 sib = FETCH(cpustate);
	scale = (sib >> 6) & 0x3;
	i = (sib >> 3) & 0x7;
	base = sib & 0x7;

	switch( base )
	{
		case 0: ea = REG32(EAX); segment = DS; break;
		case 1: ea = REG32(ECX); segment = DS; break;
		case 2: ea = REG32(EDX); segment = DS; break;
		case 3: ea = REG32(EBX); segment = DS; break;
		case 4: ea = REG32(ESP); segment = SS; break;
		case 5:
			if( mod == 0 ) {
				ea = FETCH32(cpustate);
				segment = DS;
			} else if( mod == 1 ) {
				ea = REG32(EBP);
				segment = SS;
			} else if( mod == 2 ) {
				ea = REG32(EBP);
				segment = SS;
			}
			break;
		case 6: ea = REG32(ESI); segment = DS; break;
		case 7: ea = REG32(EDI); segment = DS; break;
	}
	switch( i )
	{
		case 0: ea += REG32(EAX) * (1 << scale); break;
		case 1: ea += REG32(ECX) * (1 << scale); break;
		case 2: ea += REG32(EDX) * (1 << scale); break;
		case 3: ea += REG32(EBX) * (1 << scale); break;
		case 4: break;
		case 5: ea += REG32(EBP) * (1 << scale); break;
		case 6: ea += REG32(ESI) * (1 << scale); break;
		case 7: ea += REG32(EDI) * (1 << scale); break;
	}
	*out_ea = ea;
	*out_segment = segment;
}

static void modrm_to_EA(i386_state *cpustate,UINT8 mod_rm, UINT32* out_ea, UINT8* out_segment)
{
	INT8 disp8;
	INT16 disp16;
	INT32 disp32;
	UINT8 mod = (mod_rm >> 6) & 0x3;
	UINT8 rm = mod_rm & 0x7;
	UINT32 ea;
	UINT8 segment;

	if( mod_rm >= 0xc0 )
		fatalerror("i386: Called modrm_to_EA with modrm value %02X!\n",mod_rm);

	if( cpustate->address_size ) {
		switch( rm )
		{
			default:
			case 0: ea = REG32(EAX); segment = DS; break;
			case 1: ea = REG32(ECX); segment = DS; break;
			case 2: ea = REG32(EDX); segment = DS; break;
			case 3: ea = REG32(EBX); segment = DS; break;
			case 4: sib_byte(cpustate, mod, &ea, &segment ); break;
			case 5:
				if( mod == 0 ) {
					ea = FETCH32(cpustate); segment = DS;
				} else {
					ea = REG32(EBP); segment = SS;
				}
				break;
			case 6: ea = REG32(ESI); segment = DS; break;
			case 7: ea = REG32(EDI); segment = DS; break;
		}
		if( mod == 1 ) {
			disp8 = FETCH(cpustate);
			ea += (INT32)disp8;
		} else if( mod == 2 ) {
			disp32 = FETCH32(cpustate);
			ea += disp32;
		}

		if( cpustate->segment_prefix )
			segment = cpustate->segment_override;

		*out_ea = ea;
		*out_segment = segment;

	} else {
		switch( rm )
		{
			default:
			case 0: ea = REG16(BX) + REG16(SI); segment = DS; break;
			case 1: ea = REG16(BX) + REG16(DI); segment = DS; break;
			case 2: ea = REG16(BP) + REG16(SI); segment = SS; break;
			case 3: ea = REG16(BP) + REG16(DI); segment = SS; break;
			case 4: ea = REG16(SI); segment = DS; break;
			case 5: ea = REG16(DI); segment = DS; break;
			case 6:
				if( mod == 0 ) {
					ea = FETCH16(cpustate); segment = DS;
				} else {
					ea = REG16(BP); segment = SS;
				}
				break;
			case 7: ea = REG16(BX); segment = DS; break;
		}
		if( mod == 1 ) {
			disp8 = FETCH(cpustate);
			ea += (INT32)disp8;
		} else if( mod == 2 ) {
			disp16 = FETCH16(cpustate);
			ea += (INT32)disp16;
		}

		if( cpustate->segment_prefix )
			segment = cpustate->segment_override;

		*out_ea = ea & 0xffff;
		*out_segment = segment;
	}
}

static UINT32 GetNonTranslatedEA(i386_state *cpustate,UINT8 modrm,UINT8 *seg)
{
	UINT8 segment;
	UINT32 ea;
	modrm_to_EA(cpustate, modrm, &ea, &segment );
	if(seg) *seg = segment;
	return ea;
}

static UINT32 GetEA(i386_state *cpustate,UINT8 modrm, int rwn)
{
	UINT8 segment;
	UINT32 ea;
	modrm_to_EA(cpustate, modrm, &ea, &segment );
	return i386_translate(cpustate, segment, ea, rwn );
}

/* Check segment register for validity when changing privilege level after an RETF */
static void i386_check_sreg_validity(i386_state* cpustate, int reg)
{
	UINT16 selector = cpustate->sreg[reg].selector;
	UINT8 CPL = cpustate->CPL;
	UINT8 DPL,RPL;
	I386_SREG desc;
	int invalid = 0;

	memset(&desc, 0, sizeof(desc));
	desc.selector = selector;
	i386_load_protected_mode_segment(cpustate,&desc,NULL);
	DPL = (desc.flags >> 5) & 0x03;  // descriptor privilege level
	RPL = selector & 0x03;

	/* Must be within the relevant descriptor table limits */
	if(selector & 0x04)
	{
		if((selector & ~0x07) > cpustate->ldtr.limit)
			invalid = 1;
	}
	else
	{
		if((selector & ~0x07) > cpustate->gdtr.limit)
			invalid = 1;
	}

	/* Must be either a data or readable code segment */
	if(((desc.flags & 0x0018) == 0x0018 && (desc.flags & 0x0002)) || (desc.flags & 0x0018) == 0x0010)
		invalid = 0;
	else
		invalid = 1;

	/* If a data segment or non-conforming code segment, then either DPL >= CPL or DPL >= RPL */
	if(((desc.flags & 0x0018) == 0x0018 && (desc.flags & 0x0004) == 0) || (desc.flags & 0x0018) == 0x0010)
	{
		if((DPL < CPL) || (DPL < RPL))
			invalid = 1;
	}

	/* if segment is invalid, then segment register is nulled */
	if(invalid != 0)
	{
		cpustate->sreg[reg].selector = 0;
		i386_load_segment_descriptor(cpustate,reg);
	}
}

static int i386_limit_check(i386_state *cpustate, int seg, UINT32 offset)
{
	if(PROTECTED_MODE && !V8086_MODE)
	{
		if((cpustate->sreg[seg].flags & 0x0018) == 0x0010 && cpustate->sreg[seg].flags & 0x0004) // if expand-down data segment
		{
			// compare if greater then 0xffffffff when we're passed the access size
			if((offset <= cpustate->sreg[seg].limit) || ((cpustate->sreg[seg].d)?0:(offset > 0xffff)))
			{
				logerror("Limit check at 0x%08x failed. Segment %04x, limit %08x, offset %08x (expand-down)\n",cpustate->pc,cpustate->sreg[seg].selector,cpustate->sreg[seg].limit,offset);
				return 1;
			}
		}
		else
		{
			if(offset > cpustate->sreg[seg].limit)
			{
				logerror("Limit check at 0x%08x failed. Segment %04x, limit %08x, offset %08x\n",cpustate->pc,cpustate->sreg[seg].selector,cpustate->sreg[seg].limit,offset);
				return 1;
			}
		}
	}
	return 0;
}

static void i386_sreg_load(i386_state *cpustate, UINT16 selector, UINT8 reg, bool *fault)
{
	// Checks done when MOV changes a segment register in protected mode
	UINT8 CPL,RPL,DPL;

	CPL = cpustate->CPL;
	RPL = selector & 0x0003;

	if(!PROTECTED_MODE || V8086_MODE)
	{
		cpustate->sreg[reg].selector = selector;
		i386_load_segment_descriptor(cpustate, reg);
		if(fault) *fault = false;
		return;
	}

	if(fault) *fault = true;
	if(reg == SS)
	{
		I386_SREG stack;

		memset(&stack, 0, sizeof(stack));
		stack.selector = selector;
		i386_load_protected_mode_segment(cpustate,&stack,NULL);
		DPL = (stack.flags >> 5) & 0x03;

		if((selector & ~0x0003) == 0)
		{
			logerror("SReg Load (%08x): Selector is null.\n",cpustate->pc);
			FAULT(FAULT_GP,0)
		}
		if(selector & 0x0004)  // LDT
		{
			if((selector & ~0x0007) > cpustate->ldtr.limit)
			{
				logerror("SReg Load (%08x): Selector is out of LDT bounds.\n",cpustate->pc);
				FAULT(FAULT_GP,selector & ~0x03)
			}
		}
		else  // GDT
		{
			if((selector & ~0x0007) > cpustate->gdtr.limit)
			{
				logerror("SReg Load (%08x): Selector is out of GDT bounds.\n",cpustate->pc);
				FAULT(FAULT_GP,selector & ~0x03)
			}
		}
		if (RPL != CPL)
		{
			logerror("SReg Load (%08x): Selector RPL does not equal CPL.\n",cpustate->pc);
			FAULT(FAULT_GP,selector & ~0x03)
		}
		if(((stack.flags & 0x0018) != 0x10) && (stack.flags & 0x0002) != 0)
		{
			logerror("SReg Load (%08x): Segment is not a writable data segment.\n",cpustate->pc);
			FAULT(FAULT_GP,selector & ~0x03)
		}
		if(DPL != CPL)
		{
			logerror("SReg Load (%08x): Segment DPL does not equal CPL.\n",cpustate->pc);
			FAULT(FAULT_GP,selector & ~0x03)
		}
		if(!(stack.flags & 0x0080))
		{
			logerror("SReg Load (%08x): Segment is not present.\n",cpustate->pc);
			FAULT(FAULT_SS,selector & ~0x03)
		}
	}
	if(reg == DS || reg == ES || reg == FS || reg == GS)
	{
		I386_SREG desc;

		if((selector & ~0x0003) == 0)
		{
			cpustate->sreg[reg].selector = selector;
			i386_load_segment_descriptor(cpustate, reg );
			if(fault) *fault = false;
			return;
		}

		memset(&desc, 0, sizeof(desc));
		desc.selector = selector;
		i386_load_protected_mode_segment(cpustate,&desc,NULL);
		DPL = (desc.flags >> 5) & 0x03;

		if(selector & 0x0004)  // LDT
		{
			if((selector & ~0x0007) > cpustate->ldtr.limit)
			{
				logerror("SReg Load (%08x): Selector is out of LDT bounds.\n",cpustate->pc);
				FAULT(FAULT_GP,selector & ~0x03)
			}
		}
		else  // GDT
		{
			if((selector & ~0x0007) > cpustate->gdtr.limit)
			{
				logerror("SReg Load (%08x): Selector is out of GDT bounds.\n",cpustate->pc);
				FAULT(FAULT_GP,selector & ~0x03)
			}
		}
		if((desc.flags & 0x0018) != 0x10)
		{
			if((((desc.flags & 0x0002) != 0) && ((desc.flags & 0x0018) != 0x18)) || !(desc.flags & 0x10))
			{
				logerror("SReg Load (%08x): Segment is not a data segment or readable code segment.\n",cpustate->pc);
				FAULT(FAULT_GP,selector & ~0x03)
			}
		}
		if(((desc.flags & 0x0018) == 0x10) || ((!(desc.flags & 0x0004)) && ((desc.flags & 0x0018) == 0x18)))
		{
			// if data or non-conforming code segment
			if((RPL > DPL) || (CPL > DPL))
			{
				logerror("SReg Load (%08x): Selector RPL or CPL is not less or equal to segment DPL.\n",cpustate->pc);
				FAULT(FAULT_GP,selector & ~0x03)
			}
		}
		if(!(desc.flags & 0x0080))
		{
			logerror("SReg Load (%08x): Segment is not present.\n",cpustate->pc);
			FAULT(FAULT_NP,selector & ~0x03)
		}
	}

	cpustate->sreg[reg].selector = selector;
	i386_load_segment_descriptor(cpustate, reg );
	if(fault) *fault = false;
}

static void i386_trap(i386_state *cpustate,int irq, int irq_gate, int trap_level)
{
	/*  I386 Interrupts/Traps/Faults:
     *
     *  0x00    Divide by zero
     *  0x01    Debug exception
     *  0x02    NMI
     *  0x03    Int3
     *  0x04    Overflow
     *  0x05    Array bounds check
     *  0x06    Illegal Opcode
     *  0x07    FPU not available
     *  0x08    Double fault
     *  0x09    Coprocessor segment overrun
     *  0x0a    Invalid task state
     *  0x0b    Segment not present
     *  0x0c    Stack exception
     *  0x0d    General Protection Fault
     *  0x0e    Page fault
     *  0x0f    Reserved
     *  0x10    Coprocessor error
     */
	UINT32 v1, v2;
	UINT32 offset, oldflags = get_flags(cpustate);
	UINT16 segment;
	int entry = irq * (PROTECTED_MODE ? 8 : 4);
	int SetRPL = 0;

	if( !(PROTECTED_MODE) )
	{
		/* 16-bit */
		PUSH16(cpustate, oldflags & 0xffff );
		PUSH16(cpustate, cpustate->sreg[CS].selector );
		if(irq == 3 || irq == 4 || irq == 9 || irq_gate == 1)
			PUSH16(cpustate, cpustate->eip );
		else
			PUSH16(cpustate, cpustate->prev_eip );

		cpustate->sreg[CS].selector = READ16(cpustate, cpustate->idtr.base + entry + 2 );
		cpustate->eip = READ16(cpustate, cpustate->idtr.base + entry );

		cpustate->TF = 0;
		cpustate->IF = 0;
	}
	else
	{
		int type;
		UINT16 flags;
		I386_SREG desc;
		UINT8 CPL = cpustate->CPL, DPL = 0; //, RPL = 0;

		/* 32-bit */
		v1 = READ32PL0(cpustate, cpustate->idtr.base + entry );
		v2 = READ32PL0(cpustate, cpustate->idtr.base + entry + 4 );
		offset = (v2 & 0xffff0000) | (v1 & 0xffff);
		segment = (v1 >> 16) & 0xffff;
		type = (v2>>8) & 0x1F;
		flags = (v2>>8) & 0xf0ff;

		if(trap_level == 2)
		{
			logerror("IRQ: Double fault.\n");
			FAULT_EXP(FAULT_DF,0);
		}
		if(trap_level >= 3)
		{
			logerror("IRQ: Triple fault. CPU reset.\n");
			CPU_RESET_CALL(CPU_MODEL);
			return;
		}

		/* segment privilege checks */
		if(entry >= cpustate->idtr.limit)
		{
			logerror("IRQ (%08x): Vector %02xh is past IDT limit.\n",cpustate->pc,entry);
			FAULT_EXP(FAULT_GP,entry+2)
		}
		/* segment must be interrupt gate, trap gate, or task gate */
		if(type != 0x05 && type != 0x06 && type != 0x07 && type != 0x0e && type != 0x0f)
		{
			logerror("IRQ#%02x (%08x): Vector segment %04x is not an interrupt, trap or task gate.\n",irq,cpustate->pc,segment);
			FAULT_EXP(FAULT_GP,entry+2)
		}

		if(cpustate->ext == 0) // if software interrupt (caused by INT/INTO/INT3)
		{
			if(((flags >> 5) & 0x03) < CPL)
			{
				logerror("IRQ (%08x): Software IRQ - gate DPL is less than CPL.\n",cpustate->pc);
				FAULT_EXP(FAULT_GP,entry+2)
			}
		}

		if((flags & 0x0080) == 0)
		{
			logerror("IRQ: Vector segment is not present.\n");
			FAULT_EXP(FAULT_NP,entry+2)
		}

		if(type == 0x05)
		{
			/* Task gate */
			memset(&desc, 0, sizeof(desc));
			desc.selector = segment;
			i386_load_protected_mode_segment(cpustate,&desc,NULL);
			if(segment & 0x04)
			{
				logerror("IRQ: Task gate: TSS is not in the GDT.\n");
				FAULT_EXP(FAULT_TS,segment & ~0x03);
			}
			else
			{
				if(segment > cpustate->gdtr.limit)
				{
					logerror("IRQ: Task gate: TSS is past GDT limit.\n");
					FAULT_EXP(FAULT_TS,segment & ~0x03);
				}
			}
			if((desc.flags & 0x000f) != 0x09 && (desc.flags & 0x000f) != 0x01)
			{
				logerror("IRQ: Task gate: TSS is not an available TSS.\n");
				FAULT_EXP(FAULT_TS,segment & ~0x03);
			}
			if((desc.flags & 0x0080) == 0)
			{
				logerror("IRQ: Task gate: TSS is not present.\n");
				FAULT_EXP(FAULT_NP,segment & ~0x03);
			}
			if(!(irq == 3 || irq == 4 || irq == 9 || irq_gate == 1))
				cpustate->eip = cpustate->prev_eip;
			if(desc.flags & 0x08)
				i386_task_switch(cpustate,desc.selector,1);
			else
				i286_task_switch(cpustate,desc.selector,1);
			return;
		}
		else
		{
			/* Interrupt or Trap gate */
			memset(&desc, 0, sizeof(desc));
			desc.selector = segment;
			i386_load_protected_mode_segment(cpustate,&desc,NULL);
			CPL = cpustate->CPL;  // current privilege level
			DPL = (desc.flags >> 5) & 0x03;  // descriptor privilege level
//          RPL = segment & 0x03;  // requested privilege level

			if((segment & ~0x03) == 0)
			{
				logerror("IRQ: Gate segment is null.\n");
				FAULT_EXP(FAULT_GP,cpustate->ext)
			}
			if(segment & 0x04)
			{
				if((segment & ~0x07) > cpustate->ldtr.limit)
				{
					logerror("IRQ: Gate segment is past LDT limit.\n");
					FAULT_EXP(FAULT_GP,(segment & 0x03)+cpustate->ext)
				}
			}
			else
			{
				if((segment & ~0x07) > cpustate->gdtr.limit)
				{
					logerror("IRQ: Gate segment is past GDT limit.\n");
					FAULT_EXP(FAULT_GP,(segment & 0x03)+cpustate->ext)
				}
			}
			if((desc.flags & 0x0018) != 0x18)
			{
				logerror("IRQ: Gate descriptor is not a code segment.\n");
				FAULT_EXP(FAULT_GP,(segment & 0x03)+cpustate->ext)
			}
			if((desc.flags & 0x0080) == 0)
			{
				logerror("IRQ: Gate segment is not present.\n");
				FAULT_EXP(FAULT_NP,(segment & 0x03)+cpustate->ext)
			}
			if((desc.flags & 0x0004) == 0 && (DPL < CPL))
			{
				/* IRQ to inner privilege */
				I386_SREG stack;
				UINT32 newESP,oldSS,oldESP;

				if(V8086_MODE && DPL)
				{
					logerror("IRQ: Gate to CPL>0 from VM86 mode.\n");
					FAULT_EXP(FAULT_GP,segment & ~0x03);
				}
				/* Check new stack segment in TSS */
				memset(&stack, 0, sizeof(stack));
				stack.selector = i386_get_stack_segment(cpustate,DPL);
				i386_load_protected_mode_segment(cpustate,&stack,NULL);
				oldSS = cpustate->sreg[SS].selector;
				if(flags & 0x0008)
					oldESP = REG32(ESP);
				else
					oldESP = REG16(SP);
				if((stack.selector & ~0x03) == 0)
				{
					logerror("IRQ: New stack selector is null.\n");
					FAULT_EXP(FAULT_GP,cpustate->ext)
				}
				if(stack.selector & 0x04)
				{
					if((stack.selector & ~0x07) > cpustate->ldtr.base)
					{
						logerror("IRQ: New stack selector is past LDT limit.\n");
						FAULT_EXP(FAULT_TS,(stack.selector & ~0x03)+cpustate->ext)
					}
				}
				else
				{
					if((stack.selector & ~0x07) > cpustate->gdtr.base)
					{
						logerror("IRQ: New stack selector is past GDT limit.\n");
						FAULT_EXP(FAULT_TS,(stack.selector & ~0x03)+cpustate->ext)
					}
				}
				if((stack.selector & 0x03) != DPL)
				{
					logerror("IRQ: New stack selector RPL is not equal to code segment DPL.\n");
					FAULT_EXP(FAULT_TS,(stack.selector & ~0x03)+cpustate->ext)
				}
				if(((stack.flags >> 5) & 0x03) != DPL)
				{
					logerror("IRQ: New stack segment DPL is not equal to code segment DPL.\n");
					FAULT_EXP(FAULT_TS,(stack.selector & ~0x03)+cpustate->ext)
				}
				if(((stack.flags & 0x0018) != 0x10) && (stack.flags & 0x0002) != 0)
				{
					logerror("IRQ: New stack segment is not a writable data segment.\n");
					FAULT_EXP(FAULT_TS,(stack.selector & ~0x03)+cpustate->ext) // #TS(stack selector + EXT)
				}
				if((stack.flags & 0x0080) == 0)
				{
					logerror("IRQ: New stack segment is not present.\n");
					FAULT_EXP(FAULT_SS,(stack.selector & ~0x03)+cpustate->ext) // #TS(stack selector + EXT)
				}
				newESP = i386_get_stack_ptr(cpustate,DPL);
				if(type & 0x08) // 32-bit gate
				{
					if(newESP < (V8086_MODE?36:20))
					{
						logerror("IRQ: New stack has no space for return addresses.\n");
						FAULT_EXP(FAULT_SS,0)
					}
				}
				else // 16-bit gate
				{
					newESP &= 0xffff;
					if(newESP < (V8086_MODE?18:10))
					{
						logerror("IRQ: New stack has no space for return addresses.\n");
						FAULT_EXP(FAULT_SS,0)
					}
				}
				if(offset > desc.limit)
				{
					logerror("IRQ: New EIP is past code segment limit.\n");
					FAULT_EXP(FAULT_GP,0)
				}
				/* change CPL before accessing the stack */
				cpustate->CPL = DPL;
				/* check for page fault at new stack TODO: check if stack frame crosses page boundary */
				WRITE_TEST(cpustate, stack.base+newESP-1);
				/* Load new stack segment descriptor */
				cpustate->sreg[SS].selector = stack.selector;
				i386_load_protected_mode_segment(cpustate,&cpustate->sreg[SS],NULL);
				i386_set_descriptor_accessed(cpustate, stack.selector);
				if(flags & 0x0008)
					REG32(ESP) = i386_get_stack_ptr(cpustate,DPL);
				else
					REG16(SP) = i386_get_stack_ptr(cpustate,DPL);
				if(V8086_MODE)
				{
					logerror("IRQ (%08x): Interrupt during V8086 task\n",cpustate->pc);
					if(type & 0x08)
					{
						PUSH32(cpustate,cpustate->sreg[GS].selector & 0xffff);
						PUSH32(cpustate,cpustate->sreg[FS].selector & 0xffff);
						PUSH32(cpustate,cpustate->sreg[DS].selector & 0xffff);
						PUSH32(cpustate,cpustate->sreg[ES].selector & 0xffff);
					}
					else
					{
						PUSH16(cpustate,cpustate->sreg[GS].selector);
						PUSH16(cpustate,cpustate->sreg[FS].selector);
						PUSH16(cpustate,cpustate->sreg[DS].selector);
						PUSH16(cpustate,cpustate->sreg[ES].selector);
					}
					cpustate->sreg[GS].selector = 0;
					cpustate->sreg[FS].selector = 0;
					cpustate->sreg[DS].selector = 0;
					cpustate->sreg[ES].selector = 0;
					cpustate->VM = 0;
					i386_load_segment_descriptor(cpustate,GS);
					i386_load_segment_descriptor(cpustate,FS);
					i386_load_segment_descriptor(cpustate,DS);
					i386_load_segment_descriptor(cpustate,ES);
				}
				if(type & 0x08)
				{
					// 32-bit gate
					PUSH32(cpustate,oldSS);
					PUSH32(cpustate,oldESP);
				}
				else
				{
					// 16-bit gate
					PUSH16(cpustate,oldSS);
					PUSH16(cpustate,oldESP);
				}
				SetRPL = 1;
			}
			else
			{
				int stack_limit;
				if((desc.flags & 0x0004) || (DPL == CPL))
				{
					/* IRQ to same privilege */
					if(V8086_MODE)
					{
						logerror("IRQ: Gate to same privilege from VM86 mode.\n");
						FAULT_EXP(FAULT_GP,segment & ~0x03);
					}
					if(type == 0x0e || type == 0x0f)  // 32-bit gate
						stack_limit = 10;
					else
						stack_limit = 6;
					// TODO: Add check for error code (2 extra bytes)
					if(REG32(ESP) < stack_limit)
					{
						logerror("IRQ: Stack has no space left (needs %i bytes).\n",stack_limit);
						FAULT_EXP(FAULT_SS,0)
					}
					if(offset > desc.limit)
					{
						logerror("IRQ: Gate segment offset is past segment limit.\n");
						FAULT_EXP(FAULT_GP,0)
					}
					SetRPL = 1;
				}
				else
				{
					logerror("IRQ: Gate descriptor is non-conforming, and DPL does not equal CPL.\n");
					FAULT_EXP(FAULT_GP,segment)
				}
			}
		}

		if(type != 0x0e && type != 0x0f)  // if not 386 interrupt or trap gate
		{
			PUSH16(cpustate, oldflags & 0xffff );
			PUSH16(cpustate, cpustate->sreg[CS].selector );
			if(irq == 3 || irq == 4 || irq == 9 || irq_gate == 1)
				PUSH16(cpustate, cpustate->eip );
			else
				PUSH16(cpustate, cpustate->prev_eip );
		}
		else
		{
			PUSH32(cpustate, oldflags & 0x00ffffff );
			PUSH32(cpustate, cpustate->sreg[CS].selector );
			if(irq == 3 || irq == 4 || irq == 9 || irq_gate == 1)
				PUSH32(cpustate, cpustate->eip );
			else
				PUSH32(cpustate, cpustate->prev_eip );
		}
		if(SetRPL != 0)
			segment = (segment & ~0x03) | cpustate->CPL;
		cpustate->sreg[CS].selector = segment;
		cpustate->eip = offset;

		if(type == 0x0e || type == 0x06)
			cpustate->IF = 0;
		cpustate->TF = 0;
		cpustate->NT = 0;
	}

	i386_load_segment_descriptor(cpustate,CS);
	CHANGE_PC(cpustate,cpustate->eip);

}

static void i386_trap_with_error(i386_state *cpustate,int irq, int irq_gate, int trap_level, UINT32 error)
{
	i386_trap(cpustate,irq,irq_gate,trap_level);
	if(irq == 8 || irq == 10 || irq == 11 || irq == 12 || irq == 13 || irq == 14)
	{
		// for these exceptions, an error code is pushed onto the stack by the processor.
		// no error code is pushed for software interrupts, either.
		if(PROTECTED_MODE)
		{
			UINT32 entry = irq * 8;
			UINT32 v2,type;
			v2 = READ32PL0(cpustate, cpustate->idtr.base + entry + 4 );
			type = (v2>>8) & 0x1F;
			if(type == 5)
			{
				v2 = READ32PL0(cpustate, cpustate->idtr.base + entry);
				v2 = READ32PL0(cpustate, cpustate->gdtr.base + ((v2 >> 16) & 0xfff8) + 4);
				type = (v2>>8) & 0x1F;
			}
			if(type >= 9)
				PUSH32(cpustate,error);
			else
				PUSH16(cpustate,error);
		}
		else
			PUSH16(cpustate,error);
	}
}


static void i286_task_switch(i386_state *cpustate, UINT16 selector, UINT8 nested)
{
	UINT32 tss;
	I386_SREG seg;
	UINT16 old_task;
	UINT8 ar_byte;  // access rights byte

	/* TODO: Task State Segment privilege checks */

	/* For tasks that aren't nested, clear the busy bit in the task's descriptor */
	if(nested == 0)
	{
		if(cpustate->task.segment & 0x0004)
		{
			ar_byte = READ8(cpustate,cpustate->ldtr.base + (cpustate->task.segment & ~0x0007) + 5);
			WRITE8(cpustate,cpustate->ldtr.base + (cpustate->task.segment & ~0x0007) + 5,ar_byte & ~0x02);
		}
		else
		{
			ar_byte = READ8(cpustate,cpustate->gdtr.base + (cpustate->task.segment & ~0x0007) + 5);
			WRITE8(cpustate,cpustate->gdtr.base + (cpustate->task.segment & ~0x0007) + 5,ar_byte & ~0x02);
		}
	}

	/* Save the state of the current task in the current TSS (TR register base) */
	tss = cpustate->task.base;
	WRITE16(cpustate,tss+0x0e,cpustate->eip & 0x0000ffff);
	WRITE16(cpustate,tss+0x10,get_flags(cpustate) & 0x0000ffff);
	WRITE16(cpustate,tss+0x12,REG16(AX));
	WRITE16(cpustate,tss+0x14,REG16(CX));
	WRITE16(cpustate,tss+0x16,REG16(DX));
	WRITE16(cpustate,tss+0x18,REG16(BX));
	WRITE16(cpustate,tss+0x1a,REG16(SP));
	WRITE16(cpustate,tss+0x1c,REG16(BP));
	WRITE16(cpustate,tss+0x1e,REG16(SI));
	WRITE16(cpustate,tss+0x20,REG16(DI));
	WRITE16(cpustate,tss+0x22,cpustate->sreg[ES].selector);
	WRITE16(cpustate,tss+0x24,cpustate->sreg[CS].selector);
	WRITE16(cpustate,tss+0x26,cpustate->sreg[SS].selector);
	WRITE16(cpustate,tss+0x28,cpustate->sreg[DS].selector);

	old_task = cpustate->task.segment;

	/* Load task register with the selector of the incoming task */
	cpustate->task.segment = selector;
	memset(&seg, 0, sizeof(seg));
	seg.selector = cpustate->task.segment;
	i386_load_protected_mode_segment(cpustate,&seg,NULL);
	cpustate->task.limit = seg.limit;
	cpustate->task.base = seg.base;
	cpustate->task.flags = seg.flags;

	/* Set TS bit in CR0 */
	cpustate->cr[0] |= 0x08;

	/* Load incoming task state from the new task's TSS */
	tss = cpustate->task.base;
	cpustate->ldtr.segment = READ16(cpustate,tss+0x2a) & 0xffff;
	seg.selector = cpustate->ldtr.segment;
	i386_load_protected_mode_segment(cpustate,&seg,NULL);
	cpustate->ldtr.limit = seg.limit;
	cpustate->ldtr.base = seg.base;
	cpustate->ldtr.flags = seg.flags;
	cpustate->eip = READ16(cpustate,tss+0x0e);
	set_flags(cpustate,READ16(cpustate,tss+0x10));
	REG16(AX) = READ16(cpustate,tss+0x12);
	REG16(CX) = READ16(cpustate,tss+0x14);
	REG16(DX) = READ16(cpustate,tss+0x16);
	REG16(BX) = READ16(cpustate,tss+0x18);
	REG16(SP) = READ16(cpustate,tss+0x1a);
	REG16(BP) = READ16(cpustate,tss+0x1c);
	REG16(SI) = READ16(cpustate,tss+0x1e);
	REG16(DI) = READ16(cpustate,tss+0x20);
	cpustate->sreg[ES].selector = READ16(cpustate,tss+0x22) & 0xffff;
	i386_load_segment_descriptor(cpustate, ES);
	cpustate->sreg[CS].selector = READ16(cpustate,tss+0x24) & 0xffff;
	i386_load_segment_descriptor(cpustate, CS);
	cpustate->sreg[SS].selector = READ16(cpustate,tss+0x26) & 0xffff;
	i386_load_segment_descriptor(cpustate, SS);
	cpustate->sreg[DS].selector = READ16(cpustate,tss+0x28) & 0xffff;
	i386_load_segment_descriptor(cpustate, DS);

	/* Set the busy bit in the new task's descriptor */
	if(selector & 0x0004)
	{
		ar_byte = READ8(cpustate,cpustate->ldtr.base + (selector & ~0x0007) + 5);
		WRITE8(cpustate,cpustate->ldtr.base + (selector & ~0x0007) + 5,ar_byte | 0x02);
	}
	else
	{
		ar_byte = READ8(cpustate,cpustate->gdtr.base + (selector & ~0x0007) + 5);
		WRITE8(cpustate,cpustate->gdtr.base + (selector & ~0x0007) + 5,ar_byte | 0x02);
	}

	/* For nested tasks, we write the outgoing task's selector to the back-link field of the new TSS,
       and set the NT flag in the EFLAGS register */
	if(nested != 0)
	{
		WRITE16(cpustate,tss+0,old_task);
		cpustate->NT = 1;
	}
	CHANGE_PC(cpustate,cpustate->eip);

	cpustate->CPL = cpustate->sreg[CS].selector & 0x03;
//  printf("286 Task Switch from selector %04x to %04x\n",old_task,selector);
}

static void i386_task_switch(i386_state *cpustate, UINT16 selector, UINT8 nested)
{
	UINT32 tss;
	I386_SREG seg;
	UINT16 old_task;
	UINT8 ar_byte;  // access rights byte

	/* TODO: Task State Segment privilege checks */

	/* For tasks that aren't nested, clear the busy bit in the task's descriptor */
	if(nested == 0)
	{
		if(cpustate->task.segment & 0x0004)
		{
			ar_byte = READ8(cpustate,cpustate->ldtr.base + (cpustate->task.segment & ~0x0007) + 5);
			WRITE8(cpustate,cpustate->ldtr.base + (cpustate->task.segment & ~0x0007) + 5,ar_byte & ~0x02);
		}
		else
		{
			ar_byte = READ8(cpustate,cpustate->gdtr.base + (cpustate->task.segment & ~0x0007) + 5);
			WRITE8(cpustate,cpustate->gdtr.base + (cpustate->task.segment & ~0x0007) + 5,ar_byte & ~0x02);
		}
	}

	/* Save the state of the current task in the current TSS (TR register base) */
	tss = cpustate->task.base;
	WRITE32(cpustate,tss+0x1c,cpustate->cr[3]);  // correct?
	WRITE32(cpustate,tss+0x20,cpustate->eip);
	WRITE32(cpustate,tss+0x24,get_flags(cpustate));
	WRITE32(cpustate,tss+0x28,REG32(EAX));
	WRITE32(cpustate,tss+0x2c,REG32(ECX));
	WRITE32(cpustate,tss+0x30,REG32(EDX));
	WRITE32(cpustate,tss+0x34,REG32(EBX));
	WRITE32(cpustate,tss+0x38,REG32(ESP));
	WRITE32(cpustate,tss+0x3c,REG32(EBP));
	WRITE32(cpustate,tss+0x40,REG32(ESI));
	WRITE32(cpustate,tss+0x44,REG32(EDI));
	WRITE32(cpustate,tss+0x48,cpustate->sreg[ES].selector);
	WRITE32(cpustate,tss+0x4c,cpustate->sreg[CS].selector);
	WRITE32(cpustate,tss+0x50,cpustate->sreg[SS].selector);
	WRITE32(cpustate,tss+0x54,cpustate->sreg[DS].selector);
	WRITE32(cpustate,tss+0x58,cpustate->sreg[FS].selector);
	WRITE32(cpustate,tss+0x5c,cpustate->sreg[GS].selector);

	old_task = cpustate->task.segment;

	/* Load task register with the selector of the incoming task */
	cpustate->task.segment = selector;
	memset(&seg, 0, sizeof(seg));
	seg.selector = cpustate->task.segment;
	i386_load_protected_mode_segment(cpustate,&seg,NULL);
	cpustate->task.limit = seg.limit;
	cpustate->task.base = seg.base;
	cpustate->task.flags = seg.flags;

	/* Set TS bit in CR0 */
	cpustate->cr[0] |= 0x08;

	/* Load incoming task state from the new task's TSS */
	tss = cpustate->task.base;
	cpustate->ldtr.segment = READ32(cpustate,tss+0x60) & 0xffff;
	seg.selector = cpustate->ldtr.segment;
	i386_load_protected_mode_segment(cpustate,&seg,NULL);
	cpustate->ldtr.limit = seg.limit;
	cpustate->ldtr.base = seg.base;
	cpustate->ldtr.flags = seg.flags;
	cpustate->cr[3] = READ32(cpustate,tss+0x1c);  // CR3 (PDBR)
	cpustate->eip = READ32(cpustate,tss+0x20);
	set_flags(cpustate,READ32(cpustate,tss+0x24));
	REG32(EAX) = READ32(cpustate,tss+0x28);
	REG32(ECX) = READ32(cpustate,tss+0x2c);
	REG32(EDX) = READ32(cpustate,tss+0x30);
	REG32(EBX) = READ32(cpustate,tss+0x34);
	REG32(ESP) = READ32(cpustate,tss+0x38);
	REG32(EBP) = READ32(cpustate,tss+0x3c);
	REG32(ESI) = READ32(cpustate,tss+0x40);
	REG32(EDI) = READ32(cpustate,tss+0x44);
	cpustate->sreg[ES].selector = READ32(cpustate,tss+0x48) & 0xffff;
	i386_load_segment_descriptor(cpustate, ES);
	cpustate->sreg[CS].selector = READ32(cpustate,tss+0x4c) & 0xffff;
	i386_load_segment_descriptor(cpustate, CS);
	cpustate->sreg[SS].selector = READ32(cpustate,tss+0x50) & 0xffff;
	i386_load_segment_descriptor(cpustate, SS);
	cpustate->sreg[DS].selector = READ32(cpustate,tss+0x54) & 0xffff;
	i386_load_segment_descriptor(cpustate, DS);
	cpustate->sreg[FS].selector = READ32(cpustate,tss+0x58) & 0xffff;
	i386_load_segment_descriptor(cpustate, FS);
	cpustate->sreg[GS].selector = READ32(cpustate,tss+0x5c) & 0xffff;
	i386_load_segment_descriptor(cpustate, GS);

	/* Set the busy bit in the new task's descriptor */
	if(selector & 0x0004)
	{
		ar_byte = READ8(cpustate,cpustate->ldtr.base + (selector & ~0x0007) + 5);
		WRITE8(cpustate,cpustate->ldtr.base + (selector & ~0x0007) + 5,ar_byte | 0x02);
	}
	else
	{
		ar_byte = READ8(cpustate,cpustate->gdtr.base + (selector & ~0x0007) + 5);
		WRITE8(cpustate,cpustate->gdtr.base + (selector & ~0x0007) + 5,ar_byte | 0x02);
	}

	/* For nested tasks, we write the outgoing task's selector to the back-link field of the new TSS,
       and set the NT flag in the EFLAGS register */
	if(nested != 0)
	{
		WRITE32(cpustate,tss+0,old_task);
		cpustate->NT = 1;
	}
	CHANGE_PC(cpustate,cpustate->eip);

	cpustate->CPL = cpustate->sreg[CS].selector & 0x03;
//  printf("386 Task Switch from selector %04x to %04x\n",old_task,selector);
}

static void i386_check_irq_line(i386_state *cpustate)
{
	/* Check if the interrupts are enabled */
	if ( (cpustate->irq_state) && cpustate->IF )
	{
		cpustate->cycles -= 2;
		i386_trap(cpustate, pic_ack(), 1, 0);
	}
}

static void i386_protected_mode_jump(i386_state *cpustate, UINT16 seg, UINT32 off, int indirect, int operand32)
{
	I386_SREG desc;
	I386_CALL_GATE call_gate;
	UINT8 CPL,DPL,RPL;
	UINT8 SetRPL = 0;
	UINT16 segment = seg;
	UINT32 offset = off;

	/* Check selector is not null */
	if((segment & ~0x03) == 0)
	{
		logerror("JMP: Segment is null.\n");
		FAULT(FAULT_GP,0)
	}
	/* Selector is within descriptor table limit */
	if((segment & 0x04) == 0)
	{
		/* check GDT limit */
		if((segment & ~0x07) > (cpustate->gdtr.limit))
		{
			logerror("JMP: Segment is past GDT limit.\n");
			FAULT(FAULT_GP,segment & 0xfffc)
		}
	}
	else
	{
		/* check LDT limit */
		if((segment & ~0x07) > (cpustate->ldtr.limit))
		{
			logerror("JMP: Segment is past LDT limit.\n");
			FAULT(FAULT_GP,segment & 0xfffc)
		}
	}
	/* Determine segment type */
	memset(&desc, 0, sizeof(desc));
	desc.selector = segment;
	i386_load_protected_mode_segment(cpustate,&desc,NULL);
	CPL = cpustate->CPL;  // current privilege level
	DPL = (desc.flags >> 5) & 0x03;  // descriptor privilege level
	RPL = segment & 0x03;  // requested privilege level
	if((desc.flags & 0x0018) == 0x0018)
	{
		/* code segment */
		if((desc.flags & 0x0004) == 0)
		{
			/* non-conforming */
			if(RPL > CPL)
			{
				logerror("JMP: RPL %i is less than CPL %i\n",RPL,CPL);
				FAULT(FAULT_GP,segment & 0xfffc)
			}
			if(DPL != CPL)
			{
				logerror("JMP: DPL %i is not equal CPL %i\n",DPL,CPL);
				FAULT(FAULT_GP,segment & 0xfffc)
			}
		}
		else
		{
			/* conforming */
			if(DPL > CPL)
			{
				logerror("JMP: DPL %i is less than CPL %i\n",DPL,CPL);
				FAULT(FAULT_GP,segment & 0xfffc)
			}
		}
		SetRPL = 1;
		if((desc.flags & 0x0080) == 0)
		{
			logerror("JMP: Segment is not present\n");
			FAULT(FAULT_NP,segment & 0xfffc)
		}
		if(offset > desc.limit)
		{
			logerror("JMP: Offset is past segment limit\n");
			FAULT(FAULT_GP,0)
		}
	}
	else
	{
		if((desc.flags & 0x0010) != 0)
		{
			logerror("JMP: Segment is a data segment\n");
			FAULT(FAULT_GP,segment & 0xfffc)  // #GP (cannot execute code in a data segment)
		}
		else
		{
			switch(desc.flags & 0x000f)
			{
			case 0x01:  // 286 Available TSS
			case 0x09:  // 386 Available TSS
				logerror("JMP: Available 386 TSS at %08x\n",cpustate->pc);
				memset(&desc, 0, sizeof(desc));
				desc.selector = segment;
				i386_load_protected_mode_segment(cpustate,&desc,NULL);
				DPL = (desc.flags >> 5) & 0x03;  // descriptor privilege level
				if(DPL < CPL)
				{
					logerror("JMP: TSS: DPL %i is less than CPL %i\n",DPL,CPL);
					FAULT(FAULT_GP,segment & 0xfffc)
				}
				if(DPL < RPL)
				{
					logerror("JMP: TSS: DPL %i is less than TSS RPL %i\n",DPL,RPL);
					FAULT(FAULT_GP,segment & 0xfffc)
				}
				if((desc.flags & 0x0080) == 0)
				{
					logerror("JMP: TSS: Segment is not present\n");
					FAULT(FAULT_GP,segment & 0xfffc)
				}
				if(desc.flags & 0x0008)
					i386_task_switch(cpustate,desc.selector,0);
				else
					i286_task_switch(cpustate,desc.selector,0);
				return;
				break;
			case 0x04:  // 286 Call Gate
			case 0x0c:  // 386 Call Gate
				logerror("JMP: Call gate at %08x\n",cpustate->pc);
				SetRPL = 1;
				memset(&call_gate, 0, sizeof(call_gate));
				call_gate.segment = segment;
				i386_load_call_gate(cpustate,&call_gate);
				DPL = call_gate.dpl;
				if(DPL < CPL)
				{
					logerror("JMP: Call Gate: DPL %i is less than CPL %i\n",DPL,CPL);
					FAULT(FAULT_GP,segment & 0xfffc)
				}
				if(DPL < RPL)
				{
					logerror("JMP: Call Gate: DPL %i is less than RPL %i\n",DPL,RPL);
					FAULT(FAULT_GP,segment & 0xfffc)
				}
				if((desc.flags & 0x0080) == 0)
				{
					logerror("JMP: Call Gate: Segment is not present\n");
					FAULT(FAULT_NP,segment & 0xfffc)
				}
				/* Now we examine the segment that the call gate refers to */
				if(call_gate.selector == 0)
				{
					logerror("JMP: Call Gate: Gate selector is null\n");
					FAULT(FAULT_GP,0)
				}
				if(call_gate.selector & 0x04)
				{
					if((call_gate.selector & ~0x07) > cpustate->ldtr.limit)
					{
						logerror("JMP: Call Gate: Gate Selector is past LDT segment limit\n");
						FAULT(FAULT_GP,call_gate.selector & 0xfffc)
					}
				}
				else
				{
					if((call_gate.selector & ~0x07) > cpustate->gdtr.limit)
					{
						logerror("JMP: Call Gate: Gate Selector is past GDT segment limit\n");
						FAULT(FAULT_GP,call_gate.selector & 0xfffc)
					}
				}
				desc.selector = call_gate.selector;
				i386_load_protected_mode_segment(cpustate,&desc,NULL);
				DPL = (desc.flags >> 5) & 0x03;
				if((desc.flags & 0x0018) != 0x18)
				{
					logerror("JMP: Call Gate: Gate does not point to a code segment\n");
					FAULT(FAULT_GP,call_gate.selector & 0xfffc)
				}
				if((desc.flags & 0x0004) == 0)
				{  // non-conforming
					if(DPL != CPL)
					{
						logerror("JMP: Call Gate: Gate DPL does not equal CPL\n");
						FAULT(FAULT_GP,call_gate.selector & 0xfffc)
					}
				}
				else
				{  // conforming
					if(DPL > CPL)
					{
						logerror("JMP: Call Gate: Gate DPL is greater than CPL\n");
						FAULT(FAULT_GP,call_gate.selector & 0xfffc)
					}
				}
				if((desc.flags & 0x0080) == 0)
				{
					logerror("JMP: Call Gate: Gate Segment is not present\n");
					FAULT(FAULT_NP,call_gate.selector & 0xfffc)
				}
				if(call_gate.offset > desc.limit)
				{
					logerror("JMP: Call Gate: Gate offset is past Gate segment limit\n");
					FAULT(FAULT_GP,call_gate.selector & 0xfffc)
				}
				segment = call_gate.selector;
				offset = call_gate.offset;
				break;
			case 0x05:  // Task Gate
				logerror("JMP: Task gate at %08x\n",cpustate->pc);
				memset(&call_gate, 0, sizeof(call_gate));
				call_gate.segment = segment;
				i386_load_call_gate(cpustate,&call_gate);
				DPL = call_gate.dpl;
				if(DPL < CPL)
				{
					logerror("JMP: Task Gate: Gate DPL %i is less than CPL %i\n",DPL,CPL);
					FAULT(FAULT_GP,segment & 0xfffc)
				}
				if(DPL < RPL)
				{
					logerror("JMP: Task Gate: Gate DPL %i is less than CPL %i\n",DPL,CPL);
					FAULT(FAULT_GP,segment & 0xfffc)
				}
				if(call_gate.present == 0)
				{
					logerror("JMP: Task Gate: Gate is not present.\n");
					FAULT(FAULT_GP,segment & 0xfffc)
				}
				/* Check the TSS that the task gate points to */
				desc.selector = call_gate.selector;
				i386_load_protected_mode_segment(cpustate,&desc,NULL);
				DPL = (desc.flags >> 5) & 0x03;  // descriptor privilege level
				RPL = call_gate.selector & 0x03;  // requested privilege level
				if(call_gate.selector & 0x04)
				{
					logerror("JMP: Task Gate TSS: TSS must be global.\n");
					FAULT(FAULT_GP,call_gate.selector & 0xfffc)
				}
				else
				{
					if((call_gate.selector & ~0x07) > cpustate->gdtr.limit)
					{
						logerror("JMP: Task Gate TSS: TSS is past GDT limit.\n");
						FAULT(FAULT_GP,call_gate.selector & 0xfffc)
					}
				}
				if((call_gate.ar & 0x000f) == 0x0009 || (call_gate.ar & 0x000f) == 0x0001)
				{
					logerror("JMP: Task Gate TSS: Segment is not an available TSS.\n");
					FAULT(FAULT_GP,call_gate.selector & 0xfffc)
				}
				if(call_gate.present == 0)
				{
					logerror("JMP: Task Gate TSS: TSS is not present.\n");
					FAULT(FAULT_NP,call_gate.selector & 0xfffc)
				}
				if(call_gate.ar & 0x08)
					i386_task_switch(cpustate,call_gate.selector,0);
				else
					i286_task_switch(cpustate,call_gate.selector,0);
				return;
				break;
			default:  // invalid segment type
				logerror("JMP: Invalid segment type (%i) to jump to.\n",desc.flags & 0x000f);
				FAULT(FAULT_GP,segment & 0xfffc)
			}
		}
	}

	if(SetRPL != 0)
		segment = (segment & ~0x03) | cpustate->CPL;
	if(operand32 == 0)
		cpustate->eip = offset & 0x0000ffff;
	else
		cpustate->eip = offset;
	cpustate->sreg[CS].selector = segment;
	cpustate->performed_intersegment_jump = 1;
	i386_load_segment_descriptor(cpustate,CS);
	CHANGE_PC(cpustate,cpustate->eip);
}

static void i386_protected_mode_call(i386_state *cpustate, UINT16 seg, UINT32 off, int indirect, int operand32)
{
	I386_SREG desc;
	I386_CALL_GATE gate;
	UINT8 SetRPL = 0;
	UINT8 CPL, DPL, RPL;
	UINT16 selector = seg;
	UINT32 offset = off;
	int x;

	if((selector & ~0x03) == 0)
	{
		logerror("CALL (%08x): Selector is null.\n",cpustate->pc);
		FAULT(FAULT_GP,0)  // #GP(0)
	}
	if(selector & 0x04)
	{
		if((selector & ~0x07) > cpustate->ldtr.limit)
		{
			logerror("CALL: Selector is past LDT limit.\n");
			FAULT(FAULT_GP,selector & ~0x03)  // #GP(selector)
		}
	}
	else
	{
		if((selector & ~0x07) > cpustate->gdtr.limit)
		{
			logerror("CALL: Selector is past GDT limit.\n");
			FAULT(FAULT_GP,selector & ~0x03)  // #GP(selector)
		}
	}

	/* Determine segment type */
	memset(&desc, 0, sizeof(desc));
	desc.selector = selector;
	i386_load_protected_mode_segment(cpustate,&desc,NULL);
	CPL = cpustate->CPL;  // current privilege level
	DPL = (desc.flags >> 5) & 0x03;  // descriptor privilege level
	RPL = selector & 0x03;  // requested privilege level
	if((desc.flags & 0x0018) == 0x18)  // is a code segment
	{
		if(desc.flags & 0x0004)
		{
			/* conforming */
			if(DPL > CPL)
			{
				logerror("CALL: Code segment DPL %i is greater than CPL %i\n",DPL,CPL);
				FAULT(FAULT_GP,selector & ~0x03)  // #GP(selector)
			}
		}
		else
		{
			/* non-conforming */
			if(RPL > CPL)
			{
				logerror("CALL: RPL %i is greater than CPL %i\n",RPL,CPL);
				FAULT(FAULT_GP,selector & ~0x03)  // #GP(selector)
			}
			if(DPL != CPL)
			{
				logerror("CALL: Code segment DPL %i is not equal to CPL %i\n",DPL,CPL);
				FAULT(FAULT_GP,selector & ~0x03)  // #GP(selector)
			}
		}
		SetRPL = 1;
		if((desc.flags & 0x0080) == 0)
		{
			logerror("CALL (%08x): Code segment is not present.\n",cpustate->pc);
			FAULT(FAULT_NP,selector & ~0x03)  // #NP(selector)
		}
		if (operand32 != 0)  // if 32-bit
		{
			if(REG32(ESP) < 8)
			{
				logerror("CALL: Stack has no room for return address.\n");
				FAULT(FAULT_SS,0)  // #SS(0)
			}
		}
		else
		{
			if(REG16(SP) < 4)
			{
				logerror("CALL: Stack has no room for return address.\n");
				FAULT(FAULT_SS,0)  // #SS(0)
			}
		}
		if(offset > desc.limit)
		{
			logerror("CALL: EIP is past segment limit.\n");
			FAULT(FAULT_GP,0)  // #GP(0)
		}
	}
	else
	{
		/* special segment type */
		if(desc.flags & 0x0010)
		{
			logerror("CALL: Segment is a data segment.\n");
			FAULT(FAULT_GP,desc.selector & ~0x03)  // #GP(selector)
		}
		else
		{
			switch(desc.flags & 0x000f)
			{
			case 0x01:  // Available 286 TSS
			case 0x09:  // Available 386 TSS
				logerror("CALL: Available TSS at %08x\n",cpustate->pc);
				if(DPL < CPL)
				{
					logerror("CALL: TSS: DPL is less than CPL.\n");
					FAULT(FAULT_TS,selector & ~0x03) // #TS(selector)
				}
				if(DPL < RPL)
				{
					logerror("CALL: TSS: DPL is less than RPL.\n");
					FAULT(FAULT_TS,selector & ~0x03) // #TS(selector)
				}
				if(desc.flags & 0x0002)
				{
					logerror("CALL: TSS: TSS is busy.\n");
					FAULT(FAULT_TS,selector & ~0x03) // #TS(selector)
				}
				if(desc.flags & 0x0080)
				{
					logerror("CALL: TSS: Segment is not present.\n");
					FAULT(FAULT_NP,selector & ~0x03) // #NP(selector)
				}
				if(desc.flags & 0x08)
					i386_task_switch(cpustate,desc.selector,1);
				else
					i286_task_switch(cpustate,desc.selector,1);
				return;
				break;
			case 0x04:  // 286 call gate
			case 0x0c:  // 386 call gate
				if((desc.flags & 0x000f) == 0x04)
					operand32 = 0;
				else
					operand32 = 1;
				memset(&gate, 0, sizeof(gate));
				gate.segment = selector;
				i386_load_call_gate(cpustate,&gate);
				DPL = gate.dpl;
				logerror("CALL: Call gate at %08x (%i parameters)\n",cpustate->pc,gate.dword_count);
				if(DPL < CPL)
				{
					logerror("CALL: Call gate DPL %i is less than CPL %i.\n",DPL,CPL);
					FAULT(FAULT_GP,desc.selector & ~0x03)  // #GP(selector)
				}
				if(DPL < RPL)
				{
					logerror("CALL: Call gate DPL %i is less than RPL %i.\n",DPL,RPL);
					FAULT(FAULT_GP,desc.selector & ~0x03)  // #GP(selector)
				}
				if(gate.present == 0)
				{
					logerror("CALL: Call gate is not present.\n");
					FAULT(FAULT_NP,desc.selector & ~0x03)  // #GP(selector)
				}
				desc.selector = gate.selector;
				if((gate.selector & ~0x03) == 0)
				{
					logerror("CALL: Call gate: Segment is null.\n");
					FAULT(FAULT_GP,0)  // #GP(0)
				}
				if(desc.selector & 0x04)
				{
					if((desc.selector & ~0x07) > cpustate->ldtr.limit)
					{
						logerror("CALL: Call gate: Segment is past LDT limit\n");
						FAULT(FAULT_GP,desc.selector & ~0x03)  // #GP(selector)
					}
				}
				else
				{
					if((desc.selector & ~0x07) > cpustate->gdtr.limit)
					{
						logerror("CALL: Call gate: Segment is past GDT limit\n");
						FAULT(FAULT_GP,desc.selector & ~0x03)  // #GP(selector)
					}
				}
				i386_load_protected_mode_segment(cpustate,&desc,NULL);
				if((desc.flags & 0x0018) != 0x18)
				{
					logerror("CALL: Call gate: Segment is not a code segment.\n");
					FAULT(FAULT_GP,desc.selector & ~0x03)  // #GP(selector)
				}
				DPL = ((desc.flags >> 5) & 0x03);
				if(DPL > CPL)
				{
					logerror("CALL: Call gate: Segment DPL %i is greater than CPL %i.\n",DPL,CPL);
					FAULT(FAULT_GP,desc.selector & ~0x03)  // #GP(selector)
				}
				if((desc.flags & 0x0080) == 0)
				{
					logerror("CALL (%08x): Code segment is not present.\n",cpustate->pc);
					FAULT(FAULT_NP,desc.selector & ~0x03)  // #NP(selector)
				}
				if(DPL < CPL && (desc.flags & 0x0004) == 0)
				{
					I386_SREG stack;
					I386_SREG temp;
					UINT32 oldSS,oldESP;
					/* more privilege */
					/* Check new SS segment for privilege level from TSS */
					memset(&stack, 0, sizeof(stack));
					stack.selector = i386_get_stack_segment(cpustate,DPL);
					i386_load_protected_mode_segment(cpustate,&stack,NULL);
					if((stack.selector & ~0x03) == 0)
					{
						logerror("CALL: Call gate: TSS selector is null\n");
						FAULT(FAULT_TS,0)  // #TS(0)
					}
					if(stack.selector & 0x04)
					{
						if((stack.selector & ~0x07) > cpustate->ldtr.limit)
						{
							logerror("CALL: Call gate: TSS selector is past LDT limit\n");
							FAULT(FAULT_TS,stack.selector)  // #TS(SS selector)
						}
					}
					else
					{
						if((stack.selector & ~0x07) > cpustate->gdtr.limit)
						{
							logerror("CALL: Call gate: TSS selector is past GDT limit\n");
							FAULT(FAULT_TS,stack.selector)  // #TS(SS selector)
						}
					}
					if((stack.selector & 0x03) != DPL)
					{
						logerror("CALL: Call gate: Stack selector RPL does not equal code segment DPL %i\n",DPL);
						FAULT(FAULT_TS,stack.selector)  // #TS(SS selector)
					}
					if(((stack.flags >> 5) & 0x03) != DPL)
					{
						logerror("CALL: Call gate: Stack DPL does not equal code segment DPL %i\n",DPL);
						FAULT(FAULT_TS,stack.selector)  // #TS(SS selector)
					}
					if((stack.flags & 0x0018) != 0x10 && (stack.flags & 0x0002))
					{
						logerror("CALL: Call gate: Stack segment is not a writable data segment\n");
						FAULT(FAULT_TS,stack.selector)  // #TS(SS selector)
					}
					if((stack.flags & 0x0080) == 0)
					{
						logerror("CALL: Call gate: Stack segment is not present\n");
						FAULT(FAULT_SS,stack.selector)  // #SS(SS selector)
					}
					UINT32 newESP = i386_get_stack_ptr(cpustate,DPL);
					if(operand32 != 0)
					{
						if(newESP < ((gate.dword_count & 0x1f) + 16))
						{
							logerror("CALL: Call gate: New stack has no room for 32-bit return address and parameters.\n");
							FAULT(FAULT_SS,0) // #SS(0)
						}
						if(gate.offset > desc.limit)
						{
							logerror("CALL: Call gate: EIP is past segment limit.\n");
							FAULT(FAULT_GP,0) // #GP(0)
						}
					}
					else
					{
						newESP &= 0x0000ffff;
						if(newESP < ((gate.dword_count & 0x1f) + 8))
						{
							logerror("CALL: Call gate: New stack has no room for 16-bit return address and parameters.\n");
							FAULT(FAULT_SS,0) // #SS(0)
						}
						if((gate.offset & 0xffff) > desc.limit)
						{
							logerror("CALL: Call gate: IP is past segment limit.\n");
							FAULT(FAULT_GP,0) // #GP(0)
						}
					}
					selector = gate.selector;
					offset = gate.offset;

					cpustate->CPL = (stack.flags >> 5) & 0x03;
					/* check for page fault at new stack TODO: check if stack frame crosses page boundary */
					WRITE_TEST(cpustate, stack.base+newESP-1);
					/* switch to new stack */
					oldSS = cpustate->sreg[SS].selector;
					cpustate->sreg[SS].selector = i386_get_stack_segment(cpustate,gate.selector & 0x03);
					if(operand32 != 0)
					{
						oldESP = REG32(ESP);
					}
					else
					{
						oldESP = REG16(SP);
					}
					i386_load_segment_descriptor(cpustate, SS );
					if(operand32 != 0)
						REG32(ESP) = i386_get_stack_ptr(cpustate,gate.selector & 0x03);
					else
						REG16(SP) = i386_get_stack_ptr(cpustate,gate.selector & 0x03) & 0x0000ffff;

					if(operand32 != 0)
					{
						PUSH32(cpustate,oldSS);
						PUSH32(cpustate,oldESP);
					}
					else
					{
						PUSH16(cpustate,oldSS);
						PUSH16(cpustate,oldESP & 0xffff);
					}

					memset(&temp, 0, sizeof(temp));
					temp.selector = oldSS;
					i386_load_protected_mode_segment(cpustate,&temp,NULL);
					/* copy parameters from old stack to new stack */
					for(x=(gate.dword_count & 0x1f)-1;x>=0;x--)
					{
						UINT32 addr = oldESP + (operand32?(x*4):(x*2));
						addr = temp.base + (temp.d?addr:(addr&0xffff));
						if(operand32)
							PUSH32(cpustate,READ32(cpustate,addr));
						else
							PUSH16(cpustate,READ16(cpustate,addr));
					}
					SetRPL = 1;
				}
				else
				{
					/* same privilege */
					if (operand32 != 0)  // if 32-bit
					{
						if(REG32(ESP) < 8)
						{
							logerror("CALL: Stack has no room for return address.\n");
							FAULT(FAULT_SS,0) // #SS(0)
						}
						selector = gate.selector;
						offset = gate.offset;
					}
					else
					{
						if(REG16(SP) < 4)
						{
							logerror("CALL: Stack has no room for return address.\n");
							FAULT(FAULT_SS,0) // #SS(0)
						}
						selector = gate.selector;
						offset = gate.offset & 0xffff;
					}
					if(offset > desc.limit)
					{
						logerror("CALL: EIP is past segment limit.\n");
						FAULT(FAULT_GP,0) // #GP(0)
					}
					SetRPL = 1;
				}
				break;
			case 0x05:  // task gate
				logerror("CALL: Task gate at %08x\n",cpustate->pc);
				memset(&gate, 0, sizeof(gate));
				gate.segment = selector;
				i386_load_call_gate(cpustate,&gate);
				DPL = gate.dpl;
				if(DPL < CPL)
				{
					logerror("CALL: Task Gate: Gate DPL is less than CPL.\n");
					FAULT(FAULT_TS,selector & ~0x03) // #TS(selector)
				}
				if(DPL < RPL)
				{
					logerror("CALL: Task Gate: Gate DPL is less than RPL.\n");
					FAULT(FAULT_TS,selector & ~0x03) // #TS(selector)
				}
				if(gate.ar & 0x0080)
				{
					logerror("CALL: Task Gate: Gate is not present.\n");
					FAULT(FAULT_NP,selector & ~0x03) // #NP(selector)
				}
				/* Check the TSS that the task gate points to */
				desc.selector = gate.selector;
				i386_load_protected_mode_segment(cpustate,&desc,NULL);
				if(gate.selector & 0x04)
				{
					logerror("CALL: Task Gate: TSS is not global.\n");
					FAULT(FAULT_TS,gate.selector & ~0x03) // #TS(selector)
				}
				else
				{
					if((gate.selector & ~0x07) > cpustate->gdtr.limit)
					{
						logerror("CALL: Task Gate: TSS is past GDT limit.\n");
						FAULT(FAULT_TS,gate.selector & ~0x03) // #TS(selector)
					}
				}
				if(desc.flags & 0x0002)
				{
					logerror("CALL: Task Gate: TSS is busy.\n");
					FAULT(FAULT_TS,gate.selector & ~0x03) // #TS(selector)
				}
				if(desc.flags & 0x0080)
				{
					logerror("CALL: Task Gate: TSS is not present.\n");
					FAULT(FAULT_NP,gate.selector & ~0x03) // #TS(selector)
				}
				if(desc.flags & 0x08)
					i386_task_switch(cpustate,desc.selector,1);  // with nesting
				else
					i286_task_switch(cpustate,desc.selector,1);
				return;
				break;
			default:
				logerror("CALL: Invalid special segment type (%i) to jump to.\n",desc.flags & 0x000f);
				FAULT(FAULT_GP,selector & ~0x07)  // #GP(selector)
			}
		}
	}

	if(SetRPL != 0)
		selector = (selector & ~0x03) | cpustate->CPL;
	if(operand32 == 0)
	{
		/* 16-bit operand size */
		PUSH16(cpustate, cpustate->sreg[CS].selector );
		PUSH16(cpustate, cpustate->eip & 0x0000ffff );
		cpustate->sreg[CS].selector = selector;
		cpustate->performed_intersegment_jump = 1;
		cpustate->eip = offset;
		i386_load_segment_descriptor(cpustate,CS);
	}
	else
	{
		/* 32-bit operand size */
		PUSH32(cpustate, cpustate->sreg[CS].selector );
		PUSH32(cpustate, cpustate->eip );
		cpustate->sreg[CS].selector = selector;
		cpustate->performed_intersegment_jump = 1;
		cpustate->eip = offset;
		i386_load_segment_descriptor(cpustate, CS );
	}
	CHANGE_PC(cpustate,cpustate->eip);
}

static void i386_protected_mode_retf(i386_state* cpustate, UINT8 count, UINT8 operand32)
{
	UINT32 newCS, newEIP;
	I386_SREG desc;
	UINT8 CPL, RPL, DPL;

	UINT32 ea = i386_translate(cpustate, SS, (STACK_32BIT)?REG32(ESP):REG16(SP), 0);

	if(operand32 == 0)
	{
		newEIP = READ16(cpustate, ea) & 0xffff;
		newCS = READ16(cpustate, ea+2) & 0xffff;
	}
	else
	{
		newEIP = READ32(cpustate, ea);
		newCS = READ32(cpustate, ea+4) & 0xffff;
	}

	memset(&desc, 0, sizeof(desc));
	desc.selector = newCS;
	i386_load_protected_mode_segment(cpustate,&desc,NULL);
	CPL = cpustate->CPL;  // current privilege level
	DPL = (desc.flags >> 5) & 0x03;  // descriptor privilege level
	RPL = newCS & 0x03;

	if(RPL < CPL)
	{
		logerror("RETF (%08x): Return segment RPL is less than CPL.\n",cpustate->pc);
		FAULT(FAULT_GP,newCS & ~0x03)
	}

	if(RPL == CPL)
	{
		/* same privilege level */
		if((newCS & ~0x03) == 0)
		{
			logerror("RETF: Return segment is null.\n");
			FAULT(FAULT_GP,0)
		}
		if(newCS & 0x04)
		{
			if((newCS & ~0x07) >= cpustate->ldtr.limit)
			{
				logerror("RETF: Return segment is past LDT limit.\n");
				FAULT(FAULT_GP,newCS & ~0x03)
			}
		}
		else
		{
			if((newCS & ~0x07) >= cpustate->gdtr.limit)
			{
				logerror("RETF: Return segment is past GDT limit.\n");
				FAULT(FAULT_GP,newCS & ~0x03)
			}
		}
		if((desc.flags & 0x0018) != 0x0018)
		{
			logerror("RETF: Return segment is not a code segment.\n");
			FAULT(FAULT_GP,newCS & ~0x03)
		}
		if(desc.flags & 0x0004)
		{
			if(DPL > RPL)
			{
				logerror("RETF: Conforming code segment DPL is greater than CS RPL.\n");
				FAULT(FAULT_GP,newCS & ~0x03)
			}
		}
		else
		{
			if(DPL != RPL)
			{
				logerror("RETF: Non-conforming code segment DPL does not equal CS RPL.\n");
				FAULT(FAULT_GP,newCS & ~0x03)
			}
		}
		if((desc.flags & 0x0080) == 0)
		{
			logerror("RETF (%08x): Code segment is not present.\n",cpustate->pc);
			FAULT(FAULT_NP,newCS & ~0x03)
		}
		if(newEIP > desc.limit)
		{
			logerror("RETF: EIP is past code segment limit.\n");
			FAULT(FAULT_GP,0)
		}
		if(operand32 == 0)
		{
			UINT32 offset = (STACK_32BIT ? REG32(ESP) : REG16(SP));
			if(i386_limit_check(cpustate,SS,offset+count+3) != 0)
			{
				logerror("RETF (%08x): SP is past stack segment limit.\n",cpustate->pc);
				FAULT(FAULT_SS,0)
			}
		}
		else
		{
			UINT32 offset = (STACK_32BIT ? REG32(ESP) : REG16(SP));
			if(i386_limit_check(cpustate,SS,offset+count+7) != 0)
			{
				logerror("RETF: ESP is past stack segment limit.\n");
				FAULT(FAULT_SS,0)
			}
		}
		if(operand32 == 0)
			REG16(SP) += (4+count);
		else
			REG32(ESP) += (8+count);
	}
	else if(RPL > CPL)
	{
		UINT32 newSS, newESP;  // when changing privilege
		/* outer privilege level */
		if(operand32 == 0)
		{
			UINT32 offset = (STACK_32BIT ? REG32(ESP) : REG16(SP));
			if(i386_limit_check(cpustate,SS,offset+count+7) != 0)
			{
				logerror("RETF (%08x): SP is past stack segment limit.\n",cpustate->pc);
				FAULT(FAULT_SS,0)
			}
		}
		else
		{
			UINT32 offset = (STACK_32BIT ? REG32(ESP) : REG16(SP));
			if(i386_limit_check(cpustate,SS,offset+count+15) != 0)
			{
				logerror("RETF: ESP is past stack segment limit.\n");
				FAULT(FAULT_SS,0)
			}
		}
		/* Check CS selector and descriptor */
		if((newCS & ~0x03) == 0)
		{
			logerror("RETF: CS segment is null.\n");
			FAULT(FAULT_GP,0)
		}
		if(newCS & 0x04)
		{
			if((newCS & ~0x07) >= cpustate->ldtr.limit)
			{
				logerror("RETF: CS segment selector is past LDT limit.\n");
				FAULT(FAULT_GP,newCS & ~0x03)
			}
		}
		else
		{
			if((newCS & ~0x07) >= cpustate->gdtr.limit)
			{
				logerror("RETF: CS segment selector is past GDT limit.\n");
				FAULT(FAULT_GP,newCS & ~0x03)
			}
		}
		if((desc.flags & 0x0018) != 0x0018)
		{
			logerror("RETF: CS segment is not a code segment.\n");
			FAULT(FAULT_GP,newCS & ~0x03)
		}
		if(desc.flags & 0x0004)
		{
			if(DPL > RPL)
			{
				logerror("RETF: Conforming CS segment DPL is greater than return selector RPL.\n");
				FAULT(FAULT_GP,newCS & ~0x03)
			}
		}
		else
		{
			if(DPL != RPL)
			{
				logerror("RETF: Non-conforming CS segment DPL is not equal to return selector RPL.\n");
				FAULT(FAULT_GP,newCS & ~0x03)
			}
		}
		if((desc.flags & 0x0080) == 0)
		{
			logerror("RETF: CS segment is not present.\n");
			FAULT(FAULT_NP,newCS & ~0x03)
		}
		if(newEIP > desc.limit)
		{
			logerror("RETF: EIP is past return CS segment limit.\n");
			FAULT(FAULT_GP,0)
		}

		if(operand32 == 0)
		{
			ea += count+4;
			newESP = READ16(cpustate, ea) & 0xffff;
			newSS = READ16(cpustate, ea+2) & 0xffff;
		}
		else
		{
			ea += count+8;
			newESP = READ32(cpustate, ea);
			newSS = READ32(cpustate, ea+4) & 0xffff;
		}

		/* Check SS selector and descriptor */
		desc.selector = newSS;
		i386_load_protected_mode_segment(cpustate,&desc,NULL);
		DPL = (desc.flags >> 5) & 0x03;  // descriptor privilege level
		if((newSS & ~0x07) == 0)
		{
			logerror("RETF: SS segment is null.\n");
			FAULT(FAULT_GP,0)
		}
		if(newSS & 0x04)
		{
			if((newSS & ~0x07) > cpustate->ldtr.limit)
			{
				logerror("RETF (%08x): SS segment selector is past LDT limit.\n",cpustate->pc);
				FAULT(FAULT_GP,newSS & ~0x03)
			}
		}
		else
		{
			if((newSS & ~0x07) > cpustate->gdtr.limit)
			{
				logerror("RETF (%08x): SS segment selector is past GDT limit.\n",cpustate->pc);
				FAULT(FAULT_GP,newSS & ~0x03)
			}
		}
		if((newSS & 0x03) != RPL)
		{
			logerror("RETF: SS segment RPL is not equal to CS segment RPL.\n");
			FAULT(FAULT_GP,newSS & ~0x03)
		}
		if((desc.flags & 0x0018) != 0x0010 || (desc.flags & 0x0002) == 0)
		{
			logerror("RETF: SS segment is not a writable data segment.\n");
			FAULT(FAULT_GP,newSS & ~0x03)
		}
		if(((desc.flags >> 5) & 0x03) != RPL)
		{
			logerror("RETF: SS DPL is not equal to CS segment RPL.\n");
			FAULT(FAULT_GP,newSS & ~0x03)
		}
		if((desc.flags & 0x0080) == 0)
		{
			logerror("RETF: SS segment is not present.\n");
			FAULT(FAULT_GP,newSS & ~0x03)
		}
		cpustate->CPL = newCS & 0x03;

		/* Load new SS:(E)SP */
		if(operand32 == 0)
			REG16(SP) = (newESP+count) & 0xffff;
		else
			REG32(ESP) = newESP+count;
		cpustate->sreg[SS].selector = newSS;
		i386_load_segment_descriptor(cpustate, SS );

		/* Check that DS, ES, FS and GS are valid for the new privilege level */
		i386_check_sreg_validity(cpustate,DS);
		i386_check_sreg_validity(cpustate,ES);
		i386_check_sreg_validity(cpustate,FS);
		i386_check_sreg_validity(cpustate,GS);
	}

	/* Load new CS:(E)IP */
	if(operand32 == 0)
		cpustate->eip = newEIP & 0xffff;
	else
		cpustate->eip = newEIP;
	cpustate->sreg[CS].selector = newCS;
	i386_load_segment_descriptor(cpustate, CS );
	CHANGE_PC(cpustate,cpustate->eip);
}

static void i386_protected_mode_iret(i386_state* cpustate, int operand32)
{
	UINT32 newCS, newEIP;
	UINT32 newSS, newESP;  // when changing privilege
	I386_SREG desc,stack;
	UINT8 CPL, RPL, DPL;
	UINT32 newflags;

	CPL = cpustate->CPL;
	UINT32 ea = i386_translate(cpustate, SS, (STACK_32BIT)?REG32(ESP):REG16(SP), 0);
	if(operand32 == 0)
	{
		newEIP = READ16(cpustate, ea) & 0xffff;
		newCS = READ16(cpustate, ea+2) & 0xffff;
		newflags = READ16(cpustate, ea+4) & 0xffff;
	}
	else
	{
		newEIP = READ32(cpustate, ea);
		newCS = READ32(cpustate, ea+4) & 0xffff;
		newflags = READ32(cpustate, ea+8);
	}

	if(V8086_MODE)
	{
		UINT32 oldflags = get_flags(cpustate);
		if(!cpustate->IOP1 || !cpustate->IOP2)
		{
			logerror("IRET (%08x): Is in Virtual 8086 mode and IOPL != 3.\n",cpustate->pc);
			FAULT(FAULT_GP,0)
		}
		if(operand32 == 0)
		{
			cpustate->eip = newEIP & 0xffff;
			cpustate->sreg[CS].selector = newCS & 0xffff;
			newflags &= ~(3<<12);
			newflags |= (((oldflags>>12)&3)<<12);  // IOPL cannot be changed in V86 mode
			set_flags(cpustate,(newflags & 0xffff) | (oldflags & ~0xffff));
			REG16(SP) += 6;
		}
		else
		{
			cpustate->eip = newEIP;
			cpustate->sreg[CS].selector = newCS & 0xffff;
			newflags &= ~(3<<12);
			newflags |= 0x20000 | (((oldflags>>12)&3)<<12);  // IOPL and VM cannot be changed in V86 mode
			set_flags(cpustate,newflags);
			REG32(ESP) += 12;
		}
	}
	else if(NESTED_TASK)
	{
		UINT32 task = READ32(cpustate,cpustate->task.base);
		/* Task Return */
		logerror("IRET (%08x): Nested task return.\n",cpustate->pc);
		/* Check back-link selector in TSS */
		if(task & 0x04)
		{
			logerror("IRET: Task return: Back-linked TSS is not in GDT.\n");
			FAULT(FAULT_TS,task & ~0x03)
		}
		if((task & ~0x07) >= cpustate->gdtr.limit)
		{
			logerror("IRET: Task return: Back-linked TSS is not in GDT.\n");
			FAULT(FAULT_TS,task & ~0x03)
		}
		memset(&desc, 0, sizeof(desc));
		desc.selector = task;
		i386_load_protected_mode_segment(cpustate,&desc,NULL);
		if((desc.flags & 0x001f) != 0x000b)
		{
			logerror("IRET (%08x): Task return: Back-linked TSS is not a busy TSS.\n",cpustate->pc);
			FAULT(FAULT_TS,task & ~0x03)
		}
		if((desc.flags & 0x0080) == 0)
		{
			logerror("IRET: Task return: Back-linked TSS is not present.\n");
			FAULT(FAULT_NP,task & ~0x03)
		}
		if(desc.flags & 0x08)
			i386_task_switch(cpustate,desc.selector,0);
		else
			i286_task_switch(cpustate,desc.selector,0);
		return;
	}
	else
	{
		if(newflags & 0x00020000) // if returning to virtual 8086 mode
		{
			// 16-bit iret can't reach here
			newESP = READ32(cpustate, ea+12);
			newSS = READ32(cpustate, ea+16) & 0xffff;
			/* Return to v86 mode */
			logerror("IRET (%08x): Returning to Virtual 8086 mode.\n",cpustate->pc);
			if(CPL != 0)
			{
				UINT32 oldflags = get_flags(cpustate);
				newflags = (newflags & ~0x00003000) | (oldflags & 0x00003000);
			}
			set_flags(cpustate,newflags);
			cpustate->eip = POP32(cpustate) & 0xffff;  // high 16 bits are ignored
			cpustate->sreg[CS].selector = POP32(cpustate) & 0xffff;
			POP32(cpustate);  // already set flags
			newESP = POP32(cpustate);
			newSS = POP32(cpustate) & 0xffff;
			cpustate->sreg[ES].selector = POP32(cpustate) & 0xffff;
			cpustate->sreg[DS].selector = POP32(cpustate) & 0xffff;
			cpustate->sreg[FS].selector = POP32(cpustate) & 0xffff;
			cpustate->sreg[GS].selector = POP32(cpustate) & 0xffff;
			REG32(ESP) = newESP;  // all 32 bits are loaded
			cpustate->sreg[SS].selector = newSS;
			i386_load_segment_descriptor(cpustate,ES);
			i386_load_segment_descriptor(cpustate,DS);
			i386_load_segment_descriptor(cpustate,FS);
			i386_load_segment_descriptor(cpustate,GS);
			i386_load_segment_descriptor(cpustate,SS);
			cpustate->CPL = 3;  // Virtual 8086 tasks are always run at CPL 3
		}
		else
		{
			if(operand32 == 0)
			{
				UINT32 offset = (STACK_32BIT ? REG32(ESP) : REG16(SP));
				if(i386_limit_check(cpustate,SS,offset+3) != 0)
				{
					logerror("IRET: Data on stack is past SS limit.\n");
					FAULT(FAULT_SS,0)
				}
			}
			else
			{
				UINT32 offset = (STACK_32BIT ? REG32(ESP) : REG16(SP));
				if(i386_limit_check(cpustate,SS,offset+7) != 0)
				{
					logerror("IRET: Data on stack is past SS limit.\n");
					FAULT(FAULT_SS,0)
				}
			}
			RPL = newCS & 0x03;
			if(RPL < CPL)
			{
				logerror("IRET (%08x): Return CS RPL is less than CPL.\n",cpustate->pc);
				FAULT(FAULT_GP,newCS & ~0x03)
			}
			if(RPL == CPL)
			{
				/* return to same privilege level */
				if(operand32 == 0)
				{
					UINT32 offset = (STACK_32BIT ? REG32(ESP) : REG16(SP));
					if(i386_limit_check(cpustate,SS,offset+5) != 0)
					{
						logerror("IRET (%08x): Data on stack is past SS limit.\n",cpustate->pc);
						FAULT(FAULT_SS,0)
					}
				}
				else
				{
					UINT32 offset = (STACK_32BIT ? REG32(ESP) : REG16(SP));
					if(i386_limit_check(cpustate,SS,offset+11) != 0)
					{
						logerror("IRET (%08x): Data on stack is past SS limit.\n",cpustate->pc);
						FAULT(FAULT_SS,0)
					}
				}
				if((newCS & ~0x03) == 0)
				{
					logerror("IRET: Return CS selector is null.\n");
					FAULT(FAULT_GP,0)
				}
				if(newCS & 0x04)
				{
					if((newCS & ~0x07) >= cpustate->ldtr.limit)
					{
						logerror("IRET: Return CS selector (%04x) is past LDT limit.\n",newCS);
						FAULT(FAULT_GP,newCS & ~0x03)
					}
				}
				else
				{
					if((newCS & ~0x07) >= cpustate->gdtr.limit)
					{
						logerror("IRET: Return CS selector is past GDT limit.\n");
						FAULT(FAULT_GP,newCS & ~0x03)
					}
				}
				memset(&desc, 0, sizeof(desc));
				desc.selector = newCS;
				i386_load_protected_mode_segment(cpustate,&desc,NULL);
				DPL = (desc.flags >> 5) & 0x03;  // descriptor privilege level
				RPL = newCS & 0x03;
				if((desc.flags & 0x0018) != 0x0018)
				{
					logerror("IRET (%08x): Return CS segment is not a code segment.\n",cpustate->pc);
					FAULT(FAULT_GP,newCS & ~0x07)
				}
				if(desc.flags & 0x0004)
				{
					if(DPL > RPL)
					{
						logerror("IRET: Conforming return CS DPL is greater than CS RPL.\n");
						FAULT(FAULT_GP,newCS & ~0x03)
					}
				}
				else
				{
					if(DPL != RPL)
					{
						logerror("IRET: Non-conforming return CS DPL is not equal to CS RPL.\n");
						FAULT(FAULT_GP,newCS & ~0x03)
					}
				}
				if((desc.flags & 0x0080) == 0)
				{
					logerror("IRET: Return CS segment is not present.\n");
					FAULT(FAULT_NP,newCS & ~0x03)
				}
				if(newEIP > desc.limit)
				{
					logerror("IRET: Return EIP is past return CS limit.\n");
					FAULT(FAULT_GP,0)
				}

				if(CPL != 0)
				{
					UINT32 oldflags = get_flags(cpustate);
					newflags = (newflags & ~0x00003000) | (oldflags & 0x00003000);
				}

				if(operand32 == 0)
				{
					cpustate->eip = newEIP;
					cpustate->sreg[CS].selector = newCS;
					set_flags(cpustate,newflags);
					REG16(SP) += 6;
				}
				else
				{
					cpustate->eip = newEIP;
					cpustate->sreg[CS].selector = newCS & 0xffff;
					set_flags(cpustate,newflags);
					REG32(ESP) += 12;
				}
			}
			else if(RPL > CPL)
			{
				/* return to outer privilege level */
				memset(&desc, 0, sizeof(desc));
				desc.selector = newCS;
				i386_load_protected_mode_segment(cpustate,&desc,NULL);
				DPL = (desc.flags >> 5) & 0x03;  // descriptor privilege level
				RPL = newCS & 0x03;
				if(operand32 == 0)
				{
					UINT32 offset = (STACK_32BIT ? REG32(ESP) : REG16(SP));
					if(i386_limit_check(cpustate,SS,offset+9) != 0)
					{
						logerror("IRET: SP is past SS limit.\n");
						FAULT(FAULT_SS,0)
					}
				}
				else
				{
					UINT32 offset = (STACK_32BIT ? REG32(ESP) : REG16(SP));
					if(i386_limit_check(cpustate,SS,offset+19) != 0)
					{
						logerror("IRET: ESP is past SS limit.\n");
						FAULT(FAULT_SS,0)
					}
				}
				/* Check CS selector and descriptor */
				if((newCS & ~0x03) == 0)
				{
					logerror("IRET: Return CS selector is null.\n");
					FAULT(FAULT_GP,0)
				}
				if(newCS & 0x04)
				{
					if((newCS & ~0x07) >= cpustate->ldtr.limit)
					{
						logerror("IRET: Return CS selector is past LDT limit.\n");
						FAULT(FAULT_GP,newCS & ~0x03);
					}
				}
				else
				{
					if((newCS & ~0x07) >= cpustate->gdtr.limit)
					{
						logerror("IRET: Return CS selector is past GDT limit.\n");
						FAULT(FAULT_GP,newCS & ~0x03);
					}
				}
				if((desc.flags & 0x0018) != 0x0018)
				{
					logerror("IRET: Return CS segment is not a code segment.\n");
					FAULT(FAULT_GP,newCS & ~0x03)
				}
				if(desc.flags & 0x0004)
				{
					if(DPL > RPL)
					{
						logerror("IRET: Conforming return CS DPL is greater than CS RPL.\n");
						FAULT(FAULT_GP,newCS & ~0x03)
					}
				}
				else
				{
					if(DPL != RPL)
					{
						logerror("IRET: Non-conforming return CS DPL does not equal CS RPL.\n");
						FAULT(FAULT_GP,newCS & ~0x03)
					}
				}
				if((desc.flags & 0x0080) == 0)
				{
					logerror("IRET: Return CS segment is not present.\n");
					FAULT(FAULT_NP,newCS & ~0x03)
				}

				/* Check SS selector and descriptor */
				if(operand32 == 0)
				{
					newESP = READ16(cpustate, ea+6) & 0xffff;
					newSS = READ16(cpustate, ea+8) & 0xffff;
				}
				else
				{
					newESP = READ32(cpustate, ea+12);
					newSS = READ32(cpustate, ea+16) & 0xffff;
				}
				memset(&stack, 0, sizeof(stack));
				stack.selector = newSS;
				i386_load_protected_mode_segment(cpustate,&stack,NULL);
				DPL = (stack.flags >> 5) & 0x03;
				if((newSS & ~0x03) == 0)
				{
					logerror("IRET: Return SS selector is null.\n");
					FAULT(FAULT_GP,0)
				}
				if(newSS & 0x04)
				{
					if((newSS & ~0x07) >= cpustate->ldtr.limit)
					{
						logerror("IRET: Return SS selector is past LDT limit.\n");
						FAULT(FAULT_GP,newSS & ~0x03);
					}
				}
				else
				{
					if((newSS & ~0x07) >= cpustate->gdtr.limit)
					{
						logerror("IRET: Return SS selector is past GDT limit.\n");
						FAULT(FAULT_GP,newSS & ~0x03);
					}
				}
				if((newSS & 0x03) != RPL)
				{
					logerror("IRET: Return SS RPL is not equal to return CS RPL.\n");
					FAULT(FAULT_GP,newSS & ~0x03)
				}
				if((stack.flags & 0x0018) != 0x0010)
				{
					logerror("IRET: Return SS segment is not a data segment.\n");
					FAULT(FAULT_GP,newSS & ~0x03)
				}
				if((stack.flags & 0x0002) == 0)
				{
					logerror("IRET: Return SS segment is not writable.\n");
					FAULT(FAULT_GP,newSS & ~0x03)
				}
				if(DPL != RPL)
				{
					logerror("IRET: Return SS DPL does not equal SS RPL.\n");
					FAULT(FAULT_GP,newSS & ~0x03)
				}
				if((stack.flags & 0x0080) == 0)
				{
					logerror("IRET: Return SS segment is not present.\n");
					FAULT(FAULT_NP,newSS & ~0x03)
				}
				if(newEIP > desc.limit)
				{
					logerror("IRET: EIP is past return CS limit.\n");
					FAULT(FAULT_GP,0)
				}

//              if(operand32 == 0)
//                  REG16(SP) += 10;
//              else
//                  REG32(ESP) += 20;

				// IOPL can only change if CPL is zero
				if(CPL != 0)
				{
					UINT32 oldflags = get_flags(cpustate);
					newflags = (newflags & ~0x00003000) | (oldflags & 0x00003000);
				}

				if(operand32 == 0)
				{
					cpustate->eip = newEIP & 0xffff;
					cpustate->sreg[CS].selector = newCS;
					set_flags(cpustate,newflags);
					REG16(SP) = newESP & 0xffff;
					cpustate->sreg[SS].selector = newSS;
				}
				else
				{
					cpustate->eip = newEIP;
					cpustate->sreg[CS].selector = newCS & 0xffff;
					set_flags(cpustate,newflags);
					REG32(ESP) = newESP;
					cpustate->sreg[SS].selector = newSS & 0xffff;
				}
				cpustate->CPL = newCS & 0x03;
				i386_load_segment_descriptor(cpustate,SS);

				/* Check that DS, ES, FS and GS are valid for the new privilege level */
				i386_check_sreg_validity(cpustate,DS);
				i386_check_sreg_validity(cpustate,ES);
				i386_check_sreg_validity(cpustate,FS);
				i386_check_sreg_validity(cpustate,GS);
			}
		}
	}

	i386_load_segment_descriptor(cpustate,CS);
	CHANGE_PC(cpustate,cpustate->eip);
}

#include "cycles.h"

static UINT8 *cycle_table_rm[X86_NUM_CPUS];
static UINT8 *cycle_table_pm[X86_NUM_CPUS];

#define CYCLES_NUM(x)	(cpustate->cycles -= (x))

INLINE void CYCLES(i386_state *cpustate,int x)
{
	if (PROTECTED_MODE)
	{
		cpustate->cycles -= cpustate->cycle_table_pm[x];
	}
	else
	{
		cpustate->cycles -= cpustate->cycle_table_rm[x];
	}
}

INLINE void CYCLES_RM(i386_state *cpustate,int modrm, int r, int m)
{
	if (modrm >= 0xc0)
	{
		if (PROTECTED_MODE)
		{
			cpustate->cycles -= cpustate->cycle_table_pm[r];
		}
		else
		{
			cpustate->cycles -= cpustate->cycle_table_rm[r];
		}
	}
	else
	{
		if (PROTECTED_MODE)
		{
			cpustate->cycles -= cpustate->cycle_table_pm[m];
		}
		else
		{
			cpustate->cycles -= cpustate->cycle_table_rm[m];
		}
	}
}

static void build_cycle_table()
{
	int i, j;
	for (j=0; j < X86_NUM_CPUS; j++)
	{
		cycle_table_rm[j] = (UINT8 *)malloc(sizeof(UINT8) * CYCLES_NUM_OPCODES);
		cycle_table_pm[j] = (UINT8 *)malloc(sizeof(UINT8) * CYCLES_NUM_OPCODES);

		for (i=0; i < sizeof(x86_cycle_table)/sizeof(X86_CYCLE_TABLE); i++)
		{
			int opcode = x86_cycle_table[i].op;
			cycle_table_rm[j][opcode] = x86_cycle_table[i].cpu_cycles[j][0];
			cycle_table_pm[j][opcode] = x86_cycle_table[i].cpu_cycles[j][1];
		}
	}
}

static void report_invalid_opcode(i386_state *cpustate)
{
#ifndef DEBUG_MISSING_OPCODE
	logerror("i386: Invalid opcode %02X at %08X\n", cpustate->opcode, cpustate->pc - 1);
#else
	logerror("i386: Invalid opcode");
	for (int a = 0; a < cpustate->opcode_bytes_length; a++)
		logerror(" %02X", cpustate->opcode_bytes[a]);
	logerror(" at %08X\n", cpustate->opcode_pc);
#endif
}

static void report_unimplemented_opcode(i386_state *cpustate)
{
#ifndef DEBUG_MISSING_OPCODE
	fatalerror("i386: Unimplemented opcode %02X at %08X\n", cpustate->opcode, cpustate->pc - 1 );
#else
	astring errmsg;
	errmsg.cat("i386: Unimplemented opcode ");
	for (int a = 0; a < cpustate->opcode_bytes_length; a++)
		errmsg.catprintf(" %02X", cpustate->opcode_bytes[a]);
	errmsg.catprintf(" at %08X", cpustate->opcode_pc );
#endif
}

/* Forward declarations */
static void I386OP(decode_opcode)(i386_state *cpustate);
static void I386OP(decode_two_byte)(i386_state *cpustate);
static void I386OP(decode_three_byte66)(i386_state *cpustate);
static void I386OP(decode_three_bytef2)(i386_state *cpustate);
static void I386OP(decode_three_bytef3)(i386_state *cpustate);



#include "i386ops.c"
#include "i386op16.c"
#include "i386op32.c"
#include "i486ops.c"
#include "pentops.c"
#include "x87ops.c"
#include "i386ops.h"

static void I386OP(decode_opcode)(i386_state *cpustate)
{
	cpustate->opcode = FETCH(cpustate);
	if( cpustate->operand_size )
		cpustate->opcode_table1_32[cpustate->opcode](cpustate);
	else
		cpustate->opcode_table1_16[cpustate->opcode](cpustate);
}

/* Two-byte opcode prefix */
static void I386OP(decode_two_byte)(i386_state *cpustate)
{
	cpustate->opcode = FETCH(cpustate);
	if( cpustate->operand_size )
		cpustate->opcode_table2_32[cpustate->opcode](cpustate);
	else
		cpustate->opcode_table2_16[cpustate->opcode](cpustate);
}

/* Three-byte opcode prefix 66 0f */
static void I386OP(decode_three_byte66)(i386_state *cpustate)
{
	cpustate->opcode = FETCH(cpustate);
	if( cpustate->operand_size )
		cpustate->opcode_table366_32[cpustate->opcode](cpustate);
	else
		cpustate->opcode_table366_16[cpustate->opcode](cpustate);
}

/* Three-byte opcode prefix f2 0f */
static void I386OP(decode_three_bytef2)(i386_state *cpustate)
{
	cpustate->opcode = FETCH(cpustate);
	if( cpustate->operand_size )
		cpustate->opcode_table3f2_32[cpustate->opcode](cpustate);
	else
		cpustate->opcode_table3f2_16[cpustate->opcode](cpustate);
}

/* Three-byte opcode prefix f3 0f */
static void I386OP(decode_three_bytef3)(i386_state *cpustate)
{
	cpustate->opcode = FETCH(cpustate);
	if( cpustate->operand_size )
		cpustate->opcode_table3f3_32[cpustate->opcode](cpustate);
	else
		cpustate->opcode_table3f3_16[cpustate->opcode](cpustate);
}

/*************************************************************************/

static void i386_postload(i386_state *cpustate)
{
	int i;
	for (i = 0; i < 6; i++)
		i386_load_segment_descriptor(cpustate,i);
	CHANGE_PC(cpustate,cpustate->eip);
}

static CPU_INIT( i386 )
{
	int i, j;
	static const int regs8[8] = {AL,CL,DL,BL,AH,CH,DH,BH};
	static const int regs16[8] = {AX,CX,DX,BX,SP,BP,SI,DI};
	static const int regs32[8] = {EAX,ECX,EDX,EBX,ESP,EBP,ESI,EDI};
	i386_state *cpustate = (i386_state *)malloc(sizeof(i386_state));

	build_cycle_table();

	for( i=0; i < 256; i++ ) {
		int c=0;
		for( j=0; j < 8; j++ ) {
			if( i & (1 << j) )
				c++;
		}
		i386_parity_table[i] = ~(c & 0x1) & 0x1;
	}

	for( i=0; i < 256; i++ ) {
		i386_MODRM_table[i].reg.b = regs8[(i >> 3) & 0x7];
		i386_MODRM_table[i].reg.w = regs16[(i >> 3) & 0x7];
		i386_MODRM_table[i].reg.d = regs32[(i >> 3) & 0x7];

		i386_MODRM_table[i].rm.b = regs8[i & 0x7];
		i386_MODRM_table[i].rm.w = regs16[i & 0x7];
		i386_MODRM_table[i].rm.d = regs32[i & 0x7];
	}

	return cpustate;
}

static void build_opcode_table(i386_state *cpustate, UINT32 features)
{
	int i;
	for (i=0; i < 256; i++)
	{
		cpustate->opcode_table1_16[i] = I386OP(invalid);
		cpustate->opcode_table1_32[i] = I386OP(invalid);
		cpustate->opcode_table2_16[i] = I386OP(invalid);
		cpustate->opcode_table2_32[i] = I386OP(invalid);
		cpustate->opcode_table366_16[i] = I386OP(invalid);
		cpustate->opcode_table366_32[i] = I386OP(invalid);
		cpustate->opcode_table3f2_16[i] = I386OP(invalid);
		cpustate->opcode_table3f2_32[i] = I386OP(invalid);
		cpustate->opcode_table3f3_16[i] = I386OP(invalid);
		cpustate->opcode_table3f3_32[i] = I386OP(invalid);
	}

	for (i=0; i < sizeof(x86_opcode_table)/sizeof(X86_OPCODE); i++)
	{
		const X86_OPCODE *op = &x86_opcode_table[i];

		if ((op->flags & features))
		{
			if (op->flags & OP_2BYTE)
			{
				cpustate->opcode_table2_32[op->opcode] = op->handler32;
				cpustate->opcode_table2_16[op->opcode] = op->handler16;
				cpustate->opcode_table366_32[op->opcode] = op->handler32;
				cpustate->opcode_table366_16[op->opcode] = op->handler16;
			}
			else if (op->flags & OP_3BYTE66)
			{
				cpustate->opcode_table366_32[op->opcode] = op->handler32;
				cpustate->opcode_table366_16[op->opcode] = op->handler16;
			}
			else if (op->flags & OP_3BYTEF2)
			{
				cpustate->opcode_table3f2_32[op->opcode] = op->handler32;
				cpustate->opcode_table3f2_16[op->opcode] = op->handler16;
			}
			else if (op->flags & OP_3BYTEF3)
			{
				cpustate->opcode_table3f3_32[op->opcode] = op->handler32;
				cpustate->opcode_table3f3_16[op->opcode] = op->handler16;
			}
			else
			{
				cpustate->opcode_table1_32[op->opcode] = op->handler32;
				cpustate->opcode_table1_16[op->opcode] = op->handler16;
			}
		}
	}
}

static CPU_RESET( i386 )
{
	memset( cpustate, 0, sizeof(*cpustate) );

	cpustate->sreg[CS].selector = 0xf000;
	cpustate->sreg[CS].base		= 0xffff0000;
	cpustate->sreg[CS].limit	= 0xffff;
	cpustate->sreg[CS].valid	= true;

	cpustate->sreg[DS].base = cpustate->sreg[ES].base = cpustate->sreg[FS].base = cpustate->sreg[GS].base = cpustate->sreg[SS].base = 0x00000000;
	cpustate->sreg[DS].limit = cpustate->sreg[ES].limit = cpustate->sreg[FS].limit = cpustate->sreg[GS].limit = cpustate->sreg[SS].limit = 0xffff;
	cpustate->sreg[DS].flags = cpustate->sreg[ES].flags = cpustate->sreg[FS].flags = cpustate->sreg[GS].flags = cpustate->sreg[SS].flags = 0x0092;
	cpustate->sreg[DS].valid = cpustate->sreg[ES].valid = cpustate->sreg[FS].valid = cpustate->sreg[GS].valid = cpustate->sreg[SS].valid =true;

	cpustate->idtr.base = 0;
	cpustate->idtr.limit = 0x3ff;

	cpustate->a20_mask = ~0;

	cpustate->cr[0] = 0x7fffffe0; // reserved bits set to 1
	cpustate->eflags = 0;
	cpustate->eflags_mask = 0x00037fd7;
	cpustate->eip = 0xfff0;

	// [11:8] Family
	// [ 7:4] Model
	// [ 3:0] Stepping ID
	// Family 3 (386), Model 0 (DX), Stepping 8 (D1)
	REG32(EAX) = 0;
	REG32(EDX) = (3 << 8) | (0 << 4) | (8);

	cpustate->CPL = 0;

	build_opcode_table(cpustate, OP_I386);
	cpustate->cycle_table_rm = cycle_table_rm[CPU_CYCLES_I386];
	cpustate->cycle_table_pm = cycle_table_pm[CPU_CYCLES_I386];

	CHANGE_PC(cpustate,cpustate->eip);
}

static void i386_set_irq_line(i386_state *cpustate,int irqline, int state)
{
	if (state != CLEAR_LINE && cpustate->halted)
	{
		cpustate->halted = 0;
	}

	if ( irqline == INPUT_LINE_NMI )
	{
		/* NMI (I do not think that this is 100% right) */
		if ( state )
			i386_trap(cpustate,2, 1, 0);
	}
	else
	{
		cpustate->irq_state = state;
	}
}

static void i386_set_a20_line(i386_state *cpustate,int state)
{
	if (state)
	{
		cpustate->a20_mask = ~0;
	}
	else
	{
		cpustate->a20_mask = ~(1 << 20);
	}
}

static CPU_EXECUTE( i386 )
{
	int cycles = cpustate->cycles;
	cpustate->base_cycles = cycles;
	CHANGE_PC(cpustate,cpustate->eip);

	if (cpustate->halted)
	{
		cpustate->tsc += cycles;
		cpustate->cycles = 0;
		return;
	}

//	while( cpustate->cycles > 0 )
	{
		i386_check_irq_line(cpustate);
		cpustate->operand_size = cpustate->sreg[CS].d;
		cpustate->address_size = cpustate->sreg[CS].d;
		cpustate->operand_prefix = 0;
		cpustate->address_prefix = 0;

		cpustate->ext = 1;
		int old_tf = cpustate->TF;

		cpustate->segment_prefix = 0;
		cpustate->prev_eip = cpustate->eip;

		if(cpustate->delayed_interrupt_enable != 0)
		{
			cpustate->IF = 1;
			cpustate->delayed_interrupt_enable = 0;
		}
#ifdef DEBUG_MISSING_OPCODE
		cpustate->opcode_bytes_length = 0;
		cpustate->opcode_pc = cpustate->pc;
#endif
		try
		{
			I386OP(decode_opcode)(cpustate);
			if(cpustate->TF && old_tf)
			{
				cpustate->prev_eip = cpustate->eip;
				cpustate->ext = 1;
				i386_trap(cpustate,1,0,0);
			}

		}
		catch(UINT64 e)
		{
			cpustate->ext = 1;
			i386_trap_with_error(cpustate,e&0xffffffff,0,0,e>>32);
		}
	}
	cpustate->tsc += (cycles - cpustate->cycles);
}

/*****************************************************************************/
/* Intel 486 */


static CPU_INIT( i486 )
{
	return CPU_INIT_CALL(i386);
}

static CPU_RESET( i486 )
{
	memset( cpustate, 0, sizeof(*cpustate) );

	cpustate->sreg[CS].selector = 0xf000;
	cpustate->sreg[CS].base		= 0xffff0000;
	cpustate->sreg[CS].limit	= 0xffff;
	cpustate->sreg[CS].flags    = 0x009b;

	cpustate->sreg[DS].base = cpustate->sreg[ES].base = cpustate->sreg[FS].base = cpustate->sreg[GS].base = cpustate->sreg[SS].base = 0x00000000;
	cpustate->sreg[DS].limit = cpustate->sreg[ES].limit = cpustate->sreg[FS].limit = cpustate->sreg[GS].limit = cpustate->sreg[SS].limit = 0xffff;
	cpustate->sreg[DS].flags = cpustate->sreg[ES].flags = cpustate->sreg[FS].flags = cpustate->sreg[GS].flags = cpustate->sreg[SS].flags = 0x0092;

	cpustate->idtr.base = 0;
	cpustate->idtr.limit = 0x3ff;

	cpustate->a20_mask = ~0;

	cpustate->cr[0] = 0x00000010;
	cpustate->eflags = 0;
	cpustate->eflags_mask = 0x00077fd7;
	cpustate->eip = 0xfff0;

	x87_reset(cpustate);

	// [11:8] Family
	// [ 7:4] Model
	// [ 3:0] Stepping ID
	// Family 4 (486), Model 0/1 (DX), Stepping 3
	REG32(EAX) = 0;
	REG32(EDX) = (4 << 8) | (0 << 4) | (3);

	build_opcode_table(cpustate, OP_I386 | OP_FPU | OP_I486);
	build_x87_opcode_table(cpustate);
	cpustate->cycle_table_rm = cycle_table_rm[CPU_CYCLES_I486];
	cpustate->cycle_table_pm = cycle_table_pm[CPU_CYCLES_I486];

	CHANGE_PC(cpustate,cpustate->eip);
}

/*****************************************************************************/
/* Pentium */


static CPU_INIT( pentium )
{
	return CPU_INIT_CALL(i386);
}

static CPU_RESET( pentium )
{
	memset( cpustate, 0, sizeof(*cpustate) );

	cpustate->sreg[CS].selector = 0xf000;
	cpustate->sreg[CS].base		= 0xffff0000;
	cpustate->sreg[CS].limit	= 0xffff;
	cpustate->sreg[CS].flags    = 0x009b;

	cpustate->sreg[DS].base = cpustate->sreg[ES].base = cpustate->sreg[FS].base = cpustate->sreg[GS].base = cpustate->sreg[SS].base = 0x00000000;
	cpustate->sreg[DS].limit = cpustate->sreg[ES].limit = cpustate->sreg[FS].limit = cpustate->sreg[GS].limit = cpustate->sreg[SS].limit = 0xffff;
	cpustate->sreg[DS].flags = cpustate->sreg[ES].flags = cpustate->sreg[FS].flags = cpustate->sreg[GS].flags = cpustate->sreg[SS].flags = 0x0092;

	cpustate->idtr.base = 0;
	cpustate->idtr.limit = 0x3ff;

	cpustate->a20_mask = ~0;

	cpustate->cr[0] = 0x00000010;
	cpustate->eflags = 0x00200000;
	cpustate->eflags_mask = 0x003f7fd7;
	cpustate->eip = 0xfff0;
	cpustate->mxcsr = 0x1f80;

	x87_reset(cpustate);

	// [11:8] Family
	// [ 7:4] Model
	// [ 3:0] Stepping ID
	// Family 5 (Pentium), Model 2 (75 - 200MHz), Stepping 5
	REG32(EAX) = 0;
	REG32(EDX) = (5 << 8) | (2 << 4) | (5);

	build_opcode_table(cpustate, OP_I386 | OP_FPU | OP_I486 | OP_PENTIUM);
	build_x87_opcode_table(cpustate);
	cpustate->cycle_table_rm = cycle_table_rm[CPU_CYCLES_PENTIUM];
	cpustate->cycle_table_pm = cycle_table_pm[CPU_CYCLES_PENTIUM];

	cpustate->cpuid_id0 = 0x756e6547;	// Genu
	cpustate->cpuid_id1 = 0x49656e69;	// ineI
	cpustate->cpuid_id2 = 0x6c65746e;	// ntel

	cpustate->cpuid_max_input_value_eax = 0x01;
	cpustate->cpu_version = REG32(EDX);

	// [ 0:0] FPU on chip
	// [ 2:2] I/O breakpoints
	// [ 4:4] Time Stamp Counter
	// [ 5:5] Pentium CPU style model specific registers
	// [ 7:7] Machine Check Exception
	// [ 8:8] CMPXCHG8B instruction
	cpustate->feature_flags = 0x000001bf;

	CHANGE_PC(cpustate,cpustate->eip);
}

/*****************************************************************************/
/* Cyrix MediaGX */


static CPU_INIT( mediagx )
{
	return CPU_INIT_CALL(i386);
}

static CPU_RESET( mediagx )
{
	memset( cpustate, 0, sizeof(*cpustate) );

	cpustate->sreg[CS].selector = 0xf000;
	cpustate->sreg[CS].base		= 0xffff0000;
	cpustate->sreg[CS].limit	= 0xffff;
	cpustate->sreg[CS].flags    = 0x009b;

	cpustate->sreg[DS].base = cpustate->sreg[ES].base = cpustate->sreg[FS].base = cpustate->sreg[GS].base = cpustate->sreg[SS].base = 0x00000000;
	cpustate->sreg[DS].limit = cpustate->sreg[ES].limit = cpustate->sreg[FS].limit = cpustate->sreg[GS].limit = cpustate->sreg[SS].limit = 0xffff;
	cpustate->sreg[DS].flags = cpustate->sreg[ES].flags = cpustate->sreg[FS].flags = cpustate->sreg[GS].flags = cpustate->sreg[SS].flags = 0x0092;

	cpustate->idtr.base = 0;
	cpustate->idtr.limit = 0x3ff;

	cpustate->a20_mask = ~0;

	cpustate->cr[0] = 0x00000010;
	cpustate->eflags = 0x00200000;
	cpustate->eflags_mask = 0x00277fd7; /* TODO: is this correct? */
	cpustate->eip = 0xfff0;

	x87_reset(cpustate);

	// [11:8] Family
	// [ 7:4] Model
	// [ 3:0] Stepping ID
	// Family 4, Model 4 (MediaGX)
	REG32(EAX) = 0;
	REG32(EDX) = (4 << 8) | (4 << 4) | (1);	/* TODO: is this correct? */

	build_x87_opcode_table(cpustate);
	build_opcode_table(cpustate, OP_I386 | OP_FPU | OP_I486 | OP_PENTIUM | OP_CYRIX);
	cpustate->cycle_table_rm = cycle_table_rm[CPU_CYCLES_MEDIAGX];
	cpustate->cycle_table_pm = cycle_table_pm[CPU_CYCLES_MEDIAGX];

	cpustate->cpuid_id0 = 0x69727943;	// Cyri
	cpustate->cpuid_id1 = 0x736e4978;	// xIns
	cpustate->cpuid_id2 = 0x6d616574;	// tead

	cpustate->cpuid_max_input_value_eax = 0x01;
	cpustate->cpu_version = REG32(EDX);

	// [ 0:0] FPU on chip
	cpustate->feature_flags = 0x00000001;

	CHANGE_PC(cpustate,cpustate->eip);
}

/*****************************************************************************/
/* Intel Pentium Pro */

static CPU_INIT( pentium_pro )
{
	return CPU_INIT_CALL(pentium);
}

static CPU_RESET( pentium_pro )
{
	memset( cpustate, 0, sizeof(*cpustate) );

	cpustate->sreg[CS].selector = 0xf000;
	cpustate->sreg[CS].base		= 0xffff0000;
	cpustate->sreg[CS].limit	= 0xffff;
	cpustate->sreg[CS].flags    = 0x009b;

	cpustate->sreg[DS].base = cpustate->sreg[ES].base = cpustate->sreg[FS].base = cpustate->sreg[GS].base = cpustate->sreg[SS].base = 0x00000000;
	cpustate->sreg[DS].limit = cpustate->sreg[ES].limit = cpustate->sreg[FS].limit = cpustate->sreg[GS].limit = cpustate->sreg[SS].limit = 0xffff;
	cpustate->sreg[DS].flags = cpustate->sreg[ES].flags = cpustate->sreg[FS].flags = cpustate->sreg[GS].flags = cpustate->sreg[SS].flags = 0x0092;

	cpustate->idtr.base = 0;
	cpustate->idtr.limit = 0x3ff;

	cpustate->a20_mask = ~0;

	cpustate->cr[0] = 0x60000010;
	cpustate->eflags = 0x00200000;
	cpustate->eflags_mask = 0x00277fd7; /* TODO: is this correct? */
	cpustate->eip = 0xfff0;
	cpustate->mxcsr = 0x1f80;

	x87_reset(cpustate);

	// [11:8] Family
	// [ 7:4] Model
	// [ 3:0] Stepping ID
	// Family 6, Model 1 (Pentium Pro)
	REG32(EAX) = 0;
	REG32(EDX) = (6 << 8) | (1 << 4) | (1);	/* TODO: is this correct? */

	build_x87_opcode_table(cpustate);
	build_opcode_table(cpustate, OP_I386 | OP_FPU | OP_I486 | OP_PENTIUM | OP_PPRO);
	cpustate->cycle_table_rm = cycle_table_rm[CPU_CYCLES_PENTIUM];	// TODO: generate own cycle tables
	cpustate->cycle_table_pm = cycle_table_pm[CPU_CYCLES_PENTIUM];	// TODO: generate own cycle tables

	cpustate->cpuid_id0 = 0x756e6547;	// Genu
	cpustate->cpuid_id1 = 0x49656e69;	// ineI
	cpustate->cpuid_id2 = 0x6c65746e;	// ntel

	cpustate->cpuid_max_input_value_eax = 0x02;
	cpustate->cpu_version = REG32(EDX);

	// [ 0:0] FPU on chip
	cpustate->feature_flags = 0x00000001;		// TODO: enable relevant flags here

	CHANGE_PC(cpustate,cpustate->eip);
}

/*****************************************************************************/
/* Intel Pentium MMX */

static CPU_INIT( pentium_mmx )
{
	return CPU_INIT_CALL(pentium);
}

static CPU_RESET( pentium_mmx )
{
	memset( cpustate, 0, sizeof(*cpustate) );

	cpustate->sreg[CS].selector = 0xf000;
	cpustate->sreg[CS].base		= 0xffff0000;
	cpustate->sreg[CS].limit	= 0xffff;
	cpustate->sreg[CS].flags    = 0x009b;

	cpustate->sreg[DS].base = cpustate->sreg[ES].base = cpustate->sreg[FS].base = cpustate->sreg[GS].base = cpustate->sreg[SS].base = 0x00000000;
	cpustate->sreg[DS].limit = cpustate->sreg[ES].limit = cpustate->sreg[FS].limit = cpustate->sreg[GS].limit = cpustate->sreg[SS].limit = 0xffff;
	cpustate->sreg[DS].flags = cpustate->sreg[ES].flags = cpustate->sreg[FS].flags = cpustate->sreg[GS].flags = cpustate->sreg[SS].flags = 0x0092;

	cpustate->idtr.base = 0;
	cpustate->idtr.limit = 0x3ff;

	cpustate->a20_mask = ~0;

	cpustate->cr[0] = 0x60000010;
	cpustate->eflags = 0x00200000;
	cpustate->eflags_mask = 0x00277fd7; /* TODO: is this correct? */
	cpustate->eip = 0xfff0;
	cpustate->mxcsr = 0x1f80;

	x87_reset(cpustate);

	// [11:8] Family
	// [ 7:4] Model
	// [ 3:0] Stepping ID
	// Family 5, Model 4 (P55C)
	REG32(EAX) = 0;
	REG32(EDX) = (5 << 8) | (4 << 4) | (1);

	build_x87_opcode_table(cpustate);
	build_opcode_table(cpustate, OP_I386 | OP_FPU | OP_I486 | OP_PENTIUM | OP_MMX);
	cpustate->cycle_table_rm = cycle_table_rm[CPU_CYCLES_PENTIUM];	// TODO: generate own cycle tables
	cpustate->cycle_table_pm = cycle_table_pm[CPU_CYCLES_PENTIUM];	// TODO: generate own cycle tables

	cpustate->cpuid_id0 = 0x756e6547;	// Genu
	cpustate->cpuid_id1 = 0x49656e69;	// ineI
	cpustate->cpuid_id2 = 0x6c65746e;	// ntel

	cpustate->cpuid_max_input_value_eax = 0x01;
	cpustate->cpu_version = REG32(EDX);

	// [ 0:0] FPU on chip
	cpustate->feature_flags = 0x00000001;		// TODO: enable relevant flags here

	CHANGE_PC(cpustate,cpustate->eip);
}

/*****************************************************************************/
/* Intel Pentium II */

static CPU_INIT( pentium2 )
{
	return CPU_INIT_CALL(pentium);
}

static CPU_RESET( pentium2 )
{
	memset( cpustate, 0, sizeof(*cpustate) );

	cpustate->sreg[CS].selector = 0xf000;
	cpustate->sreg[CS].base		= 0xffff0000;
	cpustate->sreg[CS].limit	= 0xffff;
	cpustate->sreg[CS].flags    = 0x009b;

	cpustate->sreg[DS].base = cpustate->sreg[ES].base = cpustate->sreg[FS].base = cpustate->sreg[GS].base = cpustate->sreg[SS].base = 0x00000000;
	cpustate->sreg[DS].limit = cpustate->sreg[ES].limit = cpustate->sreg[FS].limit = cpustate->sreg[GS].limit = cpustate->sreg[SS].limit = 0xffff;
	cpustate->sreg[DS].flags = cpustate->sreg[ES].flags = cpustate->sreg[FS].flags = cpustate->sreg[GS].flags = cpustate->sreg[SS].flags = 0x0092;

	cpustate->idtr.base = 0;
	cpustate->idtr.limit = 0x3ff;

	cpustate->a20_mask = ~0;

	cpustate->cr[0] = 0x60000010;
	cpustate->eflags = 0x00200000;
	cpustate->eflags_mask = 0x00277fd7; /* TODO: is this correct? */
	cpustate->eip = 0xfff0;
	cpustate->mxcsr = 0x1f80;

	x87_reset(cpustate);

	// [11:8] Family
	// [ 7:4] Model
	// [ 3:0] Stepping ID
	// Family 6, Model 3 (Pentium II / Klamath)
	REG32(EAX) = 0;
	REG32(EDX) = (6 << 8) | (3 << 4) | (1);	/* TODO: is this correct? */

	build_x87_opcode_table(cpustate);
	build_opcode_table(cpustate, OP_I386 | OP_FPU | OP_I486 | OP_PENTIUM | OP_PPRO | OP_MMX);
	cpustate->cycle_table_rm = cycle_table_rm[CPU_CYCLES_PENTIUM];	// TODO: generate own cycle tables
	cpustate->cycle_table_pm = cycle_table_pm[CPU_CYCLES_PENTIUM];	// TODO: generate own cycle tables

	cpustate->cpuid_id0 = 0x756e6547;	// Genu
	cpustate->cpuid_id1 = 0x49656e69;	// ineI
	cpustate->cpuid_id2 = 0x6c65746e;	// ntel

	cpustate->cpuid_max_input_value_eax = 0x02;
	cpustate->cpu_version = REG32(EDX);

	// [ 0:0] FPU on chip
	cpustate->feature_flags = 0x00000001;		// TODO: enable relevant flags here

	CHANGE_PC(cpustate,cpustate->eip);
}

/*****************************************************************************/
/* Intel Pentium III */

static CPU_INIT( pentium3 )
{
	return CPU_INIT_CALL(pentium);
}

static CPU_RESET( pentium3 )
{
	memset( cpustate, 0, sizeof(*cpustate) );

	cpustate->sreg[CS].selector = 0xf000;
	cpustate->sreg[CS].base		= 0xffff0000;
	cpustate->sreg[CS].limit	= 0xffff;
	cpustate->sreg[CS].flags    = 0x009b;

	cpustate->sreg[DS].base = cpustate->sreg[ES].base = cpustate->sreg[FS].base = cpustate->sreg[GS].base = cpustate->sreg[SS].base = 0x00000000;
	cpustate->sreg[DS].limit = cpustate->sreg[ES].limit = cpustate->sreg[FS].limit = cpustate->sreg[GS].limit = cpustate->sreg[SS].limit = 0xffff;
	cpustate->sreg[DS].flags = cpustate->sreg[ES].flags = cpustate->sreg[FS].flags = cpustate->sreg[GS].flags = cpustate->sreg[SS].flags = 0x0092;

	cpustate->idtr.base = 0;
	cpustate->idtr.limit = 0x3ff;

	cpustate->a20_mask = ~0;

	cpustate->cr[0] = 0x60000010;
	cpustate->eflags = 0x00200000;
	cpustate->eflags_mask = 0x00277fd7; /* TODO: is this correct? */
	cpustate->eip = 0xfff0;
	cpustate->mxcsr = 0x1f80;

	x87_reset(cpustate);

	// [11:8] Family
	// [ 7:4] Model
	// [ 3:0] Stepping ID
	// Family 6, Model 8 (Pentium III / Coppermine)
	REG32(EAX) = 0;
	REG32(EDX) = (6 << 8) | (8 << 4) | (10);

	build_x87_opcode_table(cpustate);
	build_opcode_table(cpustate, OP_I386 | OP_FPU | OP_I486 | OP_PENTIUM | OP_PPRO | OP_MMX | OP_SSE);
	cpustate->cycle_table_rm = cycle_table_rm[CPU_CYCLES_PENTIUM];	// TODO: generate own cycle tables
	cpustate->cycle_table_pm = cycle_table_pm[CPU_CYCLES_PENTIUM];	// TODO: generate own cycle tables

	cpustate->cpuid_id0 = 0x756e6547;	// Genu
	cpustate->cpuid_id1 = 0x49656e69;	// ineI
	cpustate->cpuid_id2 = 0x6c65746e;	// ntel

	cpustate->cpuid_max_input_value_eax = 0x03;
	cpustate->cpu_version = REG32(EDX);

	// [ 0:0] FPU on chip
	cpustate->feature_flags = 0x00000001;		// TODO: enable relevant flags here

	CHANGE_PC(cpustate,cpustate->eip);
}

/*****************************************************************************/
/* Intel Pentium 4 */

static CPU_INIT( pentium4 )
{
	return CPU_INIT_CALL(pentium);
}

static CPU_RESET( pentium4 )
{
	memset( cpustate, 0, sizeof(*cpustate) );

	cpustate->sreg[CS].selector = 0xf000;
	cpustate->sreg[CS].base		= 0xffff0000;
	cpustate->sreg[CS].limit	= 0xffff;
	cpustate->sreg[CS].flags    = 0x009b;

	cpustate->sreg[DS].base = cpustate->sreg[ES].base = cpustate->sreg[FS].base = cpustate->sreg[GS].base = cpustate->sreg[SS].base = 0x00000000;
	cpustate->sreg[DS].limit = cpustate->sreg[ES].limit = cpustate->sreg[FS].limit = cpustate->sreg[GS].limit = cpustate->sreg[SS].limit = 0xffff;
	cpustate->sreg[DS].flags = cpustate->sreg[ES].flags = cpustate->sreg[FS].flags = cpustate->sreg[GS].flags = cpustate->sreg[SS].flags = 0x0092;

	cpustate->idtr.base = 0;
	cpustate->idtr.limit = 0x3ff;

	cpustate->a20_mask = ~0;

	cpustate->cr[0] = 0x60000010;
	cpustate->eflags = 0x00200000;
	cpustate->eflags_mask = 0x00277fd7; /* TODO: is this correct? */
	cpustate->eip = 0xfff0;
	cpustate->mxcsr = 0x1f80;

	x87_reset(cpustate);

	// [27:20] Extended family
	// [19:16] Extended model
	// [13:12] Type
	// [11: 8] Family
	// [ 7: 4] Model
	// [ 3: 0] Stepping ID
	// Family 15, Model 0 (Pentium 4 / Willamette)
	REG32(EAX) = 0;
	REG32(EDX) = (0 << 20) | (0xf << 8) | (0 << 4) | (1);

	build_x87_opcode_table(cpustate);
	build_opcode_table(cpustate, OP_I386 | OP_FPU | OP_I486 | OP_PENTIUM | OP_PPRO | OP_MMX | OP_SSE | OP_SSE2);
	cpustate->cycle_table_rm = cycle_table_rm[CPU_CYCLES_PENTIUM];	// TODO: generate own cycle tables
	cpustate->cycle_table_pm = cycle_table_pm[CPU_CYCLES_PENTIUM];	// TODO: generate own cycle tables

	cpustate->cpuid_id0 = 0x756e6547;	// Genu
	cpustate->cpuid_id1 = 0x49656e69;	// ineI
	cpustate->cpuid_id2 = 0x6c65746e;	// ntel

	cpustate->cpuid_max_input_value_eax = 0x02;
	cpustate->cpu_version = REG32(EDX);

	// [ 0:0] FPU on chip
	cpustate->feature_flags = 0x00000001;		// TODO: enable relevant flags here

	CHANGE_PC(cpustate,cpustate->eip);
}


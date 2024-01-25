// special nec instructions missing
// at the time the same like table186.h

static void (*const PREFIXV30(_instruction)[256])() =
{
		PREFIX86(_add_br8),           /* 0x00 */
		PREFIX86(_add_wr16),          /* 0x01 */
		PREFIX86(_add_r8b),           /* 0x02 */
		PREFIX86(_add_r16w),          /* 0x03 */
		PREFIX86(_add_ald8),          /* 0x04 */
		PREFIX86(_add_axd16),         /* 0x05 */
		PREFIX86(_push_es),           /* 0x06 */
		PREFIX86(_pop_es),            /* 0x07 */
		PREFIX86(_or_br8),            /* 0x08 */
		PREFIX86(_or_wr16),           /* 0x09 */
		PREFIX86(_or_r8b),            /* 0x0a */
		PREFIX86(_or_r16w),           /* 0x0b */
		PREFIX86(_or_ald8),           /* 0x0c */
		PREFIX86(_or_axd16),          /* 0x0d */
		PREFIX86(_push_cs),           /* 0x0e */
		PREFIXV30(_0fpre),            /* 0x0f */
		PREFIX86(_adc_br8),           /* 0x10 */
		PREFIX86(_adc_wr16),          /* 0x11 */
		PREFIX86(_adc_r8b),           /* 0x12 */
		PREFIX86(_adc_r16w),          /* 0x13 */
		PREFIX86(_adc_ald8),          /* 0x14 */
		PREFIX86(_adc_axd16),         /* 0x15 */
		PREFIX86(_push_ss),           /* 0x16 */
		PREFIXV30(_pop_ss),           /* 0x17 */
		PREFIX86(_sbb_br8),           /* 0x18 */
		PREFIX86(_sbb_wr16),          /* 0x19 */
		PREFIX86(_sbb_r8b),           /* 0x1a */
		PREFIX86(_sbb_r16w),          /* 0x1b */
		PREFIX86(_sbb_ald8),          /* 0x1c */
		PREFIX86(_sbb_axd16),         /* 0x1d */
		PREFIX86(_push_ds),           /* 0x1e */
		PREFIX86(_pop_ds),            /* 0x1f */
		PREFIX86(_and_br8),           /* 0x20 */
		PREFIX86(_and_wr16),          /* 0x21 */
		PREFIX86(_and_r8b),           /* 0x22 */
		PREFIX86(_and_r16w),          /* 0x23 */
		PREFIX86(_and_ald8),          /* 0x24 */
		PREFIX86(_and_axd16),         /* 0x25 */
		PREFIXV30(_es),               /* 0x26 */
		PREFIX86(_daa),               /* 0x27 */
		PREFIX86(_sub_br8),           /* 0x28 */
		PREFIX86(_sub_wr16),          /* 0x29 */
		PREFIX86(_sub_r8b),           /* 0x2a */
		PREFIX86(_sub_r16w),          /* 0x2b */
		PREFIX86(_sub_ald8),          /* 0x2c */
		PREFIX86(_sub_axd16),         /* 0x2d */
		PREFIXV30(_cs),               /* 0x2e */
		PREFIX86(_das),               /* 0x2f */
		PREFIX86(_xor_br8),           /* 0x30 */
		PREFIX86(_xor_wr16),          /* 0x31 */
		PREFIX86(_xor_r8b),           /* 0x32 */
		PREFIX86(_xor_r16w),          /* 0x33 */
		PREFIX86(_xor_ald8),          /* 0x34 */
		PREFIX86(_xor_axd16),         /* 0x35 */
		PREFIXV30(_ss),               /* 0x36 */
		PREFIX86(_aaa),               /* 0x37 */
		PREFIX86(_cmp_br8),           /* 0x38 */
		PREFIX86(_cmp_wr16),          /* 0x39 */
		PREFIX86(_cmp_r8b),           /* 0x3a */
		PREFIX86(_cmp_r16w),          /* 0x3b */
		PREFIX86(_cmp_ald8),          /* 0x3c */
		PREFIX86(_cmp_axd16),         /* 0x3d */
		PREFIXV30(_ds),               /* 0x3e */
		PREFIX86(_aas),               /* 0x3f */
		PREFIX86(_inc_ax),            /* 0x40 */
		PREFIX86(_inc_cx),            /* 0x41 */
		PREFIX86(_inc_dx),            /* 0x42 */
		PREFIX86(_inc_bx),            /* 0x43 */
		PREFIX86(_inc_sp),            /* 0x44 */
		PREFIX86(_inc_bp),            /* 0x45 */
		PREFIX86(_inc_si),            /* 0x46 */
		PREFIX86(_inc_di),            /* 0x47 */
		PREFIX86(_dec_ax),            /* 0x48 */
		PREFIX86(_dec_cx),            /* 0x49 */
		PREFIX86(_dec_dx),            /* 0x4a */
		PREFIX86(_dec_bx),            /* 0x4b */
		PREFIX86(_dec_sp),            /* 0x4c */
		PREFIX86(_dec_bp),            /* 0x4d */
		PREFIX86(_dec_si),            /* 0x4e */
		PREFIX86(_dec_di),            /* 0x4f */
		PREFIX86(_push_ax),           /* 0x50 */
		PREFIX86(_push_cx),           /* 0x51 */
		PREFIX86(_push_dx),           /* 0x52 */
		PREFIX86(_push_bx),           /* 0x53 */
		PREFIX86(_push_sp),           /* 0x54 */
		PREFIX86(_push_bp),           /* 0x55 */
		PREFIX86(_push_si),           /* 0x56 */
		PREFIX86(_push_di),           /* 0x57 */
		PREFIX86(_pop_ax),            /* 0x58 */
		PREFIX86(_pop_cx),            /* 0x59 */
		PREFIX86(_pop_dx),            /* 0x5a */
		PREFIX86(_pop_bx),            /* 0x5b */
		PREFIX86(_pop_sp),            /* 0x5c */
		PREFIX86(_pop_bp),            /* 0x5d */
		PREFIX86(_pop_si),            /* 0x5e */
		PREFIX86(_pop_di),            /* 0x5f */
		PREFIX186(_pusha),            /* 0x60 */
		PREFIX186(_popa),             /* 0x61 */
		PREFIX186(_bound),            /* 0x62 */
		PREFIX86(_invalid),           /* 0x63 */
		PREFIXV30(_repnc),            /* 0x64 */
		PREFIXV30(_repc),             /* 0x65 */
		PREFIX86(_invalid),           /* 0x66 */
		PREFIX86(_invalid),           /* 0x67 */
		PREFIX186(_push_d16),         /* 0x68 */
		PREFIX186(_imul_d16),         /* 0x69 */
		PREFIX186(_push_d8),          /* 0x6a */
		PREFIX186(_imul_d8),          /* 0x6b */
		PREFIX186(_insb),             /* 0x6c */
		PREFIX186(_insw),             /* 0x6d */
		PREFIX186(_outsb),            /* 0x6e */
		PREFIX186(_outsw),            /* 0x6f */
		PREFIX86(_jo),                /* 0x70 */
		PREFIX86(_jno),               /* 0x71 */
		PREFIX86(_jb),                /* 0x72 */
		PREFIX86(_jnb),               /* 0x73 */
		PREFIX86(_jz),                /* 0x74 */
		PREFIX86(_jnz),               /* 0x75 */
		PREFIX86(_jbe),               /* 0x76 */
		PREFIX86(_jnbe),              /* 0x77 */
		PREFIX86(_js),                /* 0x78 */
		PREFIX86(_jns),               /* 0x79 */
		PREFIX86(_jp),                /* 0x7a */
		PREFIX86(_jnp),               /* 0x7b */
		PREFIX86(_jl),                /* 0x7c */
		PREFIX86(_jnl),               /* 0x7d */
		PREFIX86(_jle),               /* 0x7e */
		PREFIX86(_jnle),              /* 0x7f */
		PREFIX86(_80pre),             /* 0x80 */
		PREFIX86(_81pre),             /* 0x81 */
		PREFIX86(_82pre),             /* 0x82 */
		PREFIX86(_83pre),             /* 0x83 */
		PREFIX86(_test_br8),          /* 0x84 */
		PREFIX86(_test_wr16),         /* 0x85 */
		PREFIX86(_xchg_br8),          /* 0x86 */
		PREFIX86(_xchg_wr16),         /* 0x87 */
		PREFIX86(_mov_br8),           /* 0x88 */
		PREFIX86(_mov_wr16),          /* 0x89 */
		PREFIX86(_mov_r8b),           /* 0x8a */
		PREFIX86(_mov_r16w),          /* 0x8b */
		PREFIX86(_mov_wsreg),         /* 0x8c */
		PREFIX86(_lea),               /* 0x8d */
		PREFIXV30(_mov_sregw),        /* 0x8e */
		PREFIX86(_popw),              /* 0x8f */
		PREFIX86(_nop),               /* 0x90 */
		PREFIX86(_xchg_axcx),         /* 0x91 */
		PREFIX86(_xchg_axdx),         /* 0x92 */
		PREFIX86(_xchg_axbx),         /* 0x93 */
		PREFIX86(_xchg_axsp),         /* 0x94 */
		PREFIX86(_xchg_axbp),         /* 0x95 */
		PREFIX86(_xchg_axsi),         /* 0x97 */
		PREFIX86(_xchg_axdi),         /* 0x97 */
		PREFIX86(_cbw),               /* 0x98 */
		PREFIX86(_cwd),               /* 0x99 */
		PREFIX86(_call_far),          /* 0x9a */
		PREFIX86(_wait),              /* 0x9b */
		PREFIX86(_pushf),             /* 0x9c */
		PREFIX86(_popf),              /* 0x9d */
		PREFIX86(_sahf),              /* 0x9e */
		PREFIX86(_lahf),              /* 0x9f */
		PREFIX86(_mov_aldisp),        /* 0xa0 */
		PREFIX86(_mov_axdisp),        /* 0xa1 */
		PREFIX86(_mov_dispal),        /* 0xa2 */
		PREFIX86(_mov_dispax),        /* 0xa3 */
		PREFIX86(_movsb),             /* 0xa4 */
		PREFIX86(_movsw),             /* 0xa5 */
		PREFIX86(_cmpsb),             /* 0xa6 */
		PREFIX86(_cmpsw),             /* 0xa7 */
		PREFIX86(_test_ald8),         /* 0xa8 */
		PREFIX86(_test_axd16),        /* 0xa9 */
		PREFIX86(_stosb),             /* 0xaa */
		PREFIX86(_stosw),             /* 0xab */
		PREFIX86(_lodsb),             /* 0xac */
		PREFIX86(_lodsw),             /* 0xad */
		PREFIX86(_scasb),             /* 0xae */
		PREFIX86(_scasw),             /* 0xaf */
		PREFIX86(_mov_ald8),          /* 0xb0 */
		PREFIX86(_mov_cld8),          /* 0xb1 */
		PREFIX86(_mov_dld8),          /* 0xb2 */
		PREFIX86(_mov_bld8),          /* 0xb3 */
		PREFIX86(_mov_ahd8),          /* 0xb4 */
		PREFIX86(_mov_chd8),          /* 0xb5 */
		PREFIX86(_mov_dhd8),          /* 0xb6 */
		PREFIX86(_mov_bhd8),          /* 0xb7 */
		PREFIX86(_mov_axd16),         /* 0xb8 */
		PREFIX86(_mov_cxd16),         /* 0xb9 */
		PREFIX86(_mov_dxd16),         /* 0xba */
		PREFIX86(_mov_bxd16),         /* 0xbb */
		PREFIX86(_mov_spd16),         /* 0xbc */
		PREFIX86(_mov_bpd16),         /* 0xbd */
		PREFIX86(_mov_sid16),         /* 0xbe */
		PREFIX86(_mov_did16),         /* 0xbf */
		PREFIX186(_rotshft_bd8),      /* 0xc0 */
		PREFIX186(_rotshft_wd8),      /* 0xc1 */
		PREFIX86(_ret_d16),           /* 0xc2 */
		PREFIX86(_ret),               /* 0xc3 */
		PREFIX86(_les_dw),            /* 0xc4 */
		PREFIX86(_lds_dw),            /* 0xc5 */
		PREFIX86(_mov_bd8),           /* 0xc6 */
		PREFIX86(_mov_wd16),          /* 0xc7 */
		PREFIX186(_enter),            /* 0xc8 */
		PREFIX186(_leave),            /* 0xc9 */
		PREFIX86(_retf_d16),          /* 0xca */
		PREFIX86(_retf),              /* 0xcb */
		PREFIX86(_int3),              /* 0xcc */
		PREFIX86(_int),               /* 0xcd */
		PREFIX86(_into),              /* 0xce */
		PREFIX86(_iret),              /* 0xcf */
		PREFIX86(_rotshft_b),         /* 0xd0 */
		PREFIX86(_rotshft_w),         /* 0xd1 */
		PREFIX86(_rotshft_bcl),       /* 0xd2 */
		PREFIX86(_rotshft_wcl),       /* 0xd3 */
		PREFIXV30(_aam),              /* 0xd4 */
		PREFIXV30(_aad),              /* 0xd5 */
		PREFIXV30(_setalc),           /* 0xd6 */
		PREFIX86(_xlat),              /* 0xd7 */
		PREFIX86(_escape),            /* 0xd8 */
		PREFIX86(_escape),            /* 0xd9 */
		PREFIX86(_escape),            /* 0xda */
		PREFIX86(_escape),            /* 0xdb */
		PREFIX86(_escape),            /* 0xdc */
		PREFIX86(_escape),            /* 0xdd */
		PREFIX86(_escape),            /* 0xde */
		PREFIX86(_escape),            /* 0xdf */
		PREFIX86(_loopne),            /* 0xe0 */
		PREFIX86(_loope),             /* 0xe1 */
		PREFIX86(_loop),              /* 0xe2 */
		PREFIX86(_jcxz),              /* 0xe3 */
		PREFIX86(_inal),              /* 0xe4 */
		PREFIX86(_inax),              /* 0xe5 */
		PREFIX86(_outal),             /* 0xe6 */
		PREFIX86(_outax),             /* 0xe7 */
		PREFIX86(_call_d16),          /* 0xe8 */
		PREFIX86(_jmp_d16),           /* 0xe9 */
		PREFIX86(_jmp_far),           /* 0xea */
		PREFIX86(_jmp_d8),            /* 0xeb */
		PREFIX86(_inaldx),            /* 0xec */
		PREFIX86(_inaxdx),            /* 0xed */
		PREFIX86(_outdxal),           /* 0xee */
		PREFIX86(_outdxax),           /* 0xef */
		PREFIX86(_lock),              /* 0xf0 */
		PREFIX86(_invalid),           /* 0xf1 */
		PREFIXV30(_repne),            /* 0xf2 */
		PREFIXV30(_repe),             /* 0xf3 */
		PREFIX86(_hlt),               /* 0xf4 */
		PREFIX86(_cmc),               /* 0xf5 */
		PREFIX86(_f6pre),             /* 0xf6 */
		PREFIX86(_f7pre),             /* 0xf7 */
		PREFIX86(_clc),               /* 0xf8 */
		PREFIX86(_stc),               /* 0xf9 */
		PREFIX86(_cli),               /* 0xfa */
		PREFIXV30(_sti),              /* 0xfb */
		PREFIX86(_cld),               /* 0xfc */
		PREFIX86(_std),               /* 0xfd */
		PREFIX86(_fepre),             /* 0xfe */
		PREFIX86(_ffpre)              /* 0xff */
};

#if defined(BIGCASE) && !defined(RS6000)
	/* Some compilers cannot handle large case statements */
#define TABLEV30 \
	switch(FETCHOP)\
	{\
	case 0x00:    PREFIX86(_add_br8)(); break;\
	case 0x01:    PREFIX86(_add_wr16)(); break;\
	case 0x02:    PREFIX86(_add_r8b)(); break;\
	case 0x03:    PREFIX86(_add_r16w)(); break;\
	case 0x04:    PREFIX86(_add_ald8)(); break;\
	case 0x05:    PREFIX86(_add_axd16)(); break;\
	case 0x06:    PREFIX86(_push_es)(); break;\
	case 0x07:    PREFIX86(_pop_es)(); break;\
	case 0x08:    PREFIX86(_or_br8)(); break;\
	case 0x09:    PREFIX86(_or_wr16)(); break;\
	case 0x0a:    PREFIX86(_or_r8b)(); break;\
	case 0x0b:    PREFIX86(_or_r16w)(); break;\
	case 0x0c:    PREFIX86(_or_ald8)(); break;\
	case 0x0d:    PREFIX86(_or_axd16)(); break;\
	case 0x0e:    PREFIX86(_push_cs)(); break;\
	case 0x0f:    PREFIX86(_invalid)(); break;\
	case 0x0f:    PREFIXV30(_0fpre)(); break;\
	case 0x10:    PREFIX86(_adc_br8)(); break;\
	case 0x11:    PREFIX86(_adc_wr16)(); break;\
	case 0x12:    PREFIX86(_adc_r8b)(); break;\
	case 0x13:    PREFIX86(_adc_r16w)(); break;\
	case 0x14:    PREFIX86(_adc_ald8)(); break;\
	case 0x15:    PREFIX86(_adc_axd16)(); break;\
	case 0x16:    PREFIX86(_push_ss)(); break;\
	case 0x17:    PREFIXV30(_pop_ss)(); break;\
	case 0x18:    PREFIX86(_sbb_br8)(); break;\
	case 0x19:    PREFIX86(_sbb_wr16)(); break;\
	case 0x1a:    PREFIX86(_sbb_r8b)(); break;\
	case 0x1b:    PREFIX86(_sbb_r16w)(); break;\
	case 0x1c:    PREFIX86(_sbb_ald8)(); break;\
	case 0x1d:    PREFIX86(_sbb_axd16)(); break;\
	case 0x1e:    PREFIX86(_push_ds)(); break;\
	case 0x1f:    PREFIX86(_pop_ds)(); break;\
	case 0x20:    PREFIX86(_and_br8)(); break;\
	case 0x21:    PREFIX86(_and_wr16)(); break;\
	case 0x22:    PREFIX86(_and_r8b)(); break;\
	case 0x23:    PREFIX86(_and_r16w)(); break;\
	case 0x24:    PREFIX86(_and_ald8)(); break;\
	case 0x25:    PREFIX86(_and_axd16)(); break;\
	case 0x26:    PREFIXV30(_es)(); break;\
	case 0x27:    PREFIX86(_daa)(); break;\
	case 0x28:    PREFIX86(_sub_br8)(); break;\
	case 0x29:    PREFIX86(_sub_wr16)(); break;\
	case 0x2a:    PREFIX86(_sub_r8b)(); break;\
	case 0x2b:    PREFIX86(_sub_r16w)(); break;\
	case 0x2c:    PREFIX86(_sub_ald8)(); break;\
	case 0x2d:    PREFIX86(_sub_axd16)(); break;\
	case 0x2e:    PREFIXV30(_cs)(); break;\
	case 0x2f:    PREFIX86(_das)(); break;\
	case 0x30:    PREFIX86(_xor_br8)(); break;\
	case 0x31:    PREFIX86(_xor_wr16)(); break;\
	case 0x32:    PREFIX86(_xor_r8b)(); break;\
	case 0x33:    PREFIX86(_xor_r16w)(); break;\
	case 0x34:    PREFIX86(_xor_ald8)(); break;\
	case 0x35:    PREFIX86(_xor_axd16)(); break;\
	case 0x36:    PREFIXV30(_ss)(); break;\
	case 0x37:    PREFIX86(_aaa)(); break;\
	case 0x38:    PREFIX86(_cmp_br8)(); break;\
	case 0x39:    PREFIX86(_cmp_wr16)(); break;\
	case 0x3a:    PREFIX86(_cmp_r8b)(); break;\
	case 0x3b:    PREFIX86(_cmp_r16w)(); break;\
	case 0x3c:    PREFIX86(_cmp_ald8)(); break;\
	case 0x3d:    PREFIX86(_cmp_axd16)(); break;\
	case 0x3e:    PREFIXV30(_ds)(); break;\
	case 0x3f:    PREFIX86(_aas)(); break;\
	case 0x40:    PREFIX86(_inc_ax)(); break;\
	case 0x41:    PREFIX86(_inc_cx)(); break;\
	case 0x42:    PREFIX86(_inc_dx)(); break;\
	case 0x43:    PREFIX86(_inc_bx)(); break;\
	case 0x44:    PREFIX86(_inc_sp)(); break;\
	case 0x45:    PREFIX86(_inc_bp)(); break;\
	case 0x46:    PREFIX86(_inc_si)(); break;\
	case 0x47:    PREFIX86(_inc_di)(); break;\
	case 0x48:    PREFIX86(_dec_ax)(); break;\
	case 0x49:    PREFIX86(_dec_cx)(); break;\
	case 0x4a:    PREFIX86(_dec_dx)(); break;\
	case 0x4b:    PREFIX86(_dec_bx)(); break;\
	case 0x4c:    PREFIX86(_dec_sp)(); break;\
	case 0x4d:    PREFIX86(_dec_bp)(); break;\
	case 0x4e:    PREFIX86(_dec_si)(); break;\
	case 0x4f:    PREFIX86(_dec_di)(); break;\
	case 0x50:    PREFIX86(_push_ax)(); break;\
	case 0x51:    PREFIX86(_push_cx)(); break;\
	case 0x52:    PREFIX86(_push_dx)(); break;\
	case 0x53:    PREFIX86(_push_bx)(); break;\
	case 0x54:    PREFIX86(_push_sp)(); break;\
	case 0x55:    PREFIX86(_push_bp)(); break;\
	case 0x56:    PREFIX86(_push_si)(); break;\
	case 0x57:    PREFIX86(_push_di)(); break;\
	case 0x58:    PREFIX86(_pop_ax)(); break;\
	case 0x59:    PREFIX86(_pop_cx)(); break;\
	case 0x5a:    PREFIX86(_pop_dx)(); break;\
	case 0x5b:    PREFIX86(_pop_bx)(); break;\
	case 0x5c:    PREFIX86(_pop_sp)(); break;\
	case 0x5d:    PREFIX86(_pop_bp)(); break;\
	case 0x5e:    PREFIX86(_pop_si)(); break;\
	case 0x5f:    PREFIX86(_pop_di)(); break;\
	case 0x60:    PREFIX186(_pusha)(); break;\
	case 0x61:    PREFIX186(_popa)(); break;\
	case 0x62:    PREFIX186(_bound)(); break;\
	case 0x63:    PREFIX86(_invalid)(); break;\
	case 0x64:    PREFIXV30(_repnc)(); break;\
	case 0x65:    PREFIXV30(_repc)(); break;\
	case 0x66:    PREFIX86(_invalid)(); break;\
	case 0x67:    PREFIX86(_invalid)(); break;\
	case 0x68:    PREFIX186(_push_d16)(); break;\
	case 0x69:    PREFIX186(_imul_d16)(); break;\
	case 0x6a:    PREFIX186(_push_d8)(); break;\
	case 0x6b:    PREFIX186(_imul_d8)(); break;\
	case 0x6c:    PREFIX186(_insb)(); break;\
	case 0x6d:    PREFIX186(_insw)(); break;\
	case 0x6e:    PREFIX186(_outsb)(); break;\
	case 0x6f:    PREFIX186(_outsw)(); break;\
	case 0x70:    PREFIX86(_jo)(); break;\
	case 0x71:    PREFIX86(_jno)(); break;\
	case 0x72:    PREFIX86(_jb)(); break;\
	case 0x73:    PREFIX86(_jnb)(); break;\
	case 0x74:    PREFIX86(_jz)(); break;\
	case 0x75:    PREFIX86(_jnz)(); break;\
	case 0x76:    PREFIX86(_jbe)(); break;\
	case 0x77:    PREFIX86(_jnbe)(); break;\
	case 0x78:    PREFIX86(_js)(); break;\
	case 0x79:    PREFIX86(_jns)(); break;\
	case 0x7a:    PREFIX86(_jp)(); break;\
	case 0x7b:    PREFIX86(_jnp)(); break;\
	case 0x7c:    PREFIX86(_jl)(); break;\
	case 0x7d:    PREFIX86(_jnl)(); break;\
	case 0x7e:    PREFIX86(_jle)(); break;\
	case 0x7f:    PREFIX86(_jnle)(); break;\
	case 0x80:    PREFIX86(_80pre)(); break;\
	case 0x81:    PREFIX86(_81pre)(); break;\
	case 0x82:    PREFIX86(_82pre)(); break;\
	case 0x83:    PREFIX86(_83pre)(); break;\
	case 0x84:    PREFIX86(_test_br8)(); break;\
	case 0x85:    PREFIX86(_test_wr16)(); break;\
	case 0x86:    PREFIX86(_xchg_br8)(); break;\
	case 0x87:    PREFIX86(_xchg_wr16)(); break;\
	case 0x88:    PREFIX86(_mov_br8)(); break;\
	case 0x89:    PREFIX86(_mov_wr16)(); break;\
	case 0x8a:    PREFIX86(_mov_r8b)(); break;\
	case 0x8b:    PREFIX86(_mov_r16w)(); break;\
	case 0x8c:    PREFIX86(_mov_wsreg)(); break;\
	case 0x8d:    PREFIX86(_lea)(); break;\
	case 0x8e:    PREFIXV30(_mov_sregw)(); break;\
	case 0x8f:    PREFIX86(_popw)(); break;\
	case 0x90:    PREFIX86(_nop)(); break;\
	case 0x91:    PREFIX86(_xchg_axcx)(); break;\
	case 0x92:    PREFIX86(_xchg_axdx)(); break;\
	case 0x93:    PREFIX86(_xchg_axbx)(); break;\
	case 0x94:    PREFIX86(_xchg_axsp)(); break;\
	case 0x95:    PREFIX86(_xchg_axbp)(); break;\
	case 0x96:    PREFIX86(_xchg_axsi)(); break;\
	case 0x97:    PREFIX86(_xchg_axdi)(); break;\
	case 0x98:    PREFIX86(_cbw)(); break;\
	case 0x99:    PREFIX86(_cwd)(); break;\
	case 0x9a:    PREFIX86(_call_far)(); break;\
	case 0x9b:    PREFIX86(_wait)(); break;\
	case 0x9c:    PREFIX86(_pushf)(); break;\
	case 0x9d:    PREFIX86(_popf)(); break;\
	case 0x9e:    PREFIX86(_sahf)(); break;\
	case 0x9f:    PREFIX86(_lahf)(); break;\
	case 0xa0:    PREFIX86(_mov_aldisp)(); break;\
	case 0xa1:    PREFIX86(_mov_axdisp)(); break;\
	case 0xa2:    PREFIX86(_mov_dispal)(); break;\
	case 0xa3:    PREFIX86(_mov_dispax)(); break;\
	case 0xa4:    PREFIX86(_movsb)(); break;\
	case 0xa5:    PREFIX86(_movsw)(); break;\
	case 0xa6:    PREFIX86(_cmpsb)(); break;\
	case 0xa7:    PREFIX86(_cmpsw)(); break;\
	case 0xa8:    PREFIX86(_test_ald8)(); break;\
	case 0xa9:    PREFIX86(_test_axd16)(); break;\
	case 0xaa:    PREFIX86(_stosb)(); break;\
	case 0xab:    PREFIX86(_stosw)(); break;\
	case 0xac:    PREFIX86(_lodsb)(); break;\
	case 0xad:    PREFIX86(_lodsw)(); break;\
	case 0xae:    PREFIX86(_scasb)(); break;\
	case 0xaf:    PREFIX86(_scasw)(); break;\
	case 0xb0:    PREFIX86(_mov_ald8)(); break;\
	case 0xb1:    PREFIX86(_mov_cld8)(); break;\
	case 0xb2:    PREFIX86(_mov_dld8)(); break;\
	case 0xb3:    PREFIX86(_mov_bld8)(); break;\
	case 0xb4:    PREFIX86(_mov_ahd8)(); break;\
	case 0xb5:    PREFIX86(_mov_chd8)(); break;\
	case 0xb6:    PREFIX86(_mov_dhd8)(); break;\
	case 0xb7:    PREFIX86(_mov_bhd8)(); break;\
	case 0xb8:    PREFIX86(_mov_axd16)(); break;\
	case 0xb9:    PREFIX86(_mov_cxd16)(); break;\
	case 0xba:    PREFIX86(_mov_dxd16)(); break;\
	case 0xbb:    PREFIX86(_mov_bxd16)(); break;\
	case 0xbc:    PREFIX86(_mov_spd16)(); break;\
	case 0xbd:    PREFIX86(_mov_bpd16)(); break;\
	case 0xbe:    PREFIX86(_mov_sid16)(); break;\
	case 0xbf:    PREFIX86(_mov_did16)(); break;\
	case 0xc0:    PREFIX186(_rotshft_bd8)(); break;\
	case 0xc1:    PREFIX186(_rotshft_wd8)(); break;\
	case 0xc2:    PREFIX86(_ret_d16)(); break;\
	case 0xc3:    PREFIX86(_ret)(); break;\
	case 0xc4:    PREFIX86(_les_dw)(); break;\
	case 0xc5:    PREFIX86(_lds_dw)(); break;\
	case 0xc6:    PREFIX86(_mov_bd8)(); break;\
	case 0xc7:    PREFIX86(_mov_wd16)(); break;\
	case 0xc8:    PREFIX186(_enter)(); break;\
	case 0xc9:    PREFIX186(_leave)(); break;\
	case 0xca:    PREFIX86(_retf_d16)(); break;\
	case 0xcb:    PREFIX86(_retf)(); break;\
	case 0xcc:    PREFIX86(_int3)(); break;\
	case 0xcd:    PREFIX86(_int)(); break;\
	case 0xce:    PREFIX86(_into)(); break;\
	case 0xcf:    PREFIX86(_iret)(); break;\
	case 0xd0:    PREFIX86(_rotshft_b)(); break;\
	case 0xd1:    PREFIX86(_rotshft_w)(); break;\
	case 0xd2:    PREFIX86(_rotshft_bcl)(); break;\
	case 0xd3:    PREFIX86(_rotshft_wcl)(); break;\
	case 0xd4:    PREFIXV30(_aam)(); break;\
	case 0xd5:    PREFIXV30(_aad)(); break;\
	case 0xd6:    PREFIXV30(_setalc)(); break;\
	case 0xd7:    PREFIX86(_xlat)(); break;\
	case 0xd8:    PREFIX86(_escape)(); break;\
	case 0xd9:    PREFIX86(_escape)(); break;\
	case 0xda:    PREFIX86(_escape)(); break;\
	case 0xdb:    PREFIX86(_escape)(); break;\
	case 0xdc:    PREFIX86(_escape)(); break;\
	case 0xdd:    PREFIX86(_escape)(); break;\
	case 0xde:    PREFIX86(_escape)(); break;\
	case 0xdf:    PREFIX86(_escape)(); break;\
	case 0xe0:    PREFIX86(_loopne)(); break;\
	case 0xe1:    PREFIX86(_loope)(); break;\
	case 0xe2:    PREFIX86(_loop)(); break;\
	case 0xe3:    PREFIX86(_jcxz)(); break;\
	case 0xe4:    PREFIX86(_inal)(); break;\
	case 0xe5:    PREFIX86(_inax)(); break;\
	case 0xe6:    PREFIX86(_outal)(); break;\
	case 0xe7:    PREFIX86(_outax)(); break;\
	case 0xe8:    PREFIX86(_call_d16)(); break;\
	case 0xe9:    PREFIX86(_jmp_d16)(); break;\
	case 0xea:    PREFIX86(_jmp_far)(); break;\
	case 0xeb:    PREFIX86(_jmp_d8)(); break;\
	case 0xec:    PREFIX86(_inaldx)(); break;\
	case 0xed:    PREFIX86(_inaxdx)(); break;\
	case 0xee:    PREFIX86(_outdxal)(); break;\
	case 0xef:    PREFIX86(_outdxax)(); break;\
	case 0xf0:    PREFIX86(_lock)(); break;\
	case 0xf1:    PREFIX86(_invalid)(); break;\
	case 0xf2:    PREFIXV30(_repne)(); break;\
	case 0xf3:    PREFIXV30(_repe)(); break;\
	case 0xf4:    PREFIX86(_hlt)(); break;\
	case 0xf5:    PREFIX86(_cmc)(); break;\
	case 0xf6:    PREFIX86(_f6pre)(); break;\
	case 0xf7:    PREFIX86(_f7pre)(); break;\
	case 0xf8:    PREFIX86(_clc)(); break;\
	case 0xf9:    PREFIX86(_stc)(); break;\
	case 0xfa:    PREFIX86(_cli)(); break;\
	case 0xfb:    PREFIXV30(_sti)(); break;\
	case 0xfc:    PREFIX86(_cld)(); break;\
	case 0xfd:    PREFIX86(_std)(); break;\
	case 0xfe:    PREFIX86(_fepre)(); break;\
	case 0xff:    PREFIX86(_ffpre)(); break;\
	};
#else
#define TABLEV30 PREFIXV30(_instruction)[FETCHOP]();
#endif

/* i8080 instructions */

static void (*const PREFIX80(_instruction)[256])() =
{
		PREFIX80(_00h),
		PREFIX80(_01h),
		PREFIX80(_02h),
		PREFIX80(_03h),
		PREFIX80(_04h),
		PREFIX80(_05h),
		PREFIX80(_06h),
		PREFIX80(_07h),
		PREFIX80(_08h),
		PREFIX80(_09h),
		PREFIX80(_0ah),
		PREFIX80(_0bh),
		PREFIX80(_0ch),
		PREFIX80(_0dh),
		PREFIX80(_0eh),
		PREFIX80(_0fh),
		PREFIX80(_10h),
		PREFIX80(_11h),
		PREFIX80(_12h),
		PREFIX80(_13h),
		PREFIX80(_14h),
		PREFIX80(_15h),
		PREFIX80(_16h),
		PREFIX80(_17h),
		PREFIX80(_18h),
		PREFIX80(_19h),
		PREFIX80(_1ah),
		PREFIX80(_1bh),
		PREFIX80(_1ch),
		PREFIX80(_1dh),
		PREFIX80(_1eh),
		PREFIX80(_1fh),
		PREFIX80(_20h),
		PREFIX80(_21h),
		PREFIX80(_22h),
		PREFIX80(_23h),
		PREFIX80(_24h),
		PREFIX80(_25h),
		PREFIX80(_26h),
		PREFIX80(_27h),
		PREFIX80(_28h),
		PREFIX80(_29h),
		PREFIX80(_2ah),
		PREFIX80(_2bh),
		PREFIX80(_2ch),
		PREFIX80(_2dh),
		PREFIX80(_2eh),
		PREFIX80(_2fh),
		PREFIX80(_30h),
		PREFIX80(_31h),
		PREFIX80(_32h),
		PREFIX80(_33h),
		PREFIX80(_34h),
		PREFIX80(_35h),
		PREFIX80(_36h),
		PREFIX80(_37h),
		PREFIX80(_38h),
		PREFIX80(_39h),
		PREFIX80(_3ah),
		PREFIX80(_3bh),
		PREFIX80(_3ch),
		PREFIX80(_3dh),
		PREFIX80(_3eh),
		PREFIX80(_3fh),
		PREFIX80(_40h),
		PREFIX80(_41h),
		PREFIX80(_42h),
		PREFIX80(_43h),
		PREFIX80(_44h),
		PREFIX80(_45h),
		PREFIX80(_46h),
		PREFIX80(_47h),
		PREFIX80(_48h),
		PREFIX80(_49h),
		PREFIX80(_4ah),
		PREFIX80(_4bh),
		PREFIX80(_4ch),
		PREFIX80(_4dh),
		PREFIX80(_4eh),
		PREFIX80(_4fh),
		PREFIX80(_50h),
		PREFIX80(_51h),
		PREFIX80(_52h),
		PREFIX80(_53h),
		PREFIX80(_54h),
		PREFIX80(_55h),
		PREFIX80(_56h),
		PREFIX80(_57h),
		PREFIX80(_58h),
		PREFIX80(_59h),
		PREFIX80(_5ah),
		PREFIX80(_5bh),
		PREFIX80(_5ch),
		PREFIX80(_5dh),
		PREFIX80(_5eh),
		PREFIX80(_5fh),
		PREFIX80(_60h),
		PREFIX80(_61h),
		PREFIX80(_62h),
		PREFIX80(_63h),
		PREFIX80(_64h),
		PREFIX80(_65h),
		PREFIX80(_66h),
		PREFIX80(_67h),
		PREFIX80(_68h),
		PREFIX80(_69h),
		PREFIX80(_6ah),
		PREFIX80(_6bh),
		PREFIX80(_6ch),
		PREFIX80(_6dh),
		PREFIX80(_6eh),
		PREFIX80(_6fh),
		PREFIX80(_70h),
		PREFIX80(_71h),
		PREFIX80(_72h),
		PREFIX80(_73h),
		PREFIX80(_74h),
		PREFIX80(_75h),
		PREFIX80(_76h),
		PREFIX80(_77h),
		PREFIX80(_78h),
		PREFIX80(_79h),
		PREFIX80(_7ah),
		PREFIX80(_7bh),
		PREFIX80(_7ch),
		PREFIX80(_7dh),
		PREFIX80(_7eh),
		PREFIX80(_7fh),
		PREFIX80(_80h),
		PREFIX80(_81h),
		PREFIX80(_82h),
		PREFIX80(_83h),
		PREFIX80(_84h),
		PREFIX80(_85h),
		PREFIX80(_86h),
		PREFIX80(_87h),
		PREFIX80(_88h),
		PREFIX80(_89h),
		PREFIX80(_8ah),
		PREFIX80(_8bh),
		PREFIX80(_8ch),
		PREFIX80(_8dh),
		PREFIX80(_8eh),
		PREFIX80(_8fh),
		PREFIX80(_90h),
		PREFIX80(_91h),
		PREFIX80(_92h),
		PREFIX80(_93h),
		PREFIX80(_94h),
		PREFIX80(_95h),
		PREFIX80(_96h),
		PREFIX80(_97h),
		PREFIX80(_98h),
		PREFIX80(_99h),
		PREFIX80(_9ah),
		PREFIX80(_9bh),
		PREFIX80(_9ch),
		PREFIX80(_9dh),
		PREFIX80(_9eh),
		PREFIX80(_9fh),
		PREFIX80(_a0h),
		PREFIX80(_a1h),
		PREFIX80(_a2h),
		PREFIX80(_a3h),
		PREFIX80(_a4h),
		PREFIX80(_a5h),
		PREFIX80(_a6h),
		PREFIX80(_a7h),
		PREFIX80(_a8h),
		PREFIX80(_a9h),
		PREFIX80(_aah),
		PREFIX80(_abh),
		PREFIX80(_ach),
		PREFIX80(_adh),
		PREFIX80(_aeh),
		PREFIX80(_afh),
		PREFIX80(_b0h),
		PREFIX80(_b1h),
		PREFIX80(_b2h),
		PREFIX80(_b3h),
		PREFIX80(_b4h),
		PREFIX80(_b5h),
		PREFIX80(_b6h),
		PREFIX80(_b7h),
		PREFIX80(_b8h),
		PREFIX80(_b9h),
		PREFIX80(_bah),
		PREFIX80(_bbh),
		PREFIX80(_bch),
		PREFIX80(_bdh),
		PREFIX80(_beh),
		PREFIX80(_bfh),
		PREFIX80(_c0h),
		PREFIX80(_c1h),
		PREFIX80(_c2h),
		PREFIX80(_c3h),
		PREFIX80(_c4h),
		PREFIX80(_c5h),
		PREFIX80(_c6h),
		PREFIX80(_c7h),
		PREFIX80(_c8h),
		PREFIX80(_c9h),
		PREFIX80(_cah),
		PREFIX80(_cbh),
		PREFIX80(_cch),
		PREFIX80(_cdh),
		PREFIX80(_ceh),
		PREFIX80(_cfh),
		PREFIX80(_d0h),
		PREFIX80(_d1h),
		PREFIX80(_d2h),
		PREFIX80(_d3h),
		PREFIX80(_d4h),
		PREFIX80(_d5h),
		PREFIX80(_d6h),
		PREFIX80(_d7h),
		PREFIX80(_d8h),
		PREFIX80(_d9h),
		PREFIX80(_dah),
		PREFIX80(_dbh),
		PREFIX80(_dch),
		PREFIX80(_ddh),
		PREFIX80(_deh),
		PREFIX80(_dfh),
		PREFIX80(_e0h),
		PREFIX80(_e1h),
		PREFIX80(_e2h),
		PREFIX80(_e3h),
		PREFIX80(_e4h),
		PREFIX80(_e5h),
		PREFIX80(_e6h),
		PREFIX80(_e7h),
		PREFIX80(_e8h),
		PREFIX80(_e9h),
		PREFIX80(_eah),
		PREFIX80(_ebh),
		PREFIX80(_ech),
		PREFIX80(_edh),
		PREFIX80(_eeh),
		PREFIX80(_efh),
		PREFIX80(_f0h),
		PREFIX80(_f1h),
		PREFIX80(_f2h),
		PREFIX80(_f3h),
		PREFIX80(_f4h),
		PREFIX80(_f5h),
		PREFIX80(_f6h),
		PREFIX80(_f7h),
		PREFIX80(_f8h),
		PREFIX80(_f9h),
		PREFIX80(_fah),
		PREFIX80(_fbh),
		PREFIX80(_fch),
		PREFIX80(_fdh),
		PREFIX80(_feh),
		PREFIX80(_ffh),
};

#if defined(BIGCASE) && !defined(RS6000)
	/* Some compilers cannot handle large case statements */
#define TABLE80 \
	switch(I8080_FETCH8())\
	{\
	case 0x00:    PREFIX80(_00h); break; \
	case 0x01:    PREFIX80(_01h); break; \
	case 0x02:    PREFIX80(_02h); break; \
	case 0x03:    PREFIX80(_03h); break; \
	case 0x04:    PREFIX80(_04h); break; \
	case 0x05:    PREFIX80(_05h); break; \
	case 0x06:    PREFIX80(_06h); break; \
	case 0x07:    PREFIX80(_07h); break; \
	case 0x08:    PREFIX80(_08h); break; \
	case 0x09:    PREFIX80(_09h); break; \
	case 0x0a:    PREFIX80(_0ah); break; \
	case 0x0b:    PREFIX80(_0bh); break; \
	case 0x0c:    PREFIX80(_0ch); break; \
	case 0x0d:    PREFIX80(_0dh); break; \
	case 0x0e:    PREFIX80(_0eh); break; \
	case 0x0f:    PREFIX80(_0fh); break; \
	case 0x10:    PREFIX80(_10h); break; \
	case 0x11:    PREFIX80(_11h); break; \
	case 0x12:    PREFIX80(_12h); break; \
	case 0x13:    PREFIX80(_13h); break; \
	case 0x14:    PREFIX80(_14h); break; \
	case 0x15:    PREFIX80(_15h); break; \
	case 0x16:    PREFIX80(_16h); break; \
	case 0x17:    PREFIX80(_17h); break; \
	case 0x18:    PREFIX80(_18h); break; \
	case 0x19:    PREFIX80(_19h); break; \
	case 0x1a:    PREFIX80(_1ah); break; \
	case 0x1b:    PREFIX80(_1bh); break; \
	case 0x1c:    PREFIX80(_1ch); break; \
	case 0x1d:    PREFIX80(_1dh); break; \
	case 0x1e:    PREFIX80(_1eh); break; \
	case 0x1f:    PREFIX80(_1fh); break; \
	case 0x20:    PREFIX80(_20h); break; \
	case 0x21:    PREFIX80(_21h); break; \
	case 0x22:    PREFIX80(_22h); break; \
	case 0x23:    PREFIX80(_23h); break; \
	case 0x24:    PREFIX80(_24h); break; \
	case 0x25:    PREFIX80(_25h); break; \
	case 0x26:    PREFIX80(_26h); break; \
	case 0x27:    PREFIX80(_27h); break; \
	case 0x28:    PREFIX80(_28h); break; \
	case 0x29:    PREFIX80(_29h); break; \
	case 0x2a:    PREFIX80(_2ah); break; \
	case 0x2b:    PREFIX80(_2bh); break; \
	case 0x2c:    PREFIX80(_2ch); break; \
	case 0x2d:    PREFIX80(_2dh); break; \
	case 0x2e:    PREFIX80(_2eh); break; \
	case 0x2f:    PREFIX80(_2fh); break; \
	case 0x30:    PREFIX80(_30h); break; \
	case 0x31:    PREFIX80(_31h); break; \
	case 0x32:    PREFIX80(_32h); break; \
	case 0x33:    PREFIX80(_33h); break; \
	case 0x34:    PREFIX80(_34h); break; \
	case 0x35:    PREFIX80(_35h); break; \
	case 0x36:    PREFIX80(_36h); break; \
	case 0x37:    PREFIX80(_37h); break; \
	case 0x38:    PREFIX80(_38h); break; \
	case 0x39:    PREFIX80(_39h); break; \
	case 0x3a:    PREFIX80(_3ah); break; \
	case 0x3b:    PREFIX80(_3bh); break; \
	case 0x3c:    PREFIX80(_3ch); break; \
	case 0x3d:    PREFIX80(_3dh); break; \
	case 0x3e:    PREFIX80(_3eh); break; \
	case 0x3f:    PREFIX80(_3fh); break; \
	case 0x40:    PREFIX80(_40h); break; \
	case 0x41:    PREFIX80(_41h); break; \
	case 0x42:    PREFIX80(_42h); break; \
	case 0x43:    PREFIX80(_43h); break; \
	case 0x44:    PREFIX80(_44h); break; \
	case 0x45:    PREFIX80(_45h); break; \
	case 0x46:    PREFIX80(_46h); break; \
	case 0x47:    PREFIX80(_47h); break; \
	case 0x48:    PREFIX80(_48h); break; \
	case 0x49:    PREFIX80(_49h); break; \
	case 0x4a:    PREFIX80(_4ah); break; \
	case 0x4b:    PREFIX80(_4bh); break; \
	case 0x4c:    PREFIX80(_4ch); break; \
	case 0x4d:    PREFIX80(_4dh); break; \
	case 0x4e:    PREFIX80(_4eh); break; \
	case 0x4f:    PREFIX80(_4fh); break; \
	case 0x50:    PREFIX80(_50h); break; \
	case 0x51:    PREFIX80(_51h); break; \
	case 0x52:    PREFIX80(_52h); break; \
	case 0x53:    PREFIX80(_53h); break; \
	case 0x54:    PREFIX80(_54h); break; \
	case 0x55:    PREFIX80(_55h); break; \
	case 0x56:    PREFIX80(_56h); break; \
	case 0x57:    PREFIX80(_57h); break; \
	case 0x58:    PREFIX80(_58h); break; \
	case 0x59:    PREFIX80(_59h); break; \
	case 0x5a:    PREFIX80(_5ah); break; \
	case 0x5b:    PREFIX80(_5bh); break; \
	case 0x5c:    PREFIX80(_5ch); break; \
	case 0x5d:    PREFIX80(_5dh); break; \
	case 0x5e:    PREFIX80(_5eh); break; \
	case 0x5f:    PREFIX80(_5fh); break; \
	case 0x60:    PREFIX80(_60h); break; \
	case 0x61:    PREFIX80(_61h); break; \
	case 0x62:    PREFIX80(_62h); break; \
	case 0x63:    PREFIX80(_63h); break; \
	case 0x64:    PREFIX80(_64h); break; \
	case 0x65:    PREFIX80(_65h); break; \
	case 0x66:    PREFIX80(_66h); break; \
	case 0x67:    PREFIX80(_67h); break; \
	case 0x68:    PREFIX80(_68h); break; \
	case 0x69:    PREFIX80(_69h); break; \
	case 0x6a:    PREFIX80(_6ah); break; \
	case 0x6b:    PREFIX80(_6bh); break; \
	case 0x6c:    PREFIX80(_6ch); break; \
	case 0x6d:    PREFIX80(_6dh); break; \
	case 0x6e:    PREFIX80(_6eh); break; \
	case 0x6f:    PREFIX80(_6fh); break; \
	case 0x70:    PREFIX80(_70h); break; \
	case 0x71:    PREFIX80(_71h); break; \
	case 0x72:    PREFIX80(_72h); break; \
	case 0x73:    PREFIX80(_73h); break; \
	case 0x74:    PREFIX80(_74h); break; \
	case 0x75:    PREFIX80(_75h); break; \
	case 0x76:    PREFIX80(_76h); break; \
	case 0x77:    PREFIX80(_77h); break; \
	case 0x78:    PREFIX80(_78h); break; \
	case 0x79:    PREFIX80(_79h); break; \
	case 0x7a:    PREFIX80(_7ah); break; \
	case 0x7b:    PREFIX80(_7bh); break; \
	case 0x7c:    PREFIX80(_7ch); break; \
	case 0x7d:    PREFIX80(_7dh); break; \
	case 0x7e:    PREFIX80(_7eh); break; \
	case 0x7f:    PREFIX80(_7fh); break; \
	case 0x80:    PREFIX80(_80h); break; \
	case 0x81:    PREFIX80(_81h); break; \
	case 0x82:    PREFIX80(_82h); break; \
	case 0x83:    PREFIX80(_83h); break; \
	case 0x84:    PREFIX80(_84h); break; \
	case 0x85:    PREFIX80(_85h); break; \
	case 0x86:    PREFIX80(_86h); break; \
	case 0x87:    PREFIX80(_87h); break; \
	case 0x88:    PREFIX80(_88h); break; \
	case 0x89:    PREFIX80(_89h); break; \
	case 0x8a:    PREFIX80(_8ah); break; \
	case 0x8b:    PREFIX80(_8bh); break; \
	case 0x8c:    PREFIX80(_8ch); break; \
	case 0x8d:    PREFIX80(_8dh); break; \
	case 0x8e:    PREFIX80(_8eh); break; \
	case 0x8f:    PREFIX80(_8fh); break; \
	case 0x90:    PREFIX80(_90h); break; \
	case 0x91:    PREFIX80(_91h); break; \
	case 0x92:    PREFIX80(_92h); break; \
	case 0x93:    PREFIX80(_93h); break; \
	case 0x94:    PREFIX80(_94h); break; \
	case 0x95:    PREFIX80(_95h); break; \
	case 0x96:    PREFIX80(_96h); break; \
	case 0x97:    PREFIX80(_97h); break; \
	case 0x98:    PREFIX80(_98h); break; \
	case 0x99:    PREFIX80(_99h); break; \
	case 0x9a:    PREFIX80(_9ah); break; \
	case 0x9b:    PREFIX80(_9bh); break; \
	case 0x9c:    PREFIX80(_9ch); break; \
	case 0x9d:    PREFIX80(_9dh); break; \
	case 0x9e:    PREFIX80(_9eh); break; \
	case 0x9f:    PREFIX80(_9fh); break; \
	case 0xa0:    PREFIX80(_a0h); break; \
	case 0xa1:    PREFIX80(_a1h); break; \
	case 0xa2:    PREFIX80(_a2h); break; \
	case 0xa3:    PREFIX80(_a3h); break; \
	case 0xa4:    PREFIX80(_a4h); break; \
	case 0xa5:    PREFIX80(_a5h); break; \
	case 0xa6:    PREFIX80(_a6h); break; \
	case 0xa7:    PREFIX80(_a7h); break; \
	case 0xa8:    PREFIX80(_a8h); break; \
	case 0xa9:    PREFIX80(_a9h); break; \
	case 0xaa:    PREFIX80(_aah); break; \
	case 0xab:    PREFIX80(_abh); break; \
	case 0xac:    PREFIX80(_ach); break; \
	case 0xad:    PREFIX80(_adh); break; \
	case 0xae:    PREFIX80(_aeh); break; \
	case 0xaf:    PREFIX80(_afh); break; \
	case 0xb0:    PREFIX80(_b0h); break; \
	case 0xb1:    PREFIX80(_b1h); break; \
	case 0xb2:    PREFIX80(_b2h); break; \
	case 0xb3:    PREFIX80(_b3h); break; \
	case 0xb4:    PREFIX80(_b4h); break; \
	case 0xb5:    PREFIX80(_b5h); break; \
	case 0xb6:    PREFIX80(_b6h); break; \
	case 0xb7:    PREFIX80(_b7h); break; \
	case 0xb8:    PREFIX80(_b8h); break; \
	case 0xb9:    PREFIX80(_b9h); break; \
	case 0xba:    PREFIX80(_bah); break; \
	case 0xbb:    PREFIX80(_bbh); break; \
	case 0xbc:    PREFIX80(_bch); break; \
	case 0xbd:    PREFIX80(_bdh); break; \
	case 0xbe:    PREFIX80(_beh); break; \
	case 0xbf:    PREFIX80(_bfh); break; \
	case 0xc0:    PREFIX80(_c0h); break; \
	case 0xc1:    PREFIX80(_c1h); break; \
	case 0xc2:    PREFIX80(_c2h); break; \
	case 0xc3:    PREFIX80(_c3h); break; \
	case 0xc4:    PREFIX80(_c4h); break; \
	case 0xc5:    PREFIX80(_c5h); break; \
	case 0xc6:    PREFIX80(_c6h); break; \
	case 0xc7:    PREFIX80(_c7h); break; \
	case 0xc8:    PREFIX80(_c8h); break; \
	case 0xc9:    PREFIX80(_c9h); break; \
	case 0xca:    PREFIX80(_cah); break; \
	case 0xcb:    PREFIX80(_cbh); break; \
	case 0xcc:    PREFIX80(_cch); break; \
	case 0xcd:    PREFIX80(_cdh); break; \
	case 0xce:    PREFIX80(_ceh); break; \
	case 0xcf:    PREFIX80(_cfh); break; \
	case 0xd0:    PREFIX80(_d0h); break; \
	case 0xd1:    PREFIX80(_d1h); break; \
	case 0xd2:    PREFIX80(_d2h); break; \
	case 0xd3:    PREFIX80(_d3h); break; \
	case 0xd4:    PREFIX80(_d4h); break; \
	case 0xd5:    PREFIX80(_d5h); break; \
	case 0xd6:    PREFIX80(_d6h); break; \
	case 0xd7:    PREFIX80(_d7h); break; \
	case 0xd8:    PREFIX80(_d8h); break; \
	case 0xd9:    PREFIX80(_d9h); break; \
	case 0xda:    PREFIX80(_dah); break; \
	case 0xdb:    PREFIX80(_dbh); break; \
	case 0xdc:    PREFIX80(_dch); break; \
	case 0xdd:    PREFIX80(_ddh); break; \
	case 0xde:    PREFIX80(_deh); break; \
	case 0xdf:    PREFIX80(_dfh); break; \
	case 0xe0:    PREFIX80(_e0h); break; \
	case 0xe1:    PREFIX80(_e1h); break; \
	case 0xe2:    PREFIX80(_e2h); break; \
	case 0xe3:    PREFIX80(_e3h); break; \
	case 0xe4:    PREFIX80(_e4h); break; \
	case 0xe5:    PREFIX80(_e5h); break; \
	case 0xe6:    PREFIX80(_e6h); break; \
	case 0xe7:    PREFIX80(_e7h); break; \
	case 0xe8:    PREFIX80(_e8h); break; \
	case 0xe9:    PREFIX80(_e9h); break; \
	case 0xea:    PREFIX80(_eah); break; \
	case 0xeb:    PREFIX80(_ebh); break; \
	case 0xec:    PREFIX80(_ech); break; \
	case 0xed:    PREFIX80(_edh); break; \
	case 0xee:    PREFIX80(_eeh); break; \
	case 0xef:    PREFIX80(_efh); break; \
	case 0xf0:    PREFIX80(_f0h); break; \
	case 0xf1:    PREFIX80(_f1h); break; \
	case 0xf2:    PREFIX80(_f2h); break; \
	case 0xf3:    PREFIX80(_f3h); break; \
	case 0xf4:    PREFIX80(_f4h); break; \
	case 0xf5:    PREFIX80(_f5h); break; \
	case 0xf6:    PREFIX80(_f6h); break; \
	case 0xf7:    PREFIX80(_f7h); break; \
	case 0xf8:    PREFIX80(_f8h); break; \
	case 0xf9:    PREFIX80(_f9h); break; \
	case 0xfa:    PREFIX80(_fah); break; \
	case 0xfb:    PREFIX80(_fbh); break; \
	case 0xfc:    PREFIX80(_fch); break; \
	case 0xfd:    PREFIX80(_fdh); break; \
	case 0xfe:    PREFIX80(_feh); break; \
	case 0xff:    PREFIX80(_ffh); break; \
	};
#else
#define TABLE80 PREFIX80(_instruction)[I8080_FETCH8()]();
#endif

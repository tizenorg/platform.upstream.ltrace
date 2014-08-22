/*
 * This file is part of ltrace.
 * Copyright (C) 2013 Petr Machata, Red Hat Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301 USA
 */

#include <sys/ptrace.h>
#include <asm/ptrace.h>
#include <assert.h>
#include <elf.h>
#include <libelf.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#include "backend.h"
#include "fetch.h"
#include "library.h"
#include "ltrace-elf.h"
#include "proc.h"
#include "ptrace.h"
#include "regs.h"
#include "type.h"
#include "value.h"

static int
get_hardfp(uint64_t abi_vfp_args)
{
	if (abi_vfp_args == 2)
		fprintf(stderr,
			"Tag_ABI_VFP_args value 2 (tool chain-specific "
			"conventions) not supported.\n");
	return abi_vfp_args == 1;
}

int
arch_elf_init(struct ltelf *lte, struct library *lib)
{
	/* Nothing in this section is strictly critical.  It's not
	 * that much of a deal if we fail to guess right whether the
	 * ABI is softfp or hardfp.  */
	unsigned hardfp = 0;

	Elf_Scn *scn;
	Elf_Data *data;
	GElf_Shdr shdr;
	if (elf_get_section_type(lte, SHT_ARM_ATTRIBUTES, &scn, &shdr) < 0
	    || (scn != NULL && (data = elf_loaddata(scn, &shdr)) == NULL)) {
		fprintf(stderr,
			"Error when obtaining ARM attribute section: %s\n",
			elf_errmsg(-1));
		goto done;

	} else if (scn != NULL && data != NULL) {
		GElf_Xword offset = 0;
		uint8_t version;
		if (elf_read_next_u8(data, &offset, &version) < 0) {
			goto done;
		} else if (version != 'A') {
			fprintf(stderr, "Unsupported ARM attribute section "
				"version %d ('%c').\n", version, version);
			goto done;
		}

		do {
			const char signature[] = "aeabi";
			/* N.B. LEN is including the length field
			 * itself.  */
			uint32_t sec_len;
			if (elf_read_u32(data, offset, &sec_len) < 0
			    || !elf_can_read_next(data, offset, sec_len)) {
				goto done;
			}
			const GElf_Xword next_offset = offset + sec_len;
			offset += 4;

			if (sec_len < 4 + sizeof signature
			    || strcmp(signature, data->d_buf + offset) != 0)
				goto skip;
			offset += sizeof signature;

			const GElf_Xword offset0 = offset;
			uint64_t tag;
			uint32_t sub_len;
			if (elf_read_next_uleb128(data, &offset, &tag) < 0
			    || elf_read_next_u32(data, &offset, &sub_len) < 0
			    || !elf_can_read_next(data, offset0, sub_len))
				goto done;

			if (tag != 1)
				/* IHI0045D_ABI_addenda: "section and
				 * symbol attributes are deprecated
				 * [...] consumers are permitted to
				 * ignore them."  */
				goto skip;

			while (offset < offset0 + sub_len) {
				if (elf_read_next_uleb128(data,
							  &offset, &tag) < 0)
					goto done;

				switch (tag) {
					uint64_t v;
				case 6: /* Tag_CPU_arch */
				case 7: /* Tag_CPU_arch_profile */
				case 8: /* Tag_ARM_ISA_use */
				case 9: /* Tag_THUMB_ISA_use */
				case 10: /* Tag_FP_arch */
				case 11: /* Tag_WMMX_arch */
				case 12: /* Tag_Advanced_SIMD_arch */
				case 13: /* Tag_PCS_config */
				case 14: /* Tag_ABI_PCS_R9_use */
				case 15: /* Tag_ABI_PCS_RW_data */
				case 16: /* Tag_ABI_PCS_RO_data */
				case 17: /* Tag_ABI_PCS_GOT_use */
				case 18: /* Tag_ABI_PCS_wchar_t */
				case 19: /* Tag_ABI_FP_rounding */
				case 20: /* Tag_ABI_FP_denormal */
				case 21: /* Tag_ABI_FP_exceptions */
				case 22: /* Tag_ABI_FP_user_exceptions */
				case 23: /* Tag_ABI_FP_number_model */
				case 24: /* Tag_ABI_align_needed */
				case 25: /* Tag_ABI_align_preserved */
				case 26: /* Tag_ABI_enum_size */
				case 27: /* Tag_ABI_HardFP_use */
				case 28: /* Tag_ABI_VFP_args */
				case 29: /* Tag_ABI_WMMX_args */
				case 30: /* Tag_ABI_optimization_goals */
				case 31: /* Tag_ABI_FP_optimization_goals */
				case 32: /* Tag_compatibility */
				case 34: /* Tag_CPU_unaligned_access */
				case 36: /* Tag_FP_HP_extension */
				case 38: /* Tag_ABI_FP_16bit_format */
				case 42: /* Tag_MPextension_use */
				case 70: /* Tag_MPextension_use as well */
				case 44: /* Tag_DIV_use */
				case 64: /* Tag_nodefaults */
				case 66: /* Tag_T2EE_use */
				case 68: /* Tag_Virtualization_use */
				uleb128:
					if (elf_read_next_uleb128
						(data, &offset, &v) < 0)
						goto done;
					if (tag == 28)
						hardfp = get_hardfp(v);
					if (tag != 32)
						continue;

					/* Tag 32 has two arguments,
					 * fall through.  */

				case 4:	/* Tag_CPU_raw_name */
				case 5:	/* Tag_CPU_name */
				case 65: /* Tag_also_compatible_with */
				case 67: /* Tag_conformance */
				ntbs:
					offset += strlen(data->d_buf
							 + offset) + 1;
					continue;
				}

				/* Handle unknown tags in a generic
				 * manner, if possible.  */
				if (tag <= 32) {
					fprintf(stderr,
						"Unknown tag %lld "
						"at offset %#llx "
						"of ARM attribute section.",
						tag, offset);
					goto skip;
				} else if (tag % 2 == 0) {
					goto uleb128;
				} else {
					goto ntbs;
				}
			}

		skip:
			offset = next_offset;

		} while (elf_can_read_next(data, offset, 1));

	}

done:
	lib->arch.hardfp = hardfp;
	return 0;
}

void
arch_elf_destroy(struct ltelf *lte)
{
}

int
arch_library_init(struct library *lib)
{
	return 0;
}

void
arch_library_destroy(struct library *lib)
{
}

int
arch_library_clone(struct library *retp, struct library *lib)
{
	retp->arch = lib->arch;
	return 0;
}

enum {
	/* How many (double) VFP registers the AAPCS uses for
	 * parameter passing.  */
	NUM_VFP_REGS = 8,
};

struct fetch_context {
	struct pt_regs regs;

	struct {
		union {
			double d[32];
			float s[64];
		};
		uint32_t fpscr;
	} fpregs;

	/* VFP register allocation.  ALLOC.S tracks whether the
	 * corresponding FPREGS.S register is taken, ALLOC.D the same
	 * for FPREGS.D.  We only track 8 (16) registers, because
	 * that's what the ABI uses for parameter passing.  */
	union {
		int16_t d[NUM_VFP_REGS];
		int8_t s[NUM_VFP_REGS * 2];
	} alloc;

	unsigned ncrn;
	arch_addr_t sp;
	arch_addr_t nsaa;
	arch_addr_t ret_struct;

	bool hardfp:1;
	bool in_varargs:1;
};

static int
fetch_register_banks(struct process *proc, struct fetch_context *context)
{
	if (ptrace(PTRACE_GETREGS, proc->pid, NULL, &context->regs) == -1)
		return -1;

	if (context->hardfp
	    && ptrace(PTRACE_GETVFPREGS, proc->pid,
		      NULL, &context->fpregs) == -1)
		return -1;

	context->ncrn = 0;
	context->nsaa = context->sp = get_stack_pointer(proc);
	memset(&context->alloc, 0, sizeof(context->alloc));

	return 0;
}

struct fetch_context *
arch_fetch_arg_init(enum tof type, struct process *proc,
		    struct arg_type_info *ret_info)
{
	struct fetch_context *context = malloc(sizeof(*context));

	{
		struct process *mainp = proc;
		while (mainp->libraries == NULL && mainp->parent != NULL)
			mainp = mainp->parent;
		context->hardfp = mainp->libraries->arch.hardfp;
	}

	if (context == NULL
	    || fetch_register_banks(proc, context) < 0) {
		free(context);
		return NULL;
	}

	if (ret_info->type == ARGTYPE_STRUCT
	    || ret_info->type == ARGTYPE_ARRAY) {
		size_t sz = type_sizeof(proc, ret_info);
		assert(sz != (size_t)-1);
		if (sz > 4) {
			/* XXX double cast */
			context->ret_struct
				= (arch_addr_t)context->regs.uregs[0];
			context->ncrn++;
		}
	}

	return context;
}

struct fetch_context *
arch_fetch_arg_clone(struct process *proc,
		     struct fetch_context *context)
{
	struct fetch_context *clone = malloc(sizeof(*context));
	if (clone == NULL)
		return NULL;
	*clone = *context;
	return clone;
}

/* 0 is success, 1 is failure, negative value is an error.  */
static int
pass_in_vfp(struct fetch_context *ctx, struct process *proc,
	    enum arg_type type, size_t count, struct value *valuep)
{
	assert(type == ARGTYPE_FLOAT || type == ARGTYPE_DOUBLE);
	unsigned max = type == ARGTYPE_DOUBLE ? NUM_VFP_REGS : 2 * NUM_VFP_REGS;
	if (count > max)
		return 1;

	size_t i;
	size_t j;
	for (i = 0; i < max; ++i) {
		for (j = i; j < i + count; ++j)
			if ((type == ARGTYPE_DOUBLE && ctx->alloc.d[j] != 0)
			    || (type == ARGTYPE_FLOAT && ctx->alloc.s[j] != 0))
				goto next;

		/* Found COUNT consecutive unallocated registers at I.  */
		const size_t sz = (type == ARGTYPE_FLOAT ? 4 : 8) * count;
		unsigned char *data = value_reserve(valuep, sz);
		if (data == NULL)
			return -1;

		for (j = i; j < i + count; ++j)
			if (type == ARGTYPE_DOUBLE)
				ctx->alloc.d[j] = -1;
			else
				ctx->alloc.s[j] = -1;

		if (type == ARGTYPE_DOUBLE)
			memcpy(data, ctx->fpregs.d + i, sz);
		else
			memcpy(data, ctx->fpregs.s + i, sz);

		return 0;

	next:
		continue;
	}
	return 1;
}

/* 0 is success, 1 is failure, negative value is an error.  */
static int
consider_vfp(struct fetch_context *ctx, struct process *proc,
	     struct arg_type_info *info, struct value *valuep)
{
	struct arg_type_info *float_info = NULL;
	size_t hfa_size = 1;
	if (info->type == ARGTYPE_FLOAT || info->type == ARGTYPE_DOUBLE)
		float_info = info;
	else
		float_info = type_get_hfa_type(info, &hfa_size);

	if (float_info != NULL && hfa_size <= 4)
		return pass_in_vfp(ctx, proc, float_info->type,
				   hfa_size, valuep);
	return 1;
}

int
arch_fetch_arg_next(struct fetch_context *ctx, enum tof type,
		    struct process *proc,
		    struct arg_type_info *info, struct value *valuep)
{
	const size_t sz = type_sizeof(proc, info);
	assert(sz != (size_t)-1);

	if (ctx->hardfp && !ctx->in_varargs) {
		int rc;
		if ((rc = consider_vfp(ctx, proc, info, valuep)) != 1)
			return rc;
	}

	/* IHI0042E_aapcs: If the argument requires double-word
	 * alignment (8-byte), the NCRN is rounded up to the next even
	 * register number.  */
	const size_t al = type_alignof(proc, info);
	assert(al != (size_t)-1);
	if (al == 8)
		ctx->ncrn = ((ctx->ncrn + 1) / 2) * 2;

	/* If the size in words of the argument is not more than r4
	 * minus NCRN, the argument is copied into core registers,
	 * starting at the NCRN.  */
	/* If the NCRN is less than r4 and the NSAA is equal to the
	 * SP, the argument is split between core registers and the
	 * stack.  */

	const size_t words = (sz + 3) / 4;
	if (ctx->ncrn < 4 && ctx->nsaa == ctx->sp) {
		unsigned char *data = value_reserve(valuep, words * 4);
		if (data == NULL)
			return -1;
		size_t i;
		for (i = 0; i < words && ctx->ncrn < 4; ++i) {
			memcpy(data, &ctx->regs.uregs[ctx->ncrn++], 4);
			data += 4;
		}
		const size_t rest = (words - i) * 4;
		if (rest > 0) {
			umovebytes(proc, ctx->nsaa, data, rest);
			ctx->nsaa += rest;
		}
		return 0;
	}

	assert(ctx->ncrn == 4);

	/* If the argument required double-word alignment (8-byte),
	 * then the NSAA is rounded up to the next double-word
	 * address.  */
	if (al == 8)
		/* XXX double cast.  */
		ctx->nsaa = (arch_addr_t)((((uintptr_t)ctx->nsaa + 7) / 8) * 8);
	else
		ctx->nsaa = (arch_addr_t)((((uintptr_t)ctx->nsaa + 3) / 4) * 4);

	value_in_inferior(valuep, ctx->nsaa);
	ctx->nsaa += sz;

	return 0;
}

int
arch_fetch_retval(struct fetch_context *ctx, enum tof type,
		  struct process *proc, struct arg_type_info *info,
		  struct value *valuep)
{
	if (fetch_register_banks(proc, ctx) < 0)
		return -1;

	if (ctx->hardfp && !ctx->in_varargs) {
		int rc;
		if ((rc = consider_vfp(ctx, proc, info, valuep)) != 1)
			return rc;
	}

	size_t sz = type_sizeof(proc, info);
	assert(sz != (size_t)-1);

	switch (info->type) {
		unsigned char *data;

	case ARGTYPE_VOID:
		return 0;

	case ARGTYPE_FLOAT:
	case ARGTYPE_DOUBLE:
		if (ctx->hardfp && !ctx->in_varargs) {
			unsigned char *data = value_reserve(valuep, sz);
			if (data == NULL)
				return -1;
			memmove(data, &ctx->fpregs, sz);
			return 0;
		}
		goto pass_in_registers;

	case ARGTYPE_ARRAY:
	case ARGTYPE_STRUCT:
		if (sz > 4) {
			value_in_inferior(valuep, ctx->ret_struct);
			return 0;
		}
		/* Fall through.  */

	case ARGTYPE_CHAR:
	case ARGTYPE_SHORT:
	case ARGTYPE_USHORT:
	case ARGTYPE_INT:
	case ARGTYPE_UINT:
	case ARGTYPE_LONG:
	case ARGTYPE_ULONG:
	case ARGTYPE_POINTER:
	pass_in_registers:
		if ((data = value_reserve(valuep, sz)) == NULL)
			return -1;
		memmove(data, ctx->regs.uregs, sz);
		return 0;
	}
	assert(info->type != info->type);
	abort();
}

void
arch_fetch_arg_done(struct fetch_context *context)
{
	free(context);
}

int
arch_fetch_param_pack_start(struct fetch_context *context,
			    enum param_pack_flavor ppflavor)
{
	if (ppflavor == PARAM_PACK_VARARGS)
		context->in_varargs = true;
	return 0;
}

void
arch_fetch_param_pack_end(struct fetch_context *context)
{
	context->in_varargs = false;
}

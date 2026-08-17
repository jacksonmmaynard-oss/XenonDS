// SPDX-License-Identifier: GPL-2.0-only
.section ".elfldr"

#include <ppc/xenonsprs.h>

.globl xenonds_elf_hold_thread
xenonds_elf_hold_thread:
	lis %r4,0x87ff
	lis %r3,xenonds_secondary_hold_addr@h
	ori %r3,%r3,xenonds_secondary_hold_addr@l
	add %r4,%r4,%r3
	lis %r3,elfldr_start@h
	ori %r3,%r3,elfldr_start@l
	sub %r4,%r4,%r3
1:
	lwz %r3,0(%r4)
	cmpwi %r3,0
	beq 1b
	li %r4,0

.globl xenonds_elf_run
xenonds_elf_run:
	mr %r31,%r3
	mr %r30,%r4

	lis %r4,0x07ff
	lis %r3,xenonds_b64@h
	ori %r3,%r3,xenonds_b64@l
	add %r4,%r4,%r3
	lis %r3,elfldr_start@h
	ori %r3,%r3,elfldr_start@l
	sub %r4,%r4,%r3
	mtsrr0 %r4

	mfmsr %r3
	li %r4,0x30
	andc %r3,%r3,%r4
	lis %r4,0x8000
	rldicr %r4,%r4,32,31
	or %r3,%r3,%r4
	mtsrr1 %r3
	rfid

xenonds_b64:
	isync
	slbia
	isync

	mfspr %r3,hid6
	li %r4,0xF
	sldi %r4,%r4,44
	andc %r3,%r3,%r4
	li %r4,0x8
	sldi %r4,%r4,44
	or %r3,%r3,%r4
	mtspr hid6,%r3
	sync
	isync

	mfspr %r3,lpcr
	li %r4,0x400
	andc %r3,%r3,%r4
	mtspr lpcr,%r3
	sync
	isync

	mfspr %r3,pir
	cmplwi %r3,0
	bne 1f
	mr %r3,%r30
1:
	mr %r4,%r31
	li %r5,0
	mtctr %r31
	bctr

.globl xenonds_secondary_hold_addr
xenonds_secondary_hold_addr:
	.long 0

/* bytecode.h */

#ifndef BYTECODE_H_FILE
#define BYTECODE_H_FILE

#include "fh_internal.h"
#include "value.h"

#define MAX_FUNC_REGS 256

enum fh_bc_opcode {
    OPC_RET,
    OPC_CALL,

    OPC_CLOSURE,
    OPC_GETUPVAL,
    OPC_SETUPVAL,
    OPC_GETGLOBAL,
    OPC_SETGLOBAL,

    OPC_MOV,
    OPC_LDNULL,
    OPC_LDC,

    OPC_JMP,
    OPC_TEST,
    OPC_CMP_EQ,
    OPC_CMP_EQI,
    OPC_CMP_EQF,
    OPC_CMP_LT,
    OPC_CMP_LTI,
    OPC_CMP_LTF,
    OPC_CMP_LE,
    OPC_CMP_LEI,
    OPC_CMP_LEF,
    OPC_CMP_GT,
    OPC_CMP_GTI,
    OPC_CMP_GTF,
    OPC_CMP_GE,
    OPC_CMP_GEI,
    OPC_CMP_GEF,

    OPC_GETEL,
    OPC_GETEL_ARRAY,
    OPC_GETEL_MAP,
    OPC_SETEL,
    OPC_NEWARRAY,
    OPC_NEWMAP,

    OPC_ADD,
    OPC_ADDI,
    OPC_ADDF,
    OPC_SUB,
    OPC_SUBI,
    OPC_SUBF,
    OPC_MUL,
    OPC_MULI,
    OPC_MULF,
    OPC_DIV,
    OPC_DIVI,
    OPC_DIVF,
    OPC_MOD,
    OPC_NEG,
    OPC_NOT,

    OPC_BAND,
    OPC_BOR,
    OPC_BXOR,
    OPC_RSHIFT,
    OPC_LSHIFT,
    OPC_BNOT,
    OPC_INC,
    OPC_DEC,

    OPC_LEN,
    OPC_APPEND
};

#define GET_INSTR_OP(instr)    (((uint32_t)(instr))&0x3f)
#define GET_INSTR_RA(instr)   ((((uint32_t)(instr))>>6)&0xff)
#define GET_INSTR_RB(instr)   ((((uint32_t)(instr))>>14)&0x1ff)
#define GET_INSTR_RC(instr)   ((((uint32_t)(instr))>>23)&0x1ff)
#define GET_INSTR_RU(instr)   ((((uint32_t)(instr))>>14)&0x3ffff)
#define GET_INSTR_RS(instr)   (((int32_t)GET_INSTR_RU(instr))-(1<<17))

#define PLACE_INSTR_OP(op)     ((uint32_t)(op)&0x3f )
#define PLACE_INSTR_RA(ra)    (((uint32_t)(ra)&0xff )<<6)
#define PLACE_INSTR_RB(rb)    (((uint32_t)(rb)&0x1ff)<<14)
#define PLACE_INSTR_RC(rc)    (((uint32_t)(rc)&0x1ff)<<23)
#define PLACE_INSTR_RU(ru)    (((uint32_t)(ru)&0x3ffff)<<14)
#define PLACE_INSTR_RS(rs)    PLACE_INSTR_RU((rs)+(1<<17))

#define INSTR_OP_MASK           0x3f
#define INSTR_RA_MASK         ( 0xff<<6)
#define INSTR_RB_MASK         (0x1ff<<14)
#define INSTR_RC_MASK         (0x1ff<<23)
#define INSTR_RU_MASK         ((uint32_t)0x3ffff<<14)
#define INSTR_RS_MASK         INSTR_RU_MASK

// ============================================================
// Instruction encoding helpers
// ------------------------------------------------------------
// Every bytecode instruction is a 32-bit word.
//
// Layout (bit positions):
//
//   31 ......... 23 22 ......... 14 13 ..... 6 5 ..... 0
//   [     RC     ][     RB     ][   RA   ][  OPCODE ]
//
//   OPCODE : 6 bits  (what operation to execute)
//   RA     : 8 bits  (destination register or primary operand)
//   RB     : 9 bits  (secondary register or constant reference)
//   RC     : 9 bits  (third register or constant reference)
//
// Not all instructions need all fields,
// so we provide multiple packing macros for convenience.
// ============================================================


// Format: [ OPCODE | RA ]
// Used for instructions with a single register operand.
// Example: RET r0, NEG r3, NOT r5
#define MAKE_INSTR_A(op, ra) \
    (PLACE_INSTR_OP(op) | PLACE_INSTR_RA(ra))


// Format: [ OPCODE | RA | RB ]
// Used for instructions with two register operands.
// Example: MOV r1, r2   => r1 = r2
//          INC r3, r3   => r3 = r3 + 1
#define MAKE_INSTR_AB(op, ra, rb) \
    (PLACE_INSTR_OP(op) | PLACE_INSTR_RA(ra) | PLACE_INSTR_RB(rb))


// Format: [ OPCODE | RA | RB | RC ]
// Used for classic three-operand instructions.
// Example: ADD r0, r1, r2   => r0 = r1 + r2
//          MUL r3, r4, r5   => r3 = r4 * r5
#define MAKE_INSTR_ABC(op, ra, rb, rc) \
    (PLACE_INSTR_OP(op) | PLACE_INSTR_RA(ra) | PLACE_INSTR_RB(rb) | PLACE_INSTR_RC(rc))


// Format: [ OPCODE | RA | RU ]
// RU = unsigned immediate value (18 bits, overlaps RB+RC space).
// Used when an instruction needs a larger immediate value.
// Example: LDC r0, const_index
//          CALL r2, arg_count
//          NEWARRAY r1, element_count
#define MAKE_INSTR_AU(op, ra, ru) \
    (PLACE_INSTR_OP(op) | PLACE_INSTR_RA(ra) | PLACE_INSTR_RU(ru))


// Format: [ OPCODE | RA | RS ]
// RS = signed immediate value (18 bits, overlaps RB+RC space).
// Used for relative jumps.
// Example: JMP +12   (jump forward 12 instructions)
//          JMP -5    (jump backward 5 instructions)
#define MAKE_INSTR_AS(op, ra, rs) \
    (PLACE_INSTR_OP(op) | PLACE_INSTR_RA(ra) | PLACE_INSTR_RS(rs))

// ============================================================
// End of instruction packing macros
// ============================================================

void fh_dump_bc_instr(struct fh_program *prog, int32_t addr, uint32_t instr);

#endif /* BYTECODE_H_FILE */

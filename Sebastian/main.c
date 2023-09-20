#include <assert.h>
#include <stdint.h>
#include <stdio.h>

typedef uint8_t u8;
typedef int8_t  i8;
typedef uint16_t u16;
typedef int16_t  i16;
typedef uint32_t u32;
typedef uint64_t u64;
typedef uint32_t b32;
typedef char* String;

#define array_length(arr_) (sizeof(arr_) / sizeof(*(arr_)))

String regs[][2] = {
    [0][0] = "al",
    [1][0] = "cl",
    [2][0] = "dl",
    [3][0] = "bl",
    [4][0] = "ah",
    [5][0] = "ch",
    [6][0] = "dh",
    [7][0] = "bh",
    [0][1] = "ax",
    [1][1] = "cx",
    [2][1] = "dx",
    [3][1] = "bx",
    [4][1] = "sp",
    [5][1] = "bp",
    [6][1] = "si",
    [7][1] = "di",
};

String rms[] = { "bx + si", "bx + di", "bp + si", "bp + di", "si", "di", "bp", "bx" };

typedef struct {
    u32 shift;
    u32 length;
} BitPattern;

u64 extract_pattern(u64 a, BitPattern pattern) {
    return pattern.length == 0 ? 0 : (a >> pattern.shift) & ((1ull << pattern.length) - 1);
}

typedef enum {
    REG_MEM_WITH_DISPLACEMENT,
    IMMEDIATE_WITH_DISPLACEMENT,
    IMMEDIATE,
    IMMEDIATE_TO_REGISTER,

    FORMAT_COUNT
} InstructionFormat;

typedef enum {
    MOV,
    ADD,
    OR,
    ADC,
    SBB,
    SUB,
    AND,
    XOR,
    CMP,
    OPERATION_COUNT
} Operation;

u32 extended_code[] = {
    [ADD] = 0b000,
    [OR]  = 0b001,
    [ADC] = 0b010,
    [SBB] = 0b011,
    [AND] = 0b100,
    [SUB] = 0b101,
    [XOR] = 0b110,
    [CMP] = 0b111
};

typedef struct {
    u32 opcode;
    BitPattern opcode_pattern;
    InstructionFormat format;
    Operation operation;
} Opcode;

BitPattern no_pattern = { 0, 0 };

const BitPattern opcode4 = { 4, 4 };
const BitPattern opcode6 = { 2, 6 };
const BitPattern opcode7 = { 1, 7 };
const BitPattern opcode8 = { 0, 8 };

const BitPattern pat_0 = { 0, 1 };
const BitPattern pat_1 = { 1, 1 };
const BitPattern pat_3 = { 3, 1 };

const BitPattern pat_67 = { 6, 2 };
const BitPattern pat_345 = { 3, 3 };
const BitPattern pat_012 = { 0, 3 };

String opname[] = {
    [MOV] = "mov",
    [ADD] = "add",
    [OR]  = "or",
    [ADC] = "adc",
    [SBB] = "sbb",
    [SUB] = "sub",
    [AND] = "and",
    [XOR] = "xor",
    [CMP] = "cmp",
};

Opcode opcodes[] = {
    // opcod  e  pattern,  format
    { 0b100010,   opcode6,  REG_MEM_WITH_DISPLACEMENT,   MOV },
    { 0b1100011,  opcode7,  IMMEDIATE_WITH_DISPLACEMENT, MOV },
    { 0b1011,     opcode4,  IMMEDIATE,                   MOV },
    { 0b001010,   opcode6,  REG_MEM_WITH_DISPLACEMENT,   SUB },
    { 0b001110,   opcode6,  REG_MEM_WITH_DISPLACEMENT,   CMP },
    { 0b100000,   opcode6,  IMMEDIATE_TO_REGISTER,       ADD },
    { 0b100000,   opcode6,  IMMEDIATE_TO_REGISTER,       OR  },
    { 0b100000,   opcode6,  IMMEDIATE_TO_REGISTER,       ADC },
    { 0b100000,   opcode6,  IMMEDIATE_TO_REGISTER,       SBB },
    { 0b100000,   opcode6,  IMMEDIATE_TO_REGISTER,       AND },
    { 0b100000,   opcode6,  IMMEDIATE_TO_REGISTER,       SUB },
    { 0b100000,   opcode6,  IMMEDIATE_TO_REGISTER,       XOR },
    { 0b100000,   opcode6,  IMMEDIATE_TO_REGISTER,       CMP },
};

typedef struct {
    u32 opcode_index;
    u8 d;     // 0
    u8 w;     // 1
    u8 s;     // 2
    u8 mod;   // 3
    u8 reg;   // 4
    u8 rm;    // 5
    u16 disp; // 6
    u16 data; // 7
} InstructionData;

typedef enum {
    MEMORY_MODE,
    MEMORY_MODE_8BIT,
    MEMORY_MODE_16BIT,
    MEMORY_MODE_REGISTER,
} Modes;

#define READ do { b = fgetc(fp); if (b == EOF) return 0; } while(0)

b32 read_maybe_wide(FILE *fp, u16 *value, b32 wide) {
    int b;
    READ;
    *value = (i8)b;
    if (wide) {
        READ;
        *value = (*value & 0xFF) | b << 8;
    }
    return 1;
}

b32 read_displacement(FILE *fp, InstructionData *instruction) {
    instruction->disp = 0;
    switch (instruction->mod) {
    case MEMORY_MODE_8BIT: {
        return read_maybe_wide(fp, &instruction->disp, 0);
        break;
    }
    case MEMORY_MODE_16BIT: {
        return read_maybe_wide(fp, &instruction->disp, 1);
        break;
    }
    case MEMORY_MODE: {
        // TODO if R/M = 110, then displacement = 16
        break;
    }
    case MEMORY_MODE_REGISTER: {
        break;
    }
    }
    return 1;
}

b32 read_instruction(FILE *fp, InstructionData *instruction) {
    int b;

    READ;
    for (u32 idx = 0; idx < array_length(opcodes); idx++) {
        Opcode opcode = opcodes[idx];
        if (extract_pattern(b, opcode.opcode_pattern) == opcode.opcode) {
            u32 pos = ftell(fp); u32 old_b = b;
            *instruction = (InstructionData) { };
            instruction->opcode_index = idx;
            switch (opcode.format) {
            case REG_MEM_WITH_DISPLACEMENT: {
                instruction->w   = extract_pattern(b, pat_0);
                instruction->d   = extract_pattern(b, pat_1);
                READ;
                instruction->rm  = extract_pattern(b, pat_012);
                instruction->reg = extract_pattern(b, pat_345);
                instruction->mod = extract_pattern(b, pat_67);
                return read_displacement(fp, instruction);
            }
            case IMMEDIATE: {
                instruction->reg = extract_pattern(b, pat_012);
                instruction->w   = extract_pattern(b, pat_3);
                return read_maybe_wide(fp, &instruction->data, instruction->w);
            }
            case IMMEDIATE_TO_REGISTER: {
                instruction->w   = extract_pattern(b, pat_0);
                instruction->s   = extract_pattern(b, pat_1);
                READ;
                if (extract_pattern(b, pat_345) == extended_code[opcode.operation]) {
                    instruction->rm  = extract_pattern(b, pat_012);
                    instruction->mod = extract_pattern(b, pat_67);
                    if (!read_displacement(fp, instruction)) return 0;
                    return read_maybe_wide(fp, &instruction->data, ((instruction->s << 1)|instruction->w) == 0x1);
                }
                break;
            }

            default: {
                assert(0);
                break;
            }
            }
            fseek(fp, pos, SEEK_SET); b = old_b;
        }
    }
    while (1) {
        printf("%02x ", b);
        READ;
    }
}

void print_instruction(InstructionData* instruction) {
    Opcode opcode = opcodes[instruction->opcode_index];
    switch (opcode.format) {
    case REG_MEM_WITH_DISPLACEMENT: {
        if (instruction->mod == MEMORY_MODE_REGISTER) {
            if (instruction->d) {
                printf("%s %s, %s\n", opname[opcode.operation], regs[instruction->reg][instruction->w], regs[instruction->rm][instruction->w]);
            } else {
                printf("%s %s, %s\n", opname[opcode.operation], regs[instruction->rm][instruction->w], regs[instruction->reg][instruction->w]);
            }
        } else {
            if (instruction->d) {
                if (instruction->disp) {
                    printf("%s %s, [%s %+i]\n", opname[opcode.operation], regs[instruction->reg][instruction->w], rms[instruction->rm], instruction->disp);
                } else {
                    printf("%s %s, [%s]\n", opname[opcode.operation], regs[instruction->reg][instruction->w], rms[instruction->rm]);
                }
            } else {
                if (instruction->disp) {
                    printf("%s [%s %+i], %s\n", opname[opcode.operation], rms[instruction->rm], instruction->disp, regs[instruction->reg][instruction->w]);
                } else {
                    printf("%s [%s], %s\n", opname[opcode.operation], rms[instruction->rm], regs[instruction->reg][instruction->w]);
                }
            }
        }
        break;
    }
    case IMMEDIATE: {
        printf("%s %s, %i\n", opname[opcode.operation], regs[instruction->reg][instruction->w], (i16)instruction->data);
        break;
    }
    case IMMEDIATE_TO_REGISTER: {
        printf("%s %s, %i\n", opname[opcode.operation], regs[instruction->rm][instruction->w], (i16)instruction->data);
        break;
    }

    default: {
        assert(0);
        break;
    }
    }
}

typedef enum {
    CF = 1 << 0,
    PF = 1 << 1,
    AF = 1 << 2,
    ZF = 1 << 3,
    SF = 1 << 4,
    OF = 1 << 5,
    IF = 1 << 6,
    DF = 1 << 7,
    TF = 1 << 8,
    FLAG_COUNT = 9
} Flags;

char flag_names[] = {
    'C',
    'P',
    'A',
    'Z',
    'S',
    'O',
    'I',
    'D',
    'T',
};

typedef struct {
    u16 regs[8];
    Flags flags;
    u8 mems[65536];
} CPUState;

void mov(CPUState* state, u32 dst_reg, u16 data) {
    state->regs[dst_reg] = data;
}

u32 modify_flag(u32 a, u32 f, b32 cond) {
    if (cond) {
        return a | f;
    } else {
        return a & (~f);
    }
}

void set_cpu_flags(CPUState* state, u16 data) {
    Flags flags = state->flags;
    flags = modify_flag(flags, ZF, data == 0);
    flags = modify_flag(flags, SF, data & 0x8000);
    flags = modify_flag(flags, PF, !(__builtin_popcount(data & 0xFF) & 1));
    state->flags = flags;
}

void add(CPUState* state, u32 dst_reg, u16 data) {
    state->regs[dst_reg] += data;
    set_cpu_flags(state, state->regs[dst_reg]);
}

void sub(CPUState* state, u32 dst_reg, u16 data) {
    state->regs[dst_reg] -= data;
    set_cpu_flags(state, state->regs[dst_reg]);
}

void cmp(CPUState* state, u32 dst_reg, u16 data) {
    set_cpu_flags(state, state->regs[dst_reg] - data);
}

void (*(operations[]))(CPUState* state, u32 dst_reg, u16 data) = {
    [MOV] = mov,
    [ADD] = add,
    //[OR]  = or,
    //[ADC] = adc,
    //[SBB] = sbb,
    [SUB] = sub,
    //[AND] = and,
    //[XOR] = xor,
    [CMP] = cmp,
};

void print_state(CPUState* state) {
    for (int i = 0; i < 8; i++) {
        printf("%s=%04x ", regs[i][1], state->regs[i]);
    }
    for (int i = 0; i < FLAG_COUNT; i++) {
        if (state->flags & (1 << i)) {
            printf("%c", flag_names[i]);
        } else {
            printf("%c", flag_names[i] + ('a' - 'A'));
        }
    }
    printf(" ");
}

void simulate_instruction(InstructionData* instruction, CPUState* state) {
    Opcode opcode = opcodes[instruction->opcode_index];
    switch (opcode.format) {
    case REG_MEM_WITH_DISPLACEMENT: {
        if (instruction->mod == MEMORY_MODE_REGISTER) {
            if (instruction->d) {
                operations[opcode.operation](state, instruction->reg, state->regs[instruction->rm]);
            } else {
                operations[opcode.operation](state, instruction->rm, state->regs[instruction->reg]);
            }
        } else {
            if (instruction->d) {
                if (instruction->disp) {
                    printf("%s %s, [%s %+i]\n", opname[opcode.operation], regs[instruction->reg][instruction->w], rms[instruction->rm], instruction->disp);
                } else {
                    printf("%s %s, [%s]\n", opname[opcode.operation], regs[instruction->reg][instruction->w], rms[instruction->rm]);
                }
            } else {
                if (instruction->disp) {
                    printf("%s [%s %+i], %s\n", opname[opcode.operation], rms[instruction->rm], instruction->disp, regs[instruction->reg][instruction->w]);
                } else {
                    printf("%s [%s], %s\n", opname[opcode.operation], rms[instruction->rm], regs[instruction->reg][instruction->w]);
                }
            }
        }
        break;
    }
    case IMMEDIATE: {
        operations[opcode.operation](state, instruction->reg, instruction->data);
        break;
    }
    case IMMEDIATE_TO_REGISTER: {
        operations[opcode.operation](state, instruction->rm, instruction->data);
        break;
    }

    default: {
        assert(0);
        break;
    }
    }

}
int main(int argc, char** argv) {

    if (argc != 2) return 0;
    FILE *fp = fopen(argv[1], "rb");
    if (!fp) return 0;


    printf("bits 16\n\n");

    CPUState state;
    while (1) {
        InstructionData instruction;
        if (!read_instruction(fp, &instruction)) break;
        print_state(&state);
        simulate_instruction(&instruction, &state);
        print_instruction(&instruction);
    }
    print_state(&state); printf("\n");
    return 0;
}

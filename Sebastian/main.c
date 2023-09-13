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
typedef char* string_t;

#define array_length(arr_) (sizeof(arr_) / sizeof(*(arr_)))

string_t regs[][2] = {
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

string_t rms[] = { "bx + si", "bx + di", "bp + si", "bp + di", "si", "di", "bp", "bx" };

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

typedef struct {
    u32 opcode;
    BitPattern opcode_pattern;
    InstructionFormat format;
    string_t string;
    u32 ext_opcode;
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


Opcode opcodes[] = {
    // opcod  e  pattern,  format
    { 0b100010,   opcode6,  REG_MEM_WITH_DISPLACEMENT,   "mov" },
    { 0b1100011,  opcode7,  IMMEDIATE_WITH_DISPLACEMENT, "mov" },
    { 0b1011,     opcode4,  IMMEDIATE,                   "mov" },
    { 0b001010,   opcode6,  REG_MEM_WITH_DISPLACEMENT,   "sub" },
    { 0b001110,   opcode6,  REG_MEM_WITH_DISPLACEMENT,   "cmp" },
    { 0b100000,   opcode6,  IMMEDIATE_TO_REGISTER,       "add", 0b000 },
    { 0b100000,   opcode6,  IMMEDIATE_TO_REGISTER,       "or",  0b001 },
    { 0b100000,   opcode6,  IMMEDIATE_TO_REGISTER,       "adc", 0b010 },
    { 0b100000,   opcode6,  IMMEDIATE_TO_REGISTER,       "sbb", 0b011 },
    { 0b100000,   opcode6,  IMMEDIATE_TO_REGISTER,       "and", 0b100 },
    { 0b100000,   opcode6,  IMMEDIATE_TO_REGISTER,       "sub", 0b101 },
    { 0b100000,   opcode6,  IMMEDIATE_TO_REGISTER,       "xor", 0b110 },
    { 0b100000,   opcode6,  IMMEDIATE_TO_REGISTER,       "cmp", 0b111 },
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
                if (extract_pattern(b, pat_345) == opcode.ext_opcode) {
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
                printf("%s %s, %s\n", opcode.string, regs[instruction->reg][instruction->w], regs[instruction->rm][instruction->w]);
            } else {
                printf("%s %s, %s\n", opcode.string, regs[instruction->rm][instruction->w], regs[instruction->reg][instruction->w]);
            }
        } else {
            if (instruction->d) {
                if (instruction->disp) {
                    printf("%s %s, [%s %+i]\n", opcode.string, regs[instruction->reg][instruction->w], rms[instruction->rm], instruction->disp);
                } else {
                    printf("%s %s, [%s]\n", opcode.string, regs[instruction->reg][instruction->w], rms[instruction->rm]);
                }
            } else {
                if (instruction->disp) {
                    printf("%s [%s %+i], %s\n", opcode.string, rms[instruction->rm], instruction->disp, regs[instruction->reg][instruction->w]);
                } else {
                    printf("%s [%s], %s\n", opcode.string, rms[instruction->rm], regs[instruction->reg][instruction->w]);
                }
            }
        }
        break;
    }
    case IMMEDIATE: {
        printf("%s %s, %i\n", opcode.string, regs[instruction->reg][instruction->w], (i16)instruction->data);
        break;
    }
    case IMMEDIATE_TO_REGISTER: {
        printf("%s %s, %i\n", opcode.string, regs[instruction->reg][instruction->w], (i16)instruction->data);
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
    
    while (1) {
        InstructionData instruction;
        if (!read_instruction(fp, &instruction)) break;
        print_instruction(&instruction);
    }
    return 0;
}

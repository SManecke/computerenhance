#include <stdio.h>
#include <stdint.h>
#include <assert.h>
#include <stdlib.h>

// types
typedef int8_t i8;
typedef int16_t i16;
typedef int32_t i32;
typedef int64_t i64;

typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;

typedef float f32;
typedef double f64;

u8 *asm_file;
size_t asm_file_size;

struct Instruction_Stream {
	u8 at;
	u8 *bytes;
	u32 size;
};

Instruction_Stream read_file(char *path) {
	FILE *f = fopen(path, "rb");
	Instruction_Stream res = { 0 };
	if (f == nullptr) {
		assert(!"file not found");
		return res;
	}
	fseek(f, 0, SEEK_END);
	res.size = ftell(f);
	fseek(f, 0, SEEK_SET);
	res.bytes = (u8 *)malloc(res.size);
	fread(res.bytes, 1, res.size, f);
	fclose(f);
	return res;
}

enum OP_Code {
	mov_reg 		= 0b10001000,
	mov_reg_mask 	= 0b11111100,
	mov_imm_r 		= 0b10110000,
	mov_imm_r_mask 	= 0b11110000,
};

enum Mode_Type {

	MOD_Mem 		= 0b00000000,
	MOD_Mem_8bit	= 0b01000000,
	MOD_Mem_16bit	= 0b10000000,
	MOD_Reg			= 0b11000000,
};

enum Register_Code_0 { AL, CL, DL, BL, AH, CH, DH, BH };
enum Register_Code_1 { AX, CX, DX, BX, SP, BP, SI, DI };
char* Register_Code_0_str[] = {"al", "cl", "dl", "bl", "ah", "ch", "dh", "bh"};
char* Register_Code_1_str[] = {"ax", "cx", "dx", "bx", "sp", "bp", "si", "di"};
char* RM_code_str[] = { "bx + si", "bx + di", "bp + si", "bp + di", "si", "di", "bp", "bx" };

void parse_mov_regular(Instruction_Stream *inst) {
	u8 byte[2] = { inst->bytes[inst->at] , inst->bytes[inst->at + 1] };
	bool W =  byte[0] & 0b00000001;
	bool D =  byte[0] & 0b00000010;
	u8 MOD =  byte[1] & 0b11000000;
	u8 REG = (byte[1] & 0b00111000) >> 3;
	u8 RM =   byte[1] & 0b00000111;
	char** reg_str = W ? Register_Code_1_str : Register_Code_0_str;

	if (MOD == MOD_Reg) {
		printf("mov %s, %s\n", D ? reg_str[REG] : reg_str[RM], D ? reg_str[RM] : reg_str[REG]);
	} else {
		i16 disp_value = MOD == MOD_Mem_16bit ? *(i16*)(&inst->bytes[inst->at + 2]) : MOD == MOD_Mem_8bit ? (i8)inst->bytes[inst->at + 2] : 0;
		char num[16] = "";
		if (disp_value > 0) sprintf(num, " + %i", disp_value);
		if (D)
			printf("mov %s, [%s%s]\n", reg_str[REG], RM_code_str[RM], num);
		else
			printf("mov [%s%s], %s\n", RM_code_str[RM], num, reg_str[REG]);
	}

	inst->at+= 2;
}

void parse_mov_immediate_reg(Instruction_Stream *inst) {
	u8 byte[2] = { inst->bytes[inst->at] , inst->bytes[inst->at + 1]};
	bool W = byte[0] & 0b00001000;
	u8 REG = byte[0] & 0b00000111;
	i16 value = W ? *(i16*)(&inst->bytes[inst->at + 1]) : (i8)inst->bytes[inst->at + 1];
	char** reg_str = W ? Register_Code_1_str : Register_Code_0_str;

	printf("mov %s, %i\n", reg_str[REG], value);
	inst->at+= 2 + W;
}

#define check_mask(_INPUT_, _MASK_, _CODE_) (((_INPUT_) & (_MASK_)) == (_CODE_))

int main(int argc, char **argv)  {
	Instruction_Stream inst = read_file(argv[1]);

	printf("bits 16\n\n");

	while(inst.at < inst.size) {
		if 	    (check_mask(inst.bytes[inst.at], mov_reg_mask, mov_reg))  		parse_mov_regular(&inst);
		else if (check_mask(inst.bytes[inst.at], mov_imm_r_mask, mov_imm_r))	parse_mov_immediate_reg(&inst);	
		else 	inst.at++;
	}
	return 0;
}

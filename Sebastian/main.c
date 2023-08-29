#include <stdio.h>

typedef char* string_t;

string_t insts[] = {
    [0x22] = "mov",
};

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

int main(int argc, char** argv) {

    if (argc != 2) return 0;
    FILE *fp = fopen(argv[1], "rb");
    if (!fp) return 0;


    printf("bits 16\n\n");
    
    int b;
    while (1) {
        b = fgetc(fp);
        if (b == EOF) break;
        int inst = (b >> 2) & 0x3F;
        int d    = (b >> 1) & 0x01;
        int w    = (b >> 0) & 0x01;
        b = fgetc(fp);
        if (b == EOF) break;
        int mod  = (b >> 6) & 0x03; 
        int reg  = (b >> 3) & 0x07; 
        int r_m  = (b >> 0) & 0x07; 

        if (mod == 0x3) {
            printf("%s %s, %s\n", insts[inst], d ? regs[reg][w] : regs[r_m][w], d ? regs[r_m][w] : regs[reg][w]);
        }
    }
    
    return 0;
}

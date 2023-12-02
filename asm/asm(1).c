#define _CRT_SECURE_NO_WARNINGS

// Imports
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdbool.h>
#include <ctype.h>
#include "helpers.h"

// Constants
#define MEMORY_SIZE 4096
#define MAX_LINE_LENGTH 300
#define MAX_LABEL_LENGTH 50

// Structs
typedef char line[MAX_LINE_LENGTH + 1];
void first_pass(FILE* file);

typedef struct {
    char* label;
    char* opcode;
    char* rd;
    char* rs;
    char* rt;
    char* imm;
    bool          is_imm;
} instruction;

typedef struct {
    int       location;
    char      label_name[MAX_LABEL_LENGTH + 1];
} label;

// Enums
typedef enum {
    EMPTY,
    REGULAR,
    LABEL,
    WORD
} line_type;

// Global Variables
static int         labels_amount = 0;
static int         memory[MEMORY_SIZE];
static label       labels[MEMORY_SIZE];

// Functions' Declarations
instruction parser(char* line);
line_type find_line_type(instruction inst);

int encode_reg(char* reg);
int encode_imm(char* imm);
int encode_opcode(char* opcode);
int encode_instruction(int opcode, int rd, int rs, int rt);
void execute(FILE* input_file, FILE* output_file);

// Main Function
int main(int argc, char* argv[]) {
    FILE* program = fopen(argv[1], "r"), * output = fopen(argv[2], "w");

    if (program == NULL || output == NULL) {
        printf("Failed to open one of the files, the program will execute with status 1"); //Failed to open one of the files.
        exit(1);
    }
    execute(program, output);
    exit(0);
}

// Funcitons
instruction parser(char* line) {
    // remove comments
    strtok(line, "#"); 

    char* line_end = line + strlen(line);
    char* current = line;
    char* token;

    // check if line is empty
    instruction inst = { NULL, NULL, NULL, NULL, NULL, NULL, NULL };
    if (current >= line_end) {
        return inst;
    }

    // check if line is label or regular instruction
    token = strtok(current, ": \t\n,");
    if (token != NULL && encode_opcode(token) != -1) {
        inst.label = NULL;
        inst.opcode = token;
    }
    else {
        inst.label = token;
        current = current + strlen(current) + 1;
        if (current >= line_end)
            return inst;
        inst.opcode = strtok(current, ": \t\n,");
    }
    current = current + strlen(current) + 1;
    if (current >= line_end) {
        return inst;
    }

    // extract rd
    inst.rd = strtok(current, " \t\n,");
    if (!strcmp(inst.rd, "$imm")) {
        inst.is_imm = true;
    }
    current = current + strlen(current) + 1;
    if (current >= line_end)
        return inst;

    // extract rs
    inst.rs = strtok(current, " \t\n,");
    if (!strcmp(inst.rs, "$imm")) {
        inst.is_imm = true;
    }
    current = current + strlen(current) + 1;
    if (current >= line_end)
        return inst;

    // extract rt
    inst.rt = strtok(current, " \t\n,");
    if (!strcmp(inst.rt, "$imm")) {
        inst.is_imm = true;
    }
    current = current + strlen(current) + 1;
    if (current >= line_end)
        return inst;

    // extract imm
    inst.imm = strtok(current, " \t\n,");
    return inst;
}

line_type find_line_type(instruction inst) {
    if ((inst.label != NULL && inst.label[0] == '#') || (inst.label == NULL && inst.opcode == NULL)) {
        return EMPTY;
    }

    else if (inst.label != NULL && inst.opcode == NULL && strcmp(inst.label, ".word") != 0) {
        return LABEL;

    }

    else if (inst.label != NULL && strcmp(inst.label, ".word") == 0) { 
        return WORD;
    }

    else {
        return REGULAR;
    }
}

// match labels to address in memory 
void first_pass(FILE* file) {
    int               cur_mem_loc = 0;
    line              current_line = { 0 };
    instruction       current_inst;
    line_type         lt;

    while (fgets(current_line, MAX_LINE_LENGTH, file) != NULL) {
        current_inst = parser(current_line);
        lt = find_line_type(current_inst);

        if (lt == LABEL) {
            strcpy(labels[labels_amount].label_name, current_inst.label);
            labels[labels_amount].location = cur_mem_loc - labels_amount;
            labels_amount++;
            cur_mem_loc++;
        }

        else if (lt == REGULAR) {
            if (current_inst.is_imm == true) {
                cur_mem_loc = cur_mem_loc + 2;
            }
            else {
                cur_mem_loc++;
            }
        }
    }
}

/*
replace any location that there is a label in the immediate field
with the address of the label computed during the first pass
*/
int second_pass(FILE* file) {
    int               address;
    int               data;
    int               program_length = 0;
    int               cur_mem_loc = 0;

    int               opcode;
    int               rd;
    int               rs;
    int               rt;
    int               imm;

    line              current_line = { 0 };
    instruction       current_inst;
    line_type         lt;

    fseek(file, 0, SEEK_SET);
    while (fgets(current_line, MAX_LINE_LENGTH, file) != NULL) {
        current_inst = parser(current_line);
        lt = find_line_type(current_inst);

        if (lt == WORD) {
            if (current_inst.opcode[0] == '0' && (current_inst.opcode[1] == 'X') || current_inst.opcode[1] == 'x') {
                address = hex_to_int(current_inst.opcode);
            }
            else {
                address = atoi(current_inst.opcode);
            }

            if (current_inst.rd[0] == '0' && (current_inst.rd[1] == 'X') || current_inst.rd[1] == 'x') {
                data = hex_to_int(current_inst.rd);
            }
            else {
                data = atoi(current_inst.rd);
            }

            memory[address] = data;
            if (address >= program_length)
                program_length = address + 1;
        }

        else if (lt == REGULAR) {
            opcode = encode_opcode(current_inst.opcode);
            rd = encode_reg(current_inst.rd);
            rs = encode_reg(current_inst.rs);
            rt = encode_reg(current_inst.rt);
            memory[cur_mem_loc] = encode_instruction(opcode, rd, rs, rt);
            cur_mem_loc++;
        }

        if (current_inst.is_imm == true) {
            imm = encode_imm(current_inst.imm);
            memory[cur_mem_loc] = imm;
            cur_mem_loc++;
        }
    }
    if (cur_mem_loc > program_length) {
        program_length = cur_mem_loc;
    }
    return program_length;
}

// convert operand string to opcode 
int encode_opcode(char* opcode) {
    char      temp[MAX_LINE_LENGTH];

    strcpy(temp, opcode);
    for (int i = 0; temp[i] != '\0'; i++) {
        temp[i] = tolower(temp[i]);
    }

    if (!strcmp(temp, "add"))
        return 0;
    else if (!strcmp(temp, "sub"))
        return 1;
    else if (!strcmp(temp, "mul"))
        return 2;
    else if (!strcmp(temp, "and"))
        return 3;
    else if (!strcmp(temp, "or"))
        return 4;
    else if (!strcmp(temp, "xor"))
        return 5;
    else if (!strcmp(temp, "sll"))
        return 6;
    else if (!strcmp(temp, "sra"))
        return 7;
    else if (!strcmp(temp, "srl"))
        return 8;
    else if (!strcmp(temp, "beq"))
        return 9;
    else if (!strcmp(temp, "bne"))
        return 10;
    else if (!strcmp(temp, "blt"))
        return 11;
    else if (!strcmp(temp, "bgt"))
        return 12;
    else if (!strcmp(temp, "ble"))
        return 13;
    else if (!strcmp(temp, "bge"))
        return 14;
    else if (!strcmp(temp, "jal"))
        return 15;
    else if (!strcmp(temp, "lw"))
        return 16;
    else if (!strcmp(temp, "sw"))
        return 17;
    else if (!strcmp(temp, "reti"))
        return 18;
    else if (!strcmp(temp, "in"))
        return 19;
    else if (!strcmp(temp, "out"))
        return 20;
    else if (!strcmp(temp, "halt"))
        return 21;
    return -1;
}

// convert register name string to int 
int encode_reg(char* reg) {
    char      temp[MAX_LINE_LENGTH];

    strcpy(temp, reg);
    for (int i = 0; temp[i] != '\0'; i++) {
        temp[i] = tolower(temp[i]);
    }

    if (reg == NULL)
        return 0;
    else if (!strcmp(temp, "$zero"))
        return 0;
    else if (!strcmp(temp, "$imm"))
        return 1;
    else if (!strcmp(temp, "$v0"))
        return 2;
    else if (!strcmp(temp, "$a0"))
        return 3;
    else if (!strcmp(temp, "$a1"))
        return 4;
    else if (!strcmp(temp, "$a2"))
        return 5;
    else if (!strcmp(temp, "$a3"))
        return 6;
    else if (!strcmp(temp, "$t0"))
        return 7;
    else if (!strcmp(temp, "$t1"))
        return 8;
    else if (!strcmp(temp, "$t2"))
        return 9;
    else if (!strcmp(temp, "$s0"))
        return 10;
    else if (!strcmp(temp, "$s1"))
        return 11;
    else if (!strcmp(temp, "$s2"))
        return 12;
    else if (!strcmp(temp, "$gp"))
        return 13;
    else if (!strcmp(temp, "$sp"))
        return 14;
    else if (!strcmp(temp, "$ra"))
        return 15;
    return atoi(temp);
}

// handle imm 
int encode_imm(char* imm) {
    int       imm_final = 0;

    // if imm is string 
    if (isalpha(imm[0])) {
        for (int i = 0; i < labels_amount; i++) {
            if (strcmp(imm, labels[i].label_name) == 0) {
                return labels[i].location;
            }
        }
    }
    
    // if imm is in hexadecimal
    if (imm[0] == '0' && (imm[1] == 'x' || imm[1] == 'X')) {
        imm_final = hex_to_int(imm);
    }
    else {
        imm_final = atoi(imm);
    }

    // if imm is negative- prevent overflow (imm_final is 32 bits - int and imm reg with is 20 bit sized)
    if (imm_final < 0) {
        return imm_final + 1048575 + 1;
    }
    else {
        return imm_final;
    }
}

// create the opcode 
int encode_instruction(int opcode, int rd, int rs, int rt) {
    return ((opcode << 12) + (rd << 8) + (rs << 4) + (rt));
}

// execute the assembler 
void execute(FILE* input_file, FILE* output_file) {
    first_pass(input_file);
    int program_length = second_pass(input_file);

    // convert intruction into hexadecimal 5 digits
    for (int i = 0; i < program_length; i++) {
        fprintf(output_file, "%05X", memory[i]);
        fwrite("\n", 1, 1, output_file);
    }
}
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
#define DISK_SIZE 16384
#define INSTRUCTION_SIZE 5
#define MONITOR_SIZE 256

// Structs
typedef struct {
    char      instruction[9];
    int       opcode;
    int       rd;
    int       rs;
    int       rt;
    int       imm;
} command;

typedef struct {
    FILE* memin;
    FILE* diskin;
    FILE* irq2in;
} inputs;

typedef struct {
    FILE* trace;
    FILE* cycles;
    FILE* memout;
    FILE* regout;
    FILE* leds;
    FILE* diskout;
    FILE* display7seg;
    FILE* hwregtrace;
    FILE* monitortxt;
} outputs;

// Global Variables
static char         memin_arr[MEMORY_SIZE][INSTRUCTION_SIZE + 1];
static char         diskout_arr[DISK_SIZE][INSTRUCTION_SIZE + 1];
static int          monitor_arr[MONITOR_SIZE][MONITOR_SIZE];
static int          reg_arr[16];

static int          pc = 0;
static int          reset_timer = 0;

static int          irq = 0;
static int          irq_ready = 1;

int                 memout_end = MEMORY_SIZE;
int                 irq2_current = 0;
int                 irq2_interruptions[MEMORY_SIZE];
int                 irq2_size = 0;
bool                execute_inst = true;

int                 memin_size = 0;
int                 flag_interrupt = 0;
int                 hd_timer = 0;
int                 irq1_active = 0;
char                io_register[23][9] = { "00000000","00000000","00000000","00000000","00000000","00000000","00000000",
                                            "00000000","00000000","00000000","00000000","00000000","00000000","00000000",
                                            "00000000","00000000","00000000","00000000","00000000","00000000","00000000",
                                            "00000000","00000000" };

// Functions' Declrations
void        fill_memin_arr(FILE* memin);
void        fill_irq2_arr(FILE* irq2in);
void        fill_diskout_arr(FILE* diskin);

command* fetch_instruction(int pc, command* cmd);
void        run(outputs out);
void        display(FILE* display7seg);
void        execute(command* cmd, outputs output_files);
void        execute_interrupt();
void        trace(command* cmd, FILE* trace);

void        hwregtrace(FILE* phwregtrace, int rw, int reg_num);
void        leds(FILE* leds);

void        write_outputs(outputs output_flies);
void        write_monitor(FILE* monitor);
void        write_regout(FILE* regout);
void        write_memout(FILE* memout);
void        write_diskout(FILE* diskout);

void        update_monitor(int data, int addr);
void        check_irq_status();
void        update(command* cmd);


inputs      open_inputs(char* argv[]);
outputs     open_outputs(char* argv[]);
void        close_outputs(outputs output_files);


// Main Function
int main(int argc, char* argv[]) {

    inputs        inputs_files = open_inputs(argc, argv);
    outputs       output_files = open_outputs(argc, argv);

    fill_diskout_arr(inputs_files.diskin);
    fill_irq2_arr(inputs_files.irq2in);
    fill_memin_arr(inputs_files.memin);
    run(output_files);

    write_outputs(output_files);
    close_outputs(output_files);
    exit(0);
}


// Functions

// run the fetch-decode-execute loop and check if can execute an interrupt
void run(outputs out) {

    command         cmd;

    while (pc < memin_size) {

        check_irq_status();
        if (irq && irq_ready) {
            execute_interrupt();
        }

        fetch_instruction(pc, &cmd);

        trace(&cmd, out.trace);
        execute(&cmd, out);
        update(&cmd);


    }
}

// inputs are the current pc and a command pointer, return the fiiled command by its filed and update pc
command* fetch_instruction(int pc, command* cmd) {

    char* command_line = memin_arr[pc];

    strcpy(cmd->instruction, command_line);

    cmd->opcode = (int)strtol((char[]) { command_line[0], command_line[1], 0 }, NULL, 16);
    cmd->rd = (int)strtol((char[]) { command_line[2], 0 }, NULL, 16);
    cmd->rs = (int)strtol((char[]) { command_line[3], 0 }, NULL, 16);
    cmd->rt = (int)strtol((char[]) { command_line[4], 0 }, NULL, 16);

    if (cmd->rd == 1 || cmd->rs == 1 || cmd->rt == 1) {

        command_line = memin_arr[pc + 1];
        cmd->imm = (int)strtol(command_line, NULL, 16);
        // if imm is bigger than 524288, use substruction to prevent overflow (int is 32 bits and imm register is 20 bits)
        if (cmd->imm >= 524288) {
            cmd->imm -= 1048576;
        }
    }

    // update cycles during fetch (access to memory)
    // if reaching 0xffffffff, clock rolls back to 0, otherwise, increase by 1 
    if (hex_to_int(io_register[8]) == hex_to_int("FFFFFFFF")) {
        int_to_hex(0, io_register[8]);
    }
    else {
        int_to_hex((hex_to_int(io_register[8]) + 1), io_register[8]);
    }

    return cmd;
}

// update the clock register according to the executed instruction 
void update_clk(command* cmd) {

    if (cmd->rd == 1 || cmd->rs == 1 || cmd->rt == 1) {
        if (hex_to_int(io_register[8]) == hex_to_int("FFFFFFFF")) {
            int_to_hex(0, io_register[8]);
        }
        else {
            int_to_hex((hex_to_int(io_register[8]) + 1), io_register[8]);
        }
    }

    if (cmd->opcode == 16 || cmd->opcode == 17) {
        if (hex_to_int(io_register[8]) == hex_to_int("FFFFFFFF")) {
            int_to_hex(0, io_register[8]);
        }
        else {
            int_to_hex((hex_to_int(io_register[8]) + 1), io_register[8]);
        }
    }
}

// update the irq_flags of irq1, irq2 and update irq to be 1 or 0 
void check_irq_status() {

    if (hex_to_int(io_register[1]) && hex_to_int(io_register[4]) && irq_ready) {
        irq1_active = 1;
    }

    /*
    turn irq2status on if the cycles is at most bigger by 3 than the cycle of external
    line interrupt. (3 is the maximum cycles for instruction)
    so in the next cycle the interrupt will be executed
    */
    if ((irq2_interruptions[irq2_current] <= hex_to_int(io_register[8]))
        && hex_to_int(io_register[8]) <= irq2_interruptions[irq2_size] + 3 && irq_ready) {
        int_to_hex(1, io_register[5]);
        irq2_current++;
    }

    irq = ((hex_to_int(io_register[0]) && hex_to_int(io_register[3])) || ((hex_to_int(io_register[1]))
        && hex_to_int(io_register[4])) || (hex_to_int(io_register[2]) && hex_to_int(io_register[5]))) ? 1 : 0;
}

// write all output files
void write_outputs(outputs output_flies) {
    fprintf(output_flies.cycles, "%d", hex_to_int(io_register[8]));
    write_regout(output_flies.regout);
    write_memout(output_flies.memout);
    write_diskout(output_flies.diskout);
    write_monitor(output_flies.monitortxt);
}

// write to regout.txt from register array reg_arr
void write_regout(FILE* regout) {
    for (int i = 2; i < 16; i++) {
        fprintf(regout, "%08X\n", reg_arr[i]);
    }
}

// write to memout.txt from memory array - memin_arr
void write_memout(FILE* memout) {
    for (int i = 0; i < memin_size; i++) {
        fprintf(memout, "%s\n", memin_arr[i]);
    }
}

// write to memout.txt from disk array - diskout_arr
void write_diskout(FILE* diskout) {
    for (int i = 0; i < DISK_SIZE; i++) {
        fprintf(diskout, "%s\n", diskout_arr[i]);
    }
}

// write to monitor.txt from monitor array - monitor_arr
void write_monitor(FILE* monitortxt) {
    char        temp[3];
    char        byte;

    for (int i = 0; i < MONITOR_SIZE; i++) {
        for (int j = 0; j < MONITOR_SIZE; j++) {
            if (monitor_arr[i][j] > 15) {
                sprintf(temp, "%0X", monitor_arr[i][j]);
            }
            else {
                sprintf(temp, "0%0X", monitor_arr[i][j]);
            }
            byte = monitor_arr[i][j];
            fputs(temp, monitortxt);
            fputc('\n', monitortxt);
        }
    }
}

// read from irq2in.txt to irq2_interruptions array and close .txt file 
void fill_irq2_arr(FILE* irq2in) {
    for (int k = 0; k < MEMORY_SIZE; k++) {
        irq2_interruptions[k] = -1;
    }

    char        line[6];

    irq2_size = 0;
    while (!feof(irq2in))
    {
        fgets(line, 10, irq2in);
        irq2_interruptions[irq2_size] = atoi(line);
        irq2_size++;
    }

    irq2_size--;

    fclose(irq2in);
}

// read from diskin.txt to diskout array and close .txt file 
void fill_diskout_arr(FILE* diskin) {
    char      line[INSTRUCTION_SIZE + 1];
    int       i = 0;

    while (!feof(diskin)) {
        fscanf(diskin, "%5[^\n]\n", line);
        strcpy(diskout_arr[i], line);
        i++;
    }

    // fill empty registers in dis with 00000 (20 bits)
    while (i < DISK_SIZE) {
        strcpy(diskout_arr[i], "00000");
        i++;
    }
    fclose(diskin);
}

// read from memin.txt to memory array and close .txt file 
void fill_memin_arr(FILE* memin) {
    char      line[INSTRUCTION_SIZE + 1];
    int       i = 0;
    while (fscanf(memin, "%5[^\n]\n", line) == 1) {

        strcpy(memin_arr[i], line);
        i++;
    }
    fclose(memin);

    memin_size = i;

    while (i < MEMORY_SIZE) {
        strcpy(memin_arr[i], "00000");
        i++;
    }
    fclose(memin);
}

// update to cycles timer of hard disk and check if disk interruptions ends at cycle 1024 
void update_hard_disk_cycles() {
    if (irq1_active) {
        if (hd_timer < 1024) {
            hd_timer++;
        }
        else {
            hd_timer = 0;
            irq1_active = 0;
            int_to_hex(0, io_register[14]);
            int_to_hex(0, io_register[17]);
            int_to_hex(1, io_register[4]);
        }
    }
}

// update timer 
void update_timer() {
    // time current != time max, increase time current by 1 
    if (hex_to_int(io_register[11]) == 1) {
        if (hex_to_int(io_register[12]) < hex_to_int(io_register[13])) {
            int_to_hex((hex_to_int(io_register[12]) + 1), io_register[12]);
        }
    }

    // timecurrect = timemax
    if ((hex_to_int(io_register[12]) == hex_to_int(io_register[13])) && ((hex_to_int(io_register[13])) != 0)) {
        int_to_hex(1, io_register[3]);
        int_to_hex(0, io_register[12]);
    }
}

// handle write or read in the hard disk 
void hard_disk(char diskout_arr[DISK_SIZE][INSTRUCTION_SIZE + 1]) {
    int         sector = hex_to_int(io_register[15]) * 128;
    int         buffer = hex_to_int(io_register[16]);

    if (!hex_to_int(io_register[17])) {
        switch (hex_to_int(io_register[14])) {
            // no command 
        case 0:
            break;
            // read from hard disk to memory
        case 1:
            int_to_hex(1, io_register[17]);
            irq1_active = 1;
            for (int i = sector; (i < sector + 128); i++) {
                if (buffer > MEMORY_SIZE) {
                    printf("Can't write to memory - Buffer address is larger than memory size.");
                    break;
                }
                strcpy(memin_arr[buffer], diskout_arr[i]);
                memin_size = max(memin_size, buffer);
                buffer++;
            }

            break;

            // write to hard disk from memory
        case 2:
            irq1_active = 1;
            int_to_hex(1, io_register[17]);
            for (int i = sector; i < sector + 128; i++) {
                if (buffer >= MEMORY_SIZE) {
                    printf("Can't read from memory - Buffer address is larger than memory size.");
                    break;
                }
                strcpy(diskout_arr[i], memin_arr[buffer]);
                buffer++;
            }
            break;
        }
    }
}

// execute instruction
void execute(command* cmd, outputs output_files) {

    char      hex_num[9];
    char      hex_num_temp[9] = "00000000";

    // check if imm is used in current line (pc), store value in imm register and update pc
    if (cmd->rd == 1) {
        reg_arr[cmd->rd] = cmd->imm;
        reg_arr[1] = cmd->imm;
        pc = pc + 1;

    }
    if (cmd->rs == 1) {
        reg_arr[cmd->rs] = cmd->imm;
        reg_arr[1] = cmd->imm;
        pc = pc + 1;
    }
    if (cmd->rt == 1) {
        reg_arr[cmd->rt] = cmd->imm;
        reg_arr[1] = cmd->imm;
        pc = pc + 1;
    }

    // execute command by opcode
    switch (cmd->opcode) {
        // ADD
    case 0:
        if (cmd->rd != 0 && cmd->rd != 1) {
            reg_arr[cmd->rd] = reg_arr[cmd->rs] + reg_arr[cmd->rt];
        }
        pc++;
        break;

        // SUB
    case 1:
        if (cmd->rd != 0 && cmd->rd != 1) {
            reg_arr[cmd->rd] = reg_arr[cmd->rs] - reg_arr[cmd->rt];
        }
        pc++;
        break;

        // MUL
    case 2:
        if (cmd->rd != 0 && cmd->rd != 1) {
            reg_arr[cmd->rd] = reg_arr[cmd->rs] * reg_arr[cmd->rt];
        }
        pc++;
        break;

        // AND
    case 3:
        if (cmd->rd != 0 && cmd->rd != 1) {
            reg_arr[cmd->rd] = reg_arr[cmd->rs] & reg_arr[cmd->rt];
        }
        pc++;
        break;

        // OR
    case 4:
        if (cmd->rd != 0 && cmd->rd != 1) {
            reg_arr[cmd->rd] = reg_arr[cmd->rs] | reg_arr[cmd->rt];
        }
        pc++;
        break;

        // XOR
    case 5:
        if (cmd->rd != 0 && cmd->rd != 1) {
            reg_arr[cmd->rd] = reg_arr[cmd->rs] ^ reg_arr[cmd->rt];
        }
        pc++;
        break;

        // SLL
    case 6:
        if (cmd->rd != 0 && cmd->rd != 1) {
            reg_arr[cmd->rd] = reg_arr[cmd->rs] << reg_arr[cmd->rt];
        }
        pc++;
        break;

        // SRA
    case 7:
        if (cmd->rd != 0 && cmd->rd != 1) {
            reg_arr[cmd->rd] = reg_arr[cmd->rs] >> reg_arr[cmd->rt];
        }
        pc++;
        break;

        // SRL
    case 8:
        if (cmd->rd != 0 && cmd->rd != 1) {
            reg_arr[cmd->rd] = logic_shift(reg_arr[cmd->rs], reg_arr[cmd->rt]);
        }
        pc++;
        break;

        // BEQ
    case 9:
        if (reg_arr[cmd->rs] == reg_arr[cmd->rt]) {
            if (cmd->rd == 1) {
                reg_arr[cmd->rd] = cmd->imm;
            }
            pc = reg_arr[cmd->rd];
        }
        else
            pc++;
        break;

        // BNE
    case 10:
        if (reg_arr[cmd->rs] != reg_arr[cmd->rt]) {
            if (cmd->rd == 1) {
                reg_arr[cmd->rd] = cmd->imm;
            }
            pc = reg_arr[cmd->rd];
        }
        else {
            pc++;
        }
        break;

        // BLT
    case 11:
        if (reg_arr[cmd->rs] < reg_arr[cmd->rt]) {
            pc = reg_arr[cmd->rd];
        }
        else {
            pc++;
        }
        break;

        // BGT
    case 12:
        if (reg_arr[cmd->rs] > reg_arr[cmd->rt]) {
            pc = reg_arr[cmd->rd];
        }
        else
            pc++;
        break;

        // BLE
    case 13:
        if (reg_arr[cmd->rs] <= reg_arr[cmd->rt]) {
            pc = reg_arr[cmd->rd];
        }
        else {
            pc++;
        }
        break;

        // BGE
    case 14:
        if (reg_arr[cmd->rs] >= reg_arr[cmd->rt]) {
            pc = reg_arr[cmd->rd];
        }
        else {
            pc++;
        }
        break;

        // JAL
    case 15:
        reg_arr[cmd->rd] = pc + 1;
        pc = reg_arr[cmd->rs];
        break;

        // LW
    case 16:
        if (cmd->rd != 0 && cmd->rd != 1) {
            reg_arr[cmd->rd] = hex_to_int(memin_arr[(reg_arr[cmd->rs]) + reg_arr[cmd->rt]]);
        }
        pc++;
        break;

        // SW
    case 17:
        int_to_hex_5_dig(reg_arr[cmd->rd], hex_num_temp);
        strcpy(memin_arr[(reg_arr[cmd->rs] + reg_arr[cmd->rt])], hex_num_temp);
        memin_size = max(memin_size, (reg_arr[cmd->rs] + reg_arr[cmd->rt]));

        pc++;
        break;

        // RETI
    case 18:
        pc = hex_to_int(io_register[7]);
        irq_ready = 1;
        break;

        // IN
    case 19:
        reg_arr[cmd->rd] = hex_to_int(io_register[reg_arr[cmd->rs] + reg_arr[cmd->rt]]);
        pc++;
        hwregtrace(output_files.hwregtrace, 1, reg_arr[cmd->rs] + reg_arr[cmd->rt]);
        break;

        // OUT
    case 20:
        int_to_hex(reg_arr[cmd->rd], io_register[reg_arr[cmd->rs] + reg_arr[cmd->rt]]);
        switch (reg_arr[cmd->rs] + reg_arr[cmd->rt]) {

            // leds
        case 9:
            leds(output_files.leds);
            break;

            // display7seg
        case 10:
            display(output_files.display7seg);
            break;

            // diskcmd
        case 14:
            hard_disk(diskout_arr);
            break;

            // monitor
        case 22:
            if (hex_to_int(io_register[22]) == 1) {
                update_monitor(hex_to_int(io_register[21]), hex_to_int(io_register[20]));
                break;
            }
        }
        hwregtrace(output_files.hwregtrace, 0, reg_arr[cmd->rs] + reg_arr[cmd->rt]);

        pc++;
        break;

        // HALT
    case 21:
        update_clk(cmd);
        update(&cmd);
        write_outputs(output_files);
        close_outputs(output_files);
        exit(0);
        break;
    }

    // update clock cycles after execution
    update_clk(cmd);

}

// update monitor data in given address addr
void update_monitor(int data, int addr) {
    monitor_arr[addr / 256][addr % 256] = data;
}

// open all input files
inputs open_inputs(int argc, char* argv[]) {
    inputs        input_files = { 0 };

    input_files.memin = fopen(argv[1], "r");
    input_files.diskin = fopen(argv[2], "r");
    input_files.irq2in = fopen(argv[3], "r");

    if (input_files.memin == NULL || input_files.diskin == NULL || input_files.irq2in == NULL) {
        printf("One of the input files could not be open\n");
        exit(1);
    }
    return input_files;
}

// open all output files 
outputs open_outputs(int argc, char* argv[]) {
    outputs         output_files = { 0 };

    output_files.memout = fopen(argv[4], "w");
    output_files.regout = fopen(argv[5], "w");

    output_files.trace = fopen(argv[6], "w");
    output_files.hwregtrace = fopen(argv[7], "w");
    output_files.cycles = fopen(argv[8], "w");
    output_files.leds = fopen(argv[9], "w");
    output_files.display7seg = fopen(argv[10], "w");
    output_files.diskout = fopen(argv[11], "w");
    output_files.monitortxt = fopen(argv[12], "w");

    if (output_files.memout == NULL || output_files.regout == NULL || output_files.trace == NULL
        || output_files.cycles == NULL || output_files.hwregtrace == NULL || output_files.leds == NULL
        || output_files.display7seg == NULL || output_files.diskout == NULL || output_files.monitortxt == NULL) {

        printf("One of the output files could not be open\n");
        exit(1);
    }

    return output_files;
}

// close all outputs files
void close_outputs(outputs output_files) {
    fclose(output_files.hwregtrace);
    fclose(output_files.leds);
    fclose(output_files.display7seg);
    fclose(output_files.diskout);
    fclose(output_files.memout);
    fclose(output_files.regout);
    fclose(output_files.trace);
    fclose(output_files.cycles);
    fclose(output_files.monitortxt);
}

// write to leds
void leds(FILE* leds) {
    fprintf(leds, "%d %s\n", hex_to_int(io_register[8]), io_register[9]);
}

// write to display7seg
void display(FILE* display7seg) {
    fprintf(display7seg, "%d %s\n", hex_to_int(io_register[8]), io_register[10]);

}

// update hard disk cycles and timer
void update(command* cmd) {
    update_hard_disk_cycles();
    update_timer();
}

/*
execute an interruptand save the current pc in
irqreturn and update the pc to value in irqhandler registwer
*/
void execute_interrupt() {
    int_to_hex(pc, io_register[7]);
    pc = hex_to_int(io_register[6]);
    irq_ready = 0;
}

// write current state to trace.txt
void trace(command* cmd, FILE* trace) {
    char pc_trace[9], inst[9], r0[9], r1[9], r2[9], r3[9], r4[9], r5[9],
        r6[9], r7[9], r8[9], r9[9], r10[9], r11[9], r12[9], r13[9], r14[9], r15[9];
    char hex_num[9];

    // pc contain 3 hexadecimal digits
    int_to_hex_3_dig(pc, hex_num);
    strcpy(pc_trace, hex_num);

    // instruction already contains 5 hexadecimal digits
    strcpy(inst, cmd->instruction);

    // reg_arr[0] cannot change during simulator and remains with the intialized value "00000000"
    int_to_hex(reg_arr[0], hex_num);
    strcpy(r0, hex_num);

    // if imm is used - copy its value, else wrtie 0 to trace for imm 
    if (cmd->rd == 1 || cmd->rs == 1 || cmd->rt == 1) {
        int_to_hex(cmd->imm, hex_num);
        strcpy(r1, hex_num);
    }
    else {
        int_to_hex(0, hex_num);
        strcpy(r1, hex_num);
    }


    int_to_hex(reg_arr[2], hex_num);
    strcpy(r2, hex_num);

    int_to_hex(reg_arr[3], hex_num);
    strcpy(r3, hex_num);

    int_to_hex(reg_arr[4], hex_num);
    strcpy(r4, hex_num);

    int_to_hex(reg_arr[5], hex_num);
    strcpy(r5, hex_num);

    int_to_hex(reg_arr[6], hex_num);
    strcpy(r6, hex_num);

    int_to_hex(reg_arr[7], hex_num);
    strcpy(r7, hex_num);

    int_to_hex(reg_arr[8], hex_num);
    strcpy(r8, hex_num);

    int_to_hex(reg_arr[9], hex_num);
    strcpy(r9, hex_num);

    int_to_hex(reg_arr[10], hex_num);
    strcpy(r10, hex_num);

    int_to_hex(reg_arr[11], hex_num);
    strcpy(r11, hex_num);

    int_to_hex(reg_arr[12], hex_num);
    strcpy(r12, hex_num);

    int_to_hex(reg_arr[13], hex_num);
    strcpy(r13, hex_num);

    int_to_hex(reg_arr[14], hex_num);
    strcpy(r14, hex_num);

    int_to_hex(reg_arr[15], hex_num);
    strcpy(r15, hex_num);

    fprintf(trace, "%s %s %s %s %s %s %s %s %s %s %s %s %s %s %s %s %s %s\n",
        pc_trace, inst, r0, r1, r2, r3, r4, r5, r6, r7, r8, r9, r10, r11, r12,
        r13, r14, r15);
}

void hwregtrace(FILE* hwregtrace, int rw, int reg_num) {
    char name[50] = "";
    switch (reg_num) {
    case 0:
        strcpy(name, "irq0enable");
        break;
    case 1:
        strcpy(name, "irq1enable");
        break;
    case 2:
        strcpy(name, "irq2enable");
        break;

    case 3:
        strcpy(name, "irq0status");
        break;

    case 4:
        strcpy(name, "irq1status");
        break;

    case 5:
        strcpy(name, "irq2status");
        break;

    case 6:
        strcpy(name, "irqhandler");
        break;

    case 7:
        strcpy(name, "irqreturn");
        break;

    case 8:
        strcpy(name, "clks");

        break;

    case 9:
        strcpy(name, "leds");
        break;

    case 10:
        strcpy(name, "display7seg");
        break;

    case 11:
        strcpy(name, "timerenable");
        break;

    case 12:
        strcpy(name, "timercurrent");
        break;

    case 13:
        strcpy(name, "timermax");
        break;

    case 14:
        strcpy(name, "diskcmd");
        break;

    case 15:
        strcpy(name, "disksector");
        break;

    case 16:
        strcpy(name, "diskbuffer");
        break;

    case 17:
        strcpy(name, "diskstatus");
        break;

    case 18:
        strcpy(name, "reserved");
        break;

    case 19:
        strcpy(name, "reserved");
        break;

    case 20:
        strcpy(name, "monitoraddr");
        break;

    case 21:
        strcpy(name, "monitordata");
        break;

    case 22:
        strcpy(name, "monitorcmd");
        break;
    }

    // if rw is 1 - log READ command (in) , else WRITE command (out)
    if (rw) {
        fprintf(hwregtrace, "%d READ %s %s\n", hex_to_int(io_register[8]), name, io_register[reg_num]);
    }
    else {
        fprintf(hwregtrace, "%d WRITE %s %s\n", hex_to_int(io_register[8]), name, io_register[reg_num]);
    }
}
#define _CRT_SECURE_NO_WARNINGS

// Imports
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdbool.h>
#include <ctype.h>

// Functions
int logic_shift(int num, int shift) {
    int        mask = 0x7FFFFFF;

    for (int i = 0; i < shift; i++) {
        num = (num >> 1) & mask;
    }
    return num;
}

// convert hex letter to int
int hex_char_to_int(char letter) {
    short result;
    switch (letter) {
    case 'A':
        result = 10;
        break;
    case 'B':
        result = 11;
        break;
    case 'C':
        result = 12;
        break;
    case 'D':
        result = 13;
        break;
    case 'E':
        result = 14;
        break;
    case 'F':
        result = 15;
        break;
    case 'a':
        result = 10;
        break;
    case 'b':
        result = 11;
        break;
    case 'c':
        result = 12;
        break;
    case 'd':
        result = 13;
        break;
    case 'e':
        result = 14;
        break;
    case 'f':
        result = 15;
        break;
    default:
        result = atoi(&letter);
        break;
    }
    return result;
}

// convert hexadecimal string to int 
int hex_to_int(char* hex) {
    int       i;
    int       result = 0;
    int       len = strlen(hex);

    for (i = 0; i < len; i++) {
        result += hex_char_to_int(hex[len - 1 - i]) * (1 << (4 * i));
    }
    if ((len <= 4) && (result & (1 << (len * 4 - 1)))) {
        result |= -1 * (1 << (len * 4));
    }
    return result;
}

// convert int to hexadecimal string with 8 chars
void int_to_hex(int num, char hex[9]) {
    sprintf(hex, "%08X", num);
}

// convert int to hexadecimal string with 3 chars
void int_to_hex_3_dig(int num, char hex[9]) {
    sprintf(hex, "%03X", num);
}

// convert int to hexadecimal string with 5 chars
void int_to_hex_5_dig(int num, char hex[9]) {
    sprintf(hex, "%05X", num);
}
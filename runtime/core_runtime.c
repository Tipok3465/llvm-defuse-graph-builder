// Core runtime library for def-use graph instrumentation
#include <stdio.h>
#include <stdlib.h>
#include <string.h>// TODO[Dkay]: my LSP says that this header is unused. Pls, setup yours too 

// Basic print functions for instrumentation
void print_i32_with_id(int value, const char* node_id, const char* name) {
    if (node_id && node_id[0]) {
        printf("%s:%d\n", node_id, value);
        return;
    }
    if (name && name[0]) {
        printf("%s:%d\n", name, value);
    }
}

void print_i64_with_id(long long value, const char* node_id, const char* name) {
    if (node_id && node_id[0]) {
        printf("%s:%lld\n", node_id, value);
        return;
    }
    if (name && name[0]) {
        printf("%s:%lld\n", name, value);
    }
}

void print_float_with_id(float value, const char* node_id, const char* name) {
    if (node_id && node_id[0]) {
        printf("%s:%f\n", node_id, value);
        return;
    }
    if (name && name[0]) {
        printf("%s:%f\n", name, value);
    }
}

void print_double_with_id(double value, const char* node_id, const char* name) {
    if (node_id && node_id[0]) {
        printf("%s:%lf\n", node_id, value);
        return;
    }
    if (name && name[0]) {
        printf("%s:%lf\n", name, value);
    }
}



// Utility functions
void log_instruction(const char* func_name, const char* instr_name, long long value) {
    printf("%s::%s:%lld\n", func_name, instr_name, value);
}

void log_argument(const char* func_name, const char* arg_name, long long value) {
    printf("%s::%s:%lld\n", func_name, arg_name, value);
}

void log_constant(const char* const_value, long long actual_value) {
    printf("%s:%lld\n", const_value, actual_value);
}

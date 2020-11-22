#include <stdint.h>

uint8_t memory[100];
extern int callback(int in_len);

int call_function(const char* function_name, void *argument, int argument_len){
    for (int i = 0; i < 4; i ++) {
        memory[i] = function_name[i];
    }
    for (int i = 0; i < argument_len; i ++) {
        memory[4+i] = ((char *) argument)[i];
    }
    return callback(argument_len);
}

int run(int arglen) {
    //argument
    uint32_t *buffer = (uint32_t *) memory;
    if (arglen != 12) return 0;

    //return
    buffer[0] = ((buffer[0] << 2U) ^ (buffer[1] << 4U) ^ buffer[2]) % 1000;
    return 4;
}

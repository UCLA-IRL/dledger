#include <stdint.h>

uint8_t callback_buffer[100];
extern int callback(int in_len);

// Function to return a pointer to our buffer
unsigned char* callback_buffer_offset() {
    return callback_buffer;
}

int call_function(const char* function_name, void *argument, int argument_len){
    for (int i = 0; i < 4; i ++) {
        callback_buffer[i] = function_name[i];
    }
    for (int i = 0; i < argument_len; i ++) {
        callback_buffer[4+i] = ((char *) argument)[i];
    }
    return callback(argument_len);
}

int run(int arglen) {
    //argument
    uint32_t *buffer = (uint32_t *) callback_buffer;
    if (arglen != 12) return 0;

    //return
    buffer[0] = (buffer[0] + buffer[1] + buffer[2]) % 1000;
    return 4;
}

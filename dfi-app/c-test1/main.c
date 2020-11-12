#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

uint32_t read_host_argument(void** return_buffer){
    // get return
    uint32_t return_len;
    fread(&return_len, 4, 1, stdin);
    *return_buffer = malloc(return_len);
    if (*return_buffer == NULL) {
        fprintf(stderr, "Error: not enough memory for wasm");
    }
    fread(*return_buffer, 1, return_len, stdin);
    return return_len;
}

uint32_t call_host(const char *func_name, const void* argument, uint32_t argument_len,
              void** return_buffer) {
    // call
    fwrite(func_name, 1, 4, stdout);
    fwrite(&argument_len, 4, 1, stdout);
    fwrite(argument, 1, argument_len, stdout);
    fflush(stdout);

    return read_host_argument(return_buffer);
}

void return_to_host(const void* argument, uint32_t argument_len) {
    // call
    fwrite("DONE", 1, 4, stdout);
    fwrite(&argument_len, 4, 1, stdout);
    fwrite(argument, 1, argument_len, stdout);
    fflush(stdout);
}

int main() {
    fprintf(stderr, "ENTER c wasi module\n");
    //argument
    char *buffer;
    int buffer_len = read_host_argument((void **) &buffer);
    buffer = realloc(buffer, buffer_len + 10);
    buffer[buffer_len] = '\0';
    fprintf(stderr, "WASM C get: %s\n", buffer);
    free(buffer);

    //call callback
    const char *argument = "Argument from C WASM";
    buffer_len = call_host("GETB", argument, strlen(argument), (void **) &buffer);
    buffer = realloc(buffer, buffer_len + 10);
    buffer[buffer_len] = '\0';
    fprintf(stderr, "WASM C getblock return: %s\n", buffer);
    free(buffer);

    //return
    const char *message = "Return from C WASM";
    return_to_host(message, strlen(message));
}

#include "controller.h"
#include <string.h>
#include <stdlib.h>
#include <ctype.h>

char *handle_input(ShmBuf *shmp, const char *input) {
    if (input == NULL) {
        return strdup("");
    }

    // Trim leading whitespace
    while (isspace(*input)) {
        input++;
    }

    if (strncmp(input, "@msg", 4) == 0 && (isspace(input[4]) || input[4] == '\0')) {
        const char *msg = input + 4;
        while (isspace(*msg)) {
            msg++;
        }
        
        if (*msg != '\0') {
            model_send_message(shmp, msg);
            return strdup("Message sent successfully");
        } else {
            return strdup("Error: Empty message");
        }
    } else {
        return execute_command(input);
    }
}

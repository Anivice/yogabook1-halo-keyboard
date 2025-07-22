#include <stdio.h>
#include <string.h>
#include "ckeyid.h"

void (*emit_key)(unsigned int) = nullptr;

void module_exit()
{
    printf("Exiting...\n");
}

int module_init(void * emit_key_, int register_vec[13])
{
    printf("[FN MOD]: Registering helper functions...");
    emit_key = (void (*)(unsigned int))emit_key_;
    memset(register_vec, 0, 13 * sizeof(int));
    register_vec[4] = 1; // No.4, airplane
    register_vec[8] = 1; // No.8, settings
    printf("done.\n");
    return 0;
}

void fn_key_handler_vector(const unsigned int key_id)
{
    switch (key_id)
    {
    case INVERTED_KEY_AIRPLANEMODE: if (DEBUG) printf("Not implemented\n"); break;
    case INVERTED_KEY_SETTINGS: printf("Settings\n"); break;
    default: break;
    }
}

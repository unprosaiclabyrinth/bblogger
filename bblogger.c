#include "dr_api.h"
#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/stat.h>

static file_t log_file;
static void event_exit(void);
    unsigned long code_vma_start_addr = 0;

// callback function prototype for the basic block event
dr_emit_flags_t log_basic_block(void *drcontext, void *tag, instrlist_t *bb,
                                bool for_trace, bool translating);

DR_EXPORT void dr_init(__attribute__((unused)) client_id_t id) {
    dr_set_client_name("DrBblogger", "https://dynamorio.org");

    log_file = dr_open_file("bbtrace.log", DR_FILE_WRITE_OVERWRITE | DR_FILE_ALLOW_LARGE);
    DR_ASSERT(log_file != INVALID_FILE);

    dr_register_exit_event(event_exit);
    dr_register_bb_event(log_basic_block);
}

static void event_exit(void) {
    dr_close_file(log_file);
}

dr_emit_flags_t log_basic_block(__attribute__((unused)) void *drcontext, void *tag,
                                __attribute__((unused)) instrlist_t *bb,
                                __attribute__((unused)) bool for_trace,
                                __attribute__((unused)) bool translating) {
    char addr_str[64];
    dr_snprintf(addr_str, sizeof(addr_str), "%#lx\n", dr_fragment_app_pc(tag));
    dr_write_file(log_file, addr_str, strlen(addr_str));
    return DR_EMIT_DEFAULT;
}
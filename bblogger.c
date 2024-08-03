#include <dr_api.h>
#include <dr_modules.h>
#include <string.h>

static file_t log_file;
static void event_exit(void);
    unsigned long code_vma_start_addr = 0;

// callback function prototype for the basic block event
dr_emit_flags_t event_app_instruction(void *drcontext, void *tag, instrlist_t *bb,
                                      bool for_trace, bool translating);

void dr_client_main(__attribute__((unused)) client_id_t id,
                    __attribute__((unused)) int argc,
                    __attribute__((unused)) const char *argv[]) {
    dr_set_client_name("DrBblogger", "https://dynamorio.org");

    log_file = dr_open_file("bbtrace.log", DR_FILE_WRITE_OVERWRITE | DR_FILE_ALLOW_LARGE);
    DR_ASSERT(log_file != INVALID_FILE);

    dr_register_exit_event(event_exit);
    dr_register_bb_event(event_app_instruction);
}

dr_emit_flags_t event_app_instruction(__attribute__((unused)) void *drcontext, void *tag,
                                      __attribute__((unused)) instrlist_t *bb,
                                      __attribute__((unused)) bool for_trace,
                                      __attribute__((unused)) bool translating) {
    app_pc start_pc = dr_fragment_app_pc(tag);
    module_data_t *mod = dr_lookup_module(start_pc);
    char addr_str[64];
    if (mod != NULL) {
        ptr_int_t rel_addr = start_pc - mod->start ;
        dr_snprintf(addr_str, sizeof(addr_str), "<%s> + %#lx\n", dr_module_preferred_name(mod), rel_addr);
        dr_write_file(log_file, addr_str, strlen(addr_str));

    }
    dr_free_module_data(mod);
    return DR_EMIT_DEFAULT;
}

static void event_exit(void) {
    dr_close_file(log_file);
}
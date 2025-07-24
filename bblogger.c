#include <dr_api.h>
#include <dr_modules.h>
#include <string.h>

static file_t log_file;
static void event_exit(void);

static void trace_bb(app_pc tag); // clean call @ BB entry

// callback function prototype for the basic block event
static dr_emit_flags_t
event_app_instruction(void *drcontext, void *tag, instrlist_t *bb,
                      bool for_trace, bool translating);

DR_EXPORT void
dr_client_main(__attribute__((unused)) client_id_t id,
               __attribute__((unused)) int argc,
               __attribute__((unused)) const char *argv[]) {
    dr_set_client_name("DrBblogger", "https://dynamorio.org");

    log_file = dr_open_file("bbtrace.log", DR_FILE_WRITE_OVERWRITE | DR_FILE_ALLOW_LARGE);
    DR_ASSERT(log_file != INVALID_FILE);

    dr_register_exit_event(event_exit);
    dr_register_bb_event(event_app_instruction);
}

// called at runtime, once per BB execution
static void trace_bb(app_pc tag) {
    app_pc app_pc = dr_fragment_app_pc(tag);
    module_data_t *mod = dr_lookup_module(app_pc);
    if (mod != NULL) {
        // compute the module-relative PC
        ptr_int_t modrel_pc = (ptr_int_t)(app_pc - mod->start);

        // determine if the basic block is from a binary or a different module (e.g.: a shared library)
        int from_binary = 0;
        #ifdef ALL
            from_binary = 1;
        #else
            from_binary = mod->names.module_name == NULL;
        #endif

        // Log according to filtering and verbosity level
        if (from_binary) {
            #ifdef VERBOSE
                dr_fprintf(log_file, "<%s> + %#lx\n", mod->names.file_name, modrel_pc);
            #else
                dr_fprintf(log_file, "%#lx\n", modrel_pc);
            #endif
        }
    }
    dr_free_module_data(mod);
}

static dr_emit_flags_t
event_app_instruction(void *drcontext, void *tag, instrlist_t *bb,
                      __attribute__((unused)) bool for_trace,
                      __attribute__((unused)) bool translating) {
    // insert a clean call at the top of the block
    dr_insert_clean_call(drcontext, bb, instrlist_first(bb),
                            (void *)trace_bb, false, 1,
                            OPND_CREATE_INTPTR(tag));
    return DR_EMIT_DEFAULT;
}

static void event_exit(void) {
    dr_close_file(log_file);
}
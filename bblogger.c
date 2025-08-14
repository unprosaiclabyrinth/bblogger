#include <dr_api.h>
#include <dr_modules.h>
#include "dr_ir_disassemble.h"
#include <string.h>

static file_t log_file;
static void event_exit(void);
static void trace_bb(app_pc app_pc); // FIX: changed param to actual app_pc instead of tag

// callback function prototype for the basic block event
static dr_emit_flags_t
event_app_instruction(void *drcontext, void *tag, instrlist_t *bb,
                      bool for_trace, bool translating);

/* Cache implementation */
#ifdef VVERBOSE
#define BB_CACHE_TABLE_BITS 10
#define BB_CACHE_TABLE_SIZE (1u << BB_CACHE_TABLE_BITS)

typedef struct bb_data_t {
    app_pc start_pc; // FIX: use actual start PC, not tag
    char *bbstr;
    struct bb_data_t *next;
} bb_data_t;

static bb_data_t **bb_cache_table = NULL;

// simple mix: shift away low bits and mask
static inline uint hash(app_pc start_pc) {
    return (((ptr_int_t)start_pc) >> 4) & (BB_CACHE_TABLE_SIZE - 1);
}

static void init_bb_cache(void) {
    bb_cache_table = dr_global_alloc(BB_CACHE_TABLE_SIZE * sizeof(*bb_cache_table));
    memset(bb_cache_table, 0, BB_CACHE_TABLE_SIZE * sizeof(*bb_cache_table));
}

static void cache_bb_string(app_pc start_pc, char *bbstr) {
    uint idx = hash(start_pc);
    bb_data_t *e = dr_global_alloc(sizeof(*e));
    e->start_pc = start_pc;
    e->bbstr = bbstr;
    e->next = bb_cache_table[idx];
    bb_cache_table[idx] = e;
}

static char *lookup_bb_string(app_pc start_pc) {
    uint idx = hash(start_pc);
    for (bb_data_t *e = bb_cache_table[idx]; e; e = e->next) {
        if (e->start_pc == start_pc)
            return e->bbstr;
    }
    return NULL;
}

static void free_bb_cache(void) {
    for (uint i = 0; i < BB_CACHE_TABLE_SIZE; i++) {
        bb_data_t *e = bb_cache_table[i];
        while (e) {
            bb_data_t *next = e->next;
            dr_global_free(e->bbstr, strlen(e->bbstr) + 1);
            dr_global_free(e, sizeof(*e));
            e = next;
        }
    }
    dr_global_free(bb_cache_table, BB_CACHE_TABLE_SIZE * sizeof(*bb_cache_table));
    bb_cache_table = NULL;
}
#endif /* VVERBOSE */

// runtime: once per BB execution
static void trace_bb(app_pc app_pc) { // FIX: receives actual PC, not tag
    module_data_t *mod = dr_lookup_module(app_pc);
    if (mod) { // FIX: guard against NULL
        ptr_int_t modrel_pc = (ptr_int_t)(app_pc - mod->start);
        const char *modulestr = mod->names.module_name ? mod->names.module_name : mod->names.file_name;

        #ifdef VVERBOSE
            char *bb = lookup_bb_string(app_pc); // FIX: key is start_pc
            if (bb) {
                dr_fprintf(log_file, "=== BB @ <%s> + %#lx ===\n%s\n", modulestr, modrel_pc, bb);
            }
        #else
            #ifdef VERBOSE
                dr_fprintf(log_file, "<%s> + %#lx\n", modulestr, modrel_pc);
            #else
                dr_fprintf(log_file, "%#lx\n", modrel_pc);
            #endif
        #endif
        dr_free_module_data(mod);
    } else {
        // FIX: handle code outside any module
        dr_fprintf(log_file, "<no module> @ %p\n", app_pc);
    }
}

static dr_emit_flags_t
event_app_instruction(void *drcontext, void *tag, instrlist_t *bb,
                      __attribute__((unused)) bool for_trace,
                      __attribute__((unused)) bool translating) {

    // FIX: compute app_pc now so we pass real PC into clean call and cache
    app_pc start_pc = dr_fragment_app_pc(tag);

    #ifdef VVERBOSE
        char local[4096] = {0};
        char *p = local;
        int rem = sizeof(local);
        for (instr_t *insn = instrlist_first_app(bb); insn; insn = instr_get_next_app(insn)) {
            char disas[256];
            instr_disassemble_to_buffer(drcontext, insn, disas, sizeof(disas));
            int n = snprintf(p, rem, "%s\n", disas);
            if (n < 0 || n >= rem) break; // FIX: prevent overflow
            p += n;
            rem -= n;
        }

        *p = '\0';
        size_t total_len = (p - local) + 1;
        char *heap_buf = dr_global_alloc(total_len);
        memcpy(heap_buf, local, total_len);
        cache_bb_string(start_pc, heap_buf); // FIX: key is start_pc
    #endif

    // FIX: pass app_pc to clean call, not tag
    dr_insert_clean_call(drcontext, bb, instrlist_first(bb),
                         (void *)trace_bb, false, 1,
                         OPND_CREATE_INTPTR(start_pc));
    return DR_EMIT_DEFAULT;
}

static void event_exit(void) {
    dr_close_file(log_file);
    #ifdef VVERBOSE
        free_bb_cache();
    #endif
}

DR_EXPORT void
dr_client_main(__attribute__((unused)) client_id_t id,
               __attribute__((unused)) int argc,
               __attribute__((unused)) const char *argv[]) {
    dr_set_client_name("DrBblogger", "https://dynamorio.org");

    const char *log_filename = "bbtrace.log";
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-o") == 0 && i+1 < argc) {
            log_filename = argv[++i];
        } else {
            dr_printf("DrBblogger: unknown option '%s'\n", argv[i]);
        }
    }

    log_file = dr_open_file(log_filename, DR_FILE_WRITE_OVERWRITE | DR_FILE_ALLOW_LARGE);
    DR_ASSERT(log_file != INVALID_FILE);

    dr_register_exit_event(event_exit);
    dr_register_bb_event(event_app_instruction);

    #ifdef VVERBOSE
        init_bb_cache();
    #endif
}
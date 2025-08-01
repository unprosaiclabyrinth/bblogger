#include <dr_api.h>
#include <dr_modules.h>
#include "dr_ir_disassemble.h"
#include <string.h>

static file_t log_file;
static void event_exit(void);

static void trace_bb(app_pc tag); // clean call @ BB entry

// callback function prototype for the basic block event
static dr_emit_flags_t
event_app_instruction(void *drcontext, void *tag, instrlist_t *bb,
                      bool for_trace, bool translating);

/* Cache implementation */
#ifdef VVERBOSE
#define BB_CACHE_TABLE_BITS 10
#define BB_CACHE_TABLE_SIZE (1u << BB_CACHE_TABLE_BITS)

typedef struct bb_data_t {
    app_pc tag;
    char *bbstr;
    struct bb_data_t *next;
} bb_data_t;

/* the bucket array of the hash table cache */
static bb_data_t **bb_cache_table = NULL;

// simple mix: shift away low bits and mask
static inline uint hash(app_pc tag) {
    return (((ptr_int_t)tag) >> 4) & (BB_CACHE_TABLE_SIZE - 1);
}

static void init_bb_cache(void) {
    // allocate and zero the bucket array
    bb_cache_table = dr_global_alloc(BB_CACHE_TABLE_SIZE * sizeof(*bb_cache_table));
    memset(bb_cache_table, 0, BB_CACHE_TABLE_SIZE * sizeof(*bb_cache_table));
}

static void cache_bb_string(app_pc tag, char *bbstr) {
    uint idx = hash(tag);
    bb_data_t *e = dr_global_alloc(sizeof(*e));
    e->tag = tag;
    e->bbstr = bbstr;
    e->next = bb_cache_table[idx];
    bb_cache_table[idx] = e;
}

static char *lookup_bb_string(app_pc tag) {
    uint idx = hash(tag);
    for (bb_data_t *e = bb_cache_table[idx]; e; e = e->next) {
        if (e->tag == tag)
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

// called at runtime, once per BB execution
static void trace_bb(app_pc tag) {
    app_pc app_pc = dr_fragment_app_pc(tag);
    module_data_t *mod = dr_lookup_module(app_pc);
    if (mod != NULL) {
        // compute the module-relative PC
        ptr_int_t modrel_pc = (ptr_int_t)(app_pc - mod->start);
        char *modulestr = mod->names.module_name ? mod->names.module_name : mod->names.file_name;

        // Log according to verbosity level
        #ifdef VVERBOSE
            char *bb = lookup_bb_string(tag);
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
    }
    dr_free_module_data(mod);
}

static dr_emit_flags_t
event_app_instruction(void *drcontext, void *tag, instrlist_t *bb,
                      __attribute__((unused)) bool for_trace,
                      __attribute__((unused)) bool translating) {
    #ifdef VVERBOSE
        // Disassemble the basic block into a local buffer
        char local[4096] = {0};
        char *p = local;
        int rem = sizeof(local);
        for (instr_t *insn = instrlist_first_app(bb); insn; insn = instr_get_next_app(insn)) {
            char disas[256];
            instr_disassemble_to_buffer(drcontext, insn, disas, sizeof(disas));
            int n = snprintf(p, rem, "%s\n", disas);
            p += n;
            rem -= n;
        }

        // Cache the local buffer into the DR heap for it to survive until the end
        *p = '\0'; // null-terminate the string
        size_t total_len = (p - local) + 1;
        char *heap_buf = dr_global_alloc(total_len);
        memcpy(heap_buf, local, total_len);
        cache_bb_string((app_pc)tag, heap_buf);
    #endif

    // insert a clean call at the top of the block
    dr_insert_clean_call(drcontext, bb, instrlist_first(bb),
                         (void *)trace_bb, false, 1,
                         OPND_CREATE_INTPTR(tag));
    return DR_EMIT_DEFAULT;
}

static void event_exit(void) {
    dr_close_file(log_file);
    #ifdef VVERBOSE
        free_bb_cache();
    #endif
}

/* Client entry point */
DR_EXPORT void
dr_client_main(__attribute__((unused)) client_id_t id,
               __attribute__((unused)) int argc,
               __attribute__((unused)) const char *argv[]) {
    dr_set_client_name("DrBblogger", "https://dynamorio.org");

    log_file = dr_open_file("bbtrace.log", DR_FILE_WRITE_OVERWRITE | DR_FILE_ALLOW_LARGE);
    DR_ASSERT(log_file != INVALID_FILE);

    dr_register_exit_event(event_exit);
    dr_register_bb_event(event_app_instruction);

    #ifdef VVERBOSE
        init_bb_cache();
    #endif
}

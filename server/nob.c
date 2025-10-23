#define NOB_STRIP_PREFIX
#define NOB_IMPLEMENTATION
#include "nob.h"
const char* path_skip_one(const char* str) {
    const char* og = str;
    while(*str && *str != '/') str++;
    return *str ? str+1 : og;
}
#define cstr_rem_suffix(__src, suffix) (int)(strlen(__src) - strlen(suffix)), (__src)
int main(int argc, char** argv) {
    NOB_GO_REBUILD_URSELF(argc, argv);
    const char* cc = getenv_or_default("CC", "cc");
    const char* bindir = getenv_or_default("BINDIR", "bin");
    if(!mkdir_if_not_exists_silent(bindir)) return 1;

    shift_args(&argc, &argv);
    bool run = false;
    while(argc) {
        char* arg = shift_args(&argc, &argv);
        if(strcmp(arg, "run") == 0) run = true;
        else {
            nob_log(NOB_ERROR, "Unexpected argument: %s", arg);
            return 1;
        }
    }
    if(!mkdir_if_not_exists_silent(temp_sprintf("%s/server", bindir))) return 1;

    Cmd cmd = { 0 };
    File_Paths dirs = { 0 };
    File_Paths c_sources = { 0 };
    File_Paths gen_sources = { 0 }; 

    if(!walk_directory("src",
            { &dirs, .file_type = FILE_DIRECTORY },
            { &gen_sources, .ext = ".gen.c" },
            { &c_sources, .ext = ".c" },
       )) return 1;
    da_append(&c_sources, "vendor/vendor.c");
    File_Paths objs = { 0 };
    String_Builder stb = { 0 };
    File_Paths pathb = { 0 };
    for(size_t i = 0; i < dirs.count; ++i) {
        size_t temp = temp_save();
        const char* dir = temp_sprintf("%s/server/%s", bindir, path_skip_one(dirs.items[i]));
        if(!mkdir_if_not_exists_silent(dir)) return 1;
        temp_rewind(temp);
    }
    for(size_t i = 0; i < gen_sources.count; ++i) {
        const char* src = gen_sources.items[i];
        const char* out = temp_sprintf("%s/server/%.*s", bindir, cstr_rem_suffix(path_skip_one(dirs.items[i]), ".c"));
        const char* gen_file = temp_sprintf("%.*s", cstr_rem_suffix(src, ".gen.c"));
        bool needs_rebuild = needs_rebuild1(gen_file, src);
        if(c_needs_rebuild1(&stb, &pathb, out, src)) {
            cmd_append(&cmd, NOB_REBUILD_URSELF(out, src), "-O1", "-MMD");
            if(!cmd_run_sync_and_reset(&cmd)) return 1;
            needs_rebuild = true;
        }
        if(!needs_rebuild) continue;
        nob_log(NOB_INFO, "Regenerating %s", gen_file);
        Fd fd = fd_open_for_write(gen_file);
        if(fd == -NOB_INVALID_FD) {
            nob_log(NOB_ERROR, "Failed to open %s", gen_file);
        }
        cmd_append(&cmd, out);
        // TODO: run in that directory
        if(!cmd_run_sync_redirect_and_reset(&cmd, (Cmd_Redirect){ .fdout = &fd })) {
            nob_log(NOB_ERROR, "FAILED to generate %s (using %s)", gen_file, src);
            fd_close(fd);
        }
        fd_close(fd);
    }
    for(size_t i = 0; i < c_sources.count; ++i) {
        const char* src = c_sources.items[i];
        const char* out = temp_sprintf("%s/server/%s.o", bindir, path_skip_one(src));
        da_append(&objs, out);
        if(!c_needs_rebuild1(&stb, &pathb, out, src)) continue;
        cmd_append(&cmd, cc,
            "-Wall", "-Wextra", 
            "-Wno-unused-function", "-Wno-missing-braces",
            "-g",
            "-O0",  
            "-MMD", "-MP",
            "-c", src, "-o", out,
            "-Ivendor", "-I../shared"
        );
        if(!cmd_run_sync_and_reset(&cmd)) {  
            char* str = temp_strdup(out);  
            size_t str_len = strlen(str);  
            assert(str_len);  
            str[str_len-1] = 'd';  
            delete_file(str);  
            return 1;  
        }
    }

    const char* exe = "./server";
    if(needs_rebuild(exe, objs.items, objs.count)) {
        cmd_append(&cmd, cc);
        da_append_many(&cmd, objs.items, objs.count);
        cmd_append(&cmd, "-o", exe);
        if(!cmd_run_sync_and_reset(&cmd)) return 1;
    }
    if(run) {
        cmd_append(&cmd, exe);
        if(!cmd_run_sync_and_reset(&cmd)) return 1;
    }
    return 0;
}

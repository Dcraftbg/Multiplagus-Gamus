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
    bool wayland = getenv("WAYLAND_DISPLAY");
    if(!mkdir_if_not_exists_silent(bindir)) return 1;

    shift_args(&argc, &argv);
    bool run = false;
    while(argc) {
        char* arg = shift_args(&argc, &argv);
        if(strcmp(arg, "run") == 0) run = true;
        else if(strcmp(arg, "wayland")) wayland = true;
        else if(strcmp(arg, "x11")) wayland = false;
        else {
            nob_log(NOB_ERROR, "Unexpected argument: %s", arg);
            return 1;
        }
    }
    if(!mkdir_if_not_exists_silent(temp_sprintf("%s/client", bindir))) return 1;

    Cmd cmd = { 0 };
    File_Paths dirs = { 0 };
    File_Paths c_sources = { 0 };
    File_Paths gen_sources = { 0 }; 

    if(!walk_directory("src",
            { &dirs, .file_type = FILE_DIRECTORY },
            { &gen_sources, .ext = ".gen.c" },
            { &c_sources, .ext = ".c" },
       )) return 1;
    da_append(&c_sources, "vendor/vendor_networking.c");
    da_append(&c_sources, "vendor/RGFW.c");
    da_append(&c_sources, "vendor/stb_truetype.c");
    if(wayland) {
        Nob_File_Paths wayland_folder = {0}; // MEMORY leak but idc since nob lives for a short time
        read_entire_dir("wayland/", &wayland_folder);
        if(wayland_folder.count == 0){
            mkdir_if_not_exists_silent("wayland/");
            cmd.count = 0;
            cmd_append(&cmd, "sh", "generate-wayland-protocols.sh"); // i know its a cheesy way but works
            if(!cmd_run_sync_and_reset(&cmd)) return 1;
            read_entire_dir("wayland/", &wayland_folder);
        }
        for(size_t i = 0; i < wayland_folder.count; i++){
            String_View sv = sv_from_cstr(wayland_folder.items[i]);
            if(sv_end_with(sv,".c")) da_append(&c_sources, temp_sprintf("wayland/%s", wayland_folder.items[i]));
        }
    }
    da_append(&c_sources, "vendor/stb_image.c");
    File_Paths objs = { 0 };
    String_Builder stb = { 0 };
    File_Paths pathb = { 0 };
    for(size_t i = 0; i < dirs.count; ++i) {
        size_t temp = temp_save();
        const char* dir = temp_sprintf("%s/client/%s", bindir, path_skip_one(dirs.items[i]));
        if(!mkdir_if_not_exists_silent(dir)) return 1;
        temp_rewind(temp);
    }
    for(size_t i = 0; i < gen_sources.count; ++i) {
        const char* src = gen_sources.items[i];
        const char* out = temp_sprintf("%s/client/%.*s", bindir, cstr_rem_suffix(path_skip_one(src), ".c"));
        const char* gen_file = temp_sprintf("%.*s", cstr_rem_suffix(src, ".gen.c"));
        // TODO: match Walk Dir Entry function
        if(sv_end_with(sv_from_cstr(gen_file), ".c")) {
            da_append(&c_sources, gen_file);
        }
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
            return 1;
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
        const char* out = temp_sprintf("%s/client/%s.o", bindir, path_skip_one(src));
        da_append(&objs, out);
        if(!c_needs_rebuild1(&stb, &pathb, out, src)) continue;
        cmd_append(&cmd, cc,
            "-Wall", "-Wextra", "-Wno-missing-braces",
            "-g",
            "-O1",  
            "-MMD", "-MP",
            "-c", src, "-o", out, 
            "-Ivendor", "-I../shared",
            "-DRGFWDEF=", "-DRGFW_OPENGL"
        );
        if(wayland) cmd_append(&cmd, "-DRGFW_WAYLAND", "-Iwayland");
        if(!cmd_run_sync_and_reset(&cmd)) {  
            char* str = temp_strdup(out);  
            size_t str_len = strlen(str);  
            assert(str_len);  
            str[str_len-1] = 'd';  
            delete_file(str);  
            return 1;  
        }
    }

    const char* exe = "./client";
    if(needs_rebuild(exe, objs.items, objs.count)) {
        cmd_append(&cmd, cc);
        da_append_many(&cmd, objs.items, objs.count);
        cmd_append(&cmd, "-o", exe);
        cmd_append(&cmd, "-lm", "-lGL");
        if(!wayland) cmd_append(&cmd, "-lX11", "-lXrandr");
        else cmd_append(&cmd, "-lwayland-client", "-lwayland-cursor", "-lwayland-egl", "-lxkbcommon", "-lEGL", "-ldl");
        if(!cmd_run_sync_and_reset(&cmd)) return 1;
    }
    if(run) {
        cmd_append(&cmd, exe);
        if(!cmd_run_sync_and_reset(&cmd)) return 1;
    }
    return 0;
}

/*
 * jbinit 3
 * made by nick chan
*/

#include <fakedyld/fakedyld.h>
#include <mount_args.h>

bool has_verbose_boot = false;

int main(int argc, char* argv[], char* envp[], char* apple[]) {
    int console_fd = open("/dev/console", O_RDWR | O_CLOEXEC | O_SYNC, 0);
    if (console_fd == -1) panic("cannot open /dev/console: %d", errno);
    set_fd_console(console_fd);
    printf("argc: %d\n", argc);
    for (int i = 0; argv[i] != NULL; i++) {
        printf("argv[%d]: %s\n", i, argv[i]);
    }
    for (int i = 0; envp[i] != NULL; i++) {
        printf("envp[%d]: %s\n", i, envp[i]);
    }
    for (int i = 0; apple[i] != NULL; i++) {
        printf("apple[%d]: %s\n", i, apple[i]);
    }
    int ret = 0;
    struct systeminfo sysinfo;
    systeminfo(&sysinfo);
    LOG("Parsed kernel version: Darwin %d.%d.%d xnu: %d",
        sysinfo.osrelease.darwinMajor,
        sysinfo.osrelease.darwinMinor,
        sysinfo.osrelease.darwinRevision,
        sysinfo.xnuMajor
    );
    LOG("boot-args: %s", sysinfo.bootargs);
    LOG("Kernel version (raw): %s", sysinfo.kversion);

    struct paleinfo pinfo;
    get_pinfo(&pinfo);
    printf(
        "kbase: 0x%llx\n"
        "kslide: 0x%llx\n"
        "flags: 0x%llx\n"
        "rootdev: %s\n"
    ,pinfo.kbase,pinfo.kslide,pinfo.flags,pinfo.rootdev
    );
    has_verbose_boot = has_verbose_boot || (pinfo.flags & palerain_option_verbose_boot);

    pinfo_check(&pinfo);
    memory_file_handle_t payload;
    memory_file_handle_t payload15_dylib;
#if 0
    if (pinfo.flags & palerain_option_bind_mount) {
        read_file("/payload.dylib", &payload15_dylib);
        read_file("/payload", &payload);
    }
#endif
    if (sysinfo.xnuMajor < 7938) mountroot(&pinfo, &sysinfo);
    prepare_rootfs(&sysinfo, &pinfo);
    memory_file_handle_t dyld_handle;
    read_file("/usr/lib/dyld", &dyld_handle);
    check_dyld(&dyld_handle);
    int platform = get_platform(&dyld_handle);
    init_cores(&sysinfo, platform);
    patch_dyld(&dyld_handle, platform);
    write_file("/cores/usr/lib/dyld", &dyld_handle);

    #define INSERT_DYLIB "DYLD_INSERT_LIBRARIES=/cores/payload.dylib"
    #define DEFAULT_TWEAKLOADER "JB_TWEAKLOADER_PATH=@default"
    #define DISABLE_DYLD_IN_CACHE "DYLD_IN_CACHE=0"
    
        char* JBRootPathEnv;
        if (pinfo.flags & palerain_option_rootful) {
            JBRootPathEnv = "JB_ROOT_PATH=/";
        } else {
            JBRootPathEnv = "JB_ROOT_PATH=/var/jb"; /* will fixup to preboot path in sysstatuscheck stage */
        }

        char pinfo_buffer[50];
        snprintf(pinfo_buffer, 50, "JB_PINFO_FLAGS=0x%llx", pinfo.flags);
        char* execve_buffer = mmap(NULL, 0x4000, PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_PRIVATE, 0, 0);
        if (execve_buffer == MAP_FAILED) {
            panic("mmap for execve failed");
        }
        char* launchd_argv0 = execve_buffer;
        char* launchd_envp0 = (execve_buffer + sizeof("/sbin/launchd"));
        char* launchd_envp1 = (launchd_envp0 + sizeof(INSERT_DYLIB));
        char* launchd_envp2 = (launchd_envp1 + strlen(pinfo_buffer) + 1);
        char* launchd_envp3 = (launchd_envp2 + strlen(JBRootPathEnv) + 1);
        char* launchd_envp4 = (launchd_envp3 + sizeof(DEFAULT_TWEAKLOADER));
        memcpy(launchd_argv0, "/sbin/launchd", sizeof("/sbin/launchd"));
        memcpy(launchd_envp0, INSERT_DYLIB, sizeof(INSERT_DYLIB));
        memcpy(launchd_envp1, pinfo_buffer, strlen(pinfo_buffer) + 1);
        memcpy(launchd_envp2, JBRootPathEnv, strlen(JBRootPathEnv) + 1);
        memcpy(launchd_envp3, DEFAULT_TWEAKLOADER, sizeof(DEFAULT_TWEAKLOADER));
        memcpy(launchd_envp4, DISABLE_DYLD_IN_CACHE, sizeof(DISABLE_DYLD_IN_CACHE));
        char** launchd_argv = (char**)((char*)launchd_envp4 + sizeof(DISABLE_DYLD_IN_CACHE));
        char** launchd_envp = (char**)((char*)launchd_argv + (2*sizeof(char*)));
        launchd_argv[0] = launchd_argv0;
        launchd_argv[1] = NULL;
        launchd_envp[0] = launchd_envp0;
        launchd_envp[1] = launchd_envp1;
        launchd_envp[2] = launchd_envp2;
        launchd_envp[3] = launchd_envp3;
        launchd_envp[4] = launchd_envp4;
        launchd_envp[5] = NULL;
        LOG("launchd environmental variables: ");
        for (int i = 0; launchd_envp[i] != NULL; i++) {
            LOG("%s", launchd_envp[i]);
        }

        ret = execve(launchd_argv0, launchd_argv, launchd_envp);
    /*}*/
    panic("execve failed with error=%d", ret);
    __builtin_unreachable();
}


_Noreturn void panic(char* fmt, ...) {
  char reason[1024], reason_real[1024];
  va_list va;
  va_start(va, fmt);
  vsnprintf(reason, 1024, fmt, va);
  va_end(va);
  snprintf(reason_real, 1024, "fakedyld: %s", reason);
  if (has_verbose_boot) {
    printf("%s\n", reason_real);
    LOG("jbinit DIED!");
    sleep(60);
  }
  abort_with_payload(42, 0x69, NULL, 0, reason_real, 0);
  __asm__ ("b .");
  __builtin_unreachable();
}

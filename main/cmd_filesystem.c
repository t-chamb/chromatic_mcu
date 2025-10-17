#include <stdio.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>
#include "esp_console.h"
#include "argtable3/argtable3.h"
#include "esp_log.h"

static const char *TAG = "cmd_fs";

static struct {
    struct arg_str *path;
    struct arg_end *end;
} ls_args;

static int do_ls(int argc, char **argv)
{
    int nerrors = arg_parse(argc, argv, (void **)&ls_args);
    if (nerrors != 0) {
        arg_print_errors(stderr, ls_args.end, argv[0]);
        return 1;
    }

    const char *path = ls_args.path->count > 0 ? ls_args.path->sval[0] : "/sdcard";
    
    DIR *dir = opendir(path);
    if (dir == NULL) {
        printf("Error: Cannot open directory '%s'\n", path);
        printf("Make sure SD card is mounted at /sdcard\n");
        return 1;
    }

    printf("\nDirectory listing of: %s\n", path);
    printf("%-30s %10s\n", "Name", "Size");
    printf("─────────────────────────────────────────────\n");

    struct dirent *entry;
    struct stat st;
    char filepath[300];
    int file_count = 0;
    int dir_count = 0;

    while ((entry = readdir(dir)) != NULL) {
        snprintf(filepath, sizeof(filepath), "%s/%s", path, entry->d_name);
        
        if (stat(filepath, &st) == 0) {
            if (S_ISDIR(st.st_mode)) {
                printf("%-30s %10s\n", entry->d_name, "<DIR>");
                dir_count++;
            } else {
                printf("%-30s %10ld\n", entry->d_name, st.st_size);
                file_count++;
            }
        } else {
            printf("%-30s %10s\n", entry->d_name, "?");
        }
    }

    closedir(dir);
    
    printf("─────────────────────────────────────────────\n");
    printf("%d file(s), %d dir(s)\n\n", file_count, dir_count);

    return 0;
}

static int do_cat(int argc, char **argv)
{
    if (argc < 2) {
        printf("Usage: cat <filepath>\n");
        printf("Example: cat /sdcard/test.txt\n");
        return 1;
    }

    const char *filepath = argv[1];
    FILE *f = fopen(filepath, "r");
    
    if (f == NULL) {
        printf("Error: Cannot open file '%s'\n", filepath);
        return 1;
    }

    printf("\n");
    char line[256];
    while (fgets(line, sizeof(line), f) != NULL) {
        printf("%s", line);
    }
    printf("\n");

    fclose(f);
    return 0;
}

void register_filesystem_commands(void)
{
    ls_args.path = arg_str0(NULL, NULL, "<path>", "Directory path (default: /sdcard)");
    ls_args.end = arg_end(1);

    const esp_console_cmd_t ls_cmd = {
        .command = "ls",
        .help = "List directory contents",
        .hint = NULL,
        .func = &do_ls,
        .argtable = &ls_args
    };
    ESP_ERROR_CHECK(esp_console_cmd_register(&ls_cmd));

    const esp_console_cmd_t cat_cmd = {
        .command = "cat",
        .help = "Display file contents",
        .hint = NULL,
        .func = &do_cat,
    };
    ESP_ERROR_CHECK(esp_console_cmd_register(&cat_cmd));

    ESP_LOGI(TAG, "Filesystem commands registered");
}

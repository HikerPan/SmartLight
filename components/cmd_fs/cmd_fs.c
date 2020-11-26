/*
 * @Author: Alex.Pan
 * @Date: 2020-11-08 22:02:23
 * @LastEditTime: 2020-11-23 21:31:02
 * @LastEditors: Alex.Pan
 * @Description:
 * @FilePath: \ESP32\project\SmartLight\components\cmd_fs\cmd_fs.c
 * @Copyright 2010-2015 LEKTEC or its affiliates. All Rights Reserved.
 */
#include <sys/types.h>
#include <dirent.h>
#include "sdkconfig.h"
#include "esp_vfs.h"
#include "esp_console.h"
#include "linenoise/linenoise.h"
#include "argtable3/argtable3.h"
#include "esp_err.h"
#include "esp_log.h"

// static const char *TAG = "cmdfs";

/**
 * @addtogroup FileApi
 */

char working_directory[255] = {"/lfs"};

/**
 * this function will normalize a path according to specified parent directory
 * and file name.
 *
 * @param directory the parent path
 * @param filename the file name
 *
 * @return the built full file path (absolute path)
 */
char *vfs_normalize_path(const char *directory, const char *filename)
{
    char *fullpath;
    char *dst0, *dst, *src;


    if (directory == NULL) /* shall use working directory */
        directory = &working_directory[0];

    if (filename[0] != '/') /* it's a absolute path, use it directly */
    {
        fullpath = (char *)malloc(strlen(directory) + strlen(filename) + 2);

        if (fullpath == NULL)
            return NULL;

        /* join path and file name */
        snprintf(fullpath, strlen(directory) + strlen(filename) + 2,
                    "%s/%s", directory, filename);
    }
    else
    {
        fullpath = strdup(filename); /* copy string */

        if (fullpath == NULL)
            return NULL;
    }

    src = fullpath;
    dst = fullpath;

    dst0 = dst;
    while (1)
    {
        char c = *src;

        if (c == '.')
        {
            if (!src[1]) src ++; /* '.' and ends */
            else if (src[1] == '/')
            {
                /* './' case */
                src += 2;

                while ((*src == '/') && (*src != '\0'))
                    src ++;
                continue;
            }
            else if (src[1] == '.')
            {
                if (!src[2])
                {
                    /* '..' and ends case */
                    src += 2;
                    goto up_one;
                }
                else if (src[2] == '/')
                {
                    /* '../' case */
                    src += 3;

                    while ((*src == '/') && (*src != '\0'))
                        src ++;
                    goto up_one;
                }
            }
        }

        /* copy up the next '/' and erase all '/' */
        while ((c = *src++) != '\0' && c != '/')
            *dst ++ = c;

        if (c == '/')
        {
            *dst ++ = '/';
            while (c == '/')
                c = *src++;

            src --;
        }
        else if (!c)
            break;

        continue;

up_one:
        dst --;
        if (dst < dst0)
        {
            free(fullpath);
            return NULL;
        }
        while (dst0 < dst && dst[-1] != '/')
            dst --;
    }

    *dst = '\0';

    /* remove '/' in the end of path if exist */
    dst --;
    if ((dst != fullpath) && (*dst == '/'))
        *dst = '\0';

    /* final check fullpath is not empty, for the special path of lwext "/.." */
    if ('\0' == fullpath[0])
    {
        fullpath[0] = '/';
        fullpath[1] = '\0';
    }

    return fullpath;
}

// int df(const char *path)
// {
//     int result;
//     int minor = 0;
//     long long cap;
//     struct statfs buffer;

//     int unit_index = 0;
//     char *unit_str[] = {"KB", "MB", "GB"};
    
//     result = statfs(path ? path : NULL, &buffer);
//     if (result != 0)
//     {
//         printf("dfs_statfs failed.\n");
//         return -1;
//     }

//     cap = ((long long)buffer.f_bsize) * ((long long)buffer.f_bfree) / 1024LL;
//     for (unit_index = 0; unit_index < 2; unit_index ++)
//     {
//         if (cap < 1024) break;

//         minor = (cap % 1024) * 10 / 1024; /* only one decimal point */
//         cap = cap / 1024;
//     }

//     printf("disk free: %d.%d %s [ %d block, %d bytes per block ]\n",
//                (unsigned long)cap, minor, unit_str[unit_index], buffer.f_bfree, buffer.f_bsize);
//     return 0;
// }

void rm(const char *filename)
{
    if (unlink(filename) < 0)
    {
        printf("Delete %s failed\n", filename);
    }
}


/** Arguments used by 'ry' function */
static struct {
    struct arg_str *path;
    struct arg_end *end;
} rm_args;

static int cmd_rm(int argc, char **argv)
{

    int nerrors = arg_parse(argc, argv, (void **) &rm_args);
    if (nerrors != 0) {
        arg_print_errors(stderr, rm_args.end, argv[0]);
        return 1;
    }
    
    rm(rm_args.path->sval[0]);

    return ESP_OK;
    

}

static void register_rm(void)
{
    // join_args.timeout = arg_int0(NULL, "timeout", "<t>", "Connection timeout, ms");
    rm_args.path = arg_str0(NULL, NULL, "<path>", "the file to unlink.");
    rm_args.end = arg_end(1);

    const esp_console_cmd_t rm_cmd = {
        .command = "rm",
        .help = "remove files or directories",
        .hint = NULL,
        .func = &cmd_rm,
        .argtable = &rm_args
    };

    ESP_ERROR_CHECK( esp_console_cmd_register(&rm_cmd) );
}



void ls(const char *pathname)
{
    struct stat statbuf;
    char *fullpath, *path;
    DIR *dir = NULL;

    fullpath = NULL;
    if (pathname == NULL)
    {
        path = &working_directory[0];
    }
    else
    {
        path = (char *)pathname;
    }

    /* list directory */
    dir = opendir(path);
    if (NULL != dir)
    {
        printf("Directory %s:\n", path);
        do
        {
            struct dirent * pEnt = readdir(dir);
            if (pEnt != NULL)
            {
                memset(&statbuf, 0, sizeof(struct stat));

                /* build full path for each file */
                fullpath = vfs_normalize_path(path, pEnt->d_name);
                if (fullpath == NULL)
                    break;               
                if(stat(fullpath, &statbuf) == 0)
                {
                    printf("%-20s", pEnt->d_name);
                    if (S_ISDIR(statbuf.st_mode))
                    {
                        printf("%-25s\n", "<DIR>");
                    }
                    else
                    {
                        printf("%-25lu\n", (unsigned long)statbuf.st_size);
                    }
                }
                else
                    printf("BAD file: %s\n", pEnt->d_name);
                free(fullpath);
            }
            else
            {
                break;
            }
        }
        while (1);

        closedir(dir);
    }
    else
    {
        printf("No such directory\n");
    }
}


/** Arguments used by 'ry' function */
static struct {
    struct arg_str *path;
    struct arg_end *end;
} ls_args;

static int cmd_ls(int argc, char **argv)
{

    int nerrors = arg_parse(argc, argv, (void **) &ls_args);
    if (nerrors != 0) {
        arg_print_errors(stderr, ls_args.end, argv[0]);
        return 1;
    }
    // if(NULL != ls_args.path->sval[0])
    //     ESP_LOGI(TAG,"path len %d",strlen(ls_args.path->sval[0]));
    // // ESP_LOG_BUFFER_HEX_LEVEL
    if(strlen(ls_args.path->sval[0]))
        ls(ls_args.path->sval[0]);
    else
        ls(NULL);
    

    return ESP_OK;
    

}

static void register_ls(void)
{
    // join_args.timeout = arg_int0(NULL, "timeout", "<t>", "Connection timeout, ms");
    ls_args.path = arg_str0(NULL, NULL, "<path>", "the path to list.");
    ls_args.end = arg_end(1);

    const esp_console_cmd_t ls_cmd = {
        .command = "ls",
        .help = "list directory contents",
        .hint = NULL,
        .func = &cmd_ls,
        .argtable = &ls_args
    };

    ESP_ERROR_CHECK( esp_console_cmd_register(&ls_cmd) );
}


void register_fs_cmd(void)
{
    register_ls();
    register_rm();
}


// /**
//  * this function is a POSIX compliant version, which will make a directory
//  *
//  * @param path the directory path to be made.
//  * @param mode
//  *
//  * @return 0 on successful, others on failed.
//  */
// int mkdir(const char *path, mode_t mode)
// {
//     int fd;
//     struct dfs_fd *d;
//     int result;

//     fd = fd_new();
//     if (fd == -1)
//     {
//         rt_set_errno(-ENOMEM);

//         return -1;
//     }

//     d = fd_get(fd);

//     result = dfs_file_open(d, path, O_DIRECTORY | O_CREAT);
//     opendir();

//     if (result < 0)
//     {
//         fd_put(d);
//         fd_put(d);
//         rt_set_errno(result);

//         return -1;
//     }

//     dfs_file_close(d);
//     fd_put(d);
//     fd_put(d);

//     return 0;
// }



/* @} */


/*
 * Copyright (c) 2022 Institute of Parallel And Distributed Systems (IPADS)
 * ChCore-Lab is licensed under the Mulan PSL v1.
 * You can use this software according to the terms and conditions of the Mulan
 * PSL v1. You may obtain a copy of Mulan PSL v1 at:
 *     http://license.coscl.org.cn/MulanPSL
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY
 * KIND, EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO
 * NON-INFRINGEMENT, MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE. See the
 * Mulan PSL v1 for more details.
 */

#include <malloc.h>
#include <stdio.h>
#include <chcore/fs/defs.h>
#include <chcore/ipc.h>
#include <string.h>
#include <chcore/internal/raw_syscall.h>
#include <chcore/assert.h>
#include <chcore/procm.h>
#include <chcore/thread.h>
#include <chcore/internal/server_caps.h>

#define SERVER_READY_FLAG(vaddr) (*(int *)(vaddr))
#define SERVER_EXIT_FLAG(vaddr)  (*(int *)((u64)vaddr + 4))

static struct ipc_struct ipc_struct;
static int tmpfs_scan_pmo_cap;

/* fs_server_cap in current process; can be copied to others */
int fs_server_cap;

#define BUFLEN 4096

struct ipc_struct *fs_ipc_struct_for_shell = NULL;

static void connect_tmpfs_server(void)
{
        int tmpfs_cap = __chcore_get_tmpfs_cap();
        chcore_assert(tmpfs_cap >= 0);
        fs_ipc_struct_for_shell = ipc_register_client(tmpfs_cap);
        chcore_assert(fs_ipc_struct_for_shell);
}

int mount_fs(const char *fspath, const char *mount_point)
{
        int ret;
        struct ipc_msg *ipc_msg = ipc_create_msg(
                fs_ipc_struct_for_shell, sizeof(struct fs_request), 0);
        chcore_assert(ipc_msg);
        struct fs_request *fr = (struct fs_request *)ipc_get_msg_data(ipc_msg);
        fr->req = FS_REQ_MOUNT;
        strcpy(fr->mount.fs_path, fspath);
        strcpy(fr->mount.mount_path, mount_point);
        ret = ipc_call(fs_ipc_struct_for_shell, ipc_msg);
        ipc_destroy_msg(fs_ipc_struct_for_shell, ipc_msg);
        return ret;
}

/* Retrieve the entry name from one dirent */
static void get_dent_name(struct dirent *p, char name[])
{
        int len;
        len = p->d_reclen - sizeof(p->d_ino) - sizeof(p->d_off)
              - sizeof(p->d_reclen) - sizeof(p->d_type);
        memcpy(name, p->d_name, len);
        name[len - 1] = '\0';
}

/* See demo_getdents() for example */
int getdents(int fd, char *buf, int count)
{
        struct ipc_msg *ipc_msg = 0;
        struct fd_record_extension *fd_ext;
        struct fs_request *fr_ptr;
        int ret = 0, remain = count, cnt;
        chcore_assert(fs_ipc_struct_for_shell);

        ipc_msg = ipc_create_msg(fs_ipc_struct_for_shell, 512, 0);
        fr_ptr = (struct fs_request *)ipc_get_msg_data(ipc_msg);
        while (remain > 0) {
                fr_ptr->req = FS_REQ_GETDENTS64;
                fr_ptr->getdents64.fd = fd;
                cnt = MIN(remain, PAGE_SIZE);
                fr_ptr->getdents64.count = cnt;
                ret = ipc_call(fs_ipc_struct_for_shell, ipc_msg);
                if (ret < 0)
                        goto error;
                memcpy(buf, ipc_get_msg_data(ipc_msg), ret);
                buf += ret;
                remain -= ret;
                if (ret != cnt)
                        break;
        }
        ret = count - remain;
error:
        ipc_destroy_msg(fs_ipc_struct_for_shell, ipc_msg);
        return ret;
}

void demo_getdents(int fd)
{
        char name[BUFLEN];
        char scan_buf[BUFLEN];
        int offset;
        struct dirent *p;

        int ret = getdents(fd, scan_buf, BUFLEN);

        for (offset = 0; offset < ret; offset += p->d_reclen) {
                p = (struct dirent *)(scan_buf + offset);
                get_dent_name(p, name);
                printf("The name is %s \n", name);
        }
}

int alloc_fd()
{
        static int cnt = 0;
        return ++cnt;
}

int do_complement(char *buf, char *complement, int complement_time)
{
        int ret = 0, j = 0;
        struct dirent *p;
        char name[BUFLEN];
        char scan_buf[BUFLEN];
        int r = -1;
        int offset;

        /* LAB 5 TODO BEGIN */
        struct ipc_msg *ipc_msg = 0;
        struct fs_request *fr_ptr;
        int fd = 0;
        size_t buf_len = strlen(buf);

        // open
        fd = alloc_fd();
        ipc_msg = ipc_create_msg(fs_ipc_struct_for_shell, 512, 0);
        fr_ptr = (struct fs_request *)ipc_get_msg_data(ipc_msg);
        fr_ptr->req = FS_REQ_OPEN;
        fr_ptr->open.new_fd = fd;
        strcpy(fr_ptr->open.pathname, "/");
        ret = ipc_call(fs_ipc_struct_for_shell, ipc_msg);
        ipc_destroy_msg(fs_ipc_struct_for_shell, ipc_msg);
        if (ret < 0) {
                return ret;
        }

        // scan
        ret = getdents(fd, scan_buf, BUFLEN);
        for (offset = 0; offset < ret; offset += p->d_reclen) {
                p = (struct dirent *)(scan_buf + offset);
                get_dent_name(p, name);
                if (*name != '.' && strncmp(buf, name, buf_len) == 0) {
                        if (j >= complement_time) {
                                memset(complement, '\0', BUFLEN);
                                strcpy(complement, name + buf_len);
                                r = 1;
                                break;
                        } else {
                                j++;
                        }
                }
        }

        // close
        ipc_msg = ipc_create_msg(fs_ipc_struct_for_shell, 512, 0);
        fr_ptr = (struct fs_request *)ipc_get_msg_data(ipc_msg);
        fr_ptr->req = FS_REQ_CLOSE;
        fr_ptr->close.fd = fd;
        ret = ipc_call(fs_ipc_struct_for_shell, ipc_msg);
        ipc_destroy_msg(fs_ipc_struct_for_shell, ipc_msg);

        /* LAB 5 TODO END */
        return r;
}

extern char getch();

// read a command from stdin leading by `prompt`
// put the commond in `buf` and return `buf`
// What you typed should be displayed on the screen
char *readline(const char *prompt)
{
        static char buf[BUFLEN];

        int i = 0, j = 0;
        signed char c = 0;
        int ret = 0;
        char complement[BUFLEN];
        int complement_time = 0;

        if (prompt != NULL) {
                printf("%s", prompt);
        }

        while (1) {
                __chcore_sys_yield();
                c = getch();

                /* LAB 5 TODO BEGIN */
                /* Fill buf and handle tabs with do_complement(). */
                if (ret == 0) {
                        memset(buf, '\0', BUFLEN);
                }
                if (c < 0) {
                        return NULL;
                } else if (c == '\t') {
                        if (do_complement(buf, complement, complement_time)
                            >= 0) {
                                complement_time++;
                                printf("%s ", complement);
                        }
                        continue;
                } else if (c == '\r' || c == '\n') {
                        printf("\n");
                        buf[ret++] = '\0';
                        break;
                } else {
                        putc(c);
                }
                buf[ret++] = c;

                /* LAB 5 TODO END */
        }

        return buf;
}

int do_top()
{
        __chcore_sys_top();
        return 0;
}

void print_file_content(char *path)
{
        /* LAB 5 TODO BEGIN */
        struct ipc_msg *ipc_msg = 0;
        struct fs_request *fr_ptr;
        int ret = 0, fd = 0;
        char name[BUFLEN];
        char buf[BUFLEN];
        int cnt;
        int remain = BUFLEN;
        struct dirent *p;

        // open
        fd = alloc_fd();
        ipc_msg = ipc_create_msg(fs_ipc_struct_for_shell, 512, 0);
        fr_ptr = (struct fs_request *)ipc_get_msg_data(ipc_msg);
        fr_ptr->req = FS_REQ_OPEN;
        fr_ptr->open.new_fd = fd;
        strcpy(fr_ptr->open.pathname, path);
        ret = ipc_call(fs_ipc_struct_for_shell, ipc_msg);
        if (ret < 0) {
                return ret;
        }

        // read
        while (remain > 0) {
                fr_ptr->req = FS_REQ_READ;
                fr_ptr->read.fd = fd;
                cnt = MIN(remain, PAGE_SIZE);
                fr_ptr->read.count = cnt;
                ret = ipc_call(fs_ipc_struct_for_shell, ipc_msg);
                if (ret < 0) {
                        goto close;
                }
                memcpy(buf, ipc_get_msg_data(ipc_msg), ret);
                for (int i = 0; i < ret; ++i) {
                        putc(buf[i]);
                }

                remain -= ret;
                if (ret != cnt)
                        break;
        }

close:
        // close
        fr_ptr->req = FS_REQ_CLOSE;
        fr_ptr->close.fd = fd;
        ret = ipc_call(fs_ipc_struct_for_shell, ipc_msg);
        ipc_destroy_msg(fs_ipc_struct_for_shell, ipc_msg);

        return ret;
        /* LAB 5 TODO END */
}

void fs_scan(char *path)
{
        /* LAB 5 TODO BEGIN */
        struct ipc_msg *ipc_msg = 0;
        struct fs_request *fr_ptr;
        int ret = 0, fd = 0;
        char name[BUFLEN];
        char scan_buf[BUFLEN];
        int offset;
        struct dirent *p;

        // open
        fd = alloc_fd();
        ipc_msg = ipc_create_msg(fs_ipc_struct_for_shell, 512, 0);
        fr_ptr = (struct fs_request *)ipc_get_msg_data(ipc_msg);
        fr_ptr->req = FS_REQ_OPEN;
        fr_ptr->open.new_fd = fd;
        strcpy(fr_ptr->open.pathname, path);
        ret = ipc_call(fs_ipc_struct_for_shell, ipc_msg);
        ipc_destroy_msg(fs_ipc_struct_for_shell, ipc_msg);
        if (ret < 0) {
                return ret;
        }

        // scan
        ret = getdents(fd, scan_buf, BUFLEN);
        for (offset = 0; offset < ret; offset += p->d_reclen) {
                p = (struct dirent *)(scan_buf + offset);
                get_dent_name(p, name);
                if (*name != '.')
                        printf("%s ", name);
        }

        // close
        ipc_msg = ipc_create_msg(fs_ipc_struct_for_shell, 512, 0);
        fr_ptr = (struct fs_request *)ipc_get_msg_data(ipc_msg);
        fr_ptr->req = FS_REQ_CLOSE;
        fr_ptr->close.fd = fd;
        ret = ipc_call(fs_ipc_struct_for_shell, ipc_msg);
        ipc_destroy_msg(fs_ipc_struct_for_shell, ipc_msg);

        return ret;
        /* LAB 5 TODO END */
}

int do_ls(char *cmdline)
{
        char pathbuf[BUFLEN];

        pathbuf[0] = '\0';
        cmdline += 2;
        while (*cmdline == ' ')
                cmdline++;
        strcat(pathbuf, cmdline);
        fs_scan(pathbuf); /* LAB 5: Complete fs_scan() */
        return 0;
}

int do_cat(char *cmdline)
{
        char pathbuf[BUFLEN];

        pathbuf[0] = '\0';
        cmdline += 3;
        while (*cmdline == ' ')
                cmdline++;
        strcat(pathbuf, cmdline);
        print_file_content(pathbuf); /* LAB 5: Complete print_file_content() */
        return 0;
}

int do_echo(char *cmdline)
{
        /* LAB 5 TODO BEGIN */
        cmdline += 4;
        while (*cmdline == ' ')
                cmdline++;
        printf("%s\n", cmdline);
        /* LAB 5 TODO END */
        return 0;
}

void do_clear(void)
{
        putc(12);
        putc(27);
        putc('[');
        putc('2');
        putc('J');
}

int builtin_cmd(char *cmdline)
{
        int ret, i;
        char cmd[BUFLEN];
        for (i = 0; cmdline[i] != ' ' && cmdline[i] != '\0'; i++)
                cmd[i] = cmdline[i];
        cmd[i] = '\0';
        if (!strcmp(cmd, "quit") || !strcmp(cmd, "exit"))
                __chcore_sys_thread_exit();
        if (!strcmp(cmd, "ls")) {
                ret = do_ls(cmdline);
                return !ret ? 1 : -1;
        }
        if (!strcmp(cmd, "echo")) {
                ret = do_echo(cmdline);
                return !ret ? 1 : -1;
        }
        if (!strcmp(cmd, "cat")) {
                ret = do_cat(cmdline);
                return !ret ? 1 : -1;
        }
        if (!strcmp(cmd, "clear")) {
                do_clear();
                return 1;
        }
        if (!strcmp(cmd, "top")) {
                ret = do_top();
                return !ret ? 1 : -1;
        }
        return 0;
}

int run_cmd(char *cmdline)
{
        int cap = 0;

        /* Hint: Function chcore_procm_spawn() could be used here. */
        /* LAB 5 TODO BEGIN */
        char pathbuf[BUFLEN];
        pathbuf[0] = '\0';
        while (*cmdline == ' ')
                cmdline++;
        if (*cmdline == '\0')
                return -1;
        if (*cmdline != '/')
                strcpy(pathbuf, "/");
        strcat(pathbuf, cmdline);

        cap = fs_server_cap;
        chcore_procm_spawn(pathbuf, &cap);

        /* LAB 5 TODO END */
        return 0;
}

void connect_fs(void)
{
        // register fs client
        connect_tmpfs_server();
}

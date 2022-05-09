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

#include "lab5_stdio.h"

extern struct ipc_struct *tmpfs_ipc_struct;

/* You could add new functions or include headers here.*/
/* LAB 5 TODO BEGIN */
int alloc_fd()
{
        static int cnt = 0;
        return ++cnt;
}
/* LAB 5 TODO END */

FILE *fopen(const char *filename, const char *mode)
{
        /* LAB 5 TODO BEGIN */
        int ret = 0;
        struct ipc_msg *ipc_msg = 0;
        struct fs_request *fr_ptr;
        FILE *file = (FILE *)malloc(sizeof(FILE));

        file->fd = alloc_fd();
        strcpy(file->path, filename);
        ipc_msg = ipc_create_msg(tmpfs_ipc_struct, 512, 0);
        fr_ptr = (struct fs_request *)ipc_get_msg_data(ipc_msg);
        fr_ptr->req = FS_REQ_OPEN;
        fr_ptr->open.new_fd = file->fd;
        strcpy(fr_ptr->open.pathname, filename);
        ret = ipc_call(tmpfs_ipc_struct, ipc_msg);
        if (ret < 0 && *mode == 'w') {
                fr_ptr->req = FS_REQ_CREAT;
                strcpy(fr_ptr->creat.pathname, filename);
                ret = ipc_call(tmpfs_ipc_struct, ipc_msg);
                fr_ptr = (struct fs_request *)ipc_get_msg_data(ipc_msg);
                fr_ptr->req = FS_REQ_OPEN;
                fr_ptr->open.new_fd = file->fd;
                strcpy(fr_ptr->open.pathname, filename);
                ret = ipc_call(tmpfs_ipc_struct, ipc_msg);
        }
        ipc_destroy_msg(tmpfs_ipc_struct, ipc_msg);
        /* LAB 5 TODO END */
        return file;
}

size_t fwrite(const void *src, size_t size, size_t nmemb, FILE *f)
{
        /* LAB 5 TODO BEGIN */
        int ret = 0;
        struct ipc_msg *ipc_msg = 0;
        struct fs_request *fr_ptr;

        ipc_msg = ipc_create_msg(
                tmpfs_ipc_struct, sizeof(struct fs_request) + size * nmemb, 0);
        fr_ptr = (struct fs_request *)ipc_get_msg_data(ipc_msg);
        fr_ptr->req = FS_REQ_WRITE;
        fr_ptr->write.fd = f->fd;
        fr_ptr->write.count = size * nmemb;
        memcpy((void *)fr_ptr + sizeof(struct fs_request), src, size * nmemb);
        ret = ipc_call(tmpfs_ipc_struct, ipc_msg);
        ipc_destroy_msg(tmpfs_ipc_struct, ipc_msg);
        /* LAB 5 TODO END */
        return size * nmemb;
}

size_t fread(void *destv, size_t size, size_t nmemb, FILE *f)
{
        /* LAB 5 TODO BEGIN */
        int ret = 0, remain = size * nmemb, cnt = 0;
        struct ipc_msg *ipc_msg = 0;
        struct fs_request *fr_ptr;

        ipc_msg = ipc_create_msg(tmpfs_ipc_struct, 512, 0);
        fr_ptr = (struct fs_request *)ipc_get_msg_data(ipc_msg);

        while (remain > 0) {
                fr_ptr->req = FS_REQ_READ;
                fr_ptr->read.fd = f->fd;
                cnt = MIN(remain, PAGE_SIZE);
                fr_ptr->read.count = cnt;
                ret = ipc_call(tmpfs_ipc_struct, ipc_msg);
                if (ret < 0) {
                        goto fail;
                }
                memcpy(destv, ipc_get_msg_data(ipc_msg), ret);

                remain -= ret;
                if (ret != cnt)
                        break;
        }
fail:
        ipc_destroy_msg(tmpfs_ipc_struct, ipc_msg);
        /* LAB 5 TODO END */
        return size * nmemb - remain;
}

int fclose(FILE *f)
{
        /* LAB 5 TODO BEGIN */
        int ret = 0;
        struct ipc_msg *ipc_msg = 0;
        struct fs_request *fr_ptr;

        ipc_msg = ipc_create_msg(tmpfs_ipc_struct, 512, 0);
        fr_ptr = (struct fs_request *)ipc_get_msg_data(ipc_msg);
        fr_ptr->req = FS_REQ_CLOSE;
        fr_ptr->close.fd = f->fd;
        ret = ipc_call(tmpfs_ipc_struct, ipc_msg);
        ipc_destroy_msg(tmpfs_ipc_struct, ipc_msg);
        /* LAB 5 TODO END */
        return ret;
}

/* Need to support %s and %d. */
int fscanf(FILE *f, const char *fmt, ...)
{
        /* LAB 5 TODO BEGIN */

        /* LAB 5 TODO END */
        return 0;
}

/* Need to support %s and %d. */
int fprintf(FILE *f, const char *fmt, ...)
{
        /* LAB 5 TODO BEGIN */

        /* LAB 5 TODO END */
        return 0;
}

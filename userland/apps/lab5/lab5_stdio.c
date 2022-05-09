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

void my_itoa(int i, char *str)
{
        int power = 0, j = 0;
        j = i;

        for (power = 1; j > 10; j /= 10)
                power *= 10;

        for (; power > 0; power /= 10) {
                *str++ = '0' + i / power;
                i %= power;
        }
        *str = '\0';
}

int my_atoi(char *str)
{
        bool bmin = false;
        int result = 0;

        if ((*str > '9' || *str < '0') && (*str == '+' || *str == '-')) {
                if (*str == '-')
                        bmin = true;
                str++;
        }

        while (*str != '\0') {
                if (*str > '9' || *str < '0')
                        break;

                result = result * 10 + (*str++ - '0');
        }
        if (*str != '\0')
                return 0;

        return bmin ? -result : result;
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
        char *p, *sval;
        char buf[BUFLEN];
        char parse_buf[BUFLEN];
        int i = 0, j = 0;
        va_list ap;
        va_start(ap, fmt);

        memset(buf, '\0', BUFLEN);
        memset(parse_buf, '\0', BUFLEN);
        fread(buf, sizeof(char), sizeof(buf), f);

        // skip space
        for (; buf[i] == ' '; ++i)
                ;

        for (p = fmt; *p; ++p) {
                if (*p != '%') {
                        continue;
                }
                switch (*++p) {
                case 'd':
                        for (j = i; buf[j] >= '0' && buf[j] <= '9'; ++j) {
                                parse_buf[j - i] = buf[j];
                        }
                        *va_arg(ap, int *) = my_atoi(parse_buf);
                        i = j;
                        for (; buf[i] == ' '; ++i)
                                ;
                        memset(parse_buf, '\0', BUFLEN);
                        break;
                case 's':
                        for (sval = va_arg(ap, char *); buf[i] != ' '; sval++)
                                *sval = buf[i++];
                        for (; buf[i] == ' '; ++i)
                                ;
                        break;
                default:
                        break;
                }
        }
        va_end(ap);
        /* LAB 5 TODO END */
        return 0;
}

/* Need to support %s and %d. */
int fprintf(FILE *f, const char *fmt, ...)
{
        /* LAB 5 TODO BEGIN */
        char *p, *sval;
        char buf[BUFLEN];
        char itoa_buf[BUFLEN];
        int i = 0, ival = 0;
        va_list ap;
        va_start(ap, fmt);

        memset(buf, '\0', BUFLEN);
        for (p = fmt; *p; ++p) {
                if (*p != '%') {
                        buf[i++] = *p;
                        continue;
                }
                switch (*++p) {
                case 'd':
                        ival = va_arg(ap, int);
                        my_itoa(ival, itoa_buf);
                        strcat(buf, itoa_buf);
                        i += strlen(itoa_buf);
                        break;
                case 's':
                        for (sval = va_arg(ap, char *); *sval; sval++)
                                buf[i++] = *sval;
                        break;
                default:
                        buf[i++] = *p;
                        break;
                }
        }
        va_end(ap);
        fwrite(buf, sizeof(char), strlen(buf), f);
        /* LAB 5 TODO END */
        return 0;
}

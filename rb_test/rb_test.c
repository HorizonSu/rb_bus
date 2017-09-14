#include <stdio.h>
#include<stdlib.h>
#include<unistd.h>
#include<sys/types.h>
#include<sys/stat.h>
#include<fcntl.h>

#define	NUM		128

int main(int argc,char **argv)
{
    int i, fd, cnt;
    char key = 0;
    char flag = 1;
    unsigned char tmp = 0x50;
    unsigned char send_buf[NUM] = {0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA};
    unsigned char recv_buf[NUM] = {0};

    fd = open("/dev/rb_dev", O_RDWR);
    if (fd < 0) {
        printf("rb_dev: open failed=%d!\n",fd);
        return -1;
    } else
        printf("rb_dev: open success,fd=%d!\n",fd);

    while (flag) {
        printf("\n-------------------------------------------------------------------------------------------\n");
        printf(" 0: Exit    1: Send-1    2: Read    3:Send-10\n");

        key = getchar();
        getchar();

        switch(key)	{
        case '0':
            flag = 0;
            break;

        case '1':
            printf("Send:\n");

            cnt = write(fd, send_buf, 16);
            if (cnt != 16)
                printf("Send ERROR!!!\n");

            for (i = 0; i < cnt; i++)
                printf("%02X  ", send_buf[i]);
            printf("\n");

            break;

        case '2':
            printf("Read:\n");

            cnt = read(fd, recv_buf, NUM);

            for (i = 0; i < cnt; i++)
                printf("%02X  ", recv_buf[i]);
            printf("\n");

            break;

        case '3':
            for (i = 0; i < 10; i++) {
                cnt = write(fd, send_buf, 16);
                if (cnt != 16)
                    printf("Send ERROR!!!\n");
                printf("cnt: %d\n", cnt);
                usleep(50000);
            }

        case '4':
            cnt = write(fd, &tmp, 1);
            if (cnt != 1)
                printf("Send ERROR!!!\n");
            printf("cnt: %d -- data: %2X\n", cnt, send_buf[0]);

        default:
            break;
        }
    }

    close(fd);

    return 0;
}

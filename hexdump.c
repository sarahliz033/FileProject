#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <getopt.h>
#include <ctype.h>

/*
 * $ hexdump -s10 -l3 file
 * hexdump shows hex values of tiny file system blocks
 * hexdump shows hex valuse of any file in 512 byte blocks
 * -s10 is the start block, -s must include a number
 * -l3 is the number of blocks, -l must include a number
 * file is the tiny file system
 */

#define BSIZE 512

#define uint unsigned int

void panic(char *s) {
    printf("%s\n", s);
    exit(1);
}

int blocks = 10, start_block = 0;
char filename[100];

void panic();

int get_opts(int count, char *args[]) {
    int opt, len, i, good = 1;
    while (good && (opt = getopt(count, args, "s:l:")) != -1) {
        int len, i;
        switch (opt) {
            case 's':
                len = strlen(optarg);
                for (i=0;i<len; i++)
                    if (!isdigit(optarg[i])) {
                        fprintf(stderr, "-s value must be a number.\n");
                        good = 0;
                        break;
                    }
                if (good)
                    start_block = atoi(optarg);
                break;
            case 'l':
                len = strlen(optarg);
                for (i=0;i<len; i++)
                    if (!isdigit(optarg[i])) {
                        fprintf(stderr, "-l value must be a number.\n");
                        good = 0;
                        break;
                    }
                if (good)
                    blocks = atoi(optarg);
                break;
            case ':':
                fprintf(stderr, "option missing value\n");
                break;
            case '?':
                if (optopt == 'l')
                    fprintf(stderr, "Option -%c requires an argument.\n", optopt);
                else if (optopt == 's')
                    fprintf(stderr, "Option -%c requires an argument.\n", optopt);
                else if (isprint(optopt))
                    fprintf(stderr, "Unknown option `-%c'.\n", optopt);
                else
                   fprintf(stderr, "Unknown option character `\\x%x'.\n", optopt);
                good = 0;
                break;
        }
    }
    if(good && optind > count-1) {
        fprintf(stderr, "Invalid number of arguments. %d\n", optind);
        good = 0;
    }
    else if (good)
        strcpy(filename, args[optind]);
    return good;

}

int fs;

int bread(uint block, unsigned char *buf) {
    int off = lseek(fs, block*BSIZE, SEEK_SET);
    if (off < 0)
        panic("bread lseek fail");
    //printf("off : %d\n", off);
    memset(buf, 0, BSIZE);
    int sz = read(fs, buf, BSIZE);
    if (sz < 0)
        panic("bread read fail");
    //printf("sz : %d\n", sz);
    return sz;
}

int openfs(char *name) {
    fs = open(name, O_RDWR, S_IRUSR | S_IWUSR);
    if (fs < 0)
        panic("openfs open fail");
    return 0;
}

int closefs() {
    close(fs);
    return 0;
}


#define LINE 32

int main(int argc, char *argv[]) {

    int status = get_opts(argc, argv);
    if (!status)
        exit(-1);

    openfs(filename);

    unsigned char buf[BSIZE];
    for (int i = start_block; i < start_block + blocks; i++) {
        if (bread(i, buf) > 0) {
            printf("block: %05d: \n", i);
            for (int j = 0; j < BSIZE/LINE; j++) {
                printf("0x%08x  ", i*BSIZE+j*LINE);
                for (int k = 0; k < LINE; k++) {
                    printf("%02x", buf[j*LINE+k]);
                    if ((j*LINE+k+1) % 4 == 0)
                        printf(" ");
                }
                printf("  ");
                for (int k = 0; k < LINE; k++)
                    //if (buf[j*LINE+k] >= 'A' && buf[j*LINE+k] <= 'z')
                    if (isprint(buf[j*LINE+k]))
                        printf("%c", buf[j*LINE+k]);
                    else
                        printf(" ");
                printf("\n");
            }
        }
        else
            break; // reached EOF
    }
    closefs();
}
// end

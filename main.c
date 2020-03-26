#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <errno.h>
#include <string.h>
#include <getopt.h>

#ifdef _WIN32
#define _CRT_SECURE_NO_DEPRECATE
#endif

// Read a little endian unsigned int from a byte buffer
uint32_t readInt(const uint8_t *buffer, const uint32_t seek) {
    return ((buffer[seek + 3u] & 0xFFu) << 24u
            | (buffer[seek + 2u] & 0xFFu) << 16u
            | (buffer[seek + 1u] & 0xFFu) << 8u
            | (buffer[seek] & 0xFFu));
}

typedef struct {
    int8_t r;
    int8_t g;
    int8_t b;
    int8_t a;
} rgba_t;

const int paletteSize = 256;
const int headerSize = 0x20;

int main(int argc, char *argv[]) {
    char usage[] = "Usage: piftool [options] img.pif\n"
                   "Options:\n"
                   "   -s: Silent mode\n"
                   "   -f: Force mode\n";

    // Silent disables all printing, force disables all checks
    int opt, silent = 0, force = 0;

    while ((opt = getopt(argc, argv, "fs")) != -1) {
        switch (opt) {
            case 'f':
                force = 1;
                break;
            case 's':
                silent = 1;
                break;
            default:
                printf("%s", usage);
                exit(1);
        }
    }

    if (optind != argc - 1) {
        printf("%s", usage);
        exit(1);
    }

    char *filename = argv[optind];
    FILE *file = fopen(filename, "rb+");

    if (file == NULL) {
        if (silent == 0)
            printf("File error: %s\n%s", strerror(errno), usage);
        fclose(file);
        exit(1);
    }

    fseek(file, 0, SEEK_END);
    long filesize = ftell(file);

    if (force == 0 && filesize > 1000000) {
        if (silent == 0)
            printf("Error: File is over 1MB, so is most likely not a save game...\n");
        fclose(file);
        exit(1);
    }

    rewind(file);
    uint8_t buffer[filesize];
    size_t result = fread(buffer, 1, (size_t) filesize, file);
    fclose(file);

    if (result != filesize) {
        if (silent == 0)
            printf("Error: Only %ld bytes could be read\n", ftell(file));
        exit(1);
    }

    if (silent == 0) {
        printf("File read successfully, filesize: %ld\n", filesize);
    }

    const char* magic = "2FIP";
    int cmp = strncmp((const char*)buffer, magic, 4);
    if (cmp != 0) {
        if (silent == 0)
            printf("Error: invalid file magic\n");
        exit(1);
    }

    int width = readInt(buffer, 0x08);
    int height = readInt(buffer, 0x0c);
    int unk1 = readInt(buffer ,0x10);
    int unk2 = readInt(buffer, 0x14);
    int textureType = readInt(buffer, 0x18);
    int bpp = readInt(buffer, 0x1c);

    if (silent == 0) printf("Width: %d\nHeight: %d\nunk1: %d\nunk2: %d\ntextureType: %d\nbpp: %d\n",
            width, height, unk1, unk2, textureType, bpp);

    rgba_t palette[paletteSize];

    for (int i=0; i < paletteSize; i++) {
        palette[i] = (rgba_t){
            buffer[headerSize + i*4 + 0],
            buffer[headerSize + i*4 + 1],
            buffer[headerSize + i*4 + 2],
            buffer[headerSize + i*4 + 3]
        };
    }

    uint8_t bmpHeader[] = {
            0x42,0x4D,0x7A,0x00,0x03,0x00,0x00,0x00,0x00,0x00,0x7A,0x00,0x00,0x00,0x6C,0x00,
            0x00,0x00,0x00,0x01,0x00,0x00,0x00,0x01,0x00,0x00,0x01,0x00,0x18,0x00,0x00,0x00,
            0x00,0x00,0x00,0x00,0x03,0x00,0x23,0x2E,0x00,0x00,0x23,0x2E,0x00,0x00,0x00,0x00,
            0x00,0x00,0x00,0x00,0x00,0x00,0x42,0x47,0x52,0x73,0x00,0x00,0x00,0x00,0x00,0x00,
            0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
            0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
            0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x02,0x00,0x00,0x00,0x00,0x00,
            0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00
    };

    uint8_t bmpBody[width * height * 3];

    for (int i=0; i < width * height; i++) {
        rgba_t rgba = palette[buffer[headerSize + paletteSize*4 + i]];

        bmpBody[i*3 + 0] = rgba.b;
        bmpBody[i*3 + 1] = rgba.g;
        bmpBody[i*3 + 2] = rgba.r;
    }


    FILE *outFile = fopen("out.bmp", "wb+");
    fwrite(&bmpHeader, sizeof(bmpHeader), 1, outFile);
    fwrite(&bmpBody, sizeof(bmpBody), 1, outFile);
    fclose(outFile);

    return 0;
}
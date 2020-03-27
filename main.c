#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
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

// Snipped from https://stackoverflow.com/questions/43163677/how-do-i-strip-a-file-extension-from-a-string-in-c/43163761
void strip_ext(char *fname) {
    char *end = fname + strlen(fname);
    while (end > fname && *end != '.' && *end != '\\' && *end != '/') {
        --end;
    }
    if ((end > fname && *end == '.') &&
        (*(end - 1) != '\\' && *(end - 1) != '/')) {
        *end = '\0';
    }
}

typedef struct {
    int8_t r;
    int8_t g;
    int8_t b;
    int8_t a;
} rgba_t;

const int PALETTE_SIZE = 0x100;
const int HEADER_SIZE = 0x20;

int main(int argc, char *argv[]) {
    char usage[] = "Usage: piftool [options] <input> [<output>]\n"
                   "Options:\n"
                   "   -v: Verbose mode\n"
                   "   -f: Force mode\n";

    // verbose enables printing, force disables all checks
    int opt, verbose = 0, force = 0;

    while ((opt = getopt(argc, argv, "fv")) != -1) {
        switch (opt) {
            case 'f':
                force = 1;
                break;
            case 'v':
                verbose = 1;
                break;
            default:
                printf("%s", usage);
                exit(EXIT_FAILURE);
        }
    }

    char *inFileName = argv[optind];
    char *outFileName = argv[optind + 1];

    if (inFileName == NULL) {
        printf("%s", usage);
        exit(1);
    }

    // Generate a filename for the output if one isn't provided
    if (outFileName == NULL) {
        outFileName = strdup(inFileName);
        strip_ext(outFileName);
        strcat(outFileName, ".bmp");
    }

    FILE *file = fopen(inFileName, "rb");

    if (file == NULL) {
        perror("File error");
        exit(1);
    }

    fseek(file, 0, SEEK_END);
    size_t fileSize = ftell(file);
    rewind(file);
    uint8_t buffer[fileSize];

    if (fread(buffer, 1, (size_t) fileSize, file) != fileSize) {
        fprintf(stderr, "Error: Only %zu bytes could be read\n", fileSize);
        fclose(file);
        exit(EXIT_FAILURE);
    }

    fclose(file);

    if (verbose == 1) {
        printf("File read successfully, fileSize: %zu\n", fileSize);
    }

    if (strncmp((const char *) buffer, "2FIP", 4) != 0) {
        fprintf(stderr, "Error: Invalid file magic\n");
        exit(EXIT_FAILURE);
    }

    int width = readInt(buffer, 0x08);
    int height = readInt(buffer, 0x0c);
    int unk1 = readInt(buffer, 0x10);
    int unk2 = readInt(buffer, 0x14);
    int textureType = readInt(buffer, 0x18);
    int bpp = readInt(buffer, 0x1c);

    if (verbose == 1)
        printf("Width: %d\nHeight: %d\nunk1: %d\nunk2: %d\ntextureType: %d\nbpp: %d\n",
               width, height, unk1, unk2, textureType, bpp);

    rgba_t palette[PALETTE_SIZE];

    for (int i = 0; i < PALETTE_SIZE; i++) {
        int offset = i * 4;

        // Palette data is bgra
        palette[i] = (rgba_t) {
                buffer[HEADER_SIZE + offset + 2],
                buffer[HEADER_SIZE + offset + 1],
                buffer[HEADER_SIZE + offset + 0],
                buffer[HEADER_SIZE + offset + 3]
        };
    }

    uint8_t bmpHeader[] = {
            0x42, 0x4D, 0x7A, 0x00, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0x7A, 0x00, 0x00, 0x00, 0x6C, 0x00,
            0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x01, 0x00, 0x18, 0x00, 0x00, 0x00,
            0x00, 0x00, 0x00, 0x00, 0x03, 0x00, 0x23, 0x2E, 0x00, 0x00, 0x23, 0x2E, 0x00, 0x00, 0x00, 0x00,
            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x42, 0x47, 0x52, 0x73, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00,
            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
    };

    uint32_t imageEnd = HEADER_SIZE + PALETTE_SIZE * 4 + width * height;

    uint8_t bmpBody[width * height * 3];
    for (int i = 0; i < width * height; i++) {
        // Rotates and flips the image while getting the buffer position
        uint8_t pos = buffer[imageEnd - i + (i % width) * 2 - width];

        // Some weird color switcheroo that this format has
        rgba_t rgba;
        if ((pos & 0x10u) == 0x10 && (pos & 0x8u) == 0) {
            rgba = palette[pos - 8];
        } else if ((pos & 0x10u) == 0 && (pos & 0x8u) == 0x8) {
            rgba = palette[pos + 8];
        } else {
            rgba = palette[pos];
        }

        int offset = i * 3;
        bmpBody[offset + 0] = rgba.r;
        bmpBody[offset + 1] = rgba.g;
        bmpBody[offset + 2] = rgba.b;
    }

    uint32_t outFileSize = sizeof(bmpHeader) + sizeof(bmpBody);
    memcpy(&bmpHeader[0x02], &outFileSize, sizeof(outFileSize));
    memcpy(&bmpHeader[0x12], &width, sizeof(width));
    memcpy(&bmpHeader[0x16], &height, sizeof(height));

    FILE *outFile = fopen(outFileName, "wb+");
    fwrite(&bmpHeader, sizeof(bmpHeader), 1, outFile);
    fwrite(&bmpBody, sizeof(bmpBody), 1, outFile);
    fclose(outFile);

    return 0;
}
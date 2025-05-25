#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include "lib/quirc/quirc.h" // For quirc types if needed early, though not directly in load_bmp

// BMP File Header Structure (packed to ensure correct byte alignment)
#pragma pack(push, 1) // Exact fit
typedef struct {
    uint16_t type;      // Magic identifier: 0x4d42 (BM)
    uint32_t size;      // File size in bytes
    uint16_t reserved1; // Not used
    uint16_t reserved2; // Not used
    uint32_t offset;    // Offset to image data in bytes from beginning of file (54 bytes)
} BMPFileHeader;

typedef struct {
    uint32_t headerSize;    // Header size in bytes (40 bytes)
    int32_t  width;         // Image width in pixels
    int32_t  height;        // Image height in pixels
    uint16_t planes;        // Number of color planes (must be 1)
    uint16_t bitDepth;      // Bits per pixel (must be 24 for this project)
    uint32_t compression;   // Compression type (0 for no compression)
    uint32_t imageSize;     // Image size in bytes (may be 0 if no compression)
    int32_t  xResolution;   // Preferred resolution in pixels per meter
    int32_t  yResolution;   // Preferred resolution in pixels per meter
    uint32_t numColors;     // Number of entries in the color map (0 for 24-bit)
    uint32_t importantColors; // Number of important colors (0 when all are important)
} BMPInfoHeader;
#pragma pack(pop) // Back to whatever the previous packing mode was

/**
 * Loads a 24-bit BMP file, converts it to grayscale, and returns the image data.
 *
 * @param filename The path to the BMP file.
 * @param width Pointer to store the width of the image.
 * @param height Pointer to store the height of the image.
 * @param image_data Pointer to store the raw grayscale image data.
 *                   The caller is responsible for freeing this memory.
 * @return 0 on success, -1 on error (e.g., file not found, invalid format).
 */
int load_bmp(const char *filename, int *width, int *height, uint8_t **image_data) {
    FILE *file = fopen(filename, "rb");
    if (!file) {
        fprintf(stderr, "Error: Cannot open BMP file '%s'.\n", filename);
        return -1;
    }

    BMPFileHeader fileHeader;
    if (fread(&fileHeader, sizeof(BMPFileHeader), 1, file) != 1) {
        fprintf(stderr, "Error: Could not read BMP file header from '%s'.\n", filename);
        fclose(file);
        return -1;
    }

    if (fileHeader.type != 0x4D42) { // 'BM'
        fprintf(stderr, "Error: '%s' is not a valid BMP file (magic number mismatch).\n", filename);
        fclose(file);
        return -1;
    }

    BMPInfoHeader infoHeader;
    if (fread(&infoHeader, sizeof(BMPInfoHeader), 1, file) != 1) {
        fprintf(stderr, "Error: Could not read BMP info header from '%s'.\n", filename);
        fclose(file);
        return -1;
    }

    if (infoHeader.bitDepth != 24) {
        fprintf(stderr, "Error: '%s' is not a 24-bit BMP file. bitDepth: %d\n", filename, infoHeader.bitDepth);
        fclose(file);
        return -1;
    }

    if (infoHeader.compression != 0) {
        fprintf(stderr, "Error: Compressed BMP files are not supported ('%s').\n", filename);
        fclose(file);
        return -1;
    }

    *width = infoHeader.width;
    // BMP height can be negative for top-down images. We'll handle positive (bottom-up) only for simplicity here,
    // or take abs value. quirc expects top-down. For now, assume positive height means bottom-up.
    // We will store it as positive, and the reading loop will handle the bottom-up row order.
    *height = infoHeader.height > 0 ? infoHeader.height : -infoHeader.height;


    // Calculate row padding (each row must be a multiple of 4 bytes)
    int row_padded = (*width * 3 + 3) & (~3);
    // int image_size_bytes = row_padded * (*height); // This is the size of the BGR data in the file // This variable is not used

    *image_data = (uint8_t *)malloc((*width) * (*height));
    if (!(*image_data)) {
        fprintf(stderr, "Error: Could not allocate memory for image data.\n");
        fclose(file);
        return -1;
    }

    // Move file pointer to the beginning of pixel data
    fseek(file, fileHeader.offset, SEEK_SET);

    uint8_t *bgr_row_buffer = (uint8_t *)malloc(row_padded);
    if (!bgr_row_buffer) {
        fprintf(stderr, "Error: Could not allocate memory for BGR row buffer.\n");
        free(*image_data);
        fclose(file);
        return -1;
    }

    // Read pixel data row by row (BMPs are typically stored bottom-up)
    for (int y = 0; y < *height; y++) {
        // Read a full padded row of BGR data
        if (fread(bgr_row_buffer, 1, row_padded, file) != (size_t)row_padded) {
            fprintf(stderr, "Error: Could not read pixel data row from '%s'.\n", filename);
            free(bgr_row_buffer);
            free(*image_data);
            fclose(file);
            return -1;
        }

        // Convert BGR to grayscale and store it in the correct (top-down for quirc) order
        // If BMP height was positive (bottom-up), we write into grayscale_row_ptr from end to beginning.
        // If BMP height was negative (top-down), we write into grayscale_row_ptr from beginning to end.
        // For simplicity, we are assuming positive height means bottom-up, so we fill image_data accordingly.
        uint8_t *grayscale_row_ptr = (*image_data) + ((*height - 1 - y) * (*width));

        for (int x = 0; x < *width; x++) {
            uint8_t b = bgr_row_buffer[x * 3 + 0];
            uint8_t g = bgr_row_buffer[x * 3 + 1];
            uint8_t r = bgr_row_buffer[x * 3 + 2];
            // Grayscale conversion: Y = 0.299R + 0.587G + 0.114B
            grayscale_row_ptr[x] = (uint8_t)(0.299 * r + 0.587 * g + 0.114 * b);
        }
    }

    free(bgr_row_buffer);
    fclose(file);
    return 0; // Success
}

#include <string.h> // For memcpy and memset

// main function for QR code decoding
int main(int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <bmp_file>\n", argv[0]);
        return 1;
    }

    int width, height;
    uint8_t *image_data = NULL;

    if (load_bmp(argv[1], &width, &height, &image_data) != 0) {
        fprintf(stderr, "Failed to load BMP: %s\n", argv[1]);
        return 1;
    }
    // printf("Loaded BMP: %s, width: %d, height: %d\n", argv[1], width, height); // Optional: for debugging load_bmp

    struct quirc *qr;
    qr = quirc_new();
    if (!qr) {
        fprintf(stderr, "Error: Failed to allocate quirc struct.\n");
        free(image_data); // image_data was allocated by load_bmp
        return 1;
    }

    if (quirc_resize(qr, width, height) < 0) {
        fprintf(stderr, "Error: Failed to resize quirc object.\n");
        quirc_destroy(qr);
        free(image_data);
        return 1;
    }

    uint8_t *quirc_buffer = quirc_begin(qr, NULL, NULL); // Pass NULL for width/height pointers as it's already sized
    if (quirc_buffer == NULL) { // It's good practice to check if quirc_begin succeeded
        fprintf(stderr, "Error: quirc_begin failed to return a valid buffer.\n");
        quirc_destroy(qr);
        free(image_data);
        return 1;
    }
    memcpy(quirc_buffer, image_data, (size_t)width * height); // Copy grayscale data
    quirc_end(qr);

    int count = quirc_count(qr); // Get number of QR codes found
    if (count < 0) {
        fprintf(stderr, "Error during QR code recognition or internal quirc error.\n");
        quirc_destroy(qr);
        free(image_data);
        return 1;
    }

    if (count == 0) {
        printf("No QR codes found in the image.\n");
    } else {
        printf("Found %d QR code(s) in the image:\n", count);
        for (int i = 0; i < count; i++) {
            struct quirc_code code;
            struct quirc_data data;
            quirc_decode_error_t err;

            // It's good practice to zero out structs before use if they are filled by functions
            memset(&code, 0, sizeof(code));
            memset(&data, 0, sizeof(data));

            quirc_extract(qr, i, &code); // Extract the i-th code
            err = quirc_decode(&code, &data); // Decode it

            if (err == QUIRC_SUCCESS) {
                printf("  QR Code #%d: Payload: \"%s\"\n", i + 1, data.payload);
            } else {
                printf("  QR Code #%d: Decode failed: %s\n", i + 1, quirc_strerror(err));
            }
        }
    }

    quirc_destroy(qr);
    free(image_data); // Free the image data loaded by load_bmp

    return 0;
}

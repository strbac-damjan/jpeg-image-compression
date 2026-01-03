#include "quanitzation.h"

QuantizedImage* quantizeImage(const DCTImage* dctImg) {
    if (dctImg == NULL || dctImg->coefficients == NULL) return NULL;

    QuantizedImage* qImg = (QuantizedImage*)malloc(sizeof(QuantizedImage));
    if (qImg == NULL) return NULL;

    qImg->width = dctImg->width;
    qImg->height = dctImg->height;
    
    int totalPixels = qImg->width * qImg->height;
    qImg->data = (int16_t*)malloc(totalPixels * sizeof(int16_t));
    
    if (qImg->data == NULL) {
        free(qImg);
        return NULL;
    }

    for (int blockY = 0; blockY < dctImg->height; blockY += 8) {
        for (int blockX = 0; blockX < dctImg->width; blockX += 8) {
            
            //Inside one 8x8 block
            for (int u = 0; u < 8; u++) {     // Block row
                for (int v = 0; v < 8; v++) { // Block column
                    
                    // Get image index
                    int imageIndex = (blockY + u) * dctImg->width + (blockX + v);
                    
                    // Get index in quant table
                    int quantIndex = u * 8 + v;
                    
                    // Calculate value
                    float dctValue = dctImg->coefficients[imageIndex];
                    float quantStep = (float)std_luminance_quant_tbl[quantIndex];
                    qImg->data[imageIndex] = (int16_t)roundf(dctValue / quantStep);
                }
            }
        }
    }

    return qImg;
}

void freeQuantizedImage(QuantizedImage *img)
{
    if (img) {
        if (img->data) {
            free(img->data);
        }
        free(img);
    }
}

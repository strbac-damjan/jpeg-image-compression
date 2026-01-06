#include "dct.h"

// Table for C(u) and C(v) scaling factors
static const float C_LUT[8] = {
    0.707107f, 1.000000f, 1.000000f, 1.000000f, 1.000000f, 1.000000f, 1.000000f, 1.000000f
};

// Table for Cosine values: [spatial_index][frequency_index] -> [x][u]
static const float COS_LUT[8][8] = {
    { 1.000000f,  0.980785f,  0.923880f,  0.831470f,  0.707107f,  0.555570f,  0.382683f,  0.195090f},
    { 1.000000f,  0.831470f,  0.382683f, -0.195090f, -0.707107f, -0.980785f, -0.923880f, -0.555570f},
    { 1.000000f,  0.555570f, -0.382683f, -0.980785f, -0.707107f,  0.195090f,  0.923880f,  0.831470f},
    { 1.000000f,  0.195090f, -0.923880f, -0.555570f,  0.707107f,  0.831470f, -0.382683f, -0.980785f},
    { 1.000000f, -0.195090f, -0.923880f,  0.555570f,  0.707107f, -0.831470f, -0.382684f,  0.980785f},
    { 1.000000f, -0.555570f, -0.382684f,  0.980785f, -0.707107f, -0.195090f,  0.923880f, -0.831470f},
    { 1.000000f, -0.831470f,  0.382684f,  0.195091f, -0.707107f,  0.980785f, -0.923879f,  0.555570f},
    { 1.000000f, -0.980785f,  0.923880f, -0.831470f,  0.707107f, -0.555570f,  0.382684f, -0.195090f}
};

/*
Function used to compute the LUT components. no longer required
void initDCTTables()
{

    // Initialize C scaling factors
    C_LUT[0] = 1.0f / sqrtf(2.0f); // 0.707...
    for (int i = 1; i < 8; i++)
    {
        C_LUT[i] = 1.0f;
    }

    // Initialize Cosine Matrix
    // Formula: cos[x][u] = cos( (2x + 1) * u * PI / 16 )
    for (int spatial = 0; spatial < 8; spatial++)
    {
        for (int freq = 0; freq < 8; freq++)
        {
            COS_LUT[spatial][freq] = cosf(((2.0f * spatial + 1.0f) * freq * M_PI) / 16.0f);
        }
    }

    printf("C_LUT\n");
    for (int i = 0; i < 8; i++)
    {
        printf("%f ", C_LUT[i]);
    }
    printf("\n");

    printf("COS_LUT\n");
    for (int i = 0; i < 8; i++)
    {
        for (int j = 0; j < 8; j++)
        {
            printf("%f ", COS_LUT[i][j]);
        }
        printf("\n");
    }
}
*/



void computeDCTBlock(const int8_t inputBlock[8][8], float outputBlock[8][8])
{
    for (int u = 0; u < 8; u++)
    {
        for (int v = 0; v < 8; v++)
        {

            float sum = 0.0f;

            for (int x = 0; x < 8; x++)
            {
                for (int y = 0; y < 8; y++)
                {
                    // DCT Formula summation part
                    float pixel = (float)inputBlock[x][y];

                    // float cosX = cosf(((2.0f * x + 1.0f) * u * PI) / 16.0f);
                    // float cosY = cosf(((2.0f * y + 1.0f) * v * PI) / 16.0f);
                    float cosX = COS_LUT[x][u];
                    float cosY = COS_LUT[y][v];

                    sum += pixel * cosX * cosY;
                }
            }

            // Apply scaling factors
            float cu = C_LUT[u];
            float cv = C_LUT[v];

            // F(u,v) = 0.25 * C(u) * C(v) * sum
            outputBlock[u][v] = 0.25f * cu * cv * sum;
        }
    }
}

DCTImage *performDCT(const CenteredYImage *image)
{

    if (image == NULL || image->data == NULL)
        return NULL;

    DCTImage *dctImg = (DCTImage *)malloc(sizeof(DCTImage));
    if (dctImg == NULL)
        return NULL;

    dctImg->width = image->width;
    dctImg->height = image->height;
    dctImg->coefficients = (float *)malloc(image->width * image->height * sizeof(float));

    if (dctImg->coefficients == NULL)
    {
        free(dctImg);
        return NULL;
    }

    // Loop through blocks
    for (int y = 0; y <= image->height - 8; y += 8)
    {
        for (int x = 0; x <= image->width - 8; x += 8)
        {

            int8_t tempBlock[8][8];
            float dctBlock[8][8];

            // Extract block
            for (int by = 0; by < 8; by++)
            {
                for (int bx = 0; bx < 8; bx++)
                {
                    tempBlock[by][bx] = image->data[(y + by) * image->width + (x + bx)];
                }
            }

            // Compute DCT
            computeDCTBlock(tempBlock, dctBlock);

            // Store result
            for (int by = 0; by < 8; by++)
            {
                for (int bx = 0; bx < 8; bx++)
                {
                    dctImg->coefficients[(y + by) * image->width + (x + bx)] = dctBlock[by][bx];
                }
            }
        }
    }

    return dctImg;
}

void freeDCTImage(DCTImage *img)
{
    if (img)
    {
        if (img->coefficients)
        {
            free(img->coefficients);
        }
        free(img);
    }
}

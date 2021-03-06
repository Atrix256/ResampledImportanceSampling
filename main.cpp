#include <random>
#include <direct.h>
#include <vector>

#include "pcg/pcg_basic.h"

#define DETERMINISTIC() false

static const size_t c_histogramBuckets = 50;

static const float c_pi = 3.14159265359f;

pcg32_random_t GetRNG_PCG()
{
    pcg32_random_t rng;
#if DETERMINISTIC()
    pcg32_srandom_r(&rng, 0x1337FEED, 0);
#else
    std::random_device device;
    std::mt19937 generator(device());
    std::uniform_int_distribution<uint32_t> dist;
    pcg32_srandom_r(&rng, dist(generator), 0);
#endif
    return rng;
}

float RandomFloat01(pcg32_random_t& rng)
{
    return float(pcg32_random_r(&rng)) / 4294967295.0f;
}

std::mt19937 GetRNG()
{
    #if DETERMINISTIC()
    std::mt19937 rng;
    #else
    std::random_device rd;
    std::mt19937 rng(rd());
    #endif
    return rng;
}

template <float XMIN, float XMAX>
struct RNG_Uniform
{
    static float PDF(float x)
    {
        if (x < XMIN || x > XMAX)
            return 0.0f;
        return 1.0f / (XMAX - XMIN);
    }

    static float Generate(std::mt19937& rng)
    {
        std::uniform_real_distribution<float> dist(XMIN, XMAX);
        return dist(rng);
    }
};

template <float SIGMA>
struct RNG_Gaussian
{
    static float PDF(float x)
    {
        return 1.0f / (SIGMA * sqrtf(2.0f * c_pi)) * exp(-x * x / (2.0f * SIGMA * SIGMA));
    }

    static float Generate(std::mt19937& rng)
    {
        std::normal_distribution<float> dist(0.0f, SIGMA);
        return dist(rng);
    }
};

// Generated by inverting the CDF
// https://blog.demofox.org/2017/08/05/generating-random-numbers-from-a-specific-distribution-by-inverting-the-cdf/
struct RNG_XSquared
{
    static float PDF(float x)
    {
        if (x < 0.0f || x > 1.0f)
            return 0.0f;
        return 3.0f * x * x;
    }

    static float Generate(std::mt19937& rng)
    {
        std::uniform_real_distribution<float> dist(0.0f, 1.0f);
        return std::cbrtf(dist(rng));
    }
};

template <typename PDF>
void MakeHistogram(const std::vector<float>& values, const char* fileName)
{
    // make the histogram
    float minValue = values[0];
    float maxValue = values[1];
    for (float f : values)
    {
        minValue = std::min(minValue, f);
        maxValue = std::max(maxValue, f);
    }
    std::vector<int> histogram(c_histogramBuckets, 0);
    std::vector<float> histogramPDFs(c_histogramBuckets, 0.0f);
    for (float f : values)
    {
        int bucket = int(float(c_histogramBuckets) * (f - minValue) / (maxValue - minValue));
        bucket = std::min(bucket, int(c_histogramBuckets - 1));
        histogram[bucket]++;
    }
    float pdfSum = 0.0f;
    for (size_t index = 0; index < c_histogramBuckets; ++index)
    {
        float x = (float(index) + 0.5f) / float(c_histogramBuckets);
        x = x * (maxValue - minValue) + minValue;
        histogramPDFs[index] = PDF::PDF(x) / float(c_histogramBuckets);
        pdfSum += histogramPDFs[index];
    }
    // normalize these PDF values to make a PMF
    for (float& f : histogramPDFs)
        f /= pdfSum;

    FILE* file = nullptr;
    fopen_s(&file, fileName, "wb");
    fprintf(file, "\"Value\",\"Expected Count\",\"Actual Count\"\n");
    for (size_t index = 0; index < c_histogramBuckets; ++index)
    {
        float x = (float(index) + 0.5f) / float(c_histogramBuckets);
        x = x * (maxValue - minValue) + minValue;

        float pdf = histogramPDFs[index];

        int expectedCount = int(pdf * float(values.size()));

        fprintf(file, "\"%f\",\"%i\",\"%i\"\n", x, expectedCount, histogram[index]);
    }
    fclose(file);
}

template <typename PDF1, typename PDF2, size_t NUM_ITEMS, size_t NUM_TESTS>
void Test(const char* baseFileName)
{
    printf("%s...\n", baseFileName);
    std::mt19937 rng = GetRNG();
    pcg32_random_t rngpcg = GetRNG_PCG(); // faster

    // Generate the values
    struct Item
    {
        float value;
        float pdf1;
        float pdf2;
        float resampleWeight;
    };
    std::vector<Item> items(NUM_ITEMS);
    float resampleWeightSum = 0.0f;
    for (Item& item : items)
    {
        item.value = PDF1::Generate(rng);
        item.pdf1 = PDF1::PDF(item.value);
        item.pdf2 = PDF2::PDF(item.value);
        item.resampleWeight = item.pdf2 / item.pdf1;
        resampleWeightSum += item.resampleWeight;
    }
    for (Item& item : items)
        item.resampleWeight /= resampleWeightSum;

    //printf("%f\n", resampleWeightSum);

    // Output the samples to a csv
    {;
        char fileName[1024];
        sprintf_s(fileName, "out/%s.start.csv", baseFileName);
        std::vector<float> values(items.size());
        for (size_t i = 0; i < items.size(); ++i)
            values[i] = items[i].value;
        MakeHistogram<PDF1>(values, fileName);
    }

    // Sample the numbers using reservoir sampling, and output them to a csv
    {
        int lastPercent = -1;
        std::vector<float> values(NUM_TESTS);
        for (size_t testIndex = 0; testIndex < NUM_TESTS; ++testIndex)
        {
            int percent = int(100.0f * float(testIndex) / float(NUM_TESTS));
            if (lastPercent != percent)
            {
                lastPercent = percent;
                printf("\r%i%%", percent);
            }

            float selectedValue = 0.0f;
            float weightSum = 0.0f;

            for (size_t itemIndex = 0; itemIndex < NUM_ITEMS; ++itemIndex)
            {
                float weight = items[itemIndex].resampleWeight;
                weightSum += weight;

                float chance = weight / weightSum;
                if (RandomFloat01(rngpcg) < chance)
                    selectedValue = items[itemIndex].value;
            }
            values[testIndex] = selectedValue;
        }
        printf("\r100%%\n");

        char fileName[1024];
        sprintf_s(fileName, "out/%s.end.csv", baseFileName);
        MakeHistogram<PDF2>(values, fileName);
    }
}

int main(int argc, char** argv)
{
    _mkdir("out");

    // examples of it working
    Test<RNG_Uniform<-1.0f, 1.0f>, RNG_Gaussian<0.1f>, 100000, 10000>("UniformToGaussian");
    Test<RNG_Gaussian<1.0f>, RNG_XSquared, 100000, 10000>("GaussianToXSquared");

    // examples of it failing
    Test<RNG_Uniform<-0.1f, 0.3f>, RNG_Gaussian<0.1f>, 100000, 10000>("UniformToGaussianFail");
    Test<RNG_Uniform<-1.0f, 1.0f>, RNG_Gaussian<0.1f>, 500, 10000>("UniformToGaussianFail2");

    system("python makegraphs.py");
    return 0;
}

/*
Note:
* don't really need to normalize the resampleWeight since reservoir sampling doesn't care about it being normalized.
*/
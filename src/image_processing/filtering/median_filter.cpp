// SPDX-License-Identifier: CeCILL-2.1
// Copyright (C) 2010-2026 Fabien Bessy

#include "median_filter.hpp"
#include "image_alpha.hpp"

#include <cassert>

namespace fluvel_ip::filter
{

void median(const ImageView& input, ImageOwner& output, int radius)
{
    Median impl;

    impl.apply(input, radius);

    std::swap(output, impl.outputRef());
}

ImageOwner median(const ImageView& input, int radius)
{
    ImageOwner out;
    median(input, out, radius);
    return out;
}

void Median::reset(const ImageView& input)
{
    width_ = input.width();
    height_ = input.height();
    channels_ = input.channels();
    activeChannels_ = std::min(channels_, 3);

    if (!output_.hasSameLayout(input))
        output_ = ImageOwner::like(input);

    const size_t colHistoSize = static_cast<size_t>(width_) * static_cast<size_t>(kHistogramSize);

    if (columnsHisto_.size() != colHistoSize)
        columnsHisto_.resize(colHistoSize);
}

void Median::apply(const ImageView& input, int radius)
{
    // The fast path needs a window strictly smaller than the image.
    if (radius >= 3 && 2 * radius + 1 < std::min(input.width(), input.height()))
        applyPerreault(input, radius);
    else
        applyNaive(input, radius);
}

void Median::applyNaive(const ImageView& input, int radius)
{
    reset(input);

    assert(input.width() == width_);
    assert(input.height() == height_);
    assert(input.channels() == channels_);
    assert(radius >= 1);

    // The window is clipped to the image (same rule as the fast path).
    const int k = 2 * radius + 1;

    std::vector<uint8_t> window;
    window.reserve(static_cast<size_t>(k) * static_cast<size_t>(k));

    for (int y = 0; y < height_; ++y)
    {
        uint8_t* dst = output_.rowPtr(y);

        for (int x = 0; x < width_; ++x)
        {
            for (int ch = 0; ch < activeChannels_; ++ch)
            {
                window.clear();

                for (int yy = std::max(0, y - radius); yy <= std::min(height_ - 1, y + radius); ++yy)
                {
                    const uint8_t* row = input.row(yy);

                    for (int xx = std::max(0, x - radius); xx <= std::min(width_ - 1, x + radius);
                         ++xx)
                    {
                        window.push_back(row[xx * channels_ + ch]);
                    }
                }

                const size_t medianIndex = window.size() / 2;

                std::nth_element(window.begin(), window.begin() + medianIndex, window.end());

                dst[x * channels_ + ch] = window[medianIndex];
            }
        }
    }

    copyAlpha(input, output_);
}

uint8_t Median::findMedian(int targetRank)
{
    assert(targetRank > 0);

    int cumulative = 0;

    // 1. Cumuler jusqu'à currentMedian
    for (int i = 0; i <= currentMedian_; ++i)
    {
        cumulative += kernelHisto_[i];
    }

    // 2. Ajuster vers le haut
    if (cumulative < targetRank)
    {
        int i = currentMedian_;
        while (cumulative < targetRank && i < kHistogramSize - 1)
        {
            ++i;
            cumulative += kernelHisto_[i];
        }
        currentMedian_ = i;
    }
    else
    {
        // 3. Ajuster vers le bas
        int i = currentMedian_;
        while (i > 0 && cumulative - kernelHisto_[i] >= targetRank)
        {
            cumulative -= kernelHisto_[i];
            --i;
        }
        currentMedian_ = i;
    }

    return static_cast<uint8_t>(currentMedian_);
}

void Median::accumulateColumn(int colIndex)
{
    const int base = kHistogramSize * colIndex;

    for (int i = 0; i < kHistogramSize; ++i)
        kernelHisto_[i] += columnsHisto_[base + i];
}

void Median::removeColumn(int colIndex)
{
    const int base = kHistogramSize * colIndex;

    for (int i = 0; i < kHistogramSize; ++i)
    {
        kernelHisto_[i] -= columnsHisto_[base + i];
    }
}

void Median::updateKernel(int addColIndex, int removeColIndex)
{
    const int addBase = kHistogramSize * addColIndex;
    const int remBase = kHistogramSize * removeColIndex;

    for (int i = 0; i < kHistogramSize; ++i)
    {
        kernelHisto_[i] += columnsHisto_[addBase + i] - columnsHisto_[remBase + i];
    }
}

void Median::initColumnsHisto(const ImageView& input, int ch, int kernelSize)
{
    for (int y = 0; y < kernelSize; ++y)
    {
        const uint8_t* row = input.rowClamped(y);

        for (int x = 0; x < width_; ++x)
        {
            const int base = kHistogramSize * x;
            const uint8_t val = row[x * channels_ + ch];

            columnsHisto_[base + val]++;
        }
    }
}

void Median::updateColumnsHisto(int colIndex, uint8_t valRemove, uint8_t valAdd)
{
    const int base = kHistogramSize * colIndex;

    columnsHisto_[base + valRemove]--;
    columnsHisto_[base + valAdd]++;
}

void Median::addToColumnsHisto(int colIndex, uint8_t valAdd)
{
    columnsHisto_[kHistogramSize * colIndex + valAdd]++;
}

void Median::removeFromColumnsHisto(int colIndex, uint8_t valRemove)
{
    columnsHisto_[kHistogramSize * colIndex + valRemove]--;
}

void Median::applyPerreault(const ImageView& input, int radius)
{
    reset(input);

    assert(input.width() == width_);
    assert(input.height() == height_);
    assert(radius >= 1);

    // The window is clipped to the image: its number of rows and columns is smaller
    // than the kernel size near the borders, and the rank of the median follows.
    assert(2 * radius + 1 < std::min(width_, height_));

    const int kernelSize = 2 * radius + 1;

    for (int ch = 0; ch < activeChannels_; ++ch)
    {
        // ================= TOP =================
        // Rows [0, y + radius]: the column histograms start with the rows [0, radius]
        // and receive one more row per line.
        clearHistogram(columnsHisto_);

        initColumnsHisto(input, ch, radius + 1);

        for (int y = 0; y < radius + 1; ++y)
        {
            const int rows = y + radius + 1;
            const int medianRank = rows * kernelSize / 2 + 1;

            clearHistogram(kernelHisto_);
            initCurrentMedian();

            for (int x = 0; x < kernelSize; ++x)
            {
                if (y > 0)
                    addToColumnsHisto(x, input.at(x, y + radius, ch));

                accumulateColumn(x);

                if (x >= radius)
                {
                    const int accumulatedColumns = x + 1;
                    const int pixelCount = rows * accumulatedColumns;
                    const int rank = pixelCount / 2 + 1;

                    const uint8_t val = findMedian(rank);
                    output_.at(x - radius, y, ch) = val;
                }
            }

            for (int x = radius + 1; x < width_ - radius; ++x)
            {
                const int col = x + radius;

                if (y > 0)
                    addToColumnsHisto(col, input.at(col, y + radius, ch));

                updateKernel(col, x - radius - 1);

                const uint8_t val = findMedian(medianRank);
                output_.at(x, y, ch) = val;
            }

            int columnsRemoved = 0;
            for (int x = width_ - radius; x < width_; ++x)
            {
                removeColumn(x - radius - 1);
                ++columnsRemoved;

                const int remainingColumns = kernelSize - columnsRemoved;
                const int pixelCount = rows * remainingColumns;
                const int rank = pixelCount / 2 + 1;

                const uint8_t val = findMedian(rank);
                output_.at(x, y, ch) = val;
            }
        }

        // ================= MIDDLE =================
        // Rows [y - radius, y + radius]: the column histograms hold the rows [0, 2 * radius].
        // They are rebuilt from the state of the line y = radius.
        clearHistogram(columnsHisto_);

        initColumnsHisto(input, ch, kernelSize);

        for (int y = radius + 1; y < height_ - radius - 1; ++y)
        {
            uint8_t* rowOut = output_.rowPtr(y);

            const int medianRank = kernelSize * kernelSize / 2 + 1;

            clearHistogram(kernelHisto_);
            initCurrentMedian();

            for (int x = 0; x < kernelSize; ++x)
            {
                const uint8_t valRemove = input.at(x, y - radius - 1, ch);
                const uint8_t valAdd = input.at(x, y + radius, ch);

                updateColumnsHisto(x, valRemove, valAdd);

                accumulateColumn(x);

                if (x >= radius)
                {
                    const int accumulatedColumns = x + 1;
                    const int pixelCount = kernelSize * accumulatedColumns;
                    const int rank = pixelCount / 2 + 1;

                    const uint8_t val = findMedian(rank);
                    output_.at(x - radius, y, ch) = val;
                }
            }

            for (int x = radius + 1; x < width_ - radius; ++x)
            {
                const int col = x + radius;

                const uint8_t valRemove = input.at(col, y - radius - 1, ch);
                const uint8_t valAdd = input.at(col, y + radius, ch);

                updateColumnsHisto(col, valRemove, valAdd);

                updateKernel(col, x - radius - 1);

                rowOut[x * channels_ + ch] = findMedian(medianRank);
            }

            int columnsRemoved = 0;
            for (int x = width_ - radius; x < width_; ++x)
            {
                removeColumn(x - radius - 1);
                ++columnsRemoved;

                const int remainingColumns = kernelSize - columnsRemoved;
                const int pixelCount = kernelSize * remainingColumns;
                const int rank = pixelCount / 2 + 1;

                const uint8_t val = findMedian(rank);
                output_.at(x, y, ch) = val;
            }
        }

        // ================= BOTTOM =================
        // Rows [y - radius, height - 1]: the first line adds the last row, then rows are
        // only removed.
        for (int y = height_ - radius - 1; y < height_; ++y)
        {
            const int rows = height_ - y + radius;
            const int medianRank = rows * kernelSize / 2 + 1;

            clearHistogram(kernelHisto_);
            initCurrentMedian();

            for (int x = 0; x < kernelSize; ++x)
            {
                removeFromColumnsHisto(x, input.at(x, y - radius - 1, ch));

                if (y + radius < height_)
                    addToColumnsHisto(x, input.at(x, y + radius, ch));

                accumulateColumn(x);

                if (x >= radius)
                {
                    const int accumulatedColumns = x + 1;
                    const int pixelCount = rows * accumulatedColumns;
                    const int rank = pixelCount / 2 + 1;

                    const uint8_t val = findMedian(rank);
                    output_.at(x - radius, y, ch) = val;
                }
            }

            for (int x = radius + 1; x < width_ - radius; ++x)
            {
                const int col = x + radius;

                removeFromColumnsHisto(col, input.at(col, y - radius - 1, ch));

                if (y + radius < height_)
                    addToColumnsHisto(col, input.at(col, y + radius, ch));

                updateKernel(col, x - radius - 1);

                const uint8_t val = findMedian(medianRank);
                output_.at(x, y, ch) = val;
            }

            int columnsRemoved = 0;
            for (int x = width_ - radius; x < width_; ++x)
            {
                removeColumn(x - radius - 1);
                ++columnsRemoved;

                const int remainingColumns = kernelSize - columnsRemoved;
                const int pixelCount = rows * remainingColumns;
                const int rank = pixelCount / 2 + 1;

                const uint8_t val = findMedian(rank);
                output_.at(x, y, ch) = val;
            }
        }
    }

    copyAlpha(input, output_);
}

ImageView Median::outputView() const noexcept
{
    return output_.view();
}

const ImageOwner& Median::output() const noexcept
{
    return output_;
}

ImageOwner& Median::outputRef() noexcept
{
    return output_;
}

} // namespace fluvel_ip::filter

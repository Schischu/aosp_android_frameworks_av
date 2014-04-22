/*
**
** Copyright 2014, The Android Open Source Project
**
** Licensed under the Apache License, Version 2.0 (the "License");
** you may not use this file except in compliance with the License.
** You may obtain a copy of the License at
**
**     http://www.apache.org/licenses/LICENSE-2.0
**
** Unless required by applicable law or agreed to in writing, software
** distributed under the License is distributed on an "AS IS" BASIS,
** WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
** See the License for the specific language governing permissions and
** limitations under the License.
*/

#include <utils/RefBase.h>
#include <utils/Vector.h>

namespace android {
// ---------------------------------------------------------------------------

struct Interpolator : RefBase {
    virtual double interpolate(double x, double y) = 0;
    virtual void reset() = 0;
};

struct WindowedLinearInterpolator : Interpolator {
    struct LinearFit {
        /**
         * Fit y = a * x + b, where each input has a weight
         */
        double mX;  // sum(w_i * x_i)
        double mXX; // sum(w_i * x_i^2)
        double mY;  // sum(w_i * y_i)
        double mYY; // sum(w_i * y_i^2)
        double mXY; // sum(w_i * x_i * y_i)
        double mW;  // sum(w_i)

        LinearFit();
        void reset();
        void combine(const LinearFit &lf);
        void add(double x, double y, double w);
        void scale(double w);
        double interpolate(double x);
        double size() const;

        DISALLOW_EVIL_CONSTRUCTORS(LinearFit);
    };

    /**
     * Linear Interpolator for f(x) = y over a tapering rolling
     * window, where y =~ a * x + b.
     *
     *     ____________
     *    /|          |\
     *   / |          | \
     *  /  |          |  \   <--- new data
     * /   |   main   |   \
     * <--><----------><-->
     * tail            head
     *
     * weight is 1 under the main window, tapers exponentially by
     * the factors given in the head and the tail.
     */
    WindowedLinearInterpolator(
            size_t headLength = 5, double headFactor = 0.5,
            size_t mainLength = 0, double tailFactor = 0.99);

    virtual void reset();

    // add a new sample (x -> y) and return an interpolated value for x
    virtual double interpolate(double x, double y);

private:
    Vector<double> mXHistory; // circular buffer
    Vector<double> mYHistory; // circular buffer
    LinearFit mHead;
    LinearFit mMain;
    LinearFit mTail;
    double mHeadFactorInv;
    double mTailFactor;
    double mFirstWeight;
    size_t mHistoryLength;
    size_t mHeadLength;
    size_t mNumSamples;
    size_t mSampleIx;

    DISALLOW_EVIL_CONSTRUCTORS(WindowedLinearInterpolator);
};

}; // namespace android



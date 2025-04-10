// ======================================================================== //
// Copyright 2009-2019 Intel Corporation                                    //
//                                                                          //
// Licensed under the Apache License, Version 2.0 (the "License");          //
// you may not use this file except in compliance with the License.         //
// You may obtain a copy of the License at                                  //
//                                                                          //
//     http://www.apache.org/licenses/LICENSE-2.0                           //
//                                                                          //
// Unless required by applicable law or agreed to in writing, software      //
// distributed under the License is distributed on an "AS IS" BASIS,        //
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. //
// See the License for the specific language governing permissions and      //
// limitations under the License.                                           //
// ======================================================================== //

#include "transfer_function.h"

namespace oidn {

  const float LogTransferFunction::xScale = 1.f / log(LogTransferFunction::yMax + 1.f);
  const float PQXTransferFunction::xScale = 1.f / PQXTransferFunction::pqxForward(PQXTransferFunction::yMax * PQXTransferFunction::yScale);

  float AutoexposureNode::autoexposure(const Image& color)
  {
    assert(color.format == Format::Float3);

    constexpr float key = 0.18f;
    constexpr float eps = 1e-8f;
    constexpr int K = 16; // downsampling amount

    // Downsample the image to minimize sensitivity to noise
    const int H  = color.height;  // original height
    const int W  = color.width;   // original width
    const int HK = (H + K/2) / K; // downsampled height
    const int WK = (W + K/2) / K; // downsampled width

    // Compute the average log luminance of the downsampled image
    using Sum = std::pair<float, int>;

    Sum sum = Sum(0.0f, 0);
    {
      {
        {
          for (int i = 0; i != HK; ++i)
          {
            for (int j = 0; j != WK; ++j)
            {
              // Compute the average luminance in the current block
              const int beginH = int(ptrdiff_t(i)   * H / HK);
              const int beginW = int(ptrdiff_t(j)   * W / WK);
              const int endH   = int(ptrdiff_t(i+1) * H / HK);
              const int endW   = int(ptrdiff_t(j+1) * W / WK);

              float L = 0.f;

              for (int h = beginH; h < endH; ++h)
              {
                for (int w = beginW; w < endW; ++w)
                {
                  const float* rgb = (const float*)color.get(h, w);

                  const float r = maxSafe(rgb[0], 0.f);
                  const float g = maxSafe(rgb[1], 0.f);
                  const float b = maxSafe(rgb[2], 0.f);

                  L += luminance(r, g, b);
                }
              }

              L /= (endH - beginH) * (endW - beginW);

              // Accumulate the log luminance
              if (L > eps)
              {
                sum.first += log2(L);
                sum.second++;
              }
            }
          }
        }
      }
    }

    return (sum.second > 0) ? (key / exp2(sum.first / float(sum.second))) : 1.f;
  }

} // namespace oidn

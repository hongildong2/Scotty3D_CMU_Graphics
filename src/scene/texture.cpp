
#include "texture.h"

#include <algorithm>
#include <array>
#include <iostream>

namespace Textures {

Spectrum sample_nearest(HDR_Image const &image, Vec2 uv) {
  // clamp texture coordinates, convert to [0,w]x[0,h] pixel space:
  float x = image.w * std::clamp(uv.x, 0.0f, 1.0f);
  float y = image.h * std::clamp(uv.y, 0.0f, 1.0f);

  // the pixel with the nearest center is the pixel that contains (x,y):
  int32_t ix = int32_t(std::floor(x));
  int32_t iy = int32_t(std::floor(y));

  // texture coordinates of (1,1) map to (w,h), and need to be reduced:
  ix = std::min(ix, int32_t(image.w) - 1);
  iy = std::min(iy, int32_t(image.h) - 1);

  return image.at(ix, iy);
}

Spectrum sample_bilinear(HDR_Image const &image, Vec2 uv) {
  // A1T6: sample_bilinear
  // TODO: implement bilinear sampling strategy on texture 'image'
  int iw = image.w * uv.x;
  int ih = image.h * uv.y;

  float fw = static_cast<float>(image.w) * uv.x - static_cast<float>(iw);
  float fh = static_cast<float>(image.h) * uv.y - static_cast<float>(ih);

  // 4 nearest samples
  typedef std::array<std::pair<int, int>, 4> sample_offset_t;
  const sample_offset_t LEFT_UP = {{{-1, 1}, {0, 1}, {0, 0}, {-1, 0}}};
  const sample_offset_t LEFT_DOWN = {{{-1, 0}, {0, 0}, {0, -1}, {-1, -1}}};
  const sample_offset_t RIGHT_UP = {{{0, 1}, {1, 1}, {1, 0}, {0, 0}}};
  const sample_offset_t RIGHT_DOWN = {{{0, 0}, {1, 0}, {1, -1}, {0, -1}}};
  const sample_offset_t CENTER = {{{-1, 0}, {0, 0}, {0, -1}, {-1, -1}}};

  // 0,1 : left , 2,3: right, add +1 if down
  const sample_offset_t OFFSETS[5] = {LEFT_UP, LEFT_DOWN, RIGHT_UP, RIGHT_DOWN,
                                      CENTER};

  int offset_index = 0;
  if (fw > 0.5f) { // right
    offset_index = 2;
  } else {
    offset_index = 0;
  }

  if (fh < 0.5) { // down
    ++offset_index;
  }

  Spectrum res(0.f, 0.f, 0.f);

  const sample_offset_t &sample_offset = OFFSETS[offset_index];
  for (const std::pair<int, int> &offset : sample_offset) {
    std::pair<int, int> sample_pos = {
        std::clamp(std::max(iw + offset.first, 0), 0,
                   static_cast<int>(image.w - 1)),
        std::clamp(std::max(ih + offset.second, 0), 0,
                   static_cast<int>(image.h - 1))};

    res += image.at(sample_pos.first, sample_pos.second) * 0.25f;
  }

  return res;
}

Spectrum sample_trilinear(HDR_Image const &base,
                          std::vector<HDR_Image> const &levels, Vec2 uv,
                          float lod) {
  // A1T6: sample_trilinear
  // TODO: implement trilinear sampling strategy on using mip-map 'levels'

  if (lod <= 0.f) {
    return sample_bilinear(base, uv);
  }

  int floor = std::max(0, static_cast<int>(lod));
  int ceil = std::min(static_cast<int>(levels.size() - 1),
                      static_cast<int>(lod + 1.f));

  float a = lod - floor;
  float b = ceil - lod;
  if (a == 0.f) {
    return sample_bilinear(levels[lod], uv);
  }

  auto sampled_floor = sample_bilinear(levels[floor], uv);
  auto sampled_ceil = sample_bilinear(levels[ceil], uv);

  return (1.f - a) * sampled_floor + (1.f - b) * sampled_ceil;
}

/*
 * generate_mipmap- generate mipmap levels from a base image.
 *  base: the base image
 *  levels: pointer to vector of levels to fill (must not be null)
 *
 * generates a stack of levels [1,n] of sizes w_i, h_i, where:
 *   w_i = max(1, floor(w_{i-1})/2)
 *   h_i = max(1, floor(h_{i-1})/2)
 *  with:
 *   w_0 = base.w
 *   h_0 = base.h
 *  and n is the smalles n such that w_n = h_n = 1
 *
 * each level should be calculated by downsampling a blurred version
 * of the previous level to remove high-frequency detail.
 *
 */
void generate_mipmap(HDR_Image const &base, std::vector<HDR_Image> *levels_) {
  assert(levels_);
  auto &levels = *levels_;

  { // allocate sublevels sufficient to scale base image all the way to 1x1:
    int32_t num_levels =
        static_cast<int32_t>(std::log2(std::max(base.w, base.h)));
    assert(num_levels >= 0);

    levels.clear();
    levels.reserve(num_levels);

    uint32_t width = base.w;
    uint32_t height = base.h;
    for (int32_t i = 0; i < num_levels; ++i) {
      assert(!(width == 1 && height == 1)); // would have stopped before this if
                                            // num_levels was computed correctly

      width = std::max(1u, width / 2u);
      height = std::max(1u, height / 2u);

      levels.emplace_back(width, height);
    }
    assert(width == 1 && height == 1);
    assert(levels.size() == uint32_t(num_levels));
  }

  // now fill in the levels using a helper:
  // downsample:
  //  fill in dst to represent the low-frequency component of src
  auto downsample = [](HDR_Image const &src, HDR_Image &dst) {
    // dst is half the size of src in each dimension:
    assert(std::max(1u, src.w / 2u) == dst.w);
    assert(std::max(1u, src.h / 2u) == dst.h);

    // A1T6: generate
    // TODO: Write code to fill the levels of the mipmap hierarchy by
    // downsampling

    int dstW = dst.w;
    int dstH = dst.h;
    if (src.w % 2 == 1) {
      --dstW;
    }

    if (src.h % 2 == 1) {
      --dstH;
    }

    for (int w = 0; w < dstW; ++w) {
      for (int h = 0; h < dstH; ++h) {
        Spectrum sample(0.f, 0.f, 0.f);
        int src_x = 2 * w;
        int src_y = 2 * h;

        for (int i = 0; i < 2; ++i) {
          for (int j = 0; j < 2; ++j) {
            sample += src.at(src_x + i, src_y + j);
          }
        }

        dst.at(w, h) = sample / 4.f;
      }
    }

    // handling odd case

    int last_x = dst.w - 1;
    int last_y = dst.h - 1;
    if (src.w % 2 == 1) {
      // last column, 3x2
      for (int j = 0; j < dst.h; ++j) {

        Spectrum s(0.f, 0.f, 0.f);
        for (int x = 0; x < 3; ++x) {
          for (int y = 0; y < 2; ++y) {
            s += src.at(last_x * 2 + x, j * 2 + y);
          }
        }
        dst.at(last_x, j) = s / 6.f;
      }
    }

    if (src.h % 2 == 1) {
      // last row, 2x3

      for (int i = 0; i < dst.w; ++i) {

        Spectrum s(0.f, 0.f, 0.f);
        for (int x = 0; x < 2; ++x) {
          for (int y = 0; y < 3; ++y) {
            s += src.at(i * 2 + x, last_y * 2 + y);
          }
        }
        dst.at(i, last_y) = s / 6.f;
      }
    }

    if (src.w % 2 == 1 && src.h % 2 == 1) {
      // 3x3
      Spectrum s(0.f, 0.f, 0.f);

      for (int i = 0; i < 3; ++i) {
        for (int j = 0; j < 3; ++j) {
          s += src.at(last_x * 2 + i, last_y * 2 + j);
        }
      }
      dst.at(last_x, last_y) = s / 9.f;
    }

    // Be aware that the alignment of the samples in dst and src will be
    // different depending on whether the image is even or odd.
  };

  std::cout << "Regenerating mipmap (" << levels.size() << " levels): ["
            << base.w << "x" << base.h << "]";
  std::cout.flush();
  for (uint32_t i = 0; i < levels.size(); ++i) {
    HDR_Image const &src = (i == 0 ? base : levels[i - 1]);
    HDR_Image &dst = levels[i];
    std::cout << " -> [" << dst.w << "x" << dst.h << "]";
    std::cout.flush();

    downsample(src, dst);
  }
  std::cout << std::endl;
}

Image::Image(Sampler sampler_, HDR_Image const &image_) {
  sampler = sampler_;
  image = image_.copy();
  update_mipmap();
}

Spectrum Image::evaluate(Vec2 uv, float lod) const {
  if (image.w == 0 && image.h == 0)
    return Spectrum();
  if (sampler == Sampler::nearest) {
    return sample_nearest(image, uv);
  } else if (sampler == Sampler::bilinear) {
    return sample_bilinear(image, uv);
  } else {
    return sample_trilinear(image, levels, uv, lod);
  }
}

void Image::update_mipmap() {
  if (sampler == Sampler::trilinear) {
    generate_mipmap(image, &levels);
  } else {
    levels.clear();
  }
}

GL::Tex2D Image::to_gl() const { return image.to_gl(1.0f); }

void Image::make_valid() { update_mipmap(); }

Spectrum Constant::evaluate(Vec2 uv, float lod) const { return color * scale; }

} // namespace Textures
bool operator!=(const Textures::Constant &a, const Textures::Constant &b) {
  return a.color != b.color || a.scale != b.scale;
}

bool operator!=(const Textures::Image &a, const Textures::Image &b) {
  return a.image != b.image;
}

bool operator!=(const Texture &a, const Texture &b) {
  if (a.texture.index() != b.texture.index())
    return false;
  return std::visit(
      [&](const auto &data) {
        return data != std::get<std::decay_t<decltype(data)>>(b.texture);
      },
      a.texture);
}

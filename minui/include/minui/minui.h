/*
 * Copyright (C) 2007 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#pragma once

#include <stdint.h>
#include <stdlib.h>
#include <sys/types.h>

#include <functional>
#include <memory>
#include <string>
#include <vector>

#include <android-base/macros.h>
#include <android-base/unique_fd.h>

//
// Graphics.
//

class GRSurface {
 public:
  static constexpr size_t kSurfaceDataAlignment = 8;

  virtual ~GRSurface() = default;

  // Creates and returns a GRSurface instance that's sufficient for storing an image of the given
  // size (i.e. row_bytes * height). The starting address of the surface data is aligned to
  // kSurfaceDataAlignment. Returns the created GRSurface instance (in std::unique_ptr), or nullptr
  // on error.
  static std::unique_ptr<GRSurface> Create(size_t width, size_t height, size_t row_bytes,
                                           size_t pixel_bytes);

  // Clones the current GRSurface instance (i.e. an image).
  std::unique_ptr<GRSurface> Clone() const;

  virtual uint8_t* data() {
    return data_.get();
  }

  const uint8_t* data() const {
    return const_cast<const uint8_t*>(const_cast<GRSurface*>(this)->data());
  }

  size_t data_size() const {
    return data_size_;
  }

  size_t width;
  size_t height;
  size_t row_bytes;
  size_t pixel_bytes;

 protected:
  GRSurface(size_t width, size_t height, size_t row_bytes, size_t pixel_bytes)
      : width(width), height(height), row_bytes(row_bytes), pixel_bytes(pixel_bytes) {}

 private:
  // The deleter for data_, whose data is allocated via aligned_alloc(3).
  struct DataDeleter {
    void operator()(uint8_t* data) {
      free(data);
    }
  };

  std::unique_ptr<uint8_t, DataDeleter> data_;
  size_t data_size_;

  DISALLOW_COPY_AND_ASSIGN(GRSurface);
};

struct GRFont {
  GRSurface* texture;
  int char_width;
  int char_height;
};

enum class GRRotation : int {
  NONE = 0,
  RIGHT = 1,
  DOWN = 2,
  LEFT = 3,
};

enum class PixelFormat : int {
  UNKNOWN = 0,
  ABGR = 1,
  RGBX = 2,
  BGRA = 3,
  ARGB = 4,
  RGBA = 5, // LSB Alpha
};

enum class GraphicsBackend : int {
  UNKNOWN = 0,
  DRM = 1,
  FBDEV = 2,
};

// Initializes the default graphics backend and loads font file. Returns 0 on success, or -1 on
// error. Note that the font initialization failure would be non-fatal, as caller may not need to
// draw any text at all. Caller can check the font initialization result via gr_sys_font() as
// needed.
int gr_init();
// Supports backend selection for minui client.
int gr_init(std::initializer_list<GraphicsBackend> backends);

// Frees the allocated resources. The function is idempotent, and safe to be called if gr_init()
// didn't finish successfully.
void gr_exit();

int gr_fb_width();
int gr_fb_height();

void gr_flip();
void gr_fb_blank(bool blank);
void gr_fb_blank(bool blank, int index);
bool gr_has_multiple_connectors();

// Clears entire surface to current color.
void gr_clear();
void gr_color(unsigned char r, unsigned char g, unsigned char b, unsigned char a);
void gr_fill(int x1, int y1, int x2, int y2);

void gr_texticon(int x, int y, const GRSurface* icon);

const GRFont* gr_sys_font();
int gr_init_font(const char* name, GRFont** dest);
void gr_text(const GRFont* font, int x, int y, const char* s, bool bold);
// Returns -1 if font is nullptr.
int gr_measure(const GRFont* font, const char* s);
// Returns -1 if font is nullptr.
int gr_font_size(const GRFont* font, int* x, int* y);

void gr_blit(const GRSurface* source, int sx, int sy, int w, int h, int dx, int dy);
unsigned int gr_get_width(const GRSurface* surface);
unsigned int gr_get_height(const GRSurface* surface);

// Sets rotation, flips gr_fb_width/height if 90 degree rotation difference
void gr_rotate(GRRotation rotation);

// Get current rotation
GRRotation gr_get_rotation();

// Returns the current PixelFormat being used.
PixelFormat gr_pixel_format();

//
// Input events.
//

struct input_event;

using ev_callback = std::function<int(int fd, uint32_t epevents)>;
using ev_set_key_callback = std::function<int(int code, int value)>;
using ev_set_sw_callback = std::function<int(int code, int value)>;

int ev_init(ev_callback input_cb, bool allow_touch_inputs = false);
void ev_exit();
int ev_add_fd(android::base::unique_fd&& fd, ev_callback cb);
void ev_iterate_available_keys(const std::function<void(int)>& key_detected);
void ev_iterate_touch_inputs(const std::function<void(int)>& touch_device_detected,
                             const std::function<void(int)>& key_detected);
int ev_sync_key_state(const ev_set_key_callback& set_key_cb);
int ev_sync_sw_state(const ev_set_sw_callback& set_sw_cb);

// 'timeout' has the same semantics as poll(2).
//    0 : don't block
//  < 0 : block forever
//  > 0 : block for 'timeout' milliseconds
int ev_wait(int timeout);

int ev_get_input(int fd, uint32_t epevents, input_event* ev);
void ev_dispatch();
int ev_get_epollfd();

//
// Resources
//

bool matches_locale(const std::string& prefix, const std::string& locale);

// res_create_*_surface() functions return 0 if no error, else
// negative.
//
// A "display" surface is one that is intended to be drawn to the
// screen with gr_blit().  An "alpha" surface is a grayscale image
// interpreted as an alpha mask used to render text in the current
// color (with gr_text() or gr_texticon()).
//
// All these functions load PNG images from "/res/images/${name}.png".

// Load a single display surface from a PNG image.
int res_create_display_surface(const char* name, GRSurface** pSurface);

// Load an array of display surfaces from a single PNG image.  The PNG
// should have a 'Frames' text chunk whose value is the number of
// frames this image represents.  The pixel data itself is interlaced
// by row.
int res_create_multi_display_surface(const char* name, int* frames, int* fps,
                                     GRSurface*** pSurface);

// Load a single alpha surface from a grayscale PNG image.
int res_create_alpha_surface(const char* name, GRSurface** pSurface);

// Load part of a grayscale PNG image that is the first match for the
// given locale.  The image is expected to be a composite of multiple
// translations of the same text, with special added rows that encode
// the subimages' size and intended locale in the pixel data.  See
// bootable/recovery/tools/recovery_l10n for an app that will generate
// these specialized images from Android resources.
int res_create_localized_alpha_surface(const char* name, const char* locale,
                                       GRSurface** pSurface);

// Return a list of locale strings embedded in |png_name|. Return a empty list in case of failure.
std::vector<std::string> get_locales_in_png(const std::string& png_name);

// Free a surface allocated by any of the res_create_*_surface()
// functions.
void res_free_surface(GRSurface* surface);

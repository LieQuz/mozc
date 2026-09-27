// Copyright 2010-2021, Google Inc.
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//     * Neither the name of Google Inc. nor the names of its
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#ifndef MOZC_RENDERER_WIN32_WIN32_DPI_UTIL_H_
#define MOZC_RENDERER_WIN32_WIN32_DPI_UTIL_H_

#include <cstdint>

#include "protocol/renderer_style.pb.h"

namespace mozc {
namespace renderer {
namespace win32 {

// Returns the DPI scaling factor for |dpi| (i.e. |dpi| / 96.0).
double GetDPIScalingFactor(uint32_t dpi);

// Returns the effective DPI of the monitor containing the given screen
// coordinates. Falls back to USER_DEFAULT_SCREEN_DPI on failure.
uint32_t GetDpiForPoint(int x, int y);

// Corner radius of the candidate window and mode indicator at |dpi|.
// 12px at 96 DPI, close to a macOS panel.
int GetWindowCornerRadiusPx(uint32_t dpi);

// Extra space between shortcut, candidate, and description columns at |dpi|.
int GetColumnGapPx(uint32_t dpi);

// "Yu Gothic UI" when that face is installed. Otherwise nullptr, and callers
// keep the system message font.
const wchar_t* GetUiFontFaceName();

// Colors shared by the candidate window and the mode indicator. Values follow
// the app light/dark setting (AppsUseLightTheme).
struct WindowsUiColors {
  int background_r;
  int background_g;
  int background_b;
  int border_r;
  int border_g;
  int border_b;
  int text_r;
  int text_g;
  int text_b;
  int description_r;
  int description_g;
  int description_b;
  int shortcut_r;
  int shortcut_g;
  int shortcut_b;
  int focused_r;
  int focused_g;
  int focused_b;
  int scrollbar_track_r;
  int scrollbar_track_g;
  int scrollbar_track_b;
  int scrollbar_thumb_r;
  int scrollbar_thumb_g;
  int scrollbar_thumb_b;
  int separator_r;
  int separator_g;
  int separator_b;
};

WindowsUiColors GetWindowsUiColors();

// Populates |style| with the default RendererStyle, then applies the Windows
// visual overrides (colors, padding, font) and scales the result for |dpi|.
void GetScaledRendererStyle(::mozc::renderer::RendererStyle* style,
                            uint32_t dpi);

}  // namespace win32
}  // namespace renderer
}  // namespace mozc

#endif  // MOZC_RENDERER_WIN32_WIN32_DPI_UTIL_H_

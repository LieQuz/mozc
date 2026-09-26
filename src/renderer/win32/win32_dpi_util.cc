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

#include "renderer/win32/win32_dpi_util.h"

#include <shellscalingapi.h>
#include <windows.h>

#include <algorithm>
#include <cmath>
#include <cstdint>

#include "protocol/renderer_style.pb.h"
#include "renderer/renderer_style_handler.h"

namespace mozc {
namespace renderer {
namespace win32 {
namespace {

constexpr int kCornerRadiusAt96Dpi = 8;
constexpr int kColumnGapAt96Dpi = 6;
constexpr int kRowPaddingAt96Dpi = 6;
constexpr wchar_t kUiFontFace[] = L"Yu Gothic UI";

struct Rgb {
  int r;
  int g;
  int b;
};

void SetColor(RendererStyle::RGBAColor* color, Rgb rgb) {
  color->set_r(rgb.r);
  color->set_g(rgb.g);
  color->set_b(rgb.b);
  color->set_a(1.0);
}

void ScaleTextStyle(RendererStyle::TextStyle* text_style, double scale_factor) {
  text_style->set_font_size(text_style->font_size() * scale_factor);
  text_style->set_left_padding(text_style->left_padding() * scale_factor);
  text_style->set_right_padding(text_style->right_padding() * scale_factor);
}

int Scale96(int value_at_96dpi, uint32_t dpi) {
  return std::max(
      1, static_cast<int>(std::lround(value_at_96dpi * GetDPIScalingFactor(dpi))));
}

int CALLBACK FontExistsProc(const LOGFONTW* /*log_font*/,
                            const TEXTMETRICW* /*metric*/, DWORD /*font_type*/,
                            LPARAM param) {
  *reinterpret_cast<bool*>(param) = true;
  return 0;
}

bool IsFontInstalled(const wchar_t* face_name) {
  LOGFONTW query = {};
  query.lfCharSet = DEFAULT_CHARSET;
  if (wcscpy_s(query.lfFaceName, face_name) != 0) {
    return false;
  }
  bool found = false;
  const HDC dc = ::GetDC(nullptr);
  ::EnumFontFamiliesExW(dc, &query, FontExistsProc,
                        reinterpret_cast<LPARAM>(&found), 0);
  ::ReleaseDC(nullptr, dc);
  return found;
}

bool AppsUseDarkTheme() {
  HKEY key = nullptr;
  if (::RegOpenKeyExW(
          HKEY_CURRENT_USER,
          L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize",
          0, KEY_READ, &key) != ERROR_SUCCESS) {
    return false;
  }
  DWORD value = 1;
  DWORD size = sizeof(value);
  DWORD type = 0;
  const LSTATUS status = ::RegQueryValueExW(
      key, L"AppsUseLightTheme", nullptr, &type,
      reinterpret_cast<LPBYTE>(&value), &size);
  ::RegCloseKey(key);
  return status == ERROR_SUCCESS && type == REG_DWORD && value == 0;
}

bool TryGetAccentColor(Rgb* accent) {
  HKEY key = nullptr;
  if (::RegOpenKeyExW(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\DWM",
                      0, KEY_READ, &key) != ERROR_SUCCESS) {
    return false;
  }
  DWORD value = 0;
  DWORD size = sizeof(value);
  DWORD type = 0;
  const LSTATUS status =
      ::RegQueryValueExW(key, L"AccentColor", nullptr, &type,
                         reinterpret_cast<LPBYTE>(&value), &size);
  ::RegCloseKey(key);
  if (status != ERROR_SUCCESS || type != REG_DWORD) {
    return false;
  }
  // AccentColor is stored as ABGR.
  accent->r = static_cast<int>(value & 0xff);
  accent->g = static_cast<int>((value >> 8) & 0xff);
  accent->b = static_cast<int>((value >> 16) & 0xff);
  return true;
}

Rgb Blend(Rgb base, Rgb accent, double accent_amount) {
  const auto channel = [accent_amount](int base_channel, int accent_channel) {
    const double mixed = base_channel * (1.0 - accent_amount) +
                         accent_channel * accent_amount;
    return std::clamp(static_cast<int>(std::lround(mixed)), 0, 255);
  };
  return Rgb{channel(base.r, accent.r), channel(base.g, accent.g),
             channel(base.b, accent.b)};
}

void ApplyFontFace(RendererStyle::TextStyle* text_style) {
  if (GetUiFontFaceName() == nullptr) {
    return;
  }
  text_style->set_font_name("Yu Gothic UI");
}

void ApplyWindowsVisualStyle(RendererStyle* style) {
  const WindowsUiColors colors = GetWindowsUiColors();
  const Rgb background = {colors.background_r, colors.background_g,
                          colors.background_b};
  const Rgb border = {colors.border_r, colors.border_g, colors.border_b};
  const Rgb text = {colors.text_r, colors.text_g, colors.text_b};
  const Rgb description = {colors.description_r, colors.description_g,
                           colors.description_b};
  const Rgb shortcut = {colors.shortcut_r, colors.shortcut_g,
                        colors.shortcut_b};
  const Rgb focused = {colors.focused_r, colors.focused_g, colors.focused_b};
  const Rgb track = {colors.scrollbar_track_r, colors.scrollbar_track_g,
                     colors.scrollbar_track_b};
  const Rgb thumb = {colors.scrollbar_thumb_r, colors.scrollbar_thumb_g,
                     colors.scrollbar_thumb_b};
  const Rgb separator = {colors.separator_r, colors.separator_g,
                         colors.separator_b};

  style->set_row_rect_padding(kRowPaddingAt96Dpi);
  style->set_scrollbar_width(6);
  SetColor(style->mutable_border_color(), border);
  SetColor(style->mutable_focused_background_color(), focused);
  SetColor(style->mutable_focused_border_color(), focused);
  SetColor(style->mutable_scrollbar_background_color(), track);
  SetColor(style->mutable_scrollbar_indicator_color(), thumb);
  SetColor(style->mutable_footer_top_color(), background);
  SetColor(style->mutable_footer_bottom_color(), background);
  style->clear_footer_border_colors();
  SetColor(style->add_footer_border_colors(), separator);

  RendererStyle::TextStyle* shortcut_style = style->mutable_shortcut_style();
  SetColor(shortcut_style->mutable_foreground_color(), shortcut);
  SetColor(shortcut_style->mutable_background_color(), background);
  shortcut_style->set_left_padding(12);
  shortcut_style->set_right_padding(12);
  ApplyFontFace(shortcut_style);

  RendererStyle::TextStyle* candidate_style = style->mutable_candidate_style();
  SetColor(candidate_style->mutable_foreground_color(), text);
  SetColor(candidate_style->mutable_background_color(), background);
  ApplyFontFace(candidate_style);

  RendererStyle::TextStyle* description_style =
      style->mutable_description_style();
  SetColor(description_style->mutable_foreground_color(), description);
  description_style->set_right_padding(12);
  ApplyFontFace(description_style);

  RendererStyle::TextStyle* footer_style = style->mutable_footer_style();
  SetColor(footer_style->mutable_foreground_color(), description);
  ApplyFontFace(footer_style);

  RendererStyle::TextStyle* footer_sub_style =
      style->mutable_footer_sub_label_style();
  SetColor(footer_sub_style->mutable_foreground_color(), description);
  ApplyFontFace(footer_sub_style);
}

}  // namespace

double GetDPIScalingFactor(uint32_t dpi) {
  return static_cast<double>(dpi) / USER_DEFAULT_SCREEN_DPI;
}

uint32_t GetDpiForPoint(int x, int y) {
  const POINT point = {x, y};
  const HMONITOR monitor = ::MonitorFromPoint(point, MONITOR_DEFAULTTONEAREST);
  UINT dpi_x = USER_DEFAULT_SCREEN_DPI;
  UINT dpi_y = USER_DEFAULT_SCREEN_DPI;
  if (FAILED(::GetDpiForMonitor(monitor, MDT_EFFECTIVE_DPI, &dpi_x, &dpi_y))) {
    return USER_DEFAULT_SCREEN_DPI;
  }
  return dpi_x;
}

int GetWindowCornerRadiusPx(uint32_t dpi) {
  return Scale96(kCornerRadiusAt96Dpi, dpi);
}

int GetColumnGapPx(uint32_t dpi) { return Scale96(kColumnGapAt96Dpi, dpi); }

const wchar_t* GetUiFontFaceName() {
  static const bool installed = IsFontInstalled(kUiFontFace);
  return installed ? kUiFontFace : nullptr;
}

WindowsUiColors GetWindowsUiColors() {
  const bool dark = AppsUseDarkTheme();
  const Rgb background = dark ? Rgb{44, 44, 44} : Rgb{255, 255, 255};
  const Rgb light_focus_fallback = {209, 234, 255};
  const Rgb dark_focus_fallback = {36, 64, 92};
  Rgb focused = dark ? dark_focus_fallback : light_focus_fallback;
  Rgb accent = {};
  if (TryGetAccentColor(&accent)) {
    focused = Blend(background, accent, dark ? 0.38 : 0.28);
  }

  if (dark) {
    return WindowsUiColors{
        background.r, background.g, background.b, 70,  70,  70,  255, 255, 255,
        180,          180,          180,          200, 200, 200, focused.r,
        focused.g,    focused.b,    32,           32,  32,  140, 140, 140, 70,
        70,           70};
  }
  return WindowsUiColors{background.r,
                         background.g,
                         background.b,
                         229,
                         229,
                         229,
                         32,
                         32,
                         32,
                         110,
                         110,
                         110,
                         96,
                         96,
                         96,
                         focused.r,
                         focused.g,
                         focused.b,
                         240,
                         240,
                         240,
                         180,
                         180,
                         180,
                         229,
                         229,
                         229};
}

void GetScaledRendererStyle(::mozc::renderer::RendererStyle* style,
                            uint32_t dpi) {
  const double scale_factor = GetDPIScalingFactor(dpi);

  RendererStyleHandler::GetRendererStyle(style);
  ApplyWindowsVisualStyle(style);

  // style->window_border is non-scalable.
  style->set_scrollbar_width(style->scrollbar_width() * scale_factor);
  style->set_row_rect_padding(style->row_rect_padding() * scale_factor);

  ScaleTextStyle(style->mutable_shortcut_style(), scale_factor);
  ScaleTextStyle(style->mutable_gap1_style(), scale_factor);
  ScaleTextStyle(style->mutable_candidate_style(), scale_factor);
  ScaleTextStyle(style->mutable_description_style(), scale_factor);

  ScaleTextStyle(style->mutable_footer_style(), scale_factor);
  ScaleTextStyle(style->mutable_footer_sub_label_style(), scale_factor);

  RendererStyle::InfolistStyle* info_style = style->mutable_infolist_style();
  // info_style->window_border and info_style->caption_padding are non-scalable.
  info_style->set_caption_height(info_style->caption_height() * scale_factor);
  info_style->set_row_rect_padding(info_style->row_rect_padding() *
                                   scale_factor);
  info_style->set_window_width(info_style->window_width() * scale_factor);

  ScaleTextStyle(info_style->mutable_caption_style(), scale_factor);
  ScaleTextStyle(info_style->mutable_title_style(), scale_factor);
  ScaleTextStyle(info_style->mutable_description_style(), scale_factor);
}

}  // namespace win32
}  // namespace renderer
}  // namespace mozc

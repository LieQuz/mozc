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

#import "renderer/mac/CandidateView.h"

#import <Foundation/Foundation.h>

#include <algorithm>
#include <set>

#include "absl/log/log.h"
#include "absl/strings/str_format.h"
#include "client/client_interface.h"
#include "protocol/commands.pb.h"
#include "protocol/renderer_style.pb.h"
#include "renderer/mac/mac_view_util.h"
#include "renderer/renderer_style_handler.h"
#include "renderer/table_layout.h"

using mozc::client::SendCommandInterface;
using mozc::commands::CandidateWindow;
using mozc::commands::Output;
using mozc::commands::SessionCommand;
using mozc::renderer::RendererStyleHandler;
using mozc::renderer::ColumnType;
using mozc::renderer::kColumnShortcut;
using mozc::renderer::kColumnGap1;
using mozc::renderer::kColumnCandidate;
using mozc::renderer::kColumnDescription;
using mozc::renderer::kNumberOfColumns;
using mozc::renderer::TableLayout;
using mozc::renderer::mac::MacTextTone;
using mozc::renderer::mac::MacViewUtil;

constexpr CGFloat kCandidateFontSize = 13.0;
constexpr CGFloat kDescriptionFontSize = 12.0;
constexpr CGFloat kFooterFontSize = 12.0;
constexpr CGFloat kSelectionRadius = 6.0;
constexpr CGFloat kSelectionInsetX = 4.0;
constexpr CGFloat kSelectionInsetY = 1.0;

// Private method declarations.
@interface CandidateView ()
- (void)initializeDefaultStyle;

// Draw the |row|-th row.
- (void)drawRow:(int)row;

// Draw footer
- (void)drawFooter;

// Draw scroll bar
- (void)drawVScrollBar;
@end

@implementation CandidateView {
  const NSImage *logoImage_;
  int columnMinimumWidth_;

  mozc::commands::CandidateWindow candidate_window_;
  mozc::renderer::TableLayout tableLayout_;
  mozc::renderer::RendererStyle style_;

  // The row which has focused background.
  int focusedRow_;

  // Cache of attributed strings which is allocated at updateLayout.
  NSArray *candidateStringsCache_;

  // |command_sender_| holds a callback for mouse clicks.
  mozc::client::SendCommandInterface *command_sender_;
}

#pragma mark initialization

- (id)initWithFrame:(NSRect)frame {
  self = [super initWithFrame:frame];
  if (self) {
    [self initializeDefaultStyle];
    focusedRow_ = -1;
  }
  return self;
}

- (void)initializeDefaultStyle {
  RendererStyleHandler::GetRendererStyle(&style_);
  // Padding and scrollbar size are local to the macOS window. Shared
  // renderer_style.textproto stays unchanged for Windows and Linux.
  style_.set_window_border(6);
  style_.set_row_rect_padding(3);
  style_.set_scrollbar_width(10);
  style_.mutable_shortcut_style()->clear_background_color();
  style_.mutable_gap1_style()->clear_background_color();
  style_.mutable_candidate_style()->clear_background_color();
  style_.mutable_description_style()->clear_background_color();

  const std::string &logo_file_name = style_.logo_file_name();
  logoImage_ = [NSImage imageNamed:[NSString stringWithUTF8String:logo_file_name.c_str()]];
  if (logoImage_) {
    // Fix the image size.  Sometimes the size can be smaller than the
    // actual size because of blank margin.
    const NSArray *logoReps = [logoImage_ representations];
    if (logoReps && [logoReps count] > 0) {
      const NSImageRep *representation = [logoReps objectAtIndex:0];
      [logoImage_ setSize:NSMakeSize([representation pixelsWide], [representation pixelsHigh])];
    }
  }

  // This macOS implementation appends two spaces to calculate the minimum width.
  // This is a workaround for compatibility with the historical behavior.
  std::string min_width_string = style_.column_minimum_width_string();
  if (!min_width_string.empty()) {
    min_width_string.append("  ");
  }
  NSString *nsstr = [NSString stringWithUTF8String:min_width_string.c_str()];
  if (nsstr == nil) {
    nsstr = @"";
  }
  NSDictionary *attr = @{NSFontAttributeName : [NSFont systemFontOfSize:kCandidateFontSize]};
  const NSAttributedString *defaultMessage = [[NSAttributedString alloc] initWithString:nsstr
                                                                             attributes:attr];
  columnMinimumWidth_ = [defaultMessage size].width;

  // default line width is specified as 1.0 *pt*, but we want to draw
  // it as 1.0 px.
  [NSBezierPath setDefaultLineWidth:1.0];
  [NSBezierPath setDefaultLineJoinStyle:NSLineJoinStyleMiter];
}

- (void)setCandidateWindow:(const CandidateWindow *)candidate_window {
  candidate_window_ = *candidate_window;
}

- (void)setSendCommandInterface:(SendCommandInterface *)command_sender {
  command_sender_ = command_sender;
}

// Override of NSView.
- (BOOL)isFlipped {
  return YES;
}

- (BOOL)isOpaque {
  return NO;
}

- (void)viewDidChangeEffectiveAppearance {
  [super viewDidChangeEffectiveAppearance];
  [self setNeedsDisplay:YES];
}

- (void)dealloc {
  candidateStringsCache_ = nil;
}

- (const TableLayout *)tableLayout {
  return &tableLayout_;
}

#pragma mark drawing

- (NSSize)updateLayout {
  candidateStringsCache_ = nil;
  tableLayout_.Initialize(candidate_window_.candidate_size(), kNumberOfColumns);
  tableLayout_.SetWindowBorder(style_.window_border());

  // calculating focusedRow_
  if (candidate_window_.has_focused_index() && candidate_window_.candidate_size() > 0) {
    const int focusedIndex = candidate_window_.focused_index();
    focusedRow_ = focusedIndex - candidate_window_.candidate(0).index();
  } else {
    focusedRow_ = -1;
  }

  // Reserve footer space.
  if (candidate_window_.has_footer()) {
    NSSize footerSize = NSZeroSize;

    const mozc::commands::Footer &footer = candidate_window_.footer();

    if (footer.has_label()) {
      const NSAttributedString *footerLabel = MacViewUtil::ToSystemAttributedString(
          footer.label(), kFooterFontSize, MacTextTone::kSecondary);
      const NSSize footerLabelSize =
          MacViewUtil::applyTheme([footerLabel size], style_.footer_style());
      footerSize.width += footerLabelSize.width;
      footerSize.height = std::max(footerSize.height, footerLabelSize.height);
    }

    if (footer.has_sub_label()) {
      const NSAttributedString *footerSubLabel = MacViewUtil::ToSystemAttributedString(
          footer.sub_label(), kFooterFontSize, MacTextTone::kSecondary);
      const NSSize footerSubLabelSize =
          MacViewUtil::applyTheme([footerSubLabel size], style_.footer_sub_label_style());
      footerSize.width += footerSubLabelSize.width;
      footerSize.height = std::max(footerSize.height, footerSubLabelSize.height);
    }

    if (footer.logo_visible() && logoImage_) {
      const NSSize logoSize = [logoImage_ size];
      footerSize.width += logoSize.width;
      footerSize.height = std::max(footerSize.height, logoSize.height);
    }

    if (footer.index_visible()) {
      const int focusedIndex = candidate_window_.focused_index();
      const int totalItems = candidate_window_.size();
      const NSString *footerIndex =
          [NSString stringWithFormat:@"%d/%d", focusedIndex + 1, totalItems];
      const NSAttributedString *footerAttributedIndex = MacViewUtil::ToSystemAttributedString(
          [footerIndex UTF8String], kFooterFontSize, MacTextTone::kSecondary);
      const NSSize footerIndexSize =
          MacViewUtil::applyTheme([footerAttributedIndex size], style_.footer_style());
      footerSize.width += footerIndexSize.width;
      footerSize.height = std::max(footerSize.height, footerIndexSize.height);
    }

    footerSize.height += style_.footer_border_colors_size();
    tableLayout_.EnsureFooterSize(MacViewUtil::ToSize(footerSize));
  }

  tableLayout_.SetRowRectPadding(style_.row_rect_padding());
  if (candidate_window_.candidate_size() < candidate_window_.size()) {
    tableLayout_.SetVScrollBar(style_.scrollbar_width());
  }

  const NSAttributedString *gap1 =
      MacViewUtil::ToSystemAttributedString(" ", kCandidateFontSize, MacTextTone::kPrimary);
  tableLayout_.EnsureCellSize(kColumnGap1, MacViewUtil::ToSize([gap1 size]));

  NSMutableArray *newCache = [[NSMutableArray array] init];
  for (size_t i = 0; i < candidate_window_.candidate_size(); ++i) {
    const CandidateWindow::Candidate &candidate = candidate_window_.candidate(i);
    const NSAttributedString *shortcut = MacViewUtil::ToSystemAttributedString(
        candidate.annotation().shortcut(), kCandidateFontSize, MacTextTone::kSecondary);
    std::string value = candidate.value();
    if (candidate.annotation().has_prefix()) {
      value.insert(0, candidate.annotation().prefix());  // Prepend the prefix() to value.
    }
    if (candidate.annotation().has_suffix()) {
      value.append(candidate.annotation().suffix());
    }
    if (!value.empty()) {
      value.append("  ");
    }

    const NSAttributedString *candidateValue =
        MacViewUtil::ToSystemAttributedString(value, kCandidateFontSize, MacTextTone::kPrimary);
    const NSAttributedString *description = MacViewUtil::ToSystemAttributedString(
        candidate.annotation().description(), kDescriptionFontSize, MacTextTone::kSecondary);
    if ([shortcut length] > 0) {
      const NSSize shortcutSize =
          MacViewUtil::applyTheme([shortcut size], style_.shortcut_style());
      tableLayout_.EnsureCellSize(kColumnShortcut, MacViewUtil::ToSize(shortcutSize));
    }
    if ([candidateValue length] > 0) {
      const NSSize valueSize =
          MacViewUtil::applyTheme([candidateValue size], style_.candidate_style());
      tableLayout_.EnsureCellSize(kColumnCandidate, MacViewUtil::ToSize(valueSize));
    }
    if ([description length] > 0) {
      const NSSize descriptionSize =
          MacViewUtil::applyTheme([description size], style_.description_style());
      tableLayout_.EnsureCellSize(kColumnDescription, MacViewUtil::ToSize(descriptionSize));
    }

    [newCache
        addObject:[NSArray arrayWithObjects:shortcut, gap1, candidateValue, description, nil]];
  }

  tableLayout_.EnsureColumnsWidth(kColumnCandidate, kColumnDescription, columnMinimumWidth_);

  candidateStringsCache_ = newCache;
  tableLayout_.FreezeLayout();
  return MacViewUtil::ToNSSize(tableLayout_.GetTotalSize());
}

- (void)drawRect:(NSRect)rect {
  if (!Category_IsValid(candidate_window_.category())) {
    LOG(WARNING) << "Unknown candidates category: " << candidate_window_.category();
    return;
  }

  for (int i = 0; i < candidate_window_.candidate_size(); ++i) {
    [self drawRow:i];
  }

  if (candidate_window_.candidate_size() < candidate_window_.size()) {
    [self drawVScrollBar];
  }
  [self drawFooter];
}

#pragma mark drawing aux methods

- (void)drawRow:(int)row {
  const bool focused = (row == focusedRow_);
  if (focused) {
    NSRect focusedRect = MacViewUtil::ToNSRect(tableLayout_.GetRowRect(focusedRow_));
    focusedRect = NSInsetRect(focusedRect, kSelectionInsetX, kSelectionInsetY);
    [[NSColor selectedContentBackgroundColor] set];
    [[NSBezierPath bezierPathWithRoundedRect:focusedRect
                                     xRadius:kSelectionRadius
                                     yRadius:kSelectionRadius] fill];
  }

  NSArray<NSAttributedString *> *candidate = [candidateStringsCache_ objectAtIndex:row];

  auto drawText = [&](ColumnType type,
                      const mozc::renderer::RendererStyle::TextStyle &text_style) {
    NSAttributedString *text = [candidate objectAtIndex:type];
    if (focused) {
      text = MacViewUtil::AttributedStringWithForeground(text, [NSColor selectedMenuItemTextColor]);
    }
    NSRect cellRect = MacViewUtil::ToNSRect(tableLayout_.GetCellRect(row, type));
    NSPoint position = cellRect.origin;
    position.x += text_style.left_padding();
    position.y += (cellRect.size.height - [text size].height) / 2;
    [text drawAtPoint:position];
  };

  drawText(kColumnShortcut, style_.shortcut_style());
  drawText(kColumnGap1, style_.gap1_style());
  drawText(kColumnCandidate, style_.candidate_style());
  drawText(kColumnDescription, style_.description_style());

  if (candidate_window_.candidate(row).has_information_id()) {
    NSRect rect = MacViewUtil::ToNSRect(tableLayout_.GetRowRect(row));
    const CGFloat markerSize = 4.0;
    rect.origin.x += rect.size.width - markerSize - 6.0;
    rect.size.width = markerSize;
    rect.origin.y += (rect.size.height - markerSize) / 2.0;
    rect.size.height = markerSize;
    NSColor *markerColor =
        focused ? [NSColor selectedMenuItemTextColor] : [NSColor tertiaryLabelColor];
    [markerColor set];
    [[NSBezierPath bezierPathWithRoundedRect:rect xRadius:2.0 yRadius:2.0] fill];
  }
}

- (void)drawFooter {
  if (!candidate_window_.has_footer()) {
    return;
  }
  const mozc::commands::Footer &footer = candidate_window_.footer();
  NSRect footerRect = MacViewUtil::ToNSRect(tableLayout_.GetFooterRect());

  [[NSColor separatorColor] set];
  const CGFloat separatorInset = 8.0;
  const NSPoint fromPoint =
      NSMakePoint(footerRect.origin.x + separatorInset, footerRect.origin.y + 0.5);
  const NSPoint toPoint = NSMakePoint(footerRect.origin.x + footerRect.size.width - separatorInset,
                                      footerRect.origin.y + 0.5);
  [NSBezierPath strokeLineFromPoint:fromPoint toPoint:toPoint];
  footerRect.origin.y += 1;
  if (footerRect.size.height > 1) {
    footerRect.size.height -= 1;
  }

  // Draw logo
  if (footer.logo_visible() && logoImage_) {
    const NSPoint logoPoint = footerRect.origin;
    const NSSize logoSize = logoImage_.size;
    const NSRect logoRect = NSMakeRect(logoPoint.x, logoPoint.y, logoSize.width, logoSize.height);
    [logoImage_ drawInRect:logoRect
                    fromRect:NSZeroRect   // Draw the entire image
                  operation:NSCompositingOperationSourceOver
                    fraction:1.0  // Opacity
              respectFlipped:YES
                      hints:nil];
    footerRect.origin.x += logoSize.width;
    footerRect.size.width -= logoSize.width;
  }

  // Draw label
  if (footer.has_label()) {
    const NSAttributedString *footerLabel = MacViewUtil::ToSystemAttributedString(
        footer.label(), kFooterFontSize, MacTextTone::kSecondary);
    footerRect.origin.x += style_.footer_style().left_padding();
    const NSSize labelSize = [footerLabel size];
    NSPoint labelPosition = footerRect.origin;
    labelPosition.y += (footerRect.size.height - labelSize.height) / 2;
    [footerLabel drawAtPoint:labelPosition];
  }

  // Draw sub_label
  if (footer.has_sub_label()) {
    const NSAttributedString *footerSubLabel = MacViewUtil::ToSystemAttributedString(
        footer.sub_label(), kFooterFontSize, MacTextTone::kSecondary);
    footerRect.origin.x += style_.footer_sub_label_style().left_padding();
    const NSSize subLabelSize = [footerSubLabel size];
    NSPoint subLabelPosition = footerRect.origin;
    subLabelPosition.y += (footerRect.size.height - subLabelSize.height) / 2;
    [footerSubLabel drawAtPoint:subLabelPosition];
  }

  // Draw footer index (e.g. "10/120")
  if (footer.index_visible()) {
    const std::string footerIndex =
        absl::StrFormat("%d/%d",
                        candidate_window_.focused_index() + 1,  // +1 to 1-origin from 0-origin.
                        candidate_window_.size());
    const NSAttributedString *footerAttributedIndex = MacViewUtil::ToSystemAttributedString(
        footerIndex, kFooterFontSize, MacTextTone::kSecondary);
    const NSSize footerSize = [footerAttributedIndex size];
    NSPoint footerPosition = footerRect.origin;
    footerPosition.x = footerPosition.x + footerRect.size.width - footerSize.width -
                        style_.footer_style().right_padding();
    footerPosition.y += (footerRect.size.height - footerSize.height) / 2;
    [footerAttributedIndex drawAtPoint:footerPosition];
  }
}

- (void)drawVScrollBar {
  const mozc::Rect vscrollRect = tableLayout_.GetVScrollBarRect();
  if (vscrollRect.IsRectEmpty() || candidate_window_.candidate_size() <= 0) {
    return;
  }

  const int beginIndex = candidate_window_.candidate(0).index();
  const int candidatesTotal = candidate_window_.size();
  const int endIndex = candidate_window_.candidate(candidate_window_.candidate_size() - 1).index();

  const NSRect track = MacViewUtil::ToNSRect(vscrollRect);
  const mozc::Rect indicatorRect =
      tableLayout_.GetVScrollIndicatorRect(beginIndex, endIndex, candidatesTotal);
  NSRect indicator = MacViewUtil::ToNSRect(indicatorRect);
  const CGFloat pillWidth = 4.0;
  indicator.origin.x = NSMidX(track) - pillWidth / 2.0;
  indicator.size.width = pillWidth;
  indicator = NSInsetRect(indicator, 0, 1.0);
  constexpr CGFloat kMinimumPillHeight = 12.0;
  if (indicator.size.height < kMinimumPillHeight) {
    const CGFloat extra = kMinimumPillHeight - indicator.size.height;
    indicator.origin.y -= extra / 2.0;
    indicator.size.height = kMinimumPillHeight;
  }
  if (NSMinY(indicator) < NSMinY(track)) {
    indicator.origin.y = NSMinY(track);
  }
  if (NSMaxY(indicator) > NSMaxY(track)) {
    indicator.origin.y = NSMaxY(track) - indicator.size.height;
  }
  if (indicator.size.height > track.size.height) {
    indicator.origin.y = track.origin.y;
    indicator.size.height = track.size.height;
  }
  [[NSColor tertiaryLabelColor] set];
  [[NSBezierPath bezierPathWithRoundedRect:indicator
                                   xRadius:pillWidth / 2.0
                                   yRadius:pillWidth / 2.0] fill];
}

#pragma mark event handling callbacks

- (void)mouseDown:(NSEvent *)event {
  const mozc::Point localPos = MacViewUtil::ToPoint([self convertPoint:[event locationInWindow]
                                                              fromView:nil]);
  int clickedRow = -1;
  for (int i = 0; i < tableLayout_.number_of_rows(); ++i) {
    const mozc::Rect rowRect = tableLayout_.GetRowRect(i);
    if (rowRect.PtrInRect(localPos)) {
      clickedRow = i;
      break;
    }
  }

  if (clickedRow >= 0 && clickedRow != focusedRow_) {
    focusedRow_ = clickedRow;
    [self setNeedsDisplay:YES];
  }
}

- (void)mouseUp:(NSEvent *)event {
  const mozc::Point localPos = MacViewUtil::ToPoint([self convertPoint:[event locationInWindow]
                                                              fromView:nil]);
  if (command_sender_ == nullptr) {
    return;
  }
  if (candidate_window_.candidate_size() < tableLayout_.number_of_rows()) {
    return;
  }
  for (int i = 0; i < tableLayout_.number_of_rows(); ++i) {
    const mozc::Rect rowRect = tableLayout_.GetRowRect(i);
    if (rowRect.PtrInRect(localPos)) {
      SessionCommand command;
      command.set_type(SessionCommand::SELECT_CANDIDATE);
      command.set_id(candidate_window_.candidate(i).id());
      Output dummy_output;
      command_sender_->SendCommand(command, &dummy_output);
      break;
    }
  }
}

- (void)mouseDragged:(NSEvent *)event {
  [self mouseDown:event];
}
@end

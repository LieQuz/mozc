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

#include <iostream>
#include <iterator>
#include <memory>
#include <string>
#include <vector>

#include "absl/flags/flag.h"
#include "absl/log/check.h"
#include "absl/log/log.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_join.h"
#include "base/file_stream.h"
#include "base/file/temp_dir.h"
#include "base/init_mozc.h"
#include "base/system_util.h"
#include "composer/composer.h"
#include "composer/table.h"
#include "converter/conversion_layer_eval.h"
#include "converter/converter_interface.h"
#include "converter/immutable_converter.h"
#include "converter/lattice.h"
#include "converter/segments.h"
#include "engine/engine.h"
#include "engine/eval_engine_factory.h"
#include "protocol/commands.pb.h"
#include "protocol/config.pb.h"
#include "request/conversion_request.h"
#include "request/options.h"

ABSL_FLAG(std::string, test_file, "", "TSV of key, value, segments, domain");
ABSL_FLAG(std::string, data_file, "", "engine data file");
ABSL_FLAG(std::string, data_type, "", "engine data type");

namespace {

std::string FinalValue(const mozc::Segments& segments) {
  std::vector<std::string> values;
  for (const mozc::Segment& segment : segments.conversion_segments()) {
    if (segment.candidates_size() == 0) {
      continue;
    }
    values.push_back(segment.candidate(0).value);
  }
  return absl::StrJoin(values, "");
}

}  // namespace

int main(int argc, char** argv) {
  mozc::InitMozc(argv[0], &argc, &argv);
  absl::StatusOr<mozc::TempDirectory> temp_dir =
      mozc::TempDirectory::Default().CreateTempDirectory();
  CHECK_OK(temp_dir);
  mozc::SystemUtil::SetUserProfileDirectory(temp_dir->path());

  absl::StatusOr<std::unique_ptr<mozc::Engine>> engine =
      mozc::CreateEvalEngine(absl::GetFlag(FLAGS_data_file),
                             absl::GetFlag(FLAGS_data_type));
  if (!engine.ok()) {
    LOG(ERROR) << engine.status();
    return 1;
  }

  mozc::InputFileStream input(absl::GetFlag(FLAGS_test_file));
  std::string tsv((std::istreambuf_iterator<char>(input)),
                  std::istreambuf_iterator<char>());
  std::vector<mozc::ConversionLayerItem> items;
  const absl::Status parsed = mozc::ParseConversionLayerTsv(tsv, &items);
  if (!parsed.ok()) {
    LOG(ERROR) << parsed;
    return 1;
  }

  mozc::ImmutableConverter immutable((*engine)->GetModulesForTesting());
  std::shared_ptr<const mozc::ConverterInterface> converter =
      (*engine)->GetConverter();
  auto table = std::make_shared<mozc::composer::Table>();
  mozc::commands::Request request;
  mozc::config::Config config;

  int counts[4] = {};
  std::cout << "key\tlayer\tboundary_f1\trank\tbest\tfinal\n";
  for (const mozc::ConversionLayerItem& item : items) {
    mozc::Segments lattice_segments;
    mozc::Segment* segment = lattice_segments.add_segment();
    segment->set_key(item.key);
    mozc::Lattice lattice;
    mozc::ConversionOptions options;
    options.request_type = mozc::RequestType::CONVERSION;
    if (!immutable.Convert(options, &lattice_segments, &lattice)) {
      LOG(ERROR) << "Convert failed: " << item.key;
      return 1;
    }
    const mozc::ConversionLayerResult layer =
        mozc::EvaluateConversionLayer(item, lattice, lattice_segments);

    mozc::composer::Composer composer(table, request, config);
    composer.SetPreeditTextForTestOnly(item.key);
    const mozc::ConversionRequest conv_req =
        mozc::ConversionRequestBuilder()
            .SetComposer(composer)
            .SetRequestView(request)
            .SetConfigView(config)
            .Build();
    mozc::Segments final_segments;
    converter->ResetConversion(&final_segments);
    if (!converter->StartConversion(conv_req, &final_segments)) {
      LOG(ERROR) << "StartConversion failed: " << item.key;
      return 1;
    }
    const std::string final_value = FinalValue(final_segments);
    std::cout << item.key << "\t" << mozc::ConversionFailureLayerName(layer.layer)
              << "\t" << layer.boundary_f1 << "\t" << layer.candidate_rank
              << "\t" << layer.best_value << "\t" << final_value << "\n";
    counts[static_cast<int>(layer.layer)]++;
  }
  std::cout << "# ok " << counts[0] << " oov " << counts[1] << " best_path "
            << counts[2] << " candidate_rank " << counts[3] << "\n";
  return 0;
}

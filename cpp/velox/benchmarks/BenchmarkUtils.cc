/*
 * Licensed to the Apache Software Foundation (ASF) under one or more
 * contributor license agreements.  See the NOTICE file distributed with
 * this work for additional information regarding copyright ownership.
 * The ASF licenses this file to You under the Apache License, Version 2.0
 * (the "License"); you may not use this file except in compliance with
 * the License.  You may obtain a copy of the License at
 *
 *    http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "BenchmarkUtils.h"

#include "compute/VeloxInitializer.h"
#include "config/GlutenConfig.h"
#include "velox/dwio/common/Options.h"

using namespace facebook;
namespace fs = std::filesystem;

DEFINE_bool(print_result, true, "Print result for execution");
DEFINE_string(write_file, "", "Write the output to parquet file, file absolute path");
DEFINE_string(batch_size, "4096", "To set velox::core::QueryConfig::kPreferredOutputBatchSize.");
DEFINE_int32(cpu, -1, "Run benchmark on specific CPU");
DEFINE_int32(threads, 1, "The number of threads to run this benchmark");
DEFINE_int32(iterations, 1, "The number of iterations to run this benchmark");

namespace {

std::unordered_map<std::string, std::string> bmConfMap = {{gluten::kSparkBatchSize, FLAGS_batch_size}};

} // anonymous namespace

void initVeloxBackend(std::unordered_map<std::string, std::string>& conf) {
  gluten::setBackendFactory([&] { return std::make_shared<gluten::VeloxBackend>(conf); });
  auto veloxInitializer = std::make_shared<gluten::VeloxInitializer>(conf);
}

void initVeloxBackend() {
  initVeloxBackend(bmConfMap);
}

arrow::Result<std::shared_ptr<arrow::Buffer>> getPlanFromFile(const std::string& filePath) {
  // Read json file and resume the binary data.
  std::ifstream msgJson(filePath);
  std::stringstream buffer;
  buffer << msgJson.rdbuf();
  std::string msgData = buffer.str();

  auto maybePlan = gluten::substraitFromJsonToPb("Plan", msgData);
  return maybePlan;
}

std::shared_ptr<velox::substrait::SplitInfo> getSplitInfos(
    const std::string& datasetPath,
    const std::string& fileFormat) {
  auto scanInfo = std::make_shared<velox::substrait::SplitInfo>();

  // Set format to scan info.
  auto format = velox::dwio::common::FileFormat::UNKNOWN;
  if (fileFormat.compare("orc") == 0) {
    format = velox::dwio::common::FileFormat::ORC;
  } else if (fileFormat.compare("parquet") == 0) {
    format = velox::dwio::common::FileFormat::PARQUET;
  }
  scanInfo->format = format;

  // Set split start, length, and path to scan info.
  std::filesystem::path fileDir(datasetPath);
  for (auto i = std::filesystem::directory_iterator(fileDir); i != std::filesystem::directory_iterator(); i++) {
    if (!is_directory(i->path())) {
      std::string singleFilePath = i->path().filename().string();
      if (endsWith(singleFilePath, "." + fileFormat)) {
        auto fileAbsolutePath = datasetPath + singleFilePath;
        scanInfo->starts.emplace_back(0);
        scanInfo->lengths.emplace_back(fs::file_size(fileAbsolutePath));
        scanInfo->paths.emplace_back("file://" + fileAbsolutePath);
      }
    } else {
      continue;
    }
  }
  return scanInfo;
}

bool checkPathExists(const std::string& filepath) {
  std::filesystem::path f{filepath};
  return std::filesystem::exists(f);
}

void abortIfFileNotExists(const std::string& filepath) {
  if (!checkPathExists(filepath)) {
    std::cerr << "File path does not exist: " << filepath << std::endl;
    ::benchmark::Shutdown();
    std::exit(EXIT_FAILURE);
  }
}

std::shared_ptr<arrow::Schema> getOutputSchema(std::shared_ptr<const facebook::velox::core::PlanNode> planNode) {
  ArrowSchema arrowSchema{};
  exportToArrow(
      velox::BaseVector::create(planNode->outputType(), 0, gluten::getDefaultVeloxLeafMemoryPool().get()), arrowSchema);
  GLUTEN_ASSIGN_OR_THROW(auto outputSchema, arrow::ImportSchema(&arrowSchema));
  return outputSchema;
}

bool endsWith(const std::string& data, const std::string& suffix) {
  return data.find(suffix, data.size() - suffix.size()) != std::string::npos;
}

#if 0
std::shared_ptr<arrow::RecordBatchReader> createReader(const std::string& path) {
  std::unique_ptr<parquet::arrow::FileReader> parquetReader;
  std::shared_ptr<arrow::RecordBatchReader> recordBatchReader;
  parquet::ArrowReaderProperties properties = parquet::default_arrow_reader_properties();

  GLUTEN_THROW_NOT_OK(parquet::arrow::FileReader::Make(
      arrow::default_memory_pool(), parquet::ParquetFileReader::OpenFile(path), properties, &parquetReader));
  GLUTEN_THROW_NOT_OK(
      parquetReader->GetRecordBatchReader(arrow::internal::Iota(parquetReader->num_row_groups()), &recordBatchReader));
  return recordBatchReader;
}
#endif

void setCpu(uint32_t cpuindex) {
  static const auto kTotalCores = std::thread::hardware_concurrency();
  cpuindex = cpuindex % kTotalCores;
  cpu_set_t cs;
  CPU_ZERO(&cs);
  CPU_SET(cpuindex, &cs);
  if (sched_setaffinity(0, sizeof(cs), &cs) == -1) {
    std::cerr << "Error binding CPU " << std::to_string(cpuindex) << std::endl;
    exit(EXIT_FAILURE);
  }
}

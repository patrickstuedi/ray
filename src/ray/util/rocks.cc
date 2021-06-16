// Copyright 2017 The Ray Authors.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//  http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "ray/util/rocks.h"

#include <rocksdb/status.h>
#include <unistd.h>

#include <cstring>
#include <thread>
#include <vector>

#include "ray/util/logging.h"
#include "rocksdb/cloud/db_cloud.h"
#include "rocksdb/options.h"

using namespace ROCKSDB_NAMESPACE;

namespace ray {

std::string kDBPath = "/tmp/rocksdb_cloud_durable";
std::string kBucketSuffix = "cloud.durable.example.";
std::string kRegion = "us-west-2";
static const bool flushAtEnd = false;
static const bool disableWAL = true;
static CloudEnv *cenv;
static DBCloud *db_;
static std::unique_ptr<CloudEnv> cloud_env;

void run_rocks() {
  CloudEnvOptions cloud_env_options;

  if (!cloud_env_options.credentials.HasValid().ok()) {
    RAY_LOG(INFO) << "### rocks: lease set env variables for rocksdb";
  }

  char *user = getenv("USER");
  kBucketSuffix.append(user);

  const std::string bucketPrefix = "rockset.";
  cloud_env_options.src_bucket.SetBucketName(kBucketSuffix, bucketPrefix);
  cloud_env_options.dest_bucket.SetBucketName(kBucketSuffix, bucketPrefix);

  const std::string bucketName = bucketPrefix + kBucketSuffix;
  RAY_LOG(INFO) << "### rocks: creds " << cloud_env_options.credentials.access_key_id
                << ", user " << kBucketSuffix << ", bucketname " << bucketName;

  rocksdb::Status s =
      CloudEnv::NewAwsEnv(Env::Default(), kBucketSuffix, kDBPath, kRegion, kBucketSuffix,
                          kDBPath, kRegion, cloud_env_options, nullptr, &cenv);
  if (!s.ok()) {
    RAY_LOG(INFO) << "### rocks: Unable to create cloud env in bucket";
  }
  cloud_env.reset(cenv);

  Options options;
  options.env = cloud_env.get();
  options.create_if_missing = true;

  std::string persistent_cache = "";

  WriteOptions wopt;
  wopt.disableWAL = disableWAL;

  // DBCloud* db;
  s = DBCloud::Open(options, kDBPath, persistent_cache, 0, &db_);
  if (!s.ok()) {
    RAY_LOG(INFO) << "### rocks: Unable to open db at path with bucket";
  }

  s = db_->Put(wopt, "keyray", "value");
  std::string value;
  s = db_->Get(ReadOptions(), "keyray", &value);

  RAY_LOG(INFO) << "### rocks: value " << value;

  if (flushAtEnd) {
    db_->Flush(FlushOptions());
  }

  // delete db_;
}

void stop_rocks() { delete db_; }

}  // namespace ray

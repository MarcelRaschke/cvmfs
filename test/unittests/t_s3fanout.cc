/**
 * This file is part of the CernVM File System.
 */

#include <gtest/gtest.h>

#include <cstdio>

#include "duplex_ssl.h"
#include "s3fanout.h"

using namespace std;  // NOLINT

TEST(T_S3Fanout, Init) {
#ifdef OPENSSL_API_INTERFACE_V09
  printf("Skipping!\n");
#else
#endif
}

TEST(T_S3Fanout, DetectThrottleIndicator) {
  s3fanout::JobInfo info(
    "", "", s3fanout::kAuthzAwsV2, "", "", "", "", NULL, "");
  info.throttle_ms = 1;

  s3fanout::S3FanoutManager::DetectThrottleIndicator("", &info);
  EXPECT_EQ(1U, info.throttle_ms);
  s3fanout::S3FanoutManager::DetectThrottleIndicator("retry-after", &info);
  EXPECT_EQ(1U, info.throttle_ms);
  s3fanout::S3FanoutManager::DetectThrottleIndicator("retry-after:", &info);
  EXPECT_EQ(1U, info.throttle_ms);
  s3fanout::S3FanoutManager::DetectThrottleIndicator("x-retry-in:", &info);
  EXPECT_EQ(1U, info.throttle_ms);

  s3fanout::S3FanoutManager::DetectThrottleIndicator("retry-after: 1", &info);
  EXPECT_EQ(1000U, info.throttle_ms);
  s3fanout::S3FanoutManager::DetectThrottleIndicator("retry-after:5", &info);
  EXPECT_EQ(5000U, info.throttle_ms);
  s3fanout::S3FanoutManager::DetectThrottleIndicator("retry-after:42", &info);
  EXPECT_EQ(10000U, info.throttle_ms);
  s3fanout::S3FanoutManager::DetectThrottleIndicator("x-retry-in:2", &info);
  EXPECT_EQ(2000U, info.throttle_ms);
  s3fanout::S3FanoutManager::DetectThrottleIndicator("x-retry-in:0", &info);
  EXPECT_EQ(2000U, info.throttle_ms);
}


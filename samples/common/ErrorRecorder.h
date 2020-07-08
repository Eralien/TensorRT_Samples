/*
 * Copyright (c) 2019, NVIDIA CORPORATION. All rights reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef ERROR_RECORDER_H
#define ERROR_RECORDER_H
#include <atomic>
#include <cstdint>
#include <exception>
#include <mutex>
#include <vector>
#include "NvInferRuntimeCommon.h"
using namespace nvinfer1;
//!
//! A simple imeplementation of the IErrorRecorder interface for
//! use by samples. This interface also can be used as a reference
//! implementation.
//! The sample Error recorder is based on a vector that pairs the error
//! code and the error string into a single element. It also uses
//! standard mutex's and atomics in order to make sure that the code
//! works in a multi-threaded environment.
//! SampleErrorRecorder is not intended for use in automotive safety
//! environments.
//!
class SampleErrorRecorder : public IErrorRecorder {
  using errorPair = std::pair<ErrorCode, std::string>;
  using errorStack = std::vector<errorPair>;

 public:
  SampleErrorRecorder() = default;

  virtual ~SampleErrorRecorder() noexcept {}
  int32_t getNbErrors() const noexcept final { return mErrorStack.size(); }
  ErrorCode getErrorCode(int32_t errorIdx) const noexcept final {
    return indexCheck(errorIdx) ? ErrorCode::kINVALID_ARGUMENT
                                : (*this)[errorIdx].first;
  };
  IErrorRecorder::ErrorDesc getErrorDesc(int32_t errorIdx) const
      noexcept final {
    return indexCheck(errorIdx) ? "errorIdx out of range."
                                : (*this)[errorIdx].second.c_str();
  }
  // This class can never overflow since we have dynamic resize via std::vector
  // usage.
  bool hasOverflowed() const noexcept final { return false; }

  // Empty the errorStack.
  void clear() noexcept final {
    try {
      // grab a lock so that there is no addition while clearing.
      std::lock_guard<std::mutex> guard(mStackLock);
      mErrorStack.clear();
    } catch (const std::exception& e) {
      getLogger()->log(ILogger::Severity::kINTERNAL_ERROR, e.what());
    }
  };

  //! Simple helper function that
  bool empty() const noexcept { return mErrorStack.empty(); }

  bool reportError(ErrorCode val,
                   IErrorRecorder::ErrorDesc desc) noexcept final {
    try {
      std::lock_guard<std::mutex> guard(mStackLock);
      mErrorStack.push_back(errorPair(val, desc));
    } catch (const std::exception& e) {
      getLogger()->log(ILogger::Severity::kINTERNAL_ERROR, e.what());
    }
    // All errors are considered fatal.
    return true;
  }

  // Atomically increment or decrement the ref counter.
  IErrorRecorder::RefCount incRefCount() noexcept final { return ++mRefCount; }
  IErrorRecorder::RefCount decRefCount() noexcept final { return --mRefCount; }

 private:
  // Simple helper functions.
  const errorPair& operator[](size_t index) const noexcept {
    return mErrorStack[index];
  }

  bool indexCheck(int32_t index) const noexcept {
    // By converting signed to unsigned, we only need a single check since
    // negative numbers turn into large positive greater than the size.
    size_t sIndex = index;
    return sIndex >= mErrorStack.size();
  }
  // Mutex to hold when locking mErrorStack.
  std::mutex mStackLock;

  // Reference count of the class. Destruction of the class when mRefCount
  // is not zero causes undefined behavior.
  std::atomic<int32_t> mRefCount{0};

  // The error stack that holds the errors recorded by TensorRT.
  errorStack mErrorStack;
};      // class SampleErrorRecorder
#endif  // ERROR_RECORDER_H

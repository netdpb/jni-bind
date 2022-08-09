/*
 * Copyright 2022 Google LLC
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
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include "jni_bind.h"

namespace {

using ::jni::Class;
using ::jni::ClassLoader;
using ::jni::JniType;
using ::jni::Jvm;
using ::jni::kNullClassLoader;
using ::jni::SupportedClassSet;

constexpr Class kClass1{"kClass"};
constexpr Class kClass2{"kClass2"};

constexpr ClassLoader kLoader1{kNullClassLoader, SupportedClassSet{kClass1}};
constexpr ClassLoader kLoader2{kNullClassLoader, SupportedClassSet{kClass2}};

constexpr Jvm kJvm1{kLoader1};
constexpr Jvm kJvm2{kLoader1};

static constexpr JniType<jobject, kClass1, kLoader1, kJvm1> kJniType1;
static constexpr JniType<jobject, kClass2, kLoader2, kJvm2> kJniType2;

static_assert(kJniType1 == kJniType1);
static_assert(kJniType1 != kJniType2);

}  // namespace
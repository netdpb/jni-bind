/*
 * Copyright 2021 Google LLC
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

#ifndef JNI_BIND_GLOBAL_CLASS_LOADER_H_
#define JNI_BIND_GLOBAL_CLASS_LOADER_H_

#include "implementation/class_loader.h"
#include "implementation/class_loader_ref.h"
#include "implementation/jni_helper/lifecycle_object.h"
#include "implementation/jvm.h"
#include "implementation/promotion_mechanics.h"
#include "jni_dep.h"

namespace jni {

template <const auto& class_loader_v_, const auto& jvm_v_ = kDefaultJvm>
class GlobalClassLoader : public ClassLoaderRef<jvm_v_, class_loader_v_> {
 public:
  using Base = ClassLoaderRef<jvm_v_, class_loader_v_>;
  using LifecycleT = LifecycleHelper<jobject, LifecycleType::GLOBAL>;

  GlobalClassLoader(CreateCopy, jobject class_loader)
      : Base(static_cast<jobject>(LifecycleT::NewReference(class_loader))) {}

  GlobalClassLoader(AdoptGlobal, jobject class_loader)
      : ClassLoaderRef<jvm_v_, class_loader_v_>(class_loader) {}

  GlobalClassLoader(PromoteToGlobal, jobject class_loader)
      : ClassLoaderRef<jvm_v_, class_loader_v_>(
            LifecycleT::Promote(class_loader)) {}

  template <const auto& class_loader_v, const auto& jvm_v>
  GlobalClassLoader(GlobalClassLoader<class_loader_v, jvm_v>&& rhs)
      : Base(rhs.Release()) {}

  ~GlobalClassLoader() {
    if (Base::object_ref_) {
      LifecycleT::Delete(Base::object_ref_);
    }
  }
};

}  // namespace jni

#endif  // JNI_BIND_GLOBAL_CLASS_LOADER_H_

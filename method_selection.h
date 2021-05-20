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

#ifndef JNI_BIND_METHOD_SELECTION_H_
#define JNI_BIND_METHOD_SELECTION_H_

#include <limits>
#include <type_traits>
#include <utility>

#include "default_class_loader.h"
#include "proxy.h"
#include "metaprogramming/concatenate.h"
#include "metaprogramming/invoke.h"
#include "metaprogramming/n_bit_sequence.h"
#include "metaprogramming/type_index_mask.h"
#include "metaprogramming/type_of_nth_element.h"

namespace jni {

static constexpr std::size_t kNoSelection = std::numeric_limits<size_t>::max();

// Represents an overload which itself may be a set of permutations.
template <typename MethodSelectionT, size_t overload_idx>
struct Overload;

// Represents a permutation (e.g. jstring => {std::string, const char*, etc...}
template <typename MethodSelectionT, typename OverloadT, size_t permutation_idx>
struct Permutation;

// Represents the exact selection of a specific parameter from a permutation.
template <typename MethodSelectionT, typename OverloadT, typename PermutationT,
          size_t param_idx>
struct ParamSelection;

// Compares a ParamSelection (the type associated with an exact parameter of an
// exact permutation) and exposes a value if they are equal.
template <typename ParamSelectionT, typename Query>
struct ParamCompare;

// Represents an indexing into a specific class and method.
template <const auto& class_loader_v_, const auto& class_v_,
          bool is_constructor, size_t method_idx>
struct MethodSelection {
  static constexpr bool kIsConstructor = is_constructor;

  static constexpr const auto& GetClass() { return class_v_; }
  static constexpr const auto& GetClassLoader() { return class_loader_v_; }

  static constexpr const auto& GetMethod() {
    if constexpr (is_constructor) {
      static_assert(method_idx == 0,
                    "If using MethodSelection for a constructor, there is only "
                    "ever one method (\"<init>\"), set method_idx to 0.");
      return class_v_.constructors_;
    } else {
      return std::get<method_idx>(class_v_.methods_);
    }
  }

  static constexpr std::size_t NumOverloads() {
    if constexpr (is_constructor) {
      return std::tuple_size<decltype(class_v_.constructors_)>();
    } else {
      return std::tuple_size<decltype(GetMethod().invocations_)>();
    }
  }

  template <typename Is, typename... Ts>
  struct Helper;

  template <size_t... Is, typename... Ts>
  struct Helper<std::index_sequence<Is...>, Ts...> {
    static constexpr bool val =
        (Overload<MethodSelection, Is>::template OverloadViable<Ts...>() ||
         ...);

    // kNoSelection is the max of std::size_t, so, this essentially selects any
    // idx (if a valid one exists), or defaults to kNoSelection.
    static constexpr std::pair<std::size_t, std::size_t>
        overload_permutation_idx_if_valid{
            std::min({Overload<MethodSelection,
                               Is>::template OverloadIdxIfViable<Ts...>()...}),
            std::min(
                {Overload<MethodSelection,
                          Is>::template PermutationIdxIfViable<Ts...>()...})};
  };

  template <typename... Ts>
  static constexpr bool ArgSetViable() {
    return Helper<std::make_index_sequence<NumOverloads()>,
                  std::decay_t<Ts>...>::val;
  }

  // The method and permutation overload that is viable for a set of args, or
  // {kNoSelection, kNoSelection} if no permutation is valid.
  template <typename... Ts>
  static constexpr std::pair<size_t, size_t> IdxPair() {
    return Helper<std::make_index_sequence<NumOverloads()>,
                  std::decay_t<Ts>...>::overload_permutation_idx_if_valid;
  }

  template <typename... Ts>
  using FindOverload = Overload<MethodSelection, IdxPair<Ts...>().first>;

  template <typename... Ts>
  using FindPermutation = Overload<MethodSelection, IdxPair<Ts...>().second>;
};

template <const auto& class_loader_v_, const auto& class_v_,
          bool is_constructor, size_t method_idx>
using MethodSelection_t =
    MethodSelection<class_loader_v_, class_v_, is_constructor, method_idx>;

template <typename MethodSelection, typename... Args>
using FindOverload_t =
    typename MethodSelection ::template FindOverload<Args...>;

template <typename MethodSelection, typename... Args>
using FindPermutation_t =
    typename MethodSelection ::template FindPermuation<Args...>;

template <typename MethodSelectionT, size_t overload_idx>
struct Overload {
  static constexpr const auto& GetParams() {
    if constexpr (MethodSelectionT::kIsConstructor) {
      return std::get<overload_idx>(MethodSelectionT::GetMethod()).params_;
    } else {
      return std::get<overload_idx>(MethodSelectionT::GetMethod().invocations_)
          .params_;
    }
  }

  static constexpr const auto& GetReturn() {
    if constexpr (MethodSelectionT::kIsConstructor) {
      return Return<void>{};
    } else {
      return std::get<overload_idx>(MethodSelectionT::GetMethod().invocations_)
          .return_;
    }
  }

  using CDecl = CDecl_t<ReturnRaw_t<std::decay_t<decltype(GetReturn())>>>;

  using ProxyForReturn =
      Proxy_t<ReturnRaw_t<std::decay_t<decltype(GetReturn())>>>;

  using ReturnProxied = Return_t<CDecl, Overload>;

  // Proxy every parameter argument as an argument that can be shown in a
  // function prototype.
  //
  // A void function generates no arguments, so we generate an empty tuple.
  using ParamsRaw = typename std::decay_t<decltype(GetParams())>::ParamsRawTup;
  using ParamsProxiedTemp =
      metaprogramming::InvokePerTupArg_t<ProxyAsArgMetaFunc, ParamsRaw>;
  using ParamsProxied =
      std::conditional_t<std::is_same_v<ParamsProxiedTemp, std::tuple<>>,
                         std::tuple<std::tuple<>>, ParamsProxiedTemp>;

  // Lastly, we create a sequence to iterate all viable permutations.
  using NBitSequence = metaprogramming::TupleUnroller_t<
      metaprogramming::GenerateBitSequenceFromTupSetsFunc, ParamsProxied>;

  static constexpr const size_t permutation_count =
      (NBitSequence::max_representable_size_ == 0
           ? 1
           : NBitSequence::max_representable_size_);

  template <typename Is, typename... Ts>
  struct Helper;

  template <size_t... Is, typename... Ts>
  struct Helper<std::index_sequence<Is...>, Ts...> {
    static_assert(sizeof...(Is) == permutation_count);

    static constexpr bool val =
        (Permutation<MethodSelectionT, Overload,
                     Is>::template PermutationViable<Ts...>() ||
         ...);

    static constexpr size_t first_valid_permutation = std::min(
        {Permutation<MethodSelectionT, Overload,
                     Is>::template PermutationIdxIfViable<Ts...>()...});
  };

  template <typename... Ts>
  static constexpr size_t PermutationIdxIfViable() {
    constexpr size_t num_params =
        std::tuple_size_v<decltype(GetParams().values_)>;
    if constexpr (sizeof...(Ts) != num_params) {
      return kNoSelection;
    } else {
      return Helper<std::make_index_sequence<permutation_count>,
                    Ts...>::first_valid_permutation;
    }
  }

  template <typename... Ts>
  static constexpr bool OverloadViable() {
    return PermutationIdxIfViable<Ts...>() != kNoSelection;
  }

  template <typename... Ts>
  static constexpr size_t OverloadIdxIfViable() {
    return OverloadViable<Ts...>() ? overload_idx : kNoSelection;
  }
};

template <typename MethodSelectionT, typename OverloadT, size_t permutation_idx>
struct Permutation {
  using NBitSelection =
      metaprogramming::Increment_t<typename OverloadT::NBitSequence,
                                   permutation_idx>;

  template <size_t I>
  using Param = ParamSelection<MethodSelectionT, OverloadT, Permutation, I>;

  template <typename Is, typename... Ts>
  struct Helper;

  template <size_t... Is, typename... Ts>
  struct Helper<std::index_sequence<Is...>, Ts...> {
    static constexpr bool val = (Param<Is>::template viable<Ts> && ...);
  };

  template <typename... Ts>
  static constexpr bool PermutationViable() {
    return Helper<std::index_sequence_for<Ts...>, Ts...>::val;
  }

  template <typename... Ts>
  static constexpr size_t PermutationIdxIfViable() {
    return PermutationViable<Ts...>() ? permutation_idx : kNoSelection;
  }
};

template <typename MethodSelectionT, typename OverloadT, typename PermutationT,
          size_t param_idx>
struct ParamSelection {
  static constexpr size_t selection_idx =
      PermutationT::NBitSelection::template GetBit<param_idx>::value_;

  constexpr static auto& GetParam() {
    return std::get<param_idx>(OverloadT::GetParams().values_);
  }

  using ParamT = metaprogramming::TypeOfNthTupleElement_t<
      selection_idx, metaprogramming::TypeOfNthTupleElement_t<
                         param_idx, typename OverloadT::ParamsProxied>>;

  template <typename T>
  static constexpr bool viable = ParamCompare<ParamSelection, T>::val;
};

template <typename ParamSelectionT, typename Query>
struct ParamCompare {
  using ParamT = typename ParamSelectionT::ParamT;

  template <typename, typename Enable = void>
  struct Helper {
    static constexpr bool val = false;
  };

  template <typename T>
  struct Helper<T, std::enable_if_t<std::is_same_v<std::remove_cv_t<T>,
                                                   std::remove_cv_t<ParamT>>>> {
    static constexpr bool val = true;
  };

  // The partial specialisation to compare a Local or Global object
  // against the specific selected permutation.
  template <template <const auto&, const auto&, const auto&> class T,
            const auto& class_v, const auto& class_loader_v, const auto& jvm_v>
  struct Helper<
      T<class_v, class_loader_v, jvm_v>,
      std::enable_if_t<std::is_base_of_v<RefBaseTag<jobject>, ParamT> &&
                       std::is_base_of_v<RefBaseTag<jobject>,
                                         T<class_v, class_loader_v, jvm_v>>>> {
    // TODO(b/174272629): Exclude objects loaded by invalid loaders.
    static constexpr bool val =
        std::string_view(class_v.name_) ==
        std::string_view(ParamSelectionT::GetParam().name_);
  };

  // TODO(b/174256299): Unfortunately, Locals come in two flavours, one with
  // a "dangerous move" tag, and one without.  Because they are parameterised
  // differently, an additional (redundant) partial specialisation is required.
  template <template <const auto&, const auto&, const auto&, typename> class T,
            const auto& class_v, const auto& class_loader_v, const auto& jvm_v,
            typename DangerousMoveTag>
  struct Helper<
      T<class_v, class_loader_v, jvm_v, DangerousMoveTag>,
      std::enable_if_t<
          std::is_base_of_v<RefBaseTag<jobject>, ParamT> &&
          std::is_base_of_v<RefBaseTag<jobject>, T<class_v, class_loader_v,
                                                   jvm_v, DangerousMoveTag>>>> {
    // TODO(b/174272629): Exclude objects loaded by invalid loaders.
    static constexpr bool val =
        std::string_view(class_v.name_) ==
        std::string_view(ParamSelectionT::GetParam().name_);
  };

  static constexpr bool val = Helper<Query>::val;
};

}  // namespace jni

#endif  // JNI_BIND_METHOD_SELECTION_H_
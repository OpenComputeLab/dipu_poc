#pragma once

#include <initializer_list>
#include <memory>
#include <type_traits>
#include <vector>

#include "detail/collector.h"
#include "detail/label.h"
#include "detail/value.h"

namespace dipu::metrics {

// LabeledValue is something like a tuple of {name, type, desc, labels, value}
// but just shared reference. This object will directly be used by users.
//
// Its safe to copy or move LabeledValue as they always refer to the same
// underlying value created by Collector. Every time it copys, reference count
// will increase.
//
// ValueOperation is used to provide some special operation for LabeledValue.
// e.g. when T is counter<int>, ValueOperation will provide {get, add, inc,
// reset} methods for current LabeledValue.
template <typename S /* string type */, typename T /* underlying value type */>
class LabeledValue : public detail::value_operation<LabeledValue<S, T>> {
  using owner_t = detail::group<S, T>;
  using value_t = typename owner_t::value_type;

  // LabeledValue is owned by Group, but it also prevents its owner being
  // destructed (via shared_ptr). Because LabeledValue(s) usually are kept by
  // external function or classes, their lifetime are hard to control.
  std::shared_ptr<owner_t> owner{};

  // value_t is actually a pair of labelset<S> and SharedValue<T>, thus:
  //
  // - pointer->first  is of type labelset<S>
  // - pointer->second is of type detail::SharedValue<T>
  value_t* pointer{};

 private:
  // Provide private access for ValueAccess (actually ValueOperation)
  friend struct detail::value_access<LabeledValue>;
  auto access() const noexcept -> T const& { return pointer->second.value; }
  auto access() noexcept -> T& {
    pointer->second.touch();
    return pointer->second.value;
  }

  // LabeledValue<S, T> should only be created by Group<S, T>
  friend class detail::group<S, T>;  // owner_type
  explicit LabeledValue(std::shared_ptr<owner_t> owner,
                        value_t& reference) noexcept
      : owner(std::move(owner)), pointer(&reference) {
    increase_reference_count();
  }

 public:
  using label = detail::label<S>;
  using labelset = detail::labelset<S>;

  auto name() const noexcept -> S const& { return owner->name(); }
  auto type() const noexcept -> S const& { return owner->type(); }

  auto labels() const noexcept -> std::vector<label> const& {
    return pointer->first.labels();
  }

  auto description() const noexcept -> S const& { return owner->description(); }

  // Create a new LabeledValue which has some external labels.
  //
  // Note: the created LabeledValue is in the same group of current
  // LabeledValue. Thus their name, type and description are same.
  [[nodiscard]] auto  //
  with(std::initializer_list<label> labels) const -> LabeledValue {
    return owner->template make<LabeledValue>(pointer->first.clone() += labels);
  }

  // Create a new LabeledValue which has some external labels.
  //
  // Note: the created LabeledValue is in the same group of current
  // LabeledValue. Thus their name, type and description are same.
  [[nodiscard]] auto with(labelset const& labels) const -> LabeledValue {
    return owner->template make<LabeledValue>(pointer->first.clone() += labels);
  }

  // Create a new LabeledValue which remove some external labels by label name.
  //
  // Note: the created LabeledValue is in the same group of current
  // LabeledValue. Thus their name, type and description are same.
  template <typename U>
  [[nodiscard]] auto  //
  without(std::initializer_list<U> names) const -> LabeledValue {
    static_assert(std::is_convertible_v<U, S>);
    return owner->template make<LabeledValue>(pointer->first.clone() -= names);
  }

  auto operator=(LabeledValue const& other) noexcept -> LabeledValue& {
    if (this != &other) {
      decrease_reference_count();
      owner = other.owner;
      pointer = other.pointer;
      increase_reference_count();
    }
    return *this;
  }

  auto operator=(LabeledValue&& other) noexcept -> LabeledValue& {
    if (this != &other) {
      decrease_reference_count();
      owner = std::move(other.owner);
      pointer = other.pointer;
      other.unset_nullptr();
    }
    return *this;
  }

  LabeledValue(LabeledValue const& other) noexcept
      : owner(other.owner), pointer(other.pointer) {
    increase_reference_count();
  }

  LabeledValue(LabeledValue&& other) noexcept
      : owner(std::move(other.owner)), pointer(other.pointer) {
    other.unset_nullptr();
  }

  ~LabeledValue() {
    decrease_reference_count();
    unset_nullptr();
  }

 private:
  auto unset_nullptr() noexcept {
    owner = nullptr;
    pointer = nullptr;
  }

  auto decrease_reference_count() noexcept {
    if (pointer) {
      pointer->second.decref();
    }
  }

  auto increase_reference_count() noexcept {
    if (pointer) {
      pointer->second.incref();
    }
  }
};

}  // namespace dipu::metrics

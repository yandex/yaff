#pragma once

#include "array.h"
#include "message.h"
#include "serializer.h"

namespace yaff::exp {

template <typename T>
struct Sizeable;

template <typename C, typename R>
void ColumnarParseTo(const C& from, R& to);

template <typename C, typename R>
::yaff::InternalOffset<C> ColumnarSerialize(::yaff::Serializer& ys, const R& from);

}  // namespace yaff::exp

#pragma once

#include <concepts>
#include <type_traits>


namespace eva::detail
{
    template<typename T>
    concept binary_value = std::same_as<T, std::remove_cv_t<T>> &&
        ((std::integral<T> && !std::same_as<T, bool>) ||
         std::same_as<T, float> || std::same_as<T, double>);
}

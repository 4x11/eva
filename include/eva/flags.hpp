#pragma once

#include <concepts>
#include <type_traits>
#include <utility>


namespace eva
{
    // class `flags`

    template<typename Enum>
        requires std::is_scoped_enum_v<Enum>
    class flags final
    {
    public:
        using enum_type = Enum;
        using underlying_type = std::make_unsigned_t<std::underlying_type_t<Enum>>;

    public:
        constexpr flags() noexcept = default;

        constexpr flags(Enum flag, std::same_as<Enum> auto... rest) noexcept
            : bits_(static_cast<underlying_type>((std::to_underlying(flag) | ... | std::to_underlying(rest))))
        {}

    public:
        [[nodiscard]] static constexpr flags from_bits(underlying_type bits) noexcept
        {
            flags result;
            result.bits_ = bits;
            return result;
        }

        [[nodiscard]] constexpr underlying_type bits() const noexcept
        {
            return bits_;
        }

        [[nodiscard]] constexpr bool empty() const noexcept
        {
            return bits_ == 0;
        }

        [[nodiscard]] constexpr bool contains_all(flags mask) const noexcept
        {
            return (bits_ & mask.bits_) == mask.bits_;
        }

        [[nodiscard]] constexpr bool contains_any(flags mask) const noexcept
        {
            return (bits_ & mask.bits_) != 0;
        }

    public:
        constexpr flags& set(flags mask) noexcept
        {
            bits_ |= mask.bits_;
            return *this;
        }

        constexpr flags& unset(flags mask) noexcept
        {
            bits_ &= static_cast<underlying_type>(~mask.bits_);
            return *this;
        }

        constexpr flags& toggle(flags mask) noexcept
        {
            bits_ ^= mask.bits_;
            return *this;
        }

        constexpr flags& clear() noexcept
        {
            bits_ = 0;
            return *this;
        }

    private:
        underlying_type bits_ = 0;
    };
}

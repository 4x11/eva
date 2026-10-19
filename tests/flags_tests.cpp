#include <eva/flags.hpp>

#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_template_test_macros.hpp>

#include <concepts>
#include <cstdint>
#include <limits>
#include <type_traits>


namespace
{
    enum class permission : std::uint8_t
    {
        none = 0,
        read = 1,
        write = 2,
        execute = 4,
        read_write = 3
    };

    enum class other_permission : std::uint8_t { read = 1 };
    enum unscoped_permission { read = 1 };

    using flags = eva::flags<permission>;

    template<typename T>
    concept flag_enum = requires { typename eva::flags<T>; };

    template<typename T>
    concept accepted_mask = requires(flags value, T mask)
    {
        value.set(mask);
        value.unset(mask);
        value.toggle(mask);
        value.contains_all(mask);
        value.contains_any(mask);
    };

    constexpr bool check_constexpr_operations()
    {
        flags value{permission::read, permission::write};
        if (value.bits() != 3 || !value.contains_all(permission::read_write))
            return false;
        value.unset(permission::write).set(permission::execute).toggle(permission::read);
        if (value.bits() != 4 || !value.contains_any({permission::write, permission::execute}))
            return false;
        if (value.contains_all({permission::read, permission::execute}))
            return false;
        value.set(flags::from_bits(0x80));
        if (value.bits() != 0x84)
            return false;
        value.clear();
        return value.empty() && value.contains_all({}) && !value.contains_any({});
    }

    static_assert(flag_enum<permission>);
    static_assert(!flag_enum<int>);
    static_assert(!flag_enum<unscoped_permission>);
    static_assert(accepted_mask<permission>);
    static_assert(accepted_mask<flags>);
    static_assert(!accepted_mask<other_permission>);
    static_assert(!accepted_mask<std::uint8_t>);
    static_assert(!std::is_convertible_v<flags, bool>);
    static_assert(!std::is_convertible_v<flags, std::uint8_t>);
    static_assert(std::is_same_v<flags::underlying_type, std::uint8_t>);
    static_assert(std::is_trivially_copyable_v<flags>);
    static_assert(std::is_standard_layout_v<flags>);
    static_assert(check_constexpr_operations());
    static_assert(noexcept(flags{}));
    static_assert(noexcept(flags{permission::read}));
    static_assert(noexcept(flags{permission::read, permission::write}));
    static_assert(noexcept(flags::from_bits(0)));
    static_assert(noexcept(flags{}.bits()));
    static_assert(noexcept(flags{}.empty()));
    static_assert(noexcept(flags{}.contains_all({})));
    static_assert(noexcept(flags{}.contains_any({})));
    static_assert(noexcept(flags{}.set({})));
    static_assert(noexcept(flags{}.unset({})));
    static_assert(noexcept(flags{}.toggle({})));
    static_assert(noexcept(flags{}.clear()));
}

TEST_CASE("flags distinguish all bits from any bits", "[flags]")
{
    const flags value{permission::read, permission::execute};

    REQUIRE(value.contains_all(permission::read));
    REQUIRE_FALSE(value.contains_all(permission::read_write));
    REQUIRE(value.contains_any(permission::read_write));
    REQUIRE_FALSE(value.contains_any(permission::write));
    REQUIRE(value.contains_all({permission::read, permission::execute}));
    REQUIRE(value.contains_all({}));
    REQUIRE_FALSE(value.contains_any({}));

    const flags empty;
    REQUIRE(empty.empty());
    REQUIRE(empty.bits() == 0);
    REQUIRE(empty.contains_all(permission::none));
    REQUIRE_FALSE(empty.contains_any(permission::none));
    REQUIRE_FALSE(empty.contains_all(permission::read));
}

TEST_CASE("flags mutate only the selected bits", "[flags]")
{
    flags value{permission::read, permission::read};
    REQUIRE(value.bits() == 1);
    REQUIRE(&value.set({permission::read, permission::write}) == &value);
    REQUIRE(value.bits() == 3);
    REQUIRE(&value.unset(permission::read) == &value);
    REQUIRE(value.bits() == 2);
    REQUIRE(&value.toggle({permission::write, permission::execute}) == &value);
    REQUIRE(value.bits() == 4);

    value.unset(permission::write);
    REQUIRE(value.bits() == 4);
    value.set({}).unset({}).toggle({});
    REQUIRE(value.bits() == 4);
    const flags mask{permission::read, permission::write};
    value.set(mask);
    REQUIRE(value.bits() == 7);
    value.unset(mask);
    REQUIRE(value.bits() == 4);
    value.toggle(mask).toggle(mask);
    REQUIRE(value.bits() == 4);
    REQUIRE(&value.clear() == &value);
    REQUIRE(value.empty());
}

TEMPLATE_TEST_CASE("flags preserve the full underlying width", "[flags]",
    std::uint8_t, std::uint16_t, std::uint32_t, std::uint64_t,
    std::int8_t, std::int16_t, std::int32_t, std::int64_t)
{
    using integer = TestType;
    using unsigned_integer = std::make_unsigned_t<integer>;
    enum class bit : integer { low = 1, high = std::numeric_limits<integer>::min() };
    using mask = eva::flags<bit>;

    static_assert(sizeof(mask) == sizeof(integer));
    static_assert(alignof(mask) == alignof(unsigned_integer));
    constexpr auto all_bits = std::numeric_limits<unsigned_integer>::max();
    constexpr auto high_bit = static_cast<unsigned_integer>(unsigned_integer{1}
        << (std::numeric_limits<unsigned_integer>::digits - 1));
    static_assert(mask::from_bits(all_bits).bits() == all_bits);
    constexpr mask high{static_cast<bit>(high_bit)};
    static_assert(high.bits() == high_bit);

    auto value = mask::from_bits(all_bits);
    REQUIRE(value.contains_all(high));
    value.unset(high);
    REQUIRE(value.bits() == static_cast<unsigned_integer>(all_bits ^ high_bit));
    value.toggle(high);
    REQUIRE(value.bits() == all_bits);
    value.unset(bit::low);
    REQUIRE(value.bits() == static_cast<unsigned_integer>(all_bits - 1));
    value.set(bit::low);
    REQUIRE(value.bits() == all_bits);
}

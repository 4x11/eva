#include <eva/binary_writer.hpp>

#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_template_test_macros.hpp>
#include <eva/binary_reader.hpp>

#include <array>
#include <cstdint>
#include <limits>
#include <type_traits>


namespace
{
    using writer = eva::binary_writer<>;
    using byte = std::byte;

    template<typename T>
    concept writable_value = requires(writer& output, T value)
    {
        output.template write_value<T>(value);
    };

    struct record { int value; };
    enum class tag { value };

    template<std::endian order, std::unsigned_integral T>
    constexpr auto encoded_integer(T value)
    {
        std::array<byte, sizeof(T)> bytes{};
        for (std::size_t i = 0; i < bytes.size(); ++i)
        {
            const auto shift = 8 * (order == std::endian::little ? i : bytes.size() - 1 - i);
            bytes[i] = static_cast<byte>((value >> shift) & 0xFFU);
        }
        return bytes;
    }

    template<std::endian order>
    constexpr bool check_constexpr_values()
    {
        std::array<byte, 6> bytes{};
        eva::binary_writer<order> output{std::span{bytes}};
        if (!output.template write_value<std::uint16_t>(0x1234) || output.position() != 2)
            return false;
        if (!output.write_value(1.5f) || !output.eof())
            return false;

        const auto integer = encoded_integer<order>(std::uint16_t{0x1234});
        const auto floating = encoded_integer<order>(std::uint32_t{0x3FC00000});
        return std::ranges::equal(std::span{bytes}.first(2), integer) &&
            std::ranges::equal(std::span{bytes}.subspan(2), floating);
    }

    constexpr bool check_constexpr_views_and_copying()
    {
        std::array<byte, 5> bytes{};
        writer output{std::span{bytes}};
        const byte source[]{byte{0x11}, byte{0x22}};
        if (!output.write(std::span{source}) || output.position() != 2)
            return false;
        auto view = output.write_view(2);
        if (!view || view->data() != bytes.data() + 2 || view->size() != 2 || output.position() != 4)
            return false;
        writer child{*view};
        if (!child.write_value<std::uint16_t>(0x1234) || !child.eof())
            return false;
        if (!output.skip(1) || !output.eof() || !output.seek(0))
            return false;

        const std::array expected{byte{0x11}, byte{0x22}, byte{0x34}, byte{0x12}, byte{0}};
        return bytes == expected && output.position() == 0 && output.remaining() == 5 &&
            output.data() == bytes.data() && output.span().size() == bytes.size();
    }

    constexpr bool check_constexpr_errors_and_empty_operations()
    {
        std::array bytes{byte{0xAA}, byte{0xBB}, byte{0xCC}};
        const auto original = bytes;
        writer output{std::span{bytes}};
        if (!output.skip(2))
            return false;
        const auto error = std::unexpected(std::errc::result_out_of_range);
        const byte source[2]{};
        if (output.write(std::span{source}) != error || output.write_value<std::uint16_t>(1) != error)
            return false;
        if (output.write_view(2) != error)
            return false;
        if (output.seek(4) != error || output.skip(std::numeric_limits<std::size_t>::max()) != error)
            return false;
        if (output.position() != 2 || bytes != original || !output.seek(3))
            return false;
        if (!output.write(std::span<const byte>{}) || !output.skip(0) || !output.write_view(0))
            return false;
        auto view = output.write_view(0);
        return view && view->empty() && view->data() == bytes.data() + 3 && output.eof() && bytes == original;
    }

    template<std::endian order, typename T>
    void check_integer_boundaries()
    {
        const std::array<T, 5> values{
            std::numeric_limits<T>::min(), std::numeric_limits<T>::max(),
            T{0}, T{1}, static_cast<T>(-1)};

        for (const auto value : values)
        {
            CAPTURE(order, +value);
            // Offset by one byte to exercise unaligned destinations as well.
            std::array<byte, sizeof(T) + 2> bytes{};
            eva::binary_writer<order> output{std::span{bytes}.subspan(1, sizeof(T))};
            REQUIRE(output.write_value(value));
            const auto expected = encoded_integer<order>(static_cast<std::make_unsigned_t<T>>(value));
            REQUIRE(std::ranges::equal(output.span(), expected));
            REQUIRE((bytes.front() == byte{}));
            REQUIRE((bytes.back() == byte{}));
            REQUIRE(output.eof());

            eva::binary_reader<order> input{output.span()};
            REQUIRE(input.template read_value<T>() == value);
        }
    }

    template<std::endian order, typename T, typename U, std::size_t count>
    void check_floating_representations(const std::array<U, count>& patterns)
    {
        for (const auto pattern : patterns)
        {
            CAPTURE(order, pattern);
            std::array<byte, sizeof(T)> bytes{};
            eva::binary_writer<order> output{std::span{bytes}};
            REQUIRE(output.write_value(std::bit_cast<T>(pattern)));
            REQUIRE((bytes == encoded_integer<order>(pattern)));
            REQUIRE(output.eof());
        }
    }
}


TEST_CASE("binary_writer accepts only supported scalar types and mutable buffers", "[binary_writer]")
{
    STATIC_REQUIRE(writable_value<std::uint8_t>);
    STATIC_REQUIRE(writable_value<std::uint16_t>);
    STATIC_REQUIRE(writable_value<std::uint32_t>);
    STATIC_REQUIRE(writable_value<std::uint64_t>);
    STATIC_REQUIRE(writable_value<std::int8_t>);
    STATIC_REQUIRE(writable_value<std::int16_t>);
    STATIC_REQUIRE(writable_value<std::int32_t>);
    STATIC_REQUIRE(writable_value<std::int64_t>);
    STATIC_REQUIRE(writable_value<float>);
    STATIC_REQUIRE(writable_value<double>);
    STATIC_REQUIRE_FALSE(writable_value<bool>);
    STATIC_REQUIRE_FALSE(writable_value<int*>);
    STATIC_REQUIRE_FALSE(writable_value<int&>);
    STATIC_REQUIRE_FALSE(writable_value<const int>);
    STATIC_REQUIRE_FALSE(writable_value<volatile int>);
    STATIC_REQUIRE_FALSE(writable_value<long double>);
    STATIC_REQUIRE_FALSE(writable_value<record>);
    STATIC_REQUIRE_FALSE(writable_value<tag>);
    STATIC_REQUIRE((std::is_constructible_v<writer, std::span<byte>>));
    STATIC_REQUIRE_FALSE((std::is_constructible_v<writer, std::span<const byte>>));
    STATIC_REQUIRE_FALSE((std::is_constructible_v<writer, const void*, std::size_t>));
    STATIC_REQUIRE_FALSE((std::is_convertible_v<std::span<byte>, writer>));
}

TEST_CASE("binary_writer supports constant evaluation", "[binary_writer][constexpr]")
{
    STATIC_REQUIRE(check_constexpr_values<std::endian::little>());
    STATIC_REQUIRE(check_constexpr_values<std::endian::big>());
    STATIC_REQUIRE(check_constexpr_values<std::endian::native>());
    STATIC_REQUIRE(check_constexpr_views_and_copying());
    STATIC_REQUIRE(check_constexpr_errors_and_empty_operations());
}

TEST_CASE("empty binary_writer permits zero-length operations", "[binary_writer]")
{
    writer output;
    REQUIRE(output.data() == nullptr);
    REQUIRE(output.span().empty());
    REQUIRE(output.size() == 0);
    REQUIRE(output.position() == 0);
    REQUIRE(output.remaining() == 0);
    REQUIRE(output.eof());
    REQUIRE(output.seek(0));
    REQUIRE(output.skip(0));
    REQUIRE(output.write(std::span<const byte>{}));
    REQUIRE(output.write_view(0));
    REQUIRE(output.write_value<std::uint8_t>(1) == std::unexpected(std::errc::result_out_of_range));
    REQUIRE(output.position() == 0);

    writer from_pointer{nullptr, 0};
    REQUIRE(from_pointer.eof());
    REQUIRE(from_pointer.write_view(0));
}

TEST_CASE("binary_writer pointer construction and getters describe the full buffer", "[binary_writer]")
{
    byte bytes[4]{};
    writer output{static_cast<void*>(bytes), sizeof(bytes)};
    REQUIRE(output.skip(2));
    const auto& observer = output;
    REQUIRE((observer.data() == bytes));
    REQUIRE((observer.span().data() == bytes));
    REQUIRE(observer.span().size() == 4);
    REQUIRE(observer.size() == 4);
    REQUIRE(observer.position() == 2);
    REQUIRE(observer.remaining() == 2);
    REQUIRE_FALSE(observer.eof());
    REQUIRE(output.write_value<std::uint16_t>(0x1234));
    REQUIRE((bytes[2] == byte{0x34}));
    REQUIRE((bytes[3] == byte{0x12}));
    REQUIRE(observer.eof());
}

TEMPLATE_TEST_CASE("binary_writer preserves integer boundaries in both byte orders", "[binary_writer][integers]",
    std::uint8_t, std::uint16_t, std::uint32_t, std::uint64_t,
    std::int8_t, std::int16_t, std::int32_t, std::int64_t)
{
    check_integer_boundaries<std::endian::little, TestType>();
    check_integer_boundaries<std::endian::big, TestType>();
}

TEST_CASE("binary_writer preserves floating-point representations in both byte orders", "[binary_writer][floating_point]")
{
    const std::array<std::uint32_t, 7> floats{
        0x00000000U, 0x80000000U, 0x3FC00000U, 0x00000001U, 0x7F800000U, 0xFF800000U, 0x7FC12345U};
    check_floating_representations<std::endian::little, float>(floats);
    check_floating_representations<std::endian::big, float>(floats);
    const std::array<std::uint64_t, 7> doubles{
        0x0000000000000000ULL, 0x8000000000000000ULL, 0x3FF8000000000000ULL,
        0x0000000000000001ULL, 0x7FF0000000000000ULL, 0xFFF0000000000000ULL, 0x7FF8123456789ABCULL};
    check_floating_representations<std::endian::little, double>(doubles);
    check_floating_representations<std::endian::big, double>(doubles);
}

TEMPLATE_TEST_CASE("binary_writer rejects every truncated scalar without modifying storage", "[binary_writer][errors]",
    std::uint8_t, std::uint16_t, std::uint32_t, std::uint64_t,
    std::int8_t, std::int16_t, std::int32_t, std::int64_t, float, double)
{
    for (std::size_t available = 0; available < sizeof(TestType); ++available)
    {
        CAPTURE(available);
        std::array<byte, sizeof(TestType) + 1> bytes;
        bytes.fill(byte{0xAA});
        const auto original = bytes;
        writer output{std::span{bytes}.first(available + 1)};
        REQUIRE(output.skip(1));
        REQUIRE(output.write_value(TestType{1}) == std::unexpected(std::errc::result_out_of_range));
        REQUIRE((bytes == original));
        REQUIRE(output.position() == 1);
        REQUIRE(output.remaining() == available);
    }
}

TEST_CASE("binary_writer copies bytes in source order and checks every short-buffer boundary", "[binary_writer][copy]")
{
    const std::array source{byte{0x11}, byte{0x22}, byte{0x33}, byte{0x44}, byte{0x55}};
    for (std::size_t offset = 0; offset <= source.size(); ++offset)
    {
        for (std::size_t count = 0; count <= source.size(); ++count)
        {
            CAPTURE(offset, count);
            std::array<byte, 5> bytes;
            bytes.fill(byte{0xAA});
            const auto original = bytes;
            eva::binary_writer<std::endian::big> output{std::span{bytes}};
            REQUIRE(output.skip(offset));
            auto result = output.write(std::span{source}.first(count));
            if (count > bytes.size() - offset)
            {
                REQUIRE(result == std::unexpected(std::errc::result_out_of_range));
                REQUIRE(output.position() == offset);
                REQUIRE((bytes == original));
            }
            else
            {
                REQUIRE(result);
                REQUIRE(output.position() == offset + count);
                for (std::size_t i = 0; i < bytes.size(); ++i)
                    REQUIRE((bytes[i] == (i >= offset && i - offset < count ? source[i - offset] : byte{0xAA})));
            }
        }
    }
}

TEST_CASE("binary_writer views share storage with independent nested cursors", "[binary_writer][view]")
{
    std::array<byte, 6> bytes{};
    writer output{std::span{bytes}};
    REQUIRE(output.skip(1));
    auto view = output.write_view(4);
    REQUIRE(view);
    REQUIRE(view->data() == bytes.data() + 1);
    REQUIRE(view->size() == 4);
    REQUIRE(output.position() == 5);
    REQUIRE((bytes == std::array<byte, 6>{}));
    eva::binary_writer<std::endian::big> child{*view};
    REQUIRE(child.write_value<std::uint32_t>(0x12345678));
    REQUIRE(child.eof());
    REQUIRE(output.position() == 5);
    const std::array expected{byte{0}, byte{0x12}, byte{0x34}, byte{0x56}, byte{0x78}, byte{0}};
    REQUIRE((bytes == expected));
}

TEST_CASE("binary_writer validates view sizes and cursor arithmetic without overflow", "[binary_writer][errors]")
{
    std::array bytes{byte{0xAA}, byte{0xBB}, byte{0xCC}};
    const auto original = bytes;
    writer output{std::span{bytes}};
    REQUIRE(output.skip(2));
    const auto error = std::unexpected(std::errc::result_out_of_range);
    for (const auto size : {std::size_t{2}, std::numeric_limits<std::size_t>::max()})
    {
        REQUIRE(output.write_view(size) == error);
        REQUIRE(output.skip(size) == error);
        REQUIRE(output.position() == 2);
        REQUIRE((bytes == original));
    }
    REQUIRE(output.seek(4) == error);
    REQUIRE(output.seek(std::numeric_limits<std::size_t>::max()) == error);
    REQUIRE(output.position() == 2);
    REQUIRE(output.seek(3));
    REQUIRE(output.eof());
    REQUIRE(output.seek(0));
    REQUIRE(output.write_value<std::uint16_t>(0x1234));
    REQUIRE((bytes[0] == byte{0x34}));
    REQUIRE((bytes[1] == byte{0x12}));
    REQUIRE((bytes[2] == byte{0xCC}));
}

TEST_CASE("binary_writer copies have independent cursors over shared storage", "[binary_writer]")
{
    byte bytes[2]{};
    writer output{std::span{bytes}};
    auto copy = output;
    REQUIRE(copy.write_value<std::uint16_t>(0x1234));
    REQUIRE(copy.eof());
    REQUIRE(output.position() == 0);
    REQUIRE(output.write_value<std::uint8_t>(0xAA));
    REQUIRE((bytes[0] == byte{0xAA}));
    REQUIRE((bytes[1] == byte{0x12}));
}

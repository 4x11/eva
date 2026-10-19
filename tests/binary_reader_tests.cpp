#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_template_test_macros.hpp>
#include <eva/binary_reader.hpp>

#include <array>
#include <cstdint>
#include <limits>
#include <type_traits>


namespace
{
    using reader = eva::binary_reader<>;
    using byte = std::byte;

    template<typename T>
    concept readable_value = requires(reader& input, const reader& const_input)
    {
        input.template read_value<T>();
        const_input.template peek_value<T>();
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

    template<std::endian order, typename T>
    void check_integer_boundaries()
    {
        const std::array<T, 5> values{
            std::numeric_limits<T>::min(), std::numeric_limits<T>::max(),
            T{0}, T{1}, static_cast<T>(-1)};

        for (const auto value : values)
        {
            CAPTURE(order, +value);
            const auto bytes = encoded_integer<order>(static_cast<std::make_unsigned_t<T>>(value));
            eva::binary_reader<order> input{std::span{bytes}};
            const auto& observer = input;
            REQUIRE(observer.template peek_value<T>() == value);
            REQUIRE(input.position() == 0);
            REQUIRE(input.remaining() == sizeof(T));
            REQUIRE(input.template read_value<T>() == value);
            REQUIRE(input.position() == sizeof(T));
            REQUIRE(input.eof());
            REQUIRE(input.template read_value<T>() == std::unexpected(std::errc::result_out_of_range));
            REQUIRE(input.position() == sizeof(T));
        }
    }

    template<std::endian order, typename T, typename U, std::size_t count>
    void check_floating_representations(const std::array<U, count>& patterns)
    {
        for (const auto pattern : patterns)
        {
            CAPTURE(order, pattern);
            const auto bytes = encoded_integer<order>(pattern);
            eva::binary_reader<order> input{std::span{bytes}};
            const auto& observer = input;
            auto preview = observer.template peek_value<T>();
            REQUIRE(preview);
            REQUIRE(std::bit_cast<U>(*preview) == pattern);
            REQUIRE(input.position() == 0);
            auto value = input.template read_value<T>();
            REQUIRE(value);
            REQUIRE(std::bit_cast<U>(*value) == pattern);
            REQUIRE(input.eof());
        }
    }

    constexpr bool check_constexpr_little_endian_values()
    {
        const byte bytes[]{byte{0x34}, byte{0x12}, byte{0x00}, byte{0x00}, byte{0xC0}, byte{0x3F}};
        reader input{std::span{bytes}};
        if (input.read_value<std::uint16_t>() != 0x1234 || input.position() != 2)
            return false;
        if (input.peek_value<float>() != 1.5f || input.position() != 2)
            return false;

        return input.read_value<float>() == 1.5f && input.eof();
    }

    constexpr bool check_constexpr_big_endian_values()
    {
        const byte bytes[]{byte{0x12}, byte{0x34}, byte{0x3F}, byte{0xC0}, byte{0x00}, byte{0x00}};
        eva::binary_reader<std::endian::big> input{std::span{bytes}};
        if (input.read_value<std::uint16_t>() != 0x1234 || input.position() != 2)
            return false;
        if (input.peek_value<float>() != 1.5f || input.position() != 2)
            return false;

        return input.read_value<float>() == 1.5f && input.eof();
    }

    constexpr bool check_constexpr_nested_views()
    {
        const byte bytes[]{byte{0xFF}, byte{0xFF}, byte{0x00}, byte{0x00}, byte{0xC0}, byte{0x3F}};
        reader input{std::span{bytes}};
        if (!input.skip(2))
            return false;
        auto preview = input.peek_view(4);
        if (!preview || preview->data() != bytes + 2 || input.position() != 2)
            return false;
        auto view = input.read_view(4);
        if (!view || view->data() != preview->data() || view->size() != 4 || !input.eof())
            return false;
        reader child{*view};

        return child.position() == 0 && child.read_value<float>() == 1.5f && child.eof() && input.eof();
    }

    constexpr bool check_constexpr_zero_length_operations()
    {
        const byte bytes[]{byte{0xAA}};
        reader input{std::span{bytes}};
        if (!input.seek(input.size()))
            return false;
        if (!input.read_view(0) || !input.skip(0))
            return false;
        if (!input.read(std::span<byte>{}) || !input.peek(std::span<byte>{}))
            return false;
        auto view = input.peek_view(0);

        return view && view->empty() && view->data() == bytes + 1 && input.position() == 1 && input.eof();
    }

    constexpr bool check_constexpr_copying()
    {
        const byte bytes[]{byte{0x34}, byte{0x12}};
        reader input{std::span{bytes}};
        byte destination[2]{};
        if (!input.peek(std::span{destination}) || input.position() != 0)
            return false;
        if (destination[0] != byte{0x34} || destination[1] != byte{0x12})
            return false;
        destination[0] = byte{};
        destination[1] = byte{};
        if (!input.read(std::span{destination}) || input.position() != 2)
            return false;

        return destination[0] == byte{0x34} && destination[1] == byte{0x12} && input.eof();
    }

    constexpr bool check_constexpr_errors_preserve_state()
    {
        const byte bytes[]{byte{0x11}, byte{0x22}, byte{0x33}};
        reader input{std::span{bytes}};
        if (!input.skip(2))
            return false;
        const auto error = std::unexpected(std::errc::result_out_of_range);
        if (input.read_value<std::uint16_t>() != error || input.peek_value<float>() != error)
            return false;
        if (input.read_view(2) != error || input.peek_view(2) != error)
            return false;
        if (input.seek(4) != error || input.skip(std::numeric_limits<std::size_t>::max()) != error)
            return false;
        byte destination[]{byte{0xAA}, byte{0xBB}};
        if (input.read(std::span{destination}) != error || input.peek(std::span{destination}) != error)
            return false;

        return input.position() == 2 && input.remaining() == 1 &&
            destination[0] == byte{0xAA} && destination[1] == byte{0xBB};
    }

    constexpr bool check_constexpr_position_and_getters()
    {
        const byte bytes[4]{};
        reader input{std::span{bytes}};
        if (!input.skip(2))
            return false;
        if (input.data() != bytes || input.span().data() != bytes || input.span().size() != 4)
            return false;
        if (input.size() != 4 || input.position() != 2 || input.remaining() != 2 || input.eof())
            return false;
        if (!input.seek(input.size()) || !input.eof() || !input.seek(0))
            return false;

        return input.position() == 0 && input.remaining() == 4 && !input.eof();
    }
}


TEST_CASE("binary_reader accepts only supported scalar types", "[binary_reader]")
{
    STATIC_REQUIRE(readable_value<std::uint8_t>);
    STATIC_REQUIRE(readable_value<std::uint16_t>);
    STATIC_REQUIRE(readable_value<std::uint32_t>);
    STATIC_REQUIRE(readable_value<std::uint64_t>);
    STATIC_REQUIRE(readable_value<std::int8_t>);
    STATIC_REQUIRE(readable_value<std::int16_t>);
    STATIC_REQUIRE(readable_value<std::int32_t>);
    STATIC_REQUIRE(readable_value<std::int64_t>);
    STATIC_REQUIRE(readable_value<float>);
    STATIC_REQUIRE(readable_value<double>);
    STATIC_REQUIRE_FALSE(readable_value<bool>);
    STATIC_REQUIRE_FALSE(readable_value<int*>);
    STATIC_REQUIRE_FALSE(readable_value<int&>);
    STATIC_REQUIRE_FALSE(readable_value<const int>);
    STATIC_REQUIRE_FALSE(readable_value<volatile int>);
    STATIC_REQUIRE_FALSE(readable_value<long double>);
    STATIC_REQUIRE_FALSE(readable_value<record>);
    STATIC_REQUIRE_FALSE(readable_value<tag>);
    STATIC_REQUIRE((std::is_constructible_v<reader, std::span<const byte>>));
    STATIC_REQUIRE_FALSE((std::is_convertible_v<std::span<const byte>, reader>));
}

TEST_CASE("binary_reader reads little-endian values during constant evaluation", "[binary_reader][constexpr]")
{
    STATIC_REQUIRE(check_constexpr_little_endian_values());
}

TEST_CASE("binary_reader reads big-endian values during constant evaluation", "[binary_reader][constexpr]")
{
    STATIC_REQUIRE(check_constexpr_big_endian_values());
}

TEST_CASE("binary_reader creates nested views during constant evaluation", "[binary_reader][constexpr]")
{
    STATIC_REQUIRE(check_constexpr_nested_views());
}

TEST_CASE("binary_reader permits zero-length operations during constant evaluation", "[binary_reader][constexpr]")
{
    STATIC_REQUIRE(check_constexpr_zero_length_operations());
}

TEST_CASE("binary_reader copies bytes during constant evaluation", "[binary_reader][constexpr]")
{
    STATIC_REQUIRE(check_constexpr_copying());
}

TEST_CASE("binary_reader preserves state on errors during constant evaluation", "[binary_reader][constexpr]")
{
    STATIC_REQUIRE(check_constexpr_errors_preserve_state());
}

TEST_CASE("binary_reader positions and observes the buffer during constant evaluation", "[binary_reader][constexpr]")
{
    STATIC_REQUIRE(check_constexpr_position_and_getters());
}

TEST_CASE("empty binary_reader permits zero-length operations", "[binary_reader]")
{
    reader input;
    REQUIRE(input.data() == nullptr);
    REQUIRE(input.span().empty());
    REQUIRE(input.size() == 0);
    REQUIRE(input.position() == 0);
    REQUIRE(input.remaining() == 0);
    REQUIRE(input.eof());
    REQUIRE(input.seek(0));
    REQUIRE(input.skip(0));
    REQUIRE(input.read(std::span<byte>{}));
    REQUIRE(input.peek(std::span<byte>{}));
    REQUIRE(input.read_view(0));
    REQUIRE(input.peek_view(0));
    REQUIRE(input.position() == 0);

    auto value = input.read_value<std::uint8_t>();
    REQUIRE_FALSE(value);
    REQUIRE(value.error() == std::errc::result_out_of_range);

    reader from_pointer{nullptr, 0};
    REQUIRE(from_pointer.eof());
    REQUIRE(from_pointer.read_view(0));
}

TEST_CASE("binary_reader getters always describe the full buffer", "[binary_reader]")
{
    const std::array bytes{byte{1}, byte{2}, byte{3}, byte{4}};
    reader from_span{std::span{bytes}};
    reader from_pointer{bytes.data(), bytes.size()};

    for (auto* input : {&from_span, &from_pointer})
    {
        REQUIRE(input->skip(2));
        REQUIRE(input->position() == 2);
        REQUIRE(input->remaining() == 2);
        REQUIRE(input->size() == bytes.size());
        REQUIRE(input->data() == bytes.data());
        REQUIRE(input->span().data() == bytes.data());
        REQUIRE(input->span().size() == bytes.size());
        REQUIRE_FALSE(input->eof());
    }
}

TEST_CASE("binary_reader bounds checks preserve the cursor and avoid overflow", "[binary_reader]")
{
    const std::array bytes{byte{1}, byte{2}, byte{3}};
    reader input{std::span{bytes}};
    REQUIRE(input.seek(1));

    const auto maximum = std::numeric_limits<std::size_t>::max();
    for (const auto count : {std::size_t{4}, maximum})
    {
        CAPTURE(count);
        REQUIRE(input.seek(count) == std::unexpected(std::errc::result_out_of_range));
        REQUIRE(input.skip(count) == std::unexpected(std::errc::result_out_of_range));
        REQUIRE(input.read_view(count) == std::unexpected(std::errc::result_out_of_range));
        REQUIRE(input.peek_view(count) == std::unexpected(std::errc::result_out_of_range));
        REQUIRE(input.position() == 1);
    }

    REQUIRE(input.read_value<std::uint32_t>() == std::unexpected(std::errc::result_out_of_range));
    REQUIRE(input.peek_value<std::uint32_t>() == std::unexpected(std::errc::result_out_of_range));
    REQUIRE(input.position() == 1);
    REQUIRE(input.skip(2));
    REQUIRE(input.eof());
    REQUIRE(input.seek(input.size()));
    REQUIRE(input.read_view(0));
    REQUIRE(input.peek_view(0));
    REQUIRE(input.read(std::span<byte>{}));
    REQUIRE(input.peek(std::span<byte>{}));
    REQUIRE(input.skip(0));
    REQUIRE(input.position() == input.size());
    REQUIRE(input.seek(0));
    REQUIRE_FALSE(input.eof());
}

TEST_CASE("binary_reader copies bytes without changing their order", "[binary_reader]")
{
    const std::array bytes{byte{1}, byte{2}, byte{3}, byte{4}, byte{5}, byte{6}};
    eva::binary_reader<std::endian::big> input{std::span{bytes}};
    const auto& observer = input;
    REQUIRE(input.skip(1));

    std::array<byte, 2> destination{};
    REQUIRE(observer.peek(std::span{destination}));
    REQUIRE((destination == std::array{byte{2}, byte{3}}));
    REQUIRE(input.position() == 1);
    REQUIRE(input.read(std::span{destination}));
    REQUIRE((destination == std::array{byte{2}, byte{3}}));
    REQUIRE(input.position() == 3);

    std::array<unsigned char, 2> raw{};
    auto raw_bytes = std::as_writable_bytes(std::span{raw});
    REQUIRE(observer.peek(raw_bytes));
    REQUIRE((raw == std::array<unsigned char, 2>{4, 5}));
    REQUIRE(input.position() == 3);
    REQUIRE(input.read(raw_bytes));
    REQUIRE((raw == std::array<unsigned char, 2>{4, 5}));
    REQUIRE(input.position() == 5);
}

TEST_CASE("binary_reader copy errors preserve destination and cursor", "[binary_reader]")
{
    const std::array bytes{byte{1}, byte{2}, byte{3}};
    reader input{std::span{bytes}};
    REQUIRE(input.seek(1));
    std::array destination{byte{0xAA}, byte{0xBB}, byte{0xCC}};
    const auto original = destination;

    REQUIRE(input.read(std::span{destination}) == std::unexpected(std::errc::result_out_of_range));
    REQUIRE((destination == original));
    REQUIRE(input.peek(std::span{destination}) == std::unexpected(std::errc::result_out_of_range));
    REQUIRE((destination == original));
    REQUIRE(input.position() == 1);
}

TEST_CASE("binary_reader views share storage with independent nested cursors", "[binary_reader]")
{
    std::array bytes{byte{1}, byte{2}, byte{3}, byte{4}, byte{5}};
    eva::binary_reader<std::endian::big> input{std::span{bytes}};
    REQUIRE(input.skip(1));

    const auto& observer = input;
    auto preview = observer.peek_view(3);
    REQUIRE(preview);
    REQUIRE(preview->data() == bytes.data() + 1);
    REQUIRE(preview->size() == 3);
    REQUIRE(input.position() == 1);

    auto block = input.read_view(3);
    REQUIRE(block);
    REQUIRE(block->data() == preview->data());
    REQUIRE(std::to_integer<unsigned>((*block)[0]) == 2);
    REQUIRE(input.position() == 4);
    reader child{*block};
    REQUIRE(child.skip(1));
    auto section = child.read_view(1);
    REQUIRE(section);
    reader leaf{*section};

    bytes[2] = byte{0xAA};
    REQUIRE(leaf.read_value<std::uint8_t>() == 0xAA);
    REQUIRE(leaf.eof());
    REQUIRE(child.position() == 2);
    REQUIRE(input.position() == 4);
    REQUIRE(child.read_view(2) == std::unexpected(std::errc::result_out_of_range));
    REQUIRE(child.position() == 2);
    REQUIRE(input.read_value<std::uint8_t>() == 5);
}

TEST_CASE("binary_reader reads unaligned little-endian integers", "[binary_reader]")
{
    alignas(std::uint64_t) const std::array bytes{
        byte{0xFF}, byte{0x34}, byte{0x12},
        byte{0x78}, byte{0x56}, byte{0x34}, byte{0x12},
        byte{0xEF}, byte{0xCD}, byte{0xAB}, byte{0x89}, byte{0x67}, byte{0x45}, byte{0x23}, byte{0x01},
        byte{0xAB}, byte{0xFE}, byte{0xFF}};
    reader input{std::span{bytes}};
    REQUIRE(input.skip(1));
    REQUIRE(input.peek_value<std::uint16_t>() == 0x1234);
    REQUIRE(input.position() == 1);
    REQUIRE(input.read_value<std::uint16_t>() == 0x1234);
    REQUIRE(input.read_value<std::uint32_t>() == 0x12345678U);
    REQUIRE(input.read_value<std::uint64_t>() == 0x0123456789ABCDEFULL);
    REQUIRE(input.read_value<std::uint8_t>() == 0xAB);
    REQUIRE(input.read_value<std::int16_t>() == -2);
    REQUIRE(input.eof());
}

TEST_CASE("binary_reader reads unaligned big-endian integers", "[binary_reader]")
{
    alignas(std::uint64_t) const std::array bytes{
        byte{0xFF}, byte{0x12}, byte{0x34},
        byte{0x12}, byte{0x34}, byte{0x56}, byte{0x78},
        byte{0x01}, byte{0x23}, byte{0x45}, byte{0x67}, byte{0x89}, byte{0xAB}, byte{0xCD}, byte{0xEF},
        byte{0xAB}, byte{0xFF}, byte{0xFE}};
    eva::binary_reader<std::endian::big> input{std::span{bytes}};
    REQUIRE(input.skip(1));
    REQUIRE(input.peek_value<std::uint16_t>() == 0x1234);
    REQUIRE(input.position() == 1);
    REQUIRE(input.read_value<std::uint16_t>() == 0x1234);
    REQUIRE(input.read_value<std::uint32_t>() == 0x12345678U);
    REQUIRE(input.read_value<std::uint64_t>() == 0x0123456789ABCDEFULL);
    REQUIRE(input.read_value<std::uint8_t>() == 0xAB);
    REQUIRE(input.read_value<std::int16_t>() == -2);
    REQUIRE(input.eof());
}

TEST_CASE("binary_reader reads little-endian floating-point values", "[binary_reader]")
{
    const std::array bytes{
        byte{0x00}, byte{0x00}, byte{0xC0}, byte{0x3F},
        byte{0x00}, byte{0x00}, byte{0x00}, byte{0x00}, byte{0x00}, byte{0x00}, byte{0x02}, byte{0xC0}};
    reader input{std::span{bytes}};
    const auto& observer = input;
    REQUIRE(observer.peek_value<float>() == 1.5f);
    REQUIRE(input.position() == 0);
    REQUIRE(input.read_value<float>() == 1.5f);
    REQUIRE(observer.peek_value<double>() == -2.25);
    REQUIRE(input.position() == 4);
    REQUIRE(input.read_value<double>() == -2.25);
    REQUIRE(input.eof());
}

TEST_CASE("binary_reader reads big-endian floating-point values", "[binary_reader]")
{
    const std::array bytes{
        byte{0x3F}, byte{0xC0}, byte{0x00}, byte{0x00},
        byte{0xC0}, byte{0x02}, byte{0x00}, byte{0x00}, byte{0x00}, byte{0x00}, byte{0x00}, byte{0x00}};
    eva::binary_reader<std::endian::big> input{std::span{bytes}};
    const auto& observer = input;
    REQUIRE(observer.peek_value<float>() == 1.5f);
    REQUIRE(input.position() == 0);
    REQUIRE(input.read_value<float>() == 1.5f);
    REQUIRE(observer.peek_value<double>() == -2.25);
    REQUIRE(input.position() == 4);
    REQUIRE(input.read_value<double>() == -2.25);
    REQUIRE(input.eof());
}

TEMPLATE_TEST_CASE("binary_reader preserves integer boundaries in both byte orders", "[binary_reader][integers]",
    std::int8_t, std::uint8_t, std::int16_t, std::uint16_t,
    std::int32_t, std::uint32_t, std::int64_t, std::uint64_t)
{
    check_integer_boundaries<std::endian::little, TestType>();
    check_integer_boundaries<std::endian::big, TestType>();
}

TEST_CASE("binary_reader decodes every single-byte integer representation", "[binary_reader][integers]")
{
    for (unsigned value = 0; value < 256; ++value)
    {
        CAPTURE(value);
        const std::array bytes{static_cast<byte>(value)};
        reader little{std::span{bytes}};
        eva::binary_reader<std::endian::big> big{std::span{bytes}};
        const auto signed_value = static_cast<std::int8_t>(value < 128 ? static_cast<int>(value) : static_cast<int>(value) - 256);

        REQUIRE(little.peek_value<std::int8_t>() == signed_value);
        REQUIRE(big.peek_value<std::int8_t>() == signed_value);
        REQUIRE(little.read_value<std::uint8_t>() == value);
        REQUIRE(big.read_value<std::uint8_t>() == value);
        REQUIRE(little.eof());
        REQUIRE(big.eof());
    }
}

TEST_CASE("binary_reader preserves float special representations", "[binary_reader][floating_point]")
{
    const std::array<std::uint32_t, 9> patterns{
        0x00000000U, 0x80000000U, 0x7F800000U, 0xFF800000U,
        0x7FC12345U, 0xFFC12345U, 0x00000001U, 0x007FFFFFU, 0x7F7FFFFFU};

    check_floating_representations<std::endian::little, float>(patterns);
    check_floating_representations<std::endian::big, float>(patterns);
}

TEST_CASE("binary_reader preserves double special representations", "[binary_reader][floating_point]")
{
    const std::array<std::uint64_t, 9> patterns{
        0x0000000000000000ULL, 0x8000000000000000ULL, 0x7FF0000000000000ULL, 0xFFF0000000000000ULL,
        0x7FF8123456789ABCULL, 0xFFF8123456789ABCULL, 0x0000000000000001ULL,
        0x000FFFFFFFFFFFFFULL, 0x7FEFFFFFFFFFFFFFULL};

    check_floating_representations<std::endian::little, double>(patterns);
    check_floating_representations<std::endian::big, double>(patterns);
}

TEMPLATE_TEST_CASE("binary_reader rejects every truncated scalar without consuming bytes", "[binary_reader][errors]",
    std::int8_t, std::uint8_t, std::int16_t, std::uint16_t,
    std::int32_t, std::uint32_t, std::int64_t, std::uint64_t, float, double)
{
    const std::array<byte, sizeof(TestType) + 2> bytes{};
    for (std::size_t offset = 0; offset <= 2; ++offset)
    {
        for (std::size_t available = 0; available < sizeof(TestType); ++available)
        {
            CAPTURE(offset, available);
            auto check = [&]<std::endian order>()
            {
                eva::binary_reader<order> input{std::span{bytes}.first(offset + available)};
                REQUIRE(input.seek(offset));
                const auto& observer = input;
                REQUIRE(observer.template peek_value<TestType>() == std::unexpected(std::errc::result_out_of_range));
                REQUIRE(input.template read_value<TestType>() == std::unexpected(std::errc::result_out_of_range));
                REQUIRE(input.position() == offset);
                REQUIRE(input.remaining() == available);
                REQUIRE(input.read_view(available));
                REQUIRE(input.eof());
            };
            check.template operator()<std::endian::little>();
            check.template operator()<std::endian::big>();
        }
    }
}

TEST_CASE("binary_reader copies only the requested destination slice", "[binary_reader][copy]")
{
    const std::array bytes{byte{0x12}, byte{0x34}, byte{0x56}};
    reader input{std::span{bytes}};
    std::array destination{byte{0xAA}, byte{0xAA}, byte{0xAA}, byte{0xAA}, byte{0xAA}};
    REQUIRE(input.read(std::span{destination}.subspan(1, 3)));
    REQUIRE((destination == std::array{byte{0xAA}, byte{0x12}, byte{0x34}, byte{0x56}, byte{0xAA}}));
    REQUIRE(input.eof());
    REQUIRE((bytes == std::array{byte{0x12}, byte{0x34}, byte{0x56}}));
}

TEST_CASE("binary_reader copying handles every short-buffer offset and length", "[binary_reader][copy][errors]")
{
    const std::array bytes{byte{1}, byte{2}, byte{3}, byte{4}, byte{5}};
    for (std::size_t offset = 0; offset <= bytes.size(); ++offset)
    {
        for (std::size_t length = 0; length <= bytes.size() + 1; ++length)
        {
            for (bool consume : {false, true})
            {
                CAPTURE(offset, length, consume);
                reader input{std::span{bytes}};
                REQUIRE(input.seek(offset));
                std::array<byte, bytes.size() + 3> destination;
                destination.fill(byte{0xAA});
                auto target = std::span{destination}.subspan(1, length);
                auto result = consume ? input.read(target) : input.peek(target);

                if (length <= bytes.size() - offset)
                {
                    REQUIRE(result);
                    REQUIRE(input.position() == offset + (consume ? length : 0));
                    REQUIRE(std::ranges::equal(target, std::span{bytes}.subspan(offset, length)));
                }
                else
                {
                    REQUIRE(result == std::unexpected(std::errc::result_out_of_range));
                    REQUIRE(input.position() == offset);
                    REQUIRE(std::ranges::all_of(target, [](byte value) { return value == byte{0xAA}; }));
                }

                REQUIRE(std::ranges::all_of(std::span{destination}.first(1), [](byte value) { return value == byte{0xAA}; }));
                REQUIRE(std::ranges::all_of(std::span{destination}.subspan(1 + length), [](byte value) { return value == byte{0xAA}; }));
            }
        }
    }
}

TEST_CASE("binary_reader views handle every short-buffer offset and length", "[binary_reader][view][errors]")
{
    const std::array bytes{byte{1}, byte{2}, byte{3}, byte{4}, byte{5}};
    for (std::size_t offset = 0; offset <= bytes.size(); ++offset)
    {
        for (std::size_t length = 0; length <= bytes.size() + 1; ++length)
        {
            for (bool consume : {false, true})
            {
                CAPTURE(offset, length, consume);
                reader input{std::span{bytes}};
                REQUIRE(input.seek(offset));
                auto result = consume ? input.read_view(length) : input.peek_view(length);
                if (length <= bytes.size() - offset)
                {
                    REQUIRE(result);
                    REQUIRE(result->data() == bytes.data() + offset);
                    REQUIRE(result->size() == length);
                    REQUIRE(input.position() == offset + (consume ? length : 0));
                }
                else
                {
                    REQUIRE(result == std::unexpected(std::errc::result_out_of_range));
                    REQUIRE(input.position() == offset);
                }
            }
        }
    }
}

TEST_CASE("binary_reader can seek backwards and resume after errors", "[binary_reader][position][errors]")
{
    const std::array bytes{byte{0x10}, byte{0x20}, byte{0x30}, byte{0x40}};
    reader input{std::span{bytes}};
    REQUIRE(input.seek(2));
    REQUIRE(input.read_value<std::uint8_t>() == 0x30);
    REQUIRE(input.read_value<std::uint32_t>() == std::unexpected(std::errc::result_out_of_range));
    REQUIRE(input.seek(5) == std::unexpected(std::errc::result_out_of_range));
    REQUIRE(input.read_value<std::uint8_t>() == 0x40);
    REQUIRE(input.eof());
    REQUIRE(input.skip(1) == std::unexpected(std::errc::result_out_of_range));
    REQUIRE(input.seek(1));
    REQUIRE(input.read_value<std::uint8_t>() == 0x20);
    REQUIRE(input.seek(0));
    REQUIRE(input.read_value<std::uint32_t>() == 0x40302010U);
    REQUIRE(input.eof());
}

TEST_CASE("binary_reader source changes affect views but not copies", "[binary_reader][view][copy]")
{
    std::array bytes{byte{0x10}, byte{0x20}};
    reader input{std::span{bytes}};
    std::array<byte, 2> copied{};
    REQUIRE(input.peek(std::span{copied}));
    auto view = input.read_view(2);
    REQUIRE(view);
    bytes[0] = byte{0xAA};
    reader child{*view};
    REQUIRE(child.read_value<std::uint8_t>() == 0xAA);
    REQUIRE((copied == std::array{byte{0x10}, byte{0x20}}));
    REQUIRE(input.eof());
}

TEST_CASE("binary_reader views outlive the parent reader when storage remains valid", "[binary_reader][view]")
{
    const std::array bytes{byte{0xFF}, byte{0x34}, byte{0x12}, byte{0xEE}};
    reader child;
    std::span<const byte> view;
    {
        reader parent{std::span{bytes}};
        REQUIRE(parent.skip(1));
        auto block = parent.read_view(2);
        REQUIRE(block);
        view = *block;
        child = reader{view};
    }

    REQUIRE(view.data() == bytes.data() + 1);
    REQUIRE(child.data() == view.data());
    REQUIRE(child.size() == 2);
    REQUIRE(child.position() == 0);
    REQUIRE(child.read_value<std::uint16_t>() == 0x1234);
    REQUIRE(child.read_value<std::uint8_t>() == std::unexpected(std::errc::result_out_of_range));
}

TEST_CASE("binary_reader copies share storage but have independent cursors", "[binary_reader][position]")
{
    const std::array bytes{byte{1}, byte{2}, byte{3}};
    reader original{std::span{bytes}};
    REQUIRE(original.skip(1));
    auto copied = original;
    reader assigned;
    assigned = original;
    REQUIRE(original.read_value<std::uint8_t>() == 2);
    REQUIRE(copied.position() == 1);
    REQUIRE(assigned.position() == 1);
    REQUIRE(copied.data() == original.data());
    REQUIRE(assigned.data() == original.data());
    REQUIRE(copied.read_value<std::uint16_t>() == 0x0302);
    REQUIRE(copied.eof());
    REQUIRE(original.position() == 2);
    REQUIRE(assigned.position() == 1);
}

TEST_CASE("binary_reader preserves the address of an empty supplied span", "[binary_reader][construction]")
{
    const std::array bytes{byte{0xAA}, byte{0xBB}};
    const auto empty = std::span{bytes}.subspan(1, 0);
    reader from_span{empty};
    reader from_pointer{empty.data(), empty.size()};
    for (auto* input : {&from_span, &from_pointer})
    {
        REQUIRE(input->data() == bytes.data() + 1);
        REQUIRE(input->size() == 0);
        REQUIRE(input->remaining() == 0);
        REQUIRE(input->eof());
        auto view = input->read_view(0);
        REQUIRE(view);
        REQUIRE(view->empty());
        REQUIRE(view->data() == empty.data());
        REQUIRE(input->read_view(1) == std::unexpected(std::errc::result_out_of_range));
    }
}

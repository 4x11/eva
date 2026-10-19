# eva

Header-only C++23 utilities.

```cpp
#include <eva/flags.hpp>

enum class permission : unsigned
{
    read = 1u << 0,
    write = 1u << 1,
    execute = 1u << 2
};

eva::flags<permission> access{permission::read, permission::write};
access.unset(permission::write).set(permission::execute);

bool readable = access.contains_all(permission::read);
bool writable_or_executable = access.contains_any({permission::write, permission::execute});
auto raw = access.bits();
auto restored = eva::flags<permission>::from_bits(raw);
```

`flags<Enum>` accepts scoped enums with bit-mask values (not bit indices).
It stores one unsigned integer of the enum's underlying width. All operations
are `constexpr` and `noexcept`; there are no overloaded operators or allocations.
Enums of different types cannot be mixed. Raw bits are imported explicitly with
`from_bits` and preserved, including bits without named enumerators.

`set`, `unset`, and `toggle` accept an enum value, a list of values, or another
`flags<Enum>`. They return a reference for chaining; `clear()` removes all bits.
For an empty mask, `contains_all` returns `true` and `contains_any` returns `false`.

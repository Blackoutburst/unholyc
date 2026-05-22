export const TYPE_SYSTEM = `
# UnholyC Type System (HolyC-style)

| UHC  | C++ equivalent     | Notes                     |
|------|--------------------|---------------------------|
| U0   | void               | No value / void return    |
| U8   | unsigned char      | 8-bit unsigned            |
| U16  | unsigned short     | 16-bit unsigned           |
| U32  | unsigned int       | 32-bit unsigned           |
| U64  | unsigned long long | 64-bit unsigned           |
| I8   | char               | 8-bit signed (also I8*)   |
| I16  | short              | 16-bit signed             |
| I32  | int                | 32-bit signed             |
| I64  | long long          | 64-bit signed             |
| F32  | float              | 32-bit float              |
| F64  | double             | 64-bit float              |

## Common Pointer Types
- \`const I8*\` — C string / format string / binary buffer
- \`U8*\` — raw byte buffer
- \`U0*\` — void pointer (generic data, thread args)

## String vs I8*
- **String** (stdlib type) → text data, file paths, names, identifiers
- **const I8*** → variadic format strings (\`const I8* fmt, ...\`), binary buffers
- **Rule**: if it's human-readable text, use \`String\`; if it's a printf format or raw bytes, use \`I8*\`/\`U8*\`

## Boolean
No \`bool\` — use \`U8\` (0 = false, non-zero = true)

## Templates
\`template<typename T>\` works. Template-within-template method calls not supported.
`;

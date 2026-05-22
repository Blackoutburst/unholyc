# UnholyC MCP Server

Language assistant for working with UnholyC. Gives Claude (and other MCP-capable models) searchable reference docs, stdlib signatures, boilerplate generators, and build instructions — without needing the source code in context.

## Tools

| Tool | Description |
|------|-------------|
| `uhc_overview` | Full language overview — start here |
| `uhc_types` | Type system (U0/U8…/I8…/F32/F64), String vs I8* |
| `uhc_namespaces` | Namespace + `self` syntax, reference semantics, header split |
| `uhc_lambdas` | Trailing lambda syntax, capture, templates, limitations |
| `uhc_syntax` | `%T`, `unused`, optional semicolons, cross-platform, interop |
| `uhc_stdlib` | Full stdlib reference (Log, String, List, Thread, Buffer, Math, TCP…) |
| `uhc_build` | Build commands, compiler modes, flags, auto-detection |
| `uhc_patterns` | Idioms, constructor pattern, server template, common mistakes |
| `uhc_search` | Keyword search across all docs |
| `uhc_generate_namespace` | Generate .uhh + .uhc boilerplate for a new namespace |

## Setup

### 1. Build

```bash
cd mcp
npm install
npm run build
```

### 2. Add to Claude Code settings

In `~/.claude/settings.json` (or project `.claude/settings.json`):

```json
{
  "mcpServers": {
    "unholyc": {
      "type": "stdio",
      "command": "node",
      "args": ["C:\\Users\\black\\Documents\\unholyc\\mcp\\dist\\index.js"]
    }
  }
}
```

On Linux/macOS:
```json
{
  "mcpServers": {
    "unholyc": {
      "type": "stdio",
      "command": "node",
      "args": ["/path/to/unholyc/mcp/dist/index.js"]
    }
  }
}
```

### 3. Restart Claude Code

The MCP tools appear automatically once connected.

## Using `uhc_generate_namespace`

```json
{
  "name": "Player",
  "fields": [
    { "type": "I32", "name": "health", "defaultValue": "100" },
    { "type": "String", "name": "name" },
    { "type": "F32", "name": "x", "defaultValue": "0.0f" },
    { "type": "F32", "name": "y", "defaultValue": "0.0f" }
  ],
  "methods": ["update", "render"],
  "withDestroy": false,
  "withToString": true,
  "includeHeaders": ["uhcio.uhh", "uhcstd.uhh"]
}
```

Outputs ready-to-use `.uhh` and `.uhc` stubs.

## Development

```bash
npm run dev   # run with tsx (no build needed)
npm run build # compile to dist/
```

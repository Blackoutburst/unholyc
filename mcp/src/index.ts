#!/usr/bin/env node
import { McpServer } from "@modelcontextprotocol/sdk/server/mcp.js";
import { StdioServerTransport } from "@modelcontextprotocol/sdk/server/stdio.js";
import { z } from "zod";

import { TYPE_SYSTEM } from "./data/types.js";
import { NAMESPACE_GUIDE, LAMBDA_GUIDE, SYNTAX_REFERENCE } from "./data/syntax.js";
import { STDLIB_REFERENCE } from "./data/stdlib.js";
import { BUILD_GUIDE, CODING_PATTERNS, OVERVIEW } from "./data/patterns.js";

// ─── All reference sections for search ─────────────────────────────────────
const ALL_DOCS: Record<string, string> = {
  overview: OVERVIEW,
  types: TYPE_SYSTEM,
  namespaces: NAMESPACE_GUIDE,
  lambdas: LAMBDA_GUIDE,
  syntax: SYNTAX_REFERENCE,
  stdlib: STDLIB_REFERENCE,
  build: BUILD_GUIDE,
  patterns: CODING_PATTERNS,
};

const server = new McpServer({
  name: "unholyc",
  version: "1.0.0",
  description:
    "UnholyC language assistant — type system, namespaces, lambdas, stdlib, build system, and idioms",
});

// ─── Tool: overview ─────────────────────────────────────────────────────────
server.tool(
  "uhc_overview",
  "Full UnholyC language overview — what it is, core concepts, file extensions, stdlib modules, compile commands, and a minimal example. Start here when beginning a new UHC project.",
  {},
  async () => ({
    content: [{ type: "text", text: OVERVIEW }],
  })
);

// ─── Tool: types ─────────────────────────────────────────────────────────────
server.tool(
  "uhc_types",
  "UnholyC type system — HolyC-style type names (U0/U8/U16/U32/U64, I8/I16/I32/I64, F32/F64), their C++ equivalents, pointer types, String vs I8*, bool substitute, and template notes.",
  {},
  async () => ({
    content: [{ type: "text", text: TYPE_SYSTEM }],
  })
);

// ─── Tool: namespaces ────────────────────────────────────────────────────────
server.tool(
  "uhc_namespaces",
  "Namespaces and the `self` keyword — the core UHC pattern. Covers: namespace syntax, self struct declaration, reference semantics (auto-& for self params), header/implementation split, utility namespaces without self.",
  {},
  async () => ({
    content: [{ type: "text", text: NAMESPACE_GUIDE }],
  })
);

// ─── Tool: lambdas ───────────────────────────────────────────────────────────
server.tool(
  "uhc_lambdas",
  "Lambda syntax in UnholyC — Kotlin-style trailing lambdas. Covers: lambda parameter declaration, trailing call syntax, templates + lambda, capture behavior (auto [&] when locals captured), limitations (no stored lambdas, no non-void return), cross-file usage.",
  {},
  async () => ({
    content: [{ type: "text", text: LAMBDA_GUIDE }],
  })
);

// ─── Tool: syntax ────────────────────────────────────────────────────────────
server.tool(
  "uhc_syntax",
  "UnholyC syntax reference — optional semicolons, %T format specifier (auto toString), `unused` keyword, post-increment/decrement, cross-platform guards, templates, C/C++ interop, array syntax, aggregate init, function pointers, file extensions.",
  {},
  async () => ({
    content: [{ type: "text", text: SYNTAX_REFERENCE }],
  })
);

// ─── Tool: stdlib ────────────────────────────────────────────────────────────
server.tool(
  "uhc_stdlib",
  "Full UnholyC standard library reference — Log, File, String, List<T>, Mutex, Thread, Buffer, Time, DynamicLibrary, Math, VectorF, VectorI, Matrix, TcpClient, TcpServer. Includes all method signatures with parameters and return types.",
  {
    module: z
      .enum([
        "all",
        "io",
        "string",
        "list",
        "threading",
        "buffer",
        "math",
        "network",
      ])
      .optional()
      .describe(
        "Filter to a specific module. Omit or use 'all' for the full reference."
      ),
  },
  async ({ module }) => {
    if (!module || module === "all") {
      return { content: [{ type: "text", text: STDLIB_REFERENCE }] };
    }

    const sections: Record<string, string> = {
      io: extractSection(STDLIB_REFERENCE, ["## Log", "## File"]),
      string: extractSection(STDLIB_REFERENCE, ["## String"]),
      list: extractSection(STDLIB_REFERENCE, ["## List<T>"]),
      threading: extractSection(STDLIB_REFERENCE, ["## Mutex", "## Thread"]),
      buffer: extractSection(STDLIB_REFERENCE, ["## Buffer", "## Time"]),
      math: extractSection(STDLIB_REFERENCE, [
        "## Math",
        "## VectorF",
        "## VectorI",
        "## Matrix",
      ]),
      network: extractSection(STDLIB_REFERENCE, ["## TcpClient", "## TcpServer"]),
    };

    const text =
      sections[module] ||
      `Module '${module}' not found. Valid: io, string, list, threading, buffer, math, network`;
    return { content: [{ type: "text", text }] };
  }
);

// ─── Tool: build ─────────────────────────────────────────────────────────────
server.tool(
  "uhc_build",
  "UnholyC build system and compiler usage — build-all.bat/sh, install.bat/sh, driver mode (transpile+compile), transpile-only mode, --preserve-source, stdlib auto-detection order, -I/-L flag syntax, project layout, minimal main.uhc.",
  {},
  async () => ({
    content: [{ type: "text", text: BUILD_GUIDE }],
  })
);

// ─── Tool: patterns ──────────────────────────────────────────────────────────
server.tool(
  "uhc_patterns",
  "UnholyC coding patterns and idioms — constructor pattern, toString for %T, threaded server pattern, file processing, buffer/network protocol, lambda for collection processing, common mistakes table (wrong extensions, stored lambdas, bool type, etc.).",
  {},
  async () => ({
    content: [{ type: "text", text: CODING_PATTERNS }],
  })
);

// ─── Tool: search ────────────────────────────────────────────────────────────
server.tool(
  "uhc_search",
  "Search across all UnholyC reference docs for a keyword or symbol. Returns all sections containing the query. Useful for quickly locating specific methods, types, or syntax.",
  {
    query: z
      .string()
      .describe(
        "Keyword or symbol to search for (e.g. 'String.format', 'lambda', 'TcpServer', 'unused')"
      ),
  },
  async ({ query }) => {
    const q = query.toLowerCase();
    const results: string[] = [];

    for (const [section, content] of Object.entries(ALL_DOCS)) {
      const lines = content.split("\n");
      const matchingLines: string[] = [];

      for (let i = 0; i < lines.length; i++) {
        if (lines[i].toLowerCase().includes(q)) {
          const start = Math.max(0, i - 2);
          const end = Math.min(lines.length - 1, i + 4);
          const snippet = lines.slice(start, end + 1).join("\n");
          if (!matchingLines.includes(snippet)) {
            matchingLines.push(snippet);
          }
        }
      }

      if (matchingLines.length > 0) {
        results.push(
          `### Found in: ${section}\n\n` + matchingLines.slice(0, 5).join("\n\n---\n\n")
        );
      }
    }

    if (results.length === 0) {
      return {
        content: [
          {
            type: "text",
            text: `No results for '${query}'. Try: type names (U8, I32, F32), namespace names (String, List, Mutex, Thread, Buffer, Time, Math, VectorF, Matrix, TcpClient, TcpServer, Log, File), or keywords (self, lambda, unused, %T).`,
          },
        ],
      };
    }

    return {
      content: [
        {
          type: "text",
          text: `# Search results for '${query}'\n\n` + results.join("\n\n---\n\n"),
        },
      ],
    };
  }
);

// ─── Tool: generate_namespace ─────────────────────────────────────────────────
server.tool(
  "uhc_generate_namespace",
  "Generate UnholyC namespace boilerplate. Produces a .uhh header and .uhc implementation stub with the self struct, create constructor, destroy (optional), and toString (optional). Paste into your project and fill in logic.",
  {
    name: z
      .string()
      .describe("Namespace name (PascalCase, e.g. Player, HttpClient, EventQueue)"),
    fields: z
      .array(
        z.object({
          type: z.string().describe("UHC type e.g. I32, String, U8, F32"),
          name: z.string().describe("Field name"),
          defaultValue: z
            .string()
            .optional()
            .describe("Optional default value (e.g. 0, -1, NULL)"),
        })
      )
      .describe("Fields for the self struct"),
    methods: z
      .array(z.string())
      .optional()
      .describe(
        "Additional method names to stub (e.g. ['update', 'render', 'serialize'])"
      ),
    withDestroy: z
      .boolean()
      .optional()
      .describe("Include a destroy method (default: false)"),
    withToString: z
      .boolean()
      .optional()
      .describe("Include toString for %T support (default: false)"),
    includeHeaders: z
      .array(z.string())
      .optional()
      .describe(
        "Headers to include (e.g. ['uhcio.uhh', 'uhcstd.uhh']). Defaults to uhcstd.uhh."
      ),
  },
  async ({
    name,
    fields,
    methods = [],
    withDestroy = false,
    withToString = false,
    includeHeaders = ["uhcstd.uhh"],
  }) => {
    const includes = includeHeaders.map((h) => `#include <${h}>`).join("\n");

    // Build self struct
    const selfFields = fields
      .map((f) => {
        const def = f.defaultValue !== undefined ? ` = ${f.defaultValue}` : "";
        return `        ${f.type} ${f.name}${def}`;
      })
      .join("\n");

    // Build create params
    const createParams = fields
      .filter((f) => !f.defaultValue)
      .map((f) => `${f.type} ${f.name}`)
      .join(", ");

    // Header declarations
    const extraDecls = methods.map((m) => `    U0 ${m}(${name} obj)`).join("\n");
    const destroyDecl = withDestroy ? `    U0 destroy(${name} obj)\n` : "";
    const toStringDecl = withToString
      ? `    const I8* toString(const ${name} obj)  ///< enables %T\n`
      : "";

    const header = `#pragma once
${includes}

namespace ${name} {
    self {
${selfFields}
    }

    /// Create and initialize a new ${name}
    ${name} create(${createParams})
${destroyDecl}${toStringDecl}${extraDecls}
}
`;

    // Implementation stubs
    const createBody = fields
      .filter((f) => !f.defaultValue)
      .map((f) => `        obj.${f.name} = ${f.name}`)
      .join("\n");

    const extraImpls = methods
      .map(
        (m) => `
    U0 ${m}(${name} obj) {
        // TODO
    }`
      )
      .join("\n");

    const destroyImpl = withDestroy
      ? `
    U0 destroy(${name} obj) {
        // TODO: release resources
    }
`
      : "";

    const toStringImpl = withToString
      ? `
    const I8* toString(const ${name} obj) {
        return String.c(String.format("[${name}]"))  // TODO: fill in fields
    }
`
      : "";

    const impl = `#include "${name.toLowerCase()}.uhh"

namespace ${name} {

    ${name} create(${createParams}) {
        ${name} obj
${createBody}
        return obj
    }
${destroyImpl}${toStringImpl}${extraImpls}
}
`;

    const text = `## ${name}.uhh
\`\`\`uhc
${header}\`\`\`

## ${name.toLowerCase()}.uhc
\`\`\`uhc
${impl}\`\`\``;

    return { content: [{ type: "text", text }] };
  }
);

// ─── Helper: extract sections ────────────────────────────────────────────────
function extractSection(doc: string, headings: string[]): string {
  const lines = doc.split("\n");
  const results: string[] = [];

  for (const heading of headings) {
    let inSection = false;
    let sectionLines: string[] = [];

    for (let i = 0; i < lines.length; i++) {
      if (lines[i].startsWith(heading)) {
        inSection = true;
        sectionLines = [lines[i]];
        continue;
      }
      if (inSection) {
        // Stop at next same-level or higher heading
        if (lines[i].match(/^---$/) || (lines[i].startsWith("## ") && !lines[i].startsWith(heading))) {
          break;
        }
        sectionLines.push(lines[i]);
      }
    }

    if (sectionLines.length > 0) {
      results.push(sectionLines.join("\n").trim());
    }
  }

  return results.join("\n\n---\n\n") || "Section not found.";
}

// ─── Start server ─────────────────────────────────────────────────────────────
const transport = new StdioServerTransport();
await server.connect(transport);

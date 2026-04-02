#!/usr/bin/env node
'use strict';

const fs   = require('fs');
const path = require('path');

// ---------------------------------------------------------------------------
// Mini-transpiler: UHC → C++
// ---------------------------------------------------------------------------

const KNOWN_NAMESPACES = [
  // stdlib
  'Mutex','Buffer','Thread','DynamicLibrary','List',
  'Log','File',
  'Math','VectorF','VectorI','Matrix',
  'TcpClient','TcpServer',
  // graphics
  'UgWindow',
  'UgContext','UgInstance',
  'UgPhysicalDevice','UgQueueFamilies','UgDevice',
  'UgBuffer','UgFrameBuffer','UgUniformBuffer','UgCommandBuffer',
  'UgCommandPool','UgDescriptorPool',
  'UgFence','UgSemaphore',
  'UgImageView','UgImageLayout',
  'UgWindowSurface','UgSwapchain',
  'UgShader','UgShaderProgram',
  'UgPipeline','UgPipelineLayout',
  'UgRenderPass','UgRenderTarget','UgRenderer',
  'UgMaterialPipelineEntry','UgMaterial',
  'UgTexture','UgSampler','UgTextureArray',
  'UgVertexAttrib','UgVertexDesc','UgVertexSettings',
  'UgUtils','UgLogCallback','UgValidationLayer',
];

const TYPE_MAP = [
  ['U0',   'void'],
  ['U8',   'unsigned char'],
  ['U16',  'unsigned short'],
  ['U32',  'unsigned int'],
  ['U64',  'unsigned long long'],
  ['I8',   'char'],
  ['I16',  'short'],
  ['I32',  'int'],
  ['I64',  'long long'],
  ['F32',  'float'],
  ['F64',  'double'],
  ['F128', 'long double'],
];

// Pre-compile regexes for namespace dot→scope conversion
const NS_DOT_RE = new RegExp(
  `\\b(${KNOWN_NAMESPACES.join('|')})\\.([A-Za-z_][A-Za-z0-9_]*)`,
  'g'
);

function transpile(code) {
  // 1. Namespace.Member → Namespace::Member
  let out = code.replace(NS_DOT_RE, '$1::$2');

  // 2. UHC types → C++ types (longest first to avoid partial matches)
  for (const [uhc, cpp] of TYPE_MAP) {
    out = out.replace(new RegExp(`\\b${uhc}\\b`, 'g'), cpp);
  }

  return out;
}

// ---------------------------------------------------------------------------
// Doc comment parsing
// ---------------------------------------------------------------------------

function parseDocs(lines) {
  const doc = { brief: '', params: new Map(), returns: null };
  const textLines = [];

  for (const line of lines) {
    const trimmed = line.replace(/^\/\/\/\s?/, '');
    if (trimmed.startsWith('@param ')) {
      const rest = trimmed.slice('@param '.length);
      const spaceIdx = rest.indexOf(' ');
      if (spaceIdx !== -1) {
        doc.params.set(rest.slice(0, spaceIdx), rest.slice(spaceIdx + 1));
      }
    } else if (trimmed.startsWith('@returns ')) {
      doc.returns = trimmed.slice('@returns '.length);
    } else {
      textLines.push(trimmed);
    }
  }

  doc.brief = textLines.filter(Boolean).join(' ');
  return doc;
}

// ---------------------------------------------------------------------------
// Header parser
// ---------------------------------------------------------------------------

function parseHeader(source) {
  const lines = source.split('\n');
  const namespaces = [];

  let i = 0;
  let pendingDocLines = [];

  // Items collected at file scope (defines between namespaces)
  let fileScopeItems = [];

  function consumeDoc() {
    const d = pendingDocLines.length ? parseDocs(pendingDocLines) : { brief: '', params: new Map(), returns: null };
    pendingDocLines = [];
    return d;
  }

  // Skip body of a block starting at lines[startI] where we already found '{'
  // Returns the index of the line after the closing '}'
  function skipBody(startI) {
    let depth = 0;
    for (let j = startI; j < lines.length; j++) {
      for (const ch of lines[j]) {
        if (ch === '{') depth++;
        else if (ch === '}') { depth--; if (depth === 0) return j + 1; }
      }
    }
    return lines.length;
  }

  // Collect all lines of a block from the opening '{' up to and including the
  // matching '}'. Returns { rawLines, nextI }.
  function collectBlock(startI) {
    const rawLines = [];
    let depth = 0;
    let j = startI;
    for (; j < lines.length; j++) {
      rawLines.push(lines[j]);
      for (const ch of lines[j]) {
        if (ch === '{') depth++;
        else if (ch === '}') { depth--; if (depth === 0) return { rawLines, nextI: j + 1 }; }
      }
    }
    return { rawLines, nextI: j };
  }

  // Collect the platform-guard struct block starting at lines[i] which is '#if defined('
  // Returns { item: ParsedItem, nextI }
  function collectPlatformGuard(startI, doc) {
    const rawLines = [];
    let j = startI;
    // collect until the #endif at depth 0 of #if nesting
    let depth = 0;
    for (; j < lines.length; j++) {
      const t = lines[j].trim();
      rawLines.push(lines[j]);
      if (t.startsWith('#if')) depth++;
      else if (t === '#endif') { depth--; if (depth === 0) { j++; break; } }
    }
    return {
      item: { kind: 'struct', platform: true, rawUhc: rawLines.join('\n'), doc },
      nextI: j,
    };
  }

  // Parse one namespace starting at lines[i] (the 'namespace X {' line)
  // Returns { ns: ParsedNamespace, nextI }
  function parseNamespace(startI, nsDoc) {
    const headerLine = lines[startI].trim();
    const nameMatch = headerLine.match(/^namespace\s+(\w+)/);
    if (!nameMatch) return null;
    const nsName = nameMatch[1];
    const ns = { name: nsName, doc: nsDoc, items: [] };

    let j = startI + 1;
    let localDocLines = [];

    while (j < lines.length) {
      const raw = lines[j];
      const t = raw.trim();

      if (t === '') { j++; continue; }

      // Closing namespace brace
      if (t === '}') { j++; break; }

      // Doc comment
      if (t.startsWith('///')) {
        localDocLines.push(t);
        j++;
        continue;
      }

      const itemDoc = localDocLines.length ? parseDocs(localDocLines) : { brief: '', params: new Map(), returns: null };

      // Platform guard struct
      if (t.startsWith('#if defined(')) {
        localDocLines = [];
        const { item, nextI } = collectPlatformGuard(j, itemDoc);
        ns.items.push(item);
        j = nextI;
        continue;
      }

      // Plain struct (non-platform)
      if (t.startsWith('struct ')) {
        localDocLines = [];
        // Collect fields until '};'
        const fieldLines = [];
        j++;
        while (j < lines.length) {
          const ft = lines[j].trim();
          if (ft === '};' || ft === '}') { j++; break; }
          if (ft !== '') {
            // Strip default value: "F32 x = 0.0f;" → "F32 x;"
            const stripped = ft.replace(/\s*=\s*[^;]+;/, ';');
            fieldLines.push(stripped);
          }
          j++;
        }
        ns.items.push({ kind: 'struct', platform: false, fieldLines, doc: itemDoc });
        continue;
      }

      // typedef
      if (t.startsWith('typedef ')) {
        localDocLines = [];
        // Extract name: typedef ... (*Name)(...); or typedef ... Name;
        let tdName = '';
        const fpMatch = t.match(/\(\*(\w+)\)/);
        if (fpMatch) tdName = fpMatch[1];
        else {
          const parts = t.replace(';', '').trim().split(/\s+/);
          tdName = parts[parts.length - 1];
        }
        ns.items.push({ kind: 'typedef', raw: t, name: tdName, doc: itemDoc });
        j++;
        continue;
      }

      // enum
      if (t.startsWith('enum ')) {
        localDocLines = [];
        const enumName = t.match(/^enum\s+(\w+)/)[1];
        const values = [];
        j++;
        while (j < lines.length) {
          const et = lines[j].trim();
          if (et === '};' || et === '}') { j++; break; }
          if (et !== '' && !et.startsWith('//')) {
            values.push(et.replace(/,$/, '').trim());
          }
          j++;
        }
        ns.items.push({ kind: 'enum', name: enumName, values, doc: itemDoc });
        continue;
      }

      // template struct or template function
      if (t.startsWith('template<')) {
        localDocLines = [];
        const templatePrefix = t; // e.g. "template<typename T>"
        j++;
        // Next non-empty line is the struct/function signature
        while (j < lines.length && lines[j].trim() === '') j++;
        const sigLine = lines[j].trim();
        j++;

        // Template struct
        if (sigLine.startsWith('struct ')) {
          // Collect fields until '};'
          const fieldLines = [];
          while (j < lines.length) {
            const ft = lines[j].trim();
            if (ft === '};' || ft === '}') { j++; break; }
            if (ft !== '') {
              const stripped = ft.replace(/\s*=\s*[^;]+;/, ';');
              fieldLines.push(stripped);
            }
            j++;
          }
          ns.items.push({ kind: 'struct', platform: false, fieldLines, templatePrefix, doc: itemDoc });
          continue;
        }

        // Template function
        const fnName = extractFunctionName(sigLine);
        let bodyStart = j - 1;
        let sigFull = sigLine;
        if (!sigLine.includes('{')) {
          while (j < lines.length && !lines[j].includes('{')) {
            sigFull += ' ' + lines[j].trim();
            j++;
          }
          bodyStart = j;
        }
        j = skipBody(bodyStart);
        const cleanSig = sigFull.replace(/\s*\{.*$/, '').trim();
        const rawSig = `${templatePrefix}\n${cleanSig};`;
        ns.items.push({ kind: 'template_function', name: fnName, raw: rawSig, doc: itemDoc });
        continue;
      }

      // Function declaration (line ends with ');')
      if (isFunctionDecl(t)) {
        localDocLines = [];
        const fnName = extractFunctionName(t);
        ns.items.push({ kind: 'function', name: fnName, raw: t, doc: itemDoc });
        j++;
        continue;
      }

      // Anything else — skip, clear doc
      localDocLines = [];
      j++;
    }

    return { ns, nextI: j };
  }

  // Main parse loop (file scope)
  while (i < lines.length) {
    const raw = lines[i];
    const t = raw.trim();

    if (t === '') { i++; continue; }

    // Doc comment
    if (t.startsWith('///')) {
      pendingDocLines.push(t);
      i++;
      continue;
    }

    // Skip pragmas and includes
    if (t.startsWith('#pragma') || t.startsWith('#include')) {
      pendingDocLines = [];
      i++;
      continue;
    }

    // Platform guard at file scope (skip — these are system includes)
    if (t.startsWith('#if') && !t.startsWith('#ifdef') && !t.startsWith('#ifndef')) {
      // Check if this guard wraps includes (not a namespace or define)
      // Heuristic: consume the whole block but check if content is all #include lines
      const guardLines = [];
      let depth = 0;
      let k = i;
      for (; k < lines.length; k++) {
        const gt = lines[k].trim();
        guardLines.push(gt);
        if (gt.startsWith('#if')) depth++;
        else if (gt === '#endif') { depth--; if (depth === 0) { k++; break; } }
      }
      // If content contains only includes/empty lines/else, skip it
      const inner = guardLines.slice(1, -1).filter(l => l !== '' && l !== '#else');
      const allIncludes = inner.every(l => l.startsWith('#include'));
      if (allIncludes) {
        pendingDocLines = [];
        i = k;
        continue;
      }
      // Otherwise treat as file-scope content (unlikely for uhcstd headers)
      pendingDocLines = [];
      i = k;
      continue;
    }

    // #define at file scope
    if (t.startsWith('#define ')) {
      const doc = consumeDoc();
      const parts = t.slice('#define '.length).split(/\s+/);
      const name = parts[0];
      const value = parts.slice(1).join(' ');
      fileScopeItems.push({ kind: 'define', name, value, doc, afterNs: namespaces.length });
      i++;
      continue;
    }

    // namespace
    if (t.startsWith('namespace ')) {
      const nsDoc = consumeDoc();
      const result = parseNamespace(i, nsDoc);
      if (result) {
        result.ns._fileScopeDefinesAfter = fileScopeItems.filter(d => d.afterNs === namespaces.length);
        namespaces.push(result.ns);
        i = result.nextI;
      } else {
        i++;
      }
      continue;
    }

    pendingDocLines = [];
    i++;
  }

  // Attach any remaining file-scope defines to a virtual "globals" bucket
  // Also store all file-scope items on the result for the section index builder
  const result = { namespaces, fileScopeItems };
  return result;
}

function isFunctionDecl(line) {
  // Must have '(' and end with ');'  and not start with keywords that aren't functions
  if (!line.includes('(')) return false;
  if (!line.endsWith(');')) return false;
  if (line.startsWith('//') || line.startsWith('#') || line.startsWith('*')) return false;
  return true;
}

function extractFunctionName(line) {
  // Matches: "ReturnType funcName(" or "ReturnType* funcName(" etc.
  // Also handles "const ReturnType* funcName("
  const m = line.match(/\b([A-Za-z_][A-Za-z0-9_]*)\s*\(/);
  if (!m) return 'unknown';
  // Skip keywords and types
  const skip = new Set([
    ...KNOWN_NAMESPACES,
    'void','unsigned','char','short','int','long','float','double',
    'const','struct','enum','typedef','template','auto',
    'U0','U8','U16','U32','U64','I8','I16','I32','I64','F32','F64','F128',
    'FILE','SOCKET','HANDLE','HINSTANCE',
  ]);
  // Find all candidates and pick the first non-skipped one
  const all = [...line.matchAll(/\b([A-Za-z_][A-Za-z0-9_]*)\s*\(/g)];
  for (const match of all) {
    if (!skip.has(match[1])) return match[1];
  }
  return m[1];
}

// ---------------------------------------------------------------------------
// MDX rendering helpers
// ---------------------------------------------------------------------------

function escapeBackticks(s) {
  // MDX template strings use backticks; escape any literal backtick in code
  return s.replace(/`/g, '\\`');
}

function indent(text, spaces) {
  const pad = ' '.repeat(spaces);
  return text.split('\n').map(l => (l.trim() === '' ? '' : pad + l)).join('\n');
}

function renderParams(doc) {
  if (!doc.params.size) return '';
  const lines = ['**Parameters**'];
  for (const [name, desc] of doc.params) {
    lines.push(`- \`${name}\` — ${desc}`);
  }
  return lines.join('\n') + '\n';
}

function renderReturns(doc) {
  if (!doc.returns) return '';
  return `**Returns** — ${doc.returns}\n`;
}

function codetabs(uhcCode, cppCode) {
  const u = escapeBackticks(uhcCode);
  const c = escapeBackticks(cppCode);
  // Single-line vs multi-line
  const uMulti = u.includes('\n');
  const cMulti = c.includes('\n');

  if (!uMulti && !cMulti) {
    return `<CodeTabs\n  uhc={\`${u}\`}\n  cpp={\`${c}\`}\n/>`;
  }

  const uBlock = uMulti ? `\`${u}\n  \`` : `\`${u}\``;
  const cBlock = cMulti ? `\`${c}\n  \`` : `\`${c}\``;
  return `<CodeTabs\n  uhc={${uBlock}}\n  cpp={${cBlock}}\n/>`;
}

// ---------------------------------------------------------------------------
// Namespace page renderer
// ---------------------------------------------------------------------------

function renderItem(item) {
  const { doc } = item;
  const brief = doc.brief || '';

  switch (item.kind) {
    case 'define': {
      const uhcLine = `#define ${item.name} ${item.value}`;
      return [
        `<H2 id="${item.name}">${item.name}</H2>`,
        '',
        brief,
        '',
        codetabs(uhcLine, uhcLine),
      ].join('\n');
    }

    case 'typedef': {
      return [
        `<H2 id="${item.name}">${item.name}</H2>`,
        '',
        brief,
        '',
        codetabs(item.raw, transpile(item.raw)),
      ].join('\n');
    }

    case 'enum': {
      const body = `enum ${item.name} {\n${item.values.map(v => `    ${v},`).join('\n')}\n};`;
      return [
        `<H2 id="${item.name}">${item.name}</H2>`,
        '',
        brief,
        '',
        codetabs(body, body),
      ].join('\n');
    }

    case 'struct': {
      if (item.platform) {
        const rawUhc = formatPlatformStruct(item.rawUhc);
        const rawCpp = transpile(rawUhc);
        return [
          `<H2 id="It">It</H2>`,
          '',
          brief,
          '',
          codetabs(rawUhc, rawCpp),
        ].join('\n');
      } else {
        // template<typename T> prefix is omitted — just show the struct definition
        const body = `struct It {\n${item.fieldLines.map(f => `    ${f}`).join('\n')}\n};`;
        return [
          `<H2 id="It">It</H2>`,
          '',
          brief,
          '',
          codetabs(body, transpile(body)),
        ].join('\n');
      }
    }

    case 'function':
    case 'template_function': {
      // For template functions: strip the template<...> line, show only the signature
      const rawSig = item.kind === 'template_function'
        ? item.raw.split('\n').slice(1).join('\n')
        : item.raw;
      const parts = [
        `<H2 id="${item.name}">${item.name}</H2>`,
        '',
        brief,
        '',
        codetabs(rawSig, transpile(rawSig)),
      ];
      const paramsStr = renderParams(doc);
      const returnsStr = renderReturns(doc);
      if (paramsStr) { parts.push(''); parts.push(paramsStr.trimEnd()); }
      if (returnsStr) { parts.push(''); parts.push(returnsStr.trimEnd()); }
      return parts.join('\n');
    }

    default:
      return '';
  }
}

// Format a raw platform-guard block for display.
// Normalizes header indentation (4-space tabs) to 2-space levels so that:
//   #if / #else / #endif → col 0
//   struct It {          → 2 spaces
//   fields               → 4 spaces
function formatPlatformStruct(raw) {
  return raw
    .split('\n')
    .filter(l => !l.trim().startsWith('///'))
    .map(l => {
      if (l.trim() === '') return '';
      const spaces = l.match(/^(\s*)/)[1].length;
      return ' '.repeat(Math.round(spaces / 2)) + l.trimStart();
    })
    .join('\n')
    .trim();
}

function renderNamespacePage(ns) {
  const parts = [`# ${ns.name}`, '', ns.doc.brief || '', ''];

  // Filter out 'struct' items with no fields and no doc for platform guards
  const items = ns.items;

  for (let idx = 0; idx < items.length; idx++) {
    const rendered = renderItem(items[idx]);
    if (rendered) {
      parts.push(rendered);
      if (idx < items.length - 1) {
        parts.push('');
        parts.push('---');
        parts.push('');
      }
    }
  }

  parts.push('');
  return parts.join('\n');
}

// ---------------------------------------------------------------------------
// Section index page + cheat sheet renderer
// ---------------------------------------------------------------------------

function stripDefaultValues(line) {
  return line.replace(/\s*=\s*[^;{,]+(?=[;,{])/g, '');
}

function buildCheatSheetNs(ns, isUhc) {
  const lines = [];
  lines.push(`namespace ${ns.name} {`);

  for (const item of ns.items) {
    switch (item.kind) {
      case 'define':
        lines.push(`  #define ${item.name} ${item.value}`);
        break;
      case 'typedef': {
        const raw = isUhc ? item.raw : transpile(item.raw);
        lines.push(`  ${raw}`);
        break;
      }
      case 'enum': {
        lines.push(`  enum ${item.name} {`);
        for (const v of item.values) lines.push(`    ${v},`);
        lines.push(`  };`);
        break;
      }
      case 'struct': {
        if (item.platform) {
          const rawBlock = formatPlatformStruct(item.rawUhc);
          const block = isUhc ? rawBlock : transpile(rawBlock);
          for (const l of block.split('\n')) lines.push(`  ${l}`);
        } else {
          lines.push(`  struct It {`);
          for (const f of item.fieldLines) {
            const stripped = stripDefaultValues(f);
            const out = isUhc ? stripped : transpile(stripped);
            lines.push(`    ${out}`);
          }
          lines.push(`  };`);
        }
        break;
      }
      case 'function': {
        const sig = isUhc ? item.raw : transpile(item.raw);
        lines.push(`  ${sig}`);
        break;
      }
      case 'template_function': {
        // In cheat sheet: no template<typename T> prefix, just the signature line
        const rawLines = item.raw.split('\n');
        // rawLines[0] = template<typename T>, rawLines[1] = signature;
        const sig = rawLines.length > 1 ? rawLines[1] : rawLines[0];
        const out = isUhc ? sig : transpile(sig);
        lines.push(`  ${out}`);
        break;
      }
    }
  }

  lines.push('}');
  return lines.join('\n');
}

function renderSectionIndexPage(section, parsedSections) {
  const { namespaces: nsConfigs } = section;
  const { namespaces, fileScopeItems } = parsedSections;

  // Build a lookup of parsed namespace by name
  const nsMap = new Map(namespaces.map(ns => [ns.name, ns]));

  // Kirk 2: use parsed doc brief from header as namespace description
  const basePath = section.basePath || '/stdlib';
  const links = nsConfigs
    .map(nsConfig => {
      const ns = nsMap.get(nsConfig.name);
      const desc = (ns && ns.doc.brief) ? ns.doc.brief : '';
      return `- [${nsConfig.name}](${basePath}/${section.id}/${nsConfig.slug}) — ${desc}`;
    })
    .join('\n');

  // Build cheat sheet in parse order (source file order)
  // Kirk 1: use 4-space indent for namespace content
  function buildCheatSheet(isUhc) {
    const parts = [];

    // File-scope defines (UHC_PI, UHC_LITTLE_ENDIAN, etc.)
    // M00-M33 are injected into the Matrix namespace block below
    const matrixDefineNames = new Set([
      'M00','M01','M02','M03','M10','M11','M12','M13',
      'M20','M21','M22','M23','M30','M31','M32','M33',
    ]);

    const topDefines = fileScopeItems.filter(d => !matrixDefineNames.has(d.name));
    const matrixDefines = fileScopeItems.filter(d => matrixDefineNames.has(d.name));

    for (const d of topDefines) {
      parts.push(`#define ${d.name} ${d.value}`);
    }

    if (parts.length > 0) parts.push('');

    for (const ns of namespaces) {
      const nsLines = [];
      nsLines.push(`namespace ${ns.name} {`);

      // Inject M-defines inside Matrix namespace
      if (ns.name === 'Matrix' && matrixDefines.length > 0) {
        for (const d of matrixDefines) {
          nsLines.push(`    #define ${d.name} ${d.value}`);
        }
      }

      for (const item of ns.items) {
        switch (item.kind) {
          case 'define':
            nsLines.push(`    #define ${item.name} ${item.value}`);
            break;
          case 'typedef': {
            const raw = isUhc ? item.raw : transpile(item.raw);
            nsLines.push(`    ${raw}`);
            break;
          }
          case 'enum': {
            nsLines.push(`    enum ${item.name} {`);
            for (const v of item.values) nsLines.push(`        ${v},`);
            nsLines.push(`    };`);
            break;
          }
          case 'struct': {
            if (item.platform) {
              // Platform guard: prefix each guard line with 4 spaces,
              // its content (struct body) already has 2-space indent from formatPlatformStruct
              // so we add 4 spaces and it becomes 4+2=6 for struct, 4+4=8 for fields
              const rawBlock = formatPlatformStruct(item.rawUhc);
              const block = isUhc ? rawBlock : transpile(rawBlock);
              for (const l of block.split('\n')) {
                nsLines.push(l.trim() === '' ? '' : `    ${l}`);
              }
            } else {
              nsLines.push(`    struct It {`);
              for (const f of item.fieldLines) {
                const stripped = stripDefaultValues(f);
                const out = isUhc ? stripped : transpile(stripped);
                nsLines.push(`        ${out}`);
              }
              nsLines.push(`    };`);
            }
            break;
          }
          case 'function': {
            const sig = isUhc ? item.raw : transpile(item.raw);
            nsLines.push(`    ${sig}`);
            break;
          }
          case 'template_function': {
            const rawLines = item.raw.split('\n');
            const sig = rawLines.length > 1 ? rawLines[1] : rawLines[0];
            const out = isUhc ? sig : transpile(sig);
            nsLines.push(`    ${out}`);
            break;
          }
        }
      }

      nsLines.push('}');
      parts.push(nsLines.join('\n'));
      parts.push('');
    }

    return parts.join('\n').trimEnd();
  }

  const cheatUhc = buildCheatSheet(true);
  const cheatCpp = buildCheatSheet(false);

  const uhcInclude = section.header.uhc;
  const cppInclude = section.header.cpp;

  return [
    `# ${section.title}`,
    '',
    section.description,
    '',
    links,
    '',
    `<H2 id="Header">Header</H2>`,
    '',
    codetabs(uhcInclude, cppInclude),
    '',
    `<H2 id="Header">Cheat sheet</H2>`,
    '',
    codetabs(cheatUhc, cheatCpp),
    '',
  ].join('\n');
}

// ---------------------------------------------------------------------------
// Nav file renderer
// ---------------------------------------------------------------------------

function itemSlug(item) {
  if (item.kind === 'struct') return 'It';
  return item.name;
}

function renderNavFile(config, allParsed) {
  const exportName = config.navExport;
  const lines = [`export const ${exportName} = [`];

  // Index entry (always first)
  lines.push(`  {`);
  lines.push(`    label: "Index",`);
  lines.push(`    slug: "",`);
  lines.push(`    namespaces: []`);
  lines.push(`  },`);

  for (let si = 0; si < config.sections.length; si++) {
    const section = config.sections[si];
    const parsed = allParsed[si];
    const nsMap = new Map(parsed.namespaces.map(ns => [ns.name, ns]));

    lines.push(`  {`);
    lines.push(`    label: "${section.title}",`);
    lines.push(`    slug: "${section.id}",`);
    lines.push(`    namespaces: [`);

    for (const nsConfig of section.namespaces) {
      const ns = nsMap.get(nsConfig.name);
      if (!ns) continue;

      lines.push(`      {`);
      lines.push(`        label: "${nsConfig.name}",`);
      lines.push(`        slug: "${nsConfig.slug}",`);
      lines.push(`        methods: [`);

      for (const item of ns.items) {
        const slug = itemSlug(item);
        lines.push(`          { label: "${slug}", slug: "${slug}" },`);
      }

      lines.push(`        ],`);
      lines.push(`      },`);
    }

    lines.push(`    ],`);
    const isLast = si === config.sections.length - 1;
    lines.push(isLast ? `  }` : `  },`);
  }

  lines.push(`]`);
  lines.push('');
  return lines.join('\n');
}

// ---------------------------------------------------------------------------
// Main
// ---------------------------------------------------------------------------

function main() {
  const configArg = process.argv[2];
  const configPath = configArg
    ? path.resolve(process.cwd(), configArg)
    : path.resolve(__dirname, 'config.json');
  const config = JSON.parse(fs.readFileSync(configPath, 'utf8'));
  const outputBase = path.resolve(__dirname, config.outputBase);
  const projectRoot = path.resolve(__dirname, '..');

  const allParsed = [];

  for (const section of config.sections) {
    // Support both sourceFile (single) and sourceFiles (array)
    const sourceFiles = section.sourceFiles
      ? section.sourceFiles
      : [section.sourceFile];

    console.log(`\nProcessing section "${section.id}" (${sourceFiles.length} file(s))`);

    // Merge namespaces and fileScopeItems from all source files
    const parsed = { namespaces: [], fileScopeItems: [] };
    for (const sf of sourceFiles) {
      const filePath = path.resolve(projectRoot, sf);
      const source = fs.readFileSync(filePath, 'utf8');
      const fileParsed = parseHeader(source);
      parsed.namespaces.push(...fileParsed.namespaces);
      parsed.fileScopeItems.push(...fileParsed.fileScopeItems);
    }

    allParsed.push(parsed);

    // Build a map of namespace name → ParsedNamespace for lookup
    const nsMap = new Map(parsed.namespaces.map(ns => [ns.name, ns]));

    // --- Write individual namespace pages ---
    for (const nsConfig of section.namespaces) {
      const ns = nsMap.get(nsConfig.name);
      if (!ns) {
        console.warn(`  WARNING: namespace "${nsConfig.name}" not found in ${section.sourceFile}`);
        continue;
      }

      const mdx = renderNamespacePage(ns);
      const outDir = path.join(outputBase, section.id, nsConfig.slug);
      fs.mkdirSync(outDir, { recursive: true });
      const outFile = path.join(outDir, 'page.mdx');
      fs.writeFileSync(outFile, mdx, 'utf8');
      console.log(`  Wrote ${path.relative(projectRoot, outFile)}`);
    }

    // --- Write section index page ---
    const indexMdx = renderSectionIndexPage(section, parsed);
    const indexDir = path.join(outputBase, section.id);
    fs.mkdirSync(indexDir, { recursive: true });
    const indexFile = path.join(indexDir, 'page.mdx');
    fs.writeFileSync(indexFile, indexMdx, 'utf8');
    console.log(`  Wrote ${path.relative(projectRoot, indexFile)}`);
  }

  // --- Write nav file ---
  if (config.navFile) {
    const navContent = renderNavFile(config, allParsed);
    const navFile = path.resolve(__dirname, config.navFile);
    fs.writeFileSync(navFile, navContent, 'utf8');
    console.log(`\nWrote nav: ${path.relative(projectRoot, navFile)}`);
  }

  console.log('\nDone.');
}

main();

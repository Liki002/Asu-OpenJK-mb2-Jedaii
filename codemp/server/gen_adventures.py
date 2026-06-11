#!/usr/bin/env python3
"""Generate sv_ranked_adventure.h from adventures.js."""
import re
import sys
import os

JS_PATH = r"c:\Users\AlphaOmega\Desktop\jedaii servers\JedaiiRankedDuel\RankedScript09.12.25\adventures.js"
OUT_PATH = os.path.join(os.path.dirname(os.path.abspath(__file__)), "sv_ranked_adventure.h")

with open(JS_PATH, "r", encoding="utf-8") as f:
    src = f.read()

# --- Extract all node definitions: id: { description: "...", choices?: [...], outcome?: {...} }
# Match id followed by : { ... } as a top-level entry.
# Since regex can't reliably balance braces, we'll scan manually.

def extract_nodes(text):
    # Find start of adventureNodes object
    m = re.search(r"const\s+adventureNodes\s*=\s*\{", text)
    if not m:
        raise SystemExit("adventureNodes not found")
    start = m.end()
    # find matching closing brace at top level
    depth = 1
    i = start
    while i < len(text) and depth > 0:
        c = text[i]
        if c == '{':
            depth += 1
        elif c == '}':
            depth -= 1
        elif c == '"' or c == "'":
            # skip string
            quote = c
            i += 1
            while i < len(text) and text[i] != quote:
                if text[i] == '\\':
                    i += 2
                    continue
                i += 1
        i += 1
    body = text[start:i-1]
    return body

def parse_nodes(body):
    """Return list of (node_id, dict-like fields raw text)"""
    nodes = []
    i = 0
    n = len(body)
    while i < n:
        # skip whitespace + commas + line comments
        while i < n and (body[i].isspace() or body[i] == ','):
            i += 1
        if i >= n: break
        if body[i:i+2] == '//':
            # line comment
            j = body.find('\n', i)
            if j < 0: break
            i = j + 1
            continue
        # node id: word until ':'
        m = re.match(r'([A-Za-z_][A-Za-z0-9_]*)\s*:\s*\{', body[i:])
        if not m:
            i += 1
            continue
        node_id = m.group(1)
        i += m.end()
        # find matching closing brace
        depth = 1
        start = i
        while i < n and depth > 0:
            c = body[i]
            if c == '{':
                depth += 1
            elif c == '}':
                depth -= 1
            elif c == '"' or c == "'":
                quote = c
                i += 1
                while i < n and body[i] != quote:
                    if body[i] == '\\':
                        i += 2
                        continue
                    i += 1
            i += 1
        node_body = body[start:i-1]
        nodes.append((node_id, node_body))
    return nodes

def parse_string(text, key):
    """Find `key: "value"` and return value string (handling escapes)."""
    m = re.search(r'\b' + key + r'\s*:\s*"((?:[^"\\]|\\.)*)"', text)
    if not m:
        return None
    return m.group(1)

def parse_choices(text):
    """Parse choices: [ {text: "...", destination: "..."}, ... ]"""
    m = re.search(r'choices\s*:\s*\[', text)
    if not m:
        return None
    start = m.end()
    depth = 1
    i = start
    while i < len(text) and depth > 0:
        c = text[i]
        if c == '[':
            depth += 1
        elif c == ']':
            depth -= 1
        elif c == '"' or c == "'":
            quote = c
            i += 1
            while i < len(text) and text[i] != quote:
                if text[i] == '\\':
                    i += 2
                    continue
                i += 1
        i += 1
    body = text[start:i-1]
    # find each {text:..., destination:...}
    choices = []
    for cm in re.finditer(r'\{\s*text\s*:\s*"((?:[^"\\]|\\.)*)"\s*,\s*destination\s*:\s*"([^"]+)"\s*\}', body):
        choices.append((cm.group(1), cm.group(2)))
    return choices

def parse_outcome(text):
    """Parse outcome: { credits: N, xp: N }"""
    m = re.search(r'outcome\s*:\s*\{\s*credits\s*:\s*(-?\d+)\s*,\s*xp\s*:\s*(-?\d+)\s*\}', text)
    if not m:
        return None
    return (int(m.group(1)), int(m.group(2)))

def parse_starting(text):
    m = re.search(r"const\s+startingAdventures\s*=\s*\[([^\]]*)\]", text)
    if not m:
        return []
    body = m.group(1)
    return [s.group(1) for s in re.finditer(r"'([^']+)'", body)]

# --- Run
body = extract_nodes(src)
nodes = parse_nodes(body)
starting = parse_starting(src)

# Build id -> index map
id_to_idx = {nid: idx for idx, (nid, _) in enumerate(nodes)}

# Validate: every choice destination + starting node must exist
for nid, raw in nodes:
    choices = parse_choices(raw) or []
    for txt, dest in choices:
        if dest not in id_to_idx:
            print(f"WARN: choice destination '{dest}' (from '{nid}') not found", file=sys.stderr)

for s in starting:
    if s not in id_to_idx:
        print(f"WARN: starting adventure '{s}' not found", file=sys.stderr)

# Generate header
def c_escape(s):
    return s.replace('\\', '\\\\').replace('"', '\\"')

lines = []
lines.append("#ifndef SV_RANKED_ADVENTURE_H")
lines.append("#define SV_RANKED_ADVENTURE_H")
lines.append("")
lines.append("// Auto-generated from adventures.js by gen_adventures.py — DO NOT EDIT BY HAND")
lines.append("")
lines.append("#define ADV_MAX_CHOICES 4")
lines.append("")
lines.append("typedef struct {")
lines.append("    const char *text;")
lines.append("    int destIndex;          // index into sv_rankedAdventureNodes[], -1 if none")
lines.append("} rankedAdvChoice_t;")
lines.append("")
lines.append("typedef struct {")
lines.append("    const char *id;")
lines.append("    const char *description;")
lines.append("    int numChoices;")
lines.append("    rankedAdvChoice_t choices[ADV_MAX_CHOICES];")
lines.append("    int hasOutcome;")
lines.append("    int outcomeCredits;")
lines.append("    int outcomeXp;")
lines.append("} rankedAdvNode_t;")
lines.append("")
lines.append(f"static rankedAdvNode_t sv_rankedAdventureNodes[] = {{")

for nid, raw in nodes:
    desc = parse_string(raw, 'description') or ""
    choices = parse_choices(raw) or []
    outcome = parse_outcome(raw)
    lines.append(f"    // [{id_to_idx[nid]}] {nid}")
    lines.append(f"    {{")
    lines.append(f'        "{nid}",')
    lines.append(f'        "{c_escape(desc)}",')
    lines.append(f'        {len(choices)},')
    lines.append(f'        {{')
    for i in range(4):
        if i < len(choices):
            txt, dest = choices[i]
            di = id_to_idx.get(dest, -1)
            lines.append(f'            {{ "{c_escape(txt)}", {di} }},')
        else:
            lines.append(f'            {{ NULL, -1 }},')
    lines.append(f'        }},')
    if outcome:
        lines.append(f'        1, {outcome[0]}, {outcome[1]}')
    else:
        lines.append(f'        0, 0, 0')
    lines.append(f"    }},")

lines.append("};")
lines.append("")
lines.append(f"static const int sv_rankedAdventureNodeCount = sizeof(sv_rankedAdventureNodes) / sizeof(sv_rankedAdventureNodes[0]);")
lines.append("")
lines.append("// Indices of starting nodes (random pick on !adventure)")
lines.append("static const int sv_rankedAdventureStartIndices[] = {")
for s in starting:
    if s in id_to_idx:
        lines.append(f"    {id_to_idx[s]}, // {s}")
lines.append("};")
lines.append("static const int sv_rankedAdventureStartCount = sizeof(sv_rankedAdventureStartIndices) / sizeof(sv_rankedAdventureStartIndices[0]);")
lines.append("")
lines.append("#endif // SV_RANKED_ADVENTURE_H")

with open(OUT_PATH, "w", encoding="utf-8") as f:
    f.write("\n".join(lines))

print(f"Wrote {OUT_PATH}: {len(nodes)} nodes, {len(starting)} starting points")

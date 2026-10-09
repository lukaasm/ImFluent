#!/usr/bin/env python3
"""Call a tool on an MCP server that speaks the Streamable HTTP transport (JSON replies).

Usage:
    python tools/mcp_call.py [--port 7801] <tool> [json-arguments] [--out image.png]
    python tools/mcp_call.py --port 7801 --list

Text content is printed. Image content is written to --out (default: <tool>.png).
Port 7801 is ImFluentGallery, port 7802 is the WinUI 3 Gallery driver (tools/winui_mcp).
"""

import argparse
import base64
import json
import sys
import urllib.request


def rpc(port, method, params=None, rid=1):
    body = {"jsonrpc": "2.0", "id": rid, "method": method}
    if params is not None:
        body["params"] = params
    req = urllib.request.Request(
        f"http://127.0.0.1:{port}/mcp",
        data=json.dumps(body).encode("utf-8"),
        headers={"Content-Type": "application/json", "Accept": "application/json, text/event-stream"},
        method="POST",
    )
    with urllib.request.urlopen(req, timeout=1900) as resp:
        return json.loads(resp.read().decode("utf-8"))


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--port", type=int, default=7801)
    ap.add_argument("--list", action="store_true", help="list the tools")
    ap.add_argument("--out", default=None, help="file for image content")
    ap.add_argument("tool", nargs="?")
    ap.add_argument("args", nargs="?", default="{}")
    a = ap.parse_args()

    if a.list:
        reply = rpc(a.port, "tools/list")
        for t in reply["result"]["tools"]:
            print(f"{t['name']}: {t['description']}")
        return 0
    if not a.tool:
        ap.error("tool name needed")

    reply = rpc(a.port, "tools/call", {"name": a.tool, "arguments": json.loads(a.args)})
    if "error" in reply:
        print(json.dumps(reply["error"], indent=1))
        return 2
    result = reply["result"]
    image_index = 0
    for c in result.get("content", []):
        if c.get("type") == "text":
            print(c["text"])
        elif c.get("type") == "image":
            out = a.out or f"{a.tool}.png"
            if image_index:
                out = out.replace(".png", f"_{image_index}.png")
            with open(out, "wb") as f:
                f.write(base64.b64decode(c["data"]))
            print(f"[image saved to {out}]")
            image_index += 1
    return 1 if result.get("isError") else 0


if __name__ == "__main__":
    sys.exit(main())

using System.Text.Json;
using System.Text.Json.Nodes;

namespace WinUiMcp;

// JSON-RPC 2.0 / MCP layer. Same methods, tool names, argument names and result shapes as the ImFluent
// server (src/automation/imfluent_automation.cpp), so both galleries can be driven with identical calls.
internal sealed class McpProtocol
{
    const string ServerName = "winui-gallery";
    const string ServerVersion = "0.1.0";
    static readonly string[] SupportedVersions = { "2025-06-18", "2025-03-26", "2024-11-05" };
    static readonly JsonSerializerOptions s_Json = new() { Encoder = System.Text.Encodings.Web.JavaScriptEncoder.UnsafeRelaxedJsonEscaping };

    readonly StaDispatcher _sta;
    readonly Tools _tools;
    readonly JsonArray _definitions = ToolDefinitions();
    readonly object _callLock = new();

    public McpProtocol(StaDispatcher sta, Tools tools)
    {
        _sta = sta;
        _tools = tools;
    }

    public string HandleBody(string body)
    {
        JsonNode? msg;
        try
        {
            msg = JsonNode.Parse(body);
        }
        catch (JsonException)
        {
            return RpcError(null, -32700, "Parse error").ToJsonString(s_Json);
        }
        if (msg is JsonArray batch)
        {
            var output = new JsonArray();
            foreach (var m in batch)
            {
                var r = HandleRpc(m);
                if (r != null)
                    output.Add(r);
            }
            return output.Count == 0 ? "" : output.ToJsonString(s_Json);
        }
        var reply = HandleRpc(msg);
        return reply == null ? "" : reply.ToJsonString(s_Json);
    }

    static JsonObject RpcResult(JsonNode? id, JsonNode result) => new() { ["jsonrpc"] = "2.0", ["id"] = id?.DeepClone(), ["result"] = result };

    static JsonObject RpcError(JsonNode? id, int code, string message) => new()
    {
        ["jsonrpc"] = "2.0",
        ["id"] = id?.DeepClone(),
        ["error"] = new JsonObject { ["code"] = code, ["message"] = message },
    };

    JsonObject? HandleRpc(JsonNode? node)
    {
        if (node is not JsonObject msg)
            return RpcError(null, -32600, "Invalid Request");
        if (msg["method"] is not JsonValue mv || !mv.TryGetValue(out string? method))
            return null; // a response from the client: nothing to answer
        if (!msg.ContainsKey("id"))
            return null; // notification
        var id = msg["id"];
        var prms = msg["params"] as JsonObject ?? new JsonObject();

        switch (method)
        {
            case "initialize":
            {
                string requested = prms["protocolVersion"] is JsonValue pv && pv.TryGetValue(out string? s) ? s! : "";
                string version = SupportedVersions.Contains(requested) ? requested : SupportedVersions[0];
                return RpcResult(id, new JsonObject
                {
                    ["protocolVersion"] = version,
                    ["capabilities"] = new JsonObject { ["tools"] = new JsonObject { ["listChanged"] = false } },
                    ["serverInfo"] = new JsonObject { ["name"] = ServerName, ["version"] = ServerVersion },
                    ["instructions"] = "Drives the real WinUI 3 Gallery through UI Automation, with the same tools as the ImFluent gallery server. "
                        + "Typical loop: goto_page -> screenshot -> list_items -> click/key/type_text -> nav_state/screenshot. "
                        + "Rectangles and mouse coordinates are physical pixels relative to the gallery's client area. "
                        + "Input is real (SendInput): the gallery is brought to the foreground and the mouse cursor moves.",
                });
            }
            case "ping":
                return RpcResult(id, new JsonObject());
            case "tools/list":
                return RpcResult(id, new JsonObject { ["tools"] = _definitions.DeepClone() });
            case "tools/call":
            {
                string name = prms["name"] is JsonValue nv && nv.TryGetValue(out string? n) ? n! : "";
                if (!_definitions.Any(t => (string?)t?["name"] == name))
                    return RpcError(id, -32602, "Unknown tool: " + name);
                var args = prms["arguments"] as JsonObject ?? new JsonObject();
                JsonObject result;
                try
                {
                    lock (_callLock)
                    {
                        var copy = (JsonObject)args.DeepClone();
                        result = _sta.Invoke(() => _tools.Call(name, copy), TimeSpan.FromSeconds(120));
                    }
                }
                catch (TimeoutException)
                {
                    result = Tools.ToolError($"{name}: timed out (is the gallery hung or showing a modal system dialog?)");
                }
                return RpcResult(id, result);
            }
            case "resources/list":
                return RpcResult(id, new JsonObject { ["resources"] = new JsonArray() });
            case "prompts/list":
                return RpcResult(id, new JsonObject { ["prompts"] = new JsonArray() });
            default:
                return RpcError(id, -32601, "Method not found: " + method);
        }
    }

    static JsonObject Prop(string type, string? description = null, JsonArray? enumValues = null, JsonObject? items = null)
    {
        var p = new JsonObject { ["type"] = type };
        if (items != null)
            p["items"] = items;
        if (enumValues != null)
            p["enum"] = enumValues;
        if (description != null)
            p["description"] = description;
        return p;
    }

    static JsonArray ToolDefinitions()
    {
        const string refDesc = "Item reference. A plain label (\"Standard XAML button\") is searched by UIA Name under the current page (then popups and the whole window). "
            + "Use \"Parent/Label\" to search under a parent, or an id from list_items: an AutomationId (\"Combo1\", or \"id:Combo1\") or a runtime id (\"rt:42.1234.5\").";
        const string windowDesc = "Window that references resolve against. Default: the current page content. "
            + "Use \"$FOCUSED\" for the focused popup (open flyout, menu or combo box list), \"$MAIN\" for the whole window, or a name from list_windows.";
        const string speedDesc = "Accepted for compatibility with the ImFluent server and ignored (real input always runs at normal speed).";
        var tools = new JsonArray();
        void Add(string name, string desc, JsonObject props, params string[] required)
        {
            var schema = new JsonObject { ["type"] = "object", ["properties"] = props };
            if (required.Length > 0)
                schema["required"] = new JsonArray(required.Select(r => (JsonNode)r).ToArray());
            tools.Add(new JsonObject { ["name"] = name, ["description"] = desc, ["inputSchema"] = schema });
        }
        JsonArray Enum(params string[] values) => new(values.Select(v => (JsonNode)v).ToArray());

        Add("app_info", "Application state: client size, DPI scale, theme, current gallery page, window rectangle.", new JsonObject());
        Add("list_pages", "List the gallery page ids accepted by goto_page (from the gallery's ControlInfoData.json).", new JsonObject());
        Add("goto_page", "Show a gallery page. Accepts a page id from list_pages or a control name such as \"Button\" or \"ComboBox\".",
            new JsonObject { ["page"] = Prop("string") }, "page");
        Add("set_theme", "Switch the gallery theme (through its Settings page; the choice is saved by the gallery). High contrast is not available in the WinUI gallery; \"system\" restores \"Use system setting\".",
            new JsonObject { ["theme"] = Prop("string", enumValues: Enum("light", "dark", "highcontrast")) }, "theme");
        Add("resize_window", "Resize the gallery window so its client area is width x height physical pixels.",
            new JsonObject { ["width"] = Prop("integer"), ["height"] = Prop("integer") }, "width", "height");
        Add("screenshot", "Capture the gallery client area as PNG (PrintWindow, works while occluded; windowed popups are composited). Crop to a window or a rectangle; downscale with 'scale'.",
            new JsonObject
            {
                ["window"] = Prop("string", "Window name from list_windows, or \"$FOCUSED\"."),
                ["rect"] = Prop("array", "[x, y, width, height] in client coordinates (physical pixels).", items: new JsonObject { ["type"] = "number" }),
                ["margin"] = Prop("number"),
                ["scale"] = Prop("number", "0.05..1 downscale factor."),
                ["save_path"] = Prop("string", "Also write the PNG to this file."),
                ["inline"] = Prop("boolean", "Return the image in the result (default true)."),
            });
        Add("list_windows", "List the gallery window and its open popups (flyouts, menus, combo box lists, tooltips) with their rectangles.",
            new JsonObject { ["include_hidden"] = Prop("boolean") });
        Add("nav_state", "Keyboard focus (UIA focused element: id, label, type, rect, window), open popups, last input source, whether a text field has focus.", new JsonObject());
        Add("list_items", "List the UIA elements (control view) under the current page, a window or an item, with id, label, type, rectangle and state flags.",
            new JsonObject
            {
                ["ref"] = Prop("string", "Parent item. Default: the whole page."),
                ["window"] = Prop("string", windowDesc),
                ["depth"] = Prop("integer"),
                ["filter"] = Prop("string", "Keep labels (or AutomationIds) that contain this text (case-insensitive)."),
                ["visible_only"] = Prop("boolean"),
                ["labeled_only"] = Prop("boolean"),
                ["max"] = Prop("integer"),
            });
        Add("item_info", "Information about one item (adds AutomationId, class name and supported UIA patterns).",
            new JsonObject { ["ref"] = Prop("string", refDesc), ["window"] = Prop("string", windowDesc) }, "ref");
        Add("click", "Move the real mouse to an item (scrolling it into view) and click it.",
            new JsonObject
            {
                ["ref"] = Prop("string", refDesc),
                ["window"] = Prop("string", windowDesc),
                ["button"] = Prop("string", enumValues: Enum("left", "right", "middle")),
                ["double"] = Prop("boolean"),
                ["speed"] = Prop("string", speedDesc),
            }, "ref");
        Add("hover", "Move the real mouse over an item.", new JsonObject { ["ref"] = Prop("string", refDesc), ["window"] = Prop("string", windowDesc) }, "ref");
        Add("key", "Press keys. 'keys' holds chords separated by spaces, e.g. \"Tab\", \"Shift+Tab\", \"Alt+Down\", \"Down Down Enter\". Returns the keyboard focus afterwards.",
            new JsonObject { ["keys"] = Prop("string"), ["count"] = Prop("integer", "Repeat the whole sequence."), ["speed"] = Prop("string", speedDesc) }, "keys");
        Add("type_text", "Type characters into the focused text field, or click 'ref' first.",
            new JsonObject
            {
                ["text"] = Prop("string"),
                ["ref"] = Prop("string", refDesc),
                ["window"] = Prop("string", windowDesc),
                ["replace"] = Prop("boolean", "Select all and replace the existing text."),
                ["enter"] = Prop("boolean", "Press Enter afterwards."),
            }, "text");
        Add("mouse", "Low-level real mouse at client coordinates (physical pixels): mouse_move, mouse_click, mouse_down, mouse_up, wheel.",
            new JsonObject
            {
                ["action"] = Prop("string", enumValues: Enum("mouse_move", "mouse_click", "mouse_down", "mouse_up", "wheel")),
                ["x"] = Prop("number"),
                ["y"] = Prop("number"),
                ["button"] = Prop("string", enumValues: Enum("left", "right", "middle")),
                ["count"] = Prop("integer"),
                ["dx"] = Prop("number"),
                ["dy"] = Prop("number"),
            }, "action");
        Add("run_steps",
            "Run a sequence of steps in one call. Each step is an object with 'action' and its arguments: "
            + "click{ref,button,double}, hover{ref}, hold{ref,seconds}, open{ref}, close{ref}, check{ref}, uncheck{ref}, focus{ref}, scroll_to{ref}, "
            + "key{keys,count}, key_down{keys}, key_up{keys}, type{text,ref,replace,enter}, mouse_move{x,y}, mouse_click{x,y,button,count}, mouse_down, mouse_up, wheel{dx,dy}, "
            + "wait{frames|seconds} (one frame = 16 ms), close_popups, input_mode{mode:mouse|keyboard}. Add \"state\":true to a step to record the keyboard focus after it.",
            new JsonObject
            {
                ["steps"] = Prop("array", items: new JsonObject { ["type"] = "object" }),
                ["window"] = Prop("string", windowDesc),
                ["keyboard"] = Prop("boolean", "Drive item actions with the keyboard (focus + Space/Enter) instead of the mouse."),
                ["speed"] = Prop("string", speedDesc),
            }, "steps");
        Add("run_tests", "Not supported by the WinUI driver (kept so both servers expose the same tool list).",
            new JsonObject { ["filter"] = Prop("string"), ["speed"] = Prop("string", speedDesc) });
        return tools;
    }
}

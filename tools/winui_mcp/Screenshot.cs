using System.Buffers.Binary;
using System.IO.Compression;
using static WinUiMcp.Native;

namespace WinUiMcp;

// RGBA8 image, rows top to bottom.
internal sealed class Image
{
    public int Width, Height;
    public byte[] Pixels = Array.Empty<byte>();

    public Image Crop(int x, int y, int w, int h)
    {
        var dst = new Image { Width = w, Height = h, Pixels = new byte[w * h * 4] };
        for (int row = 0; row < h; ++row)
            Buffer.BlockCopy(Pixels, ((y + row) * Width + x) * 4, dst.Pixels, row * w * 4, w * 4);
        return dst;
    }

    // Box-filter downscale, same as DownscaleRgba() in the ImFluent server.
    public Image Downscale(float scale)
    {
        int dw = Math.Max(1, (int)(Width * scale)), dh = Math.Max(1, (int)(Height * scale));
        var dst = new Image { Width = dw, Height = dh, Pixels = new byte[dw * dh * 4] };
        for (int y = 0; y < dh; ++y)
            for (int x = 0; x < dw; ++x)
            {
                int x0 = x * Width / dw, x1 = Math.Max(x0 + 1, (x + 1) * Width / dw);
                int y0 = y * Height / dh, y1 = Math.Max(y0 + 1, (y + 1) * Height / dh);
                int r = 0, g = 0, b = 0, a = 0, n = 0;
                for (int yy = y0; yy < y1; ++yy)
                    for (int xx = x0; xx < x1; ++xx)
                    {
                        int o = (yy * Width + xx) * 4;
                        r += Pixels[o]; g += Pixels[o + 1]; b += Pixels[o + 2]; a += Pixels[o + 3];
                        ++n;
                    }
                int d = (y * dw + x) * 4;
                dst.Pixels[d] = (byte)(r / n); dst.Pixels[d + 1] = (byte)(g / n); dst.Pixels[d + 2] = (byte)(b / n); dst.Pixels[d + 3] = (byte)(a / n);
            }
        return dst;
    }

    // Average luminance (0..255) of a rectangle, used to guess the theme.
    public double Luminance(int x, int y, int w, int h)
    {
        double sum = 0;
        int n = 0;
        for (int yy = Math.Max(0, y); yy < Math.Min(Height, y + h); ++yy)
            for (int xx = Math.Max(0, x); xx < Math.Min(Width, x + w); ++xx)
            {
                int o = (yy * Width + xx) * 4;
                sum += 0.2126 * Pixels[o] + 0.7152 * Pixels[o + 1] + 0.0722 * Pixels[o + 2];
                ++n;
            }
        return n > 0 ? sum / n : 0;
    }

    public byte[] EncodePng()
    {
        using var ms = new MemoryStream();
        ms.Write(new byte[] { 0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A });
        var ihdr = new byte[13];
        BinaryPrimitives.WriteInt32BigEndian(ihdr.AsSpan(0), Width);
        BinaryPrimitives.WriteInt32BigEndian(ihdr.AsSpan(4), Height);
        ihdr[8] = 8;  // bit depth
        ihdr[9] = 6;  // RGBA
        WriteChunk(ms, "IHDR", ihdr);

        using var raw = new MemoryStream();
        using (var z = new ZLibStream(raw, CompressionLevel.Fastest, leaveOpen: true))
        {
            int stride = Width * 4;
            var line = new byte[stride + 1];
            var prev = new byte[stride];
            for (int y = 0; y < Height; ++y)
            {
                // Filter "Up": cheap and compresses UI screenshots well.
                line[0] = 2;
                int o = y * stride;
                for (int i = 0; i < stride; ++i)
                    line[i + 1] = (byte)(Pixels[o + i] - prev[i]);
                Buffer.BlockCopy(Pixels, o, prev, 0, stride);
                z.Write(line, 0, line.Length);
            }
        }
        WriteChunk(ms, "IDAT", raw.ToArray());
        WriteChunk(ms, "IEND", Array.Empty<byte>());
        return ms.ToArray();
    }

    static void WriteChunk(Stream s, string type, byte[] data)
    {
        Span<byte> len = stackalloc byte[4];
        BinaryPrimitives.WriteInt32BigEndian(len, data.Length);
        s.Write(len);
        byte[] typeBytes = System.Text.Encoding.ASCII.GetBytes(type);
        s.Write(typeBytes);
        s.Write(data);
        uint crc = Crc32(typeBytes, 0xFFFFFFFFu);
        crc = Crc32(data, crc) ^ 0xFFFFFFFFu;
        Span<byte> c = stackalloc byte[4];
        BinaryPrimitives.WriteUInt32BigEndian(c, crc);
        s.Write(c);
    }

    static readonly uint[] s_CrcTable = Enumerable.Range(0, 256).Select(n =>
    {
        uint c = (uint)n;
        for (int k = 0; k < 8; ++k)
            c = (c & 1) != 0 ? 0xEDB88320u ^ (c >> 1) : c >> 1;
        return c;
    }).ToArray();

    static uint Crc32(byte[] data, uint crc)
    {
        foreach (byte b in data)
            crc = s_CrcTable[(crc ^ b) & 0xFF] ^ (crc >> 8);
        return crc;
    }
}

internal static class Screenshot
{
    // Captures a window with PrintWindow(PW_RENDERFULLCONTENT) (works while occluded) and returns its
    // client area. Falls back to null when the capture fails.
    public static Image? CaptureClient(IntPtr hwnd) => CaptureClient(hwnd, keepAlpha: false);

    // keepAlpha: keep the alpha channel PrintWindow produced (premultiplied; windowed XAML popups have
    // transparent shadow margins). Otherwise the image is made opaque.
    public static Image? CaptureClient(IntPtr hwnd, bool keepAlpha)
    {
        if (!GetWindowRect(hwnd, out RECT wr) || wr.Width <= 0 || wr.Height <= 0)
            return null;
        GetClientRect(hwnd, out RECT cr);
        var origin = ClientOrigin(hwnd);
        int cx = origin.X - wr.Left, cy = origin.Y - wr.Top;

        IntPtr screen = GetDC(IntPtr.Zero);
        IntPtr mem = CreateCompatibleDC(screen);
        IntPtr bmp = CreateCompatibleBitmap(screen, wr.Width, wr.Height);
        IntPtr old = SelectObject(mem, bmp);
        try
        {
            if (!PrintWindow(hwnd, mem, PW_RENDERFULLCONTENT))
                return null;
            SelectObject(mem, old);
            var bmi = new BITMAPINFOHEADER
            {
                biSize = System.Runtime.InteropServices.Marshal.SizeOf<BITMAPINFOHEADER>(),
                biWidth = wr.Width,
                biHeight = -wr.Height, // top-down
                biPlanes = 1,
                biBitCount = 32,
            };
            var bgra = new byte[wr.Width * wr.Height * 4];
            if (GetDIBits(mem, bmp, 0, (uint)wr.Height, bgra, ref bmi, DIB_RGB_COLORS) == 0)
                return null;
            for (int i = 0; i < bgra.Length; i += 4)
            {
                (bgra[i], bgra[i + 2]) = (bgra[i + 2], bgra[i]);
                if (!keepAlpha)
                    bgra[i + 3] = 255;
            }
            var full = new Image { Width = wr.Width, Height = wr.Height, Pixels = bgra };
            int w = Math.Min(cr.Width, wr.Width - cx), h = Math.Min(cr.Height, wr.Height - cy);
            if (cx < 0 || cy < 0 || w <= 0 || h <= 0)
                return full;
            return full.Crop(cx, cy, w, h);
        }
        finally
        {
            SelectObject(mem, old);
            DeleteObject(bmp);
            DeleteDC(mem);
            ReleaseDC(IntPtr.Zero, screen);
        }
    }
}

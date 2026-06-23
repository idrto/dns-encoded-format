using System.Text;

namespace Idrto.DnsEncodedFormat;

public static class Def
{
    public const int MaxLabelLength = 63;

    private static readonly UTF8Encoding Utf8Strict = new(false, true);

    public enum ErrorCode
    {
        LabelTooLong,
        InvalidEscape,
        InvalidUtf8
    }

    public sealed class DefException : Exception
    {
        public DefException(string message, ErrorCode code) : base(message)
        {
            Code = code;
        }

        public ErrorCode Code { get; }
    }

    private static bool IsLiteralByte(byte value) =>
        (value >= 0x61 && value <= 0x7a) || (value >= 0x30 && value <= 0x39);

    private static string Canonicalize(string input)
    {
        var builder = new StringBuilder(input.Length);
        foreach (var ch in input)
        {
            builder.Append(ch is >= 'A' and <= 'Z' ? (char)(ch + ('a' - 'A')) : ch);
        }
        return builder.ToString();
    }

    private static void EnsureFits(int currentLen, int addLen)
    {
        if (currentLen + addLen > MaxLabelLength)
        {
            throw new DefException("encoded label exceeds 63 characters", ErrorCode.LabelTooLong);
        }
    }

    public static string Encode(string input)
    {
        var bytes = Encoding.UTF8.GetBytes(Canonicalize(input));
        var builder = new StringBuilder();
        var currentLen = 0;

        foreach (var value in bytes)
        {
            if (IsLiteralByte(value))
            {
                EnsureFits(currentLen, 1);
                builder.Append((char)value);
                currentLen++;
            }
            else
            {
                var escape = $"-{value:x2}";
                EnsureFits(currentLen, escape.Length);
                builder.Append(escape);
                currentLen += escape.Length;
            }
        }

        return builder.ToString();
    }

    private static int? ParseHexByte(char h1, char h2)
    {
        int? Hi(char c) => c switch
        {
            >= '0' and <= '9' => c - '0',
            >= 'a' and <= 'f' => c - 'a' + 10,
            _ => null
        };

        var hi = Hi(h1);
        var lo = Hi(h2);
        return hi is null || lo is null ? null : hi.Value * 16 + lo.Value;
    }

    public static string Decode(string encoded)
    {
        if (encoded.Length > MaxLabelLength)
        {
            throw new DefException("encoded label exceeds 63 characters", ErrorCode.LabelTooLong);
        }

        var bytes = new List<byte>(encoded.Length);
        for (var i = 0; i < encoded.Length;)
        {
            var ch = encoded[i];
            if (IsLiteralByte((byte)ch))
            {
                bytes.Add((byte)ch);
                i++;
                continue;
            }

            if (ch != '-')
            {
                throw new DefException("invalid character in encoded input", ErrorCode.InvalidEscape);
            }

            if (i + 3 > encoded.Length)
            {
                throw new DefException("truncated escape sequence", ErrorCode.InvalidEscape);
            }

            var value = ParseHexByte(encoded[i + 1], encoded[i + 2]);
            if (value is null)
            {
                throw new DefException("invalid escape sequence", ErrorCode.InvalidEscape);
            }

            bytes.Add((byte)value);
            i += 3;
        }

        try
        {
            return Utf8Strict.GetString(bytes.ToArray());
        }
        catch (DecoderFallbackException ex)
        {
            throw new DefException("invalid utf-8 byte sequence", ErrorCode.InvalidUtf8, ex);
        }
    }
}

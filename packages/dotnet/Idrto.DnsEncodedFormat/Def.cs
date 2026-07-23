using System.Numerics;
using System.Security.Cryptography;
using System.Text;

namespace Idrto.DnsEncodedFormat;

public static class Def
{
    public const int MaxLabelLength = 63;
    public const string IdrtoHashMarker = "idrto-h1--";
    public const int HashBodyLength = 50;
    public const string StructuralSeparator = "--";
    public const string StructuralSeparatorEscaped = "-2d-2d";

    private const string Base36Alphabet = "0123456789abcdefghijklmnopqrstuvwxyz";
    private static ReadOnlySpan<byte> Hex => "0123456789abcdef"u8;
    private static readonly UTF8Encoding Utf8Strict = new(false, true);

    public enum ErrorCode
    {
        LabelTooLong,
        InvalidEscape,
        InvalidUtf8,
        InvalidEncoding,
        NotDecodable
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

    private static bool IsBase36Char(char c) =>
        (c >= '0' && c <= '9') || (c >= 'a' && c <= 'z');

    private static byte[] CanonicalizeToBytes(string input)
    {
        var bytes = Encoding.UTF8.GetBytes(input);
        for (var i = 0; i < bytes.Length; i++)
        {
            if (bytes[i] is >= 0x41 and <= 0x5a)
            {
                bytes[i] += 32;
            }
        }

        return bytes;
    }

    private static string EncodeBytes(ReadOnlySpan<byte> bytes)
    {
        var builder = new StringBuilder(bytes.Length * 3);
        foreach (var value in bytes)
        {
            if (IsLiteralByte(value))
            {
                builder.Append((char)value);
            }
            else
            {
                builder.Append('-');
                builder.Append((char)Hex[value >> 4]);
                builder.Append((char)Hex[value & 0x0f]);
            }
        }

        return builder.ToString();
    }

    /// <summary>Encode one DEF component.</summary>
    public static string EncodeComponent(string input) =>
        EncodeBytes(CanonicalizeToBytes(input));

    /// <summary>Encode input to a DEF body, preserving structural separators.</summary>
    public static string EncodeBody(string input)
    {
        var builder = new StringBuilder(input.Length * 2);
        var componentStart = 0;
        while (true)
        {
            var separator = input.IndexOf(StructuralSeparator, componentStart, StringComparison.Ordinal);
            if (separator < 0)
            {
                builder.Append(EncodeComponent(input[componentStart..]));
                return builder.ToString();
            }

            builder.Append(EncodeComponent(input[componentStart..separator]));
            builder.Append(StructuralSeparator);
            componentStart = separator + StructuralSeparator.Length;
        }
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

    /// <summary>Decode one DEF component.</summary>
    public static string DecodeComponent(string component)
    {
        if (component.Contains(StructuralSeparator, StringComparison.Ordinal))
        {
            throw new DefException("raw structural separator in component", ErrorCode.InvalidEscape);
        }

        var bytes = new byte[component.Length];
        var len = 0;

        for (var i = 0; i < component.Length;)
        {
            var code = component[i];
            if (IsLiteralByte((byte)code))
            {
                bytes[len++] = (byte)code;
                i++;
                continue;
            }

            if (code != '-')
            {
                throw new DefException("invalid character in encoded input", ErrorCode.InvalidEscape);
            }

            if (i + 3 > component.Length)
            {
                throw new DefException("truncated escape sequence", ErrorCode.InvalidEscape);
            }

            var value = ParseHexByte(component[i + 1], component[i + 2]);
            if (value is null)
            {
                throw new DefException("invalid escape sequence", ErrorCode.InvalidEscape);
            }

            bytes[len++] = (byte)value;
            i += 3;
        }

        try
        {
            return Utf8Strict.GetString(bytes.AsSpan(0, len));
        }
        catch (DecoderFallbackException)
        {
            throw new DefException("invalid utf-8 byte sequence", ErrorCode.InvalidUtf8);
        }
    }

    /// <summary>Decode a DEF body, preserving structural separators.</summary>
    public static string DecodeBody(string body)
    {
        var builder = new StringBuilder(body.Length);
        var componentStart = 0;
        while (true)
        {
            var separator = body.IndexOf(StructuralSeparator, componentStart, StringComparison.Ordinal);
            if (separator < 0)
            {
                builder.Append(DecodeComponent(body[componentStart..]));
                return builder.ToString();
            }

            builder.Append(DecodeComponent(body[componentStart..separator]));
            builder.Append(StructuralSeparator);
            componentStart = separator + StructuralSeparator.Length;
        }
    }

    private static string Base36(ReadOnlySpan<byte> data)
    {
        var n = new BigInteger(data, isUnsigned: true, isBigEndian: true);
        if (n.IsZero)
        {
            return new string('0', HashBodyLength);
        }

        var builder = new StringBuilder(HashBodyLength);
        while (n > 0)
        {
            n = BigInteger.DivRem(n, 36, out var rem);
            builder.Append(Base36Alphabet[(int)rem]);
        }

        var chars = builder.ToString().ToCharArray();
        Array.Reverse(chars);
        var outValue = new string(chars);
        return outValue.Length < HashBodyLength
            ? new string('0', HashBodyLength - outValue.Length) + outValue
            : outValue;
    }

    private static void ValidateMarker(string marker)
    {
        if (marker.Length < 3
            || marker.Length > 13
            || !marker.EndsWith(StructuralSeparator, StringComparison.Ordinal))
        {
            throw new DefException("invalid provider hash marker", ErrorCode.InvalidEncoding);
        }

        if (marker.StartsWith("xn--", StringComparison.Ordinal)
            || marker.Any(c => !(c is >= 'a' and <= 'z'
                || c is >= '0' and <= '9'
                || c == '-')))
        {
            throw new DefException("invalid provider hash marker", ErrorCode.InvalidEncoding);
        }
    }

    /// <summary>Encode profile input with the configured hash marker.</summary>
    public static string EncodeProfile(string input, string marker = IdrtoHashMarker)
    {
        ValidateMarker(marker);

        var canonical = CanonicalizeToBytes(input);
        var canonicalText = Utf8Strict.GetString(canonical);
        var label = EncodeBody(canonicalText);
        if (label.Length == 0 || label.StartsWith('-') || label.EndsWith('-'))
        {
            throw new DefException("invalid profile encoding", ErrorCode.InvalidEncoding);
        }

        if (label.Length <= MaxLabelLength
            && !label.StartsWith(marker, StringComparison.Ordinal))
        {
            return label;
        }

        var hashInput = EncodeComponent(canonicalText);
        var digest = SHA256.HashData(Encoding.UTF8.GetBytes(hashInput));
        var encoded = marker + Base36(digest);
        if (encoded.Length > MaxLabelLength)
        {
            throw new DefException("encoded label exceeds 63 characters", ErrorCode.LabelTooLong);
        }

        return encoded;
    }

    /// <summary>Decode a profile label with the configured hash marker.</summary>
    public static string DecodeProfile(string label, string marker = IdrtoHashMarker)
    {
        ValidateMarker(marker);

        if (label.StartsWith(marker, StringComparison.Ordinal))
        {
            var digest = label[marker.Length..];
            if (digest.Length != HashBodyLength)
            {
                throw new DefException("invalid profile hash label", ErrorCode.InvalidEncoding);
            }

            foreach (var c in digest)
            {
                if (!IsBase36Char(c))
                {
                    throw new DefException("invalid profile hash label", ErrorCode.InvalidEncoding);
                }
            }

            throw new DefException("profile hash label is not decodable", ErrorCode.NotDecodable);
        }

        if (label.Length > MaxLabelLength)
        {
            throw new DefException("encoded label exceeds 63 characters", ErrorCode.LabelTooLong);
        }

        if (label.Length == 0 || label.StartsWith('-') || label.EndsWith('-'))
        {
            throw new DefException("invalid profile encoding", ErrorCode.InvalidEncoding);
        }

        return DecodeBody(label);
    }

    /// <summary>Encode input with the default profile.</summary>
    public static string Encode(string input) => EncodeProfile(input, IdrtoHashMarker);

    /// <summary>Decode input with the default profile.</summary>
    public static string Decode(string label) => DecodeProfile(label, IdrtoHashMarker);
}

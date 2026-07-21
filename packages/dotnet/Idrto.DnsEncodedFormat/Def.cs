using System.Numerics;
using System.Security.Cryptography;
using System.Text;

namespace Idrto.DnsEncodedFormat;

public static class Def
{
    public const int MaxLabelLength = 63;
    public const int MaxDefBodyLength = 62;
    public const char DefPrefix = 'd';
    public const char HashPrefix = 'h';

    private const string Base36Alphabet = "0123456789abcdefghijklmnopqrstuvwxyz";
    private const int HashBodyLength = 50;
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

    private static string Canonicalize(string input)
    {
        var builder = new StringBuilder(input.Length);
        foreach (var ch in input)
        {
            builder.Append(ch is >= 'A' and <= 'Z' ? (char)(ch + ('a' - 'A')) : ch);
        }
        return builder.ToString();
    }

    private static string EncodeDefBody(byte[] bytes)
    {
        var builder = new StringBuilder();
        foreach (var value in bytes)
        {
            if (IsLiteralByte(value))
            {
                builder.Append((char)value);
            }
            else
            {
                builder.Append($"-{value:x2}");
            }
        }
        return builder.ToString();
    }

    private static string Base36(byte[] data)
    {
        var n = new BigInteger(data, isUnsigned: true, isBigEndian: true);
        if (n.IsZero)
        {
            return new string('0', HashBodyLength);
        }

        var builder = new StringBuilder();
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

    private static string EncodeHash(byte[] canonicalBytes)
    {
        var hash = SHA256.HashData(canonicalBytes);
        var encoded = HashPrefix + Base36(hash);
        if (encoded.Length > MaxLabelLength)
        {
            throw new DefException("encoded label exceeds 63 characters", ErrorCode.LabelTooLong);
        }
        return encoded;
    }

    public static string Encode(string input)
    {
        var bytes = Encoding.UTF8.GetBytes(Canonicalize(input));
        var body = EncodeDefBody(bytes);

        if (body.Length <= MaxDefBodyLength)
        {
            var encoded = DefPrefix + body;
            if (encoded.Length > MaxLabelLength)
            {
                throw new DefException("encoded label exceeds 63 characters", ErrorCode.LabelTooLong);
            }
            return encoded;
        }

        return EncodeHash(Encoding.UTF8.GetBytes(body));
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

    private static string DecodeDefBody(string body)
    {
        var bytes = new List<byte>(body.Length);
        for (var i = 0; i < body.Length;)
        {
            var ch = body[i];
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

            if (i + 3 > body.Length)
            {
                throw new DefException("truncated escape sequence", ErrorCode.InvalidEscape);
            }

            var value = ParseHexByte(body[i + 1], body[i + 2]);
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
        catch (DecoderFallbackException)
        {
            throw new DefException("invalid utf-8 byte sequence", ErrorCode.InvalidUtf8);
        }
    }

    public static string Decode(string encoded)
    {
        if (encoded.Length > MaxLabelLength)
        {
            throw new DefException("encoded label exceeds 63 characters", ErrorCode.LabelTooLong);
        }

        if (encoded.Length == 0)
        {
            throw new DefException("missing encoding prefix", ErrorCode.InvalidEncoding);
        }

        return encoded[0] switch
        {
            HashPrefix => throw new DefException("hash-encoded label is not decodable", ErrorCode.NotDecodable),
            DefPrefix => DecodeDefBody(encoded[1..]),
            _ => throw new DefException("unrecognized encoding prefix", ErrorCode.InvalidEncoding)
        };
    }
}

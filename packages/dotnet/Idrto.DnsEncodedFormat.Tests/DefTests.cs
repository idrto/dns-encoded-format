using System.Text.Json;

namespace Idrto.DnsEncodedFormat.Tests;

public class DefTests
{
    private static JsonDocument LoadVectors()
    {
        var path = Path.GetFullPath(Path.Combine(AppContext.BaseDirectory, "..", "..", "..", "..", "..", "..", "vectors", "test-vectors.json"));
        return JsonDocument.Parse(File.ReadAllText(path));
    }

    private static Def.ErrorCode ReasonToCode(string reason) => reason switch
    {
        "label_too_long" => Def.ErrorCode.LabelTooLong,
        "invalid_escape" => Def.ErrorCode.InvalidEscape,
        "invalid_utf8" => Def.ErrorCode.InvalidUtf8,
        _ => throw new ArgumentOutOfRangeException(nameof(reason), reason, null)
    };

    [Fact]
    public void EncodeVectors()
    {
        using var vectors = LoadVectors();
        foreach (var element in vectors.RootElement.GetProperty("encode").EnumerateArray())
        {
            var input = element.GetProperty("input").GetString()!;
            var encoded = element.GetProperty("encoded").GetString()!;
            Assert.Equal(encoded, Def.Encode(input));
        }
    }

    [Fact]
    public void EncodeErrors()
    {
        using var vectors = LoadVectors();
        foreach (var element in vectors.RootElement.GetProperty("encode_errors").EnumerateArray())
        {
            var input = element.GetProperty("input").GetString()!;
            var reason = element.GetProperty("reason").GetString()!;
            var ex = Assert.Throws<Def.DefException>(() => Def.Encode(input));
            Assert.Equal(ReasonToCode(reason), ex.Code);
        }
    }

    [Fact]
    public void DecodeVectors()
    {
        using var vectors = LoadVectors();
        foreach (var element in vectors.RootElement.GetProperty("decode").EnumerateArray())
        {
            var input = element.GetProperty("input").GetString()!;
            var decoded = element.GetProperty("decoded").GetString()!;
            Assert.Equal(decoded, Def.Decode(input));
        }
    }

    [Fact]
    public void DecodeErrors()
    {
        using var vectors = LoadVectors();
        foreach (var element in vectors.RootElement.GetProperty("decode_errors").EnumerateArray())
        {
            var input = element.GetProperty("input").GetString()!;
            var reason = element.GetProperty("reason").GetString()!;
            var ex = Assert.Throws<Def.DefException>(() => Def.Decode(input));
            Assert.Equal(ReasonToCode(reason), ex.Code);
        }
    }
}

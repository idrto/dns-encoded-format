using System.Text.Json;
using Xunit;

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
        "invalid_encoding" => Def.ErrorCode.InvalidEncoding,
        "invalid_locator" => Def.ErrorCode.InvalidLocator,
        "not_decodable" => Def.ErrorCode.NotDecodable,
        _ => throw new ArgumentOutOfRangeException(nameof(reason), reason, null)
    };

    [Fact]
    public void EncodeBodyVectors()
    {
        using var vectors = LoadVectors();
        foreach (var element in vectors.RootElement.GetProperty("encode_body").EnumerateArray())
        {
            var input = element.GetProperty("input").GetString()!;
            var encoded = element.GetProperty("encoded").GetString()!;
            Assert.Equal(encoded, Def.EncodeBody(input));
        }
    }

    [Fact]
    public void DecodeBodyVectors()
    {
        using var vectors = LoadVectors();
        foreach (var element in vectors.RootElement.GetProperty("decode_body").EnumerateArray())
        {
            var input = element.GetProperty("input").GetString()!;
            var decoded = element.GetProperty("decoded").GetString()!;
            Assert.Equal(decoded, Def.DecodeBody(input));
        }
    }

    [Fact]
    public void DecodeBodyErrors()
    {
        using var vectors = LoadVectors();
        foreach (var element in vectors.RootElement.GetProperty("decode_body_errors").EnumerateArray())
        {
            var input = element.GetProperty("input").GetString()!;
            var reason = element.GetProperty("reason").GetString()!;
            var ex = Assert.Throws<Def.DefException>(() => Def.DecodeBody(input));
            Assert.Equal(ReasonToCode(reason), ex.Code);
        }
    }

    [Fact]
    public void EncodeProfileVectors()
    {
        using var vectors = LoadVectors();
        foreach (var element in vectors.RootElement.GetProperty("encode_profile").EnumerateArray())
        {
            var input = element.GetProperty("input").GetString()!;
            var encoded = element.GetProperty("encoded").GetString()!;
            Assert.Equal(encoded, Def.EncodeProfile(input, Def.IdrtoHashMarker));
        }
    }

    [Fact]
    public void EncodeProfileHashVectors()
    {
        using var vectors = LoadVectors();
        foreach (var element in vectors.RootElement.GetProperty("encode_profile_hash").EnumerateArray())
        {
            var input = element.GetProperty("input").GetString()!;
            var encoded = element.GetProperty("encoded").GetString()!;
            Assert.Equal(encoded, Def.EncodeProfile(input, Def.IdrtoHashMarker));
        }
    }

    [Fact]
    public void EncodeProfileErrors()
    {
        using var vectors = LoadVectors();
        foreach (var element in vectors.RootElement.GetProperty("encode_profile_errors").EnumerateArray())
        {
            var input = element.GetProperty("input").GetString()!;
            var reason = element.GetProperty("reason").GetString()!;
            var ex = Assert.Throws<Def.DefException>(() => Def.EncodeProfile(input, Def.IdrtoHashMarker));
            Assert.Equal(ReasonToCode(reason), ex.Code);
        }
    }

    [Fact]
    public void DecodeProfileVectors()
    {
        using var vectors = LoadVectors();
        foreach (var element in vectors.RootElement.GetProperty("decode_profile").EnumerateArray())
        {
            var input = element.GetProperty("input").GetString()!;
            var decoded = element.GetProperty("decoded").GetString()!;
            Assert.Equal(decoded, Def.DecodeProfile(input, Def.IdrtoHashMarker));
        }
    }

    [Fact]
    public void DecodeProfileErrors()
    {
        using var vectors = LoadVectors();
        foreach (var element in vectors.RootElement.GetProperty("decode_profile_errors").EnumerateArray())
        {
            var input = element.GetProperty("input").GetString()!;
            var reason = element.GetProperty("reason").GetString()!;
            var ex = Assert.Throws<Def.DefException>(() => Def.DecodeProfile(input, Def.IdrtoHashMarker));
            Assert.Equal(ReasonToCode(reason), ex.Code);
        }
    }

    [Fact]
    public void EncodeShortcutVectors()
    {
        using var vectors = LoadVectors();
        foreach (var element in vectors.RootElement.GetProperty("encode_profile").EnumerateArray())
        {
            var input = element.GetProperty("input").GetString()!;
            var encoded = element.GetProperty("encoded").GetString()!;
            Assert.Equal(encoded, Def.Encode(input));
        }
    }

    [Fact]
    public void DecodeShortcutVectors()
    {
        using var vectors = LoadVectors();
        foreach (var element in vectors.RootElement.GetProperty("decode_profile").EnumerateArray())
        {
            var input = element.GetProperty("input").GetString()!;
            var decoded = element.GetProperty("decoded").GetString()!;
            Assert.Equal(decoded, Def.Decode(input));
        }
    }
}

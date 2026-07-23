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
        "not_decodable" => Def.ErrorCode.NotDecodable,
        _ => throw new ArgumentOutOfRangeException(nameof(reason), reason, null)
    };

    [Fact]
    public void EncodeComponentVectors()
    {
        using var vectors = LoadVectors();
        foreach (var element in vectors.RootElement.GetProperty("encode_component").EnumerateArray())
        {
            var input = element.GetProperty("input").GetString()!;
            var encoded = element.GetProperty("encoded").GetString()!;
            Assert.Equal(encoded, Def.EncodeComponent(input));
        }
    }

    [Fact]
    public void DecodeComponentVectors()
    {
        using var vectors = LoadVectors();
        foreach (var element in vectors.RootElement.GetProperty("decode_component").EnumerateArray())
        {
            var input = element.GetProperty("input").GetString()!;
            var decoded = element.GetProperty("decoded").GetString()!;
            Assert.Equal(decoded, Def.DecodeComponent(input));
        }
    }

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
    public void DeterministicRoundTripsPreserveSeparators()
    {
        for (var i = 0; i < 256; i++)
        {
            var component = $"A{i}-x@用户{(char)('a' + i % 26)}";
            var encodedComponent = Def.EncodeComponent(component);
            Assert.DoesNotContain(Def.StructuralSeparator, encodedComponent);
            Assert.Equal(component.ToLowerInvariant(), Def.DecodeComponent(encodedComponent));

            var input = component
                + string.Concat(Enumerable.Repeat(Def.StructuralSeparator, i % 4))
                + (i % 3 == 0 ? "-" : "")
                + $"B{255 - i}";
            var encodedBody = Def.EncodeBody(input);
            Assert.Equal(SeparatorCount(input), SeparatorCount(encodedBody));
            Assert.Equal(input.ToLowerInvariant(), Def.DecodeBody(encodedBody));
        }

        var ex = Assert.Throws<Def.DefException>(() => Def.DecodeComponent("a--b"));
        Assert.Equal(Def.ErrorCode.InvalidEscape, ex.Code);
    }

    private static int SeparatorCount(string value)
    {
        var count = 0;
        var offset = 0;
        while (true)
        {
            var found = value.IndexOf(Def.StructuralSeparator, offset, StringComparison.Ordinal);
            if (found < 0)
            {
                return count;
            }

            count++;
            offset = found + Def.StructuralSeparator.Length;
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
    public void ProfileIsGenericAndHashesMarkerCollisions()
    {
        Assert.Equal("xn--name", Def.EncodeProfile("XN--Name"));
        Assert.Equal("xn--name", Def.DecodeProfile("xn--name"));

        const string marker = "abc--";
        var collision = Def.EncodeProfile("abc--value", marker);
        Assert.StartsWith(marker, collision);
        var ex = Assert.Throws<Def.DefException>(() => Def.DecodeProfile(collision, marker));
        Assert.Equal(Def.ErrorCode.NotDecodable, ex.Code);
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

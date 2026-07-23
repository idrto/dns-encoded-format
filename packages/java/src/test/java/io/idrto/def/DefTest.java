package io.idrto.def;

import com.google.gson.Gson;
import com.google.gson.JsonArray;
import com.google.gson.JsonObject;
import org.junit.jupiter.api.Test;

import java.nio.file.Files;
import java.nio.file.Path;
import java.util.Locale;

import static org.junit.jupiter.api.Assertions.assertEquals;
import static org.junit.jupiter.api.Assertions.assertFalse;
import static org.junit.jupiter.api.Assertions.assertThrows;
import static org.junit.jupiter.api.Assertions.assertTrue;

class DefTest {
    private static JsonObject loadVectors() throws Exception {
        Path path = Path.of("..", "..", "vectors", "test-vectors.json").normalize();
        String json = Files.readString(path);
        return new Gson().fromJson(json, JsonObject.class);
    }

    private static Def.ErrorCode reasonToCode(String reason) {
        return switch (reason) {
            case "label_too_long" -> Def.ErrorCode.LABEL_TOO_LONG;
            case "invalid_escape" -> Def.ErrorCode.INVALID_ESCAPE;
            case "invalid_utf8" -> Def.ErrorCode.INVALID_UTF8;
            case "invalid_encoding" -> Def.ErrorCode.INVALID_ENCODING;
            case "not_decodable" -> Def.ErrorCode.NOT_DECODABLE;
            default -> throw new IllegalArgumentException("unknown reason: " + reason);
        };
    }

    @Test
    void encodeComponentVectors() throws Exception {
        JsonArray cases = loadVectors().getAsJsonArray("encode_component");
        for (var element : cases) {
            JsonObject c = element.getAsJsonObject();
            assertEquals(c.get("encoded").getAsString(), Def.encodeComponent(c.get("input").getAsString()));
        }
    }

    @Test
    void decodeComponentVectors() throws Exception {
        JsonArray cases = loadVectors().getAsJsonArray("decode_component");
        for (var element : cases) {
            JsonObject c = element.getAsJsonObject();
            assertEquals(c.get("decoded").getAsString(), Def.decodeComponent(c.get("input").getAsString()));
        }
    }

    @Test
    void encodeBodyVectors() throws Exception {
        JsonArray cases = loadVectors().getAsJsonArray("encode_body");
        for (var element : cases) {
            JsonObject c = element.getAsJsonObject();
            assertEquals(c.get("encoded").getAsString(), Def.encodeBody(c.get("input").getAsString()));
        }
    }

    @Test
    void deterministicRoundTripsPreserveSeparators() throws Exception {
        for (int i = 0; i < 256; i++) {
            String component = "A" + i + "-x@用户" + (char) ('a' + (i % 26));
            String encodedComponent = Def.encodeComponent(component);
            assertFalse(encodedComponent.contains(Def.STRUCTURAL_SEPARATOR));
            assertEquals(component.toLowerCase(Locale.ROOT), Def.decodeComponent(encodedComponent));

            String input = component
                    + Def.STRUCTURAL_SEPARATOR.repeat(i % 4)
                    + (i % 3 == 0 ? "-" : "")
                    + "B" + (255 - i);
            String encodedBody = Def.encodeBody(input);
            assertEquals(separatorCount(input), separatorCount(encodedBody));
            assertEquals(input.toLowerCase(Locale.ROOT), Def.decodeBody(encodedBody));
        }

        Def.DefException ex = assertThrows(
                Def.DefException.class,
                () -> Def.decodeComponent("a--b")
        );
        assertEquals(Def.ErrorCode.INVALID_ESCAPE, ex.getCode());
    }

    private static int separatorCount(String value) {
        int count = 0;
        int offset = 0;
        int found;
        while ((found = value.indexOf(Def.STRUCTURAL_SEPARATOR, offset)) >= 0) {
            count++;
            offset = found + Def.STRUCTURAL_SEPARATOR.length();
        }
        return count;
    }

    @Test
    void decodeBodyVectors() throws Exception {
        JsonArray cases = loadVectors().getAsJsonArray("decode_body");
        for (var element : cases) {
            JsonObject c = element.getAsJsonObject();
            assertEquals(c.get("decoded").getAsString(), Def.decodeBody(c.get("input").getAsString()));
        }
    }

    @Test
    void decodeBodyErrors() throws Exception {
        JsonArray cases = loadVectors().getAsJsonArray("decode_body_errors");
        for (var element : cases) {
            JsonObject c = element.getAsJsonObject();
            Def.DefException ex = assertThrows(
                    Def.DefException.class,
                    () -> Def.decodeBody(c.get("input").getAsString())
            );
            assertEquals(reasonToCode(c.get("reason").getAsString()), ex.getCode());
        }
    }

    @Test
    void encodeProfileVectors() throws Exception {
        JsonArray cases = loadVectors().getAsJsonArray("encode_profile");
        for (var element : cases) {
            JsonObject c = element.getAsJsonObject();
            assertEquals(
                    c.get("encoded").getAsString(),
                    Def.encodeProfile(c.get("input").getAsString(), Def.IDRTO_HASH_MARKER)
            );
        }
    }

    @Test
    void encodeProfileHashVectors() throws Exception {
        JsonArray cases = loadVectors().getAsJsonArray("encode_profile_hash");
        for (var element : cases) {
            JsonObject c = element.getAsJsonObject();
            assertEquals(
                    c.get("encoded").getAsString(),
                    Def.encodeProfile(c.get("input").getAsString(), Def.IDRTO_HASH_MARKER)
            );
        }
    }

    @Test
    void encodeProfileErrors() throws Exception {
        JsonArray cases = loadVectors().getAsJsonArray("encode_profile_errors");
        for (var element : cases) {
            JsonObject c = element.getAsJsonObject();
            Def.DefException ex = assertThrows(
                    Def.DefException.class,
                    () -> Def.encodeProfile(c.get("input").getAsString(), Def.IDRTO_HASH_MARKER)
            );
            assertEquals(reasonToCode(c.get("reason").getAsString()), ex.getCode());
        }
    }

    @Test
    void decodeProfileVectors() throws Exception {
        JsonArray cases = loadVectors().getAsJsonArray("decode_profile");
        for (var element : cases) {
            JsonObject c = element.getAsJsonObject();
            assertEquals(
                    c.get("decoded").getAsString(),
                    Def.decodeProfile(c.get("input").getAsString(), Def.IDRTO_HASH_MARKER)
            );
        }
    }

    @Test
    void decodeProfileErrors() throws Exception {
        JsonArray cases = loadVectors().getAsJsonArray("decode_profile_errors");
        for (var element : cases) {
            JsonObject c = element.getAsJsonObject();
            Def.DefException ex = assertThrows(
                    Def.DefException.class,
                    () -> Def.decodeProfile(c.get("input").getAsString(), Def.IDRTO_HASH_MARKER)
            );
            assertEquals(reasonToCode(c.get("reason").getAsString()), ex.getCode());
        }
    }

    @Test
    void profileIsGenericAndHashesMarkerCollisions() throws Exception {
        assertEquals("xn--name", Def.encodeProfile("XN--Name"));
        assertEquals("xn--name", Def.decodeProfile("xn--name"));

        String marker = "abc--";
        String collision = Def.encodeProfile("abc--value", marker);
        assertTrue(collision.startsWith(marker));
        Def.DefException ex = assertThrows(
                Def.DefException.class,
                () -> Def.decodeProfile(collision, marker)
        );
        assertEquals(Def.ErrorCode.NOT_DECODABLE, ex.getCode());
    }

    @Test
    void idrtoShortcuts() throws Exception {
        JsonObject vectors = loadVectors();

        for (var element : vectors.getAsJsonArray("encode_profile")) {
            JsonObject c = element.getAsJsonObject();
            assertEquals(c.get("encoded").getAsString(), Def.encode(c.get("input").getAsString()));
        }

        for (var element : vectors.getAsJsonArray("decode_profile")) {
            JsonObject c = element.getAsJsonObject();
            assertEquals(c.get("decoded").getAsString(), Def.decode(c.get("input").getAsString()));
        }
    }
}

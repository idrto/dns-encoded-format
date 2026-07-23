package io.idrto.def;

import com.google.gson.Gson;
import com.google.gson.JsonArray;
import com.google.gson.JsonObject;
import org.junit.jupiter.api.Test;

import java.nio.file.Files;
import java.nio.file.Path;

import static org.junit.jupiter.api.Assertions.assertEquals;
import static org.junit.jupiter.api.Assertions.assertThrows;

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
            case "invalid_locator" -> Def.ErrorCode.INVALID_LOCATOR;
            case "not_decodable" -> Def.ErrorCode.NOT_DECODABLE;
            default -> throw new IllegalArgumentException("unknown reason: " + reason);
        };
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

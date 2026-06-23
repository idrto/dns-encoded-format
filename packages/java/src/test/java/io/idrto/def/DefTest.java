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
            default -> throw new IllegalArgumentException("unknown reason: " + reason);
        };
    }

    @Test
    void encodeVectors() throws Exception {
        JsonArray cases = loadVectors().getAsJsonArray("encode");
        for (var element : cases) {
            JsonObject c = element.getAsJsonObject();
            assertEquals(c.get("encoded").getAsString(), Def.encode(c.get("input").getAsString()));
        }
    }

    @Test
    void encodeErrors() throws Exception {
        JsonArray cases = loadVectors().getAsJsonArray("encode_errors");
        for (var element : cases) {
            JsonObject c = element.getAsJsonObject();
            Def.DefException ex = assertThrows(
                    Def.DefException.class,
                    () -> Def.encode(c.get("input").getAsString())
            );
            assertEquals(reasonToCode(c.get("reason").getAsString()), ex.getCode());
        }
    }

    @Test
    void decodeVectors() throws Exception {
        JsonArray cases = loadVectors().getAsJsonArray("decode");
        for (var element : cases) {
            JsonObject c = element.getAsJsonObject();
            assertEquals(c.get("decoded").getAsString(), Def.decode(c.get("input").getAsString()));
        }
    }

    @Test
    void decodeErrors() throws Exception {
        JsonArray cases = loadVectors().getAsJsonArray("decode_errors");
        for (var element : cases) {
            JsonObject c = element.getAsJsonObject();
            Def.DefException ex = assertThrows(
                    Def.DefException.class,
                    () -> Def.decode(c.get("input").getAsString())
            );
            assertEquals(reasonToCode(c.get("reason").getAsString()), ex.getCode());
        }
    }
}

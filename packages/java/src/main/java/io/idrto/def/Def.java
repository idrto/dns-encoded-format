package io.idrto.def;

import java.nio.ByteBuffer;
import java.nio.charset.CharacterCodingException;
import java.nio.charset.CodingErrorAction;
import java.nio.charset.StandardCharsets;

public final class Def {
    public static final int MAX_LABEL_LENGTH = 63;

    public enum ErrorCode {
        LABEL_TOO_LONG,
        INVALID_ESCAPE,
        INVALID_UTF8
    }

    public static final class DefException extends Exception {
        private final ErrorCode code;

        public DefException(String message, ErrorCode code) {
            super(message);
            this.code = code;
        }

        public ErrorCode getCode() {
            return code;
        }
    }

    private Def() {}

    private static boolean isLiteralByte(int value) {
        return (value >= 0x61 && value <= 0x7a) || (value >= 0x30 && value <= 0x39);
    }

    private static String canonicalize(String input) {
        StringBuilder out = new StringBuilder(input.length());
        for (int i = 0; i < input.length(); i++) {
            char ch = input.charAt(i);
            if (ch >= 'A' && ch <= 'Z') {
                out.append((char) (ch + ('a' - 'A')));
            } else {
                out.append(ch);
            }
        }
        return out.toString();
    }

    private static void ensureFits(int currentLen, int addLen) throws DefException {
        if (currentLen + addLen > MAX_LABEL_LENGTH) {
            throw new DefException("encoded label exceeds 63 characters", ErrorCode.LABEL_TOO_LONG);
        }
    }

    public static String encode(String input) throws DefException {
        byte[] bytes = canonicalize(input).getBytes(StandardCharsets.UTF_8);
        StringBuilder out = new StringBuilder();
        int currentLen = 0;

        for (byte value : bytes) {
            int unsigned = value & 0xff;
            if (isLiteralByte(unsigned)) {
                ensureFits(currentLen, 1);
                out.append((char) unsigned);
                currentLen++;
            } else {
                String escape = String.format("-%02x", unsigned);
                ensureFits(currentLen, escape.length());
                out.append(escape);
                currentLen += escape.length();
            }
        }

        return out.toString();
    }

    private static Integer parseHexByte(char h1, char h2) {
        int hi = hexNibble(h1);
        int lo = hexNibble(h2);
        if (hi < 0 || lo < 0) {
            return null;
        }
        return hi * 16 + lo;
    }

    private static int hexNibble(char ch) {
        if (ch >= '0' && ch <= '9') {
            return ch - '0';
        }
        if (ch >= 'a' && ch <= 'f') {
            return ch - 'a' + 10;
        }
        return -1;
    }

    public static String decode(String encoded) throws DefException {
        if (encoded.length() > MAX_LABEL_LENGTH) {
            throw new DefException("encoded label exceeds 63 characters", ErrorCode.LABEL_TOO_LONG);
        }

        byte[] out = new byte[encoded.length()];
        int outLen = 0;

        for (int i = 0; i < encoded.length(); ) {
            char ch = encoded.charAt(i);
            if (isLiteralByte(ch)) {
                out[outLen++] = (byte) ch;
                i++;
                continue;
            }

            if (ch != '-') {
                throw new DefException("invalid character in encoded input", ErrorCode.INVALID_ESCAPE);
            }
            if (i + 3 > encoded.length()) {
                throw new DefException("truncated escape sequence", ErrorCode.INVALID_ESCAPE);
            }

            Integer value = parseHexByte(encoded.charAt(i + 1), encoded.charAt(i + 2));
            if (value == null) {
                throw new DefException("invalid escape sequence", ErrorCode.INVALID_ESCAPE);
            }

            out[outLen++] = value.byteValue();
            i += 3;
        }

        try {
            var decoder = StandardCharsets.UTF_8.newDecoder()
                    .onMalformedInput(CodingErrorAction.REPORT)
                    .onUnmappableCharacter(CodingErrorAction.REPORT);
            return decoder.decode(ByteBuffer.wrap(out, 0, outLen)).toString();
        } catch (CharacterCodingException ex) {
            throw new DefException("invalid utf-8 byte sequence", ErrorCode.INVALID_UTF8);
        }
    }
}

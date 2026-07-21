package io.idrto.def;

import java.math.BigInteger;
import java.nio.ByteBuffer;
import java.nio.charset.CharacterCodingException;
import java.nio.charset.CodingErrorAction;
import java.nio.charset.StandardCharsets;
import java.security.MessageDigest;
import java.security.NoSuchAlgorithmException;

public final class Def {
    public static final int MAX_LABEL_LENGTH = 63;
    public static final int MAX_DEF_BODY_LENGTH = 62;
    public static final char DEF_PREFIX = 'd';
    public static final char HASH_PREFIX = 'h';

    private static final String BASE36_ALPHABET = "0123456789abcdefghijklmnopqrstuvwxyz";
    private static final int HASH_BODY_LENGTH = 50;

    public enum ErrorCode {
        LABEL_TOO_LONG,
        INVALID_ESCAPE,
        INVALID_UTF8,
        INVALID_ENCODING,
        NOT_DECODABLE
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

    private static String encodeDefBody(byte[] bytes) {
        StringBuilder out = new StringBuilder();
        for (byte value : bytes) {
            int unsigned = value & 0xff;
            if (isLiteralByte(unsigned)) {
                out.append((char) unsigned);
            } else {
                out.append(String.format("-%02x", unsigned));
            }
        }
        return out.toString();
    }

    private static String base36(byte[] data) {
        BigInteger n = new BigInteger(1, data);
        if (n.signum() == 0) {
            return "0".repeat(HASH_BODY_LENGTH);
        }

        StringBuilder out = new StringBuilder();
        BigInteger base = BigInteger.valueOf(36);
        while (n.signum() > 0) {
            BigInteger[] divRem = n.divideAndRemainder(base);
            out.append(BASE36_ALPHABET.charAt(divRem[1].intValue()));
            n = divRem[0];
        }

        out.reverse();
        while (out.length() < HASH_BODY_LENGTH) {
            out.insert(0, '0');
        }
        return out.toString();
    }

    private static String encodeHash(byte[] canonicalBytes) throws DefException {
        try {
            MessageDigest digest = MessageDigest.getInstance("SHA-256");
            byte[] hash = digest.digest(canonicalBytes);
            String encoded = HASH_PREFIX + base36(hash);
            if (encoded.length() > MAX_LABEL_LENGTH) {
                throw new DefException("encoded label exceeds 63 characters", ErrorCode.LABEL_TOO_LONG);
            }
            return encoded;
        } catch (NoSuchAlgorithmException ex) {
            throw new IllegalStateException("SHA-256 not available", ex);
        }
    }

    public static String encode(String input) throws DefException {
        byte[] bytes = canonicalize(input).getBytes(StandardCharsets.UTF_8);
        String body = encodeDefBody(bytes);

        if (body.length() <= MAX_DEF_BODY_LENGTH) {
            String encoded = DEF_PREFIX + body;
            if (encoded.length() > MAX_LABEL_LENGTH) {
                throw new DefException("encoded label exceeds 63 characters", ErrorCode.LABEL_TOO_LONG);
            }
            return encoded;
        }

        return encodeHash(body.getBytes(StandardCharsets.UTF_8));
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

    private static String decodeDefBody(String body) throws DefException {
        byte[] out = new byte[body.length()];
        int outLen = 0;

        for (int i = 0; i < body.length(); ) {
            char ch = body.charAt(i);
            if (isLiteralByte(ch)) {
                out[outLen++] = (byte) ch;
                i++;
                continue;
            }

            if (ch != '-') {
                throw new DefException("invalid character in encoded input", ErrorCode.INVALID_ESCAPE);
            }
            if (i + 3 > body.length()) {
                throw new DefException("truncated escape sequence", ErrorCode.INVALID_ESCAPE);
            }

            Integer value = parseHexByte(body.charAt(i + 1), body.charAt(i + 2));
            if (value == null) {
                throw new DefException("invalid escape sequence", ErrorCode.INVALID_ESCAPE);
            }

            out[outLen++] = value.byteValue();
            i += 3;
        }

        try {
            var decoder = StandardCharsets.UTF_8.newDecoder()
                    .onMalformedInput(CodingErrorAction.REPORT)
                    .onUnmappedCharacter(CodingErrorAction.REPORT);
            return decoder.decode(ByteBuffer.wrap(out, 0, outLen)).toString();
        } catch (CharacterCodingException ex) {
            throw new DefException("invalid utf-8 byte sequence", ErrorCode.INVALID_UTF8);
        }
    }

    public static String decode(String encoded) throws DefException {
        if (encoded.length() > MAX_LABEL_LENGTH) {
            throw new DefException("encoded label exceeds 63 characters", ErrorCode.LABEL_TOO_LONG);
        }

        if (encoded.isEmpty()) {
            throw new DefException("missing encoding prefix", ErrorCode.INVALID_ENCODING);
        }

        char prefix = encoded.charAt(0);
        if (prefix == HASH_PREFIX) {
            throw new DefException("hash-encoded label is not decodable", ErrorCode.NOT_DECODABLE);
        }
        if (prefix != DEF_PREFIX) {
            throw new DefException("unrecognized encoding prefix", ErrorCode.INVALID_ENCODING);
        }

        return decodeDefBody(encoded.substring(1));
    }
}

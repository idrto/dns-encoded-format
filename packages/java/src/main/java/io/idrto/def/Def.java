package io.idrto.def;

import java.math.BigInteger;
import java.nio.ByteBuffer;
import java.nio.charset.CharacterCodingException;
import java.nio.charset.CodingErrorAction;
import java.nio.charset.StandardCharsets;
import java.security.MessageDigest;
import java.security.NoSuchAlgorithmException;
import java.util.Arrays;

public final class Def {
    public static final int MAX_LABEL_LENGTH = 63;
    public static final String IDRTO_HASH_MARKER = "idrto-h1--";
    public static final int HASH_BODY_LENGTH = 50;
    public static final String STRUCTURAL_SEPARATOR = "--";
    public static final String STRUCTURAL_SEPARATOR_ESCAPED = "-2d-2d";

    private static final String BASE36_ALPHABET = "0123456789abcdefghijklmnopqrstuvwxyz";
    private static final char[] HEX_CHARS = "0123456789abcdef".toCharArray();
    private static final byte[] HEX_DIGITS = new byte[256];

    static {
        Arrays.fill(HEX_DIGITS, (byte) -1);
        for (int i = 0; i < 10; i++) {
            HEX_DIGITS['0' + i] = (byte) i;
        }
        for (int i = 0; i < 6; i++) {
            HEX_DIGITS['a' + i] = (byte) (10 + i);
        }
    }

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

    private static byte[] canonicalizeToBytes(String input) {
        byte[] raw = input.getBytes(StandardCharsets.UTF_8);
        byte[] bytes = new byte[raw.length];
        for (int i = 0; i < raw.length; i++) {
            byte value = raw[i];
            bytes[i] = (value >= 'A' && value <= 'Z') ? (byte) (value + ('a' - 'A')) : value;
        }
        return bytes;
    }

    private static String encodeBytes(byte[] bytes) {
        StringBuilder out = new StringBuilder(bytes.length * 2);
        for (byte value : bytes) {
            int unsigned = value & 0xff;
            if (isLiteralByte(unsigned)) {
                out.append((char) unsigned);
            } else {
                out.append('-');
                out.append(HEX_CHARS[unsigned >>> 4]);
                out.append(HEX_CHARS[unsigned & 0x0f]);
            }
        }
        return out.toString();
    }

    /** Encode one DEF component. */
    public static String encodeComponent(String input) {
        return encodeBytes(canonicalizeToBytes(input));
    }

    /** Encode input to a DEF body, preserving structural separators. */
    public static String encodeBody(String input) {
        StringBuilder out = new StringBuilder(input.length() * 2);
        int componentStart = 0;
        int separator;
        while ((separator = input.indexOf(STRUCTURAL_SEPARATOR, componentStart)) >= 0) {
            out.append(encodeComponent(input.substring(componentStart, separator)));
            out.append(STRUCTURAL_SEPARATOR);
            componentStart = separator + STRUCTURAL_SEPARATOR.length();
        }
        out.append(encodeComponent(input.substring(componentStart)));
        return out.toString();
    }

    private static int parseHexPair(int h1, int h2) {
        if (h1 < 0 || h1 >= 256 || h2 < 0 || h2 >= 256) {
            return -1;
        }
        int n1 = HEX_DIGITS[h1];
        int n2 = HEX_DIGITS[h2];
        if (n1 < 0 || n2 < 0) {
            return -1;
        }
        return n1 * 16 + n2;
    }

    /** Decode one DEF component. */
    public static String decodeComponent(String component) throws DefException {
        if (component.contains(STRUCTURAL_SEPARATOR)) {
            throw new DefException("raw structural separator in component", ErrorCode.INVALID_ESCAPE);
        }

        byte[] out = new byte[component.length()];
        int outLen = 0;

        for (int i = 0; i < component.length(); ) {
            char ch = component.charAt(i);
            if (isLiteralByte(ch)) {
                out[outLen++] = (byte) ch;
                i++;
                continue;
            }

            if (ch != '-') {
                throw new DefException("invalid character in encoded input", ErrorCode.INVALID_ESCAPE);
            }
            if (i + 3 > component.length()) {
                throw new DefException("truncated escape sequence", ErrorCode.INVALID_ESCAPE);
            }

            int value = parseHexPair(component.charAt(i + 1), component.charAt(i + 2));
            if (value < 0) {
                throw new DefException("invalid escape sequence", ErrorCode.INVALID_ESCAPE);
            }

            out[outLen++] = (byte) value;
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

    /** Decode a DEF body, preserving structural separators. */
    public static String decodeBody(String body) throws DefException {
        StringBuilder out = new StringBuilder(body.length());
        int componentStart = 0;
        int separator;
        while ((separator = body.indexOf(STRUCTURAL_SEPARATOR, componentStart)) >= 0) {
            out.append(decodeComponent(body.substring(componentStart, separator)));
            out.append(STRUCTURAL_SEPARATOR);
            componentStart = separator + STRUCTURAL_SEPARATOR.length();
        }
        out.append(decodeComponent(body.substring(componentStart)));
        return out.toString();
    }

    private static String base36(byte[] data) {
        BigInteger n = new BigInteger(1, data);
        if (n.signum() == 0) {
            return "0".repeat(HASH_BODY_LENGTH);
        }

        char[] digits = new char[HASH_BODY_LENGTH];
        BigInteger base = BigInteger.valueOf(36);
        for (int i = HASH_BODY_LENGTH - 1; i >= 0; i--) {
            BigInteger[] divRem = n.divideAndRemainder(base);
            digits[i] = BASE36_ALPHABET.charAt(divRem[1].intValue());
            n = divRem[0];
        }
        return new String(digits);
    }

    private static void validateMarker(String marker) throws DefException {
        if (marker.length() < 3
                || marker.length() > 13
                || !marker.endsWith(STRUCTURAL_SEPARATOR)
                || marker.startsWith("xn--")
                || !marker.chars().allMatch(ch ->
                        (ch >= 'a' && ch <= 'z')
                                || (ch >= '0' && ch <= '9')
                                || ch == '-')) {
            throw new DefException("invalid provider hash marker", ErrorCode.INVALID_ENCODING);
        }
    }

    private static boolean isBase36Char(int code) {
        return (code >= 0x30 && code <= 0x39) || (code >= 0x61 && code <= 0x7a);
    }

    /** Encode profile input with the configured hash marker. */
    public static String encodeProfile(String input) throws DefException {
        return encodeProfile(input, IDRTO_HASH_MARKER);
    }

    /** Encode profile input with the configured hash marker. */
    public static String encodeProfile(String input, String marker) throws DefException {
        validateMarker(marker);

        byte[] canonical = canonicalizeToBytes(input);
        String canonicalText = new String(canonical, StandardCharsets.UTF_8);
        String label = encodeBody(canonicalText);
        if (label.isEmpty() || label.startsWith("-") || label.endsWith("-")) {
            throw new DefException("invalid profile encoding", ErrorCode.INVALID_ENCODING);
        }

        if (label.length() <= MAX_LABEL_LENGTH && !label.startsWith(marker)) {
            return label;
        }

        String hashInput = encodeComponent(canonicalText);
        byte[] digest;
        try {
            MessageDigest md = MessageDigest.getInstance("SHA-256");
            digest = md.digest(hashInput.getBytes(StandardCharsets.UTF_8));
        } catch (NoSuchAlgorithmException ex) {
            throw new IllegalStateException("SHA-256 not available", ex);
        }

        String encoded = marker + base36(digest);
        if (encoded.length() > MAX_LABEL_LENGTH) {
            throw new DefException("encoded label exceeds 63 characters", ErrorCode.LABEL_TOO_LONG);
        }
        return encoded;
    }

    /** Decode a profile label with the configured hash marker. */
    public static String decodeProfile(String label) throws DefException {
        return decodeProfile(label, IDRTO_HASH_MARKER);
    }

    /** Decode a profile label with the configured hash marker. */
    public static String decodeProfile(String label, String marker) throws DefException {
        validateMarker(marker);

        if (label.startsWith(marker)) {
            String digest = label.substring(marker.length());
            if (digest.length() != HASH_BODY_LENGTH) {
                throw new DefException("invalid profile hash label", ErrorCode.INVALID_ENCODING);
            }
            for (int i = 0; i < digest.length(); i++) {
                if (!isBase36Char(digest.charAt(i))) {
                    throw new DefException("invalid profile hash label", ErrorCode.INVALID_ENCODING);
                }
            }
            throw new DefException("profile hash label is not decodable", ErrorCode.NOT_DECODABLE);
        }

        if (label.length() > MAX_LABEL_LENGTH) {
            throw new DefException("encoded label exceeds 63 characters", ErrorCode.LABEL_TOO_LONG);
        }

        if (label.isEmpty() || label.startsWith("-") || label.endsWith("-")) {
            throw new DefException("invalid profile encoding", ErrorCode.INVALID_ENCODING);
        }

        return decodeBody(label);
    }

    /** Encode input with the default profile. */
    public static String encode(String input) throws DefException {
        return encodeProfile(input, IDRTO_HASH_MARKER);
    }

    /** Decode input with the default profile. */
    public static String decode(String label) throws DefException {
        return decodeProfile(label, IDRTO_HASH_MARKER);
    }
}

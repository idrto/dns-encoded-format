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
            HEX_DIGITS['A' + i] = (byte) (10 + i);
        }
    }

    public enum ErrorCode {
        LABEL_TOO_LONG,
        INVALID_ESCAPE,
        INVALID_UTF8,
        INVALID_ENCODING,
        INVALID_LOCATOR,
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

    private static boolean isHostStart(int value) {
        return isLiteralByte(value);
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

    /** Encode input to a DEF body (§9). */
    public static String encodeBody(String input) {
        return encodeBytes(canonicalizeToBytes(input));
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

    /** Decode a DEF body (§10). */
    public static String decodeBody(String body) throws DefException {
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

            int value = parseHexPair(body.charAt(i + 1), body.charAt(i + 2));
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

    private static String[] splitLocator(String locator) throws DefException {
        int sep = locator.indexOf(STRUCTURAL_SEPARATOR);
        if (sep <= 0 || sep + 2 >= locator.length()) {
            throw new DefException("invalid profile locator", ErrorCode.INVALID_LOCATOR);
        }

        String host = locator.substring(0, sep);
        String entity = locator.substring(sep + 2);
        if (host.contains(STRUCTURAL_SEPARATOR) || entity.isEmpty()) {
            throw new DefException("invalid profile locator", ErrorCode.INVALID_LOCATOR);
        }

        byte[] hostBytes = host.getBytes(StandardCharsets.UTF_8);
        if (hostBytes.length == 0 || !isHostStart(hostBytes[0] & 0xff)) {
            throw new DefException("invalid profile host", ErrorCode.INVALID_LOCATOR);
        }

        return new String[] { host, entity };
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
                || marker.startsWith("xn--")) {
            throw new DefException("invalid provider hash marker", ErrorCode.INVALID_ENCODING);
        }
    }

    private static boolean isBase36Char(int code) {
        return (code >= 0x30 && code <= 0x39) || (code >= 0x61 && code <= 0x7a);
    }

    /** Encode a profile locator with the configured hash marker (§12). */
    public static String encodeProfile(String locator) throws DefException {
        return encodeProfile(locator, IDRTO_HASH_MARKER);
    }

    /** Encode a profile locator with the configured hash marker (§12). */
    public static String encodeProfile(String locator, String marker) throws DefException {
        validateMarker(marker);

        byte[] canonical = canonicalizeToBytes(locator);
        String canonicalText = new String(canonical, StandardCharsets.UTF_8);
        String[] parts = splitLocator(canonicalText);
        String host = parts[0];
        String entity = parts[1];

        String hostBody = encodeBytes(host.getBytes(StandardCharsets.UTF_8));
        String entityBody = encodeBytes(entity.getBytes(StandardCharsets.UTF_8));
        String label = hostBody + STRUCTURAL_SEPARATOR + entityBody;

        if (label.length() <= MAX_LABEL_LENGTH && !label.startsWith("xn--")) {
            return label;
        }

        String hashInput = hostBody + STRUCTURAL_SEPARATOR_ESCAPED + entityBody;
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

    /** Decode a profile label with the configured hash marker (§12). */
    public static String decodeProfile(String label) throws DefException {
        return decodeProfile(label, IDRTO_HASH_MARKER);
    }

    /** Decode a profile label with the configured hash marker (§12). */
    public static String decodeProfile(String label, String marker) throws DefException {
        validateMarker(marker);

        if (label.length() > MAX_LABEL_LENGTH) {
            throw new DefException("encoded label exceeds 63 characters", ErrorCode.LABEL_TOO_LONG);
        }

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

        if (label.startsWith("xn--")) {
            throw new DefException("invalid profile label", ErrorCode.INVALID_ENCODING);
        }

        int sep = label.indexOf(STRUCTURAL_SEPARATOR);
        if (sep <= 0 || sep + 2 > label.length()) {
            throw new DefException("missing profile separator", ErrorCode.INVALID_ENCODING);
        }

        String host = decodeBody(label.substring(0, sep));
        String entity = decodeBody(label.substring(sep + 2));

        if (host.isEmpty() || entity.isEmpty() || host.contains(STRUCTURAL_SEPARATOR)) {
            throw new DefException("invalid decoded profile locator", ErrorCode.INVALID_LOCATOR);
        }

        return host + STRUCTURAL_SEPARATOR + entity;
    }

    /** Encode an idr.to identity locator. */
    public static String encode(String locator) throws DefException {
        return encodeProfile(locator, IDRTO_HASH_MARKER);
    }

    /** Decode an idr.to profile label. */
    public static String decode(String label) throws DefException {
        return decodeProfile(label, IDRTO_HASH_MARKER);
    }
}

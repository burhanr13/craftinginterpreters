import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;
import java.util.Map;

public class Lexer {
    private String input;
    private int start;
    private int cur;
    private int line;
    private List<Token> output;

    private static Map<String, Token.Type> keywords;

    static {
        keywords = new HashMap<>();
        keywords.put("and", Token.Type.AND);
        keywords.put("break", Token.Type.BREAK);
        keywords.put("class", Token.Type.CLASS);
        keywords.put("continue", Token.Type.CONTINUE);
        keywords.put("else", Token.Type.ELSE);
        keywords.put("false", Token.Type.FALSE);
        keywords.put("for", Token.Type.FOR);
        keywords.put("fun", Token.Type.FUN);
        keywords.put("if", Token.Type.IF);
        keywords.put("nil", Token.Type.NIL);
        keywords.put("or", Token.Type.OR);
        keywords.put("print", Token.Type.PRINT);
        keywords.put("return", Token.Type.RETURN);
        keywords.put("super", Token.Type.SUPER);
        keywords.put("this", Token.Type.THIS);
        keywords.put("true", Token.Type.TRUE);
        keywords.put("var", Token.Type.VAR);
        keywords.put("while", Token.Type.WHILE);
    }

    public Lexer(String input) {
        this.input = input;
    }

    public void lex() {
        line = 1;
        start = 0;
        output = new ArrayList<Token>();

        while (start < input.length()) {
            scanToken();
        }

        addToken(Token.Type.EOF);
    }

    public List<Token> output() {
        return output;
    }

    void scanToken() {
        cur = start;
        char c = input.charAt(cur++);
        if (c == '\n') {
            line++;
        } else if (Character.isDigit(c)) {
            while (cur < input.length() && Character.isDigit(input.charAt(cur))) {
                cur++;
            }
            addToken(Token.Type.NUMBER, Double.parseDouble(input.substring(start, cur)));
        } else if (c == '_' || Character.isLetter(c)) {
            while (cur < input.length() &&
                    (Character.isLetterOrDigit(input.charAt(cur)) || input.charAt(cur) == '_')) {
                cur++;
            }
            String word = input.substring(start, cur);
            addToken(keywords.getOrDefault(word, Token.Type.IDENTIFIER), word);
        } else if (!Character.isWhitespace(c)) {
            switch (c) {
                case '(' -> addToken(Token.Type.LEFT_PAREN);
                case ')' -> addToken(Token.Type.RIGHT_PAREN);
                case '{' -> addToken(Token.Type.LEFT_BRACE);
                case '}' -> addToken(Token.Type.RIGHT_BRACE);
                case ',' -> addToken(Token.Type.COMMA);
                case '.' -> addToken(Token.Type.DOT);
                case '+' -> addToken(Token.Type.PLUS);
                case ';' -> addToken(Token.Type.SEMICOLON);
                case '*' -> addToken(Token.Type.STAR);
                case '?' -> addToken(Token.Type.QUESTION);
                case ':' -> addToken(Token.Type.COLON);
                case '%' -> addToken(Token.Type.PERCENT);
                case '-' -> {
                    if (cur < input.length() && input.charAt(cur) == '>') {
                        cur++;
                        addToken(Token.Type.ARROW);
                    } else {
                        addToken(Token.Type.MINUS);
                    }
                }
                case '!' -> {
                    if (cur < input.length() && input.charAt(cur) == '=') {
                        cur++;
                        addToken(Token.Type.NOT_EQUAL);
                    } else {
                        addToken(Token.Type.NOT);
                    }
                }
                case '=' -> {
                    if (cur < input.length() && input.charAt(cur) == '=') {
                        cur++;
                        addToken(Token.Type.EQUAL_EQUAL);
                    } else {
                        addToken(Token.Type.EQUAL);
                    }
                }
                case '<' -> {
                    if (cur < input.length() && input.charAt(cur) == '=') {
                        cur++;
                        addToken(Token.Type.LESS_EQUAL);
                    } else {
                        addToken(Token.Type.LESS);
                    }
                }
                case '>' -> {
                    if (cur < input.length() && input.charAt(cur) == '=') {
                        cur++;
                        addToken(Token.Type.GREATER_EQUAL);
                    } else {
                        addToken(Token.Type.GREATER);
                    }
                }
                case '/' -> {
                    if (cur < input.length()) {
                        if (input.charAt(cur) == '/') {
                            cur++;
                            while (cur < input.length() && input.charAt(cur++) != '\n') {
                            }
                            if (cur < input.length())
                                cur--;
                        } else if (input.charAt(cur) == '*') {
                            cur++;
                            while (true) {
                                if (cur < input.length() - 1) {
                                    if (input.charAt(cur) == '\n')
                                        line++;
                                    if (input.charAt(cur++) == '*') {
                                        if (input.charAt(cur++) == '/') {
                                            break;
                                        }
                                    }
                                } else {
                                    Lox.error(line, "Unterminated block comment.");
                                    break;
                                }
                            }
                        } else {
                            addToken(Token.Type.SLASH);
                        }
                    } else {
                        addToken(Token.Type.SLASH);
                    }
                }
                case '"' -> {
                    boolean terminated = false;
                    while (cur < input.length() && input.charAt(cur) != '\n') {
                        if (input.charAt(cur++) == '"') {
                            terminated = true;
                            break;
                        }
                    }
                    if (terminated) {
                        String s = input.substring(start + 1, cur - 1);
                        addToken(Token.Type.STRING, s);
                    } else {
                        Lox.error(line, "Unterminated string literal.");
                    }
                }
                default -> Lox.error(line, "Invalid Token.");
            }
        }
        start = cur;
    }

    void addToken(Token.Type t) {
        addToken(t, null);
    }

    void addToken(Token.Type t, Object val) {
        output.add(new Token(t, input.substring(start, cur), val, line));
    }

}

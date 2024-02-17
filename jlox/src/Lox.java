public class Lox {

    static boolean error = false;

    private static Interpreter interpreter;

    static {
        interpreter = new Interpreter();
    }

    static void error(int line, String message) {
        error = true;
        System.err.printf("Error line %d: %s\n", line, message);
    }

    static void run(String input) {
        error = false;
        var lexer = new Lexer(input);
        lexer.lex();
        if (error)
            return;
        var parser = new Parser(lexer.output());
        parser.parse();
        if (error)
            return;
        interpreter.setInput(parser.output());
        interpreter.run();
    }
}

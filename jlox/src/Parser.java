import java.util.ArrayList;
import java.util.HashMap;
import java.util.HashSet;
import java.util.List;
import java.util.Map;
import java.util.Set;
import java.util.function.Supplier;

public class Parser {

    private List<Token> input;
    private int cur;
    private List<Stmt> output;

    private boolean error;

    private int loopDepth;
    private List<Integer> loopStk;
    private int funDepth;
    private int classDepth;

    private static List<Set<Token.Type>> binaryOps;
    private static Set<Token.Type> unaryOps;

    private Map<Token.Type, Supplier<Stmt>> decls;
    private Map<Token.Type, Supplier<Stmt>> stmts;

    static {
        binaryOps = new ArrayList<>();

        binaryOps.add(new HashSet<>());
        binaryOps.getLast().add(Token.Type.EQUAL_EQUAL);
        binaryOps.getLast().add(Token.Type.NOT_EQUAL);

        binaryOps.add(new HashSet<>());
        binaryOps.getLast().add(Token.Type.LESS_EQUAL);
        binaryOps.getLast().add(Token.Type.GREATER_EQUAL);
        binaryOps.getLast().add(Token.Type.LESS);
        binaryOps.getLast().add(Token.Type.GREATER);

        binaryOps.add(new HashSet<>());
        binaryOps.getLast().add(Token.Type.PLUS);
        binaryOps.getLast().add(Token.Type.MINUS);

        binaryOps.add(new HashSet<>());
        binaryOps.getLast().add(Token.Type.STAR);
        binaryOps.getLast().add(Token.Type.SLASH);
        binaryOps.getLast().add(Token.Type.PERCENT);

        unaryOps = new HashSet<>();
        unaryOps.add(Token.Type.MINUS);
        unaryOps.add(Token.Type.NOT);
    }

    public Parser(List<Token> input) {
        this.input = input;

        decls = new HashMap<>();
        decls.put(Token.Type.VAR, this::parseVarDecl);
        decls.put(Token.Type.FUN, this::parseFunDecl);
        decls.put(Token.Type.CLASS, this::parseClassDecl);

        stmts = new HashMap<>();
        stmts.put(Token.Type.EOF, this::parseEOF);
        stmts.put(Token.Type.LEFT_BRACE, this::parseBlockStmt);
        stmts.put(Token.Type.PRINT, this::parsePrintStmt);
        stmts.put(Token.Type.IF, this::parseIfStmt);
        stmts.put(Token.Type.WHILE, this::parseWhileStmt);
        stmts.put(Token.Type.FOR, this::parseForStmt);
        stmts.put(Token.Type.BREAK, this::parseJumpStmt);
        stmts.put(Token.Type.CONTINUE, this::parseJumpStmt);
        stmts.put(Token.Type.RETURN, this::parseReturnStmt);
        stmts.put(Token.Type.SEMICOLON, this::parseEmptyStmt);
    }

    public void parse() {
        error = false;
        loopDepth = 0;
        loopStk = new ArrayList<>();
        funDepth = 0;
        classDepth = 0;
        cur = 0;
        output = new ArrayList<>();

        while (true) {
            Stmt s = parseStmtOrDecl();
            if (cur >= input.size())
                break;
            if (error) {
                error = false;
                synchronize();
            } else {
                output.add(s);
            }
        }
    }

    public List<Stmt> output() {
        return output;
    }

    void parseError(String message) {
        if (!error) {
            back();
            Lox.error(top().line(), message);
            error = true;
        }
    }

    Token next() {
        return input.get(cur++);
    }

    Token top() {
        return input.get(cur - 1);
    }

    void back() {
        cur--;
    }

    Expr parseExpr() {
        return parseCommaExpr();
    }

    Expr parseCommaExpr() {
        Expr e = parseAssignExpr();
        if (next().type() == Token.Type.COMMA) {
            return new Expr.Binary(e, top(), parseCommaExpr());
        }
        back();
        return e;
    }

    Expr parseAssignExpr() {
        Expr l = parseTernaryExpr();
        if (next().type() == Token.Type.EQUAL) {
            Expr r = parseAssignExpr();
            switch (l) {
                case Expr.Identifier ll -> {
                    return new Expr.Assign(ll.id(), r);
                }
                case Expr.Dot ll -> {
                    return new Expr.DotAssign(ll.l(), ll.id(), r);
                }
                default -> {
                    parseError("Invalid assign lvalue.");
                    return null;
                }
            }
        }
        back();
        return l;
    }

    Expr parseTernaryExpr() {
        Expr c = parseOrExpr();
        if (next().type() != Token.Type.QUESTION) {
            back();
            return c;
        }
        Expr t = parseTernaryExpr();
        if (next().type() != Token.Type.COLON) {
            parseError("Expected colon.");
            return null;
        }
        Expr f = parseTernaryExpr();
        return new Expr.Ternary(c, t, f);
    }

    Expr parseOrExpr() {
        Expr e = parseAndExpr();
        if (next().type() == Token.Type.OR) {
            return new Expr.Logical(e, top(), parseOrExpr());
        }
        back();
        return e;
    }

    Expr parseAndExpr() {
        Expr e = parseBinExpr(0);
        if (next().type() == Token.Type.AND) {
            return new Expr.Logical(e, top(), parseAndExpr());
        }
        back();
        return e;
    }

    Expr parseBinExpr(int precedence) {
        boolean endOfBinary = precedence + 1 == binaryOps.size();
        Expr e = endOfBinary ? parseUnaryExpr() : parseBinExpr(precedence + 1);
        while (binaryOps.get(precedence).contains(next().type())) {
            Token op = top();
            Expr r = endOfBinary ? parseUnaryExpr() : parseBinExpr(precedence + 1);
            e = new Expr.Binary(e, op, r);
        }
        back();
        return e;
    }

    Expr parseUnaryExpr() {
        if (unaryOps.contains(next().type())) {
            return new Expr.Unary(top(), parseUnaryExpr());
        } else {
            back();
            return parseCallExpr();
        }
    }

    Expr parseCallExpr() {
        Expr f = parsePrimaryExpr();
        while (true) {
            boolean doneOut = false;
            switch (next().type()) {
                case Token.Type.LEFT_PAREN -> {
                    var op = top();
                    var args = new ArrayList<Expr>();
                    if (next().type() == Token.Type.RIGHT_PAREN) {
                        f = new Expr.Call(f, op, args);
                        continue;
                    }
                    back();
                    while (true) {
                        if (args.size() >= 255) {
                            Lox.error(top().line(), "Too many arguments in call.");
                            break;
                        }
                        args.add(parseAssignExpr());
                        boolean done = false;
                        switch (next().type()) {
                            case Token.Type.RIGHT_PAREN -> {
                                done = true;
                                f = new Expr.Call(f, op, args);
                            }
                            case Token.Type.COMMA -> {
                            }
                            default -> {
                                parseError("Expected comma or closing parens after argument.");
                                return null;
                            }
                        }
                        if (done)
                            break;
                    }
                }
                case Token.Type.DOT -> {
                    if (next().type() != Token.Type.IDENTIFIER) {
                        parseError("Expected identifer.");
                        return null;
                    }
                    f = new Expr.Dot(f, top());
                }
                default -> {
                    doneOut = true;
                }
            }
            if (doneOut)
                break;
        }
        back();
        return f;
    }

    Expr parsePrimaryExpr() {
        switch (next().type()) {
            case Token.Type.LEFT_PAREN -> {
                Expr e = parseExpr();
                if (next().type() == Token.Type.RIGHT_PAREN) {
                    return e;
                } else {
                    parseError("Unclosed parenthesis.");
                    return null;
                }
            }
            case Token.Type.NUMBER, Token.Type.STRING -> {
                return new Expr.Value(top().value());
            }
            case Token.Type.TRUE -> {
                return new Expr.Value(true);
            }
            case Token.Type.FALSE -> {
                return new Expr.Value(false);
            }
            case NIL -> {
                return new Expr.Value(null);
            }
            case Token.Type.IDENTIFIER -> {
                return new Expr.Identifier(top());
            }
            case Token.Type.THIS -> {
                if (classDepth > 0) {
                    return new Expr.Identifier(top());
                } else {
                    parseError("Cannot use 'this' outside method.");
                    return null;
                }
            }
            case Token.Type.FUN -> {
                var params = parseParams();
                Stmt s;
                switch (next().type()) {
                    case Token.Type.LEFT_BRACE -> {
                        funDepth++;
                        loopStk.add(loopDepth);
                        loopDepth = 0;
                        s = parseBlockStmt();
                        loopDepth = loopStk.removeLast();
                        funDepth--;
                    }
                    case Token.Type.ARROW -> {
                        s = new Stmt.Jump(new Token(Token.Type.RETURN, "", null, top().line()), parseAssignExpr());
                    }
                    default -> {
                        parseError("Expected open brace or arrow for lambda function.");
                        return null;
                    }
                }

                return new Expr.Fun(params, s);
            }
            default -> {
                parseError("Unexpected token '" + top().word() + "'.");
                return null;
            }
        }
    }

    Stmt parseStmtOrDecl() {
        var f = decls.getOrDefault(next().type(), null);
        if (f != null) {
            return f.get();
        }
        back();
        return parseStmt();
    }

    Stmt parseStmt() {
        var f = stmts.getOrDefault(next().type(), null);
        if (f != null) {
            return f.get();
        }
        back();
        return parseExprStmt();
    }

    Stmt parseEOF() {
        return null;
    }

    Stmt parseBlockStmt() {
        var sl = new ArrayList<Stmt>();
        while (next().type() != Token.Type.RIGHT_BRACE) {
            back();
            Stmt si = parseStmtOrDecl();
            if (si != null) {
                sl.add(si);
            } else {
                parseError("Unclosed curly brace.");
                return null;
            }
        }
        return new Stmt.Block(sl);
    }

    Stmt parseExprStmt() {
        Expr e = parseExpr();
        if (e == null)
            return null;
        Stmt s = new Stmt.ExprStmt(e);
        if (next().type() != Token.Type.SEMICOLON) {
            parseError("Expected semicolon.");
            return null;
        }
        return s;
    }

    Stmt parsePrintStmt() {
        Stmt s = new Stmt.Print(parseExpr());
        if (next().type() != Token.Type.SEMICOLON) {
            parseError("Expected semicolon.");
            return null;
        }
        return s;
    }

    Stmt parseIfStmt() {
        if (next().type() != Token.Type.LEFT_PAREN) {
            parseError("Expected parenthesis.");
            return null;
        }
        Expr e = parseExpr();
        if (next().type() != Token.Type.RIGHT_PAREN) {
            parseError("Unclosed parenthesis.");
            return null;
        }
        Stmt t = parseStmt();
        if (next().type() != Token.Type.ELSE) {
            back();
            return new Stmt.If(e, t, null);
        }
        Stmt f = parseStmt();
        return new Stmt.If(e, t, f);
    }

    Stmt parseWhileStmt() {
        if (next().type() != Token.Type.LEFT_PAREN) {
            parseError("Expected parenthesis.");
            return null;
        }
        Expr e = parseExpr();
        if (next().type() != Token.Type.RIGHT_PAREN) {
            parseError("Unclosed parenthesis.");
            return null;
        }
        loopDepth++;
        Stmt b = parseStmt();
        loopDepth--;
        return new Stmt.While(e, b);
    }

    Stmt parseForStmt() {
        if (next().type() != Token.Type.LEFT_PAREN) {
            parseError("Expected parenthesis.");
            return null;
        }

        Stmt a;
        switch (next().type()) {
            case Token.Type.SEMICOLON -> a = null;
            case Token.Type.VAR -> a = parseVarDecl();
            default -> {
                back();
                a = parseExprStmt();
            }
        }
        Expr c;
        switch (next().type()) {
            case Token.Type.SEMICOLON -> c = null;
            default -> {
                back();
                c = parseExpr();
                if (next().type() != Token.Type.SEMICOLON) {
                    parseError("Expected semicolon.");
                    return null;
                }
            }
        }
        Expr i;
        switch (next().type()) {
            case Token.Type.RIGHT_PAREN -> i = null;
            default -> {
                back();
                i = parseExpr();
                if (next().type() != Token.Type.RIGHT_PAREN) {
                    parseError("Unclosed parenthesis.");
                    return null;
                }
            }
        }
        loopDepth++;
        Stmt b = parseStmt();
        loopDepth--;
        return new Stmt.For(a, c, i, b);
    }

    Stmt parseJumpStmt() {
        if (loopDepth == 0) {
            Lox.error(top().line(), "'" + top().word() + "' must be inside a loop.");
        }
        Token t = top();
        if (next().type() != Token.Type.SEMICOLON) {
            parseError("Expected semicolon.");
            return null;
        }
        return new Stmt.Jump(t, null);
    }

    Stmt parseReturnStmt() {
        if (funDepth == 0) {
            Lox.error(top().line(), "'return' must be inside a function.");
        }
        Token t = top();
        Expr e = null;
        if (next().type() != Token.Type.SEMICOLON) {
            back();
            e = parseExpr();
            if (next().type() != Token.Type.SEMICOLON) {
                parseError("Expected semicolon.");
                return null;
            }
        }
        return new Stmt.Jump(t, e);
    }

    Stmt parseEmptyStmt() {
        return new Stmt.Empty();
    }

    Stmt parseVarDecl() {
        if (next().type() != Token.Type.IDENTIFIER) {
            parseError("Expected identifier.");
            return null;
        }
        Token id = top();
        switch (next().type()) {
            case Token.Type.SEMICOLON -> {
                return new Stmt.VarDecl(id, null);
            }
            case Token.Type.EQUAL -> {
                Expr e = parseExpr();
                if (next().type() != Token.Type.SEMICOLON) {
                    parseError("Expected semicolon.");
                }
                return new Stmt.VarDecl(id, e);
            }
            default -> {
                parseError("Expected semicolon or initializer.");
                return null;
            }
        }
    }

    List<Token> parseParams() {
        if (next().type() != Token.Type.LEFT_PAREN) {
            parseError("Expected parenthesis for function parameters.");
            return null;
        }
        var params = new ArrayList<Token>();
        if (next().type() != Token.Type.RIGHT_PAREN) {
            back();
            while (true) {
                if (params.size() >= 255) {
                    Lox.error(top().line(), "Too many parameters in function declaration.");
                    break;
                }
                if (next().type() != Token.Type.IDENTIFIER) {
                    parseError("Expected identifier for parameters.");
                    return null;
                }
                params.add(top());
                boolean done = false;
                switch (next().type()) {
                    case Token.Type.RIGHT_PAREN -> {
                        done = true;
                    }
                    case Token.Type.COMMA -> {
                    }
                    default -> {
                        parseError("Expected comma or closing parens after parameter.");
                        return null;
                    }
                }
                if (done)
                    break;
            }
        }
        return params;
    }

    Stmt parseFunDecl() {
        if (next().type() != Token.Type.IDENTIFIER) {
            parseError("Expected identifier.");
            return null;
        }
        Token id = top();
        var params = parseParams();
        if (next().type() != Token.Type.LEFT_BRACE) {
            parseError("Expected opening brace.");
            return null;
        }
        funDepth++;
        loopStk.add(loopDepth);
        loopDepth = 0;
        var s = parseBlockStmt();
        loopDepth = loopStk.removeLast();
        funDepth--;
        return new Stmt.FunDecl(id, params, s);
    }

    Stmt parseClassDecl() {
        if (next().type() != Token.Type.IDENTIFIER) {
            parseError("Expected identifier.");
            return null;
        }
        Token id = top();
        if (next().type() != Token.Type.LEFT_BRACE) {
            parseError("Expected opening brace.");
            return null;
        }
        var ms = new ArrayList<Stmt>();
        var cms = new ArrayList<Stmt>();
        classDepth++;
        while (next().type() != Token.Type.RIGHT_BRACE) {
            back();

            boolean classm = false;
            if (next().type() == Token.Type.CLASS) {
                classm = true;
            } else {
                back();
            }
            Stmt m = parseFunDecl();
            if (m != null) {
                if (classm) {
                    cms.add(m);
                } else {
                    ms.add(m);
                }
            } else {
                parseError("Unclosed curly brace.");
                return null;
            }
        }
        classDepth--;
        return new Stmt.ClassDecl(id, ms, cms);
    }

    void synchronize() {
        while (true) {
            var t = next();
            if (t.type() == Token.Type.SEMICOLON) {
                break;
            }
            if (stmts.containsKey(t.type()) || decls.containsKey(t.type())) {
                back();
                break;
            }
        }
    }

}

import java.util.List;

public sealed interface Expr {
    record Assign(Token id, Expr e) implements Expr {
    }

    record DotAssign(Expr l, Token id, Expr e) implements Expr {
    }

    record Ternary(Expr c, Expr t, Expr f) implements Expr {
    }

    record Logical(Expr l, Token op, Expr r) implements Expr {
    }

    record Binary(Expr l, Token op, Expr r) implements Expr {
    }

    record Unary(Token op, Expr r) implements Expr {
    }

    record Call(Expr f, Token op, List<Expr> args) implements Expr {
    }

    record Dot(Expr l, Token id) implements Expr {
    }

    record Value(Object val) implements Expr {
    }

    record Fun(List<Token> ps, Stmt b) implements Expr {
    }

    record Identifier(Token id) implements Expr {
    }
}
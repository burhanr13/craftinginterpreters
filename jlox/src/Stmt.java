import java.util.List;

public sealed interface Stmt {
    record Block(List<Stmt> sl) implements Stmt {
    }

    record VarDecl(Token id, Expr e) implements Stmt {
    }

    record FunDecl(Token id, List<Token> ps, Stmt b) implements Stmt {
    }

    record ClassDecl(Token id, List<Stmt> ms, List<Stmt> cms) implements Stmt {
    }

    record ExprStmt(Expr e) implements Stmt {
    }

    record Print(Expr e) implements Stmt {
    }

    record If(Expr c, Stmt t, Stmt f) implements Stmt {
    }

    record While(Expr c, Stmt b) implements Stmt {
    }

    record For(Stmt a, Expr c, Expr i, Stmt b) implements Stmt {
    }

    record Jump(Token t, Expr v) implements Stmt {
    }

    record Empty() implements Stmt {
    }
}

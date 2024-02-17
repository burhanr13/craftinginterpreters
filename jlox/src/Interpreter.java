import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;
import java.util.Map;
import java.util.function.BiFunction;
import java.util.function.Function;

public class Interpreter {

    static record LoxFunction(List<Token> params, Stmt body, Environment closure, boolean isInit) {
    }

    static record LoxBuiltin(Function<List<Object>, Object> f) {
    }

    static record LoxClass(String name, Map<String, LoxFunction> methods, Map<String, LoxFunction> classMethods) {
    }

    static record LoxObject(LoxClass c, Map<String, Object> fields) {
    }

    private List<Stmt> input;

    private Environment state;
    private List<Environment> callStack;

    private Object retVal;

    private boolean error;

    private static Map<Token.Type, BiFunction<Double, Double, Object>> numBinOps;

    static {
        numBinOps = new HashMap<>();
        numBinOps.put(Token.Type.GREATER, (a, b) -> a > b);
        numBinOps.put(Token.Type.LESS, (a, b) -> a < b);
        numBinOps.put(Token.Type.GREATER_EQUAL, (a, b) -> a >= b);
        numBinOps.put(Token.Type.LESS_EQUAL, (a, b) -> a <= b);
        numBinOps.put(Token.Type.PLUS, (a, b) -> a + b);
        numBinOps.put(Token.Type.MINUS, (a, b) -> a - b);
        numBinOps.put(Token.Type.STAR, (a, b) -> a * b);
        numBinOps.put(Token.Type.SLASH, (a, b) -> a / b);
        numBinOps.put(Token.Type.PERCENT, (a, b) -> a % b);
    }

    public Interpreter(List<Stmt> input) {
        this.input = input;

        state = new Environment();

        state.addVar("clock", new LoxBuiltin((l) -> {
            return (double) System.currentTimeMillis() / 1000;
        }));

        state.addVar("random", new LoxBuiltin((l) -> {
            return Math.random();
        }));

        callStack = new ArrayList<>();
    }

    public Interpreter() {
        this(null);
    }

    public void setInput(List<Stmt> input) {
        this.input = input;
    }

    public void run() {
        error = false;
        for (var s : input) {
            runStmt(s);
            if (error)
                break;
        }
    }

    void enterScope() {
        state = new Environment(state);
    }

    void leaveScope() {
        state = state.getParent();
    }

    boolean isTruthy(Object o) {
        if (o == null)
            return false;
        switch (o) {
            case Boolean b -> {
                return b;
            }
            default -> {
                return true;
            }
        }
    }

    String toLoxString(Object o) {
        if (o == null)
            return "nil";
        switch (o) {
            case Double d -> {
                String s = d.toString();
                if (s.substring(s.length() - 2).equals(".0"))
                    s = s.substring(0, s.length() - 2);
                return s;
            }
            case LoxBuiltin b -> {
                return "<Lox Builtin>";
            }
            case LoxFunction f -> {
                return "<function>";
            }
            case LoxClass c -> {
                return "<class " + c.name() + ">";
            }
            case LoxObject oo -> {
                return "<instance " + oo.c().name() + ">";
            }
            default -> {
                return o.toString();
            }
        }
    }

    Object evalExpr(Expr e) {
        if (e == null)
            return null;
        switch (e) {
            case Expr.Assign ee -> {
                if (state.hasVar(ee.id().word())) {
                    state.setVar(ee.id().word(), evalExpr(ee.e()));
                    return state.getVar(ee.id().word());
                } else {
                    Lox.error(ee.id().line(), "Undefined variable '" + ee.id().word() + "'.");
                    error = true;
                    return null;
                }
            }
            case Expr.DotAssign ee -> {
                switch (evalExpr(ee.l())) {
                    case LoxObject o -> {
                        o.fields.put(ee.id().word(), evalExpr(ee.e()));
                        return o.fields.get(ee.id().word());
                    }
                    default -> {
                        Lox.error(ee.id().line(), "Only access fields of objects.");
                        error = true;
                        return null;
                    }
                }
            }
            case Expr.Ternary ee -> {
                return isTruthy(evalExpr(ee.c())) ? evalExpr(ee.t())
                        : evalExpr(ee.f());
            }
            case Expr.Logical ee -> {
                Object o1 = evalExpr(ee.l());
                switch (ee.op().type()) {
                    case Token.Type.AND -> {
                        if (!isTruthy(o1))
                            return o1;
                    }
                    case Token.Type.OR -> {
                        if (isTruthy(o1))
                            return o1;
                    }
                    default -> {
                        return null;
                    }
                }
                return evalExpr(ee.r());
            }
            case Expr.Binary ee -> {
                Object o1 = evalExpr(ee.l());
                Object o2 = evalExpr(ee.r());
                switch (ee.op().type()) {
                    case Token.Type.COMMA -> {
                        return o2;
                    }
                    case Token.Type.EQUAL_EQUAL -> {
                        if (o1 == null) {
                            if (o2 == null) {
                                return true;
                            } else {
                                return false;
                            }
                        } else {
                            return o1.equals(o2);
                        }
                    }
                    case Token.Type.NOT_EQUAL -> {
                        if (o1 == null) {
                            if (o2 == null) {
                                return false;
                            } else {
                                return true;
                            }
                        } else {
                            return !o1.equals(o2);
                        }
                    }
                    default -> {
                        if (o1 == null)
                            return null;
                        switch (o1) {
                            case Double d1 -> {
                                switch (o2) {
                                    case Double d2 -> {
                                        var f = numBinOps.getOrDefault(ee.op().type(), null);
                                        if (f != null) {
                                            return f.apply(d1, d2);
                                        } else {
                                            return null;
                                        }
                                    }
                                    default -> {
                                        Lox.error(ee.op().line(),
                                                "Invalid types for operation '" + ee.op().word() + "'.");
                                        error = true;
                                        return null;
                                    }
                                }
                            }
                            case String s1 -> {
                                if (ee.op().type() == Token.Type.PLUS) {
                                    return s1 + toLoxString(o2);
                                }
                                Lox.error(ee.op().line(),
                                        "Invalid types for operation '" + ee.op().word() + "'.");
                                error = true;
                                return null;
                            }
                            default -> {
                                Lox.error(ee.op().line(),
                                        "Invalid types for operation '" + ee.op().word() + "'.");
                                error = true;
                                return null;
                            }
                        }
                    }
                }
            }
            case Expr.Unary ee -> {
                switch (ee.op().type()) {
                    case Token.Type.MINUS -> {
                        switch (evalExpr(ee.r())) {
                            case Double d -> {
                                return -d.doubleValue();
                            }
                            default -> {
                                Lox.error(ee.op().line(),
                                        "Invalid types for operation '" + ee.op().word() + "'.");
                                error = true;
                                return null;
                            }
                        }
                    }
                    case Token.Type.NOT -> {
                        return !isTruthy(evalExpr(ee.r()));
                    }
                    default -> {
                        return null;
                    }
                }
            }
            case Expr.Call ee -> {
                var f = evalExpr(ee.f());
                if (f == null)
                    return null;
                var args = new ArrayList<Object>();
                for (var a : ee.args()) {
                    args.add(evalExpr(a));
                }
                switch (f) {
                    case LoxBuiltin b -> {
                        return b.f.apply(args);
                    }
                    case LoxFunction ff -> {
                        if (args.size() != ff.params().size()) {
                            Lox.error(ee.op().line(), "Incorrect argument count for function.");
                            error = true;
                            return null;
                        }

                        callStack.add(state);
                        state = ff.closure();
                        enterScope();
                        for (int i = 0; i < ff.params.size(); i++) {
                            state.addVar(ff.params().get(i).word(), args.get(i));
                        }
                        var r = runStmt(ff.body());
                        state = callStack.removeLast();
                        if (ff.isInit()) {
                            return ff.closure().getVar("this");
                        }
                        if (r == null)
                            return null;
                        return retVal;
                    }
                    case LoxClass c -> {
                        var o = new LoxObject(c, new HashMap<>());
                        var init = c.methods.getOrDefault("init", null);
                        if (init != null) {
                            if (ee.args().size() != init.params().size()) {
                                Lox.error(ee.op().line(), "Incorrect argument count for constructor.");
                                error = true;
                                return null;
                            }
                            callStack.add(state);
                            state = init.closure();
                            enterScope();
                            state.addVar("this", o);
                            for (int i = 0; i < init.params.size(); i++) {
                                state.addVar(init.params().get(i).word(), args.get(i));
                            }
                            runStmt(init.body());
                            state = callStack.removeLast();
                        }
                        return o;
                    }
                    default -> {
                        Lox.error(ee.op().line(), "Invalid callee.");
                        return null;
                    }
                }
            }
            case Expr.Dot ee -> {
                switch (evalExpr(ee.l())) {
                    case LoxClass c -> {
                        var f = c.classMethods().getOrDefault(ee.id().word(), null);
                        if (f != null) {
                            return f;
                        } else {
                            Lox.error(ee.id().line(), "Unknown class function.");
                            error = true;
                            return null;
                        }
                    }
                    case LoxObject o -> {
                        if (o.fields.containsKey(ee.id().word())) {
                            return o.fields().get(ee.id().word());
                        } else if (o.c().methods().containsKey(ee.id().word())) {
                            var m = o.c.methods.get(ee.id().word());
                            m = new LoxFunction(m.params(), m.body(), new Environment(m.closure()), m.isInit());
                            m.closure.addVar("this", o);
                            return m;
                        } else if (o.c().classMethods().containsKey(ee.id().word())) {
                            return o.c().classMethods().get(ee.id().word());
                        } else {
                            Lox.error(ee.id().line(), "Uninitialized field.");
                            error = true;
                            return null;
                        }
                    }
                    default -> {
                        Lox.error(ee.id().line(), "Only access fields of objects.");
                        error = true;
                        return null;
                    }
                }
            }
            case Expr.Value ee -> {
                return ee.val();
            }
            case Expr.Fun ee -> {
                return new LoxFunction(ee.ps(), ee.b(), state, false);
            }
            case Expr.Identifier ee -> {
                if (state.hasVar(ee.id().word())) {
                    return state.getVar(ee.id().word());
                } else {
                    Lox.error(ee.id().line(), "Undefined variable '" + ee.id().word() + "'.");
                    error = true;
                    return null;
                }
            }
        }
    }

    Stmt.Jump runStmt(Stmt s) {
        if (s == null)
            return null;
        switch (s) {
            case Stmt.Block ss -> {
                enterScope();
                for (var si : ss.sl()) {
                    var j = runStmt(si);
                    if (j != null) {
                        leaveScope();
                        return j;
                    }
                    if (error)
                        break;
                }
                leaveScope();
            }
            case Stmt.Print ss -> {
                System.out.println(toLoxString(evalExpr(ss.e())));
            }
            case Stmt.ExprStmt ss -> {
                evalExpr(ss.e());
            }
            case Stmt.VarDecl ss -> {
                state.addVar(ss.id().word(), evalExpr(ss.e()));
            }
            case Stmt.FunDecl ss -> {
                state.addVar(ss.id().word(), new LoxFunction(ss.ps(), ss.b(), state, false));
            }
            case Stmt.ClassDecl ss -> {
                var methods = new HashMap<String, LoxFunction>();
                for (var f : ss.ms()) {
                    Stmt.FunDecl ff = (Stmt.FunDecl) f;
                    methods.put(ff.id().word(), new LoxFunction(ff.ps(), ff.b(), state, ff.id().word().equals("init")));
                }
                var classMethods = new HashMap<String, LoxFunction>();
                for (var f : ss.cms()) {
                    Stmt.FunDecl ff = (Stmt.FunDecl) f;
                    classMethods.put(ff.id().word(),
                            new LoxFunction(ff.ps(), ff.b(), state, ff.id().word().equals("init")));
                }
                state.addVar(ss.id().word(), new LoxClass(ss.id().word(), methods, classMethods));
            }
            case Stmt.If ss -> {
                if (isTruthy(evalExpr(ss.c()))) {
                    return runStmt(ss.t());
                } else {
                    return runStmt(ss.f());
                }
            }
            case Stmt.While ss -> {
                while (isTruthy(evalExpr(ss.c()))) {
                    var j = runStmt(ss.b());
                    if (j != null) {
                        if (j.t().type() == Token.Type.BREAK)
                            break;
                        if (j.t().type() == Token.Type.CONTINUE)
                            continue;
                        leaveScope();
                        return j;
                    }
                    if (error)
                        break;
                }
            }
            case Stmt.For ss -> {
                enterScope();
                for (runStmt(ss.a()); ss.c() == null || isTruthy(evalExpr(ss.c())); evalExpr(ss.i())) {
                    var j = runStmt(ss.b());
                    if (j != null) {
                        if (j.t().type() == Token.Type.BREAK)
                            break;
                        if (j.t().type() == Token.Type.CONTINUE)
                            continue;
                        leaveScope();
                        return j;
                    }
                    if (error)
                        break;
                }
                leaveScope();
            }
            case Stmt.Jump ss -> {
                retVal = evalExpr(ss.v());
                return ss;
            }
            case Stmt.Empty ss -> {
            }
        }
        return null;
    }

}

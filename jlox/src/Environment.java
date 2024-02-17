import java.util.HashMap;
import java.util.Map;

public class Environment {

    private Environment parent;
    private Map<String, Object> vars;

    public Environment(Environment parent) {
        this.parent = parent;
        vars = new HashMap<>();
    }

    public Environment() {
        this(null);
    }

    public Environment getParent() {
        return parent;
    }

    public void addVar(String id, Object val) {
        vars.put(id, val);
    }

    public void setVar(String id, Object val) {
        if (vars.containsKey(id)) {
            vars.put(id, val);
        } else if (parent != null) {
            parent.setVar(id, val);
        }
    }

    public Object getVar(String id) {
        if (vars.containsKey(id)) {
            return vars.get(id);
        } else if (parent != null) {
            return parent.getVar(id);
        }
        return null;
    }

    public boolean hasVar(String id) {
        if (vars.containsKey(id)) {
            return true;
        } else if (parent != null) {
            return parent.hasVar(id);
        }
        return false;
    }
}

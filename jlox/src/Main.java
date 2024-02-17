import java.io.BufferedReader;
import java.io.IOException;
import java.io.InputStreamReader;
import java.nio.file.Files;
import java.nio.file.Path;

public class Main {
    public static void main(String[] args) throws IOException {
        if (args.length < 1) {
            System.out.println("Jlox shell");
            BufferedReader br = new BufferedReader(new InputStreamReader(System.in));
            while (true) {
                System.out.print("jlox> ");
                String line = br.readLine();
                if (line == null)
                    break;
                Lox.run(line);
            }
            br.close();
        } else {
            String input = Files.readString(Path.of(args[0]));
            Lox.run(input);
            if (Lox.error) {
                System.exit(1);
            }
        }
    }
}

import java.io.BufferedReader;
import java.io.BufferedWriter;
import java.io.InputStreamReader;
import java.io.OutputStreamWriter;
import java.net.InetSocketAddress;
import java.net.Socket;
import java.nio.charset.StandardCharsets;

/** Minimal Java client for the IronBridge JSON line protocol. */
public final class IronBridgeClient {
    private IronBridgeClient() {}

    private static void usage() {
        System.err.println("Uso: java IronBridgeClient [host] [port] transaction-id account-id amount [currency] [operation]");
    }

    private static String json(String value) {
        return "\"" + value.replace("\\", "\\\\").replace("\"", "\\\"") + "\"";
    }

    private static String arg(String[] args, int index, String fallback) {
        return args.length > index ? args[index] : fallback;
    }

    public static void main(String[] args) throws Exception {
        if (args.length < 3) {
            usage();
            System.exit(2);
        }

        String host = arg(args, 0, "127.0.0.1");
        int port;
        try {
            port = Integer.parseInt(arg(args, 1, "3900"));
        } catch (NumberFormatException e) {
            throw new IllegalArgumentException("porta inválida", e);
        }
        String transactionId = args[2];
        String accountId = arg(args, 3, "123456789012");
        String amount = arg(args, 4, "125.37");
        String currency = arg(args, 5, "BRL");
        String operation = arg(args, 6, "DEBIT");

        String request = "{\"transaction_id\":" + json(transactionId)
                + ",\"account_id\":" + json(accountId)
                + ",\"amount\":" + json(amount)
                + ",\"currency\":" + json(currency)
                + ",\"operation\":" + json(operation) + "}\n";

        try (Socket socket = new Socket()) {
            socket.connect(new InetSocketAddress(host, port), 3000);
            socket.setSoTimeout(3000);
            try (BufferedWriter writer = new BufferedWriter(new OutputStreamWriter(
                    socket.getOutputStream(), StandardCharsets.UTF_8));
                 BufferedReader reader = new BufferedReader(new InputStreamReader(
                    socket.getInputStream(), StandardCharsets.UTF_8))) {
                writer.write(request);
                writer.flush();
                String response = reader.readLine();
                if (response == null) {
                    throw new IllegalStateException("gateway fechou a conexão sem resposta");
                }
                System.out.println(response);
            }
        }
    }
}

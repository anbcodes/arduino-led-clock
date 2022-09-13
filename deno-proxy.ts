import { serve } from "https://deno.land/std/http/mod.ts";
import {
  readAll,
  readerFromStreamReader,
} from "https://deno.land/std/streams/conversion.ts";

const host = "192.168.86.105";
const port = 30003;

serve(async (req) => {
  if (req.method === "POST") {
    const reader = req.body?.getReader();
    if (reader) {
      console.log("req");
      const conn = await Deno.connect({ hostname: host, port });
      const data = await readAll(readerFromStreamReader(reader));
      conn.write(data);
      conn.write(new Uint8Array([0]));
      const res = await readAll(conn);
      console.log("Got response", res);
      return new Response(res);
    }
    return new Response("Invalid Request");
  }
  return new Response("Invalid Request");
}, { hostname: "0.0.0.0", port: +(Deno.args[0] || 8080) });
